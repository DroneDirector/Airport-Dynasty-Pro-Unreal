# Airport Dynasty Pro – Unreal Engine 5 edition: project rules

- The PC version of Airport Dynasty Pro on Unreal Engine 5 (Windows only). It is built next to the Babylon.js edition
  (DroneDirector/Airline-Manager-Pro-Babylon), which is never changed by work here and keeps working until this edition
  replaces it.
- Sign every commit as `DroneDirector <simon@dronedirector.fo>`, with no co-author or session lines.
- Never mention in code, comments, text or commits that anything is made with AI.
- Licences of anything used must allow commercial sale in a paid game, asking at most for credit (and the licence
  text). Never non-commercial, share-alike on the game, or paid-only terms. Our own models, pictures and sounds only.
  List every third-party part in THIRD-PARTY-NOTICES.md and keep it up to date.
- Never put registration keys, passwords or secret keys in the repo. For Supabase, only the Project URL and the
  anon/publishable key.
- Big files (models, textures, sounds, .uasset, .umap) go through Git LFS (see .gitattributes).
- Updates install over the old version; the version number goes up with every release.
- Show progress as percentages (building and publishing), about every 10%.
- Don't run tests or check that things work unless the owner asks: build what is needed and release.
- Don't make or share screenshots unless asked.
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
