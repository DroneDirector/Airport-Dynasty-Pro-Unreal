// The people in the terminal: passengers as Mass Entities, each on a departing passenger's trip (from the kerb to the
// check-in desks, through security, a stop upstairs at the Food Walk, Tax Free or the bar, to their gate, then aboard
// through the jetway or the walk-out door), walking the shortest way through the plan's doors and stairs. When one has
// boarded, a new passenger arrives at the kerb in their place. People far from the player are redrawn less often.
// Replaces the first crowd test; the shared world will give them their real flights and timings (batch 4).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MassEntityQuery.h"
#include "TerminalPlan.h"
#include "WalkMap.h"
#include "TerminalCrowd.generated.h"

class UInstancedStaticMeshComponent;
struct FPaxFragment;

UCLASS()
class AIRPORTDYNASTYPRO_API ATerminalCrowd : public AActor
{
	GENERATED_BODY()

public:
	ATerminalCrowd();

	/** Makes Count passengers, spread over every stage of the trip. */
	void Start(const FTerminalPlan& Plan, int32 Count);

	virtual void Tick(float DeltaSeconds) override;

private:
	#pragma region Trips
	/** Gives a passenger the next stage of their trip and the way there. */
	void Next(FPaxFragment& P);
	/** Puts a passenger somewhere along their trip (at the start, or anywhere for the first crowd). */
	void Begin(FPaxFragment& P, bool bAnywhere);
	void WalkTo(FPaxFragment& P, int32 Place, const FVector& At);
	void Step(FPaxFragment& P, float Dt);
	#pragma endregion

	#pragma region Figures
	void Draw(const FPaxFragment& P, bool bLast);
	#pragma endregion

	FWalkMap Map;
	FRandomStream Rng;
	float F2Y = 6;
	int32 Frame = 0;
	TArray<TArray<FVector>> Ways;   // each passenger's way, by their number
	TArray<int32> BodyKit;          // which coloured body list each passenger is in
	TArray<int32> BodyIdx;          // and their place in it
	int32 Kerb = 0, Hall = 0, Security = 0;
	TArray<int32> Upstairs, Gates;  // the places for the stop upstairs; gate places by gate number - 1
	TArray<FVector> GateWayOut;     // where each gate boards: the jetway door or the walk-out door
	TArray<int32> GateOut;          // the place beyond it (the jetway or the apron)
	FMassEntityQuery Query;
	bool bStarted = false;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Bodies;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Heads;
};
