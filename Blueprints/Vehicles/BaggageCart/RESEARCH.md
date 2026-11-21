# Covered baggage cart - research notes (2026-11-21)

The covered baggage trailer ("baggage cart", "bag cart", "covered dolly") that bags ride on between the make-up hall and
the aircraft's belt loader, towed in trains of 4-6 by the baggage tractor. Built in Blender by
`Tools/make_baggage_cart.py` (shared helpers + checks: `Tools/vehicle_kit.py`).

## Real carts looked at (sizes)

| Cart | L x W x H (mm) | Deck / bed | Tow-bar / eye | Steering / brake | Wheels | Notes |
|---|---|---|---|---|---|---|
| SPS BW-40 (Schiphol) | 3977 x 1650 x 1938 | (platform height not given) | tow-bar 1100, eye 40 mm | turntable; parking brake on the front wheels, set when the tow-bar is upright | 4 x 4.00-8 (3.00 D rim) | 670 kg tare, 1500 kg load, hot-dip galvanised, cover: 2 sides canvas, SPS E-hitch (Ringfeder / Rockinger option), 25 km/h |
| SPS BW-53 | 4080 x 1510 x 1686 | - | tow-bar 850 | turntable | - | 582 kg tare, 1500 kg load |
| Handle-iT HI-ABT-006 (curtain trailer) | base 2500 x 1250 | - | - | turntable bearing head on the front axle, fixed rear axle | 400 mm wheels | 160 gsm tarpaulin curtains, 2000 kg |
| Maximo KDCBC covered cart (US) | deck 2850 x 1530 | bed 610 | - | - | - | covered, curtains both sides |
| Metaltech cabin-baggage trailer | bed 2303 x 1106, 1351 high | - | lift-to-lock tow-bar | 180 deg turntable | - | |
| Avro AV-CBC-001 covered cart | - | bed 584 (23 in) | - | turntable / fifth wheel, 360 deg | - | non-webbed curtains both sides |
| Fenixplus open cart | platform 2400 x 1400 | - | drawbar | parking brake applied when the drawbar is lifted | - | open (no roof) |

Sources: https://sps.aero/wp-content/uploads/2025/10/BW-40_DataSheet_SPS_GSE_08.pdf (read: sizes as above; the sheet's
labels are shifted by one line - 3977/1650/1938 are length/width/height), https://sps.aero/product/bw-53-baggage-cart/ ,
https://aeroexpo.online/prod/handle-it-ltd/product-180712-78129.html , https://directindustry.com/prod/handle-it-ltd/product-173707-2589981.html ,
https://www.environmental-expert.com/products/covered-baggage-cart-595669 (Maximo KDCBC), https://www.airport-suppliers.com/?p=217233 (Metaltech),
https://aeroexpo.online/prod/avro-gse/product-185988-32353.html (Avro), https://www.aeroexpo.online/prod/fenixplus/product-187999-78942.html

What all of them share (and the model follows): a galvanised steel chassis with a flat floor, four wheels, the front
axle on a turntable steered by the drawbar, a drawbar that springs up when let go and then sets the brake (dead-man),
a towing eye at the tractor's hitch height (~400 mm), a rear coupling (jaw + pin, or an E-hitch) at the same height so
carts chain, a roof on corner posts, PVC/canvas curtains both sides that open for loading, a closed front end and a
net or gate at the rear, reflectors.

## Reference photos (several angles; looked at for shape only - no brand, no livery, no photo used as a texture)

- Covered cart from above-rear-left, white steel end panel with pressed horizontal ribs, dark tarpaulin side, red round
  rear reflectors low on the frame, drawbar standing up at the next dolly, wheel chocks, galvanised frame -
  https://commons.wikimedia.org/wiki/File:United_Airlines_baggage_carts_at_Tokyo-Narita_International_Airport.jpg (CC0)
- Trains of covered carts (white frames, blue curtains, roofs) behind a tractor on the apron; open baggage carts being
  unloaded at a belt loader - https://commons.wikimedia.org/wiki/File:Wizzair_ground_operations.jpg (CC BY-SA 4.0)
- Line of carts on the apron (side view from a taxiing aircraft) - https://commons.wikimedia.org/wiki/File:Delta_baggage_carts_on_tarmac_LAX.jpg (CC BY-SA 3.0)
- Covered cart, close (curtain, frame) - https://commons.wikimedia.org/wiki/File:UnitedBaggageCart-Dulles_(34092392800).jpg (CC BY-SA 2.0)
- Carts at Suvarnabhumi - https://commons.wikimedia.org/wiki/File:Baggage_carts_at_airport.JPG (public domain)
- Stereo pair of baggage handling carts (tyres, frame) - https://commons.wikimedia.org/wiki/File:Flickr_-_jimf0390_-_JimF_05-22-10-0003a_baggage_handling_carts.jpg (CC BY 2.0)
- SPS BW-40 data-sheet picture (gabled roof, framed ends, galvanised chassis, drawbar, curtains) - maker's sheet above (look only)
- More: https://commons.wikimedia.org/wiki/Category:Airport_baggage_carts

## What the model is (Blender metres; x forward = drawbar end, y left, z up; origin on the ground under the middle)

Our own compact European covered cart (between the Handle-iT base and the SPS BW-40):
- Overall about 3400 long with the drawbar down (coupling jaw lip x -1.37 to towing eye x +2.06), about 1580 wide over the
  roof edge beads (deck 1400), 1972 high at the roof crown. Body 2300 long.
- Floor (10 mm chequer plate) 540 above the ground; load space 2200 x 1225 x 1240 (posts / bag rails to roof bows).
- Wheelbase 1600, track 1200, tyres 4.00-8 (415 x 105, real lathed profile, no modelled tread) on 8 in split rims.
- Front axle on a turntable (upper ring 600 dia), steers +-80 deg (checked clear); dead-man drawbar 710 from hinge to eye,
  40 mm eye at 400, yellow hand loop, cam that pulls the brake rod; tyre-tread brake shoes on the front tyres.
- Rear coupling jaw (flared lips, 60 mm gap) with a lift pin at 400 so the next cart's eye drops in (checked to fit).
- Six 60 x 60 posts (corners + middle), side top rails, crowned aluminium roof (60 mm crown) on end and middle bows.
- Front: white ribbed steel bulkhead with a blank ID plate. Rear: cargo net hooked to the posts and the rear channel.
- Sides: navy PVC curtains with soft folds, weighted hem, 4 tie-down straps with buckles to deck hooks, yellow pull strap;
  each rolls up onto a tube under the roof edge (CurtainRoll_*). Bag rails inside at 800 and 1200.
- Reflectors set into the channels: white front, amber sides (2 each), red rear.
