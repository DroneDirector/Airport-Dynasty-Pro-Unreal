# Terminal changes v2: summary

*23 November 2026. The sheets are TCv2-1 to TCv2-7. Drawings only: nothing is built, and everything waits for your OK.*

## What is new

- **East wing, two floors.** Ground floor: East Hall, customs, non-Schengen claim hall, passport control in, staff rooms, coach station. Floor 2: the non-Schengen concourse (323 x 32 m: duty free, food and bar, lounge, seating, prayer room, three toilet blocks, transfer centre).
- **Passport control out** beside the walkway, with a glass wall at x 240. **Swing gates 4 and 13** share a swing lobby between them.
- **Arrivals from outside Schengen go down at their own gate** into a closed corridor under the walkway.
- **Arrivals duty free** in both claim halls (323 and 264 m²). Claim-hall toilets, lost and found, oversize belts, trolleys, ATM and exchange, SIM kiosks, info desks, two taxi ranks, 10 pay machines, 5 more coach bays, double-skin front at x 114-156.
- **Grand Hall:** 2 160 m², exactly as today. **Extension past x 551: not needed** (sheet 6), so no new gates.

## What moves (and what changed from the accepted sheets)

- No gate, stand, jetway, room or wall is removed. Existing walls get new doorways: at z 52 and z 68, and in the inner walls of gates 4 and 13-16.
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
- **Fire:** 35 m to an exit stair (50 m in tall rooms if the fire authority agrees), two stairs above 900 m², 0.6 m of stair per 60 people (VKF 16-15, 2.4). Hence 11 stairs.
- **You did not ask for, but the rules need:** EES kiosks, second-line rooms, border police and customs offices, staff security, transfer security, escape stairs, PRM points.

## Questions for you

1. Arrivals going down at the gate instead of the Floor 2 corridor: OK?
2. Gate 4's glass arrivals gallery outside the windows (x 195-303): OK, or only gate 13 swings?
3. Doors in the view-bay glass for the gate 13-16 jetways, and the dead doors removed?
4. Security has no reserve (1 680 of 1 686 an hour): a 7th lane?
5. Past the stairs of gates 4 and 13 the way is 1.4 m wide: fine, or turn the stairs?
6. Car park exit with barriers: where? Two taxi ranks or only the west one?
7. Roof kinks: smooth them, or leave them?
