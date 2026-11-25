// The airport's real timetable (to-do 174; owner, 2026-11-20 night): the departures and arrivals at the airport you are at,
// read from the simulation - every airline's, yours and the AI airlines' - in local time, with flight numbers, places,
// delays, gates and the status a real board shows (Scheduled, Check-in, Gate open, Boarding, Final call, Gate closed,
// Departed / Expected, Delayed, Landed, Diverted, Cancelled). The terminal's boards, the gate screens, the boarding calls
// and the phone all read this one list, so they always agree.
#pragma once

#include "CoreMinimal.h"

class UADPSim;

struct FADPBoardRow
{
	FDateTime Sched;      // scheduled (local time at the airport)
	FDateTime Expected;   // expected / actual (local)
	FString Flight;       // "CHB 214"
	FString Place;        // the other airport's town
	FString PlaceCode;    // its IATA (or ICAO)
	FString Status;
	FColor Col = FColor(127, 196, 245);
	int32 Gate = 0;       // 1..16 (0: none yet)
	FString Reg, AirlineId, PlanId;   // (PlanId: its planned leg, or the flight made from it - Sim/ADPSchedule.h)
	bool bBoarding = false, bFinalCall = false;
	FString Time() const { return FString::Printf(TEXT("%02d:%02d"), Sched.GetHour(), Sched.GetMinute()); }
};

namespace ADPBoards
{
	/** The airport these boards are at (the site's ICAO). */
	FString HereIcao();
	/** True when a career is running in the simulation (else the boards show nothing real). */
	bool Live(const UObject* Ctx);
	/** The next departures (bArr false) or arrivals, oldest still shown first; at most N. */
	TArray<FADPBoardRow> Rows(const UObject* Ctx, bool bArr, int32 N);
	/** A page of a timetable stays this long before the next one comes (seconds; real airports' boards: 10 to 15). */
	constexpr double PageSeconds = 12.0;
	/** The turns so far: one every PageSeconds, the same for every screen (so all screens of a kind turn together). */
	int32 Turn();
	/** The page of a list of Count rows, Per on a page, shown now (0...), and how many pages there are (1 at least). */
	int32 Page(int32 Count, int32 Per, int32& Pages);
	/** The rows the terminal's boards show now: times and statuses as they change; flights come and go at a turn (at once
	 *  while the list is short). Empty without a career: there is no sample timetable (to-do 286). */
	const TArray<FADPBoardRow>& Shown(const UObject* Ctx, bool bArr);
	/** The flight at a gate now (the next departure there), if any. */
	bool AtGate(const UObject* Ctx, int32 Gate, FADPBoardRow& Out);
	/** The gate a departure uses (until stands and gates are planned - to-do 183 - each aircraft keeps one by its registration). */
	int32 GateFor(const FString& Reg);
	/** A flight number for an airline's route ("CHB 214"). */
	FString FlightNumber(const FString& AirlineCode, const FString& RouteId, int32 LegIdx);
	/** Local time at the site for a UTC time. */
	FDateTime Local(const FDateTime& Utc);
}
