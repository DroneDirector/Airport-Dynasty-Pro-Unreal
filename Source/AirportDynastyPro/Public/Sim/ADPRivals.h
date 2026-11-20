// The AI airlines (docs/AI-Simulation-Plan.md part 7; owner, 2026-11-20: real AI airlines that buy aircraft, open routes,
// hire and can go bankrupt - your own airline is yours alone). Each AI airline has its hub, money and quality and decides
// every day: buy or lease an aircraft when it can afford one and its flights are full, open a route for an idle aircraft
// to the best market in reach of its hub (hiring the pilot and cabin crew it needs), close routes that lose money, cut
// fares when you start flying one of its pairs, sell aircraft when short of money, and go bankrupt after too long in the
// red. They fly through the same flights, demand and economy as you.
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"

class AIRPORTDYNASTYPRO_API FADPRivalsModule : public IADPSimModule
{
public:
	virtual FName Name() const override { return TEXT("Rivals"); }
	virtual double Interval() const override { return 3600.0; }   // (every hour; each airline decides once a day, spread over the day)
	virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(30.0); }
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) override;
	/** Founds the AI airlines if there are none yet (Count of them, hubs spread over the busiest airports, half near yours). */
	void Found(FADPWorld& W, int32 Count, const FDateTime& At);
	int32 Bought = 0, Opened = 0, Closed = 0, Sold = 0, Bankrupt = 0, FareWars = 0;
	FRandomStream Rng = FRandomStream(5150);
private:
	void Decide(FADPWorld& W, FADPAirline& A, const FDateTime& At);
	bool OpenRoute(FADPWorld& W, FADPAirline& A, FADPAircraft& Craft, const FDateTime& At);
};
