#pragma once
#include <lvgl.h>
#include "data.h"

// The room page: "Cola's room", a second page next to the usage meters. A
// full-screen scene (art in room_assets.h, from assets/cola_room/) with her
// standing on the rug, doing small things on her own (room_policy.h holds the
// rules). It is its own page: the usage meters, the director and cola's
// usage layer aren't changed by it and don't know it exists; while it's up
// it simply covers them. A swipe left on the meters brings it up, a swipe
// right on the room goes back.
//
// Memory: nothing is allocated until the page is first shown; then about
// 790 KB of PSRAM (the 480x480 RGB565 background, one frame's worth of
// decompressed indices, her sprite).
//
// Built only where the env sets -DCLAWD_COLA=1; everywhere else these are
// no-ops.

#if CLAWD_COLA

// parent: the screen. Call after director_init() so the page sits above
// everything the screen holds (only the screensaver face, on the top layer,
// is above it).
void room_create(lv_obj_t* parent);
void room_tick(void);
// Per parsed payload: the app's wall clock (`t`) sets the time of day.
void room_on_payload(const UsageData* d);
bool room_visible(void);
// QA, the `room <what>` serial command: on | off | auto | day | dusk | night |
// idle | game | drink | stretch | read | sleep | snack (hold that act until
// `room auto`) | poke [surprise | shy | angry] | status. False for an unknown
// word.
bool room_debug(const char* what);

#else

static inline void room_create(lv_obj_t*) {}
static inline void room_tick(void) {}
static inline void room_on_payload(const UsageData*) {}
static inline bool room_visible(void) { return false; }
static inline bool room_debug(const char*) { return false; }

#endif
