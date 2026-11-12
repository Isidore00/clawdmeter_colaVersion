#!/usr/bin/env node
// Placeholder art for the cola layer: one flat colour block per state with the
// state's name, the frame number and a row of dots that steps with the frame,
// plus three short tones. Writes assets/cola/cola_<state>_<nn>.png and
// assets/cola/cola_<state>.wav; real art replaces them under the same names
// (tools/png_to_cola.js then regenerates firmware/src/cola_assets.h).
//
//   (cd tools && npm install --no-save pngjs)     # once
//   node tools/make_cola_placeholders.js

const fs = require('fs');
const path = require('path');
const { PNG } = require('pngjs');

const OUT = path.join(__dirname, '..', 'assets', 'cola');
const W = 160, H = 160;

// name, frames, block colour
const STATES = [
    ['idle',    4, 0x4A6FA5],
    ['prompt',  4, 0x7B5EA7],
    ['working', 6, 0x2E8B57],
    ['reply',   4, 0xD97757],
    ['low',     4, 0xC9A227],
    ['empty',   2, 0xC0392B],
    ['reset',   6, 0x2AA7C9],
];

// 5x7 glyphs, only what the labels need.
const FONT = {
    D: ['11110', '10001', '10001', '10001', '10001', '10001', '11110'],
    E: ['11111', '10000', '10000', '11110', '10000', '10000', '11111'],
    G: ['01110', '10001', '10000', '10111', '10001', '10001', '01111'],
    I: ['01110', '00100', '00100', '00100', '00100', '00100', '01110'],
    K: ['10001', '10010', '10100', '11000', '10100', '10010', '10001'],
    L: ['10000', '10000', '10000', '10000', '10000', '10000', '11111'],
    M: ['10001', '11011', '10101', '10101', '10001', '10001', '10001'],
    N: ['10001', '11001', '10101', '10011', '10001', '10001', '10001'],
    O: ['01110', '10001', '10001', '10001', '10001', '10001', '01110'],
    P: ['11110', '10001', '10001', '11110', '10000', '10000', '10000'],
    R: ['11110', '10001', '10001', '11110', '10100', '10010', '10001'],
    S: ['01111', '10000', '10000', '01110', '00001', '00001', '11110'],
    T: ['11111', '00100', '00100', '00100', '00100', '00100', '00100'],
    W: ['10001', '10001', '10001', '10101', '10101', '11011', '10001'],
    Y: ['10001', '10001', '01010', '00100', '00100', '00100', '00100'],
    0: ['01110', '10001', '10011', '10101', '11001', '10001', '01110'],
    1: ['00100', '01100', '00100', '00100', '00100', '00100', '01110'],
    2: ['01110', '10001', '00001', '00010', '00100', '01000', '11111'],
    3: ['11110', '00001', '00001', '01110', '00001', '00001', '11110'],
    4: ['00010', '00110', '01010', '10010', '11111', '00010', '00010'],
    5: ['11111', '10000', '11110', '00001', '00001', '10001', '01110'],
    6: ['00110', '01000', '10000', '11110', '10001', '10001', '01110'],
    7: ['11111', '00001', '00010', '00100', '01000', '01000', '01000'],
    8: ['01110', '10001', '10001', '01110', '10001', '10001', '01110'],
    9: ['01110', '10001', '10001', '01111', '00001', '00010', '01100'],
    '/': ['00001', '00001', '00010', '00100', '01000', '10000', '10000'],
};

function put(png, x, y, rgb, a = 255) {
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    const i = (y * W + x) * 4;
    png.data[i] = (rgb >> 16) & 0xFF;
    png.data[i + 1] = (rgb >> 8) & 0xFF;
    png.data[i + 2] = rgb & 0xFF;
    png.data[i + 3] = a;
}

function rect(png, x0, y0, x1, y1, rgb) {
    for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) put(png, x, y, rgb);
}

function roundRect(png, x0, y0, x1, y1, r, rgb) {
    for (let y = y0; y <= y1; y++) {
        for (let x = x0; x <= x1; x++) {
            const cx = x < x0 + r ? x0 + r : x > x1 - r ? x1 - r : x;
            const cy = y < y0 + r ? y0 + r : y > y1 - r ? y1 - r : y;
            if ((x - cx) ** 2 + (y - cy) ** 2 <= r * r) put(png, x, y, rgb);
        }
    }
}

function disc(png, cx, cy, r, rgb) {
    for (let y = cy - r; y <= cy + r; y++)
        for (let x = cx - r; x <= cx + r; x++)
            if ((x - cx) ** 2 + (y - cy) ** 2 <= r * r) put(png, x, y, rgb);
}

function textWidth(s, scale) { return s.length * 6 * scale - scale; }

function text(png, s, cx, y, scale, rgb) {
    let x = Math.round(cx - textWidth(s, scale) / 2);
    for (const ch of s) {
        const g = FONT[ch];
        if (g) {
            for (let r = 0; r < 7; r++)
                for (let c = 0; c < 5; c++)
                    if (g[r][c] === '1') rect(png, x + c * scale, y + r * scale, x + (c + 1) * scale - 1, y + (r + 1) * scale - 1, rgb);
        }
        x += 6 * scale;
    }
}

function darker(rgb) {
    return (((rgb >> 16) & 0xFF) * 0.6 << 16) | (((rgb >> 8) & 0xFF) * 0.6 << 8) | ((rgb & 0xFF) * 0.6);
}

function frame(name, n, count, rgb) {
    const png = new PNG({ width: W, height: H });   // zero-filled: transparent
    roundRect(png, 8, 8, 151, 151, 18, rgb);
    text(png, name.toUpperCase(), W / 2, 44, 3, 0xFFFFFF);
    text(png, `${n + 1}/${count}`, W / 2, 80, 2, 0xFFFFFF);
    // One dot per frame; the current one is lit and sits a little higher.
    const gap = 18, x0 = Math.round(W / 2 - (count - 1) * gap / 2);
    for (let i = 0; i < count; i++) {
        if (i === n) disc(png, x0 + i * gap, 120, 7, 0xFFFFFF);
        else         disc(png, x0 + i * gap, 124, 5, darker(rgb));
    }
    return PNG.sync.write(png);
}

// 16-bit mono WAV of a few notes, each with a short fade in and out.
function wav(notes, rate = 22050) {
    const samples = [];
    for (const [hz, secs] of notes) {
        const n = Math.round(secs * rate), fade = Math.round(0.01 * rate);
        for (let i = 0; i < n; i++) {
            const env = Math.min(1, i / fade, (n - 1 - i) / fade);
            samples.push(Math.round(Math.sin(2 * Math.PI * hz * i / rate) * env * 0.3 * 32767));
        }
    }
    const buf = Buffer.alloc(44 + samples.length * 2);
    buf.write('RIFF', 0); buf.writeUInt32LE(36 + samples.length * 2, 4); buf.write('WAVE', 8);
    buf.write('fmt ', 12); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(1, 22);
    buf.writeUInt32LE(rate, 24); buf.writeUInt32LE(rate * 2, 28); buf.writeUInt16LE(2, 32); buf.writeUInt16LE(16, 34);
    buf.write('data', 36); buf.writeUInt32LE(samples.length * 2, 40);
    samples.forEach((s, i) => buf.writeInt16LE(s, 44 + i * 2));
    return buf;
}

fs.mkdirSync(OUT, { recursive: true });
let files = 0;
for (const [name, count, rgb] of STATES) {
    for (let n = 0; n < count; n++) {
        const file = `cola_${name}_${String(n + 1).padStart(2, '0')}.png`;
        fs.writeFileSync(path.join(OUT, file), frame(name, n, count, rgb));
        files++;
    }
}
fs.writeFileSync(path.join(OUT, 'cola_reply.wav'), wav([[880, 0.12], [1175, 0.16]]));
fs.writeFileSync(path.join(OUT, 'cola_empty.wav'), wav([[440, 0.14], [330, 0.22]]));
fs.writeFileSync(path.join(OUT, 'cola_reset.wav'), wav([[523, 0.1], [659, 0.1], [784, 0.1], [1047, 0.18]]));
console.log(`${files} placeholder frames and 3 tones written to ${OUT}`);
