// Crowd test: about a hundred simple walkers wandering inside the terminal's rooms, to measure how the
// building runs with people in it. Replaced by Mass Entity crowds with daily lives later.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrowdTest.generated.h"

class UInstancedStaticMeshComponent;
struct FTerminalPlan;

UCLASS()
class AIRPORTDYNASTYPRO_API ACrowdTest : public AActor
{
	GENERATED_BODY()

public:
	ACrowdTest();
	void Start(const FTerminalPlan& Plan, int32 Count);
	virtual void Tick(float Dt) override;

private:
	#pragma region Walkers
	struct FWalker
	{
		FVector2f Pos, Target;
		float Speed = 1.3f, Wait = 0.f, Height = 0.f;
		int32 Room = 0;
	};
	FVector2f RandomPointIn(int32 Room) const;
	void Place(int32 i, bool bLast);

	struct FArea { float X0, Z0, X1, Z1, Y; };
	TArray<FArea> Areas;
	TArray<FWalker> Walkers;
	FRandomStream Rng;
	#pragma endregion

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Bodies;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Heads;
};
