#!/usr/bin/env node
// Cola layer assets -> firmware/src/cola_assets.h.
//
// Reads assets/cola/cola_<state>_<nn>.png (frames, played in number order) and
// the optional assets/cola/cola_<state>.wav, for the states in STATES below
// (the order of ColaState in firmware/src/cola_policy.h). Every frame must be
// the same size.
//
// Frames are stored one byte per pixel: an index into that frame's own palette
// of up to 255 colours (RGB565 + 8-bit alpha); index 0 is transparent. A frame
// with more colours than that is reduced by median cut. Sound is 16-bit mono
// PCM at any rate the firmware can double up to the codec's (22050 Hz).
//
//   (cd tools && npm install --no-save pngjs)     # once
//   node tools/png_to_cola.js

const fs = require('fs');
const path = require('path');
const { PNG } = require('pngjs');

const SRC = path.join(__dirname, '..', 'assets', 'cola');
const OUT = path.join(__dirname, '..', 'firmware', 'src', 'cola_assets.h');
const STATES = ['idle', 'prompt', 'working', 'reply', 'low', 'empty', 'reset'];
const MAX_COLORS = 255;
const ALPHA_MIN = 8;          // below this a pixel is simply transparent
const CLIP_MAX_S = 2.0;

function rgb565(r, g, b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

// Median cut over the frame's distinct colours, weighted by how often each is
// used. Returns the palette and, per distinct colour, its palette index.
function quantize(colors) {
    let boxes = [colors];
    while (boxes.length < MAX_COLORS) {
        let best = -1, bestRange = 0, bestCh = 0;
        boxes.forEach((box, i) => {
            if (box.length < 2) return;
            for (let ch = 0; ch < 4; ch++) {
                let lo = 255, hi = 0;
                for (const c of box) { if (c.v[ch] < lo) lo = c.v[ch]; if (c.v[ch] > hi) hi = c.v[ch]; }
                if (hi - lo > bestRange) { bestRange = hi - lo; best = i; bestCh = ch; }
            }
        });
        if (best < 0) break;
        const box = boxes[best].slice().sort((a, b) => a.v[bestCh] - b.v[bestCh]);
        const total = box.reduce((s, c) => s + c.n, 0);
        let cut = 0, seen = 0;
        while (cut < box.length - 1 && seen + box[cut].n <= total / 2) seen += box[cut++].n;
        if (cut === 0) cut = 1;
        boxes.splice(best, 1, box.slice(0, cut), box.slice(cut));
    }
    const palette = [];
    boxes.forEach((box, i) => {
        const total = box.reduce((s, c) => s + c.n, 0);
        const avg = [0, 1, 2, 3].map(ch => Math.round(box.reduce((s, c) => s + c.v[ch] * c.n, 0) / total));
        palette.push(avg);
        for (const c of box) c.index = i + 1;
    });
    return palette;
}

function convertFrame(file, size) {
    const png = PNG.sync.read(fs.readFileSync(file));
    if (!size.w) { size.w = png.width; size.h = png.height; }
    if (png.width !== size.w || png.height !== size.h)
        throw new Error(`${path.basename(file)} is ${png.width}x${png.height}; the other frames are ${size.w}x${size.h}`);
    const total = png.width * png.height;
    const distinct = new Map();
    const keys = new Array(total);
    for (let i = 0; i < total; i++) {
        const r = png.data[i * 4], g = png.data[i * 4 + 1], b = png.data[i * 4 + 2], a = png.data[i * 4 + 3];
        if (a < ALPHA_MIN) { keys[i] = null; continue; }
        // What the panel can show: RGB565, and alpha as stored.
        const key = (rgb565(r, g, b) << 8) | a;
        let c = distinct.get(key);
        if (!c) { c = { v: [r, g, b, a], n: 0, index: 0 }; distinct.set(key, c); }
        c.n++;
        keys[i] = c;
    }
    const colors = [...distinct.values()];
    let palette;
    if (colors.length <= MAX_COLORS) {
        palette = colors.map((c, i) => { c.index = i + 1; return c.v; });
    } else {
        palette = quantize(colors);
    }
    const px = new Uint8Array(total);
    for (let i = 0; i < total; i++) px[i] = keys[i] ? keys[i].index : 0;
    return {
        px,
        rgb: [0, ...palette.map(v => rgb565(v[0], v[1], v[2]))],
        alpha: [0, ...palette.map(v => v[3])],
        reduced: colors.length > MAX_COLORS ? colors.length : 0,
    };
}

function readWav(file) {
    const buf = fs.readFileSync(file);
    if (buf.toString('ascii', 0, 4) !== 'RIFF' || buf.toString('ascii', 8, 12) !== 'WAVE')
        throw new Error(`${path.basename(file)} is not a WAV file`);
    let fmt = null, data = null;
    for (let p = 12; p + 8 <= buf.length;) {
        const id = buf.toString('ascii', p, p + 4), len = buf.readUInt32LE(p + 4);
        if (id === 'fmt ') fmt = { tag: buf.readUInt16LE(p + 8), ch: buf.readUInt16LE(p + 10), rate: buf.readUInt32LE(p + 12), bits: buf.readUInt16LE(p + 22) };
        if (id === 'data') data = buf.subarray(p + 8, Math.min(p + 8 + len, buf.length));
        p += 8 + len + (len & 1);
    }
    if (!fmt || !data) throw new Error(`${path.basename(file)} has no fmt or data chunk`);
    if (fmt.tag !== 1 || fmt.ch !== 1 || fmt.bits !== 16)
        throw new Error(`${path.basename(file)} must be 16-bit mono PCM (it is format ${fmt.tag}, ${fmt.ch} channel(s), ${fmt.bits}-bit)`);
    const frames = data.length >> 1;
    if (frames / fmt.rate > CLIP_MAX_S)
        throw new Error(`${path.basename(file)} is ${(frames / fmt.rate).toFixed(2)} s; keep clips under ${CLIP_MAX_S} s`);
    const pcm = new Int16Array(frames);
    for (let i = 0; i < frames; i++) pcm[i] = data.readInt16LE(i * 2);
    return { pcm, rate: fmt.rate };
}

function cArray(type, name, values, perLine) {
    const lines = [];
    for (let i = 0; i < values.length; i += perLine) lines.push('    ' + Array.prototype.join.call(values.slice(i, i + perLine), ','));
    return `static const ${type} ${name}[${values.length}] = {\n${lines.join(',\n')}\n};\n`;
}

const all = fs.readdirSync(SRC);
const size = { w: 0, h: 0 };
let out = '';
const anims = [], clips = [];
let frameBytes = 0, clipBytes = 0, frameCount = 0;

for (const state of STATES) {
    const re = new RegExp(`^cola_${state}_(\\d+)\\.png$`);
    const files = all.filter(f => re.test(f)).sort((a, b) => parseInt(a.match(re)[1], 10) - parseInt(b.match(re)[1], 10));
    if (!files.length) throw new Error(`no frames for "${state}" (expected ${SRC}${path.sep}cola_${state}_01.png, ...)`);
    if (files.length > 255) throw new Error(`"${state}" has ${files.length} frames; the limit is 255`);
    const entries = [];
    files.forEach((f, i) => {
        const fr = convertFrame(path.join(SRC, f), size);
        if (fr.reduced) console.warn(`${f}: ${fr.reduced} colours reduced to ${MAX_COLORS}`);
        const sym = `cola_${state}_${i}`;
        out += cArray('uint8_t', `${sym}_px`, fr.px, 40);
        out += cArray('uint16_t', `${sym}_rgb`, fr.rgb.map(v => '0x' + v.toString(16).padStart(4, '0').toUpperCase()), 12);
        out += cArray('uint8_t', `${sym}_a`, fr.alpha, 24);
        entries.push(`    { ${sym}_px, ${sym}_rgb, ${sym}_a }`);
        frameBytes += fr.px.length + fr.rgb.length * 2 + fr.alpha.length;
        frameCount++;
    });
    out += `static const cola_frame_t cola_${state}_frames[${files.length}] = {\n${entries.join(',\n')}\n};\n\n`;
    anims.push(`    { cola_${state}_frames, ${files.length} },   // ${state}`);

    const wavFile = path.join(SRC, `cola_${state}.wav`);
    if (fs.existsSync(wavFile)) {
        const clip = readWav(wavFile);
        out += cArray('int16_t', `cola_${state}_pcm`, clip.pcm, 20) + '\n';
        clips.push(`    { cola_${state}_pcm, ${clip.pcm.length}, ${clip.rate} },   // ${state}`);
        clipBytes += clip.pcm.length * 2;
    } else {
        clips.push(`    { 0, 0, 0 },   // ${state}: no sound`);
    }
}

const header = `// ============================================================
// Cola layer assets, generated by tools/png_to_cola.js from
// assets/cola/. Do not edit by hand: change the PNG / WAV sources
// and run the converter again. Include from cola.cpp only (about
// ${Math.round((frameBytes + clipBytes) / 1024)} KB of static data).
// ============================================================
#pragma once
#include <stdint.h>

#define COLA_W ${size.w}
#define COLA_H ${size.h}
#define COLA_ANIM_COUNT ${STATES.length}

// One frame: COLA_W x COLA_H palette indices, and that frame's palette
// (index 0 is transparent).
typedef struct {
    const uint8_t*  px;
    const uint16_t* rgb;     // RGB565 per index
    const uint8_t*  alpha;   // 0..255 per index
} cola_frame_t;

typedef struct {
    const cola_frame_t* frames;
    uint8_t             count;
} cola_anim_t;

// 16-bit mono PCM; pcm is null for a state with no sound.
typedef struct {
    const int16_t* pcm;
    uint32_t       frames;
    uint32_t       rate;
} cola_clip_t;

`;

out = header + out;
out += `// Indexed by ColaState (cola_policy.h).\n`;
out += `static const cola_anim_t cola_anims[COLA_ANIM_COUNT] = {\n${anims.join('\n')}\n};\n\n`;
out += `static const cola_clip_t cola_clips[COLA_ANIM_COUNT] = {\n${clips.join('\n')}\n};\n`;
fs.writeFileSync(OUT, out);
console.log(`${frameCount} frames at ${size.w}x${size.h} (${Math.round(frameBytes / 1024)} KB) and ` +
            `${clips.filter(c => !c.includes('no sound')).length} clips (${Math.round(clipBytes / 1024)} KB) -> ${OUT}`);
