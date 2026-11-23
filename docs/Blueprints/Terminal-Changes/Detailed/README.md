# Terminal changes: the detailed pictures (to-do 213)

4K. Gold line + NEW = new in the plan, placed from the redrawn sheets (`../v2`). Grey TODAY = there already. BEFORE = the same camera without the new parts.

| File | What it shows |
|---|---|
| D1-Aerial-Forecourt / D1b-...-BEFORE | The whole terminal from the forecourt |
| D2-East-End-Forecourt / D2b-...-BEFORE | The east end from the forecourt |
| D3a-Cutaway-East-Floor2 | The east end cut open: Floor 2, the concourse beside the walkway and gates 13-16 |
| D3b-Cutaway-East-Ground-Floor | The same, ground floor: claim hall, passport control in, staff area, coach station |
| D3c-Cutaway-Concourse-Close | Close: passport wall, passport control, duty free |
| D4-Floor2-Passport-Wall / D4b-...-BEFORE | Along the Floor 2 walkway at the passport wall |
| D4c-Floor2-Passport-Control | The queue hall, booths and e-gates |
| D5-Concourse-Walking-In, D5c-Concourse-Inside, D5d-Concourse-Window-Gallery | The non-Schengen concourse |
| D6-Claim-Hall-Arrivals-Duty-Free | The east claim hall |
| D7-Grand-Hall / D7b-...-BEFORE | The Grand Hall |
| D8-Taxi-Rank | The taxi rank east |
| D9-Double-Skin-Forecourt / D9b-...-BEFORE, D10-Double-Skin-From-Office | The double-skin front from outside and from an office |
| D11-East-Hall | The East Hall |

## What is real and what is not

- **Real geometry from the game:** the terminal (`Data/terminal-layout.json` with the meshes of the terminal model and `Art/TerminalAnnex`, the game's own cuts applied), the jetways, the A320, and in the AFTER pictures the new smooth roof and its pillars (2026-11-23).
- **Modelled for these pictures from the v2 numbers:** every gold part, its furniture, shops and signs. They are not the game's assets (the shell now in the game, `Art/TerminalEast`, is not used here).
- **Simple stand-ins:** road, car park, cars, coaches, taxis, people, grass, plants. An A320 stands at every gate, also at 13-16 (A380 / B777 stands).
- **BEFORE** pictures are renders of the game's geometry with the roof as it was before 2026-11-23, not game screenshots.

## Differences from the first accepted sheets (TC-1 to TC-6)

- The curved roof and pillars over the east wing were there already: TC-4's BEFORE (closed wall, flat roof) was wrong. Today it is open grass under the roof.
- Arrivals go down at each gate into a closed ground-floor corridor. TC-3's Floor 2 corridor and mezzanine are gone (v2).
- The passport control stands beside the walkway; only the glass wall crosses it, at x 240 (v2).
- Only gate 13 swings. Gate 4's outside gallery is dropped and not shown.
- The link gallery runs to x 162, as the shell in the game does, so that it turns into the East Hall (v2 sheets: x 156).

Scripts: `Blueprints/Terminal/Changes/Detailed/` (build_scene.py, then build_new.py, then queue.sh).
