#include "reminder.h"

#if CLAWD_COLA

#include "reminder_policy.h"
#include "clawd_director.h"
#include "room.h"
#include "idle.h"
#include "hal/sound_hal.h"
#include <Arduino.h>
#include <string.h>

// How long her line may wait for the ding to finish before it's dropped.
#define REMIND_VOICE_WAIT_MS 4000

static bool          have_clock = false;
static long          clock_epoch = 0;
static uint32_t      clock_ms = 0;
static ReminderClock clocks[8];

static bool     active = false;
static int      active_prog = -1;
static uint32_t started_ms = 0;
static bool     voice_pending = false;
static bool     on_room = false;          // which screen is showing it

static long clock_now_s(void) {
    return clock_epoch + (long)((uint32_t)(millis() - clock_ms) / 1000);
}

// Shows it on the screen that's up now, or takes it off. One kind so far
// (REMIND_WATER: drinking, on either screen); a new kind picks its own act here.
static void show(bool on) {
    if (on) {
        on_room = room_visible();
        if (on_room) room_remind(true);
        else         director_remind(true, REMIND_SHOW_MS);
    } else {
        room_remind(false);
        director_remind(false, 0);
    }
}

static void start(int prog) {
    if (active) show(false);
    active = true;
    active_prog = prog;
    started_ms = millis();
    voice_pending = true;
    idle_remind_begin();
    sound_hal_chime();
    show(true);
    Serial.printf("[%lu] reminder: %s\n", (unsigned long)started_ms, REMINDER_PROGRAMS[prog].name);
}

static void end(bool acked) {
    if (!active) return;
    active = false;
    voice_pending = false;
    show(false);
    idle_remind_end(acked);
    Serial.printf("[%lu] reminder: %s %s\n", (unsigned long)millis(), REMINDER_PROGRAMS[active_prog].name,
                  acked ? "answered" : "timed out");
}

void reminder_on_payload(const UsageData* d) {
    if (d->clock_epoch <= 0) return;
    have_clock = true;
    clock_epoch = d->clock_epoch;
    clock_ms = millis();
}

bool reminder_active(void) { return active; }

void reminder_ack(void) { end(true); }

void reminder_tick(void) {
    static bool inited = false;
    if (!inited) {
        inited = true;
        for (auto& c : clocks) reminder_clock_init(&c);
    }
    const uint32_t now = millis();
    if (active) {
        // Her line, once the ding has let go of the speaker.
        if (voice_pending) {
            if (room_say_reminder()) voice_pending = false;
            else if ((uint32_t)(now - started_ms) >= REMIND_VOICE_WAIT_MS) voice_pending = false;
        }
        // The screen changed under it (a swipe): it follows.
        if (room_visible() != on_room) {
            show(false);
            show(true);
        }
        if ((uint32_t)(now - started_ms) >= REMIND_SHOW_MS) end(false);
    }
    if (!have_clock) return;
    const long t = clock_now_s();
    for (int i = 0; i < REMINDER_PROGRAM_COUNT && i < (int)(sizeof(clocks) / sizeof(clocks[0])); i++)
        if (reminder_clock_due(&clocks[i], &REMINDER_PROGRAMS[i], t)) start(i);
}

bool reminder_debug(const char* what) {
    if (strcmp(what, "stop") == 0) { end(true); return true; }
    if (strcmp(what, "status") == 0) {
        const long t = have_clock ? clock_now_s() : 0;
        Serial.printf("{\"remind\":{\"clock\":%s,\"weekday\":%d,\"minute\":%d,\"active\":%s",
                      have_clock ? "true" : "false", have_clock ? clock_weekday(t) : -1,
                      have_clock ? clock_minute_of_day(t) : -1, active ? "true" : "false");
        if (active) Serial.printf(",\"program\":\"%s\",\"ms\":%lu", REMINDER_PROGRAMS[active_prog].name,
                                  (unsigned long)(millis() - started_ms));
        Serial.print(",\"next\":[");
        for (int i = 0; i < REMINDER_PROGRAM_COUNT; i++) {
            const long n = have_clock ? reminder_next_epoch(&REMINDER_PROGRAMS[i], t + 1) : -1;
            Serial.printf("%s{\"name\":\"%s\",\"in_s\":%ld,\"weekday\":%d,\"minute\":%d}", i ? "," : "",
                          REMINDER_PROGRAMS[i].name, n >= 0 ? n - t : -1L,
                          n >= 0 ? clock_weekday(n) : -1, n >= 0 ? clock_minute_of_day(n) : -1);
        }
        Serial.println("]}}");
        return true;
    }
    for (int i = 0; i < REMINDER_PROGRAM_COUNT; i++) {
        if (strcmp(what, REMINDER_PROGRAMS[i].name) == 0) { start(i); return true; }
    }
    return false;
}

#endif  // CLAWD_COLA
