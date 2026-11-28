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
		bool bLeaving = false;   // the climb leaves the visible world now (its 10,000 ft announcement is due)
	};
	/** A moving aircraft, every frame. */
	AIRPORTDYNASTYPRO_API void Moving(ABabylonModel* B, const FString& Reg, const FIn& In, float Dt);
	/** Engine e's fan now (0 left, 1 right): N1 % as the real fan turns (a gentle start, a run-down of about a minute),
	 *  and whether the engine lit this frame (its light-off). 0 for a model Moving() has not met. */
	AIRPORTDYNASTYPRO_API float FanN1(const ABabylonModel* B, int32 e, bool* bLitNow = nullptr);
	/** A parked aircraft, every frame: APU, packs, cabin and the boarding announcements from its turnaround. */
	AIRPORTDYNASTYPRO_API void Parked(ABabylonModel* B, const FString& Reg, float Dt);
}
