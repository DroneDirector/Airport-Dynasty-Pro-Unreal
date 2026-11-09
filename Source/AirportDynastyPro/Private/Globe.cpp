// The globe: a Cesium georeference at the terminal's origin, turned to the runway's heading; the Earth's ellipsoid as a
// tileset with the Sentinel-2 cloudless 2016 imagery on it.
#include "Globe.h"
#include "CesiumGeoreference.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Cesium3DTileset.h"
#include "CesiumUrlTemplateRasterOverlay.h"
#include "Engine/World.h"

#pragma region Placing

void AGlobe::Build(const FAirfieldData& F)
{
	UWorld* W = GetWorld();
	if (!W) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// the terminal's origin on the Earth: the plan's x runs true 068° (the runway), its y true 338°; the runway's middle
	// (x 81, y 646) is at the airport's coordinates, so the origin is 166.9 m east and 629.3 m south of it
	const double H = FMath::DegreesToRadians(double(F.Heading));
	const double MidX = (F.RwyX0 + F.RwyX1) / 2, MidY = F.RwyY;
	const double East = -MidX * FMath::Sin(H) - MidY * FMath::Sin(H - HALF_PI);
	const double North = -MidX * FMath::Cos(H) - MidY * FMath::Cos(H - HALF_PI);
	const double Lat = F.Lat + North / 111320.0;
	const double Lon = F.Lon + East / (111320.0 * FMath::Cos(FMath::DegreesToRadians(F.Lat)));

	ACesiumGeoreference* G = W->SpawnActor<ACesiumGeoreference>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (!G) return;
	// the terrain that ships with the game (Tools/MakeTerrain.py): its layer.json in Content/Terrain (a release), or the
	// converter's output on this PC; the airport's ground height (above the WGS84 ellipsoid) comes with it, so Unreal's
	// ground (z = 0) sits on it. Without terrain: the smooth globe 1 m under the airport.
	FString Layer = FPaths::ProjectContentDir() / TEXT("Terrain/layer.json");
	if (!FPaths::FileExists(Layer)) Layer = TEXT("F:/ADP-Terrain/tiles/layer.json");
	const bool bTerrain = FPaths::FileExists(Layer);
	double Ground = -1.0;
	{
		FString Text;
		TSharedPtr<FJsonObject> J;
		for (const FString& F2 : {FPaths::ProjectContentDir() / TEXT("Data/terrain.json"), FPaths::ProjectDir() / TEXT("Data/terrain.json")})
			if (bTerrain && FFileHelper::LoadFileToString(Text, *F2) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid())
			{ Ground = J->GetNumberField(TEXT("airportHeight")); break; }
	}
	// the terrain's flat airport 40 cm under Unreal's own ground (no flicker where they meet)
	G->SetOriginLongitudeLatitudeHeight(FVector(Lon, Lat, Ground >= 0 ? Ground + 0.4 : 1.0));
	// Unreal's x is east on the globe; turned so it runs along the runway (068°, 22° north of east)
	G->SetActorRotation(FRotator(0, 90.0 - F.Heading, 0));

	ACesium3DTileset* Earth = W->SpawnActor<ACesium3DTileset>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (!Earth) return;
	Earth->SetGeoreference(G);
	if (bTerrain && Ground >= 0)
	{
		Earth->SetTilesetSource(ETilesetSource::FromUrl);
		Earth->SetUrl(TEXT("file:///") + FPaths::ConvertRelativePathToFull(Layer));
	}
	else Earth->SetTilesetSource(ETilesetSource::FromEllipsoid);

	// Sentinel-2 cloudless 2016 by EOX (CC BY 4.0; only this 2016 layer: the later ones are non-commercial)
	UCesiumUrlTemplateRasterOverlay* Imagery = NewObject<UCesiumUrlTemplateRasterOverlay>(Earth, TEXT("Sentinel2Cloudless2016"));
	Imagery->TemplateUrl = TEXT("https://tiles.maps.eox.at/wmts/1.0.0/s2cloudless_3857/default/g/{z}/{y}/{x}.jpg");
	Imagery->Projection = ECesiumUrlTemplateRasterOverlayProjection::WebMercator;
	Imagery->RegisterComponent();
	Earth->AddInstanceComponent(Imagery);
	UE_LOG(LogTemp, Log, TEXT("Globe placed: origin %.6f N, %.6f E, runway %.0f°"), Lat, Lon, F.Heading);
}

#pragma endregion
