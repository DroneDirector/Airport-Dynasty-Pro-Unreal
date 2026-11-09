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
- On Play, `ADPGameMode` adds sun, sky, fog and exposure, builds the terminal from `Data/terminal-plan.json`
  (`TerminalBuilder`), adds the doors, fittings, lifts, jetways, screens and 400 passengers (`TerminalCrowd`, Mass Entities) and spawns the first-person player (`ADPWalker`) with the interface (`UI/ADPHUD`).
  Start map is the empty `/Game/Maps/Airport` (`Content/Maps/Airport.umap`, Git LFS) for both the editor and the game;
  everything in it is spawned at runtime. Legacy input mappings are in `Config/DefaultInput.ini`.
- The editor rewrites `DefaultEngine.ini` and `DefaultInput.ini` with its own defaults when it opens (and drops the
  comments). Revert those rewrites before committing; only commit config changes made on purpose.
- The caches live on drive F: (C: is nearly full): `DefaultEngine.ini` points Zen and the local DDC to
  F:/UnrealCache (keep those lines when reverting the editor's config rewrites); the project's `Intermediate` folder
  is a link to F:/UnrealCache/ADP-Intermediate; the raw terrain data is in F:/ADP-Terrain.
- Pushing: command-line git on the owner's PC has no GitHub login; push through GitHub Desktop (it's signed in).
- Plan metres to Unreal: `FTerminalPlan::ToWorld(x, height, z)` = (x*100, -z*100, height*100) cm.

- Materials are made by Python scripts run headless: `UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript=<file.py>
  -unattended -nullrhi` (close any open editor of UE 5.7 first: its Live Coding blocks every compile). Kit materials:
  `Content/Materials/Kit/` (`M_Surface` + `MI_<Babylon kit name>`); glass: `Content/Materials/M_Glass`.
- The Babylon terminal to port is `Airline-Manager-Pro-Babylon/src/sections/terminal/` (doors.ts, furnish.ts,
  fixtures.ts, lifts.ts, jetway.ts, fids.ts, ceilings.ts); materials in kit.ts, room materials in shell.ts.

- Releasing the PC edition: set `pc-version.json`, write `release-notes-pc/<version>.md`, commit and push; the
  workflow `unreal-pc.yml` builds on the owner's PC and publishes the release `pc-unreal-v<version>`. The build copies
  `Data/` into `Content/Data/` (staged with the game; ignored by git); `TerminalPlan` reads it from there first.

## The Babylon model as the terminal (in progress, 2026-11-09)
- Exported: `F:/ADP-Export/terminal.glb` (117.8 MB, 17,825 nodes, 2,739 meshes, 708 materials, 355 textures) by
  `Tools/BabylonExport` (`node Tools/BabylonExport/export.mjs`, then open http://localhost:8642/).
- Imported as assets: `Content/Babylon/Terminal/terminal/{StaticMeshes,Materials,Textures}` (terminal_mesh_<glTF mesh
  index>), Nanite on, plus `SceneImport_terminal`.
- NOT YET IN THE MAP: `Tools/import_babylon_model.py` (headless, -nullrhi) places all 17,826 actors via Interchange
  `import_scene` + `wait_until_all_tasks_done`, but saving `/Game/Maps/Airport` does not keep them (the .umap stays
  8 KB). Without -nullrhi the editor crashes in SlateRHIRenderer while saving. Next: place the model at runtime from the
  glTF node tree (a C++ actor reading the GLB's node transforms and spawning the imported meshes), or save the placed
  actors into a separate level / Level Instance. Then check the axes against `FTerminalPlan::ToWorld` (x, -z, y).
- UPDATE: `ABabylonModel` places the model at runtime from `Data/babylon-terminal.json` (14,855 objects, 2,739 meshes as
  instanced lists): it shows in the game (wave roof, pier, jetways, aircraft, museum dome, trees, car park). Open:
  (1) the model looks mirrored north-south (landside trees and car park on the airside): Babylon's exporter keeps
  left-handed coordinates and Interchange swaps y/z; a root scale of y = -1 turned everything black (lighting), so the
  fix has to mirror the mesh data itself (re-export with the z axis negated in Tools/BabylonExport before saving, e.g.
  a root node scaled z = -1 baked by the exporter, or Babylon's useRightHandedSystem); (2) everything renders grey
  although the materials carry the right colours and textures (Interchange's MI_Default_* parents): check vertex
  colours, exposure and the material parents.
- `ADPGameMode` already steps Unreal's stand-ins aside when Content/Babylon/Terminal exists: right now that leaves the
  terminal empty in the game until the model is placed.
- Also seen: every kit material renders light grey with a speckle pattern (the MI colours not applied?): check M_Surface.

## Next steps
The batches and what is done are in ROADMAP.md (PC versions 0.5.0 to 0.9.0 committed; release notes in
release-notes-pc/). Next, in order:
1. Batch 6, the mountains: download Copernicus GLO-90 (public AWS bucket copernicus-dem-90m), the Faroese national DSM
   (Føroyakort, keep a copy of its terms with the download date) and ÍslandsDEM 10 m (LMÍ, CC BY 4.0); write the
   converter to compact height tiles (finest wins) and the runtime terrain read from disk, the airport flattened within
   2.6 km and blended to 4.4 km. Then the ocean, swimming and diving.
2. Batch 7: the Kokoro voice for the announcements (the ten steps in the Kokoro PDF).
3. Batch 4's open part: the bridge from the game (web view) to Unreal for the real flights on the screens and traffic.
4. Waiting on the owner: Unreal 5.8 installed (reminder set for 16:00 on 2026-11-09), then move the project to 5.8
   before the MetaHumans; the build machine registered (Tools/RegisterBuildMachine.ps1); security camera screenshots;
   the ATC tower model.
