#include "room_policy.h"
#include <string.h>

static const uint8_t FRAMES[RA_COUNT] = {
    2,  // idle
    3,  // game
    3,  // drink
    3,  // stretch
    3,  // read   (patch variants)
    3,  // sleep  (patch variants)
    2,  // surprise
    2,  // shy
    2,  // angry
    3,  // snack
};

static const char* const ACT_NAMES[RA_COUNT] = {
    "idle", "game", "drink", "stretch", "read", "sleep", "surprise", "shy", "angry", "snack",
};

static const char* const TONE_NAMES[TONE_COUNT] = { "day", "dusk", "night" };

uint8_t room_act_frames(RoomAct a) { return (unsigned)a < RA_COUNT ? FRAMES[a] : 0; }
bool room_act_is_scene(RoomAct a) { return a == RA_READ || a == RA_SLEEP; }
bool room_act_is_reaction(RoomAct a) { return a == RA_SURPRISE || a == RA_SHY || a == RA_ANGRY; }
const char* room_act_name(RoomAct a) { return (unsigned)a < RA_COUNT ? ACT_NAMES[a] : NULL; }
RoomAct room_act_from_name(const char* n) {
    for (int i = 0; i < RA_COUNT; i++) if (strcmp(n, ACT_NAMES[i]) == 0) return (RoomAct)i;
    return RA_COUNT;
}
const char* room_tone_name(RoomTone t) { return (unsigned)t < TONE_COUNT ? TONE_NAMES[t] : NULL; }
RoomTone room_tone_from_name(const char* n) {
    for (int i = 0; i < TONE_COUNT; i++) if (strcmp(n, TONE_NAMES[i]) == 0) return (RoomTone)i;
    return TONE_COUNT;
}

RoomTone room_tone_for_minute(int m) {
    if (m < 0) m = 0;
    m %= 1440;
    if (m >= 19 * 60 + 30 || m < 5 * 60 + 30) return TONE_NIGHT;
    if (m >= 17 * 60 || m < 7 * 60) return TONE_DUSK;
    return TONE_DAY;
}

// Per-channel scale out of 256: red, green, blue.
static const uint16_t TINT[TONE_COUNT][3] = {
    { 256, 256, 256 },   // day: as drawn
    { 256, 196, 150 },   // dusk: warm
    {  80, 100, 170 },   // night: dim and blue
};

uint16_t room_tint565(uint16_t c, RoomTone tone) {
    if ((unsigned)tone >= TONE_COUNT || tone == TONE_DAY) return c;
    unsigned r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    r = (r * TINT[tone][0]) >> 8;
    g = (g * TINT[tone][1]) >> 8;
    b = (b * TINT[tone][2]) >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

uint8_t room_brightness_pct(RoomTone tone, bool clock_known) {
    if (!clock_known) return 60;
    switch (tone) {
    case TONE_DUSK:  return 60;
    case TONE_NIGHT: return 25;
    default:         return 100;
    }
}

void room_shift_for_step(unsigned step, int* dx, int* dy) {
    static const int8_t RING[ROOM_SHIFT_STEPS][2] = {
        {0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1},
    };
    const unsigned i = step % ROOM_SHIFT_STEPS;
    *dx = RING[i][0];
    *dy = RING[i][1];
}

static const RoomTuning TUNING = {
    700,             // idle_hold_ms
    15000, 40000,    // idle_wait
    320,             // act_hold_ms
    3, 5,            // loops
    2500,            // read_hold_ms
    20000, 40000,    // read
    1800,            // sleep_hold_ms
    1500, 400,       // react_ms, react_hold_ms
    10UL * 60 * 1000,// hungry_after_ms
    3UL * 60 * 1000, // shift_ms
    2,               // read_weight
};
const RoomTuning& room_tuning(void) { return TUNING; }

// ---- brain ---------------------------------------------------------------

static uint32_t rnd(RoomBrain* b) {
    uint32_t x = b->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    b->rng = x ? x : 0x9E3779B9u;
    return b->rng;
}

static uint32_t rnd_range(RoomBrain* b, uint32_t lo, uint32_t hi) {
    return hi <= lo ? lo : lo + rnd(b) % (hi - lo + 1);
}

static bool due(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

static RoomAct base_act(const RoomBrain* b) { return b->tone == TONE_NIGHT ? RA_SLEEP : RA_IDLE; }

static void enter(RoomBrain* b, RoomAct a, uint32_t now) {
    const RoomTuning& t = TUNING;
    b->act = a;
    b->frame = 0;
    b->frame_at = now;
    b->act_at = now;
    b->act_end = 0;
    b->loops_left = 0;
    switch (a) {
    case RA_IDLE:
        b->next_at = now + rnd_range(b, t.idle_wait_min_ms, t.idle_wait_max_ms);
        break;
    case RA_GAME: case RA_DRINK: case RA_STRETCH: case RA_SNACK:
        b->loops_left = (uint8_t)rnd_range(b, t.loops_min, t.loops_max);
        break;
    case RA_READ:
        b->act_end = now + rnd_range(b, t.read_min_ms, t.read_max_ms);
        break;
    case RA_SURPRISE: case RA_SHY: case RA_ANGRY:
        b->act_end = now + t.react_ms;
        break;
    default:
        break;
    }
}

void room_brain_init(RoomBrain* b, uint32_t now, uint32_t seed) {
    memset(b, 0, sizeof(*b));
    b->rng = seed ? seed : 0x2545F491u;
    b->forced = -1;
    b->tone = TONE_DAY;
    b->last_snack = now;
    enter(b, RA_IDLE, now);
}

// What she does next while standing about.
static RoomAct pick_daily(RoomBrain* b, uint32_t now) {
    if (due(now, b->last_snack + TUNING.hungry_after_ms)) return RA_SNACK;
    const uint32_t read_w = b->tone == TONE_NIGHT ? 0 : TUNING.read_weight;
    uint32_t r = rnd(b) % (9 + read_w);
    if (r < 3) return RA_GAME;
    if (r < 6) return RA_DRINK;
    if (r < 9) return RA_STRETCH;
    return RA_READ;
}

bool room_brain_step(RoomBrain* b, const RoomIn* in) {
    const RoomTuning& t = TUNING;
    const uint32_t now = in->now;
    const RoomAct   was_act = b->act;
    const uint8_t   was_frame = b->frame;
    b->tone = in->tone;

    if (in->force != b->forced) {
        b->forced = in->force;
        enter(b, in->force >= 0 && in->force < RA_COUNT ? (RoomAct)in->force : base_act(b), now);
    }

    if (in->poke && !room_act_is_reaction(b->act)) {
        RoomAct r;
        if (in->poke_kind >= 0 && room_act_is_reaction((RoomAct)in->poke_kind)) r = (RoomAct)in->poke_kind;
        else r = (RoomAct)(RA_SURPRISE + rnd(b) % 3);
        enter(b, r, now);
    }

    const bool free_run = b->forced < 0;
    const uint8_t n = room_act_frames(b->act);
    switch (b->act) {
    case RA_IDLE:
        if (due(now, b->frame_at + t.idle_hold_ms)) {
            b->frame ^= 1;
            b->frame_at = now;
        }
        if (free_run) {
            if (b->tone == TONE_NIGHT) enter(b, RA_SLEEP, now);
            else if (due(now, b->next_at)) enter(b, pick_daily(b, now), now);
        }
        break;

    case RA_GAME: case RA_DRINK: case RA_STRETCH: case RA_SNACK:
        if (due(now, b->frame_at + t.act_hold_ms)) {
            b->frame_at = now;
            if (++b->frame >= n) {
                b->frame = 0;
                if (b->loops_left) b->loops_left--;
                if (b->loops_left == 0) {
                    if (b->act == RA_SNACK) b->last_snack = now;
                    if (free_run) enter(b, base_act(b), now);
                    else b->loops_left = t.loops_min;     // a held act keeps going
                }
            }
        }
        break;

    case RA_READ:
        if (due(now, b->frame_at + t.read_hold_ms)) {
            b->frame = (uint8_t)((b->frame + 1) % n);
            b->frame_at = now;
        }
        if (free_run) {
            if (b->tone == TONE_NIGHT) enter(b, RA_SLEEP, now);
            else if (due(now, b->act_end)) enter(b, RA_IDLE, now);
        }
        break;

    case RA_SLEEP:
        if (due(now, b->frame_at + t.sleep_hold_ms)) {
            b->frame = (uint8_t)((b->frame + 1) % n);
            b->frame_at = now;
        }
        if (free_run && b->tone != TONE_NIGHT) enter(b, RA_IDLE, now);
        break;

    case RA_SURPRISE: case RA_SHY: case RA_ANGRY:
        if (due(now, b->frame_at + t.react_hold_ms)) {
            b->frame = (uint8_t)((b->frame + 1) % n);
            b->frame_at = now;
        }
        if (due(now, b->act_end))
            enter(b, free_run ? base_act(b) : (RoomAct)b->forced, now);
        break;

    default:
        break;
    }
    return b->act != was_act || b->frame != was_frame;
}
