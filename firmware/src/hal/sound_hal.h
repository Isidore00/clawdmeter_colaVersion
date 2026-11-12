#pragma once
#include <stdint.h>

// Optional audio output: the ding (ding.h), played for an attention alert
// and when the session window resets. ES8311 boards stream it through the
// shared chime engine (chime.h); boards without sound no-op on init/tick and
// ignore play requests.
//
// Playback is non-blocking: sound_hal_chime() only *queues* the ding and
// returns immediately, so the LVGL render loop never stalls.

void sound_hal_init(void);
void sound_hal_tick(void);
void sound_hal_chime(void);
// Speaker tuning (`chime <gain> [volume]`): try a gain (0..1) and codec volume (0..100,
// < 0 keeps it) until reboot, and play the ding. No-op without sound.
void sound_hal_tune(float gain, int volume);
// A clip of 16-bit mono PCM for the cola layer (cola.h), only on boards built
// with it (-DCLAWD_COLA=1): the other boards neither call nor define this.
// Non-blocking; `pcm` must stay valid. The ding goes first: false (try again
// later) while one is playing or waiting, or without sound.
bool sound_hal_clip(const int16_t* pcm, uint32_t frames, uint32_t rate);
