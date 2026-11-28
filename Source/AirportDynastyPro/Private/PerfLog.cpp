// The performance log (owner, 2026-11-16: "log super bad frames and the reason why - not a crash log"): every frame
// much slower than the ones around it goes into Saved/Logs/ADP-Perf.log with what took the time (the game's own work,
// the render thread, the graphics card, a clean-up of unused memory, files still loading) and where the camera was and
// which way it looked, so a stutter can be traced to a place and a cause. Every 30 s in the airport: a short summary
// (average, the slowest 1 % and the worst frame). The run before is kept as ADP-Perf-prev.log.
#include "PerfLog.h"
#include "GraphicsMemory.h"
#include "ADPFrameGen.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
#include "ADPLog.h"
#include "TerminalPlan.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "RHI.h"
#include "RenderCore.h"
#include "Misc/FileHelper.h"
#include "Algo/Accumulate.h"
#include "UObject/Package.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "HAL/Runnable.h"
#include "Async/Async.h"
#include <atomic>
#include "HAL/RunnableThread.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/ThreadManager.h"

namespace ADPPerfLogInternal
{
	uint64 PerfFgShown = 0, PerfFgMade = 0, PerfSecShown = 0, PerfSecMade = 0; double PerfSecStart = 0.0;   // (frame generation counts, ScreenLine)
	FArchive* PerfFile = nullptr;
	FString PerfLastReport;
	FTSTicker::FDelegateHandle PerfTick;
	double PerfGCStart = 0.0, PerfGCMs = 0.0;          // the last clean-up's length, and whether it fell in this frame
	int32 PerfGCCount = 0, PerfGCSeen = 0;
	double PerfAvg = 1.0 / 60.0;                    // a running average of the frame time (s)
	TArray<float> PerfWindow;                       // this summary's frames (ms)
	double PerfWindowStart = 0.0, PerfLastLine = 0.0;
	int32 PerfLinesThisSecond = 0, PerfBad = 0;
	TArray<FString> PerfSyncLoads;                  // files loaded on the spot (the game waiting for them) since the last frame
	// a measured stretch (the stress walk): its frames, its findings by part and cause
	struct FPerfFinding { FString Part, Why, Where, Files; float Worst = 0.f; int32 N = 0; };
	FString PerfAudit;
	TArray<float> PerfLightReach, PerfLightDraw;
	// the watchdog (-StallWatch): a thread of its own that sees when the game thread hasn't moved on for 60 ms and writes
	// where it is stuck at that moment (its call stack, by name where the build has its symbols)
	void PerfLine(const FString& L);
	void StallLine(const FString& T);   // (with the part running then: a Views step, a stress walk part)
	std::atomic<uint64> PerfBeat{0};
	uint32 PerfGameThreadId = 0;
	uint64 PerfExeBase = 0;
	double PerfStallAt = 0.06;   // (-StallWatch=<ms>)
	class FPerfWatch : public FRunnable
	{
	public:
		std::atomic<bool> bStop{false};
		uint32 Run() override
		{
			uint64 Last = 0; double Since = FPlatformTime::Seconds(); int32 Caught = 0;
			while (!bStop)
			{
				FPlatformProcess::Sleep(0.004f);
				const uint64 B = PerfBeat.load();
				if (B != Last) { Last = B; Since = FPlatformTime::Seconds(); Caught = 0; continue; }
				// (looked at again every 8 ms, then every 100, for as long as it lasts: what it is in changes)
				if (const double Held = FPlatformTime::Seconds() - Since; Caught < 12 && B > 100 && Held > PerfStallAt + (Caught < 6 ? Caught * 0.008 : 0.048 + (Caught - 6) * 0.1))
				{
					++Caught;
					uint64 Stack[48] = {};
					const int32 N = FPlatformStackWalk::CaptureThreadStackBackTrace(PerfGameThreadId, Stack, 48);
					FString T = FString::Printf(TEXT("STALL: the game thread hasn't moved for %.0f ms; it is in:"), Held * 1000.0);
					for (int32 i = 0; i < N && i < 40; ++i)
					{
						ANSICHAR Name[512] = {}; FPlatformStackWalk::ProgramCounterToHumanReadableString(i, Stack[i], Name, sizeof(Name));
						// (+offset: the address inside the game's program, for Tools/stall_names.sh where a build carries no names)
						const uint64 Off = Stack[i] - PerfExeBase;
						T += TEXT("\n    ") + FString(ANSI_TO_TCHAR(Name)) + (Off < 0x20000000ull ? FString::Printf(TEXT("  +0x%llx"), Off) : FString());
					}
					AsyncTask(ENamedThreads::GameThread, [T]() { StallLine(T); });
				}
			}
			return 0;
		}
	};
	FPerfWatch* PerfWatch = nullptr; FRunnableThread* PerfWatchThread = nullptr;
	bool bPerfSection = false; FString PerfSectionName, PerfCurPart; TArray<float> PerfSectionFrames; TArray<FPerfFinding> PerfFindings; TArray<FString> PerfNotes;
	void StallLine(const FString& T) { PerfLine(PerfCurPart.IsEmpty() ? T : T.Replace(TEXT("STALL:"), *(TEXT("STALL (at ") + PerfCurPart + TEXT("):")))); }

	void PerfLine(const FString& Ln)
	{
		const FString Out = FDateTime::Now().ToString(TEXT("%H:%M:%S.%s ")) + Ln + LINE_TERMINATOR;
		ADPLog::Later([Out]()   // (written by the log's own thread: its flush cost the game up to 30 ms)
		{
		if (!PerfFile)
		{
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Logs"), Now = Dir / TEXT("ADP-Perf.log");
			IFileManager& FM = IFileManager::Get(); FM.MakeDirectory(*Dir, true);
			if (FM.FileExists(*Now)) FM.Move(*(Dir / TEXT("ADP-Perf-prev.log")), *Now, true, true, false, true);
			PerfFile = FM.CreateFileWriter(*Now, FILEWRITE_AllowRead);
			if (!PerfFile) return;
		}
		const FTCHARToUTF8 U(*Out);
		PerfFile->Serialize(const_cast<ANSICHAR*>(U.Get()), U.Length());
		PerfFile->Flush();
		});
	}

	UWorld* PerfGameWorld()
	{
		if (!GEngine) return nullptr;
		for (const FWorldContext& C : GEngine->GetWorldContexts())
			if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.World()) return C.World();
		return nullptr;
	}

	bool PerfOnTick(float Dt)
	{
		PerfBeat.fetch_add(1);
		const double Now = FPlatformTime::Seconds();
		const float Ms = Dt * 1000.f;
		if (Dt <= 0.f || Dt > 10.f) return true;
		PerfWindow.Add(Ms);
		// (a frame's length comes in the tick after it: a clean-up seen now or one tick ago)
		const bool bGCNow = PerfGCCount != PerfGCSeen; PerfGCSeen = PerfGCCount;
		static bool bGCPrev = false; const bool bGC = bGCNow || bGCPrev; bGCPrev = bGCNow;
		if (bGCNow && PerfGCMs > 3.0) PerfLine(FString::Printf(TEXT("clean-up of unused memory: %.1f ms, %d objects in the game"), PerfGCMs, GUObjectArray.GetObjectArrayNumMinusAvailable()));
		const TArray<FString> Loaded = MoveTemp(PerfSyncLoads); PerfSyncLoads.Reset();
		if (bPerfSection) PerfSectionFrames.Add(Ms);
		// a bad frame: over 50 ms and at least 2.5 times the frames around it
		if (Ms > 50.f && Dt > PerfAvg * 2.5)
		{
			++PerfBad;
			if (Now - PerfLastLine >= 1.0) { PerfLastLine = Now; PerfLinesThisSecond = 0; }
			if (PerfLinesThisSecond++ < 10)
			{
				const float GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
				const float RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
				const float GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
				const int32 Loading = GetNumAsyncPackages();
				FString Why;
				if (Loaded.Num()) Why = TEXT("files loaded on the spot while you play (the game waited for them; they belong behind the loading screen)");
				else if (bGC && PerfGCMs > Ms * 0.3) Why = FString::Printf(TEXT("a clean-up of unused memory (garbage collection, %.0f ms)"), PerfGCMs);
				else if (Loading > 0 && GameMs >= RenderMs && GameMs >= GpuMs) Why = FString::Printf(TEXT("files loading while you play (%d still loading)"), Loading);
				else if (GpuMs >= GameMs && GpuMs >= RenderMs) Why = TEXT("the graphics card (too much to draw in this view)");
				else if (RenderMs >= GameMs) Why = TEXT("the render thread (most likely a first-time shader or pipeline build for something just coming into view)");
				else Why = TEXT("the game's own work this frame");
				FString Where = TEXT("no camera");
				if (UWorld* W = PerfGameWorld())
					if (APlayerController* PC = W->GetFirstPlayerController())
						if (PC->PlayerCameraManager)
						{
							const FVector L = PC->PlayerCameraManager->GetCameraLocation(); const FRotator R = PC->PlayerCameraManager->GetCameraRotation();
							Where = FString::Printf(TEXT("camera at plan x %.1f, height %.1f, z %.1f m, looking %.0f deg (pitch %.0f)"), L.X / 100.f, L.Z / 100.f, -L.Y / 100.f, FRotator::NormalizeAxis(R.Yaw), R.Pitch);
						}
				PerfLine(FString::Printf(TEXT("BAD FRAME %.0f ms (usual %.1f ms): game %.1f, render %.1f, GPU %.1f ms%s%s - most likely %s; %s"),
					Ms, PerfAvg * 1000.0, GameMs, RenderMs, GpuMs, bGC ? *FString::Printf(TEXT(", clean-up %.0f ms"), PerfGCMs) : TEXT(""),
					Loading ? *FString::Printf(TEXT(", %d files loading"), Loading) : TEXT(""), *Why, *Where));
				const FString Files = FString::Join(Loaded, TEXT(", "));
				if (Loaded.Num()) PerfLine(TEXT("    loaded on the spot: ") + Files.Left(600));
				if (bPerfSection)
				{
					FPerfFinding* F = PerfFindings.FindByPredicate([&](const FPerfFinding& X) { return X.Part == PerfCurPart && X.Why == Why; });
					if (!F) { F = &PerfFindings.AddDefaulted_GetRef(); F->Part = PerfCurPart; F->Why = Why; F->Where = Where; }
					++F->N; if (Ms > F->Worst) { F->Worst = Ms; F->Where = Where; }
					if (Loaded.Num() && F->Files.Len() < 400) F->Files += (F->Files.IsEmpty() ? TEXT("") : TEXT(", ")) + Files.Left(400);
				}
			}
		}
		else PerfAvg = PerfAvg * 0.95 + Dt * 0.05;      // (a bad frame doesn't move the "usual")
		// (to-do 149: every frame's game / render / graphics-card times summed, so each 30 s line says which one holds the frame)
		static double PerfSumGame = 0.0, PerfSumRender = 0.0, PerfSumGpu = 0.0;
		PerfSumGame += FPlatformTime::ToMilliseconds(GGameThreadTime); PerfSumRender += FPlatformTime::ToMilliseconds(GRenderThreadTime); PerfSumGpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		if (PerfWindowStart == 0.0) PerfWindowStart = Now;
		if (Now - PerfWindowStart >= 30.0 && PerfWindow.Num() > 10)
		{
			TArray<float> S = PerfWindow; S.Sort();
			double Sum = 0; for (float F : S) Sum += F;
			const float Low1 = S[FMath::Clamp(int32(S.Num() * 0.99f), 0, S.Num() - 1)];
			PerfLine(FString::Printf(TEXT("30 s: %.0f fps on average (%.1f ms), slowest 1%% %.1f ms (%.0f fps), worst %.0f ms, %d bad frames%s"),
				1000.0 * S.Num() / Sum, Sum / S.Num(), Low1, 1000.f / Low1, S.Last(), PerfBad, *ADPPerfLog::ScreenLine(Now - PerfWindowStart, PerfFgShown, PerfFgMade)));
			{ const double N = FMath::Max(1, S.Num()); PerfLine(FString::Printf(TEXT("      each frame on average: game %.1f ms, render %.1f ms, graphics card %.1f ms; %s"), PerfSumGame / N, PerfSumRender / N, PerfSumGpu / N, *ADPGraphicsMemory::Line())); }
			PerfSumGame = PerfSumRender = PerfSumGpu = 0.0;
			PerfWindow.Reset(); PerfWindowStart = Now; PerfBad = 0;
		}
		return true;
	}
}

void ADPPerfLog::Start()
{
	using namespace ADPPerfLogInternal;
	if (PerfTick.IsValid()) return;
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddLambda([]() { PerfGCStart = FPlatformTime::Seconds(); });
	FCoreUObjectDelegates::GetPostGarbageCollect().AddLambda([]() { PerfGCMs = (FPlatformTime::Seconds() - PerfGCStart) * 1000.0; ++PerfGCCount; });
#if WITH_EDITOR
	// (editor builds: which files the game stopped to load, by name; the packaged game has no such hook)
	FCoreUObjectDelegates::OnEndLoadPackage.AddLambda([](const FEndLoadPackageContext& C)
	{
		if (!C.bSynchronous || C.RecursiveDepth > 0 || !IsInGameThread()) return;
		for (UPackage* P : C.LoadedPackages) if (P && PerfSyncLoads.Num() < 40) PerfSyncLoads.Add(P->GetName());
	});
#endif
	PerfTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&PerfOnTick));
	if (FString(FCommandLine::Get()).Contains(TEXT("-StallWatch"))) ADPPerfLog::StartStallWatch();
	PerfLine(TEXT("Performance log: frames over 50 ms and 2.5 times the usual, with their likely cause and the camera's place"));
}

void ADPPerfLog::Stop()
{
	if (ADPPerfLogInternal::PerfWatch) { ADPPerfLogInternal::PerfWatch->bStop = true; if (ADPPerfLogInternal::PerfWatchThread) { ADPPerfLogInternal::PerfWatchThread->WaitForCompletion(); delete ADPPerfLogInternal::PerfWatchThread; } delete ADPPerfLogInternal::PerfWatch; ADPPerfLogInternal::PerfWatch = nullptr; ADPPerfLogInternal::PerfWatchThread = nullptr; }
	using namespace ADPPerfLogInternal;
	if (PerfTick.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(PerfTick); PerfTick.Reset(); }
	ADPLog::Drain();
	if (PerfFile) { PerfFile->Close(); delete PerfFile; PerfFile = nullptr; }
}

FString ADPPerfLog::ScreenLine(double Seconds, uint64& Shown0, uint64& Made0)
{
	// what the screen showed (owner, 2026-11-18): the pictures our frame generation really put on the screen in this time,
	// and how many of them were in-between pictures it made - the fps above are the frames the game drew itself
	uint64 Shown = 0, Made = 0; ADPFrameGen::Counts(Shown, Made);
	const uint64 dS = Shown - Shown0, dM = Made - Made0; Shown0 = Shown; Made0 = Made;
	if (!ADPFrameGen::Installed()) return TEXT(" | on screen: the same (our frame generation not in place)");
	if (Seconds <= 0.0 || dS == 0) return TEXT(" | on screen: no pictures counted");
	return FString::Printf(TEXT(" | on screen %.0f fps (frame generation %s: %.0f in-between pictures a second, %.0f%% of what was shown)"),
		dS / Seconds, ADPFrameGen::Enabled() ? TEXT("on") : TEXT("off"), dM / Seconds, 100.0 * dM / dS);
}

void ADPPerfLog::BeginSection(const FString& Name)
{
	using namespace ADPPerfLogInternal;
	ADPPerfLog::StartStallWatch();   // (on for every measured stretch: the stress walk, owner 2026-11-16)
	{ uint64 a = 0, b = 0; ADPFrameGen::Counts(a, b); PerfSecShown = a; PerfSecMade = b; PerfSecStart = FPlatformTime::Seconds(); }
	bPerfSection = true; PerfSectionName = Name; PerfCurPart.Reset(); PerfAudit.Reset(); PerfSectionFrames.Reset(); PerfFindings.Reset(); PerfNotes.Reset();
	PerfLine(TEXT("==== ") + Name + TEXT(": start"));
}

void ADPPerfLog::Mark(const FString& What)
{
	using namespace ADPPerfLogInternal;
	if (What.StartsWith(TEXT("("))) PerfNotes.Add(PerfCurPart + TEXT(" ") + What); else PerfCurPart = What;
	PerfLine(TEXT("---- ") + What);
}

FString ADPPerfLog::EndSection()
{
	using namespace ADPPerfLogInternal;
	bPerfSection = false;
	FString Sum = TEXT("no frames");
	if (PerfSectionFrames.Num())
	{
		TArray<float> S = PerfSectionFrames; S.Sort(); double T = 0; for (float F : S) T += F;
		const float Low1 = S[FMath::Clamp(int32(S.Num() * 0.99f), 0, S.Num() - 1)];
		Sum = FString::Printf(TEXT("%.0f fps on average, slowest 1%% %.0f fps, worst frame %.0f ms, %d bad frames"), 1000.0 * S.Num() / T, 1000.f / Low1, S.Last(), PerfFindings.Num() ? Algo::Accumulate(PerfFindings, 0, [](int32 A, const FPerfFinding& F) { return A + F.N; }) : 0);
	}
	Sum = TEXT("drawn by the game: ") + Sum + ADPPerfLog::ScreenLine(FPlatformTime::Seconds() - PerfSecStart, PerfSecShown, PerfSecMade).Replace(TEXT(" | "), TEXT("\n"));
	FString R = FString::Printf(TEXT("Airport Dynasty Pro - %s report (%s)\n%s\n"), *PerfSectionName, *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")), *Sum);
	PerfFindings.Sort([](const FPerfFinding& A, const FPerfFinding& B) { return A.Worst * A.N > B.Worst * B.N; });
	R += PerfFindings.Num() ? TEXT("\nWhat it found (worst first):\n") : TEXT("\nNo bad frames.\n");
	for (const FPerfFinding& F : PerfFindings)
	{
		R += FString::Printf(TEXT("- %s: %d bad frame%s, worst %.0f ms - %s\n    %s\n"), *F.Part, F.N, F.N == 1 ? TEXT("") : TEXT("s"), F.Worst, *F.Why, *F.Where);
		if (!F.Files.IsEmpty()) R += TEXT("    files loaded on the spot: ") + F.Files + TEXT("\n");
	}
	for (const FString& N : PerfNotes) R += TEXT("- the walk: ") + N + TEXT("\n");
	R += TEXT("\nFull detail: Saved/Logs/ADP-Perf.log; this report: Saved/Logs/StressWalk-Report.txt\n");
	PerfLine(TEXT("==== ") + PerfSectionName + TEXT(": end - ") + Sum);
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Logs");
	FFileHelper::SaveStringToFile(R, *(Dir / TEXT("StressWalk-Report.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FFileHelper::SaveStringToFile(R, *(Dir / FString::Printf(TEXT("StressWalk-Report-%s.txt"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M")))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	PerfLastReport = R + PerfAudit;
	FFileHelper::SaveStringToFile(PerfLastReport, *(Dir / TEXT("StressWalk-Report.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	return Sum;
}

FString ADPPerfLog::Report() { return ADPPerfLogInternal::PerfLastReport; }

FString ADPPerfLog::SceneAudit(UWorld* W)
{
	using namespace ADPPerfLogInternal;
	if (!W) return FString();
	// what the world holds, measured: every drawn mesh, Nanite or not, its triangles, copies, materials and shadows
	struct FRow { FString Mesh, Owner; int32 Copies = 0, Tris = 0, Sections = 0; float Size = 0.f; bool bNanite = false, bShadow = false, bTranslucent = false; };
	TMap<FString, FRow> Rows; int32 Lights = 0, ShadowLights = 0, MovableShadowLights = 0, Skel = 0; int64 SkelTris = 0; TArray<FString> LightList;
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		UPrimitiveComponent* C = *It;
		if (!C || C->GetWorld() != W || !C->IsRegistered() || !C->IsVisible() || (C->GetOwner() && C->GetOwner()->IsHidden())) continue;
		if (UStaticMeshComponent* S = Cast<UStaticMeshComponent>(C))
		{
			UStaticMesh* M = S->GetStaticMesh(); if (!M || !M->GetRenderData() || !M->GetRenderData()->LODResources.Num()) continue;
			const UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(S);
			const int32 N = I ? I->GetInstanceCount() : 1; if (N <= 0) continue;
			// (one row per mesh AND the kind of thing placing it, see-through or not - 6671 engine cubes all showed as "Actor")
			bool bSee = false; for (int32 k = 0; k < S->GetNumMaterials(); ++k) if (UMaterialInterface* Mi = S->GetMaterial(k)) if (IsTranslucentBlendMode(Mi->GetBlendMode())) bSee = true;
			const FString Who = S->GetOwner() ? (S->GetOwner()->GetClass() == AActor::StaticClass() ? S->GetOwner()->GetName() : S->GetOwner()->GetClass()->GetName()) : FString(TEXT("?"));
			FRow& R = Rows.FindOrAdd(M->GetPathName() + TEXT("|") + Who + (bSee ? TEXT("|t") : TEXT("")));
			R.Mesh = M->GetPathName(); if (R.Owner.IsEmpty()) R.Owner = Who + (S->GetMaterial(0) ? TEXT(" [") + S->GetMaterial(0)->GetName() + TEXT("]") : FString());
			R.Copies += N; R.Tris = M->GetRenderData()->LODResources[0].GetNumTriangles(); R.Sections = M->GetRenderData()->LODResources[0].Sections.Num();
			R.Size = FMath::Max(R.Size, (float)S->Bounds.SphereRadius * 2.f / 100.f);
			R.bNanite = M->HasValidNaniteData() && !S->bDisallowNanite; R.bShadow |= S->CastShadow;
			for (int32 k = 0; k < S->GetNumMaterials(); ++k) if (UMaterialInterface* Mi = S->GetMaterial(k)) if (IsTranslucentBlendMode(Mi->GetBlendMode())) R.bTranslucent = true;
		}
		else if (USkeletalMeshComponent* K = Cast<USkeletalMeshComponent>(C))
		{
			++Skel; if (K->GetSkeletalMeshAsset() && K->GetSkeletalMeshAsset()->GetResourceForRendering() && K->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData.Num()) SkelTris += K->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0].GetTotalFaces();
		}
	}
	for (TObjectIterator<ULightComponent> It; It; ++It)
	{
		ULightComponent* L = *It;
		if (!L || L->GetWorld() != W || !L->IsRegistered() || !L->IsVisible() || L->Intensity <= 0.f) continue;
		{
			// how far each lamp reaches, and from how far away it is drawn at all (0: no limit of its own)
			const ULocalLightComponent* LL = Cast<ULocalLightComponent>(L);
			if (LL) { PerfLightReach.Add(LL->AttenuationRadius / 100.f); PerfLightDraw.Add(L->MaxDrawDistance / 100.f); }
		}
		++Lights; if (L->CastShadows) { ++ShadowLights; if (L->Mobility == EComponentMobility::Movable) { ++MovableShadowLights; if (LightList.Num() < 12) LightList.Add((L->GetOwner() ? L->GetOwner()->GetName() : FString(TEXT("?"))) + TEXT("/") + L->GetName()); } }
	}
	auto Spread = [](TArray<float> V) -> FString
	{
		if (!V.Num()) return FString(TEXT("none"));
		V.Sort(); auto P = [&](float q) { return V[FMath::Clamp(int32(V.Num() * q), 0, V.Num() - 1)]; };
		return FString::Printf(TEXT("smallest %.0f m, half under %.0f m, 90%% under %.0f m, largest %.0f m"), V[0], P(0.5f), P(0.9f), V.Last());
	};
	int32 NoLimit = 0; for (float D : PerfLightDraw) if (D <= 0.f) ++NoLimit;
	const FString LightText = FString::Printf(TEXT("- the lamps (%d local lights): their light reaches %s; %d of them have no draw-distance limit of their own (drawn as far as their light reaches on screen)\n"), PerfLightReach.Num(), *Spread(PerfLightReach), NoLimit);
	PerfLightReach.Reset(); PerfLightDraw.Reset();
	TArray<FRow> A; Rows.GenerateValueArray(A);
	int64 NonNaniteTris = 0, NaniteTris = 0; int32 NonNaniteDraws = 0, NonNaniteMeshes = 0;
	for (const FRow& R : A) { if (R.bNanite) NaniteTris += (int64)R.Tris * R.Copies; else { NonNaniteTris += (int64)R.Tris * R.Copies; NonNaniteDraws += R.Sections; ++NonNaniteMeshes; } }
	FString T = FString::Printf(TEXT("\nWhat the world holds (measured):\n- %d different meshes drawn: Nanite %lld triangles in all; not Nanite: %d meshes, %lld triangles, at least %d draw calls a view (one per material section, before shadows)\n- %d skinned meshes (people, pets, animals), %lld triangles at full detail\n- %d lights on, %d casting shadows, %d of them movable (each a shadow pass every frame)\n"),
		A.Num(), NaniteTris, NonNaniteMeshes, NonNaniteTris, NonNaniteDraws, Skel, SkelTris, Lights, ShadowLights, MovableShadowLights);
	A.Sort([](const FRow& X, const FRow& Y) { return (int64)X.Tris * X.Copies > (int64)Y.Tris * Y.Copies; });
	T += TEXT("- not Nanite, most triangles first (mesh file - placed by - copies x triangles, materials, size):\n");
	int32 n = 0; for (const FRow& R : A) if (!R.bNanite && n++ < 15) T += FString::Printf(TEXT("    %s - %s - %d x %d, %d materials, %.0f m%s%s\n"), *R.Mesh, *R.Owner, R.Copies, R.Tris, R.Sections, R.Size, R.bShadow ? TEXT(", casts shadows") : TEXT(""), R.bTranslucent ? TEXT(", see-through") : TEXT(""));
	A.Sort([](const FRow& X, const FRow& Y) { return X.Size > Y.Size; });
	T += TEXT("- not Nanite and casting shadows, largest first (these fill the shadow map's \"non-Nanite\" queue):\n");
	n = 0; for (const FRow& R : A) if (!R.bNanite && R.bShadow && n++ < 12) T += FString::Printf(TEXT("    %s - %s - %.0f m across, %d copies\n"), *R.Mesh, *R.Owner, R.Size, R.Copies);
	T += TEXT("- Nanite meshes with see-through materials (Nanite can't draw those parts):\n");
	n = 0; for (const FRow& R : A) if (R.bNanite && R.bTranslucent && n++ < 12) T += FString::Printf(TEXT("    %s - %s - %d copies\n"), *R.Mesh, *R.Owner, R.Copies);
	T += LightText;
	if (LightList.Num()) T += TEXT("- movable shadow-casting lights (first 12): ") + FString::Join(LightList, TEXT(", ")) + TEXT("\n");
	PerfLine(T);
	return T;
}

void ADPPerfLog::SetAudit(const FString& A) { ADPPerfLogInternal::PerfAudit = A; }

void ADPPerfLog::StartStallWatch()
{
	using namespace ADPPerfLogInternal;
	if (PerfWatch) return;
	PerfGameThreadId = GGameThreadId; FPlatformStackWalk::InitStackWalking();
#if PLATFORM_WINDOWS
	PerfExeBase = (uint64)::GetModuleHandleW(nullptr);
#endif
	PerfWatch = new FPerfWatch(); PerfWatchThread = FRunnableThread::Create(PerfWatch, TEXT("ADPStallWatch"));
	{ float Ms = 0.f; if (FParse::Value(FCommandLine::Get(), TEXT("StallWatch="), Ms) && Ms >= 10.f) PerfStallAt = Ms / 1000.0; }
	PerfLine(FString::Printf(TEXT("Stall watch on: where the game thread is stuck whenever it stops for %.0f ms (Tools/stall_names.sh names them)"), PerfStallAt * 1000.0));
}

void ADPPerfLog::SlowWork(const TCHAR* What, double Ms, float BudgetMs)
{
	using namespace ADPPerfLogInternal;
	if (!IsInGameThread()) return;
	static TMap<const TCHAR*, int32> Count;
	int32& N = Count.FindOrAdd(What); ++N;
	if (N == 1 || N % 20 == 0)
		PerfLine(FString::Printf(TEXT("SLOW WORK: %s took %.1f ms (its budget %.0f ms; %d times over so far)"), What, Ms, BudgetMs, N));
	if (bPerfSection && Ms > 30.0) PerfNotes.Add(FString::Printf(TEXT("slow work: %s %.0f ms"), What, Ms));
}
