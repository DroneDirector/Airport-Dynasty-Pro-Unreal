// Maintenance in the simulation (to-do 181; owner, 2026-11-20 night), on real airline practice:
//  - checks by flight hours, cycles and calendar, whichever comes first (A320 family maintenance planning, simplified):
//    A-check every 750 FH / 750 FC / 120 days (about 10 h, overnight, 2 mechanics), C-check every 24 months or 7,500 FH /
//    5,000 FC (14 days in a hangar, 8 mechanics), D-check (heavy) every 6 years (42 days in a hangar, 12 mechanics);
//    other classes scaled (turboprops and bush planes shorter intervals, widebodies longer checks);
//  - a licensed mechanic at every base where the aircraft is maintained; fewer mechanics than the check needs: it takes
//    longer in proportion; no mechanic: it can't start (an outside team is flown in - 3x the labour);
//  - parts from the airline's parts stock (bought at the parts market's price); short of parts: bought on the spot as
//    "AOG" at 1.3x;
//  - a check falling due starts at the base when the aircraft gets there; 10% overdue: the aircraft is grounded until
//    checked (the authority's rule); each check brings its condition back (A +8, C to 96, D to 100) - and a badly kept
//    aircraft fails more often (the flights module's failure chance follows the condition and overdue checks);
//  - checks in the airport's own hangars (C and D need a free hangar bay; the site's hangars from airfield.json).
// Your airline in full; the AI airlines get the same checks without the mechanic and stock bookkeeping.
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"

namespace ADPMaint
{
	struct FCheckRule { const TCHAR* Kind; double Hours, Cycles, Days, DurationH; int32 Mechanics; double PartsUsd; bool bHangar; };
	/** The checks for a type (A, C, D) - intervals and work. */
	TArray<FCheckRule> Rules(const FString& TypeId);
	/** The next check due for an aircraft and how far (0..1+) through its interval it is. */
	FString NextDue(const FADPWorld& W, const FADPAircraft& A, const FDateTime& Now, double& Fraction);
	/** Starts a check now (the Overview, or by itself when due). False: not at a base with mechanics, or no hangar bay. */
	bool StartCheck(FADPWorld& W, FADPAircraft& A, const FString& Kind, const FDateTime& Now, FString& Why);
	/** Buys parts for the stock (USD of parts at today's parts market price). */
	bool BuyParts(FADPWorld& W, const FString& AirlineId, double Usd, const FDateTime& Now);
	/** Hangar bays at an airport (our site: its hangars; elsewhere 1). */
	int32 HangarBays(const FString& Icao);
}

class AIRPORTDYNASTYPRO_API FADPMaintModule : public IADPSimModule
{
public:
	virtual FName Name() const override { return TEXT("Maintenance"); }
	virtual double Interval() const override { return 1800.0; }   // (every half hour)
	virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(30.0); }
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) override;
	int32 Checks = 0, Groundings = 0, AogBuys = 0;
};
