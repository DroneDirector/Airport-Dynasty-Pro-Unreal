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
	/** The A320's nose tyre behind its nose line (m): the stop bar's distance (Airfield.cpp FStopType). */
	constexpr float NoseGearBehindNose = 5.1f;
}
