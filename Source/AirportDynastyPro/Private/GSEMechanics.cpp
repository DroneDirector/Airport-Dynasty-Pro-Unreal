// A check's hired mechanics in the hangar hall (to-do 311, docs/Hired-Crew-Plan.md "Mechanics"). The simulation lets a check
// start only with enough free hired mechanics (Sim/ADPMaint.cpp); one of them is the tow's brake rider (GSEMaint.cpp), the
// others come here in 3D: asked for once the aircraft stands in the hall (MaintHall), they ride a maintenance van across the
// runway (VanRide; on foot only on the hall's own side, the hired people's way), walk in through the open hall door on the
// side away from the hall's stair and stand by the aircraft's nose on its right, clear of the engines, the stair and the tug's
// lane. When the check is over they walk out (the door opened for them if it is shut) before the tow back may start, and ride
// home. At the fault itself (hangar step 4, GSEMechWork.cpp): each walks round the gear, engines and the stair to its own
// place at the fault (Data/aircraft-workpoints.json), turns to it and works there in pairs, one holding, one fastening. Talk and "Call to my office" are the pool's own (UStaffPeople IdOf / CallIn).
#include "GSE.h"
#include "Hangar.h"
#include "Tugs.h"
#include "StaffPeople.h"
#include "ADPLog.h"
#include "Sim/ADPSim.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 MkMost = 7;              // (people in 3D for one check at most, besides its brake rider; the rest of its crew works in the simulation only)
	constexpr float MkAcross = 7.f, MkStep = 1.5f, MkWall = 2.5f;   // (m: their row beside the nose, right of the centre line)
	constexpr double MkOutMost = 1200.0;     // (s: the tow back never waits longer than that for them)
}

bool AGSE::MechCheckOn(const FString& Reg) const
{
	const UADPSim* Sim = UADPSim::Get(GetWorld()); if (!Sim) return false;
	const FADPAircraft* A = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; });
	return A && A->AirlineId == UADPSim::Me() && A->Status == TEXT("maintenance");
}

void AGSE::MechDoor(const FHallStair& H, const FString& Who) const
{
	if (HallOpen(H) > 0.05f) return;
	if (AHangar* Hg = Cast<AHangar>(H.Door.Get())) { Hg->Command(true); ADP_LOG(TEXT("GSE: hangar %s - the door opens for %s"), *H.Hall, *Who); }
}

void AGSE::MechStart(const FString& Reg, const FHallStair& H)
{
	if (const FMechCrew* Old = MechCrews.Find(Reg); Old && Old->Phase >= 4 && MechCheckOn(Reg) && Old->Hall != H.Hall) MechCrews.Remove(Reg);   // (a crew of an earlier check, lost on its way home)
	if (MechCrews.Contains(Reg) || !MechCheckOn(Reg)) return;
	const double Tn = GetWorld()->GetTimeSeconds();
	if (const double* T = MechTry.Find(Reg); T && Tn - *T < 5.0) return;
	MechTry.Add(Reg, Tn);
	const UADPSim* Sim = UADPSim::Get(GetWorld()); ATugs* G = Tg(); UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
	if (!Sim || !G || !Sp) return;
	const FADPAircraft* A = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; });
	const int32 Want = FMath::Min(MkMost, FMath::Max(A ? A->CheckCrew : 2, 1) - 1);   // (one of the check's crew is its brake rider)
	auto Say = [this, &Reg](const FString& Why) { FString& Was = MechWhy.FindOrAdd(Reg); if (Was != Why) { Was = Why; ADP_LOG(TEXT("GSE: %s's check in hangar - %s"), *Reg, *Why); } };
	if (Want <= 0) { Say(TEXT("its brake rider is its whole crew")); return; }
	const float Sd = -H.Side, Hf = H.Half;   // (the side away from the hall's stair)
	const FVector Gate = HallP(H, Sd * (Hf - 8.f), H.Front - 8.f), In = HallP(H, Sd * (Hf - 4.f), H.Front + 3.f);
	const FString Key = FString::Printf(TEXT("check:%s"), *Reg), What = FString::Printf(TEXT("%s %s"), *Reg, UStaffPeople::TaskName(EStaffTask::Maint));
	TArray<UStaffPeople::FTaken> Team;
	if (!Sp->AskTeam(Key, EStaffTask::Maint, Sim->Now(), Gate, What, Team)) { Say(FString::Printf(TEXT("no mechanic comes yet: %s"), *Sp->WaitsFor(Key))); return; }
	while (Team.Num() && Team.Num() < Want) { UStaffPeople::FTaken X; if (!Sp->AskExtra(EStaffDuty::Mechanic, Gate, What, X)) break; Team.Add(X); }
	FMechCrew C; C.Hall = H.Hall;
	MechSpots(Reg, H, 64, C.All, C.Keep);   // (every place at the fault in order - theirs first, the rest free for you; the gear and engines to walk round)
	for (const UStaffPeople::FTaken& X : Team)
	{
		const int32 Ci = G->Enlist(X.A.Get(), X.Id, UStaffPeople::DutyName(X.Duty));
		if (Ci == INDEX_NONE) { Sp->Release(X.Id); continue; }
		G->Crew[Ci].bGse = true; C.Men.Add(Ci);
	}
	if (!C.Men.Num()) { Say(TEXT("no place for its mechanics")); return; }
	const float Xs = FMath::Min(MkAcross, Hf - MkWall);
	for (int32 m = 0; m < C.Men.Num(); ++m)
	{
		// (each one's place, shared with their walk and work: changed for them if you take it first - GSEMechYou.cpp)
		FMechSpot P; if (C.All.IsValidIndex(m)) P = C.All[m]; else { P.At = HallP(H, Sd * Xs, MkStep * m - 1.f); P.bWork = false; P.What = TEXT("stand-by row"); }
		C.Places.Add(MakeShared<FMechSpot>(P));
	}
	MechWhy.Remove(Reg);
	MechCrews.Add(Reg, C);
	bWorked = true;
	ADP_LOG(TEXT("GSE: %s's check - %d of its mechanics go to hangar %s (wanted %d)"), *Reg, C.Men.Num(), *H.Hall, Want);
	const FString Hall = H.Hall;
	for (int32 m = 0; m < C.Men.Num(); ++m)
	{
		const int32 Ci = C.Men[m];
		const TSharedRef<FMechSpot> Pm = C.Places[m];   // (at the fault; else beside the nose, the row along the aircraft)
		auto Out = [this, Reg, Hall, Ci, Gate, In]
		{
			// the check over: out through the door (opened if it is shut), then home by van
			WaitFor(Ci, [this, Reg, Hall](float) { const FHallStair* X = HallNamed(Hall); if (!X) return true; MechDoor(*X, Reg + TEXT("'s mechanics")); return HallOpen(*X) > 0.6f; });
			WalkTo(Ci, [In] { return In; }, 40.f);
			WalkTo(Ci, [Gate] { return Gate; }, 60.f);
			Do(Ci, [this, Reg, Ci]
			{
				FMechCrew* K = MechCrews.Find(Reg);
				if (K && ++K->Out >= K->Men.Num()) { K->Phase = 4; ADP_LOG(TEXT("GSE: %s's mechanics are out of the hall"), *Reg); }
				auto Gone = [this, Reg, Ci]
				{
					GiveBack(Ci);
					FMechCrew* Q = MechCrews.Find(Reg);
					if (Q && ++Q->Home >= Q->Men.Num()) { MechCrews.Remove(Reg); MechTry.Remove(Reg); ADP_LOG(TEXT("GSE: %s's mechanics are back"), *Reg); }
				};
				Later([this, Ci, Gone] { if (!VanRide(Ci, VanDropSouth(INDEX_NONE, 0.f), TEXT("mechanic"), Gone)) Gone(); });
			});
		};
		auto Rest = [this, Reg, Hall, Ci, Gate, In, Pm, Out]
		{
			WalkTo(Ci, [Gate] { return Gate; }, 60.f);
			WaitFor(Ci, [this, Reg, Hall](float) { const FHallStair* X = HallNamed(Hall); if (!X) return true; MechDoor(*X, Reg + TEXT("'s mechanics")); return HallOpen(*X) > 0.6f; });   // (never through a shut door)
			WalkTo(Ci, [In] { return In; }, 40.f);
			WalkTo(Ci, [Pm] { return Pm->At; }, 30.f);
			if (Pm->bWork) MechWork(Ci, Reg, Pm);
			else Do(Ci, [Pm] { Pm->bThere = true; });
			Do(Ci, [this, Reg]
			{
				FMechCrew* K = MechCrews.Find(Reg);
				if (K && ++K->There >= K->Men.Num() && K->Phase == 1) { K->Phase = 2; ADP_LOG(TEXT("GSE: %s's mechanics stand by it in hangar %s"), *Reg, *K->Hall); }
			});
			WaitFor(Ci, [this, Reg](float) { return !MechCheckOn(Reg); });
			Do(Ci, [this, Reg, Ci, Out]
			{
				FMechCrew* K = MechCrews.Find(Reg);
				if (K && K->Phase < 3) { K->Phase = 3; ADP_LOG(TEXT("GSE: %s's check is over - its mechanics walk out of hangar %s"), *Reg, *K->Hall); }
				if (ATugs* Gt = Tg(); Gt && Gt->Crew.IsValidIndex(Ci)) Gt->Crew[Ci].Hold = nullptr;   // (the work pose let go before they walk)
				Out();
			});
		};
		// (from the stands' side over the runway only in a maintenance van, never on foot)
		const FVector Drop = HallP(H, Sd * (Hf + 6.f), H.Front - 20.f);
		const bool bOver = G->Crew.IsValidIndex(Ci) && G->Crew[Ci].A.IsValid() && Across(G->Crew[Ci].A->GetActorLocation(), Gate);
		if (bOver && VanRide(Ci, Drop, TEXT("mechanic"), Rest)) continue;
		if (bOver) ADP_WARN(TEXT("GSE: there is no maintenance van - %s walks to hangar %s"), *G->Crew[Ci].Name, *Hall);
		HiredWay(Ci, Gate, 0.f);
		Rest();
	}
}

bool AGSE::MechHolds(const FString& Reg, FString& Why)
{
	FMechCrew* K = MechCrews.Find(Reg); if (!K || K->Phase >= 4) return false;
	const double Tn = GetWorld()->GetTimeSeconds();
	if (K->OutAt <= 0.0) K->OutAt = Tn;   // (asked by the tow back: the check is over)
	if (Tn - K->OutAt > MkOutMost)
	{
		ADP_WARN(TEXT("GSE: %s's mechanics are not out of hangar %s after %.0f s - the tow back goes on"), *Reg, *K->Hall, MkOutMost);
		K->Phase = 4; return false;
	}
	Why = K->Phase == 3 ? TEXT("its mechanics walk out of the hall") : TEXT("its mechanics are still at work");
	return true;
}
