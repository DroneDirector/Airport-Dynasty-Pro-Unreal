// The potable water truck's and the toilet truck's own moving parts (to-do 212, Private/GSEWater.cpp): what a FTractor of
// AGSE needs besides the shared ones.
#pragma once

#include "CoreMinimal.h"

class UCableComponent;

struct FSvcWet
{
	enum : uint8 { InPlace, RightHand, LeftHand, Put, YourHand };
	enum : uint8 { Chock0, Chock1, Main, Rinse, Cap, NThings };   // (Main: the water coupling or the waste coupling)
	// (Grip: the part's middle from the hand; Tip: about the truck's side axis; Turn: about its own; bRide: put on the platform)
	struct FThing { int32 G = INDEX_NONE; FVector Piv = FVector::ZeroVector, Grip = FVector::ZeroVector, At = FVector::ZeroVector, Now = FVector::ZeroVector; uint8 Mode = InPlace; bool bRide = false; float Tip = 0.f, TipNow = 0.f, Turn = 0.f; };
	FThing Th[NThings];
	bool bToilet = false, bLive = true, bConn = false, bRinseConn = false, bRun = false, bStop = false; float YouStandX = -1e6f;   // (you: a stop pressed, the aircraft you serve)  (bRun: filling, draining, rinsing)
	int32 GLift = INDEX_NONE, GGate = INDEX_NONE, GReel = INDEX_NONE, GTail = INDEX_NONE, GHose = INDEX_NONE, GTray = INDEX_NONE, GValve = INDEX_NONE, GPLever = INDEX_NONE;
	TArray<int32> GTurn;
	UCableComponent *Hose = nullptr, *RinseHose = nullptr;
	float Lift = 0.f, LiftWant = 0.f, Gate = 0.f, GateWant = 0.f, Valve = 0.f, ValveWant = 0.f, Tray = 0.f, TrayWant = 0.f, Reel = 0.f, OutWas = 0.f;
	FVector Couple = FVector::ZeroVector;   // (the aircraft's coupling, root cm)
	int32 Ci = INDEX_NONE;
};
