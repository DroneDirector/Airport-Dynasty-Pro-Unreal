# Airport Dynasty Pro – Unreal Engine 5 edition: project rules

- The PC version of Airport Dynasty Pro on Unreal Engine 5 (Windows only). It is built next to the Babylon.js edition
  (DroneDirector/Airline-Manager-Pro-Babylon), which is never changed by work here and keeps working until this edition
  replaces it.
- Sign every commit as `DroneDirector <simon@dronedirector.fo>`, with no co-author or session lines.
- Never mention in code, comments, text or commits that anything is made with AI.
- Licences of anything used must allow commercial sale in a paid game, asking at most for credit (and the licence
  text). Never non-commercial, share-alike on the game, or paid-only terms. Our own models, pictures and sounds by default;
  models made by others are allowed where needed (licence checked first). The people are MetaHumans (part of the Unreal
  Engine licence), shaped from MetaHuman Creator's presets; their clothes and uniforms are ours.
  List every third-party part in THIRD-PARTY-NOTICES.md and keep it up to date.
- It is a commercial game: never use anything that doesn't allow commercial use in a paid game. Check the licence of
  every plugin, asset (including Fab and other marketplaces), font, sound, data source and code library before it goes
  in; if the terms are unclear or missing, leave it out. Engine content or samples count too.
- Never put registration keys, passwords or secret keys in the repo. For Supabase, only the Project URL and the
  anon/publishable key.
- Big files (models, textures, sounds, .uasset, .umap) go through Git LFS (see .gitattributes).
- Don't publish in small pieces: build and collect many changes, then publish them together in one release. When the
  owner asks for a small separate publish, remind him of this rule every time, however often it happens.
- Updates install over the old version; the version number goes up with every release.
- Show progress as percentages (building and publishing), about every 10%.
- Don't run tests or check that things work unless the owner asks: build what is needed and release.
- Don't make or share screenshots unless asked. Avoid image and test renders as much as possible (the owner looks at
  results himself in the editor); when he asks for a render he doesn't really need, remind him of this every time.
- Text and menus must never overlap anywhere.
- Never add far-object impostors.

## Plan (agreed 2026-11-07)
- Engine: Unreal Engine 5, PC only. Royalty-free until $1M gross per product, then 5% (Epic Games Store sales excluded).
- First step: a test of the main hall and one gate (Nanite, Lumen, about a hundred people walking), then the rest of
  the 3D places, then the interface screen by screen (meanwhile the existing interface may run in Unreal's web
  browser widget), then the simulation.
- The terminal's shapes can be exported from the Babylon edition as glTF and imported with Nanite on; materials,
  glass and lighting are redone in Unreal.
- Crowds with daily lives: Mass Entity, Smart Objects, StateTree. Aircraft taxiing, take-offs, landings, weather and
  lightning on Unreal's own systems.
- The globe: Cesium for Unreal (free, Apache 2.0) with free commercial-use data only, no monthly fees:
  - imagery: Sentinel-2 cloudless 2016 by EOX (CC BY 4.0, layer s2cloudless_3857 only; the 2018+ layers are
    non-commercial);
  - elevation: Terrain Tiles on AWS or Copernicus DEM GLO-30, converted once to Cesium's terrain format and hosted by us;
  - rivers, lakes and coastlines: OpenStreetMap (ODbL, credit).
  - Not used: Cesium ion (paid for commercial games), Google Photorealistic 3D Tiles (billed per use).
- Building from anywhere: the owner's PC as a GitHub self-hosted build machine, and Claude Code on that PC reachable
  from the phone; Parsec or Steam Link to see the game remotely.
- Converting each part: first rebuild it to look the same as in the Babylon edition, then offer upgrades the owner
  picks from. Example, the 2D holographic map (Babylon `src/ui/holomap.ts`): same blue holographic grid, floor label,
  floor plan lines, the player's arrow with its view cone, corner brackets, size and placement; then offered: real
  glow with shimmer and a sweeping scan line, a slight 3D tilt when opened larger with the floors stacked like glass
  layers and animated floor switches, live dots for walking people and aircraft outlines at the gates, crisp at any
  size. The same for the timetable screens, the glass, the menus and the settings.
- The terminal grows in play, like building in The Sims: the airport manager adds terminal extensions (and later more
  buildings) to meet traffic. So everything is built from modular, snap-together pieces placed from saved data at run
  time (instanced Nanite meshes), not baked into fixed maps; lighting is Lumen only (nothing baked), the walking paths
  rebuild themselves where something is added, and each wing or extension is its own zone that loads and drops its
  detail by distance and by which zone the player is in. People far away use Mass's cheaper levels of detail. Distant
  buildings may use simplified merged meshes (HLOD mesh merge), never picture impostors.
- The Babylon edition's performance workarounds (thin instances, mesh merging, texture arrays, freezing and octrees,
  KTX2, collision proxies, mirror-texture reflections, glow trimming, the eye-adaptation fix) are NOT ported: Unreal has
  its own built in (Nanite, culling, texture compression, Virtual Shadow Maps, Lumen). What carries over is the design:
  the graphics settings and their sliders, the device scan, the performance overlay, the problem log and report.
- Two editions, one world: the Babylon edition (Android) and this Unreal edition (PC) share the same world, kept in
  sync: economy, money, aircraft buying/selling/leasing, car rental, AI airlines and traffic, hired crew, timetables,
  staff and all game logic. The world's code is written once (the Babylon edition's TypeScript game code) and runs in
  both (in Unreal through the browser view or an embedded JavaScript engine), so the rules never drift apart. One
  save in the cloud (Supabase); one device runs the world at a time, and watching and managing work on both, Android and PC; time while
  both are closed is worked out on opening. Airport mode (the 3D terminal, people, vehicles, jetways, the custom airport) is Unreal only, with the heavy
  graphics on the PC;
  its passengers come from the shared world's numbers and what they spend goes back into it.
- The custom airport on both: its layout (runways, taxiways, stands, gates and the terminal's zones) is part of the
  shared world. Android shows it as the 2D layout map and a light, generic building made from that same layout; the
  detailed terminal is only in Unreal. Passengers live in the shared world as data (identity, booking, stage of the
  trip, which zone they are in, mood, money) with times worked out from the layout's walking distances, so Android
  runs their whole trip without animating them. When airport mode opens on the PC, each passenger appears where the
  world says they are and Unreal animates them from there; the world's logic stays in charge of timing (Unreal
  adjusts walking to match), and what happens in airport mode is written back to the same passenger. Changes built
  on the PC (extensions, new gates) update the shared layout, so Android sees them too.
