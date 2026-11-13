// The walking-mode interface: the holographic map, the top-left bar, Hide UI, the settings panel and the performance
// overlay, all in Slate, styled in the Android edition's holographic blues.
#include "UI/ADPHUD.h"
#include "FireTruck.h"
#include "DroneCam.h"
#include "UI/TitleBg.h"
#include "ADPLog.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "TowerADC.h"
#include "Hangar.h"
#include "FireStation.h"
#include "Airfield.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UI/SHoloMap.h"
#include "UI/SProgressText.h"
#include "UI/GameView.h"
#include "UI/ADPBridge.h"
#include "UI/InputGlyphs.h"
#include "UI/SDragBox.h"
#include "SunClock.h"
#include "Wardrobe.h"
#include "Engine/PostProcessVolume.h"
#include "Globe.h"
#include "ADPWalker.h"
#include "LightMasts.h"
#include "WalkAnywhere.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Components/VolumetricCloudComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#include "TerminalPlan.h"
#include "OpsMonitors.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Puddles.h"
#include "GrassField.h"
#include "FlightViewer.h"
#include "CharacterCreate.h"
#include "TerrainPacks.h"
#include "Components/LocalLightComponent.h"
#include "ADPFrameGen.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "RHI.h"
#include "Scalability.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "SWebBrowser.h"
#include "BabylonModel.h"
#include "EngineUtils.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#pragma region The gamepad in the menus
// While the menus are in front, the controller is theirs: its buttons and sticks are read here (before anything else
// sees them) and handed to the page as a standard gamepad, so the menus' own controller code (the pointer, the d-pad
// jumps, Cross / A to click, Circle / B back, the shoulder buttons for the pages) works as on the phone. Y / Triangle
// goes back to the 3D airport. Nothing reaches the walker meanwhile (no walking about behind the menus).
class FADPPadToMenus : public IInputProcessor
{
public:
	TWeakObjectPtr<AADPHUD> Hud;
	float Buttons[17] = {};
	float Axes[4] = {};
	bool bDirty = false;

	static int32 IndexOf(const FKey& K)
	{
		// the standard gamepad's order (the W3C mapping); the triggers come as axes below
		static const FKey Order[17] = {EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left,
			EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder, EKeys::Invalid, EKeys::Invalid,
			EKeys::Gamepad_Special_Left, EKeys::Gamepad_Special_Right, EKeys::Gamepad_LeftThumbstick, EKeys::Gamepad_RightThumbstick,
			EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_DPad_Right, EKeys::Invalid};
		for (int32 i = 0; i < 17; ++i) if (Order[i].IsValid() && Order[i] == K) return i;
		return INDEX_NONE;
	}
	bool Menus() const { const AADPHUD* H = Hud.Get(); return H && H->MenusOpen(); }
	void Clear() { for (float& B : Buttons) B = 0; for (float& A : Axes) A = 0; bDirty = true; }

	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& E) override
	{
		if (!E.GetKey().IsGamepadKey() || !Menus()) return false;
		if (E.GetKey() == EKeys::Gamepad_FaceButton_Top)
		{
			if (!E.IsRepeat()) if (AADPHUD* H = Hud.Get()) H->LeaveMenus();
			return true;
		}
		const int32 I = IndexOf(E.GetKey());
		if (I != INDEX_NONE && !E.IsRepeat()) { Buttons[I] = 1; bDirty = true; }
		return true;
	}
	virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& E) override
	{
		if (!E.GetKey().IsGamepadKey()) return false;
		const int32 I = IndexOf(E.GetKey());
		if (I != INDEX_NONE && Buttons[I] != 0) { Buttons[I] = 0; bDirty = true; }
		return Menus();
	}
	virtual bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent& E) override
	{
		const FKey K = E.GetKey();
		float* To = nullptr; float V = E.GetAnalogValue();
		if (K == EKeys::Gamepad_LeftX) To = &Axes[0];
		else if (K == EKeys::Gamepad_LeftY) { To = &Axes[1]; V = -V; }   // the web's sticks: down is +1
		else if (K == EKeys::Gamepad_RightX) To = &Axes[2];
		else if (K == EKeys::Gamepad_RightY) { To = &Axes[3]; V = -V; }
		else if (K == EKeys::Gamepad_LeftTriggerAxis) To = &Buttons[6];
		else if (K == EKeys::Gamepad_RightTriggerAxis) To = &Buttons[7];
		if (!To) return false;
		if (FMath::Abs(*To - V) > 0.01f) { *To = V; bDirty = true; }
		return Menus();
	}
	// the middle mouse button frees the pointer or gives the view back: caught here, before any widget can take it
	virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& E) override
	{
		if (E.GetEffectingButton() != EKeys::MiddleMouseButton) return false;
		AADPHUD* H = Hud.Get();
		if (!H || H->MenusOpen()) return false;
		H->TogglePointer();
		return true;
	}
	virtual const TCHAR* GetDebugName() const override { return TEXT("ADPPadToMenus"); }
};
#pragma endregion

#pragma region Look
namespace
{
	const FLinearColor Panel(0.024f, 0.094f, 0.17f, 0.88f);
	const FSlateColor Ink(FLinearColor(0.75f, 0.92f, 1.f));
	FSlateFontInfo Font(int32 Size, const char* Kind = "Bold") { return FCoreStyle::GetDefaultFontStyle(Kind, Size); }

	TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> Click)
	{
		return SNew(SButton)
			.ButtonColorAndOpacity(FLinearColor(0.05f, 0.2f, 0.36f, 0.9f))
			.ContentPadding(FMargin(10, 4))
			.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); })
			[SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(9)).ColorAndOpacity(Ink)];
	}
	TSharedRef<SWidget> BigButton(const FString& Label, TFunction<void()> Click, bool bLit = false)
	{
		return SNew(SButton)
			.ButtonColorAndOpacity(bLit ? FLinearColor(0.1f, 0.45f, 0.75f, 1.f) : FLinearColor(0.05f, 0.2f, 0.36f, 0.9f))
			.ContentPadding(FMargin(16, 9))
			.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); })
			[SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(13)).ColorAndOpacity(Ink)];
	}
	TSharedRef<SWidget> Caption(const FString& Words)
	{
		return SNew(STextBlock).Text(FText::FromString(Words)).Font(Font(10)).ColorAndOpacity(Ink);
	}
	// the title screen's own look (Content/Web/styles.css, owner 2026-11-12): dark purple glass, a faint blue edge, cyan and
	// pink neon, the Audiowide lettering (Content/Fonts, OFL)
	namespace Neon
	{
		const FLinearColor Glass(0.012f, 0.008f, 0.045f, 0.94f);
		const FLinearColor Edge(0.28f, 0.52f, 1.f, 0.32f);
		const FLinearColor Cyan(0.207f, 0.94f, 1.f);
		const FLinearColor Pink(1.f, 0.247f, 0.72f);
		const FLinearColor Paper(0.93f, 0.91f, 1.f);
		const FLinearColor BarFill(0.055f, 0.04f, 0.157f, 0.72f);
		const FLinearColor BarLit(0.62f, 0.12f, 0.45f, 0.85f);
		FSlateFontInfo Face(int32 Size, int32 Spacing = 60)
		{
			static const FString Path = FPaths::ProjectContentDir() / TEXT("Fonts/Audiowide-Regular.ttf");
			static const bool bHave = FPaths::FileExists(Path);
			FSlateFontInfo F = bHave ? FSlateFontInfo(Path, Size) : Font(Size);
			F.LetterSpacing = Spacing;
			return F;
		}
		TSharedRef<SWidget> Panel(TSharedRef<SWidget> In, float Pad = 22.f)
		{
			return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Edge).Padding(1)
				[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Glass).Padding(Pad)[In]];
		}
		/** A menu bar as on the title screen: a cyan stripe at its left, pink and lit when it is the one chosen. */
		TSharedRef<SWidget> Bar(const FString& Label, TFunction<void()> Click, bool bLit = false, int32 Size = 13)
		{
			return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0)
				.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(3)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(bLit ? Pink : FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.6f))]]
					+ SHorizontalBox::Slot().FillWidth(1)
					[
						SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(bLit ? BarLit : BarFill).Padding(FMargin(16, 9))
						[SNew(STextBlock).Text(FText::FromString(Label.ToUpper())).Font(Face(Size, bLit ? 140 : 80)).ColorAndOpacity(bLit ? FLinearColor::White : Paper)]
					]
				];
		}
		TSharedRef<SWidget> Small(const FString& Words, const FLinearColor& Col = Cyan)
		{
			return SNew(STextBlock).Text(FText::FromString(Words.ToUpper())).Font(Face(9, 400)).ColorAndOpacity(Col);
		}
	}
	UGameUserSettings* Prefs() { return GEngine ? GEngine->GetGameUserSettings() : nullptr; }
	void Save(UGameUserSettings* S) { if (S) { S->ApplySettings(false); S->SaveSettings(); } }
}
#pragma endregion

#pragma region Setup

void AADPHUD::BeginPlay()
{
	Super::BeginPlay();
	Current = this;
	// indoors the shadows are lifted: where the lamps don't reach it is only about a quarter dark (the owner's choice,
	// 2026-11-09), so the whole building can be seen; the lit areas still stand out (the lamps aren't made brighter)
	if ((Indoor = GetWorld()->SpawnActor<APostProcessVolume>()) != nullptr)
	{
		Indoor->bUnbound = true;
		Indoor->Priority = 5.f;
		Indoor->BlendWeight = 0.f;
		FPostProcessSettings& S = Indoor->Settings;
		// (2026-11-10: less washed out in the gates: a little less lift and leak, more contact shading)
		S.bOverride_LumenSkylightLeaking = true; S.LumenSkylightLeaking = 0.12f;
		S.bOverride_IndirectLightingIntensity = true; S.IndirectLightingIntensity = 1.25f;
		S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = 0.5f;
		S.bOverride_AutoExposureBias = true; S.AutoExposureBias = -0.5f;   // indoors a little less bright overall
	}
	GConfig->GetBool(TEXT("ADP.Boxes"), TEXT("FrameRateOn"), bFpsBox, GGameUserSettingsIni);
	{ bool bFG = false; GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("FrameGeneration"), bFG, GGameUserSettingsIni); SetFrameGen(bFG); }
	// an update that could not be put in place last time is said in Settings > Updates (not passed over in silence)
	{ const FString P = LastUpdateProblem(); if (!P.IsEmpty()) UpdateStatus = TEXT("The last update could not be installed: ") + P.RightChop(15).TrimStart() + TEXT(" Check for updates and install it again (Windows asks for permission)."); }
	// bloom and blur as last chosen (Settings: Off, Low, Medium, High, Epic)
	{ int32 B = 4, M = 4; GConfig->GetInt(TEXT("ADP.Graphics"), TEXT("Bloom"), B, GGameUserSettingsIni); GConfig->GetInt(TEXT("ADP.Graphics"), TEXT("Blur"), M, GGameUserSettingsIni); SetBloom(B); SetBlur(M); }
	PrimaryActorTick.bCanEverTick = true;
	FirstStartScan();

	Plan.Load();
	HoloMap = SNew(SHoloMap);
	HoloMap->SetPlan(Plan);
	HoloMap->OnOpen = [this]() { ToggleMap(); };
	Settings = MakeSettings();
	MakeWardrobe();
	Perf = MakePerf();
	TopBar = MakeTopBar();
	Settings->SetVisibility(EVisibility::Collapsed);
	CrashPanel = MakeCrashPanel();
	UnstuckPanel = MakeUnstuckPanel();
	LoadingBox = MakeLoading();
	DeviceLegend = MakeDeviceLegend();
	DeviceLegend->SetVisibility(EVisibility::Collapsed);
	LoadingBox->SetVisibility(EVisibility::Collapsed);
	UnstuckPanel->SetVisibility(EVisibility::Collapsed);
	CrashPanel->SetVisibility(FindNewCrash() ? EVisibility::Visible : EVisibility::Collapsed);
	Breadcrumb(TEXT("Started, the title screen"));
	Perf->SetVisibility(EVisibility::Collapsed);

	GameBox = SNew(SBox);
	GameBox->SetVisibility(EVisibility::Collapsed);

	// the hangars' shortcuts for the full-screen map: one for each hangar on the airfield, outside it facing its doors
	TSharedRef<SVerticalBox> HangarList = SNew(SVerticalBox);
	{
		FAirfieldData Field;
		if (Field.Load())
			for (int32 i = 0; i < Field.Hangars.Num(); ++i)
			{
				const FAirfieldData::FHangarSite& H = Field.Hangars[i];
				HangarAt.Add(FVector2D(H.X, H.Y)); HangarSize.Add(H.Size);
				const FString Label = FString::Printf(TEXT("HANGAR %d (%s): OUTSIDE"), i + 1, *FHangarSpec::Get(H.Size).Name);
				HangarList->AddSlot().AutoHeight().Padding(0, 0, 0, i + 1 == Field.Hangars.Num() ? 10 : 4)[Button(Label, [this, i]() { GoHangar(i); })];
			}
		// the fire station's: outside straight on, outside from the side, inside the apparatus hall by the trucks
		HangarList->AddSlot().AutoHeight().Padding(0, 0, 0, 4)[Button(TEXT("FIRE STATION: OUTSIDE"), [this]() { GoFireStation(0); })];
		HangarList->AddSlot().AutoHeight().Padding(0, 0, 0, 4)[Button(TEXT("FIRE STATION: FROM THE SIDE"), [this]() { GoFireStation(1); })];
		HangarList->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[Button(TEXT("FIRE STATION: INSIDE, BY THE TRUCKS"), [this]() { GoFireStation(2); })];
	}

	Root = SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()[GameBox.ToSharedRef()]
		// drone mode's line, bottom centre (only while flying, the map closed; clear of the time and weather bar above,
		// the small map at the left and the Buttons panel at the right): speed, the wheel's step, height
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(16, 16, 16, 44)
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.02f, 0.06f, 0.1f, 0.75f)).Padding(FMargin(14, 6))
			.Visibility_Lambda([this]() { return Drone.IsValid() && !bGame ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(STextBlock).Font(Font(11)).ColorAndOpacity(Ink)
				.Text_Lambda([this]()
				{
					if (!Drone.IsValid()) return FText::GetEmpty();
					const ADroneCam* D = Drone.Get();
					const FString H = D->HeightAbove() < 0.f ? FString(TEXT("-")) : FString::Printf(TEXT("%.0f m"), D->HeightAbove());
					const FString Sp = D->Boosting() ? FString(TEXT("BOOST")) : FString::Printf(TEXT("step %g m/s"), D->StepSpeed());
					return FText::FromString(FString::Printf(TEXT("DRONE   %.0f km/h   %s   height %s"), D->SpeedNow() * 3.6f, *Sp, *H));
				})
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(16)[TopBar.ToSharedRef()]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(16)
		[
			SAssignNew(MiniBox, SBox).WidthOverride(240).HeightOverride(240)
			.Visibility_Lambda([this]() { return bGame || bInLounge ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				// the map, and beside it (full screen only) its shortcuts in their own column: nothing of the interface lies
				// over the map's drawing (owner, 2026-11-13: no UI in the way of another)
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[HoloMap.ToSharedRef()]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(12, 0, 0, 0)
				[
					SNew(SVerticalBox)
					.Visibility_Lambda([this]() { return HoloMap.IsValid() && HoloMap->IsFull() ? EVisibility::Visible : EVisibility::Collapsed; })
					// drone mode: the map closes and you fly (its own row at the top of the column, nothing over the map)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[Button(TEXT("DRONE MODE"), [this]() { EnterDrone(); })]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[Button(TEXT("GATE 4 (UPPER FLOOR)"), [this]() { GoGate4(); })]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[Button(TEXT("APRON: THE TERMINAL AND JETWAY"), [this]() { GoApron(); })]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[Button(TEXT("MAIN ENTRANCE"), [this]() { GoEntrance(); })]
					// the control tower's three
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[Button(TEXT("TOWER: THE CAB"), [this]() { GoTowerCab(); })]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[Button(TEXT("TOWER: THE BALCONY"), [this]() { GoTowerBalcony(); })]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[Button(TEXT("TOWER: OUTSIDE, AT ITS DOOR"), [this]() { GoTowerFoot(); })]
					// one for each hangar
					+ SVerticalBox::Slot().AutoHeight()[HangarList]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).Visibility_Lambda([this]() { return HoloMap.IsValid() && HoloMap->HasRoute() ? EVisibility::Visible : EVisibility::Collapsed; })
						[Button(TEXT("CLEAR WAYPOINTS"), [this]() { if (HoloMap.IsValid()) HoloMap->ClearRoute(); })]
					]
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0, 8, 0, 0)
		[
			// TESTING (for now): the time of day and the weather, at the very top in the middle (free the pointer with the
			// middle mouse button to use it); the announcements' captions sit below it, never on it
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Panel)
			.Padding(FMargin(10, 4))
			.Visibility_Lambda([this]() { return bGame || bHidden || bWardrobe || bWhoAreYou ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("TEST  ·  TIME")))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(180)
					[
						SNew(SSlider)
						.Value_Lambda([]() { return ASunClock::HourOverride < 0 ? 0.5f : ASunClock::HourOverride / 24.f; })
						.OnValueChanged_Lambda([](float V) { ASunClock::HourOverride = V * 24.f; ASunClock::Refresh(); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 6, 0).VAlign(VAlign_Center)
				[SNew(SBox).WidthOverride(64)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { const float H = ASunClock::HourOverride; return FText::FromString(H < 0 ? FString(TEXT("REAL")) : FString::Printf(TEXT("%02d:%02d"), int32(H) % 24, int32(H * 60) % 60)); })]]
				+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("REAL TIME"), []() { ASunClock::HourOverride = -1.f; ASunClock::Refresh(); })]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)[WeatherButtons()]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(16)[Perf.ToSharedRef()]
		+ SOverlay::Slot()
		[
			// the frame-rate box (Settings): small, draggable with the pointer free (middle mouse)
			SAssignNew(FpsBox, SDragBox).Name(TEXT("FrameRate")).Start(FVector2D(0.86, 0.22))
			.Visibility_Lambda([this]() { return bFpsBox && !bGame && !bHidden ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(Panel)
				.Padding(FMargin(10, 5))
				.Cursor(EMouseCursor::GrabHand)
				[
					SNew(STextBlock).Font(Font(11)).ColorAndOpacity(Ink)
					.Text_Lambda([this]() { const int32 Mul = ADPFrameGen::Enabled() ? ADPFrameGen::Multiplier() : (bFrameGen ? 2 : 1); return FText::FromString(Mul > 1 ? FString::Printf(TEXT("%d FPS with frame generation  ·  %d rendered"), FMath::RoundToInt(FpsSmooth * Mul), FMath::RoundToInt(FpsSmooth)) : FString::Printf(TEXT("%d FPS  ·  %.1f ms"), FMath::RoundToInt(FpsSmooth), 1000.f / FMath::Max(1.f, FpsSmooth))); })
				]
			]
		]
		+ SOverlay::Slot()
		[
			// the 3D viewer's flight: what it is, where from and to, how high and how fast (drag it with the pointer free)
			SNew(SDragBox).Name(TEXT("FlightInfo")).Start(FVector2D(0.02, 0.3))
			.Visibility_Lambda([this]() { return bViewer && !bGame && !bHidden && Viewer && Viewer->HasFlight() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(Panel)
				.Padding(FMargin(14, 10))
				.Cursor(EMouseCursor::GrabHand)
				[
					SNew(SBox).WidthOverride(330)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[SNew(STextBlock).Font(Font(16)).ColorAndOpacity(FLinearColor(1.f, 0.82f, 0.29f)).Text_Lambda([this]() { const FFlightSample& F = Viewer->Now(); return FText::FromString(F.Callsign.IsEmpty() ? F.Id : F.Callsign); })]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
						[SNew(STextBlock).Font(Font(10)).ColorAndOpacity(Ink).AutoWrapText(true).Text_Lambda([this]() { const FFlightSample& F = Viewer->Now(); return FText::FromString(FString::Printf(TEXT("%s  ·  %s  %s"), *F.Airline, *F.Type, *F.Reg)); })]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
						[SNew(STextBlock).Font(Font(12)).ColorAndOpacity(Ink).AutoWrapText(true).Text_Lambda([this]() { const FFlightSample& F = Viewer->Now(); return FText::FromString(FString::Printf(TEXT("%s %s  \u2192  %s %s"), *F.From, *F.FromName, *F.To, *F.ToName)); })]
						+ SVerticalBox::Slot().AutoHeight()
						[SNew(STextBlock).Font(Font(10)).ColorAndOpacity(Ink).AutoWrapText(true).Text_Lambda([this]() { const FFlightSample& F = Viewer->Now();
							return FText::FromString(FString::Printf(TEXT("Altitude %s ft   Speed %d kt\nHeading %03d\u00b0   Climb %+d ft/min\n%s%s"), *FText::AsNumber(FMath::RoundToInt(F.AltM / 0.3048 / 10) * 10).ToString(), FMath::RoundToInt(F.Gs), FMath::RoundToInt(F.Hdg) % 360, FMath::RoundToInt(F.Vs),
								*F.Phase, F.PaxMax > 0 ? *FString::Printf(TEXT("   ·   %d / %d passengers"), F.Pax, F.PaxMax) : TEXT(""))); })]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
						[SNew(STextBlock).Font(Font(8)).ColorAndOpacity(FLinearColor(0.6f, 0.68f, 0.78f)).AutoWrapText(true).Text(FText::FromString(TEXT("Mouse: look round   Wheel: closer / further   Middle button: move this box   Esc: back"))) ]
					]
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Settings.ToSharedRef()]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(48, 0, 0, 0)[WardrobeBox.ToSharedRef()]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[CrashPanel.ToSharedRef()]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[UnstuckPanel.ToSharedRef()]
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[LoadingBox.ToSharedRef()]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(24, 0, 0, 84))[DeviceLegend.ToSharedRef()]   // (above a computer's taskbar)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			// the first start's question: the scan's suggestion, taken only if you say yes
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Panel)
			.Padding(20)
			.Visibility_Lambda([this]() { return SuggestedQuality >= 0 ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(STextBlock).Font(Font(13)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("YOUR PC WAS MEASURED")))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
				[
					SNew(STextBlock).Font(Font(10, "Regular")).ColorAndOpacity(Ink).AutoWrapText(true)
					.Text_Lambda([this]() { const TCHAR* N[] = {TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC"), TEXT("CINEMATIC")};
						return FText::FromString(FString::Printf(TEXT("Suggested graphics quality for this PC: %s.  Use it? Nothing changes unless you say yes; you can change it any time in Settings (Tab)."), N[FMath::Clamp(SuggestedQuality, 0, 4)])); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[Button(TEXT("YES, USE IT"), [this]() { AnswerQuality(true); })]
					+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("NO, KEEP MINE"), [this]() { AnswerQuality(false); })]
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(16)
		[
			// the Buttons panel: the lower right corner is its own, nothing else is ever placed there (in the 3D airport;
			// the menus have their own buttons there, and the controller works them as on the phone)
			SNew(SBorder)
			.Visibility_Lambda([this]() { return bGame ? EVisibility::Collapsed : EVisibility::Visible; })
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Panel)
			.Padding(FMargin(10, 6))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(ButtonsList, SVerticalBox)
					.Visibility_Lambda([this]() { return bButtonsOpen && !bWardrobe ? EVisibility::Visible : EVisibility::Collapsed; })
				]
				// at the wardrobe the legend is the wardrobe's (owner, 2026-11-12), not the walking keys
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SVerticalBox)
					.Visibility_Lambda([this]() { return bButtonsOpen && bWardrobe ? EVisibility::Visible : EVisibility::Collapsed; })
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("YOUR WARDROBE")))]
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font(9, "Regular")).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("Click a category: it opens, and the view goes with it")))]
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font(9, "Regular")).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("   (your face for hats and sunglasses, your feet for shoes)")))]
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font(9, "Regular")).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("Click a piece: you wear it, and it is saved")))]
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font(9, "Regular")).ColorAndOpacity(Ink).Text_Lambda([]() { return FText::FromString(ADPInput::CurrentPad() != ADPInput::EPad::None ? TEXT("Pad X, B: close the wardrobe") : TEXT("E, Esc: close the wardrobe")); })]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[SNew(STextBlock).Font(Font(9, "Regular")).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("The holographic map is back when you close it")))]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("BUTTONS")))]
					+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(ButtonsHeaderGlyph, SBox)]
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(8)
		[
			// the imagery's credit, as its licence (CC BY 4.0) asks
			// every credit in one line here (wrapping between the map and the Buttons tab, never over them)
			SAssignNew(Credit, STextBlock).Font(Font(7, "Regular")).ColorAndOpacity(FLinearColor(1, 1, 1, 0.55f)).Justification(ETextJustify::Center).WrapTextAt(1000)
			.Text_Lambda([]() { return FText::FromString(AGlobe::SourceCredit(AGlobe::ShownSource) + TEXT(" · Copernicus DEM GLO-90 © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018 provided under COPERNICUS by the European Union and ESA · Data from the Faroese Environment Agency, Umhvørvisstovan: DSM hæddarmodell 2015, November 2026 · Cesium")); })
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0, 92, 0, 0)
		[
			SAssignNew(CaptionBox, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Panel)
			.Padding(FMargin(16, 8))
			.Visibility_Lambda([this]() { return CaptionTime > 0 && CallGain() > 0.05f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[SNew(STextBlock).Font(Font(12)).ColorAndOpacity(Ink).Text_Lambda([this]() { return FText::FromString(TEXT("ANNOUNCEMENT  ·  ") + CallText); })]
		];

	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->AddViewportWidgetContent(Root.ToSharedRef(), 10);
	if (FSlateApplication::IsInitialized())
	{
		PadToMenus = MakeShared<FADPPadToMenus>();
		PadToMenus->Hud = this;
		FSlateApplication::Get().RegisterInputPreProcessor(PadToMenus);
	}
	// the game starts on the app's own title screen and main menu (not in photo mode)
	if (!FParse::Param(FCommandLine::Get(), TEXT("PhotoTour"))) ToggleGame();

	// the keys: M map, Tab settings, P performance, H hide, Esc or a double-click to show again
	if (APlayerController* PC = GetOwningPlayerController())
	{
		EnableInput(PC);
		// the controls table is checked first: no key or button is ever used twice
		ADPInput::CheckNoConflicts();
		ADPInput::CurrentPad() = ADPInput::DetectPad();
		// (it only watches which device is in use: it never uses up the key, so walking and everything else still get it)
		InputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &AADPHUD::OnAnyKey).bConsumeInput = false;
		InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AADPHUD::ToggleButtons);
		InputComponent->BindKey(EKeys::Gamepad_Special_Left, IE_Pressed, this, &AADPHUD::ToggleButtons);
		InputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Pressed, this, &AADPHUD::ToggleGame);
		InputComponent->BindKey(EKeys::Gamepad_RightShoulder, IE_Pressed, this, &AADPHUD::ToggleMap);
		InputComponent->BindKey(EKeys::Gamepad_LeftShoulder, IE_Pressed, this, &AADPHUD::ToggleHide);
		InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &AADPHUD::ToggleSettings);
		InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &AADPHUD::Interact);
		InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AADPHUD::ToggleGame);
		InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AADPHUD::Interact);
		// (a click on the phone or a screen uses it too; the click still goes on to everything else)
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AADPHUD::Interact).bConsumeInput = false;
		InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AADPHUD::ToggleMap);
		InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AADPHUD::ToggleSettings);
		InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AADPHUD::TogglePerf);
		InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AADPHUD::HideUI);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AADPHUD::OnEscape);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_DoubleClick, this, &AADPHUD::ShowUI);
	}
}

void AADPHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Root.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Root.ToSharedRef());
	if (PadToMenus.IsValid() && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(PadToMenus);
	Super::EndPlay(Reason);
}

void AADPHUD::FirstStartScan()
{
	// the device scan, once: the PC is measured and a quality SUGGESTED (Yes / No); nothing changes unless you say yes
	const FString Flag = FPaths::ProjectSavedDir() / TEXT("DeviceScanned.txt");
	if (FPaths::FileExists(Flag)) return;
	const Scalability::FQualityLevels Q = Scalability::BenchmarkQualityLevels();
	SuggestedQuality = Q.GetSingleQualityLevel();
	if (SuggestedQuality < 0) SuggestedQuality = FMath::Clamp(FMath::Min(Q.ShadowQuality, Q.GlobalIlluminationQuality), 0, 4);
}

void AADPHUD::AnswerQuality(bool bYes)
{
	if (bYes)
		if (UGameUserSettings* S = Prefs()) { S->SetOverallScalabilityLevel(SuggestedQuality); Save(S); }
	SuggestedQuality = -1;
	FFileHelper::SaveStringToFile(bYes ? TEXT("suggestion taken") : TEXT("suggestion declined"), *(FPaths::ProjectSavedDir() / TEXT("DeviceScanned.txt")));
}

#pragma endregion

#pragma region Panels

TSharedRef<SWidget> AADPHUD::MakeTopBar()
{
	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
	Bar->AddSlot().AutoWidth().Padding(0, 0, 6, 0)[Button(TEXT("GAME  (G)"), [this]() { ToggleGame(); })];
	Bar->AddSlot().AutoWidth().Padding(0, 0, 6, 0)[Button(TEXT("MAP  (M)"), [this]() { ToggleMap(); })];
	Bar->AddSlot().AutoWidth().Padding(0, 0, 6, 0)[Button(TEXT("SETTINGS  (TAB)"), [this]() { ToggleSettings(); })];
	Bar->AddSlot().AutoWidth()[Button(TEXT("HIDE UI  (H)"), [this]() { HideUI(); })];
	// under the bar: your airline, its money and the time (once a game is running in the menus)
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Bar]
		+ SVerticalBox::Slot().AutoHeight().Padding(2, 6, 0, 0)
		[
			SNew(STextBlock).Font(Font(10)).ColorAndOpacity(Ink)
			.Visibility_Lambda([this]() { return StatusLine.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			.Text_Lambda([this]() { return FText::FromString(StatusLine + TEXT("   ·   ") + FDateTime::Now().ToString(TEXT("%H:%M"))); })
		];
}

TSharedRef<SWidget> AADPHUD::VolumeSlider(int32 I)
{
	// a smooth slider from off to full, kept in the settings file
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
		[
			SNew(SSlider)
			.Value_Lambda([this, I]() { return Vol[I]; })
			.OnValueChanged_Lambda([this, I](float V) { Vol[I] = V; })
			.OnMouseCaptureEnd_Lambda([this, I]() { GConfig->SetFloat(TEXT("ADP.Announcements"), *FString::Printf(TEXT("Volume%d"), I), Vol[I], GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(10, 0, 0, 0).VAlign(VAlign_Center)
		[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([this, I]() { return FText::FromString(Vol[I] < 0.01f ? FString(TEXT("OFF")) : FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Vol[I] * 100))); })];
}

TSharedRef<SWidget> AADPHUD::WeatherButtons()
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 T = 0; T < 8; ++T)
		Row->AddSlot().AutoWidth().Padding(0, 0, 3, 0)
		[
			SNew(SButton)
			.ButtonColorAndOpacity_Lambda([T]() { return ASunClock::Theme == T ? FLinearColor(0.15f, 0.45f, 0.75f, 1.f) : FLinearColor(0.05f, 0.2f, 0.36f, 0.9f); })
			.ContentPadding(FMargin(6, 2))
			.OnClicked_Lambda([T]() { ASunClock::Theme = T; GConfig->SetInt(TEXT("ADP.Testing"), TEXT("Weather"), T, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); ASunClock::Refresh(); return FReply::Handled(); })
			[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text(FText::FromString(ASunClock::ThemeName(T)))]
		];
	return Row;
}

TSharedRef<SWidget> AADPHUD::MakeSettings()
{
	for (int32 I = 0; I < 4; ++I) GConfig->GetFloat(TEXT("ADP.Announcements"), *FString::Printf(TEXT("Volume%d"), I), Vol[I], GGameUserSettingsIni);
	TSharedRef<SHorizontalBox> Quality = SNew(SHorizontalBox);
	const TCHAR* Names[] = {TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC"), TEXT("CINEMATIC")};
	for (int32 q = 0; q < 5; ++q)
		Quality->AddSlot().AutoWidth().Padding(0, 0, 4, 0)[Button(Names[q], [q]() { if (UGameUserSettings* S = Prefs()) { S->SetOverallScalabilityLevel(q); Save(S); } })];
	TSharedRef<SHorizontalBox> Limit = SNew(SHorizontalBox);
	for (const float L : {30.f, 60.f, 120.f, 0.f})
		Limit->AddSlot().AutoWidth().Padding(0, 0, 4, 0)[Button(L > 0 ? FString::Printf(TEXT("%d FPS"), int32(L)) : TEXT("NO LIMIT"), [L]() { if (UGameUserSettings* S = Prefs()) { S->SetFrameRateLimit(L); Save(S); } })];

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(Panel)
		.Padding(18)
		[
			SNew(SBox).WidthOverride(560)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[SNew(STextBlock).Text(FText::FromString(TEXT("SETTINGS"))).Font(Font(14)).ColorAndOpacity(Ink)]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Graphics quality"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 10)[Quality]
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font(10)).ColorAndOpacity(Ink).Text_Lambda([]()
				{
					const UGameUserSettings* S = Prefs();
					return FText::FromString(FString::Printf(TEXT("Render scale: %d%%  (sharpened back up to full size by TSR)"), S ? FMath::RoundToInt(S->GetResolutionScaleNormalized() * 50 + 50) : 100));
				})]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 10)
				[
					SNew(SSlider)
					.Value_Lambda([]() { const UGameUserSettings* S = Prefs(); return S ? S->GetResolutionScaleNormalized() : 1.f; })
					.OnValueChanged_Lambda([](float V) { if (UGameUserSettings* S = Prefs()) { S->SetResolutionScaleNormalized(V); S->ApplySettings(false); } })
					.OnMouseCaptureEnd_Lambda([]() { Save(Prefs()); })
				]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Announcements: all"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 6)[VolumeSlider(0)]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Flight calls (boarding, final call, gate closed, delays)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 6)[VolumeSlider(1)]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Passenger calls (passengers asked to the gate or the information desk)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 6)[VolumeSlider(2)]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Information (notices for everyone)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 10)[VolumeSlider(3)]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Satellite pictures on the globe (free sources, the whole Earth)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)[Button(TEXT("EOX SENTINEL-2 (10 M)"), []() { AGlobe::UseSource(0); })]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)[Button(TEXT("NASA LANDSAT (30 M)"), []() { AGlobe::UseSource(1); })]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)[Button(TEXT("NASA BLUE MARBLE (500 M)"), []() { AGlobe::UseSource(2); })]
					+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("LAND COVER, OFFLINE (ESA)"), []() { AGlobe::UseSource(3); })]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[SNew(STextBlock).Font(Font(9, "Regular")).ColorAndOpacity(Ink).AutoWrapText(true).Text_Lambda([]() { return FText::FromString(AGlobe::Note); })]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Updates"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 2)
				[
					SNew(STextBlock).Font(Font(9, "Regular")).ColorAndOpacity(Ink).AutoWrapText(true)
					.Text_Lambda([this]() { return FText::FromString(UpdateStatus.IsEmpty() ? FString::Printf(TEXT("This is version %s."), *UpdaterVersion()) : UpdateStatus); })
				]
				// the download's bar, its percentage in the middle and the speed beside it (while downloading)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 4)
				[
					SNew(SBox).Visibility_Lambda([this]() { return UpdateFrac >= 0.f && UpdateBusy ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SProgressText).Height(22.f)
						.Fraction_Lambda([this]() { return UpdateFrac; })
						.Extra_Lambda([this]() { return UpdateSpeed > 0 ? FString::Printf(TEXT("%.1f MB/s"), UpdateSpeed / 1048576.0) : FString(); })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 10)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[Button(TEXT("CHECK FOR UPDATES"), [this]() { CheckForUpdates(); })]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).Visibility_Lambda([this]() { return (UpdateChain.Num() || !UpdateFullUrl.IsEmpty()) && !UpdateBusy ? EVisibility::Visible : EVisibility::Collapsed; })
						[Button(TEXT("INSTALL UPDATE"), [this]() { InstallUpdate(); })]
					]
				]
				// bloom (the glow round bright lights) and blur (motion blur and the soft background of depth of field):
				// Off, Low, Medium, High, Epic; the one in use lit
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Bloom (the glow round bright lights)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 8)[LevelRow(true)]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Blur (motion blur and depth of field)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 10)[LevelRow(false)]
				// the time of year (owner, 2026-11-12): a month and a day, or the real date; the sun's height, the day's
				// length and the sunrise and sunset follow it at once (worked out for this year, so it shifts a little
				// from year to year as in life); and the real-world weather switch (its feed comes later)
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Time of year (the sun's height and the day's length follow it)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[SNew(SBox).WidthOverride(44)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("Month")))]]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { return (FMath::Max(ASunClock::Month, 1) - 1) / 11.f; })
						.OnValueChanged_Lambda([](float V) { ASunClock::SetDate(1 + FMath::RoundToInt(V * 11.f), FMath::Max(ASunClock::Day, 1)); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 0, 6, 0)[SNew(SBox).WidthOverride(28)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("Day")))]]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { return (FMath::Max(ASunClock::Day, 1) - 1) / 30.f; })
						.OnValueChanged_Lambda([](float V) { ASunClock::SetDate(FMath::Max(ASunClock::Month, 1), 1 + FMath::RoundToInt(V * 30.f)); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 6, 0).VAlign(VAlign_Center)
					[SNew(SBox).WidthOverride(96)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { return FText::FromString(ASunClock::DateName()); })]]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Button(TEXT("REAL DATE"), []() { ASunClock::SetDate(0, 0); })]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 10)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.ButtonColorAndOpacity_Lambda([]() { return ASunClock::bRealWeather ? FLinearColor(0.1f, 0.45f, 0.75f, 1.f) : FLinearColor(0.05f, 0.2f, 0.36f, 0.9f); })
						.ContentPadding(FMargin(10, 4))
						.OnClicked_Lambda([]() { ASunClock::SetRealWeather(!ASunClock::bRealWeather); return FReply::Handled(); })
						[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { return FText::FromString(ASunClock::bRealWeather ? TEXT("ON") : TEXT("OFF")); })]
					]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[SNew(STextBlock).Font(Font(9, "Regular")).ColorAndOpacity(Ink).AutoWrapText(true).Text(FText::FromString(TEXT("Real-world weather at the airport (the live weather feed is coming; until then the weather buttons set it)")))]
				]
				// walk anywhere on Earth: a named place or a latitude and longitude; back to the airport
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Walk anywhere on Earth"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 4)
				[SNew(STextBlock).Font(Font(9, "Regular")).ColorAndOpacity(Ink).AutoWrapText(true).Text_Lambda([]() { return FText::FromString(AWalkAnywhere::Status() + TEXT("  Pick a place, or type a latitude and longitude (e.g. 62.01, -6.77) and press Enter.")); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 4)
				[
					SNew(SEditableTextBox).Font(Font(10, "Regular")).HintText(FText::FromString(TEXT("Latitude, longitude  or a place's name")))
					.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type C) { if (C == ETextCommit::OnEnter) { if (AWalkAnywhere::GoText(GetWorld(), T.ToString())) { if (bSettings) ToggleSettings(); } } })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 10)
				[
					WalkPlaces()
				]
				// the world's terrain: downloaded once (about 6.6 GB) and kept up to date pack by pack
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("The world's terrain (mountains and valleys everywhere)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 4)
				[SNew(STextBlock).Font(Font(9, "Regular")).ColorAndOpacity(Ink).AutoWrapText(true).Text_Lambda([]() { return FText::FromString(ADPTerrain::Status()); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 4)
				[
					SNew(SBox).Visibility_Lambda([]() { return ADPTerrain::Fraction() >= 0.f ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SProgressText).Height(22.f)
						.Fraction_Lambda([]() { return ADPTerrain::Fraction(); })
						.Extra_Lambda([]() { const double S = ADPTerrain::Speed(); return S > 0 ? FString::Printf(TEXT("%.1f MB/s"), S / 1048576.0) : FString(); })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 10)
				[
					SNew(SBox).Visibility_Lambda([]() { return ADPTerrain::Busy() ? EVisibility::Collapsed : EVisibility::Visible; })
					[
						SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6, 6))
						+ SWrapBox::Slot()[Button(TEXT("DOWNLOAD / UPDATE THE WORLD'S TERRAIN"), []() { ADPTerrain::Start(); })]
						+ SWrapBox::Slot()[Button(TEXT("MOVE IT TO ANOTHER FOLDER"), []() { ADPTerrain::ChooseAndMove(); })]
					]
				]
				// the grass: on or off, and how far the dense near blades reach (the airfield's far tufts are always there)
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Grass (how far the dense blades reach)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 10)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.ButtonColorAndOpacity_Lambda([]() { return AGrassField::bOn ? FLinearColor(0.1f, 0.45f, 0.75f, 1.f) : FLinearColor(0.05f, 0.2f, 0.36f, 0.9f); })
						.ContentPadding(FMargin(10, 4))
						.OnClicked_Lambda([]() { AGrassField::bOn = !AGrassField::bOn; GConfig->SetBool(TEXT("ADP.Graphics"), TEXT("Grass"), AGrassField::bOn, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); return FReply::Handled(); })
						[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { return FText::FromString(AGrassField::bOn ? TEXT("ON") : TEXT("OFF")); })]
					]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { return AGrassField::NearDistance / 150.f; })
						.OnValueChanged_Lambda([](float V) { AGrassField::NearDistance = FMath::RoundToFloat(V * 150.f); })
						.OnMouseCaptureEnd_Lambda([]() { GConfig->SetFloat(TEXT("ADP.Graphics"), TEXT("GrassNear"), AGrassField::NearDistance, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[SNew(SBox).WidthOverride(56)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { return FText::FromString(FString::Printf(TEXT("%.0f m"), AGrassField::NearDistance)); })]]
				]
				// the puddles (owner, 2026-11-13): a Rainy day switch to see them wet (it wets over about a minute, then dries
				// slowly), the wetness now, and their tuning - saved to Content/Settings/Puddles.ini when a slider is let go
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Puddles and wet ground (Rainy day, how wet now, and their look)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.ButtonColorAndOpacity_Lambda([]() { APuddles* P = APuddles::Get(); return P && P->IsRainyDay() ? FLinearColor(0.1f, 0.45f, 0.75f, 1.f) : FLinearColor(0.05f, 0.2f, 0.36f, 0.9f); })
						.ContentPadding(FMargin(10, 4))
						.OnClicked_Lambda([]() { if (APuddles* P = APuddles::Get()) P->SetRainyDay(!P->IsRainyDay()); return FReply::Handled(); })
						[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { APuddles* P = APuddles::Get(); return FText::FromString(P && P->IsRainyDay() ? TEXT("RAINY DAY: ON") : TEXT("RAINY DAY: OFF")); })]
					]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { APuddles* P = APuddles::Get(); return P ? P->GetWetness() : 0.f; })
						.OnValueChanged_Lambda([](float V) { if (APuddles* P = APuddles::Get()) P->SetWetness(V); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[SNew(SBox).WidthOverride(72)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { APuddles* P = APuddles::Get(); return FText::FromString(P ? FString::Printf(TEXT("wet %.0f%%"), P->GetWetness() * 100.f) : TEXT("-")); })]]
				]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Puddles: how many"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { APuddles* P = APuddles::Get(); return P ? FMath::Clamp((P->GetTuning(TEXT("Amount")) - 0.f) / (1.f - 0.f), 0.f, 1.f) : 0.f; })
						.OnValueChanged_Lambda([](float V) { if (APuddles* P = APuddles::Get()) P->SetTuning(TEXT("Amount"), 0.f + V * (1.f - 0.f)); })
						.OnMouseCaptureEnd_Lambda([]() { if (APuddles* P = APuddles::Get()) P->Save(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[SNew(SBox).WidthOverride(72)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { APuddles* P = APuddles::Get(); return FText::FromString(P ? FString::Printf(TEXT("%.2f"), P->GetTuning(TEXT("Amount"))) : TEXT("-")); })]]
				]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Puddles: their size (metres)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { APuddles* P = APuddles::Get(); return P ? FMath::Clamp((P->GetTuning(TEXT("Size")) - 0.5f) / (12.f - 0.5f), 0.f, 1.f) : 0.f; })
						.OnValueChanged_Lambda([](float V) { if (APuddles* P = APuddles::Get()) P->SetTuning(TEXT("Size"), 0.5f + V * (12.f - 0.5f)); })
						.OnMouseCaptureEnd_Lambda([]() { if (APuddles* P = APuddles::Get()) P->Save(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[SNew(SBox).WidthOverride(72)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { APuddles* P = APuddles::Get(); return FText::FromString(P ? FString::Printf(TEXT("%.1f m"), P->GetTuning(TEXT("Size"))) : TEXT("-")); })]]
				]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Puddles: how mirror-like the water is"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { APuddles* P = APuddles::Get(); return P ? FMath::Clamp((P->GetTuning(TEXT("Shine")) - 0.f) / (1.f - 0.f), 0.f, 1.f) : 0.f; })
						.OnValueChanged_Lambda([](float V) { if (APuddles* P = APuddles::Get()) P->SetTuning(TEXT("Shine"), 0.f + V * (1.f - 0.f)); })
						.OnMouseCaptureEnd_Lambda([]() { if (APuddles* P = APuddles::Get()) P->Save(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[SNew(SBox).WidthOverride(72)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { APuddles* P = APuddles::Get(); return FText::FromString(P ? FString::Printf(TEXT("%.2f"), P->GetTuning(TEXT("Shine"))) : TEXT("-")); })]]
				]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Puddles: rain ripples"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { APuddles* P = APuddles::Get(); return P ? FMath::Clamp((P->GetTuning(TEXT("Ripples")) - 0.f) / (2.f - 0.f), 0.f, 1.f) : 0.f; })
						.OnValueChanged_Lambda([](float V) { if (APuddles* P = APuddles::Get()) P->SetTuning(TEXT("Ripples"), 0.f + V * (2.f - 0.f)); })
						.OnMouseCaptureEnd_Lambda([]() { if (APuddles* P = APuddles::Get()) P->Save(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[SNew(SBox).WidthOverride(72)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { APuddles* P = APuddles::Get(); return FText::FromString(P ? FString::Printf(TEXT("%.2f"), P->GetTuning(TEXT("Ripples"))) : TEXT("-")); })]]
				]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Puddles: drying time after rain (hours)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { APuddles* P = APuddles::Get(); return P ? FMath::Clamp((P->GetTuning(TEXT("DryingHours")) - 0.05f) / (24.f - 0.05f), 0.f, 1.f) : 0.f; })
						.OnValueChanged_Lambda([](float V) { if (APuddles* P = APuddles::Get()) P->SetTuning(TEXT("DryingHours"), 0.05f + V * (24.f - 0.05f)); })
						.OnMouseCaptureEnd_Lambda([]() { if (APuddles* P = APuddles::Get()) P->Save(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[SNew(SBox).WidthOverride(72)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { APuddles* P = APuddles::Get(); return FText::FromString(P ? FString::Printf(TEXT("%.1f h"), P->GetTuning(TEXT("DryingHours"))) : TEXT("-")); })]]
				]
				// the distant landscape's sharpness (owner, 2026-11-11): soft (fastest) to very sharp (costs frame rate
				// when looking far); the slider runs from soft on the left to sharp on the right
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Distant landscape sharpness (sharper costs frame rate when looking far)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 10)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { return FMath::Clamp(FMath::Log2(32.f / AGlobe::LandError) / 3.f, 0.f, 1.f); })
						.OnValueChanged_Lambda([](float V) { AGlobe::LandError = 32.f / FMath::Pow(2.f, FMath::RoundToFloat(V * 6.f) / 2.f); AGlobe::ApplyLand(); })
						.OnMouseCaptureEnd_Lambda([]() { GConfig->SetFloat(TEXT("ADP.Graphics"), TEXT("LandError"), AGlobe::LandError, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[SNew(SBox).WidthOverride(72)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { return FText::FromString(AGlobe::LandName()); })]]
				]
				// the field of view: degrees side to side as on a 16:9 screen (wider screens see more to the sides)
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Field of view (walking)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 10)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { return (AADPWalker::Fov - 60.f) / 60.f; })
						.OnValueChanged_Lambda([](float V) { AADPWalker::Fov = FMath::RoundToFloat(60.f + V * 60.f); })
						.OnMouseCaptureEnd_Lambda([]() { GConfig->SetFloat(TEXT("ADP.Graphics"), TEXT("Fov"), AADPWalker::Fov, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[SNew(SBox).WidthOverride(56)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { return FText::FromString(FString::Printf(TEXT("%.0f°"), AADPWalker::Fov)); })]]
				]
				// the light masts' beams: how far the evening haze they show in reaches (weaker cards: shorter)
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Light beams (how far the masts' beams show in the haze)"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 10)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Value_Lambda([]() { return (ALightMasts::BeamDistance - 60.f) / 440.f; })
						.OnValueChanged_Lambda([](float V) { ALightMasts::BeamDistance = FMath::RoundToFloat(60.f + V * 440.f); ALightMasts::Refresh(); })
						.OnMouseCaptureEnd_Lambda([]() { GConfig->SetFloat(TEXT("ADP.Graphics"), TEXT("BeamDistance"), ALightMasts::BeamDistance, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[SNew(SBox).WidthOverride(56)[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { return FText::FromString(FString::Printf(TEXT("%.0f m"), ALightMasts::BeamDistance)); })]]
				]
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Frame limit"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 10)[Limit]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6, 6))   // (wraps onto a second line, never past the panel)
					+ SWrapBox::Slot()[Button(TEXT("VSYNC ON / OFF"), []() { if (UGameUserSettings* S = Prefs()) { S->SetVSyncEnabled(!S->IsVSyncEnabled()); Save(S); } })]
					+ SWrapBox::Slot()[Button(TEXT("PERFORMANCE  (P)"), [this]() { TogglePerf(); })]
					+ SWrapBox::Slot()
					[
						SNew(SButton)
						.ButtonColorAndOpacity(FLinearColor(0.05f, 0.2f, 0.36f, 0.9f))
						.ContentPadding(FMargin(10, 4))
						.OnClicked_Lambda([this]() { SetFrameGen(!bFrameGen); return FReply::Handled(); })
						[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([this]() { return FText::FromString(bFrameGen ? TEXT("FRAME GENERATION (AMD FSR): ON") : TEXT("FRAME GENERATION (AMD FSR): OFF")); })]
					]
					+ SWrapBox::Slot()
					[
						// the game's own frame generation (made for NVIDIA cards first): takes effect at the next start
						SNew(SButton)
						.ButtonColorAndOpacity_Lambda([]() { bool b = false; GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("OwnFrameGen"), b, GGameUserSettingsIni); return b ? FLinearColor(0.1f, 0.45f, 0.75f, 1.f) : FLinearColor(0.05f, 0.2f, 0.36f, 0.9f); })
						.ContentPadding(FMargin(10, 4))
						.OnClicked_Lambda([this]() { bool b = false; GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("OwnFrameGen"), b, GGameUserSettingsIni); b = !b; GConfig->SetBool(TEXT("ADP.Graphics"), TEXT("OwnFrameGen"), b, GGameUserSettingsIni); if (b && bFrameGen) SetFrameGen(false); GConfig->Flush(false, GGameUserSettingsIni); return FReply::Handled(); })
						[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text_Lambda([]() { bool b = false; GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("OwnFrameGen"), b, GGameUserSettingsIni);
							return FText::FromString(b == ADPFrameGen::Enabled() ? (b ? TEXT("OUR FRAME GENERATION: ON") : TEXT("OUR FRAME GENERATION: OFF")) : (b ? TEXT("OUR FRAME GENERATION: ON AT NEXT START") : TEXT("OUR FRAME GENERATION: OFF AT NEXT START"))); })]
					]
					+ SWrapBox::Slot()[Button(TEXT("FRAME-RATE BOX"), [this]() { bFpsBox = !bFpsBox; GConfig->SetBool(TEXT("ADP.Boxes"), TEXT("FrameRateOn"), bFpsBox, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); })]
					+ SWrapBox::Slot()[Button(TEXT("SCAN MY PC"), []() { if (UGameUserSettings* S = Prefs()) { S->RunHardwareBenchmark(); S->ApplyHardwareBenchmarkResults(); Save(S); } })]
					+ SWrapBox::Slot()[Button(TEXT("PROBLEM LOG"), []() { FPlatformProcess::ExploreFolder(*(FPaths::ProjectLogDir() / TEXT("Problems.log"))); })]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 4)[Caption(TEXT("Controls (keyboard, and the gamepad in use)"))]
				+ SVerticalBox::Slot().AutoHeight()[SAssignNew(SettingsControls, SVerticalBox)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[Button(TEXT("CLOSE  (TAB)"), [this]() { ToggleSettings(); })]
			]
		];
}

TSharedRef<SWidget> AADPHUD::MakePerf()
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(Panel)
		.Padding(10)
		[
			SNew(STextBlock).Font(Font(10, "Mono")).ColorAndOpacity(Ink).Text_Lambda([this]()
			{
				return FText::FromString(FString::Printf(TEXT("%3.0f FPS\nGame   %5.1f ms\nRender %5.1f ms\nGPU    %5.1f ms"),
					FpsSmooth, FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())));
			})
		];
}

#pragma endregion

#pragma region Showing and hiding

void AADPHUD::UpdateMouse()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	const bool bFree = bGame || bDevice || bSettings || bWardrobe || bWhoAreYou || bPointer || (HoloMap.IsValid() && HoloMap->IsFull());
	// (Unreal counts these calls: cleared first, so the view is never left locked by an earlier one)
	PC->ResetIgnoreLookInput();
	if (bPointer && !bGame) PC->SetIgnoreLookInput(true);
	// the phone or a computer up (or the phone on its way): you stay where you are, looking at it
	PC->ResetIgnoreMoveInput();
	if ((bDevice || PhoneDir != 0 || bWardrobe || bWhoAreYou) && !bGame) { PC->SetIgnoreLookInput(true); PC->SetIgnoreMoveInput(true); }
	PC->bShowMouseCursor = bFree;
	if (bFree) { FInputModeGameAndUI M; M.SetHideCursorDuringCapture(false); PC->SetInputMode(M); }
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		// the keyboard back to the game (not left on the hidden menus page), so walking works at once
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void AADPHUD::ToggleMap()
{
	if (!HoloMap.IsValid() || bHidden || bInLounge) return;
	const bool bFull = !HoloMap->IsFull();
	HoloMap->SetFull(bFull);
	// full screen: the map fills most of the window; small: back in its corner
	StaticCastSharedPtr<SBox>(MiniBox)->SetWidthOverride(bFull ? FOptionalSize(1450) : FOptionalSize(240));   // (the map 1200 wide, its shortcut column beside it)
	StaticCastSharedPtr<SBox>(MiniBox)->SetHeightOverride(bFull ? FOptionalSize(720) : FOptionalSize(240));
	UpdateMouse();
}

void AADPHUD::OpenViewer()
{
	Breadcrumb(TEXT("In the 3D viewer (Unreal)"));
	if (bGame) ToggleGame();
	if (bInLounge) ShowTerminal();
	if (!Viewer)
	{
		FActorSpawnParameters S;
		S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Viewer = GetWorld()->SpawnActor<AFlightViewer>(FVector::ZeroVector, FRotator::ZeroRotator, S);
	}
	if (!Viewer) return;
	Viewer->Open();
	bViewer = true;
	// the walking bar and the map belong to the airport: away while you watch the flight
	if (TopBar.IsValid()) TopBar->SetVisibility(EVisibility::Collapsed);
	if (MiniBox.IsValid()) MiniBox->SetVisibility(EVisibility::Collapsed);
	UpdateMouse();
}

void AADPHUD::CloseViewer()
{
	if (!bViewer) return;
	bViewer = false;
	if (Viewer) Viewer->Close();
	if (TopBar.IsValid()) TopBar->SetVisibility(bGame ? EVisibility::Collapsed : EVisibility::Visible);
	if (MiniBox.IsValid()) MiniBox->SetVisibility(EVisibility::Visible);
	// the menus stop sending the flight
	if (Browser.IsValid()) Browser->ExecuteJavascript(TEXT("window.__ueViewerClosed && window.__ueViewerClosed()"));
}

void AADPHUD::OnViewerFlight(const FString& Json)
{
	if (bViewer && Viewer) Viewer->OnSample(Json);
}

void AADPHUD::ToggleGame()
{
	if (bHidden) return;
	if (!bGame && Drone.IsValid()) LeaveDrone();   // (the menus from the drone: you're back on your feet first)
	// from the 3D viewer to the menus: the viewer closes, the airport comes back behind them
	if (!bGame && bViewer) CloseViewer();
	bGame = !bGame;
	bPointer = false;
	if (bGame) Breadcrumb(TEXT("In the menus (the airline game)"));
	// made the first time it is opened, then kept (the game keeps running behind the terminal)
	if (bGame && !bGameMade)
	{
		Bridge = NewObject<UADPBridge>(this);
		Bridge->Hud = this;
		StaticCastSharedPtr<SBox>(GameBox)->SetContent(ADPGameView::Make(Bridge, &Browser));
		bGameMade = true;
	}
	// back to the menus: the title screen again, or the game where you left it
	else if (bGame && Browser.IsValid()) Browser->ExecuteJavascript(TEXT("window.__ueMenus && window.__ueMenus()"));
	// the controller is the menus' only while they are in front
	if (PadToMenus.IsValid()) PadToMenus->Clear();
	GameBox->SetVisibility(bGame ? EVisibility::Visible : EVisibility::Collapsed);
	// the walking interface (the bar, the map, the credit line) hides while the menus are open: nothing ever overlaps
	TopBar->SetVisibility(bGame ? EVisibility::Collapsed : EVisibility::Visible);
	if (Credit.IsValid()) Credit->SetVisibility(bGame ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
	UpdateMouse();
}

TSharedRef<SWidget> AADPHUD::MakeWardrobe()
{
	// the title screen's look (owner, 2026-11-12): the categories as menu bars on the left, and attached at the right a
	// bigger window showing what the open category holds, as tiles; you stand to the right of it all
	WardrobeBox = SNew(SBox).Visibility(EVisibility::Collapsed)
	[
		Neon::Panel(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(300)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Neon::Small(TEXT("Your office"))]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 18)[SNew(STextBlock).Text(FText::FromString(TEXT("WARDROBE"))).Font(Neon::Face(24, 120)).ColorAndOpacity(Neon::Cyan)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[Neon::Small(TEXT("Categories"), FLinearColor(Neon::Paper.R, Neon::Paper.G, Neon::Paper.B, 0.6f))]
					+ SVerticalBox::Slot().AutoHeight()[SAssignNew(WardrobeList, SVerticalBox)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)[Neon::Bar(TEXT("Close"), [this]() { CloseWardrobe(); })]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(22, 0, 0, 0)
			[
				SNew(SBox).WidthOverride(620)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(1, 1, 1, 0.035f)).Padding(18)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Neon::Small(TEXT("What you can wear"))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 14)[SNew(STextBlock).Font(Neon::Face(18, 100)).ColorAndOpacity(Neon::Paper).Text_Lambda([this]() { return FText::FromString(WardrobeCategory.ToUpper()); })]
						+ SVerticalBox::Slot().AutoHeight()[SAssignNew(WardrobeTiles, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10, 10))]
					]
				]
			]
		)
	];
	return WardrobeBox.ToSharedRef();
}

void AADPHUD::OpenWardrobe(AWardrobe* W)
{
	if (!W || bWardrobe || bHidden || !WardrobeList.IsValid()) return;
	bWardrobe = true;
	WardrobeOpen = W;
	W->SetOpen(true);
	Breadcrumb(TEXT("At your wardrobe"));
	const FVector Me = GetOwningPawn() ? GetOwningPawn()->GetActorLocation() : W->GetActorLocation();
	W->Dress(AWardrobe::Chosen(), Me);
	WardrobeCategory = TEXT("Outfits");
	// the menu is the page's (the title screen's own graphics, src/ui/wardrobe3d.ts), drawn see-through over the room
	MakeGameView();
	if (GameBox.IsValid()) GameBox->SetVisibility(EVisibility::Visible);
	FillWardrobe();
	WardrobeTicks = 0;
	W->Look(AWardrobe::EView::Body, GetOwningPlayerController());
	// your walking body away while the copy by the wardrobe stands for you (one of you in the picture, and less to draw)
	if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) if (AActor* H = Wk->HumanActor()) H->SetActorHiddenInGame(true);
	// the holographic map and the walking bar away while the wardrobe menu is up, back when it closes
	if (MiniBox.IsValid()) MiniBox->SetVisibility(EVisibility::Collapsed);
	if (TopBar.IsValid()) TopBar->SetVisibility(EVisibility::Collapsed);
	UpdateMouse();
}

void AADPHUD::FillWardrobe()
{
	// the page's menu told what to show: every category, the open one, and what it holds (your real outfits for Outfits,
	// demo pieces in the others until the clothes are made), the one you wear marked
	if (!Browser.IsValid()) return;
	auto Q = [](const FString& In) { return In.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")); };
	FString Cats;
	for (const FString& C : AWardrobe::Categories()) Cats += FString::Printf(TEXT("%s\"%s\""), Cats.IsEmpty() ? TEXT("") : TEXT(","), *Q(C));
	FString Items;
	auto Item = [&](const FString& Label, const FString& Who, bool bOn, const FString& Note)
	{
		Items += FString::Printf(TEXT("%s{\"label\":\"%s\",\"who\":\"%s\",\"on\":%s,\"note\":\"%s\"}"), Items.IsEmpty() ? TEXT("") : TEXT(","), *Q(Label), *Q(Who), bOn ? TEXT("true") : TEXT("false"), *Q(Note));
	};
	if (WardrobeCategory == TEXT("Outfits"))
		for (const AWardrobe::FOutfit& O : AWardrobe::Outfits()) { const bool bOn = O.Who == AWardrobe::Chosen(); Item(O.Label, O.Who, bOn, bOn ? TEXT("wearing") : TEXT("yours")); }
	else
	{
		// demo pieces (owner, 2026-11-12): what the window will look like once the clothes are made
		static const TMap<FString, TArray<FString>> Demo = {
			{TEXT("Jackets"), {TEXT("Navy suit jacket"), TEXT("Charcoal blazer"), TEXT("Rain shell"), TEXT("Wool overcoat")}},
			{TEXT("Shirts"), {TEXT("White shirt"), TEXT("Light blue shirt"), TEXT("Grey tee"), TEXT("Polo, dark green")}},
			{TEXT("Trousers"), {TEXT("Navy suit trousers"), TEXT("Chinos, sand"), TEXT("Dark jeans")}},
			{TEXT("Shoes"), {TEXT("Black oxfords"), TEXT("Brown brogues"), TEXT("White trainers")}},
			{TEXT("Hats"), {TEXT("Flat cap"), TEXT("Beanie, grey")}},
			{TEXT("Sunglasses"), {TEXT("Aviators"), TEXT("Wayfarer, black")}},
			{TEXT("Ties"), {TEXT("Navy tie"), TEXT("Burgundy tie"), TEXT("Black bow tie")}}};
		if (const TArray<FString>* D = Demo.Find(WardrobeCategory)) for (const FString& Name : *D) Item(Name, FString(), false, TEXT("demo, coming"));
	}
	Browser->ExecuteJavascript(FString::Printf(TEXT("window.__ueWardrobe && window.__ueWardrobe('{\"cat\":\"%s\",\"cats\":[%s],\"items\":[%s]}')"), *Q(WardrobeCategory), *Cats, *Items).Replace(TEXT("\\\""), TEXT("\\\\\\\"")));
}

void AADPHUD::WardrobeFromPage(const FString& Action, const FString& Arg)
{
	if (!bWardrobe) return;
	AWardrobe* Wd = WardrobeOpen.Get();
	if (Action == TEXT("close")) { CloseWardrobe(); return; }
	if (Action == TEXT("category") && AWardrobe::Categories().Contains(Arg))
	{
		WardrobeCategory = Arg;
		FillWardrobe();
		if (Wd) Wd->Look(AWardrobe::ViewFor(Arg), GetOwningPlayerController());
	}
	if (Action == TEXT("wear") && Wd)
	{
		const FVector Me = GetOwningPawn() ? GetOwningPawn()->GetActorLocation() : Wd->GetActorLocation();
		Wd->Dress(Arg, Me);
		FillWardrobe();
	}
}

void AADPHUD::OpenWhoAreYou()
{
	if (bWhoAreYou || !Browser.IsValid()) return;
	bWhoAreYou = true; bWhoAreYouCreator = false; bWhoAreYouBack = false;
	// the menus' page stays up, see-through, over the 3D stage: the world drawn again behind it
	if (bViewer) CloseViewer();
	bGame = false;
	if (GameBox.IsValid()) GameBox->SetVisibility(EVisibility::Visible);
	if (TopBar.IsValid()) TopBar->SetVisibility(EVisibility::Collapsed);
	if (Credit.IsValid()) Credit->SetVisibility(EVisibility::Collapsed);
	if (MiniBox.IsValid()) MiniBox->SetVisibility(EVisibility::Collapsed);
	if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) if (AActor* H = Wk->HumanActor()) H->SetActorHiddenInGame(true);
	Breadcrumb(TEXT("A new career: who are you?"));
	ADPCharacterCreate::BuildConcept(GetWorld(), GetOwningPlayerController(), true);
	ADPCharacterCreate::Nobody();
	Browser->ExecuteJavascript(TEXT("window.__ueWhoAreYou && window.__ueWhoAreYou('choose')"));
	UpdateMouse();
}

void AADPHUD::WhoAreYouFromPage(const FString& Action, const FString& Arg)
{
	if (!bWhoAreYou) return;
	if (Action == TEXT("hover")) ADPCharacterCreate::Choose(Arg == TEXT("she"));
	else if (Action == TEXT("choose")) ADPCharacterCreate::Confirm(Arg == TEXT("she"));
	else if (Action == TEXT("back")) { ADPCharacterCreate::ClearCompany(); ADPCharacterCreate::Back(); bWhoAreYouBack = true; }
	else if (Action == TEXT("partner")) ADPCharacterCreate::ShowPartner(Arg == TEXT("married"));
	else if (Action == TEXT("pet"))
	{
		FString Kind, Hex; if (!Arg.Split(TEXT(":"), &Kind, &Hex)) Kind = Arg;
		ADPCharacterCreate::ShowPet(Kind, Hex.IsEmpty() ? FLinearColor::White : FLinearColor(FColor::FromHex(Hex)));
	}
	else if (Action == TEXT("cancel")) CloseWhoAreYou(false);
	else if (Action == TEXT("done")) CloseWhoAreYou(true, Arg);
}

void AADPHUD::CloseWhoAreYou(bool bGoOn, const FString& Difficulty)
{
	if (!bWhoAreYou) return;
	bWhoAreYou = false;
	ADPCharacterCreate::Teardown(GetWorld(), GetOwningPlayerController());
	if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) if (AActor* H = Wk->HumanActor()) H->SetActorHiddenInGame(false);
	if (Browser.IsValid())
	{
		Browser->ExecuteJavascript(TEXT("window.__ueWhoAreYouClose && window.__ueWhoAreYouClose()"));
		// on: the game's own new-game flow (the pilot, the airline, the Overview); back: the title screen
		const FString D = Difficulty.IsEmpty() ? TEXT("normal") : Difficulty.Left(12).Replace(TEXT("'"), TEXT(""));
		Browser->ExecuteJavascript(bGoOn ? FString::Printf(TEXT("window.__ueNewGameDone && window.__ueNewGameDone('%s', '%s')"), *D, ADPCharacterCreate::UiOnRight() ? TEXT("f") : TEXT("m")) : TEXT("window.__ueNewGameCancel && window.__ueNewGameCancel()"));
	}
	// the menus in front again (they were, before the stage)
	if (!bGame) ToggleGame();
}

void AADPHUD::CloseWardrobe()
{
	if (!bWardrobe) return;
	bWardrobe = false;
	if (AWardrobe* Wd = WardrobeOpen.Get()) { Wd->LookBack(GetOwningPlayerController()); Wd->SetOpen(false); }
	if (Browser.IsValid()) Browser->ExecuteJavascript(TEXT("window.__ueWardrobeClose && window.__ueWardrobeClose()"));
	if (!bGame && GameBox.IsValid()) GameBox->SetVisibility(EVisibility::Collapsed);
	if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) if (AActor* H = Wk->HumanActor()) H->SetActorHiddenInGame(false);
	if (MiniBox.IsValid()) MiniBox->SetVisibility(EVisibility::Visible);
	if (TopBar.IsValid()) TopBar->SetVisibility(bGame ? EVisibility::Collapsed : EVisibility::Visible);
	WardrobeOpen = nullptr;
	WardrobeBox->SetVisibility(EVisibility::Collapsed);
	Breadcrumb(TEXT("In the 3D airport"));
	UpdateMouse();
}

void AADPHUD::ToggleSettings()
{
	if (bHidden) return;
	bSettings = !bSettings;
	// closed: the view is yours again (the middle mouse button works as before Settings opened)
	if (!bSettings) bPointer = false;
	Settings->SetVisibility(bSettings ? EVisibility::Visible : EVisibility::Collapsed);
	UpdateMouse();
}

TSharedRef<SWidget> AADPHUD::LevelRow(bool bBloomRow)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	static const TCHAR* Names[] = {TEXT("OFF"), TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC")};
	for (int32 L = 0; L < 5; ++L)
		Row->AddSlot().AutoWidth().Padding(0, 0, 4, 0)
		[
			SNew(SButton)
			.ButtonColorAndOpacity_Lambda([this, L, bBloomRow]() { return (bBloomRow ? BloomLevel : BlurLevel) == L ? FLinearColor(0.1f, 0.45f, 0.75f, 1.f) : FLinearColor(0.05f, 0.2f, 0.36f, 0.9f); })
			.ContentPadding(FMargin(10, 4))
			.OnClicked_Lambda([this, L, bBloomRow]() { if (bBloomRow) SetBloom(L); else SetBlur(L); return FReply::Handled(); })
			[SNew(STextBlock).Font(Font(9)).ColorAndOpacity(Ink).Text(FText::FromString(Names[L]))]
		];
	return Row;
}

void AADPHUD::SetBloom(int32 Level)
{
	BloomLevel = FMath::Clamp(Level, 0, 4);
	static const int32 Q[] = {0, 1, 3, 4, 5};
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality"))) V->Set(Q[BloomLevel], ECVF_SetByGameSetting);
	GConfig->SetInt(TEXT("ADP.Graphics"), TEXT("Bloom"), BloomLevel, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void AADPHUD::SetBlur(int32 Level)
{
	BlurLevel = FMath::Clamp(Level, 0, 4);
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality"))) V->Set(BlurLevel, ECVF_SetByGameSetting);
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DepthOfFieldQuality"))) V->Set(BlurLevel, ECVF_SetByGameSetting);
	GConfig->SetInt(TEXT("ADP.Graphics"), TEXT("Blur"), BlurLevel, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void AADPHUD::SetFrameGen(bool bOn)
{
	if (ADPFrameGen::Enabled()) bOn = false;   // our own frame generation runs: AMD's stays off (one at a time)
	bFrameGen = bOn;
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FI.Enabled"))) V->Set(bOn ? 1 : 0, ECVF_SetByGameSetting);
	GConfig->SetBool(TEXT("ADP.Graphics"), TEXT("FrameGeneration"), bOn, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void AADPHUD::EnterDrone()
{
	APlayerController* PC = GetOwningPlayerController();
	APawn* Me = GetOwningPawn();
	if (!PC || !Me || Drone.IsValid() || !Cast<AADPWalker>(Me)) return;   // (on foot only: not from a truck)
	if (HoloMap.IsValid() && HoloMap->IsFull()) ToggleMap();
	// the drone starts where your eyes are, looking the way you look
	FVector At = Me->GetPawnViewLocation(); FRotator Look = PC->GetControlRotation();
	if (PC->PlayerCameraManager) { At = PC->PlayerCameraManager->GetCameraLocation(); Look = PC->PlayerCameraManager->GetCameraRotation(); }
	FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADroneCam* D = GetWorld()->SpawnActor<ADroneCam>(At, FRotator(0.f, Look.Yaw, 0.f), S);
	if (!D) return;
	DroneFrom = Me; DroneFromLook = PC->GetControlRotation();
	D->OnLeave = [this]() { LeaveDrone(); };
	PC->Possess(D);
	PC->SetControlRotation(Look);
	if (PC->PlayerCameraManager) { PC->PlayerCameraManager->ViewPitchMin = -89.f; PC->PlayerCameraManager->SetGameCameraCutThisFrame(); }
	Drone = D;
	// the Buttons panel open with the drone's controls (as it was before once you land)
	bButtonsBeforeDrone = bButtonsOpen; bButtonsOpen = true;
	RefreshControls();
	UpdateMouse();
	ADP_LOG(TEXT("Drone mode: flying from %s"), *At.ToString());
}

void AADPHUD::LeaveDrone()
{
	if (!Drone.IsValid()) return;
	APlayerController* PC = GetOwningPlayerController();
	ADroneCam* D = Drone.Get();
	Drone = nullptr;
	if (PC && DroneFrom.IsValid()) { PC->Possess(DroneFrom.Get()); PC->SetControlRotation(DroneFromLook); if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame(); }
	D->Destroy();
	bButtonsOpen = bButtonsBeforeDrone;
	RefreshControls();
	UpdateMouse();
	ADP_LOG(TEXT("Drone mode: back on your feet"));
}

void AADPHUD::OnEscape()
{
	if (bWhoAreYou) { CloseWhoAreYou(false); return; }
	// on the loading screen: straight back to the menus
	if (bLoading) { StopLoading(); if (!bGame) ToggleGame(); return; }
	// the phone or a computer up: put away (the page handles its own Esc while it has the keys)
	if (bDevice || PhoneDir != 0) { PutDeviceAway(); return; }
	// in the menus the page handles Esc itself (the Overview asks before leaving to the title screen)
	if (bGame) return;
	if (bHidden) { ShowUI(); return; }
	// in the 3D world: an open panel closes first, back into the 3D view
	if (HoloMap.IsValid() && HoloMap->IsFull()) { ToggleMap(); return; }
	if (bSettings) { ToggleSettings(); return; }
	if (bWardrobe) { CloseWardrobe(); return; }
	// flying the drone: back to where you stood
	if (Drone.IsValid()) { LeaveDrone(); return; }
	// nothing open: the Overview, always
	ToggleGame();
	if (Browser.IsValid()) Browser->ExecuteJavascript(TEXT("window.__ueOverview && window.__ueOverview()"));
}

void AADPHUD::TogglePointer()
{
	if (bGame || bHidden || bSettings) return;   // (with Settings open the pointer is free anyway)
	bPointer = !bPointer;
	UpdateMouse();
}

void AADPHUD::TogglePerf()
{
	bPerf = !bPerf;
	Perf->SetVisibility(bPerf ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}

void AADPHUD::HideUI()
{
	bHidden = true;
	if (HoloMap->IsFull()) ToggleMap();
	if (bSettings) ToggleSettings();
	Root->SetVisibility(EVisibility::Collapsed);
}

void AADPHUD::ShowUI()
{
	if (bHidden) { bHidden = false; Root->SetVisibility(EVisibility::SelfHitTestInvisible); return; }
	// Esc while a panel is open closes it
	if (bGame) ToggleGame();
	else if (bSettings) ToggleSettings();
	else if (HoloMap->IsFull()) ToggleMap();
}

TArray<TPair<FString, uint8>> AADPHUD::Calls;

TWeakObjectPtr<AADPHUD> AADPHUD::Current;

void AADPHUD::Announce(const FString& Words, uint8 Kind)
{
	if (Calls.Num() < 8) Calls.Add(TPair<FString, uint8>(Words, Kind));
	// the same words, as text, in the game's messages on the Overview (heard only in the 3D view, read anywhere)
	if (AADPHUD* H = Current.Get())
		if (H->Browser.IsValid())
		{
			const FString Safe = Words.Replace(TEXT("\\"), TEXT(" ")).Replace(TEXT("'"), TEXT("\\'"));
			H->Browser->ExecuteJavascript(FString::Printf(TEXT("window.__ueNotice && window.__ueNotice(%d, '%s')"), int32(Kind), *Safe));
		}
}

void AADPHUD::PlayGong()
{
	// the classic airport call: two tones, E then C, made here (no recording needed); gentle on the ear (owner, 2026-11-09)
	if (!Gong)
	{
		Gong = NewObject<USoundWaveProcedural>(this);
		Gong->SetSampleRate(44100);
		Gong->NumChannels = 1;
		Gong->Duration = 1.6f;
		Gong->SoundGroup = SOUNDGROUP_Default;
		Gong->bLooping = false;
	}
	TArray<int16> Pcm;
	const int32 Rate = 44100;
	for (const float Hz : {659.f, 523.f})
		for (int32 i = 0; i < Rate * 0.8f; ++i)
		{
			const float T = i / float(Rate), Env = FMath::Exp(-T * 3.2f) * FMath::Min(1.f, T * 40.f);
			const float V = (FMath::Sin(2 * PI * Hz * T) + 0.12f * FMath::Sin(4 * PI * Hz * T)) * Env * 0.3f;
			Pcm.Add(int16(V * 32767));
		}
	Gong->ResetAudio();
	Gong->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
	if (GongOut) GongOut->Stop();
	GongOut = UGameplayStatics::CreateSound2D(this, Gong, 1.f, 1.f, 0.f, nullptr, false, false);
	if (GongOut) { GongOut->SetVolumeMultiplier(FMath::Max(0.001f, CallGain())); GongOut->SetLowPassFilterEnabled(true); GongOut->SetLowPassFilterFrequency(ZoneMuffle()); GongOut->Play(); }
}

void AADPHUD::Interact()
{
	// a screen in front of you (a desk's monitor, a timetable): the airline game on it
	APlayerController* PC = GetOwningPlayerController();
	if (bWardrobe) { CloseWardrobe(); return; }
	if (!PC || bGame || bDevice || PhoneDir != 0 || bSettings || bPointer || (HoloMap.IsValid() && HoloMap->IsFull())) return;
	// in a fire truck: E / X gets you out (FireTruck.h)
	if (AFireTruck* T = Cast<AFireTruck>(GetOwningPawn())) { T->Exit(); return; }
	if (Drone.IsValid()) return;   // (a drone presses nothing)
	FVector Eye; FRotator Look;
	PC->GetPlayerViewPoint(Eye, Look);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPInteract), false, GetOwningPawn());
	// reach counted from you, not from the camera: seen from behind (third person) the camera is metres back
	float InteractReach = 300.f;
	if (APawn* Me = GetOwningPawn())
	{
		InteractReach += FVector::Dist(Eye, Me->GetPawnViewLocation());
		if (AADPWalker* Wk = Cast<AADPWalker>(Me)) if (Wk->HumanActor()) Q.AddIgnoredActor(Wk->HumanActor());
	}
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Look.Vector() * InteractReach, ECC_Visibility, Q) && Hit.GetComponent())
	{
		// a fire truck within reach: you get in (FireTruck.h)
		if (AFireTruck* T = AFireTruck::FromActor(Hit.GetActor())) { T->Enter(GetOwningPawn()); return; }
		// the office wardrobe: open, the outfits listed (Wardrobe.h)
		if (AWardrobe* Wd = Cast<AWardrobe>(Hit.GetActor())) { OpenWardrobe(Wd); return; }
		// your phone, the operations monitor, your office's computer (Devices.cpp)
		if (TryDevice(Hit)) return;
		// a hangar door's control button (OPEN, CLOSE, STOP)
		if (AHangar* Hg = Cast<AHangar>(Hit.GetActor())) if (Hg->Press(Hit.GetComponent())) return;
		const FString N = Hit.GetComponent()->GetName();
		if (N.Contains(TEXT("screenPanel")) || N.Contains(TEXT("black"))) { ToggleGame(); return; }
	}
	// a fire truck right beside you that you face: in, even when the aim missed it
	if (AFireTruck* T = AFireTruck::Beside(GetOwningPawn())) T->Enter(GetOwningPawn());
}

namespace
{
	// the lounge stands on its own, high above the airport in clear air (no ground fog reaches it, nobody sees it from
	// the ground): closed in by its own walls and ceiling, its windows looking out on the Babylon edition's own apron
	// picture, lit by its own lamps, fire and picture lights
	const FVector LoungeAt(0, 0, 250000.f);
}

void AADPHUD::ShowTerminal()
{
	Breadcrumb(TEXT("In the terminal (3D)"));
	if (bViewer) CloseViewer();
	if (bGame) ToggleGame();
	// back from the lounge: where you were in the terminal
	if (bInLounge)
	{
		bInLounge = false;
		if (APawn* P = GetOwningPawn())
		{
			P->TeleportTo(BeforeLounge.GetLocation(), BeforeLounge.Rotator());
			if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(BeforeLounge.Rotator());
		}
	}
}

void AADPHUD::OpenLounge()
{
	Breadcrumb(TEXT("In the lounge (3D)"));
	if (bGame) ToggleGame();
	APawn* P = GetOwningPawn();
	if (!P) return;
	if (!Lounge)
	{
		FActorSpawnParameters S;
		S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Lounge = GetWorld()->SpawnActor<ABabylonModel>(LoungeAt, FRotator::ZeroRotator, S);
		if (Lounge && !Lounge->Place(TEXT("Lounge"))) { Lounge->Destroy(); Lounge = nullptr; }
		if (Lounge) Lounge->AddLights(TEXT("Lounge"));
	}
	if (!Lounge) return;
	if (!bInLounge) { BeforeLounge = FTransform(P->GetControlRotation(), P->GetActorLocation()); bInLounge = true; }
	// on the welcome mat inside the doors, facing the windows, as in the Babylon edition (its (4.7, 4.7) in the room)
	const FRotator Face(0, -90.f, 0);
	P->TeleportTo(LoungeAt + FVector(470.f, 470.f, P->GetSimpleCollisionHalfHeight() + 20.f), Face);
	if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(Face);
}

void AADPHUD::OpenSite()
{
	Breadcrumb(TEXT("At the airport site (3D)"));
	ShowTerminal();
	GoEntrance();
}

void AADPHUD::GoTo(const FVector& PlanAt, const FVector& PlanLook)
{
	if (AFireTruck* T = Cast<AFireTruck>(GetOwningPawn())) T->ExitNow();   // out of a truck first: the quick-starts move you, not it
	APawn* P = GetOwningPawn();
	if (!P) return;
	if (bGame) ToggleGame();
	bInLounge = false;
	// the holographic map stays as it is (open, if you jumped from it) until you close it yourself
	// the floor under the spot (a trace down), the body standing on it, then moved out of anything it would touch
	const float Half = P->GetSimpleCollisionHalfHeight();
	FVector At = FTerminalPlan::ToWorld(PlanAt.X, PlanAt.Y, PlanAt.Z);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPGoTo), false, P);
	if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 150.f), At - FVector(0, 0, 300.f), ECC_Visibility, Q)) At = Hit.ImpactPoint;
	At.Z += Half + 5.f;
	const FVector Look = FTerminalPlan::ToWorld(PlanLook.X, PlanLook.Y, PlanLook.Z) - At;
	FRotator Rot = Look.Rotation();
	Rot.Roll = 0;
	GetWorld()->FindTeleportSpot(P, At, FRotator(0, Rot.Yaw, 0));
	P->TeleportTo(At, FRotator(0, Rot.Yaw, 0));
	if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(FRotator(FMath::Clamp(Rot.Pitch, -20.f, 15.f), Rot.Yaw, 0));
}

// gate 4 upstairs: behind all the rows of seats (at the walkway side of the gate room), looking out through its big windows
void AADPHUD::GoGate4() { GoTo(FVector(186.f, Plan.F2Y, 69.6f), FVector(186.f, Plan.F2Y + 1.6f, 90.f)); }
// out on the apron, looking at the terminal: gate 4, its jetway and a good part of the building in one view
void AADPHUD::GoApron() { GoTo(FVector(236.f, 0.f, 168.f), FVector(184.f, 5.f, 84.f)); }
// in front of the main entrance to the hall, at the kerb under the wave roof
void AADPHUD::GoEntrance() { GoTo(FVector(79.f, 0.f, -14.f), FVector(79.f, 1.7f, 10.f)); }
// the control tower (TowerADC): in the cab between the stair and the desks, looking out over the apron
void AADPHUD::GoTowerCab() { GoTo(FVector(ATowerADC::AtX + 2.9f, ATowerADC::CabY, ATowerADC::AtZ - 0.9f), FVector(ATowerADC::AtX + 60.f, ATowerADC::CabY + 1.6f, ATowerADC::AtZ + 4.f)); }
// on the balcony (the walkway round the cab), looking out east over the apron
void AADPHUD::GoTowerBalcony() { GoTo(FVector(ATowerADC::AtX + 6.4f, ATowerADC::CabY - 0.05f, ATowerADC::AtZ - 2.f), FVector(ATowerADC::AtX + 80.f, ATowerADC::CabY, ATowerADC::AtZ - 10.f)); }
// on the ground outside the tower's entrance, looking up at it
void AADPHUD::GoTowerFoot() { GoTo(FVector(ATowerADC::AtX - 14.f, 0.f, ATowerADC::AtZ + 3.f), FVector(ATowerADC::AtX, 14.f, ATowerADC::AtZ)); }
// a hangar: on the apron 38 m out on its lead-in line, looking straight at its doors (the whole front in view); inside,
// at the back of the hall looking out through the door; or in its annex's crew lounge
void AADPHUD::GoFireStation(int32 Where)
{
	// the approved sketch's own views (docs/fire-station/sketch/render.ts: bays, hero, inside), sketch metres to plan
	auto P = [](float Sx, float Sz) { return ADPFireStation::ToPlan(Sx, Sz); };
	FVector2D Eye, At; float AtH = 5.f;
	if (Where == 1) { Eye = P(-17.f, -21.f); At = P(9.f, 3.f); AtH = 5.f; }
	else if (Where == 2) { Eye = P(53.f, 21.f); At = P(ADPFireStation::BayX(1.5f), 6.f); AtH = 2.2f; }
	else if (Where == 3) { const float Hx = (ADPFireStation::BayX(3) + ADPFireStation::BayX(4)) * 0.5f; Eye = P(Hx - 2.5f, -4.5f); At = P(Hx, 0.f); AtH = 1.0f; }   // the wash hose on its wall
	else if (Where == 4) { Eye = P(46.f, -9.f); At = P(46.15f, -8.75f); AtH = 0.f; }      // looking down at your own body
	else { Eye = P(40.f, -36.f); At = P(40.f, 0.f); AtH = 5.2f; }
	GoTo(FVector(Eye.X, 0.f, Eye.Y), FVector(At.X, AtH, At.Y));
}

void AADPHUD::GoHangar(int32 I, int32 Where)
{
	if (!HangarAt.IsValidIndex(I)) return;
	const FHangarSpec& S = FHangarSpec::Get(HangarSize[I]);
	const float X = HangarAt[I].X, Y = HangarAt[I].Y, Door = Y + S.Lip();
	if (Where == 1) GoTo(FVector(X + 6.f, 0.f, Door + S.D - 6.f), FVector(X, 5.f, Door - 20.f));
	else if (Where == 2) { const float Ax = X - S.WallX(), Az = Door + S.Back() - AHangar::AnnexL; GoTo(FVector(Ax - 11.f, 0.f, Az + 1.5f), FVector(Ax - 5.f, 1.2f, Az + 10.f)); }
	else GoTo(FVector(X, 0.f, Y - 38.f), FVector(X, S.DH * 0.45f, Door));
}

void AADPHUD::OnGameState(const FString& Json)
{
	TSharedPtr<FJsonObject> O;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) || !O.IsValid()) return;
	FString Name, Money;
	O->TryGetStringField(TEXT("name"), Name);
	O->TryGetStringField(TEXT("money"), Money);
	Airline = Name;
	StatusLine = Name.IsEmpty() ? FString() : Name.ToUpper() + (Money.IsEmpty() ? FString() : TEXT("   ·   ") + Money);
}

void AADPHUD::ToggleButtons()
{
	if (bHidden) return;
	// one key: with the map full screen it opens or closes only the map's legend; anywhere else the Buttons panel
	if (HoloMap.IsValid() && HoloMap->IsFull()) { HoloMap->ToggleLegend(); return; }
	bButtonsOpen = !bButtonsOpen;
}

void AADPHUD::OnAnyKey(FKey Key)
{
	const bool bPad = Key.IsGamepadKey();
	if (bPad && ADPInput::CurrentPad() == ADPInput::EPad::None)
	{
		const ADPInput::EPad Found = ADPInput::DetectPad();
		ADPInput::CurrentPad() = Found == ADPInput::EPad::None ? ADPInput::EPad::Xbox : Found; // an XInput pad
	}
	ADPInput::UsingPad() = bPad;
}

void AADPHUD::RefreshControls()
{
	const bool bPad = ADPInput::UsingPad();
	const ADPInput::EPad Pad = ADPInput::CurrentPad();
	auto Row = [&](const ADPInput::FAction& A, bool bBoth)
	{
		TSharedRef<SHorizontalBox> R = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 2, 12, 2)
			[SNew(STextBlock).Font(Font(9, "Regular")).ColorAndOpacity(Ink).Text(FText::FromString(A.Name))];
		if (bBoth)
		{
			R->AddSlot().AutoWidth().Padding(0, 2, 6, 2)[ADPInput::Glyph(A, false, Pad)];
			if (Pad != ADPInput::EPad::None) R->AddSlot().AutoWidth().Padding(0, 2)[ADPInput::Glyph(A, true, Pad)];
		}
		else R->AddSlot().AutoWidth().Padding(0, 2)[ADPInput::Glyph(A, bPad, Pad)];
		return R;
	};
	if (ButtonsList.IsValid())
	{
		ButtonsList->ClearChildren();
		// flying the drone: its own controls
		for (const ADPInput::FAction& A : Drone.IsValid() ? ADPInput::DroneActions() : ADPInput::Actions())
			if (A.Name != TEXT("Buttons")) ButtonsList->AddSlot().AutoHeight()[Row(A, false)];
	}
	if (SettingsControls.IsValid())
	{
		SettingsControls->ClearChildren();
		for (const ADPInput::FAction& A : ADPInput::Actions()) SettingsControls->AddSlot().AutoHeight()[Row(A, true)];
	}
	if (ButtonsHeaderGlyph.IsValid())
		if (const ADPInput::FAction* B = ADPInput::Find(TEXT("Buttons"))) ButtonsHeaderGlyph->SetContent(ADPInput::Glyph(*B, bPad, Pad));
	// the map's legend: what the symbols mean, and the controls that matter there
	if (HoloMap.IsValid())
	{
		TArray<FString> L = {TEXT("LEGEND"), TEXT("Bright lines: walls on this floor"), TEXT("Faint lines: the other floor"),
			TEXT("Squares: stairs and lifts"), TEXT("White arrow: you, and where you look"), TEXT("Orange line: your route")};
		if (const ADPInput::FAction* M = ADPInput::Find(TEXT("Map"))) L.Add(ADPInput::Label(*M, bPad, Pad) + TEXT(": open or close the map"));
		L.Add(TEXT("Click where you want to go: a route there"));
		const ADPInput::FAction* B = ADPInput::Find(TEXT("Buttons"));
		HoloMap->SetLegend(L, B ? ADPInput::Label(*B, bPad, Pad) : FString());
	}
}

void AADPHUD::Tick(float Dt)
{
	if (bWardrobe && (++WardrobeTicks == 20 || WardrobeTicks == 90)) FillWardrobe();
	// the stage's transition over: the creator's panel on the free side
	if (bWhoAreYou && !bWhoAreYouCreator && ADPCharacterCreate::Done() && Browser.IsValid())
	{
		bWhoAreYouCreator = true;
		Browser->ExecuteJavascript(FString::Printf(TEXT("window.__ueWhoAreYou && window.__ueWhoAreYou('creator', '%s')"), ADPCharacterCreate::UiOnRight() ? TEXT("right") : TEXT("left")));
	}
	// Back's reverse over: the choice again, the chosen one still in the light
	if (bWhoAreYou && bWhoAreYouBack && ADPCharacterCreate::Choosing() && Browser.IsValid())
	{
		bWhoAreYouBack = false; bWhoAreYouCreator = false;
		Browser->ExecuteJavascript(FString::Printf(TEXT("window.__ueWhoAreYou && window.__ueWhoAreYou('choose', '%s')"), ADPCharacterCreate::UiOnRight() ? TEXT("she") : TEXT("he")));
	}   // (the page's menu again once the page is surely up)
	// from the menus into the 3D airport: the loading screen until everything is in (test runs skip it)
	{
		static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("UIShot")) && !FParse::Param(FCommandLine::Get(), TEXT("UILoading"));
		if (bWasGame && !bGame && !bTest) StartLoading();
		if (bGame && bLoading) StopLoading();
		bWasGame = bGame;
		TickLoading(Dt);
		TickDevice(Dt);
		// the fire station on the holographic map
		if (!bFireOnMap && HoloMap.IsValid()) { TArray<FVector4f> W, D; ADPFireStation::MapOutline(W, D); HoloMap->AddHangar(TEXT("FIRE STATION"), W, D); bFireOnMap = true; }
		// the hangars on the holographic map, once they stand (H1, H2, H3 from west to east, as the airfield lists them)
		if (!bHangarsOnMap && HoloMap.IsValid())
		{
			TArray<AHangar*> Hs;
			for (TActorIterator<AHangar> It(GetWorld()); It; ++It) Hs.Add(*It);
			if (Hs.Num())
			{
				Hs.Sort([](const AHangar& A, const AHangar& B) { return A.GetActorLocation().X < B.GetActorLocation().X; });
				for (int32 i = 0; i < Hs.Num(); ++i) { TArray<FVector4f> W, D; Hs[i]->MapOutline(W, D); HoloMap->AddHangar(FString::Printf(TEXT("HANGAR H%d"), i + 1), W, D); }
				ADP_LOG(TEXT("Holomap: %d hangars on the map"), Hs.Num());
				bHangarsOnMap = true;
			}
		}
	}
	// while the menus fill the screen (opaque: nothing of the 3D world shows), the 3D world isn't drawn behind them, so
	// the title screen and the menus run as smoothly as on Android (two engines drawing at once made them stutter)
	if (GEngine && GEngine->GameViewport)
	{
		const bool bBehind = bGame && GameBox.IsValid() && GameBox->GetVisibility() == EVisibility::Visible;
		if (GEngine->GameViewport->bDisableWorldRendering != bBehind) GEngine->GameViewport->bDisableWorldRendering = bBehind;
	}
	// stuck inside something: the panel (and the pointer free to click it); gone again once you move
	if (UnstuckPanel.IsValid())
	{
		const bool bStuck = IsStuck(Dt);
		const bool bShown = UnstuckPanel->GetVisibility() == EVisibility::Visible;
		if (bStuck && !bShown) { UnstuckPanel->SetVisibility(EVisibility::Visible); if (APlayerController* PC = GetOwningPlayerController()) { PC->SetShowMouseCursor(true); PC->SetInputMode(FInputModeGameAndUI()); } }
		else if (!bStuck && bShown && StuckFor == 0.f && !bPointer) UnstuckPanel->SetVisibility(EVisibility::Collapsed);
	}
	Super::Tick(Dt);
	// the controller to the menus' page whenever it changes (and once more, all released, when they close)
	if (PadToMenus.IsValid() && PadToMenus->bDirty && Browser.IsValid() && (bGame || bPadSent))
	{
		const ADPInput::EPad Pad = ADPInput::CurrentPad();
		const TCHAR* Id = Pad == ADPInput::EPad::PlayStation ? TEXT("DualSense Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 0ce6)")
			: Pad == ADPInput::EPad::Generic ? TEXT("Controller (STANDARD GAMEPAD)") : TEXT("Xbox Controller (STANDARD GAMEPAD Vendor: 045e)");
		FString Ax, Bt;
		for (int32 i = 0; i < 4; ++i) Ax += FString::Printf(TEXT("%s%.3f"), i ? TEXT(",") : TEXT(""), PadToMenus->Axes[i]);
		for (int32 i = 0; i < 17; ++i) Bt += FString::Printf(TEXT("%s%.3f"), i ? TEXT(",") : TEXT(""), PadToMenus->Buttons[i]);
		Browser->ExecuteJavascript(FString::Printf(TEXT("window.__uePad && window.__uePad('%s',[%s],[%s])"), Id, *Ax, *Bt));
		PadToMenus->bDirty = false;
		bPadSent = bGame;
	}
	// the updater's state to the menus' Settings > Updates, whenever it changes
	if (Browser.IsValid())
	{
		const bool bCan = !UpdateBusy && (UpdateChain.Num() || !UpdateFullUrl.IsEmpty());
		const FString Now = FString::Printf(TEXT("%s|%d|%d|%.3f|%.1f"), *UpdateStatus, bCan ? 1 : 0, UpdateBusy ? 1 : 0, UpdateFrac, UpdateSpeed / 1048576.0);
		if (Now != UpdateSent)
		{
			UpdateSent = Now;
			const FString Safe = UpdateStatus.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("'"), TEXT("\\'"));
			Browser->ExecuteJavascript(FString::Printf(TEXT("window.__ueUpdate && window.__ueUpdate('%s', %s, %s, '%s', %.4f, %.2f)"), *Safe, bCan ? TEXT("true") : TEXT("false"), UpdateBusy ? TEXT("true") : TEXT("false"), *UpdaterVersion(), UpdateFrac, UpdateSpeed / 1048576.0));
		}
	}
	// the symbols follow the device in use
	if (bShownPad != ADPInput::UsingPad() || ShownPadKind != uint8(ADPInput::CurrentPad()))
	{
		bShownPad = ADPInput::UsingPad();
		ShownPadKind = uint8(ADPInput::CurrentPad());
		RefreshControls();
	}
	// -UIPack: the announcement preview pack, then the game closes
	if (FParse::Param(FCommandLine::Get(), TEXT("UIPack"))) { static bool bDone = false; if (!bDone) { bDone = true; WritePreviewPack(); FGenericPlatformMisc::RequestExit(false); } }
	// -UIShot: one picture with the interface showing (the title screen), then the game closes (a check of the interface)
	static float ShotClock = 0;
	if (FParse::Param(FCommandLine::Get(), TEXT("UIShot")))
	{
		const float Before = ShotClock;
		ShotClock += Dt;
		if (Before < 45.f && ShotClock >= 45.f) FScreenshotRequest::RequestScreenshot(TEXT("TitleScreen"), true, false);
		// -UIOverview: then Continue game, and a picture of the Overview (your airline's screen)
		const bool bOverview = FParse::Param(FCommandLine::Get(), TEXT("UIOverview"));
		if (bOverview && Before < 50.f && ShotClock >= 50.f && Browser.IsValid())
			Browser->ExecuteJavascript(TEXT("(function(){var b=document.querySelector('[data-id=\"game:continue\"]');if(b)b.click();})()"));
		if (bOverview && Before < 70.f && ShotClock >= 70.f) Announce(TEXT("This is a passenger announcement. Would passenger Ms Anna Hansen, travelling to Copenhagen, please contact the information desk in the main hall. Thank you."), 2);
		if (bOverview && Before < 85.f && ShotClock >= 85.f) FScreenshotRequest::RequestScreenshot(TEXT("Overview"), true, false);
		// -UILounge: the lounge opened as the title screen's Lounge does, and a picture of it
		const bool bLounge = FParse::Param(FCommandLine::Get(), TEXT("UILounge"));
		if (bLounge && Before < 48.f && ShotClock >= 48.f) OpenLounge();
		if (bLounge && Before < 75.f && ShotClock >= 75.f) FScreenshotRequest::RequestScreenshot(TEXT("Lounge"), true, false);
		// -UICall: one passenger call, to hear the announcer
		if (FParse::Param(FCommandLine::Get(), TEXT("UICall")) && Before < 40.f && ShotClock >= 40.f)
			if (bGame) ToggleGame();
		if (FParse::Param(FCommandLine::Get(), TEXT("UICall")) && Before < 40.f && ShotClock >= 40.f)
			Announce(TEXT("This is a passenger announcement. Would passenger Ms Anna Hansen, travelling to Copenhagen, please contact the information desk in the main hall. Thank you."), 2);
		// -UIWalk: out of the menus, W held for two seconds as a real key press arrives, then the middle mouse twice;
		// the log says how far you walked and whether the pointer freed and came back
		if (FParse::Param(FCommandLine::Get(), TEXT("UIWalk")))
		{
			static FVector WalkFrom;
			APlayerController* PC = GetOwningPlayerController();
			APawn* Me = GetOwningPawn();
			if (Before < 46.f && ShotClock >= 46.f) { ShowTerminal(); if (Me) WalkFrom = Me->GetActorLocation(); }
			if (PC && Before < 47.f && ShotClock >= 47.f) PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Pressed, 1.f));
			if (PC && Before < 49.f && ShotClock >= 49.f)
			{
				PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Released, 0.f));
				ADP_LOG(TEXT("Walk test: moved %.0f cm with W"), Me ? FVector::Dist2D(Me->GetActorLocation(), WalkFrom) : -1.f);
				FPointerEvent Mid(0, FVector2D(400, 300), FVector2D(400, 300), TSet<FKey>({EKeys::MiddleMouseButton}), EKeys::MiddleMouseButton, 0, FModifierKeysState());
				FSlateApplication::Get().ProcessMouseButtonDownEvent(nullptr, Mid);
				FSlateApplication::Get().ProcessMouseButtonUpEvent(Mid);
				ADP_LOG(TEXT("Walk test: middle mouse once: pointer %s"), bPointer ? TEXT("free") : TEXT("not free"));
				FSlateApplication::Get().ProcessMouseButtonDownEvent(nullptr, Mid);
				FSlateApplication::Get().ProcessMouseButtonUpEvent(Mid);
				ADP_LOG(TEXT("Walk test: middle mouse twice: pointer %s, looking %s"), bPointer ? TEXT("free") : TEXT("captured"), PC->IsLookInputIgnored() ? TEXT("off") : TEXT("on"));
				// also to a file (a release build keeps no log)
				FFileHelper::SaveStringToFile(FString::Printf(TEXT("moved %.0f cm; pointer after two middle clicks: %s; looking %s"), Me ? FVector::Dist2D(Me->GetActorLocation(), WalkFrom) : -1.f, bPointer ? TEXT("free") : TEXT("captured"), PC->IsLookInputIgnored() ? TEXT("off") : TEXT("on")), *(FPaths::ProjectSavedDir() / TEXT("WalkTest.txt")));
			}
		}
		// -UIShortcuts: the map's three shortcuts in turn, a picture of each
		const bool bShort = FParse::Param(FCommandLine::Get(), TEXT("UIShortcuts"));
		if (bShort)
		{
			// test pictures in daylight (the real sun may be down); and where each jump really put you
			if (Before < 1.f && ShotClock >= 1.f) { ASunClock::HourOverride = 12.f; ASunClock::Refresh(); }
			for (const float At : {48.f, 58.f, 68.f})
				if (Before < At && ShotClock >= At)
					if (const APawn* Me = GetOwningPawn())
					{
						const FVector L = Me->GetActorLocation();
						ADP_LOG(TEXT("Shortcut test: at plan x %.1f z %.1f, feet %.2f m"), L.X / 100.f, -L.Y / 100.f, (L.Z - Me->GetSimpleCollisionHalfHeight()) / 100.f);
					}
			if (Before < 46.f && ShotClock >= 46.f) GoGate4();
			if (Before < 54.f && ShotClock >= 54.f) FScreenshotRequest::RequestScreenshot(TEXT("ShortcutGate4"), true, false);
			if (Before < 56.f && ShotClock >= 56.f) GoApron();
			if (Before < 64.f && ShotClock >= 64.f) FScreenshotRequest::RequestScreenshot(TEXT("ShortcutApron"), true, false);
			if (Before < 66.f && ShotClock >= 66.f) GoEntrance();
			if (Before < 74.f && ShotClock >= 74.f) FScreenshotRequest::RequestScreenshot(TEXT("ShortcutEntrance"), true, false);
		}
		// -UIHall: the main hall under the wave roof with its lights, after dark (the real time now) and at midday
		const bool bHall = FParse::Param(FCommandLine::Get(), TEXT("UIHall"));
		if (bHall)
		{
			if (Before < 46.f && ShotClock >= 46.f) { ASunClock::HourOverride = 21.f; ASunClock::Refresh(); GoTo(FVector(60.f, 0.f, 3.f), FVector(88.f, 7.f, 26.f)); }
			if (Before < 56.f && ShotClock >= 56.f) FScreenshotRequest::RequestScreenshot(TEXT("HallEvening"), false, false);
			if (Before < 57.f && ShotClock >= 57.f) { ASunClock::HourOverride = 12.f; ASunClock::Refresh(); }
			if (Before < 66.f && ShotClock >= 66.f) FScreenshotRequest::RequestScreenshot(TEXT("HallMidday"), false, false);
			// and the long walkway upstairs, looking up at its glass roof (midday)
			if (Before < 67.f && ShotClock >= 67.f) GoTo(FVector(150.f, Plan.F2Y, 57.f), FVector(150.f, Plan.F2Y + 9.f, 62.f));
			if (Before < 70.f && ShotClock >= 70.f) if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(FRotator(60.f, PC->GetControlRotation().Yaw, 0));
			if (Before < 75.f && ShotClock >= 75.f) FScreenshotRequest::RequestScreenshot(TEXT("WalkwayRoof"), false, false);
			// the walkway's wood floor and its strip lamps (evening, so the lamps' own light shows), then gate 4's floor
			if (Before < 76.f && ShotClock >= 76.f) { ASunClock::HourOverride = 19.5f; ASunClock::Refresh(); if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(FRotator(-8.f, 0.f, 0)); }
			if (Before < 84.f && ShotClock >= 84.f) FScreenshotRequest::RequestScreenshot(TEXT("WalkwayFloor"), false, false);
			if (Before < 85.f && ShotClock >= 85.f) { ASunClock::HourOverride = 12.f; ASunClock::Refresh(); GoGate4(); if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(FRotator(-25.f, PC->GetControlRotation().Yaw, 0)); }
			if (Before < 93.f && ShotClock >= 93.f) FScreenshotRequest::RequestScreenshot(TEXT("Gate4Floor"), false, false);
			if (Before < 94.f && ShotClock >= 94.f) if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(FRotator(50.f, PC->GetControlRotation().Yaw, 0));
			if (Before < 100.f && ShotClock >= 100.f) FScreenshotRequest::RequestScreenshot(TEXT("Gate4Ceiling"), false, false);
		}
		// -UIPillar: the tree columns' stone up close, in daylight: a trunk at eye level, a trunk up to its arms, one
		// outside at the kerb
		if (FParse::Param(FCommandLine::Get(), TEXT("UIPillar")))
		{
			APlayerController* PC = GetOwningPlayerController();
			auto Look = [PC](float Pitch) { if (PC) PC->SetControlRotation(FRotator(Pitch, PC->GetControlRotation().Yaw, 0)); };
			if (Before < 44.f && ShotClock >= 44.f) { ASunClock::HourOverride = 12.f; ASunClock::Theme = 1; ASunClock::Refresh(); }
			if (Before < 46.f && ShotClock >= 46.f) { GoTo(FVector(75.f, 0.f, 9.5f), FVector(75.f, 1.6f, 6.f)); Look(0.f); }
			if (Before < 54.f && ShotClock >= 54.f) FScreenshotRequest::RequestScreenshot(TEXT("PillarClose"), false, false);
			if (Before < 55.f && ShotClock >= 55.f) { GoTo(FVector(68.f, 0.f, 12.f), FVector(75.f, 8.f, 6.f)); Look(30.f); }
			if (Before < 63.f && ShotClock >= 63.f) FScreenshotRequest::RequestScreenshot(TEXT("PillarUp"), false, false);
			if (Before < 64.f && ShotClock >= 64.f) { GoTo(FVector(50.f, 0.f, -14.f), FVector(56.f, 4.f, -9.f)); Look(20.f); }
			if (Before < 72.f && ShotClock >= 72.f) FScreenshotRequest::RequestScreenshot(TEXT("PillarKerb"), false, false);
			if (Before < 75.f && ShotClock >= 75.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIRoof: the wave roof from outside in daylight: under its edge at the kerb, from across the road, from behind
		if (FParse::Param(FCommandLine::Get(), TEXT("UIRoof")))
		{
			APlayerController* PC = GetOwningPlayerController();
			auto Look = [PC](float Pitch) { if (PC) PC->SetControlRotation(FRotator(Pitch, PC->GetControlRotation().Yaw, 0)); };
			if (Before < 44.f && ShotClock >= 44.f) { ASunClock::HourOverride = 12.f; ASunClock::Theme = 1; ASunClock::Refresh(); }
			if (Before < 46.f && ShotClock >= 46.f) { GoTo(FVector(60.f, 0.f, -16.f), FVector(75.f, 14.f, 0.f)); Look(32.f); }
			if (Before < 54.f && ShotClock >= 54.f) FScreenshotRequest::RequestScreenshot(TEXT("RoofKerb"), false, false);
			if (Before < 55.f && ShotClock >= 55.f) { GoTo(FVector(75.f, 0.f, -55.f), FVector(75.f, 12.f, 0.f)); Look(14.f); }
			if (Before < 63.f && ShotClock >= 63.f) FScreenshotRequest::RequestScreenshot(TEXT("RoofAcross"), false, false);
			if (Before < 64.f && ShotClock >= 64.f) { GoTo(FVector(30.f, 0.f, -30.f), FVector(90.f, 12.f, 10.f)); Look(16.f); }
			if (Before < 72.f && ShotClock >= 72.f) FScreenshotRequest::RequestScreenshot(TEXT("RoofSide"), false, false);
			if (Before < 73.f && ShotClock >= 73.f) { GoTo(FVector(70.f, 0.f, -16.f), FVector(80.f, 0.f, -8.f)); Look(-28.f); }
			if (Before < 80.f && ShotClock >= 80.f) FScreenshotRequest::RequestScreenshot(TEXT("KerbPaving"), false, false);
			if (Before < 81.f && ShotClock >= 81.f) { GoTo(FVector(72.f, 0.f, -18.5f), FVector(76.f, 0.f, -21.f)); Look(-35.f); }
			if (Before < 88.f && ShotClock >= 88.f) FScreenshotRequest::RequestScreenshot(TEXT("CurbRoad"), false, false);
			if (Before < 90.f && ShotClock >= 90.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UINew: the grass on the airfield near and far, then the 3D viewer following a made-up flight off the south coast
		if (FParse::Param(FCommandLine::Get(), TEXT("UINew")))
		{
			APlayerController* PC = GetOwningPlayerController();
			auto Look = [PC](float Pitch) { if (PC) PC->SetControlRotation(FRotator(Pitch, PC->GetControlRotation().Yaw, 0)); };
			if (Before < 44.f && ShotClock >= 44.f) { if (bGame) ToggleGame(); ASunClock::HourOverride = 13.f; ASunClock::Theme = 1; ASunClock::Refresh(); }
			if (Before < 46.f && ShotClock >= 46.f) { GoTo(FVector(-200.f, 0.f, 150.f), FVector(-180.f, 0.f, 200.f)); Look(-12.f); }
			if (Before < 60.f && ShotClock >= 60.f) FScreenshotRequest::RequestScreenshot(TEXT("GrassNear"), false, false);
			if (Before < 61.f && ShotClock >= 61.f) Look(-1.f);
			if (Before < 66.f && ShotClock >= 66.f) FScreenshotRequest::RequestScreenshot(TEXT("GrassFar"), false, false);
			if (Before < 67.f && ShotClock >= 67.f) OpenViewer();
			if (bViewer && ShotClock >= 67.f && ShotClock < 84.f)
			{
				static double T0 = FPlatformTime::Seconds();
				const double T = FPlatformTime::Seconds() - T0;
				OnViewerFlight(FString::Printf(TEXT("{\"me\":{\"id\":\"T1\",\"callsign\":\"ADP 214\",\"airline\":\"Airport Dynasty\",\"type\":\"A320\",\"reg\":\"OY-ADP\",\"from\":\"BIMS\",\"fromName\":\"Myrdalssandur\",\"to\":\"EKCH\",\"toName\":\"Copenhagen\",\"phase\":\"Climb\",\"pax\":152,\"paxMax\":180,\"lat\":%.6f,\"lon\":%.6f,\"altM\":%.1f,\"hdg\":120,\"gs\":290,\"vs\":1800,\"onGround\":false}}"),
					63.40 - T * 0.0004, -18.60 + T * 0.0009, 1500.0 + T * 9.0));
			}
			if (Before < 80.f && ShotClock >= 80.f) FScreenshotRequest::RequestScreenshot(TEXT("Viewer"), true, false);
			if (Before < 84.f && ShotClock >= 84.f) CloseViewer();
			if (Before < 90.f && ShotClock >= 90.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIMap: the full-screen map from inside the tower's cab, a jetway and the car rental centre (and small, in the cab)
		if (FParse::Param(FCommandLine::Get(), TEXT("UIMap")))
		{
			auto Open = [this]() { if (HoloMap.IsValid() && !HoloMap->IsFull()) ToggleMap(); };
			if (Before < 44.f && ShotClock >= 44.f) { if (bGame) ToggleGame(); GoTowerCab(); }
			if (Before < 48.f && ShotClock >= 48.f) FScreenshotRequest::RequestScreenshot(TEXT("Map_TowerSmall"), true, false);
			if (Before < 49.f && ShotClock >= 49.f) Open();
			if (Before < 51.f && ShotClock >= 51.f) FScreenshotRequest::RequestScreenshot(TEXT("Map_Tower"), true, false);
			if (Before < 52.f && ShotClock >= 52.f) GoTo(FVector(191.f, 6.f, 88.f), FVector(191.f, 6.f, 100.f));
			if (Before < 55.f && ShotClock >= 55.f) Open();
			if (Before < 57.f && ShotClock >= 57.f) FScreenshotRequest::RequestScreenshot(TEXT("Map_Jetway"), true, false);
			if (Before < 58.f && ShotClock >= 58.f) GoTo(FVector(240.f, 0.f, 20.f), FVector(250.f, 0.f, 20.f));
			if (Before < 61.f && ShotClock >= 61.f) Open();
			if (Before < 63.f && ShotClock >= 63.f) FScreenshotRequest::RequestScreenshot(TEXT("Map_Rental"), true, false);
			// the hangars: the corner map at hangar H1, then the full map zoomed out to the whole airport
			if (Before < 64.f && ShotClock >= 64.f && HangarAt.Num()) { if (HoloMap.IsValid() && HoloMap->IsFull()) ToggleMap(); GoHangar(0); }
			if (Before < 70.f && ShotClock >= 70.f) FScreenshotRequest::RequestScreenshot(TEXT("Map_HangarSmall"), true, false);
			if (Before < 71.f && ShotClock >= 71.f) { Open(); if (HoloMap.IsValid()) HoloMap->ShowAll(); }
			if (Before < 73.f && ShotClock >= 73.f) FScreenshotRequest::RequestScreenshot(TEXT("Map_AllHangars"), true, false);
			if (Before < 76.f && ShotClock >= 76.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIFire: the lounge's fireplace, its fire burning (two pictures a moment apart)
		if (FParse::Param(FCommandLine::Get(), TEXT("UIFire")))
		{
			if (Before < 44.f && ShotClock >= 44.f) { if (bGame) ToggleGame(); OpenLounge(); }
			if (Before < 48.f && ShotClock >= 48.f && Lounge && Lounge->FireLights.Num())
				if (APawn* Me = GetOwningPawn())
				{
					const FVector Out = (Lounge->FireLights[0]->GetComponentLocation() - Lounge->FireWorld).GetSafeNormal2D();
					const FVector Eye = Lounge->FireWorld + Out * 380.f + FVector(0, 0, 60.f);
					Me->TeleportTo(Eye, FRotator::ZeroRotator);
					if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation((Lounge->FireWorld + FVector(0, 0, 45.f) - (Eye + FVector(0, 0, 60.f))).Rotation());
				}
			// a short run of pictures, a fifth of a second apart (made into a moving picture afterwards)
			for (int32 k = 0; k < 16; ++k)
			{
				const float At = 56.f + k * 0.2f;
				if (Before < At && ShotClock >= At) FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Fire_%02d"), k), false, false);
			}
			if (Before < 62.f && ShotClock >= 62.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIBright: the views the owner found too bright: the tower's door, gate 4, the apron at night, the lounge
		if (FParse::Param(FCommandLine::Get(), TEXT("UIBright")))
		{
			auto Sun = [](float H) { ASunClock::HourOverride = H; ASunClock::Theme = 1; ASunClock::Refresh(); };
			if (Before < 44.f && ShotClock >= 44.f) { if (bGame) ToggleGame(); Sun(12.f); GoTowerFoot(); }
			if (Before < 52.f && ShotClock >= 52.f) FScreenshotRequest::RequestScreenshot(TEXT("Bright_TowerDoor"), false, false);
			if (Before < 53.f && ShotClock >= 53.f) GoGate4();
			if (Before < 60.f && ShotClock >= 60.f) FScreenshotRequest::RequestScreenshot(TEXT("Bright_Gate4"), false, false);
			if (Before < 61.f && ShotClock >= 61.f) { Sun(22.f); GoApron(); }
			if (Before < 71.f && ShotClock >= 71.f) FScreenshotRequest::RequestScreenshot(TEXT("Bright_ApronNight"), false, false);
			if (Before < 72.f && ShotClock >= 72.f) { Sun(12.f); OpenLounge(); }
			if (Before < 82.f && ShotClock >= 82.f) FScreenshotRequest::RequestScreenshot(TEXT("Bright_Lounge"), false, false);
			if (Before < 86.f && ShotClock >= 86.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UITower: the control tower in pictures: from the apron, at its door, from the balcony and in the cab, at midday
		// and in the golden hour, then the same at cinematic quality in 4K
		if (FParse::Param(FCommandLine::Get(), TEXT("UITower")))
		{
			auto Sun = [](float H) { ASunClock::HourOverride = H; ASunClock::Theme = 1; ASunClock::Refresh(); };
			auto Wide = [this]() { GoTo(FVector(ATowerADC::AtX + 70.f, 0.f, ATowerADC::AtZ + 45.f), FVector(ATowerADC::AtX, 13.f, ATowerADC::AtZ)); };
			auto Hq = [this](const TCHAR* N) { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(TEXT("HighResShot 2")); ADP_LOG(TEXT("TOWERSHOT %s"), N); };
			struct FStep { float At; TFunction<void()> Do; };
			const FStep TourSteps[] = {
				{44.f, [&]() { if (bGame) ToggleGame(); Sun(13.f); Wide(); }},
				{53.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Tower_Day_Apron"), false, false); }},
				{54.f, [this]() { GoTowerFoot(); }},
				{61.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Tower_Day_Door"), false, false); }},
				{62.f, [this]() { GoTowerBalcony(); }},
				{69.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Tower_Day_Balcony"), false, false); }},
				{70.f, [this]() { GoTowerCab(); }},
				{77.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Tower_Day_Cab"), false, false); }},
				{78.f, [&]() { Sun(15.7f); Wide(); }},
				{87.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Tower_Golden_Apron"), false, false); }},
				{88.f, [this]() { GoTowerFoot(); }},
				{95.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Tower_Golden_Door"), false, false); }},
				{96.f, [this]() { GoTowerBalcony(); }},
				{103.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Tower_Golden_Balcony"), false, false); }},
				{104.f, [this]() { GoTowerCab(); }},
				{111.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Tower_Golden_Cab"), false, false); }},
				// cinematic quality, 4K
				{112.f, [&]() { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(TEXT("scalability 4")); Sun(13.f); Wide(); }},
				{124.f, [&]() { Hq(TEXT("Tower_HQ_Day_Apron")); }},
				{126.f, [this]() { GoTowerBalcony(); }},
				{134.f, [&]() { Hq(TEXT("Tower_HQ_Day_Balcony")); }},
				{136.f, [&]() { Sun(15.7f); Wide(); }},
				{146.f, [&]() { Hq(TEXT("Tower_HQ_Golden_Apron")); }},
				{148.f, [this]() { GoTowerCab(); }},
				{156.f, [&]() { Hq(TEXT("Tower_HQ_Golden_Cab")); }},
				{162.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			// -UITowerHQ: only the cinematic 4K pictures (started at once)
			const bool bHqOnly = FParse::Param(FCommandLine::Get(), TEXT("UITowerHQ"));
			const float Off = bHqOnly ? 68.f : 0.f;
			for (const FStep& St : TourSteps)
				if ((!bHqOnly || St.At >= 112.f) && Before < St.At - Off && ShotClock >= St.At - Off) St.Do();
			if (bHqOnly && Before < 44.f && ShotClock >= 44.f && bGame) ToggleGame();
		}
		// -UIHangars: the hangars in pictures: outside the first two (their doors open and shut, the light masts at their
		// corners), inside the first (looking out), its crew lounge; then at dusk outside and inside with the lamps lit
		if (FParse::Param(FCommandLine::Get(), TEXT("UIHangars")) && HangarAt.Num())
		{
			auto Sun = [](float H) { ASunClock::HourOverride = H; ASunClock::Theme = 1; ASunClock::Refresh(); };
			const int32 Two = FMath::Min(1, HangarAt.Num() - 1);
			// (twice the window's size, as the game draws it: no interface, no filter; -ShotScale=N for another factor)
			auto Shot = [this](const TCHAR* Name) { int32 K = 2; FParse::Value(FCommandLine::Get(), TEXT("ShotScale="), K); if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot %d filename=%s"), FMath::Clamp(K, 1, 4), Name)); };
			struct FStep { float At; TFunction<void()> Do; };
			const FStep HangarSteps[] = {
				{40.f, [&]() { if (bGame) ToggleGame(); Sun(13.f); GoHangar(0); if (APlayerController* P = GetOwningPlayerController()) P->ConsoleCommand(TEXT("r.HighResScreenshotDelay 12")); }},
				{58.f, [&]() { Shot(TEXT("Hangar_Day_Outside1")); }},
				// (the ground's layers at a few airfield spots: every surface straight down, its height in plan metres)
				{58.5f, [&]() { for (const FVector2D& Pt : { FVector2D(-280, 1150), FVector2D(-280, 1086), FVector2D(-530, 990), FVector2D(0, 906), FVector2D(800, 906), FVector2D(800, 700) }) {
					TArray<FHitResult> Hits; FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPLayers), true); const FVector Top = FTerminalPlan::ToWorld(Pt.X, 300.f, Pt.Y), Bot = FTerminalPlan::ToWorld(Pt.X, -30.f, Pt.Y);
					FHitResult H; FVector From = Top; FString L;
					for (int32 n = 0; n < 6 && GetWorld()->LineTraceSingleByChannel(H, From, Bot, ECC_Visibility, Q); ++n) { L += FString::Printf(TEXT(" | %s %.2f m"), H.GetActor() ? *H.GetActor()->GetClass()->GetName() : TEXT("-"), H.ImpactPoint.Z / 100.f); Q.AddIgnoredComponent(H.GetComponent()); From = H.ImpactPoint + FVector(0, 0, 1.f); }
					ADP_LOG(TEXT("Ground layers at plan (%.0f, %.0f):%s"), Pt.X, Pt.Y, *L); } }},
				// (what lies under you in front of the hangar: the first thing straight down, its part and material, in the log)
				{59.f, [&]() { if (APawn* P = GetOwningPawn()) { FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPGround), true, P); TArray<FHitResult> Hits;
					const FVector At = P->GetActorLocation(); GetWorld()->LineTraceMultiByChannel(Hits, At + FVector(0, 0, 50.f), At - FVector(0, 0, 3000.f), ECC_Visibility, Q);
					for (const FHitResult& H : Hits) ADP_LOG(TEXT("Under the hangar apron spot: %s / %s, material %s, %.1f cm below the feet"), H.GetActor() ? *H.GetActor()->GetName() : TEXT("-"), H.GetComponent() ? *H.GetComponent()->GetName() : TEXT("-"),
						H.GetComponent() && Cast<UPrimitiveComponent>(H.GetComponent())->GetMaterial(0) ? *Cast<UPrimitiveComponent>(H.GetComponent())->GetMaterial(0)->GetName() : TEXT("-"), (At.Z - P->GetSimpleCollisionHalfHeight() - H.ImpactPoint.Z)); } }},
				{60.f, [&]() { GoHangar(Two); }},
				{68.f, [&]() { Shot(TEXT("Hangar_Day_Outside2")); }},
				{70.f, [&]() { GoHangar(0, 1); }},
				{78.f, [&]() { Shot(TEXT("Hangar_Day_Inside")); }},
				{80.f, [&]() { GoHangar(0, 2); }},
				{88.f, [&]() { Shot(TEXT("Hangar_Day_Lounge")); }},
				{90.f, [&]() { Sun(17.6f); GoHangar(0); }},
				{100.f, [&]() { Shot(TEXT("Hangar_Dusk_Outside")); }},
				{102.f, [&]() { GoHangar(0, 1); }},
				{110.f, [&]() { Shot(TEXT("Hangar_Dusk_Inside")); }},
				{114.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			for (const FStep& St : HangarSteps)
				if (Before < St.At && ShotClock >= St.At) St.Do();
		}
		// -UIMasts: the apron light masts in pictures: the row from the apron, a mast's foot and its head, by day and at night
		if (FParse::Param(FCommandLine::Get(), TEXT("UIMasts")))
		{
			auto Sun = [](float H) { ASunClock::HourOverride = H; ASunClock::Theme = 1; ASunClock::Refresh(); };
			const float MX = 38.71f, MZ = 90.f;      // mast 3, beside jetway 2
			auto Row = [this]() { GoTo(FVector(150.f, 8.f, 150.f), FVector(60.f, 12.f, 90.f)); };
			auto Foot = [this, MX, MZ]() { GoTo(FVector(MX + 6.f, 1.7f, MZ + 7.f), FVector(MX, 1.2f, MZ)); };
			auto Head = [this, MX, MZ]() { GoTo(FVector(MX + 14.f, 24.f, MZ + 16.f), FVector(MX, 29.f, MZ)); };
			struct FStep { float At; TFunction<void()> Do; };
			const FStep MastSteps[] = {
				{44.f, [&]() { if (bGame) ToggleGame(); Sun(13.f); Row(); }},
				{53.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Masts_Day_Row"), false, false); }},
				{54.f, [&]() { Foot(); }},
				{61.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Masts_Day_Foot"), false, false); }},
				{62.f, [&]() { Head(); }},
				{69.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Masts_Day_Head"), false, false); }},
				{70.f, [&]() { Sun(22.5f); Row(); }},
				{80.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Masts_Night_Row"), false, false); }},
				{81.f, [&]() { Head(); }},
				{88.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Masts_Night_Head"), false, false); }},
				{89.f, [&]() { Foot(); }},
				{96.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Masts_Night_Foot"), false, false); }},
				{100.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			for (const FStep& St : MastSteps)
				if (Before < St.At && ShotClock >= St.At) St.Do();
		}
		// -UIOffices: the old office's things in Office 1 (yours) and Office 3, the operations room's and the Academy's
		if (FParse::Param(FCommandLine::Get(), TEXT("UIOffices")))
		{
			struct FStep { float At; TFunction<void()> Do; };
			const FStep OSteps[] = {
				{44.f, [&]() { if (bGame) ToggleGame(); ASunClock::HourOverride = 13.f; ASunClock::Refresh(); GoTo(FVector(130.f, 0.f, 5.f), FVector(117.f, 1.2f, 5.f)); }},
				{52.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Office1_FromDoor"), false, false); }},
				{53.f, [&]() { GoTo(FVector(123.f, 0.f, 9.f), FVector(117.f, 0.6f, 3.f)); }},
				{60.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Office1_Side"), false, false); }},
				{61.f, [&]() { GoTo(FVector(119.5f, 0.f, 2.f), FVector(117.f, 0.9f, 5.f)); }},
				{68.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Office1_Desk"), false, false); }},
				{69.f, [&]() { GoTo(FVector(138.f, 0.f, 25.f), FVector(152.f, 1.2f, 25.f)); }},
				{76.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Office3_FromDoor"), false, false); }},
				// from the chair's side: the drawers, the monitor and the laptop facing you
				{77.f, [&]() { GoTo(FVector(114.5f, 0.f, 6.2f), FVector(116.4f, 0.8f, 5.f)); }},
				{84.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Office1_Chair"), false, false); }},
				{85.f, [&]() { GoTo(FVector(131.f, 0.f, 28.f), FVector(115.5f, 1.f, 28.f)); }},
				{92.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Ops_FromDoor"), false, false); }},
				{93.f, [&]() { GoTo(FVector(137.5f, 0.f, 10.f), FVector(154.f, 1.2f, 10.f)); }},
				{100.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Academy_FromDoor"), false, false); }},
				{101.f, [&]() { GoTo(FVector(147.f, 0.f, 4.f), FVector(152.f, 0.8f, 9.f)); }},
				{108.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Academy_Desks"), false, false); }},
				// your pet asleep in its bed, the bowls and the football
				{109.f, [&]() { GoTo(FVector(120.6f, 0.f, 9.4f), FVector(118.3f, 0.15f, 7.2f)); }},
				{116.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Office1_Pet"), false, false); }},
				// the operations desk and its monitor (the one for this window's shape), from the chair
				{117.f, [&]() { GoTo(FVector(117.6f, 0.f, 28.8f), FVector(115.3f, 1.05f, 28.f)); }},
				{124.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Ops_Desk"), false, false); }},
				{126.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			for (const FStep& St : OSteps)
				if (Before < St.At && ShotClock >= St.At) St.Do();
		}
		// -UIDevice: your phone taken up off your office's desk (on its way, its apps open), put down; the operations
		// room's computer (its desktop and taskbar), put away
		if (FParse::Param(FCommandLine::Get(), TEXT("UIDevice")))
		{
			struct FStep { float At; TFunction<void()> Do; };
			const FStep DSteps[] = {
				{44.f, [&]() { if (bGame) ToggleGame(); ASunClock::HourOverride = 13.f; ASunClock::Refresh(); GoTo(FVector(117.6f, 0.f, 6.4f), FVector(115.85f, 0.8f, 5.42f)); }},
				{50.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Device_PhoneOnDesk"), false, false); }},
				{51.f, [&]() { PickPhoneNear(FTerminalPlan::ToWorld(115.85f, 0.8f, 5.42f)); }},
				{51.3f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Device_PhoneLifting"), true, false); }},
				{62.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Device_PhoneOpen"), true, false); }},
				{63.f, [&]() { PutDeviceAway(); }},
				{63.5f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Device_PhoneLowering"), true, false); }},
				{66.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Device_PhoneBack"), false, false); }},
				{67.f, [&]() { GoTo(FVector(117.3f, 0.4f, 29.3f), FVector(115.3f, 1.0f, 28.f)); }},
				{72.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Device_OpsDesk"), true, false); }},
				{72.2f, [&]() { for (TActorIterator<AOpsMonitors> It(GetWorld()); It; ++It) It->Describe(); }},
				{86.2f, [&]() { for (TActorIterator<AOpsMonitors> It(GetWorld()); It; ++It) It->Describe(); }},
				{73.f, [&]() { OpenDevice(TEXT("ops")); }},
				{82.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Device_OpsComputer"), true, false); }},
				{83.f, [&]() { PutDeviceAway(); }},
				{86.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Device_OpsBack"), false, false); }},
				{86.5f, [&]() { GoTo(FVector(116.25f, -0.1f, 28.f), FVector(115.3f, 1.13f, 28.f)); }},
				{87.5f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Device_OpsScreenClose"), false, false); }},
				{90.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			for (const FStep& St : DSteps)
				if (Before < St.At && ShotClock >= St.At) St.Do();
		}
		// -UIFireStation: the fire station in pictures, at twice the window's size as the game draws it: straight on, from the
		// side, inside by the trucks; at dusk outside; then the full map zoomed out with it on
		if (FParse::Param(FCommandLine::Get(), TEXT("UIFireStation")))
		{
			// (the forecourt faces north, to the taxiway: in the low winter sun it stands in its own shadow; a June evening, the sun
			// in the west-north-west, lights it)
			auto Sun = [](float H, int32 Day = -1) { ASunClock::DayOverride = Day; ASunClock::HourOverride = H; ASunClock::Theme = 1; ASunClock::Refresh(); };
			auto Shot = [this](const TCHAR* Name) { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 2 filename=%s"), Name)); };
			struct FStep { float At; TFunction<void()> Do; };
			const FStep FSteps[] = {
				{40.f, [&]() { if (bGame) ToggleGame(); Sun(21.f, 172); GoFireStation(0); if (APlayerController* P = GetOwningPlayerController()) P->ConsoleCommand(TEXT("r.HighResScreenshotDelay 12")); }},
				{58.f, [&]() { Shot(TEXT("Fire_Day_Front")); }},
				{60.f, [&]() { GoFireStation(1); }},
				{68.f, [&]() { Shot(TEXT("Fire_Day_Side")); }},
				{70.f, [&]() { GoFireStation(2); }},
				{78.f, [&]() { Shot(TEXT("Fire_Inside_Trucks")); }},
				{79.f, [&]() { GoFireStation(3); }},
				{86.f, [&]() { Shot(TEXT("Fire_Hose")); }},
				{87.f, [&]() { GoFireStation(4); if (APlayerController* P = GetOwningPlayerController()) P->SetControlRotation(FRotator(-70.f, P->GetControlRotation().Yaw, 0)); }},   // looking right down at your body
				{93.f, [&]() { Shot(TEXT("Body_LookDown")); }},
				{94.f, [&]() { Sun(17.6f); GoFireStation(0); }},
				{104.f, [&]() { Shot(TEXT("Fire_Dusk_Front")); }},
				{106.f, [&]() { Sun(21.f, 172); if (HoloMap.IsValid() && !HoloMap->IsFull()) ToggleMap(); if (HoloMap.IsValid()) HoloMap->ShowAll(); }},
				{109.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Fire_Map"), true, false); }},
				{112.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			for (const FStep& St : FSteps)
				if (Before < St.At && ShotClock >= St.At) St.Do();
		}
		// -UIPeople[=MH_016_passenger,MH_024_passenger,...] (without a list: the first two): each MetaHuman person named
		// (Content/People/Built) stands before you in the lit fire station hall, facing you: a full-length picture, then
		// their face close up (the view narrowed to 30 degrees, 1.2 m off, the camera turned to their own eyes' height);
		// People_<name>_Body and _Face, ten seconds a person, then the game closes
		if ((FParse::Param(FCommandLine::Get(), TEXT("UIPeople")) || FCString::Stristr(FCommandLine::Get(), TEXT("-UIPeople=")) != nullptr))
		{
			auto Shot = [this](const FString& Name) { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 2 filename=%s"), *Name)); };
			static TArray<FString> Who; static TWeakObjectPtr<AActor> Stand; static FVector Eye0, Pawn0; static FRotator Look0;
			const float T0 = 46.f, Step = 10.f;
			if (Before < 40.f && ShotClock >= 40.f)
			{
				if (bGame) ToggleGame(); ASunClock::DayOverride = 172; ASunClock::HourOverride = 13.f; ASunClock::Theme = 1; ASunClock::Refresh(); GoFireStation(2);
				FString List; if (!FParse::Value(FCommandLine::Get(), TEXT("UIPeople="), List, false) || List.IsEmpty()) List = TEXT("MH_000_passenger,MH_001_passenger");
				List.ParseIntoArray(Who, TEXT(","), true);
			}
			if (Before < 45.f && ShotClock >= 45.f) if (APlayerController* P = GetOwningPlayerController()) if (P->PlayerCameraManager && GetOwningPawn())
			{ Eye0 = P->PlayerCameraManager->GetCameraLocation(); Look0 = FRotator(0.f, P->PlayerCameraManager->GetCameraRotation().Yaw, 0.f); Pawn0 = GetOwningPawn()->GetActorLocation(); }
			APlayerController* P = GetOwningPlayerController(); APawn* Me = GetOwningPawn();
			for (int32 K = 0; P && Me && K < Who.Num(); ++K)
			{
				const float S = T0 + K * Step;
				auto At = [&](float D) { return Before < S + D && ShotClock >= S + D; };
				if (At(0.f))
				{
					// back where you stood with the plain view; this person 3.4 m before you on the floor, turned to face you
					Me->SetActorLocation(Pawn0); P->SetControlRotation(FRotator(-3.f, Look0.Yaw, 0.f)); AADPWalker::Fov = 90.f;
					UClass* C = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Game/People/Built/%s/BP_%s.BP_%s_C"), *Who[K], *Who[K], *Who[K]));
					if (!C) { ADP_WARN(TEXT("UIPeople: %s isn't built"), *Who[K]); continue; }
					FVector Spot = Eye0 + Look0.Vector() * 340.f; Spot.Z = Eye0.Z - 158.f;
					Stand = GetWorld()->SpawnActor<AActor>(C, Spot, FRotator(0.f, Look0.Yaw + 90.f, 0.f));   // (the MetaHuman faces its own right)
					if (Stand.IsValid()) ADP_LOG(TEXT("UIPeople: %s standing"), *Who[K]);
				}
				if (At(4.f)) Shot(FString::Printf(TEXT("People_%s_Body"), *Who[K]));
				if (At(5.f) && Stand.IsValid())
				{
					// up to their face: 1.2 m off, the camera turned to their eyes (the top of them less 12 cm), the view narrowed
					FVector O, E; Stand->GetActorBounds(false, O, E);
					const float EyeZ = O.Z + E.Z - 12.f;
					Me->SetActorLocation(Pawn0 + Look0.Vector() * (340.f - 120.f));
					P->SetControlRotation(FRotator(FMath::RadiansToDegrees(FMath::Atan2(EyeZ - Eye0.Z, 120.f)), Look0.Yaw, 0.f));
					AADPWalker::Fov = 30.f;
				}
				if (At(8.5f)) Shot(FString::Printf(TEXT("People_%s_Face"), *Who[K]));
				if (At(9.5f) && Stand.IsValid()) { Stand->Destroy(); Stand = nullptr; }
			}
			const float End = T0 + Who.Num() * Step + 1.f;
			if (Who.Num() && Before < End && ShotClock >= End) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIWardrobe: in your office at the wardrobe: opened (you stand beside it in what you wear), a picture, closed
		if (FParse::Param(FCommandLine::Get(), TEXT("UIWardrobe")))
		{
			auto Shot = [](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(Name, true, false); };   // with the menu
			if (Before < 40.f && ShotClock >= 40.f) { if (bGame) ToggleGame(); ASunClock::HourOverride = 13.f; ASunClock::Theme = 1; ASunClock::Refresh(); GoTo(FVector(124.f, 0.f, 6.2f), FVector(126.f, 1.3f, 9.6f)); }
			if (Before < 46.f && ShotClock >= 46.f) Shot(TEXT("Wardrobe_Closed"));
			if (Before < 47.f && ShotClock >= 47.f)
			{
				AWardrobe* Near = nullptr; float Best = 1e9f;
				const FVector Me = GetOwningPawn() ? GetOwningPawn()->GetActorLocation() : FVector::ZeroVector;
				for (TActorIterator<AWardrobe> It(GetWorld()); It; ++It) { const float D = FVector::Dist(It->GetActorLocation(), Me); if (D < Best) { Best = D; Near = *It; } }
				if (Near) OpenWardrobe(Near); else ADP_WARN(TEXT("UIWardrobe: no wardrobe"));
			}
			if (Before < 51.f && ShotClock >= 51.f) Shot(TEXT("Wardrobe_Open"));
			if (Before < 52.f && ShotClock >= 52.f) WardrobeFromPage(TEXT("category"), TEXT("Hats"));
			if (Before < 55.f && ShotClock >= 55.f) Shot(TEXT("Wardrobe_Face"));
			if (Before < 56.f && ShotClock >= 56.f) WardrobeFromPage(TEXT("category"), TEXT("Shoes"));
			if (Before < 59.f && ShotClock >= 59.f) Shot(TEXT("Wardrobe_Feet"));
			if (Before < 60.f && ShotClock >= 60.f) WardrobeFromPage(TEXT("category"), TEXT("Outfits"));
			if (Before < 63.f && ShotClock >= 63.f) Shot(TEXT("Wardrobe_Outfits"));
			if (Before < 64.f && ShotClock >= 64.f) CloseWardrobe();
			if (Before < 67.f && ShotClock >= 67.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIAirfield: the airfield's grade and edges: down the runway from near the 08 end, an edge close up at taxiway A,
		// the runway's end from the grass
		if (FParse::Param(FCommandLine::Get(), TEXT("UIAirfield")))
		{
			auto Shot = [this](const TCHAR* Name) { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 2 filename=%s"), Name)); };
			const FAirfieldData& F = AAirfield::Shared();
			auto Go = [&](float X, float Y, float LX, float LY, float LH) { GoTo(FVector(X, 0.f, Y), FVector(LX, LH, LY)); };
			if (Before < 40.f && ShotClock >= 40.f) { if (bGame) ToggleGame(); ASunClock::DayOverride = 172; ASunClock::HourOverride = 14.f; ASunClock::Theme = 1; ASunClock::Refresh(); Go(F.RwyX0 + 260.f, F.RwyY - 70.f, F.RwyX0 + 1400.f, F.RwyY, 1.4f); }
			if (Before < 48.f && ShotClock >= 48.f) Shot(TEXT("Airfield_Runway"));
			if (Before < 49.f && ShotClock >= 49.f) Go(81.f + 40.f, F.AY + 31.5f, 81.f + 60.f, F.AY + 30.f, 0.9f);
			if (Before < 56.f && ShotClock >= 56.f) Shot(TEXT("Airfield_Edge"));
			if (Before < 57.f && ShotClock >= 57.f) Go(F.RwyX0 - 90.f, F.RwyY - 60.f, F.RwyX0 - 40.f, F.RwyY - 20.f, 0.8f);
			if (Before < 64.f && ShotClock >= 64.f) Shot(TEXT("Airfield_End"));
			if (Before < 65.f && ShotClock >= 65.f) { ASunClock::DayOverride = -1; ASunClock::HourOverride = -1.f; ASunClock::Refresh(); }
			if (Before < 66.f && ShotClock >= 66.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIPuddles (with -RainyDay): the puddles on a rainy day (owner, 2026-11-13): the apron, a taxiway and the runway,
		// each from eye height and from about an aircraft's tail-fin height (12 m) looking gently down with the terminal in
		// view, and one close over a puddle
		if (FParse::Param(FCommandLine::Get(), TEXT("UIPuddles")))
		{
			auto Shot = [this](const TCHAR* Name) { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 2 filename=%s"), Name)); };
			const FAirfieldData& F = AAirfield::Shared();
			static TWeakObjectPtr<ACameraActor> PCam;
			// a free camera at a plan point (x, height, y) looking at another
			auto View = [this](float X, float H, float Y, float LX, float LH, float LY)
			{
				APlayerController* PC = GetOwningPlayerController(); if (!PC) return;
				const FVector At = FTerminalPlan::ToWorld(X, H, Y), To = FTerminalPlan::ToWorld(LX, LH, LY);
				if (!PCam.IsValid()) PCam = GetWorld()->SpawnActor<ACameraActor>(At, (To - At).Rotation());
				else PCam->SetActorLocationAndRotation(At, (To - At).Rotation());
				if (PCam.IsValid()) { PCam->GetCameraComponent()->SetConstraintAspectRatio(false); PCam->GetCameraComponent()->SetFieldOfView(60.f); PC->SetViewTarget(PCam.Get()); }
			};
			struct FV { const TCHAR* Name; float X, H, Y, LX, LH, LY; };
			// (plan metres: the terminal's apron APRON-T runs y 82..226 with the stands' noses at y 108-110, taxiway A at
			// y = AY, the runway at RwyY; the terminal lies at y < 82)
			const FV GroundViews[] = {
				{TEXT("Puddles_ApronLow"),    0.f, 1.7f,  165.f, 0.f, 4.f, 60.f},
				{TEXT("Puddles_ApronHigh"),   0.f, 12.f,  185.f, 0.f, 3.f, 95.f},
				{TEXT("Puddles_TaxiwayLow"),  0.f,  1.7f,  F.AY, 300.f, 0.8f, F.AY - 2.f},
				{TEXT("Puddles_TaxiwayHigh"), -60.f, 12.f, F.AY + 45.f, 240.f, 2.f, F.AY - 60.f},
				{TEXT("Puddles_RunwayLow"),   -200.f, 4.0f, F.RwyY - 8.f, 200.f, 2.5f, F.RwyY},
				{TEXT("Puddles_RunwayHigh"),  -260.f, 12.f, F.RwyY + 50.f, 160.f, 0.f, F.RwyY - 90.f},
				{TEXT("Puddles_Close"),       0.f, 1.6f, 160.f, 0.4f, 0.f, 163.f},
				// the runway's touchdown zone (tyre rubber), low and from 12 m; the whole airport from 250 m up
				{TEXT("Ground_Touchdown"),    F.RwyX0 + 520.f, 2.5f, F.RwyY - 10.f, F.RwyX0 + 900.f, 0.5f, F.RwyY},
				{TEXT("Ground_TouchdownHigh"), F.RwyX0 + 380.f, 14.f, F.RwyY - 45.f, F.RwyX0 + 900.f, 0.f, F.RwyY + 5.f},
				{TEXT("Ground_RoadTower"),     -120.f, 30.f, 230.f, -190.f, 0.f, 140.f},
				{TEXT("Ground_RoadFire"),      -1130.f, 45.f, 90.f, -1205.f, 0.f, 200.f},
				{TEXT("Ground_LotFire"),       -1120.f, 22.f, 75.f, -1185.f, 0.f, 30.f},
				{TEXT("Ground_FireTop"),       -1190.f, 190.f, 60.f, -1190.f, 0.f, 59.f},
				{TEXT("Ground_RoadHold"),      -1199.f, 2.2f, 330.f, -1205.f, 0.f, 372.f},
				{TEXT("Ground_StopBars"),      24.f, 2.4f, 111.f, 19.7f, 0.f, 125.f},
				{TEXT("Ground_JetwayHoses"),  29.f, 4.f, 129.f, 23.5f, 2.4f, 122.f},
				{TEXT("Ground_JetwayDoor"),   34.f, 5.f, 141.f, 22.6f, 4.2f, 125.5f},
				{TEXT("Ground_JetwayAircraft"), 52.f, 13.f, 160.f, 20.f, 3.f, 117.f},
								{TEXT("Ground_Designator"),   -2080.f, 25.f, 646.f, -1855.f, 0.f, 646.f},
				{TEXT("Ground_Jetway"),       62.f, 9.f, 132.f, 24.f, 3.f, 104.f},
				{TEXT("Ground_StandNumber"),  101.4f, 9.f, 220.f, 101.4f, 0.f, 192.f},
				{TEXT("Ground_ApronWide"),    115.f, 150.f, 470.f, 115.f, 0.f, 130.f},
				{TEXT("Ground_ApronClose"),   40.f, 14.f, 222.f, 40.f, 0.f, 120.f},
				{TEXT("Ground_Stand"),        112.f, 1.7f, 178.f, 101.f, 0.f, 112.f},
				{TEXT("Ground_Hold"),         81.f, 5.5f, 495.f, 81.f, 0.f, 545.f},
				{TEXT("Ground_Edge"),         520.f, 1.7f, 360.f, 545.f, 0.f, 367.f},
				{TEXT("Ground_EdgeWide"),     480.f, 4.5f, 352.f, 560.f, 0.f, 372.f},
				{TEXT("Ground_Junction"),     160.f, 14.f, 320.f, 81.f, 0.f, 410.f},
				{TEXT("Ground_JunctionHigh"), -60.f, 110.f, 240.f, 81.f, 0.f, 430.f},
				{TEXT("Ground_Overview"),     -350.f, 250.f, 900.f, 150.f, 0.f, 250.f},
				// each stand's own wear (M_StandWear): soot behind the engines, drips at the wheels, a lead-in's tyre streaks
				{TEXT("StandWear_Engines"),   117.f, 2.6f, 162.f, 104.f, 0.f, 141.f},
				{TEXT("StandWear_Wheels"),    92.f, 1.5f, 131.f, 101.f, 0.f, 135.f},
				{TEXT("StandWear_Top"),       128.f, 28.f, 190.f, 101.f, 0.f, 142.f},
				{TEXT("StandWear_Stand5"),    -3.f, 2.4f, 166.f, -18.f, 0.f, 142.f},
				{TEXT("StandWear_LeadIn"),    -18.f, 1.8f, 215.f, -21.f, 0.f, 168.f},
			};
			// (-ShotOnly=<text>: only the views whose name holds it, e.g. -ShotOnly=StandWear)
			FString Only; FParse::Value(FCommandLine::Get(), TEXT("ShotOnly="), Only);
			TArray<FV> Picked; for (const FV& V : GroundViews) if (Only.IsEmpty() || FString(V.Name).Contains(Only)) Picked.Add(V);
			// (-SunsetLight: a low evening sun, early September at 19:20, for the sun on the paving)
			if (Before < 40.f && ShotClock >= 40.f) { if (bGame) ToggleGame(); const bool bLow = FParse::Param(FCommandLine::Get(), TEXT("SunsetLight")); ASunClock::DayOverride = bLow ? 250 : 172; ASunClock::HourOverride = bLow ? 18.6f : 15.f; ASunClock::Theme = 1; ASunClock::Refresh(); }
			for (int32 i = 0; i < Picked.Num(); ++i)
			{
				const float T0 = 41.f + 8.f * i;
				if (Before < T0 && ShotClock >= T0) View(Picked[i].X, Picked[i].H, Picked[i].Y, Picked[i].LX, Picked[i].LH, Picked[i].LY);
				if (Before < T0 + 6.f && ShotClock >= T0 + 6.f) Shot(Picked[i].Name);
			}
			const float TE = 41.f + 8.f * Picked.Num();
			if (Before < TE && ShotClock >= TE) { ASunClock::DayOverride = -1; ASunClock::HourOverride = -1.f; ASunClock::Refresh(); }
			if (Before < TE + 1.f && ShotClock >= TE + 1.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UITitle: the title screen with its film, pictures with the interface at a few moments (-NoNativeTitle: the
		// page's own canvas instead of Unreal's, to compare)
		if (FParse::Param(FCommandLine::Get(), TEXT("UITitle")))
		{
			const TCHAR* Sfx = FParse::Param(FCommandLine::Get(), TEXT("NoNativeTitle")) ? TEXT("_page") : TEXT("");
			const float At[] = {44.f, 47.f, 51.f, 55.f, 59.f, 63.f};
			for (int32 i = 0; i < 6; ++i) if (Before < At[i] && ShotClock >= At[i]) FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Title_%d%s"), i, Sfx), true, false);
			if (Before < 64.f && ShotClock >= 64.f) { ADP_LOG(TEXT("UITitle: Unreal drawing the film %d"), ADPTitleBg::IsOn() ? 1 : 0); FGenericPlatformMisc::RequestExit(false); }
		}
		// -UIDrone: drone mode from the full-screen map: the button, flying low, boosting up to the clouds, the interface
		// hidden and back, and Esc back onto your feet where you stood
		if (FParse::Param(FCommandLine::Get(), TEXT("UIDrone")))
		{
			auto Shot = [this](const TCHAR* Name) { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 2 filename=%s"), Name)); };
			static FVector StoodAt = FVector::ZeroVector;
			auto D = [this]() { return Drone.Get(); };
			if (Before < 40.f && ShotClock >= 40.f) { if (bGame) ToggleGame(); ASunClock::DayOverride = 172; ASunClock::HourOverride = 14.f; ASunClock::Theme = 1; ASunClock::Refresh(); GoApron(); }
			if (Before < 41.f && ShotClock >= 41.f) { if (GetOwningPawn()) StoodAt = GetOwningPawn()->GetActorLocation(); ToggleMap(); }
			if (Before < 42.f && ShotClock >= 42.f) FScreenshotRequest::RequestScreenshot(TEXT("Drone_Map_Button"), true, false);
			if (Before < 43.f && ShotClock >= 43.f) { EnterDrone(); ADP_LOG(TEXT("UIDrone: in drone mode %d, map full %d"), Drone.IsValid() ? 1 : 0, HoloMap.IsValid() && HoloMap->IsFull() ? 1 : 0); }
			if (Before < 44.f && ShotClock >= 44.f && D()) D()->SetTestInput(1.f, 0.f, 0.3f, false);
			if (Before < 46.5f && ShotClock >= 46.5f) FScreenshotRequest::RequestScreenshot(TEXT("Drone_Low_UI"), true, false);
			if (Before < 47.f && ShotClock >= 47.f && D()) { ADP_LOG(TEXT("UIDrone: at the step %.1f m/s, height %.0f m"), D()->SpeedNow(), D()->HeightAbove()); D()->SetTestInput(0.3f, 0.f, 1.f, true); }
			if (Before < 55.f && ShotClock >= 55.f && D()) { ADP_LOG(TEXT("UIDrone: boosting %.0f m/s, height %.0f m"), D()->SpeedNow(), D()->HeightAbove()); D()->SetTestInput(0.f, 0.f, 0.f, false); if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(FRotator(-25.f, PC->GetControlRotation().Yaw, 0.f)); }
			if (Before < 57.5f && ShotClock >= 57.5f) FScreenshotRequest::RequestScreenshot(TEXT("Drone_High_UI"), true, false);
			if (Before < 58.f && ShotClock >= 58.f) Shot(TEXT("Drone_High"));
			if (Before < 59.f && ShotClock >= 59.f) HideUI();
			if (Before < 60.f && ShotClock >= 60.f) Shot(TEXT("Drone_High_NoUI"));
			if (Before < 61.f && ShotClock >= 61.f) { OnEscape(); ADP_LOG(TEXT("UIDrone: Esc with the interface hidden: interface back %d, still flying %d"), bHidden ? 0 : 1, Drone.IsValid() ? 1 : 0); }
			if (Before < 62.f && ShotClock >= 62.f) { OnEscape(); APawn* Me = GetOwningPawn(); ADP_LOG(TEXT("UIDrone: Esc again: flying %d, on foot %d, %.1f m from where you stood"), Drone.IsValid() ? 1 : 0, Cast<AADPWalker>(Me) ? 1 : 0, Me ? FVector::Dist(Me->GetActorLocation(), StoodAt) / 100.f : -1.f); }
			if (Before < 63.5f && ShotClock >= 63.5f) Shot(TEXT("Drone_Back"));
			if (Before < 64.f && ShotClock >= 64.f) { ASunClock::DayOverride = -1; ASunClock::HourOverride = -1.f; ASunClock::Refresh(); }
			if (Before < 65.f && ShotClock >= 65.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIRun: running and turning (owner, 2026-11-13): in first person the body must stay right under you (its pelvis
		// measured against your capsule every frame), and from behind the camera must glide, not jump (its offset from you
		// measured for jolts every frame); pictures looking down at your feet and of a turn from behind
		if (FParse::Param(FCommandLine::Get(), TEXT("UIRun")))
		{
			// (-NoShots: no pictures, so no stalled frames in the camera's measurement)
			auto Shot = [this](const TCHAR* Name) { if (FParse::Param(FCommandLine::Get(), TEXT("NoShots"))) return; if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 2 filename=%s"), Name)); };
			static float MaxDrift = 0.f, MaxJolt = 0.f; static FVector O1 = FVector::ZeroVector, O2 = FVector::ZeroVector; static int32 NO = 0;
			AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn());
			APlayerController* PCr = GetOwningPlayerController();
			if (Before < 40.f && ShotClock >= 40.f) { if (bGame) ToggleGame(); ASunClock::DayOverride = 172; ASunClock::HourOverride = 14.f; ASunClock::Theme = 1; ASunClock::Refresh(); GoTo(FVector(72.f, 0.f, -22.f), FVector(79.f, 1.7f, 10.f)); if (Wk) Wk->SetThirdPerson(false); }
			const bool bRunFP = ShotClock >= 41.f && ShotClock < 46.f, bRunTP = ShotClock >= 48.f && ShotClock < 54.f;
			if (Before < 47.f && ShotClock >= 47.f && Wk) Wk->SetThirdPerson(true);
			if (Wk && (bRunFP || bRunTP))
			{
				Wk->GetCharacterMovement()->MaxWalkSpeed = 450.f;
				Wk->AddMovementInput(Wk->GetActorForwardVector(), 1.f);
				// running a circle (FP), then straight and into a turn (TP)
				const float TurnSpeed = bRunFP ? 55.f : (ShotClock >= 50.f ? 70.f : 0.f);
				if (PCr) { FRotator R = PCr->GetControlRotation(); R.Yaw += TurnSpeed * GetWorld()->GetDeltaSeconds(); if (bRunFP) R.Pitch = ShotClock >= 43.f && ShotClock < 44.5f ? -55.f : -5.f; PCr->SetControlRotation(R); }
				if (bRunFP && ShotClock >= 42.f && Wk->HumanActor())
				{
					TArray<USkeletalMeshComponent*> Sk; Wk->HumanActor()->GetComponents<USkeletalMeshComponent>(Sk);
					for (USkeletalMeshComponent* S : Sk) if (S->GetName().StartsWith(TEXT("Body")))
					{
						const FVector Lp = Wk->GetActorTransform().InverseTransformPosition(S->GetBoneLocation(TEXT("pelvis")));
						const FVector Lf = Wk->GetActorTransform().InverseTransformPosition(S->GetBoneLocation(TEXT("foot_l")));
						MaxDrift = FMath::Max(MaxDrift, float(FVector(Lp.X, Lp.Y, 0.f).Size()));
						static float Next = 0.f; if (ShotClock >= Next) { Next = ShotClock + 0.25f; ADP_LOG(TEXT("UIRun: %.2f s pelvis (%.0f, %.0f, %.0f) left foot (%.0f, %.0f, %.0f) in your capsule's frame, %.0f cm/s"), ShotClock, Lp.X, Lp.Y, Lp.Z, Lf.X, Lf.Y, Lf.Z, Wk->GetVelocity().Size2D()); }
					}
				}
				if (bRunTP && ShotClock >= 49.f && PCr && PCr->PlayerCameraManager)
				{
					const FVector O = PCr->PlayerCameraManager->GetCameraLocation() - Wk->GetActorLocation();
					if (GetWorld()->GetDeltaSeconds() > 0.05f) NO = 0;   // (a picture being taken stalls the frame: not the camera)
					if (NO >= 2) { const float J = float((O - 2.f * O1 + O2).Size()); MaxJolt = FMath::Max(MaxJolt, J); if (J > 15.f) ADP_LOG(TEXT("UIRun: %.2f s camera jolt %.0f cm, offset now %s, dt %.3f"), ShotClock, J, *O.ToString(), GetWorld()->GetDeltaSeconds()); }
					O2 = O1; O1 = O; ++NO;
				}
			}
			if (Before < 44.f && ShotClock >= 44.f) Shot(TEXT("Run_FP_Feet"));
			// standing still in first person, looking straight down and a little ahead: your own body from your eyes
			if (ShotClock >= 40.5f && ShotClock < 41.f && PCr) PCr->AddPitchInput(30.f);   // trying to look straight down
			if (Before < 40.9f && ShotClock >= 40.9f) { Shot(TEXT("Run_FP_StandDown")); if (PCr && PCr->PlayerCameraManager) ADP_LOG(TEXT("UIRun: looking down as far as you can: %.1f degrees"), PCr->PlayerCameraManager->GetCameraRotation().Pitch); }
			if (Before < 45.5f && ShotClock >= 45.5f) Shot(TEXT("Run_FP_Ahead"));
			if (Before < 46.f && ShotClock >= 46.f) ADP_LOG(TEXT("UIRun: first person, the body's pelvis at most %.0f cm from your capsule while running"), MaxDrift);
			if (Before < 49.5f && ShotClock >= 49.5f) Shot(TEXT("Run_TP_Straight"));
			if (Before < 51.5f && ShotClock >= 51.5f) Shot(TEXT("Run_TP_Turn"));
			if (Before < 54.f && ShotClock >= 54.f) { ADP_LOG(TEXT("UIRun: third person, the camera's largest jolt %.1f cm over %d frames"), MaxJolt, NO); if (Wk) { Wk->GetCharacterMovement()->MaxWalkSpeed = 150.f; Wk->SetThirdPerson(false); } ASunClock::DayOverride = -1; ASunClock::HourOverride = -1.f; ASunClock::Refresh(); }
			if (Before < 55.f && ShotClock >= 55.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIBody: you from behind in the hall: standing, then walking forward (your MetaHuman body in its outfit)
		if (FParse::Param(FCommandLine::Get(), TEXT("UIBody")))
		{
			auto Shot = [this](const TCHAR* Name) { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 2 filename=%s"), Name)); };
			// out of the canopy's shadow, in the sun before the entrance (the owner, 2026-11-12), from behind and from the front
			if (Before < 40.f && ShotClock >= 40.f) { if (bGame) ToggleGame(); ASunClock::DayOverride = 172; ASunClock::HourOverride = 14.f; ASunClock::Theme = 1; ASunClock::Refresh(); GoTo(FVector(72.f, 0.f, -22.f), FVector(79.f, 1.7f, 10.f)); if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) Wk->SetThirdPerson(true); }
			if (Before < 46.f && ShotClock >= 46.f) Shot(TEXT("Body_Standing"));
			if (ShotClock >= 47.f && ShotClock < 51.f) if (APawn* Me = GetOwningPawn()) Me->AddMovementInput(Me->GetActorForwardVector(), 1.f);
			if (Before < 49.f && ShotClock >= 49.f) Shot(TEXT("Body_Walking"));
			// your body turned round to face the camera (the camera stays where it is behind the capsule)
			if (Before < 52.f && ShotClock >= 52.f) if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) if (AActor* H = Wk->HumanActor()) H->AddActorLocalRotation(FRotator(0.f, 180.f, 0.f));
			if (Before < 55.f && ShotClock >= 55.f) Shot(TEXT("Body_Front"));
			if (Before < 56.f && ShotClock >= 56.f) { if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) Wk->SetThirdPerson(false); ASunClock::DayOverride = -1; ASunClock::HourOverride = -1.f; ASunClock::Refresh(); }
			if (Before < 57.f && ShotClock >= 57.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIHallWalk: you walking in the main hall of the passenger terminal (owner, 2026-11-12), from behind and from the front
		if (FParse::Param(FCommandLine::Get(), TEXT("UIHallWalk")))
		{
			auto Shot = [this](const TCHAR* Name) { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 2 filename=%s"), Name)); };
			if (Before < 40.f && ShotClock >= 40.f) { if (bGame) ToggleGame(); ASunClock::DayOverride = 172; ASunClock::HourOverride = 14.f; ASunClock::Theme = 1; ASunClock::Refresh(); GoTo(FVector(58.f, 0.f, 4.f), FVector(88.f, 1.7f, 22.f)); if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) Wk->SetThirdPerson(true); }
			if (ShotClock >= 44.f && ShotClock < 49.f) if (APawn* Me = GetOwningPawn()) Me->AddMovementInput(Me->GetActorForwardVector(), 1.f);
			if (Before < 47.f && ShotClock >= 47.f) Shot(TEXT("Hall_Walking_Behind"));
			if (Before < 50.f && ShotClock >= 50.f) if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) if (AActor* H = Wk->HumanActor()) H->AddActorLocalRotation(FRotator(0.f, 180.f, 0.f));
			if (ShotClock >= 51.f && ShotClock < 55.f) if (APawn* Me = GetOwningPawn()) Me->AddMovementInput(-Me->GetActorForwardVector(), 1.f);
			if (Before < 53.5f && ShotClock >= 53.5f) Shot(TEXT("Hall_Walking_Front"));
			if (Before < 56.f && ShotClock >= 56.f) { if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) Wk->SetThirdPerson(false); ASunClock::DayOverride = -1; ASunClock::HourOverride = -1.f; ASunClock::Refresh(); }
			if (Before < 57.f && ShotClock >= 57.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UINewGame: the real thing, driven as the page's overlay would (hover, choose, the panel, done): the title screen's
		// New game, she hovered, chosen, the creator panel, and on into the game's own new-game flow
		if (FParse::Param(FCommandLine::Get(), TEXT("UINewGame")))
		{
			auto Shot = [](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(Name, true, false); };
			auto Js = [this](const TCHAR* Code) { if (Browser.IsValid()) Browser->ExecuteJavascript(Code); };
			if (Before < 40.f && ShotClock >= 40.f) { if (!bGame) ToggleGame(); }
			if (Before < 46.f && ShotClock >= 46.f) Js(TEXT("(function(){ var u = window.ue && ue.adp; if (u && u.newgame) u.newgame(); })()"));
			if (Before < 51.f && ShotClock >= 51.f) Shot(TEXT("NewGame_Choose"));
			// the real screen as frames for an animation (owner): the choice moving, the pick, the walk-out, the tubes, the panel
			static int32 AnimFrame = 0;
			if (ShotClock >= 51.f && ShotClock < 68.f && int32(ShotClock / 0.3f) != int32(Before / 0.3f)) Shot(*FString::Printf(TEXT("NewGame_Anim_%03d"), AnimFrame++));
			if (Before < 51.5f && ShotClock >= 51.5f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}))"));   // him first
			if (Before < 52.f && ShotClock >= 52.f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowLeft'}))"));
			if (Before < 55.f && ShotClock >= 55.f) Shot(TEXT("NewGame_SheLit"));
			if (Before < 56.f && ShotClock >= 56.f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}))"));
			if (Before < 66.f && ShotClock >= 66.f) Shot(TEXT("NewGame_Creator"));
			if (Before < 67.f && ShotClock >= 67.f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'Backspace'}))"));   // Back: the reverse
			if (Before < 71.f && ShotClock >= 71.f) Shot(TEXT("NewGame_BackHalfway"));
			if (Before < 76.f && ShotClock >= 76.f) Shot(TEXT("NewGame_BackAtChoice"));
			if (Before < 77.f && ShotClock >= 77.f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}))"));
			if (Before < 87.f && ShotClock >= 87.f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}))"));   // Next: your pet
			if (Before < 87.5f && ShotClock >= 87.5f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'})); window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}))"));   // Dog
			if (Before < 88.f && ShotClock >= 88.f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}))"));   // the second coat
			if (Before < 89.f && ShotClock >= 89.f) Shot(TEXT("NewGame_Pet"));
			if (Before < 90.f && ShotClock >= 90.f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}))"));   // Next: your partner
			if (Before < 90.5f && ShotClock >= 90.5f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}))"));   // Married: he walks in
			if (Before < 92.f && ShotClock >= 92.f) Shot(TEXT("NewGame_PartnerWalking"));
			if (Before < 95.5f && ShotClock >= 95.5f) Shot(TEXT("NewGame_Partner"));
			if (Before < 96.5f && ShotClock >= 96.5f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}))"));   // Next: your airline
			// the airline founded on the stage (no game form after Start game): Regional, a name typed (code and callsign follow it), your name
			if (Before < 97.5f && ShotClock >= 97.5f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}))"));
			if (Before < 98.f && ShotClock >= 98.f) Js(TEXT("(function(){ function put(f, v){ var e = document.querySelector('.wy [data-f=' + f + ']'); if (e) { e.value = v; e.dispatchEvent(new Event('input', {bubbles: true})); } } put('name', 'Aurora Atlantic Express'); put('pilot', 'Simon Test'); })()"));
			if (Before < 99.5f && ShotClock >= 99.5f) Shot(TEXT("NewGame_Airline"));
			if (Before < 100.5f && ShotClock >= 100.5f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}))"));   // Next: the difficulty
			if (Before < 101.5f && ShotClock >= 101.5f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}))"));   // Hard
			if (Before < 103.f && ShotClock >= 103.f) Shot(TEXT("NewGame_Difficulty"));
			if (Before < 104.f && ShotClock >= 104.f) Js(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}))"));   // Start game: the loading screen, the airline founded unseen
			if (Before < 106.f && ShotClock >= 106.f) Shot(TEXT("NewGame_Loading"));
			if (Before < 110.f && ShotClock >= 110.f) Shot(TEXT("NewGame_Loading2"));
			if (Before < 114.f && ShotClock >= 114.f) Shot(TEXT("NewGame_Loading3"));
			if (Before < 124.f && ShotClock >= 124.f) Js(TEXT("(function(){ var f = document.querySelector('.game-frame'); var c = f && f.contentWindow; if (c) ue.adp.log('NEWGAME airline=' + c.eval('JSON.stringify((function(){var x = company(); return x ? {n: x.name, i: x.icao, c: x.callsign, h: x.hub, k: x.kind, b: Math.round(x.balance)} : null})())') + ' pilot=' + c.eval('myPilot().name')); })()"));
			if (Before < 125.f && ShotClock >= 125.f) Shot(TEXT("NewGame_OnIntoTheGame"));
			if (Before < 127.f && ShotClock >= 127.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIGender: the concept of the new career's "who are you" screen (owner, 2026-11-12): the dark neon room with
		// the fog, she in the light and he in shadow, with the title screen's lettering over it
		if (FParse::Param(FCommandLine::Get(), TEXT("UIGender")))
		{
			auto Shot = [](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(Name, true, false); };
			if (Before < 40.f && ShotClock >= 40.f)
			{
				if (bGame) ToggleGame();
				ASunClock::DayOverride = 172; ASunClock::HourOverride = 23.5f; ASunClock::Theme = 1; ASunClock::Refresh();
				if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) if (AActor* H = Wk->HumanActor()) H->SetActorHiddenInGame(true);
				if (TopBar.IsValid()) TopBar->SetVisibility(EVisibility::Collapsed);
				if (MiniBox.IsValid()) MiniBox->SetVisibility(EVisibility::Collapsed);
				if (Credit.IsValid()) Credit->SetVisibility(EVisibility::Collapsed);
				bHidden = true;   // (the airport's interface away: this is its own screen)
				ADPCharacterCreate::BuildConcept(GetWorld(), GetOwningPlayerController(), true);
				if (GEngine && GEngine->GameViewport)
					GEngine->GameViewport->AddViewportWidgetContent(
						SNew(SOverlay)
						+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(64, 56, 0, 0)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Neon::Small(TEXT("Airport Dynasty Pro  ·  a new career"))]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[SNew(STextBlock).Text(FText::FromString(TEXT("WHO ARE YOU?"))).Font(Neon::Face(34, 120)).ColorAndOpacity(FLinearColor::White)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[Neon::Small(TEXT("She, or he  ·  then make them yours"), FLinearColor(Neon::Paper.R, Neon::Paper.G, Neon::Paper.B, 0.7f))]
						]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 44)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 300, 0)[SNew(STextBlock).Text(FText::FromString(TEXT("SHE"))).Font(Neon::Face(18, 300)).ColorAndOpacity(Neon::Pink)]
							+ SHorizontalBox::Slot().AutoWidth().Padding(300, 0, 0, 0)[SNew(STextBlock).Text(FText::FromString(TEXT("HE"))).Font(Neon::Face(18, 300)).ColorAndOpacity(FLinearColor(Neon::Paper.R, Neon::Paper.G, Neon::Paper.B, 0.35f))]
						], 20);
			}
			// (-UIGenderHe: he chosen instead, so the other way round)
			static const bool bHe = FParse::Param(FCommandLine::Get(), TEXT("UIGenderHe"));
			if (Before < 46.f && ShotClock >= 46.f) ADPCharacterCreate::Choose(!bHe);
			if (Before < 51.f && ShotClock >= 51.f) Shot(bHe ? TEXT("Creator_He") : TEXT("Creator_She"));
			if (Before < 52.f && ShotClock >= 52.f) ADPCharacterCreate::Confirm(!bHe);
			// the transition as frames (an animation for the owner), then the creator's panel on the free side
			static int32 Frame = 0;
			if (ShotClock >= 52.f && ShotClock < 61.f && int32(ShotClock / 0.35f) != int32(Before / 0.35f)) Shot(*FString::Printf(TEXT("Creator_Anim%s_%02d"), bHe ? TEXT("He") : TEXT("She"), Frame++));
			if (Before < 62.f && ShotClock >= 62.f && GEngine && GEngine->GameViewport)
			{
				const bool bRight = ADPCharacterCreate::UiOnRight();
				TSharedRef<SVerticalBox> Bars = SNew(SVerticalBox);
				const TCHAR* Parts[] = {TEXT("Face"), TEXT("Skin"), TEXT("Body"), TEXT("Height"), TEXT("Hair"), TEXT("Clothes")};
				for (int32 i = 0; i < 6; ++i) Bars->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[Neon::Bar(Parts[i], []() {}, i == 0)];
				GEngine->GameViewport->AddViewportWidgetContent(
					SNew(SOverlay)
					+ SOverlay::Slot().HAlign(bRight ? HAlign_Right : HAlign_Left).VAlign(VAlign_Center).Padding(bRight ? 0 : 96, 0, bRight ? 96 : 0, 0)
					[
						SNew(SBox).WidthOverride(360)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Neon::Small(TEXT("Make them yours"))]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 16)[SNew(STextBlock).Text(FText::FromString(TEXT("YOUR LOOKS"))).Font(Neon::Face(28, 120)).ColorAndOpacity(FLinearColor::White)]
							+ SVerticalBox::Slot().AutoHeight()[Bars]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)[Neon::Small(TEXT("Then: your pet  ·  your partner"), FLinearColor(Neon::Paper.R, Neon::Paper.G, Neon::Paper.B, 0.6f))]
						]
					], 21);
			}
			if (Before < 64.f && ShotClock >= 64.f) Shot(bHe ? TEXT("Creator_AfterHe") : TEXT("Creator_AfterShe"));
			if (Before < 65.f && ShotClock >= 65.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIGate: you at gate 4's window (from behind inside, towards your face from the apron), then walking out the
		// jetway (from behind inside it, and from outside through its glass), summer afternoon light (owner, 2026-11-12)
		if (FParse::Param(FCommandLine::Get(), TEXT("UIGate")))
		{
			auto Shot = [this](const TCHAR* Name) { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 2 filename=%s"), Name)); };
			static TWeakObjectPtr<ACameraActor> Cam;
			auto Outside = [this](const FVector& PlanAt, const FVector& PlanLook)
			{
				APlayerController* PC = GetOwningPlayerController(); if (!PC) return;
				const FVector At = FTerminalPlan::ToWorld(PlanAt.X, PlanAt.Y, PlanAt.Z), To = FTerminalPlan::ToWorld(PlanLook.X, PlanLook.Y, PlanLook.Z);
				if (!Cam.IsValid()) Cam = GetWorld()->SpawnActor<ACameraActor>(At, (To - At).Rotation());
				else Cam->SetActorLocationAndRotation(At, (To - At).Rotation());
				if (Cam.IsValid()) { Cam->GetCameraComponent()->SetConstraintAspectRatio(false); Cam->GetCameraComponent()->SetFieldOfView(55.f); PC->SetViewTarget(Cam.Get()); }
			};
			auto Back = [this]() { if (APlayerController* PC = GetOwningPlayerController()) if (APawn* Me = GetOwningPawn()) PC->SetViewTarget(Me); };
			const float F2 = Plan.F2Y;
			if (Before < 40.f && ShotClock >= 40.f) { if (bGame) ToggleGame(); ASunClock::DayOverride = 172; ASunClock::HourOverride = 15.f; ASunClock::Theme = 1; ASunClock::Refresh(); GoTo(FVector(186.f, F2, 79.4f), FVector(186.f, F2 + 1.6f, 95.f)); if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) Wk->SetThirdPerson(true); }
			if (Before < 46.f && ShotClock >= 46.f) Shot(TEXT("Gate_Inside"));
			if (Before < 47.f && ShotClock >= 47.f) Outside(FVector(186.f, F2 + 1.7f, 89.f), FVector(186.f, F2 + 1.4f, 80.f));
			if (Before < 50.f && ShotClock >= 50.f) Shot(TEXT("Gate_Outside"));
			if (Before < 51.f && ShotClock >= 51.f) { Back(); GoTo(FVector(191.05f, F2, 79.f), FVector(191.05f, F2 + 1.6f, 100.f)); }
			if (ShotClock >= 53.f && ShotClock < 58.f) if (APawn* Me = GetOwningPawn()) Me->AddMovementInput(Me->GetActorForwardVector(), 1.f);
			if (Before < 57.f && ShotClock >= 57.f) Shot(TEXT("Jetway_Inside"));
			if (Before < 58.5f && ShotClock >= 58.5f) if (APawn* Me = GetOwningPawn()) { const FVector P = Me->GetActorLocation(); Outside(FVector(P.X / 100.f + 7.f, P.Z / 100.f + 0.9f, -P.Y / 100.f + 1.5f), FVector(P.X / 100.f, P.Z / 100.f + 0.3f, -P.Y / 100.f)); }
			if (Before < 61.5f && ShotClock >= 61.5f) Shot(TEXT("Jetway_Outside"));
			if (Before < 62.5f && ShotClock >= 62.5f) { Back(); if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) Wk->SetThirdPerson(false); ASunClock::DayOverride = -1; ASunClock::HourOverride = -1.f; ASunClock::Refresh(); }
			if (Before < 63.5f && ShotClock >= 63.5f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIClouds: from above the clouds (owner, 2026-11-12): broken cover, the low sun on the cloud tops, then daylight
		if (FParse::Param(FCommandLine::Get(), TEXT("UIClouds")))
		{
			auto Shot = [this](const TCHAR* Name) { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 2 filename=%s"), Name)); };
			auto Up = [this]()
			{
				if (APawn* Me = GetOwningPawn())
				{
					if (ACharacter* Ch = Cast<ACharacter>(Me)) Ch->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
					Me->TeleportTo(FTerminalPlan::ToWorld(200.f, 15400.f, 600.f), FRotator(0.f, -20.f, 0.f));   // (the cloud layer's top is at 15 km)
					if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(FRotator(-14.f, -20.f, 0.f));
				}
			};
			if (Before < 40.f && ShotClock >= 40.f) { if (bGame) ToggleGame(); ASunClock::DayOverride = 172; ASunClock::HourOverride = 22.6f; ASunClock::Theme = 1; ASunClock::Refresh(); Up(); }
			if (Before < 44.f && ShotClock >= 44.f) Up();
			if (Before < 50.f && ShotClock >= 50.f) Shot(TEXT("Clouds_Sunset"));
			if (Before < 51.f && ShotClock >= 51.f) { ASunClock::HourOverride = 13.f; ASunClock::Refresh(); Up(); }
			if (Before < 57.f && ShotClock >= 57.f) Shot(TEXT("Clouds_Day"));
			if (Before < 58.f && ShotClock >= 58.f) { ASunClock::DayOverride = -1; ASunClock::HourOverride = -1.f; ASunClock::Refresh(); }
			if (Before < 59.f && ShotClock >= 59.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UISeason: the time of year (Settings): on the apron at noon in clear weather, 21 December then 21 June, a
		// picture of each (the sun low and long shadows, then high), the real date put back; the log has the sun's height
		if (FParse::Param(FCommandLine::Get(), TEXT("UISeason")))
		{
			auto Shot = [this](const TCHAR* Name) { if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 2 filename=%s"), Name)); };
			auto Height = [this](const TCHAR* Label) { double El = 0, Az = 0; const int32 Y = FDateTime::UtcNow().GetYear(); ASunClock::SunAt(FDateTime(Y, FMath::Max(ASunClock::Month, 1), FMath::Max(ASunClock::Day, 1), 12), 63.452335, -18.710348, El, Az); ADP_LOG(TEXT("UISeason %s: %s, the sun %.1f deg up at noon"), Label, *ASunClock::DateName(), El); };
			if (Before < 40.f && ShotClock >= 40.f) { if (bGame) ToggleGame(); ASunClock::HourOverride = 12.f; ASunClock::Theme = 0; ASunClock::Refresh(); GoApron(); }
			if (Before < 46.f && ShotClock >= 46.f) { ASunClock::SetDate(12, 21); Height(TEXT("winter")); }
			if (Before < 52.f && ShotClock >= 52.f) Shot(TEXT("Season_Dec21"));
			if (Before < 54.f && ShotClock >= 54.f) { ASunClock::SetDate(6, 21); Height(TEXT("summer")); }
			if (Before < 60.f && ShotClock >= 60.f) Shot(TEXT("Season_Jun21"));
			if (Before < 62.f && ShotClock >= 62.f) { ASunClock::SetDate(0, 0); ASunClock::HourOverride = -1.f; ASunClock::Refresh(); }
			if (Before < 64.f && ShotClock >= 64.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIDrive: a test drive in fire truck 1: in, out of the bay through its door, a turn, the driver's view, braking, out
		if (FParse::Param(FCommandLine::Get(), TEXT("UIDrive")))
		{
			auto Sun = [](float H, int32 Day = -1) { ASunClock::DayOverride = Day; ASunClock::HourOverride = H; ASunClock::Theme = 1; ASunClock::Refresh(); };
			auto Shot = [](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(Name, false, false); };
			auto Truck = [this]() -> AFireTruck* { if (AFireTruck* T = Cast<AFireTruck>(GetOwningPawn())) return T; for (const TWeakObjectPtr<AFireTruck>& T : AFireTruck::All) if (T.IsValid()) return T.Get(); return nullptr; };
			struct FStep { float At; TFunction<void()> Do; };
			const FStep DSteps[] = {
				{40.f, [&]() { if (bGame) ToggleGame(); Sun(13.f, 172); GoFireStation(2); }},
				{46.f, [&]() { Shot(TEXT("Drive_Walker_Bay")); if (APlayerController* P = GetOwningPlayerController()) if (P->PlayerCameraManager) ADP_LOG(TEXT("UIDrive: walker camera at %s"), *P->PlayerCameraManager->GetCameraLocation().ToString()); }},
				{45.f, [&]() { if (AFireTruck* T = Truck()) if (APawn* Me = GetOwningPawn()) { const FVector At = T->DoorSpot(); Me->TeleportTo(At, FRotator(0.f, (T->GetActorLocation() - At).Rotation().Yaw, 0.f)); if (GetOwningPlayerController()) GetOwningPlayerController()->SetControlRotation(FRotator(-5.f, (T->GetActorLocation() - At).Rotation().Yaw, 0.f)); } }},
				{46.5f, [&]() { Shot(TEXT("Drive_Before")); }},
				// (-DriveThird: from behind yourself, getting in and out with E itself, as you press it)
				{44.f, [&]() { if (FParse::Param(FCommandLine::Get(), TEXT("DriveThird"))) if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) Wk->SetThirdPerson(true); }},
				{47.f, [&]() { if (FParse::Param(FCommandLine::Get(), TEXT("DriveThird"))) { Interact(); if (AFireTruck* T = Truck()) { ADP_LOG(TEXT("UIDrive: E pressed from third person, truck %s"), T->IsDriven() ? TEXT("taken") : TEXT("NOT taken")); T->SetView(true); } } else if (AFireTruck* T = Truck()) { T->Enter(GetOwningPawn()); T->SetView(true); } }},
				{47.3f, [&]() { Shot(TEXT("Drive_Walk")); }},
				{47.6f, [&]() { Shot(TEXT("Drive_Climb1")); }},
				{48.0f, [&]() { Shot(TEXT("Drive_Handle")); }},
				{48.2f, [&]() { Shot(TEXT("Drive_Climb2")); }},
				{48.8f, [&]() { Shot(TEXT("Drive_Climb3")); }},
				{49.4f, [&]() { Shot(TEXT("Drive_Climb4")); }},
				{50.0f, [&]() { Shot(TEXT("Drive_Climb5")); }},
				{50.85f, [&]() { Shot(TEXT("Drive_In_Bay")); }},
				{52.f, [&]() { if (AFireTruck* T = Truck()) T->SetTestInput(0.6f, 0.f, 0.f); }},   // (the walk and the climb take about 5 s)
				{53.f, [&]() { Shot(TEXT("Drive_T53")); }},
				{54.f, [&]() { if (FParse::Param(FCommandLine::Get(), TEXT("DriveThird"))) if (AFireTruck* T = Truck()) { T->PressView(); ADP_LOG(TEXT("UIDrive: V pressed, view step %d (0 cab, 1 close, 2 far)"), T->ViewStep()); } }},
				{55.f, [&]() { Shot(TEXT("Drive_T55")); }},
				{56.f, [&]() { if (FParse::Param(FCommandLine::Get(), TEXT("DriveThird"))) if (AFireTruck* T = Truck()) { T->PressView(); ADP_LOG(TEXT("UIDrive: V pressed, view step %d"), T->ViewStep()); } }},
				{57.f, [&]() { Shot(TEXT("Drive_T57")); }},
				{60.f, [&]() { Shot(TEXT("Drive_Out")); }},
				{60.5f, [&]() { if (AFireTruck* T = Truck()) { T->SetTestTurn(2); T->SetView(false); } }},
				{60.9f, [&]() { Shot(TEXT("Drive_Indicator_Stalk")); if (AFireTruck* T = Truck()) T->SetView(true); }},
				{61.f, [&]() { if (AFireTruck* T = Truck()) { T->SetTestInput(0.4f, 0.f, 0.6f); T->ToggleSiren(); } }},
				{67.f, [&]() { Shot(TEXT("Drive_Turn")); }},
				{69.5f, [&]() { if (AFireTruck* T = Truck()) ADP_LOG(TEXT("UIDrive: indicator now %d (0 = off)"), T->TurnState()); }},
				{68.f, [&]() { if (AFireTruck* T = Truck()) { T->SetTestInput(0.3f, 0.f, -0.4f); T->SetView(false); } }},
				{72.f, [&]() { Shot(TEXT("Drive_Cab")); }},
				{73.f, [&]() { if (AFireTruck* T = Truck()) { T->SetTestInput(0.f, 1.f, 0.f); T->SetView(true); } }},
				{75.f, [&]() { if (AFireTruck* T = Truck()) T->SetTestInput(0.f, 1.f, 0.f); }},
				{76.f, [&]() { Sun(23.f, 172); if (AFireTruck* T = Truck()) T->SetTestLights(true, true); }},
				{78.4f, [&]() { Shot(TEXT("Drive_Night_Lights")); }},
				{78.6f, [&]() { Shot(TEXT("Drive_Night_Lights2")); }},
				{77.5f, [&]() { Shot(TEXT("Drive_Reversing")); if (AFireTruck* T = Truck()) { ADP_LOG(TEXT("UIDrive: reversing at %.1f km/h"), T->SpeedKmh()); T->SetTestInput(0.f, 0.f, 0.f); } }},
				{79.f, [&]() { Shot(TEXT("Drive_Stopped")); if (AFireTruck* T = Truck()) ADP_LOG(TEXT("UIDrive: stopped at %.1f km/h"), T->SpeedKmh()); }},
				{79.6f, [&]() { if (AFireTruck* T = Truck()) T->SetView(FParse::Param(FCommandLine::Get(), TEXT("DriveThird"))); }},
				{79.9f, [&]() { Shot(TEXT("Drive_Night_Cab")); }},
				{80.f, [&]() { if (AFireTruck* T = Truck()) { if (T->SirenOn()) T->ToggleSiren(); T->SetTestLights(false, false); if (FParse::Param(FCommandLine::Get(), TEXT("DriveThird"))) Interact(); else T->Exit(); } }},
				{80.8f, [&]() { Shot(TEXT("Drive_Out1")); }},
				{81.8f, [&]() { Shot(TEXT("Drive_Out2")); }},
				{82.8f, [&]() { Shot(TEXT("Drive_Out3")); }},
				{84.f, [&]() { Shot(TEXT("Drive_Out_Of_Truck")); if (AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn())) ADP_LOG(TEXT("UIDrive: out, walking in %s"), Wk->IsThirdPerson() ? TEXT("third person") : TEXT("first person")); }},
				{85.f, [&]() { AFireStationDoors::bTestOpen = true; GoFireStation(0); }},
				{92.f, [&]() { Shot(TEXT("Drive_Doors_Open")); GoFireStation(2); }},
				{98.f, [&]() { Shot(TEXT("Drive_Doors_Open_Inside")); }},
				{100.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			for (const FStep& St : DSteps)
				if (Before < St.At && ShotClock >= St.At) St.Do();
			if (ShotClock > 47.f && ShotClock < 80.f && FMath::Fmod(ShotClock, 1.f) < FMath::Fmod(Before, 1.f))
				if (AFireTruck* T = Truck()) ADP_LOG(TEXT("UIDrive: %.0f s, %.1f km/h at %s, turning %.2f deg/s"), ShotClock, T->SpeedKmh(), *T->GetActorLocation().ToString(), T->TurnRate());
		}
		// -UILoading: from the menus into the airport, the loading screen pictured while it works and when it's done
		if (FParse::Param(FCommandLine::Get(), TEXT("UILoading")))
		{
			if (Before < 40.f && ShotClock >= 40.f && bGame) ToggleGame();
			if (Before < 43.f && ShotClock >= 43.f) { FScreenshotRequest::RequestScreenshot(TEXT("Loading_Screen"), true, false); ADP_LOG(TEXT("LOADTEST %s %.0f%% shown %d"), *LoadStep, LoadFrac * 100.f, bLoading ? 1 : 0); }
			static bool bLogged = false;
			if (!bLogged && ShotClock > 41.f && !bLoading) { bLogged = true; ADP_LOG(TEXT("LOADTEST done after %.1f s"), LoadT); }
			if (Before < 100.f && ShotClock >= 100.f) FGenericPlatformMisc::RequestExit(false);
		}
		// -UIPart2: night on the apron (the terminal's brightness), the entrance canopies and doors, the tower's balcony and
		// entrance lights, then the entrance by day
		if (FParse::Param(FCommandLine::Get(), TEXT("UIPart2")))
		{
			auto Sun = [](float H) { ASunClock::HourOverride = H; ASunClock::Theme = 1; ASunClock::Refresh(); };
			struct FStep { float At; TFunction<void()> Do; };
			const FStep P2Steps[] = {
				{44.f, [&]() { if (bGame) ToggleGame(); Sun(22.5f); GoApron(); }},
				{56.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Part2_ApronNight"), false, false); }},
				{57.f, [&]() { GoTo(FVector(75.f, 0.f, -12.f), FVector(75.f, 3.f, 0.f)); }},
				{66.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Part2_EntranceNight"), false, false); }},
				{67.f, [this]() { GoTowerFoot(); }},
				{76.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Part2_TowerDoorNight"), false, false); }},
				{77.f, [this]() { GoTowerBalcony(); }},
				{86.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Part2_BalconyNight"), false, false); }},
				{87.f, [&]() { Sun(13.f); GoTo(FVector(75.f, 0.f, -12.f), FVector(75.f, 3.f, 0.f)); }},
				{96.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Part2_EntranceDay"), false, false); }},
				{98.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			for (const FStep& St : P2Steps)
				if (Before < St.At && ShotClock >= St.At) St.Do();
		}
		// -UIPart1: the tower cab walked forward (where you really go, "moonwalking"), the exit signs from inside the hall
		if (FParse::Param(FCommandLine::Get(), TEXT("UIPart1")))
		{
			static FVector CabStart, CabFwd;
			APawn* Me = GetOwningPawn();
			if (Me && ShotClock > 50.f && ShotClock < 54.f) Me->AddMovementInput(Me->GetActorForwardVector(), 1.f);
			struct FStep { float At; TFunction<void()> Do; };
			const FStep P1Steps[] = {
				{44.f, [&]() { if (bGame) ToggleGame(); ASunClock::HourOverride = 13.f; ASunClock::Refresh(); GoTowerCab(); }},
				{49.f, [&]() { FScreenshotRequest::RequestScreenshot(TEXT("Part1_Cab"), false, false); }},
				{50.f, [&]() { if (Me) { CabStart = Me->GetActorLocation(); CabFwd = Me->GetActorForwardVector(); } }},
				{55.f, [&]() { if (Me) { const FVector D = Me->GetActorLocation() - CabStart; ADP_LOG(TEXT("P1TEST cab walk: moved %.2f m, along the facing %.2f m, sideways %.2f m, up %.2f m"), D.Size2D() / 100.f, FVector::DotProduct(D, CabFwd) / 100.f, FVector::CrossProduct(CabFwd, D).Z / 100.f, D.Z / 100.f); } }},
				{56.f, [&]() { GoTo(FVector(75.f, 0.f, 9.f), FVector(75.f, 3.6f, 0.f)); }},
				{63.f, [&]() { FScreenshotRequest::RequestScreenshot(TEXT("Part1_ExitSign"), false, false); }},
				{64.f, [&]() { GoTo(FVector(101.f, 0.f, 7.f), FVector(101.f, 3.3f, 0.f)); }},
				{70.f, [&]() { FScreenshotRequest::RequestScreenshot(TEXT("Part1_ExitWest"), false, false); }},
				{72.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			for (const FStep& St : P1Steps)
				if (Before < St.At && ShotClock >= St.At) St.Do();
		}
		// -UIAnywhere: walk anywhere: Tórshavn, Skógafoss, Zermatt, then back to the airport
		if (FParse::Param(FCommandLine::Get(), TEXT("UIAnywhere")))
		{
			auto Look = [this](float Yaw, float Pitch) { if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(FRotator(Pitch, Yaw, 0.f)); };
			struct FStep { float At; TFunction<void()> Do; };
			const FStep AnySteps[] = {
				{44.f, [&]() { if (bGame) ToggleGame(); ASunClock::DayOverride = 172; ASunClock::HourOverride = 14.f; ASunClock::Theme = 1; ASunClock::Refresh(); AWalkAnywhere::GoText(GetWorld(), TEXT("Tórshavn")); }},
				{75.f, [&]() { Look(0.f, -5.f); ADP_LOG(TEXT("ANYTEST %s"), *AWalkAnywhere::Status()); }},
				{78.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Anywhere_Torshavn"), false, false); }},
				{79.f, [&]() { AWalkAnywhere::GoText(GetWorld(), TEXT("Skógafoss")); }},
				{110.f, [&]() { Look(270.f, 5.f); ADP_LOG(TEXT("ANYTEST %s"), *AWalkAnywhere::Status()); }},
				{113.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Anywhere_Skogafoss"), false, false); }},
				{114.f, [&]() { AWalkAnywhere::GoText(GetWorld(), TEXT("46.0207, 7.7491")); }},
				{145.f, [&]() { Look(250.f, 8.f); ADP_LOG(TEXT("ANYTEST %s"), *AWalkAnywhere::Status()); }},
				{148.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Anywhere_Zermatt"), false, false); }},
				{149.f, [&]() { AWalkAnywhere::Back(GetWorld()); }},
				{158.f, [&]() { ADP_LOG(TEXT("ANYTEST %s"), *AWalkAnywhere::Status()); FScreenshotRequest::RequestScreenshot(TEXT("Anywhere_Back"), false, false); }},
				{162.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			for (const FStep& St : AnySteps)
				if (Before < St.At && ShotClock >= St.At) St.Do();
		}
		// -UIGround: standing on the real ground round the airport (beyond the airfield): grass where the land cover is grassy
		if (FParse::Param(FCommandLine::Get(), TEXT("UIGround")))
		{
			static TWeakObjectPtr<ACameraActor> GCam;
			auto Stand = [this](float X, float Y, float Yaw)
			{
				APawn* P = GetOwningPawn();
				if (!P) return;
				FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPGround), true, P);
				FVector At(X * 100.f, Y * 100.f, 0.f);
				if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 60000.f), At - FVector(0, 0, 60000.f), ECC_Visibility, Q)) At = Hit.ImpactPoint;
				P->SetActorLocation(At + FVector(0, 0, P->GetSimpleCollisionHalfHeight() + 20.f), false, nullptr, ETeleportType::TeleportPhysics);
				if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(FRotator(-8.f, Yaw, 0.f));
				ADP_LOG(TEXT("GROUNDTEST at %.0f %.0f ground %.1f m hit %s"), X, Y, At.Z / 100.f, Hit.GetActor() ? *Hit.GetActor()->GetClass()->GetName() : TEXT("nothing"));
			};
			struct FStep { float At; TFunction<void()> Do; };
			const FStep GroundSteps[] = {
				{44.f, [&]() { if (bGame) ToggleGame(); ASunClock::DayOverride = 172; ASunClock::HourOverride = 14.f; ASunClock::Theme = 1; ASunClock::Refresh(); Stand(1200.f, 0.f, 0.f); }},
				{60.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Ground_1"), false, false); }},
				{61.f, [&]() { Stand(0.f, 1500.f, 90.f); }},
				{77.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Ground_2"), false, false); }},
				{78.f, [&]() { Stand(0.f, -1500.f, 270.f); }},
				{94.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("Ground_3"), false, false); }},
				{96.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			for (const FStep& St : GroundSteps)
				if (Before < St.At && ShotClock >= St.At) St.Do();
		}
		// -UIWorld: the world from the air: the land cover on this PC (ESA WorldCover), then the satellite pictures,
		// from 1.5 km, 8 km and 60 km above the airport, looking out over the land
		if (FParse::Param(FCommandLine::Get(), TEXT("UIWorld")))
		{
			static TWeakObjectPtr<ACameraActor> Cam;
			auto Air = [this](float Up, float Back, float LookDown)
			{
				if (!Cam.IsValid()) Cam = GetWorld()->SpawnActor<ACameraActor>();
				if (!Cam.IsValid()) return;
				Cam->SetActorLocationAndRotation(FVector(-Back * 100.f, Back * 100.f, Up * 100.f), FRotator(-LookDown, -45.f, 0.f));
				Cam->GetCameraComponent()->SetFieldOfView(70.f);
				if (APlayerController* PC = GetOwningPlayerController()) PC->SetViewTarget(Cam.Get());
			};
			struct FStep { float At; TFunction<void()> Do; };
			const FStep WorldSteps[] = {
				{44.f, [&]() { if (bGame) ToggleGame(); ASunClock::DayOverride = 172; ASunClock::HourOverride = 13.f; ASunClock::Theme = 0; ASunClock::Refresh(); for (TActorIterator<AVolumetricCloud> It(GetWorld()); It; ++It) It->SetActorHiddenInGame(true); AGlobe::UseSource(3); Air(1500.f, 3000.f, 18.f); }},
				{60.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("World_LandCover_1500m"), false, false); }},
				{61.f, [&]() { if (Cam.IsValid()) { Cam->SetActorLocationAndRotation(FVector(0.f, -600000.f, 40000.f), FRotator(-89.f, 0.f, 0.f)); } }},
				{90.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("World_LandCover_Down400m"), false, false); }},
				{91.f, [&]() { Air(8000.f, 12000.f, 25.f); }},
				{77.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("World_LandCover_8km"), false, false); }},
				{78.f, [&]() { Air(60000.f, 60000.f, 45.f); }},
				{96.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("World_LandCover_60km"), false, false); }},
				{97.f, [&]() { AGlobe::UseSource(0); Air(8000.f, 12000.f, 25.f); }},
				{115.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("World_Satellite_8km"), false, false); }},
				{116.f, [&]() { ADP_LOG(TEXT("WORLDTEST %s"), *AGlobe::Note);
					// what is big and flat out there: every actor over 5 km wide, then the ocean and the airfield hidden
					for (TActorIterator<AActor> It(GetWorld()); It; ++It) { FVector O, E; It->GetActorBounds(false, O, E); if (E.X > 250000.f || E.Y > 250000.f) ADP_LOG(TEXT("WORLDTEST big %s %s %.0f x %.0f km z %.0f..%.0f m"), *It->GetName(), *It->GetClass()->GetName(), E.X / 50000.f, E.Y / 50000.f, (O.Z - E.Z) / 100.f, (O.Z + E.Z) / 100.f); }
					for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->GetClass()->GetName().Contains(TEXT("Ocean"))) It->SetActorHiddenInGame(true); }},
				{124.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("World_NoOcean_8km"), false, false); }},
				{125.f, [&]() { for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->GetClass()->GetName().Contains(TEXT("Airfield")) || It->GetClass()->GetName().Contains(TEXT("Grass"))) It->SetActorHiddenInGame(true); }},
				{133.f, []() { FScreenshotRequest::RequestScreenshot(TEXT("World_NoField_8km"), false, false); }},
				{135.f, []() { FGenericPlatformMisc::RequestExit(false); }},
			};
			for (const FStep& St : WorldSteps)
				if (Before < St.At && ShotClock >= St.At) St.Do();
		}
		// -UIQuit: the title screen's gear (Unreal's Settings over the menus, a picture), then its Exit (the game must quit)
		if (FParse::Param(FCommandLine::Get(), TEXT("UIQuit")) && Browser.IsValid())
		{
			if (Before < 30.f && ShotClock >= 30.f) Browser->ExecuteJavascript(TEXT("document.querySelector('.gear').click()"));
			if (Before < 36.f && ShotClock >= 36.f) FScreenshotRequest::RequestScreenshot(TEXT("TitleSettings"), true, false);
			if (Before < 33.f && ShotClock >= 33.f) Browser->ExecuteJavascript(TEXT("Promise.resolve(ue.adp.savewrite('test-1.json','hello save')).then(function(r){ue.adp.log('SAVETEST write '+r);return ue.adp.saveread('test-1.json');}).then(function(s){ue.adp.log('SAVETEST read '+s);ue.adp.savewrite('test-1.json','');}).catch(function(e){ue.adp.log('SAVETEST error '+e);});"));
			if (Before < 38.f && ShotClock >= 38.f) { if (bSettings) ToggleSettings(); Browser->ExecuteJavascript(TEXT("document.querySelector('.exit').click()")); }
		}
		// -UIUpd: the menus' Settings > Updates asks Unreal to check, and reports what came back
		if (FParse::Param(FCommandLine::Get(), TEXT("UIUpd")) && Browser.IsValid())
		{
			if (Before < 30.f && ShotClock >= 30.f) Browser->ExecuteJavascript(TEXT("var __o=window.__ueUpdate;window.__ueUpdate=function(s,c,b,v){ue.adp.log('UPDTEST got: '+s+' can='+c+' busy='+b+' ver='+v);if(__o)__o(s,c,b,v);};ue.adp.log('UPDTEST hooked='+(typeof __o));ue.adp.checkupdates();"));
		}
		// -UISite: outside at the kerb (the light, the sky, the credits), and a picture
		const bool bSite = FParse::Param(FCommandLine::Get(), TEXT("UISite"));
		if (bSite && Before < 48.f && ShotClock >= 48.f) { OpenSite(); if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(FRotator(5.f, 90.f, 0)); }
		if (bSite && Before < 58.f && ShotClock >= 58.f) FScreenshotRequest::RequestScreenshot(TEXT("Site"), true, false);
		// -UIDoor: walk up to a sliding entrance door (it should open), and a picture of it
		const bool bDoor = FParse::Param(FCommandLine::Get(), TEXT("UIDoor"));
				if (bDoor && ((Before < 48.f && ShotClock >= 48.f) || (Before < 54.f && ShotClock >= 54.f)))
		{
			const float Back = ShotClock < 50.f ? 600.f : 200.f;   // first from 6 m (shut), then from 2 m (open)
			if (bGame) ToggleGame();
			FVector At;
			for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
				if (It->SlidingDoorAt(At))
					if (APawn* Me = GetOwningPawn())
					{
						const FVector From = At + FVector(0, Back, Me->GetSimpleCollisionHalfHeight() + 10.f);
						const FRotator Look = (At + FVector(0, 0, 150.f) - From).Rotation();
						Me->TeleportTo(From, FRotator(0, Look.Yaw, 0));
						if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(Look);
						break;
					}
		}
		if (bDoor && Before < 52.f && ShotClock >= 52.f) FScreenshotRequest::RequestScreenshot(TEXT("DoorShut"), true, false);
		if (bDoor && Before < 58.f && ShotClock >= 58.f) FScreenshotRequest::RequestScreenshot(TEXT("DoorOpen"), true, false);
		if (!FParse::Param(FCommandLine::Get(), TEXT("UIRoof")) && !FParse::Param(FCommandLine::Get(), TEXT("UIPillar")) && !FParse::Param(FCommandLine::Get(), TEXT("UINew")) && !FParse::Param(FCommandLine::Get(), TEXT("UITower")) && !FParse::Param(FCommandLine::Get(), TEXT("UIBright")) && !FParse::Param(FCommandLine::Get(), TEXT("UIFire")) && !FParse::Param(FCommandLine::Get(), TEXT("UIMap")) && !FParse::Param(FCommandLine::Get(), TEXT("UIMasts")) && !FParse::Param(FCommandLine::Get(), TEXT("UIWorld")) && !FParse::Param(FCommandLine::Get(), TEXT("UIGround")) && !FParse::Param(FCommandLine::Get(), TEXT("UIAnywhere")) && !FParse::Param(FCommandLine::Get(), TEXT("UIPart1")) && !FParse::Param(FCommandLine::Get(), TEXT("UIPart2")) && !FParse::Param(FCommandLine::Get(), TEXT("UILoading")) && !FParse::Param(FCommandLine::Get(), TEXT("UIOffices")) && !FParse::Param(FCommandLine::Get(), TEXT("UIDevice")) && !FParse::Param(FCommandLine::Get(), TEXT("UIHangars")) && !FParse::Param(FCommandLine::Get(), TEXT("UIFireStation")) && !FParse::Param(FCommandLine::Get(), TEXT("UIDrive")) && !FParse::Param(FCommandLine::Get(), TEXT("UISeason")) && !FParse::Param(FCommandLine::Get(), TEXT("UIWardrobe")) && !FParse::Param(FCommandLine::Get(), TEXT("UIAirfield")) && !FParse::Param(FCommandLine::Get(), TEXT("UIPuddles")) && !FParse::Param(FCommandLine::Get(), TEXT("UIBody")) && !FParse::Param(FCommandLine::Get(), TEXT("UIRun")) && !FParse::Param(FCommandLine::Get(), TEXT("UIDrone")) && !FParse::Param(FCommandLine::Get(), TEXT("UITitle")) && !FParse::Param(FCommandLine::Get(), TEXT("UIHallWalk")) && !FParse::Param(FCommandLine::Get(), TEXT("UIGender")) && !FParse::Param(FCommandLine::Get(), TEXT("UINewGame")) && !FParse::Param(FCommandLine::Get(), TEXT("UIGate")) && !FParse::Param(FCommandLine::Get(), TEXT("UIClouds")) && !(FParse::Param(FCommandLine::Get(), TEXT("UIPeople")) || FCString::Stristr(FCommandLine::Get(), TEXT("-UIPeople=")) != nullptr) && ShotClock >= (bOverview ? 92.f : bLounge ? 80.f : bDoor ? 62.f : bSite ? 62.f : bShort ? 78.f : bHall ? 103.f : 52.f)) FGenericPlatformMisc::RequestExit(false);
	}
	// the announcements, one at a time: the gong, then the words for ten seconds
	// the announcements: the chime, then the announcer; how loud they are follows where you are
	HeardGain = FMath::FInterpTo(HeardGain, ZoneGain(), Dt, 2.f);
	// the indoor lighting blends in through the doors
	IndoorWeight = FMath::FInterpTo(IndoorWeight, ZoneKind().IsEmpty() ? 0.f : 1.f, Dt, 1.5f);
	if (Indoor) Indoor->BlendWeight = IndoorWeight;
	for (UAudioComponent* A : {VoiceOut.Get(), GongOut.Get()})
		if (A) { A->SetVolumeMultiplier(FMath::Max(0.001f, CallGain())); A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency(ZoneMuffle()); }
	if ((CallsClock += Dt) >= 1.f) { CallsClock = 0; RunCalls(); }
	if (SpeakIn > 0 && (SpeakIn -= Dt) <= 0) Speak(CallText);
	if (CaptionTime > 0) CaptionTime -= Dt;
	// only in the 3D view: while the menus are open the calls wait (and old ones are dropped)
	else if (Calls.Num() && bGame) Calls.RemoveAt(0);   // in the menus: read on the Overview instead, never played later
	else if (Calls.Num()) { CallText = Calls[0].Key; CallKind = Calls[0].Value; Calls.RemoveAt(0); CaptionTime = 12.f; PlayGong(); SpeakIn = 1.7f; }
	if (Dt > 0) FpsSmooth = FMath::Lerp(FpsSmooth, 1.f / Dt, 0.05f);
	const APawn* P = GetOwningPawn();
	if (P && HoloMap.IsValid())
	{
		const FVector L = P->GetActorLocation();
		const float Feet = (L.Z - P->GetSimpleCollisionHalfHeight()) / 100.f;
		HoloMap->SetPlayer(FVector(L.X / 100.f, Feet, -L.Y / 100.f), P->GetControlRotation().Yaw);
	}
}

#pragma endregion

TSharedRef<SWidget> AADPHUD::WalkPlaces()
{
	// every named place as a button, then the way back
	TSharedRef<SWrapBox> Box = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6, 6));
	for (const AWalkAnywhere::FPlace& Pl : AWalkAnywhere::Places())
	{
		const double La = Pl.Lat, Lo = Pl.Lon; const FString N = Pl.Name;
		Box->AddSlot()[Button(N.ToUpper(), [this, La, Lo, N]() { AWalkAnywhere::Go(GetWorld(), Lo, La, N); if (bSettings) ToggleSettings(); })];
	}
	Box->AddSlot()[Button(TEXT("BACK TO THE AIRPORT"), [this]() { AWalkAnywhere::Back(GetWorld()); if (bSettings) ToggleSettings(); })];
	return Box;
}
