// The sea round the airport: a water surface meeting the real coastline (Tools/MakeOcean.py, from ESA WorldCover),
// placed on the globe through Cesium's georeference (so it lies exactly where the satellite pictures show the sea and
// follows the Earth's curve), at sea level. Its material (Tools/set_ocean.py) moves the swell and the ripples with the
// wind and breaks the waves into foam along the shore. Shown only near the ground: from higher up (where Cesium draws
// coarser terrain, which the exact sea would cover along low coasts) the terrain's own water mask draws the sea.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ocean.generated.h"

class ACesiumGeoreference;

UCLASS()
class AIRPORTDYNASTYPRO_API AOcean : public AActor
{
	GENERATED_BODY()

public:
	AOcean();
	/** The sea at SeaHeight metres above the WGS84 ellipsoid. */
	bool Build(ACesiumGeoreference* G, double SeaHeight);
	virtual void Tick(float Dt) override;

private:
	UPROPERTY() TObjectPtr<class UProceduralMeshComponent> Water;
	double SeaZ = 0;
};
