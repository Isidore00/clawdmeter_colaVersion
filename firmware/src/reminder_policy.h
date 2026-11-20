#pragma once
#include <stdbool.h>
#include <stdint.h>

// Reminder programs: small nags on a wall-clock schedule (drink water first;
// stand up and rest your eyes can follow). Pure rules, no Arduino or LVGL, so
// test_reminder_policy runs them on the host. reminder.cpp plays them.
//
// Every time here is local wall-clock time: the epoch the app sends as `t`
// (seconds, already shifted to the computer's time zone), so day and minute
// come straight from it. A program fires on its period's boundaries, counted
// from midnight (period 15: :00 :15 :30 :45), on the days in `days`, from
// `start_min` up to but not including `end_min`.

enum ReminderKind : uint8_t {
    REMIND_WATER,   // the face drinks through a straw; on the room page she drinks
    REMIND_KIND_COUNT,
};

struct ReminderProgram {
    const char*  name;        // serial name, `remind <name>`
    ReminderKind kind;
    uint16_t     period_min;  // minutes between reminders, > 0
    uint16_t     start_min;   // first minute of the day it may fire (incl.)
    uint16_t     end_min;     // last minute of the day, excl.
    uint8_t      days;        // bit 0 Sunday .. bit 6 Saturday
};

#define REMIND_WEEKDAYS 0x3E  // Monday to Friday

// The programs that run, in order. Fixed for now (no app settings yet).
extern const ReminderProgram REMINDER_PROGRAMS[];
extern const int REMINDER_PROGRAM_COUNT;

// 0 Sunday .. 6 Saturday, and the minute of the day, for a local epoch.
int clock_weekday(long epoch_s);
int clock_minute_of_day(long epoch_s);

// Which period slot a local epoch falls in (minutes since 1970 / period).
// A program is due once each time this changes, if the new slot starts
// inside its window.
long reminder_slot(const ReminderProgram* p, long epoch_s);
bool reminder_slot_in_window(const ReminderProgram* p, long slot);

// The local epoch of the next slot start at or after epoch_s that is inside
// the window (looks up to 8 days ahead), or -1 if there is none.
long reminder_next_epoch(const ReminderProgram* p, long epoch_s);

// The slot tracker: feed it the clock; it says when a program is due. The
// first reading only sets the slot (a clock that arrives at 10:07 waits for
// 10:15). A clock that jumps back (a new `t` from the app) is just a new
// reading.
struct ReminderClock {
    long slot;     // last slot seen, -1 before the first reading
};
void reminder_clock_init(ReminderClock* c);
bool reminder_clock_due(ReminderClock* c, const ReminderProgram* p, long epoch_s);

// How full the glass is, in percent, ms into a reminder that lasts total_ms:
// from 100 down to 15 (a few sips are left when it times out).
int reminder_water_pct(uint32_t ms, uint32_t total_ms);
