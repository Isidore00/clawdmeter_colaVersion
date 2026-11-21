#include "reminder_policy.h"

const ReminderProgram REMINDER_PROGRAMS[] = {
    // Drink water: every 15 min, every day, 08:00 to 20:00 (the last at 19:45).
    { "water", REMIND_WATER, 15, 8 * 60, 20 * 60, REMIND_EVERYDAY },
};
const int REMINDER_PROGRAM_COUNT = (int)(sizeof(REMINDER_PROGRAMS) / sizeof(REMINDER_PROGRAMS[0]));

static long floor_div(long a, long b) {
    long q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

int clock_weekday(long epoch_s) {
    // 1970-01-01 was a Thursday (4).
    const long days = floor_div(epoch_s, 86400L);
    long w = (days + 4) % 7;
    if (w < 0) w += 7;
    return (int)w;
}

int clock_minute_of_day(long epoch_s) {
    long s = epoch_s % 86400L;
    if (s < 0) s += 86400L;
    return (int)(s / 60);
}

long reminder_slot(const ReminderProgram* p, long epoch_s) {
    const long period = p->period_min ? p->period_min : 1;
    return floor_div(floor_div(epoch_s, 60L), period);
}

bool reminder_slot_in_window(const ReminderProgram* p, long slot) {
    const long period = p->period_min ? p->period_min : 1;
    const long start_s = slot * period * 60L;
    if (!(p->days & (1u << clock_weekday(start_s)))) return false;
    const int m = clock_minute_of_day(start_s);
    return m >= p->start_min && m < p->end_min;
}

long reminder_next_epoch(const ReminderProgram* p, long epoch_s) {
    const long period = p->period_min ? p->period_min : 1;
    long slot = reminder_slot(p, epoch_s);
    if (slot * period * 60L < epoch_s) slot++;       // the current slot has already started
    const long limit = slot + (8L * 24 * 60) / period + 1;
    for (; slot <= limit; slot++)
        if (reminder_slot_in_window(p, slot)) return slot * period * 60L;
    return -1;
}

void reminder_clock_init(ReminderClock* c) { c->slot = -1; c->fired = -1; }

bool reminder_clock_due(ReminderClock* c, const ReminderProgram* p, long epoch_s) {
    const long slot = reminder_slot(p, epoch_s);
    if (c->slot < 0) { c->slot = slot; return false; }
    if (slot == c->slot) return false;
    const bool forward = slot > c->slot;
    c->slot = slot;
    if (!forward || slot == c->fired || !reminder_slot_in_window(p, slot)) return false;
    c->fired = slot;
    return true;
}

int reminder_water_pct(uint32_t ms, uint32_t total_ms) {
    if (total_ms == 0 || ms >= total_ms) return 15;
    return 100 - (int)((uint64_t)85 * ms / total_ms);
}
