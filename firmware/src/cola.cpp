#include "cola.h"

#if CLAWD_COLA

#include "cola_policy.h"
#include "cola_assets.h"      // the only include of the generated art
#include "clawd_policy.h"     // the alert timeout the director's alert uses
#include "ui.h"
#include "hal/sound_hal.h"
#include <Arduino.h>
#include <string.h>
#include <esp_heap_caps.h>

static_assert(COLA_ANIM_COUNT == COLA_STATE_COUNT, "cola_assets.h and ColaState drifted");

// Cola's spoken clips (reply / empty / reset). Off by default: the usage page
// keeps the alert ding and the art, no voice. Build with -DCOLA_VOICE=1 to
// turn them back on (e.g. for the room page). The audio data stays in
// cola_assets.h; with the switch off the linker drops it from the image.
#ifndef COLA_VOICE
#define COLA_VOICE 0
#endif

// How long each frame stays up, per state.
static const uint16_t HOLD_MS[COLA_STATE_COUNT] = {
    250,   // idle
    100,   // prompt
    110,   // working
    150,   // reply
    200,   // low
    400,   // empty
    110,   // reset
};
// A sound that couldn't start within this long (the ding was playing) is
// dropped rather than played late.
#define CLIP_WAIT_MS   3000
// Not live (stale data, or the pairing hint): the art stays, dimmed like the
// meters.
#define DIM_OPA        LV_OPA_40

static lv_obj_t*      img = NULL;
static uint8_t*       buf = NULL;      // planar RGB565A8: colour plane, then alpha
static lv_image_dsc_t dsc;

static ColaState state = COLA_IDLE;
static uint8_t   frame = 0;
static uint32_t  frame_at = 0;
static bool      dimmed = false;
static int       forced = -1;          // `cola <state>`: a ColaState held for QA

// What it has seen
static bool      have_pct = false;
static float     pct = 0;
static bool      limited = false;
static bool      was_empty = false;
static bool      alert = false;
static uint32_t  alert_since = 0;
static bool      prompt_playing = false;
static bool      reset_playing = false;
static int       restart = -1;         // a one-shot asked for again: that state, from its first frame

// A sound waiting for the speaker
static int       clip = -1;
static uint32_t  clip_at = 0;

static void draw(void) {
    const cola_frame_t& f = cola_anims[state].frames[frame];
    const size_t n = (size_t)COLA_W * COLA_H;
    uint16_t* color = (uint16_t*)buf;
    uint8_t*  a = buf + n * 2;
    for (size_t i = 0; i < n; i++) {
        const uint8_t idx = f.px[i];
        color[i] = f.rgb[idx];
        a[i] = f.alpha[idx];
    }
    lv_obj_invalidate(img);
}

static void enter(ColaState s, uint32_t now) {
    state = s;
    frame = 0;
    frame_at = now;
    // A one-shot that something more urgent cut short doesn't come back.
    if (s != COLA_PROMPT) prompt_playing = false;
    if (s != COLA_RESET)  reset_playing = false;
    draw();
}

static void want_clip(ColaState s) {
    if (!COLA_VOICE) return;
    if (!cola_clips[s].pcm) return;
    clip = s;
    clip_at = millis();
}

// A tap dismisses an alert (clawd_director.cpp); this layer's copy of it too.
static void tap_cb(lv_event_t* e) {
    (void)e;
    alert = false;
}

void cola_create(lv_obj_t* parent, int right, int bottom) {
    const size_t bytes = (size_t)COLA_W * COLA_H * 3;
    buf = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!buf) { Serial.println("cola: no PSRAM for its frame, layer off"); return; }
    memset(buf, 0, bytes);

    dsc.header.w = COLA_W;
    dsc.header.h = COLA_H;
    dsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
    dsc.header.stride = COLA_W * 2;
    dsc.data = buf;
    dsc.data_size = bytes;

    img = lv_image_create(parent);
    lv_image_set_src(img, &dsc);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(img, right - COLA_W, bottom - COLA_H);

    lv_indev_t* touch = lv_indev_get_next(NULL);
    if (touch) lv_indev_add_event_cb(touch, tap_cb, LV_EVENT_PRESSED, NULL);

    enter(COLA_IDLE, millis());
}

void cola_on_payload(const UsageData* d, const SignalsUpdate* sig) {
    if (!img) return;
    const uint32_t now = millis();
    if (d->alert_msg[0]) {
        if (!alert) want_clip(COLA_REPLY);     // a refresh of the same alert is silent
        alert = true;
        alert_since = now;
    }
    if (d->alert_clear) alert = false;
    if (d->valid && d->ok) {
        have_pct = true;
        pct = d->session_pct;
        limited = strcmp(d->status, "limited") == 0;
        const bool empty = cola_empty(have_pct, pct, limited);
        if (empty && !was_empty) want_clip(COLA_EMPTY);
        was_empty = empty;
    }
    if (sig->event_fired && sig->event == 'p') {
        prompt_playing = true;
        restart = COLA_PROMPT;
    }
    if (sig->session_reset) {
        reset_playing = true;
        restart = COLA_RESET;
        want_clip(COLA_RESET);
    }
}

void cola_tick(void) {
    if (!img) return;
    const uint32_t now = millis();
    if (alert && (uint32_t)(now - alert_since) >= clawd_tuning().alert_timeout_ms) alert = false;

    ColaState want;
    if (forced >= 0) {
        want = (ColaState)forced;
    } else {
        const ColaInput in = {
            !ui_pair_hint_visible(), have_pct, pct, limited, signals_activity(),
            alert, prompt_playing, reset_playing,
        };
        want = cola_pick(&in);
    }

    if (want != state || restart == (int)want) {
        enter(want, now);
    } else if ((uint32_t)(now - frame_at) >= HOLD_MS[state]) {
        frame_at = now;
        if (frame + 1 < cola_anims[state].count) {
            frame++;
            draw();
        } else if (cola_oneshot(state) && forced < 0) {
            // Played through: its last frame stays up until the next tick
            // picks what holds now.
            if (state == COLA_PROMPT) prompt_playing = false;
            else                      reset_playing = false;
        } else if (cola_anims[state].count > 1) {
            frame = 0;
            draw();
        }
    }
    restart = -1;

    const bool dim = forced < 0 && !ui_meters_live();
    if (dim != dimmed) {
        dimmed = dim;
        lv_obj_set_style_image_opa(img, dim ? DIM_OPA : LV_OPA_COVER, 0);
    }

    if (clip >= 0) {
        const cola_clip_t& c = cola_clips[clip];
        if ((uint32_t)(now - clip_at) > CLIP_WAIT_MS) clip = -1;
        else if (sound_hal_clip(c.pcm, c.frames, c.rate)) clip = -1;
    }
}

bool cola_debug(const char* name) {
    if (!img) return false;
    if (strcmp(name, "auto") == 0) {
        forced = -1;
        return true;
    }
    const ColaState s = cola_state_from_name(name);
    if (s == COLA_STATE_COUNT) return false;
    forced = s;
    enter(s, millis());
    want_clip(s);
    return true;
}

#endif  // CLAWD_COLA
