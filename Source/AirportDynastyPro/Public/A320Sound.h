// The A320's own sounds (owner 2026-11-28), from the FlightGear A320-family package (GPL v2 like the model:
// THIRD-PARTY-NOTICES.md; the wavs in Art/Sounds/A320, SOURCES.md there; imported by Tools/import_vehicle_sounds.py A320).
// Mixed as FlightGear's Sounds/cfm56-sound.xml + Systems/cfm56-sound.xml and A320-common-sound.xml do: each CFM56's layers
// (idle, spool, low spool, buzz, rumble, distant idle/rumble, reverse) faded and pitched by its N1 / N2 / reverser by
// FlightGear's own tables; starter, start-up and shut-down once; gear, flap motor, APU, touchdown, tyre screech, wheel
// roll, pushback, packs and the cabin's announcements - each at its part, each with its own reach. Inside the cabin the
// engines are the wing-aft recordings and the outside ones muffled. Shared by the moving aircraft (SimTraffic) and the
// parked ones (ParkedAircraft).
#pragma once
#include "CoreMinimal.h"

class ABabylonModel;

namespace ADPA320Snd
{
	/** What a moving aircraft does now (SimTraffic's FMove). Stage: its phase - 0 push, 1 taxi, 2 lined up / take-off
	 *  roll, 3 climb, 4 approach, 5 roll-out, 6 taxi in, 7 on the stand. */
	struct FIn
	{
		bool bEngines = false;   // the engines are wanted running (started on the push once it moves)
		float Power = 0.f;       // 0 off, 0.15 idle .. 1 take-off power, eased as the engines spool
		float Rev = 0.f;         // the reverser doors 0 stowed .. 1 open
		uint8 GearSt = 0;        // 0 down, 1 retracting, 2 up, 3 extending
		float Flap = 0.f;        // the flaps' degrees now
		float Height = 0.f;      // metres over the runway
		float Speed = 0.f;       // m/s over the ground
		uint8 Stage = 0;
	};
	/** A moving aircraft, every frame. */
	AIRPORTDYNASTYPRO_API void Moving(ABabylonModel* B, const FString& Reg, const FIn& In, float Dt);
	/** A parked aircraft, every frame: APU, packs, cabin and the boarding announcements from its turnaround. */
	AIRPORTDYNASTYPRO_API void Parked(ABabylonModel* B, const FString& Reg, float Dt);
}
