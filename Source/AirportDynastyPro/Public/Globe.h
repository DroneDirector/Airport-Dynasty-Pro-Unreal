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

	UPROPERTY() TObjectPtr<class ACesium3DTileset> Earth;
	UPROPERTY() TObjectPtr<class UCesiumUrlTemplateRasterOverlay> Imagery;
	/** The land cover's sharper North Atlantic layer (the Faroes and Iceland), over the world one. */
	UPROPERTY() TObjectPtr<class UCesiumUrlTemplateRasterOverlay> Detail;
	static TWeakObjectPtr<AGlobe> Current;
	void SetImagery(int32 S);
	void Probe(int32 S);

	/** The airfield's boundary (plan metres): the terrain is cut away inside it, its own ground is the airfield's. */
	FBox2D Airfield = FBox2D(ForceInit);
	int32 PawnOnTerrain = -1;

public:
	AGlobe();
	virtual void Tick(float Dt) override;
	/** Places the globe under the airport. */
	void Build(const FAirfieldData& Field);

	#pragma region The satellite pictures
	// Free sources that cover the whole Earth and allow a paid game, each with its credit:
	// 0 Sentinel-2 cloudless 2016 by EOX (10 m, CC BY 4.0), 1 NASA Landsat global mosaic (30 m) and 2 NASA Blue Marble
	// (500 m), both from NASA's GIBS (NASA's imagery is free to use, with credit). The player's choice is kept; at start
	// the chosen source is tried, and when it refuses (EOX answers 403 since 2026-11-09) the next one is used.
	static int32 Source;
	static FString SourceName(int32 S);
	static FString SourceCredit(int32 S);
	/** Use this source now (and keep the choice). */
	static void UseSource(int32 S);
	/** What the game says about the pictures (the source in use, or that it fell back). */
	static FString Note;
	/** The source whose pictures are on the globe now (for the credit line). */
	static int32 ShownSource;
	#pragma endregion
};
