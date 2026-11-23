// The timetable (owner, 2026-11-23: "passengers and crew work through all their tasks in a real new career"; to-do 211: one
// timeline per flight). You make lines in it - an aircraft, a place, a time, the days - and each line becomes planned legs
// three days ahead: on sale, booked by the demand, checked in, boarded, gone (then an FADPFlight with the same id). The
// other airlines' flights here get their planned leg as soon as the aircraft is on its way, so their passengers come in
// time too. Everything that waits for a flight reads its planned leg: the boards, the passengers (PaxFlow), the turnaround,
// the tugs and the baggage. No planned leg: no passengers.
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"

namespace ADPSchedule
{
	/** A new line: Reg flies from its base to To and back, off-blocks at DepMin (minutes of the base's local day) on Days. */
	bool AddSlot(FADPWorld& W, const FString& Reg, const FString& To, int32 DepMin, int32 Days, const FDateTime& Now, FString& Why);
	/** Takes a line out: its legs not yet under way are cancelled. */
	bool RemoveSlot(FADPWorld& W, const FString& Id, const FDateTime& Now);
	bool HasSlots(const FADPWorld& W, const FString& Reg);
	/** May this aircraft's type fly there from its base (range, runway)? */
	bool CanReach(const FString& TypeId, const FString& From, const FString& To, FString& Why);
	/** The block time (minutes), kept once worked out; 0: no way there. */
	double Block(const FString& TypeId, const FString& From, const FString& To);
	/** A Schengen country (ISO code)? (no passport control between two of them) */
	bool Schengen(const FString& Country);

	/** The aircraft's next planned leg from where it is (not gone, not cancelled), or null. */
	FADPPlanned* Next(FADPWorld& W, const FString& Reg, const FString& From);
	const FADPPlanned* Next(const FADPWorld& W, const FString& Reg, const FString& From);
	FADPPlanned* ById(FADPWorld& W, const FString& Id);
	/** Cancels it (and its way back): the notice, the passengers sent home. */
	void Cancel(FADPWorld& W, FADPPlanned& P, const FDateTime& Now, const FString& Why);

	/** Plans, bookings, expected times, stands set aside - called by the flights' step (so they never drift apart). */
	void Step(FADPWorld& W, const FDateTime& Now);

	/** Your people of a job on shift at your airport now. */
	int32 OnShift(const FADPWorld& W, const TCHAR* Role, const FDateTime& Now);
	/** The ground steps of your flights that have nobody on shift ("" = everyone needed is there). */
	FString MissingStaff(const FADPWorld& W, const FDateTime& Now);
	/** Tells the player (a notice and the log), once per key and half hour. */
	void Tell(FADPWorld& W, const FDateTime& Now, const FString& Key, const FString& Title, const FString& Text);

	/** Set by the 3D airport while it runs: may this flight leave (everyone aboard, the bags in, the jetway back)? False:
	 *  what it waits for. Not asked for while the career catches up. */
	extern TFunction<bool(const FADPPlanned& P, const FDateTime& Now, FString& Why)> Ready3D;
	/** Set by the 3D airport's hired crew (to-do 206): the jobs that wait for people now ("" = none), for the missing-staff text. */
	extern TFunction<FString()> Waiting3D;
}
