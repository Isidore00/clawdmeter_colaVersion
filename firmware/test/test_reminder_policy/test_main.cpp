// reminder_policy: the reminder programs' wall-clock schedule.
//   pio test -d firmware -e native -f test_reminder_policy
#include <unity.h>
#include "reminder_policy.h"

void setUp(void) {}
void tearDown(void) {}

// Local epochs (the app's `t`): 2026-11-23 is a Monday.
static const long SAT_1007 = 1795255620L;   // Saturday 10:07
static const long MON_0759 = 1795420740L;
static const long MON_0800 = 1795420800L;
static const long MON_1945 = 1795463100L;
static const long MON_2000 = 1795464000L;
static const long FRI_1945 = 1795808700L;
static const long MON2_0800 = 1796025600L;  // the next Monday

static const ReminderProgram* water(void) { return &REMINDER_PROGRAMS[0]; }

static void test_clock_parts(void) {
    TEST_ASSERT_EQUAL(6, clock_weekday(SAT_1007));
    TEST_ASSERT_EQUAL(10 * 60 + 7, clock_minute_of_day(SAT_1007));
    TEST_ASSERT_EQUAL(1, clock_weekday(MON_0800));
    TEST_ASSERT_EQUAL(8 * 60, clock_minute_of_day(MON_0800));
    TEST_ASSERT_EQUAL(4, clock_weekday(0));                  // 1970-01-01, a Thursday
}

static void test_window(void) {
    const ReminderProgram* p = water();
    TEST_ASSERT_TRUE(reminder_slot_in_window(p, reminder_slot(p, MON_0800)));
    TEST_ASSERT_TRUE(reminder_slot_in_window(p, reminder_slot(p, MON_1945)));
    TEST_ASSERT_FALSE(reminder_slot_in_window(p, reminder_slot(p, MON_0759)));   // 07:45 slot
    TEST_ASSERT_FALSE(reminder_slot_in_window(p, reminder_slot(p, MON_2000)));   // 20:00 is out
    TEST_ASSERT_FALSE(reminder_slot_in_window(p, reminder_slot(p, SAT_1007)));   // weekend
}

static void test_tracker(void) {
    const ReminderProgram* p = water();
    ReminderClock c;
    reminder_clock_init(&c);
    TEST_ASSERT_FALSE(reminder_clock_due(&c, p, MON_0800 + 7 * 60));    // first reading: 08:07, waits
    TEST_ASSERT_FALSE(reminder_clock_due(&c, p, MON_0800 + 14 * 60 + 59));
    TEST_ASSERT_TRUE(reminder_clock_due(&c, p, MON_0800 + 15 * 60));     // 08:15
    TEST_ASSERT_FALSE(reminder_clock_due(&c, p, MON_0800 + 15 * 60 + 30)); // once per slot
    TEST_ASSERT_FALSE(reminder_clock_due(&c, p, MON_0800 + 5 * 60));     // clock went back: no fire
    TEST_ASSERT_FALSE(reminder_clock_due(&c, p, MON_0800 + 15 * 60));    // forward again: 08:15 already rang
    TEST_ASSERT_TRUE(reminder_clock_due(&c, p, MON_0800 + 30 * 60));     // 08:30

    reminder_clock_init(&c);
    TEST_ASSERT_FALSE(reminder_clock_due(&c, p, MON_0759));
    TEST_ASSERT_TRUE(reminder_clock_due(&c, p, MON_0800));               // the first one of the day
    reminder_clock_init(&c);
    TEST_ASSERT_FALSE(reminder_clock_due(&c, p, MON_1945 + 60));
    TEST_ASSERT_FALSE(reminder_clock_due(&c, p, MON_2000));              // the window is over
}

static void test_next(void) {
    const ReminderProgram* p = water();
    TEST_ASSERT_EQUAL(MON_0800, reminder_next_epoch(p, MON_0759));
    TEST_ASSERT_EQUAL(MON_0800, reminder_next_epoch(p, MON_0800));
    TEST_ASSERT_EQUAL(MON_0800 + 15 * 60, reminder_next_epoch(p, MON_0800 + 1));
    TEST_ASSERT_EQUAL(MON_0800 + 24L * 3600, reminder_next_epoch(p, MON_2000));   // Tuesday 08:00
    TEST_ASSERT_EQUAL(MON2_0800, reminder_next_epoch(p, FRI_1945 + 1));           // over the weekend
    TEST_ASSERT_EQUAL(MON_0800, reminder_next_epoch(p, SAT_1007));                // Saturday: Monday 08:00
}

static void test_water(void) {
    TEST_ASSERT_EQUAL(100, reminder_water_pct(0, 60000));
    TEST_ASSERT_EQUAL(15, reminder_water_pct(60000, 60000));
    TEST_ASSERT_EQUAL(15, reminder_water_pct(90000, 60000));
    const int mid = reminder_water_pct(30000, 60000);
    TEST_ASSERT_TRUE(mid > 15 && mid < 100);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_clock_parts);
    RUN_TEST(test_window);
    RUN_TEST(test_tracker);
    RUN_TEST(test_next);
    RUN_TEST(test_water);
    return UNITY_END();
}
