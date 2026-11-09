// The lifts (E-01, E-G1–4), as in the Android edition (lifts.ts): each in its own shaft through both floors, clad
// outside, with a landing door on each floor and the car riding up and down inside it (a floor, brushed steel walls with
// a mirror at the back, a handrail, a lit ceiling, the button panel and the floor shown over the door, its own doors).
// They work as lifts do: come up to the doors and the lift is called to your floor; it opens; step in and after a
// moment the doors close and it takes you to the other floor (gently, speeding up and slowing down), then opens again.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerminalPlan.h"
#include "TerminalLifts.generated.h"

class UStaticMesh;
class UTextRenderComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API ATerminalLifts : public AActor
{
	GENERATED_BODY()

public:
	ATerminalLifts();

	/** Builds every lift of the plan. */
	void Build(const FTerminalPlan& Plan);

	virtual void Tick(float DeltaSeconds) override;

private:
	#pragma region Lift state
	struct FDoorPair { USceneComponent* A = nullptr; USceneComponent* B = nullptr; };
	struct FLift
	{
		FPlanPoint P;
		USceneComponent* Car = nullptr;
		UTextRenderComponent* Display = nullptr;
		TArray<FDoorPair> Landing;   // one pair on each floor
		FDoorPair CarDoor;
		float Y = 0, To = 0, V = 0;  // the car's height and where it is going, metres; its speed
		float K = 0;                 // the doors: 0 shut … 1 open
		float Hold = 0, InT = 0;
		bool bMoving = false;
		FString Shown;
	};
	TArray<FLift> Lifts;
	float F2Y = 6, F2Ceil = 11.4f;
	#pragma endregion

	#pragma region Parts
	USceneComponent* Node(USceneComponent* Parent, float X, float Y, float Z, float YawDeg = 0);
	/** A box under Parent, its size and centre in metres in the lift's frame (x across, y up, z out of the doors). */
	UStaticMeshComponent* Box(USceneComponent* Parent, const FName Mat, float W, float H, float D, float X, float Y, float Z, bool bCollide = true);
	UTextRenderComponent* Words(USceneComponent* Parent, const FString& Text, float X, float Y, float Z, float Size, FColor Colour);
	void SetDoors(FLift& L);
	void ShowFloor(FLift& L);
	#pragma endregion

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
};
