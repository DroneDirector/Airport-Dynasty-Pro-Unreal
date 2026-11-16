// The terminal's LED advert screens (to-do 52): big bright LED cabinets where real airports hang them - the check-in
// hall's end walls and the walls between its entrances, the airside lobby, the Food Walk, along the walkway to the
// gates and in each gate's waiting area - showing our own made-up brands (Art/Adverts), each advert slowly moving,
// wiping over to the next every 10 s or so (each screen its own order and timing). Each spot is a point in a room and
// the way to its wall: the screen goes where a look that way meets the terminal's wall (the model's own walls, so it
// follows the building). One instanced Nanite list for all (custom data: advert A, advert B, the wipe).
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Adverts.generated.h"

struct FTerminalPlan;

UCLASS()
class AIRPORTDYNASTYPRO_API AAdverts : public AActor
{
	GENERATED_BODY()
public:
	AAdverts();
	void Build(const FTerminalPlan& Plan);
	virtual void Tick(float Dt) override;
	/** Where each screen is (its face's middle) and the way it faces: the tests look at them. */
	TArray<TPair<FVector, FVector>> Faces;
private:
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Screens;
	struct FShow { int32 A = 0, B = 1; float Clock = 0.f, Hold = 10.f; };
	TArray<FShow> Shows;
};
