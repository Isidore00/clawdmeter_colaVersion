#pragma once
#include <stdint.h>

// IMA ADPCM, mono, 4 bits a sample, low nibble first: what tools/room_voice.py
// writes for the room's voice clips. Pure arithmetic, native-tested.
struct AdpcmState {
    int pred;     // last sample
    int index;    // step table position
};

static inline void adpcm_reset(AdpcmState* s) { s->pred = 0; s->index = 0; }

// Decodes one 4-bit code.
int16_t adpcm_decode(AdpcmState* s, uint8_t code);
// Sample n (0-based) of a packed clip, advancing the state; n must be the
// next sample in order.
static inline int16_t adpcm_next(AdpcmState* s, const uint8_t* data, uint32_t n) {
    const uint8_t b = data[n >> 1];
    return adpcm_decode(s, (n & 1) ? (b >> 4) : (b & 15));
}
