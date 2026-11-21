// Ground handling, stands and gates (to-do 182 / 183; owner, 2026-11-20 night).
//  182 - every turnaround at your own airport is worked by your ground teams: fuel (a fuel truck and its driver), catering
//    (a truck and two loaders), cleaning (two to four cleaners) and baggage (four handlers with a tractor); each task's time
//    from the people and vehicles actually free; a team busy with another aircraft, a missing truck or too few people on
//    shift makes the turnaround longer - and the departure late, as in real life. Shifts: about 40% of a team is on duty at
//    any hour (three 8-hour shifts plus days off), half that at night. Away from your airport a handling agent does it for a
//    fee per turnaround (the usual 35-45 minutes for an A320).
//  183 - at your airport each aircraft gets a stand and gate by its size (ICAO code C: A320 family, E: 777 / 787, F: A380)
//    and time: a contact stand with a jetway for passenger flights when one is free, else a remote stand with a bus; cargo
//    on the cargo apron; an arrival is given its stand 30 minutes before landing, a late one moved to the next free stand;
//    the stand is let go when the aircraft pushes back. The boards and gate screens show these gates (to-do 174).
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"

namespace ADPGround
{
	/** The turnaround (minutes) for an aircraft just landed at At - from the teams free at that airport (your airport:
	 *  your people and vehicles; elsewhere: a handling agent, fee booked). Occupies the teams it uses. */
	double Turnaround(FADPWorld& W, FADPAircraft& A, const FDateTime& At);
	/** ICAO aerodrome reference code letter for a type (C, D, E, F). */
	TCHAR CodeOf(const FString& TypeId);
	/** Gives the aircraft a stand at our airport (when parked there, or arriving within 30 minutes); "" if none (remote). */
	FString AssignStand(FADPWorld& W, FADPAircraft& A, const FDateTime& Now, const FDateTime& Until);
	/** Lets go of the aircraft's stand (it pushed back). */
	void FreeStand(FADPAircraft& A);
	/** The gate number of a stand (STAND-n -> n; remote or cargo: 0). */
	int32 GateOfStand(const FString& Stand);
	/** The teams one turnaround needs (people per job) - for the Overview's "what you need". */
	void TeamNeeds(TMap<FString, int32>& Out);
	/** Buys a ground vehicle for your airport (fuel, catering, tractor): USD price; false if short of money. */
	bool BuyVehicle(FADPWorld& W, const FString& AirlineId, const FString& Kind, const FDateTime& At);
}

/** Every 5 minutes: stands for aircraft parked at our airport and for the arrivals coming in. */
class AIRPORTDYNASTYPRO_API FADPStandsModule : public IADPSimModule
{
public:
	virtual FName Name() const override { return TEXT("Stands"); }
	virtual double Interval() const override { return 30.0; }   // (every half minute: a new or landed aircraft gets its stand at once)
	virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(2.0); }
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) override;
	int32 Assigned = 0, Remote = 0, Moved = 0;
};
