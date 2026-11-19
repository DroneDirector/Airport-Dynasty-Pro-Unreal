// The loading screen into airport mode (owner, 2026-11-11): from the menus (the Overview's Terminal or Airport site, or
// the quick shortcut) into the 3D airport, a screen that really waits until everything is in: the shaders compiled,
// the terrain and its pictures round the airport, the textures streamed. A progress bar (its percentage in the middle)
// and a line saying what it is doing; Esc on it goes straight back to the menus. Test runs skip it.
#include "UI/ADPHUD.h"
#include "UI/SNeonLoading.h"
#include "ADPLog.h"
#include "People.h"
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

TSharedRef<SWidget> AADPHUD::MakeLoading()
{
	return SNew(SNeonLoading)
		.Kicker(TEXT("Airport Dynasty Pro  ·  into the airport"))
		.Title(TEXT("Loading the airport"))
		.Step_Lambda([this]() { return LoadStep; })
		.Fraction_Lambda([this]() { return LoadFrac; })
		.Done_Lambda([]() { return GLoadDoneAt >= 0.0; })
		.Hint(TEXT("Esc: back to the menus"));
}

void AADPHUD::StartLoading()
{
	if (!LoadingBox.IsValid()) return;
	bLoading = true; LoadT = 0; LoadFrac = 0; LoadStep = TEXT("Getting the airport ready…"); GLoadDoneAt = -1.0; GPsoMax = 0; GPsoFrom = -1.0;
	ShadersMax = 1; TexMax = 1;
	LoadingBox->SetVisibility(EVisibility::Visible);
}

void AADPHUD::StopLoading()
{
	bLoading = false; GLoadDoneAt = -1.0;
	if (AADPPeople* Pp = AADPPeople::Get(GetWorld())) Pp->StartWanderers();   // (the passengers about the terminal, once it is in - owner, 2026-11-19)
	FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Background);   // (the rest, if any: quietly)
	if (LoadingBox.IsValid()) LoadingBox->SetVisibility(EVisibility::Collapsed);
}

void AADPHUD::TickLoading(float Dt)
{
	if (!bLoading) return;
	LoadT += Dt;
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
	const float Goal = bWorld ? 0.85f + 0.15f * PsoK : Want;
	LoadFrac = FMath::Max(LoadFrac, FMath::Min(Goal, LoadFrac + Dt * 0.6f));   // never backwards, smooth
	LoadStep = bWorld ? (Pso > 0 ? FString::Printf(TEXT("Loading shader cache… %d %%"), FMath::RoundToInt(100.f * PsoK)) : FString(TEXT("Ready")))
		: Shaders > 0 ? FString::Printf(TEXT("Compiling shaders… (%d to go)"), Shaders)
		: Terrain < 0.99f ? TEXT("Loading the terrain round the airport…")
		: Tex > 0 ? TEXT("Loading the textures…")
		: Pictures < 1.f ? TEXT("Loading the satellite pictures…") : TEXT("Ready");
	// done: all of it in (the shader cache at most 90 s - beyond that it finishes in the background)
	const bool bDone = bWorld && (Pso == 0 || LoadT - GPsoFrom > 90.0) && LoadFrac >= 0.85f + 0.15f * PsoK - 0.001f;
	if (bDone || LoadT > 210.f)
	{
		LoadFrac = 1.f; GLoadDoneAt = LoadT;
		ADP_LOG(TEXT("Loading: done after %.1f s (shader cache %d of %d built, %d left for the background)"), LoadT, GPsoMax - Pso, GPsoMax, Pso);
	}
}
