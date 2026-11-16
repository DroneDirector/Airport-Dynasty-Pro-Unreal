# Jetway animation - plan (owner, 2026-11-16)

Owner: animate the jetways so they dock to the aircraft's front door - any aircraft of that size and bigger; control
them from inside at the cab's console (E, like every interaction: a menu "Dock / Undock"; later a real remote on the
jetway); and a small button in the lower right of the screen that sends the jetway out or back.
His rules ([[jetway-rules]] in memory): his own jetways only; at rest pulled HALF WAY BACK (wheels half as far out from
the rotunda as when docked - PARK = 0.5); FRONT (L1) door only; hoses hang in their pipes; nothing stretched.

## The source of truth: his generator
Airline-Manager-Pro-Babylon/src/sections/terminal/jetway.ts already has the whole movement (lines 553-643):
- the WHEELS LEAD: the drive unit's point follows a gentle curve (quadratic, bowed away from the building) from the
  rest point Wp (PARK 0.5) to the docked point Wd; the tunnel's yaw, pitch and length come from that point (ik):
  yaw = atan2(x - rx, z - rz); dh = |(x,z) - rotunda| - (RR - 0.25); pitch = atan2(yR - h, dh);
  len = hypot(dh, yR - h) + BD + SEC[2].l - zW
- setPose: boom (ROTUNDA-SLEEVE) yaw, its pitch child; sections: z0 = BD, z3 = len - BD - SEC[2].l, middle one between
  lo = max(z0, z3 + OV - SEC[1].l) and hi = min(z0 + SEC[0].l - OV, z3, z3 + SEC[2].l - SEC[1].l), at (lo + hi) / 2;
  cab at the tunnel's end + dirH * (CR - 0.25), cab yaw = pose.cab, cab sleeve yaw = yaw + pi - cab; bogie at the
  drive unit's point (zW on section 3) on the ground, steering to its way, wheels rolling d / WR
- docking: drive 75 s (S 0 -> 0.92 eased), crawl 30 s (S -> 1, the cab turning square to the fuselage), canopy 15 s
  (canopy then shutter out); undocking: canopy 15 s back, back 20 s, home 60 s (JW_TIMES)
- constants: IW 2.5, IH 2.4, RR 1.8, CR 1.7, FRONT 1.4, BD 1.25, SEC 10.5 / 10.1 / 9.7 m, OV 1.5,
  zW = SEC[2].l - 1.0, LMIN = SEC[0].l + 2 BD, LMAX = sum(SEC) - 2 OV + 2 BD, WR 0.55
Parts in the export (Data/babylon-jetways.json, J-n/...): ROTUNDA, ROTUNDA-SLEEVE, TUNNEL-1..3, CAB, CAB-SLEEVE,
CAB-FRONT, CAB-DOOR (two leaves), CANOPY, CAB-CONSOLE, DRIVE-UNIT, BOGIE, BOGIE-STEER, WHEEL, LIFT-LEG, LIFT-SLEEVE(S),
LIFT-BEAM, LIFT-CABLE, STAIR, STAIR-LANDING, STEP, STAIR-RAIL, STAIR-POST, STAIR-WHEEL, BELLOWS-ROTUNDA, BELLOWS-CAB
(+ linings), PCA-HOSE, PCA-PIPES, BEACON.

## Parts of the work (each 2-3 h, committed on its own)
1. **The rig (Unreal):** a JetwayRig per J-n, its moving nodes found by name in the Jetways ABabylonModel (NRoot, NInst,
   NKids: each subtree's instances with their transform relative to the node), the docked numbers read from the
   export's docked pose (Wd, rotunda, boom pivot), the pose maths above ported one to one, updating the instances each
   frame while it moves. Starts at REST (half way back) - his rule - except where an aircraft is at the stand.
2. **Any aircraft:** the door to dock to = the parked aircraft's front (L1) door (ParkedAircraft: each type's door from
   its maker's airport planning document, to-do 43b); Wd worked out from that door (the cab's front at the door's sill
   height); types too small for the jetway (below its lowest reach) are refused with a note.
3. **The things that bend:** bellows and their linings squeezed / stretched between their flanges (accordion), lift
   cables and PCA hoses re-laid along their path, the stair re-angled with its steps level - as his generator does.
4. **Controls:** E at the cab's console -> a small menu (Dock / Undock, its state shown); the lower-right button (the
   nearest jetway: out if parked, back if docked); the beacon flashing while it moves, the lights inside up when docked.
5. **Test and release:** the stress walk passes through a docked and a moving jetway; pictures for the owner.
