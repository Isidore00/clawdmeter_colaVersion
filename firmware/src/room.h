#pragma once
#include <lvgl.h>
#include "data.h"

// The room page: "Cola's room", a second page next to the usage meters. A
// full-screen scene (art in room_assets.h, from assets/cola_room/) with her
// standing on the rug, doing small things on her own (room_policy.h holds the
// rules). It is its own page: the usage meters and cola's usage layer don't
// know it exists; while it's up it simply covers them, and the director is
// only told to keep the screensaver face away (director_hold_screensaver).
// Touch: a tap on her is a poke (1st within 10 s surprise, 2nd and 3rd shy,
// later ones angry); a press held ~0.6 s on her head (top 30%), moved under
// 20 px, is a head pat. A press held about a second on the floor or walls
// (anywhere but her), or a tap on the speaker badge at the top centre, turns her
// lines on or off (kept in NVS; `room voice on|off` does the same). A swipe left on the meters brings it up, a swipe right on the room goes
// back.
//
// Memory: nothing is allocated until the page is first shown; then about
// 790 KB of PSRAM (the 480x480 RGB565 background, one frame's worth of
// decompressed indices, her sprite).
//
// Built only where the env sets -DCLAWD_COLA=1; everywhere else these are
// no-ops.

#if CLAWD_COLA

// parent: the screen. Call after director_init() so the page sits above
// everything the screen holds. (The face lives on the top layer, above it:
// the screensaver is held off while the room is up, an alert's face still
// shows.)
void room_create(lv_obj_t* parent);
void room_tick(void);
// Per parsed payload: the app's wall clock (`t`) sets the time of day.
void room_on_payload(const UsageData* d);
bool room_visible(void);
// A reminder (reminder.h) on the page: she drinks until it's off, and a tap
// answers it instead of poking her.
void room_remind(bool on);
// The reminder's line (her drink line), wherever the page is; false while the
// ding still has the speaker (try again). True when there's nothing to play:
// her lines are off, or this build has none.
bool room_say_reminder(void);
// Her lines' switch (NVS clawdmeter/room_voice), shared with the face's
// long press (main.cpp): it mutes her, never the ding.
bool room_voice_on(void);
void room_voice_set(bool on);
// QA, the `room <what>` serial command: on | off | auto | day | dusk | night |
// idle | game | drink | stretch | read | sleep | snack (hold that act until
// `room auto`) | poke [surprise | shy | angry | pat] (a bare poke counts like
// a touch) | pat | say <poke_surprise | poke_shy | poke_angry | love |
// iloveyou | morning | night | snack | drink> | voice on | voice off | status | burnin (the panel
// brightness scale, tone, USB-sleep permission and drift offset). False for an unknown
// word.
bool room_debug(const char* what);

#else

static inline void room_create(lv_obj_t*) {}
static inline void room_tick(void) {}
static inline void room_on_payload(const UsageData*) {}
static inline bool room_visible(void) { return false; }
static inline void room_remind(bool) {}
static inline bool room_say_reminder(void) { return true; }
static inline bool room_voice_on(void) { return false; }
static inline void room_voice_set(bool) {}
static inline bool room_debug(const char*) { return false; }

#endif
