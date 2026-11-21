#pragma once
#include <stdbool.h>
#include "data.h"
#include "signals.h"

// Moods for the orange face while Claude Code works (cola builds): decides
// which mood face_mood_policy.h picks from the situation (a prompt arrived, a
// turn is running and its status word, a turn finished, a fresh window, usage
// near the limit, a long quiet) and hands it to face.cpp, with the status word
// as a small caption. Only runs while the face is up and not drinking.
//
// Built only where the env sets -DCLAWD_COLA=1; everywhere else these are
// no-ops.

#if CLAWD_COLA

void face_mood_tick(void);
void face_mood_on_payload(const UsageData* d, const SignalsUpdate* sig);
// The caption under the face (the status word), on or off.
void face_mood_set_caption_enabled(bool on);
// QA, `clawd mood <name|auto|status>`: hold a mood (none, startled, think,
// work, idea, slow, happy, heart, nervous, asleep) until `auto`. False for an
// unknown word.
bool face_mood_debug(const char* what);

#else

static inline void face_mood_tick(void) {}
static inline void face_mood_on_payload(const UsageData*, const SignalsUpdate*) {}
static inline void face_mood_set_caption_enabled(bool) {}
static inline bool face_mood_debug(const char*) { return false; }

#endif
