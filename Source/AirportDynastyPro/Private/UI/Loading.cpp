// The loading screen into airport mode (owner, 2026-11-11): from the menus (the Overview's Terminal or Airport site, or
// the quick shortcut) into the 3D airport, a screen that really waits until everything is in: the shaders compiled,
// the terrain and its pictures round the airport, the textures streamed. A progress bar (its percentage in the middle)
// and a line saying what it is doing; Esc on it goes straight back to the menus. Test runs skip it.
#include "UI/ADPHUD.h"
#include "UI/SProgressText.h"
#include "Cesium3DTileset.h"
#include "ContentStreaming.h"
#include "EngineUtils.h"
#include "ShaderCompiler.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> AADPHUD::MakeLoading()
{
	const FLinearColor InkC(0.85f, 0.95f, 1.f);
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.01f, 0.03f, 0.06f, 1.f))
		.HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(640.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 18)
				[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 22)).ColorAndOpacity(InkC).Text(FText::FromString(TEXT("AIRPORT DYNASTY PRO")))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[SNew(SProgressText).Height(26.f).Fraction_Lambda([this]() { return LoadFrac; })]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 24)
				[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11)).ColorAndOpacity(InkC).Text_Lambda([this]() { return FText::FromString(LoadStep); })]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)).ColorAndOpacity(FLinearColor(0.5f, 0.62f, 0.72f)).Text(FText::FromString(TEXT("Esc: back to the menus")))]
			]
		];
}

void AADPHUD::StartLoading()
{
	if (!LoadingBox.IsValid()) return;
	bLoading = true; LoadT = 0; LoadFrac = 0; LoadStep = TEXT("Getting the airport ready…");
	ShadersMax = 1; TexMax = 1;
	LoadingBox->SetVisibility(EVisibility::Visible);
}

void AADPHUD::StopLoading()
{
	bLoading = false;
	if (LoadingBox.IsValid()) LoadingBox->SetVisibility(EVisibility::Collapsed);
}

void AADPHUD::TickLoading(float Dt)
{
	if (!bLoading) return;
	LoadT += Dt;
	// the shaders still to compile, the terrain's tiles round here, the textures still streaming in
	const int32 Shaders = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
	ShadersMax = FMath::Max(ShadersMax, Shaders);
	float Terrain = 1.f;
	for (TActorIterator<ACesium3DTileset> It(GetWorld()); It; ++It) if (!It->IsHidden()) Terrain = FMath::Min(Terrain, It->GetLoadProgress() / 100.f);
	const int32 Tex = IStreamingManager::Get().GetNumWantingResources();
	TexMax = FMath::Max(TexMax, Tex);
	const float S = 1.f - float(Shaders) / float(ShadersMax), T = 1.f - float(Tex) / float(TexMax);
	// the pictures over the terrain aren't reported: a few seconds more once the terrain is in
	const float Pictures = FMath::Clamp((LoadT - 2.f) / 6.f, 0.f, 1.f);
	const float Want = (S + Terrain + T + Pictures) / 4.f;
	LoadFrac = FMath::Max(LoadFrac, FMath::Min(Want, LoadFrac + Dt * 0.6f));   // never backwards, smooth
	LoadStep = Shaders > 0 ? FString::Printf(TEXT("Compiling shaders… (%d to go)"), Shaders)
		: Terrain < 0.99f ? TEXT("Loading the terrain round the airport…")
		: Tex > 0 ? TEXT("Loading the textures…")
		: Pictures < 1.f ? TEXT("Loading the satellite pictures…") : TEXT("Ready");
	const bool bDone = Shaders == 0 && Terrain >= 0.99f && Tex == 0 && Pictures >= 1.f && LoadFrac >= 0.999f;
	if (bDone || LoadT > 120.f) { LoadFrac = 1.f; StopLoading(); }
}
