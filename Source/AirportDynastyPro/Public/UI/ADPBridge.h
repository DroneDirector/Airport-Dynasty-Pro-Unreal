// The link from the app's menus (in the web view) to Unreal: the menus call window.ue.adp.<function>() and Unreal does
// it here, e.g. the title screen's Terminal opens Unreal's own 3D terminal instead of the Android edition's.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ADPBridge.generated.h"

class AADPHUD;

UCLASS()
class AIRPORTDYNASTYPRO_API UADPBridge : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<AADPHUD> Hud;

	/** The title screen's Terminal: Unreal's own 3D terminal, the menus hidden (G or Esc brings them back). */
	UFUNCTION() void OpenTerminal();
	/** The title screen's Airport site: Unreal's own world round the terminal, standing at the kerb outside. */
	UFUNCTION() void OpenSite();
	/** The airline, every few seconds while the game runs: {name, icao, hub, money} (JSON). */
	UFUNCTION() void GameState(const FString& Json);
	/** The departures and arrivals at your airport, as the game's timetable screens show them (JSON). */
	UFUNCTION() void GameFids(const FString& Json);
};
