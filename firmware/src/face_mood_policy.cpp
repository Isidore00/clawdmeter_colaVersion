#include "face_mood_policy.h"
#include <string.h>

static const char* const NAMES[MOOD_COUNT] = {
    "none", "startled", "think", "work", "idea", "slow", "happy", "heart", "nervous", "asleep",
};

const char* face_mood_name(FaceMood m) { return (unsigned)m < MOOD_COUNT ? NAMES[m] : "?"; }

FaceMood face_mood_from_name(const char* name) {
    for (int i = 0; i < MOOD_COUNT; i++)
        if (strcmp(name, NAMES[i]) == 0) return (FaceMood)i;
    return MOOD_COUNT;
}

// The status words of the meters' line (ui.cpp anim_messages), by what they
// feel like. Anything not listed is a doing word.
struct VerbMood { const char* verb; FaceMood mood; };
static const VerbMood VERBS[] = {
    // thinking
    { "Elucidating", MOOD_THINK }, { "Perusing", MOOD_THINK }, { "Philosophising", MOOD_THINK },
    { "Pondering", MOOD_THINK }, { "Pontificating", MOOD_THINK }, { "Puzzling", MOOD_THINK },
    { "Cerebrating", MOOD_THINK }, { "Cogitating", MOOD_THINK }, { "Considering", MOOD_THINK },
    { "Contemplating", MOOD_THINK }, { "Deciphering", MOOD_THINK }, { "Deliberating", MOOD_THINK },
    { "Determining", MOOD_THINK }, { "Inferring", MOOD_THINK }, { "Meandering", MOOD_THINK },
    { "Moseying", MOOD_THINK }, { "Mulling", MOOD_THINK }, { "Musing", MOOD_THINK },
    { "Ruminating", MOOD_THINK }, { "Thinking", MOOD_THINK }, { "Wandering", MOOD_THINK },
    // ideas
    { "Enchanting", MOOD_IDEA }, { "Envisioning", MOOD_IDEA }, { "Booping", MOOD_IDEA },
    { "Flibbertigibbeting", MOOD_IDEA }, { "Clauding", MOOD_IDEA }, { "Coalescing", MOOD_IDEA },
    { "Combobulating", MOOD_IDEA }, { "Concocting", MOOD_IDEA }, { "Conjuring", MOOD_IDEA },
    { "Discombobulating", MOOD_IDEA }, { "Divining", MOOD_IDEA }, { "Frolicking", MOOD_IDEA },
    { "Germinating", MOOD_IDEA }, { "Hatching", MOOD_IDEA }, { "Honking", MOOD_IDEA },
    { "Ideating", MOOD_IDEA }, { "Imagining", MOOD_IDEA }, { "Jiving", MOOD_IDEA },
    { "Manifesting", MOOD_IDEA }, { "Noodling", MOOD_IDEA }, { "Scheming", MOOD_IDEA },
    { "Shimmying", MOOD_IDEA }, { "Sussing", MOOD_IDEA }, { "Unfurling", MOOD_IDEA },
    { "Vibing", MOOD_IDEA }, { "Wibbling", MOOD_IDEA }, { "Wizarding", MOOD_IDEA },
    // slow cooking
    { "Baking", MOOD_SLOW }, { "Brewing", MOOD_SLOW }, { "Puttering", MOOD_SLOW },
    { "Cooking", MOOD_SLOW }, { "Incubating", MOOD_SLOW }, { "Marinating", MOOD_SLOW },
    { "Percolating", MOOD_SLOW }, { "Simmering", MOOD_SLOW }, { "Stewing", MOOD_SLOW },
};

FaceMood face_mood_for_verb(const char* verb) {
    if (!verb) return MOOD_WORK;
    for (const VerbMood& v : VERBS)
        if (strcmp(verb, v.verb) == 0) return v.mood;
    return MOOD_WORK;
}

bool face_mood_before(uint32_t now, uint32_t until) { return (int32_t)(now - until) < 0; }

FaceMoodPick face_mood_pick(const FaceMoodInput* in) {
    FaceMoodPick p = { MOOD_NONE, false };
    if (in->sleepy) { p.mood = MOOD_ASLEEP; return p; }
    if (face_mood_before(in->now, in->reset_until)) { p.mood = MOOD_HEART; return p; }
    if (face_mood_before(in->now, in->done_until)) { p.mood = MOOD_HAPPY; return p; }
    p.sweat = in->near_limit;
    if (face_mood_before(in->now, in->prompt_until)) p.mood = MOOD_STARTLED;
    else if (in->working) p.mood = in->verb_mood == MOOD_NONE ? MOOD_WORK : in->verb_mood;
    else if (in->near_limit) p.mood = MOOD_NERVOUS;
    return p;
}
