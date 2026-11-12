// The runway windsocks (owner, 2026-11-11): one near each end of runway 08/26, placed as real airports place them
// (ICAO Annex 14): 300 m in past the landing threshold, on the pilots' left as they land, outside the runway strip and
// clear of the taxiway, seen from the approach and from the holding points. A 6 m mast on a free swivel
// (Art/WindSock, Tools/set_wind_sock.py; the masts instanced with Nanite), the sock 3.6 m long in five orange and
// white bands. The sock turns its mouth into the wind (its tail points downwind) and stands out as far as the wind
// is strong: each band takes about 3 knots, straight out from 15 knots, hanging limp in calm. It flutters and sways
// more in gusts. Lit at night (floodlights round the top, the red light above).
// The wind itself is kept here too (WindFrom / WindKt), from the weather theme for now; the real weather feed sets it
// later, and anything else that needs the wind (smoke, flags, aircraft) reads it from here.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindSocks.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class USpotLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class AIRPORTDYNASTYPRO_API AWindSocks : public AActor
{
	GENERATED_BODY()

public:
	AWindSocks();
	void Build();
	virtual void Tick(float Dt) override;

	#pragma region The wind
	/** Where the wind blows from (degrees true, as in a METAR) and how strong it is right now (knots, gusts included). */
	static float WindFrom;
	static float WindKt;
	/** The gusts' share right now (0 steady .. 1 in the strongest gust): the socks flutter more. */
	static float Gustiness;
	#pragma endregion

private:
	struct FSock
	{
		FVector Foot;                 // the mast's foot (world)
		float Yaw = 0, YawSpeed = 0;  // the swivel (world yaw, degrees) and how fast it is turning
		float Phase = 0;              // its own flutter phase, so the two never move in step
		UProceduralMeshComponent* Mesh = nullptr;
	};
	void UpdateWind(float Dt);
	void DrawSock(FSock& S, bool bFirst);

	TArray<FSock> Socks;
	float Clock = 0;
	bool bLit = false;
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Masts;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Frames;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> SockMeshes;
	UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Lamps;   // four per mast, one from each floodlight
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Lens;
};
