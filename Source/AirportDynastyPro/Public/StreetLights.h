// The roads' street lights (owner, 2026-11-14): the Faroese swan-neck street light (Art/StreetLight,
// Tools/set_street_light.py) along the roads that have "lights" in Data/roads.json - both sides at the airport, one side
// on the approach, none from about 1 km out (RoadNetwork.cpp places them). Drawn as instances (one draw for all, Nanite),
// each with a light under its head pointing down on the road; switched as real street lights are, by a light sensor: on
// at sunset, off just after sunrise, and on in fog and storms (as the apron's masts, LightMasts.h), the LED faces glowing.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StreetLights.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API AStreetLights : public AActor
{
	GENERATED_BODY()

public:
	AStreetLights();
	/** A post standing at At (world, cm, its foot), its arm reaching out along Toward (world, flat). False if the model is missing. */
	bool AddPost(const FVector& At, const FVector& Toward);
	int32 Num() const { return Count; }
	virtual void Tick(float Dt) override;

private:
	bool Ready();
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Posts;
	UPROPERTY() TArray<TObjectPtr<class USpotLightComponent>> Lights;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Faces;
	FVector2D ArmLocal = FVector2D(0, 1);   // the model's arm direction (from its bounds), and its reach (cm)
	float Reach = 140.f, HeadZ = 812.f;
	int32 Count = 0;
	bool bOn = false;
};
