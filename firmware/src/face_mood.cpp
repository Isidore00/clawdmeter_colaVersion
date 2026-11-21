#include "face_mood.h"

#if CLAWD_COLA

#include "face.h"
#include "face_mood_policy.h"
#include "idle.h"
#include "idle_cfg.h"
#include "signals.h"
#include "ui.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#define PROMPT_STARTLE_MS 1200
#define DONE_HAPPY_MS     3500
#define RESET_HEART_MS    3500
#define PROMPT_STALE_MS   (15UL * 60UL * 1000UL)   // a prompt with no stop for this long: Claude isn't working

static bool     prompted = false;       // a prompt came and no stop since
static bool     stopped = false;        // a stop came and no prompt since
static uint32_t prompted_ms = 0;
static uint32_t prompt_until = 0, done_until = 0, reset_until = 0;
static bool     near_limit = false;
static bool     caption_on = true;

static FaceMood forced = MOOD_COUNT;    // `clawd mood <name>`; MOOD_COUNT: automatic
static const char* forced_verb = NULL;

static const char* verb = NULL;         // the status word now
static uint32_t verb_until = 0;
static unsigned verb_idx = 0;
static char     last_caption[48] = "";

void face_mood_set_caption_enabled(bool on) { caption_on = on; }

void face_mood_on_payload(const UsageData* d, const SignalsUpdate* sig) {
    const uint32_t now = millis();
    if (sig->event_fired) {
        if (sig->event == 'p') {
            prompted = true; stopped = false; prompted_ms = now;
            prompt_until = now + PROMPT_STARTLE_MS;
        } else if (sig->event == 's') {
            prompted = false; stopped = true;
            done_until = now + DONE_HAPPY_MS;
        }
    }
    if (sig->session_reset) reset_until = now + RESET_HEART_MS;
    if (d->ok) near_limit = d->session_pct >= FACE_NEAR_LIMIT_PCT;
}

static void pick_verb(uint32_t now) {
    const unsigned n = ui_verb_count();
    if (!n) { verb = NULL; return; }
    unsigned i = (unsigned)random(n);
    if (i == verb_idx) i = (i + 1) % n;
    verb_idx = i;
    verb = ui_verb_at(i);
    verb_until = now + (uint32_t)random(4500, 7500);
}

static void set_caption(const char* text) {
    if (!caption_on) text = NULL;
    const char* t = text ? text : "";
    if (strcmp(t, last_caption) == 0) return;
    snprintf(last_caption, sizeof(last_caption), "%s", t);
    face_set_caption(text);
}

void face_mood_tick(void) {
    const uint32_t now = millis();
    // An expired stamp is moved up to now every tick, so it never looks
    // like a future time again once millis() is 2^31 ms (24.8 days) past it.
    if (!face_mood_before(now, reset_until)) reset_until = now;
    if (!face_mood_before(now, done_until)) done_until = now;
    if (!face_mood_before(now, prompt_until)) prompt_until = now;
    if (!face_visible() || face_moving() || face_drinking()) { last_caption[0] = '\0'; return; }

    const int activity = signals_activity();
    const bool prompt_fresh = prompted && (uint32_t)(now - prompted_ms) < PROMPT_STALE_MS;
    const bool working = !stopped && (prompt_fresh || activity >= 1);
    if (working && (!verb || face_mood_before(verb_until, now) || verb_until == 0)) pick_verb(now);

    FaceMoodInput in = {};
    in.now = now;
    in.sleepy = idle_quiet_ms() + FACE_SLEEP_LEAD_MS >= IDLE_TIMEOUT_MS;
    in.reset_until = reset_until;
    in.done_until = done_until;
    in.prompt_until = prompt_until;
    in.working = working;
    in.verb_mood = face_mood_for_verb(verb);
    in.near_limit = near_limit;
    FaceMoodPick p = face_mood_pick(&in);
    const char* word = verb;
    if (forced != MOOD_COUNT) { p.mood = forced; p.sweat = forced == MOOD_NERVOUS; word = forced_verb; }

    face_set_mood(p.mood, p.sweat);

    char buf[48];
    const char* cap = NULL;
    switch (p.mood) {
    case MOOD_ASLEEP: cap = "Zzz"; break;
    case MOOD_HAPPY:  cap = "All done"; break;
    case MOOD_HEART:  cap = "Fresh start"; break;
    case MOOD_THINK: case MOOD_WORK: case MOOD_IDEA: case MOOD_SLOW:
        if (word && (working || forced != MOOD_COUNT)) {
            snprintf(buf, sizeof(buf), "%s%.*s", word, 1 + (int)((now / 450) % 3), "...");
            cap = buf;
        }
        break;
    default: break;
    }
    set_caption(cap);
}

bool face_mood_debug(const char* what) {
    if (strcmp(what, "auto") == 0) { forced = MOOD_COUNT; forced_verb = NULL; return true; }
    if (strcmp(what, "status") == 0) {
        Serial.printf("{\"mood\":{\"face\":\"%s\",\"forced\":%s,\"prompted\":%s,\"stopped\":%s,"
                      "\"near_limit\":%s,\"activity\":%d,\"quiet_ms\":%lu,\"verb\":\"%s\",\"caption\":%s}}\n",
                      face_mood_name(face_current_mood()), forced != MOOD_COUNT ? "true" : "false",
                      prompted ? "true" : "false", stopped ? "true" : "false",
                      near_limit ? "true" : "false", signals_activity(), (unsigned long)idle_quiet_ms(),
                      verb ? verb : "", caption_on ? "true" : "false");
        return true;
    }
    const FaceMood m = face_mood_from_name(what);
    if (m == MOOD_COUNT) return false;
    forced = m;
    switch (m) {
    case MOOD_THINK: forced_verb = "Mulling"; break;
    case MOOD_WORK:  forced_verb = "Crunching"; break;
    case MOOD_IDEA:  forced_verb = "Conjuring"; break;
    case MOOD_SLOW:  forced_verb = "Simmering"; break;
    default:         forced_verb = NULL; break;
    }
    return face_visible();
}

#endif  // CLAWD_COLA
