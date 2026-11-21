#include "reminder.h"

#if CLAWD_COLA

#include "reminder_policy.h"
#include "clawd_director.h"
#include "room.h"
#include "idle.h"
#include "hal/sound_hal.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

// How long her line may wait for the ding to finish before it's dropped.
#define REMIND_VOICE_WAIT_MS 4000

static bool          have_clock = false;
static long          clock_epoch = 0;
static uint32_t      clock_ms = 0;
static ReminderClock clocks[8];
static bool          enabled = true;       // the switch (NVS clawdmeter/remind_on, on by default)

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
    if (active) show(false);              // one up already: this one takes over, the panel hold stays
    else        idle_remind_begin();
    active = true;
    active_prog = prog;
    started_ms = millis();
    voice_pending = true;
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
    // Each payload's `t` is whole seconds, sent a moment before it lands: a
    // step back of a second or two is that lag, not a new time. Keep ours.
    if (have_clock) {
        const long back = clock_now_s() - d->clock_epoch;
        if (back > 0 && back <= 5) return;
    }
    have_clock = true;
    clock_epoch = d->clock_epoch;
    clock_ms = millis();
}

bool reminder_active(void) { return active; }

// The switch is read from NVS the first time anything needs it, so a serial
// `remind on` in the first loop can't be lost to a later load.
static void load_enabled(void) {
    static bool loaded = false;
    if (loaded) return;
    loaded = true;
    Preferences prefs;
    prefs.begin("clawdmeter", true);
    enabled = prefs.getBool("remind_on", true);
    prefs.end();
}

bool reminder_enabled(void) { load_enabled(); return enabled; }

void reminder_set_enabled(bool on) {
    load_enabled();
    if (on == enabled) return;
    enabled = on;
    if (!on) end(true);                   // one up now goes away like an answered one
    Preferences prefs;
    prefs.begin("clawdmeter", false);
    prefs.putBool("remind_on", enabled);
    prefs.end();
    Serial.printf("{\"remind_on\":%s}\n", enabled ? "true" : "false");
}

void reminder_ack(void) { end(true); }

void reminder_tick(void) {
    static bool inited = false;
    if (!inited) {
        inited = true;
        for (auto& c : clocks) reminder_clock_init(&c);
    }
    load_enabled();
    const uint32_t now = millis();
    if (active && (uint32_t)(now - started_ms) >= REMIND_SHOW_MS) end(false);
    if (active) {
        // Her line, once the ding has let go of the speaker.
        if (voice_pending) {
            if (room_say_reminder()) voice_pending = false;
            else if ((uint32_t)(now - started_ms) >= REMIND_VOICE_WAIT_MS) voice_pending = false;
        }
        // The screen changed under it (a swipe): it follows.
        if (room_visible() != on_room) {
            show(false);
            on_room = room_visible();
            if (on_room) room_remind(true);
            else director_remind(true, REMIND_SHOW_MS - (now - started_ms));   // the glass drains over what's left
        }
    }
    if (!have_clock) return;
    const long t = clock_now_s();
    for (int i = 0; i < REMINDER_PROGRAM_COUNT && i < (int)(sizeof(clocks) / sizeof(clocks[0])); i++)
        // Off: the clocks keep running so turning it on waits for the next
        // boundary instead of firing for one that already passed.
        if (reminder_clock_due(&clocks[i], &REMINDER_PROGRAMS[i], t) && enabled) start(i);
}

bool reminder_debug(const char* what) {
    if (strcmp(what, "stop") == 0) { end(true); return true; }
    if (strcmp(what, "on") == 0)  { reminder_set_enabled(true);  return true; }
    if (strcmp(what, "off") == 0) { reminder_set_enabled(false); return true; }
    if (strcmp(what, "status") == 0) {
        load_enabled();
        const long t = have_clock ? clock_now_s() : 0;
        Serial.printf("{\"remind\":{\"enabled\":%s,\"clock\":%s,\"weekday\":%d,\"minute\":%d,\"active\":%s",
                      enabled ? "true" : "false", have_clock ? "true" : "false", have_clock ? clock_weekday(t) : -1,
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
