// The Chablais bus station's lights and its platform clock: Data/busstation-lights.json (Art/BusStation, all in
// model-local metres) says where every fitting sits. The deck's disc lamps hang under the vault and light the
// platform, the walk and the coaches at the bays; the uplights wash the vault's ribs along their own aim; the hall's
// ceiling panels light the waiting hall. All of it goes through the model's own lamp system
// (ABabylonModel::AddLamp: no shadows, no flares, no haze, each with its reach and a draw distance, dimmed by day
// with the model's other lamps). The lamp faces themselves get the kit's glowing "lamp" material.
// The two clock faces over the middle of the deck (west and east) show the game's own time: each pair of hands sits
// on its own boss and is turned every second from the site's clock (or the hour the game overrides it with), so a
// sped-up day moves them; nothing of the world is searched for in Tick.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BusStationLife.generated.h"

class ABabylonModel;

UCLASS()
class AIRPORTDYNASTYPRO_API ABusStationLife : public AActor
{
	GENERATED_BODY()
public:
	ABusStationLife();
	/** The bus station's lights and clock on its model (once, after the model stands). */
	static ABusStationLife* Make(ABabylonModel* Model);
	virtual void Tick(float Dt) override;

private:
	/** One clock hand: its group in the model, the boss it turns on, which way round its face reads, and whether it
	 *  is the minute hand. */
	struct FHand
	{
		int32 Group = INDEX_NONE;
		FVector Pivot = FVector::ZeroVector;
		float Sense = 1.f;   // (+1 or -1: the face's own clockwise, from which way it looks)
		bool bMinute = false;
	};
	TArray<FHand> Hands;
	TWeakObjectPtr<ABabylonModel> Model;
	float ShownHour = -1.f;   // (the time the hands stand at, so they are only moved when it changes)
};
