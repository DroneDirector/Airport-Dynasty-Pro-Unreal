# Turnaround plan (owner 2026-11-21)

Every aircraft at a gate gets a plan of real tasks. The planner is in `Source/AirportDynastyPro/Private/Turnaround.cpp`.

- **Starting a task:** a task with a vehicle in the game calls that vehicle. A task without one runs as data, with its real time.
- **Delays:** a late task pushes back the tasks that wait for it. A moved departure shifts the departure tasks, keeping the same gaps.
- **New vehicles:** when a new vehicle is built, it joins its task here and everything stays in sync.

## Source

Airbus A320 Aircraft Characteristics – Airport and Maintenance Planning, section 5-2-0, *Full Servicing Turn Round Time* (May 2017): **TRT 44 min**. Section 5-3-0, *Outstation* (stairs, no fuel), gives **22 min**.

Airbus assumptions:
- **Passengers:** 150, using one passenger bridge at door L1.
  - Bridge on and door open: +2 min. Door closed and bridge off: +1.5 min.
  - Deplaning: 20 passengers/min.
  - Boarding: 12 passengers/min, plus 2 min for last passenger seating and headcount.
- **Cargo:** 2 cargo loaders and 1 belt loader. Positioning: +2 min; removal: +1.5 min.
  - Containers: forward hold 3, aft hold 4, at 1.5 min each to unload and 1.5 min each to load.
  - Bulk: 500 kg, unloaded at 150 kg/min and loaded at 120 kg/min.
- **Refuelling:** 20,000 l through one hose on the right wing. Connecting and positioning: +2.5 min.
- **Catering:** one truck, door R1 (4 trolleys) and then door R2 (7 trolleys). Each trolley exchange: 1.2 min. Driving between doors: +2 min. Positioning: +2 min; removal: +1.5 min.
- **Cleaning:** in the time available.
- **Other equipment:** starts at t0 + 1 min.
- **GPU:** up to 90 kVA.
- **Servicing:** potable water 200 l; toilets drained and rinsed.

## Tasks (minutes from on-blocks t0; G = ground time)

| Task | Vehicle | Start | Length | Waits for |
|---|---|---|---|---|
| Jetway on, door L1 open | jetway (3D) | 0 | 2 | – |
| Chocks and cones | crew | 0.5 | 1 | – |
| Ground power connected | GPU (3D) | 1 | 1.5 | – |
| Deplaning | crew | 2 | pax / 20 | jetway |
| Forward hold, 3 containers | cargo loader (later) | 1 | 12.5 | – |
| Aft hold, 4 containers | cargo loader (later) | 1 | 15.5 | – |
| Bulk hold, 500 kg | belt loader (later) | 1 | ~11 | – |
| Potable water | water truck (later) | 1 | 7.5 | – |
| Toilets | toilet truck (later) | 1 | 9.5 | – |
| Catering R1 | catering truck (later) | after deplaning | 8.3 | deplaning |
| Catering R2 | catering truck (later) | R1 end + 2 | 11.9 | R1 |
| Cleaning | crew | after deplaning | until boarding | deplaning |
| Refuelling | fuel truck (later) | after deplaning | 2.5 + litres / 1000 | deplaning |
| Boarding | crew | G − 1.5 − (pax / 12 + 2) | pax / 12 + 2 | cleaning, catering R2 |
| Doors closed, jetway off | jetway (3D) | G − 1.5 | 1.5 | boarding |
| APU started | crew | G − 10 | 1 | – |
| Ground power off and stowed | GPU (3D) | G − 8 | 4 | APU |
| Tug and pushback crew ready | tug (3D) | G − 8 | 5 | – |
| Pushback (the 11 steps) | tug (3D) | G | ~3 | doors, ground power off |

If the ground time is too short for the work before boarding, boarding and everything after it move later. The plan logs how late that aircraft will be.
