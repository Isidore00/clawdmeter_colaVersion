# Face moods (cola builds)

While the orange face is up (the resting screen, see `docs/reminders.md`), it
follows what Claude Code is doing. Cola builds only (`-DCLAWD_COLA=1`); a water
reminder always wins, and the voice sign is unchanged.

| Situation | Face |
| --- | --- |
| a prompt arrives | the eyes widen once, then blink |
| a turn is running, a thinking word (Mulling, Pondering...) | glances up and to the side, slowly back and forth |
| a doing word (Crunching, Tinkering, any word not listed) | the eyes squint "> <" in rhythm |
| an idea word (Conjuring, Ideating...) | a blink, then "^ ^", then eyes up |
| a slow-cook word (Simmering, Stewing...) | half-closed, drowsy, a slow blink now and then |
| a turn finished | "^ ^", caption "All done", 3.5 s |
| a fresh 5-hour window | heart eyes, caption "Fresh start", 3.5 s |
| usage at 90 % or more | a sweat drop; with nothing else going on, small darting eyes |
| no touch for 8 min (2 min before the panel sleeps) | shut eyes, caption "Zzz" |

The status word is picked at random from the meters' status line list
(`ui_verb_at`) and changes every 4.5-7.5 s; the caption under the face is that
word with 1-3 moving dots, dim (the body colour darkened about 27 %) and drifting
with the face for burn-in. Which word is which mood: `face_mood_policy.cpp`
(native-tested in `test_face_mood_policy`, including priorities: asleep, heart,
happy, startled, working, nervous).

"Working" is a prompt with no stop since (given up after 15 min), or hook activity
with no stop since. Only a touch, a button or an alert counts as you being at the
board (docs/reminders.md), so the face still comes up while Claude Code runs.

Code: `face_mood.cpp` decides, `face.cpp` draws (`face_set_mood`,
`face_set_caption`: LVGL shapes and a label, no sprite art). The caption's on/off
switch in the desktop app is not built yet (`face_mood_set_caption_enabled`).

QA: `clawd peek 240` to bring the face up, then
`clawd mood <none|startled|think|work|idea|slow|happy|heart|nervous|asleep>` holds
a mood, `clawd mood auto` lets go, `clawd mood status` prints the state, and
`screenshot top` captures it.
