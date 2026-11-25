# Rain on the glass (to-do 295)

Part A (built): the drops, the weather's part, every vehicle's windows, the wipers.
Part B (open): the terminal, tower, hangar, fire station, jetway and aircraft glass.

## What there is

- `Tools/make_glass_rain.py` makes `/Game/Weather/MPC_GlassRain`, `/Game/Weather/MF_GlassRain` and `/Game/Truck/M_TruckGlass`. It may be run again at any time (it refills the assets). `Tools/set_water_materials.py` no longer makes M_TruckGlass, so the order of the two does not matter. While it clears the old nodes the log shows "Failed to compile" lines for M_TruckGlass with "missing input": those are half-cleared states. What counts is the line `M_TruckGlass: pixel instructions N` with no failure after it.
- `GlassRain.h/.cpp`: `UGlassRain` (a world subsystem) writes the collection every frame while there is water: GlassFill 0..1, GlassDry 0..1, GlassTime (seconds, wraps at 3600 without a jump), GlassRun (how many runners). `adp.GlassRain 0` turns it all off.
- Filling: about 4.5 minutes in ordinary rain, shorter in heavy rain, much longer in drizzle. Snow: nothing. Drying: about 5 minutes, faster in sun and wind.

## Part B: putting it on other glass

Add a MaterialFunctionCall of MF_GlassRain. Every input has a default; a fixed pane needs only Outer, Shelter and PaneSeed.

| Input | Type | Default | Fixed glass |
|---|---|---|---|
| PositionCm | V3 | the instance's local position | leave |
| PaneNormal | V3 | vertex normal, instance space | leave |
| Gravity | V3 | world down, instance space | leave |
| PaneSeed | S | 0 | any number, e.g. PerInstanceRandom * 50 |
| Shelter | S | 1 | 0 indoors or under a canopy |
| Outer | S | 1 | 1 on the outside face, 0 on the inside |
| Lean | V4 | 0 | leave (aircraft: backwards in that frame, w 0..1) |
| WiperA0 | V4 | 0 | pivot, inner radius |
| WiperB0 | V4 | 0 | direction at rest, outer radius (0: no blade) |
| WiperC0 | V4 | 0 | axis, sweep in radians |
| WiperA1, WiperB1, WiperC1 | V4 | 0 | the second blade |
| WiperT | V4 | (1000,1000,1.5,0) | seconds since the sweep began, how long before that the one before began, seconds there and back, its number |
| WiperH | S | 0 | how far the blade passes beside its pivot, cm |

Outputs: Normal (tangent space: needs the material's Tangent Space Normal on), Roughness (use `max(own, it)`), Opacity (add: `own + (1 - own) * it`), Specular (0..1 water: lerp own to 0.5), Lens (0..1: lerp the base colour towards (0.5, 0.56, 0.62); never emissive).

- The frame is the instance's: everything (PositionCm, normals, wiper data) in centimetres in that frame. A mesh with a scaled instance gives scaled drops: pass world position relative to the object instead.
- Walls that have an inside: Outer must be 0 on the inner pane (a double pane has two surfaces). With a one-surface pane seen from both sides, Outer is 1.
- Fixed glass under a roof: one `UGlassRain::Roofed` ray at start is enough, no repeat.
- It is off in one branch when GlassFill is 0. Cost with everything on is about 2,100 instructions (upper bound over all branches): use it on real windows only.

## Vehicles

`FGlassRainVehicle` (in `FVehicleDirtState` and the fire truck) finds each model's M_TruckGlass panes, tells them where outside is (RainC), one ray up every 3 to 5 s for the roof, and the airflow. `FGlassWipers` takes the blades: cleaning van, refueller, catering truck, water truck, toilet truck, passenger stairs (open and roofed), de-icer cab, de-icer spray cabin, apron bus, taxi bus, fire truck.
