// The requests of the aircraft at the stands without a jetway (to-do 296, 277 D6), and what AGSE does about an urgent one: its
// place in the queue first, then the crew of stairs that have not set off yet for somebody later, then another vehicle for one that
// is stuck. Nobody is taken out of a job half done.
#include "StandAsk.h"
#include "ADPLog.h"
#include "Sim/ADPCrew.h"
#include "GSE.h"
#include "ServiceVehicles.h"
#include "StaffPeople.h"
#include "Tugs.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSchedule.h"
#include "Engine/World.h"

namespace
{
	struct FStandAsk { FDateTime Since, Next; double NextT = 0.0, SeenT = 0.0; int32 N = 0; FString What, Who; };
	TMap<FString, FStandAsk> Asks; TWeakObjectPtr<UWorld> AskWorld; FDateTime AskNow;
	constexpr float StairsStuckS = 240.f, StairsCrewS = 600.f;
}

int32 ADPStandAsk::Check(UWorld* W, const FDateTime& Now, const FString& Key, const FString& What, const FString& Who, int32 Where, bool bCan, const FString& Why)
{
	UADPSim* Sim = UADPSim::Get(W); if (!W || !Sim) return 0;
	if (AskWorld.Get() != W || Now < AskNow - FTimespan::FromMinutes(1.0))
	{
		if (Asks.Num()) ADP_LOG(TEXT("StandAsk: another game - %d requests begin again"), Asks.Num());
		Asks.Reset(); AskWorld = W;
	}
	AskNow = Now;
	const double T = W->GetTimeSeconds();
	for (auto It = Asks.CreateIterator(); It; ++It)
		if (T - It.Value().SeenT > 60.0)
		{
			if (It.Value().N) ADP_LOG(TEXT("StandAsk: %s for %s no longer needed (asked %d times)"), *It.Value().What, *It.Value().Who, It.Value().N);
			It.RemoveCurrent();
		}
	if (Where == 2)
	{
		const FStandAsk* A = Asks.Find(Key);
		if (A && A->N)
		{
			ADPSchedule::Tell(Sim->World, Now, TEXT("standask:") + Key + TEXT(":ok"), What + TEXT(" there"), FString::Printf(TEXT("%s: served, %.0f min after it should have been."), *Who, (Now - A->Since).GetTotalMinutes()));
			ADP_LOG(TEXT("StandAsk: %s for %s is there - request %s closed"), *What, *Who, *Key);
		}
		Asks.Remove(Key); return 0;
	}
	FStandAsk* A = Asks.Find(Key);
	if (!A) { A = &Asks.Add(Key); A->Since = Now; A->Next = Now + FTimespan::FromMinutes(3.0); A->NextT = T + 20.0; }
	A->SeenT = T; A->What = What; A->Who = Who;
	// (on its way: the request stays open; asked for again only when it falls out)
	if (Where == 1) { A->Next = FMath::Max(A->Next, Now + FTimespan::FromMinutes(3.0)); A->NextT = FMath::Max(A->NextT, T + 20.0); return A->N; }
	if (Now < A->Next || T < A->NextT) return A->N;
	++A->N; A->Next = Now + FTimespan::FromMinutes(5.0); A->NextT = T + 20.0;
	const FString Reason = Why.IsEmpty() ? FString(TEXT("not on the way")) : Why, Since = A->Since.ToString(TEXT("%H:%M"));
	ADP_LOG(TEXT("StandAsk: %s - %s, request %d (since %s): %s%s"), *Who, *What, A->N, *Since, *Reason, bCan ? TEXT("") : TEXT(" - the airport cannot do it"));
	const FString TellKey = FString::Printf(TEXT("standask:%s:%d"), *Key, A->N);
	if (bCan) ADPSchedule::Tell(Sim->World, Now, TellKey, What + TEXT(" requested"), FString::Printf(TEXT("%s asks for it, request %d (since %s): %s."), *Who, A->N, *Since, *Reason));
	else if (A->N % 6 == 1) ADPSchedule::Tell(Sim->World, Now, TellKey, What + TEXT(" missing"), FString::Printf(TEXT("%s cannot be served: %s."), *Who, *Reason));
	return A->N;
}

// ------------------------------------------------------------------ the stairs

bool AGSE::StairsCan(float StandX, FString& Why) const
{
	const UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); bool bAny = false, bHired = false;
	for (const FTractor& T : Tractors)
		if (ADPService::All().IsValidIndex(T.Svc) && ADPService::All()[T.Svc].bStairs && T.M.IsValid())
		{
			bAny = true; if (Sp && Sp->HiresRole(ADPService::All()[T.Svc].Role)) bHired = true;
		}
	if (!bAny) { Why = TEXT("the airport has no passenger stairs vehicle"); return false; }
	if (!bHired) { Why = TEXT("fewer than two ground crew are hired for the passenger stairs"); return false; }
	float Sill = 0.f; if (!StairsReach(StandX, 0, Sill)) { Why = FString::Printf(TEXT("its door sill is %.0f cm up, out of the passenger stairs' reach"), Sill); return false; }
	if (!SvcFor(TEXT("stairs"), StandX)) { Why = TEXT("the passenger stairs do not serve this stand"); return false; }
	return true;
}

int32 AGSE::StairsWhere(float StandX, FString& Why) const
{
	FStairsWay Wy; if (StairsReady(StandX, 0, Wy)) return 2;
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		const FSvcJob& J = P.Value; if (J.CtDoor != 0 || J.Phase >= 3 || FMath::Abs(J.StandX - StandX) > 1.f || !ADPService::All()[J.Svc].bStairs) continue;
		if (J.Phase == 0)
		{
			const UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
			Why = Sp && Sp->OnDuty(ADPService::All()[J.Svc].Role) == 0 ? FString::Printf(TEXT("%s: nobody on shift"), *ADPCrew::RoleName(ADPService::All()[J.Svc].Role)) : J.Why.IsEmpty() ? FString(TEXT("their people are being called")) : J.Why;
			return 0;
		}
		if (J.Phase == 1 && J.StillT > (Tractors.IsValidIndex(J.Ti) && Tractors[J.Ti].Job == EJob::Boarding ? StairsCrewS : StairsStuckS)) { Why = TEXT("the passenger stairs sent to it do not get on"); return 0; }
		if (J.bGo || J.Stage >= 4) { Why = TEXT("the passenger stairs are just leaving it"); return 0; }
		return 1;
	}
	Why = TEXT("no passenger stairs are on the way"); return 0;
}

void AGSE::StairsUrge(float StandX, int32 Level)
{
	ATugs* G = Tg(); int32 Jk = INDEX_NONE; if (!G || Level <= 0) return;
	for (TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.CtDoor == 0 && P.Value.Phase < 3 && FMath::Abs(P.Value.StandX - StandX) < 1.f && ADPService::All()[P.Value.Svc].bStairs) Jk = P.Key;
	FSvcJob* J = SvcJ(Jk); if (!J) return;
	if (J->Urgent != Level) { J->Urgent = Level; ADP_LOG(TEXT("GSE: the passenger stairs for %s (stand x %.0f) - urgency %d"), *J->Reg, StandX, Level); }
	if (J->Phase == 1 && Tractors.IsValidIndex(J->Ti))
	{
		// (stuck on the apron with its crew seated in it: home as soon as it can, another pair or the same asked for anew)
		FTractor& T = Tractors[J->Ti];
		if (J->StillT < StairsStuckS || T.Job != EJob::Out || T.Player.IsValid() || FMath::Abs(T.Speed) > 1.f || FVector::Dist2D(T.At, T.Home) < 200.f || !SvcAll(Jk, [](const FSvcMan& M) { return M.bSeated; })) return;
		ADP_WARN(TEXT("GSE: %s is stuck on the way to %s (%.0f s at plan (%.0f, %.0f)) - it is sent home and replaced"), *T.Name, *J->Reg, J->StillT, T.At.X / 100.f, -T.At.Y / 100.f);
		const FString Reg = J->Reg, Flight = J->Flight; const FDateTime When = J->When, Want = J->Want;
		T.Path.Reset(); T.OnArrive = nullptr; T.Until = nullptr; T.AfterUntil = nullptr; T.bEndYaw = false;
		SvcLeave(Jk); SvcAsk(TEXT("stairs"), StandX, Reg, Flight, When, 0.f, Want, false);
		return;
	}
	if (Level < 2 || J->Phase != 0 || J->PullT > GetWorld()->GetTimeSeconds()) return;
	// (a pair whose people are still walking to it, for a rear door or a flight later and less urgent: they come here)
	int32 From = INDEX_NONE;
	for (TPair<int32, FSvcJob>& P : SvcJobs)
	{
		FSvcJob& X = P.Value; if (P.Key == Jk || X.Phase != 1 || !ADPService::All()[X.Svc].bStairs || !Tractors.IsValidIndex(X.Ti)) continue;
		if (X.CtDoor == 0 && (X.Urgent >= Level || X.Want <= J->Want)) continue;
		const FTractor& V = Tractors[X.Ti]; if (V.Job != EJob::Boarding || V.Player.IsValid() || V.Path.Num() || V.Until || FVector::Dist2D(V.At, V.Home) > 200.f) continue;
		bool bFree = X.Men.Num() > 0;
		for (const FTractor::FSvcDoor& D : V.SvcDoors) if (D.Now > 0.01f || D.Want > 0.01f) bFree = false;
		for (const FSvcMan& M : X.Men) if (M.bSeated || M.bGone || !G->Crew.IsValidIndex(M.Ci) || G->Crew[M.Ci].bUp || G->Crew[M.Ci].bPosed || !G->Crew[M.Ci].A.IsValid() || FVector::Dist2D(G->Crew[M.Ci].A->GetActorLocation(), V.At) < 700.) bFree = false;
		if (bFree && (From == INDEX_NONE || X.CtDoor)) From = P.Key;
	}
	if (From == INDEX_NONE) return;
	FSvcJob& X = SvcJobs[From]; FTractor& V = Tractors[X.Ti];
	ADP_LOG(TEXT("GSE: the crew walking to %s for %s (%s door) are called off - %s needs them first"), *V.Name, *X.Reg, X.CtDoor ? TEXT("rear") : TEXT("front"), *J->Reg);
	for (const FSvcMan& M : X.Men) { ATugs::FCrew& C = G->Crew[M.Ci]; C.Q.Reset(); C.Hold = nullptr; GiveBack(M.Ci, 0.f); }
	X.Men.Reset(); X.Phase = 0; X.Ti = INDEX_NONE; X.T = 0.f; X.StillT = 0.f; X.Why.Reset();
	V.Job = EJob::None; V.Driver = INDEX_NONE; V.Key.Reset();
	J->PullT = GetWorld()->GetTimeSeconds() + 120.0;
}

// ------------------------------------------------------------------ the buses

bool AGSE::BusCan(FString& Why) const
{
	const UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); bool bAny = false;
	for (const FTractor& T : Tractors) if (T.Bu && T.M.IsValid()) bAny = true;
	if (!bAny) { Why = TEXT("the airport has no apron bus"); return false; }
	if (!Sp || !Sp->HiresRole(TEXT("bus"))) { Why = TEXT("no bus driver is hired"); return false; }
	if (Sp->OnDuty(TEXT("bus")) == 0) Why = TEXT("no bus driver is on shift");
	return true;
}

// its order is withdrawn (only one that stands): it stays where it is until the next
bool AGSE::BusHalt(int32 Ti)
{
	if (!Tractors.IsValidIndex(Ti) || !Tractors[Ti].Bu || Tractors[Ti].Player.IsValid() || FMath::Abs(Tractors[Ti].Speed) > 1.f) return false;
	FTractor& T = Tractors[Ti]; ++BusOrder.FindOrAdd(Ti);
	T.Path.Reset(); T.OnArrive = nullptr; T.Until = nullptr; T.AfterUntil = nullptr; T.bEndYaw = false;
	ADP_LOG(TEXT("Apron bus %s: its order is withdrawn at plan (%.0f, %.0f)"), *T.Name, T.At.X / 100.f, -T.At.Y / 100.f);
	return true;
}
