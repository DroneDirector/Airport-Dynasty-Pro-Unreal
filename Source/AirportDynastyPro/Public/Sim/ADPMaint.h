// Maintenance in the simulation (to-do 181; owner, 2026-11-20 night), on real airline practice:
//  - checks by flight hours, cycles and calendar, whichever comes first (A320 family maintenance planning, simplified):
//    A-check every 750 FH / 750 FC / 120 days (about 10 h, overnight, 4 mechanics, up to 8), C-check every 24 months or 7,500 FH /
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
	/** What StartCheck would ask now, nothing changed (the Maintenance page, to-do 311): its price in USD (labour, and parts
	 *  the stock lacks bought AOG), its length in game hours under the current Realism setting. False: Why it would refuse
	 *  (in the air, already in a check, no fault to put right, no free hangar bay, not enough money). */
	bool Quote(const FADPWorld& W, const FADPAircraft& A, const FString& Kind, double& Usd, double& Hours, FString& Why);
	/** Buys parts for the stock (USD of parts at today's parts market price). */
	bool BuyParts(FADPWorld& W, const FString& AirlineId, double Usd, const FDateTime& Now);
	/** Hangar bays at an airport (our site: its hangars; elsewhere 1). */
	int32 HangarBays(const FString& Icao);

	/** What a check is for (to-do 311): the faults an aircraft comes in with and the routine checks, each with its length
	 *  under Settings > Realism > Maintenance time - Fast (the default) and Real; Quick is 30 minutes for all. RealH 0: the
	 *  check rule's own length (Rules). Parts: USD at a 100 M aircraft (scaled by the type's value as the rules are). */
	struct FFault
	{
		const TCHAR* Key; const TCHAR* Name; double FastH, RealH, PartsUsd;
		/** The real crew for the job (hangar step 4): CrewMin hands it cannot start without, CrewMax the most that still help
		 *  (more hands up to there: shorter, the length times CrewMin / hands). A routine check takes its rule's Mechanics
		 *  as its least and CrewMax / CrewMin times that as its most. */
		int32 CrewMin, CrewMax;
		/** Where on the aircraft the work is (Data/aircraft-workpoints.json's areas, comma-separated; a fault picks one side). */
		const TCHAR* Areas;
	};
	const TArray<FFault>& Faults();
	const FFault* Fault(const FString& Key);
	/** The crew a check takes: Least it cannot start without, Most that still help (the table above, not branches). */
	void CrewFor(const FString& FaultKey, int32 RuleMechanics, int32& Least, int32& Most);
	/** The hands it gets from Free mechanics (0: not enough) and its length factor with them (Least / hands, 1 at Least). */
	int32 HandsFor(const FString& FaultKey, int32 RuleMechanics, int32 Free, double& Factor);
	/** Settings > Realism > Maintenance time: 0 Quick (30 min), 1 Fast (1.5-3 h by fault, the default), 2 Real. Kept in
	 *  GameUserSettings (ADP.Realism MaintTime); read when a check begins, so a running check keeps its length. */
	int32 MaintTime();
	void SetMaintTime(int32 Mode);
	/** A check's length in game hours for that mode: Real = RealH (crew and beginners already in it); Fast times Hands
	 *  (HandsFor's factor: more hands, shorter); Quick 30 minutes whatever the crew. */
	double CheckHours(const FString& FaultKey, double RealH, int32 Mode, double Hands = 1.0);
	/** A fault found in flight (FaultDue): its kind for the flights module's failures (engine, tailstrike, hydraulic, minor). */
	FString RollFault(const TCHAR* What, double Roll01);
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
