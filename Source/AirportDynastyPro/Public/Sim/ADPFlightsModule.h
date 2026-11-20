// Hired pilots flying the routes by themselves (docs/AI-Simulation-Plan.md part 3): every aircraft with a route and a
// pilot flies its legs in turn - the checks before departure (aircraft, pilot, duty time, rest), the flight through its
// phases on its real profile, the landing (hours, cycles, where it now stands), the next leg after the turnaround, the
// pilot's rest when the next leg would break the duty limit, delays at random. Your own airline and the AI airlines
// both run here (the AI airlines' own decisions come in part 7). Weather, failures and the money are their own parts.
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPWorldData.h"

class AIRPORTDYNASTYPRO_API FADPFlightsModule : public IADPSimModule
{
public:
	static constexpr double TurnMin = 50.0;        // the turnaround
	static constexpr double RestH = 12.0;          // a pilot's rest
	static constexpr double MaxDutyH = 10.0;       // the duty limit
	static constexpr double CancelAfterH = 5.0;    // a leg waiting longer is cancelled
	virtual FName Name() const override { return TEXT("Flights"); }
	virtual double Interval() const override { return 15.0; }
	virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(14.0); }
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) override;

	/** A flight's plan (kept while it flies; made again after a load). */
	const FADPProfile* ProfileOf(const FADPFlight& F);
	int64 Departures = 0, Landings = 0, Delayed = 0, Rests = 0, WxWaits = 0, DeIcings = 0;

private:
	TMap<FString, FADPProfile> Profiles;
	FRandomStream Rng = FRandomStream(4711);
	void TryDepart(FADPWorld& W, FADPAircraft& A, const FDateTime& Now);
	void Land(FADPWorld& W, int32 FlightIdx, const FDateTime& Now);
};
