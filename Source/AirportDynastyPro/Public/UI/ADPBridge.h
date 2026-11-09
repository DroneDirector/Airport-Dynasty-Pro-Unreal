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
	/** The title screen's Lounge: the Babylon edition's business lounge, in Unreal. */
	UFUNCTION() void OpenLounge();
	/** The airline, every few seconds while the game runs: {name, icao, hub, money} (JSON). */
	UFUNCTION() void GameState(const FString& Json);
	/** The departures and arrivals at your airport, as the game's timetable screens show them (JSON). */
	UFUNCTION() void GameFids(const FString& Json);
	/** The announcer's words as sound, made by the menus (24 kHz, 16-bit, base64). */
	UFUNCTION() void SpeechPcm(int32 Id, const FString& B64);
	/** The game's natural voices aren't on this PC: Unreal speaks the line with Windows' own female voice. */
	UFUNCTION() void SpeechFail(int32 Id);
	/** A timetable board's picture (PNG, base64), drawn by the menus. */
	/** A line from the menus for the game's log (their errors; Unreal's browser doesn't pass on the page's console). */
	UFUNCTION() void Log(const FString& Line);
	UFUNCTION() void Board(const FString& Key, const FString& B64);
};
