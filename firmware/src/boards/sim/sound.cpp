#include "../../hal/sound_hal.h"
#include <stdio.h>

void sound_hal_init(void) {}
void sound_hal_tick(void) {}
void sound_hal_chime(void) { printf("[sim] chime! (session reset)\n"); }
void sound_hal_tune(float gain, int volume) { printf("[sim] chime! (gain %.2f, volume %d)\n", gain, volume); }
bool sound_hal_clip(const int16_t* pcm, uint32_t frames, uint32_t rate) {
    (void)pcm;
    printf("[sim] clip (%u frames at %u Hz)\n", (unsigned)frames, (unsigned)rate);
    return true;
}
bool sound_hal_voice(const uint8_t* adpcm, uint32_t samples, uint32_t rate) {
    (void)adpcm;
    printf("[sim] voice (%u samples at %u Hz)\n", (unsigned)samples, (unsigned)rate);
    return true;
}
void sound_hal_voice_stop(void) {}
