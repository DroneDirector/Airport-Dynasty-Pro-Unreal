# To-do: Airport Dynasty Pro (PC, Unreal; and the Android edition)

The one complete list of what is still open, updated 2026-11-11 (after 0.14.7). ROADMAP.md keeps the history and the
details behind each item. Marks: [ ] open, [~] started or partly done, [?] built earlier but the owner should check it.

## 0. Now, in this order (owner, 2026-11-11)
- [ ] The old rooms' OBJECTS (never the room itself: walls, floor, ceiling, window, door stay the airport's) placed in the
      terminal's rooms so they look right: Office 1 (yours, with the pet: dog or cat as chosen, its bed, bowls and
      football) and Offices 2-4, Operations, the Academy; the court once the owner says where; the hangars wait
- [ ] Footsteps by surface with a volume slider
- [ ] The rest of the graphics sliders and toggles in Settings
- [ ] Unreal trees in place of every outdoor tree (licence allowing a paid game)
- [ ] World Partition / HLOD (distant buildings fade, no popping) and a performance pass

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

## 3. The airport and buildings
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

## 7. One shared world with Android (batch 4)
- [ ] The bridge: the timetable screens and passengers from the world's real flights; airport mode writes back
- [ ] The automatic one-day check that both setups give exactly the same world
- [ ] The problem report sent from the game

## 8. Android edition
- [ ] With the next Android release: the start screen fits a phone (done in www/styles.css, ships with the next version)
- [ ] Chapters in the Android code: paused by the owner

## 9. Housekeeping
- [x] ROADMAP.md's known issue "cooking the whole /CesiumForUnreal folder crashes Shipping" is out of date: since
      0.14.0 it is cooked whole and runs fine (it was the fix for the terrain crash)
- [x] The crash box shows an old report (14:58 on 2026-11-10, the fixed Cesium crash) on the owner's PC: clear old
      reports, or show each only once
- [ ] After all of the above: the next logical things that make the game run and give it features

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
