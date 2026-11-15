# Terminal: the new floor-2 sections (owner, 2026-11-15) - collecting, more photos coming

Plan picture 1: terminal-floor2-plan-1.png (roof plan with the new parts in turquoise; his red arrows, a turquoise
painted strip next to the museum).

- **Doors** to all the new sections.
- **Escalator:** its entrance on floor 1 has no door and no wall in front of it, as wide as the corridor; it starts on
  floor 1 and ends exactly at floor 2's height.
- **New offices:** reached by a working lift from the staff area's hallway (as on his photo), the lift's door on
  floor 2 opening into the offices. The offices reach north all the way to the gate corridor's (pier's) outer wall
  (red arrow on the right).
- **Offices' window:** one very big window along that side, almost the whole wall, ~35 cm left at each end; the same
  glass as the terminal's; frames between the panes like the main hall's big windows; panes spaced evenly so every
  gap is the same.
- **Museum:** a door in its outside wall facing away from the apron, on the passenger side. The museum stretches north
  all the way to the gate corridor's outer wall (red arrow on the left) and fills across to the small section painted
  turquoise next to it.
- **No clipping** anywhere where the new walls meet the corridor walls.

Drawings 2-9 (terminal-floor2-plan-2..9.png): east and west side elevations (old and with the new parts), the east
end close-up with the inset of the offices' west wall, the south front elevation, the north back elevations, the
west end close-up (museum 36 m with its entrance from outside, escalator room 36 m: landing on floor 2 21 m, 15 m open
to the roof). Sizes: museum 36, escalator 36, Grand Hall 78, offices 42 over the staff wing, lift wing 12 (to +13.20),
204 m terminal with the new parts; floor 2 at +6.00, roofs +12.00.
- **The offices' big window = their west wall onto the Grand Hall** (inset): glass from the office floor (+6.00) to the
  ceiling (+11.40), flush with the wall's face, frames like the gates' windows; ~35 cm of wall left at each end; every
  pane the same width, every frame gap the same.

Owner's go (2026-11-15), the blueprints confirmed, plus:
- **Two escalators side by side**, one up, one down, as in a real airport, in fine detail (steps with their cleats,
  the yellow edges, comb plates, balustrades, handrails). Built so the steps run endlessly: they go round under the
  floor plates at each end, never seen on their way back; you always see the steps and where to step on.
  References: Commons photos (CC BY / CC0 / PD), Art/Escalator/Reference.
- **From the landing on Floor 2 a wide open way into the long gate walkway** - no door at all, wide, designed to
  look very nice.
- When done: pictures of every room, outside and inside: the escalators and the lift on Floor 1 and Floor 2, the
  museum.
- Open question sent to the owner: the Floor 2 bar (x 104-146, z 30-52) lies where the offices would stretch north.

Progress (2026-11-15 evening):
- Owner: "if there is any open ground, then just fill it so there is actually a floor" (the west end: today the
  Vortex Gallery pod stands at x 16-36 with grass west of it).
- Escalators: model, moving steps, smoothed handrails done (Art/Escalator, Tools/import_escalator.py, Escalators.h);
  not yet placed in the terminal; nothing of the floor-2 changes published.
- Tools ready: Tools/terminal_where.py (where every terminal piece is, in plan metres), Tools/terminal_cut.py (cut boxes
  out of merged terminal pieces inside Unreal), -UIPlanView= camera pictures.
- Open: where the floor-2 toilets and family room go once the bar moves to x 72-114.

Built (2026-11-15, committed): west end (museum hall with the old museum's insides, escalator room, landing with
guardrail, open way to the link/food walk, toilets + family room moved onto the landing, plant room narrower), the bar
at x 72..114 open to the walkway, tax free and airside landing x 56..72, the offices x 114..156 to the walkway with the
big window, the staff lift (inside the staff entrance hall's NE corner, not outside: the car rental's covered walk is
at x 156+), the escalators placed. Not yet: a release with it; the lift's ride not tested by a walk; museum door is
fixed glass (not a sliding door yet).
