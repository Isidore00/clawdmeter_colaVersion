#include "cola_lz4.h"

bool cola_lz4_decode(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) {
    size_t ip = 0, op = 0;
    while (ip < src_len) {
        const uint8_t token = src[ip++];

        size_t lit = token >> 4;
        if (lit == 15) {
            uint8_t b;
            do {
                if (ip >= src_len) return false;
                b = src[ip++];
                lit += b;
            } while (b == 255);
        }
        if (lit > src_len - ip || lit > dst_len - op) return false;
        for (size_t i = 0; i < lit; i++) dst[op++] = src[ip++];

        if (ip == src_len) break;                      // the last sequence has literals only

        if (src_len - ip < 2) return false;
        const size_t off = (size_t)src[ip] | ((size_t)src[ip + 1] << 8);
        ip += 2;
        if (off == 0 || off > op) return false;

        size_t len = token & 15;
        if (len == 15) {
            uint8_t b;
            do {
                if (ip >= src_len) return false;
                b = src[ip++];
                len += b;
            } while (b == 255);
        }
        len += 4;
        if (len > dst_len - op) return false;
        // Byte by byte on purpose: a match may overlap its own output (a run).
        for (size_t i = 0; i < len; i++, op++) dst[op] = dst[op - off];
    }
    return op == dst_len;
}
