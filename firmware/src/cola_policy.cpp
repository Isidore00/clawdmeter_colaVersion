#include "cola_policy.h"
#include <string.h>

static const char* const NAMES[COLA_STATE_COUNT] = {
    "idle", "prompt", "working", "reply", "low", "empty", "reset",
};

bool cola_empty(bool have_pct, float pct, bool limited) {
    return limited || (have_pct && pct >= COLA_EMPTY_PCT);
}

ColaState cola_pick(const ColaInput* in) {
    if (!in->connected) return COLA_IDLE;
    if (in->reset_playing) return COLA_RESET;
    if (in->alert) return COLA_REPLY;
    if (cola_empty(in->have_pct, in->pct, in->limited)) return COLA_EMPTY;
    if (in->prompt_playing) return COLA_PROMPT;
    if (in->have_pct && in->pct >= COLA_LOW_PCT) return COLA_LOW;
    if (in->activity >= 1) return COLA_WORKING;
    return COLA_IDLE;
}

bool cola_oneshot(ColaState s) { return s == COLA_PROMPT || s == COLA_RESET; }

const char* cola_state_name(ColaState s) {
    return (s >= 0 && s < COLA_STATE_COUNT) ? NAMES[s] : NULL;
}

ColaState cola_state_from_name(const char* name) {
    if (name)
        for (int i = 0; i < COLA_STATE_COUNT; i++)
            if (strcmp(name, NAMES[i]) == 0) return (ColaState)i;
    return COLA_STATE_COUNT;
}
