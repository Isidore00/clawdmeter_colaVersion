#pragma once
#include <stdint.h>

// Optional TF (microSD) card, for boards that have a slot wired up
// (BoardCaps.has_sd). A board without one gets the weak defaults in
// sd_hal_default.cpp: nothing is present and every call is a harmless no-op.
//
// Nothing touches the card at boot (a missing card would cost the boot a
// delay). The card is mounted the first time something asks for it, and every
// call after a failed mount tries again, so a card pushed in later is found.
// A missing or unreadable card is never an error on screen: callers just get
// false / -1.

void sd_hal_init(void);                 // set up the bus pins only; no card access
bool sd_hal_present(void);              // mounts if needed; true when a card is mounted
void sd_hal_status(void);               // `sd status`: one JSON line on Serial
void sd_hal_raw(void);                  // `sd raw`: card init + sector 0 bytes, no FAT (diagnostic)
void sd_hal_ls(void);                   // `sd ls`: the files in /, one JSON line each
// Reads up to `len` bytes of `path` from `offset` into `buf`. Returns the
// bytes read (0 at the end of the file), or -1 when there is no card or no such
// file. Blocking; for loading assets, not for the render loop.
int32_t sd_hal_read(const char* path, uint32_t offset, uint8_t* buf, uint32_t len);
