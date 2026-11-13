// cola_lz4: the block decoder behind the cola layer's compressed frames.
//   pio test -d firmware -e native -f test_cola_lz4
#include <unity.h>
#include <string.h>
#include "cola_lz4.h"

void setUp(void) {}
void tearDown(void) {}

void test_literals_only(void) {
    const uint8_t src[] = {0x50, 'a', 'b', 'c', 'd', 'e'};
    uint8_t out[5];
    TEST_ASSERT_TRUE(cola_lz4_decode(src, sizeof src, out, sizeof out));
    TEST_ASSERT_EQUAL_MEMORY("abcde", out, 5);
}

// A match that overlaps its own output is a run: "a" then 9 copies at offset 1.
void test_overlapping_match_is_a_run(void) {
    const uint8_t src[] = {0x15, 'a', 0x01, 0x00,       // 1 literal, match 5+4 at offset 1
                           0x10, 'b'};                  // last sequence: 1 literal
    uint8_t out[11];
    TEST_ASSERT_TRUE(cola_lz4_decode(src, sizeof src, out, sizeof out));
    TEST_ASSERT_EQUAL_MEMORY("aaaaaaaaaab", out, 11);
}

// 300 literals and a 300-byte match: both lengths use the 255-run extension.
void test_long_lengths(void) {
    uint8_t src[1 + 2 + 300 + 2 + 2 + 1 + 1];
    size_t n = 0;
    src[n++] = 0xFF;                                     // literal 15+.., match 15+..
    src[n++] = 255; src[n++] = 30;                       // literals 15 + 255 + 30 = 300
    for (int i = 0; i < 300; i++) src[n++] = (uint8_t)i;
    src[n++] = 0x2C; src[n++] = 0x01;                    // offset 300
    src[n++] = 255; src[n++] = 26;                       // match 4 + 15 + 255 + 26 = 300
    src[n++] = 0x10; src[n++] = 0x7F;                    // final literal
    static uint8_t out[601];
    TEST_ASSERT_TRUE(cola_lz4_decode(src, n, out, sizeof out));
    for (int i = 0; i < 300; i++) {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)i, out[i]);
        TEST_ASSERT_EQUAL_UINT8((uint8_t)i, out[300 + i]);
    }
    TEST_ASSERT_EQUAL_UINT8(0x7F, out[600]);
}

void test_rejects_malformed(void) {
    uint8_t out[16];
    const uint8_t ok[] = {0x15, 'a', 0x01, 0x00, 0x10, 'b'};          // decodes to 11 bytes
    TEST_ASSERT_FALSE(cola_lz4_decode(ok, sizeof ok, out, 10));       // output too small
    TEST_ASSERT_FALSE(cola_lz4_decode(ok, sizeof ok, out, 12));       // output not filled
    TEST_ASSERT_FALSE(cola_lz4_decode(ok, sizeof ok - 1, out, 11));   // truncated
    const uint8_t zero_off[] = {0x10, 'a', 0x00, 0x00, 0x10, 'b'};
    TEST_ASSERT_FALSE(cola_lz4_decode(zero_off, sizeof zero_off, out, 16));
    const uint8_t far_off[] = {0x10, 'a', 0x02, 0x00, 0x10, 'b'};     // before the start
    TEST_ASSERT_FALSE(cola_lz4_decode(far_off, sizeof far_off, out, 16));
    const uint8_t long_lit[] = {0x90, 'a'};                           // claims 9 literals
    TEST_ASSERT_FALSE(cola_lz4_decode(long_lit, sizeof long_lit, out, 16));
    TEST_ASSERT_FALSE(cola_lz4_decode(ok, 0, out, 11));               // empty input
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_literals_only);
    RUN_TEST(test_overlapping_match_is_a_run);
    RUN_TEST(test_long_lengths);
    RUN_TEST(test_rejects_malformed);
    return UNITY_END();
}
