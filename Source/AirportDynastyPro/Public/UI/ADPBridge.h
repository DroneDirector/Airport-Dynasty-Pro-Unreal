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
	/** New game, Continue or Load game: the loading screen is up - Unreal loads the rest of the world behind it. */
	UFUNCTION() void StartLoad();
	UFUNCTION() void OpenTerminal();
	UFUNCTION() void StressWalk();
	/** The title screen's Airport site: Unreal's own world round the terminal, standing at the kerb outside. */
	UFUNCTION() void OpenSite();
	/** The title screen's Lounge: the Babylon edition's business lounge, in Unreal. */
	UFUNCTION() void OpenLounge();
	/** The title screen's town demos (owner, 2026-11-15): outside a town building, 25-30 m off, looking at it - "dealership", "villa". */
	UFUNCTION() void OpenPlace(const FString& Id);
	/** The 3D viewer: Unreal's own, following a flight (its positions come through ViewerFlight). */
	UFUNCTION() void OpenViewer();
	/** Where the followed flight is (the game's 3D viewer feed, JSON {me: {...}}), four times a second. */
	UFUNCTION() void ViewerFlight(const FString& Json);
	/** The airline, every few seconds while the game runs: {name, icao, hub, money} (JSON). */
	UFUNCTION() void GameState(const FString& Json);
	/** The departures and arrivals at your airport, as the game's timetable screens show them (JSON). */
	UFUNCTION() void GameFids(const FString& Json);
	/** The announcer's words as sound, made by the menus (24 kHz, 16-bit, base64). */
	UFUNCTION() void SpeechPcm(int32 Id, const FString& B64);
	/** The game's natural voices aren't on this PC: Unreal speaks the line with Windows' own female voice. */
	UFUNCTION() void SpeechFail(int32 Id);
	/** The announcer's line as Kokoro's sound numbers (JSON: a list per sentence); Unreal speaks it with the natural voice. */
	UFUNCTION() void Kokoro(int32 Id, const FString& IdsJson);
	/** A crew / pilot / controller line as Kokoro's sound numbers (CrewVoice.cpp asked for it). */
	UFUNCTION() void KokoroCrew(int32 Id, const FString& IdsJson);
	/** A timetable board's picture (PNG, base64), drawn by the menus. */
	/** A line from the menus for the game's log (their errors; Unreal's browser doesn't pass on the page's console). */
	UFUNCTION() void Log(const FString& Line);
	/** The title screen: up or gone, and its music clock (seconds); Unreal draws its film then (TitleBg.h). Returns
	 *  whether Unreal can (else the page draws it itself). */
	UFUNCTION() bool TitleBg(bool bOn, float Seconds);
	/** The menus' Settings > Updates on the PC: look for a newer PC version / install the one found (the game's own
	 *  updater; its state comes back to the page through window.__ueUpdate). */
	/** The menus' Exit (title screen and game): the game quits (the page can't close Unreal's window itself). */
	/** The airline game's save slots as files (Saved/Saves/<name>) on the PC: written, or removed when Data is empty. */
	UFUNCTION() bool SaveWrite(const FString& Name, const FString& Data);
	/** A save slot's file, or empty. */
	UFUNCTION() FString SaveRead(const FString& Name);
	UFUNCTION() void Quit();
	/** The title screen's gear: Unreal's own Settings panel, the same as in the airport, over the menus. */
	UFUNCTION() void OpenSettings();
	UFUNCTION() void CheckUpdates();
	UFUNCTION() void InstallUpdates();
	UFUNCTION() void Board(const FString& Key, const FString& B64);
	/** The phone or a computer opened over the 3D room has been put away (ue.adp.devicedone(kind)). */
	UFUNCTION() void DeviceDone(const FString& Kind);
	/** A button on a jetway's HMI page (ue.adp.hmi(action, arg)), to-do 115. */
	UFUNCTION() void Hmi(const FString& Action, const FString& Arg);
	/** The Overview from your tablet or a computer (ue.adp.overview(kind)), and its logo back (overviewback) - to-do 117. */
	UFUNCTION() void Overview(const FString& Kind);
	UFUNCTION() void OverviewBack(const FString& Kind);
	/** The tablet's wallpaper for its 3D screen (ue.adp.wallpaper(kind, jpeg data url)). */
	UFUNCTION() void Wallpaper(const FString& Kind, const FString& DataUrl);
	/** The Esc question in the 3D world answered "stay" (ue.adp.back3d()). */
	UFUNCTION() void Back3D();
	/** The wardrobe menu over the room (ue.adp.wardrobe): 'category' <name>, 'wear' <outfit>, 'close'. */
	UFUNCTION() void Wardrobe(const FString& Action, const FString& Arg);
	/** The title screen's New game: Unreal's "who are you" stage first (CharacterCreate), the page's overlay over it. */
	UFUNCTION() void NewGame();
	/** The stage's overlay (ue.adp.whoAreYou): 'hover' she|he, 'choose' she|he, 'cancel', 'done'. */
	UFUNCTION() void WhoAreYou(const FString& Action, const FString& Arg);
	/** The simulation for the menus (ue.adp.sim(action, arg) - to-do 176; the native Overview uses the same calls later):
	 *  'crew' (your people, the hiring pool, what your fleet needs, JSON), 'hire' id|base, 'dismiss' id, 'pay' id|weekly,
	 *  'base' id|icao, 'train' id|type, 'dayoff' id|days, 'interview' id, 'advert' role, 'contract' id|full/part,
	 *  'leave' index|yes/no, 'fleet' (your aircraft and their checks), 'check' reg|A/C/D, 'parts' usd. Returns JSON ({ok, ...}). */
	UFUNCTION() FString Sim(const FString& Action, const FString& Arg);
};
