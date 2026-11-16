#include "StressWalk.h"
#include "PerfLog.h"
#include "ADPLog.h"
#include "TerminalPlan.h"
#include "Escalators.h"
#include "ADPGameMode.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectGlobals.h"

namespace ADPStressWalkInternal
{
	// one part of the walk; places in the terminal plan's metres (x east, h up, z north)
	enum class EWalkKind : uint8 { Go, Walk, Look, Face, Escalator };
	struct FWalkPart { const TCHAR* What; EWalkKind Kind; TArray<FVector> Points; float Seconds; float Yaw; };   // (Points: x, h, z)

	TArray<FWalkPart> WalkRoute()
	{
		auto P = [](float X, float H, float Z) { return FVector(X, H, Z); };
		// the jetway's middle line from its rotunda round to the cab (the sleeve runs from (-50.3, 96.5) to (-58.3, 109.5))
		TArray<FVector> Sleeve, SleeveBack;
		for (int32 i = 1; i <= 6; ++i) { const float T = i / 6.f; Sleeve.Add(P(-50.3f - 8.f * T, 4.7f - 1.7f * T, 96.5f + 13.f * T)); }
		for (int32 i = 5; i >= 0; --i) { const float T = i / 6.f; SleeveBack.Add(P(-50.3f - 8.f * T, 4.7f - 1.7f * T, 96.5f + 13.f * T)); }
		return {
			{TEXT("outside, past the road lanes, facing the departures entrance"), EWalkKind::Go, {P(79.5f, 0.f, -62.f)}, 0.f, 0.f},
			{TEXT("looking at the main entrance"), EWalkKind::Face, {}, 4.f, 90.f},
			{TEXT("walking up to the main entrance"), EWalkKind::Walk, {P(79.5f, 0.f, -30.f), P(79.5f, 0.f, -8.f)}, 40.f, 0.f},
			{TEXT("turning round near the entrance"), EWalkKind::Look, {}, 10.f, 0.f},
			{TEXT("the Grand Hall, middle"), EWalkKind::Go, {P(75.f, 0.f, 15.f)}, 0.f, 0.f},
			{TEXT("looking round the Grand Hall"), EWalkKind::Look, {}, 12.f, 0.f},
			{TEXT("walking east through the Grand Hall"), EWalkKind::Walk, {P(92.f, 0.f, 18.f), P(105.f, 0.f, 20.f)}, 25.f, 0.f},
			{TEXT("looking round"), EWalkKind::Look, {}, 8.f, 0.f},
			{TEXT("at the airside stairs"), EWalkKind::Go, {P(54.f, 0.f, 47.f)}, 0.f, 0.f},
			{TEXT("up the airside stairs to Floor 2"), EWalkKind::Walk, {P(57.5f, 0.f, 47.f), P(66.6f, 6.f, 47.f), P(67.f, 6.f, 45.f)}, 25.f, 0.f},
			{TEXT("looking round at the top of the stairs"), EWalkKind::Look, {}, 10.f, 0.f},
			{TEXT("up the escalator"), EWalkKind::Escalator, {}, 18.f, 0.f},
			{TEXT("looking round on Floor 2 by the escalators"), EWalkKind::Look, {}, 12.f, 0.f},
			{TEXT("walking into the gate walkway"), EWalkKind::Walk, {P(10.f, 6.f, 26.f), P(10.f, 6.f, 45.f), P(10.f, 6.f, 59.f)}, 35.f, 0.f},
			{TEXT("walking the gate walkway east"), EWalkKind::Walk, {P(60.f, 6.f, 60.f), P(120.f, 6.f, 60.f)}, 45.f, 0.f},
			{TEXT("looking round in the gate walkway"), EWalkKind::Look, {}, 10.f, 0.f},
			{TEXT("in gate 1"), EWalkKind::Go, {P(-60.f, 6.f, 74.f)}, 0.f, 0.f},
			{TEXT("looking round gate 1"), EWalkKind::Look, {}, 10.f, 0.f},
			{TEXT("through the jetway door"), EWalkKind::Go, {P(-50.6f, 6.f, 83.6f)}, 0.f, 0.f},
			{TEXT("down the jetway corridor"), EWalkKind::Walk, {P(-50.6f, 5.4f, 88.f), P(-50.6f, 4.8f, 93.5f), P(-50.4f, 4.7f, 96.3f)}, 25.f, 0.f},
			{TEXT("along the jetway to its cab"), EWalkKind::Walk, Sleeve, 30.f, 0.f},
			{TEXT("looking round in the jetway's cab"), EWalkKind::Look, {}, 8.f, 0.f},
			{TEXT("back to the rotunda"), EWalkKind::Walk, SleeveBack, 30.f, 0.f},
			{TEXT("down the jetway's service stair"), EWalkKind::Walk, {P(-51.5f, 4.7f, 98.6f), P(-51.5f, 4.3f, 100.0f), P(-51.5f, 0.3f, 105.8f), P(-51.5f, 0.f, 107.6f)}, 30.f, 0.f},
			{TEXT("out onto the apron"), EWalkKind::Walk, {P(-47.f, 0.f, 115.f), P(-42.f, 0.f, 128.f)}, 30.f, 0.f},
			{TEXT("looking at the terminal from the apron"), EWalkKind::Face, {}, 8.f, 90.f},
			{TEXT("looking round on the apron"), EWalkKind::Look, {}, 12.f, 0.f},
		};
	}

	bool bWalkOn = false, bWalkQuit = false, bWalkStarted = false;
	float WalkWait = 15.f, WalkClock = 0.f, WalkPartT = 0.f, WalkStuckT = 0.f, WalkBestDist = 1e9f, WalkEndNoteT = 0.f;
	int32 WalkPart = -1, WalkPoint = 0, WalkSide = 1;
	TArray<FWalkPart> WalkParts;
	FString WalkEndNote;

	FVector WalkWorld(const FVector& PlanP) { return FTerminalPlan::ToWorld(PlanP.X, PlanP.Y, PlanP.Z); }

	void WalkSnap(UWorld* W, ACharacter* Ch, const FVector& PlanP, float Yaw)
	{
		FVector At = WalkWorld(PlanP);
		FHitResult H; FCollisionQueryParams Q(TEXT("StressSnap"), true, Ch);
		if (W->LineTraceSingleByChannel(H, At + FVector(0, 0, 200.f), At - FVector(0, 0, 600.f), ECC_Visibility, Q)) At.Z = H.ImpactPoint.Z;
		At.Z += Ch->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.f;
		Ch->TeleportTo(At, FRotator(0, Yaw, 0));
	}

	// a way ahead free of walls, tables and chairs: the desired way if clear, else the nearest turn either side that is
	bool WalkClear(UWorld* W, ACharacter* Ch, const FVector& Dir)
	{
		const float R = Ch->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.9f;
		const FVector From = Ch->GetActorLocation() + FVector(0, 0, 25.f);      // (above small lips and the steps' rise)
		FHitResult H; FCollisionQueryParams Q(TEXT("StressAhead"), false, Ch);
		if (!W->SweepSingleByChannel(H, From, From + Dir * 110.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(R, 45.f), Q)) return true;
		return H.ImpactNormal.Z > 0.6f;                                          // (a floor or a ramp ahead isn't in the way)
	}

	FVector WalkSteer(UWorld* W, ACharacter* Ch, const FVector& Want)
	{
		if (WalkClear(W, Ch, Want)) return Want;
		for (const float A : {30.f, 55.f, 80.f, 110.f})
			for (const int32 S : {WalkSide, -WalkSide})
			{
				const FVector D = Want.RotateAngleAxis(A * S, FVector::UpVector);
				if (WalkClear(W, Ch, D)) { WalkSide = S; return D; }
			}
		return Want;
	}
}

void ADPStressWalk::Begin(bool bQuitWhenDone)
{
	using namespace ADPStressWalkInternal;
	bWalkOn = true; bWalkQuit = bQuitWhenDone; bWalkStarted = false; WalkWait = 15.f; WalkClock = 0.f; WalkPart = -1; WalkEndNote.Reset();
	WalkParts = WalkRoute();
	ADP_LOG(TEXT("Stress walk: waiting for the world to load in fully, then a 15 s countdown"));
}

bool ADPStressWalk::Active() { return ADPStressWalkInternal::bWalkOn; }

FText ADPStressWalk::Note()
{
	using namespace ADPStressWalkInternal;
	if (bWalkOn && !bWalkStarted)
		return FText::FromString(FString::Printf(TEXT("STRESS WALK\nWaiting for assets to load in order to measure the best frame rates possible. Please wait.\n%d"), FMath::Max(1, FMath::CeilToInt(WalkWait))));
	if (!WalkEndNote.IsEmpty()) return FText::FromString(WalkEndNote);
	return FText::GetEmpty();
}

void ADPStressWalk::Tick(UWorld* W, APlayerController* PC, float Dt)
{
	using namespace ADPStressWalkInternal;
	if (WalkEndNoteT > 0.f) { WalkEndNoteT -= Dt; if (WalkEndNoteT <= 0.f) WalkEndNote.Reset(); }
	if (!bWalkOn || !W || !PC) return;
	ACharacter* Ch = Cast<ACharacter>(PC->GetPawn());
	if (!Ch) return;
	// the wait: the late world (roads, town, land) in, nothing still loading, then the countdown
	if (!bWalkStarted)
	{
		const bool bLoaded = ADPLateWorld::Done() && !IsAsyncLoading();
		if (bLoaded) WalkWait -= Dt;
		if (WalkWait <= 0.f) { bWalkStarted = true; WalkPart = 0; WalkPartT = 0.f; ADPPerfLog::BeginSection(TEXT("stress walk")); ADPPerfLog::SetAudit(ADPPerfLog::SceneAudit(W)); }
		return;
	}
	if (WalkPart >= WalkParts.Num())
	{
		bWalkOn = false;
		const FString Sum = ADPPerfLog::EndSection();
		ADP_LOG(TEXT("Stress walk: done - %s"), *Sum);
		WalkEndNote = TEXT("STRESS WALK FINISHED\n") + Sum + TEXT("\nEvery slow frame and where it happened: Saved/Logs/ADP-Perf.log"); WalkEndNoteT = 12.f;
		if (bWalkQuit) FGenericPlatformMisc::RequestExit(false);
		return;
	}
	const FWalkPart& Pt = WalkParts[WalkPart];
	if (WalkPartT == 0.f)
	{
		ADP_LOG(TEXT("Stress walk: %s"), Pt.What);
		ADPPerfLog::Mark(Pt.What);
		WalkPoint = 0; WalkStuckT = 0.f; WalkBestDist = 1e9f;
		if (Pt.Kind == EWalkKind::Go) WalkSnap(W, Ch, Pt.Points[0], PC->GetControlRotation().Yaw);
		if (Pt.Kind == EWalkKind::Escalator)
			for (TActorIterator<AEscalatorPair> It(W); It; ++It)
			{
				const FTransform ET = It->GetActorTransform(); const FVector Dir = ET.TransformVectorNoScale(FVector(0, -1, 0));
				Ch->TeleportTo(ET.TransformPosition(FVector(0, 350, 110)), Dir.Rotation()); PC->SetControlRotation(Dir.Rotation());
			}
	}
	WalkPartT += Dt;
	bool bDone = false;
	switch (Pt.Kind)
	{
	case EWalkKind::Go: bDone = WalkPartT > 0.5f; break;
	case EWalkKind::Look:
	{
		FRotator R = PC->GetControlRotation(); R.Yaw += 40.f * Dt; R.Pitch = 6.f * FMath::Sin(WalkPartT * 0.8f); PC->SetControlRotation(R);
		bDone = WalkPartT >= Pt.Seconds; break;
	}
	case EWalkKind::Face:
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), FRotator(2.f, Pt.Yaw, 0.f), Dt, 3.f));
		bDone = WalkPartT >= Pt.Seconds; break;
	case EWalkKind::Escalator:
		for (TActorIterator<AEscalatorPair> It(W); It; ++It) Ch->AddMovementInput(It->GetActorTransform().TransformVectorNoScale(FVector(0, -1, 0)), 0.6f);
		bDone = WalkPartT >= Pt.Seconds; break;
	case EWalkKind::Walk:
	{
		if (WalkPoint >= Pt.Points.Num()) { bDone = true; break; }
		const FVector T = WalkWorld(Pt.Points[WalkPoint]);
		FVector D = T - Ch->GetActorLocation(); D.Z = 0.f;
		const float Dist = D.Size();
		if (Dist < 45.f) { ++WalkPoint; WalkStuckT = 0.f; WalkBestDist = 1e9f; break; }
		const FVector Dir = WalkSteer(W, Ch, D / Dist);
		Ch->AddMovementInput(Dir, 1.f);
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), FRotator(0.f, Dir.Rotation().Yaw, 0.f), Dt, 5.f));
		// no nearer for 3 s: stuck - noted, and on to the next point
		if (Dist < WalkBestDist - 30.f) { WalkBestDist = Dist; WalkStuckT = 0.f; }
		else if ((WalkStuckT += Dt) > 3.f)
		{
			ADP_WARN(TEXT("Stress walk: stuck %.1f m from a point of \"%s\" - moved on to it"), Dist / 100.f, Pt.What);
			ADPPerfLog::Mark(FString::Printf(TEXT("(stuck %.1f m short, moved on)"), Dist / 100.f));
			WalkSnap(W, Ch, Pt.Points[WalkPoint], PC->GetControlRotation().Yaw); ++WalkPoint; WalkStuckT = 0.f; WalkBestDist = 1e9f;
		}
		if (WalkPartT > Pt.Seconds) bDone = true;
		break;
	}
	}
	if (bDone) { ++WalkPart; WalkPartT = 0.f; }
}
