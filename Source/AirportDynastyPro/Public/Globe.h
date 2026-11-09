// The globe round your airport (Cesium for Unreal, Apache 2.0): the whole Earth, the airport placed at its real spot
// (the runway's middle at 63.457988 N, 18.713703 W) and turned so the runway runs true 068°, with the free Sentinel-2
// cloudless 2016 imagery by EOX (CC BY 4.0, credited on screen). The mountains come from the free elevation data
// (Copernicus GLO-30 or the AWS terrain tiles) once it has been converted and hosted; until then the globe is smooth
// and the airport's own level ground (the grass inside its boundary) lies just above it, so nothing flickers.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Airfield.h"
#include "Globe.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API AGlobe : public AActor
{
	GENERATED_BODY()

public:
	/** Places the globe under the airport. */
	void Build(const FAirfieldData& Field);
};
