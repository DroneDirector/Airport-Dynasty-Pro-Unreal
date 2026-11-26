// The money (docs/AI-Simulation-Plan.md part 5), our own model: every flight's profit and loss booked when it lands
// (tickets and cargo; fuel; landing, parking, handling and air-navigation fees; the maintenance reserve; crew
// allowances; booking costs and overhead), the weekly payroll, and the difficulty's effect on income and costs. Every
// line goes into the ledger and the airline's balance - yours and the AI airlines' alike.
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"

namespace ADPEconomy
{
	/** Books a landed flight's money. */
	void BookLeg(FADPWorld& W, const FADPFlight& F, const FDateTime& At);
	/** Adds a ledger line and moves the airline's balance. */
	void Pay(FADPWorld& W, const FString& AirlineId, const FDateTime& At, const TCHAR* Kind, double Amount, const FString& What, const FString& Link = FString());
	/** Fuel price (USD per kg) at an airport. */
	double FuelPrice(const FString& Icao, const FDateTime& When);
	/** The fuel of a leg (kg), what BookLeg charges and what the refueller pumps. */
	double LegFuelKg(const FString& TypeId, double BlockMin);
	/** A new loan: the money in now, a fixed instalment every month (annuity). */
	void TakeLoan(FADPWorld& W, const FString& AirlineId, double Amount, double RateYear, int32 Months, const FDateTime& At);
	/** A new aircraft: bought outright (the price now) or leased (two months' rent as deposit, then the rent monthly).
	 *  Returns its registration (empty: not enough money). */
	FString BuyAircraft(FADPWorld& W, const FString& AirlineId, const FString& TypeId, bool bLease, const FString& Base, const FDateTime& At);
	/** A used one from the market (its offer index). */
	FString BuyUsed(FADPWorld& W, const FString& AirlineId, int32 Offer, const FString& Base, const FDateTime& At);
	/** Sold (owned) or handed back (leased: three months' rent to end early). */
	bool SellAircraft(FADPWorld& W, const FString& Reg, const FDateTime& At);
	/** A marketing campaign for weeks (route empty: the whole airline): cost by size and reach. */
	void StartCampaign(FADPWorld& W, const FString& AirlineId, const FString& RouteId, int32 Weeks, const FDateTime& At);
	/** The campaign boost on this airline/route now (0 = none). */
	double CampaignBoost(const FADPWorld& W, const FString& AirlineId, const FString& RouteId, const FDateTime& At);
	/** Monthly lease rent for a type (about 0.7 % of its value a month). */
	double LeaseRate(const FString& TypeId);
	/** Owner 11-26: never an empty market - at least 5 A320 for sale and 4 to lease, from the first minute on. */
	void FillMarket(FADPWorld& W, FRandomStream& R);
}

/** The weekly payroll (and later the monthly items: loans, insurance, rents). */
class AIRPORTDYNASTYPRO_API FADPEconomyModule : public IADPSimModule
{
public:
	virtual FName Name() const override { return TEXT("Economy"); }
	virtual double Interval() const override { return 3600.0; }   // (looked at every hour; pays on Monday 00:00 UTC)
	virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(56.0); }   // (8 weeks)
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) override;
	int32 Paydays = 0, MonthEnds = 0, MarketNews = 0;
	FRandomStream Rng = FRandomStream(1337);
};
