// Every job you can hire people for, by area of the airport (to-do 206 part 3): the one table the simulation (the hiring
// pool), the 3D airport (StaffPeople: which jobs are people in 3D) and the Overview's Crew screen (sent over the bridge)
// all read. A new job is one line in ADPCrew.cpp; its wage is ADPStaff's (no line there: the general wage).
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"

namespace ADPCrew
{
	struct FArea { const TCHAR* Id; const TCHAR* Label; const TCHAR* Duties; };
	struct FRole
	{
		const TCHAR* Id; const TCHAR* Name; const TCHAR* Area;
		bool b3D;      // their job is done by them in the 3D airport (StaffPeople)
		bool bSim;     // the simulation counts them (turn-rounds, check-in, the fire category, the flight crews)
		bool bShift;   // 8 hour shifts (early, day, late, night); the others: flight rosters, or the fire service's 24 hour watches
		bool bTalk;    // can be offered to somebody you talk to (no licence needed)
	};
	const TArray<FArea>& Areas();
	const TArray<FRole>& Roles();
	const FRole* Role(const FString& Id);
	FString RoleName(const FString& Id);
	/** A job at this airport (not a flight crew's or a mechanic's, who belong to their aircraft's base). */
	bool AtAirport(const FString& Id);

	struct FShift { const TCHAR* Name; const TCHAR* Label; int32 From, To; float Weight; };   // (local time)
	const TArray<FShift>& Shifts();
	const FShift* Shift(const FString& Name);
	bool InShift(const FShift& S, float Hour);
	/** Gives somebody without a shift the one with the fewest people for its share (two early, two late, one night). */
	void GiveShift(FADPWorld& W, FADPPerson& P);
	bool SetShift(FADPWorld& W, const FString& PersonId, const FString& Shift, FString& Why);

	/** How many of a job you need here by the game's own rules (0: no rule, or no fleet yet). */
	int32 Need(const FADPWorld& W, const FString& RoleId);
	/** On your staff here (given notice: no longer). */
	int32 Have(const FADPWorld& W, const FString& RoleId, const FString& Icao);

	/** Somebody you asked in the airport joins your staff (their name, age and face): starts at the next local midnight. */
	bool HireTalk(FADPWorld& W, const FString& RoleId, const FString& First, bool bWoman, int32 Age, const FString& Face, const FDateTime& At, FString& Name);
	/** Somebody with this face already works for you. */
	bool FaceWorks(const FADPWorld& W, const FString& Face);
}
