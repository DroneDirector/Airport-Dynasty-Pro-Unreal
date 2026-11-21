// Every door in the plan as its own object, as in the Android edition (doors.ts): sliding glass doors, revolving
// doors (the wings turn all the time), swing doors, secure staff doors (steel, a card reader with its light), the
// double door, the one-way door out of security, jetway doors, and the open ways through (a frame and nothing to stop
// you). They open as you come up to them and shut behind you. The room's name hangs over the door on the public side.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerminalPlan.h"
#include "TerminalDoors.generated.h"

class UStaticMesh;
class UMaterialInterface;

UCLASS()
class AIRPORTDYNASTYPRO_API ATerminalDoors : public AActor
{
	GENERATED_BODY()

public:
	ATerminalDoors();

	/** Places every door of the plan. */
	void Build(const FTerminalPlan& Plan);

	virtual void Tick(float DeltaSeconds) override;

	/** People other than you walking through the doors (world cm, refreshed every frame by whoever walks them - the
	 *  passengers, PaxFlow.h): the doors open for them as they do for you. */
	static TArray<FVector> Openers;
	/** Passengers still on their way through check-in and security (PaxFlow): the security doors stand open until the last one is through. */
	static bool bHoldSecurity;
	/** Passengers about (PaxFlow): the entrance doors into the Grand Hall stand open until the airport is empty (a player close switch later). */
	static bool bHoldEntrances;

private:
	#pragma region Door rigs
	struct FSlide { USceneComponent* Part = nullptr; float X0 = 0, Dir = 1; };
	struct FHinge { USceneComponent* Leaf = nullptr; float Dir = 1; };
	struct FDoorRig
	{
		FPlanDoor D;
		float K = 0;                     // 0 shut … 1 open
		TArray<FSlide> Slides;
		TArray<FHinge> Hinges;
		USceneComponent* Wings = nullptr; // a revolving door's turning wings
	};
	TArray<FDoorRig> Rigs;
	float Time = 0;
	float F2Y = 6;
	#pragma endregion

	#pragma region Building parts
	void BuildDoor(const FPlanDoor& D, const FTerminalPlan& Plan);
	/** A box part under Parent: size and centre in metres in the door's own frame (x across, y up, z through). */
	UStaticMeshComponent* Part(USceneComponent* Parent, const FName Mat, float W, float H, float Dp, float X, float Y, float Z, bool bCollide = true);
	USceneComponent* Node(USceneComponent* Parent, float X, float Y, float Z, float YawDeg);
	UMaterialInterface* M(const FName Name);
	#pragma endregion

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UMaterialInterface> GlassM;
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
};
