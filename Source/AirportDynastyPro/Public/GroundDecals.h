// Ground decals (to-do 22a, the cheap tier): marks on the airport's paving from the airfield's own layout - oil stains
// under each passenger stand's engines and nose gear, rubber streaks in the runway's touchdown zones, repair patches
// on the aprons. Our own patterns (Tools/make_ground_decals.py), each copy varied by its seed, the same every start.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GroundDecals.generated.h"

struct FAirfieldData;

UCLASS()
class AIRPORTDYNASTYPRO_API AGroundDecals : public AActor
{
	GENERATED_BODY()
public:
	AGroundDecals();
	void Place(const FAirfieldData& Field);
private:
	int32 Count = 0;
	void Add(class UMaterialInterface* M, float X, float Y, float SizeAlong, float SizeAcross, float Yaw, float Seed, float Strength);
};
