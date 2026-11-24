// The catering high-loader's own moving parts and its trolleys (to-do 212, Private/GSECater.cpp): what a FTractor of AGSE
// needs besides the shared ones.
#pragma once

#include "CoreMinimal.h"

class UInstancedStaticMeshComponent;

struct FSvcCater
{
	int32 GBody = INDEX_NONE, GPlat = INDEX_NONE, GRail[2] = {INDEX_NONE, INDEX_NONE}, GGate = INDEX_NONE, GFlap = INDEX_NONE, GSc[2] = {INDEX_NONE, INDEX_NONE};
	int32 GRamB[2] = {INDEX_NONE, INDEX_NONE}, GRamR[2] = {INDEX_NONE, INDEX_NONE}, GLeg[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE}, GLev[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
	TArray<int32> GRoll;
	// its movements (cm, or 0..1) and where they are going
	float Lift = 0.f, LiftWant = 0.f, Out = 0.f, OutWant = 0.f, Legs = 0.f, LegsWant = 0.f, Roll = 0.f, RollWant = 0.f, Flap = 0.f, FlapWant = 0.f;
	float Rail[2] = {0.f, 0.f}, RailWant[2] = {0.f, 0.f}, Lev[3] = {0.f, 0.f, 0.f};   // (rails: left, right; levers: legs, lift, platform)
	bool bLive = true;
	// the trolleys and boxes: instanced, they ride with the body
	struct FBit { UInstancedStaticMeshComponent* K = nullptr; int32 Ix = 0; FTransform Rel; uint8 Pedal = 0; };   // (Pedal: 1 red, 2 green)
	struct FTrolley { TArray<FBit> Bits; FVector Home = FVector::ZeroVector; };
	TArray<FTrolley> Trolleys; TArray<UInstancedStaticMeshComponent*> Kits;
	int32 Mov = INDEX_NONE; FVector MovAt = FVector::ZeroVector; bool bMovShow = true; uint8 Pedal = 0;   // (the one being pushed: its place on the floor, root cm with the lift; the pedal trodden on)
	float Sill = 387.f;   // (the aircraft's sill over the truck's own ground)
};
