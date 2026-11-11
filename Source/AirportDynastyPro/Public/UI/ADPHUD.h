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
	/** The 3D viewer (Unreal's own): following a flight of the game; the menus hide. Closed by Esc or G (FlightViewer.cpp). */
	void OpenViewer();
	void CloseViewer();
	void OnViewerFlight(const FString& Json);
	bool bViewer = false;
	UPROPERTY() TObjectPtr<class AFlightViewer> Viewer;
	/** Esc: the open panel closes; with nothing open, the Overview (the menus handle Esc themselves). */
	void OnEscape();
	/** Settings (Tab, the bar's button, or the title screen's gear in the menus). */
	void ToggleSettings();
	/** Bloom and blur (Settings): 0 off, 1 low, 2 medium, 3 high, 4 epic; kept in the game's settings file. */
	void SetBloom(int32 Level);
	void SetBlur(int32 Level);
	int32 BloomLevel = 4, BlurLevel = 4;
	TSharedRef<SWidget> LevelRow(bool bBloomRow);
	#pragma region The updater (Updater.cpp)
	static FString UpdaterVersion();
	static FString LastUpdateProblem();
	#pragma region Crash report (CrashReport.cpp)
	/** After a crash, on the next start: what went wrong (Unreal's crash record: the error, where in the program, the
	 *  version, what you were doing), shown over the title screen, with a button that writes it as an e-mail to
	 *  crashlog@airportdynasty.com in your own mail program (nothing is sent without you), and one that opens the folder. */
	TSharedRef<SWidget> MakeCrashPanel();
	/** The unstuck panel (Unstuck.cpp): stuck for 2 s, a panel; one click, free floor nearby. */
	TSharedRef<SWidget> MakeUnstuckPanel();
	bool IsStuck(float Dt);
	void Unstick();
	TSharedPtr<SWidget> UnstuckPanel;
	/** The loading screen into airport mode (Loading.cpp). */
	TSharedRef<SWidget> MakeLoading();
	void StartLoading();
	void StopLoading();
	void TickLoading(float Dt);
	TSharedPtr<SWidget> LoadingBox;
	/** Your phone and the computers in the rooms (Devices.cpp, owner 2026-11-11): the game's own phone (its apps) and
	 *  desktop (the taskbar, the operations screens as apps) over the 3D room. Opened from the room (E, the controller's
	 *  X, or a click on it), put away with Esc or the legend's button; the phone is picked up off the desk and put back. */
public:
	void OpenDevice(const FString& Kind);
	/** Takes up the phone nearest a point (the device test, -UIDevice). */
	void PickPhoneNear(const FVector& At);
	void DeviceDone(const FString& Kind);
private:
	bool TryDevice(const FHitResult& Hit);
	void TickDevice(float Dt);
	void PutDeviceAway();
	void MakeGameView();
	TSharedRef<SWidget> MakeDeviceLegend();
	TSharedPtr<SWidget> DeviceLegend;
	FString DeviceKind, DevicePending;
	bool bDevice = false;
	/** The phone you pick up: on the desk (0) to held up in front of you (1); moving up (+1), down (-1) or still (0). */
	TWeakObjectPtr<class ABabylonModel> Phone;
	FTransform PhoneHome;
	float PhoneK = 0.f;
	int32 PhoneDir = 0;
public:
	bool bLoading = false, bWasGame = true;
	float LoadT = 0, LoadFrac = 0;
	FString LoadStep;
	int32 ShadersMax = 1, TexMax = 1;
	float StuckFor = 0;
	bool FindNewCrash();
	void SendCrashMail();
	TSharedPtr<SWidget> CrashPanel;
	FString CrashText, CrashFolder, CrashId;
	/** What you were doing, kept on disk as it changes (a crash leaves no log in the released game). */
	static void Breadcrumb(const FString& What);
	#pragma endregion
	void CheckForUpdates();
	void InstallUpdate();
	void DownloadNext(int32 I, const FString& Dir);
	void ApplyUpdate(const FString& Dir);
	FString UpdateStatus;
	/** The download's progress (0..1, -1 when none) and speed (bytes a second), for the bar and the menus. */
	float UpdateFrac = -1.f;
	double UpdateSpeed = 0, UpdateT0 = 0;
	uint64 UpdateGot0 = 0;
	bool UpdateBusy = false;
	/** What the menus' Settings > Updates was last told (sent again only when it changes). */
	FString UpdateSent;
	TArray<TPair<FString, FString>> UpdateChain;   // version, update package
	TArray<FString> UpdateFiles;
	FString UpdateFullUrl;
	#pragma endregion
	/** The middle mouse button: the pointer free (click things, drag boxes about) or back to looking round. */
	void TogglePointer();
	/** From the menus: Unreal's world round the terminal, standing at the kerb outside the main entrance. */
	void OpenSite();
	/** The map's three shortcuts (and Airport site): you are put on free floor at a spot in the plan (metres: x, the
	 *  floor's height, z), looking at another; never inside a wall or the floor. */
	void GoTo(const FVector& PlanAt, const FVector& PlanLook);
	void GoGate4();
	void GoApron();
	void GoEntrance();
	void GoTowerCab();
	void GoTowerBalcony();
	void GoTowerFoot();
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
	/** A timetable board's picture (PNG, base64) from the menus, for every screen of that kind (Boards.cpp). */
	void OnBoard(const FString& Key, const FString& B64);
	/** Without the game's voices: Windows' own female voice (SAPI), made off the game thread, then the same loudspeakers. */
	void SpeechFallback(int32 Id);
	/** The announcer's line as the voice model's sound numbers (JSON, one list per sentence): Kokoro in Unreal (Kokoro.cpp). */
	void KokoroSpeak(int32 Id, const FString& IdsJson);
	void PlayAnnouncer(int32 Id, const TArray<int16>& Pcm);
	/** -UIPack: one of each call, as heard in the main hall, to Saved/AnnouncementPack.wav. */
	void WritePreviewPack();

private:
	#pragma region Panels
	TSharedRef<SWidget> MakeTopBar();
	TSharedRef<SWidget> MakeSettings();
	TSharedRef<SWidget> VolumeSlider(int32 I);
	TSharedRef<SWidget> WeatherButtons();
	TSharedRef<SWidget> MakePerf();
	void ToggleMap();
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
	bool bPointer = false, bFpsBox = false, bFrameGen = false;
	/** AMD's frame generation (FSR, any graphics card) on or off; kept in the settings file, off unless chosen. */
	void SetFrameGen(bool bOn);
	TSharedPtr<class SDragBox> FpsBox;
	#pragma endregion

	/** Settings' walk-anywhere places (WalkAnywhere.h). */
	TSharedRef<SWidget> WalkPlaces();
	TSharedPtr<SWidget> Root;
	TSharedPtr<SWebBrowser> Browser;
	#pragma region Announcements (Announcements.cpp)
	struct FLatePassenger { FString Name; float ArriveAt = 0; bool bRun = false, bBoarded = false; };
	struct FDeparture { FString Place, Status; int32 Sched = -1, Exp = -1, Gate = 1; bool bBoarding = false, bPaged = false, bFinal = false, bClosed = false; TArray<FLatePassenger> Late; };
	TMap<FString, FDeparture> Departures;
	FString Airline;
	FTerminalPlan Plan;
	UPROPERTY() TMap<FString, TObjectPtr<class UTexture2D>> BoardTex;
	TMap<FString, TArray<TObjectPtr<class UMaterialInstanceDynamic>>> BoardMats;
	/** Indoors: shadows lifted (only about a quarter dark), blended in when you are inside the terminal or the lounge. */
	UPROPERTY() TObjectPtr<class APostProcessVolume> Indoor;
	float IndoorWeight = 0;
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
