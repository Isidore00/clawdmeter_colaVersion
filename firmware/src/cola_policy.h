#pragma once
#include <stdint.h>

// Which state the cola layer (cola.h) shows. Pure rules, no Arduino or LVGL:
// native-tested in test/test_cola_policy.
//
// The order here is the order of the art in cola_assets.h
// (tools/png_to_cola.js: STATES); keep the two in step.
enum ColaState {
    COLA_IDLE,       // connected, nothing going on (also: not connected)
    COLA_PROMPT,     // a prompt was just sent; plays once
    COLA_WORKING,    // Claude is working (hook activity 1-3)
    COLA_REPLY,      // Claude needs a reply (an alert is up)
    COLA_LOW,        // the limit is nearly used up
    COLA_EMPTY,      // the limit is used up
    COLA_RESET,      // the limit was just refilled; plays once
    COLA_STATE_COUNT
};

#define COLA_LOW_PCT    80.0f
#define COLA_EMPTY_PCT  99.5f    // shown as 100 % on the meters

struct ColaInput {
    bool  connected;        // the link to the app is up
    bool  have_pct;         // a usage reading has arrived
    float pct;              // the main meter, 0-100
    bool  limited;          // the app says a window or the spend cap is used up
    int   activity;         // hook activity 0-3; < 0 unknown
    bool  alert;            // an alert is up
    bool  prompt_playing;   // the prompt animation hasn't finished
    bool  reset_playing;    // the reset animation hasn't finished
};

// The limit is used up: the meter reads 100 %, or the app says so (a weekly
// limit can run out while the session meter still has room).
bool cola_empty(bool have_pct, float pct, bool limited);

// When several hold at once, highest first: reset, reply, empty, prompt, low,
// working, idle. Not connected is always idle.
ColaState cola_pick(const ColaInput* in);

// Plays through once, then gives way to whatever holds next.
bool cola_oneshot(ColaState s);

// "idle", "prompt", ... ; NULL when out of range.
const char* cola_state_name(ColaState s);
// COLA_STATE_COUNT when the name is unknown.
ColaState cola_state_from_name(const char* name);
