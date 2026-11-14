# Stop note (2026-11-14 - owner stopped a truck test to ask about the extra paving)

**Done (committed and pushed):** true Unreal terminal parts 1-4 (pieces, materials, own layout, no Babylon tie).
**Built, not committed yet:** 0.17.13 test package (terminal only, F:\ADP-Packaged\0.17.13, NOT published; version
0.17.13 committed locally, not pushed). Truck: flicker moves for both trucks (Data/firetruck1/2-zfight.json, applied by
ABabylonModel), M_TruckPaint (dirt, wear, wetness) and M_TruckLens (lit lenses, LED dots, roof-bar rhythm) in
Tools/set_truck_materials.py and FireTruck.cpp.
**Half-done:** the truck's lens pieces weren't all found (materials already copied by the model's tone-down step);
the fix (LensMids, parent names) is written in FireTruck.cpp - the compile/test that was running was stopped.
**Build list still to do:** truck lenses tested + screenshots; wash hose (pick up, water, drag, clean the dirt);
the extra paving at the fire station and the tower (owner: remove it, the new ground everywhere); then test all, publish
together, screenshots, then hold and ask what next.
