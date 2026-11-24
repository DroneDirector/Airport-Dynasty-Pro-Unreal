// The refueller's own moving parts (to-do 212, Private/GSEFuel.cpp): what a FTractor of AGSE needs besides the shared ones.
#pragma once

#include "CoreMinimal.h"

class UCableComponent;
class UBoxComponent;
class UTextRenderComponent;

struct FSvcRefuel
{
	int32 GWheel[2] = {INDEX_NONE, INDEX_NONE}, GSteer[2] = {INDEX_NONE, INDEX_NONE};   // the third axle
	int32 GReel = INDEX_NONE, GTail = INDEX_NONE, GNozzle = INDEX_NONE, GPlat = INDEX_NONE, GGate = INDEX_NONE, GPLever = INDEX_NONE, GScissor[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
	int32 GBondReel = INDEX_NONE, GClamp = INDEX_NONE, GBondTail = INDEX_NONE, GChock[2] = {INDEX_NONE, INDEX_NONE}, GDead = INDEX_NONE, GDeadLead = INDEX_NONE;
	int32 GLever[2] = {INDEX_NONE, INDEX_NONE}, GNeedle[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};   // (the pump's and the left hose's lever)
	TArray<int32> GTurn;
	UCableComponent *Hose = nullptr, *Bond = nullptr, *Lead = nullptr; UBoxComponent* Floor = nullptr; UTextRenderComponent* Meter = nullptr;
	enum : uint8 { InPlace, RightHand, LeftHand, Put };
	// where the carried things are: in their place, in a hand of Ci, or put somewhere (At, Yaw: root frame)
	uint8 Noz = InPlace, Clamp = InPlace, Chock[2] = {InPlace, InPlace}, Dead = InPlace;
	FVector NozAt = FVector::ZeroVector, ClampAt = FVector::ZeroVector, Couple = FVector(-430.f, -118.f, 380.f), NozNow = FVector::ZeroVector;
	float NozYaw = 0.f, NozYawNow = 0.f;
	int32 Ci = INDEX_NONE;
	float Lift = 0.f, LiftWant = 0.f, Gate = 0.f, GateWant = 0.f, Out = 0.f, OutWas = 0.f, Reel = 0.f, BondOut = 0.f, BondSpin = 0.f;
	float LeverNow[2] = {0.f, 0.f}, LeverWant[2] = {0.f, 0.f}, Flow = 0.f, Litres = 0.f, Shown = -1.f;
	bool bConn = false, bLive = true, bPump = false;   // (bLive: its parts have moved or may move; bPump: the dead man pressed)
};
