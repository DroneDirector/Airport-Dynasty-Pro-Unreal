// A hangar tow by the stand's own tug and crew (to-do 311, docs/Hired-Crew-Plan.md): the same calls, limits and talk as a
// pushback (Push, Release), with its own drive out along the taxiways to the aircraft and home again. HangarTow.cpp asks
// for it; your own tow (step C) is to go through the same calls.
#include "Tugs.h"
#include "StaffPeople.h"
#include "ADPLog.h"
#include "CrewPose.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
	ATugs::FCrewAct TwWalk(const FVector& To, float Stop = 20.f) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Walk; A.To = [To] { return To; }; A.Stop = Stop; A.Max = 150.f; return A; }
	ATugs::FCrewAct TwDo(TFunction<void()> F) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Do; A.Fn = MoveTemp(F); return A; }
	// down on the haunches at a chock, both hands on it (K 0..1 down and back up again)
	ATugs::FCrewAct TwCrouch(const FVector& At, const FVector& Face, const FVector& Chock, float Dur)
	{
		ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Pose; A.Dur = Dur;
		A.PoseFn = [At, Face, Chock](float K, float PH, FDriverPose& P)
		{
			const FDriverPose S = ADPPose::Standing(At, Face, PH); FDriverPose D = S; const FVector L = ADPPose::LeftOf(S.Facing);
			D.Pelvis = At + FVector::UpVector * (PH * 0.58f) - S.Facing * 13.f; D.Lean = 14.f;
			D.Foot[0] = At + L * 19.f + S.Facing * 4.f; D.Foot[1] = At - L * 19.f + S.Facing * 4.f;
			for (int32 h = 0; h < 2; ++h) { D.bHand[h] = true; D.Hand[h] = Chock + FVector::UpVector * 14.f + L * (h ? -9.f : 9.f); D.Grip[h] = 0.9f; }
			P = ADPPose::Lerp(S, D, FMath::Sin(PI * FMath::Clamp(K, 0.f, 1.f)));
		};
		return A;
	}
}

bool ATugs::TowPlan(const FString& Key, float StandX, const TArray<FVector2f>& Way, const TArray<FVector2f>& Back, const FString& Ask, const FString& Ok, FString& Why)
{
	Tows.Add(Key);
	const int32 I = Assign(Key, StandX);
	if (I == INDEX_NONE) { Why = TEXT("no tug or tow crew is free at that stand"); return false; }
	FTug& T = Tugs[I];
	if (!T.bTow)
	{
		T.bTow = true; T.TowAsk = Ask; T.TowOk = Ok; T.TowWay.Reset(); T.TowBack.Reset();
		for (const FVector2f& P : Way) T.TowWay.Add(Plan(P.X, P.Y));
		for (const FVector2f& P : Back) T.TowBack.Add(Plan(P.X, P.Y));
		ADP_LOG(TEXT("Tugs: tug %d takes the hangar tow %s (hired crew %d, %d way points)"), I, *Key, T.bHired, Way.Num());
	}
	if (T.bHired && T.State == EState::Parked)
	{
		const UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
		const FString M = Sp ? Sp->WaitsFor(TEXT("push:") + Key) : FString();
		Why = M.IsEmpty() ? FString(TEXT("its tow crew is being called")) : TEXT("the tow waits: ") + M;
	}
	return true;
}

bool ATugs::TowHooked(const FString& Key) const
{
	for (const FTug& T : Tugs) if (T.Key == Key) return AtNose(T);
	return false;
}

float ATugs::TowWalkerLag(const FString& Key) const
{
	// (the places as CrewWalkers: 2 m off each wingtip, level with it, and behind the tail)
	for (const FTug& T : Tugs)
	{
		if (T.Key != Key) continue;
		if (!T.bWalkersOut) return 0.f;
		const FVector F = T.Fwd, L = ADPPose::LeftOf(F), G = T.Fit - F * 60.f; float Worst = 0.f;
		for (int32 r = 0; r < T.Walkers.Num(); ++r)
		{
			if (!Crew.IsValidIndex(T.Walkers[r]) || !Crew[T.Walkers[r]].A.IsValid() || Crew[T.Walkers[r]].Q.Num()) continue;   // (still walking out: the push waits for him anyway)
			const FVector P = r == 0 ? G - F * 1200.f + L * 1990.f : r == 1 ? G - F * 1200.f - L * 1990.f : G - F * 3900.f;
			Worst = FMath::Max(Worst, float(FVector::Dist2D(Crew[T.Walkers[r]].A->GetActorLocation(), P)) / 100.f);
		}
		return Worst;
	}
	return 0.f;
}

UStaticMesh* ATugs::ChockMesh() const { return ChkMesh.Num() ? ChkMesh[0]->GetStaticMesh() : nullptr; }

bool ATugs::TowChocks(const FString& Key, bool bOn, const FVector& Store, const FVector Spots[4], const FVector& Fwd, TFunction<void(int32, bool)> Each)
{
	// a wing walker of the tow (else its handler) sets the hangar's own chocks fore and aft of the right nose wheel and the
	// right main gear's outer wheel once the brakes are set - or takes them away before the tow back (Spots: the four chocks)
	for (int32 I = 0; I < Tugs.Num(); ++I)
	{
		FTug& T = Tugs[I];
		if (T.Key != Key) continue;
		int32 Ci = INDEX_NONE; int32 At = 0;
		for (const int32 w : T.Walkers) if (Crew.IsValidIndex(w) && Crew[w].A.IsValid()) { Ci = w; break; }
		if (Ci == INDEX_NONE && T.Handler != INDEX_NONE && Crew.IsValidIndex(T.Handler)) { Ci = T.Handler; At = FMath::Min(1, Crew[Ci].Q.Num()); }   // (after his "brakes set" wait)
		if (Ci == INDEX_NONE) return false;
		FCrew& C = Crew[Ci];
		const FVector R = -ADPPose::LeftOf(Fwd);
		TArray<FCrewAct> Acts;
		if (bOn) Acts.Add(TwWalk(Store, 40.f));
		for (int32 p = 0; p < 2; ++p)
		{
			const FVector Mid = (Spots[p * 2] + Spots[p * 2 + 1]) * 0.5f, Stand = Mid + R * 70.f;   // (beside the wheel, outboard)
			Acts.Add(TwWalk(Stand, 15.f));
			for (int32 s = 0; s < 2; ++s) Acts.Add(TwCrouch(Stand + (Spots[p * 2 + s] - Mid) * 0.6f, -R, Spots[p * 2 + s], 2.4f));
			Acts.Add(TwDo([Each, p, bOn] { if (Each) Each(p, bOn); }));
		}
		if (!bOn) { Acts.Add(TwWalk(Store, 40.f)); Acts.Add(TwDo([Each] { if (Each) Each(-1, false); })); }
		if (At == 0) C.T = 0.f;
		C.Q.Insert(Acts, At);
		ADP_LOG(TEXT("Tugs: %s %s the chocks of %s in the hangar"), *C.Name, bOn ? TEXT("sets") : TEXT("takes away"), *Key);
		return true;
	}
	return false;
}
