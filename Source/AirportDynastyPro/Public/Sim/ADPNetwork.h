// The airline's network and money markets (to-do 184-186; owner, 2026-11-20 night).
//  184 - connections: at an airline's hub, passengers change between its own flights (more routes at the hub: more of them,
//    up to a quarter of a departure); a frequent-flyer scheme (on/off; costs a little per member a year) brings members back
//    (up to +8% load); alliances: the AI airlines found two or three, each with members; you may join one (reputation 55+,
//    a fee) or found your own: code-shares on routes touching a member's hub (+5% load), its members' passengers connect too;
//  185 - your airline's share price from its money, fleet, profit and reputation (moving a little every day); going public
//    (IPO): 30% of the shares sold for money, once; buying and selling shares in the AI airlines (yours to hold); fuel
//    hedging: fuel bought ahead at today's price (+3% a month for up to 6 months), used first on every leg;
//  186 - three weekly challenges every Monday (passengers, punctuality, new airports, profit, legs) with money, XP and a
//    badge; the league ranks every airline by passengers (30 days), punctuality, reputation and profit.
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"

namespace ADPNetwork
{
	/** Passengers a departure gains from connections, frequent flyers and code-shares (seats permitting). */
	void Boost(FADPWorld& W, FADPFlight& F, int32 Seats);
	/** A leg's fuel cost: hedged fuel first (its locked price), the rest at the spot price. */
	double FuelCost(FADPWorld& W, const FString& AirlineId, double FuelKg, double SpotPerKg);
	/** After a landing: the airline's counters (passengers, legs - the league and the challenges). */
	void Landed(FADPWorld& W, const FADPFlight& F);
	// the player's decisions (the Overview, ue.adp.sim)
	bool SetFrequentFlyer(FADPWorld& W, const FString& AirlineId, bool bOn);
	bool JoinAlliance(FADPWorld& W, const FString& AirlineId, const FString& Name, const FDateTime& At, FString& Why);
	bool FoundAlliance(FADPWorld& W, const FString& AirlineId, const FString& Name, const FDateTime& At, FString& Why);
	bool LeaveAlliance(FADPWorld& W, const FString& AirlineId);
	bool Ipo(FADPWorld& W, const FString& AirlineId, const FDateTime& At, FString& Why);
	bool TradeShares(FADPWorld& W, const FString& Target, double Shares, const FDateTime& At, FString& Why);   // + buy, - sell
	bool Hedge(FADPWorld& W, const FString& AirlineId, double Tonnes, int32 Months, const FDateTime& At, FString& Why);
	/** The league: every flying airline's score and parts, best first. */
	struct FRank { FString AirlineId, Name; double Score = 0, Pax30 = 0, OnTime = 0, Reputation = 0, Profit30 = 0; };
	TArray<FRank> League(const FADPWorld& W);
}

class AIRPORTDYNASTYPRO_API FADPNetworkModule : public IADPSimModule
{
public:
	virtual FName Name() const override { return TEXT("Network"); }
	virtual double Interval() const override { return 3600.0; }
	virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(30.0); }
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) override;
	FRandomStream Rng = FRandomStream(1866);
};
