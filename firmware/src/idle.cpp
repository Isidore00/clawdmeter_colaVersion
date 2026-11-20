#include <Arduino.h>
#include "idle.h"
#include "idle_cfg.h"
#include "hal/display_hal.h"
#include "hal/power_hal.h"

enum IdleState {
    STATE_AWAKE,
    STATE_FADING_OUT,
    STATE_ASLEEP,
    STATE_FADING_IN,
};

static IdleState state = STATE_AWAKE;
static uint32_t last_activity_ms = 0;
static uint32_t fade_started_ms  = 0;
static uint32_t fade_last_step_ms = 0;
static uint8_t  fade_from = DISPLAY_DEFAULT_BRIGHTNESS;
static uint8_t  fade_to   = 0;
static uint8_t  awake_brightness = DISPLAY_DEFAULT_BRIGHTNESS;  // user-set "full" level (brightness.cpp)
static uint8_t  awake_scale_pct = 100;                          // a page dims the awake level (idle_set_awake_scale)
static bool     sleep_on_usb = false;                           // sleep even on USB power (idle_set_sleep_on_usb)
static bool     remind_hold = false;                            // a reminder holds the panel on (idle_remind_begin)
static bool     remind_woke = false;                            // ...and woke it: nobody was here
static uint8_t  remind_presses = 0;                             // presses since it woke the panel

// What "awake" means: the user's level, scaled. Never 0 while the level isn't,
// so a dimmed panel stays on.
static uint8_t awake_level(void) {
    if (awake_scale_pct >= 100 || awake_brightness == 0) return awake_brightness;
    const uint32_t v = ((uint32_t)awake_brightness * awake_scale_pct + 50) / 100;
    return (uint8_t)(v ? v : 1);
}

static void apply_brightness(uint8_t b) {
    display_hal_set_brightness(b);
}

static void begin_fade(uint8_t to, uint32_t now) {
    fade_from = (to == 0) ? awake_level() : 0;
    fade_to   = to;
    fade_started_ms = now;
    fade_last_step_ms = now;
}

void idle_init(void) {
    state = STATE_AWAKE;
    last_activity_ms = millis();
    apply_brightness(awake_level());
}

void idle_set_awake_brightness(uint8_t level) {
    awake_brightness = level;
    // Apply now if fully awake so a button press is visible immediately;
    // during fades/sleep the next fade-in picks it up.
    if (state == STATE_AWAKE) apply_brightness(awake_level());
}

void idle_set_awake_scale(uint8_t pct) {
    if (pct > 100) pct = 100;
    if (pct == awake_scale_pct) return;
    awake_scale_pct = pct;
    if (state == STATE_AWAKE) apply_brightness(awake_level());
}

uint8_t idle_get_awake_scale(void) { return awake_scale_pct; }

void idle_set_sleep_on_usb(bool on) { sleep_on_usb = on; }

bool idle_get_sleep_on_usb(void) { return sleep_on_usb; }

void idle_remind_begin(void) {
    remind_hold = true;
    remind_presses = 0;
    if (state == STATE_ASLEEP || state == STATE_FADING_OUT) {
        begin_fade(awake_level(), millis());
        state = STATE_FADING_IN;
        remind_woke = true;
    } else {
        remind_woke = false;
    }
}

void idle_remind_end(bool acked) {
    if (!remind_hold) return;
    remind_hold = false;
    if (!remind_woke) return;
    remind_woke = false;
    const uint32_t now = millis();
    if (acked && remind_presses <= 1)
        last_activity_ms = now - IDLE_TIMEOUT_MS + IDLE_REMIND_ACK_MS;
    else if (!acked && remind_presses == 0)
        last_activity_ms = now - IDLE_TIMEOUT_MS;
}

void idle_note_activity(void) {
    if (remind_hold) remind_woke = false;   // you're here: the usual timeout from now
    last_activity_ms = millis();
    if (state == STATE_FADING_IN) return;
    if (state == STATE_AWAKE) return;
    // Asleep/fading-out shouldn't reach here in normal flow (callers gate via
    // idle_consume_wake_press first), but if it does: trigger a wake.
    begin_fade(awake_level(), last_activity_ms);
    state = STATE_FADING_IN;
}

bool idle_consume_wake_press(void) {
    if (state == STATE_ASLEEP || state == STATE_FADING_OUT) {
        uint32_t now = millis();
        last_activity_ms = now;
        begin_fade(awake_level(), now);
        state = STATE_FADING_IN;
        return true;
    }
    if (state == STATE_FADING_IN) {
        // Mid-wake — still swallow this press; the user shouldn't get a
        // half-wake half-action surprise.
        last_activity_ms = millis();
        return true;
    }
    if (remind_hold && remind_woke && remind_presses < 255) remind_presses++;
    last_activity_ms = millis();
    return false;
}

bool idle_is_asleep(void) {
    return state == STATE_ASLEEP || state == STATE_FADING_OUT;
}

void idle_tick(void) {
    uint32_t now = millis();

    // While on USB power (if configured), don't sleep — and wake from sleep
    // when power comes back. Treats USB-in as continuous activity.
    if (!IDLE_SLEEP_WHEN_CHARGING && !sleep_on_usb && power_hal_is_vbus_in()) {
        last_activity_ms = now;
        if (state == STATE_ASLEEP || state == STATE_FADING_OUT) {
            begin_fade(awake_level(), now);
            state = STATE_FADING_IN;
        }
    }

    switch (state) {
    case STATE_AWAKE:
        if (!remind_hold && now - last_activity_ms >= IDLE_TIMEOUT_MS) {
            begin_fade(0, now);
            state = STATE_FADING_OUT;
        }
        break;

    case STATE_FADING_OUT:
    case STATE_FADING_IN: {
        if (now - fade_last_step_ms < IDLE_FADE_STEP_MS) break;
        fade_last_step_ms = now;
        uint32_t dur = (state == STATE_FADING_OUT) ? IDLE_FADE_OUT_MS : IDLE_FADE_IN_MS;
        uint32_t elapsed = now - fade_started_ms;
        if (elapsed >= dur) {
            apply_brightness(fade_to);
            state = (state == STATE_FADING_OUT) ? STATE_ASLEEP : STATE_AWAKE;
        } else {
            // Linear interpolation fade_from -> fade_to over dur ms.
            int32_t span = (int32_t)fade_to - (int32_t)fade_from;
            int32_t b = (int32_t)fade_from + (span * (int32_t)elapsed) / (int32_t)dur;
            if (b < 0) b = 0;
            if (b > 255) b = 255;
            apply_brightness((uint8_t)b);
        }
        break;
    }

    case STATE_ASLEEP:
        break;
    }
}
