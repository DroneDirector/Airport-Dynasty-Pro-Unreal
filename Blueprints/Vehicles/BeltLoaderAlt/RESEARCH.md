# Baggage belt loader (self-propelled, electric) - research notes (2026-11-21)

The conveyor that carries bags and loose parcels between the baggage carts and the aircraft's hold (bulk loading of a
narrow-body such as the A320; wide-bodies use it at the bulk door). A driven chassis with a long belt boom on top whose
front end is raised to the door sill and whose rear end stays low where the bags go on from the carts. Our own design;
no brand names or logos.

## Real units looked at (sizes)

| Unit | Size | Boom / belt | Heights | Other |
|---|---|---|---|---|
| **TLD NBL-E** (electric) | length ~8,100, width ~2,100, height ~1,880 mm, ~4,100 kg (dealer sheet) | - | reaches the lowest door sills on the market (maker) | 15 kW AC drive motor + 4 kW electro-hydraulic pump for steering, lifting and belt (2007 launch note) |
| **TLD NBL** (series, datasheet) | - | boom 7,800 long, 700 wide (780 with the left fixed guide), belt 600; 1,200 kg spread, 600 kg at full boom angle, 250 kg point load; belt 18 m/min fixed (adjustable optional); 9 m boom option | - | tyres 225/70 R15 tubeless; two single-acting cylinders lift steel frames; belt turned by a hydraulic motor; boom and belt controls through solenoid valves; driver's protection grid (without cab) option; folding handrails, full-width front bumper with anti-pinch option |
| **Mallaghan SkyBelt** (electric) | - | - | front sill height ~838 mm (33 in) to 4,190 mm (165 in) | 80 V 300 Ah lithium-ion; AC rear wheel motors with gearbox and hydraulic park brake |
| **TLD NBT-ER** (walk-behind, electric) | - | boom 6,000 long, 600 wide | front 1,122-2,515, rear 742-889 mm | emergency stops on all four corners; 8-30 m/min belt; 810 kg boom down, 455 kg at max angle |
| **TUG 440E** (regional, electric) | chassis 3,175 long, 1,778 wide, wheelbase 1,829 mm | - | - | recessed headlights, tail/stop and reversing lamps, non-slip fender tops, emergency lifting pump |

Sources:
- TLD NBL Electric (dealer, sizes): https://fortbrand.com/products/tld-nbl-electric/
- TLD NBL datasheet (boom, belt, loads, tyres, hydraulics): https://aeroservicios.com/uploads/manuals/NBL_datasheet.pdf
- TLD NBL series options (protection grid, handrails, bumper, belt speeds): https://www.aerospecialties.com/aviation-ground-support-equipment-gse-products/aviation-baggage-cargo-service-equipment/belt-loaders/tld-nbl-belt-loader-gas-diesel-or-electric/
- TLD NBL-E maker page: https://www.tld-group.com/products/belt-loaders/nbl-e/ ; launch note: https://www.aviationpros.com/home/product/10026662/tld-nbl-e
- Mallaghan SkyBelt: https://aviationsouk.com/products/mallaghan-skybelt-belt-loader/ , https://aeroexpo.online/prod/mallaghan/product-168807-79891.html
- TLD NBT-ER: https://www.aerospecialties.com/product/tld-nbt-er-belt-loader/
- TUG 440E: https://aeroservicios.com/uploads/manuals/5135_BL TUG 440E.pdf

## The A320's holds (what it is set to)

- Forward and aft cargo doors 1.82 x 1.24 / 1.23 m, bulk door 0.95 x 0.77 m (Etihad cargo hold sheet):
  https://www.etihad.com/content/dam/eag/etihadairways/etihadcom/2025/pdf/medical/cargo-hold-information-36.pdf
- Door clearances, Airbus "A320 Aircraft Characteristics - Airport and Maintenance Planning" (section 2-7-0): forward cargo
  door about 2.11 m (IAE) / 2.18 m (CFM) on the A320-200, 2.05 m on the A320neo; forward door about 8.16 m from the nose,
  aft door about 22.69 m: https://aircraft.airbus.com/sites/g/files/jlcbta126/files/2024-06/AC_A320_0624.pdf
  (the text extraction did not label these clearly as sill heights: check the figure before relying on the last cm).
- The model is saved set to 2.10 m at the front (the belt top just under the forward hold's sill) and 0.95 m at the rear.

## What the model is

Built by Tools/make_belt_loader_alt.py (Blender; the shared helpers and checks of Tools/vehicle_kit.py, with a small
compatibility layer so it also runs on Blender 4.1+). Our own design in the TLD NBL-E class:
- chassis 4,700 long, 1,900 wide (2,100 over the fenders), wheelbase 3,000, track 1,720, tyres 225/70 R15 (696 dia),
  front wheels steer, rear axle driven (electric motor + differential), 80 V lithium battery box under the driver's deck,
  hydraulic tank and pump at the front, motor controller, charge socket in the rear face;
- boom 7,800 long, 700 wide (780 with the left fixed guide), belt 600 wide with low rubber cleats, drive drum at the rear
  (belt motor outside the right channel), idler drum at the front, roller bed, return run, side channels with lightening
  slots standing 100 above the belt, rear loading flares, rubber nose bumper with a yellow anti-pinch edge, work light
  under the nose, folding handrails (1 m above the belt) along the front 3 m with the front control box (belt in / out,
  boom up / down, emergency stop) and an amber strobe;
- the lift: rear arm (1,000) pinned to the boom 1,350 from its rear end, front arm (2,700) with a roller in a track
  under the boom; each with its own hydraulic cylinder (chrome rod, hoses). Front 0.84-4.19 m, rear 0.85-1.30 m.
  Art/BeltLoaderAlt/beltloader-rig.json has the sums and the presets (A320 forward / aft / bulk, lowest, highest);
- operator station on the left: suspension seat with bellows, armrests, boom joystick and belt switches on the right
  armrest, steering wheel on a raked column out of a dash pod with a display, 2 pedals, park brake, protection grid on the
  boom side, roof guard (2,050) with the amber beacon on its rear post, step + grab handle, convex mirror;
- lamps set into the faces: headlamps, front and rear indicators, tail/brake, reversing, side markers; reflectors;
  emergency stops on the chassis front corners and the boom's rear corners; rear pin hitch, front tow eye.
- 354 parts, 30 IK / pivot points; the automatic checks: 0 floating, 0 lamp clashes, 0 overhangs, 0 loose, 0 flickering
  faces. Blueprint: BeltLoader-Blueprint-1..4.png (wireframe views from the real mesh in wire/).
