// The loading screen into airport mode (owner, 2026-11-11): from the menus (the Overview's Terminal or Airport site, or
// the quick shortcut) into the 3D airport, a screen that really waits until everything is in: the shaders compiled,
// the terrain and its pictures round the airport, the textures streamed. A progress bar (its percentage in the middle)
// and a line saying what it is doing; Esc on it goes straight back to the menus. Test runs skip it.
#include "UI/ADPHUD.h"
#include "UI/SNeonLoading.h"
#include "ADPLog.h"
#include "People.h"
#include "PaxNav.h"
// (Windows' own OPAQUE macro would break Cesium's glTF names when the build puts a Windows file before this one)
#ifdef OPAQUE
#undef OPAQUE
#endif
#include "Cesium3DTileset.h"
#include "ContentStreaming.h"
#include "EngineUtils.h"
#include "ShaderCompiler.h"
#include "ShaderPipelineCache.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

// owner, 2026-11-17: the plain blue screen ("Airport Dynasty Pro", nothing else) replaced by the neon one, the same as
// the title screen's web loading screen (SNeonLoading), and like it: once all is in, "Loading complete · Get ready for
// takeoff" for 4 s before the airport shows
static double GLoadDoneAt = -1.0;
// the shipped shader cache (to-do 75, owner 2026-11-19): once the airport itself is in, "Loading shader cache" - the
// recorded pipelines (day and night, Build/Windows/PipelineCaches) built at full speed before you see the airport, so
// the first lamp at night or the first rain doesn't stall; afterwards whatever is left goes on quietly in the background
static int32 GPsoMax = 0; static double GPsoFrom = -1.0;
// (owner, 2026-11-23: a new career's loading hung at 92 %, "Loading taf": the menus' page had to say the game was in, its
// own code had failed, and nothing ever gave up.) New game, Continue and Load: this screen from the first moment - the
// world and your career first (NewGameLoad.cpp), then the airport here, one bar; the page is never waited for, nor the
// internet, and every wait has an end.
static constexpr float GStartPart = 0.5f;   // (the bar's first half: what NewGameLoad.cpp loads)
static TWeakPtr<SNeonLoading> GNeon;
static float GBase = 0.f;

TSharedRef<SWidget> AADPHUD::MakeLoading()
{
	TSharedRef<SNeonLoading> Neon = SNew(SNeonLoading)
		.Kicker(TEXT("Airport Dynasty Pro  ·  into the airport"))
		.Title_Lambda([this]() { return bStartLoad ? FString(TEXT("Loading game assets")) : FString(TEXT("Loading the airport")); })
		.Step_Lambda([this]() { return LoadStep; })
		.Fraction_Lambda([this]() { return LoadFrac; })
		.Done_Lambda([]() { return GLoadDoneAt >= 0.0; })
		.Hint_Lambda([this]() { return bStartLoad || GBase > 0.f ? FString() : FString(TEXT("Esc: back to the menus")); });
	GNeon = Neon;
	return Neon;
}

void AADPHUD::StartLoading()
{
	if (!LoadingBox.IsValid()) return;
	// (after New game, Continue or Load the bar carries on where the world's loading left it)
	GBase = bLoadCarryOn ? GStartPart : 0.f;
	if (!bLoadCarryOn) if (TSharedPtr<SNeonLoading> N = GNeon.Pin()) N->Restart();
	bLoadCarryOn = false;
	bLoading = true; LoadT = 0; LoadFrac = GBase; LoadStep = TEXT("Getting the airport ready…"); GLoadDoneAt = -1.0; GPsoMax = 0; GPsoFrom = -1.0;
	ShadersMax = 1; TexMax = 1;
	LoadingBox->SetVisibility(EVisibility::Visible);
}

void AADPHUD::ShowStartLoad()
{
	if (!LoadingBox.IsValid()) return;
	bStartLoad = true; bLoading = true; StartLoadT = 0.f; LoadT = 0.f; LoadFrac = 0.f; LoadStep = TEXT("Loading game assets"); GLoadDoneAt = -1.0; GBase = 0.f;
	if (TSharedPtr<SNeonLoading> N = GNeon.Pin()) N->Restart();
	LoadingBox->SetVisibility(EVisibility::Visible);
	// (the keys away from the page under it: nothing it has can be pressed by accident meanwhile)
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

void AADPHUD::EndStartLoad()
{
	if (!bStartLoad) return;
	bStartLoad = false; bLoadCarryOn = true;
	// straight into the airport on foot (a new career: outside the main hall; else where you were), behind this screen
	OpenSite();
	bWasGame = bGame;   // (the loading goes on from here itself)
	// (a test run's own steps don't wait for the airport's part, as ever)
	static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("UIShot")) && !FParse::Param(FCommandLine::Get(), TEXT("UILoading"));
	if (bTest) { bLoadCarryOn = false; StopLoading(); } else StartLoading();
}

void AADPHUD::StopLoading()
{
	bLoading = false; GLoadDoneAt = -1.0; GBase = 0.f;
	if (AADPPeople* Pp = AADPPeople::Get(GetWorld())) Pp->StartWanderers();   // (the passengers about the terminal, once it is in - owner, 2026-11-19)
	FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Background);   // (the rest, if any: quietly)
	if (LoadingBox.IsValid()) LoadingBox->SetVisibility(EVisibility::Collapsed);
}

void AADPHUD::TickLoading(float Dt)
{
	if (!bLoading) return;
	LoadT += Dt;
	PaxNav::Kick(GetWorld());   // (the walkers' navmesh made behind the loading screen)
	if (bStartLoad) return;     // (the first half: NewGameLoad.cpp)
	// all in: the finished look for 4 s, then the airport
	if (GLoadDoneAt >= 0.0) { if (LoadT - GLoadDoneAt >= 4.0) StopLoading(); return; }
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
	const float Want = 0.85f * (S + Terrain + T + Pictures) / 4.f;   // (the airport: the bar's first 85 %)
	const bool bWorld = (Shaders == 0 && Terrain >= 0.99f && Tex == 0 && Pictures >= 1.f) || LoadT > 120.f;
	// then the shader cache: its last 15 %
	const int32 Pso = (int32)FShaderPipelineCache::NumPrecompilesRemaining();
	if (bWorld && GPsoFrom < 0.0) { GPsoFrom = LoadT; GPsoMax = FMath::Max(1, Pso); FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast); }
	const float PsoK = GPsoFrom < 0.0 ? 0.f : 1.f - float(Pso) / float(GPsoMax);
	const float Goal = GBase + (1.f - GBase) * (bWorld ? 0.85f + 0.15f * PsoK : Want);
	LoadFrac = FMath::Max(LoadFrac, FMath::Min(Goal, LoadFrac + Dt * 0.6f));   // never backwards, smooth
	// and the walkers' navmesh (where the walls and floors are): ready-made, in at once; only a changed terminal has to make
	// it - then a minute at most here, and the rest in the background (the walkers keep their straight ways until then)
	const bool bNav = PaxNav::Ready(GetWorld()) || PaxNav::Failed(GetWorld()) || (!PaxNav::Started(GetWorld()) && LoadT > 10.f) || LoadT > 60.f;
	LoadStep = bWorld ? (Pso > 0 ? FString::Printf(TEXT("Loading shader cache… %d %%"), FMath::RoundToInt(100.f * PsoK))
		: !bNav ? FString::Printf(TEXT("Mapping the walkways… (%d to go)"), PaxNav::JobsLeft(GetWorld())) : FString(TEXT("Ready")))
		: Shaders > 0 ? FString::Printf(TEXT("Compiling shaders… (%d to go)"), Shaders)
		: Terrain < 0.99f ? TEXT("Loading the terrain round the airport…")
		: Tex > 0 ? TEXT("Loading the textures…")
		: Pictures < 1.f ? TEXT("Loading the satellite pictures…") : TEXT("Ready");
	// done: all of it in (the shader cache at most 90 s - beyond that it finishes in the background)
	const bool bDone = bWorld && bNav && (Pso == 0 || LoadT - GPsoFrom > 90.0) && LoadFrac >= GBase + (1.f - GBase) * (0.85f + 0.15f * PsoK) - 0.001f;
	if (bDone || LoadT > 210.f)
	{
		if (!bDone) ADP_WARN(TEXT("Loading: 210 s - going in as it is (shaders %d, terrain %.0f %%, textures %d, shader cache %d left, walking map %s)"), Shaders, Terrain * 100.f, Tex, Pso, bNav ? TEXT("in") : TEXT("not in"));
		LoadFrac = 1.f; GLoadDoneAt = LoadT;
		ADP_LOG(TEXT("Loading: done after %.1f s%s (shader cache %d of %d built, %d left for the background)"), LoadT, GBase > 0.f ? *FString::Printf(TEXT(" + %.1f s for the world and your career"), StartLoadT) : TEXT(""), GPsoMax - Pso, GPsoMax, Pso);
	}
}
