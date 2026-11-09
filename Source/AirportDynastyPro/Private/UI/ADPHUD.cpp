// The walking-mode interface: the holographic map, the top-left bar, Hide UI, the settings panel and the performance
// overlay, all in Slate, styled in the Android edition's holographic blues.
#include "UI/ADPHUD.h"
#include "UI/SHoloMap.h"
#include "UI/GameView.h"
#include "UI/ADPBridge.h"
#include "UI/InputGlyphs.h"
#include "UnrealClient.h"
#include "TerminalPlan.h"
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
	TSharedRef<SWidget> Caption(const FString& Words)
	{
		return SNew(STextBlock).Text(FText::FromString(Words)).Font(Font(10)).ColorAndOpacity(Ink);
	}
	UGameUserSettings* Prefs() { return GEngine ? GEngine->GetGameUserSettings() : nullptr; }
	void Save(UGameUserSettings* S) { if (S) { S->ApplySettings(false); S->SaveSettings(); } }
}
#pragma endregion

#pragma region Setup

void AADPHUD::BeginPlay()
{
	Super::BeginPlay();
	PrimaryActorTick.bCanEverTick = true;
	FirstStartScan();

	Plan.Load();
	HoloMap = SNew(SHoloMap);
	HoloMap->SetPlan(Plan);
	HoloMap->OnOpen = [this]() { ToggleMap(); };
	Settings = MakeSettings();
	Perf = MakePerf();
	TopBar = MakeTopBar();
	Settings->SetVisibility(EVisibility::Collapsed);
	Perf->SetVisibility(EVisibility::Collapsed);

	GameBox = SNew(SBox);
	GameBox->SetVisibility(EVisibility::Collapsed);

	Root = SNew(SOverlay)
		+ SOverlay::Slot()[GameBox.ToSharedRef()]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(16)[TopBar.ToSharedRef()]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(16)
		[
			SAssignNew(MiniBox, SBox).WidthOverride(240).HeightOverride(240)
			.Visibility_Lambda([this]() { return bGame || bInLounge ? EVisibility::Collapsed : EVisibility::Visible; })[HoloMap.ToSharedRef()]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(16)[Perf.ToSharedRef()]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Settings.ToSharedRef()]
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
					.Visibility_Lambda([this]() { return bButtonsOpen ? EVisibility::Visible : EVisibility::Collapsed; })
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
			SAssignNew(Credit, STextBlock).Font(Font(7, "Regular")).ColorAndOpacity(FLinearColor(1, 1, 1, 0.55f))
			.Text(FText::FromString(TEXT("Sentinel-2 cloudless 2016 (s2maps.eu) by EOX IT Services GmbH, contains modified Copernicus Sentinel data 2016 · Cesium")))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0, 70, 0, 0)
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
		InputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &AADPHUD::OnAnyKey);
		InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AADPHUD::ToggleButtons);
		InputComponent->BindKey(EKeys::Gamepad_Special_Left, IE_Pressed, this, &AADPHUD::ToggleButtons);
		InputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Pressed, this, &AADPHUD::ToggleGame);
		InputComponent->BindKey(EKeys::Gamepad_RightShoulder, IE_Pressed, this, &AADPHUD::ToggleMap);
		InputComponent->BindKey(EKeys::Gamepad_LeftShoulder, IE_Pressed, this, &AADPHUD::ToggleHide);
		InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &AADPHUD::ToggleSettings);
		InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &AADPHUD::Interact);
		InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AADPHUD::ToggleGame);
		InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AADPHUD::Interact);
		InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AADPHUD::ToggleMap);
		InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AADPHUD::ToggleSettings);
		InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AADPHUD::TogglePerf);
		InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AADPHUD::HideUI);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AADPHUD::ShowUI);
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
				+ SVerticalBox::Slot().AutoHeight()[Caption(TEXT("Frame limit"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 10)[Limit]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[Button(TEXT("VSYNC ON / OFF"), []() { if (UGameUserSettings* S = Prefs()) { S->SetVSyncEnabled(!S->IsVSyncEnabled()); Save(S); } })]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[Button(TEXT("PERFORMANCE  (P)"), [this]() { TogglePerf(); })]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[Button(TEXT("SCAN MY PC"), []() { if (UGameUserSettings* S = Prefs()) { S->RunHardwareBenchmark(); S->ApplyHardwareBenchmarkResults(); Save(S); } })]
					+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("PROBLEM LOG"), []() { FPlatformProcess::ExploreFolder(*(FPaths::ProjectLogDir() / TEXT("Problems.log"))); })]
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
	const bool bFree = bGame || bSettings || (HoloMap.IsValid() && HoloMap->IsFull());
	PC->bShowMouseCursor = bFree;
	if (bFree) { FInputModeGameAndUI M; M.SetHideCursorDuringCapture(false); PC->SetInputMode(M); }
	else PC->SetInputMode(FInputModeGameOnly());
}

void AADPHUD::ToggleMap()
{
	if (!HoloMap.IsValid() || bHidden || bInLounge) return;
	const bool bFull = !HoloMap->IsFull();
	HoloMap->SetFull(bFull);
	// full screen: the map fills most of the window; small: back in its corner
	StaticCastSharedPtr<SBox>(MiniBox)->SetWidthOverride(bFull ? FOptionalSize(1200) : FOptionalSize(240));
	StaticCastSharedPtr<SBox>(MiniBox)->SetHeightOverride(bFull ? FOptionalSize(720) : FOptionalSize(240));
	UpdateMouse();
}

void AADPHUD::ToggleGame()
{
	if (bHidden) return;
	bGame = !bGame;
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

void AADPHUD::ToggleSettings()
{
	if (bHidden) return;
	bSettings = !bSettings;
	Settings->SetVisibility(bSettings ? EVisibility::Visible : EVisibility::Collapsed);
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

void AADPHUD::Announce(const FString& Words, uint8 Kind) { if (Calls.Num() < 8) Calls.Add(TPair<FString, uint8>(Words, Kind)); }

void AADPHUD::PlayGong()
{
	// the classic airport call: two tones, high then low, made here (no recording needed)
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
	for (const float Hz : {784.f, 523.f})
		for (int32 i = 0; i < Rate * 0.8f; ++i)
		{
			const float T = i / float(Rate), Env = FMath::Exp(-T * 3.2f) * FMath::Min(1.f, T * 80.f);
			const float V = (FMath::Sin(2 * PI * Hz * T) + 0.3f * FMath::Sin(4 * PI * Hz * T)) * Env * 0.35f;
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
	if (!PC || bGame) return;
	FVector Eye; FRotator Look;
	PC->GetPlayerViewPoint(Eye, Look);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPInteract), false, GetOwningPawn());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Look.Vector() * 300.f, ECC_Visibility, Q) && Hit.GetComponent())
	{
		const FString N = Hit.GetComponent()->GetName();
		if (N.Contains(TEXT("screenPanel")) || N.Contains(TEXT("black"))) ToggleGame();
	}
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
	ShowTerminal();
	// outside the main entrance, at the kerb under the wave roof, facing the terminal
	if (APawn* P = GetOwningPawn())
	{
		P->TeleportTo(FTerminalPlan::ToWorld(79.f, 0.f, -14.f) + FVector(0, 0, P->GetSimpleCollisionHalfHeight() + 30.f), FRotator(0, -90.f, 0));
		if (APlayerController* PC = GetOwningPlayerController()) PC->SetControlRotation(FRotator(0, -90.f, 0));
	}
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
		for (const ADPInput::FAction& A : ADPInput::Actions())
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
	// the symbols follow the device in use
	if (bShownPad != ADPInput::UsingPad() || ShownPadKind != uint8(ADPInput::CurrentPad()))
	{
		bShownPad = ADPInput::UsingPad();
		ShownPadKind = uint8(ADPInput::CurrentPad());
		RefreshControls();
	}
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
		if (ShotClock >= (bOverview ? 92.f : bLounge ? 80.f : bDoor ? 62.f : 52.f)) FGenericPlatformMisc::RequestExit(false);
	}
	// the announcements, one at a time: the gong, then the words for ten seconds
	// the announcements: the chime, then the announcer; how loud they are follows where you are
	HeardGain = FMath::FInterpTo(HeardGain, ZoneGain(), Dt, 2.f);
	for (UAudioComponent* A : {VoiceOut.Get(), GongOut.Get()})
		if (A) { A->SetVolumeMultiplier(FMath::Max(0.001f, CallGain())); A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency(ZoneMuffle()); }
	if ((CallsClock += Dt) >= 1.f) { CallsClock = 0; RunCalls(); }
	if (SpeakIn > 0 && (SpeakIn -= Dt) <= 0) Speak(CallText);
	if (CaptionTime > 0) CaptionTime -= Dt;
	// only in the 3D view: while the menus are open the calls wait (and old ones are dropped)
	else if (Calls.Num() && !bGame) { CallText = Calls[0].Key; CallKind = Calls[0].Value; Calls.RemoveAt(0); CaptionTime = 12.f; PlayGong(); SpeakIn = 1.7f; }
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
