// Every door of an aircraft model, measured on the model (Data/aircraft-doors.json, Tools/make_aircraft_doors.py): where its
// handle is (outside and inside: hand IK to open and to close), where a person stands outside on the stair / jetway / cater
// or loader top, where inside, and where a jetway's, stair's or vehicle's front docks. Model frame, cm: x right, y aft,
// z up, the nose wheel on the ground at the root. Read by GSECater, GSEStairs (its own reader), GSELoader(You), JetwayRigs.
#pragma once
#include "CoreMinimal.h"

namespace ADPDoors
{
	struct FDoor
	{
		FString Name;                                   // L1, L4, R1, R4, Cargo1 (forward), Cargo2 (aft), Cargo3 (bulk)
		FVector2D Skin = FVector2D::ZeroVector;         // the middle of the sill on the skin (x, y)
		float Sill = 0.f, Turn = 0.f;                   // sill height; degrees the skin is turned from straight out (toward the tail)
		FVector2D Out = FVector2D(1.0, 0.0);            // the skin's outward direction in plan
		FVector HandleOut = FVector::ZeroVector, HandleIn = FVector::ZeroVector, Hinge = FVector::ZeroVector;
		FVector Outside = FVector::ZeroVector, Inside = FVector::ZeroVector, Dock = FVector::ZeroVector;
		FBox Frame = FBox(ForceInit);
	};
	/** The named door of the type's model; null when the file has no such door (logged once). */
	AIRPORTDYNASTYPRO_API const FDoor* Find(const FString& Type, const FString& Name);
	/** How far behind the nose wheel the door's middle is (m) - Fallback when not measured. */
	AIRPORTDYNASTYPRO_API float AlongM(const FString& Type, const FString& Name, float Fallback);
	/** The type's nose tip along the body (model y, cm); false when not measured. */
	AIRPORTDYNASTYPRO_API bool NoseY(const FString& Type, float& Out);
}
