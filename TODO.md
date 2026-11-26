# To-do: Airport Dynasty Pro (PC, Unreal; and the Android edition)

The one complete list of what is still open, updated 2026-11-11 (after 0.14.7). ROADMAP.md keeps the history and the
details behind each item. Marks: [ ] open, [~] started or partly done, [?] built earlier but the owner should check it.

## 0. Now, in this order (owner, 2026-11-11)
- [ ] **Documents folder and the title video (owner, 2026-11-18; from the PC chat's findings, put on the list by the owner):**
      - [ ] **Documents folder opening by itself:** the game's updater restarts the game through Windows Explorer with a
            forward-slash path (`F:/Airport Dynasty Pro Unreal/AirportDynastyPro.exe`); Explorer doesn't read it and opens
            Documents instead (four updates on 2026-11-18, 0.17.23-0.17.26, fit "often, when it closes or starts"). Fix:
            write the path with backslashes, or start the game directly instead of through Explorer. (The only other
            folder openers are the "Open the logs' folder", "Problem log" and "Open the crash folder" buttons, on click.)
      - [ ] **Title background stutters:** (1) the film snaps to the title page's music clock whenever they drift more
            than 0.08 s apart, and the web page's timers are uneven - give Unreal its own smooth clock that is only
            nudged gently towards the music; (2) the title page (neon logo, scanlines, menus) redraws the whole
            3840x1080 page at up to 60 fps and each frame is copied into Unreal - cut that cost (the 3D world behind the
            title is already off).
      - [ ] **A real background video, light on the PC:** play a proper film (e.g. made from his MP4) behind the page
            with **Bink Video** (recommended; Unreal's own game-video format, very low load) - or the Electra player
            (H.264/HEVC on the RTX 3070 Ti's video chip). Both are built into Unreal 5.7 and free. Keep the smooth clock
            from (1) either way.
- [ ] **FIRST (owner, 2026-11-17 evening): find why performance dropped with 0.17.22** (60-70 fps with frame generation,
      less than before): suspects in HANDOVER.md "performance dropped again" - the jetway lamps' shadow-casting
      lights, the see-through jetway glass, the new stands' parts. Measure, report, fix the code-only ones.
- [ ] **FIRST (owner, 2026-11-16):** complete the jetway in the stress walk; measure performance only without video
      recording, in the shipping build with frame generation on. When the jetway is built, send the owner a preview at night
      with its flashing lights on and wait for his OK. See HANDOVER.md "about the stress walk".
- [ ] **16 gates instead of 8** (owner, 2026-11-17), each with its own aircraft stand; layout suggested to him first;
      then measure performance in the stress walk and report. See HANDOVER.md "16 gates instead of 8".
- [ ] **Dog clip for the owner** (2026-11-15), after the dog's jaw and texture fixes and the creator's pet section:
      walk in, sit, pant (mouth moving, breath vapour), walk out; on a COPY of the she/he selector stage moved to its
      own area (the real one in "Who are you?" stays untouched): dark blue/turquoise, fog, no light bars, well lit;
      full HD, Epic settings; delete the copy when the owner says OK. About 5-8 h nonstop. See HANDOVER.md "the dog's animation clip".
- [ ] **NUMBER ONE (owner, 2026-11-14): the wash hose's water jet like Art/Reference/water-jet-reference.jpg** - one solid glassy column, clear whitish water (NOT the reference's red tint, that's only its lighting), with the reference's flecked, streaked texture racing along it very fast, perfectly connected to the nozzle at its exact width, breaking into spray further out. HANDOVER.md, top section.
- [ ] **Self check-in kiosk in the main hall** (owner, 2026-11-14): model ready in Art/Kiosk; reflective screen like the windows; placed from data near the hall's middle with a clear zone. HANDOVER.md.
- [ ] **NEXT, right after the t-shirt / wardrobe work** (owner, 2026-11-13; ahead of everything else below):
      **puddles and wet ground, the fast way** (material-based like The Matrix Awakens, about 4-7 h): see HANDOVER.md
      "NEWEST: STOP AND READ". Replaces the older wetness-map plan. With a "Rainy day" switch so the owner can see
      them react; **published as its own release straight away** (owner's wish; remind him of the rule).
- [ ] Set up Unreal's MCP server for Claude Code first (about 30 min; speeds up everything below; HANDOVER.md).
- [ ] **After the puddles, FIRST: faraway crowds (AnimToTexture, ~1 day) and reactive sound (MetaSounds, ~1 day)**
      (owner, 2026-11-13). Why first: the biggest difference to how alive the airport looks and sounds for the least
      work, no dependence on the road layout or an engine upgrade, little risk.
- [ ] Then the same way as the puddles: runway/grass edges, tyre marks and cracks in the ground materials (4-8 h).
- [ ] Road network with MetaRoad Free (roads only, from the owner's map plans by script; ~28-44 h), then our own
      module for scheduled taxis, buses and cars (16-24 h) and staff/crew commutes by shift (6-10 h). HANDOVER.md 3b.
      Free version approved; ask the owner before buying Pro.
- [ ] City Sample's cars (~half a day), then traffic with Mass (1-2 days, lanes generated from the layout file), then
      the home town with PCG (1-3 days + Unreal 5.8 upgrade). Details in HANDOVER.md.
- [ ] **FIRST AFTER THE METAHUMAN WORK** (owner, 2026-11-12): finish the MetaHuman people COMPLETELY first (builder,
      uniforms, crowds, every role's actions). Then, as the very next job before anything else on this list: the
      **use outline**. Look at something you can use and HOLD E (use): a crisp amber outline (with a thin light edge)
      traces only that object's outer silhouette, fading in and out, gently pulsing; a small prompt beside the
      crosshair shows the E key and the object's name ("E  Bag drop point 1"). Not shown when E isn't held or you
      look away; nothing else outlined. Every usable object, now and later, gets it automatically. In Unreal: custom
      depth/stencil on the aimed object plus an outline post-process material. The owner saw a demo on 2026-11-11
      (bag drop point 1)
- [ ] The old rooms' OBJECTS (never the room itself: walls, floor, ceiling, window, door stay the airport's) placed in the
      terminal's rooms so they look right: Office 1 (yours, with the pet: dog or cat as chosen, its bed, bowls and
      football) and Offices 2-4, Operations, the Academy; the court once the owner says where; the hangars wait
- [ ] Footsteps by surface with a volume slider
- [ ] The rest of the graphics sliders and toggles in Settings
- [ ] Unreal trees in place of every outdoor tree (licence allowing a paid game)
- [ ] World Partition / HLOD (distant buildings fade, no popping) and a performance pass
- [ ] **NUMBER 2 - right after everything above in this section is done (owner, 2026-11-20): the world rules on their
      own JavaScript engine inside the game, and the menus moved to Unreal's interface screen by screen.**
      - [ ] **The world rules out of the hidden web view:** the shared TypeScript world (NPC airlines and their traffic,
            timetables and inbound flights with their gates and stands, crew and staff rotas and shifts, duties such as
            the gate agent at the jetway console about 20 min before an arrival, passengers as data, the economy) runs on
            a small JavaScript engine embedded in the game, from the start and whether or not a menu is open - no longer
            only in the menus' web browser, which today is made the first time the menus open. Same code as Android, so
            the editions never drift apart; much lighter than a whole browser (memory, background work). About 15-30 h
            nonstop. Unreal's side stays C++ (Mass Entity, StateTree, Smart Objects): the NPCs leaving home, driving or
            taking the bus, walking in, being at the stand on time (walking adjusted to the world's times), animations.
      - [ ] **The menus as Unreal's own interface, screen by screen** (about 2-6 h nonstop each), each one asking the
            embedded world for its data; when the last screen is moved, the web view goes completely.
      - Goes with section 7 (the bridge, the one-day check in both setups, one save in the cloud). Why: the only
        Babylon-era code still running in the PC game is the menus' web view (the buildings are all Unreal models);
        NPC schedules need a world that always runs.

## 1. Distribution: updates, installer, selling
- [x] **Updater shows its speed and a progress bar** (owner, 2026-11-10): while updating the game, the download speed
      (MB/s) and a progress bar; the same for the world's terrain download. The percentage sits IN the bar, centred,
      readable all the time: one colour over the empty part, another where the filled part has reached it (the text
      changes colour exactly where the bar's edge passes, so it's clear both before and after the bar gets there)
- [ ] **Real patches** (xdelta): an update holds only the changed bytes, applied to the player's own files and checked
      afterwards; much smaller updates, worthless without the bought game. Needed above all for chunk 0 (225 MB, mostly
      Unreal's own shaders) and the program file (167 MB), which today go out whole when anything in them changes
- [ ] **The split installer with the whole world inside**: Setup.exe plus .bin pieces under 2 GB each (Inno Setup disk
      spanning), the terrain and land cover installed with the game, no download button needed (the Settings download
      stays for updates of the world)
- [ ] **Branded installer** (Inno Setup with the game's look: logo, artwork, colours, wording). Keep the install folder
      and the installer's AppId as they are, so later installers upgrade existing players
- [ ] Later, maybe: a fully custom installer / launcher
- [ ] **Code signing**: Azure Trusted Signing (about $10/month, sign only when releasing) once sales start; until then
      unsigned (Windows' "unknown publisher" warning) or through Steam
- [ ] **Where the game is sold and downloaded from**, decide: Steam (hosting, updates, ownership check), or Cloudflare R2
      with download links only for buyers (licence check in a Cloudflare Worker), or itch.io
- [ ] **Two repositories**: the code in a private repository; a public one with only patches and world packs (the full
      game itself never on GitHub)
- [ ] Faster world download: fetch 4-6 packs at once in the game's downloader (now one at a time, about 13 Mbit/s)
- [ ] Updates still large until real patches are in: 0.14.2 -> 0.14.3 was 475 MB (the program and chunk 0 go out whole)
- [ ] Maybe: the ATC tower as real mesh assets (same shapes) so it gets its own chunk; today its shape is in the
      program and its materials in the materials chunk

## 2. The world (globe, terrain, sea, land cover)
- [x] Land cover seen on screen; the globe's pictures in the right place (0.14.4: the tile rows were counted from the
      wrong pole, so the ground showed the far southern ocean)
- [x] Satellite imagery checked: EOX answers again; Sentinel-2 cloudless 2016 is CC BY 4.0 (paid game OK; later years
      are not, so we stay on 2016). Still streamed online; shipping it offline is for later (many GB)
- [x] Grass on the real ground by the land cover (grass, farmland, scrub, moss, wetland; none on sand, rock, snow, water)
      and the terrain solid all round you (committed, not released yet)
- [x] Walk anywhere on Earth: Settings, 12 places or a latitude and longitude, put down on the real ground, the airport
      set aside, back with one button (committed, not released yet)
- [x] Release the two items above (next build)
- [ ] **The airport moved to Switzerland: the Chablais plain** (owner, 2026-11-20; planned, the site studied in the
      phone chat). Centre 46.3624 N, 6.9145 E, 374 m, on the Rhône plain between Noville, Chessel and Roche (canton of
      Vaud), runways 34/16 (true 336°), room for a second runway 1,035 m out (independent landings), straight-in
      approaches clear about 12 km both ways (over Lake Geneva / up the plain), ground within about 6 m under the
      whole airport. The town is built where Rennaz / south Villeneuve are (the real buildings there removed), about
      4 km from behind the terminal by the existing back road, never over ~1.3 %, levelling up to ~4.5 m. Maps:
      `docs/Switzerland/` (copied to the PC); scripts `Tools/switzerland/` (swisstopo DHM200 heights; the Rhône line
      in them is approximate). In this order:
      - [ ] **Licences first:** swisstopo's free geodata (OGD) terms, free also for commercial use, the source must be
            named: check the exact credit wording for SWISSIMAGE, swissALTI3D and swissTLM3D on swisstopo's terms
            page (and the OSM wiki's note about imagery), then THIRD-PARTY-NOTICES.md and the on-screen credit.
      - [ ] **Real roads and buildings:** download OpenStreetMap (ODbL) and/or swissTLM3D for the map area; the
            airport joined to the real road network, the motorway/railway/villages it covers cleared or moved,
            the town's buildings placed on the real street pattern where Rennaz is.
      - [ ] **SWISSIMAGE stored in the game, not streamed:** download once, cut into a local tile pyramid, shipped
            with the game or as our own download pack (like the terrain packs): 10 cm round the airport, the road
            and the town (~1 GB), 50 cm for the rest of the ~47 x 44 km map (~2 GB; 25 cm would be ~7-9 GB);
            Cesium shows these where they exist and the streamed Sentinel-2 everywhere else.
      - [ ] **swissALTI3D heights** (0.5 m laser) converted to the game's local terrain for the map area, the
            streamed terrain beyond; the airport and town keep their own ground.
      - About 20-35 h nonstop for the imagery and heights part (tool, tiling, packing, Cesium layers, credit).
- [ ] Rock, sand and snow as ground detail on the real terrain (now only the pictures show them; grass is the only 3D layer)
- [ ] The land cover / relief in the 2D map (the game's menus)
- [ ] The ocean, rest of the plan: Unreal's Water system round the player, swell with the wind and weather, waves
      breaking on every coast (OSM coastlines, ODbL), foam round rocks
- [ ] Swimming and diving (0.15.0 plan): waves moving you, diving, underwater haze and light, climbing out, breath meter,
      your own 3D body swimming
- [ ] Distance and speed: World Partition, HLOD for distant buildings and terrain, fading instead of popping; a
      performance pass for every card (the game only suggests settings, never forces)
- [x] Terrain from far away: thin steps / walls along some edges (where Iceland's or the Faroes' own model meets the world's,
      or tile seams); check and smooth in MakeTerrain.py
- [x] Walk anywhere: the ground stays blurry for about half a minute in a new place while the picture tiles load (show a
      short loading note, or wait for the sharp tiles before putting you down)

- [ ] **Trees and plants with Nanite Foliage (own release; approved 2026-11-15, NOT started: wait for the owner's go).**
      Unreal 5.7's Nanite Foliage (Nanite Assemblies + Nanite Skinning + Nanite Voxels, experimental) with Lumen, and
      the Dynamic Wind plugin (wind by bones, not World Position Offset). Our own plants, made in the Procedural
      Vegetation Editor or Blender (no marketplace assets unless the licence is checked first). Distant trees turn into
      Nanite voxels (real 3D geometry, not picture impostors). Turn on: Project Settings > Rendering > Nanite Foliage.
      **First: the downy birch (Icelandic birch, "Atlantic birch") on its own**, then the rest.
      Times are NONSTOP hours (the PC chat working around the clock), as the owner asked:
      - [ ] Nanite Foliage + Dynamic Wind turned on and tested: 3-4 h nonstop
      - [ ] Downy birch, 2-3 size variants (young, medium, old): 4-6 h nonstop  <- FIRST
            (first downy birch incl. the setup above: 7-10 h nonstop, same day)
      - [ ] Mountain birch (low, twisted): 3-4 h nonstop
      - [ ] Rowan (red berries in autumn): 3-4 h nonstop
            (the three trees together incl. setup: 14-18 h nonstop, under 1 day around the clock)
      - [ ] Crowberry heath patches: 2-3 h nonstop
      - [ ] Moss carpets and lumps on the lava fields: 3-4 h nonstop
      - [ ] Lyme grass clumps on sand and verges: 2-3 h nonstop
      - [ ] Wind tied to the game's weather (calm, breeze, storm): 3-4 h nonstop
      - [ ] Planted automatically (PCG) around the airport, the road, the town and the courthouse: 6-8 h nonstop
      - [ ] Seasons: green summer, yellow autumn, bare winter birches: 3-4 h nonstop
      - [ ] Smoothness tuning on the PC: 3-4 h nonstop
      Whole release: 35-48 h nonstop = about 1.5-2 days around the clock (2-3 days with normal pauses). With the Unreal
      MCP connection set up it lands nearer the low end; add 4-8 h nonstop if the experimental feature misbehaves.

## 3. The airport and buildings
- [ ] **The player's villa** (shown to the owner 2026-11-13): bring in Art/Villa (Villa.glb, Nanite, materials redone in
      Unreal) on the player's lot (town plan marker 8), furnish the bedroom wing and garage inside, the wardrobe and the
      pet's things at home. See HANDOVER.md "The player's villa"
- [ ] **The VELDRA car dealership in the home town** (owner approved, 2026-11-13): bring in Art/Dealership
      (VELDRA-dealership.glb, Nanite, materials redone in Unreal from its textures) on the car dealer's lot (town plan
      marker 3), with its full interior (lounge, toilets, offices, reception, staff-only door, workshop), the hero car
      on the showroom turntable turning all the time (one turn every 30 s), the cars for sale, and the pylon sign.
      See HANDOVER.md "The VELDRA car dealership"
- [x] **Entrance signs seen from inside say "Exit"** (owner, 2026-11-10): the signs over the three entrances on the
      inside of the main hall read EXIT WEST, MAIN EXIT and EXIT EAST (outside they stay ENTRANCE WEST, MAIN ENTRANCE,
      ENTRANCE EAST). Check where the Babylon model draws the inner signs' text (sign textures / labels) and change only
      the inside ones; Android edition the same, so both match
- [x] **Entrance doors' glass too bright** (owner, 2026-11-10): the inner sliding doors (door 2) of the Main Entrance,
      Entrance West and Entrance East, and the revolving doors, are far too bright. Give their glass the same material
      and brightness as the big glass windows on the terminal's back (airside) facade, so all the terminal's glass
      matches
- [x] **Entrance canopies' undersides glow** (owner, 2026-11-10): outside the main hall, the ceiling under the canopy
      above Entrance West, the Main Entrance and Entrance East is far too bright (probably a glowing / lamp material,
      or the ToneDown pass missing it). They must not be light sources: give them the same kind of material as the
      kerb's departures sign (same material, not its texture: matte, non-glowing); find which Babylon mesh and
      material it is first
- [x] **Terminal far too bright seen from the apron** (owner, 2026-11-10): standing on the apron looking at the passenger
      terminal (with the light masts lit), the building is washed out white. Bring its brightness down to about 25% of
      now (what lights it from outside: the masts' beams on its face, its glowing windows and lamps, the auto-exposure
      when you stand in the dark) and the light masts' own light to about 30% of now; check by day too, and that the
      ground stays readable
- [x] **ATC tower entrance canopy lights** (owner, 2026-11-10): five or six small recessed spotlights in the underside of
      the little roof over the tower's main entrance, lighting the doorway and the ground round it; on at night (the
      same sensor as the masts and the balcony lights)
- [x] **ATC tower balcony downlights** (owner, 2026-11-10): small recessed spotlights all round the underside of the
      cab's roof overhang, shining down onto the balcony walkway, on at night (with the masts' sensor). Their light
      must not show on the cab's windows (no reflections or glare on the glass: lights excluded from the glass, narrow
      cones aimed away from it)
- [x] **ATC tower lift: walking out** (owner, 2026-11-10): you sometimes get caught between the lift door and the
      floor when walking out. The doors stay open about 10 seconds after arriving (and re-open while someone stands
      in the doorway, as real lifts do); the car's floor level with each landing, no gap or lip to catch on; the doors'
      collision only where the doors really are
- [x] **The ATC tower's lift is far too dark inside** (owner, 2026-11-10): give the car a proper ceiling light (as real
      lift cars: an even panel or downlights), lighting the walls, floor and buttons, following the car up and down
- [?] The light masts (0.14.3): the owner checks them in the game by day and night (beams, ground light, textures)
- [ ] **Outdoor trees**: replace all of them in place with good-looking Unreal trees (kerb rows 42, car rental 22, car
      park), licence allowing a paid game with credit; none removed until then
- [ ] Aircraft at the gates and moving (0.17.0 plan): the Babylon aircraft and vehicles exact, turning round, taxiing,
      take-off and landing; the jetways moving with them
- [ ] **The apron bus into the game: IK and photoreal textures** (owner, 2026-11-23): the airside bus between the terminal
      and the remote stands (`Art/ApronBus/`, built by `Tools/make_apron_bus.py`, sheets in `Blueprints/Vehicles/ApronBus/`).
      - [ ] **IK:** wire up its 112 IK points and pivots (`apronbus-parts.json`): the driver seated with hands on the wheel,
            feet on the pedals and the door switches; MetaHuman passengers standing at the poles and straps (holding on,
            swaying when it brakes and turns), sitting on the 9 seats, putting bags on the racks, a wheelchair or pram space;
            the 14 door leaves swinging out on their pivots, the cab door, the wipers, the front steering, kneeling 80 mm at stops
      - [ ] **Photoreal textures** (licence allowing a paid game, CC0 preferred, listed in THIRD-PARTY-NOTICES.md): white
            painted aluminium body with the blue band, wear and dirt by the dirt system; tinted glass, stainless and yellow
            poles, grey anti-slip vinyl floor with yellow step edges, seat fabric, black rubber and plastics, steel wheels
            and real tyre sidewalls; the lit destination display, door warning lamps, LED strips and flight screens
      - [ ] **Its electric-bus sounds** (owner, 2026-11-23: real-world sounds, realistic for an EV): the 21 sounds are made
            (`Art/Sounds/ApronBus/`, `Tools/make_apron_bus_sounds.py`); wire them up in MetaSounds as SOURCES.md there says:
            no engine idle; motor whine and regeneration pitched with speed; AVAS up to 20 km/h (UN R138); the air tank
            driving the compressor, dryer purge, brakes, kneeling and doors; placed at their sources, duller inside
      - [ ] Its lights, and its route: terminal bus gates to the remote stands and back
- [~] **The taxi bus's front needs adjusting** (owner, 2026-11-23): the minibus taxi at the landside kerb (`Art/TaxiBus/`,
      `Tools/make_taxi_bus.py`, sheets in `Blueprints/Vehicles/TaxiBus/`). Done the same day: the bonnet follows the owner's
      red line (starts at the windscreen foot 1.43 m up, almost flat, one smooth curve down to a vertical front over the
      bumper) and the front made modern (slim LED headlamps with twin projectors swept round rounded corners, a full-width
      light bar over a gloss-black band, a large slatted grille). The owner checks it; any further changes from him
- [ ] **Drive every airport vehicle yourself, as your own avatar, with IK** (owner, 2026-11-23): today only the fire
      trucks can be driven by you (your own MetaHuman from the creator, else the mannequin: FireTruck::Enter and
      TruckDriverRig, two-bone IK for arms and legs, walk to the door, open it by its handle, climb in, sit, hands on
      the wheel, feet on the pedals). Give the same to EVERY airport vehicle: the apron buses, pushback tugs, baggage
      tractors, belt loaders, GPUs, fuel and catering trucks and the rest as they are added - E / X by the driver's door,
      the vehicle's own IK points (the `*-parts.json` / rig files from each builder: seat, back, hands on the wheel or
      controls, pedals, steps, handles) driving TruckDriverRig, each vehicle's own handling (the apron bus: electric,
      kneeling, doors from the cab's switches). IK for ALL driving vehicles, for you and for the NPC drivers alike (the
      airport's hired drivers and operators use the same rig and points), and the taxi bus's and coach's private
      drivers too (you can't drive those: not the airport's).
- [ ] **The airport's livery on every airport vehicle** (owner, 2026-11-23): like the aircraft livery designer
      (Airline-Manager-Pro-Babylon/game/src/screens/livery.ts), an airport livery: body colour, main colour, style, the
      airport's name on the sides; applied at run time to every airport vehicle through the same material slots on each
      model (`*_Livery_Body`, `*_Livery_Main`, the name panels `*_Livery_Name_*`). Default: YELLOW body (the apron bus
      first, owner 2026-11-23) with a dark navy band. The airport's name is written at run time from Data/airfield.json
      (never baked), as on the coach. Private vehicles (taxi bus, coach, cars) keep their own colours.
- [ ] **Your office moves onto the Grand Hall window, furnished as the owner's render** (owner, 2026-11-26: "I want a clean view
      over the Grand Hall"; renders in Blueprints/Terminal/Office/, made by Tools/render_owner_office.py):
      - [x] **The move** (done 2026-11-26: walls, door at x 124.5, glass guard + handrail on the hall window, oak floor; walking map and holomap walls not re-made yet):
            your office F2-OFFICE-1 from x 118.7..131 to x 114..126.3 (same size, 12.3 x 9 m, z 0..9), its west
            wall the Grand Hall window itself (3 of its 10 panes, floor to ceiling) and its south wall the forecourt windows;
            the walkway's dead-end stretch F2-STAFF-WALK (x 114..118.7, z 0..9) goes into it (the walkway north of the
            corridor, z 12..30, stays); Office 2 grows west to x 126.3 (126.3..139.1). Its door from the staff corridor at
            x 124.5 (near the east end), glass partition to the corridor as now. Data/terminal-plan.json, terminal-cuts,
            Art/TerminalAnnex/make_offices_f2.py, Offices.cpp (the pet, phone, wardrobe move with it), the holomap's plan.
      - [ ] **Furnished as the render:** your desk facing the Grand Hall (walnut, black steel sled legs, drawer pedestal), your
            ergonomic chair, the visitor's chair facing you across the desk, a modern laptop (no brand) showing the game,
            desk lamp, desk phone, the radio (to-do of 2026-11-12), notebook, mug, a small plant; a rug; two linear pendants;
            a round meeting table with four chairs by the window; a low sideboard under the forecourt windows (books, a
            model aircraft, a photo); the sofa corner with an armchair and a coffee table; the wardrobe; two pictures; tall
            plants; the dog's (or cat's) bed, bowls and football by the forecourt window. Real, good models (Fab / Quixel
            with a licence for a paid game, or ours from the Blender pieces in the script).
      - [ ] **The visitor's chair - called-in staff** (owner): when you call someone in (for a conduct talk, praise, etc.)
            they come to your door, KNOCK (sound + you hear it), wait for you to say "come in" (a button / key), open the
            door, walk in, close it, sit down on the visitor's chair facing you and look at you (eyes and head on you, look-at
            IK) while you talk; then stand, push the chair in and leave. Whoever sits on that chair is a MetaHuman at the
            HIGHEST quality (owner: "max quality ... because it's only one of them in the room"): LOD 0 forced, strand (groom)
            hair, the highest texture set, full facial rig with blinking, breathing and talk / listen animation - only while
            in your office, back to the normal crowd LODs outside.
- [ ] **The apron bus is APPROVED (owner, 2026-11-23)** - its model (`Tools/make_apron_bus.py`: the airport's yellow livery
      with the airport's name on both sides, photo-scanned paint, thin clear glass) and its sounds. It must be drivable by
      BOTH the airport's own drivers and the owner himself (his avatar from the creator), through the same IK rig as the
      fire trucks (see "Drive every airport vehicle yourself"): walk to the cab's door, open it, sit, hands on the wheel,
      feet on the pedals, the door switches on the dash; the doors, kneeling and the chime worked from the cab.
      - [ ] **New staff job: "Apron bus driver"** (real title; also called airside / ramp bus driver), part of the
            ground crew (ground handling, ramp services) next to fuel, catering, cleaning and ground (ADPStaff wages
            table and ADPGround::TeamNeeds); needs an airside driving permit (part of hiring); one driver per bus on shift,
            the usual three shifts; their wage is an airport cost. Without a driver on shift a bus stays parked.
- [ ] **The apron buses run on the real flight schedule** (owner, 2026-11-23: "follow the actual airplane schedules, so it
      is ready to serve the airplanes and its passengers", from real-world practice, working with the game's sync):
      - [ ] **In the world simulation, not in Unreal:** a new module `FADPApronBusModule` (next to FADPStandsModule in
            ADPGround: every 30 s, MaxCatchUp 2 days) plans and runs the buses from the flights' times (STD / STA, the
            estimated times when late) and their stands. Its state lives in the shared world data (each bus: id, battery,
            state, the flight and trip it serves, where it is and until when; each passenger's stage gets "waiting at
            the bus gate", "on bus N", "at the stand"), so it is saved in the cloud save with the format's version
            number, synced, caught up after time away, and part of the daily two-setup comparison like everything else.
            Unreal only animates what the module says (buses along the bus road at the times it gives, adjusting speed to
            match - the world stays in charge of timing); Android shows the buses as dots on the 2D map / holomap.
      - [ ] **Departures from a remote stand** (times before STD, tunable; real-world sources below):
            - 50 min: a free bus with a driver on shift and enough charge leaves the depot for the bus gate.
            - 45 min: the bus is at the bus gate; 40 min: boarding opens (boarding commonly starts 30-45 min before).
            - each bus leaves for the stand when its registered passengers are aboard (or it is full: 80 passengers a
              bus with hand luggage, tunable, though it holds up to 110); with nobody registered it waits the fixed 2 min.
            - 30 min: the gate closes (easyJet / Ryanair today; others 20 min - an airline setting); the last bus leaves
              then at the latest; no-shows are left behind and offloaded.
            - buses per flight = passengers booked / 80, rounded up (A320 180 seats: 3; A321 220: 3; A350 / B777: 4-5);
              each bus shuttles back for the next load when the flight needs more trips than there are buses.
      - [ ] **Arrivals on a remote stand:** the bus is at the stand 5 min before the aircraft's estimated on-block time,
            waiting outside the red restraint line; it moves in beside the forward left door when the aircraft has
            stopped, its beacon is off and the stairs are on; it leaves when its passengers are aboard (or full) for the
            arrivals bus door; more buses as for departures.
      - [ ] **Planning:** buses are reserved for each flight's time windows in order of time; a missing bus (all busy,
            no driver on shift, battery low) makes boarding late and is logged as the cause of the delay (as the other
            ground teams in ADPGround::Turnaround). Idle buses go back to the depot and charge below 40 % (120 kWh,
            about 1.7 kWh/km: 70 km a charge).
      - [ ] Sources (2026-11-23): easyJet's turnaround (key.aero): gate agents 50 min before STD, the gate closes 30 min
            before; Ryanair's gate closes 30 min before (raised from 20); boarding typically 30-45 min before and gate
            closing 10-20 min (airline-dependent); remote stands use bus lounges / bus gates that may serve more than two
            remote stands (Auckland Airport's stand allocation principles); e.COBUS 3000: 120 kWh, about 70 km per charge.
- [ ] **The buses' rules, fleets and money** (owner, 2026-11-23) - for the apron bus and the taxi bus (and the rules for
      the landside coach where they apply):
      - [ ] **Who rides the apron bus:** only passengers of a flight parked on a REMOTE stand (no jetway), both ways:
            departures from the bus gate in the terminal out to the aircraft's stairs, arrivals from the stairs back to
            the arrivals door. Passengers of a jetway (contact) stand never use it. (The sim already decides contact or
            remote per flight: ADPGround::AssignStand, to-do 183.)
      - [ ] **It waits for every registered passenger:** each bus trip has a list of the passengers assigned to it
            (booked on that flight and given that bus); the doors stay open and the bus does not drive off until all of
            them have boarded. Passengers who never come (missed / no-show) stop the wait when the flight's boarding
            closes (the gate closes about 20 minutes before departure), so a bus can never wait for ever.
      - [ ] **Nobody registered:** the bus still opens its doors at its stop, waits a fixed, realistic dwell of
            2 minutes (game time; a tunable setting), then gives the door chime, closes and drives off.
      - [ ] **The taxi bus the same way:** it waits at the landside taxi rank until all the passengers booked for that
            ride (a group of up to 8 with their bags in the back) have got in; with no one booked it waits the same
            fixed time (taxis wait their turn in the rank and move up; a realistic rank dwell before a car leaves empty:
            5 minutes, tunable), then drives off.
      - [ ] **Fleets at the start of a new game:** 3 apron buses (owned by the airport) and 5 taxi buses (not owned).
      - [ ] **Ownership and the economy:**
            - The APRON BUS is an airport vehicle, owned by the airport, and in the airport's economy like the other
              apron vehicles (ADPGround::BuyVehicle, FuelTrucks / CateringTrucks / Tractors): bought by the player when more
              are needed (the buy list only offers vehicles that have a 3D model in the game; more models follow),
              charged at the depot charger (120 kWh battery, about 70 km per charge, as the e.COBUS 3000; the energy at
              the local electricity price is a running cost), serviced (a yearly service and repairs as running costs),
              and each bus needs a bus driver from the staff (hired, on shifts, paid wages; a bus without a driver on
              shift stays parked, so remote-stand boarding gets slower). Price: no maker publishes one; use about
              USD 650,000 new (our estimate for a 14 m electric apron bus), tunable.
            - The TAXI BUS and the landside coach are NOT the airport's: private operators run them; they cost the
              airport nothing and earn it nothing (later at most a taxi-rank or bus-bay fee, only if the owner asks).
      - [ ] **More remote stands than buses** makes boarding wait for a free bus (as real airports): the bus count, its
            drivers on shift and the round-trip times set how fast remote-stand flights board and deboard, and late
            buses make departures late, like the other ground teams in ADPGround::Turnaround.
      - [ ] **The sound reference** (owner: the demo "is perfect, use that one"): `Tools/make_apron_bus_sound_demo.py`
            and `Art/Sounds/ApronBus/ApronBus-SoundDemo.wav` - the MetaSounds must match its layers, gains and speed curves.
- [ ] **The taxi bus into the game: IK and photoreal textures** (owner, 2026-11-23), as the apron bus above:
      - [ ] **IK:** wire up its 70 IK points and pivots (`taxibus-parts.json`): the driver seated with hands on the wheel,
            feet on the pedals, reaching the taximeter, gear selector and card terminal; passengers getting in through the
            sliding door (grab pole, entry step), sitting in the 8 seats with seat, back, head and buckle points (fastening
            the belts), holding the roof grab handles; the driver loading bags at the rear; a wheelchair up the ramp to its
            space; pivots for the front doors, the sliding door on its tracks, the rear doors, the ramp, wipers and steering
      - [ ] **Photoreal textures** (licence allowing a paid game, CC0 preferred, listed in THIRD-PARTY-NOTICES.md): taxi
            yellow paint with the black checker band, wear and dirt by the dirt system; privacy and clear glass, black
            plastics and gloss-black front band, chrome rings, LED lamp inners; dark seat fabric with bolsters, belts,
            rubber floor mat, grey headliner and trim panels; dashboard plastics, the lit taximeter and screens; steel
            wheels with covers and real tyre sidewalls; the lit TAXI sign (lit when free)
      - [ ] Its sounds (doors, sliding door run and latch, drive, indicator, meter and card beeps, reverse beeper, horn),
            lights, and its place in the landside taxi rank
- [ ] OWNER BUILDS: the information desk in 3D (the announcements already send passengers there)
- [ ] Security cameras: waiting for the owner's screenshots of where they go (feeds in your office and security, live
      people, one feed in full detail at a time)
- [?] The airfield's ground came out light grey all over, and the white walls too dark (first pictures, 2026-11-09):
      check they are fixed
- [ ] RULE kept: floors only on floor faces, never walls or stairs; the tower is "ATC" on signs, never ADC
- [ ] **Copies of the airport in Europe and America** (owner, 2026-11-11; NOT to build yet): the airport as it is
      at release, with every change made to it before then, placed twice more on the globe: one copy somewhere in
      Europe and one somewhere in America. The owner gives the exact coordinates (latitude, longitude, heading) later;
      nothing is placed until he does

## 4. Playing: walking, menus, controls
- [x] **Loading screen into airport mode** (owner, 2026-11-11): going into the 3D airport (from the quick shortcut or
      the Overview's Terminal / Airport site) shows a loading screen that really waits for everything: the terrain and
      its pictures round the airport, the terminal's meshes and textures, the grass, the lights, the shaders compiled.
      A progress bar (the same style as the updater's: percentage centred in the bar, changing colour where the fill
      passes) and a line saying what it is doing ("Loading the terrain…", "Compiling shaders…"); it stays until all
      is done, so nothing pops in afterwards. Esc on it goes straight back to the main menu. Skipped in test runs
      (-UIShot and the other test switches) so testing doesn't wait
- [x] **Holographic map stays open after a quick view** (owner, 2026-11-10): picking a quick view (e.g. Gate 4) moves
      you there but the full map closes by itself; it must stay open (showing you at the new spot) until you minimise
      it yourself (GoTo currently closes it: AADPHUD::GoTo, "if (HoloMap->IsFull()) ToggleMap()")
- [x] **The walking view feels zoomed in, everything too close** (owner, 2026-11-10): the eyes' field of view is 80°
      (horizontal); on the owner's super-wide 3840 x 1080 screen that is very narrow. Widen the default (about 90-100°,
      kept right for wide screens: Unreal keeps the horizontal angle, so ultra-wide needs the vertical one held
      instead) and add a Field of view slider in Settings (60-120°), saved
- [?] **"Moonwalking" in the ATC tower cab** (0.14.6: walking there tested straight; the owner checks) (owner, 2026-11-10): after the map's TOWER: THE CAB view, walking there
      makes you slide / move the wrong way, only in the cab. Check first: the cab floor being the lift's moving
      platform (based on it, so you're carried), the view's turn vs the walking direction after the jump, and the cab's
      floor collision (a slope or step under you)
- [x] **Title screen's moving splash image lags** (owner, 2026-11-10): smooth on Android/Babylon, stutters on PC. Check
      first: the web view's frame rate (Unreal's offscreen browser is often capped at 24-30 fps; raise it to the
      screen's rate), Unreal rendering the 3D world behind the full-screen menus (pause it there), and the copy of the
      web page into Unreal each frame; make it as smooth as on Android
- [x] **Unstuck panel**: when you are stuck inside something, a small panel puts you back on free floor in one click
- [ ] **Footsteps by surface** (concrete, tarmac, wood, carpet, metal, from the floor's material) with a volume slider;
      sounds free for a paid game (CC0 / CC BY), credited
- [?] **Escape flow**: Esc closes a panel; in 3D with nothing open goes to the Overview; in the Overview asks "Are you
      sure?" with Save game before the title screen; the gamepad's Back the same. Esc handling exists: check every step
- [?] Walking after the menus (0.12.0 bug: keys didn't reach the game; middle mouse didn't give the view back):
      fixed in later versions, check with keyboard, mouse and pads in the installed game
- [ ] The owner checks every menu with Xbox and PlayStation 5 pads in hand
- [ ] RULE: no interface ever on top of another (walking, 3D viewer)
- [ ] Your own 3D body while walking (legs, arms, shadow, reflection in glass), everywhere, when the people get bodies

- [ ] **Radial menu** (owner, 2026-11-12): hold a key (or a pad button) to open a wheel of handy shortcuts, so common
      things don't need digging deep into the menus (which shortcuts go on it is decided with the owner; ideas: map,
      phone, bank, staff, timetable, call a vehicle, quick-start places, camera/view). Pick with mouse or stick, release
      to choose; in the controls legend; text never overlapping

- [ ] **Driving sounds from the surface, tyre drifting, ground markings, in and outside the airport** (owner, 2026-11-18;
      about 8-12 nonstop hours): every drivable vehicle (the 6x6 crash tenders now, later your own car and the others)
      sounds different by what its wheels are on, read per wheel:
      - [ ] **Surfaces:** asphalt (runway, taxiways, roads), concrete apron slabs (a soft tick-tick at the slab joints,
            faster with speed), grooved runway (a light hum), gravel and dirt (crunch and stones on the underbody),
            grass and moss (soft swish), wet (spray hiss, heavier in rain), snow and slush, ice (quieter, thinner),
            metal (drain gratings, plates, hangar door tracks: a clank), cattle grids and speed bumps outside. Each surface
            is a physical material on the ground pieces, so new pieces (and the town and road) pick it up by themselves
      - [ ] **Tyre drifting:** the squeal grows with each tyre's sideways slip and with wheel spin or locked brakes
            (the slip the tyre model already works out); a rubbery howl on asphalt, a scrub on concrete, spraying gravel
            off-road, almost nothing on ice; skid marks left on asphalt and concrete that fade slowly
      - [ ] **Ground markings:** painted lines give a faint rougher sound and a tiny bump under the wheel (thick
            thermoplastic on the runway and road edge lines more than thin paint); raised road studs and reflective
            markers a thump each; rumble strips on the road's edge and before the town a loud buzz and a shake
      - [ ] **Outside the airport too:** the road to the home town and the town's streets, car parks, petrol station and
            driveways, with the same surfaces, markings and rules
      - [ ] **Where the sounds come from (owner, 2026-11-18): the Sonniss GDC Game Audio Bundles** (sonniss.com, free
            every year since 2015; royalty-free, commercial use, no credit needed; the only limit is not training
            models with them). Search all the yearly bundles for tyres, skids, gravel, wet roads, grass, gratings and
            road studs; check each bundle's licence page before a file goes in and list it in THIRD-PARTY-NOTICES.md.
            Our own recordings (and the game's own synthesis, like the engine and siren) fill any gap. Recordings are
            played through MetaSounds, driven by each tyre's slip, load and speed. Volume in the sound settings

- [ ] **Vehicle gauges: fuel and cleanliness** (owner, 2026-11-12): while driving any vehicle, two small round gauges on
      the right side of the screen, the cleanliness gauge right under the fuel gauge. Each is a small circle with an
      icon in the middle (a petrol-pump icon; a cleanliness icon such as a sparkle / water drop) and a ring around it
      that shrinks round the circle as fuel runs down or as the vehicle gets dirtier; no percentages. Filling up makes
      the fuel ring grow back; washing the vehicle makes the cleanliness ring grow back. The cleanliness ring is linked
      to the dirt system (season / weather build-up, mud, washing with the hose or the car wash). Text and gauges never
      overlap anything
      **Range on a full tank / charge** (owner, 2026-11-12): petrol car about 780 km; electric car about 450 km. Fire
      trucks (the 6x6 crash tenders, diesel): about 450 km of driving on a full tank (a real Striker 6x6 has a 341 L
      fuel tank; no maker publishes a range, so this is our estimate from heavy-truck consumption of roughly 70 L per
      100 km), and pumping water/foam also burns fuel while standing still. All as tunable settings
      **Electric vehicles** (owner, 2026-11-12): the top gauge shows the battery instead of fuel, with a plug icon in the
      middle; its ring shrinks as the battery runs down and grows back while charging (at home or a public charger)
- [ ] **Real mirrors on the fire trucks** (owner, 2026-11-12): the side mirrors (and the other vehicles' mirrors) show a
      real reflection of the world around them, so you can use them when driving

- [ ] **The courtroom in Unreal, with its people** (owner, 2026-11-12; details later): when you are summoned (e.g. the
      personal-car fine) or go to court on someone else's behalf (one of your crew), the courtroom from the Babylon
      edition (Airline-Manager-Pro-Babylon/src/sections/court: the panelled hall, judges' bench, witness box,
      prosecution and defence tables, the bar, public benches, its seat list in seats.ts) is rebuilt in Unreal, filled
      with people: the judges, the lawyers (prosecution and defence), the accused, the public listening on the
      benches, and police guarding the doors; MetaHumans in their clothes, sitting, standing to speak, looking at
      whoever talks. The court building stands in your home town (a courthouse added to the town plan); you drive or
      fast travel there. Worked out with the owner later

- [ ] **Radio in your office and the operations room** (owner, 2026-11-12): a small radio by the computer where you sit;
      switch it on for the latest news and music. The music is the owner's own (he adds it; listed in
      THIRD-PARTY-NOTICES.md if any part needs it). The news is read with the same voice engine as the MetaHumans
      (Chatterbox, Kokoro as fallback). Mostly everyday news unrelated to the airport, but anything newsworthy that
      connects to YOUR airport is reported too: accidents, staff taken to court for something serious in their job,
      and a boss whose reputation with staff has dropped low (how terrible a boss you are, the reasons and the
      sources), all from the shared world's events (the Babylon game already writes news for crashes)
- [ ] **Your reputation as a boss, with sub-rankings** (owner, 2026-11-12): an overall boss rank that levels up as
      before, plus sub-rankings that go up and down: how you treat your **employees** and how you treat
      **passengers** (fair = up, unfair or rude = down). Changes are slow: a rank doesn't drop at once (rank 20 doesn't
      become 19 straight away), the score drifts down and only when it passes below your current rank does the
      number drop, shown visibly. The scale (approved by the owner 2026-11-12): score 0-100 behind each rank, 50 neutral;
      80-100 excellent (blue-green), 60-79 good (green), 40-59 neutral (yellow), 20-39 poor (orange), 0-19 very poor
      (red); the ranking line's colour shifts smoothly between these as the score moves, and a small buffer stops a
      rank flickering up and down at a boundary. Reaching certain ranks earns rewards, for you and for the airport,
      as in the Babylon edition (trophies, plaques, badges); a low staff ranking can put you in the news (radio item)
- [ ] **Rankings and awards remade in Unreal, new look** (owner, 2026-11-12): the ranking system keeps its rules and
      functions from the shared game code, but everything you SEE is redone in Unreal with a new design: the ranking
      screens and lines, the rank-up / rank-down moments, and every trophy, plaque, badge, medal and certificate as
      new 3D models and art. Nothing of the Babylon edition's look is reused
- [ ] **Up to 5,000 awards** (owner, 2026-11-12): badges, plaques, trophies, medals and certificates to earn, for you
      and for the airport, growing to 5,000 in all (the Babylon edition has 22 achievements in game/src/core/goals.ts
      plus the career awards in career-rides.ts; those carry over and the rest are added in tiers and series, each with
      its own name, look and condition, never duplicates)
      - **The museum** (the Vortex Gallery) shows the airport's awards on its display places. You can swap any displayed
        award for another one you've earned, or let the game choose: it picks one at random, puts it up and takes
        another one down. Swaps are saved in the shared world
      - **The museum swap menu** (owner, 2026-11-12): same look as the new Settings menu being built: categories on the
        left (plaques, trophies, badges, medals, certificates...), and clicking one fills the larger panel on the right
        (attached to it, as in the new Settings design) with the awards you have in that category. Clicking an award
        opens it in a 3D view: turn it round, read what it says, then place it (or replace a displayed one with it)
      - **At the display itself:** walk up to an award on public display and press E (use): choose View (it comes up to
        the front in the 3D view to turn and read), Put back, or Replace (opens the swap menu for that place)
- [ ] **E always confirms, everywhere** (owner, 2026-11-12): throughout the whole game E is the use / confirm key, and it
      only ever acts on the front-most thing: the window or prompt in front when a use is possible; nothing behind it
      in the background is ever triggered
- [ ] **One Back key, everywhere** (owner, 2026-11-12): the same Back key / pad button (the one already set for back,
      e.g. B on the pad) closes the front-most menu or window in every part of the game, and also leaves a
      conversation with any AI person (passenger, crew, anyone)
      - **Cutting someone off:** you can leave a conversation at once; they stop talking mid-sentence and react to
        being cut off, varied: sometimes nothing, sometimes just a sound ("huh") and they move on, sometimes a line
        saying they didn't like it (with the MetaHuman voice and face). Cutting people off often counts against you in
        the rankings: staff for crew, passengers for passengers (approved by the owner 2026-11-12)

- [ ] **Drinking and getting drunk** (owner, 2026-11-12; as in the Babylon edition's bar, src/sections/lounge/bar.ts,
      but written anew for Unreal): each drink makes you more drunk: swaying view and walk, unsteady steps, and at the
      top you can't walk any more: you really fall to your knees, put your hands out in front so you don't hit the
      floor face first, struggle and get back up (without falling again), then walk on; keep drinking and you fall
      again. After about 20 seconds the effect wears off and you are sober, and your character says one of several
      random lines, e.g. "I shouldn't be drinking on the job, that wasn't too smart." Reputation only drops if you
      actually get drunk on the job (one drink without being drunk costs nothing); the drunker and the more often,
      the more it drops (boss reputation, seen by staff and passengers)
- [ ] **Fired at reputation 0** (owner, 2026-11-12): when your reputation reaches 0 you are fired and the run ends:
      progress since your last save or autosave is lost; you can load a save game and carry on from there. The moment
      itself: the whole screen softly blurs (like a phone's portrait mode, everything behind out of focus), then a
      beautiful title effect brings up "YOU ARE FIRED" with the reason underneath (e.g. drunk on the job, how you
      treated your staff); then Load a save / Main menu. Text never overlapping. Reputation never drops to 0 from one
      thing at once: it only falls slowly over time through repeated bad behaviour; only if it ends at 0 are you fired,
      and the explanation sums up what brought it there (the main causes, from the reputation history)
- [ ] **Rank up / rank down pop-up** (owner, 2026-11-12): whenever a ranking moves from one number to the next (e.g.
      10 to 11), a beautiful animated pop-up appears where it's noticeable but in nobody's way (not over the crosshair,
      prompts, gauges or menus), in a calm, relaxing colour, saying which rank you reached ("Rank 11"). Going down
      plays exactly the same animation in a reddish colour, saying the rank you fell to ("Rank 9"). Names which ranking
      moved (overall, staff, passengers). Never overlapping other text
- [ ] **How the rankings fit together** (owner, 2026-11-12):
      - sub-rankings (staff / crew, passengers, more added later): each goes from level 0 to 100; a sub-ranking alone
        never gets you fired
      - the overall ranking goes from level 0 to 5,000; reaching 0 on the overall ranking is what gets you fired
      - one level up or down in a sub-ranking does NOT move the overall ranking; every 10 levels a sub-ranking gains,
        the overall ranking goes up 1; every 10 it loses, the overall goes down 1 (counted per sub-ranking)
      - the colour bands (80-100 excellent ... 0-19 very poor) are read as a share of each ranking's range
        (so for the overall ranking: 4,000-5,000 excellent, 3,000-3,999 good, 2,000-2,999 neutral, 1,000-1,999
        poor, 0-999 very poor; approved by the owner 2026-11-12)

## 5. Graphics settings
- [x] A "light beams distance" slider (the light masts' haze, now 500 m) so weaker cards can turn it down
- [~] Many more graphics controls in Settings (owner, 2026-11-09): sliders per quality group (view distance, upscaler,
      shadows, Lumen GI and reflections, post-processing, textures, effects, foliage, shading, volumetric clouds and fog)
      and toggles (Lumen, hardware ray tracing, motion blur, bloom on by default, depth of field, AO, film grain, SSR);
      some are in, check which are missing. Later the owner brings his own Settings layout
- [ ] Bloom: stays on; the owner is still finding out what looked wrong

## 6. People and life
- [ ] MetaHumans for every AI person (own face from a seed, real skin, alive faces, gentle sway, LOD without popping);
      needs Unreal 5.8 (MetaHuman Crowds); the owner decides when
- [ ] People walking and running to the gates (late passengers already work as logic, not shown yet)
- [ ] Lip movement with the Kokoro voice (with the MetaHumans)

- [ ] **Your own life outside the airport** (owner, 2026-11-12; details worked out when we get there):
      - **Home and family:** you have your own house, where the family you made when creating your character lives.
        The house is modelled and rendered (inside and out)
      - **Your own car:** modelled and rendered, drivable. Buy a new one, or sell yours for money; what you get depends
        on its value, set by several things (model, age, distance driven, condition, dirt and damage, market)
      - **Two bank accounts:** the airport's account (already in the game) and a separate personal account. The bank app
        on the PC and on the phone asks which one (Airport / Personal) and opens that account. A real monthly salary
        is paid into the personal account: what a well-paid airport boss earns in real life (look up real figures)
      - **The road home and a city:** a road from the airport to where you live, through a reasonably detailed city
        (Nanite makes that affordable) that you drive through to work and home
      - **Shifts:** either a real shift like in real life (go home in the evening, lie down in bed to let the night pass
        quickly, back to work the next morning) or carry on at the airport for ever; switchable any time, and switching
        back to real shifts follows the schedule again
      - **Sleeping in bed is single player only** (owner, 2026-11-12): in multiplayer there is no bed sleeping at all,
        because the time of day has to stay the same for every player
      - **Coming home late:** do it too often and your wife starts asking what keeps you at work (worried about the job,
        never about anything else)
      - **Pets at home:** you decide if you have pets at home too, separate from the office pet, or whether the office
        pet comes home with you
      - **Detailed town buildings, first the player's villa and a typical Icelandic house** (owner, 2026-11-12; later):
        modelled properly in Blender (bevelled edges, real corrugated roof sheets, trims, gutters, detailed windows and
        doors, realistic materials), game quality, one final render each, then into Unreal. Needs Blender's MCP add-on
        running (the PC has Blender 3.3; a newer Blender may be needed) and only when the other chat isn't using Blender.
        Layout from the town plan v2 (curved streets, crescents, cul-de-sacs; Home-Town-Plan-v2.png)
      - **Fast travel with your own car** (owner, 2026-11-12): you never have to drive the whole way between home and
        work; fast travel to any quick-start / spawn place works from anywhere, also while driving:
        - fast travel while IN your car: the car comes with you and is parked at the destination: outside your house
          (home), outside the shop / fast food / petrol station you picked; at the terminal in a car park area chosen
          later (whether it should go back home instead is decided later)
        - fast travel while NOT in the car: only you are moved; the car stays where it is
        - "bring my car": call your own car to where you are in the world; it appears in the closest free parking spot
          (or the nearest safe roadside spot) near you
        - never on the airport's airside (apron, taxiways, stands): that is for airport vehicles only; called there,
          it goes to the nearest landside car park
        - **calling it inside the airport boundary** (owner, 2026-11-12): a message appears, only in that case: "You
          can't have your own personal car inside the airport boundaries; it has to stay outside. Your car has been
          placed at the nearest parking spot outside the terminal."
        - **driving it inside the boundary** (somehow, e.g. through a gate): a warning: "Personal vehicles are not
          allowed inside the airport boundaries. Leave immediately or suffer the consequences."
          - the timing (owner, 2026-11-12): the court date applies once you have been inside the boundary for 20
            seconds; after the warning you have 1 minute to leave; still inside after 1 minute, the court is stricter;
            after 2 minutes stricter again; after 3 minutes stricter again. How long you stayed decides how big the fine
            is. Applies to your own car and to any personal vehicle you buy later. Other punishments, and the court
            itself, are decided later with the owner
        - the map always shows where your own car is, and where each fire truck is (holomap and the full map, with
          their icons)
      - **The town's people** (owner, 2026-11-12): AI citizens living in your town, each with a real home and their own
        daily routine (work, shopping, home, sleep). Staff you hire show where they come from; those registered in your
        town finish their shift, get into their own car, drive home along the road and go into their real house
      - **Homes in town, how they work** (owner, 2026-11-12): every house in town can be someone's home, but not every
        house has to be filled: only the people who actually live in your town get one (most of them NOT airport staff).
        Houses have no interiors (except yours): a citizen walks up to their front door, goes in, the door shuts and
        they are unloaded; coming out plays the same thing in reverse (door opens, they step out) and they walk about
        town or get into their car and drive to a real destination (petrol station, grocery shop, work...)
      - **Citizens who work at your airport** keep their own schedule like in real life: you see them leave home in
        time, drive to the airport, park, walk in to work, and drive home again after their shift
      - **Shops and fuel:** buy groceries in town; petrol stations; public chargers for electric cars, and charging at
        home if you own an electric car. The charging cable is handled with the same animation as the fire station's
        wash hose (picked up, plugged into the car, hung back), only charging instead of water
      - Shared world: the personal account, salary, car and house belong to the shared save (Android sees and manages
        them); the house, city, driving and family scenes are PC airport mode only

- [ ] **Cleaners** (owner, 2026-11-12): a new "Cleaners" category in the Crew section of the UI, next to the others,
      with everything the other staff categories have (hire, see where they come from, pay, schedules, fire...).
      - **At work:** each cleaner fetches their cleaning trolley (on small wheels, with the soap, buckets and supplies,
        like real airport cleaning carts) from the cleaners' store room, pushes it to where they work, takes the mop off
        it and mops the floor round about, moving up to 10-15 m away from the trolley; then goes back, pushes the
        trolley on another 10-15 m and carries on. They work through every area passengers use: the main hall, the
        long hallway, the gate areas (never in staff-only areas, never in the jetways, never outside). Done, they hang
        the mop back on the trolley (the reverse of taking it), push it back to the store room and park it there
      - **The store room** for the cleaning equipment goes in a terminal extension the owner is still planning; the car
        park also has to grow (staff cars)
      - **Their day:** like everyone else (staff, passengers, MetaHumans) they have their own daily rhythm: no popping in
        or out. They arrive for their shift and leave at the end of it through the staff corridors, never through
        security
- [ ] **Staff going home and coming to work, for every role** (owner, 2026-11-12): at the end of the shift each staff
      member walks from where they finished (e.g. the cleaners from the store room) through the staff corridors and
      out of the terminal to their own car in the car park (each has a specific car and spot). FOR NOW they get in and
      sit in it until the next morning, then walk back in to start their shift (the cycle repeats every day). They keep
      their uniform on going home and coming in, so you can tell who they are (a pilot, a cleaner, a mechanic, a
      security officer...)
      - **REMINDER for the owner once the town is fully built:** then make staff who live in your town drive off home
        to their own house, and the others drive away out of the area, instead of waiting in their cars

- [ ] **MONDAY 30 NOVEMBER - Protective covers on parked aircraft ("blanking")** (owner, 2026-11-26: build on Monday
      2026-11-30, everything below). Real world: engine inlet and
      exhaust covers ("engine blanks", red, with "REMOVE BEFORE FLIGHT" streamers), pitot probe covers, static port,
      AoA vane and TAT probe covers, APU inlet/exhaust covers, gear lock pins with streamers. Airbus procedures go by
      how long the aircraft stands: parking up to 7 days, 15 days or 12 weeks, then storage; EASA asks for pitot covers
      even on short parking (dust, sand, insect nests). Game rule (our choice, set in Settings > Realism):
      - quick turnaround: nothing;
      - long ground time (about 3 h or more, and every night stop): pitot covers and gear pins by the ground crew;
      - parked over 24 h, or sooner in dust, sand, snow or heavy rain: engine inlet and exhaust covers and APU covers
        added (a mechanic with a ladder or a high-lift for the bigger engines);
      - Parking/storage (over 7 days): every opening blanked, as Airbus's parking task.
      Before departure the covers come off at the walkaround and the streamers are counted back in; one left on is an
      incident (a rejected take-off or a turn-back, unreliable airspeed if a pitot cover). Covers are our own models
      (red fabric, white letters, streamers that move in the wind), sized for each engine type; stored in the hangar
      and the ground equipment area; bought like other ground equipment.
      How they look and move (owner, 2026-11-26): fitted flush to the real seating - the inlet cover's sewn-in spring
      steel hoop sits just inside the intake lip and the fabric is pulled taut over the lip by its straps; the exhaust
      cover fits the same way over the nozzle. Once fitted it is a still part of the engine (at most a faint ripple in
      strong wind). Off the engine it is heavy coated fabric, like a thick tarpaulin: it sags, bends and folds under
      its own weight, the hoop springs back round, and it folds in three to carry - never flag-like. Only the
      streamers flutter freely. Build: cloth physics (heavy, stiff settings) only while it is carried, fitted or
      taken off; a fixed mesh the rest of the time.

- [ ] **Airport hotel** (owner, 2026-11-26: "We should build a hotel for the airport"). A hotel next to the terminal,
      owned by the airport and part of its economy (rooms, staff, income), a short walk or a shuttle from the
      terminal. Crews on night stops and passengers stay there. Later, when flights are cancelled or held overnight,
      passengers get hotel and meal vouchers and go there instead of queueing or sleeping in the hall (no camp beds:
      the terminal has no room for them). Placement on the site to be agreed with the owner first.

## 7. One shared world with Android (batch 4)
- [ ] The bridge: the timetable screens and passengers from the world's real flights; airport mode writes back
- [ ] The automatic one-day check that both setups give exactly the same world
- [ ] The problem report sent from the game

## 8. Android edition
- [ ] With the next Android release: the start screen fits a phone (done in www/styles.css, ships with the next version)
- [ ] Chapters in the Android code: paused by the owner

## 9. Housekeeping
- [x] **Company name is now Pixel Norma** (owner, 2026-11-15): Unreal CompanyName and CopyrightNotice, both
      installers' publisher, the Android/Babylon edition's publisher and credits, the frame-generation plugin, the
      website (plugin author, default copyright name). Kept on purpose: the app ids (com.dronedirector...) and the
      installer AppIds, and the registry key Software\DroneDirector\AirportDynastyPro, because changing them would stop
      updates installing over existing copies. On a live WordPress site, set Settings > Airport Dynasty Pro website >
      Copyright name to "Pixel Norma" once (a saved value overrides the new default). Goes out with the next release.
- [x] ROADMAP.md's known issue "cooking the whole /CesiumForUnreal folder crashes Shipping" is out of date: since
      0.14.0 it is cooked whole and runs fine (it was the fix for the terrain crash)
- [x] The crash box shows an old report (14:58 on 2026-11-10, the fixed Cesium crash) on the owner's PC: clear old
      reports, or show each only once
- [ ] After all of the above: the next logical things that make the game run and give it features

## Possible candidates (not decided - the owner wants a reminder now and then; not to be built until he says so)
- [ ] **NOTAMs** (2026-11-26): official notices the airport publishes - a taxiway closed, a stand out of use, a PAPI
      or ILS unserviceable, work in progress, a crane near the approach. The NPC airlines, ATC and the taxi routes really
      work around them; each has a start and end time; shown on the holographic map and in the tower.
- [ ] **Delay codes** (2026-11-26): every delay gets the real IATA industry delay code (catering, technical, ATC,
      weather, late incoming aircraft, passengers, baggage ...), worked out from what actually held the flight. A
      monthly on-time report shows which department or partner caused how much delay.
- [ ] **Load sheet** (2026-11-26): load control works out each departure's weight and balance (passengers, bags,
      cargo, fuel) and issues the load sheet to the crew. A last-minute change - a late passenger, a bag offloaded,
      extra cargo - means a new sheet and can hold the departure a few minutes (and gives it its delay code).
- [ ] **Baggage tracing desk** (2026-11-26): bags that miss their flight are traced, sent on the next flight as
      "rush bags" and delivered to the passenger; passengers without their bag come to the desk in the arrivals hall.
- [ ] **Plane spotters** (2026-11-26, the owner likes it; how it works best still to be worked out with him): a
      spotting hill or a spot along the fence with photographers and their cameras. More of them turn up when a rare
      aircraft type or a special livery is due.
- [ ] **Follow-me car** (2026-11-26): a yellow car with a lit FOLLOW ME sign that leads a visiting pilot who does not
      know the airport to the right stand.
- [ ] **Bird strikes** (2026-11-26): bird control is in the game, a strike is not. An aircraft returns after hitting
      birds, the runway is checked for remains, the engine is inspected, and the airport gets a report. Better bird
      control means fewer strikes.
- [ ] **FOD walks** (2026-11-26): staff walk the apron in a line every morning picking up loose bolts and rubbish that
      could be sucked into an engine. Skipped walks raise the risk of engine damage.
- [ ] **Fuel farm** (2026-11-26): the airport's own fuel storage, filled by road tankers; the fuel price the airport
      sets affects which airlines want to come.
- [ ] **Open day** (2026-11-26): once a year the airport opens to the public - aircraft on show, the fire trucks
      demonstrating, families walking the apron. Good for the airport's reputation and brings in some money.
- [ ] **Pet relief area** (2026-11-26): a small fenced grass corner by the terminal for passengers' dogs.
- [ ] **Staff shuttle bus** (2026-11-26): takes staff between the staff car park and the terminal or the hangar at
      shift changes.

## Done recently (for reference)
- 0.14.4 the globe's satellite pictures and land cover in the right place; the detailed sea only near the ground
- 0.14.3 the 8 apron light masts at the gates (textured, striped guardrail, sensor-switched, beams along the row, own chunk);
  Blender files Art/LightMast, Art/GateSeat, Art/Jetway kept up to date by their build scripts
- 0.14.0 the world's terrain (download in Settings, move to another folder), the sea everywhere; 0.14.1 the tower's
  materials in the release; 0.14.2 the content in chunks, one per building; the land cover packs online
- The ATC tower is in the game (0.13.2-0.13.7); the old "ATC tower 3D model" item is done

## Rules that always apply (for every item above)
- Commits by DroneDirector <simon@dronedirector.fo>, no co-author lines, never mention AI
- Every licence must allow a paid game (credit fine; never non-commercial or share-alike on the game)
- Quick views live only in the holographic map; loudspeaker sounds differ by area, female announcer, sliders
- Settings only suggest, never force; imports keep the Babylon geometry exact, textures always applied
- Every in-game object model (not buildings) has its .blend in Art/<Object>/, updated with every change
- Every model: no empty material slots (checked in Blender before export and in Unreal after saving), sampler types
  matching the textures, and a look at a game screenshot before it's called done
- Textures: fewer materials and packed maps (roughness/metal/AO in one) to save draws; no atlases that cost quality
- Release builds load content by name: anything new loaded that way must be in the cook list (DefaultGame.ini), and
  every release is tested as the installed game, not only in the editor
- Dirt and cleaning for every drivable vehicle and the crew: slow build-up by season and weather (summer about a month,
  normal 1-2 weeks, winter / bad weather about twice a week), dust, low road grime, mud splatter behind the wheels,
  streaks, glass film; washed by hand with the wash hose outside the station, by you or by crew members doing the same / laundry for the crew's gear; optional, with a Settings switch.
  Textures in Art/FireTruck/T_Dirt_*.png; rules in docs/fire-station/SPEC.md.
- Fire trucks, next (owner, 2026-11-11): reverse lights that come on while reversing, and a reversing sound (the truck's
  beeper). Real recorded sounds where they matter (engine, beeper, siren): commercial-use sources only (CC0 on
  Freesound, the yearly royalty-free GDC game-audio bundles); downloading full-quality Freesound files needs the owner's
  account signed in once in the browser pane.
- MetaHuman people (owner, 2026-11-11): Tools/make_metahumans.py builds them from Epic's 27 preset faces, 38 hairstyles,
  brows, beards; uniforms still to make as MetaHuman garments (pilot, cabin crew, ground crew hi-vis, security, fire
  crew; the library has one default garment only). Then the crowd uses them (walk/idle from the mannequin set), far
  crowds as baked-animation instances (AnimToTexture); each role's own actions (serving, bag checks, marshalling, hose).
- **Security detection dogs (K9)** (owner, 2026-11-12; with the security people, after the MetaHumans): security
  officers can work a detection dog for drugs and other contraband in passengers' bags.
  - **The dog:** a good-looking, fully animated dog (the owner sends how it should look). Walk, trot, run, sniff with
    nose down, sit on its haunches, lie down, stand and stare; normal calm breathing (chest and flanks) most of the time,
    and every now and then fast panting with the mouth open and the tongue out. Our own model or a commercially
    licensed one (licence checked first).
  - **On the lead:** the officer holds it on a short lead (about 1.8 m) while screening passengers and bags; the lead is
    drawn as a real rope between hand and collar, slack or tight.
  - **Let loose:** for gameplay reasons (catching a running criminal, or a free search sniffing round an area) the officer
    unclips the lead; the dog ranges on its own but always within a radius of its officer: working radius about 15 m in
    the terminal (in sight, on voice control), called back if it gets past about 20 m; on a chase it may go further
    until the person is caught, then returns. A long line (about 18 m) as an in-between option for searches. All three
    distances as tunable settings.
  - **Finding drugs:** the dog smells contraband in a bag (real dogs find it reliably right next to the source and
    rarely from much more than a few metres away, so it must pass close: about 2 m sure, fading out to about 5 m).
    It alerts the trained "passive" way used at airports: it stops, sits next to the bag or person and stares at it,
    waiting for its officer; the officer rewards it (a pat, a toy) and deals with the passenger.
  - **The guilty passenger reacts:** if the bag's owner is nearby when the dog sits, they react: shocked or scared look,
    a step back, glancing around, maybe trying to walk away (which can start the chase).
  - **The dogs' own room (a kennel)** (owner, 2026-11-12): when not working, the dogs are kept in a secure kennel room
    somewhere in the airport, each in its own kennel. An officer walks there, takes their dog out on the lead, walks it
    to the terminal and works it; when the job is done they walk it all the way back and put it in its kennel. No
    teleporting: the full walk both ways, for NPC officers and for the player (you walk over to fetch the dog and to
    bring it back). Where the room goes is decided with the owner.
  - **Research when we get to it (not before):** what the room is called and how it's laid out; in which areas of an
    airport detection dogs normally work (checkpoint, landside halls, baggage, arrivals, cargo...); and how many hours a
    day / week a team typically works, with breaks, from real data. The game's dog schedules follow that.
  - **Good dog AND cat animations** (owner, 2026-11-12) for every animal the AI runs (the security dogs, the office pet
    dog or cat, any later animals): find good-quality animation sets (walk, trot, run, sniff, sit, lie, idle with
    breathing, panting, play; cats: walk, stretch, groom, sit, lie, jump) with a licence that allows a paid game
    (checked first, listed in THIRD-PARTY-NOTICES.md); otherwise make our own.
  - Which passengers carry something comes from the shared world (part of the passenger's data), so Android knows it
    too; the dog and the scene are airport mode on the PC only.
- Getting in and out of vehicles, the full pass (owner, 2026-11-11): the same animation for NPCs and the player - walk
  over, open the door by its handle, climb the steps holding the handles, walk to and sit in the middle seat, hands on
  the wheel, feet on the pedals, eyes out through the windscreen, then drive; out the same way. Every contact point read
  from the model (steps, handles, seat, wheel, pedals), the body fitted to each character's size (no jumping, no
  clipping through the cab), either door.
- Laundry building for the staff's clothing (owner, 2026-11-11: he makes the building later). Until then the laundry is
  an instant wash: a button in the Overview UI that cleans the staff's clothes at once.
- Ground wetness and puddles: MOVED to section 0 (the fast, material-based way, 2026-11-13; see HANDOVER.md).
