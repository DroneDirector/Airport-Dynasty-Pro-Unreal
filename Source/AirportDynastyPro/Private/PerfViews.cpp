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
//   speed <career> <world>                the career's clock and the world's (1 1: as played)
//   follow <seconds> <name>               beside a walking passenger with most people around, clear of walls and people, measured
//   people <n> <seconds>                  waits until n people are about; if they don't come, the run ends there, as failed
// The camera never stands in or goes through anything: a view in something solid is left out, a flight ends before it.
//   quit
#include "PerfViews.h"
#include "People.h"
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
#include "Sim/ADPSim.h"
#include "Kismet/GameplayStatics.h"
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
	// the people about and how fast each goes (looked at twice a second)
	TMap<TWeakObjectPtr<AActor>, FVector> Was; TMap<TWeakObjectPtr<AActor>, FVector> Going; double LookedAt = 0.0;
	TWeakObjectPtr<AActor> Subject; bool bFollow = false, bCut = false; int32 Need = 0; float Side = 35.f;
	void LookAtPeople(UWorld* W)
	{
		const double Now = FPlatformTime::Seconds(), D = Now - LookedAt;
		if (D < 0.5) return;
		LookedAt = Now; Going.Reset(); TMap<TWeakObjectPtr<AActor>, FVector> Is;
		if (AADPPeople* P = AADPPeople::Get(W))
			for (const AADPPeople::FWho& Q : P->Everyone())
				if (AActor* A = Q.A.Get(); A && !A->IsHidden())
				{
					Is.Add(A, A->GetActorLocation());
					if (const FVector* L = Was.Find(A)) Going.Add(A, (A->GetActorLocation() - *L) / D); else Going.Add(A, FVector::ZeroVector);
				}
		Was = MoveTemp(Is);
	}
	int32 Around(const FVector& Spot, double R) { int32 N = 0; for (const auto& E : Was) if (FVector::Dist(E.Value, Spot) < R) ++N; return N; }
	bool Solid(UWorld* W, const FVector& Spot, const AActor* Me)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ViewsCam), false, Me);
		return W->OverlapBlockingTestByChannel(Spot, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(22.f), Q) || Around(Spot - FVector(0, 0, 90.f), 70.0) > 0;
	}
	bool NextSubject()
	{
		AActor* Best = nullptr; int32 Most = -1;
		for (const auto& E : Going) if (AActor* A = E.Key.Get()) if (A != Subject.Get() && E.Value.Size2D() > 30.0) { const int32 N = Around(A->GetActorLocation(), 2500.0); if (N > Most) { Most = N; Best = A; } }
		if (!Best) return false;
		Subject = Best; bCut = true;
		ADP_LOG(TEXT("Views: following %s at %.1f %.1f %.1f, %d people within 25 m"), *Best->GetName(), Best->GetActorLocation().X / 100.f, Best->GetActorLocation().Z / 100.f, -Best->GetActorLocation().Y / 100.f, Most);
		return true;
	}
	/** Beside and behind the one followed, the whole person in view: the first of a ring of places with a free line of sight. */
	bool FollowAt(UWorld* W, const AActor* Me, float Dt)
	{
		AActor* A = Subject.Get();
		const FVector* G = A ? Going.Find(A) : nullptr;
		if (!A || !G) { if (!NextSubject()) return false; A = Subject.Get(); G = Going.Find(A); if (!G) return false; }
		const FVector Mid = A->GetActorLocation() + FVector(0, 0, 110.f), Fw = G->Size2D() > 30.0 ? G->GetSafeNormal2D() : A->GetActorRightVector().GetSafeNormal2D();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ViewsCam), false, Me); Q.AddIgnoredActor(A);
		static const float Turn[] = { 0.f, -70.f, 40.f, -110.f, 100.f, -30.f, 150.f };
		for (float Out : { 480.f, 330.f, 230.f })
			for (float T : Turn)
			{
				const FVector C = Mid - Fw.RotateAngleAxis(Side + T, FVector::UpVector) * Out + FVector(0, 0, 70.f);
				FHitResult H;
				if (W->SweepSingleByChannel(H, Mid, C, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(24.f), Q) || Solid(W, C, Me)) continue;
				// (gliding there; if something stands between, it cuts over instead of going through it)
				const bool bWay = !bCut && FVector::Dist(Want, C) < 900.0 && !W->SweepSingleByChannel(H, Want, C, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(20.f), Q);
				Want = bWay ? FMath::VInterpTo(Want, C, Dt, 2.5f) : C; bCut = false;
				const FRotator To = (Mid - Want).Rotation(); Rot = bWay ? FMath::RInterpTo(Rot, To, Dt, 4.f) : To;
				if (T != 0.f && bWay) Side = FRotator::NormalizeAxis(Side + T * FMath::Min(1.f, Dt));
				return true;
			}
		return true;   // (no free place just now: the camera stays where it is)
	}
	void Place(APlayerController* PC)
	{
		APawn* P = PC->GetPawn(); if (!P || !PC->PlayerCameraManager) return;
		if (ACharacter* Ch = Cast<ACharacter>(P)) { Ch->SetActorEnableCollision(false); if (Ch->GetCharacterMovement()) { Ch->GetCharacterMovement()->StopMovementImmediately(); Ch->GetCharacterMovement()->DisableMovement(); } }
		FVector W = Want; FRotator R = Rot;
		if (FlyLen > 0.0)
		{
			const double T = FMath::Clamp((FPlatformTime::Seconds() - FlyStart) / FlyLen, 0.0, 1.0); W = FMath::Lerp(From, Want, T); R = FMath::Lerp(RotFrom, Rot, (float)T);
			FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(ViewsCam), false, P);
			const FVector Ahead = W + (Want - From).GetSafeNormal() * 60.0;
			if (P->GetWorld()->SweepSingleByChannel(H, W, Ahead, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(22.f), Q) || Around(Ahead - FVector(0, 0, 90.f), 70.0) > 0)
			{
				if (H.bBlockingHit) ADP_LOG(TEXT("Views: the flight ends before %s"), H.GetActor() ? *H.GetActor()->GetName() : TEXT("something"));
				Want = W; Rot = R; FlyLen = 0.0;   // (it waits there)
			}
		}
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
		People = Was.Num(); int32 Walking = 0; for (const auto& E : Going) if (E.Value.Size2D() > 30.0) ++Walking;
		const APlayerController* Pc = ViewsWorld() ? ViewsWorld()->GetFirstPlayerController() : nullptr;
		const int32 Close = Pc && Pc->PlayerCameraManager ? Around(Pc->PlayerCameraManager->GetCameraLocation(), 3000.0) : 0;
		uint64 S = 0, M = 0; ADPFrameGen::Counts(S, M); const double Sec = FMath::Max(0.1, FPlatformTime::Seconds() - MStart);
		ADP_LOG(TEXT("Views: %-46s frame %s | gpu %s | render %s | game %s | shown %.0f/s (%.0f made) | %d people, %d walking, %d within 30 m | graphics memory %s"), What, *Stat(Fr), *Stat(Gp), *Stat(Rn), *Stat(Gm), (S - Shown0) / Sec, (M - Made0) / Sec, People, Walking, Close, *Mem());
	}
	void BeginMeasure(double Sec, const FString& N) { Name = N; Fr.Reset(); Gp.Reset(); Rn.Reset(); Gm.Reset(); MStart = FPlatformTime::Seconds(); Until = MStart + Sec; ADPFrameGen::Counts(Shown0, Made0); }

	bool Tick(float Dt)
	{
		UWorld* W = ViewsWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (!PC || !PC->PlayerCameraManager) return true;
		LookAtPeople(W);
		if (bFollow) FollowAt(W, PC->GetPawn(), Dt);
		if (bHeld) Place(PC);
		for (TStrongObjectPtr<UTextureRenderTarget2D>& T : Eaten) UKismetRenderingLibrary::ClearRenderTarget2D(W, T.Get(), FLinearColor::Black);
		if (!Name.IsEmpty()) { Fr.Add(Dt * 1000.f); Gp.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())); Rn.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime)); Gm.Add(FPlatformTime::ToMilliseconds(GGameThreadTime)); }
		if (Need > 0)
		{
			if (Was.Num() >= Need) { ADP_LOG(TEXT("Views: %d people are about"), Was.Num()); Need = 0; Until = 0.0; }
			else if (FPlatformTime::Seconds() >= Until) { ADP_ERROR(TEXT("Views: FAILED - %d people about instead of %d, the run ends here"), Was.Num(), Need); FPlatformMisc::RequestExit(false); return false; }
		}
		if (FPlatformTime::Seconds() < Until) return true;
		if (!Name.IsEmpty()) { Report(*Name); Name.Reset(); bFollow = false; }
		if (At >= Steps.Num()) return false;
		const FString L = Steps[At++].TrimStartAndEnd();
		FString K, A; if (!L.Split(TEXT(" "), &K, &A)) K = L;
		TArray<FString> V; A.ParseIntoArrayWS(V);
		if ((K == TEXT("view") && V.Num() >= 5) || (K == TEXT("fly") && V.Num() >= 6))
		{
			const FVector To(FCString::Atof(*V[0]) * 100.f, -FCString::Atof(*V[2]) * 100.f, FCString::Atof(*V[1]) * 100.f);
			if (K == TEXT("view") && Solid(W, To, PC->GetPawn())) { ADP_WARN(TEXT("Views: %s is inside something - left out"), *A); return true; }
			From = PC->PlayerCameraManager->GetCameraLocation(); RotFrom = Rot;
			Want = To;
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
		else if (K == TEXT("speed") && V.Num())
		{
			if (UADPSim* S = UADPSim::Get(W)) S->SetSpeed(FCString::Atof(*V[0]));
			UGameplayStatics::SetGlobalTimeDilation(W, V.Num() > 1 ? FCString::Atof(*V[1]) : 1.f); ADP_LOG(TEXT("Views: speed %s"), *A);
		}
		else if (K == TEXT("people") && V.Num() > 1) { Need = FCString::Atoi(*V[0]); Until = FPlatformTime::Seconds() + FCString::Atof(*V[1]); }
		else if (K == TEXT("follow") && V.Num())
		{
			FString S2, N2; const bool bN = A.Split(TEXT(" "), &S2, &N2);
			if (!NextSubject()) ADP_WARN(TEXT("Views: nobody walks (%d people)"), Was.Num());
			else { Want = PC->PlayerCameraManager->GetCameraLocation(); bHeld = bFollow = true; FlyLen = 0.0; }
			BeginMeasure(FCString::Atof(*V[0]), bN ? N2 : A);
		}
		else if (K == TEXT("textures"))
		{
			// how sharp the textures of these kit materials are just now (to-do 248)
			for (const FString& N : V)
				if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Materials/Kit/MI_%s.MI_%s"), *N, *N)))
				{
					TArray<UTexture*> Tx; M->GetUsedTextures(Tx); FString Ln;
					for (UTexture* T : Tx) if (UTexture2D* T2 = Cast<UTexture2D>(T)) Ln += FString::Printf(TEXT(" %s %d of %d%s"), *T2->GetName(), T2->GetSizeX() >> FMath::Max(0, T2->GetNumMips() - T2->GetNumResidentMips()), T2->GetSizeX(), T2->IsStreamable() ? TEXT("") : TEXT(" (always)"));
					ADP_LOG(TEXT("Views: textures of %s:%s"), *N, *Ln);
				}
				else ADP_WARN(TEXT("Views: MI_%s is missing"), *N);
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
