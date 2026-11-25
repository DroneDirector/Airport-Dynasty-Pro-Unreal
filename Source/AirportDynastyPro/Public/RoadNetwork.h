// The road network (owner, 2026-11-14; HANDOVER 3b, to-do 55): the terminal's forecourt, the 7.8 km road to town and
// the town's streets, built with MetaRoad Free (udrive, Fab Standard License) when the game starts - never placed by
// hand: Data/roads.json lists every road (its path in airport plan metres, its lanes left and right of that path, each
// with a type - Driving, Bus, Parking, Sidewalk, Shoulder... - and a width), made from the owner's map plans by
// Tools/make_roads.py so the whole network can be built again at any time. Each road is laid on our own ground
// (AAirfield::GroundAt, every few metres), right-hand traffic, in the airport's own asphalt and concrete.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoadNetwork.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API ARoadNetwork : public AActor
{
	GENERATED_BODY()
public:
	ARoadNetwork();
	virtual void Tick(float Dt) override;
	/** Builds every road in Data/roads.json (a road marked "test" only with -RoadTest). Returns how many were built. */
	/** Reads Data/roads.json and gets the build ready; the roads themselves are built by BuildSome. Returns how many. */
	int32 Build();
	/** Builds roads until Budget seconds have gone (at least one road), then the paint, car parks and corners after
	 *  the last; true when everything is built. */
	bool BuildSome(double Budget);
	/** 0..1, how much of the network is built. */
	float Progress() const;
	/** Editor only: builds every road once and saves its meshes as finished models in /Game/Roads/Baked, with
	 *  Data/roads-baked.json (each road's fingerprint and its parts); the game then places unchanged roads from them
	 *  instead of building (owner, 2026-11-15: the start's loading bar stalled on every road). A road added or changed
	 *  later is built at run time until the next bake (Tools/bake_roads.sh). Returns the roads saved. */
	UFUNCTION(BlueprintCallable, Category = "ADP")
	static int32 BakeAll(UObject* WorldContext);
	/** The ground's height (m) at a plan point: the airfield's own inside its boundary, the globe's outside it (measured by the
	 *  -RoadGroundProbe run into Data/road-ground.json: Cesium's terrain, which falls away from the airport toward the coast),
	 *  blended over the last 60 m inside the boundary so there is no step at the fence. */
	static double GroundAt(double X, double Y);
	/** A few placed things the -UIRoadAirport test looks at (world cm): a gully grating and a manhole cover, and which way out. */
	/** The coach stands (owner, 2026-11-19): each lettered stand box's middle on the road's surface, facing the way the coaches
	 *  drive in - filled as the forecourt's paint is laid. */
	static TArray<FTransform> BusStands;
	/** Every zebra crossing: its two ends on the ground (world), for the walks (the coaches' stress walk). */
	static TArray<TPair<FVector, FVector>> ZebraCrossings;
	static FVector TestDrain;
	static FVector TestDrainOut;
	static FVector TestManhole;

private:
	UPROPERTY() TObjectPtr<class UMetaRoadRuntimeBuildHost> Host;
	UPROPERTY() TArray<TObjectPtr<AActor>> Roads;
	/** The saved roads' meshes (their shadows off when far away). */
	TArray<TWeakObjectPtr<class UStaticMeshComponent>> FlatParts;
	TArray<FString> RoadIds;   // (each built road's id, beside Roads)
	UPROPERTY() TObjectPtr<class AStreetLights> Lights;
	UPROPERTY() TObjectPtr<class ARoadSigns> Signs;   // the roadside airport signs (RoadSigns.h)
	UPROPERTY() TMap<FString, TObjectPtr<class UInstancedStaticMeshComponent>> Props;   // the roads' fittings by model: gully gratings, manhole covers
	UPROPERTY() TObjectPtr<class UProceduralMeshComponent> Zebras;   // the zebra crossings' white bars (all roads)
	int32 ZebraSections = 0;   // every road's street lights (StreetLights.h)
};
