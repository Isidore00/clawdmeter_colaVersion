// room_policy: the room page's behaviour (acts, timing, time of day).
//   pio test -d firmware -e native -f test_room_policy
#include <unity.h>
#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include "room_policy.h"

void setUp(void) {}
void tearDown(void) {}

static RoomIn in_at(uint32_t now, RoomTone tone = TONE_DAY) {
    RoomIn i = {};
    i.now = now;
    i.tone = tone;
    i.force = -1;
    i.poke_kind = -1;
    return i;
}

// Runs the brain from `from` to `to` in 50 ms steps; returns every act seen
// as a bit mask.
static uint32_t run(RoomBrain* b, uint32_t from, uint32_t to, RoomTone tone = TONE_DAY) {
    uint32_t seen = 0;
    for (uint32_t t = from; t <= to; t += 50) {
        RoomIn i = in_at(t, tone);
        room_brain_step(b, &i);
        seen |= 1u << b->act;
    }
    return seen;
}

static void test_names(void) {
    for (int a = 0; a < RA_COUNT; a++)
        TEST_ASSERT_EQUAL(a, room_act_from_name(room_act_name((RoomAct)a)));
    TEST_ASSERT_EQUAL(RA_COUNT, room_act_from_name("dance"));
    TEST_ASSERT_NULL(room_act_name(RA_COUNT));
    for (int t = 0; t < TONE_COUNT; t++)
        TEST_ASSERT_EQUAL(t, room_tone_from_name(room_tone_name((RoomTone)t)));
    TEST_ASSERT_EQUAL(TONE_COUNT, room_tone_from_name("noon"));
}

static void test_tone_by_time(void) {
    TEST_ASSERT_EQUAL(TONE_NIGHT, room_tone_for_minute(0));
    TEST_ASSERT_EQUAL(TONE_NIGHT, room_tone_for_minute(5 * 60 + 29));
    TEST_ASSERT_EQUAL(TONE_DUSK,  room_tone_for_minute(5 * 60 + 30));
    TEST_ASSERT_EQUAL(TONE_DAY,   room_tone_for_minute(7 * 60));
    TEST_ASSERT_EQUAL(TONE_DAY,   room_tone_for_minute(16 * 60 + 59));
    TEST_ASSERT_EQUAL(TONE_DUSK,  room_tone_for_minute(17 * 60));
    TEST_ASSERT_EQUAL(TONE_DUSK,  room_tone_for_minute(19 * 60 + 29));
    TEST_ASSERT_EQUAL(TONE_NIGHT, room_tone_for_minute(19 * 60 + 30));
    TEST_ASSERT_EQUAL(TONE_NIGHT, room_tone_for_minute(23 * 60 + 59));
}

static void test_tint(void) {
    const uint16_t white = 0xFFFF;
    TEST_ASSERT_EQUAL_HEX16(white, room_tint565(white, TONE_DAY));
    TEST_ASSERT_EQUAL_HEX16(0, room_tint565(0, TONE_NIGHT));
    // Night is darker and bluer than white; dusk is warmer (blue cut most).
    const uint16_t n = room_tint565(white, TONE_NIGHT);
    TEST_ASSERT_TRUE(((n >> 11) & 31) < 16);
    TEST_ASSERT_TRUE((n & 31) > ((n >> 11) & 31));
    const uint16_t d = room_tint565(white, TONE_DUSK);
    TEST_ASSERT_TRUE((d & 31) < ((d >> 11) & 31));
}

static void test_brightness_by_tone(void) {
    TEST_ASSERT_EQUAL(100, room_brightness_pct(TONE_DAY, true));
    TEST_ASSERT_EQUAL(60, room_brightness_pct(TONE_DUSK, true));
    TEST_ASSERT_EQUAL(25, room_brightness_pct(TONE_NIGHT, true));
    // No wall clock yet: the tone is a guess (always day), so dusk's level.
    TEST_ASSERT_EQUAL(60, room_brightness_pct(TONE_DAY, false));
    TEST_ASSERT_EQUAL(60, room_brightness_pct(TONE_NIGHT, false));
}

static void test_shift_ring(void) {
    int px = 0, py = 0;
    room_shift_for_step(0, &px, &py);
    TEST_ASSERT_EQUAL(0, px);
    TEST_ASSERT_EQUAL(0, py);
    for (unsigned i = 1; i <= 3 * ROOM_SHIFT_STEPS; i++) {
        int x, y;
        room_shift_for_step(i, &x, &y);
        TEST_ASSERT_TRUE(x >= 0 && x <= 2 && y >= 0 && y <= 2);
        TEST_ASSERT_TRUE(abs(x - px) <= 1 && abs(y - py) <= 1);     // never a visible jump
        TEST_ASSERT_TRUE(x != px || y != py);                       // and always a step
        px = x; py = y;
    }
    // Round the ring and back to the start.
    room_shift_for_step(ROOM_SHIFT_STEPS, &px, &py);
    TEST_ASSERT_EQUAL(0, px);
    TEST_ASSERT_EQUAL(0, py);
}

static void test_starts_idle_and_alternates(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 1);
    TEST_ASSERT_EQUAL(RA_IDLE, b.act);
    RoomIn i = in_at(0);
    TEST_ASSERT_FALSE(room_brain_step(&b, &i));
    i = in_at(room_tuning().idle_hold_ms);
    TEST_ASSERT_TRUE(room_brain_step(&b, &i));
    TEST_ASSERT_EQUAL(RA_IDLE, b.act);
    TEST_ASSERT_EQUAL(1, b.frame);
    i = in_at(2 * room_tuning().idle_hold_ms);
    TEST_ASSERT_TRUE(room_brain_step(&b, &i));
    TEST_ASSERT_EQUAL(0, b.frame);
}

static void test_daily_acts_come_and_go(void) {
    for (uint32_t seed = 1; seed < 40; seed++) {
        RoomBrain b;
        room_brain_init(&b, 0, seed * 7919);
        const uint32_t seen = run(&b, 0, 5UL * 60 * 1000);
        const uint32_t daily = (1u << RA_GAME) | (1u << RA_DRINK) | (1u << RA_STRETCH) | (1u << RA_READ);
        TEST_ASSERT_TRUE_MESSAGE(seen & daily, "no daily act in 5 minutes");
        TEST_ASSERT_FALSE(seen & (1u << RA_SLEEP));            // daytime: never asleep
        TEST_ASSERT_FALSE(seen & (1u << RA_SNACK));            // not hungry yet
    }
}

static void test_every_daily_act_and_reading_happen(void) {
    uint32_t seen = 0;
    for (uint32_t seed = 1; seed < 40; seed++) {
        RoomBrain b;
        room_brain_init(&b, 0, seed * 104729);
        seen |= run(&b, 0, 9UL * 60 * 1000);
    }
    for (int a : { RA_IDLE, RA_GAME, RA_DRINK, RA_STRETCH, RA_READ })
        TEST_ASSERT_TRUE_MESSAGE(seen & (1u << a), room_act_name((RoomAct)a));
}

static void test_daily_act_loops_then_idle(void) {
    // Whatever act she picks (not reading), it ends within loops_max passes
    // of its frames and she stands again.
    const RoomTuning& t = room_tuning();
    for (uint32_t seed = 1; seed < 60; seed++) {
        RoomBrain b;
        room_brain_init(&b, 0, seed * 31337);
        uint32_t t0 = 0;
        for (uint32_t now = 0; now < 3UL * 60 * 1000; now += 50) {
            RoomIn i = in_at(now);
            room_brain_step(&b, &i);
            if (b.act == RA_GAME || b.act == RA_DRINK || b.act == RA_STRETCH) {
                if (!t0) t0 = b.act_at;
                const uint32_t limit = t.loops_max * 3u * (t.act_hold_ms + 50) + 200;
                TEST_ASSERT_TRUE((uint32_t)(now - b.act_at) <= limit);
            } else {
                t0 = 0;
            }
        }
    }
}

static void test_poke_reacts_then_returns(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 3);
    uint32_t seen_reactions = 0;
    for (int k = 0; k < 60; k++) {
        const uint32_t t0 = 1000 + (uint32_t)k * 10000;
        RoomIn i = in_at(t0);
        room_brain_step(&b, &i);                 // let the time pass to t0
        i.poke = true;
        room_brain_step(&b, &i);
        TEST_ASSERT_TRUE(room_act_is_reaction(b.act));
        seen_reactions |= 1u << b.act;
        // A second poke while reacting changes nothing.
        const RoomAct r = b.act;
        i = in_at(t0 + 100);
        i.poke = true;
        room_brain_step(&b, &i);
        TEST_ASSERT_EQUAL(r, b.act);
        // About 1.5 s later she is back to standing about.
        i = in_at(t0 + room_tuning().react_ms + 60);
        room_brain_step(&b, &i);
        TEST_ASSERT_FALSE(room_act_is_reaction(b.act));
    }
    TEST_ASSERT_TRUE(seen_reactions & (1u << RA_SURPRISE));
    TEST_ASSERT_TRUE(seen_reactions & (1u << RA_SHY));
    TEST_ASSERT_TRUE(seen_reactions & (1u << RA_ANGRY));
}

static void test_poke_kind_is_honoured(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 3);
    RoomIn i = in_at(100);
    i.poke = true;
    i.poke_kind = RA_SHY;
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_SHY, b.act);
}

static void test_night_sleeps_and_wakes(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 9);
    RoomIn i = in_at(100, TONE_NIGHT);
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_SLEEP, b.act);
    // Asleep for a long time: no daily act, no reading, no snack.
    const uint32_t seen = run(&b, 200, 30UL * 60 * 1000, TONE_NIGHT);
    TEST_ASSERT_EQUAL_HEX32(1u << RA_SLEEP, seen);
    // A poke wakes her for a moment; then back to sleep.
    i = in_at(31UL * 60 * 1000, TONE_NIGHT);
    i.poke = true;
    room_brain_step(&b, &i);
    TEST_ASSERT_TRUE(room_act_is_reaction(b.act));
    i = in_at(31UL * 60 * 1000 + 1600, TONE_NIGHT);
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_SLEEP, b.act);
    // Morning.
    i = in_at(32UL * 60 * 1000, TONE_DAY);
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_IDLE, b.act);
}

static void test_reading_cycles_and_ends(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 1);
    RoomIn i = in_at(0);
    i.force = RA_READ;
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_READ, b.act);
    // Held: stays, cycling its three variants.
    uint8_t seen = 0;
    for (uint32_t t = 0; t < 60000; t += 100) {
        i = in_at(t);
        i.force = RA_READ;
        room_brain_step(&b, &i);
        TEST_ASSERT_EQUAL(RA_READ, b.act);
        seen |= 1u << b.frame;
    }
    TEST_ASSERT_EQUAL_HEX8(7, seen);
    // Let go: idle; read again free-running and she puts it down by herself.
    RoomBrain f;
    room_brain_init(&f, 0, 1);
    i = in_at(0);
    i.force = RA_READ;
    room_brain_step(&f, &i);
    i.force = -1;
    i.now = 10;
    room_brain_step(&f, &i);
    TEST_ASSERT_EQUAL(RA_IDLE, f.act);
}

static void test_hunger_brings_the_snack(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 11);
    const uint32_t hungry = room_tuning().hungry_after_ms;
    // Nothing before she's hungry.
    TEST_ASSERT_FALSE(run(&b, 0, hungry - 1000) & (1u << RA_SNACK));
    // After it, the next daily act is the snack, with the cabinet open.
    uint32_t t = hungry;
    bool opened = false, closed_again = false;
    for (; t < hungry + 120000; t += 50) {
        RoomIn i = in_at(t);
        room_brain_step(&b, &i);
        if (b.act == RA_SNACK) opened = opened || room_brain_cabinet_open(&b);
        else if (opened) { closed_again = true; break; }
    }
    TEST_ASSERT_TRUE(opened);
    TEST_ASSERT_TRUE(closed_again);
    TEST_ASSERT_FALSE(room_brain_cabinet_open(&b));
    // Fed: not hungry again right away.
    TEST_ASSERT_FALSE(run(&b, t, t + hungry - 5000) & (1u << RA_SNACK));
}

static void test_snack_plays_its_three_frames(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 2);
    RoomIn i = in_at(0);
    i.force = RA_SNACK;
    room_brain_step(&b, &i);
    uint8_t seen = 0;
    for (uint32_t t = 0; t < 3000; t += 50) {
        i = in_at(t);
        i.force = RA_SNACK;
        room_brain_step(&b, &i);
        seen |= 1u << b.frame;
    }
    TEST_ASSERT_EQUAL_HEX8(7, seen);
}

static void test_force_holds_and_releases(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 4);
    for (int a = 0; a < RA_COUNT; a++) {
        if (room_act_is_reaction((RoomAct)a)) continue;
        RoomIn i = in_at(1000);
        i.force = a;
        room_brain_step(&b, &i);
        TEST_ASSERT_EQUAL(a, b.act);
        for (uint32_t t = 1000; t < 3600000; t += 997) {
            i = in_at(t);
            i.force = a;
            room_brain_step(&b, &i);
            TEST_ASSERT_EQUAL(a, b.act);
        }
    }
    RoomIn i = in_at(4000000);
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_IDLE, b.act);
}

static void test_poke_over_a_held_act_returns_to_it(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 4);
    RoomIn i = in_at(100);
    i.force = RA_READ;
    room_brain_step(&b, &i);
    i = in_at(200);
    i.force = RA_READ;
    i.poke = true;
    room_brain_step(&b, &i);
    TEST_ASSERT_TRUE(room_act_is_reaction(b.act));
    i = in_at(200 + room_tuning().react_ms + 100);
    i.force = RA_READ;
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_READ, b.act);
}

static void test_frames_in_range(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 77);
    for (uint32_t t = 0; t < 30UL * 60 * 1000; t += 37) {
        RoomIn i = in_at(t, (RoomTone)((t / 600000) % 3));
        i.poke = (t % 90001) < 37;
        room_brain_step(&b, &i);
        TEST_ASSERT_TRUE(b.frame < room_act_frames(b.act));
    }
}

static void test_pokes_escalate_and_reset(void) {
    PokeCount c = {};
    TEST_ASSERT_EQUAL(RA_SURPRISE, room_poke_reaction(&c, 1000));
    TEST_ASSERT_EQUAL(RA_SHY, room_poke_reaction(&c, 2000));
    TEST_ASSERT_EQUAL(RA_SHY, room_poke_reaction(&c, 3000));
    TEST_ASSERT_EQUAL(RA_ANGRY, room_poke_reaction(&c, 4000));
    TEST_ASSERT_EQUAL(RA_ANGRY, room_poke_reaction(&c, 5000));
    // Exactly 10 s since the last still counts; more than that starts over.
    TEST_ASSERT_EQUAL(RA_ANGRY, room_poke_reaction(&c, 15000));
    TEST_ASSERT_EQUAL(RA_SURPRISE, room_poke_reaction(&c, 25001));
    TEST_ASSERT_EQUAL(RA_SHY, room_poke_reaction(&c, 26000));
    // Each poke refreshes the window: pokes 9 s apart keep escalating.
    PokeCount d = {};
    uint32_t t = 100;
    room_poke_reaction(&d, t);
    for (int i = 0; i < 3; i++) { t += 9000; room_poke_reaction(&d, t); }
    TEST_ASSERT_EQUAL(RA_ANGRY, room_poke_reaction(&d, t + 9000));
    // The millis() wrap does not break the window.
    PokeCount w = {};
    room_poke_reaction(&w, 0xFFFFFF00u);
    TEST_ASSERT_EQUAL(RA_SHY, room_poke_reaction(&w, 500));
}

static void test_voice_names_and_lines(void) {
    for (int v = 0; v < RV_COUNT; v++) {
        TEST_ASSERT_EQUAL(v, room_voice_from_name(room_voice_name((RoomVoice)v)));
        char buf[40];
        snprintf(buf, sizeof buf, "room_%s", room_voice_name((RoomVoice)v));
        TEST_ASSERT_EQUAL(v, room_voice_from_name(buf));
    }
    TEST_ASSERT_EQUAL(RV_COUNT, room_voice_from_name("game"));
    TEST_ASSERT_NULL(room_voice_name(RV_COUNT));
    TEST_ASSERT_EQUAL(RV_POKE_SURPRISE, room_voice_for_reaction(RA_SURPRISE));
    TEST_ASSERT_EQUAL(RV_POKE_SHY, room_voice_for_reaction(RA_SHY));
    TEST_ASSERT_EQUAL(RV_POKE_ANGRY, room_voice_for_reaction(RA_ANGRY));
    TEST_ASSERT_EQUAL(RV_COUNT, room_voice_for_reaction(RA_PAT));    // a pat picks love / iloveyou itself
    TEST_ASSERT_EQUAL(RV_COUNT, room_voice_for_reaction(RA_IDLE));
}

static void test_morning_window(void) {
    TEST_ASSERT_FALSE(room_is_morning(5 * 60 + 29));
    TEST_ASSERT_TRUE(room_is_morning(5 * 60 + 30));
    TEST_ASSERT_TRUE(room_is_morning(10 * 60 + 59));
    TEST_ASSERT_FALSE(room_is_morning(11 * 60));
}

static void test_pat_lasts_then_returns(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 5);
    RoomIn i = in_at(1000);
    i.poke = true;
    i.poke_kind = RA_PAT;
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_PAT, b.act);
    // Two frames alternate for the whole pat; other pokes change nothing.
    uint32_t frames_seen = 0;
    for (uint32_t t = 1050; t < 1000 + room_tuning().pat_ms - 100; t += 50) {
        i = in_at(t);
        if (t % 300 == 0) { i.poke = true; i.poke_kind = RA_ANGRY; }
        room_brain_step(&b, &i);
        TEST_ASSERT_EQUAL(RA_PAT, b.act);
        frames_seen |= 1u << b.frame;
    }
    TEST_ASSERT_EQUAL_HEX32(3, frames_seen);
    i = in_at(1000 + room_tuning().pat_ms + 60);
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_IDLE, b.act);
    TEST_ASSERT_EQUAL(2, room_act_frames(RA_PAT));
    TEST_ASSERT_TRUE(room_tuning().pat_ms >= 2400 && room_tuning().pat_ms <= 2600);
}

static void test_named_poke_takes_over_a_reaction(void) {
    RoomBrain b;
    room_brain_init(&b, 0, 9);
    RoomIn i = in_at(1000);
    i.poke = true;
    i.poke_kind = RA_SURPRISE;
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_SURPRISE, b.act);
    i = in_at(1300);
    i.poke = true;
    i.poke_kind = RA_SHY;
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_SHY, b.act);
    // The reaction runs its full time from the newest poke.
    i = in_at(1300 + room_tuning().react_ms - 100);
    room_brain_step(&b, &i);
    TEST_ASSERT_EQUAL(RA_SHY, b.act);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_names);
    RUN_TEST(test_tone_by_time);
    RUN_TEST(test_tint);
    RUN_TEST(test_brightness_by_tone);
    RUN_TEST(test_shift_ring);
    RUN_TEST(test_starts_idle_and_alternates);
    RUN_TEST(test_daily_acts_come_and_go);
    RUN_TEST(test_every_daily_act_and_reading_happen);
    RUN_TEST(test_daily_act_loops_then_idle);
    RUN_TEST(test_poke_reacts_then_returns);
    RUN_TEST(test_poke_kind_is_honoured);
    RUN_TEST(test_night_sleeps_and_wakes);
    RUN_TEST(test_reading_cycles_and_ends);
    RUN_TEST(test_hunger_brings_the_snack);
    RUN_TEST(test_snack_plays_its_three_frames);
    RUN_TEST(test_force_holds_and_releases);
    RUN_TEST(test_poke_over_a_held_act_returns_to_it);
    RUN_TEST(test_frames_in_range);
    RUN_TEST(test_pokes_escalate_and_reset);
    RUN_TEST(test_voice_names_and_lines);
    RUN_TEST(test_morning_window);
    RUN_TEST(test_pat_lasts_then_returns);
    RUN_TEST(test_named_poke_takes_over_a_reaction);
    return UNITY_END();
}
