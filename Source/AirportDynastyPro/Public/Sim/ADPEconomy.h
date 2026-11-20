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
}

/** The weekly payroll (and later the monthly items: loans, insurance, rents). */
class AIRPORTDYNASTYPRO_API FADPEconomyModule : public IADPSimModule
{
public:
	virtual FName Name() const override { return TEXT("Economy"); }
	virtual double Interval() const override { return 3600.0; }   // (looked at every hour; pays on Monday 00:00 UTC)
	virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(56.0); }   // (8 weeks)
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) override;
	int32 Paydays = 0;
};
