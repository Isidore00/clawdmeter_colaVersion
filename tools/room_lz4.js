#!/usr/bin/env node
// stdin: raw bytes -> stdout: LZ4 block, made with the same compressor
// tools/png_to_cola.js uses for the cola frames (so firmware/src/cola_lz4.cpp
// decodes it). Used by tools/room_convert.py; it lifts the two functions out
// of png_to_cola.js so there is only one implementation to keep in sync.
const fs = require('fs');
const path = require('path');
const src = fs.readFileSync(path.join(__dirname, 'png_to_cola.js'), 'utf8');
const a = src.indexOf('// LZ4 block format'), b = src.indexOf('function rgb565');
if (a < 0 || b < a) throw new Error('png_to_cola.js layout changed: cannot find the LZ4 functions');
const { lz4Compress, lz4Decode } = new Function(src.slice(a, b) + ';return {lz4Compress,lz4Decode};')();
const raw = fs.readFileSync(0);
const out = lz4Compress(raw);
const back = lz4Decode(out, raw.length);
if (Buffer.compare(Buffer.from(back), raw)) throw new Error('LZ4 round trip failed');
fs.writeSync(1, Buffer.from(out));
