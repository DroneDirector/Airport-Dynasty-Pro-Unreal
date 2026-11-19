# Staff daily life - plan (to-do 116)

Owner, 2026-11-18: every member of your staff is a real person with a home in your town, who comes to work by car or
bus, clocks in, works an 8-hour shift from a rota and goes home again - never appearing out of nowhere. Built first,
before the crew use the jetway HMI (HMI part 7). Built in parts of 2-3 working hours, each committed on its own.

## What is there to build on
- **The people:** 138 MetaHumans in `Data/people-roster.json` (name, role, woman/man, age), placed by `AADPPeople`
  (People.cpp), with talk and conduct menus. Roles today: passengers and some staff roles.
- **The town:** `Data/town-buildings.json` (the dealership, your villa, ...) placed along the roads by
  TownBuildings.cpp; the road network (RoadNetwork.cpp, Tools/make_roads.py) from the town to the airport.
- **The terminal:** the staff entrance hall with its lift (ADPGameMode.cpp), the walking map with ways through the
  terminal (WalkMap), the jetway HMI and its gate agent T-10 procedure (docs/Jetway-HMI-Plan.md).
- **Car parks:** the plan has lots (`LOT-*` in Data/airfield.json); a staff car park is not there yet.
- **Game time:** the sun clock (SunClock) runs the real time of day; the web game knows your staff (pilots, cabin
  crew) and the flight schedule.

## The parts

| Part | What | Hours |
|---|---|---|
| 1 | **The person record and the rota.** Each staff member: name, job, home (a house in the town - placeholder addresses along the town's roads until the town has its homes), car or bus, shift pattern (early 06-14, day 08-16 for office jobs, late 14-22, night 22-06), days off, holidays, sick days. The rota for every job and hour (a missing person = a late flight). Saved with the game. Shown on the tablet/PCs (a Staff app: who is on, who is off, who is coming). | 2-3 |
| 2 | **The day clock for staff.** From game time: who should leave home now (shift start minus their travel time), who is on duty, who hands over. Off-shift staff are not in the terminal; on-shift staff are, at their duty place. Logged for checking. | 2-3 |
| 3 | **The staff car park and staff entrance.** A level staff car park by the staff entrance (pad + paving per the building rule), marked bays; the staff entrance door (E/X rule, card reader look); the way from the car park to the door and inside to the changing rooms. Blueprint first for your OK. | 2-3 |
| 4 | **Coming by car.** A staff member's own car (simple car models, own colours) drives from their home along the real roads (RoadNetwork lanes), parks in a free bay, they get out and walk in; at shift end the reverse. Cars far away are cheap (no full simulation off-screen, just their arrival time). | 3 |
| 5 | **Coming by bus.** A bus line town - airport (a bus model, timetable every 30 min, stops in town and at the forecourt); staff without a car walk to a stop, ride, walk in. | 3 |
| 6 | **Clocking in and changing.** In through the staff entrance, the changing room (uniform on if the job has one - the wardrobe's uniforms), then to the duty place; the reverse at the end. Hand-over between shifts at the duty place. | 2-3 |
| 7 | **The duties.** A duty list per job and shift; first the gate agent: to the gate and the jetway console about 20 min before an arrival, the T-10 preparation on the HMI, docking only when the aircraft is parked with engines and beacon off, undocking before departure; breaks. (This is HMI part 7, the crew using it.) Then the other jobs one by one. | 3 + |

About 17-21 working hours in all (2-3 days), in seven parts. Performance: only the people near you are full
MetaHumans; the rest are kept as times and places (a person on the bus is a seat on the bus, a person at home is a
record), so a full rota costs almost nothing.

## Questions for the owner (before part 3)
1. The staff car park: where - next to the staff entrance by the offices (west of the main hall), or across the road?
2. The bus: one bus line between the town and the airport to start with - all right?
3. How many staff on each shift to start with (gate agents per gate, ground crew per stand)?
