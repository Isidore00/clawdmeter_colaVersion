#pragma once
#include <stddef.h>
#include <stdint.h>

// LZ4 block decoder for the cola layer's frames (cola_assets.h stores each
// frame's palette indices compressed by tools/png_to_cola.js). Arduino-free so
// the host tests can run it.
//
// Decodes exactly `dst_len` bytes of `src` into `dst`. Returns true only if the
// block is well formed and fills `dst` exactly; it never reads past `src_len`
// or writes past `dst_len`.
bool cola_lz4_decode(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len);
