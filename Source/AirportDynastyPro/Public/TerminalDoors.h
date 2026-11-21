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

	static ATerminalDoors* Find(const UWorld* W);

	#pragma region Revolving doors
	// (owner 2026-11-21: the side entrances' revolving doors work for passengers - nobody ever stuck, everyone out).
	// An automatic revolving door: 4 wings, so 4 compartments, turning about 3-4 rpm (3.6 with people near, 3.0 idle, 2.7
	// while someone steps in - the presence sensor). A passenger waits outside until an EMPTY compartment's opening lines
	// up with their side, claims it (one passenger or one party per compartment, never strangers together), is carried
	// round with it (PaxFlow moves them kinematically on the compartment's arc - never by collision, so never into the
	// glass) and steps out when it lines up with the far opening. Missed the exit: the door stops, waits a moment, turns
	// back to the exit and holds there until they are out (the safety stop, logged). The drum is off the navmesh
	// (PaxNav: a null area over it), so nobody else is ever routed through the glass.
	int32 NumRevolving() const { return Revs.Num(); }
	/** The revolving door nearest a world point (INDEX_NONE: none); its distance in cm. */
	int32 NearestRevolving(const FVector& World, float* DistCm = nullptr) const;
	/** Its centre on the floor (world), the axis through its two openings (world, unit), its radius in cm. */
	bool RevolvingFrame(int32 Ix, FVector& Centre, FVector& Axis, float& RadiusCm) const;
	FString RevolvingId(int32 Ix) const;
	/** Someone waiting at the opening on side Side (+1 along the axis, -1 against it) asks to step in: an empty
	 *  compartment lined up with that opening is claimed for Who (and returned); INDEX_NONE while none is. */
	int32 RevolvingBoard(int32 Ix, int32 Side, int32 Who, const FString& Name);
	/** Stepped in: now riding (the safety stop watches them from here on). */
	void RevolvingRiding(int32 Ix, int32 Comp);
	/** Where a compartment's middle is now (world, Frac of the radius out), the way it is moving (world unit) and how
	 *  fast (cm/s). */
	FVector RevolvingSpot(int32 Ix, int32 Comp, float Frac, FVector* Dir = nullptr, float* SpeedCm = nullptr) const;
	/** The compartment lined up with its rider's far opening: they may step out now. bHeld: the door has stopped for
	 *  them (the safety stop) - out now, whatever is in the way. */
	bool RevolvingAtExit(int32 Ix, int32 Comp, bool* bHeld = nullptr) const;
	/** Out (or gone): the compartment is free again. */
	void RevolvingLeave(int32 Ix, int32 Comp);
	#pragma endregion

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
		USceneComponent* Frame = nullptr; // the door's own frame (x across, y through, z up; cm)
		int32 NWings = 0;
	};
	TArray<FDoorRig> Rigs;
	struct FComp { int32 Who = INDEX_NONE; int32 Side = 0; bool bRiding = false; float Progress = 0.f; FString Name; };   // Progress: degrees turned from the entry opening (180 = lined up with the exit)
	enum class ERev : uint8 { Run, Pause, Back, Hold };
	struct FRevolve { int32 Rig = INDEX_NONE; float Angle = 0.f, Omega = 0.f; ERev Mode = ERev::Run; float ModeT = 0.f; int32 Safety = INDEX_NONE; TArray<FComp> Comps; };   // Angle, Omega: degrees, degrees a second
	TArray<FRevolve> Revs;
	void StepRevolving(FRevolve& V, float Dt, const FVector& Player);
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
