# Pushback sequence (owner 2026-11-21)

Order of work: finish the pushback tug as a whole first, then rework the pushback into these 11 steps.
- Every step waits for the one before it.
- Before each step there is a random 2-5 s pause, so it doesn't feel scripted.
- Radio chatter between the crew, the pilot and ATC sounds like a real radio. The interphone line between the crew and the pilot sounds like a headset.

| # | Step | Who | Words (typical) | Wait after |
|---|---|---|---|---|
| 1 | Tug in, towbar on the nose gear, steering bypass pin in | crew | - | 2-5 min, before the doors close |
| 2 | Headset plugged into the nose interphone, check | headset <-> pilot | "Cockpit, ground." / "Ground, cockpit, go ahead." | ~30 s |
| 3 | Brakes set, doors closed, pin confirmed | pilot <-> headset | "Brakes set, doors closed, confirm bypass pin in?" / "Pin in, ready for push." | ~30 s |
| 4 | Pilot requests pushback | pilot -> ATC | "Ground, [callsign], stand 9, request pushback." | ATC replies in 10-60 s (longer with traffic) |
| 5 | ATC approves, with the direction | ATC -> pilot | "[callsign], pushback approved, face east." | - |
| 6 | Pilot tells the crew, brakes released | pilot <-> headset | "Cleared to push, face east, brakes released." / "Commencing push." | the push starts only after "brakes released" |
| 7 | Push: straight back, then the turn; wing and tail walkers; engine start | pilot <-> headset | "Cleared to start engines." | 2-4 min |
| 8 | Push complete, brakes set | headset <-> pilot | "Push complete, set brakes." / "Brakes set." | ~10 s |
| 9 | Towbar off, tug clears, headset off | headset <-> pilot | "Disconnecting headset, watch for my signal on the left." | 1-2 min |
| 10 | Headset man walks out left of the nose, shows the bypass pin, thumbs up | hand signal | - | 20-30 s |
| 11 | Pilot requests taxi; ATC gives the route | pilot <-> ATC | "Ground, [callsign], ready to taxi." / "Taxi via ..." | taxi starts only when everything is clear |

## Hard rules (in code, not just timing)
- The push never starts before "brakes released".
- The tug never disconnects before "brakes set".
- The headset man (the pushback handler) shows the bypass pin to the cockpit from outside before the aircraft may taxi.
- The pilot never taxis until all crew and equipment are clear and visible.

## Lights (animated as on a real tug)
- Low beams, tail lamps and markers while it runs.
- Brake lamps when slowing.
- Indicators blink in turns.
- Amber beacons flash.
- Reversing lamps and the beeper when backing.

## Sources
- SKYbrary - Pushback
- Bristol Airport OSI GO-017
- Learn ATC
- PPRuNe phraseology thread
- NASA ASRS Callback 515
- UK CAA "What is he doing?"
