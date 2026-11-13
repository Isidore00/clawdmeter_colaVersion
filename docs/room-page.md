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
| Tap her | a poke, about 1.5 s, then back. By the pokes in the last 10 s: the 1st surprise, the 2nd and 3rd shy, the 4th on angry; 10 s without one starts the count over. A poke during a reaction starts the next one at once |
| Hold her head | a head pat: press the top 30% of her and hold about 0.6 s without moving more than 20 px; two frames alternate for 2.5 s. A finger that moves first is a swipe or drag, never a pat; the release of a pat does nothing else |

The director's screensaver face doesn't start while the room is up (nor does
it come back after an alert); leaving the room restarts its quiet time. An
alert's face still shows.

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
- While eating she stands by the cabinet, at (384, 428) instead of the rug
  (240, 418): as close as she can get with the drawer patch (x 417–466,
  y 283–340) clear in all three frames; `preview_snack_pos.png` shows it.
- Every timing is in `RoomTuning` (`room_policy.cpp`).

## Time of day

The wall clock comes from the app's payload (`t`, the same one that drives the
meters' clock). The AMOLED-2.16 has no RTC the firmware uses, so before the
first payload the room is in daylight. Day, dusk (17:00–19:30, 05:30–07:00)
and night recolour the background palette and every sprite palette
(`room_tint565`), so she is lit like the room. `room day|dusk|night` forces a
tone, `room auto` goes back to the clock.

## Burn-in protection

A still room on an AMOLED is the worst case, so while the page is up:

- **Dimming** (`idle_set_awake_scale`, idle.cpp): the awake brightness is the
  user's level times day 100 %, dusk 60 %, night 25 %. Before the app has sent
  a wall clock (the tone is only "day" by default) it's 60 %. Leaving the room
  puts it back to 100 %.
- **USB sleep** (`idle_set_sleep_on_usb`): at night the usual 30 minute
  auto-sleep also runs on USB power (normally it never sleeps there); a
  button or touch wakes the panel.
- **Drift**: every 3 minutes the whole page (background, patches and her,
  hit boxes included) steps one pixel round a ring of 0-2 px offsets and
  back to (0, 0): 24 minutes a lap.
- `room burnin` prints the brightness scale, tone, clock known, USB-sleep
  permission and the current drift offset.

## Memory

Nothing is allocated until the room is first shown. Then, in PSRAM: the
RGB565 background 460,800 B, one decompression scratch 230,400 B (the
background's indices, then a patch's, then a sprite frame's: one at a time),
and her sprite 116,508 B (RGB565A8): **807,708 B (about 789 KB)**. The art is
flash: 572,028 B of LZ4 blocks (pat frames included).

## Serial commands (QA)

`room on|off|auto|status`, `room day|dusk|night`,
`room idle|game|drink|stretch|read|sleep|snack` (holds the act until
`room auto`), `room poke [surprise|shy|angry|pat]` (a bare `room poke` counts
like a touch), `room pat`, `room say <line>` with the line one of
`poke_surprise poke_shy poke_angry love iloveyou morning night snack drink`
(also prints `{"say":"…","played":…}`; `played` is false when the ding has the
speaker or the build has no voice). Each prints `{"room_cmd":"…","ok":true}`.

## Voice

Nine lines, spoken only while the room is up (the usage page stays quiet),
behind `-DROOM_VOICE=1` (on in the `waveshare_amoled_216` and `sim` envs; its
own switch, apart from `COLA_VOICE`, the usage layer's clips). Without it the
clips aren't compiled in and `room say` reports `voice_built:false`.

| Line | When |
| --- | --- |
| `poke_surprise` / `poke_shy` / `poke_angry` | the poke's reaction (see the table above) |
| `love` or `iloveyou`, at random | a head pat |
| `morning` | the first time she is entered on a day, 05:30-11:00, once the app's clock is known |
| `night` | entering the room at night, or her falling asleep; once per stretch of night |
| `snack` | a snack starts |
| `drink` | a drink starts, one time in three |
| game, stretch, read, idle | silent |

Playing: the ding outranks a line (a line asked for while one plays or waits
is dropped; a ding during a line cuts it off); a new line cuts the one that
is playing; leaving the room stops it. Level is the codec volume the ding
uses (the device has no separate mute: the app's `c` setting only gates the
reset ding).

Data: `tools/room_voice.py` reads `assets/cola_room/room_<line>.mp3` with
ffmpeg, to 16 kHz mono, trims the silence at both ends (25 ms kept), fades the
cut, peak-normalises every clip to 0.70 of full scale and encodes IMA ADPCM
(4 bits a sample, `adpcm.cpp` decodes it) into `firmware/src/room_voice_assets.h`:
100.5 KB for all nine (budget 200 KB). `chime.cpp` (`chime_play_voice`)
decodes while it streams and resamples 16 kHz to the I2S rate by linear
interpolation, so nothing is buffered. The MP3s stay in `assets/`, not in the
firmware.

## Art pipeline

`assets/cola_room_raw/` (the generated pictures, not committed) →
`tools/room_convert.py` (standing poses: idle, game, drink, stretch, surprise, shy, angry, snack, pat) → `assets/cola_room/*.bin` (formats in its docstring)
→ `tools/room_to_header.py` → `firmware/src/room_assets.h` (generated, never
hand-edited; only `room.cpp` includes it).
