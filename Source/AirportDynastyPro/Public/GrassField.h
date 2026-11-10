// The grass round the airport: real blades, our own clumps (Tools/make_grass.py, set_grass.py), on every grass surface
// (the airfield's grass and the Babylon model's lawns), from your feet out to the far edge of the airfield.
//  - near you: dense, finely curved clumps swaying in the wind (out to the Settings' grass distance)
//  - further out, to 300 m: simpler clumps, still swaying
//  - the whole airfield, to the horizon: wide tufts laid once, still
// All three are Nanite meshes, so the far grass is drawn with as few triangles as its distance needs; the wind is only
// worked out close by; no grass casts shadows. Where grass is, a downward look finds out once per patch of ground.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrassField.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API AGrassField : public AActor
{
	GENERATED_BODY()

public:
	AGrassField();
	/** The airfield's outline (plan metres, as Airfield.h's boundary): the far tufts cover all of it. */
	void Start(float X0, float Y0, float X1, float Y1);
	virtual void Tick(float Dt) override;

	/** How far the dense near grass reaches (metres, 0 = none): Settings' slider. */
	static float NearDistance;
	/** Grass on or off (Settings). */
	static bool bOn;
	/** Walking somewhere else on Earth (WalkAnywhere.h): the airfield's own grass set aside, only the grass round you. */
	static bool bAway;

private:
	/** A patch of ground (25 m square): the grass's height at each metre (none where it isn't grass). */
	struct FPatch { TArray<float> H; };
	static constexpr float Cell = 2500.f;    // cm
	const FPatch& Patch(const FIntPoint& C);
	bool GroundAt(float X, float Y, float& Z) const;
	UHierarchicalInstancedStaticMeshComponent* NewLayer(UStaticMesh* Mesh, float WpoUntil, float CullFrom, float CullTo);
	void FillCell(UHierarchicalInstancedStaticMeshComponent* K, const FIntPoint& C, float PerM2, float S0, float S1, int32 Seed);
	void ApplyOn();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMesh> Near;
	UPROPERTY() TObjectPtr<UStaticMesh> Mid;
	UPROPERTY() TObjectPtr<UStaticMesh> Tuft;
	UPROPERTY() TMap<FIntPoint, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> NearCells;
	UPROPERTY() TMap<FIntPoint, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> MidCells;
	UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> FarRows;
	TMap<FIntPoint, FPatch> Patches;
	FBox2D Field;                 // world cm
	float FarY = 0;               // the far tufts laid row by row from the start, a few each frame
	bool bLastOn = true;
	bool bLastAway = false;
};
