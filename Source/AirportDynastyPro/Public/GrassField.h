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
	/** H: the ground every metre where the normal grass grows; L: where the low lawn grows instead (the forecourt's flat
	 *  grass - its island, the roundabout's middle, the lawns beside it; owner, 2026-11-28). Lowest where neither. */
	struct FPatch { TArray<float> H; TArray<float> L; int32 Rows = 0; };
	bool LawnAt(float X, float Y, float& Z) const;
	void FillLawn(UHierarchicalInstancedStaticMeshComponent* K, const FIntPoint& C, const FVector& From, float Reach);
	int32 LawnClumps = 0; bool bLawnLogged = false;
	/** Measures more of a patch's ground (a row at a time) until the deadline; true once it is all known. */
	bool PatchStep(const FIntPoint& C, double Deadline);
	static constexpr float Cell = 2500.f;    // cm
	const FPatch& Patch(const FIntPoint& C);
	bool GroundAt(float X, float Y, float& Z) const;
	UHierarchicalInstancedStaticMeshComponent* NewLayer(UStaticMesh* Mesh, float WpoUntil, float CullFrom, float CullTo);
	void FillCell(UHierarchicalInstancedStaticMeshComponent* K, const FIntPoint& C, float PerM2, float S0, float S1, int32 Seed, const FVector& From = FVector::ZeroVector, float Reach = 0.f);
	void ApplyOn();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMesh> Near;
	UPROPERTY() TObjectPtr<UStaticMesh> Mid;
	UPROPERTY() TObjectPtr<UStaticMesh> Tuft;
	UPROPERTY() TMap<FIntPoint, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> NearCells;
	UPROPERTY() TMap<FIntPoint, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> MidCells;
	/** The far grass round the camera (owner, 2026-11-15: "much higher draw distance ... very low quality far off"): the
	 *  airfield's still tufts, sparse and wide, on 100 m patches out to FarReach - from the air the grass goes on. */
	UPROPERTY() TMap<FIntPoint, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> FarCells;
	// each far piece's spacing band (owner, 2026-11-15: the same amount of grass from every height - make it cheaper
	// some other way): 0 = a tuft every 4 m (to 1 km), 1 = every 8 m at twice the size (to 1.8 km), 2 = every 12 m at three
	// times (beyond) - the same ground covered by a quarter to a ninth as many tufts, which is what costs (Nanite pays per
	// tuft, not per triangle, for tufts a pixel or two wide)
	TMap<FIntPoint, int32> FarBand;
	static int32 FarBandAt(float DistanceM) { return DistanceM < 1000.f ? 0 : DistanceM < 1800.f ? 1 : 2; }
	void FillFarCell(UHierarchicalInstancedStaticMeshComponent* K, const FIntPoint& C, const FVector& From, float Reach, const TArray<float>& Grid);
	void FillMidCell(UHierarchicalInstancedStaticMeshComponent* K, const FIntPoint& C, const FVector& From, float Reach, const TArray<float>& Grid);
	/** The patch being laid now (its ground measured a little each frame: never a stall): layer 0 near, 1 middle, 2 far. */
	struct FJob { int32 Layer = -1; FIntPoint C; TArray<float> Grid; int32 Done = 0; float Reach = 0; FVector From = FVector::ZeroVector; };
	FJob Job;
	float CamHeight = 0.f; double CamHeightAt = 0.0;
public:
	/** How far the far grass reaches (m): on the ground FarReachGround, rising with the camera's height to FarReach. */
	static float FarReach;
	static float FarReachGround;
private:
	UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> FarRows;
	TMap<FIntPoint, FPatch> Patches;
	// (big pieces: each is one component Unreal manages every frame - 650 to 1150 small ones cost 8-10 ms a frame on the
	// game thread, 2026-11-15)
	static constexpr float FarCell = 40000.f;   // cm (the far grass's pieces: 400 m)
	static constexpr int32 FarGrid = 21;        // its ground measured on a 21 x 21 grid (20 m apart)
	static constexpr float MidCell = 10000.f;   // cm (the middle grass's pieces: 100 m)
	static constexpr int32 MidGrid = 41;        // its ground on a 41 x 41 grid (2.5 m apart)
	FBox2D Field;                 // world cm
	float FarY = 0;               // the far tufts laid row by row from the start, a few each frame
	bool bLastOn = true;
	bool bLastAway = false;
};
