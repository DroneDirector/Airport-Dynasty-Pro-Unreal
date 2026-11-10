// The apron light masts: one in front of each of the 8 gates, on the side away from where the jetways turn (owner's
// layout, 2026-11-10), 30 m galvanised masts with 8 LED floodlights and a striped guardrail round the foot
// (Art/LightMast, Tools/set_light_mast.py). Placed as instances (one draw for all 8): the mast itself with Nanite, its
// thin parts (cables, the lightning rod, U-bolts) without. Lit as real aprons are: a light sensor switches them on at
// sunset and off just after sunrise (and on in fog and storms); a light from each floodlight, its beam visible in the
// evening haze, and the LED faces glowing.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LightMasts.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API ALightMasts : public AActor
{
	GENERATED_BODY()

public:
	ALightMasts();
	void Build();
	virtual void Tick(float Dt) override;

	/** Where they stand, in the terminal plan's metres (x along the building, z out onto the apron). */
	static const TArray<FVector2D>& Spots();
	/** How far the beams show in the haze, metres (Settings' slider, 60-500; the haze's own reach). */
	static float BeamDistance;
	/** The slider moved: the haze's reach set again while the masts are lit. */
	static void Refresh();

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Masts;
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Thin;
	UPROPERTY() TArray<TObjectPtr<class USpotLightComponent>> Lights;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Faces;
	TArray<int32> LightMast;    // which mast each light belongs to (they switch on one mast after another)
	bool bOn = false;
	static TWeakObjectPtr<ALightMasts> Current;
	float Since = 0.f;
};
