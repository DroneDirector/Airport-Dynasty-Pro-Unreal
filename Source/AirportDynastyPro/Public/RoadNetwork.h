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
	/** Builds every road in Data/roads.json (a road marked "test" only with -RoadTest). Returns how many were built. */
	int32 Build();

private:
	UPROPERTY() TObjectPtr<class UMetaRoadRuntimeBuildHost> Host;
	UPROPERTY() TArray<TObjectPtr<AActor>> Roads;
	UPROPERTY() TObjectPtr<class AStreetLights> Lights;
	UPROPERTY() TObjectPtr<class UProceduralMeshComponent> Zebras;   // the zebra crossings' white bars (all roads)
	int32 ZebraSections = 0;   // every road's street lights (StreetLights.h)
};
