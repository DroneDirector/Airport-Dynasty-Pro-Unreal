// The walking-mode interface: the holographic map, the top-left bar, Hide UI, the settings panel and the performance
// overlay, all in Slate, styled in the Android edition's holographic blues.
#include "UI/ADPHUD.h"
#include "UI/SHoloMap.h"
#include "UI/GameView.h"
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
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

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

	FTerminalPlan Plan;
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
			SAssignNew(MiniBox, SBox).WidthOverride(240).HeightOverride(240)[HoloMap.ToSharedRef()]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(16)[Perf.ToSharedRef()]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Settings.ToSharedRef()]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0, 70, 0, 0)
		[
			SAssignNew(CaptionBox, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Panel)
			.Padding(FMargin(16, 8))
			.Visibility_Lambda([this]() { return CaptionTime > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[SNew(STextBlock).Font(Font(12)).ColorAndOpacity(Ink).Text_Lambda([this]() { return FText::FromString(TEXT("ANNOUNCEMENT  ·  ") + CallText); })]
		];

	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->AddViewportWidgetContent(Root.ToSharedRef(), 10);

	// the keys: M map, Tab settings, P performance, H hide, Esc or a double-click to show again
	if (APlayerController* PC = GetOwningPlayerController())
	{
		EnableInput(PC);
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
	Super::EndPlay(Reason);
}

void AADPHUD::FirstStartScan()
{
	// the device scan, once: the PC is measured and the quality set from it; later changes are the player's own
	const FString Flag = FPaths::ProjectSavedDir() / TEXT("DeviceScanned.txt");
	if (FPaths::FileExists(Flag)) return;
	if (UGameUserSettings* S = Prefs())
	{
		S->RunHardwareBenchmark();
		S->ApplyHardwareBenchmarkResults();
		Save(S);
	}
	FFileHelper::SaveStringToFile(TEXT("scanned"), *Flag);
}

#pragma endregion

#pragma region Panels

TSharedRef<SWidget> AADPHUD::MakeTopBar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[Button(TEXT("GAME  (G)"), [this]() { ToggleGame(); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[Button(TEXT("MAP  (M)"), [this]() { ToggleMap(); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[Button(TEXT("SETTINGS  (TAB)"), [this]() { ToggleSettings(); })]
		+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("HIDE UI  (H)"), [this]() { HideUI(); })];
}

TSharedRef<SWidget> AADPHUD::MakeSettings()
{
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
	if (!HoloMap.IsValid() || bHidden) return;
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
	if (bGame && !bGameMade) { StaticCastSharedPtr<SBox>(GameBox)->SetContent(ADPGameView::Make()); bGameMade = true; }
	GameBox->SetVisibility(bGame ? EVisibility::Visible : EVisibility::Collapsed);
	MiniBox->SetVisibility(bGame ? EVisibility::Collapsed : EVisibility::Visible);
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

TArray<FString> AADPHUD::Calls;

void AADPHUD::Announce(const FString& Words) { if (Calls.Num() < 6) Calls.Add(Words); }

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
	UGameplayStatics::PlaySound2D(this, Gong);
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

void AADPHUD::Tick(float Dt)
{
	Super::Tick(Dt);
	// the announcements, one at a time: the gong, then the words for ten seconds
	if (CaptionTime > 0) CaptionTime -= Dt;
	else if (Calls.Num()) { CallText = Calls[0]; Calls.RemoveAt(0); CaptionTime = 10.f; PlayGong(); }
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
