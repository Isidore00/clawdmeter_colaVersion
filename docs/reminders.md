# Reminders (cola builds)

Small nags on a wall-clock schedule, on the boards built with
`-DCLAWD_COLA=1` (the AMOLED-2.16 and the simulator). There is one program so
far, drinking water; standing up and resting your eyes can join it as more
rows in the list.

| Program | When | What you see and hear |
| --- | --- | --- |
| `water` | every 15 min, Monday to Friday, 08:00 to 20:00 (the last at 19:45) | the ding, then her drink line; on the room page she drinks, elsewhere the face comes up and drinks through a straw |

Settings are fixed in the firmware for now (`REMINDER_PROGRAMS`,
`reminder_policy.cpp`); app settings would need protocol keys on both sides.

## How it works

- **Clock**: the app's local epoch `t` (`docs/protocol.md`). The app sends it
  only while its **Clock** setting is on (Auto, 12 h or 24 h; the default is
  Off), so turn it on or no reminder ever fires. The day and minute
  come straight from it and the board keeps counting between payloads. Before
  the first `t` since boot there are no reminders. A program fires on its
  period's boundaries counted from midnight (`:00 :15 :30 :45`), once per
  boundary that starts inside its window; a clock that first arrives at 10:07
  waits for 10:15 (`reminder_clock_due`, native-tested in
  `test_reminder_policy`).
- **Sound**: the ding first, then her line (`room_drink`, `RV_DRINK`), played
  as soon as the ding lets go of the speaker (given up after 4 s). Her voice
  switch (the room page's, NVS `clawdmeter/room_voice`) mutes the line; the
  ding always rings.
- **Screen**: on the room page she holds her drinking act (`room_remind`).
  Anywhere else the director's `remind` beat sends whoever is out away (one
  Clawd), brings the face up, or uses the one that's up, and the face drinks:
  eyes shut "‿", a glass with a bent straw, the water going down over the
  minute. Then a blink and it rests as the screensaver. Under an alert or in
  minimal mode there's only the ding and the line. A swipe to or from the
  room moves the reminder along.
- **Answering**: a tap anywhere (or a button) ends it; so does a hold, which
  also switches her voice. On the room page that
  tap isn't a poke.
- **The panel**: a reminder holds the panel on without counting as you being
  here (`idle_remind_begin`). If it woke a dark panel, the panel goes dark
  again 3 s after the tap that answered it (`IDLE_REMIND_ACK_MS`), or at once
  when it ends unanswered after a minute (`REMIND_SHOW_MS`). Anything else
  you do meanwhile (another press, a prompt in Claude Code) keeps it awake
  the usual way. If the panel was already on, it just carries on.

## The screen on the cola builds

- The panel dims and sleeps after **10 minutes** without a touch or a button,
  on USB power as on battery, every day (`idle_cfg.h`). Other boards keep 30
  minutes and stay on while plugged in.
- **Only you at the board counts**: a touch, a button or an alert. Claude Code
  working (prompts, tool calls, usage moving, "back after a gap") does not
  restart the 10 minutes or keep the face away, and doesn't send it down
  either (`face_without_link` in `clawd_policy.h`, `main.cpp`). An alert still
  wakes the panel and brings the meters.
- The face is the resting screen: it comes up after **30 s** of quiet, with
  or without the app connected (`face_without_link`), and a peek stays up
  until you tap it. Not on the room page. With no app it covers the pairing
  hint until you tap it.
- The face's eyes (and the glass and the sign) drift one step round a small
  ring once a minute while it's up, against burn-in.

## The voice switch on the face

A press held about a second without moving, on the meters or the face, turns
her lines on or off (the room page has its own hold, anywhere but on her).
The face answers with a sign: a little white board on a stick comes up from
the bottom with a speaker on it, waves for sound on or a red slash for muted,
and goes back down after a moment. On the meters he comes up first, as for a
tap. A hold is never a tap: touch on the director becomes a tap when the
finger lifts, if it stayed within 24 px.

## Serial commands (QA)

| Command | |
| --- | --- |
| `remind water` | that reminder now (any day, any time) |
| `remind stop` | end it as if answered |
| `remind status` | clock known, weekday (0 Sunday), minute of the day, whether one is up, and when each program next fires |
| `room voice on\|off` | her lines (also what the long press switches) |

To check the drinking face on hardware: `remind water`, wait for the face to
come up, then `screenshot top` (the face lives on the top layer).
