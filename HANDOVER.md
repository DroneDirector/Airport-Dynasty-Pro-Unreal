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
  Start map is the engine's empty `/Engine/Maps/Entry`; legacy input mappings are in `Config/DefaultInput.ini`.
- Plan metres to Unreal: `FTerminalPlan::ToWorld(x, height, z)` = (x*100, -z*100, height*100) cm.

## Next steps
1. ~~Compile and open the project on the PC~~ (done 2026-11-09, UE 5.7).
2. Save a real start map in `Content/Maps/` and set it in `DefaultEngine.ini`.
3. Glass material for the glass walls (only frames are built now), then the main hall and one gate with Nanite meshes.
4. Replace `CrowdTest` with Mass Entity crowds (ZoneGraph lanes from the plan, StateTree behaviour).
5. Cesium for Unreal with the free data sources in CLAUDE.md.
6. Waiting on the owner: security camera screenshots; the ATC tower model.
