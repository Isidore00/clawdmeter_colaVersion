#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "face_mood_policy.h"

// The #198 face: Clawd up close, resting on the glass. Clawd-orange full
// field, two tall dark eyes; a blink is the solid-cell > < squint; on a glance
// the far eye travels 1.5x. A field on lv_layer_top() (field.h) with the
// eyes as small LVGL rectangles on it — no pixel buffer — so it runs on every
// board, PSRAM or not. Rising and dropping redraw only the rows the field
// gains or loses plus the eyes, not the whole face, every frame.
//
// It's Clawd come up to the glass: the screensaver, and the peek when you
// tap the meters. It rises from behind the bottom bezel and drops back below it,
// straight up and down with a flat top edge, arms on the rise, squash and
// stretch and follow-through (face_motion.h). Owned and sequenced by the
// director.

void face_init(void);
void face_tick(void);

void face_rise(void);          // up from the bottom bezel, overshoot, settle
void face_rise_toward(int x);  // the same, curious about screen x (a peek): resting glances go back there
void face_drop(void);          // anticipation hop, then down behind the bezel; hidden when gone
void face_hide_now(void);      // no animation (mode switch / reset only)

bool face_visible(void);       // any part of it on screen
bool face_moving(void);        // rising (until settled) or dropping
bool face_busy(void);          // moving, blinking or glancing

void face_blink(bool twice);   // the noticing beat
void face_set_resting(bool on);// irregular blinks + glances while up
void face_apply_theme(void);   // pick up a new palette (TH_CLAWD, TH_EYE)
// Look toward screen x (a tap): the eyes glance proportionally (up to 1.5x
// the resting glance, far eye 1.5x on top), hold for hold_ms, then come back
// to you. face_busy() until they're back.
void face_look_at_x(int x, uint32_t hold_ms);
// A water reminder: eyes shut, a glass with a straw in front of him, the
// water going down over `ms` (reminder_water_pct). Only while the face is up
// and still; off (or a drop, a rise) puts the glass away and opens the eyes.
// A blink is skipped while he drinks.
void face_drink(bool on, uint32_t ms);
bool face_drinking(void);
// The voice switch, answered with a sign: he holds up a little board with a
// speaker on it (waves: sound on; a red slash: muted) for a moment and puts it
// down. If the face isn't up and still, the sign waits a few seconds for it.
void face_show_sign(bool sound_on);
// A mood while Claude Code works (face_mood_policy.h; cola builds): the eyes
// glance, squint, droop or shut by the clock, with a sweat drop when `sweat`.
// MOOD_NONE gives the usual resting blinks back. Only while the face is up and
// still (not drinking); a rise, a drop or a drink ends it.
void     face_set_mood(FaceMood m, bool sweat);
FaceMood face_current_mood(void);
// A small caption under the face (the status word); NULL or "" hides it.
void face_set_caption(const char* text);
// QA, `clawd face <happy|heart|wink|surprise|sleepy>`: one of the rarer
// expressions now. False if the face isn't up and still, or the word is unknown.
bool face_debug(const char* what);
