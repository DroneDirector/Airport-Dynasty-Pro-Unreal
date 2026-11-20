# The game's AI, rebuilt in Unreal - plan (to-dos 154, 155, 153, 156, 157)

Owner, 2026-11-20. Every AI and simulation feature of the Babylon version, learned from what it does and rebuilt from
scratch in Unreal (C++), never ported. No web pages left in the game: the Overview, the title screen, the new-game
steps, menus and Settings all become native Unreal screens. One simulation drives both the Overview's numbers and the
3D world, and the PC game and the Android app stay in sync. Built in parts of 2-3 working hours, each committed.

## What the Babylon version does today (found in its code)

- About 59,000 lines of game logic in the web game, running on the real clock (no game speed), each system with its own
  timer, catching up on what happened while the game was closed (flights 14 days, pay 8 weeks, bank 12 months).
- Hired pilots fly your routes on their own: departure checks (repairs, weather, ground crew, sickness, crew, stands,
  strikes, curfews), real flight profiles, delays, weather reroutes and diversions, failures, landings that book the
  money, the next leg, duty time and rest.
- 12 rival airlines and 3 helicopter operators with daily schedules on the map and in the league; rivals answer your new
  routes and low fares (fare war, more flights, pull-out), poach your staff, offer to invest; you can take one over.
  They never grow, buy aircraft or go bankrupt.
- Passenger demand per route (airport size, distance, season, special days), your share against the rivals by fare and
  quality, load factor, business/first, cargo, connections at your hubs, reviews and stars, moods, pets, medical
  diversions, drunk and unruly passengers.
- The economy: profit and loss on every leg (tickets, on-board sales, bags, cargo, fuel by airport, every fee,
  maintenance, crew, overhead, emissions), weekly pay, loans, insurance, leases both ways, used market, marketing,
  fuel and parts markets with news, stock, hedging, shares and IPO, scenarios, car rental at your airport, difficulty.
- Staff: generated people by nationality, hiring pool, levels 1-500, morale in 7 areas, requests, quitting, unions and
  strikes, sickness and cover, crew leasing, type ratings and 27 courses (Academy), conduct reports, discipline, the
  office interview, court cases, investigations, whistleblowers; your airport's own staff rota (not yet paid or saved).
- Life events, the jobs board, tours, weekly challenges, reputation, ranks, 1,000+ badges, 22 achievements, the league,
  news, notifications, the morning briefing, laptop mail, phone messages, gifts, voices for the NPCs.
- The Overview and 15 screens (fleet, routes, jobs, schedule, dispatch, planner, maintenance, finance, logbook, crew,
  ranking, career, news, co-op, map), car rental, hangars, lounges, Settings with 22 sections.
- Save in the browser's storage; cloud sync PC - Android (encrypted, one device runs the world, the other shows it) -
  not switched on; the Flight Simulator link; co-op (demo only).
- Unfinished there: the simulated calendar, rivals that grow, some state lost on restart (route reactions, league,
  airport staff), airport staff unpaid, calls partly, co-op online.

## How it is built in Unreal

1. **One simulation core** (C++, runs whether you are in the Overview or walking the terminal): the clock with
   catch-up, flights, rivals, demand, economy, staff, events - each a module with its own update rate. Nothing in it
   draws anything; screens and the 3D world only read it and send it your decisions.
2. **The 3D world reads the same simulation** (to-do 155): the coaches and cars bring the passengers the simulation
   books, they check in, go through security, walk Floor 2, sit, shop, board the flights the simulation runs; conduct
   cases become the security/police scenes already planned; staff follow their rota. Only people near you are full
   MetaHumans - the rest are records (as the staff plan says), so thousands cost almost nothing.
3. **One data model, saved and synced**: the save as files in the game's own folder (with slots and autosave); the
   sync with the Android app through the same encrypted cloud channel, with the live positions (coaches, every
   passenger as a dot, conduct cases as red dots, boarding) sent a few times a minute for the tracking map. Places for
   what the 3D world does not have yet (cabins) are in the model now, marked unfinished.
4. **Native screens** (to-dos 153, 156, 157): the Overview and every screen remade in Unreal - very nice, dynamic and
   responsive; the title screen with its graphics, animations and the neon sign exactly as today; the title music with
   random soundtracks and the bottom-centre mute.

## The parts, in order (working hours)

1. The core: clock, catch-up, module timers, the data model, save files and slots - 6-8 h
2. Aircraft, flight profiles, the flight lifecycle and positions - 6-8 h
3. Hired-pilot flying: departure checks, delays, weather, diversions, failures, landing, next leg, duty/rest - 8-10 h
4. Demand, fares, load, classes, cargo, connections, services, loyalty, lounges, alliances - 6-8 h
5. The economy: leg P&L, fuel and markets, payroll, loans, insurance, leases, sales, shares, scenarios - 8-10 h
6. Staff: people, hiring pool, levels, morale, unions/strikes, sickness, training, conduct, court - 10-12 h
7. Rival airlines as real AI airlines: their own economy, buying/selling aircraft, opening and closing routes, hiring, reactions, poaching, takeover, bankruptcy, league - 10-14 h
8. Events, jobs, reputation, ranks/badges/achievements, news, notifications, mail, briefing - 6-8 h
9. Your airport live (155): passengers from the coaches to the aircraft, staff rota in 3D, conduct scenes - 10-14 h
10. Sync with the Android app + the live tracking data - 6-8 h
11. The new Overview: design previews, then the clickable demo APK for your tablet - STOP for your OK - 6-8 h
12. The Overview and all screens built for real, Settings, menus - 20-30 h
13. Title screen, new-game steps, neon sign, title music (156/157) - 10-14 h
14. Babylon web pages switched off; a full stress walk and test - 4-6 h

15. Our own flight planning and dispatch from real-world data (no outside services) - 10-14 h

About 125-170 working hours in all (16-21 working days). Each part is tested and committed; a release whenever a
useful group is done (your go-ahead covers publishing, except the Overview's look, which waits for the demo APK).

## Questions for the owner (before part 1)

1. ANSWERED (owner, 2026-11-20): rival airlines become real AI airlines - they buy (and sell) aircraft, open routes,
   hire and can go bankrupt, each running its own economy. Your own airline is yours alone: you choose its aircraft,
   routes, economy and staff - the AI never decides for it.
2. Time: keep the real clock (as now, with the catch-up), or add a game speed?
3. The Android app: it keeps running its own web game for now - must the PC's new save stay readable by it (so the two
   still sync), or is the Android app rebuilt from the new design later too?
4. ANSWERED (owner, 2026-11-20): no links to outside services at all - no SimBrief, VATSIM/IVAO, Discord, ACARS or the
   Flight Simulator link. Their useful features are built into our own system instead: with our own aircraft you plan
   routes professionally - departure and arrival procedures, waypoints, airways, levels, an operational flight plan and
   ACARS-style messages - from real-world navigation data (sources checked for a licence that allows it in a sold game
   before use). Added as part 15: our own flight planning (10-14 h).
