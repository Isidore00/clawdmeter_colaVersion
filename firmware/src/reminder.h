#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "data.h"

// Reminders: the programs in reminder_policy.h, played on whatever screen is
// up. A reminder rings the ding, then her voice line (the room voice switch
// mutes the line, never the ding), wakes the panel without counting as you
// being here (idle_remind_begin), and shows:
//   room page  she drinks (room_remind)
//   otherwise  the face comes up and drinks through a straw (director_remind)
// A tap or a button answers it (reminder_ack): it ends, and a panel it woke
// goes dark again a moment later. Unanswered, it ends after REMIND_SHOW_MS
// and a panel it woke goes dark at once. No wall clock from the app yet (`t`):
// no reminders.
//
// Built only where the env sets -DCLAWD_COLA=1; everywhere else these are
// no-ops.

#define REMIND_SHOW_MS (60UL * 1000UL)   // how long a reminder stays up unanswered

#if CLAWD_COLA

void reminder_tick(void);
void reminder_on_payload(const UsageData* d);   // the app's wall clock (`t`)
bool reminder_active(void);
void reminder_ack(void);                        // a tap or a button answered it
// QA, the `remind <what>` serial command: <program name> (e.g. `water`: that
// reminder now) | stop | status. False for an unknown word.
bool reminder_debug(const char* what);

#else

static inline void reminder_tick(void) {}
static inline void reminder_on_payload(const UsageData*) {}
static inline bool reminder_active(void) { return false; }
static inline void reminder_ack(void) {}
static inline bool reminder_debug(const char*) { return false; }

#endif
