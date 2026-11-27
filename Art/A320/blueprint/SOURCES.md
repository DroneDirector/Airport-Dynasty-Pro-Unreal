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

## Notes
- Older Airbus AC URLs (`AC_A320_0624.pdf`, `ac_a320_1223.pdf`) now return 404; use the mediaassets link above.
- SKYbrary file 2372.pdf turned out to be the A340 AC and was discarded.
- Plan drawing span scales to 35.46 m vs the stated 35.80 m (about 1 % short); side drawing height is 11.78 m vs 11.98 m at MRW, because the drawing is not at a stated weight. Lengths were scaled to 37.57 m.
- Cockpit interior: Airbus publishes no dimensions; everything there except the pilot eye position is estimated.
