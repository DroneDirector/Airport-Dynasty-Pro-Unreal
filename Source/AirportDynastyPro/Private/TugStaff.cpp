// The tugs and your hired people (to-do 206 part 2a, docs/Hired-Crew-Plan.md; StaffPeople.h): with adp.HiredCrew on, your own
// airline's flights are pushed back and put on ground power by the people you have hired - claimed when the job is got
// ready, walking over along the terminal's front, working with the same acts and IK as the stand-ins (TugCrew.cpp), and
// given back when they are done. Nobody free: the job waits in the queue, the earliest departure first; the tug stays in
// its box (never by itself, never half a crew) and the flight is late by the time it waited (CrewReady, asked by the
// flight's own readiness). The other airlines' flights keep the stand-ins.
#include "Tugs.h"
#include "StaffPeople.h"
#include "GSE.h"
#include "People.h"
#include "TruckDriverRig.h"
#include "BabylonModel.h"
#include "ADPLog.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSchedule.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

bool ATugs::HiredLive(const FString& Key, FDateTime* When, FString* Flight) const
{
	const UADPSim* Sim = UADPSim::Get(GetWorld()); if (!Sim) return false;
	if (const FADPPlanned* P = Sim->World.Planned.FindByPredicate([&](const FADPPlanned& X) { return X.Id == Key && X.State == TEXT("planned"); }))
	{ if (When) *When = P->Est; if (Flight) *Flight = P->FlightNo; return true; }
	if (const FADPFlight* F = Sim->World.Flights.FindByPredicate([&](const FADPFlight& X) { return X.Id == Key; }))
	{ if (When) *When = F->DepAt; if (Flight) *Flight = F->FlightNo; return true; }
	return false;
}

bool ATugs::HiredJob(const FString& Key) const
{
	const UADPSim* Sim = UADPSim::Get(GetWorld());
	const UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
	if (!Sim || !Sp || !Sp->Hires()) return false;   // (too few hired for any job: the stand-ins)
	for (const FADPPlanned& P : Sim->World.Planned) if (P.Id == Key) return P.AirlineId == UADPSim::Me();
	for (const FADPFlight& F : Sim->World.Flights) if (F.Id == Key) return F.AirlineId == UADPSim::Me();
	return false;
}

int32 ATugs::Enlist(AActor* A, const FString& StaffId, const FString& Label)
{
	int32 Ci = Crew.IndexOfByPredicate([](const FCrew& C) { return C.bSlotFree; });
	// (the list never moves: the acts hold references into it)
	if (Ci == INDEX_NONE && Crew.Num() < Crew.Max()) Ci = Crew.AddDefaulted();
	UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
	if (Ci == INDEX_NONE || !A) { if (Sp) Sp->Release(StaffId); ADP_WARN(TEXT("Tugs: no place for one more %s"), *Label); return INDEX_NONE; }
	FCrew& C = Crew[Ci]; C = FCrew();
	C.A = A; C.bHired = true; C.StaffId = StaffId; C.Name = Sp ? Sp->NameOf(StaffId) : StaffId;
	C.Spot = C.Anchor = A->GetActorLocation(); C.AnchorYaw = C.SpotYaw = A->GetActorRotation().Yaw + 90.f; C.IdleT = FMath::FRandRange(2.f, 9.f);
	return Ci;
}

void ATugs::LeaveAct(FCrew& C)
{
	FCrewAct A; A.Kind = FCrewAct::Do; A.Fn = [&C] { C.bLeave = true; }; C.Q.Add(A);   // (Tick: Discharge once the act is over)
}

void ATugs::Discharge(int32 Ci)
{
	FCrew& C = Crew[Ci];
	if (!C.bHired) { C.bLeave = false; return; }
	C.FidgetDur = 0.f; C.Hold = nullptr; C.Follow = nullptr; Unpose(C);
	for (FTug& T : Tugs)
	{
		if (T.Driver == Ci) T.Driver = INDEX_NONE;
		if (T.Handler == Ci) T.Handler = INDEX_NONE;
		if (T.GpuCi == Ci) T.GpuCi = INDEX_NONE;
		if (T.Gpu.Holder == Ci && !T.Gpu.bInHand) T.Gpu.Holder = INDEX_NONE;
		T.Walkers.Remove(Ci);
		if (T.Ck.Ci == Ci) T.Ck.Ci = INDEX_NONE;
	}
	const FString Id = C.StaffId;
	if (C.Rig) delete C.Rig;
	if (C.Rider) C.Rider->DestroyComponent();
	C = FCrew(); C.bSlotFree = true;
	if (UStaffPeople* Sp = UStaffPeople::Get(GetWorld())) Sp->Release(Id);
}

void ATugs::SafeWay(FCrew& C, float StandX)
{
	// from wherever they are on the apron to the gate's crew side (west of the tug's box, away from the jetway): a far way
	// goes along the terminal's front, under the jetways' fixed corridors (UStaffPeople::AlongWall)
	if (!C.A.IsValid()) return;
	const FVector Me = C.A->GetActorLocation();
	auto Walk = [&](float X, float Z, float Stop) { FCrewAct A; A.Kind = FCrewAct::Walk; const FVector P = Plan(X, Z); A.To = [P] { return P; }; A.Stop = Stop; A.Max = 160.f; C.Q.Add(A); };
	const float X = float(Me.X / 100.0), To = StandX - 22.f;
	if (FMath::Abs(X - To) < 12.f) return;   // (there already: the job's own walks)
	Walk(X, 85.45f, 35.f);
	Walk(To, 85.45f, 35.f);
	Walk(To, 90.f, 60.f);
}

bool ATugs::PersonAhead(const FVector& Here, const FVector& Dir, float Reach) const
{
	const FVector At = Here + Dir * Reach;
	if (const AADPPeople* P = AADPPeople::Get(GetWorld())) if (P->AnyoneNear(At, 36.f)) return true;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	return Me && FMath::Abs(Me->GetActorLocation().Z - At.Z) < 200.f && FVector::DistSquared2D(Me->GetActorLocation(), At) < FMath::Square(55.f);
}

void ATugs::HiredEnd(FTug& T)
{
	T.bHired = false; T.Driver = T.StandInDriver; T.Handler = T.StandInHandler;
}

void ATugs::HiredTick(int32 Tug, float Dt)
{
	FTug& T = Tugs[Tug];
	if ((T.CrewAsk -= Dt) > 0.f) return;
	T.CrewAsk = 2.f;
	UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); if (!Sp) return;
	if (T.Key.IsEmpty())
	{
		// (the job is over - the tug in its box, its people gone, the aircraft cleared to taxi: the gate's own again)
		if (T.State == EState::Parked && HiredFree(T) && (T.Seq == 0 || T.Seq >= 14)) HiredEnd(T);
		return;
	}
	FDateTime When; FString Flight;
	const bool bLive = HiredLive(T.Key, &When, &Flight);
	if (T.State == EState::Parked)
	{
		if (!bLive || !Sp->Hires())
		{
			ADP_LOG(TEXT("Tugs: the pushback of %s at x %.0f is off before its crew came"), *T.Key, T.StandX);
			T.Key.Reset(); return;
		}
		if (T.GpuCi != INDEX_NONE) return;   // (one of your people is still at its ground power)
		TArray<UStaffPeople::FTaken> Team;
		if (!Sp->AskTeam(TEXT("push:") + T.Key, EStaffTask::Pushback, When, T.At, FString::Printf(TEXT("%s pushback"), *Flight), Team)) return;
		for (const UStaffPeople::FTaken& K : Team)
		{
			const int32 Ci = Enlist(K.A.Get(), K.Id, UStaffPeople::DutyName(K.Duty));
			if (K.Duty == EStaffDuty::TugDriver) T.Driver = Ci; else T.Handler = Ci;
		}
		if (T.Driver == INDEX_NONE || T.Handler == INDEX_NONE)
		{
			// (no room in the list - never half a crew)
			for (const int32 Ci : {T.Driver, T.Handler}) if (Ci != INDEX_NONE) Discharge(Ci);
			return;
		}
		ADP_LOG(TEXT("Tugs: %s at x %.0f - %s drives the tug, %s is the pushback handler"), *Flight, T.StandX, *Crew[T.Driver].Name, *Crew[T.Handler].Name);
		Crew[T.Driver].bDriver = true;
		SafeWay(Crew[T.Driver], T.StandX); SafeWay(Crew[T.Handler], T.StandX);
		T.State = EState::Boarding; CrewBoard(Tug);
		return;
	}
	// (the flight was called off with the crew already here, the tug still in its box: out of it again, everyone free)
	if (T.State == EState::Waiting && !T.bPushReq && !bLive && T.Driver != INDEX_NONE)
	{
		ADP_LOG(TEXT("Tugs: the pushback of %s at x %.0f is off - its crew stands down"), *T.Key, T.StandX);
		T.Key.Reset(); T.State = EState::Parked; Engine(T, false);
		CrewParked(Tug);
		if (T.Handler != INDEX_NONE) LeaveAct(Crew[T.Handler]);   // (after what he is at: the ground power comes off first)
		for (const int32 W : T.Walkers) LeaveAct(Crew[W]);
		T.WalkersReady = 0; T.bWalkersOut = false;
		return;
	}
	// (hooked on, the talk begun: who walks the wings - fewer than three is said once)
	if (T.State == EState::Hooked && T.Walkers.Num() < 3 && T.Seq > 0)
	{
		static TSet<FString> Said;
		if (!Said.Contains(T.Key)) { Said.Add(T.Key); ADP_LOG(TEXT("Tugs: %s is pushed back with %d of 3 wing / tail walkers - no more of your ground crew free"), *Flight, T.Walkers.Num()); }
	}
}

void ATugs::LostTick(int32 Ci, float Dt)
{
	// one of your people vanished at work (their body was taken away; StaffPeople brings them back on duty by the door): the
	// job's acts stay in this slot, the next free person walks over and goes on with them - first in the queue
	FCrew& C = Crew[Ci];
	AGSE* G = AGSE::Find(GetWorld());
	const TCHAR* Job = C.bGse ? TEXT("the bags") : TEXT("the tug and the ground power");
	if (!C.bLost)
	{
		C.bLost = true; C.LostAsk = C.LostT = 0.f; C.bRelief = false;
		if (C.Hold) C.LostHold = C.Hold;
		if (C.Rig) delete C.Rig;
		C.Rig = nullptr; C.Rider = nullptr; C.Anim = nullptr; C.bPosed = false; C.FidgetDur = 0.f; C.Hold = nullptr; C.Speed = 0.f;
		ADP_WARN(TEXT("Tugs: %s's body is gone at work (%s)"), *C.Name, Job);
	}
	bool bDrop = false;
	const bool bGse = G && G->Relieve(Ci, bDrop);
	// (done anyway, or a second pair of hands: the job goes on without them)
	if (bDrop || C.bLeave || (!C.Q.Num() && !C.LostHold && !C.Follow))
	{
		ADP_LOG(TEXT("Tugs: %s's part of the job (%s) is let go - it goes on without"), *C.Name, Job);
		C.Q.Reset(); Discharge(Ci); return;
	}
	if ((C.LostT += Dt) > 600.f) { C.LostT = 0.f; ADP_WARN(TEXT("Tugs: nobody free to take over from %s (%s) for ten minutes"), *C.Name, Job); }
	if ((C.LostAsk -= Dt) > 0.f) return;
	C.LostAsk = 2.f;
	UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); if (!Sp) return;
	TArray<UStaffPeople::FTaken> Team;
	if (!Sp->AskTeam(FString::Printf(TEXT("relief:%d"), Ci), C.bRelClean ? EStaffTask::CleanRelief : EStaffTask::Relief, FDateTime(2000, 1, 1), C.LastAt, FString::Printf(TEXT("taking over from %s"), *C.Name), Team) || !Team.Num() || !Team[0].A.IsValid()) return;
	const FString Was = C.Name;
	C.A = Team[0].A; C.StaffId = Team[0].Id; C.Name = Sp->NameOf(C.StaffId); C.bLost = false; C.bRelief = true; C.T = 0.f; C.BlockT = C.StuckT = C.SideT = 0.f;
	C.Spot = C.Anchor = C.A->GetActorLocation();
	ADP_LOG(TEXT("Tugs: %s takes over from %s (%s)"), *C.Name, *Was, Job);
	TArray<FCrewAct> Old = MoveTemp(C.Q); C.Q.Reset();
	const FPoseFn Held = C.LostHold; C.LostHold = nullptr;
	const FVector At = C.LastAt;
	int32 Ti = INDEX_NONE, Tu = INDEX_NONE;
	if (bGse && G) { for (int32 i = 0; i < G->Tractors.Num(); ++i) if (G->Tractors[i].Driver == Ci) Ti = i; }
	else for (int32 i = 0; i < Tugs.Num(); ++i) if (Tugs[i].Driver == Ci) Tu = i;
	if (Ti != INDEX_NONE) G->HiredWay(Ci, G->Tractors[Ti].At, G->Tractors[Ti].StandX);
	else for (const FTug& T : Tugs) if (T.Driver == Ci || T.Handler == Ci || T.GpuCi == Ci || T.Walkers.Contains(Ci)) { SafeWay(C, T.StandX); break; }
	if (Held && Ti != INDEX_NONE) G->ClimbIn(Ci, Ti, nullptr);
	else if (Held && Tu != INDEX_NONE)
	{
		// (into the tug only where it stands: never into a moving one)
		FCrewAct W; W.Kind = FCrewAct::WaitFor; W.Until = [this, Tu](float) { return Tugs[Tu].Path.Num() == 0; }; C.Q.Add(W);
		ClimbIn(C, Tu, nullptr);
	}
	else
	{
		if (Ti != INDEX_NONE) G->WalkTo(Ci, [At] { return At; }, 40.f);
		else { FCrewAct W; W.Kind = FCrewAct::Walk; W.To = [At] { return At; }; W.Stop = 40.f; C.Q.Add(W); }
		if (Held) { FCrewAct H; H.Kind = FCrewAct::Do; H.Fn = [&C, Held] { C.Hold = Held; }; C.Q.Add(H); }
	}
	FCrewAct E; E.Kind = FCrewAct::Do; E.Fn = [&C] { C.bRelief = false; }; C.Q.Add(E);
	C.Q.Append(MoveTemp(Old));
}

bool ATugs::ArrivedHired(float StandX, const FString& Reg, const FDateTime& When)
{
	for (int32 i = 0; i < Tugs.Num(); ++i)
	{
		FTug& T = Tugs[i];
		if (FMath::Abs(T.StandX - StandX) >= 1.f) continue;
		if (!T.Gpu.M.IsValid() || T.Gpu.bConnected || T.Gpu.bInHand || T.GpuCi != INDEX_NONE) return true;
		UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); if (!Sp) return true;
		TArray<UStaffPeople::FTaken> Team;
		if (!Sp->AskTeam(TEXT("gpu:") + Reg, EStaffTask::Gpu, When, T.Gpu.M->GetActorLocation(), FString::Printf(TEXT("%s ground power"), *Reg), Team)) return false;
		const int32 Ci = Team.Num() ? Enlist(Team[0].A.Get(), Team[0].Id, TEXT("ground power handler")) : INDEX_NONE;
		if (Ci == INDEX_NONE) return false;
		T.GpuCi = Ci;
		FCrew& C = Crew[Ci];
		ADP_LOG(TEXT("Tugs: %s on blocks at x %.0f - %s puts it on ground power"), *Reg, StandX, *C.Name);
		SafeWay(C, StandX);
		const FVector Side = Plan(StandX - 18.5f, 104.f);
		FCrewAct W; W.Kind = FCrewAct::Walk; W.To = [Side] { return Side; }; W.Stop = 60.f; C.Q.Add(W);
		GpuConnect(i, C);
		C.Q.Add(W);
		LeaveAct(C);
		return true;
	}
	return true;   // (no tug or ground power at that stand)
}

bool ATugs::CrewReady(const FString& Key, float StandX, FString& Why)
{
	if (!HiredJob(Key)) return true;
	if (!Tugs.ContainsByPredicate([&](const FTug& T) { return FMath::Abs(T.StandX - StandX) < 1.f; })) return true;   // (no tug at that stand)
	const int32 I = Assign(Key, StandX);
	if (I == INDEX_NONE) { Why = TEXT("its pushback tug is not back yet"); return false; }
	const FTug& T = Tugs[I];
	if (!T.bHired) return true;
	if (T.State == EState::Parked)
	{
		const UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
		const FString M = Sp ? Sp->WaitsFor(TEXT("push:") + Key) : FString();
		Why = M.IsEmpty() ? FString(TEXT("its pushback crew is being called")) : TEXT("the pushback waits: ") + M;
		return false;
	}
	if (T.State == EState::Boarding) { Why = TEXT("its pushback crew is on the way"); return false; }
	return true;
}
