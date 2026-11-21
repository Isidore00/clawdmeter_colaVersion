#pragma once
#include <stdbool.h>
#include <stdint.h>

// The face's moods while Claude Code works (cola builds): pure rules, no
// Arduino or LVGL, native-tested in test_face_mood_policy. face_mood.cpp feeds
// them the situation and face.cpp draws the result.
//
//   STARTLED  a prompt just arrived: the eyes widen once
//   THINK     thinking words (Mulling, Pondering...): glances up and to the side
//   WORK      doing words (Crunching, Tinkering...): the eyes squint "> <" in rhythm
//   IDEA      idea words (Conjuring, Ideating...): a blink, then "^ ^"
//   SLOW      slow-cook words (Simmering, Stewing...): half-closed, drowsy
//   HAPPY     a turn finished: "^ ^"
//   HEART     a fresh 5-hour window: heart eyes
//   NERVOUS   usage near the limit while nothing else is going on: small eyes
//   ASLEEP    nobody has touched the board for a long while: shut eyes, "Zzz"
// A sweat drop rides along near the limit (except asleep, happy and heart).

enum FaceMood : uint8_t {
    MOOD_NONE,        // the usual resting blinks and glances
    MOOD_STARTLED,
    MOOD_THINK,
    MOOD_WORK,
    MOOD_IDEA,
    MOOD_SLOW,
    MOOD_HAPPY,
    MOOD_HEART,
    MOOD_NERVOUS,
    MOOD_ASLEEP,
    MOOD_COUNT
};

const char* face_mood_name(FaceMood m);
// MOOD_COUNT for an unknown name; "none" is MOOD_NONE.
FaceMood    face_mood_from_name(const char* name);

// The mood a status word (the same words as the meters' status line) asks
// for; a word that isn't listed is a doing word (WORK).
FaceMood    face_mood_for_verb(const char* verb);

// Usage this high (percent) is "near the limit".
#define FACE_NEAR_LIMIT_PCT 90.0f
// The face falls asleep this long before the panel's own sleep.
#define FACE_SLEEP_LEAD_MS  (2UL * 60UL * 1000UL)

struct FaceMoodInput {
    uint32_t now;
    bool     sleepy;        // quiet long enough to nod off (the caller knows the panel's timeout)
    uint32_t reset_until;   // a fresh window: heart eyes until then (now < until)
    uint32_t done_until;    // a turn finished: happy until then
    uint32_t prompt_until;  // a prompt arrived: startled until then
    bool     working;       // Claude is working on a turn
    FaceMood verb_mood;     // what the current word asks for (when working)
    bool     near_limit;
};

struct FaceMoodPick {
    FaceMood mood;
    bool     sweat;
};

FaceMoodPick face_mood_pick(const FaceMoodInput* in);

// True while now is before until (wrap-safe).
bool face_mood_before(uint32_t now, uint32_t until);
