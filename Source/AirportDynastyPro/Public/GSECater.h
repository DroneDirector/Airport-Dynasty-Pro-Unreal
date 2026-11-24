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
	enum : uint8 { AtHome, YourHands, Parked, InCabin };
	struct FTrolley { TArray<FBit> Bits; FVector Home = FVector::ZeroVector, At = FVector::ZeroVector; uint8 State = AtHome, Door = 0; bool bEmpty = false, bOwed = false; float CabinT = 0.f; };   // (At: root cm without the lift)
	TArray<FTrolley> Trolleys; TArray<UInstancedStaticMeshComponent*> Kits;
	int32 Mov = INDEX_NONE; FVector MovAt = FVector::ZeroVector; bool bMovShow = true; uint8 Pedal = 0;   // (the one being pushed: its place on the floor, root cm with the lift; the pedal trodden on)
	// yours (to-do 238): the rear roll-up door, the tail lift (Fold 1: level; Tail 0 floor .. 1 ground), the gate, the two stops
	int32 GTailArm = INDEX_NONE, GTailPl = INDEX_NONE; TArray<int32> GRollR;
	float RollR = 0.f, RollRWant = 0.f, Fold = 0.f, FoldWant = 0.f, Tail = 0.f, TailWant = 0.f, Gate = 0.f, GateWant = 0.f, PedalT = 0.f, SeeT = 0.f, YouSill = 0.f, YouStandX = -1e6f;
	int32 YouDoor = 0, YouMov = INDEX_NONE, PedalTr = INDEX_NONE;
	float Dir = 1.f; bool bStop = false, bStabStop = false, bStow = false, bSqueeze = false;   // (Dir +1: you stand behind the trolley, -1: in front of it)
	float Sill = 387.f;   // (the aircraft's sill over the truck's own ground)
	// its own wheel chocks (left drive wheel) and two nesting cones (Cone_1 sits on Cone_0): in the tray, in a hand, set out
	enum : uint8 { InTray, InRight, InLeft, AtPlace, InYours };
	int32 GChock[2] = {INDEX_NONE, INDEX_NONE}, GCone[2] = {INDEX_NONE, INDEX_NONE}, GearCi = INDEX_NONE;   // (GearCi: the crew who carries them)
	uint8 Chock[2] = {InTray, InTray}, Cone[2] = {InTray, InTray};
};
