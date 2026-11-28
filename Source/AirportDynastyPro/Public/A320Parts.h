// The A320's last moving parts, as FlightGear's own model moves them (Models/Fuselage/fuselage.xml, owner 2026-11-28):
// passenger doors L1/R1/L4/R4, cargo doors 1/2/3 (3 = the bulk door), the APU's inlet flap, the RAT stowed in its bay and
// the flap mechanisms hidden (FlightGear never shows them). Pivots, axes and tables from Art/A320/a320_surfaces.py (DOOR).
// Shared by the parked aircraft (ParkedAircraft.cpp, doors driven by the turnaround's tasks) and the moving ones (SimTraffic).
#pragma once
#include "CoreMinimal.h"

class ABabylonModel;

namespace ADPA320
{
	enum EPart : int32 { PL1, PR1, PL4, PR4, Cargo1, Cargo2, Cargo3, Apu, PartNum };
	/** Where each part should go (0 closed .. 1 open); the parts get there at FlightGear's own speeds. */
	struct FWant { float K[PartNum] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }; };
	/** Binds the parts once (RAT stowed, flap mechanisms hidden, all doors shut) - call right after Place("A320"). */
	AIRPORTDYNASTYPRO_API void Rest(ABabylonModel* M);
	AIRPORTDYNASTYPRO_API void Set(ABabylonModel* M, const FWant& W);
	/** Moves the parts toward their wants; false once all have arrived (nothing to do). */
	AIRPORTDYNASTYPRO_API bool Step(ABabylonModel* M, float Dt);
	/** The turnaround's tasks for this registration -> its doors and APU flap (bApu: the APU runs). */
	AIRPORTDYNASTYPRO_API FWant FromTurnaround(const FString& Reg, bool bApu);
}
