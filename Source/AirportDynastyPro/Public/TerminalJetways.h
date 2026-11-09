// The jetways J-1 to J-4 (apron-drive passenger boarding bridges), laid out as in the Android edition (jetway.ts,
// plan.ts jetwayAt): the glazed Fixed corridor straight out from the gate's jetway door, sloping down 5°; the Rotunda on
// its column; three glazed telescopic Tunnel sections with steel chords; the bellows at the joints; the round Cab at the
// aircraft's front door sill (3.3 m) with its Canopy; the Drive unit (two lift columns, the bogie, four wheels).
// Built in the docked position for now; docking and undocking come with the aircraft (batch 5). All walkable.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerminalPlan.h"
#include "TerminalJetways.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

UCLASS()
class AIRPORTDYNASTYPRO_API ATerminalJetways : public AActor
{
	GENERATED_BODY()

public:
	ATerminalJetways();

	/** Builds a jetway at every jetway door of the plan, reaching to its stand. */
	void Build(const FTerminalPlan& Plan);

private:
	#pragma region Pieces
	UInstancedStaticMeshComponent* Kit(int32 Shape, const FName Mat);
	/** A box along the line A→B (plan metres: x, height, z), W wide and H high, shifted Lat to the right and Up above the
	 *  line, and Ext metres longer at each end. */
	void Along(const FName Mat, const FVector& A, const FVector& B, float W, float H, float Lat, float Up, float Ext = 0);
	/** An upright cylinder (or a wheel lying on its side when bWheel) at a plan point, radius R and height H. */
	void Drum(const FName Mat, const FVector& At, float R, float H, float YawDeg = 0, bool bWheel = false);
	/** One walkable glazed section from A to B (floor centre line): floor, lower panels, glass, roof and chords. */
	void Section(const FVector& A, const FVector& B, float W, float Hgt);
	#pragma endregion

	#pragma region Jetway
	void BuildJetway(const FPlanDoor& Door, const FPlanPoint& Stand, const FTerminalPlan& Plan);
	#pragma endregion

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Kits;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
};
