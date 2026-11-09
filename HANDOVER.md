# Handover: where the work stands (2026-11-09)

Read CLAUDE.md (rules and the agreed plan), TODO.md (open items) and CODE-MAP.md (where everything is) first.

## Android version (Babylon edition, `Airline-Manager-Pro-Babylon/`)
- Last release: 0.4.6 (walk anywhere in the world, vegetation and ground detail, rain/snow curtains, VIP charter flights,
  ATC radio, own SIDs/STARs for BIAP, the clickable sky map, terrain-aware AI aircraft). 0.4.6 was published without a
  final error check.
- Release flow: bump `app.config.json` and `game/app.config.json` (version and versionCode), add `release-notes/X.md`,
  `npm run typecheck` and `cd game && npx tsc -p tsconfig.json --noEmit`, commit as DroneDirector, fetch and rebase,
  push. `.github/workflows/build.yml` builds the Android APK and the old PC build.
- The 3D viewer's sky dome is `V.sky`; the corner map is `V.skyMap` (reusing `V.sky` made the 3D view black).

## PC version (Unreal Engine 5.7, repo root)
- First commit of the project: `AirportDynastyPro.uproject`, `Config/`, `Source/AirportDynastyPro/`.
- 2026-11-09: switched from 5.8 to 5.7 (the version installed on the owner's PC). Compiles with no changes to the code;
  the editor opens, and running the game logs "Terminal built: 54 rooms, 71 doors" with no errors.
- On Play, `ADPGameMode` adds sun, sky, fog and exposure, builds the grey-box terminal from `Data/terminal-plan.json`
  (`TerminalBuilder`), starts about 100 simple walkers (`CrowdTest`) and spawns the first-person player (`ADPWalker`).
  Start map is the empty `/Game/Maps/Airport` (`Content/Maps/Airport.umap`, Git LFS) for both the editor and the game;
  everything in it is spawned at runtime. Legacy input mappings are in `Config/DefaultInput.ini`.
- The editor rewrites `DefaultEngine.ini` and `DefaultInput.ini` with its own defaults when it opens (and drops the
  comments). Revert those rewrites before committing; only commit config changes made on purpose.
- Pushing: command-line git on the owner's PC has no GitHub login; push through GitHub Desktop (it's signed in).
- Plan metres to Unreal: `FTerminalPlan::ToWorld(x, height, z)` = (x*100, -z*100, height*100) cm.

- Materials are made by Python scripts run headless: `UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript=<file.py>
  -unattended -nullrhi` (close any open editor of UE 5.7 first: its Live Coding blocks every compile). Kit materials:
  `Content/Materials/Kit/` (`M_Surface` + `MI_<Babylon kit name>`); glass: `Content/Materials/M_Glass`.
- The Babylon terminal to port is `Airline-Manager-Pro-Babylon/src/sections/terminal/` (doors.ts, furnish.ts,
  fixtures.ts, lifts.ts, jetway.ts, fids.ts, ceilings.ts); materials in kit.ts, room materials in shell.ts.

## Next steps
The batches and what is done are in ROADMAP.md. Batch 1 continues with the doors, then the main hall's fittings.
Waiting on the owner: security camera screenshots; the ATC tower model.
