# Terminal changes v2.1: summary (approved)

*Approved by the owner on 23 November 2026, being built. Only sheet 2f (car park exit) still waits for your OK.*

## What is new

- **East wing, two floors.** Ground floor: East Hall, customs, non-Schengen claim hall, passport control in, staff rooms, coach station. Floor 2: the non-Schengen concourse (323 x 32 m: duty free, food and bar, lounge, seating, prayer room, three toilet blocks, transfer centre).
- **Passport control out** beside the walkway, with a glass wall at x 240. **Swing gates 4 and 13** share a swing lobby between them.
- **Arrivals from outside Schengen go down at their own gate** into a closed corridor under the walkway.
- **Arrivals duty free** in both claim halls (323 and 264 m²). Claim-hall toilets, lost and found, oversize belts, trolleys, ATM and exchange, SIM kiosks, info desks, two taxi ranks, 10 pay machines, 5 more coach bays, double-skin front at x 114-156.
- **Grand Hall:** 2 160 m², exactly as today. **Extension past x 551: not needed** (sheet 6), so no new gates.

## What moves (and what changed from the accepted sheets)

- No gate, stand, jetway or room is removed. Existing walls get new doorways: at z 52 and z 68, and in the inner walls of gates 4 and 13-16.
- **Arrivals corridor and mezzanine of TC-3: replaced.** They would have closed the concourse off from the walkway and stopped departures at each arrival.
- **Passport booths: from inside the walkway to beside it.** The gap between two moving walkways is only 10 m.
- **East claim belts are fed from x 286-314.** The accepted feed at x 256 crossed the departure bag trains' line.
- **Café in the west arrivals hall made small.** At first it made the hall too small (94 % of the need).

## Discrepancies found (the data against the game)

1. **The east wing is open grass** under the wave roof: no floor, glass or walls (terminal-plan.json and TC-2/TC-4 say "empty wing", "closed wall, flat roof at 12 m"). The **roof and eight tree pillars are already there**. The roof covers z -16 to 34 only; z 34-52 is open sky. No Floor 2 floor, room or obstruction exists above x 156-551.
2. **Floor 2 toilets** really are at x 0-14, z 0-10 (plan: x 90-104). The **bar** is at x 72-114 (plan: 104-114), **tax free and landing** x 56-72 (plan: to 90). Two bar doors listed at x 118 and 132 don't exist.
3. **Gates 13-16: the jetways start at the VIEW BAYS** (x 309/314/321, 389/399, 461/471, 534/544). The bays' glass has **no doors there**, and the old doors at x 277, 357, 429, 501 lead to no jetway. The stairs and lifts of gates 13-16 exist but are not in the plan.
4. **Staff door at x 156, z 46:** TC-2 put toilets in front of it; it keeps a 4 m lane now. The offices' **east windows** (x 156) will look into the East Hall.
5. **Forecourt:** pavement and kerb lane end at about x 241-340. The **car park has no vehicle exit** in the data. The seven coach stands are at x 17-148 (free kerb: x -80 to 17 and 154 to 241).
6. **Roof copies meet in small points** (kinks) at x 126, 233, 341 and 448. They exist today.
7. **Bag trains checked** (GSE.cpp, traced walls): roller door x 127.9-135.7, line z 57 to x 265, Ramp services 13 at x 269. Everything new stays clear. Gate 4's arrivals gallery crosses it 5.6 m up.

## The real rules (with sources)

- **Schengen.** Passengers on internal flights are physically separated from all others; transfers are checked in at the first Schengen airport and out at the last (Schengen Borders Code 2016/399, Annex VI 2.1: legislation.gov.uk/eur/2016/399). **EES in force:** fully since 10 April 2026, Geneva and Zurich included (home-affairs.ec.europa.eu, swissinfo.ch): kiosks, and long first registrations.
- **Non-Schengen flights from Chablais:** UK, **Ireland**, **Cyprus** (expected 2027), Turkey, Balkans outside the EU, North Africa, Gulf, Israel, Americas, Asia. **Schengen:** 29 countries, including **Bulgaria and Romania** (air borders gone 31 March 2024) (home-affairs.ec.europa.eu, Schengen area).
- **Security.** Screened departing passengers must not mix with arrivals, except from the EU / EEA / Switzerland or recognised countries; transfers from those need no new screening (Regulation 300/2008, 4.1.2 and 4.2). Recognised today: UK, USA, Canada, Singapore (Changi), Serbia (Belgrade), Montenegro, Vágar, Svalbard (2015/1998, attachment 4-B, version of 1 October 2026; Israel, Jersey, Guernsey, Isle of Man removed in 2025). Both apply in Switzerland (FOCA list, 1 July 2026).
- **Swing gate:** no law names it; it must satisfy the two rules above: one side open at a time, swept, locked, logged, and searched if unscreened arrivals used it (2015/1998, 1.1.3.4).
- **Arrivals duty free** is allowed at Swiss airports since June 2011 (Zurich, Geneva, Basel, Bern, Lugano named: swissinfo.ch; gva.ch). The airport needs a federal permit and a permanently staffed customs office (Customs Act, art. 17, read in its 2007 text).
- **Customs:** red / green for EVERY flight, CHF 150 limit (bazg.admin.ch).
- **PRM:** marked points inside and outside, help to connections (1107/2006, art. 5 and 7, Annex I; in force in Switzerland, FOCA).
- **Fire:** 35 m to an exit stair (50 m in tall rooms if the fire authority agrees), two stairs above 900 m², 0.6 m of stair per 60 people (VKF 16-15, 2.4). Correction to v2: the 11 stairs leave up to 35.3 m in a straight line, more round walls; sheet 9 settles it.
- **You did not ask for, but the rules need:** EES kiosks, second-line rooms, border police and customs offices, staff security, transfer security, escape stairs, PRM points.

## Your decisions, drawn in v2.1

1. **Arrivals go down at the gate.** Yes.
2. **Gate 4's glass arrivals gallery** is kept: gates 4 and 13 both swing. (Open: your curved gate windows would touch it.)
3. **Gates 13-16:** 1.9 m doors cut into the view-bay glass at the real jetways; the four dead doors are glazed.
4. **Seven security lanes** in the same room (sheet 2g): 5.78 m apart. 116 % of the need at 280 an hour each, 100 % at 240.
5. **Stairs of gates 4 and 13 turned** along the walkway wall (2.8 x 11 m, foot west). The way past is 7.8 m (gate 4) and 3.1 m (gate 13), instead of 1.4 m.
6. **Two taxi ranks.** 7. **The wave roof is one smooth wave** (x -246 to 576, crests every 30 m, one more tree pillar at x 556).

## For your OK

- **Sheet 2f, the car park exit:** at the back, onto the road to the town, at x 120; 2 entry and 2 exit lanes. Option 1: give-way junction (my pick). Option 2: roundabout. It costs 224 of 3 982 spaces.
- **Found:** gate 4's stair and lift lead down to gate 8 (Schengen). They lock while gate 4 is swung.
- **Found:** at Chablais the forecourt road starts at a dead end (x 1856), so no car can reach the one-way lanes. A link is needed for the traffic job.
