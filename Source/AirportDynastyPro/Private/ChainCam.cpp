// The camera director of the chain test's window (owner, 2026-11-23: the test's camera shows what is being tested, not the
// player from behind): it cuts between steady views of what happens now - a passenger at the step shown least lately (car park,
// desk, security, escalator, gate, jetway, off the aircraft, belt, walking out), an aircraft moving, a jetway driving out - each
// at least 8 s, slow smooth moves, never through a wall, the interface away but a one-line caption. The view is the player's
// camera's, so streaming and detail come with it. It only looks: nothing of the test changes. Off: -NoChainCam.
// -ChainShots: the pictures of passengers in the jetway (Chain_Jetway_Inside / _Outside), taken at the game's own speed.
#include "ChainTest.h"
#include "ADPLog.h"
#include "JetwayRigs.h"
#include "PaxFlow.h"
#include "SimTraffic.h"
#include "DepartureBelts.h"
#include "BaggageBelts.h"
#include "TerminalDoors.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSim.h"
#include "UI/ADPHUD.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace
{
	struct FDirector
	{
		TWeakObjectPtr<ACameraActor> Cam; TSharedPtr<STextBlock> Text;
		FString Id, Line; int32 Kind = -1; double Since = 0.0, LostAt = 0.0; FVector Eye = FVector::ZeroVector, Look = FVector::ZeroVector, Drift = FVector::ZeroVector; FRotator Rot = FRotator::ZeroRotator;
		TMap<int32, double> Shown;      // when each kind was last shown
		bool bCut = true; float Fov = 60.f;
		int32 Shot = 0; double ShotAt = 0.0; FString ShotName; float SpeedWas = 1.f, DilWas = 1.f;
		FVector ShotEye = FVector::ZeroVector, ShotLook = FVector::ZeroVector; int32 Shots = 0;
	};
	FDirector GDir;

	/** Nothing solid between two points (people, bags and glass doors don't count). */
	bool CcClear(UWorld* W, const FVector& A, const FVector& B, const AActor* Who)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ChainCam), false, Who);
		if (AActor* Db = ADepartureBelts::Get(W)) Q.AddIgnoredActor(Db);
		if (AActor* Ab = ABaggageBelts::Get(W)) Q.AddIgnoredActor(Ab);
		FHitResult H;
		return !W->SweepSingleByObjectType(H, A, B, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(14.f), Q);
	}

	/** A place to look from: round the subject, its front first, 3.5 - 6.5 m off at eye height, in plain sight of it. */
	bool CcPlace(UWorld* W, const FVector& Feet, const FVector& Face, const AActor* Who, FVector& Eye)
	{
		const FVector Head = Feet + FVector(0, 0, 140.f);
		const float Angs[] = {35.f, -35.f, 70.f, -70.f, 105.f, -105.f, 10.f, 140.f, -140.f, 180.f}, Ds[] = {560.f, 420.f, 300.f};
		for (const float D : Ds)
			for (const float An : Angs)
			{
				const FVector P = Feet + Face.GetSafeNormal2D().RotateAngleAxis(An, FVector::UpVector) * D + FVector(0, 0, 185.f);
				if (CcClear(W, Head, P, Who)) { Eye = P; return true; }
			}
		return false;
	}

	/** Two views of a jetway with people in it. Inside: on the tunnel's middle line as the passengers' flow measured it, ahead of those
	 *  walking, looking along it at their faces. Outside: from the apron, the aircraft's nose and door and the tunnel all in the frame. */
	bool CcJetway(UWorld* W, int32 Jet, bool bInside, int32 Variant, const TArray<APaxFlow::FScene>& All, FVector& Eye, FVector& Look)
	{
		AJetwayRigs* JR = AJetwayRigs::Get(W); const APaxFlow* P = APaxFlow::Find(W);
		FVector Ro, Dr, Mid, Front; if (!JR || !P || !JR->Frame(Jet, Ro, Dr) || !JR->CabView(Jet, Mid, Front)) return false;
		Ro.Z = P->Floor2Z();
		TArray<FVector> Line; P->JetwayLine(Jet, Line);   // (its floor's middle, every 40 cm from the aircraft's door to the gate's)
		if (bInside)
		{
			if (Line.Num() < 12) return false;
			// who walks in it: where along the line, and which way
			int32 Lo = MAX_int32, Hi = -1, Out = 0, In = 0;
			for (const APaxFlow::FScene& S : All)
			{
				if (S.Jet != Jet) continue;
				int32 Bk = 0; float Bd = MAX_flt;
				for (int32 k = 0; k < Line.Num(); ++k) { const float D = FVector::DistSquared2D(Line[k], S.At); if (D < Bd) { Bd = D; Bk = k; } }
				if (Bd > FMath::Square(250.f)) continue;
				Lo = FMath::Min(Lo, Bk); Hi = FMath::Max(Hi, Bk); ++(S.Kind == 5 ? In : Out);
			}
			if (Hi < 0) return false;
			// ahead of the first of them, as far as the tunnel is straight (5 to 9 m): the whole line of people comes towards the camera
			const bool bOut = Out >= In; const int32 Lead = bOut ? Hi : Lo, Step = bOut ? 1 : -1, Last = Line.Num() - 2;
			int32 At = INDEX_NONE;
			for (int32 n = 22; n >= 4 && At == INDEX_NONE; --n)
			{
				const int32 k = Lead + Step * n; if (k < 2 || k > Last) continue;
				if (CcClear(W, Line[k] + FVector(0, 0, 165.f), Line[Lead] + FVector(0, 0, 150.f), nullptr)) At = k;
			}
			if (At == INDEX_NONE)
			{
				// (the first of them at its end already: from just past the middle of them, at those behind)
				const int32 k = FMath::Clamp(Lead, 2, Last), q = FMath::Clamp(Lead - Step * 14, 0, Line.Num() - 1);
				if (FMath::Abs(k - q) < 6 || !CcClear(W, Line[k] + FVector(0, 0, 165.f), Line[q] + FVector(0, 0, 150.f), nullptr)) return false;
				At = k;
			}
			const int32 To = FMath::Clamp(At - Step * 18, 0, Line.Num() - 1);   // (7 m on, along the middle line)
			Eye = Line[At] + FVector(0, 0, 168.f); Look = Line[To] + FVector(0, 0, 125.f);
			return true;
		}
		// the aircraft's nose end (it stands nose in: the end towards the terminal), its door, the tunnel's middle
		const AActor* Ac = JR->AircraftAtStand(Jet);
		FVector Nose = Mid + Front * 500.f; FBox Ab(ForceInit);
		if (Ac) { Ab = Ac->GetComponentsBoundingBox(false); if (Ab.IsValid) Nose = FVector(Ab.GetCenter().X, Ab.Max.Y - 350.f, Mid.Z - 60.f); }
		const FVector Tun = FMath::Lerp(Ro, Mid, 0.45f), Focus = (Nose + Mid + Tun) / 3.f;
		const float Apron = P->Floor2Z() - 600.f;
		TArray<TPair<float, FVector>> Best;
		for (const float D : {1900.f, 2500.f, 3100.f})
			for (int32 k = 0; k < 24; ++k)
			{
				FVector E = Focus + FRotator(0.f, k * 15.f, 0.f).Vector() * D; E.Z = Apron + 380.f;   // (3.8 m over the apron)
				if (E.Y > Ro.Y - 250.f) continue;                                                    // (out on the apron, never the terminal's side)
				if (Ab.IsValid && Ab.ExpandBy(FVector(250.f, 250.f, 0.f)).IsInsideXY(E)) continue;
				// (the three in one frame, and nothing before them: the last metres towards them are their own)
				float Wide = 0.f; const FVector Pt[3] = {Nose, Mid, Tun}; bool bOk = true; const FVector Mv = (Focus - E).GetSafeNormal2D();
				for (const FVector& Q : Pt)
				{
					const FVector V = Q - E; Wide = FMath::Max(Wide, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(FVector::DotProduct(V.GetSafeNormal2D(), Mv)), -1.f, 1.f))));
					if (!CcClear(W, E, Q - V.GetSafeNormal() * FMath::Min(450.f, V.Size() * 0.3f), nullptr)) { bOk = false; break; }
				}
				if (!bOk || Wide > 24.f) continue;
				// (the tunnel seen from the side rather than end on)
				const float Side = FMath::Abs(FVector::CrossProduct((Mid - Ro).GetSafeNormal2D(), Mv).Z);
				Best.Emplace(Side * 30.f + Wide, E);
			}
		if (!Best.Num()) return false;
		Best.Sort([](const TPair<float, FVector>& A, const TPair<float, FVector>& B2) { return A.Key > B2.Key; });
		Eye = Best[FMath::Min(Variant, Best.Num() - 1)].Value; Look = Focus;
		return true;
	}
}

void ADPChainTest::Camera(UWorld* W, float Dt)
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("NoChainCam"));
	static const bool bShots = FParse::Param(FCommandLine::Get(), TEXT("ChainShots"));
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; const APaxFlow* P = W ? APaxFlow::Find(W) : nullptr;
	if (bOff || !PC || !P || !StartStage()) return;
	const double Now = FPlatformTime::Seconds();
	if (!GDir.Cam.IsValid())
	{
		FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GDir.Cam = W->SpawnActor<ACameraActor>(PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector, FRotator::ZeroRotator, Sp);
		if (!GDir.Cam.IsValid()) return;
		GDir.Cam->GetCameraComponent()->bConstrainAspectRatio = false;
		if (AADPHUD* H = Cast<AADPHUD>(PC->GetHUD())) H->TestHide();
		if (GEngine && GEngine->GameViewport)
			GEngine->GameViewport->AddViewportWidgetContent(
				SNew(SOverlay) + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(24.f, 0.f, 0.f, 18.f)
				[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.45f)).Padding(FMargin(12.f, 5.f)).Visibility(EVisibility::HitTestInvisible)
					[ SAssignNew(GDir.Text, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).ColorAndOpacity(FLinearColor::White) ] ], 90);
	}
	// what happens now
	TArray<APaxFlow::FScene> All; P->Scenes(All); ASimTraffic::Scenes(W, All);
	const double Held = Now - GDir.Since;
	const APaxFlow::FScene* Cur = All.FindByPredicate([](const APaxFlow::FScene& S) { return S.Id == GDir.Id; });
	// the pictures asked for: five or more people in a jetway - at the game's own speed, two views
	if (bShots && GDir.Shot == 0 && GDir.Shots < 3 && Now - GDir.ShotAt > 12.0)
	{
		TMap<int32, int32> InJet; for (const APaxFlow::FScene& S : All) if (S.Jet != INDEX_NONE) ++InJet.FindOrAdd(S.Jet);
		for (const auto& K : InJet) if (K.Value >= 5 && CcJetway(W, K.Key, true, GDir.Shots, All, GDir.ShotEye, GDir.ShotLook))
		{
			if (UADPSim* S = UADPSim::Get(W)) { GDir.SpeedWas = S->Speed(); S->SetSpeed(1.0); }
			GDir.DilWas = UGameplayStatics::GetGlobalTimeDilation(W); UGameplayStatics::SetGlobalTimeDilation(W, 1.f);
			// (at the window's own size: a change to 4K did not take while it ran - for 4K the run itself is started with -ResX / -ResY)
			GDir.Shot = 1 + K.Key * 10; GDir.ShotAt = Now; GDir.bCut = true; ADP_LOG(TEXT("CHAIN TEST: camera in jetway %d, %d passengers in it"), K.Key, K.Value); break;
		}
	}
	if (GDir.Shot)
	{
		const int32 St = GDir.Shot % 10, Jet = GDir.Shot / 10; const double T = Now - GDir.ShotAt;
		if (St == 1 && T > 2.5) { GDir.ShotName = FString::Printf(TEXT("Chain_Jetway_Inside_%d"), GDir.Shots + 1); FScreenshotRequest::RequestScreenshot(GDir.ShotName, false, false); GDir.Shot = 2 + Jet * 10; GDir.ShotAt = Now; }
		else if (St == 2 && T > 1.0) 
		{
			// (no place outside from which all of it is seen: no picture rather than a wrong one)
			const bool bSeen = CcJetway(W, Jet, false, GDir.Shots, All, GDir.ShotEye, GDir.ShotLook);
			if (!bSeen) ADP_LOG(TEXT("CHAIN TEST: no place on the apron sees jetway %d and its aircraft together - no outside picture"), Jet);
			GDir.bCut = true; GDir.Shot = (bSeen ? 3 : 4) + Jet * 10; GDir.ShotAt = Now;
		}
		else if (St == 3 && T > 3.5) { GDir.ShotName = FString::Printf(TEXT("Chain_Jetway_Outside_%d"), GDir.Shots + 1); FScreenshotRequest::RequestScreenshot(GDir.ShotName, false, false); GDir.Shot = 4 + Jet * 10; GDir.ShotAt = Now; }
		else if (St == 4 && T > 1.0)
		{
			if (UADPSim* S = UADPSim::Get(W)) S->SetSpeed(GDir.SpeedWas);
			UGameplayStatics::SetGlobalTimeDilation(W, GDir.DilWas);
			++GDir.Shots; GDir.Shot = 0; GDir.ShotAt = Now; GDir.Id.Empty(); ADP_LOG(TEXT("CHAIN TEST: jetway pictures %d taken"), GDir.Shots); return;
		}
		GDir.Cam->GetCameraComponent()->SetFieldOfView(St <= 2 ? 72.f : 60.f);
		GDir.Cam->SetActorLocationAndRotation(GDir.ShotEye, (GDir.ShotLook - GDir.ShotEye).Rotation());
		if (PC->GetViewTarget() != GDir.Cam.Get()) PC->SetViewTarget(GDir.Cam.Get());
		return;
	}
	const bool bLost = !Cur || (Cur->Kind != GDir.Kind && Held > 8.0) || (Cur->Kind < 20 && FVector::Dist(GDir.Eye, Cur->At) > 1500.f);
	if (bLost && GDir.LostAt == 0.0) GDir.LostAt = Now;
	if (!bLost) GDir.LostAt = 0.0;
	if (GDir.Id.IsEmpty() || Held > 12.0 || (bLost && Held > 8.0) || (!Cur && Held > 2.0))
	{
		// the step shown least lately that happens now; of it the one with a place to look from
		All.Sort([](const APaxFlow::FScene& A, const APaxFlow::FScene& B)
		{
			const double* X = GDir.Shown.Find(A.Kind); const double* Y = GDir.Shown.Find(B.Kind); const double Xa = X ? *X : 0.0, Yb = Y ? *Y : 0.0;
			return Xa != Yb ? Xa < Yb : A.News > B.News;
		});
		int32 Tried = 0;
		for (const APaxFlow::FScene& S : All)
		{
			if (++Tried > 40) break;
			if (S.Id == GDir.Id && All.Num() > 1) continue;
			FVector Eye, Look = S.At + FVector(0, 0, 110.f);
			if (S.Kind >= 20) { Eye = S.At + S.Face.GetSafeNormal2D().RotateAngleAxis(GDir.Shown.Num() % 2 ? 40.f : -40.f, FVector::UpVector) * 5200.f + FVector(0, 0, 620.f); Look = S.At + FVector(0, 0, 250.f); }
			else if (S.Jet != INDEX_NONE) { if (!CcJetway(W, S.Jet, (int32(Now) / 10) % 2 == 0, 0, All, Eye, Look)) continue; }
			else if (!CcPlace(W, S.At, S.Face, S.Who.Get(), Eye)) continue;
			GDir.Id = S.Id; GDir.Kind = S.Kind; GDir.Since = Now; GDir.LostAt = 0.0; GDir.Eye = Eye; GDir.Look = Look; GDir.Line = S.Line; GDir.bCut = true; GDir.Shown.Add(S.Kind, Now);
			GDir.Drift = FVector::CrossProduct((Look - Eye).GetSafeNormal2D(), FVector::UpVector) * (S.Kind >= 20 ? 40.f : 9.f);
			GDir.Fov = S.Jet != INDEX_NONE ? 70.f : S.Kind >= 20 ? 45.f : 58.f;
			break;
		}
		Cur = All.FindByPredicate([](const APaxFlow::FScene& S) { return S.Id == GDir.Id; });
	}
	if (GDir.Id.IsEmpty()) return;
	// steady: the eye drifts slowly sideways, the look eases after the subject
	if (Cur && !(Cur->Jet != INDEX_NONE && GDir.Fov == 70.f)) { const FVector To = Cur->At + FVector(0, 0, Cur->Kind >= 20 ? 250.f : 110.f); GDir.Look = GDir.bCut ? To : FMath::VInterpTo(GDir.Look, To, Dt, 2.f); GDir.Line = Cur->Line; }
	const FVector Next = GDir.Eye + GDir.Drift * Dt;
	if (GDir.Kind >= 20 || CcClear(W, GDir.Eye, Next, nullptr)) GDir.Eye = Next;
	const FRotator Want = (GDir.Look - GDir.Eye).Rotation();
	GDir.Rot = GDir.bCut ? Want : FMath::RInterpTo(GDir.Rot, Want, Dt, 2.5f);
	GDir.Cam->GetCameraComponent()->SetFieldOfView(GDir.Fov);
	GDir.Cam->SetActorLocationAndRotation(GDir.Eye, GDir.Rot);
	if (PC->GetViewTarget() != GDir.Cam.Get()) PC->SetViewTarget(GDir.Cam.Get());
	if (GDir.bCut && PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
	GDir.bCut = false;
	if (GDir.Text.IsValid()) if (const UADPSim* S = UADPSim::Get(W)) GDir.Text->SetText(FText::FromString(FString::Printf(TEXT("%s    %s    x%.0f"), *GDir.Line, *ADPBoards::Local(S->Now()).ToString(TEXT("%H:%M")), S->Speed())));
}
