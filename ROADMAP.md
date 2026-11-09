# Roadmap: the PC version (Unreal) in batches

Each batch is one release: built and compiled piece by piece, committed and pushed as it goes, published once when
the whole batch is done. Each part is first rebuilt to look and work like the Android (Babylon) edition, then upgrades
are offered. Sources: CLAUDE.md (the agreed plan), the PDFs in the repo, the Babylon code.

## Batch 1: the terminal you can walk around in (in progress)
- [x] Unreal 5.7, project compiles and opens; empty start map `Maps/Airport`
- [x] Glass walls: frames and clear glass panes
- [x] The building kit's look: each room's floor, walls and ceiling in the Babylon kit's materials (patterned textures such as tile grout, wood grain and herringbone still to come)
- [x] Doors: sliding glass, revolving, swing, secure staff, double, one-way and jetway doors that open as you walk up, the room's name over each door
- [x] Main hall fittings: check-in desks 1–24 with belts, banners, chairs and queue lines, 8 bag drops, trees, benches, bins and signs; seats, gate desk and sign at all 8 gates
- [ ] The other rooms' fittings (security lanes, lounge, offices, Operations, Academy, crew room, toilets, baggage hall, Food Walk, eateries, Tax Free, bar, family room, view bays) and the ceiling lights
- [x] Stairs with oak and walnut steps, glass sides, handrails and a glass balustrade round the opening; openings cut through Floor 2 and the ceilings; the 5 lifts called, opening and riding between the floors
- [x] The jetways J-1 to J-4 (fixed corridor, rotunda, three telescopic tunnel sections, bellows, cab with canopy, drive unit), docked, walkable; docking and undocking come with the aircraft in batch 5
- [x] Timetable screens at all 23 places (hall walls, totems, pairs, departure screens, gate screens), showing the Android edition's sample flights round the real time until the shared world arrives (batch 4)
- [x] Release pipeline for the PC: `.github/workflows/unreal-pc.yml` builds, packages and publishes on the owner's PC (self-hosted build machine), only when `pc-version.json` has release notes in `release-notes-pc/`; catches up every 3 hours; the installer installs over the previous version. Waiting on the owner: register the PC once (`Tools/RegisterBuildMachine.ps1`)
- Publish: version 0.5.0 (PC) together with the next Android release

## Batch 2: people in the terminal
- Mass Entity crowds on ZoneGraph lanes made from the plan, StateTree behaviour (replaces CrowdTest)
- A passenger's day: arrival, check-in queues, security with bags, shops and seats (Smart Objects), boarding
- Cheaper detail for people far away; about a hundred people first, then thousands

## Batch 3: the interface
- The existing interface in Unreal's web browser view first, then screen by screen in Unreal
- The holographic map (same look as Babylon, then the offered upgrades), Hide UI, cinema, settings
- Graphics settings and sliders, the device scan, the performance overlay, the problem log and report

## Batch 4: one shared world with Android
- The game's TypeScript world code running in Unreal (embedded JavaScript engine)
- One save in the cloud (Supabase), one device runs the world, the other watches and manages
- Passengers from the shared world appear where the world says they are; airport mode writes back
- The automatic one-day check that both setups give exactly the same world

## Batch 5: the airport site
- The custom airport BIAP at Mýrdalssandur from the shared layout: runway 08/26, taxiways, stands, gates
- Aircraft at the stands, taxiing, take-offs and landings; ground vehicles and the turnaround (fuel, catering, bags)
- Weather, clouds, rain and lightning on Unreal's own systems

## Batch 6: the globe
- Cesium for Unreal with the free data (Sentinel-2 2016 imagery, Copernicus/AWS terrain, OpenStreetMap water)
- The airport placed at 63.457988 N, 18.713703 W, 40 m (130 ft), runway heading 68°, the ground flattened within
  2.6 km and blended out to 4.4 km exactly as in the Babylon edition
- World Partition streaming and zones that load by distance

## Batch 7: sound and life
- Airport announcements with Kokoro and the loudspeaker sound (the ten steps in the Kokoro PDF)
- Office monitors you can click into; security cameras (waiting on the screenshots); the ATC tower (waiting on the model)

## Later
- MetaHuman people, and your own body in walking mode everywhere
- Terminal extensions placed in play (snap-together pieces), HLOD for distant buildings
