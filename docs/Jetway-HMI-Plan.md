# Jetway HMI - the control screen on each jetway's console (to-do 115)

Owner, 2026-11-18: the screen on the jetway console becomes a real, working HMI (operator touch screen) for that jetway.
The same system as the office PC monitors (the game's web screens: E opens it, the camera goes up close), with its own
custom UI. Built in parts (2-3 h each), each part committed on its own; not published until he says.

## How you use it
- Walking in the cab: the screen gets the thin interact outline (as other interactables).
- **E** (X on a pad): the camera glides to just in front of the screen - its path checked so it never passes through the
  cab, the console's hood or the canopy - and the HMI page opens on the screen. Mouse/touch on the page.
- **Esc / E** again: back to walking, the camera returns the same way.
- One jetway each: the HMI shows and drives only the jetway you stand in (gate number in its top bar).

## The screens (tabs along the bottom, as on real HMIs)
Top bar on every screen: gate + jetway number, mode (AUTO / MANUAL / MAINTENANCE / E-STOP), operator logged in, clock,
and an alarm lamp (red = active alarm, amber = warning, grey = none).

1. **Position (home)** - plan view and side view of this jetway, drawn live from the game:
   - rotunda angle, tunnel extension (m, and % of its travel), cab rotation (deg), height (m above apron), wheel bogie
     angle and steering direction, drive speed.
   - the aircraft at the gate (its outline) and the target door, with the distance cab-to-door (m) and the alignment
     (left/right, up/down) - green when within tolerance.
   - buttons: DOCK / UNDOCK (the real jetway moves), STOP, and jog arrows (in, out, left, right, up, down) in MANUAL.
2. **Pre-positioning** - the stored positions per aircraft type and door (A320 1L, B737 1L, A330 1L/2L, B777 1L/2L,
   A380 M1L/M2L/U1L...). Choose one, GO: the jetway drives to it and waits ("pre-positioned"); the same door data the
   game docks with already (DoorOf).
3. **Cameras** - the door camera (as now: CAM 1 - DOOR 1L, door outline, green guide line), the bogie camera
   (wheels, apron below) and the cab-floor camera; live pictures from small scene cameras (low resolution, only while
   the screen is open - no cost otherwise).
   **The cameras in 3D on every jetway** (owner, 2026-11-18: "create the cameras in 3D and place them on the jetway,
   find where they are located in real life, and how they look"). Where real ones sit (research 2026-11-18):
   - *Bogie camera* - a weatherproof bullet camera (white housing, sun shield, swivel bracket, cable in a conduit)
     under the tunnel at the drive column, looking down at the drive wheels and the apron round them; shown on the
     console all the time (Syracuse airport PBB specification: "a camera image of the PBB horizontal drive wheels
     area").
   - *Door camera* - a small camera inside the cab at the front, high above the doorway, looking out at the aircraft
     door (ShinMaywa's docking camera "installed inside the cab at the front"); and an *outside door camera* on the
     cab's front roof edge (newer bridges: AI/stereo cameras outside the cab find the door - TK Elevator, ShinMaywa).
   - *Cab ceiling dome* - a 360 degree dome camera in the cab ceiling, watching the operator and the cab.
   Modelled from reference photos our own way (Art/JetwayCamera/JetwayCamera.blend), instanced + Nanite, riding with the
   part they're fixed to; each feeds its picture on the HMI.
   How the pictures look (as a real jetway's CCTV): wide-angle with a little barrel bend at the edges, slightly soft
   and grainy, muted colour (the bogie camera at night black-and-white with its IR glow), a timestamp and camera name
   burnt in, the door camera with its docking guide lines over it.
4. **Auto-levelling** - the cab floor against the aircraft door sill: height difference (mm), auto-level ON/OFF, the
   levelling wheel state; as the aircraft is loaded or unloaded the game's aircraft sinks or rises a little and the
   cab follows.
5. **System status** - hydraulics (pressure bar), electrical supply (400 V, current), drive motors (left/right, temp),
   lift columns, control system, E-stop circuit, cab door, canopy (folded / extended, sealed), PCA air and 400 Hz
   ground power (the yellow hoses/cable) - each a green/amber/red tile.
6. **Safety** - aircraft proximity sensors, collision bumpers, anti-crush edge, wheel/steering interlock, aircraft
   contact switch, travel limits (max extension, slope, rotation) - live states; what is stopping a move right now.
7. **Alarms** - active alarms (time, code, text, acknowledge), and **Alarm history** - the past faults and events of
   this jetway (stored in the save game).
8. **Maintenance** - diagnostics (hours run, docks since service, motor hours, hydraulic oil, brake test), run a system
   check (it really runs: each part tested in turn, the jetway moves a little), service due date.
9. **Settings** - (authorised only) drive speed limit, auto-level on/off, cameras on/off, screen brightness.
10. **Login** - Operator (dock/undock, pre-position, cameras), Maintenance (system check, settings), Supervisor (alarm
    reset of a lock-out). You (the airport's owner) can log in as any of them; the crew use their own roles.

Alarms the game can really raise (from its own state): Emergency stop activated; Hydraulic pressure fault; Motor
overload; Sensor malfunction; Maximum extension reached; Incorrect operating position (the cab not square to the door);
Safety interlock activated (someone in the way, a door open, the canopy not folded before moving); Aircraft contact lost.

## Built on what the jetways already do
JetwayRigs already drives each jetway (rotunda, tunnel, cab, lift, bogie), docks to an aircraft type's door, has the
service door and the console (Console()), the door camera picture, the lights and beacons. The HMI reads those values
and calls Dock / Undock / jog through the same code - so it works for that specific jetway, for the player and the crew.

## The crew (NPCs) using it - what, when and why
They use only what the job needs, at the right moments, from the cab, standing at the console:
- **Before an arrival** (aircraft on approach, ~10 min out): the jetway operator walks out to the cab, logs in
  (Operator), checks System status and Alarms are clear, selects the **pre-position** for the arriving aircraft's
  type and door, GO - the bridge waits near the stand. Why: docking is faster and safe when the bridge is ready.
- **Arrival** (aircraft stopped at the stop line, engines off, beacon off): Cameras -> door camera, then DOCK; watches
  the distance/alignment; auto-level ON; opens the canopy; then knocks on the aircraft door. Why: real procedure -
  the bridge is only moved when the aircraft is safe (beacon off).
- **During the turnaround**: auto-levelling watched while passengers and bags come off and on; a warning (aircraft
  sinking) is acknowledged.
- **Before departure** (doors closing, ~5 min before off-block): retract the canopy, UNDOCK to the parked position,
  check "jetway clear" on the Safety screen so the pushback can start. Why: the pushback must not start until the
  bridge is clear - the game can hold the pushback until the HMI says so.
- **Maintenance checks** (night, no flights at that gate, or every N docks): a technician logs in (Maintenance), runs the
  System check, looks at Alarm history, books a service if due. Why: keeps faults (hydraulics, motor) from happening;
  a skipped service raises the chance of a fault (delays).
- **Faults**: when an alarm comes up, the operator acknowledges it; a lock-out needs the Supervisor or a technician -
  the gate's turnaround waits (a delay the player sees and can avoid by keeping service up).

Ideas for later (not in the first build):
- Jetway faults as a game event (random by condition: hydraulic leak, motor overload) - the gate needs stairs and a bus.
- A training/licence for jetway operators in the academy; untrained crew are slower to dock.
- Weather on the HMI: wind speed at the cab (high wind limits), ice warning (heating on the cab floor in winter).
- The HMI's history counts in the airport's safety rating.
- The player can take over from a crew member mid-docking.

## Parts (working hours; 1 day = 8 h)
1. E on the screen, outline, the camera up close with no clipping, the HMI page opens on that jetway (empty frame, top
   bar, tabs) - 2-3 h
2. Position screen live (plan + side view, values), DOCK/UNDOCK/STOP, MANUAL jog driving the real jetway - 3 h
3. Pre-positioning (stored positions per type/door) + Auto-levelling - 2-3 h
4. Cameras: the 3D camera models placed on every jetway (bogie, door inside + outside, cab dome), and their live
   CCTV-looking pictures on the page, only while open - 4-5 h
5. System status + Safety + Alarms + Alarm history (in the save) - 3 h
6. Maintenance (system check that runs) + Settings + Login roles - 2-3 h
7. The crew using it (arrival, departure, maintenance, faults), pushback held until the bridge is clear - 3 h
Total about 19-23 working hours (about 3 days).

## Owner's chosen concept (2026-11-18, his ChatGPT chat "Jetway Monitor Functions") - this leads the design
**A mode-adaptive HMI with CCTV built in:**
- **AUTO docking** (a stored / remembered position for the aircraft type and door): the bridge docks by itself; while
  it does, every HMI screen and the CCTV can be used freely - the docking goes on in the background. The docking
  camera comes up as the main view when it starts.
- **MANUAL docking** (joystick): the HMI switches by itself to a focused docking screen - aircraft detected (about
  1.5 m out), distance, slow approach, collision protection, bumper switches (at least two must close), cab rotation,
  wheel direction, elevation, tunnel extension, safety interlocks (which moves are allowed now) and CCTV (the bogie
  camera while moving, the aircraft-door camera on the final approach); then trim lowering -> "TRIM ON AIRCRAFT" ->
  canopy. Alarms and safety stay reachable.
- **A status strip on every screen**: mode (AUTO / MANUAL), docking status and %, aircraft detected, safety system,
  CCTV available, emergency stop.
- **Navigation**: HOME, CCTV, BACK, DOCKING; you can leave the HMI at any time - leaving never stops a docking, never
  turns off safety monitoring or hides an alarm.
- **Main menu**: Docking, CCTV, System status, Alarms (+ history), Settings, Maintenance.
- **Cameras**: 1 Bogie, 2 Aircraft interface (door, canopy), 3 Apron, 4 Bridge exterior, 5 Tunnel interior; any one
  full screen; back to the screen you came from.

## On the monitor itself (owner, 2026-11-18)
- The HMI is **live on the jetway's monitor in 3D before you touch it** - you see its screens from across the cab.
  Performance: one live HMI at a time (the cab you're in or nearest, within a few metres); the other jetways' monitors
  show a still picture of their home screen (glow by material only).
- **E** (outline on the monitor) zooms **as far in as possible** - the monitor filling the view - with no camera
  passing through anything; you then use the HMI on the monitor itself (the mouse on the screen).
- **A flat monitor**: the console stays as it is, but its hooded screen becomes a **flat industrial touch monitor** on
  the console (as on real jetway consoles: a panel PC set into / on the sloped console top, bezel, mounting), the
  console's geometry adjusted to take it - modelled from real photos.
