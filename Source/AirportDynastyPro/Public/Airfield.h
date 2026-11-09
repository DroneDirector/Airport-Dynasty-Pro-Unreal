// Your airport's airfield (Data/airfield.json, from the Android edition's airfield plan, the same axes and origin as
// the terminal): the grass inside the boundary, the aprons, runway 08/26 with its markings, the taxiways with their
// yellow centre lines, the holding positions and the cargo building. The data is shared with the traffic.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Airfield.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

#pragma region Airfield data

struct AIRPORTDYNASTYPRO_API FAirfieldData
{
	struct FTwy { FString Id, Kind; TArray<FVector2f> Pts; float W = 25; };
	struct FStand { FString Id; float X = 0, Nose = 0, Dir = -1, Span = 36, Len = 44.5f; bool bJetway = false, bCargo = false; };
	struct FRect { float X0, Y0, X1, Y1; };

	float RwyX0 = -1919, RwyX1 = 2081, RwyY = 646, RwyW = 60;
	float AY = 386, BY = 906, TY = 206, KY = 1086;
	TArray<FTwy> Taxiways;
	TArray<FStand> Stands;
	TArray<FVector2f> Holds;
	TArray<FRect> Aprons;
	FRect Boundary{-2000, -170, 2240, 1320}, Cargo{200, 1231, 590, 1301};
	double Lat = 63.457988, Lon = -18.713703;
	float ElevFt = 130, Heading = 68;

	bool Load();
	const FTwy* Find(const FString& Id) const { return Taxiways.FindByPredicate([&](const FTwy& T) { return T.Id == Id; }); }
};

#pragma endregion

UCLASS()
class AIRPORTDYNASTYPRO_API AAirfield : public AActor
{
	GENERATED_BODY()

public:
	AAirfield();
	/** Lays out the airfield; with bModelSite the Babylon model already has the site round the terminal (aprons, kerb). */
	bool Build(bool bModelSite = false);
	const FAirfieldData& Data() const { return D; }

private:
	#pragma region Pieces
	UInstancedStaticMeshComponent* Kit(const FName Mat);
	/** A flat slab between two corners (plan x, y on the airfield) from Bottom to Top metres. */
	void Slab(const FName Mat, float X0, float Y0, float X1, float Y1, float Bottom, float Top);
	/** A strip along a line (a taxiway, a marking), W wide, its top at Top metres. */
	void Strip(const FName Mat, const FVector2f& A, const FVector2f& B, float W, float Top, float Thick = 0.02f);
	#pragma endregion

	FAirfieldData D;
	float Sink = 0; // with the Babylon model: the whole airfield 25 cm lower, so the model's own ground always wins
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Kits;
};
