#include "room.h"

#if CLAWD_COLA

#include "room_policy.h"
#include "room_assets.h"      // the only include of the generated art
#include "cola_lz4.h"
#include "clawd_director.h"
#include "idle.h"
#include "hal/board_caps.h"
#include <Arduino.h>
#include <string.h>
#include <esp_heap_caps.h>

// Her spoken clips on this page. Off, as on the usage page: the room has no
// audio yet. With -DCOLA_VOICE=1 voice_hook() is where a clip for the act
// that just began would be asked for (cola.cpp: want_clip / sound_hal_clip).
#ifndef COLA_VOICE
#define COLA_VOICE 0
#endif

// Where her feet are on the rug (room pixels, 480x480).
#define FEET_X 240
#define FEET_Y 418

// A swipe: this far sideways, this little up or down, this quickly.
#define SWIPE_MIN_DX   100
#define SWIPE_MAX_DY   80
#define SWIPE_MAX_MS   800
// A tap (poke): barely moved.
#define TAP_MAX_MOVE   25
#define TAP_MAX_MS     600

static lv_obj_t* root = NULL;
static lv_obj_t* bg_img = NULL;
static lv_obj_t* sp_img = NULL;
static int       off_x = 0, off_y = 0;       // the 480x480 room's corner on the screen

static uint8_t*  bgbuf = NULL;               // RGB565 480x480 (PSRAM)
static uint8_t*  scratch = NULL;             // decompressed indices of what's being drawn (PSRAM)
static uint8_t*  spbuf = NULL;               // sprite: RGB565 plane, then alpha plane (PSRAM)
static lv_image_dsc_t bg_dsc, sp_dsc;

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
static bool      poke_pending = false;
static int       poke_kind = -1;

// Touch tracking (see the indev callback)
static lv_point_t press_pt;
static uint32_t   press_ms = 0;

static void voice_hook(RoomAct a) {
    (void)a;
#if COLA_VOICE
    // No room clips yet: ask for one here when there are.
#endif
}

// ---- tone ----------------------------------------------------------------
static RoomTone current_tone(void) {
    if (tone_override >= 0) return (RoomTone)tone_override;
    if (!have_clock) return TONE_DAY;
    const long now_s = clock_epoch + (long)((uint32_t)(millis() - clock_ms) / 1000);
    return room_tone_for_minute((int)((now_s % 86400) / 60));
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
        lv_obj_move_foreground(root);
        lv_obj_clear_flag(root, LV_OBJ_FLAG_HIDDEN);
        bg_drawn = false;
        drawn_tone = TONE_COUNT;
        drawn_act = drawn_frame = -1;
        visible = true;
        refresh(current_tone());
    } else {
        visible = false;
        lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    }
    Serial.printf("{\"room\":\"%s\"}\n", on ? "on" : "off");
}

// Her hit box: the sprite, or the scene's patch while she's part of it.
static bool hit(int x, int y) {
    int x0, y0, x1, y1;
    const room_patch_t* p = room_act_is_scene(brain.act) ? patch_for(&brain) : NULL;
    if (p) {
        x0 = off_x + p->x; y0 = off_y + p->y; x1 = x0 + p->w; y1 = y0 + p->h;
    } else {
        x0 = off_x + FEET_X - ROOM_POSE_W / 2;
        y0 = off_y + FEET_Y - ROOM_POSE_GROUND;
        x1 = x0 + ROOM_POSE_W; y1 = y0 + ROOM_POSE_H;
    }
    return x >= x0 && x < x1 && y >= y0 && y < y1;
}

// One callback on the touch device. A press records where and when; the
// release says what it was: a swipe (page change) or a tap (a poke).
static void indev_cb(lv_event_t* e) {
    lv_indev_t* dev = (lv_indev_t*)lv_event_get_target(e);
    lv_point_t p = { 0, 0 };
    lv_indev_get_point(dev, &p);
    const uint32_t now = millis();
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        press_pt = p;
        press_ms = now;
        return;
    }
    if (!root || idle_is_asleep()) return;
    const int dx = p.x - press_pt.x, dy = p.y - press_pt.y;
    const uint32_t dt = now - press_ms;
    if (abs(dx) >= SWIPE_MIN_DX && abs(dy) <= SWIPE_MAX_DY && dt <= SWIPE_MAX_MS) {
        if (!visible && dx < 0) show(true);
        else if (visible && dx > 0) show(false);
    } else if (visible && abs(dx) <= TAP_MAX_MOVE && abs(dy) <= TAP_MAX_MOVE && dt <= TAP_MAX_MS) {
        if (hit(press_pt.x, press_pt.y)) { poke_pending = true; poke_kind = -1; }
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

void room_tick(void) {
    if (!root || !visible || idle_is_asleep()) return;
    const RoomTone tone = current_tone();
    RoomIn in = {};
    in.now = millis();
    in.tone = tone;
    in.force = forced;
    in.poke = poke_pending;
    in.poke_kind = poke_kind;
    poke_pending = false;
    const RoomAct before = brain.act;
    room_brain_step(&brain, &in);
    if (brain.act != before) voice_hook(brain.act);
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
    if (strcmp(what, "status") == 0) {
        const RoomTone t = current_tone();
        Serial.printf("{\"room\":{\"visible\":%s,\"act\":\"%s\",\"frame\":%d,\"tone\":\"%s\","
                      "\"clock\":%s,\"forced\":%s}}\n",
                      visible ? "true" : "false", room_act_name(brain.act), brain.frame,
                      room_tone_name(t), have_clock ? "true" : "false", forced >= 0 ? "true" : "false");
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
