// Dirt and washing for every ground vehicle (owner 2026-11-14: "the same goes for all future vehicles"; 2026-11-21: the
// pushback tug, the GPU, the baggage tractor and the baggage cart in one batch). The fire truck's system (FireTruck.cpp
// PushDirt / Wash, Tools/set_truck_materials.py), made to fit any vehicle model:
//  - every painted, metal, rubber and plastic material of the model becomes M_VehicleDirt (Tools/set_vehicle_dirt_material.py:
//    the truck paint's road dirt, spray, wear chips, wetness, beads of water and wet sheen - keeping the model's own colour,
//    normal and metal/roughness textures), its glass the fire truck's glass (M_TruckGlass) with the same beads
//  - dirt builds by the distance driven (on the apron's paving slowly, faster in the wet, fastest off the paving) and a
//    little with time standing out on the ramp; heavy grime only off the paving
//  - the wash hose (WashHose.cpp) takes it off where its jet lands, in runs from the top down, and leaves beads and a wet
//    sheen on paint, tyres, plastic and glass that dry in ~20 minutes and blow off faster the faster it drives
// The 12 areas are the truck's: 6 lengths of 2 m along the vehicle x 2 sides (a short vehicle uses the middle ones).
#pragma once

#include "CoreMinimal.h"

class ABabylonModel;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;

struct FVehicleDirtState
{
	TWeakObjectPtr<ABabylonModel> M;
	FString Name;
	float Zone[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	float Goal[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	float Beads[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	float Blow = 0.f, Odometer = 0.f, Sent = -1.f, GroundCheck = 0.f;
	bool bOffRoad = false;
	TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> Paint, Glass;
	FVector Centre = FVector::ZeroVector, HalfExt = FVector(150.f, 80.f, 100.f);   // (the model root's frame: its parts' box)
	FVector LastO = FVector(1e9), LastF = FVector::ZeroVector;
};

struct AIRPORTDYNASTYPRO_API FVehicleDirt
{
	TSharedPtr<FVehicleDirtState> S;
	/** Puts the dirt materials on the model's parts (its lenses, mirrors and screens left as they are) and registers it for the
	 *  wash hose. Start: how dirty it is at first (0..1; < 0 picks a working vehicle's 0.2-0.5). */
	void Dress(ABabylonModel* Model, UObject* Outer, const FString& Name, float Start = -1.f);
	/** Every frame: SpeedMs its speed (m/s, measured); the dirt, the beads and the paint's parameters. */
	void Tick(float Dt, float SpeedMs);
	bool IsValid() const { return S.IsValid() && S->M.IsValid(); }
	void SetDirt(float V);
	/** The hose's jet landing on the vehicle at At (world): that part a little cleaner and wet. */
	void Wash(const FVector& At, float Amount);
	/** A part that is a wheel by its mesh's box (two sides equal, the third, along the axle, narrower): C its middle in the
	 *  mesh's frame, Size its radius, half its width, the axle 0 x / 1 y / 2 z, and -1 for a mirrored part (TyreC, TyreS). */
	static bool WheelShape(const UInstancedStaticMeshComponent* K, FLinearColor& C, FLinearColor& Size);
	/** The wash hose (WashHose.cpp): does a drop going from A to B hit any dressed vehicle (their parts have no collision)?
	 *  If so it washes it there; HitP / HitN the place and the face it hit. */
	static bool WashSegment(const FVector& A, const FVector& B, float Amount, FVector& HitP, FVector& HitN);
};
