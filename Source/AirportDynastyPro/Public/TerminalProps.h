// The terminal's placed props, from data (owner, 2026-11-14: "it will be moved later, so place it from data"):
// Data/terminal-props.json lists each one - its model, where it stands (plan metres x along the building, z into it
// from the landside front, its floor), which way it faces (heading: 0 = facing the landside entrances, 90 = facing
// east/+x...) and the clear zone it keeps (m in front, at the sides and behind). Moving one, or adding another kiosk,
// is changing numbers. Passengers walk round each one's clear zone (ATerminalCrowd's obstacles).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerminalProps.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API ATerminalProps : public AActor
{
	GENERATED_BODY()

public:
	ATerminalProps();
	/** Places every prop in Data/terminal-props.json. Returns how many. */
	int32 Build();
	/** Where each prop stands (world cm) and which way its front looks, by id (the tests' cameras look at them). */
	static TMap<FString, TPair<FVector, FVector>> Placed;

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TMap<FString, TObjectPtr<class UInstancedStaticMeshComponent>> ByMesh;
};
