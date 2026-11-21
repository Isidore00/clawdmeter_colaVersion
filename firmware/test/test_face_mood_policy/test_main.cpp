// face_mood_policy: which mood the face wears while Claude Code works.
//   pio test -d firmware -e native -f test_face_mood_policy
#include <unity.h>
#include "face_mood_policy.h"

void setUp(void) {}
void tearDown(void) {}

static FaceMoodInput base(void) {
    FaceMoodInput in = {};
    in.now = 10000;
    in.verb_mood = MOOD_THINK;
    return in;
}

static void test_names_round_trip(void) {
    for (int i = 0; i < MOOD_COUNT; i++)
        TEST_ASSERT_EQUAL(i, face_mood_from_name(face_mood_name((FaceMood)i)));
    TEST_ASSERT_EQUAL(MOOD_COUNT, face_mood_from_name("grumpy"));
}

static void test_verbs(void) {
    TEST_ASSERT_EQUAL(MOOD_THINK, face_mood_for_verb("Mulling"));
    TEST_ASSERT_EQUAL(MOOD_WORK, face_mood_for_verb("Crunching"));
    TEST_ASSERT_EQUAL(MOOD_WORK, face_mood_for_verb("Tinkering"));     // not listed: a doing word
    TEST_ASSERT_EQUAL(MOOD_IDEA, face_mood_for_verb("Conjuring"));
    TEST_ASSERT_EQUAL(MOOD_SLOW, face_mood_for_verb("Simmering"));
    TEST_ASSERT_EQUAL(MOOD_WORK, face_mood_for_verb("Nonsense"));
    TEST_ASSERT_EQUAL(MOOD_WORK, face_mood_for_verb(NULL));
}

static void test_quiet_is_none(void) {
    FaceMoodInput in = base();
    FaceMoodPick p = face_mood_pick(&in);
    TEST_ASSERT_EQUAL(MOOD_NONE, p.mood);
    TEST_ASSERT_FALSE(p.sweat);
}

static void test_working_follows_the_word(void) {
    FaceMoodInput in = base();
    in.working = true;
    TEST_ASSERT_EQUAL(MOOD_THINK, face_mood_pick(&in).mood);
    in.verb_mood = MOOD_IDEA;
    TEST_ASSERT_EQUAL(MOOD_IDEA, face_mood_pick(&in).mood);
    in.verb_mood = MOOD_NONE;                                           // no word yet: a doing face
    TEST_ASSERT_EQUAL(MOOD_WORK, face_mood_pick(&in).mood);
}

static void test_priorities(void) {
    FaceMoodInput in = base();
    in.working = true;
    in.prompt_until = in.now + 500;
    TEST_ASSERT_EQUAL(MOOD_STARTLED, face_mood_pick(&in).mood);         // the prompt first
    in.done_until = in.now + 500;
    TEST_ASSERT_EQUAL(MOOD_HAPPY, face_mood_pick(&in).mood);            // done outranks it
    in.reset_until = in.now + 500;
    TEST_ASSERT_EQUAL(MOOD_HEART, face_mood_pick(&in).mood);            // a fresh window outranks that
    in.sleepy = true;
    TEST_ASSERT_EQUAL(MOOD_ASLEEP, face_mood_pick(&in).mood);           // and nodding off outranks all
}

static void test_windows_end(void) {
    FaceMoodInput in = base();
    in.done_until = in.now;                                             // ended this very ms
    TEST_ASSERT_EQUAL(MOOD_NONE, face_mood_pick(&in).mood);
    in.done_until = in.now + 1;
    TEST_ASSERT_EQUAL(MOOD_HAPPY, face_mood_pick(&in).mood);
    TEST_ASSERT_TRUE(face_mood_before(0xFFFFFFF0u, 5));                 // wraps
    TEST_ASSERT_FALSE(face_mood_before(5, 0xFFFFFFF0u));
}

static void test_near_limit(void) {
    FaceMoodInput in = base();
    in.near_limit = true;
    FaceMoodPick p = face_mood_pick(&in);
    TEST_ASSERT_EQUAL(MOOD_NERVOUS, p.mood);
    TEST_ASSERT_TRUE(p.sweat);
    in.working = true;                                                  // working: the word's face, still sweating
    p = face_mood_pick(&in);
    TEST_ASSERT_EQUAL(MOOD_THINK, p.mood);
    TEST_ASSERT_TRUE(p.sweat);
    in.done_until = in.now + 100;                                       // a happy face doesn't sweat
    p = face_mood_pick(&in);
    TEST_ASSERT_EQUAL(MOOD_HAPPY, p.mood);
    TEST_ASSERT_FALSE(p.sweat);
    in.done_until = 0;
    in.sleepy = true;
    p = face_mood_pick(&in);
    TEST_ASSERT_EQUAL(MOOD_ASLEEP, p.mood);
    TEST_ASSERT_FALSE(p.sweat);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_names_round_trip);
    RUN_TEST(test_verbs);
    RUN_TEST(test_quiet_is_none);
    RUN_TEST(test_working_follows_the_word);
    RUN_TEST(test_priorities);
    RUN_TEST(test_windows_end);
    RUN_TEST(test_near_limit);
    return UNITY_END();
}
