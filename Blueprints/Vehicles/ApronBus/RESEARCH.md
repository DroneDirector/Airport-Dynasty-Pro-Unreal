# Apron bus: research

This file describes what the real buses do and what our own design takes from them. The model is our own design:
no brand names, no logos, and no copied styling.

## #region ── What it is ──

The apron bus is the airside bus that carries passengers between the terminal and remote stands. It is low and
wide, has very few seats and a lot of standing room, and has wide doors on both sides, so it can load at a gate
on either side and unload at the aircraft stairs. It only drives on the apron at low speed, so it is never
registered for the road.

## #endregion

## #region ── Real sizes (the e.COBUS 3000 / COBUS Vega class) ──

| Item | Real | Our model |
|---|---|---|
| Length | 13,920 (e.COBUS 3000); Wikipedia gives 13,800 for the Cobus 3000; 14,000 for the Vega | 13,920 |
| Width | 3,000 | 3,000 |
| Height | 3,100, with the batteries on the roof | 2,860 body, 3,100 with the roof batteries and A/C |
| Headroom inside | 2,400 | 2,400 |
| Wheelbase | 7,100 | 7,100 |
| Entrance step | about 290, with pneumatic kneeling | floor 330, kneels 80 to 250 |
| Turning radius | 14,069 | (game: 14.1 m) |
| Doors | 3 extra-wide doors on each side, plus a front right door (Vega) | 3 + 3 double doors, 1,900 clear, plus a front right door |
| Passengers | up to 110, of which 8 to 14 seated | 9 seats on three wheel housings (the front left one is under the cab), about 100 standing, a wheelchair or pram space |
| Gross weight | 20,400 kg | (game: 20.4 t) |
| Body | aluminium | (material: the airport's livery, yellow by default, changeable) |
| Tyres | 275/70 R22.5 (city bus size) | 275/70 R22.5, single front, twin rear |
| Overhangs | eCitaro for comparison: 2,805 front, 3,430 rear | 3,200 front, 3,620 rear |

## #endregion

## #region ── What the model has ──

- **Wheels and drive:** a front beam axle with kingpins, spindles and steering pivots, and a rear drive axle with a
  motor and twin tyres.
- **Floor:** a low floor with vinyl. The wheel housings are boxed and hollow over the wheels, with step plates.
- **Doors:** every passenger door has two glazed leaves that swing out on pivot posts. Each door has rubber edges,
  handle bars, a warning lamp over it, yellow step edges, step lights, open buttons inside and outside, and an
  emergency release.
- **Window band:** pillars, glazing with tilting top lights, door headers, cant rails and the roof.
- **Front:** a raked windscreen, a destination display, headlamps, daytime running lights, indicators, markers,
  "rabbit-ear" mirrors, wipers with their pivots, a bumper and a tow eye.
- **Rear:** the rear window, tail and brake lamps, indicators, reversing lamps, markers, a third brake light and a tow
  eye.
- **Roof:** four battery pods with ribs, an A/C unit with fans, two roof hatches and an amber beacon.
- **Inside:**
  - A ceiling lining, LED light strips and A/C ducts.
  - Stainless and yellow grab poles with their feet and tops, and stop buttons.
  - Two ceiling rails with hangers and hanging straps.
  - 9 seats on the wheel housings, with grab rails.
  - Two luggage racks.
  - A wheelchair backrest with its rail.
  - Two double-sided flight information screens.
- **Driver's cab (front left):**
  - A partition with glazing and a lockable door.
  - The dashboard with its cluster and the door switch panel.
  - The steering wheel and column, and the pedals.
  - A suspension seat with a headrest, a sun visor and a fire extinguisher.

## #endregion

## #region ── IK points and pivots ──

- **Driver:** seat, back, both hands on the wheel, accelerator, brake, door controls and the cab door handle.
- **Passengers:** each pole hand-hold, each strap loop, each seat, the luggage racks and 4 standing spots.
- **Wheels:** the wheels and the front steering pivots.
- **Pivots** (named PIV_…): every door leaf, the wipers and the cab door.

## #endregion

## #region ── Lights and sounds (for Unreal) ──

- **Lights:**
  - Headlamps, daytime running lights, indicators, tail and brake lamps, reversing lamps, markers and the
    third brake light.
  - Door warning lamps (they flash while the doors move) and step lights.
  - Interior LED strips, the destination display and the amber beacon.
- **Sounds** (Sonniss GDC bundles or CC0):
  - The electric drive whine, the air brake and kneeling hiss, the door chime and door air.
  - The A/C fans, the indicator relay, the reverse beeper and the horn.

## #endregion

## #region ── Sources ──

- e.COBUS 3000 data sheet (COBUS Industries): length, width, height, headroom, wheelbase, step height, kneeling,
  turning radius, doors, capacity and weight.
- Wikipedia, "Cobus 3000": 13.8 m long, 3.0 m wide, 3.1 m high, 7.1 m wheelbase.
- COBUS Vega launch material: 14 m by 3 m, batteries on the roof, 3.1 m high, three extra-wide doors per side and a
  front right door.
- City bus tyre size 275/70 R22.5, and the eCitaro overhangs for comparison.

## #endregion
