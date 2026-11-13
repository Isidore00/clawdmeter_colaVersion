#pragma once
#include <stdint.h>

// What the room page (room.h) does: pure rules, no Arduino or LVGL, native-
// tested in test/test_room_policy.
//
// The order of RoomAct is the order of the per-act tables in room.cpp and
// the names `room <act>` takes.
enum RoomAct {
    RA_IDLE,        // standing on the rug, two frames alternating
    RA_GAME,        // daily acts: loop their frames a few times, then idle
    RA_DRINK,
    RA_STRETCH,
    RA_READ,        // daytime: the room scene with her reading (a patch on the background)
    RA_SLEEP,       // night: the room scene with her asleep (a patch)
    RA_SURPRISE,    // a poke: one of these three for ~1.5 s
    RA_SHY,
    RA_ANGRY,
    RA_SNACK,       // hungry: the cabinet drawer open (a patch), snack frames
    RA_COUNT
};

enum RoomTone { TONE_DAY, TONE_DUSK, TONE_NIGHT, TONE_COUNT };

// Frames per act: pose frames for the standing acts, patch variants for
// read and sleep.
uint8_t room_act_frames(RoomAct a);
bool    room_act_is_scene(RoomAct a);       // read / sleep: her pose is part of the background
bool    room_act_is_reaction(RoomAct a);    // surprise / shy / angry
const char* room_act_name(RoomAct a);       // "idle", "game", ...; NULL out of range
RoomAct room_act_from_name(const char* name);   // RA_COUNT when unknown
const char* room_tone_name(RoomTone t);
RoomTone room_tone_from_name(const char* name); // TONE_COUNT when unknown

// Time of day from the local minute of the day (0..1439): night 19:30-05:30,
// dusk 17:00-19:30 and 05:30-07:00, day between.
RoomTone room_tone_for_minute(int minute_of_day);

// The room's palette per tone: an RGB565 colour scaled per channel. Day is
// the art as drawn. Applied to the background palette and every sprite
// palette, so she's lit like the room.
uint16_t room_tint565(uint16_t rgb565, RoomTone tone);

struct RoomTuning {
    uint32_t idle_hold_ms;        // each of the two idle frames
    uint32_t idle_wait_min_ms;    // standing about before the next daily act
    uint32_t idle_wait_max_ms;
    uint32_t act_hold_ms;         // each frame of game / drink / stretch / snack
    uint8_t  loops_min, loops_max;
    uint32_t read_hold_ms;        // each read patch variant
    uint32_t read_min_ms, read_max_ms;   // how long she reads
    uint32_t sleep_hold_ms;       // each sleep patch variant
    uint32_t react_ms;            // a poke reaction
    uint32_t react_hold_ms;       // its frames
    uint32_t hungry_after_ms;     // since the last snack
    uint8_t  read_weight;         // chance weights for the next daily act (of game/drink/stretch = 3 each)
};
const RoomTuning& room_tuning(void);

// An input to one step. `force` >= 0 holds that act (QA, `room <act>`);
// `poke` starts a reaction (poke_kind: a reaction RoomAct, or < 0 for a
// random one) unless one is already running.
struct RoomIn {
    uint32_t now;
    RoomTone tone;
    int      force;
    bool     poke;
    int      poke_kind;
};

struct RoomBrain {
    RoomAct  act;
    uint8_t  frame;
    uint8_t  loops_left;
    uint32_t frame_at;         // when the current frame came up
    uint32_t act_at;           // when the current act started
    uint32_t act_end;          // timed acts: when they end (reaction, read)
    uint32_t next_at;          // idle: when the next daily act starts
    uint32_t last_snack;
    int      forced;           // the act being held, or -1
    RoomTone tone;
    uint32_t rng;
};

void room_brain_init(RoomBrain* b, uint32_t now, uint32_t seed);
// Advances the brain; true when what's on screen (act or frame) changed.
bool room_brain_step(RoomBrain* b, const RoomIn* in);

// The cabinet is open while she has a snack.
static inline bool room_brain_cabinet_open(const RoomBrain* b) { return b->act == RA_SNACK; }
