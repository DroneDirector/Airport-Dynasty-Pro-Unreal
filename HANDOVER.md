# Handover: where the work stands (2026-11-09; updated 2026-11-11)

Read CLAUDE.md (rules and the agreed plan), TODO.md (open items) and CODE-MAP.md (where everything is) first.

## Fire station and new to-dos (2026-11-11 09:20, from the hangar / fire station chat) — FOR THE PC CHAT
- **Fire station (ARFF), its training tower and two drivable ARFF trucks:** the full brief is `docs/fire-station/SPEC.md`
  (also in the project docs as claude/fire-station-spec.md). Status: design sketch only, shown to the owner; he asked
  for a richer training tower (done in the sketch: drill shaft with floor bands, staggered red-shuttered windows, two
  balconies, a taller charcoal stair shaft with a red crown and the emblem ring, overhanging roof, rappelling arm,
  beacon mast). `docs/fire-station/sketch/` holds the sketch's source (Babylon offscreen renders, a drawing aid only).
  **Do NOT build or publish it until the owner says so.** When he does, start it only after everything else on your
  list is built and published; build it in Unreal C++ the way the hangars are (runtime ISM kit, photo textures made into
  materials by a headless Python script, Lumen lighting at the terminal's strength). Where it stands on the airfield
  is not decided yet: ask the owner.
- **The fire station's textures** are in your working copy at `Art/FireStation/` (16 PNGs, colour `_D` and normal `_N`
  for Board, Silver, Charcoal, RedDoor, Apron, Tower, HallFloor, Asphalt; Asphalt is the hangar asphalt's copy). Git
  LFS could not be pushed from the cloud: commit them (LFS) with your next batch. They are our own, made from the
  owner's reference photos (`docs/fire-station/sketch/maketex.py`). Only make materials from them when the build starts.
- **Copies of the airport in Europe and America:** new in TODO.md ("3. The airport and buildings"): the airport as it is
  at release, with every change before then, placed twice more on the globe. NOT to build yet; the owner gives the
  coordinates (latitude, longitude, heading) later.
- **The hangars:** the section below was the hangar chat's hand-over; 0.15.0 and 0.15.1 appear to have taken it on.
  Tick off what is done there and leave the rest.

## Hangars and the hangar apron (2026-11-11, from the hangar chat) — FOR THE PC CHAT TO FINISH AND PUBLISH
- New: `Source/.../Hangar.{h,cpp}` (four sizes S/M/L/XL + the crew annex, from `Blueprints/Hangars`), the hangar apron
  west of the cargo apron with taxilane K carried west and link Q0 to B (`Data/airfield.json` aprons/taxiways/"hangars";
  the Android plan.ts has the same), three MEDIUM hangars placed (H1 open, H2 shut, H3 open), each one's lead-in line from
  K to its threshold (Airfield.cpp) carried on inside the hall on its centre line (Hangar.cpp), two light masts at each
  hangar's front corners (LightMasts Spots()), one quick-start per hangar in the full-screen holomap (ADPHUD
  GoHangar, built from the hangar list), and `-UIHangars` (screenshots: outside two hangars, inside, the crew lounge,
  dusk outside and inside, then quits).
- The main hangar doors work as real ones: OPEN (green), CLOSE (black) and the red STOP mushroom on a panel inside by
  the west pocket and on the west pocket's front outside (E / click / gamepad X on a button: AHangar::Press from
  ADPHUD::Interact); fully open or shut takes 10 minutes, STOP holds it where it is, the amber beacons flash while it
  moves. Every personnel door (hangar walls, the annex's entrance, rooms and store) swings open away from you as you come
  and shuts behind you; the wall doors sit in a bay's middle, clear of the columns, with real openings in the walls.
- The code is pushed and compiles without the new materials (plain colours stand in). To finish, on the PC:
  1. The textures (Git LFS, could not be pushed from the cloud) are already in your working copy: `Art/Hangar/*.png`
     and `Blueprints/` (confirmed renders and the owner's photos). Commit them.
  2. Close the editor; clear the read-only flag on `Content/`; run
     `UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_hangar.py" -unattended -nullrhi`
     (makes `Content/Materials/Hangar/M_Hangar*` from the textures); commit the new assets.
  3. Build the editor and the game; run the game with `-UIHangars` and send the six `Hangar_*` screenshots to the owner.
  4. Publish the PC release together with your batch: add the hangar notes below to that version's
     `release-notes-pc/<version>.md`. Android 0.4.8 (the apron and hangar footprints on the maps) is pushed already.
- Release notes for the PC version:
  ## Hangars
  - A new hangar apron west of the cargo apron, with taxilane K carried on along it and a new link, Q0, to taxiway B.
  - Three medium hangars (each sized for an Airbus A320) side by side, with room between them. Each has a yellow lead-in line from K to its door that carries on into the hall to the stop bar.
  - Each hangar: dark grey steel frames, white ribbed walls and lining, a glass rooflight down the middle, twin-strip LED lamps as bright as the terminal's hall, a polished concrete floor, and a sliding main door whose leaves, rails, guides, motors and cables are all there. One hangar's door is shut, two are open.
  - A two-storey crew building against each hangar: lobby with steel stairs, corridor from the hall, toilets, crew lounge, store with roll-up doors, and upstairs a kitchen, an office and a mezzanine.
  - Two apron light masts in front of each hangar, at its corners.
  - Each hangar has its own place in the full-screen map: outside, facing its doors.
  - The main doors open and close as real hangar doors do: OPEN, CLOSE and STOP buttons inside and outside (press with E, a click or X on the gamepad). Fully open or shut takes 10 minutes; the beacons flash while they move.
  - The personnel doors (two on each side wall, into the crew building and through its rooms) swing open as you walk up to them.
  - Textures made from photos of a real hangar.
  - Small, large and extra large hangars (A318/Learjet, A321, A380) are built too, ready for airport expansions and swapping a hangar.

## Android version (Babylon edition, `Airline-Manager-Pro-Babylon/`)
- Last release: 0.4.7 (the control tower at site A in airport mode, quick places in the full-screen map). Note: 0.4.7 bumped only `app.config.json`; `game/app.config.json` still says 0.4.6/114, so bump both to the next version together. Before it, 0.4.6 (walk anywhere in the world, vegetation and ground detail, rain/snow curtains, VIP charter flights,
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
- `.gitattributes` marks .uasset/.umap as LFS "lockable", so committed assets are READ-ONLY on disk and Unreal's
  saves fail silently. Clear the read-only flag on Content first (PowerShell: `$_.IsReadOnly = $false`) before any
  script that changes existing assets.
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
  FIXED: orientation (the exporter flips x; the model is turned 180° at placement, no mirroring, no negative scale).
  FIXED: the materials now point at instancing-ready copies of Interchange's base materials (Content/Babylon/Masters,
  Tools/fix_babylon_materials.py); every mesh slot carries its Babylon material (checked). FIXED 2026-11-09: the grey was Unreal's default material: the base materials lacked "used with instanced static
  meshes" (Tools/set_material_usage.py marks every base material and relinks the Babylon chain to Content/Babylon/Masters).
  (was:) everything
  renders grey and the wave roof shows a checker although the materials hold the right colours/textures; next: check
  the copied masters' static switches/texture defaults (the roof's whiteGloss has no texture, so a default texture
  seems to be sampled), and look in the editor viewport. Then the windows: a real glass material (Lumen front-layer
  translucency reflections; strong reflection outside, clear inside, see-through at night). Passengers are off
  (-Crowd brings them back).
  (old) (1) the model looks mirrored north-south (landside trees and car park on the airside): Babylon's exporter keeps
  left-handed coordinates and Interchange swaps y/z; a root scale of y = -1 turned everything black (lighting), so the
  fix has to mirror the mesh data itself (re-export with the z axis negated in Tools/BabylonExport before saving, e.g.
  a root node scaled z = -1 baked by the exporter, or Babylon's useRightHandedSystem); (2) everything renders grey
  although the materials carry the right colours and textures (Interchange's MI_Default_* parents): check vertex
  colours, exposure and the material parents.
- `ADPGameMode` already steps Unreal's stand-ins aside when Content/Babylon/Terminal exists: right now that leaves the
  terminal empty in the game until the model is placed.
- Also seen: every kit material renders light grey with a speckle pattern (the MI colours not applied?): check M_Surface.

## The control tower (added 2026-11-10, Babylon 0.4.7)
- The Babylon edition has the control tower (`Airline-Manager-Pro-Babylon/src/sections/terminal/tower.ts`) at site A
  (plan x -205, y 120, west of the pier), built with the terminal in airport mode: walk-in entrance, a lift from the
  ground to the lift lobby (+13.5 m), spiral stair to the cab (+17.6 m), balcony door with a lift-up desk flap, three
  quick spots (cab, balcony, foot) in the full-screen holographic map. The placeholder tower in `site/index.ts` is gone.
- In Unreal it is built natively, not exported: `TowerADC` (same sizes and place, collision, moving doors, flap and its
  lift through `ATerminalLifts::AddLift`), spawned by `ADPGameMode`; the full map's shortcuts have TOWER: THE CAB,
  THE BALCONY and OUTSIDE, AT ITS DOOR (PC 0.13.2). It is not in the Babylon export, so it is not doubled.

## Now: the interface (owner, 2026-11-09) - see ROADMAP.md "NOW, main priority"
The terminal model rests (it's in and textured). Next: the PC version starts on the Android edition's own title screen
and main menu (its built www/index.html in Unreal's web view, served at https://game.adp.local/), every button and
screen working as on Android; then the bridge so the title screen's walk/airport entries open Unreal's 3D terminal
and back. Babylon www is built with Node.js (installed) by `npm install && npm run build` in Airline-Manager-Pro-Babylon.

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
