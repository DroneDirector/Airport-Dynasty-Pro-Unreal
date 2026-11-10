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

/** One lift: where it stands (plan metres, its doors' facing), its two stops and the top of its shaft (metres), its
 *  sizes (the shaft's outer half width, the car's half width, a door leaf's width), its speed and what its stops are
 *  called. The terminal's lifts use the defaults; the control tower's runs from its ground floor to its lift lobby. */
struct FLiftSpec
{
	FPlanPoint P;
	float Lo = 0, Hi = 6, Top = 11.4f;
	float Out = 1.06f, Car = 0.84f, Door = 0.5f, Speed = 1.1f;
	FString Names[2] = { TEXT("1"), TEXT("2") };
	FString Sign;   // over the landing doors (default: "<id> · LIFT")
};

UCLASS()
class AIRPORTDYNASTYPRO_API ATerminalLifts : public AActor
{
	GENERATED_BODY()

public:
	ATerminalLifts();

	/** Builds every lift of the plan. */
	void Build(const FTerminalPlan& Plan);
	/** Builds one lift (the control tower's, or any other); returns its car (it rides up and down). */
	USceneComponent* AddLift(const FLiftSpec& Spec);

	virtual void Tick(float DeltaSeconds) override;

private:
	#pragma region Lift state
	struct FDoorPair { USceneComponent* A = nullptr; USceneComponent* B = nullptr; };
	struct FLift
	{
		FLiftSpec S;
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
	#pragma endregion

	#pragma region Parts
	USceneComponent* Node(USceneComponent* Parent, float X, float Y, float Z, float YawDeg = 0);
	/** A box under Parent, its size and centre in metres in the lift's frame (x across, y up, z out of the doors). */
	UStaticMeshComponent* Box(USceneComponent* Parent, const FName Mat, float W, float H, float D, float X, float Y, float Z, bool bCollide = true);
	UTextRenderComponent* Words(USceneComponent* Parent, const FString& Text, float X, float Y, float Z, float Size, FColor Colour);
	void SetDoors(FLift& L);
	void ShowFloor(FLift& L);
	static bool Upper(const FLift& L, float Y) { return Y > (L.S.Lo + L.S.Hi) / 2; }
	#pragma endregion

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
};
