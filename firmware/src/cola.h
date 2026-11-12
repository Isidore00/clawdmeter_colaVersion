#pragma once
#include <lvgl.h>
#include "data.h"
#include "signals.h"

// The cola layer: a second, independent sprite on the meters with its own
// art (cola_assets.h, from assets/cola/) and its own states (cola_policy.h).
// It only reads what the firmware already knows (the link, usage, hook
// activity, alerts, the reset) and never commands Clawd: the director, the
// corner Clawd, the performer and the face don't know it exists. It's created
// before them, so Clawd always walks in front of it and the face covers it.
//
// Built only where the env sets -DCLAWD_COLA=1 (the art is about 800 KB of
// flash and its frame buffer needs PSRAM); everywhere else these are no-ops.

#if CLAWD_COLA

// right / bottom: the screen edges of the box its lower right corner sits in.
void cola_create(lv_obj_t* parent, int right, int bottom);
void cola_tick(void);
// Per parsed payload, after signals_ingest() and after the ding was asked for
// (the ding goes first; cola's own sounds wait for it or are skipped).
void cola_on_payload(const UsageData* d, const SignalsUpdate* sig);
// QA, the `cola <state>` serial command: hold a state (idle | prompt |
// working | reply | low | empty | reset) until `cola auto`. False for an
// unknown name.
bool cola_debug(const char* state);

#else

static inline void cola_create(lv_obj_t*, int, int) {}
static inline void cola_tick(void) {}
static inline void cola_on_payload(const UsageData*, const SignalsUpdate*) {}
static inline bool cola_debug(const char*) { return false; }

#endif
