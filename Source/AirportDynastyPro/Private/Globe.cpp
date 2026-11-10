// The globe: a Cesium georeference at the terminal's origin, turned to the runway's heading; the Earth's ellipsoid as a
// tileset with the Sentinel-2 cloudless 2016 imagery on it.
#include "Globe.h"
#include "CesiumGeoreference.h"
#include "Ocean.h"
#include "Materials/MaterialInterface.h"
#include "TerrainPacks.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Cesium3DTileset.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
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
	// the world's terrain downloaded by the game (TerrainPacks.h), once every pack is in
	if (!FPaths::FileExists(Layer) && ADPTerrain::Ready()) Layer = ADPTerrain::Dir() / TEXT("layer.json");
	// the converter's output on this PC only when run from the editor (a release uses what it downloaded)
	if (!FPaths::FileExists(Layer) && !FPlatformProperties::RequiresCookedData()) Layer = TEXT("F:/ADP-Terrain/tiles-v2/layer.json");
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

	ACesium3DTileset* Tiles = W->SpawnActor<ACesium3DTileset>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (!Tiles) return;
	Tiles->SetGeoreference(G);
	if (bTerrain && Ground >= 0)
	{
		Tiles->SetTilesetSource(ETilesetSource::FromUrl);
		Tiles->SetUrl(TEXT("file:///") + FPaths::ConvertRelativePathToFull(Layer));
		// the sea everywhere the terrain's water mask says so (MakeTerrain.py marks it): Cesium's water material
		Tiles->SetEnableWaterMask(true);
		if (UMaterialInterface* Water = LoadObject<UMaterialInterface>(nullptr, TEXT("/CesiumForUnreal/Materials/Instances/MI_CesiumThreeOverlaysAndClippingAndWater.MI_CesiumThreeOverlaysAndClippingAndWater")))
			Tiles->SetMaterial(Water);
	}
	else Tiles->SetTilesetSource(ETilesetSource::FromEllipsoid);

	// the sea round the airport, meeting the real coastline, at sea level: with the shipped terrain, the airport's
	// ground height less its height above the sea; on the smooth globe, just above it
	if (AOcean* Sea = W->SpawnActor<AOcean>(FVector::ZeroVector, FRotator::ZeroRotator, P))
		Sea->Build(G, bTerrain && Ground >= 0 ? Ground - F.ElevFt * 0.3048 + 0.1 : 0.25);

	Earth = Tiles;
	Current = this;
	GConfig->GetInt(TEXT("ADP.Graphics"), TEXT("Imagery"), Source, GGameUserSettingsIni);
	Probe(FMath::Clamp(Source, 0, 2));
	UE_LOG(LogTemp, Log, TEXT("Globe placed: origin %.6f N, %.6f E, runway %.0f°"), Lat, Lon, F.Heading);
}

#pragma endregion

#pragma region The satellite pictures

int32 AGlobe::Source = 0;
int32 AGlobe::ShownSource = 0;
FString AGlobe::Note;
TWeakObjectPtr<AGlobe> AGlobe::Current;

namespace
{
	struct FSource { const TCHAR* Name; const TCHAR* Url; int32 MaxLevel; const TCHAR* Probe; const TCHAR* Credit; };
	const FSource Sources[] = {
		{TEXT("Sentinel-2 cloudless 2016 (EOX, 10 m)"), TEXT("https://tiles.maps.eox.at/wmts/1.0.0/s2cloudless_3857/default/g/{z}/{y}/{x}.jpg"), 15,
			TEXT("https://tiles.maps.eox.at/wmts/1.0.0/s2cloudless_3857/default/g/0/0/0.jpg"),
			TEXT("Sentinel-2 cloudless 2016 (s2maps.eu) by EOX IT Services GmbH, contains modified Copernicus Sentinel data 2016")},
		{TEXT("NASA Landsat global mosaic (30 m)"), TEXT("https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/Landsat_WELD_CorrectedReflectance_TrueColor_Global_Annual/default/GoogleMapsCompatible_Level12/{z}/{y}/{x}.jpg"), 12,
			TEXT("https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/Landsat_WELD_CorrectedReflectance_TrueColor_Global_Annual/default/GoogleMapsCompatible_Level12/0/0/0.jpg"),
			TEXT("Imagery: NASA Global Imagery Browse Services (GIBS), Landsat WELD (NASA / USGS)")},
		{TEXT("NASA Blue Marble (500 m)"), TEXT("https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/BlueMarble_NextGeneration/default/GoogleMapsCompatible_Level8/{z}/{y}/{x}.jpeg"), 8,
			TEXT("https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/BlueMarble_NextGeneration/default/GoogleMapsCompatible_Level8/0/0/0.jpeg"),
			TEXT("Imagery: NASA Global Imagery Browse Services (GIBS), Blue Marble Next Generation (NASA Earth Observatory)")},
	};
}

FString AGlobe::SourceName(int32 S) { return Sources[FMath::Clamp(S, 0, 2)].Name; }
FString AGlobe::SourceCredit(int32 S) { return Sources[FMath::Clamp(S, 0, 2)].Credit; }

void AGlobe::UseSource(int32 S)
{
	S = FMath::Clamp(S, 0, 2);
	GConfig->SetInt(TEXT("ADP.Graphics"), TEXT("Imagery"), S, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	if (AGlobe* G = Current.Get()) G->Probe(S);
}

void AGlobe::Probe(int32 S)
{
	// one small picture first: when the source refuses (or can't be reached), the next one is used
	Note = FString::Printf(TEXT("Trying %s…"), *SourceName(S));
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
	R->SetURL(Sources[S].Probe);
	R->SetVerb(TEXT("GET"));
	TWeakObjectPtr<AGlobe> Me(this);
	const int32 Wanted = FMath::Clamp(Source, 0, 2);
	R->OnProcessRequestComplete().BindLambda([Me, S, Wanted](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
	{
		AGlobe* G = Me.Get();
		if (!G) return;
		const bool bWorks = bOk && Res.IsValid() && Res->GetResponseCode() == 200;
		if (bWorks || S >= 2)
		{
			G->SetImagery(S);
			UE_LOG(LogTemp, Log, TEXT("Globe pictures: %s (asked for %s, answer %d)"), *SourceName(S), *SourceName(Wanted), Res.IsValid() ? Res->GetResponseCode() : 0);
			Note = S == Wanted || !bWorks ? FString::Printf(TEXT("Satellite pictures: %s."), *SourceName(S))
				: FString::Printf(TEXT("%s isn't answering, so the game uses %s."), *SourceName(Wanted), *SourceName(S));
			return;
		}
		G->Probe(S + 1);
	});
	R->ProcessRequest();
	Source = Wanted;
}

void AGlobe::SetImagery(int32 S)
{
	if (!Earth) return;
	if (Imagery) { Imagery->DestroyComponent(); Imagery = nullptr; }
	Imagery = NewObject<UCesiumUrlTemplateRasterOverlay>(Earth, FName(*FString::Printf(TEXT("Imagery%d"), S)));
	Imagery->TemplateUrl = Sources[S].Url;
	Imagery->Projection = ECesiumUrlTemplateRasterOverlayProjection::WebMercator;
	Imagery->MaximumLevel = Sources[S].MaxLevel;
	// the credits are in the game's own credit line (no text on top of the map); the switch has no setter, so by name
	if (FBoolProperty* Show = FindFProperty<FBoolProperty>(UCesiumRasterOverlay::StaticClass(), TEXT("ShowCreditsOnScreen"))) Show->SetPropertyValue_InContainer(Imagery, false);
	Imagery->RegisterComponent();
	Earth->AddInstanceComponent(Imagery);
	ShownSource = S;
}

#pragma endregion
