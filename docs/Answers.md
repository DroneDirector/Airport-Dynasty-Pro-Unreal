# Answers to the owner's questions (kept so they don't need asking again)

Updated 2026-11-13. Newest first within each section. The PDF next to this file (docs/Answers.pdf) is made from it by
Tools/make_answers_pdf.py.

## The ground: puddles, cracks, tyre marks, edges, the apron

**Can the whole airport move to another continent without redoing it?** Yes. The airport is laid out in its own
coordinates (Data/airfield.json: the layout plus a latitude, longitude, height and heading); moving it means new numbers
there. Every ground effect (cracks, puddles, tyre marks, edges, fillets, markings) is placed from the layout and world
position, so it all comes along by itself. Only the place itself changes: the terrain under it is flattened to the
airfield's grade automatically; outside Iceland and the Faroes the world height map is coarser, and the land around
(grass, town) follows the land-cover data there.

**Do the ground effects cost many frames, and how sharp can they go?** Little: about 20 texture reads per pixel, well
under 1 ms on the owner's PC. Resolution costs graphics memory, not frames. The scans are 4K now (about 1 mm per pixel on
the asphalt); 8K exists for most of them. Planned setting "Airport ground": Low 2K, Medium, High 4K, Ultra 8K with fine
relief.

**Why don't the textures show square repeats?** Stochastic tiling (as the big games do it): every few metres the scan is
turned and shifted at random, cross-faded softly. It is fixed to the ground, so nothing pops when the camera moves.

**Is there grass built into the taxiways that should be removed?** No. The green islands are the one grass sheet under
the whole airfield; the 3D grass grows only where a ray from above hits that sheet, never on paving.

**The taxiway joins:** real fillets (curved widening), curved yellow lead-on and lead-off lines, ICAO double edge stripes,
7.5 m shoulders, and the grass edge following the whole outline: copied from real airports (Boston, Seattle, Frankfurt).

**Ground markings:** every marking and every distance (hold-short lines, the red runway-ahead signs before them, stand
lines) follows the ICAO Annex 14 and FAA diagrams. Cracks run through the paint, in the cracks' own colours.

**Snow:** not part of the puddles; a separate light snow layer in the same master material, from winter photos of real
aprons, taxiways and runways.

**Iceland's torn mountains (walls and steps):** the Icelandic height data meets the world height map without a feathered
seam, and gaps are stretched into walls. Fixable for sure (on the to-do list); elsewhere only the world map is used, so
no walls, just softer detail.

**Are cracks, puddles and tyre marks part of the asphalt picture?** No. The master ground material works in separate
layers: (1) the surface itself (the asphalt or concrete scan, or a picture), (2) cracks and tar sealant, (3) tyre
rubber, (4) wetness and puddles on top. Changing the asphalt is one setting (a new picture); every other layer stays
and works on the new surface straight away. Nothing has to be redone.

**If the apron is made wider or deeper later, do the details have to be drawn again?** No. Cracks, sealant, wear,
puddles and tyre marks are placed by world position; the runway's tyre marks by the runway's own line; the gate dirt
and markings by the stand positions in the layout file. A bigger or moved apron gets everything in the right places by
itself.

**Is the ground seamless from runway to taxiway to apron to the stands?** Yes: all of it uses the same world-space
patterns, so cracks, stains and textures run straight across from one piece to the next. Where the material itself
changes (asphalt to concrete), the joint looks like a real construction joint.

**One master ground material:** every paved surface (runway, taxiways, aprons, markings, the terminal's apron, the
rental car park, and any new airport or road) is a version of one master, so every effect is made once and shows
everywhere. The terminal apron keeps its own picture.

**Tyre marks:** strong in the runway's touchdown zones (hard landings and braking), faint along some stands' lead-in
lines only, each different, much weaker than on the runway.

**Gate dirt:** dirt and stains at the gates (spills, engine starts), never the same at two gates.

**Puddles on other airports (30,000+):** they live in the ground material, so every airport built with the game's
ground gets them automatically; the number of airports makes no difference to performance.

**Drying in an Iceland-like climate (5-12 °C, no snow):** thin wet film on pavement 1-3 h with sun and wind, 4-8 h
overcast and still; puddles half a day to 2 days, shrinking from the edges; drops on windows 1-3 h (sheltered glass
longer). Tunable in Settings (drying time).

**Reflections:** the puddles reflect the real sky now; aircraft and buildings in the water need the game's sky light
fixed (a separate lighting job; it improves reflections everywhere).

## Clothes

**Can a garment be changed later (another shirt, other shoes)?** Yes, without redoing everything: each garment piece is
its own part with its own material; a new one goes through the same route in about 10 minutes, and the wardrobe picks
it up.

**Many colours of the same garment?** Yes, without remodelling: colours and fabrics are settings on the same mesh;
patterns (stripes, checks, prints) are a picture on it. Only a different cut needs new modelling.

**Will the clothes fit bigger people?** They are made on your body; Epic's resizing fits them to other bodies (the AI
people) - tested when the crew uniforms go on the people.

**Prints on the t-shirt:** the print is its own high-detail layer; any image or text can be swapped in later.

## How the work runs

**Stopping:** safe at any time; finished parts are committed as they go, so a stop loses at most the step in progress.

**Releases:** collected, except the puddles (0.17.10, released on its own at the owner's request). A release costs
about 1-1.5 hours (packaging, testing, upload).

**The MCP server (Unreal's link for Claude Code):** lets changes be made and seen in the open editor in seconds instead
of a hidden editor run (1-2 min) plus a test launch (3-5 min) each time: faster material tuning, placing things, laying
out roads and the town. It doesn't speed up Blender modelling or packaging.

**Gate areas deeper:** about 1 day for the gate area alone, 1.5-2 days if the building grows towards the apron (jetways,
stands and apron lines move with it), synced with the Android edition.
