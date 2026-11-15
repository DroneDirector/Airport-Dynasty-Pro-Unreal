# Stop note — 2026-11-15 (pause during the town roads)

**Done (committed, pushed):**
- Every town street re-traced to the plan's middle line and smoothed. Tools/check_roads.py passes all town streets except a gentle real S-bend on Main Street.
- The road to town follows the overview from the 3 km mark; the airport area stays as approved.
- Junctions have curved corners. Each street now has its own lanes (a shared-lanes bug had put pavement openings in the wrong places).
- The villa and the dealership are placed in town (the villa by its drive as on the plan). Their import naming is fixed in Tools/glb_nodes.py and Tools/import_model.py.
- The title screen has town demos (Car dealership, Your villa), and the demo buttons are half size.
- The land is rebuilt as Nanite.

**Half done:**
- Thin leftover kerb strips still cross the junctions. They are the narrowed-away pavements.
- The fix is written but not applied: cut each street into sections with no pavement across a mouth, and give turning circles their pavement as arcs round the opening. The edit never ran, so nothing is broken.

**Next:**
1. Apply the sections fix in Tools/make_roads.py.
2. Run `python Tools/check_roads.py`.
3. Compile, then take the -TownShots pictures (TW_01-TW_17).
4. Commit.
5. The court house waits for the owner's hand-over.
