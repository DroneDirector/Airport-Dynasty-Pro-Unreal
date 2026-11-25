// An apron bus's battery and its charging (to-do 277 E3b): GSECharge.cpp.
#pragma once
#include "CoreMinimal.h"

struct FBusCharge
{
	enum EPlug : uint8 { Holster, CrewHand, YourHand, Inlet, Dropped };   // (Dropped: let go of, on the ground on its cable)
	enum EStage : uint8 { Idle, Plugging, Unplugging };
	bool bHasSoc = false; float Soc = 1.f;                                       // 0..1
	FVector LastAt = FVector::ZeroVector; FDateTime LastClock; uint8 Doors = 0;
	int32 GFlap = INDEX_NONE; FVector FlapPiv = FVector::ZeroVector, InletP = FVector::ZeroVector; float FlapNow = 0.f, FlapWant = 0.f, FlapWas = -1.f;
	uint8 Plug = Holster, Stage = Idle; int32 Ci = INDEX_NONE;
	FString Cable;                                         // the charger its plug belongs to ("" none)
	FVector PlugAt = FVector::ZeroVector, PlugX = FVector::ForwardVector;   // (world, as drawn)
	FString Bay; FVector BayAt = FVector::ZeroVector; float BayYaw = 0.f; bool bGo = false;   // (a charging bay kept for it, not its own place)
	FString BayName; bool bOwnBay = false, bBayRead = false;                // (its own place, when that is a charging bay)
	FVector WasHome = FVector(1e9, 0., 0.); bool bHoming = false;   // (the place it left for a shared bay; on its way home)
	bool bNeeded = false; FDateTime NotBefore; float Wait = 0.f, SaidT = 0.f; FString Why;
	TWeakObjectPtr<class UAudioComponent> Hum; TWeakObjectPtr<class UTextRenderComponent> Read; int32 Shown = -1;
	/** Not to be driven: the plug, its crew or the open flap. */
	bool Holds() const { return Plug != Holster || Stage != Idle || FlapNow > 0.02f || FlapWant > 0.f; }
};
