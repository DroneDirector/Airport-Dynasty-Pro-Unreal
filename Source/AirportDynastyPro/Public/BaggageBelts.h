// The baggage claim's machinery (to-do 72, owner 2026-11-16; design docs/Baggage-Claim-Concept.md): five flat-plate
// carousels in the arrivals wing's claim hall and the conveyors that feed them from the crew side.
//   A bag's way: the ground crew put it on the long conveyor in the baggage sorting hall (the corridor under the gates'
//   walkway) at the loading point by its gate's ramp door; it rides west to its belt's input, turns onto the input
//   conveyor, rides to the wall and through its rubber flaps (out of sight - no basement), rises out of the belt's feed
//   shaft up the incline, through the hood's flaps, and drops onto the plates; then round and round until it's taken.
//   Each belt only runs when it is started: by the crew's button at its input, or the passengers' start / stop button
//   on its skirt (E / X, the glow outline, the button pressed in and out). Bags are fixed to whatever carries them.
//   Every landing: the crew empty the hold, the bags come to that flight's belt, the crew start it, and stop it once it
//   is empty. The flaps swing aside as a bag pushes through and settle back. Sounds: the start-up once (it fades out),
//   the running loop seamless until the belt stops (then a fade), a clack now and then - heard as from where you are:
//   fading with distance, muffled behind walls.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaggageBelts.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UTextRenderComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API ABaggageBelts : public AActor
{
	GENERATED_BODY()
public:
	ABaggageBelts();
	static ABaggageBelts* Get(UWorld* W);
	void Build();
	virtual void Tick(float Dt) override;
	FTransform PlateAt(float BeltX, const FVector2f& P, const FVector2f& D) const;
	/** E / X: a button you look at or stand by presses it; a bag in reach is taken off. True if something was used. */
	bool Interact(UPrimitiveComponent* Hit, const FVector& Where);
	/** An aircraft has parked at a stand (plan x of its nose): its bags come to a belt. */
	static void OnArrived(UWorld* W, float PlanX, const FString& Flight);
	/** Start or stop a belt (0..4). */
	void SetRunning(int32 Belt, bool bOn);
	bool IsRunning(int32 Belt) const { return Belts.IsValidIndex(Belt) && Belts[Belt].bRun; }
	/** Tests: bags straight onto a belt's input. */
	void Unload(int32 Belt, int32 Count, int32 Station = -1);

private:
	enum class EStage : uint8 { Trunk, Stub, Hidden, Rise, Belt, Gone };
	struct FBag { EStage Stage = EStage::Gone; int32 Belt = 0, Kind = 0, Ix = 0; float P = 0.f, Yaw = 0.f, Side = 0.f, Lap = 0.f, Wait = 0.f; };
	struct FFlap { TObjectPtr<UInstancedStaticMeshComponent> K; int32 Ix = 0; FVector Hinge; FQuat Base; FVector Axis; float A = 0.f, V = 0.f; };
	struct FBelt
	{
		float X = 0.f;                  // its middle (plan x); all at plan z 31
		bool bRun = false;
		float Speed = 0.f, Phase = 0.f, Idle = 0.f, ClackIn = 3.f, Press = 0.f, CrewPress = 0.f;
		int32 Waiting = 0;              // bags still to come from the hold
		FString Flight;
		TObjectPtr<UInstancedStaticMeshComponent> Plates;
		TObjectPtr<UStaticMeshComponent> Button, CrewButton;
		TObjectPtr<UAudioComponent> SStart, SRun, SClack;
		TObjectPtr<UTextRenderComponent> Screen;
		float Env = 1.f, EnvLp = 20000.f, EarIn = 0.f;
		int32 FirstFlap = 0, NumFlaps = 0;
		FVector BtnBase = FVector::ZeroVector, CrewBase = FVector::ZeroVector;
	};
	TArray<FBelt> Belts;
	TArray<FBag> Bags;
	TArray<FFlap> Flaps;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> BagMeshes;
	UPROPERTY() TArray<TObjectPtr<UObject>> Keep;
	TArray<TArray<int32>> FreeIx;       // per bag kind: hidden instances to reuse
	float SpawnIn = 0.f;
	struct FUnload { int32 Belt, Station, Left; float Every, In; };
	TArray<FUnload> Unloads;

	FVector W(float X, float H, float Z) const;
	FVector2f PathAt(float S, FVector2f& Dir) const;     // a belt's centre line (local plan x, z), anticlockwise
	bool Free(int32 Belt, EStage St, float P, float Gap, int32 Except) const;
	void PlaceBag(FBag& B);
	void Sounds(FBelt& Bt, int32 Index, float Dt);
	int32 NewBag(int32 Belt, int32 Station);
};
