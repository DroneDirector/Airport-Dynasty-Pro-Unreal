// The operations room's monitor (owner, 2026-11-11): the one that matches the player's own screen, so it looks like
// the screen they play on: a 49" 32:9 super-ultrawide for super-ultrawide screens, a 34" 21:9 ultrawide for ultrawide
// ones and a 27" 16:9 widescreen for the rest. All three are high-detail models (Art/Monitor/make_monitors.py, Nanite,
// instanced), placed on the desk; the one shown follows the game window's shape (a change of resolution swaps it).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OpsMonitors.generated.h"

class ABabylonModel;

UCLASS()
class AIRPORTDYNASTYPRO_API AOpsMonitors : public AActor
{
	GENERATED_BODY()
public:
	AOpsMonitors();
	/** The three monitors, here on the desk (the screens' middle over this actor, facing along its +x). */
	void Build();
	virtual void Tick(float Dt) override;
	/** Which monitor a screen of this shape (width / height) gets: 0 the 16:9, 1 the 21:9, 2 the 32:9. */
	static int32 KindFor(float Aspect);
	/** The monitor shown now (its screen is the one the game's picture goes on). */
	ABabylonModel* Shown() const { return Kinds.IsValidIndex(Current) ? Kinds[Current].Get() : nullptr; }
private:
	UPROPERTY() TArray<TObjectPtr<ABabylonModel>> Kinds;
	int32 Current = -1;
};
