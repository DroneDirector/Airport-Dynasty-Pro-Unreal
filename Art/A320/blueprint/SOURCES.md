# A320neo blueprint data: sources

Data file: `a320neo-data.json`. Each group has `src` codes (below) and `q` = measured / estimated.
Local copies (not committed) are in `F:/ADP-Refs/A320neo/`.

| Code | Source | What it gave |
|---|---|---|
| S1 | Airbus, *A320 Aircraft Characteristics - Airport and Maintenance Planning*, Rev Jul 01/26 (435 pages). Index page: https://aircraft.airbus.com/en/customer-care/fleet-wide-care/airport-operations-and-aircraft-characteristics/aircraft-characteristics. PDF: https://mediaassets.airbus.com/pm_38_916_916266-iujedqawwy.pdf?fileName=aca32001-jul-2026-2.pdf. Local: `F:/ADP-Refs/A320neo/aca320-jul-2026.pdf`, text `ac.txt`, page renders `pages/`. | Main source for nearly all numbers. p8-9 general dimensions; p10-19 cabin layouts and cross-sections; p48-49 fig 2-2-0-991-009 sheets 1-2 (neo side and plan views, used for the vectorised silhouettes and all detail coordinates); p52 front view; p57 ground clearances fig 2-3-0-991-032 (neo); p60-67 flap, flap track, spoiler, aileron and slat heights; p62-74 door clearances and door sizes (fig 2-7-0); p76-91 and p101-114 landing gear, tyres, gear doors; p123-141 LEAP-1A and PW1100G nacelle dimensions; pilot visibility figure (eye position, view angles); lights fig 2-10-0-991-009; p198, p419, p428-431 doors, radome, APU and service access. |
| S2 | Wikipedia, Airbus A320neo family: https://en.wikipedia.org/wiki/Airbus_A320neo_family and Airbus A320 family: https://en.wikipedia.org/wiki/Airbus_A320_family | Height 11.76, cabin width 3.70, fan diameters (LEAP-1A 78 in, PW1100G 81 in), sharklet 2.5 m, seat counts 165 / 195, wing area, aspect ratio, sweep 25 deg. |
| S3 | HAW Hamburg (Prof. Scholz) A320 wing / aircraft design PDFs, https://www.fzt.haw-hamburg.de/pers/Scholz/ | Wing area and planform cross-checks. |
| S4 | arXiv 2512.14397 (SuperWing dataset) | Tip dihedral 4.4 deg, kink at 39.2 % semi-span, aspect ratio 8.79 (reference-area definition differs). |
| S5 | Smithsonian Air & Space wing explainer; Wikipedia "Comparison of airliners" table | Root t/c about 15.2 %; taper ratio 0.240. |
| S6 | MobiFlight open-source panels, https://github.com/MobiFlight/mobiflight-panels (aircraft/airbus-a32x). Local: `F:/ADP-Refs/A320neo/mobiflight/fcu.json`, `efisL.json` | FCU and EFIS control-panel PCB outlines (sizes estimated, EasyEDA unit assumed 10 mil). |
| S7 | Slideshare A320 avionics presentation (STL 945.7136/97) | Display unit size 7.25 x 7.25 in. |
| S8 | MSFS forum threads https://forums.flightsimulator.com/t/a320-neo-panels-measurements/457319 and thread 660250; simflight "A320 and B767 cockpit dimensions" https://www.simflight.com/2012/04/09/a320-and-b767-cockpit-dimensions/; AVSIM threads 133956 (DZUS strips) and 134156 (glareshield) | Display about 165 mm square (forum); DZUS panel width 5.75 in / 146 mm; general cockpit proportions. No exact Airbus cockpit drawings found. |
| S9 | Rowsfire A320 overhead panel product page | Overhead replica outer size 405 x 270 x 60 mm (scaled replica, used only for proportions). |
| S10 | MakerWorld model 3064717 (A320 OPU printer panel) | Aft pedestal panel 146 x 175.5 x 6 mm (hobby model, confirms 146 mm DZUS width). |
| S11 | Airbus A320 SRM 51-00-00 fig 6 sheet 2 (fuselage station information), hosted at data.ntsb.gov Docket Document ID 40183902 | Section ends with Airbus X stations in cm: FR1 350, FR24 950, FR35 1537, FR47 2136, FR64 3043, FR70 3366, FR77 3655, FR87 4011. |
| S12 | Airbus AMM 06-31-53 figs 005A and 006A (same NTSB document) | Frame-by-frame stations FR64-FR87 in mm. Those pages are A319 effectivity; the aft sections are the same length, so they are fitted onto the measured A320 anchors FR64 and FR77 (shift 3739 mm, residual under 5 mm). |
| S13 | aviationhunt.com "Airbus A320 ATA 06 - dimensions and areas" | Airbus X datum 2540 mm forward of the nose tip; 87 fuselage frames FR1-FR87; 27 ribs per wing; 11 fin ribs; 14 HTP ribs per side. |

## Notes
- Older Airbus AC URLs (`AC_A320_0624.pdf`, `ac_a320_1223.pdf`) now return 404; use the mediaassets link above.
- SKYbrary file 2372.pdf turned out to be the A340 AC and was discarded.
- Plan drawing span scales to 35.46 m vs the stated 35.80 m (about 1 % short); side drawing height is 11.78 m vs 11.98 m at MRW, because the drawing is not at a stated weight. Lengths were scaled to 37.57 m.
- Cockpit interior: Airbus publishes no dimensions; everything there except the pilot eye position is estimated.
- Frame stations (sheets 9-12): the datum offset used is 2.5425 m, taken from the data file itself (MLG axle Airbus X 20.2525 m = 17.71 m from the nose); it reproduces the overall length exactly (FR87 -> x 37.5675 m) and puts FR47 at 18.82 m against the 18.89 m wing root trailing edge and FR35 at 12.83 m against the 12.88 m root leading edge.
- Measured frames on sheets 9-12: FR1, FR24, FR35, FR47 (S11) and FR64-FR87 (S11 + S12). Everything between them (FR2-FR23 at 261 mm, FR25-FR34 at 534 mm, FR36-FR46 at 499 mm, FR48-FR63 at 534 mm) is interpolated at equal pitch and drawn in grey as estimated.
- Measured frame identities (Airbus AC, S1): NLG bay FR9-FR20, fwd pax door FR16/FR20, fwd hold FR24A-FR34, aft hold FR47-FR59, bulk hold FR59-FR65, overwing slide FR47, APU doors FR80-FR84, fwd jack FR8, shoring FR15/FR33/FR42/FR63/FR64, tail safety stay FR73/FR74.
- Estimated on sheets 9-12: all stringer positions and count (about 36 a section, no source found), seat track y, floor beam depth and strut angle, keel beam size, centre wing box frames (FR37-FR47), rear pressure bulkhead frame (FR70), fin box frames (FR74/FR80), THS pivot frame (FR77), MLG bay frames (FR45-FR50), spar chord fractions (15 % / 60 %), slat track positions and all rib y positions.
- Wing rib y: 27 ribs evenly spaced from y 1.98 to y 16.29 reproduces every rib named in the Airbus AC (RIB9 jack pad, RIB14/15 refuel at the front spar, RIB19/20 overwing gravity cap, RIB22-25 vent/surge, RIB4/13/22 shoring) to within half a bay, so the count is measured and the positions estimated.
