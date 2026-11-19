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
#include "JetwayRigs.h"
#include "SunClock.h"
#include "BabylonModel.h"
#include "UObject/UObjectGlobals.h"

namespace ADPStressWalkInternal
{
	// one part of the walk; places in the terminal plan's metres (x east, h up, z north)
	enum class EWalkKind : uint8 { Go, Walk, Look, Face, Escalator, Night, JetwayRide, JetwayWatch };
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
			// the 16 gates (owner, 2026-11-17): a while in the tax free, to and into the A380 gate, both baggage claim areas
			{TEXT("at the tax free's way in"), EWalkKind::Go, {P(-95.f, 6.f, 58.f)}, 0.f, 0.f},
			{TEXT("into the tax free and along its promenade"), EWalkKind::Walk, {P(-95.f, 6.f, 48.f), P(-110.f, 6.f, 44.f), P(-130.f, 6.f, 42.f), P(-150.f, 6.f, 42.f)}, 45.f, 0.f},
			{TEXT("looking round in beauty and liquor"), EWalkKind::Look, {}, 12.f, 0.f},
			{TEXT("past the aurora plaza"), EWalkKind::Walk, {P(-160.f, 6.f, 42.f), P(-168.f, 6.f, 42.f), P(-178.f, 6.f, 42.f)}, 25.f, 0.f},
			{TEXT("looking round the aurora plaza"), EWalkKind::Look, {}, 12.f, 0.f},
			{TEXT("through the Iceland market to the pay lounge"), EWalkKind::Walk, {P(-195.f, 6.f, 42.f), P(-212.f, 6.f, 40.f), P(-222.f, 6.f, 34.f)}, 35.f, 0.f},
			{TEXT("looking round the luxury shops and the pay lounge"), EWalkKind::Look, {}, 12.f, 0.f},
			{TEXT("out of the tax free to gates 9-12"), EWalkKind::Walk, {P(-228.f, 6.f, 46.f), P(-228.f, 6.f, 58.f)}, 25.f, 0.f},
			{TEXT("in the gate walkway by gate 13"), EWalkKind::Go, {P(240.f, 6.f, 60.f)}, 0.f, 0.f},
			{TEXT("walking to and into the A380 gate"), EWalkKind::Walk, {P(262.f, 6.f, 60.f), P(267.4f, 6.f, 66.f), P(267.4f, 6.f, 76.f)}, 30.f, 0.f},
			{TEXT("looking round the A380 gate"), EWalkKind::Look, {}, 12.f, 0.f},
			{TEXT("looking out at the A380 stand"), EWalkKind::Face, {}, 6.f, -90.f},
			{TEXT("in the baggage claim hall"), EWalkKind::Go, {P(-75.f, 0.f, 20.f)}, 0.f, 0.f},
			{TEXT("walking the baggage claim hall past the belts"), EWalkKind::Walk, {P(-55.f, 0.f, 20.f), P(-30.f, 0.f, 20.f), P(-5.f, 0.f, 22.f)}, 40.f, 0.f},
			{TEXT("looking round the baggage claim"), EWalkKind::Look, {}, 12.f, 0.f},
			{TEXT("in the crew side's baggage hall"), EWalkKind::Go, {P(-5.f, 0.f, 60.f)}, 0.f, 0.f},
			{TEXT("walking the baggage hall past the belts' inputs"), EWalkKind::Walk, {P(-30.f, 0.f, 60.f), P(-60.f, 0.f, 60.f), P(-75.f, 0.f, 60.f)}, 35.f, 0.f},
			{TEXT("looking round the baggage hall"), EWalkKind::Look, {}, 10.f, 0.f},
			// the night part (owner, 2026-11-16: every stress walk - inside a jetway and outside it in the dark, the jetway
			// moving to dock, its lights inside and out, you carried with it, also while walking in it)
			{TEXT("night falls (21:00)"), EWalkKind::Night, {}, 4.f, 0.f},
			{TEXT("in the jetway's cab at night"), EWalkKind::Go, {P(-58.f, 3.1f, 109.f)}, 0.f, 0.f},
			{TEXT("the jetway pulls back with you inside, walking in it as it moves"), EWalkKind::JetwayRide, {}, 220.f, 0.f},
			{TEXT("out on the apron at night"), EWalkKind::Go, {P(-40.f, 0.f, 124.f)}, 0.f, 0.f},
			{TEXT("watching the jetway drive out and dock in the dark"), EWalkKind::JetwayWatch, {}, 220.f, 0.f},
		};
	}

	bool bWalkOn = false, bWalkQuit = false, bWalkStarted = false;
	float WalkWait = 15.f, WalkClock = 0.f, WalkPartT = 0.f, WalkStuckT = 0.f, WalkBestDist = 1e9f, WalkEndNoteT = 0.f;
	int32 WalkPart = -1, WalkPoint = 0, WalkSide = 1;
	TArray<FWalkPart> WalkParts;
	FString WalkEndNote;
	float WalkHourWas = -1.f; int32 WalkJet = INDEX_NONE; bool WalkToCab = false;

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

	FVector WalkSteerNear(UWorld* W, ACharacter* Ch, const FVector& Want)
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

	// ---- seeing the way (owner, 2026-11-19: "be much more aware of your surroundings ... you are constantly bumping into
	// things ... walk around stuff to avoid hitting them"; "use escalators, open doors, walk through doors without any
	// hiccups"): before each stretch the floor between here and the next point is looked at in small squares - is there
	// floor (no step higher than a stair's), is a body-sized space free there - and the shortest way round the tables,
	// chairs, pillars, belts and people is found (A*). Spread over frames (about a millisecond each) so it doesn't show in
	// the measured frame times; then walked with the eyes ahead along it, planned again if something steps into it.
	// Doors on the way: the automatic ones open as you come; a closed hinged door (or a jetway's service door) in front
	// of you is opened with E as you would.
	struct FSWGrid
	{
		FVector Origin = FVector::ZeroVector; int32 NX = 0, NY = 0; float Cell = 35.f;
		TArray<int8> Ok; TArray<float> FloorZ, G; TArray<int32> From; TArray<uint8> Closed;
		TArray<TPair<float, int32>> Open;
		int32 Start = -1; FVector GoalW = FVector::ZeroVector; bool bActive = false; int32 Expanded = 0;
	};
	FSWGrid SWG; TArray<FVector> SWPath; int32 SWPathI = 0, SWReplans = 0; float SWNoGainT = 0.f, SWBest = 1e9f, SWDirectT = 0.f, SWDoorWait = 0.f;

	bool SWCellFree(UWorld* W, ACharacter* Ch, int32 C, float RefZ)
	{
		if (SWG.Ok[C] >= 0) return SWG.Ok[C] == 1;
		const FVector P = SWG.Origin + FVector((C % SWG.NX) * SWG.Cell, (C / SWG.NX) * SWG.Cell, 0.f);
		FHitResult H; FCollisionQueryParams Q(TEXT("SWFloor"), false, Ch);
		bool bOk = false;
		if (W->LineTraceSingleByChannel(H, FVector(P.X, P.Y, RefZ + 160.f), FVector(P.X, P.Y, RefZ - 260.f), ECC_Visibility, Q) && H.ImpactNormal.Z > 0.6f)
		{
			SWG.FloorZ[C] = H.ImpactPoint.Z;
			// a body standing there, from a step's height up to head height (low lips and stair noses don't count)
			const float R = Ch->GetCapsuleComponent()->GetScaledCapsuleRadius() + 8.f;
			bOk = !W->OverlapAnyTestByChannel(FVector(P.X, P.Y, H.ImpactPoint.Z + 115.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(R, 75.f), Q);
		}
		SWG.Ok[C] = bOk ? 1 : 0; return bOk;
	}

	bool SWLess(const TPair<float, int32>& L, const TPair<float, int32>& R) { return L.Key < R.Key; }

	void SWBegin(ACharacter* Ch, const FVector& Goal)
	{
		const FVector A = Ch->GetActorLocation();
		const FVector Lo(FMath::Min(A.X, Goal.X) - 1000.f, FMath::Min(A.Y, Goal.Y) - 1000.f, 0.f), Hi(FMath::Max(A.X, Goal.X) + 1000.f, FMath::Max(A.Y, Goal.Y) + 1000.f, 0.f);
		SWG = FSWGrid(); SWG.Cell = FMath::Max(35.f, FMath::Max(Hi.X - Lo.X, Hi.Y - Lo.Y) / 240.f);
		SWG.NX = FMath::CeilToInt((Hi.X - Lo.X) / SWG.Cell) + 1; SWG.NY = FMath::CeilToInt((Hi.Y - Lo.Y) / SWG.Cell) + 1; SWG.Origin = Lo;
		const int32 N = SWG.NX * SWG.NY;
		SWG.Ok.Init(-1, N); SWG.FloorZ.Init(0.f, N); SWG.G.Init(1e30f, N); SWG.From.Init(-1, N); SWG.Closed.Init(0, N);
		const int32 sx = FMath::Clamp(FMath::RoundToInt((A.X - Lo.X) / SWG.Cell), 0, SWG.NX - 1), sy = FMath::Clamp(FMath::RoundToInt((A.Y - Lo.Y) / SWG.Cell), 0, SWG.NY - 1);
		SWG.Start = sy * SWG.NX + sx; SWG.GoalW = Goal; SWG.bActive = true;
		SWG.Ok[SWG.Start] = 1; SWG.FloorZ[SWG.Start] = A.Z - Ch->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); SWG.G[SWG.Start] = 0.f;
		SWG.Open.Reset(); SWG.Open.HeapPush(TPair<float, int32>(0.f, SWG.Start), SWLess);
		SWPath.Reset(); SWPathI = 0;
	}

	FVector SWCellW(int32 C) { return SWG.Origin + FVector((C % SWG.NX) * SWG.Cell, (C / SWG.NX) * SWG.Cell, SWG.FloorZ[C]); }

	// one frame's share of the search; true when it has ended (SWPath filled, or empty: no way found)
	bool SWStep(UWorld* W, ACharacter* Ch)
	{
		if (!SWG.bActive) return true;
		const double Until = FPlatformTime::Seconds() + 0.0012;
		const FVector2D Gl(SWG.GoalW.X, SWG.GoalW.Y);
		while (SWG.Open.Num() && FPlatformTime::Seconds() < Until)
		{
			TPair<float, int32> Top; SWG.Open.HeapPop(Top, SWLess);
			const int32 C = Top.Value; if (SWG.Closed[C]) continue; SWG.Closed[C] = 1;
			const FVector Cw = SWCellW(C);
			if (FVector2D::Distance(FVector2D(Cw.X, Cw.Y), Gl) < FMath::Max(60.f, SWG.Cell * 1.2f))
			{
				TArray<FVector> Rev; for (int32 K = C; K != -1; K = SWG.From[K]) Rev.Add(SWCellW(K));
				for (int32 i = Rev.Num() - 1; i >= 0; --i) SWPath.Add(Rev[i]);
				SWPath.Add(FVector(SWG.GoalW.X, SWG.GoalW.Y, Cw.Z));
				SWG.bActive = false; return true;
			}
			if (++SWG.Expanded > 40000) break;
			const int32 x = C % SWG.NX, y = C / SWG.NX;
			for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx)
			{
				if (!dx && !dy) continue;
				const int32 nx = x + dx, ny = y + dy; if (nx < 0 || ny < 0 || nx >= SWG.NX || ny >= SWG.NY) continue;
				const int32 Nb = ny * SWG.NX + nx; if (SWG.Closed[Nb]) continue;
				if (!SWCellFree(W, Ch, Nb, SWG.FloorZ[C])) continue;
				if (FMath::Abs(SWG.FloorZ[Nb] - SWG.FloorZ[C]) > 45.f) continue;
				// (no cutting a corner past a blocked square)
				if (dx && dy && (!SWCellFree(W, Ch, y * SWG.NX + nx, SWG.FloorZ[C]) || !SWCellFree(W, Ch, ny * SWG.NX + x, SWG.FloorZ[C]))) continue;
				const float Ng = SWG.G[C] + SWG.Cell * ((dx && dy) ? 1.4142f : 1.f);
				if (Ng >= SWG.G[Nb]) continue;
				SWG.G[Nb] = Ng; SWG.From[Nb] = C;
				const FVector Nw = SWCellW(Nb);
				SWG.Open.HeapPush(TPair<float, int32>(Ng + FVector2D::Distance(FVector2D(Nw.X, Nw.Y), Gl), Nb), SWLess);
			}
		}
		if (SWG.Open.Num() && SWG.Expanded <= 40000) return false;
		SWG.bActive = false; SWPath.Reset(); return true;   // (no way found)
	}

	// the next point to walk to on the way: the farthest one still in plain, free sight (a body-sized sweep)
	void SWSmooth(UWorld* W, ACharacter* Ch)
	{
		if (SWPathI >= SWPath.Num()) return;
		const FVector Me = Ch->GetActorLocation();
		const float FootZ = Me.Z - Ch->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float R = Ch->GetCapsuleComponent()->GetScaledCapsuleRadius();
		FCollisionQueryParams Q(TEXT("SWSight"), false, Ch);
		for (int32 j = FMath::Min(SWPath.Num() - 1, SWPathI + 30); j > SWPathI; --j)
		{
			if (FMath::Abs(SWPath[j].Z - FootZ) > 60.f) continue;   // (over a stair: square by square)
			FHitResult H;
			const FVector A(Me.X, Me.Y, FootZ + 115.f), B(SWPath[j].X, SWPath[j].Y, SWPath[j].Z + 115.f);
			if (!W->SweepSingleByChannel(H, A, B, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(R + 4.f, 75.f), Q)) { SWPathI = j; return; }
		}
	}

	// a closed door in front of you: opened with E as you would (a hinged door, a jetway's service door). True while
	// waiting for it to swing open.
	bool SWDoors(UWorld* W, ACharacter* Ch, const FVector& Facing, float Dt)
	{
		if (SWDoorWait > 0.f) { SWDoorWait -= Dt; return true; }
		const FVector Feet = Ch->GetActorLocation() - FVector(0, 0, Ch->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		bool bOpen = true;
		if (ABabylonModel::DoorNear(Feet, Facing, bOpen) && !bOpen && ABabylonModel::UseDoor(Ch)) { ADP_LOG(TEXT("Stress walk: opening a door (E)")); SWDoorWait = 1.3f; return true; }
		if (AJetwayRigs* JR = AJetwayRigs::Get(W))
			if (JR->ServiceDoorNear(Feet, Facing, bOpen) && !bOpen && JR->UseServiceDoor(Feet, Facing)) { ADP_LOG(TEXT("Stress walk: opening a jetway's service door (E)")); SWDoorWait = 1.4f; return true; }
		return false;
	}

	// walking to T round everything in the way; true once there
	bool SWWalkTo(UWorld* W, ACharacter* Ch, APlayerController* PC, const FVector& T, float Dt, const TCHAR* What)
	{
		FVector D = T - Ch->GetActorLocation(); D.Z = 0.f;
		const float Dist = D.Size();
		if (Dist < 45.f) { SWPath.Reset(); SWG.bActive = false; SWReplans = 0; SWDirectT = 0.f; return true; }
		if (SWDoors(W, Ch, SWPath.Num() ? (SWPath[FMath::Min(SWPathI, SWPath.Num() - 1)] - Ch->GetActorLocation()).GetSafeNormal2D() : D / Dist, Dt)) return false;
		// no way seen (often a closed door between): straight on for a moment, steering round what's near, opening doors
		if (SWDirectT > 0.f)
		{
			SWDirectT -= Dt;
			const FVector Dir = WalkSteerNear(W, Ch, D / Dist);
			Ch->AddMovementInput(Dir, 0.8f);
			PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), FRotator(0.f, Dir.Rotation().Yaw, 0.f), Dt, 3.f));
			return false;
		}
		if (!SWG.bActive && SWPath.Num() == 0)
		{
			if (SWReplans > 4) return false;   // (the caller moves you on)
			SWBegin(Ch, T); ++SWReplans; SWNoGainT = 0.f; SWBest = 1e9f;
		}
		if (SWG.bActive)
		{
			// planning: a few frames, standing and looking that way
			if (SWStep(W, Ch))
			{
				if (SWPath.Num()) ADP_LOG(TEXT("Stress walk: way planned (%d squares of %.0f cm, %d looked at)"), SWPath.Num(), SWG.Cell, SWG.Expanded);
				else { ADP_LOG(TEXT("Stress walk: no clear way seen yet to the next point of \"%s\" (try %d) - on towards it"), What, SWReplans); SWDirectT = 3.f; }
			}
			PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), FRotator(0.f, D.Rotation().Yaw, 0.f), Dt, 2.f));
			return false;
		}
		// walking the way: the farthest square in free sight next, the eyes on the way about 3 m ahead
		SWSmooth(W, Ch);
		const FVector Me = Ch->GetActorLocation();
		FVector To = SWPath[FMath::Min(SWPathI, SWPath.Num() - 1)] - Me; To.Z = 0.f;
		if (To.Size() < 40.f && SWPathI < SWPath.Num() - 1) { ++SWPathI; return false; }
		FVector Look = To; float Acc = 0.f;
		for (int32 j = SWPathI; j < SWPath.Num() && Acc < 300.f; ++j) { Look = SWPath[j] - Me; Look.Z = 0.f; Acc = Look.Size(); }
		Ch->AddMovementInput(To.GetSafeNormal(), 1.f);
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), FRotator(0.f, Look.Rotation().Yaw, 0.f), Dt, 3.f));
		// something stepped into the way (a person, a door): no nearer for 1.5 s - look again and plan anew
		const float Left = To.Size();
		if (Left < SWBest - 15.f) { SWBest = Left; SWNoGainT = 0.f; }
		else if ((SWNoGainT += Dt) > 1.5f) { ADP_LOG(TEXT("Stress walk: the way is blocked - planning again")); SWPath.Reset(); SWBest = 1e9f; SWNoGainT = 0.f; }
		return false;
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
	ADP_PERF_SCOPE("the stress walk (each frame)", 2.f);
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
		ASunClock::HourOverride = WalkHourWas; ASunClock::Refresh();   // (the night part over: the clock as it was)
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
		SWPath.Reset(); SWG.bActive = false; SWReplans = 0; SWDirectT = 0.f; SWDoorWait = 0.f; WalkToCab = false;
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
	{
		// walked to its foot (round everything), then on: stepping on and riding it up, a gentle walk on the steps
		AEscalatorPair* E = nullptr; float Bd = 1e12f;
		for (TActorIterator<AEscalatorPair> It(W); It; ++It) { const float Dd = FVector::DistSquared(It->GetActorLocation(), Ch->GetActorLocation()); if (Dd < Bd) { Bd = Dd; E = *It; } }
		if (!E) { bDone = true; break; }
		const FTransform ET = E->GetActorTransform(); const FVector Up = ET.TransformVectorNoScale(FVector(0, -1, 0));
		if (!WalkToCab)
		{
			if (SWWalkTo(W, Ch, PC, ET.TransformPosition(FVector(0, 350, 0)), Dt, Pt.What) || WalkPartT > 40.f) { WalkToCab = true; WalkPartT = 0.01f; ADP_LOG(TEXT("Stress walk: stepping onto the escalator")); }
			break;
		}
		Ch->AddMovementInput(Up, 0.6f);
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), FRotator(0.f, Up.Rotation().Yaw, 0.f), Dt, 3.f));
		bDone = WalkPartT >= Pt.Seconds; if (bDone) WalkToCab = false;
		break;
	}
	case EWalkKind::Night:
		if (WalkPartT <= Dt) { WalkHourWas = ASunClock::HourOverride; ASunClock::HourOverride = 21.f; ASunClock::Refresh(); }
		bDone = WalkPartT >= Pt.Seconds; break;
	case EWalkKind::JetwayRide:
	case EWalkKind::JetwayWatch:
	{
		AJetwayRigs* JR = AJetwayRigs::Get(W);
		if (!JR) { bDone = true; break; }
		if (WalkPartT <= Dt)
		{
			WalkJet = JR->Nearest(Ch->GetActorLocation(), 6000.f);
			if (WalkJet != INDEX_NONE) { if (JR->State(WalkJet) == AJetwayRigs::EState::Docked) JR->Undock(WalkJet); else JR->Dock(WalkJet); }
			WalkToCab = false;
			ADP_LOG(TEXT("Stress walk: jetway %s %s"), WalkJet != INDEX_NONE ? *JR->Name(WalkJet) : TEXT("none"), Pt.Kind == EWalkKind::JetwayRide ? TEXT("moving with you inside") : TEXT("docking, watched from outside"));
		}
		if (WalkJet == INDEX_NONE) { bDone = true; break; }
		FVector Ro, Dr, Mid, Fr;
		if (JR->Frame(WalkJet, Ro, Dr) && JR->CabView(WalkJet, Mid, Fr))
		{
			if (Pt.Kind == EWalkKind::JetwayRide)
			{
				// walking up and down the moving jetway: to the rotunda's end, back to the cab, again
				const FVector Tgt = WalkToCab ? Mid : Ro + (Mid - Ro) * 0.15f;
				FVector D = Tgt - Ch->GetActorLocation(); D.Z = 0.f;
				if (D.Size() < 150.f) WalkToCab = !WalkToCab;
				else { Ch->AddMovementInput(D.GetSafeNormal(), 0.7f); PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), FRotator(0.f, D.Rotation().Yaw, 0.f), Dt, 4.f)); }
			}
			else PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), (Mid - Ch->GetActorLocation()).Rotation(), Dt, 3.f));   // (watching the cab)
		}
		const AJetwayRigs::EState St = JR->State(WalkJet);
		bDone = (WalkPartT > 8.f && (St == AJetwayRigs::EState::Docked || St == AJetwayRigs::EState::Parked)) || WalkPartT >= Pt.Seconds;
		break;
	}
	case EWalkKind::Walk:
	{
		if (WalkPoint >= Pt.Points.Num()) { bDone = true; break; }
		if (SWWalkTo(W, Ch, PC, WalkWorld(Pt.Points[WalkPoint]), Dt, Pt.What)) { ++WalkPoint; break; }
		if (SWReplans > 4 && !SWG.bActive && SWPath.Num() == 0 && SWDirectT <= 0.f)
		{
			ADP_WARN(TEXT("Stress walk: no way found to a point of \"%s\" - moved on to it"), Pt.What);
			ADPPerfLog::Mark(TEXT("(no way found, moved on)"));
			WalkSnap(W, Ch, Pt.Points[WalkPoint], PC->GetControlRotation().Yaw); ++WalkPoint; SWReplans = 0;
		}
		if (WalkPartT > Pt.Seconds) bDone = true;
		break;
	}
	}
	if (bDone) { ++WalkPart; WalkPartT = 0.f; }
}
