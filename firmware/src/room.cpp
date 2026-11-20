#include "room.h"

#if CLAWD_COLA

#include "room_policy.h"
#include "room_assets.h"      // the only include of the generated art
#include "cola_lz4.h"
#include "clawd_director.h"
#include "idle.h"
#include "reminder.h"
#include "hal/board_caps.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include <esp_heap_caps.h>

// Her spoken lines on this page (room_voice_assets.h: 9 clips, IMA ADPCM,
// 16 kHz, about 100 KB of flash). Only here: the usage page stays quiet. Its
// own switch, apart from COLA_VOICE (the usage layer's clips): the
// waveshare_amoled_216 env sets -DROOM_VOICE=1; without it nothing is
// compiled in and say() does nothing.
#ifndef ROOM_VOICE
#define ROOM_VOICE 0
#endif
#if ROOM_VOICE
#include "room_voice_assets.h"
#include "hal/sound_hal.h"
static_assert(ROOM_VOICE_N == RV_COUNT, "room_voice_assets.h and RoomVoice drifted");
#endif

// The voice switch: a small speaker badge at the top centre of the page; a tap
// on it turns her lines on or off, and the choice is kept in NVS
// (clawdmeter/room_voice, on by default). Room pixels, like everything here.
// At the centre, not a corner: on a unit measured with the touch x axis
// mirrored against the picture, a corner badge was unreachable, and the centre
// is the same either way.
#define VOICE_BTN_X    218
#define VOICE_BTN_Y    10
#define VOICE_BTN_SIZE 44
#define VOICE_BTN_HIT  64          // the tap area, a bit bigger than the badge
#define VOICE_HOLD_MS  1000        // hold this long on empty floor or wall to switch it (see held_voice)

// Where her feet are on the rug (room pixels, 480x480).
#define FEET_X 240
#define FEET_Y 418
// Eating a snack she stands by the cabinet: as close as she can get with the
// drawer patch (x 417-466, y 283-340) left clear in all three frames
// (assets/cola_room/preview_snack_pos.png).
#define SNACK_FEET_X 384
#define SNACK_FEET_Y 428

// A swipe: this far sideways, this little up or down, this quickly.
#define SWIPE_MIN_DX   100
#define SWIPE_MAX_DY   80
#define SWIPE_MAX_MS   800
// A tap (poke): barely moved.
#define TAP_MAX_MOVE   25
#define TAP_MAX_MS     600
// A head pat: pressed and held this long, finger moved at most this far, on
// the top part of her (a share of her height).
#define PAT_HOLD_MS    600
#define PAT_MAX_MOVE   20
#define PAT_HEAD_PCT   30

static lv_obj_t* root = NULL;
static lv_obj_t* bg_img = NULL;
static lv_obj_t* sp_img = NULL;
static int       off_x = 0, off_y = 0;       // the 480x480 room's corner on the screen
// Burn-in drift: everything on the page (background, patches, her) moves
// together by this much, a pixel at a time, every few minutes.
static int       shift_x = 0, shift_y = 0;
static unsigned  shift_step = 0;
static uint32_t  shift_at = 0;

static uint8_t*  bgbuf = NULL;               // RGB565 480x480 (PSRAM)
static uint8_t*  scratch = NULL;             // decompressed indices of what's being drawn (PSRAM)
static uint8_t*  spbuf = NULL;               // sprite: RGB565 plane, then alpha plane (PSRAM)
static lv_image_dsc_t bg_dsc, sp_dsc;

static lv_obj_t* voice_badge = NULL;         // the speaker badge, and the slash over it while muted
static lv_obj_t* voice_slash = NULL;
static bool      voice_on = true;            // her lines are on (the NVS switch)

static RoomBrain brain;
static bool      visible = false;
static bool      mem_failed = false;

// What's drawn now, so only changes are redrawn.
static const room_patch_t* drawn_patch = NULL;
static bool      bg_drawn = false;
static RoomTone  drawn_tone = TONE_COUNT;
static int       drawn_act = -1, drawn_frame = -1;
static uint16_t  lut[256];                   // the background palette in the current tone

// Time of day
static bool      have_clock = false;
static long      clock_epoch = 0;
static uint32_t  clock_ms = 0;
static int       tone_override = -1;         // `room day|dusk|night`, -1 = from the clock

static int       forced = -1;                // `room <act>`
static bool      remind_on = false;          // a reminder (room_remind): she drinks
static int       remind_saved = -1;          // what `forced` was before it
static bool      poke_pending = false;
static int       poke_kind = -1;             // a reaction RoomAct (RA_PAT too), or -1: by the poke count
static PokeCount pokes = {};
static bool      night_said = false;         // the night line has been spoken for this stretch of night
static long      morning_day = -1;           // the day the morning line was spoken

// Touch tracking (see the indev callback)
static lv_point_t press_pt;
static uint32_t   press_ms = 0;
static lv_indev_t* press_dev = NULL;
static bool       pressing = false;
static bool       hold_dead = false;         // moved too far, or already decided: no pat from this press
static bool       hold_fired = false;        // this press made a pat: its release is nothing else
static bool       vhold_dead = false;        // moved too far, or already decided: no voice switch from this press
static bool       press_in_remind = false;   // the press began during a reminder: its tap answers it

// Where her feet are for an act (room pixels).
static void feet_for(RoomAct a, int* x, int* y) {
    *x = a == RA_SNACK ? SNACK_FEET_X : FEET_X;
    *y = a == RA_SNACK ? SNACK_FEET_Y : FEET_Y;
}

static void place_sprite(RoomAct a) {
    int fx, fy;
    feet_for(a, &fx, &fy);
    lv_obj_set_pos(sp_img, off_x + shift_x + fx - ROOM_POSE_W / 2, off_y + shift_y + fy - ROOM_POSE_GROUND);
}

// One step round the drift ring: the background moves, and she with it.
static void apply_shift(void) {
    room_shift_for_step(shift_step, &shift_x, &shift_y);
    lv_obj_set_pos(bg_img, off_x + shift_x, off_y + shift_y);
    if (voice_badge) lv_obj_set_pos(voice_badge, off_x + shift_x + VOICE_BTN_X, off_y + shift_y + VOICE_BTN_Y);
    place_sprite((RoomAct)brain.act);
}

// Panel dimming and the USB sleep permission that go with the page: dim by
// the time of day, and at night let the panel sleep even on USB power.
static void apply_burnin(RoomTone tone, bool clock_known) {
    idle_set_awake_scale(room_brightness_pct(tone, clock_known));
    idle_set_sleep_on_usb(tone == TONE_NIGHT);
}

static void sound_stop_voice(void) {
#if ROOM_VOICE
    sound_hal_voice_stop();
#endif
}

// ---- tone ----------------------------------------------------------------
static RoomTone current_tone(void) {
    if (tone_override >= 0) return (RoomTone)tone_override;
    if (!have_clock) return TONE_DAY;
    const long now_s = clock_epoch + (long)((uint32_t)(millis() - clock_ms) / 1000);
    return room_tone_for_minute((int)((now_s % 86400) / 60));
}

// Speaks a line, cutting off one that is playing; nothing while the page is
// down, and nothing if the ding has the speaker. True if it started.
static bool say(RoomVoice v) {
#if ROOM_VOICE
    if (!visible || !voice_on || (unsigned)v >= RV_COUNT) return false;
    return sound_hal_voice(room_voices[v].adpcm, room_voices[v].samples, room_voices[v].rate);
#else
    (void)v;
    return false;
#endif
}

// The clock's local time: seconds since the epoch the app sent, moved on.
static long clock_now_s(void) {
    return clock_epoch + (long)((uint32_t)(millis() - clock_ms) / 1000);
}

// What she says when an act begins (a poke's line is spoken with the poke).
static void on_act(RoomAct a) {
    switch (a) {
    case RA_SNACK: say(RV_SNACK); break;
    case RA_DRINK: if (!remind_on && random(3) == 0) say(RV_DRINK); break;   // a reminder speaks for itself
    case RA_SLEEP:
        if (!night_said) { night_said = true; say(RV_NIGHT); }
        break;
    default: break;
    }
}

// ---- drawing -------------------------------------------------------------
static bool alloc_mem(void) {
    if (bgbuf) return true;
    if (mem_failed) return false;
    const size_t bg_bytes = (size_t)ROOM_W * ROOM_H * 2;
    const size_t sp_n = (size_t)ROOM_POSE_W * ROOM_POSE_H;
    bgbuf   = (uint8_t*)heap_caps_malloc(bg_bytes, MALLOC_CAP_SPIRAM);
    scratch = (uint8_t*)heap_caps_malloc((size_t)ROOM_W * ROOM_H, MALLOC_CAP_SPIRAM);
    spbuf   = (uint8_t*)heap_caps_malloc(sp_n * 3, MALLOC_CAP_SPIRAM);
    if (!bgbuf || !scratch || !spbuf) {
        Serial.println("room: no PSRAM for the room page, page off");
        heap_caps_free(bgbuf); heap_caps_free(scratch); heap_caps_free(spbuf);
        bgbuf = scratch = spbuf = NULL;
        mem_failed = true;
        return false;
    }
    memset(bgbuf, 0, bg_bytes);
    memset(spbuf, 0, sp_n * 3);

    bg_dsc.header.w = ROOM_W;
    bg_dsc.header.h = ROOM_H;
    bg_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    bg_dsc.header.stride = ROOM_W * 2;
    bg_dsc.data = bgbuf;
    bg_dsc.data_size = bg_bytes;
    lv_image_set_src(bg_img, &bg_dsc);

    sp_dsc.header.w = ROOM_POSE_W;
    sp_dsc.header.h = ROOM_POSE_H;
    sp_dsc.header.cf = LV_COLOR_FORMAT_RGB565A8;     // planar: colour, then alpha
    sp_dsc.header.stride = ROOM_POSE_W * 2;
    sp_dsc.data = spbuf;
    sp_dsc.data_size = sp_n * 3;
    lv_image_set_src(sp_img, &sp_dsc);
    return true;
}

static void make_lut(RoomTone tone) {
    for (int i = 0; i < ROOM_PALETTE_N; i++) lut[i] = room_tint565(room_palette[i], tone);
    lut[255] = lut[0];
}

// The background, decompressed, in the current tone, with a patch over it (her
// reading or asleep, the snack drawer open). One decompression at a time:
// the scratch holds the background's indices, then the patch's.
static void render_bg(const room_patch_t* patch) {
    const size_t n = (size_t)ROOM_W * ROOM_H;
    uint16_t* out = (uint16_t*)bgbuf;
    if (!cola_lz4_decode(room_bg_px, room_bg_len, scratch, n)) return;    // corrupt flash: keep what's up
    for (size_t i = 0; i < n; i++) out[i] = lut[scratch[i]];
    if (patch && patch->x + patch->w <= ROOM_W && patch->y + patch->h <= ROOM_H &&
        cola_lz4_decode(patch->px, patch->px_len, scratch, (size_t)patch->w * patch->h)) {
        for (int r = 0; r < patch->h; r++) {
            const uint8_t* src = scratch + (size_t)r * patch->w;
            uint16_t* dst = out + (size_t)(patch->y + r) * ROOM_W + patch->x;
            for (int c = 0; c < patch->w; c++) dst[c] = lut[src[c]];
        }
    }
    drawn_patch = patch;
    bg_drawn = true;
    lv_obj_invalidate(bg_img);
}

static const room_pose_t* pose_for(RoomAct a, uint8_t frame) {
    switch (a) {
    case RA_IDLE:     return &room_pose_idle[frame % 2];
    case RA_GAME:     return &room_pose_game[frame % 3];
    case RA_DRINK:    return &room_pose_drink[frame % 3];
    case RA_STRETCH:  return &room_pose_stretch[frame % 3];
    case RA_SURPRISE: return &room_pose_surprise[frame % 2];
    case RA_SHY:      return &room_pose_shy[frame % 2];
    case RA_ANGRY:    return &room_pose_angry[frame % 2];
    case RA_SNACK:    return &room_pose_snack[frame % 3];
    case RA_PAT:      return &room_pose_pat[frame % 2];
    default:          return NULL;
    }
}

static void draw_pose(const room_pose_t* f, RoomTone tone) {
    const size_t n = (size_t)ROOM_POSE_W * ROOM_POSE_H;
    if (!cola_lz4_decode(f->px, f->px_len, scratch, n)) return;
    uint16_t pal[256];
    for (int i = 0; i < f->ncol; i++) pal[i] = room_tint565(f->rgb[i], tone);
    uint16_t* color = (uint16_t*)spbuf;
    uint8_t*  a = spbuf + n * 2;
    for (size_t i = 0; i < n; i++) {
        const uint8_t p = scratch[i] < f->ncol ? scratch[i] : 0;
        color[i] = pal[p];
        a[i] = f->alpha[p];
    }
    lv_obj_invalidate(sp_img);
}

static const room_patch_t* patch_for(const RoomBrain* b) {
    switch (b->act) {
    case RA_READ:  return room_read_patches[b->frame % 3];
    case RA_SLEEP: return room_sleep_patches[b->frame % 3];
    case RA_SNACK: return &room_snack_patch;
    default:       return NULL;
    }
}

// Brings the screen in line with the brain and the tone.
static void refresh(RoomTone tone) {
    const bool tone_changed = tone != drawn_tone;
    if (tone_changed) {
        make_lut(tone);
        drawn_tone = tone;
    }
    const room_patch_t* patch = patch_for(&brain);
    if (tone_changed || !bg_drawn || patch != drawn_patch) render_bg(patch);

    if (room_act_is_scene(brain.act)) {
        lv_obj_add_flag(sp_img, LV_OBJ_FLAG_HIDDEN);
        drawn_act = brain.act;
        drawn_frame = brain.frame;
    } else if (tone_changed || drawn_act != brain.act || drawn_frame != brain.frame) {
        const room_pose_t* f = pose_for(brain.act, brain.frame);
        if (drawn_act != brain.act) place_sprite((RoomAct)brain.act);
        if (f) draw_pose(f, tone);
        lv_obj_clear_flag(sp_img, LV_OBJ_FLAG_HIDDEN);
        drawn_act = brain.act;
        drawn_frame = brain.frame;
    }
}

// ---- showing and hiding --------------------------------------------------
static void show(bool on) {
    if (!root || on == visible) return;
    if (on) {
        if (!alloc_mem()) return;
        // Whatever the meters were doing (a peek from the press that started
        // this swipe, an alert) is over: the director goes home first.
        director_debug("home");
        director_hold_screensaver(true);
        lv_obj_move_foreground(root);
        lv_obj_clear_flag(root, LV_OBJ_FLAG_HIDDEN);
        bg_drawn = false;
        drawn_tone = TONE_COUNT;
        drawn_act = drawn_frame = -1;
        visible = true;
        shift_at = millis();
        apply_shift();
        apply_burnin(current_tone(), have_clock || tone_override >= 0);
        pressing = hold_dead = hold_fired = vhold_dead = false;
        refresh(current_tone());
        // Coming in: the morning line, once a day, and the night line at night.
        if (have_clock) {
            const long now_s = clock_now_s();
            if (room_is_morning((int)((now_s % 86400) / 60)) && now_s / 86400 != morning_day) {
                morning_day = now_s / 86400;
                say(RV_MORNING);
            }
        }
        if (current_tone() == TONE_NIGHT && !night_said) {
            night_said = true;
            say(RV_NIGHT);
        }
    } else {
        sound_stop_voice();
        night_said = false;
        visible = false;
        lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
        idle_set_awake_scale(100);          // the meters keep the user's brightness
        idle_set_sleep_on_usb(false);
        director_hold_screensaver(false);
    }
    Serial.printf("{\"room\":\"%s\"}\n", on ? "on" : "off");
}

// ---- the voice switch ----------------------------------------------------
static void voice_load(void) {
    Preferences prefs;
    prefs.begin("clawdmeter", true);
    voice_on = prefs.getBool("room_voice", true);
    prefs.end();
}

static void voice_save(void) {
    Preferences prefs;
    prefs.begin("clawdmeter", false);
    prefs.putBool("room_voice", voice_on);
    prefs.end();
}

// The badge shows the state: a speaker with sound waves, and a red slash over
// it while her lines are off.
static void voice_badge_update(void) {
    if (!voice_slash) return;
    if (voice_on) lv_obj_add_flag(voice_slash, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(voice_slash, LV_OBJ_FLAG_HIDDEN);
}

// Turns her lines on or off for good (NVS); off cuts a line that is playing.
static void voice_set(bool on) {
    if (on == voice_on) return;
    voice_on = on;
    if (!voice_on) sound_stop_voice();
    voice_save();
    voice_badge_update();
    Serial.printf("{\"room_voice\":%s}\n", voice_on ? "true" : "false");
}

static bool voice_hit(int x, int y) {
    const int cx = off_x + shift_x + VOICE_BTN_X + VOICE_BTN_SIZE / 2;
    const int cy = off_y + shift_y + VOICE_BTN_Y + VOICE_BTN_SIZE / 2;
    return abs(x - cx) <= VOICE_BTN_HIT / 2 && abs(y - cy) <= VOICE_BTN_HIT / 2;
}

// Her box: the sprite, or the scene's patch while she's part of it.
static void her_box(int* ox0, int* oy0, int* ox1, int* oy1) {
    int x0, y0, x1, y1;
    const room_patch_t* p = room_act_is_scene(brain.act) ? patch_for(&brain) : NULL;
    if (p) {
        x0 = off_x + shift_x + p->x; y0 = off_y + shift_y + p->y; x1 = x0 + p->w; y1 = y0 + p->h;
    } else {
        int fx, fy;
        feet_for(brain.act, &fx, &fy);
        x0 = off_x + shift_x + fx - ROOM_POSE_W / 2;
        y0 = off_y + shift_y + fy - ROOM_POSE_GROUND;
        x1 = x0 + ROOM_POSE_W; y1 = y0 + ROOM_POSE_H;
    }
    *ox0 = x0; *oy0 = y0; *ox1 = x1; *oy1 = y1;
}

static bool hit(int x, int y) {
    int x0, y0, x1, y1;
    her_box(&x0, &y0, &x1, &y1);
    return x >= x0 && x < x1 && y >= y0 && y < y1;
}

// The top part of her box: her head.
static bool head_hit(int x, int y) {
    int x0, y0, x1, y1;
    her_box(&x0, &y0, &x1, &y1);
    return x >= x0 && x < x1 && y >= y0 && y < y0 + (y1 - y0) * PAT_HEAD_PCT / 100;
}

// One callback on the touch device. A press records where and when; the
// release says what it was: a swipe (page change) or a tap (a poke). A press
// held on her head turns into a pat before it is released (held_pat).
static void indev_cb(lv_event_t* e) {
    lv_indev_t* dev = (lv_indev_t*)lv_event_get_target(e);
    lv_point_t p = { 0, 0 };
    lv_indev_get_point(dev, &p);
    const uint32_t now = millis();
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        press_pt = p;
        press_ms = now;
        press_dev = dev;
        pressing = true;
        hold_dead = hold_fired = vhold_dead = false;
        press_in_remind = remind_on;
        return;
    }
    pressing = false;
    if (hold_fired) {                  // it was a pat: not a swipe or a tap too
        hold_fired = false;
        return;
    }
    if (!root || idle_is_asleep()) return;
    const int dx = p.x - press_pt.x, dy = p.y - press_pt.y;
    const uint32_t dt = now - press_ms;
    if (abs(dx) >= SWIPE_MIN_DX && abs(dy) <= SWIPE_MAX_DY && dt <= SWIPE_MAX_MS) {
        if (!visible && dx < 0) show(true);
        else if (visible && dx > 0) show(false);
    } else if (visible && abs(dx) <= TAP_MAX_MOVE && abs(dy) <= TAP_MAX_MOVE && dt <= TAP_MAX_MS) {
        if (press_in_remind || remind_on) reminder_ack();   // answers it: no poke (the director may have already)
        else if (voice_hit(press_pt.x, press_pt.y)) voice_set(!voice_on);
        else if (hit(press_pt.x, press_pt.y)) { poke_pending = true; poke_kind = -1; }
    }
}

// ---- public --------------------------------------------------------------
void room_create(lv_obj_t* parent) {
    const BoardCaps& c = board_caps();
    off_x = (c.width - ROOM_W) / 2;
    off_y = (c.height - ROOM_H) / 2;

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, c.width, c.height);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);

    bg_img = lv_image_create(root);
    lv_obj_clear_flag(bg_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(bg_img, off_x, off_y);
    lv_obj_set_size(bg_img, ROOM_W, ROOM_H);

    sp_img = lv_image_create(root);
    lv_obj_clear_flag(sp_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(sp_img, off_x + FEET_X - ROOM_POSE_W / 2, off_y + FEET_Y - ROOM_POSE_GROUND);
    lv_obj_set_size(sp_img, ROOM_POSE_W, ROOM_POSE_H);

    // The voice badge: a dim round plate, a speaker drawn from a box, a cone
    // and a wave, and a slash that shows while she's muted. Plain LVGL shapes.
    static const lv_point_precise_t cone_pts[] = { {18, 17}, {26, 10}, {26, 34}, {18, 27}, {18, 17} };
    static const lv_point_precise_t wave_pts[] = { {31, 17}, {34, 22}, {31, 27} };
    static const lv_point_precise_t slash_pts[] = { {9, 9}, {35, 35} };
    voice_badge = lv_obj_create(root);
    lv_obj_remove_style_all(voice_badge);
    lv_obj_set_size(voice_badge, VOICE_BTN_SIZE, VOICE_BTN_SIZE);
    lv_obj_set_pos(voice_badge, off_x + VOICE_BTN_X, off_y + VOICE_BTN_Y);
    lv_obj_set_style_radius(voice_badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(voice_badge, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(voice_badge, LV_OPA_40, 0);
    lv_obj_clear_flag(voice_badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(voice_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* box = lv_obj_create(voice_badge);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 8, 10);
    lv_obj_set_pos(box, 10, 17);
    lv_obj_set_style_bg_color(box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_80, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
    const struct { const lv_point_precise_t* pts; uint32_t n; } lines[] = {
        { cone_pts, 5 }, { wave_pts, 3 },
    };
    for (const auto& l : lines) {
        lv_obj_t* ln = lv_line_create(voice_badge);
        lv_line_set_points(ln, l.pts, l.n);
        lv_obj_set_style_line_width(ln, 2, 0);
        lv_obj_set_style_line_color(ln, lv_color_white(), 0);
        lv_obj_set_style_line_opa(ln, LV_OPA_80, 0);
        lv_obj_set_style_line_rounded(ln, true, 0);
        lv_obj_clear_flag(ln, LV_OBJ_FLAG_CLICKABLE);
    }
    voice_slash = lv_line_create(voice_badge);
    lv_line_set_points(voice_slash, slash_pts, 2);
    lv_obj_set_style_line_width(voice_slash, 3, 0);
    lv_obj_set_style_line_color(voice_slash, lv_color_make(0xE0, 0x44, 0x3C), 0);
    lv_obj_set_style_line_rounded(voice_slash, true, 0);
    lv_obj_clear_flag(voice_slash, LV_OBJ_FLAG_CLICKABLE);
    voice_load();
    voice_badge_update();

    lv_indev_t* touch = lv_indev_get_next(NULL);
    if (touch) {
        lv_indev_add_event_cb(touch, indev_cb, LV_EVENT_PRESSED, NULL);
        lv_indev_add_event_cb(touch, indev_cb, LV_EVENT_RELEASED, NULL);
    }
    room_brain_init(&brain, millis(), (uint32_t)micros() * 2654435761u ^ (uint32_t)millis());
}

void room_on_payload(const UsageData* d) {
    if (d->clock_epoch > 0) {
        have_clock = true;
        clock_epoch = d->clock_epoch;
        clock_ms = millis();
    }
}

bool room_visible(void) { return visible; }

void room_remind(bool on) {
    if (on == remind_on) return;
    remind_on = on;
    if (on) { remind_saved = forced; forced = RA_DRINK; }
    else    { forced = remind_saved; remind_saved = -1; }
}

bool room_say_reminder(void) {
#if ROOM_VOICE
    if (!voice_on) return true;                // muted: nothing to wait for
    return sound_hal_voice(room_voices[RV_DRINK].adpcm, room_voices[RV_DRINK].samples, room_voices[RV_DRINK].rate);
#else
    return true;
#endif
}

bool room_voice_on(void) { return voice_on; }
void room_voice_set(bool on) { voice_set(on); }

// A press held on her head for PAT_HOLD_MS without moving more than
// PAT_MAX_MOVE px is a pat. Decided once, when the time is up; a finger that
// strays before then (a swipe) rules it out.
static void held_pat(uint32_t now) {
    if (!pressing || hold_dead || !press_dev) return;
    if (remind_on) { hold_dead = true; return; }   // she's drinking: no pat now
    lv_point_t p = press_pt;
    lv_indev_get_point(press_dev, &p);
    if (abs(p.x - press_pt.x) > PAT_MAX_MOVE || abs(p.y - press_pt.y) > PAT_MAX_MOVE) {
        hold_dead = true;
        return;
    }
    if ((uint32_t)(now - press_ms) < PAT_HOLD_MS) return;
    hold_dead = true;
    if (head_hit(press_pt.x, press_pt.y) && brain.act != RA_PAT) {
        hold_fired = true;
        poke_pending = true;
        poke_kind = RA_PAT;
    }
}

// A press held VOICE_HOLD_MS anywhere that isn't her, without moving, turns
// her lines on or off. It doesn't depend on where the finger lands, so it works
// however the board is turned (a corner badge was out of reach on a rotated
// unit).
static void held_voice(uint32_t now) {
    if (!pressing || vhold_dead || !press_dev) return;
    lv_point_t p = press_pt;
    lv_indev_get_point(press_dev, &p);
    if (abs(p.x - press_pt.x) > PAT_MAX_MOVE || abs(p.y - press_pt.y) > PAT_MAX_MOVE) {
        vhold_dead = true;
        return;
    }
    if ((uint32_t)(now - press_ms) < VOICE_HOLD_MS) return;
    vhold_dead = true;
    if (!hit(press_pt.x, press_pt.y)) {
        hold_fired = true;
        voice_set(!voice_on);
    }
}

void room_tick(void) {
    if (!root || !visible || idle_is_asleep()) return;
    const RoomTone tone = current_tone();
    RoomIn in = {};
    in.now = millis();
    in.tone = tone;
    in.force = forced;
    held_pat(in.now);
    held_voice(in.now);
    if (tone != TONE_NIGHT) night_said = false;
    // A poke or pat asked for: which reaction, and its line. Not while she is
    // being patted. A touch poke (kind -1) goes by the pokes in the last 10 s.
    if (poke_pending && brain.act != RA_PAT) {
        const RoomAct k = poke_kind >= 0 ? (RoomAct)poke_kind : room_poke_reaction(&pokes, in.now);
        in.poke = true;
        in.poke_kind = k;
        if (k == RA_PAT) say(random(2) ? RV_LOVE : RV_ILOVEYOU);
        else say(room_voice_for_reaction(k));
    }
    poke_pending = false;
    apply_burnin(tone, have_clock || tone_override >= 0);
    if ((uint32_t)(in.now - shift_at) >= room_tuning().shift_ms) {
        shift_at = in.now;
        shift_step++;
        apply_shift();
    }
    const RoomAct before = brain.act;
    room_brain_step(&brain, &in);
    if (brain.act != before) on_act((RoomAct)brain.act);
    refresh(tone);
}

bool room_debug(const char* what) {
    if (!root) return false;
    if (strcmp(what, "on") == 0)  { show(true);  return visible; }
    if (strcmp(what, "off") == 0) { show(false); return true; }
    if (strcmp(what, "auto") == 0) {
        forced = -1;
        tone_override = -1;
        return true;
    }
    if (strcmp(what, "burnin") == 0) {
        const RoomTone t = current_tone();
        Serial.printf("{\"burnin\":{\"brightness_pct\":%d,\"tone\":\"%s\",\"clock\":%s,"
                      "\"usb_sleep\":%s,\"shift\":[%d,%d],\"visible\":%s}}\n",
                      (int)idle_get_awake_scale(), room_tone_name(t), have_clock ? "true" : "false",
                      idle_get_sleep_on_usb() ? "true" : "false", shift_x, shift_y,
                      visible ? "true" : "false");
        return true;
    }
    if (strcmp(what, "status") == 0) {
        const RoomTone t = current_tone();
        Serial.printf("{\"room\":{\"visible\":%s,\"act\":\"%s\",\"frame\":%d,\"tone\":\"%s\","
                      "\"clock\":%s,\"forced\":%s,\"voice\":%s,\"voice_on\":%s}}\n",
                      visible ? "true" : "false", room_act_name(brain.act), brain.frame,
                      room_tone_name(t), have_clock ? "true" : "false", forced >= 0 ? "true" : "false",
                      ROOM_VOICE ? "true" : "false", voice_on ? "true" : "false");
        return true;
    }
    if (strncmp(what, "voice ", 6) == 0) {      // `room voice on|off`: her lines, kept across reboots
        if (strcmp(what + 6, "on") == 0)  { voice_set(true);  return true; }
        if (strcmp(what + 6, "off") == 0) { voice_set(false); return true; }
        return false;
    }
    if (strncmp(what, "say ", 4) == 0) {         // `room say <name>`: one of her lines, now
        const RoomVoice v = room_voice_from_name(what + 4);
        if (v == RV_COUNT) return false;
        show(true);
        if (!visible) return false;
        const bool played = say(v);
        Serial.printf("{\"say\":\"%s\",\"played\":%s,\"voice_built\":%s}\n", room_voice_name(v),
                      played ? "true" : "false", ROOM_VOICE ? "true" : "false");
        return true;
    }
    const RoomTone t = room_tone_from_name(what);
    if (t != TONE_COUNT) {
        tone_override = t;
        return true;
    }
    if (strncmp(what, "poke", 4) == 0 && (what[4] == '\0' || what[4] == ' ')) {
        int kind = -1;
        if (what[4] == ' ') {
            const RoomAct a = room_act_from_name(what + 5);
            if (!room_act_is_reaction(a)) return false;
            kind = a;
        }
        show(true);
        if (!visible) return false;
        poke_pending = true;
        poke_kind = kind;
        return true;
    }
    const RoomAct a = room_act_from_name(what);
    if (a == RA_COUNT) return false;
    show(true);
    if (!visible) return false;
    if (room_act_is_reaction(a)) {      // `room shy`: one reaction, then back
        poke_pending = true;
        poke_kind = a;
    } else {
        forced = a;
    }
    return true;
}

#endif  // CLAWD_COLA
