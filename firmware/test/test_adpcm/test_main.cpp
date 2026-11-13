// adpcm: the IMA ADPCM decoder behind the room's voice clips. The encoder
// here mirrors the one in tools/room_voice.py, so a clip made by the tool
// decodes to what it was made from.
//   pio test -d firmware -e native -f test_adpcm
#include <unity.h>
#include <math.h>
#include <stdlib.h>
#include <vector>
#include "adpcm.h"

void setUp(void) {}
void tearDown(void) {}

static const int STEPS[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408,
    449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
    9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767,
};
static const int ADJ[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static std::vector<uint8_t> encode(const std::vector<int>& pcm) {
    int pred = 0, idx = 0;
    std::vector<uint8_t> nib;
    for (int s : pcm) {
        const int step = STEPS[idx];
        int diff = s - pred, code = 0;
        if (diff < 0) { code = 8; diff = -diff; }
        if (diff >= step) { code |= 4; diff -= step; }
        if (diff >= (step >> 1)) { code |= 2; diff -= step >> 1; }
        if (diff >= (step >> 2)) code |= 1;
        int d = step >> 3;
        if (code & 4) d += step;
        if (code & 2) d += step >> 1;
        if (code & 1) d += step >> 2;
        pred += (code & 8) ? -d : d;
        pred = pred > 32767 ? 32767 : (pred < -32768 ? -32768 : pred);
        idx += ADJ[code & 7];
        idx = idx < 0 ? 0 : (idx > 88 ? 88 : idx);
        nib.push_back((uint8_t)code);
    }
    if (nib.size() & 1) nib.push_back(0);
    std::vector<uint8_t> out;
    for (size_t i = 0; i < nib.size(); i += 2) out.push_back((uint8_t)(nib[i] | (nib[i + 1] << 4)));
    return out;
}

static void test_silence_stays_silent(void) {
    AdpcmState s;
    adpcm_reset(&s);
    for (int i = 0; i < 50; i++) TEST_ASSERT_TRUE(abs(adpcm_decode(&s, 0)) <= 1);
}

static void test_sine_round_trip(void) {
    std::vector<int> pcm;
    for (int n = 0; n < 4000; n++) pcm.push_back((int)(22900 * sinf(2.0f * 3.14159265f * 440.0f * n / 16000.0f)));
    const std::vector<uint8_t> packed = encode(pcm);
    TEST_ASSERT_EQUAL(2000, (int)packed.size());
    AdpcmState s;
    adpcm_reset(&s);
    double err = 0, sig = 0;
    int peak_err = 0;
    for (uint32_t n = 0; n < pcm.size(); n++) {
        const int d = adpcm_next(&s, packed.data(), n);
        if (n > 40) {                          // the step size needs a few samples to catch up
            err += (double)(d - pcm[n]) * (d - pcm[n]);
            sig += (double)pcm[n] * pcm[n];
            if (abs(d - pcm[n]) > peak_err) peak_err = abs(d - pcm[n]);
        }
    }
    TEST_ASSERT_TRUE(err / sig < 0.001);       // better than 30 dB signal to noise
    TEST_ASSERT_TRUE(peak_err < 1500);
}

static void test_clamps_at_the_rails(void) {
    AdpcmState s;
    adpcm_reset(&s);
    for (int i = 0; i < 200; i++) adpcm_decode(&s, 7);
    TEST_ASSERT_EQUAL(32767, s.pred);
    TEST_ASSERT_EQUAL(88, s.index);
    for (int i = 0; i < 200; i++) adpcm_decode(&s, 15);
    TEST_ASSERT_EQUAL(-32768, s.pred);
}

static void test_low_nibble_first(void) {
    const uint8_t data[1] = { 0x2B };          // low 0xB (negative), high 0x2
    AdpcmState a, b;
    adpcm_reset(&a);
    adpcm_reset(&b);
    const int16_t first = adpcm_next(&a, data, 0);
    const int16_t second = adpcm_next(&a, data, 1);
    TEST_ASSERT_EQUAL(adpcm_decode(&b, 0xB), first);
    TEST_ASSERT_EQUAL(adpcm_decode(&b, 0x2), second);
    TEST_ASSERT_TRUE(first < 0);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_silence_stays_silent);
    RUN_TEST(test_sine_round_trip);
    RUN_TEST(test_clamps_at_the_rails);
    RUN_TEST(test_low_nibble_first);
    return UNITY_END();
}
