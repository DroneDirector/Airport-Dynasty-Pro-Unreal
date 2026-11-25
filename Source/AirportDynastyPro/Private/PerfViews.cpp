// The fixed-views performance run (to-do 269): a text file, one step a line, so a new run needs no compile:
//   view <x> <height> <z> <yaw> <pitch>   the camera there, held (plan metres and degrees as in ADP-Perf.log)
//   fly <x> <height> <z> <yaw> <pitch> <seconds>   the camera moved there in that time, measured while it moves
//   cmd <console command>
//   wait <seconds>
//   day <of the year, 1-365; the summer day if not given>, hour <0-24>, weather <0 clear .. 2 broken, 3 overcast .. 7 snow>
//   measure <seconds> <name>              frame, game, render and graphics-card times into ADP.log ("Views: ...")
//   gpu                                   every pass of one frame into the log (ProfileGPU; not in the packaged game)
//   census                                instances in the distance field scene, by mesh
//   shot <name>
//   hmi <jetway> [tab]  /  hmi close      the jetway's screen used (its camera page: hmi 0 cam)
//   eat <MB>                              so much graphics memory taken and used every frame, as another program would
//   quit
#include "PerfViews.h"
#include "ADPLog.h"
#include "Containers/Ticker.h"
#include "Misc/FileHelper.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "RHI.h"
#include "RenderCore.h"
#include "UnrealClient.h"
#include "ADPFrameGen.h"
#include "SunClock.h"
#include "GraphicsMemory.h"
#include "JetwayHmi.h"
#include "UI/ADPHUD.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectIterator.h"
#include "UObject/StrongObjectPtr.h"

namespace ADPPerfViewsInternal
{
	TArray<FString> Steps; int32 At = 0;
	bool bHeld = false; FVector Want = FVector::ZeroVector, From = FVector::ZeroVector; FRotator Rot = FRotator::ZeroRotator, RotFrom = FRotator::ZeroRotator;
	double Until = 0.0, FlyStart = 0.0, FlyLen = 0.0; FString Name; TArray<float> Fr, Gp, Rn, Gm; uint64 Shown0 = 0, Made0 = 0; double MStart = 0.0;

	TArray<TStrongObjectPtr<UTextureRenderTarget2D>> Eaten;

	UWorld* ViewsWorld()
	{
		if (GEngine) for (const FWorldContext& C : GEngine->GetWorldContexts()) if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.World()) return C.World();
		return nullptr;
	}
	void Place(APlayerController* PC)
	{
		APawn* P = PC->GetPawn(); if (!P || !PC->PlayerCameraManager) return;
		if (ACharacter* Ch = Cast<ACharacter>(P)) { Ch->SetActorEnableCollision(false); if (Ch->GetCharacterMovement()) { Ch->GetCharacterMovement()->StopMovementImmediately(); Ch->GetCharacterMovement()->DisableMovement(); } }
		FVector W = Want; FRotator R = Rot;
		if (FlyLen > 0.0) { const double T = FMath::Clamp((FPlatformTime::Seconds() - FlyStart) / FlyLen, 0.0, 1.0); W = FMath::Lerp(From, Want, T); R = FMath::Lerp(RotFrom, Rot, (float)T); }
		const FVector D = W - PC->PlayerCameraManager->GetCameraLocation();
		P->SetActorLocation(P->GetActorLocation() + D, false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(R);
	}
	FString Stat(TArray<float> V)
	{
		if (!V.Num()) return TEXT("-");
		V.Sort(); double S = 0; for (float f : V) S += f;
		return FString::Printf(TEXT("%.1f (1%% %.1f, worst %.1f)"), S / V.Num(), V[FMath::Min(V.Num() - 1, int32(V.Num() * 0.99f))], V.Last());
	}
	FString Mem()
	{
		FTextureMemoryStats M; RHIGetTextureMemoryStats(M);
		return FString::Printf(TEXT("textures %lld of %lld MB, %s"), (int64)(M.StreamingMemorySize / 1048576), (int64)(M.TexturePoolSize / 1048576), *ADPGraphicsMemory::Line());
	}
	void Report(const TCHAR* What)
	{
		int32 People = 0;
		if (UWorld* W = ViewsWorld()) for (TActorIterator<ACharacter> It(W); It; ++It) if (!It->IsHidden()) ++People;
		uint64 S = 0, M = 0; ADPFrameGen::Counts(S, M); const double Sec = FMath::Max(0.1, FPlatformTime::Seconds() - MStart);
		ADP_LOG(TEXT("Views: %-46s frame %s | gpu %s | render %s | game %s | shown %.0f/s (%.0f made) | %d people | graphics memory %s"), What, *Stat(Fr), *Stat(Gp), *Stat(Rn), *Stat(Gm), (S - Shown0) / Sec, (M - Made0) / Sec, People, *Mem());
	}
	void BeginMeasure(double Sec, const FString& N) { Name = N; Fr.Reset(); Gp.Reset(); Rn.Reset(); Gm.Reset(); MStart = FPlatformTime::Seconds(); Until = MStart + Sec; ADPFrameGen::Counts(Shown0, Made0); }

	bool Tick(float Dt)
	{
		UWorld* W = ViewsWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (!PC || !PC->PlayerCameraManager) return true;
		if (bHeld) Place(PC);
		for (TStrongObjectPtr<UTextureRenderTarget2D>& T : Eaten) UKismetRenderingLibrary::ClearRenderTarget2D(W, T.Get(), FLinearColor::Black);
		if (!Name.IsEmpty()) { Fr.Add(Dt * 1000.f); Gp.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())); Rn.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime)); Gm.Add(FPlatformTime::ToMilliseconds(GGameThreadTime)); }
		if (FPlatformTime::Seconds() < Until) return true;
		if (!Name.IsEmpty()) { Report(*Name); Name.Reset(); }
		if (At >= Steps.Num()) return false;
		const FString L = Steps[At++].TrimStartAndEnd();
		FString K, A; if (!L.Split(TEXT(" "), &K, &A)) K = L;
		TArray<FString> V; A.ParseIntoArrayWS(V);
		if ((K == TEXT("view") && V.Num() >= 5) || (K == TEXT("fly") && V.Num() >= 6))
		{
			From = PC->PlayerCameraManager->GetCameraLocation(); RotFrom = Rot;
			Want = FVector(FCString::Atof(*V[0]) * 100.f, -FCString::Atof(*V[2]) * 100.f, FCString::Atof(*V[1]) * 100.f);
			Rot = FRotator(FCString::Atof(*V[4]), FCString::Atof(*V[3]), 0.f); bHeld = true; FlyLen = 0.0;
			if (K == TEXT("fly")) { FlyLen = FCString::Atof(*V[5]); FlyStart = FPlatformTime::Seconds(); BeginMeasure(FlyLen, TEXT("fly to ") + A); }
			else ADP_LOG(TEXT("Views: at %s"), *A);
		}
		else if (K == TEXT("cmd")) { ADP_LOG(TEXT("Views: %s"), *A); PC->ConsoleCommand(A); }
		else if (K == TEXT("hour")) { if (ASunClock::DayOverride < 0) ASunClock::DayOverride = 172; ASunClock::HourOverride = FCString::Atof(*A); ASunClock::Refresh(); }
		else if (K == TEXT("day")) { ASunClock::DayOverride = FCString::Atoi(*A); ASunClock::Refresh(); }
		else if (K == TEXT("weather")) { if (!ASunClock::ManualWeatherAllowed()) ADP_WARN(TEXT("Views: the weather is live here, it can not be set")); ASunClock::Theme = FCString::Atoi(*A); ASunClock::Refresh(); ADP_LOG(TEXT("Views: weather %s"), ASunClock::ThemeName(ASunClock::Theme)); }
		else if (K == TEXT("wait")) Until = FPlatformTime::Seconds() + FCString::Atof(*A);
		else if (K == TEXT("measure") && V.Num()) { FString S, N; const bool bN = A.Split(TEXT(" "), &S, &N); BeginMeasure(FCString::Atof(*V[0]), bN ? N : A); }
		else if (K == TEXT("gpu")) { PC->ConsoleCommand(TEXT("ProfileGPU")); Until = FPlatformTime::Seconds() + 2.0; }
		else if (K == TEXT("hmi") && V.Num())
		{
			AADPHUD* H = Cast<AADPHUD>(PC->GetHUD());
			if (H && V[0] == TEXT("close")) H->EndHmi();
			else if (H) { bHeld = false; H->OpenHmi(FCString::Atoi(*V[0])); if (AJetwayHmi* M = AJetwayHmi::Get(W); M && V.Num() > 1) M->Command(TEXT("tab:") + V[1]); }
			ADP_LOG(TEXT("Views: hmi %s"), *A);
		}
		else if (K == TEXT("eat"))
		{
			for (int32 Mb = FCString::Atoi(*A); Mb > 0; Mb -= 128)
			{
				UTextureRenderTarget2D* T = NewObject<UTextureRenderTarget2D>(); T->InitCustomFormat(4096, 4096, PF_FloatRGBA, true); T->UpdateResourceImmediate(true);
				Eaten.Add(TStrongObjectPtr<UTextureRenderTarget2D>(T));
			}
			ADP_LOG(TEXT("Views: %s MB taken; %s"), *A, *ADPGraphicsMemory::Line());
		}
		else if (K == TEXT("census"))
		{
			// how many instances of each mesh are in the distance field scene (its buffers are sized by them)
			TMap<FString, FIntPoint> By; int64 All = 0, Df = 0, Tiny = 0;
			for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
			{
				UStaticMeshComponent* C = *It;
				if (C->GetWorld() != W || !C->IsRegistered() || !C->IsVisible() || !C->GetStaticMesh()) continue;
				const UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(C);
				const int32 Nn = I ? I->GetInstanceCount() : 1; All += Nn;
				if (!C->bAffectDistanceFieldLighting) continue;
				const float Rad = C->GetStaticMesh()->GetBounds().SphereRadius;
				Df += Nn; if (Rad < 15.f) Tiny += Nn;
				FIntPoint& E = By.FindOrAdd(FString::Printf(TEXT("%s / %s %s r=%.0f"), C->GetOwner() ? *C->GetOwner()->GetClass()->GetName() : TEXT("-"), *C->GetStaticMesh()->GetName(), C->bAffectDynamicIndirectLighting ? TEXT("lumen") : TEXT("-"), Rad));
				E.X += Nn; ++E.Y;
			}
			By.ValueSort([](const FIntPoint& P, const FIntPoint& Q) { return P.X > Q.X; });
			ADP_LOG(TEXT("Views: census %lld instances, %lld in the distance fields (%lld under 15 cm)"), All, Df, Tiny);
			int32 k = 0; for (const auto& E : By) { if (++k > 40) break; ADP_LOG(TEXT("Views: census %7d in %4d  %s"), E.Value.X, E.Value.Y, *E.Key); }
		}
		else if (K == TEXT("shot")) FScreenshotRequest::RequestScreenshot(A, false, false);
		else if (K == TEXT("shotui")) FScreenshotRequest::RequestScreenshot(A, true, false);   // (with the stats on it)
		else if (K == TEXT("quit")) { ADP_LOG(TEXT("Views: done")); FPlatformMisc::RequestExit(false); return false; }
		else if (!L.IsEmpty() && !L.StartsWith(TEXT("#"))) ADP_WARN(TEXT("Views: not understood: %s"), *L);
		return true;
	}
}

void ADPPerfViews::Begin(const FString& File)
{
	using namespace ADPPerfViewsInternal;
	if (!FFileHelper::LoadFileToStringArray(Steps, *File)) { ADP_ERROR(TEXT("Views: no file %s"), *File); return; }
	At = 0; Until = 0.0; Name.Reset();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
	ADP_LOG(TEXT("Views: %d steps from %s"), Steps.Num(), *File);
}
