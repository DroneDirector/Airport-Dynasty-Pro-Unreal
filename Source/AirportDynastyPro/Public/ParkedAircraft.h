// The aircraft parked at the terminal's gates (owner, 2026-11-15: "use this model ... replace all of the custom
// airplanes at the apron ... the nose wheel actually touches the right holding position that are marked on the
// ground ... nanite supported, and also lumen"): his Airbus A320 (Art/A320, Tools/import_model.py - Art/A320/A320.glb
// A320; one Nanite mesh, an ordinary lit material, so Lumen lights it) on every narrow-body stand at the terminal
// (stands 1-8, code C), in place of the Babylon terminal's own parked aircraft (BabylonModel.cpp leaves those out when
// this model is there). Each on its stand's yellow lead-in line, heading in, its nose tyre on the stand's
// "A319 A320 A321 B757" stop bar (Airfield.cpp: 5.1 m behind the nose line), its tyres on the apron (measured).
// Later (owner): its lights - wings, top, fuselage, gear - and the animation at the gate.
#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FAirfieldData;

namespace ADPParked
{
	/** Whether the A320 model is imported (then the terminal's own parked aircraft step aside). */
	bool HaveModel();
	/** Parks one on every narrow-body terminal stand. Returns how many. */
	int32 Place(UWorld* W, const FAirfieldData& Field);
	/** The traffic stress test (to-do 74): -TrafficTest=N on the command line (0 when it's not there). */
	int32 TrafficTestCount();
	/** Fills every other stand (wide-body, cargo, by the hangars) and slides the rest of N along the taxiways, the runway
	 *  and the approach (test movement only). Returns how many aircraft in all. */
	int32 TrafficTest(UWorld* W, const FAirfieldData& Field, int32 N);
	/** The simulation's aircraft at their stands (to-do 191, first step): once a career runs, the static aircraft give
	 *  way and each of the simulation's aircraft standing at our airport is shown at the stand it was given (A320 model
	 *  for every type until more models come), arriving and leaving with it. Checked every few seconds. */
	void StartLive(UWorld* W, const FAirfieldData& Field);
	/** The registrations still taxiing in (SimTraffic): their parked model waits. */
	extern TSet<FString> Coming;
	/** Its moving model has taken over (SimTraffic). */
	void Release(const FString& Reg);
	/** The A320's nose tyre behind its nose line (m): the stop bar's distance (Airfield.cpp FStopType). */
	constexpr float NoseGearBehindNose = 5.1f;
}
