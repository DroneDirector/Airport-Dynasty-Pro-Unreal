# Stop note - 2026-11-21 morning (the night's work while the owner slept, full control)

Everything below is committed and pushed to GitHub (code, tools, data, pictures). **Nothing is published as a game
release**: your rule of 11-20 22:50 ("do not publish until everything on the to-do list is ready") - the list still has
the multi-day items 188-191. The game is packaged for the PC as **0.18.0** (F:/ADP-Packaged/0.18.0); pc-version.json says
0.18.0 on this PC only (not pushed - the updater must not see it before a real release exists). The release script itself
(Tools/release_pc.sh) was blocked by the app's safety check tonight: the release with its upload needs your go.

## Done tonight
| # | What | State |
|---|------|-------|
| 172 | Our airports (BIAP, LSCH) in the simulation's world list | built, self-test PASS |
| 173 | New game makes your career in the simulation at the site; Continue/Load picks it up and catches up | built, self-test PASS |
| 174 | Boards, gate screens and boarding calls from the simulation's real flights (yours and AI airlines') | built (self-test: "19:08 CHB 980 London, gate 9") |
| 175 | Simulation messages, the 06:00 briefing and news on the phone | built |
| 176 | The Crew screen on the simulation | built |
| 177 | EASA flight time limits rostering | built, self-test PASS |
| 178-180 | Promotions, candidates, interviews, adverts, contracts, notice, severance, holidays | built |
| 181-183 | Maintenance checks, ground handling teams, stands and gates | built |
| 184-186 | Connections, frequent flyers, alliances, shares/IPO, fuel hedging, challenges, league | built |
| 192 | Save now (F5, Esc question), named saves under Load game | built |
| 193 | Always the real weather (METAR near the airport) | built |
| 194 | No outline round your own character | none in the pictures |
| 195 | Clouds: the sky capture time-sliced (first step; measuring is next) | built |
| 197 | ICAO fire cover; fire station staffed from day one | built |
| 199 | Night curfew, the town, and the syndic | built |
| 163 | Updater downloads never start over | built |
| 167 | The Overview shows Chablais | built |
| 168 | Swiss flags: our own cloth simulation in the real wind | pictures |
| 170 | Bilingual signs (French / English) + "Bienvenue / Welcome to Chablais Airport"; the boards say "Departures · Départs" on Swiss time | pictures (New_08, New_09) - the old English letters no longer show through |
| 171 | Mountain names layer on the holographic map | picture (New_10_Holomap_Mountains) |
| 188 | The new Overview: one live dashboard on the simulation | first version (no demo APK: you waived the look-first stop) |
| 191 | Aircraft in 3D, first steps: parked at their stands, pushback, taxi on the centre lines (nose wheel on the line, 25/10/5 kt), take-off; arrivals land and taxi in | pictures (docs/Pictures/Chablais/Night-2026-11-21/Aircraft3D_*) |
| 200 | 1.81 million real trees, seasonal | pictures (summer, autumn, winter) |
| 161 | Walking test at Chablais (terminal, forecourt, coaches; day and night) | done: 41-48 fps in the EDITOR at 4K (not the packaged game) |

Pictures: docs/Pictures/Chablais/Night-2026-11-21 (4K) and docs/Pictures/Chablais/Trees (tree checks over the photo).

## Still open
- 188 (the rest), 189 native screens, 190 sync and live tracking, 191 (the rest: lights, control surfaces, nose-up
  attitudes, ATC clearances, holding short, the tug; 196/198 pushback crew) - the multi-day items.
- 160 pyramid check, 162 title flash (the film now syncs once at the start and only corrects 0.6 s+ drift - watch a
  normal start), 195 measuring.
- 169 showcase: all 24 shots retaken (docs/Pictures/Chablais/Showcase-2026-11-21).
- The trees up close still look a little stylised: a leaf-texture pass with reference photos is next.
- 165 and 166 are done already (pack folder choice exists; the cube icon by the phone chat) - they go out with the release.

## Found and fixed at the end of the night
- The terminal's timetable screens (departures/arrivals walls, the wide screens: 15 screens) were sunk inside their own
  dark housings - the Unreal copy of the terminal came out mirrored - so they showed as dark panels, at Iceland too. Now
  brought out in front of the housing at start, the right way up. Worth a look at Iceland next time you are there.
- The boards at Chablais said "Brottfarir" and showed this PC's clock (an hour off Swiss time): fixed.
- Aircraft in 3D checked again: pushback, taxi along the curves on the centre lines, waiting at the holding point,
  take-off (pictures Aircraft3D_Taxi_*).

## The overnight batch (to-do 164)
1. French photo tiles redone - done.
2. The 28 GB of photos into ~1 GB packs - DONE 03:24 (24 packs + manifest, F:/ADP-Terrain/packs-chablais).
3. Chablais terrain packs from tiles-v2 - running since 03:24 (F:/ADP-Terrain/packs-v3, log pack-v4.log, "finished" at its end).
4. The game packaged as 0.18.0 - see below.
