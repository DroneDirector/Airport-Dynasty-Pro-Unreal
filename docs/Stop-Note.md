# Stop note - 2026-11-15 evening

**Done**
- Release build fixed: Windows min/max/GetObject macros leaking from Cesium into the roads code (Windows headers now
  included the Unreal way before Cesium's; RoadNetwork's includes guarded).
- Skin material: the tattoo layers share one sampler (they took the skin past 16 samplers; its shader failed).
- Tattoos: batches 2 and 3 drawn and OK'd (46 designs in Data/tattoos.json). Import into Unreal WAITS for his green light.
- Pixel Norma logo splash on the to-do list (item 64, 5th open item).

**Half-done**
- 0.17.16 NOT published. The cook still stops on one engine check ("Ensure: SerializedElementSize == ElementSize",
  Array.h) right after "Waiting for skinned assets to be ready (SK_HouseCat / SK_GermanShepherd)" - the pets' models,
  when the cook builds them. Cooking each pet alone passes, so it shows only in the full cook.

**Next**
- Find what in the pets' skeletal meshes trips that check (re-import them with Tools/import_pet.py from the FBX, or
  their build settings); then rerun `bash Tools/release_pc.sh 0.17.16 0.17.15 0.17.14`, then `--upload` (about 12 min).
