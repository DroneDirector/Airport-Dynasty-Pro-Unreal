// Passengers and demand (docs/AI-Simulation-Plan.md part 4), our own model: how many people want to travel between two
// airports a day (the airports' size, the distance, the season), how they split between the airlines flying the pair
// (fare and quality), how full a departure is, its classes and cargo, and the ticket money it brings (booked by the
// economy, part 5).
#pragma once

#include "CoreMinimal.h"

struct FADPWorld;
struct FADPAirportInfo;
struct FADPTypeInfo;

struct FADPBooking { int32 Pax = 0, PaxJ = 0, PaxF = 0; double CargoKg = 0, LoadFactor = 0, Revenue = 0, FareY = 0; };

namespace ADPDemand
{
	/** One-way travellers a day between two airports (everyone, all airlines). */
	double MarketPax(const FADPAirportInfo& A, const FADPAirportInfo& B, const FDateTime& When);
	/** The economy fare (USD) for this distance and aircraft class. */
	double EconomyFare(double DistNm, const FString& Class, bool bInternational);
	/** One departure's passengers, classes, cargo and ticket money for this airline on this pair. */
	FADPBooking Book(const FADPWorld& W, const FString& AirlineId, const FString& RouteId, const FADPTypeInfo& Type,
		const FADPAirportInfo& A, const FADPAirportInfo& B, const FDateTime& When, FRandomStream& Rng);
}
