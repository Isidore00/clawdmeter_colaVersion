#!/usr/bin/env python3
"""Room layer art -> assets/cola_room/ (indexed, LZ4-compressed, plus previews).

Reads assets/cola_room_raw/ and writes, to assets/cola_room/:

  room_bg.bin, room_palette.bin       background (480x480, 255 colours)
  room_snack_patch.bin                snack cabinet open (patch over the bg)
  room_<read|sleep>_NN.bin            room scenes as patches over the bg
  room_<action>_NN.bin / .png         the 20 standing poses, transparent
  preview*.png, align_check.png       what the above look like

Formats (all little-endian; LZ4 is the block format of tools/png_to_cola.js,
compressed by tools/room_lz4.js):

  room_bg.bin       LZ4( 480*480 palette indices )
  room_palette.bin  255 x u16 RGB565, shared by the bg and every patch
  *_patch.bin,      u16 x, y, w, h (position in the 480x480 room) +
  room_read/sleep   LZ4( w*h indices into room_palette.bin )
  room_<pose>.bin   u16 w, h, u8 ncolours (index 0 = transparent, counted),
                    ncolours x (u16 RGB565, u8 alpha), LZ4( w*h indices )

Needs: pip install pillow numpy scipy; node on PATH. Run from anywhere:

  python tools/room_convert.py
"""
import os
import struct
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
RAW = os.path.join(ROOT, 'assets', 'cola_room_raw')
OUT = os.path.join(ROOT, 'assets', 'cola_room')
LZ4_JS = os.path.join(ROOT, 'tools', 'room_lz4.js')

# --- room scenes --------------------------------------------------------
CROP = 12            # px trimmed from every side of the 1254x1254 art
N = 480              # room size on the device
DIFF_MIN = 40        # max-channel difference that counts as "changed"
SCENES = ['read_01', 'read_02', 'read_03', 'sleep_01', 'sleep_02', 'sleep_03']

# --- standing poses -----------------------------------------------------
POSES = {            # action -> frames
    'idle': 2, 'game': 3, 'drink': 3, 'stretch': 3,
    'surprise': 2, 'shy': 2, 'angry': 2, 'snack': 3,
}
STAND_H = 240        # hair top to sole, px on the device
BG_TOL = 10          # colour distance from the backdrop that is still backdrop
HOLE_MIN = 2500      # enclosed backdrop pieces at least this big (full-res px) are cut out too
PAD_X, PAD_TOP, PAD_BOT = 3, 3, 4


def lz4(raw):
    r = subprocess.run(['node', LZ4_JS], input=bytes(raw), capture_output=True, check=True)
    return r.stdout


def lz4_decode(buf, n):
    o = bytearray()
    ip = 0
    while ip < len(buf):
        t = buf[ip]; ip += 1
        lit = t >> 4
        if lit == 15:
            while True:
                b = buf[ip]; ip += 1; lit += b
                if b != 255: break
        o += buf[ip:ip + lit]; ip += lit
        if ip >= len(buf): break
        off = buf[ip] | (buf[ip + 1] << 8); ip += 2
        ml = t & 15
        if ml == 15:
            while True:
                b = buf[ip]; ip += 1; ml += b
                if b != 255: break
        for _ in range(ml + 4): o.append(o[-off])
    assert len(o) == n, (len(o), n)
    return bytes(o)


def pack_lz4(idx):
    raw = idx.astype(np.uint8).tobytes()
    c = lz4(raw)
    assert lz4_decode(c, len(raw)) == raw
    return c


def rgb565(a):
    a = a.astype(int)
    return ((a[..., 0] & 0xF8) << 8) | ((a[..., 1] & 0xFC) << 3) | (a[..., 2] >> 3)


def from565(v):
    v = np.asarray(v).astype(int)
    r, g, b = (v >> 11) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3
    return np.stack([r | (r >> 5), g | (g >> 6), b | (b >> 5)], -1).astype(np.uint8)


def save(name, data):
    with open(os.path.join(OUT, name), 'wb') as f:
        f.write(data)
    return len(data)


# =========================================================================
# room: background, snack patch, scenes
# =========================================================================
def load_room(name):
    im = Image.open(os.path.join(RAW, name + '.png')).convert('RGB')
    w, h = im.size
    return im.crop((CROP, CROP, w - CROP, h - CROP)).resize((N, N), Image.LANCZOS)


def changed_rect(a, b, thr=DIFF_MIN):
    """Bounding box of the largest connected block where a and b differ by more than thr."""
    d = np.abs(np.array(a).astype(int) - np.array(b).astype(int)).max(2)
    m = d > thr
    # join fragments a pixel or two apart; the box still comes from real pixels
    lab, n = ndi.label(ndi.binary_dilation(m, iterations=2), structure=np.ones((3, 3)))
    if n == 0:
        return None, 0
    lab = np.where(m, lab, 0)
    sizes = np.bincount(lab.ravel())[1:]
    k = int(sizes.argmax()) + 1
    ys, xs = np.where(lab == k)
    return (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1), n - 1


def room():
    sizes = {}
    bg = load_room('room_bg')
    q = bg.quantize(colors=255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = np.array(q.getpalette()[:255 * 3], dtype=np.uint8).reshape(-1, 3)
    p565 = rgb565(pal)
    bi = np.array(q)
    back = lambda i: from565(p565)[i]
    sizes['room_bg'] = save('room_bg.bin', pack_lz4(bi))
    sizes['room_palette'] = save('room_palette.bin', p565.astype('<u2').tobytes())
    Image.fromarray(back(bi)).save(os.path.join(OUT, 'preview.png'))

    def patch(name, img, rect):
        x0, y0, x1, y1 = (int(v) for v in rect)
        idx = np.array(img.quantize(palette=q, dither=Image.Dither.NONE))[y0:y1, x0:x1]
        data = struct.pack('<HHHH', x0, y0, x1 - x0, y1 - y0) + pack_lz4(idx)
        return idx, save(name + '.bin', data)

    # snack cabinet open: same picture, only the drawer differs
    op = load_room('room_snack_open')
    rect, extra = changed_rect(bg, op, 6)
    idx, sizes['room_snack_patch'] = patch('room_snack_patch', op, rect)
    o = bi.copy(); o[rect[1]:rect[3], rect[0]:rect[2]] = idx
    Image.fromarray(back(o)).save(os.path.join(OUT, 'preview_open.png'))
    info = {'snack_patch': (rect, sizes['room_snack_patch'], extra)}

    for s in SCENES:
        im = load_room('room_' + s)
        rect, extra = changed_rect(bg, im)
        idx, sz = patch('room_' + s, im, rect)
        sizes['room_' + s] = sz
        info[s] = (rect, sz, extra)
        o = bi.copy(); o[rect[1]:rect[3], rect[0]:rect[2]] = idx
        Image.fromarray(back(o)).save(os.path.join(OUT, 'preview_%s.png' % s))
    return sizes, info


# =========================================================================
# standing poses
# =========================================================================
def backdrop_model(a):
    """Smooth per-channel fit of the grey backdrop from the frame's own border."""
    h, w, _ = a.shape
    yy, xx = np.mgrid[0:h, 0:w]
    ring = (yy < 8) | (yy >= h - 8) | (xx < 8) | (xx >= w - 8)
    x, y = xx.ravel() / w, yy.ravel() / h
    A = np.stack([np.ones(h * w), x, y, x * x, y * y, x * y], 1)
    m = ring.ravel()
    model = np.zeros_like(a)
    for c in range(3):
        v = a[..., c].ravel()
        mm = m.copy()
        for _ in range(5):       # drop border pixels the figure touches (raised hands)
            coef = np.linalg.lstsq(A[mm], v[mm], rcond=None)[0]
            mm = m & (np.abs(A @ coef - v) < 6)
        model[..., c] = (A @ coef).reshape(h, w)
    return model


def cut_out(path):
    """-> RGBA float image with the backdrop made transparent (edges un-mixed)."""
    a = np.array(Image.open(path).convert('RGB')).astype(float)
    model = backdrop_model(a)
    d = np.abs(a - model).max(2)
    lab, n = ndi.label(d < BG_TOL)
    sizes = np.bincount(lab.ravel())
    touch = set(lab[0]) | set(lab[-1]) | set(lab[:, 0]) | set(lab[:, -1])
    keep = [i for i in range(1, n + 1) if i in touch or sizes[i] >= HOLE_MIN]
    bg = np.isin(lab, keep)
    fg = ~bg
    # drop specks of "figure" (backdrop noise) that are not the figure
    flab, fn = ndi.label(fg)
    fs = np.bincount(flab.ravel())
    fg = np.isin(flab, [i for i in range(1, fn + 1) if fs[i] >= 400])
    # soft edge: a pixel just inside the cut-out is a mix of figure and backdrop.
    # Take the figure colour from the nearest solid pixel and solve for the mix.
    core = ndi.binary_erosion(fg, iterations=3)
    _, (iy, ix) = ndi.distance_transform_edt(~core, return_indices=True)
    f = a[iy, ix]
    band = fg & ~core
    fb = f - model
    den = (fb * fb).sum(2)
    alpha = np.where(den > 25, ((a - model) * fb).sum(2) / np.maximum(den, 1), 1.0)
    alpha = np.clip(alpha, 0, 1)
    alpha = np.where(band, alpha, np.where(fg, 1.0, 0.0))
    alpha[band & (alpha < 0.08)] = 0
    rgb = np.where(band[..., None] & (alpha[..., None] > 0.3),
                   np.clip(model + (a - model) / np.maximum(alpha[..., None], 0.3), 0, 255), np.where(band[..., None], f, a))
    return np.dstack([rgb, alpha * 255]), alpha > 0.5


def orange_hem(rgba, mask):
    """Lowest row with a skirt's worth of orange (>=150 px spread over >=120 px): the skirt hem.
    Shoe flowers and the lanyard are far narrower; pleat shadows are still orange."""
    r, g, b = rgba[..., 0], rgba[..., 1], rgba[..., 2]
    org = mask & (r > 120) & (g > 35) & (g < 175) & (b < 95) & (r - b > 80) & (r - g > 55)
    for y in range(org.shape[0] - 1, -1, -1):
        xs = np.where(org[y])[0]
        if len(xs) >= 150 and xs[-1] - xs[0] >= 120:
            return y
    raise RuntimeError('no skirt found')


def measure(name):
    rgba, solid = cut_out(os.path.join(RAW, name + '.png'))
    ys, xs = np.where(solid)
    top, sole = ys.min(), ys.max() + 1
    x0, x1 = xs.min(), xs.max() + 1
    hem = orange_hem(rgba, solid)
    cx = xs[ys > sole - 0.3 * (sole - top)].mean()
    return dict(name=name, rgba=rgba, top=top, sole=sole, x0=x0, x1=x1, hem=hem, cx=cx, leg=sole - hem)


def quantize_rgba(img):
    """RGBA PIL image -> (indices with 0 = transparent, palette[(r,g,b,a)])."""
    a = np.array(img)
    q = img.quantize(colors=255, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)
    pal = np.array(q.getpalette(rawmode='RGBA')).reshape(-1, 4)
    idx = np.array(q).astype(int)
    pal5 = pal.copy()
    # index 0 reserved for transparent
    clear = a[..., 3] < 8
    out_pal = [(0, 0, 0, 0)]
    remap = {}
    out = np.zeros(idx.shape, np.uint8)
    for k in np.unique(idx[~clear]):
        remap[k] = len(out_pal)
        out_pal.append(tuple(int(v) for v in pal5[k]))
    assert len(out_pal) <= 256
    for k, v in remap.items():
        out[(idx == k) & ~clear] = v
    return out, out_pal


def poses():
    frames = []
    for act, n in POSES.items():
        for i in range(1, n + 1):
            frames.append(measure('room_%s_%02d' % (act, i)))
    # scale on a pose-independent length: sole to skirt hem. The hair-top
    # height of the ordinary standing frames then gives the scale for STAND_H.
    ordinary = [f for f in frames if 'stretch_0' not in f['name'] or f['name'].endswith('_03')]
    ordinary = [f for f in ordinary if f['name'] not in ('room_stretch_01', 'room_stretch_02')]
    ratio = np.median([f['leg'] / (f['sole'] - f['top']) for f in ordinary])
    leg_px = STAND_H * ratio
    for f in frames:
        f['s'] = leg_px / f['leg']
        f['h_out'] = (f['sole'] - f['top']) * f['s']
    # canvas that holds every pose with the centre line and ground line fixed
    half = max(max(f['cx'] - f['x0'], f['x1'] - f['cx']) * f['s'] for f in frames)
    up = max(f['h_out'] for f in frames)
    W = 2 * int(np.ceil(half + PAD_X))
    H = int(np.ceil(up + PAD_TOP + PAD_BOT))
    H += H & 1
    ground = H - PAD_BOT
    sizes = {}
    tiles = {}
    for f in frames:
        y0, x0, x1 = int(f['top']), int(f['x0']), int(f['x1'])
        crop = Image.fromarray(f['rgba'][y0:int(f['sole']), x0:x1].round().clip(0, 255).astype(np.uint8), 'RGBA')
        nw, nh = max(1, round(crop.width * f['s'])), max(1, round(crop.height * f['s']))
        small = crop.resize((nw, nh), Image.LANCZOS)
        canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        canvas.paste(small, (round(W / 2 - (f['cx'] - x0) * f['s']), ground - nh))
        idx, pal = quantize_rgba(canvas)
        rgb = rgb565(np.array(pal)[:, :3]); al = np.array(pal)[:, 3]
        hdr = struct.pack('<HHB', W, H, len(pal))
        body = b''.join(struct.pack('<HB', int(rgb[k]), int(al[k])) for k in range(len(pal)))
        sizes[f['name']] = save(f['name'] + '.bin', hdr + body + pack_lz4(idx))
        # what the device draws: RGB565-rounded colours, 8-bit alpha
        pr = np.concatenate([from565(rgb), al.astype(np.uint8)[:, None]], 1)
        prev = Image.fromarray(pr[idx], 'RGBA')
        prev.save(os.path.join(OUT, f['name'] + '.png'))
        tiles[f['name']] = prev
    return sizes, frames, tiles, (W, H, ground)


def align_check(tiles, W, H, ground):
    """Every action's frames laid over each other on one tile, with the ground and centre lines."""
    acts = list(POSES)
    cols = 4
    z = 2
    tw, th = W * z, H * z
    sheet = Image.new('RGB', (cols * tw, ((len(acts) + cols - 1) // cols) * (th + 18)), (70, 70, 74))
    dr = ImageDraw.Draw(sheet)
    for k, act in enumerate(acts):
        ox, oy = (k % cols) * tw, (k // cols) * (th + 18)
        acc = np.zeros((H, W, 3)); wt = np.zeros((H, W))
        for i in range(1, POSES[act] + 1):
            t = np.array(tiles['room_%s_%02d' % (act, i)]).astype(float)
            a = t[..., 3] / 255
            acc += t[..., :3] * a[..., None]; wt += a
        # each frame at 1/n opacity: sharp where they agree, ghosted where they do not
        base = np.array([110, 110, 116.0])
        n = POSES[act]
        img = base * (1 - wt[..., None] / n) + acc / n
        im = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).resize((tw, th), Image.NEAREST)
        sheet.paste(im, (ox, oy + 18))
        dr.text((ox + 4, oy + 3), '%s x%d' % (act, n), fill=(255, 255, 255))
        dr.line([(ox + tw // 2, oy + 18), (ox + tw // 2, oy + 18 + th)], fill=(0, 220, 255), width=1)
        dr.line([(ox, oy + 18 + ground * z), (ox + tw, oy + 18 + ground * z)], fill=(255, 60, 60), width=1)
    sheet.save(os.path.join(OUT, 'align_check.png'))


def main():
    os.makedirs(OUT, exist_ok=True)
    rs, info = room()
    ps, frames, tiles, (W, H, ground) = poses()
    align_check(tiles, W, H, ground)

    print('== room (bg palette %d B) ==' % rs['room_palette'])
    print('room_bg.bin            %8d B' % rs['room_bg'])
    for k, (rect, sz, extra) in info.items():
        x0, y0, x1, y1 = (int(v) for v in rect)
        print('%-22s %8d B  patch x=%d y=%d w=%d h=%d  (+%d other changed blocks ignored)' % (k, sz, x0, y0, x1 - x0, y1 - y0, extra))
    print('== poses: canvas %dx%d, ground y=%d, centre x=%d ==' % (W, H, ground, W // 2))
    total_pose = 0
    for act, n in POSES.items():
        s = sum(ps['room_%s_%02d' % (act, i)] for i in range(1, n + 1))
        total_pose += s
        hs = ['%.0f' % next(f['h_out'] for f in frames if f['name'] == 'room_%s_%02d' % (act, i)) for i in range(1, n + 1)]
        print('%-9s %2d frames %8d B  (%s)  hair-top height px: %s' % (
            act, n, s, ', '.join(str(ps['room_%s_%02d' % (act, i)]) for i in range(1, n + 1)), ' '.join(hs)))
    scenes = sum(info[s][1] for s in SCENES)
    bg = rs['room_bg'] + rs['room_palette'] + rs['room_snack_patch']
    print('pose total   %9d B' % total_pose)
    print('scene patches%9d B' % scenes)
    print('bg+palette+snack patch %d B' % bg)
    print('ALL          %9d B' % (total_pose + scenes + bg))


if __name__ == '__main__':
    sys.exit(main())
