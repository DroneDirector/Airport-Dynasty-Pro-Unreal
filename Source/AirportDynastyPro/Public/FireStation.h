// The airport fire station (ARFF), its training tower and its two crash tenders (owner, 2026-11-11: the brief in
// docs/fire-station/SPEC.md). Built as the owner's approved design sketch builds it (docs/fire-station/sketch, the photo
// textures in Art/FireStation), exported from it (Tools/BabylonExport ?m=firestation, ?m=firetruck&n=1|2) and placed
// here: on the airfield where the ground layout has it (west of the terminal, south of taxiway A, its forecourt facing
// north to the taxiway), the two trucks each their own object, parked in bays 2 and 3 behind the shut doors.
#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace ADPFireStation
{
	/** Where it stands (plan metres) and how it is turned: the sketch's front (its -z) faces north. */
	constexpr float X = -1169.f, Y = 20.f, Yaw = 180.f;
	/** A point of the sketch (x along the front, z back from it) in plan metres. */
	inline FVector2D ToPlan(float Sx, float Sz) { return FVector2D(X - Sx, Y - Sz); }
	/** The bays' middles along the front (the sketch's bayX): bay 1 is index 0. */
	inline float BayX(float I) { return 18.f + 4.5f + I * 8.f; }
	/** Places the station, its lights and the two trucks. */
	void Place(UWorld* W);
	/** Its outline for the map (plan metres, x0, z0, x1, z1): the walls; the bay doors apart. */
	void MapOutline(TArray<FVector4f>& Walls, TArray<FVector4f>& Doors);
}
