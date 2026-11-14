// The roadside airport signs (owner, 2026-11-14): a blue sign on two galvanised posts at the right-hand side of the road,
// facing the cars driving to the airport, telling its name and how far it is (Art/RoadSign, Tools/set_road_sign.py).
// The name is never in the picture (owner: a copy of the airport placed anywhere must show its own name): each sign's
// face is composed when the game starts - the base picture, then the airport's name, the local word for airport and the
// distance, written in our own letters (Data/sign-glyphs.json) from Data/airfield.json's "airport".
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoadSigns.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API ARoadSigns : public AActor
{
	GENERATED_BODY()

public:
	ARoadSigns();
	/** A sign standing at At (world, cm, on the ground between its posts), its face looking along Face (flat), Km to go. */
	bool AddSign(const FVector& At, const FVector& Face, const FString& Km);
	/** The airport's name and the local word for airport ("Flugvöllur" in Iceland ... "Airport" where English is spoken). */
	static void AirportWords(FString& Name, FString& Word);
	/** The last sign placed (the -UIRoad test's camera looks at it). */
	static FVector TestAt;
	static FVector TestFace;

private:
	bool Ready();
	class UTexture* Compose(const FString& Km);
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<class UStaticMesh> Mesh;
	UPROPERTY() TObjectPtr<class UTexture2D> Base;
	UPROPERTY() TObjectPtr<class UTexture2D> Glyphs;
	UPROPERTY() TObjectPtr<class UMaterialInterface> FaceMat;
	UPROPERTY() TArray<TObjectPtr<class UTextureRenderTarget2D>> Faces;
	TMap<FString, TObjectPtr<class UTextureRenderTarget2D>> ByKm;
	TSharedPtr<class FJsonObject> GlyphData;
	int32 FaceSlot = INDEX_NONE;
};
