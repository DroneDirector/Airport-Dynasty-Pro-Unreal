// Staff (docs/AI-Simulation-Plan.md part 6), our own model: people made with names from the airline's part of the world,
// the going wage for each job and region, the hiring pool that changes every few hours, the crews a flight needs, and
// each person's days - sickness, morale (pay above all), and quitting after too long unhappy. Your airline and the AI
// airlines' staff alike. (Training, unions and strikes, conduct and court come in the next steps of part 6.)
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"

namespace ADPStaff
{
	/** The going weekly wage (USD) for a job in a country (ISO code). */
	double GoingRate(const FString& Role, const FString& Country);
	/** A new person for a job (not yet hired: AirlineId empty). */
	FADPPerson MakePerson(const FString& Role, const FString& Country, FRandomStream& Rng);
	/** Hires someone from the pool for an airline at a base: the first week's wage as the hiring fee. */
	bool Hire(FADPWorld& W, const FString& PersonId, const FString& AirlineId, const FString& Base, const FDateTime& At);
	/** Cabin crew a type needs (1 per 50 seats from 20 seats), pilots 2. */
	int32 CabinCrewFor(int32 Seats);
	/** On-duty cabin crew of an airline at a base (not sick, not on leave). */
	int32 CabinCrewAt(const FADPWorld& W, const FString& AirlineId, const FString& Base);
	/** The airline's country (its hub airport's) - never fixed to one place (the airport can move). */
	FString CountryOf(const FADPWorld& W, const FString& AirlineId);
}

class AIRPORTDYNASTYPRO_API FADPStaffModule : public IADPSimModule
{
public:
	virtual FName Name() const override { return TEXT("Staff"); }
	virtual double Interval() const override { return 3600.0; }   // (every hour; the day's events at 00:00 UTC)
	virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(56.0); }
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) override;
	int32 SickDays = 0, Quits = 0, PoolChanges = 0;
	FRandomStream Rng = FRandomStream(2601);
};
