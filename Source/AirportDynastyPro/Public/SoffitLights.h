// The light under the terminal's wave roof, outside and in its tall halls (owner, 2026-11-23: the shade under the roof was
// black, the front black at night). By day the sky's light that really reaches under a 12 m high open roof - the game's
// sky light gives nothing (its real-time capture comes out black, everywhere), so it is given here, under this roof only;
// by night the roof's own downlights (their fittings glow: ROOF/DOWNLIGHT) light the paving, the promenade and the halls.
// A few long soft panels, one per 40 m, under the roof's lowest point and above every wall: no shadows, no flares, no haze,
// nothing on the glass, a distance limit; the halls' ones off by day. Console: adp.SoffitDay / adp.SoffitNight (1 = as built).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoffitLights.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API ASoffitLights : public AActor
{
	GENERATED_BODY()

public:
	ASoffitLights();
	void Build();
	virtual void Tick(float Dt) override;

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<class URectLightComponent>> Lights;
	struct FPanel { float DayCd = 0.f, NightCd = 0.f; };
	TArray<FPanel> Panels;
	float Check = 0.f, LastDay = -1.f, LastNight = -1.f;
	bool bOn = false;
};
