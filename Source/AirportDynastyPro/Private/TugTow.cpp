// A hangar tow by the hangar tug and your hired crew (to-do 311, docs/Hired-Crew-Plan.md): the same calls, limits and talk as a
// pushback (Push, Release), with its own drive from its place by the bus chargers out to the aircraft and home again. HangarTow.cpp asks
// for it; your own tow (step C) is to go through the same calls.
#include "Tugs.h"
#include "StaffPeople.h"
#include "GSE.h"
#include "Sim/ADPVehicleArea.h"
#include "ADPLog.h"
#include "CrewPose.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
	// (the hangar tug's place and the vehicle area's lane and road, plan metres - GSESpare.cpp's DpLane/DpRoad; the tug's box
	//  in front of the terminal, Tugs.cpp's BoxX/BoxY; taxilane T, HangarTow.cpp)
	constexpr float HtLane = 273.5f, HtRoad = -245.f, HtR = 6.2f, HtK = 580.f, HtBoxX = -13.f, HtBoxY = 91.f, HtTY = 206.f;
	constexpr float HtNone = -100000.f;   // (at no stand: home in its place)
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
	if (I == INDEX_NONE)
	{
		const FTug* H = Tugs.FindByPredicate([](const FTug& U) { return U.bHangar; });
		Why = !H ? TEXT("there is no hangar tug here (it stands in the vehicle area)") : H->State == EState::You ? TEXT("you have the hangar tug - couple it at the nose, or leave it to its crew") : !HiredJob(Key) ? TEXT("a hangar tow needs your own hired crew") : TEXT("the hangar tug is busy");
		return false;
	}
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

void ATugs::HangarSetup()
{
	// one hangar tug, on place 11 of the vehicle area (kept for it: "fixed" in Data/vehicle-area.json), nose out to the lane
	if (Tugs.ContainsByPredicate([](const FTug& U) { return U.bHangar; })) return;
	const ADPVehicleArea::FPlace* P = ADPVehicleArea::Built() ? ADPVehicleArea::Place(TEXT("11")) : nullptr;
	if (!P) { ADP_LOG(TEXT("Tugs: no vehicle area here - no hangar tug, no hangar tows")); return; }
	FTug T; T.bHangar = true; T.StandX = HtNone; T.Home = T.At = Plan(P->X, P->Y); T.HomeYaw = T.Yaw = -P->Heading;
	if (!SpawnAt(T, TEXT("HangarTug"))) return;
	Pose(T); Tugs.Add(MoveTemp(T));
	ADP_LOG(TEXT("Tugs: the hangar tug %d stands on place %s of the vehicle area (%.1f, %.1f)"), Tugs.Num() - 1, *P->Name, P->X, P->Y);
}

int32 ATugs::StandTug(float StandX) const
{
	return Tugs.IndexOfByPredicate([StandX](const FTug& U) { return !U.bHangar && FMath::Abs(U.StandX - StandX) < 1.f; });
}

int32 ATugs::JobTug(int32 Tug) const
{
	const FTug& T = Tugs[Tug];
	if (T.bHangar || !T.Key.IsEmpty()) return Tug;
	for (int32 i = 0; i < Tugs.Num(); ++i) if (Tugs[i].bHangar && !Tugs[i].Key.IsEmpty() && FMath::Abs(Tugs[i].StandX - T.StandX) < 1.f) return i;
	return Tug;
}

void ATugs::HangarLegs(TArray<FVector>& Out, bool bOut) const
{
	// the vehicle area's own way (GSESpare.cpp HomeOut, SpareHome): out of the place forward to the lane, east along it,
	// south onto the depot road; home the same in reverse, ending in line with the place beyond the lane - then reversed in
	const FTug* H = Tugs.FindByPredicate([](const FTug& U) { return U.bHangar; });
	const AGSE* G = AGSE::Find(GetWorld()); if (!H || !G) return;
	const float x = float(H->Home.X / 100.), y = float(-H->Home.Y / 100.), r = HtR, k = HtK;
	if (bOut)
	{
		G->Leg(Out, x, y, 0.f, 1.f, x, HtLane - r, 0.f, 1.f, k);
		G->Leg(Out, x, HtLane - r, 0.f, 1.f, x + r + 0.4f, HtLane, 1.f, 0.f, k);
		G->Leg(Out, x + r + 0.4f, HtLane, 1.f, 0.f, HtRoad - r, HtLane, 1.f, 0.f, k);
		G->Leg(Out, HtRoad - r, HtLane, 1.f, 0.f, HtRoad, HtLane - (r + 2.f), 0.f, -1.f, k);
	}
	else
	{
		G->Leg(Out, HtRoad, HtLane - (r + 2.f), 0.f, 1.f, HtRoad - r, HtLane, -1.f, 0.f, k);
		G->Leg(Out, HtRoad - r, HtLane, -1.f, 0.f, x + r + 0.4f, HtLane, -1.f, 0.f, k);
		G->Leg(Out, x + r + 0.4f, HtLane, -1.f, 0.f, x, HtLane + r, 0.f, 1.f, k);
	}
}

bool ATugs::HangarOut(FTug& T, const FVector& F)
{
	// out of its place, down the depot road; to an aircraft at its stand round the apron's things to the stand tug's own way in
	// (AGSE::Apron: clear of the stands' lines, aircraft, tugs, ground power and vehicles), to one at the hangar along the taxiways
	const double Now = GetWorld()->GetTimeSeconds(); if (Now < T.HtRetry) return false;
	const AGSE* G = AGSE::Find(GetWorld()); if (!G) return false;
	TArray<FVector> P; HangarLegs(P, true);
	if (!P.Num()) return false;
	if (T.TowWay.Num())
	{
		P.Add(Plan(HtRoad, HtTY));
		for (int32 i = 1; i < T.TowWay.Num(); ++i) P.Add(T.TowWay[i]);
		// (to a cargo stand, HangarTow: along K to beside its nose, then across in front of it - the cargo building 15 m
		//  ahead leaves room for a 7 m run-in only)
		P.Add(T.HookAt + F * (CargoStand(T.StandX) ? 700.f : 1500.f)); P.Add(T.HookAt);
	}
	else if (RemoteStand(T.StandX))
	{
		// (a remote stand, to-do 311: round the apron's things by the bus road to 15 m ahead of the nose, then straight in)
		TArray<FVector> A;
		if (!G->Apron(A, P.Last(), T.HookAt + F * 1500.f, nullptr))
		{
			T.HtRetry = float(Now + 5.0);
			if (Now - T.TowSaid > 30.0) { T.TowSaid = Now; ADP_LOG(TEXT("Tugs: the hangar tug waits for %s - no clear way to the remote stand yet"), *T.Key); }
			return false;
		}
		P.Append(A); P.Add(T.HookAt);
	}
	else
	{
		TArray<FVector> A;
		if (!G->Apron(A, P.Last(), Plan(T.StandX + HtBoxX * 0.5f, HtBoxY + 12.5f), nullptr))
		{
			T.HtRetry = float(Now + 5.0);
			if (Now - T.TowSaid > 30.0) { T.TowSaid = Now; ADP_LOG(TEXT("Tugs: the hangar tug waits for %s - no clear way to the stand yet"), *T.Key); }
			return false;
		}
		P.Append(A); P.Add(Plan(T.StandX, HtBoxY + 18.f)); P.Add(T.Handler != INDEX_NONE ? T.HookAt + F * 150.f : T.HookAt);
	}
	T.Path = MoveTemp(P); T.HtRetry = 0.f;
	ADP_LOG(TEXT("Tugs: the hangar tug drives out of its place to %s (%d points)"), *T.Key, T.Path.Num());
	return true;
}

bool ATugs::HangarHome(FTug& T, const TArray<FVector>& TowHome)
{
	// home after a leg: from the hangar along the taxiways to the depot road, else round the apron's things to it; then into
	// its place as the vehicle area's own do (the Home tick reverses it in)
	const double Now = GetWorld()->GetTimeSeconds();
	const AGSE* G = AGSE::Find(GetWorld());
	TArray<FVector> P;
	const FVector Road = Plan(HtRoad, HtLane - (HtR + 2.f));
	if (TowHome.Num())
	{
		P = TowHome; if (P.Num() > 1) P.Pop();   // (not back to the stand's line: along taxilane T to the road)
		P.Add(Plan(HtRoad, HtTY)); P.Add(Road);
	}
	else if (!G || !G->Apron(P, T.At, Road, nullptr))
	{
		T.HtRetry = FMath::Max(0.01f, float(Now + 5.0));
		if (Now - T.TowSaid > 30.0) { T.TowSaid = Now; ADP_LOG(TEXT("Tugs: the hangar tug waits at x %.0f - no clear way home yet"), T.StandX); }
		return false;
	}
	HangarLegs(P, false);
	T.Path = MoveTemp(P); T.HtRetry = 0.f;
	ADP_LOG(TEXT("Tugs: the hangar tug drives home to its place (%d points)"), T.Path.Num());
	return true;
}
