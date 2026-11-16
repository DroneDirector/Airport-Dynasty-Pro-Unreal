// Taxiway lights (to-do 40e, owner 2026-11-13 / -16): ICAO Annex 14 - blue elevated edge lights on frangible stems
// along every taxiway's edges (at most 60 m apart on straights, closer on curves), tiny green inset centreline lights
// (30 m on straights, 7.5 m on curves); none on the runway, none where another pavement joins. Our own fittings
// (Art/AirfieldLights), each kind one instanced Nanite list. Lit at night and in fog; dim by day.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AirfieldLights.generated.h"

struct FAirfieldData;

UCLASS()
class AIRPORTDYNASTYPRO_API AAirfieldLights : public AActor
{
	GENERATED_BODY()
public:
	AAirfieldLights();
	void Place(const FAirfieldData& Field);
	virtual void Tick(float Dt) override;
private:
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Edge;
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Centre;
	UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> Lenses;
	TArray<float> LensGlow;
	float Level = -1.f;
};
