#include "hal/sd_hal.h"
#include <Arduino.h>

// Weak defaults: boards without a TF slot (or without the driver) implement
// nothing and report no card.
__attribute__((weak)) void sd_hal_init(void) {}
__attribute__((weak)) bool sd_hal_present(void) { return false; }
__attribute__((weak)) void sd_hal_status(void) { Serial.println("{\"sd\":{\"supported\":false}}"); }
__attribute__((weak)) void sd_hal_ls(void) { Serial.println("{\"sd\":{\"supported\":false}}"); }
__attribute__((weak)) int32_t sd_hal_read(const char*, uint32_t, uint8_t*, uint32_t) { return -1; }
