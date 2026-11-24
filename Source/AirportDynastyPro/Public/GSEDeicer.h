// The de-icing truck's own moving parts and its work (to-do 212, Private/GSEDeicer.cpp): what a FTractor of AGSE needs besides the shared ones.
#pragma once

#include "CoreMinimal.h"

class UInstancedStaticMeshComponent;
class UCableComponent;
class UAudioComponent;

struct FSvcDeicer
{
	struct FPose { float Slew = 0.f, Lift = 0.f, Ext = 0.f; };          // degrees (its yaw, 0 over the tanks), degrees, cm
	struct FStop { float Lat = 0.f, Aft = 0.f; int8 Side = 1; bool bWing = true, bOut = false; uint8 Part = 0; };   // Part: 0 the rear / 1 the front half of the wing or tail, 2 the wing's root and the fuselage's top from behind; m left of the centre line, m behind the nose wheel; Side: 1 the aircraft's right; bOut: the last in its lane
	struct FAim { FVector At = FVector::ZeroVector; FPose Q; };          // (the place on the aircraft, world; the boom for it)
	struct FDrop { FVector P = FVector::ZeroVector, V = FVector::ZeroVector; float Life = 0.f, Seed = 0.f, Q = 0.f; uint8 Kind = 0; };   // 0 type I, 1 type IV, 2 spray
	enum EPh : uint8 { Idle, Wait, Drive, OpIn, Work, Stow, OpOut, Leave };
	int32 GTur = INDEX_NONE, GBase = INDEX_NONE, GSec[2] = {INDEX_NONE, INDEX_NONE}, GCab = INDEX_NONE, GDoor = INDEX_NONE, GWip = INDEX_NONE, GJoy[2] = {INDEX_NONE, INDEX_NONE}, GSpray = INDEX_NONE, GPan = INDEX_NONE, GTilt = INDEX_NONE, GBarrel = INDEX_NONE, GRod = INDEX_NONE, GHose[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
	TArray<int32> GChain;
	UCableComponent* Hose[3] = {nullptr, nullptr, nullptr};
	FPose Now, Was; TArray<FPose> Route;
	float Pan = 0.f, Tilt = 0.f, PanWant = 0.f, TiltWant = 0.f, Joy[2] = {0.f, 0.f}, WipT = 0.f, SlewV = 0.f, LiftV = 0.f, ExtV = 0.f, LaneT = 0.f;
	float Water = 2600.f, TankI = 2400.f, TankIV = 2400.f;              // litres
	float UsedI = 0.f, UsedIV = 0.f;                                     // at this aircraft
	bool bLive = true, bOpIn = false, bOpBusy = false, bBroken = false, bYours = false, bLaneFree = false;
	FString LaneWhy;
	EPh Ph = Idle; float PhT = 0.f;
	TArray<FStop> Stops; int32 Stop = 0, Done = 0, Missed = 0, Skipped = 0;
	TArray<FAim> Aims; int32 Aim = 0, Pass = 0; float Dwell = 0.f; bool bAiming = false; FVector AimAt = FVector::ZeroVector;
	int32 Jet = -1;                                                      // the fluid now on: 0 type I, 1 type IV, -1 none
	float Emit = 0.f; TArray<FDrop> Drops;
	UInstancedStaticMeshComponent *Stream = nullptr, *Tint[2] = {nullptr, nullptr}, *Mist = nullptr;
	UAudioComponent* Snd = nullptr; bool bSndUp = true;
	// by hand (to-do 238): the side panels' doors (pump panel, hose reel, heater), the tanks' caps, the valves (water, I, IV, hand; degrees, 90 shut), the hand nozzle
	struct FSpot { FVector R = FVector::ZeroVector; float L[2] = {0.f, 0.f}; };   // (a place of the aircraft, litres of type I / IV on it from you)
	int32 GPDoor[3] = {-1, -1, -1}, GCap[3] = {-1, -1, -1}, GValve[4] = {-1, -1, -1, -1}, GNeedle[4] = {-1, -1, -1, -1}, GHand = -1, GTail = -1;
	float DN[3] = {0.f, 0.f, 0.f}, DW[3] = {0.f, 0.f, 0.f}, CN[3] = {0.f, 0.f, 0.f}, CW[3] = {0.f, 0.f, 0.f}, VN[4] = {0.f, 0.f, 0.f, 0.f}, VW[4] = {0.f, 0.f, 0.f, 0.f}, Gauge[4] = {82.f, 0.f, 0.f, 82.f};
	bool bPump = true, bHeat = true, bWipe = false, bHandJet = false, bHandOut = false; uint8 Halt = 0, Hand = 0;   // Halt: the panel's / the cabin's emergency stop; Hand: the nozzle in yours
	float TempI = 82.f, LeftT = 0.f, NearT = 0.f, YouStandX = -1e9f; int32 Told = 0;
	FVector HandDir = FVector::ForwardVector, HandAt = FVector::ZeroVector; UCableComponent* Line = nullptr;
	TArray<FSpot> Spots; FDateTime SpotEnd = FDateTime::MaxValue();
	TWeakObjectPtr<const AActor> Plane; FBox Box = FBox(ForceInit);      // (the aircraft it stands at, its bounds in its own frame)
};
