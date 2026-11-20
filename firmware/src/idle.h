#pragma once
#include <stdbool.h>

void idle_init(void);
void idle_tick(void);
void idle_note_activity(void);

// Set the "awake" brightness target (0..255). idle owns display brightness
// (it fades between this and 0), so user brightness control routes through
// here. Applied immediately if the screen is currently fully awake; otherwise
// picked up by the next fade-in. See brightness.{h,cpp}.
void idle_set_awake_brightness(uint8_t level);

// A page dims the awake level to pct percent of what the user chose (0..100;
// 100 = as chosen). Applied at once while awake, and by every fade-in. The
// room page uses it to dim by time of day (burn-in, room.cpp).
void idle_set_awake_scale(uint8_t pct);
uint8_t idle_get_awake_scale(void);

// While on, the 30 minute auto-sleep also runs on USB power
// (IDLE_SLEEP_WHEN_CHARGING stays the default when off), and plugging USB in
// no longer wakes the panel. A button or touch still wakes it.
void idle_set_sleep_on_usb(bool on);
bool idle_get_sleep_on_usb(void);

// A reminder (reminder.h) holds the panel on while it runs, without counting
// as you being here: a dark panel fades in, and the idle timeout doesn't run
// out under it. idle_remind_end() lets go. If the reminder woke the panel and
// nothing but one press (the tap that answered it, `acked`) came since, it
// goes dark again: IDLE_REMIND_ACK_MS after an answer, at once if nobody
// answered. Any other activity meanwhile (another press, a prompt) keeps it
// awake the usual way.
void idle_remind_begin(void);
void idle_remind_end(bool acked);

// Returns true if this press was consumed as a wake-up (caller MUST skip the
// button's normal action). Returns false when already awake — also notes the
// activity, so callers don't need a separate idle_note_activity() call.
bool idle_consume_wake_press(void);

// Touch should NOT count as activity (avoids accidental wakes from pets,
// sleeves, etc.). Callers use this to silently drop touch events while the
// panel is dark.
bool idle_is_asleep(void);
