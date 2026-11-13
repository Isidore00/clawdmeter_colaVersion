# The room page

A second page next to the usage meters: a 480×480 room with Cola standing on
the rug, doing small things on her own. It is built only where the env sets
`-DCLAWD_COLA=1` (the AMOLED-2.16 and the simulator). The usage page, the
director and the usage layer (`cola.cpp`) are untouched: the room is a
full-screen object on top of them, hidden until asked for.

## Using it

| | |
| --- | --- |
| Swipe left on the meters | open the room |
| Swipe right on the room | back to the meters |
| Tap her | a poke: surprise, shy or angry, about 1.5 s, then back |

All three physical buttons already have jobs (HID Space, Shift+Tab,
brightness; users can remap them in the app), so the page switch is a swipe,
not a button.

## What she does (`room_policy.cpp`, native-tested)

- **Idle**: `room_idle` two frames alternating (700 ms each).
- **Daily acts**, after 15–40 s of standing about: `game`, `drink`, `stretch`
  (3 frames, 3–5 loops, then idle) or, by day and at dusk, **reading**: the
  `read` patch variants cycle on the background for 20–40 s.
- **Night** (from the clock, 19:30–05:30): she sleeps (`sleep` patch
  variants) until morning; a poke wakes her for a moment.
- **Hungry** (10 min since the last snack): the next act is the snack: the
  cabinet drawer patch is applied and the `snack` frames loop 3–5 times.
- Every timing is in `RoomTuning` (`room_policy.cpp`).

## Time of day

The wall clock comes from the app's payload (`t`, the same one that drives the
meters' clock). The AMOLED-2.16 has no RTC the firmware uses, so before the
first payload the room is in daylight. Day, dusk (17:00–19:30, 05:30–07:00)
and night recolour the background palette and every sprite palette
(`room_tint565`), so she is lit like the room. `room day|dusk|night` forces a
tone, `room auto` goes back to the clock.

## Memory

Nothing is allocated until the room is first shown. Then, in PSRAM: the
RGB565 background 460,800 B, one decompression scratch 230,400 B (the
background's indices, then a patch's, then a sprite frame's: one at a time),
and her sprite 116,508 B (RGB565A8): **807,708 B (about 789 KB)**. The art is
flash: 552,867 B of LZ4 blocks.

## Serial commands (QA)

`room on|off|auto|status`, `room day|dusk|night`,
`room idle|game|drink|stretch|read|sleep|snack` (holds the act until
`room auto`), `room poke [surprise|shy|angry]`. Each prints
`{"room_cmd":"…","ok":true}`.

## Voice

Off. `room.cpp` keeps `voice_hook()` where a clip for the act that just began
would be asked for, behind the same `COLA_VOICE` switch as the usage layer
(default 0).

## Art pipeline

`assets/cola_room_raw/` (the generated pictures, not committed) →
`tools/room_convert.py` → `assets/cola_room/*.bin` (formats in its docstring)
→ `tools/room_to_header.py` → `firmware/src/room_assets.h` (generated, never
hand-edited; only `room.cpp` includes it).
