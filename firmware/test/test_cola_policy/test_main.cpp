// cola_policy: which state the cola layer shows, and which wins when several hold.
//   pio test -d firmware -e native -f test_cola_policy
#include <unity.h>
#include "cola_policy.h"

void setUp(void) {}
void tearDown(void) {}

// Connected, a usage reading at 40 %, nothing going on.
static ColaInput quiet(void) {
    ColaInput in = {};
    in.connected = true;
    in.have_pct = true;
    in.pct = 40.0f;
    in.activity = 0;
    return in;
}

static void test_idle_and_working(void) {
    ColaInput in = quiet();
    TEST_ASSERT_EQUAL(COLA_IDLE, cola_pick(&in));
    in.activity = -1;                      // no hooks: unknown is not working
    TEST_ASSERT_EQUAL(COLA_IDLE, cola_pick(&in));
    in.activity = 1;
    TEST_ASSERT_EQUAL(COLA_WORKING, cola_pick(&in));
    in.activity = 3;
    TEST_ASSERT_EQUAL(COLA_WORKING, cola_pick(&in));
}

static void test_usage_thresholds(void) {
    ColaInput in = quiet();
    in.pct = 79.9f;
    TEST_ASSERT_EQUAL(COLA_IDLE, cola_pick(&in));
    in.pct = 80.0f;
    TEST_ASSERT_EQUAL(COLA_LOW, cola_pick(&in));
    in.pct = 99.4f;                        // still shown as 99 %
    TEST_ASSERT_EQUAL(COLA_LOW, cola_pick(&in));
    in.pct = 99.5f;                        // shown as 100 %
    TEST_ASSERT_EQUAL(COLA_EMPTY, cola_pick(&in));
    in.pct = 104.0f;
    TEST_ASSERT_EQUAL(COLA_EMPTY, cola_pick(&in));
}

static void test_limited_is_empty_whatever_the_meter_reads(void) {
    ColaInput in = quiet();
    in.limited = true;                     // the weekly limit ran out at 40 % of the session
    TEST_ASSERT_EQUAL(COLA_EMPTY, cola_pick(&in));
    TEST_ASSERT_TRUE(cola_empty(false, 0.0f, true));
    TEST_ASSERT_FALSE(cola_empty(false, 100.0f, false));   // no reading yet: 100 is not a reading
}

static void test_no_reading_is_never_low(void) {
    ColaInput in = quiet();
    in.have_pct = false;
    in.pct = 95.0f;
    TEST_ASSERT_EQUAL(COLA_IDLE, cola_pick(&in));
}

// reset > reply > empty > prompt > low > working > idle
static void test_priority(void) {
    ColaInput in = quiet();
    in.activity = 2;
    in.pct = 85.0f;
    TEST_ASSERT_EQUAL(COLA_LOW, cola_pick(&in));           // low over working
    in.prompt_playing = true;
    TEST_ASSERT_EQUAL(COLA_PROMPT, cola_pick(&in));        // prompt over low
    in.pct = 100.0f;
    TEST_ASSERT_EQUAL(COLA_EMPTY, cola_pick(&in));         // empty over prompt
    in.alert = true;
    TEST_ASSERT_EQUAL(COLA_REPLY, cola_pick(&in));         // reply over empty
    in.reset_playing = true;
    TEST_ASSERT_EQUAL(COLA_RESET, cola_pick(&in));         // reset over everything
}

static void test_not_connected_is_idle(void) {
    ColaInput in = quiet();
    in.connected = false;
    in.activity = 3;
    in.pct = 100.0f;
    in.limited = true;
    in.alert = true;
    in.prompt_playing = true;
    in.reset_playing = true;
    TEST_ASSERT_EQUAL(COLA_IDLE, cola_pick(&in));
}

static void test_oneshots_and_names(void) {
    for (int s = 0; s < COLA_STATE_COUNT; s++) {
        TEST_ASSERT_EQUAL(s == COLA_PROMPT || s == COLA_RESET, cola_oneshot((ColaState)s));
        TEST_ASSERT_EQUAL(s, cola_state_from_name(cola_state_name((ColaState)s)));
    }
    TEST_ASSERT_EQUAL_STRING("reply", cola_state_name(COLA_REPLY));
    TEST_ASSERT_NULL(cola_state_name(COLA_STATE_COUNT));
    TEST_ASSERT_EQUAL(COLA_STATE_COUNT, cola_state_from_name("auto"));
    TEST_ASSERT_EQUAL(COLA_STATE_COUNT, cola_state_from_name(nullptr));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_and_working);
    RUN_TEST(test_usage_thresholds);
    RUN_TEST(test_limited_is_empty_whatever_the_meter_reads);
    RUN_TEST(test_no_reading_is_never_low);
    RUN_TEST(test_priority);
    RUN_TEST(test_not_connected_is_idle);
    RUN_TEST(test_oneshots_and_names);
    return UNITY_END();
}
