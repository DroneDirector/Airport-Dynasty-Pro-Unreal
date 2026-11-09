# Roadmap: the PC version (Unreal) in batches

Each batch is one release: built and compiled piece by piece, committed and pushed as it goes, published once when
the whole batch is done. Each part is first rebuilt to look and work like the Android (Babylon) edition, then upgrades
are offered. Sources: CLAUDE.md (the agreed plan), the PDFs in the repo, the Babylon code.

## Batch 1: the terminal you can walk around in (done; 0.5.0 publishes once the build machine is registered)
- [x] Unreal 5.7, project compiles and opens; empty start map `Maps/Airport`
- [x] Glass walls: frames and clear glass panes
- [x] The building kit's look: each room's floor, walls and ceiling in the Babylon kit's materials (patterned textures such as tile grout, wood grain and herringbone still to come)
- [x] Doors: sliding glass, revolving, swing, secure staff, double, one-way and jetway doors that open as you walk up, the room's name over each door
- [x] Main hall fittings: check-in desks 1–24 with belts, banners, chairs and queue lines, 8 bag drops, trees, benches, bins and signs; seats, gate desk and sign at all 8 gates
- [x] The other rooms' fittings (security lanes, lounge, offices, Operations, Academy, crew room, toilets, baggage hall, Food Walk, eateries, Tax Free, bar, family room, view bays, walkway, kerb) and the ceiling lights
- [x] Stairs with oak and walnut steps, glass sides, handrails and a glass balustrade round the opening; openings cut through Floor 2 and the ceilings; the 5 lifts called, opening and riding between the floors
- [x] The jetways J-1 to J-4 (fixed corridor, rotunda, three telescopic tunnel sections, bellows, cab with canopy, drive unit), docked, walkable; docking and undocking come with the aircraft in batch 5
- [x] Timetable screens at all 23 places (hall walls, totems, pairs, departure screens, gate screens), showing the Android edition's sample flights round the real time until the shared world arrives (batch 4)
- [x] Release pipeline for the PC: `.github/workflows/unreal-pc.yml` builds, packages and publishes on the owner's PC (self-hosted build machine), only when `pc-version.json` has release notes in `release-notes-pc/`; catches up every 3 hours; the installer installs over the previous version. Waiting on the owner: register the PC once (`Tools/RegisterBuildMachine.ps1`)
- Publish: version 0.5.0 (PC) together with the next Android release

## Batch 2: people in the terminal (done: 0.6.0)
- [x] Passengers as Mass Entities (replaces CrowdTest), 400 to start
- [x] A walking map made from the plan (rooms joined by doors, floors by stairs) and the shortest way through it
- [x] A departing passenger's trip: kerb, check-in desk, security lane, a stop upstairs, the gate, aboard; a new passenger arrives for each one gone
- [x] Cheaper redrawing for people far away
- Later: arriving passengers (their way out through the terminal), keeping clear of furniture and each other (Mass avoidance), Smart Objects for seats, shops and queues, thousands of people, MetaHuman bodies

## Batch 3: the interface (walking mode done: 0.7.0)
- [x] The holographic map as in Babylon (small, turned with you; full screen with rooms' names and click-to-route)
- [x] The clean top-left bar, Hide UI (H; double-click or Esc back)
- [x] Settings (quality, render scale, frame limit, VSync, performance overlay, scan my PC, problem log); the device scan on first start
- [x] The performance overlay; the problem log (Saved/Logs/Problems.log)
- Moved to batch 4 (they need the game's world running here): the game's own menus and screens (first in Unreal's web browser view, then screen by screen), the problem report sent from the game
- Later: the holographic map's offered upgrades (real glow and shimmer, scan line, 3D tilt with stacked floors, live dots for people and aircraft)

## Batch 4: one shared world with Android (first part done: 0.8.0)
- [x] The game's own code (every menu and screen) running in the PC version in Unreal's web browser view (G), served from a local secure address; built by the PC release from the Android edition and staged with the game
- [x] One world through the game's existing cloud sync (Supabase, encrypted on the device; one device runs the world, the other shows it live)
- [ ] A bridge from the game to Unreal: the timetable screens and the passengers from the world's real flights; airport mode writes back
- [ ] The automatic one-day check that both setups give exactly the same world
- [ ] The problem report sent from the game

## Batch 5: the airport site (done: 0.9.0)
- [x] The airfield from the Android edition's airfield plan (Data/airfield.json): runway 08/26 with markings, taxiways A/B with links and curves, aprons, holding positions, cargo building, grass inside the boundary
- [x] Aircraft at stands 1–8: turnaround with fuel, catering and baggage vehicles, pushback, taxi out, holding, take-off on 08, away, 3° approach, landing, rollout, taxi in; one runway user at a time
- [x] Volumetric clouds
- Later: proper aircraft and vehicle models, the jetways docking, the real flights and aircraft from the shared world, runway 26 in east winds, rain, snow and lightning, cargo and stand 9 traffic

## Fixes seen in the first pictures (2026-11-09), before batch A
- [ ] THE TERMINAL IS THE BABYLON MODEL ITSELF (owner, 2026-11-09; geometry untouched: no extra lines, cuts, merges or
      remeshing, original normals and UVs kept so it stays easy to texture; only Unreal's rendering on top (Nanite, Lumen,
      materials, effects); new details only when the owner asks): no new modelling, nothing rebuilt; the whole Babylon scene
      (interior, doors, stairs, pillars, wave roof, jetways, rental centre, everything) exported as glTF and imported
      into Unreal with Nanite as the terminal; Unreal's own grey-box builders step aside (kept switchable). Method: Babylon
      app built with Node.js, a local page builds the terminal and exports it with @babylonjs/serializers, posting the
      GLB to a local server that writes it to F:/ADP-Export; imported by Python (Interchange, Nanite on). Earlier note: export the
      Babylon scene's static shell (terminal structure and glass, the wave roof with skylights and tree columns, kerb
      canopies, the rental car centre, the museum, the pier, other buildings) as glTF from the built app (Node.js to
      build, Babylon's glTF serializer in the page), import into Unreal with Nanite on (Interchange, by Python), and use
      it in place of the grey-box shell; the moving parts stay Unreal's own (doors, lifts, people, screens, jetways)
- [ ] (covered by the import) The Grand Hall's wave roof as on Android (plan.ts WAVE, waveSouth, waveH): x 24–126, its south edge sweeping
      out over the kerb (z −7 to −16), height 18.4 ± 3.4 m in waves 30 m long, white on top, warm timber under it, three
      lens skylights along the crests, the eight tree columns; the hall's walls rising to meet it (no flat roof)
- [ ] The airfield's ground comes out light grey all over (grass, aprons and the dark runway alike): check the materials
- [ ] The terminal's white walls render too dark: check lighting and exposure

## Batch 6: the globe (in progress)
- [x] Cesium for Unreal 2.31.0 (fetched by Tools/GetCesium.ps1), the airport placed at its real spot and turned to the runway's 068°, the Sentinel-2 2016 imagery with its credit on screen
- [ ] The mountains, shipped with the game (no streaming), decided by the owner 2026-11-09:
  - the whole world: Copernicus GLO-90 (90 m, free commercial use with credit), about 15–25 GB shipped
  - the Faroe Islands: the national elevation model (DSM, 2/5/10 m) from Umhvørvisstovan via Føroyakort, open data
    licence with the credit "Data from the Faroese Environment Agency, Umhvørvisstovan" + dataset name + download date
    (keep a copy of the terms as downloaded: they can change)
  - Iceland: ÍslandsDEM from Landmælingar Íslands (CC BY 4.0), at 10 m (the 2 m version would be about 20 GB for
    Iceland alone)
  - a one-time converter to compact height tiles (finest available tiles win), read from disk; the airport's ground
    flattened within 2.6 km and blended out to 4.4 km as on Android
- [ ] The ocean as real water (PC only), right after the mountains, decided by the owner 2026-11-09:
  - Unreal's Water system (with the engine; Cesium already includes it): a moving ocean round the player with waves,
    sky reflections and Lumen light, the swell with the wind, calmer or rougher with the weather
  - meeting every coastline in the world, from the OpenStreetMap coastlines (ODbL, credit) and the shipped depths:
    waves steepening and breaking in the shallows, foam along the shore and round rocks and sea stacks
  - swimming: waves lifting and dropping you, swell and currents pushing you; diving in 3D with floating back up and
    the water's drag; underwater haze darkening with depth, light shafts and rippling light on the sea floor, muffled
    sound; climbing out onto beaches and rocks; an optional breath meter
  - with the swimming, the player's own 3D body (arms and legs swimming)
- [ ] The people as MetaHumans, ready by the time the swimming is in (owner, 2026-11-09):
  - every AI person (passengers, crew, pilots, staff) gets their own face, made once from a seed and kept with that
    person in the shared world, so no two look alike: a large set of MetaHumans generated from MetaHuman Creator's
    presets (blended faces, skin tone, age, hair, eyes, build), our own clothes and uniforms
  - real skin (MetaHuman's scanned skin textures with pores and fine detail), never plastic or clay-like
  - faces alive: blinking, eyes looking round, small expressions; lip movement with the Kokoro voice later
  - standing still: a gentle, real sway (breathing, small weight shifts), never leaning on nothing; sitting: calmer,
    breathing and small head and hand movements, no excessive sway
  - detail by distance with no visible popping (owner, 2026-11-09), researched:
    - near: the full MetaHuman (LOD0-1, strand hair, full face animation); always the best quality closest to you
    - further: lower LODs cross-faded (dithered LOD transitions, smoothed by TSR) instead of switching; Nanite on the
      skinned bodies where the materials allow (continuous detail like Nanite buildings; MetaHuman faces use
      translucency, so the face stays on normal LODs); hair handed from strands to cards to meshes with a fade
    - far: the Mass crowd's cheap instanced representation (Epic's MetaHuman Crowds, Unreal 5.8), faded in, same body
    - a quality floor: nobody in view drops below a set LOD; the number of full-quality people is limited instead
    - animation updated less often far away but interpolated (Animation Budget Allocator), so it never stutters
    - Epic's MetaHuman Crowds and MetaHuman Collections are in Unreal 5.8 (experimental): move the project to 5.8
      (it was made for 5.8) before this part
- Cesium for Unreal with the free data (Sentinel-2 2016 imagery, Copernicus/AWS terrain, OpenStreetMap water)
- The airport placed at 63.457988 N, 18.713703 W, 40 m (130 ft), runway heading 68°, the ground flattened within
  2.6 km and blended out to 4.4 km exactly as in the Babylon edition
- World Partition streaming and zones that load by distance

## Batch 7: sound and life (in progress)
- [x] Office and timetable screens you can use: E in front of one opens the airline game on it
- [x] Announcements: boarding and final calls from the aircraft at the stands, the two-tone gong, a caption, a queue so calls never overlap
- [ ] The Kokoro voice and the loudspeaker sound (the ten steps in the Kokoro PDF)
- Waiting on the owner: security cameras (the screenshots); the ATC tower (the model)

## Throughout: Nanite and streaming like the Matrix Awakens demo (owner, 2026-11-09)
- Nanite for the terminal and every building and object where it helps (the big terminal and its detail, the cargo
  building, jetways, aircraft at rest, vehicles, terrain props); not for glass and other see-through materials
- World Partition streaming in cells (128 m round the airport, 512 m for the perimeter, 2,048 m for the terrain) and
  HLOD for distant buildings (simplified merged meshes, never picture impostors), so detail fades in and out smoothly
  with distance, as in the Matrix Awakens demo
- The building kit's pieces become proper Nanite meshes (first the main hall and a gate), instanced so the terminal
  can still grow in play

## Later
- MetaHuman people, and your own body in walking mode everywhere
- Terminal extensions placed in play (snap-together pieces), HLOD for distant buildings
