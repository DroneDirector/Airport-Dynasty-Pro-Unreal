// Our own land along the roads outside the airfield (owner, 2026-11-14), as the airfield's own ground inside it: the
// globe's terrain is drawn coarser far off (its levels differ by up to 8 m over the hills toward Vik) and a road laid on
// it floated or sank with distance. Here the globe is cut away in a band along the roads (AGlobe, from the band's "cut")
// and this actor draws the land itself, always at full detail: Data/land-corridor.json/.bin/.png from
// Tools/make_land_corridor.py - heights every 2.5 m from the finest terrain tiles, the globe's land-cover colours. The
// roads take their heights from it (ARoadNetwork::GroundAt) and it is shaped under them (a few cm below their surface,
// easing back to the land beside them), so they lie on it exactly. Its edge is hidden: a wandering outline, its last
// metres eased to the globe's coarser ground, a skirt down under its rim.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LandCorridor.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API ALandCorridor : public AActor
{
	GENERATED_BODY()
public:
	ALandCorridor();
	/** The band's natural ground (m, the game's z) at a plan point, if the point is on it. */
	static bool HeightAt(double X, double Y, double& Z);
	/** A road's surface along its middle (plan m, z m, its half width m), sent by the road network as it builds. */
	static void AddRoadSample(double X, double Y, double Z, double HalfWidth, bool bKerbEdge = false);
	/** Draws the band (after the roads are built: it is shaped under them). Returns its triangles. */
	int32 Build();
	/** The band's outline cut out of the globe (plan m), for AGlobe. */
	static const TArray<TArray<FVector2D>>& GlobeCut();

	virtual void Tick(float Dt) override;

private:
	void ShowColours();
	void PlantRim(const void*);
	UPROPERTY() TObjectPtr<class UProceduralMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Mat;
	UPROPERTY() TMap<int32, TObjectPtr<class UTexture2D>> Colours;   // the band's colours per globe picture (Settings > Imagery)
	UPROPERTY() TObjectPtr<class UHierarchicalInstancedStaticMeshComponent> Plants;   // the grass clumps along its rim
	TArray<TPair<FVector, FVector>> Rim;   // the rim's sides: middle, and a point inward (world cm)
	int32 Shown = -1;
};
