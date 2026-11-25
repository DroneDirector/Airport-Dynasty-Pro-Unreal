// The PC version's interface. The airline game itself (G: every menu and screen, the shared world) full screen over
// the terminal; and the walking-mode interface, as in the Android edition: the holographic map in the corner (M or a click opens it full
// screen with routes), a clean top-left bar (Map, Settings, Hide UI), Hide UI (H; a double-click or Esc brings it
// back), the settings panel (Tab: quality, render scale, frame limit, VSync, the performance overlay, scan my PC, the
// problem log) and the performance overlay (P). On the very first start the PC is scanned and the quality set from it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TerminalPlan.h"
#include "HAL/PlatformProcess.h"
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
	static void Announce(const FString& Words, uint8 Kind = 1, bool bFirst = false);   // (bFirst: before anything waiting; the airport's other calls: UI/AirportCalls.h)

	/** From the menus: Unreal's 3D terminal, the menus hidden. */
	void ShowTerminal();
	/** The stress walk from the title screen (StressWalk.h): into the terminal, the walk, back to the title with its report. */
	void StartStressWalk();
	/** The Settings panel's frame: categories on the left, the chosen one's options on the right (neon). */
	TSharedRef<SWidget> MakeSettingsFrame(TSharedRef<class SVerticalBox> CatGraphics, TSharedRef<class SVerticalBox> CatSound, TSharedRef<class SVerticalBox> CatWorld, TSharedRef<class SVerticalBox> CatControls, TSharedRef<class SVerticalBox> CatUpdates);
	int32 SettingsCat = 0;
	bool bStressFromTitle = false;
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
	/** The office wardrobe (Wardrobe.h): its doors open and the outfits you own listed; one picked stands you beside it. */
	void OpenWardrobe(class AWardrobe* W);
	void CloseWardrobe();
	/** What you clicked in the page's wardrobe menu (through the bridge). */
	void WardrobeFromPage(const FString& Action, const FString& Arg);
	/** New game: the "who are you" stage (the page's overlay over Unreal's neon room), then the game's own new-game flow. */
	void OpenWhoAreYou();
	void WhoAreYouFromPage(const FString& Action, const FString& Arg);
	void CloseWhoAreYou(bool bGoOn, const FString& Difficulty = FString());
	bool bWhoAreYou = false, bWhoAreYouCreator = false, bWhoAreYouBack = false;
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
	/** New game, Continue, Load: the loading screen from the first moment (the world and your career, NewGameLoad.cpp), then
	 *  straight into the airport, the bar carrying on (Loading.cpp). */
	void ShowStartLoad();
	void EndStartLoad();
	bool bStartLoad = false, bLoadCarryOn = false;
	float StartLoadT = 0.f;
	/** Nothing of the airport's own interface: the new career's steps, the site question, the loading (owner, 2026-11-23). */
	bool OwnScreen() const;
	/** You really are in the airport: no title, menus, site question, creator or loading (to its last second) in front. Only
	 *  then the airport's own interface shows (owner, 2026-11-23, to-do 227). */
	bool InAirport() const;
	TSharedPtr<SWidget> HudBox;      // the airport's interface, the one layer
	bool bMiniAway = false;          // the holographic map away for a test's pictures
	/** -UINewGame: the title -> new game -> steps -> loading -> airport path walked by itself (NewGameTest.cpp). */
	void TestNewGame(float Dt);
	/** -UIShot: the test pictures by subject, called from Tick in this order (Drives: true = nothing more this frame). */
	void TickShotsBuildings(float Dt, const float Before, float& ShotClock);
	void TickShotsGround(float Dt, const float Before, float& ShotClock);
	void TickShotsJetways(float Dt, const float Before, float& ShotClock);
	void TickShotsRides(float Dt, const float Before, float& ShotClock);
	void TickShotsScreens(float Dt, const float Before, float& ShotClock);
	void TickShotsLooks(float Dt, const float Before, float& ShotClock);
	bool TickShotsDrives(float Dt, const float Before, float& ShotClock);
	void TickShotsHalls(float Dt, const float Before, float& ShotClock);
	/** Tests: the interface away (as H). */
	void TestHide() { if (!bHidden) HideUI(); }
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
	TSharedRef<SWidget> MakePanelHint();
	TSharedPtr<SWidget> DeviceLegend;
	FString DeviceKind, DevicePending;
	bool bDevice = false;
	/** The jetway's HMI (to-do 115): E at its console's screen - the camera glides up close (its path checked so it never
	 *  passes through the cab), then the HMI page opens exactly over the screen; Esc: the camera back to you. */
	void TickHmi(float Dt);
	int32 HmiJ = -1; float HmiOpenIn = -1.f, HmiSend = 0.f, HmiBackIn = -1.f;
	TWeakObjectPtr<class ACameraActor> HmiCam; bool bHmiPointer = false;
	void TickTablet(float Dt);
	TWeakObjectPtr<class ABabylonModel> Tablet;
	float TabWant = 0.f, TabK = 0.f, TabCloseWant = 0.f, TabCloseK = 0.f, BlurK = 0.f, TabHomeLeft = 0.f; bool bTabAway = false;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> TabScreenMat;
	UPROPERTY() TObjectPtr<class UTexture2D> TabWallTex;
	FString OverviewReturn, HintText; float HintLeft = 0.f;
	TSharedPtr<SWidget> HintBox;
public:
	void OpenHmi(int32 J);
	void EndHmi();
	/** Your tablet (Tablet.cpp, to-do 117): Tab / the pad's D-pad Down takes it up into your hands or puts it away;
	 *  TabletUse (E while holding it) brings it up close and its screen opens. */
	void ToggleTablet();
	/** The phone or the tablet in your hands (held, on its way, or up close): the game's own UI steps aside, only the
	 *  Buttons legend stays (owner, 2026-11-18, to-do 120). */
	bool HoldingDevice() const;
	/** Esc / the pad's Circle while you hold or use your phone or tablet: always put it all the way down (owner, 2026-11-19). */
	void DevicesDown();
	bool TabletUse();
	/** Its 3D screen's picture (a JPEG data URL from its screen; empty: just make sure it has its material). */
	void TabletScreen(const FString& DataUrl);
	void TabletScreenClosed();
	/** The Overview only from your tablet and the computers (owner, 2026-11-18); its logo brings you back there. */
	void OverviewFrom(const FString& Kind);
	void OverviewBack();
	/** G / the pad's Y: no longer the Overview from the 3D world (a note instead); from the menus back into 3D. */
	void MenusKey();
	/** A short note low in the middle of the screen for a few seconds. */
	void Hint(const FString& Text);
	/** The Esc question in the 3D world answered "stay": back into the 3D view. */
	void Back3D();
	/** Up close at a jetway's monitor (the live HMI on it takes the mouse). */
	bool AtHmi() const { return HmiJ != INDEX_NONE; }
	/** A button pressed on the jetway's HMI page (ue.adp.hmi): DOCK, UNDOCK, STOP ... */
	void HmiCommand(const FString& Action, const FString& Arg);
private:
	/** The phone you pick up: on the desk (0) to held up in front of you (1); moving up (+1), down (-1) or still (0). */
	TWeakObjectPtr<class ABabylonModel> Phone;
	FTransform PhoneHome;
	float PhoneK = 0.f;
	int32 PhoneDir = 0;
	/** The phone in your hand (owner, 2026-11-19; docs/Device-Hold-Plan.md): held (its screen live on the 3D one), the
	 *  view free (middle mouse) or the "+" pointer on its screen (the default), the pointer (0..1 across and down), the
	 *  thumb's press on the side button (1 just pressed .. 0), the screen's corners last sent to the page. */
	bool bPhoneHeld = false, bPhoneLook = false;
	float PhU = 0.5f, PhV = 0.5f, PhThumb = 0.f;
	float PhQuad[8] = {};
	FString PhPtrSent;
	void PhoneHand(class ABabylonModel* P, float HandReach);
	void TestStrollers(float Before, float ShotClock);
	void TestCoaches(float Before, float ShotClock);
	void TestChablais(float Before, float ShotClock);
	void TestCoachUse(float Before, float ShotClock);
public:
	/** The phone's side button (+1 down the list, -1 up): the thumb presses it, a click, the highlight steps. */
	void PhoneSide(int32 Dir);
	void PhoneSideUp() { PhoneSide(-1); }
	void PhoneSideDown() { PhoneSide(1); }
	/** The mouse wheel over the phone in your hand: a finger's swipe on its screen. */
	void PhoneWheelUp();
	void PhoneWheelDown();
private:
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
	/** The update's download in progress (to-do 163): curl straight to disk, carrying on from what is there - never again
	 *  from the start, even after the game was closed; watched here each frame (its size on disk is the progress). */
	void TickUpdateDownload(float Dt);
	FProcHandle UpdateProc; int32 UpdateIdx = -1, UpdateTries = 0; FString UpdateDir, UpdateFile; float UpdatePoll = 0.f;
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
	TArray<int64> UpdateSizes;                      // each package's size from the release listing (the bar never loses its length)
	double UpdateSentAt = 0;
	bool bMenusReady = false;
	/** "Where is your airport?" (SSiteChoice.h, Site.h): shown first in a new career when there is more than one site. */
	TSharedPtr<class SSiteChoice> SiteChoice;
	bool bSitePicked = false;     // (this new career's site is chosen: go on to "who are you")
	float ResumeNewGameAt = -1.f; // (the world loaded again for the chosen site: the new career goes on once the menus are up)
	void OpenSiteChoice();
	void CloseSiteChoice();
	bool bBootSent = false; float BootT = 0.f;   // the start's loading screen told everything is in
	bool bMenusPending = false; int32 MenusWaitFrames = 0;   // the title screen, opened after the loading frame                       // the menus' page has loaded and said so                         // when the page was last told (progress at most 5 times a second)
	TArray<FString> UpdateFiles;
	FString UpdateFullUrl;
	#pragma endregion
	/** The middle mouse button: the pointer free (click things, drag boxes about) or back to looking round. */
	void TogglePointer();
	/** From the menus: Unreal's world round the terminal, standing at the kerb outside the main entrance. */
	void OpenSite();
	/** Outside a town building (TownBuildings.h), from the title screen's demos or the quick-starts. */
	void OpenTownPlace(const FString& Id);
	/** The map's three shortcuts (and Airport site): you are put on free floor at a spot in the plan (metres: x, the
	 *  floor's height, z), looking at another; never inside a wall or the floor. */
	void GoTo(const FVector& PlanAt, const FVector& PlanLook);
	void GoGate4();
	void GoTaxFree();
	void GoGate13();
	void GoClaim();
	/** Drone mode (DroneCam.h): from the full-screen map's button; Esc / B back to where you stood. */
	void EnterDrone();
	void LeaveDrone();
	bool InDrone() const { return Drone.IsValid(); }
	TWeakObjectPtr<class ADroneCam> Drone;
	TWeakObjectPtr<APawn> DroneFrom;
	FRotator DroneFromLook = FRotator::ZeroRotator;
	bool bButtonsBeforeDrone = false;
	/** The fire station's bay doors: within 25 m their button (O / D-pad right) works and shows in the Buttons panel. */
	void ToggleBayDoors();
	TSharedPtr<SWidget> PanelHint;   // the door panel's controls, low in the middle of the screen
	bool bNearPanel = false;
	bool bNearStation = false;
	/** The wash hose: in your hand, or within reach of its hook. */
	bool bHoseHeld = false, bNearHose = false;
	/** A seat within reach, or sitting on one (Seats.h). */
	bool bNearSeat = false, bSeated = false;
	bool bNearDoor = false, bDoorOpen = false;
	// the new career's loading screen: what Unreal itself loads, with its real progress (UI/NewGameLoad.cpp, to-do 32h)
	void StartUnrealLoad();
	void TickUnrealLoad(float Dt);
	TArray<TSharedPtr<struct FStreamableHandle>> PreLoad;
	TArray<FString> PreNames;
	float PreSent = 0.f; bool bPreLoading = false, bPreDone = false;   // at a hinged door (BabylonModel UseDoor): the Buttons panel's open / close
	void GoApron();
	void GoEntrance();
	TSharedPtr<class SCompassStrip> Compass;
	/** A field-of-view slider for the settings panel (-1: all views; 0..3: one view - ADPFov.h). */
	TSharedRef<SWidget> FovSlider(int32 View);
	/** Pose.cpp: the New Game start spot, your place kept for Continue / Load. */
	void GoNewGameStart(); void SavePose(); bool RestorePose(); void OpenSiteStart();
	FString SimDifficulty;   // (the difficulty chosen at New game, for the simulation's new career - to-do 173)
	bool bNewCareer = false, bPoseLive = false; float PoseLeft = 5.f;
	void GoTowerCab();
	void GoTowerBalcony();
	void GoTowerFoot();
	/** Outside hangar I (as listed in Data/airfield.json "hangars"): on the apron in front of it, on its lead-in line,
	 *  looking straight at its doors; with bInside, in the hall looking out; with bLounge, in its annex's crew lounge.
	 *  One shortcut per hangar in the full-screen map, so every hangar added later has one. */
	void GoHangar(int32 I, int32 Where = 0);
	/** The fire station's places in the full map: 0 outside straight on, 1 outside from the side, 2 inside by the trucks. */
	void GoFireStation(int32 Where);
	bool bFireOnMap = false;
	bool bHangarsOnMap = false;
	TArray<FVector2D> HangarAt;      // each hangar's centre line and the apron edge (plan metres)
	TArray<FString> HangarSize;
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
	/** Its picture on its screens (decoded in the background by OnBoard). */
	void ShowBoard(const FString& Key, int32 Wd, int32 Ht, TArray<uint8> Bgra);
	TMap<FString, double> BoardSought;   // (when a board's screens were last looked for in vain)
	/** Without the game's voices: Windows' own female voice (SAPI), made off the game thread, then the same loudspeakers. */
	void SpeechFallback(int32 Id);
	/** The announcer's line as the voice model's sound numbers (JSON, one list per sentence): Kokoro in Unreal (Kokoro.cpp). */
	void KokoroSpeak(int32 Id, const FString& IdsJson);
	/** A crew line to Kokoro's sound numbers (the menus' pronunciation): answered through UADPBridge::KokoroCrew. */
	bool CrewIds(int32 Id, const FString& Words);
	/** The announcer's line through the menus' pronunciation - only when the game's own (G2P.cpp) can't. */
	void SpeakWeb(int32 Id);
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
	// the jetway's control panel menu (E at its console): which jetway, open or not (JetwayRigs.h)
	bool bJetMenu = false; int32 JetMenuJ = -1; bool bJetPointerWas = false;
	bool bPeopleMenuWas = false;
	TSharedPtr<class SBorder> TalkPanel;
	TSharedPtr<class SVerticalBox> PeopleChoices; void BuildPeopleChoices();   // (the people menu was open last frame: the mouse follows it)
	void OpenJetMenu(int32 J); void CloseJetMenu();
	int32 NearJetway(float MaxCm) const;
	/** AMD's frame generation (FSR, any graphics card) on or off; kept in the settings file, off unless chosen. */
	void SetFrameGen(bool bOn);
	/** Our own frame generation on or off - live while it's in place (ADPFrameGen::Installed), else at the next start. */
	void SetOwnFrameGen(bool bOn);
	TSharedPtr<class SDragBox> FpsBox;
	#pragma endregion

	/** Settings' walk-anywhere places (WalkAnywhere.h). */
	TSharedRef<SWidget> WalkPlaces();
	TSharedPtr<SWidget> Root;
	TSharedPtr<SWebBrowser> Browser;
	#pragma region Announcements (Announcements.cpp)
	struct FLatePassenger { FString Name; float ArriveAt = 0; bool bRun = false, bBoarded = false; };
	struct FDeparture { FString Flight, Place, Status, Who, PlanId; int32 Sched = -1, Exp = -1, Gate = 1; bool bBoarding = false, bPaged = false, bFinal = false, bClosed = false; TArray<FLatePassenger> Late; };
	TMap<FString, FDeparture> Departures;
	/** The departures from the simulation's real timetable (to-do 174) into Departures, when a career runs. */
	void SimCalls();
	/** The simulation's new notices, briefing and news to the phone's Messages (to-do 175). */
	void SimToPhone();
	/** F5 / the pad's View button: save now into a slot named by the date and place (to-do 192). */
	void SaveNow();
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
	bool bHidden = false, bSettings = false, bPerf = false, bGame = false, bGameMade = false, bWardrobe = false;
	float FpsSmooth = 60.f;
	TSharedPtr<SWidget> CaptionBox;
	TSharedPtr<SWidget> WardrobeBox;
	TSharedPtr<class SVerticalBox> WardrobeList;
	TSharedPtr<class SWrapBox> WardrobeTiles;
	FString WardrobeCategory;
	int32 WardrobeTicks = 0;
	void FillWardrobe();
	TWeakObjectPtr<class AWardrobe> WardrobeOpen;
	TSharedRef<SWidget> MakeWardrobe();
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
