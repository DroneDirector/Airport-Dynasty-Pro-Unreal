// The PC version's interface. The airline game itself (G: every menu and screen, the shared world) full screen over
// the terminal; and the walking-mode interface, as in the Android edition: the holographic map in the corner (M or a click opens it full
// screen with routes), a clean top-left bar (Map, Settings, Hide UI), Hide UI (H; a double-click or Esc brings it
// back), the settings panel (Tab: quality, render scale, frame limit, VSync, the performance overlay, scan my PC, the
// problem log) and the performance overlay (P). On the very first start the PC is scanned and the quality set from it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TerminalPlan.h"
#include "ADPHUD.generated.h"

class SHoloMap;
class SWidget;
class SWebBrowser;
class FADPPadToMenus;

UCLASS()
class AIRPORTDYNASTYPRO_API AADPHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** An airport announcement: the gong, then its words as a caption (the Kokoro voice reads them later). Queued so
	 *  calls never overlap. */
	/** Kind: 1 flight calls (boarding, final call, gate closed, delays), 2 passenger calls (paging by name), 3 information
	 *  (general notices). Each kind has its own volume in Settings, under the overall announcements volume. */
	static void Announce(const FString& Words, uint8 Kind = 1);

	/** From the menus: Unreal's 3D terminal, the menus hidden. */
	void ShowTerminal();
	/** From the menus: Unreal's world round the terminal, standing at the kerb outside the main entrance. */
	void OpenSite();
	/** From the menus: the business lounge (the Babylon edition's, placed apart from the airport, lit as there). */
	void OpenLounge();
	/** The menus are in front (the gamepad then works them). */
	bool MenusOpen() const { return bGame && !bHidden; }
	/** Y / Triangle in the menus: back to the 3D airport. */
	void LeaveMenus() { if (bGame) ToggleGame(); }
	/** The airline from the game: its name and money for the top bar. */
	void OnGameState(const FString& Json);
	/** The game's departures: a change to boarding, final call or a delay is announced in the terminal. */
	void OnGameFids(const FString& Json);
	/** The announcer's words as sound (24 kHz, 16-bit, base64), made by the menus; played through the loudspeakers. */
	void SpeechPcm(int32 Id, const FString& B64);
	/** Without the game's voices: Windows' own female voice (SAPI), made off the game thread, then the same loudspeakers. */
	void SpeechFallback(int32 Id);
	void PlayAnnouncer(int32 Id, const TArray<int16>& Pcm);
	/** -UIPack: one of each call, as heard in the main hall, to Saved/AnnouncementPack.wav. */
	void WritePreviewPack();

private:
	#pragma region Panels
	TSharedRef<SWidget> MakeTopBar();
	TSharedRef<SWidget> MakeSettings();
	TSharedRef<SWidget> VolumeSlider(int32 I);
	TSharedRef<SWidget> MakePerf();
	void ToggleMap();
	void ToggleSettings();
	void TogglePerf();
	/** The airline game itself (the shared world's screens), full screen over the terminal. */
	void ToggleGame();
	void HideUI();
	void ShowUI();
	/** The mouse free for the interface (map full screen, settings) or captured for looking round. */
	void UpdateMouse();
	void FirstStartScan();
	/** E: a desk's or a timetable's screen in front of you opens the airline game on it. */
	void Interact();
	void PlayGong();
	/** F1 (keyboard) or View / Create (gamepad): the Buttons panel open or minimized. */
	void ToggleButtons();
	/** LB / L1: hide the interface, or bring it back. */
	void ToggleHide() { if (bHidden) ShowUI(); else HideUI(); }
	/** Any key or button: keyboard and mouse or the gamepad in use; the symbols shown follow it. */
	void OnAnyKey(FKey Key);
	/** The Buttons panel's list, the controls in Settings and the map's legend, for the device in use. */
	void RefreshControls();
	/** The middle mouse button: the pointer free (click things, drag boxes about) or back to looking round. */
	void TogglePointer();
	bool bPointer = false, bFpsBox = false, bFrameGen = false;
	/** AMD's frame generation (FSR, any graphics card) on or off; kept in the settings file, off unless chosen. */
	void SetFrameGen(bool bOn);
	TSharedPtr<class SDragBox> FpsBox;
	#pragma endregion

	TSharedPtr<SWidget> Root;
	TSharedPtr<SWebBrowser> Browser;
	#pragma region Announcements (Announcements.cpp)
	struct FLatePassenger { FString Name; float ArriveAt = 0; bool bRun = false, bBoarded = false; };
	struct FDeparture { FString Place, Status; int32 Sched = -1, Exp = -1, Gate = 1; bool bBoarding = false, bPaged = false, bFinal = false, bClosed = false; TArray<FLatePassenger> Late; };
	TMap<FString, FDeparture> Departures;
	FString Airline;
	FTerminalPlan Plan;
	UPROPERTY() TObjectPtr<class UAudioComponent> VoiceOut;
	UPROPERTY() TObjectPtr<class UAudioComponent> GongOut;
	int32 SpeechId = 0;
	FString SpokenText;
	float HeardGain = 1.f, CallsClock = 0, SpeakIn = -1;
	/** How well the loudspeakers are heard where you stand (1 in the public areas, faint in offices, 0 in toilets). */
	float ZoneGain() const;
	/** How the loudspeakers sound where you stand: the highest frequency that comes through (walls muffle them). */
	float ZoneMuffle() const;
	/** The room kind where you stand ("" outside, "lounge" in the business lounge). */
	FString ZoneKind() const;
	/** The volumes: 0 all announcements, 1 flight calls, 2 passenger calls, 3 information; 0..1, kept in the settings file. */
	float Vol[4] = {1, 1, 1, 1};
	float NextInfoAt = -1;
	int32 InfoTurn = 0;
	float CallGain() const { return (bGame ? 0.f : 1.f) * HeardGain * Vol[0] * Vol[FMath::Clamp<int32>(CallKind, 1, 3)]; }
	void Speak(const FString& Words);
	void RunCalls();
	FString AirlineName() const;
	#pragma endregion
	UPROPERTY() TObjectPtr<class ABabylonModel> Lounge;
	bool bInLounge = false;
	/** First start: the quality the PC scan suggests (asked, never set by itself); -1 when there's nothing to ask. */
	int32 SuggestedQuality = -1;
	void AnswerQuality(bool bYes);
	FTransform BeforeLounge;
	TSharedPtr<FADPPadToMenus> PadToMenus;
	FString StatusLine;
	TMap<FString, FString> DepStatus;
	bool bPadSent = false;
	TSharedPtr<SHoloMap> HoloMap;
	TSharedPtr<SWidget> MiniBox, FullBox, Settings, Perf, TopBar, GameBox;
	bool bHidden = false, bSettings = false, bPerf = false, bGame = false, bGameMade = false;
	float FpsSmooth = 60.f;
	TSharedPtr<SWidget> CaptionBox;
	TSharedPtr<class STextBlock> Credit;
	TSharedPtr<class SVerticalBox> ButtonsList, SettingsControls;
	TSharedPtr<class SBox> ButtonsHeaderGlyph;
	bool bButtonsOpen = false, bShownPad = true;
	uint8 ShownPadKind = 255;
	FString CallText;
	float CaptionTime = 0;
	static TArray<TPair<FString, uint8>> Calls;
	static TWeakObjectPtr<AADPHUD> Current;
	uint8 CallKind = 1;
	UPROPERTY() TObjectPtr<class USoundWaveProcedural> Gong;
	UPROPERTY() TObjectPtr<class UADPBridge> Bridge;
};
