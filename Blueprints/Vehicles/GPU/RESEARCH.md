# Ground power unit (GPU) - research notes (2026-11-21)

Towable diesel ground power unit, 90 kVA, 3-phase 115/200 V 400 Hz (optional 28.5 V DC), the class used at the gate for
A320/B737. Built in Blender by `Tools/make_gpu.py` (shared helpers + checks: `Tools/vehicle_kit.py`).

## Real units looked at (sizes)

| Unit | L x W x H (mm) | Weight | Notes |
|---|---|---|---|
| Guinault GA 90 (GA series 90/140/180 kVA) | 3300 x 1880 x 1790 | 2200-2700 kg | white moulded canopy on a galvanised chassis tub, cable trays along both sides, fifth-wheel steering; drawbar stands up when parked |
| TLD GPU-409 / 409 E-CUP | 2845 x 1727 x 1499 (112 x 68 x 59 in) | ~2400 kg | galvanised steel chassis, powder-coated aluminium + fibreglass panels, fifth-wheel steering, no-drop tow bar, rubber torsion axle, 174 l tank, Cummins/Deutz diesel, 28.5 V DC option |
| Hobart / ITW GSE 4400 90 kVA | ~2616 long incl. drawbar (103 in) | ~1800 kg dry | low-profile towable trailer, fifth-wheel steering, double-skin polyethylene canopy, cable trays, Cummins QSB4.5 |
| Avro 90 kVA (11-4090-0000) | 2883 x 2007 x 1892 | 1724 kg | 132 l tank |
| GPU-490 (90 kVA) | 3023 x 2108 x 1930 | - | |
| Jiangsu Tianyi TY-GPU90PT | 2570 x 1700 x 1700 | - | |

Sources: https://fortbrand.com/products/tldgpu-409-e-cup/ , https://fortbrand.com/products/hobart-4400-90kva/ ,
https://www.directindustry.com/prod/guinault-sa-france/product-126909-1545139.html (GA series),
https://www.airport-suppliers.com/?p=115760 (Guinault GA range), https://proservaviation.com/products/11-4090-0000 (Avro),
https://www.aeroexpo.online/prod/jiangsu-tianyi-aviation-industry-co-ltd/product-168761-83500.html

Output cable on these units: 4-core 400 Hz cable, about 40-50 mm thick, typically 15-25 m, stored in side cable trays or
on hooks/a reel; the aircraft end is the 6-pin (MS 90362 / NATO type) connector, head about 120 mm across, with a handle.

## Reference photos (several angles; looked at for shape only - no brand, no livery, no photo used as a texture)

- Guinault GPU, 3/4 front-right: canopy with rounded edges, galvanised tub with cable trough along the side, drawbar
  standing up, extinguisher box on the front, plug holder at the corner -
  https://commons.wikimedia.org/wiki/File:Luxair,_ground_power_unit-101.jpg (CC BY-SA 3.0 lu)
- Older diesel GPU (Lechmotoren), 3/4 rear-left: control panel behind a window, louvres, beacon on the roof, drawbar up
  - https://commons.wikimedia.org/wiki/File:Luxair,_ground_power_unit-102.jpg (CC BY-SA 3.0 lu)
- TLD-type GPU on the apron next to an A320 (side view, plug holders on the front, galvanised frame) -
  https://commons.wikimedia.org/wiki/File:Volotea_Airbus_A320_EC-OEI_Milan_Bergamo_2024_(02).jpg (CC BY-SA 4.0)
- TLD-type GPU parked at an A320's nose, cable to the nose area -
  https://commons.wikimedia.org/wiki/File:Air_Arabia_Airbus_A320_A6-AOV_Krakow_2024_(01).jpg (CC BY-SA 4.0)
- GPU connected under a narrow-body's nose (cable rising to the belly ahead of the nose gear) -
  https://commons.wikimedia.org/wiki/File:201701_EI-XLD_with_GPU_supply.jpg (CC BY-SA 4.0)
- Small towed unit, cable coiled on hooks, plug detail - https://commons.wikimedia.org/wiki/File:Aircraft_Ground_Power_Unit_at_Wagga_Wagga_Airport_01.jpg (CC BY-SA 3.0 au)
- More: https://commons.wikimedia.org/wiki/Category:Ground_power_units

## What the model is (Blender metres; x forward = drawbar end, y left, z up; origin on the ground under the chassis)

- Overall 3373 long with the drawbar lowered (rear lamp lenses x -1.244 to towing eye x +2.129), 1750 wide (over the side
  reflectors; trays 1732), 1827 high (exhaust rain cap; roof 1640, beacon top 1805). Body (bed plate) 2200 long.
- Wheelbase 1400 (front turntable x 0.62, rear axle x -0.78), track 1400, tyres 560 x 165 (5.00-10 class, real lathed
  profile, no modelled tread). Front axle steers on a fifth-wheel turntable, +-40 deg checked clear.
- White moulded canopy (rounded edges), galvanised chassis tub (narrow at the front for the steering), bed plate that is
  also the floor of the two side cable trays; engine door (left), generator door (right), control panel behind a
  windowed door at the rear, emergency stop outside on the rear face, radiator grille on the sloped top-front, beacon,
  lifting eye and exhaust on the roof, fuel filler left-rear, air outlet right-rear, extinguisher box + the plug's dummy
  socket + the cable gland on the front face, 20.4 m of 45 mm cable in 5 loops in the right tray (+ ~2 m of runs),
  2 wheel chocks in the left tray, tail/brake + indicator + reflectors set into the rear bumper, side and front reflectors.

## A320 external power receptacle (for the main session)

Source: Airbus "A320 Aircraft Characteristics - Airport and Maintenance Planning" (AC), Issue Sep 30/85, Rev May 01/17,
section 5-4-4 Electrical System + 5-4-3 Grounding Points (copy: https://skybrary.aero/sites/default/files/bookshelf/2369.pdf,
the A321 AC Mar 01/22 gives the same figures):

- A/C External Power, Access Door 121AL: **2.55 m aft of the nose, on the centreline, mean height 2.00 m above ground**
  (distances approximate). One receptacle to MS 90362-3, 90 kVA, 3-phase 115/200 V 400 Hz.
- Nose landing gear (grounding stud on the NLG leg): 5.07 m aft of the nose, on the centreline, 0.94 m high.
  Nose tyres: 30 x 8.8 R15 (about 0.76 m diameter), twin.
- So, measured from the nose tyres (NLG leg / axle line): the receptacle is **about 2.52 m FORWARD of the nose wheels**
  (towards the nose), **0.0 m sideways (on the centreline; the door's 'L' suffix puts it just left of centre, within
  ~0.1-0.2 m)**, **2.00 m above the ground**, on the underside of the forward fuselage. NOTE: it is NOT aft of the nose
  gear on the right-hand side - it is ahead of the nose gear, under the forward fuselage.
- Crew practice: the GPU is parked off to the side ahead of the nose gear, outside the nose wheel's path, and the cable
  is run up to the belly; the plug is pushed straight up into the receptacle.
