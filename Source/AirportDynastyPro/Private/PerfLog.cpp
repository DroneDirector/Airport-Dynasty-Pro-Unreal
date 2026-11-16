// The performance log (owner, 2026-11-16: "log super bad frames and the reason why - not a crash log"): every frame
// much slower than the ones around it goes into Saved/Logs/ADP-Perf.log with what took the time (the game's own work,
// the render thread, the graphics card, a clean-up of unused memory, files still loading) and where the camera was and
// which way it looked, so a stutter can be traced to a place and a cause. Every 30 s in the airport: a short summary
// (average, the slowest 1 % and the worst frame). The run before is kept as ADP-Perf-prev.log.
#include "PerfLog.h"
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
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"
#include "Rendering/SkeletalMeshRenderData.h"

namespace
{
	FArchive* File = nullptr;
	FString LastReport;
	FTSTicker::FDelegateHandle Tick;
	double GCStart = 0.0, GCMs = 0.0;          // the last clean-up's length, and whether it fell in this frame
	int32 GCCount = 0, GCSeen = 0;
	double Avg = 1.0 / 60.0;                    // a running average of the frame time (s)
	TArray<float> Window;                       // this summary's frames (ms)
	double WindowStart = 0.0, LastLine = 0.0;
	int32 LinesThisSecond = 0, Bad = 0;
	TArray<FString> SyncLoads;                  // files loaded on the spot (the game waiting for them) since the last frame
	// a measured stretch (the stress walk): its frames, its findings by part and cause
	struct FFinding { FString Part, Why, Where, Files; float Worst = 0.f; int32 N = 0; };
	FString Audit;
	bool bSection = false; FString SectionName, CurPart; TArray<float> SectionFrames; TArray<FFinding> Findings; TArray<FString> Notes;

	void Line(const FString& L)
	{
		if (!File)
		{
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Logs"), Now = Dir / TEXT("ADP-Perf.log");
			IFileManager& FM = IFileManager::Get(); FM.MakeDirectory(*Dir, true);
			if (FM.FileExists(*Now)) FM.Move(*(Dir / TEXT("ADP-Perf-prev.log")), *Now, true, true, false, true);
			File = FM.CreateFileWriter(*Now, FILEWRITE_AllowRead);
			if (!File) return;
		}
		const FString Out = FDateTime::Now().ToString(TEXT("%H:%M:%S.%s ")) + L + LINE_TERMINATOR;
		const FTCHARToUTF8 U(*Out);
		File->Serialize(const_cast<ANSICHAR*>(U.Get()), U.Length());
		File->Flush();
	}

	UWorld* GameWorld()
	{
		if (!GEngine) return nullptr;
		for (const FWorldContext& C : GEngine->GetWorldContexts())
			if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.World()) return C.World();
		return nullptr;
	}

	bool OnTick(float Dt)
	{
		const double Now = FPlatformTime::Seconds();
		const float Ms = Dt * 1000.f;
		if (Dt <= 0.f || Dt > 10.f) return true;
		Window.Add(Ms);
		const bool bGC = GCCount != GCSeen; GCSeen = GCCount;
		const TArray<FString> Loaded = MoveTemp(SyncLoads); SyncLoads.Reset();
		if (bSection) SectionFrames.Add(Ms);
		// a bad frame: over 50 ms and at least 2.5 times the frames around it
		if (Ms > 50.f && Dt > Avg * 2.5)
		{
			++Bad;
			if (Now - LastLine >= 1.0) { LastLine = Now; LinesThisSecond = 0; }
			if (LinesThisSecond++ < 10)
			{
				const float GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
				const float RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
				const float GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
				const int32 Loading = GetNumAsyncPackages();
				FString Why;
				if (Loaded.Num()) Why = TEXT("files loaded on the spot while you play (the game waited for them; they belong behind the loading screen)");
				else if (bGC && GCMs > Ms * 0.3) Why = FString::Printf(TEXT("a clean-up of unused memory (garbage collection, %.0f ms)"), GCMs);
				else if (Loading > 0 && GameMs >= RenderMs && GameMs >= GpuMs) Why = FString::Printf(TEXT("files loading while you play (%d still loading)"), Loading);
				else if (GpuMs >= GameMs && GpuMs >= RenderMs) Why = TEXT("the graphics card (too much to draw in this view)");
				else if (RenderMs >= GameMs) Why = TEXT("the render thread (most likely a first-time shader or pipeline build for something just coming into view)");
				else Why = TEXT("the game's own work this frame");
				FString Where = TEXT("no camera");
				if (UWorld* W = GameWorld())
					if (APlayerController* PC = W->GetFirstPlayerController())
						if (PC->PlayerCameraManager)
						{
							const FVector L = PC->PlayerCameraManager->GetCameraLocation(); const FRotator R = PC->PlayerCameraManager->GetCameraRotation();
							Where = FString::Printf(TEXT("camera at plan x %.1f, height %.1f, z %.1f m, looking %.0f deg (pitch %.0f)"), L.X / 100.f, L.Z / 100.f, -L.Y / 100.f, FRotator::NormalizeAxis(R.Yaw), R.Pitch);
						}
				Line(FString::Printf(TEXT("BAD FRAME %.0f ms (usual %.1f ms): game %.1f, render %.1f, GPU %.1f ms%s%s - most likely %s; %s"),
					Ms, Avg * 1000.0, GameMs, RenderMs, GpuMs, bGC ? *FString::Printf(TEXT(", clean-up %.0f ms"), GCMs) : TEXT(""),
					Loading ? *FString::Printf(TEXT(", %d files loading"), Loading) : TEXT(""), *Why, *Where));
				const FString Files = FString::Join(Loaded, TEXT(", "));
				if (Loaded.Num()) Line(TEXT("    loaded on the spot: ") + Files.Left(600));
				if (bSection)
				{
					FFinding* F = Findings.FindByPredicate([&](const FFinding& X) { return X.Part == CurPart && X.Why == Why; });
					if (!F) { F = &Findings.AddDefaulted_GetRef(); F->Part = CurPart; F->Why = Why; F->Where = Where; }
					++F->N; if (Ms > F->Worst) { F->Worst = Ms; F->Where = Where; }
					if (Loaded.Num() && F->Files.Len() < 400) F->Files += (F->Files.IsEmpty() ? TEXT("") : TEXT(", ")) + Files.Left(400);
				}
			}
		}
		else Avg = Avg * 0.95 + Dt * 0.05;      // (a bad frame doesn't move the "usual")
		if (WindowStart == 0.0) WindowStart = Now;
		if (Now - WindowStart >= 30.0 && Window.Num() > 10)
		{
			TArray<float> S = Window; S.Sort();
			double Sum = 0; for (float F : S) Sum += F;
			const float Low1 = S[FMath::Clamp(int32(S.Num() * 0.99f), 0, S.Num() - 1)];
			Line(FString::Printf(TEXT("30 s: %.0f fps on average (%.1f ms), slowest 1%% %.1f ms (%.0f fps), worst %.0f ms, %d bad frames"),
				1000.0 * S.Num() / Sum, Sum / S.Num(), Low1, 1000.f / Low1, S.Last(), Bad));
			Window.Reset(); WindowStart = Now; Bad = 0;
		}
		return true;
	}
}

void ADPPerfLog::Start()
{
	if (Tick.IsValid()) return;
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddLambda([]() { GCStart = FPlatformTime::Seconds(); });
	FCoreUObjectDelegates::GetPostGarbageCollect().AddLambda([]() { GCMs = (FPlatformTime::Seconds() - GCStart) * 1000.0; ++GCCount; });
#if WITH_EDITOR
	// (editor builds: which files the game stopped to load, by name; the packaged game has no such hook)
	FCoreUObjectDelegates::OnEndLoadPackage.AddLambda([](const FEndLoadPackageContext& C)
	{
		if (!C.bSynchronous || C.RecursiveDepth > 0 || !IsInGameThread()) return;
		for (UPackage* P : C.LoadedPackages) if (P && SyncLoads.Num() < 40) SyncLoads.Add(P->GetName());
	});
#endif
	Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&OnTick));
	Line(TEXT("Performance log: frames over 50 ms and 2.5 times the usual, with their likely cause and the camera's place"));
}

void ADPPerfLog::Stop()
{
	if (Tick.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(Tick); Tick.Reset(); }
	if (File) { File->Close(); delete File; File = nullptr; }
}

void ADPPerfLog::BeginSection(const FString& Name)
{
	bSection = true; SectionName = Name; CurPart.Reset(); Audit.Reset(); SectionFrames.Reset(); Findings.Reset(); Notes.Reset();
	Line(TEXT("==== ") + Name + TEXT(": start"));
}

void ADPPerfLog::Mark(const FString& What)
{
	if (What.StartsWith(TEXT("("))) Notes.Add(CurPart + TEXT(" ") + What); else CurPart = What;
	Line(TEXT("---- ") + What);
}

FString ADPPerfLog::EndSection()
{
	bSection = false;
	FString Sum = TEXT("no frames");
	if (SectionFrames.Num())
	{
		TArray<float> S = SectionFrames; S.Sort(); double T = 0; for (float F : S) T += F;
		const float Low1 = S[FMath::Clamp(int32(S.Num() * 0.99f), 0, S.Num() - 1)];
		Sum = FString::Printf(TEXT("%.0f fps on average, slowest 1%% %.0f fps, worst frame %.0f ms, %d bad frames"), 1000.0 * S.Num() / T, 1000.f / Low1, S.Last(), Findings.Num() ? Algo::Accumulate(Findings, 0, [](int32 A, const FFinding& F) { return A + F.N; }) : 0);
	}
	FString R = FString::Printf(TEXT("Airport Dynasty Pro - %s report (%s)\n%s\n"), *SectionName, *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")), *Sum);
	Findings.Sort([](const FFinding& A, const FFinding& B) { return A.Worst * A.N > B.Worst * B.N; });
	R += Findings.Num() ? TEXT("\nWhat it found (worst first):\n") : TEXT("\nNo bad frames.\n");
	for (const FFinding& F : Findings)
	{
		R += FString::Printf(TEXT("- %s: %d bad frame%s, worst %.0f ms - %s\n    %s\n"), *F.Part, F.N, F.N == 1 ? TEXT("") : TEXT("s"), F.Worst, *F.Why, *F.Where);
		if (!F.Files.IsEmpty()) R += TEXT("    files loaded on the spot: ") + F.Files + TEXT("\n");
	}
	for (const FString& N : Notes) R += TEXT("- the walk: ") + N + TEXT("\n");
	R += TEXT("\nFull detail: Saved/Logs/ADP-Perf.log; this report: Saved/Logs/StressWalk-Report.txt\n");
	Line(TEXT("==== ") + SectionName + TEXT(": end - ") + Sum);
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Logs");
	FFileHelper::SaveStringToFile(R, *(Dir / TEXT("StressWalk-Report.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FFileHelper::SaveStringToFile(R, *(Dir / FString::Printf(TEXT("StressWalk-Report-%s.txt"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M")))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	LastReport = R + Audit;
	FFileHelper::SaveStringToFile(LastReport, *(Dir / TEXT("StressWalk-Report.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	return Sum;
}

FString ADPPerfLog::Report() { return LastReport; }

FString ADPPerfLog::SceneAudit(UWorld* W)
{
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
			FRow& R = Rows.FindOrAdd(M->GetPathName());
			R.Mesh = M->GetPathName(); if (R.Owner.IsEmpty() && S->GetOwner()) R.Owner = S->GetOwner()->GetClass()->GetName();
			R.Copies += N; R.Tris = M->GetRenderData()->LODResources[0].GetNumTriangles(); R.Sections = M->GetRenderData()->LODResources[0].Sections.Num();
			R.Size = FMath::Max(R.Size, (float)S->Bounds.SphereRadius * 2.f / 100.f);
			R.bNanite = M->IsNaniteEnabled() && !S->bDisallowNanite; R.bShadow |= S->CastShadow;
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
		++Lights; if (L->CastShadows) { ++ShadowLights; if (L->Mobility == EComponentMobility::Movable) { ++MovableShadowLights; if (LightList.Num() < 12) LightList.Add((L->GetOwner() ? L->GetOwner()->GetName() : FString(TEXT("?"))) + TEXT("/") + L->GetName()); } }
	}
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
	if (LightList.Num()) T += TEXT("- movable shadow-casting lights (first 12): ") + FString::Join(LightList, TEXT(", ")) + TEXT("\n");
	Line(T);
	return T;
}

void ADPPerfLog::SetAudit(const FString& A) { Audit = A; }
