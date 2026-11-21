// The pushback tugs (owner, 2026-11-21: the airport's ground vehicles, the tug first - Tools/make_pushback_tug.py, imported as
// PushbackTug). One manager for every tug at work:
//  - a departure's pushback (SimTraffic.cpp): a tug is hooked to the aircraft's nose gear - its towbar's head on the tow
//    fitting just ahead of the nose wheel, the bar on the tug's front hitch - and pushes it back with it, then lets go
//    (the hitch clunk at both), backs away with its reversing beeper and leaves
//  - its lamps glow by material (the fire truck's lens material: M_TruckLens, one value per lens): low beams, tail lamps
//    and markers while it runs, the reversing lamps when it backs, the amber roof beacon and the two rear flashers flashing
//    as the jetway beacons do; its windows are the fire truck's glass (M_TruckGlass)
//  - its sounds where it is: the engine (start, idle loop, stop), the hitch, the reversing beeper (Art/Sounds/PushbackTug)
// Next steps (to-do 196/198, the vehicles list): the operator (a MetaHuman walking out, hooking on and climbing in with the
// tug's IK points), driving it yourself (E/X, the outline outside only), dirt and washing as the fire truck.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tugs.generated.h"

class ABabylonModel;
class UAudioComponent;
class UInstancedStaticMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API ATugs : public AActor
{
	GENERATED_BODY()
public:
	ATugs();
	static ATugs* Get(UWorld* W);
	virtual void Tick(float Dt) override;
	/** A pushback in progress: Key the move, Fitting the aircraft's tow fitting (world), Fwd the aircraft's nose direction
	 *  (world, flat), Speed m/s. The first call hooks a tug on. */
	void Push(const FString& Key, const FVector& Fitting, const FVector& Fwd, float Speed);
	/** The pushback is done: the tug lets go, backs away and leaves. */
	void Release(const FString& Key);
	/** Tests: how many tugs are at work. */
	int32 NumAtWork() const { return Jobs.Num(); }

private:
	struct FLens { UInstancedStaticMeshComponent* K = nullptr; int32 Ix = 0; uint8 Kind = 0; float Now = -1.f; };
	struct FJob
	{
		TWeakObjectPtr<ABabylonModel> M;
		TArray<FLens> Lenses;
		UAudioComponent* Engine = nullptr; UAudioComponent* Beep = nullptr;
		FVector At = FVector::ZeroVector, Fwd = FVector::ForwardVector;   // the tug's place and the aircraft's nose direction
		float Speed = 0.f, Clock = 0.f, Leave = -1.f;                       // Leave >= 0: let go, backing away (seconds)
		bool bHooked = false;
	};
	TMap<FString, FJob> Jobs;
	float Flash = 0.f;
	bool Spawn(FJob& J);
	void PoseTug(FJob& J);
	void Lamps(FJob& J, bool bReverse);
	void Sound(const TCHAR* Name, const FVector& At);
};
