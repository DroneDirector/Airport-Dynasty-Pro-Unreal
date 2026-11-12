# To-do: Airport Dynasty Pro (PC, Unreal; and the Android edition)

The one complete list of what is still open, updated 2026-11-11 (after 0.14.7). ROADMAP.md keeps the history and the
details behind each item. Marks: [ ] open, [~] started or partly done, [?] built earlier but the owner should check it.

## 0. Now, in this order (owner, 2026-11-11)
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
      - **Coming home late:** do it too often and your wife starts asking what keeps you at work (worried about the job,
        never about anything else)
      - **Pets at home:** you decide if you have pets at home too, separate from the office pet, or whether the office
        pet comes home with you
      - **The town's people** (owner, 2026-11-12): AI citizens living in your town, each with a real home and their own
        daily routine (work, shopping, home, sleep). Staff you hire show where they come from; those registered in your
        town finish their shift, get into their own car, drive home along the road and go into their real house
      - **Shops and fuel:** buy groceries in town; petrol stations; public chargers for electric cars, and charging at
        home if you own an electric car. The charging cable is handled with the same animation as the fire station's
        wash hose (picked up, plugged into the car, hung back), only charging instead of water
      - Shared world: the personal account, salary, car and house belong to the shared save (Android sees and manages
        them); the house, city, driving and family scenes are PC airport mode only

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
- Ground wetness and puddles for the whole airport (one system): heavy rain leaves real puddles and dark wet surfaces
  on the runways, taxiways and apron; the wash hose leaves the same locally where you wash; drying over time. Spec in
  docs/fire-station/SPEC.md (wash hose section).
