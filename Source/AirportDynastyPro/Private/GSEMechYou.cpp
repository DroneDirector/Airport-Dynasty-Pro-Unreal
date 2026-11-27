// Hangar step 5 (owner GO 2026-11-27, docs/Hired-Crew-Plan.md "Mechanics"): you work on a fault beside the hired mechanics.
// Within 1.5 m of a free work place of a job on one of your aircraft (a check running in the hangar, or a fault or check
// still waiting for its crew where the aircraft stands) E / X takes the place: you count as one pair of hands in ADPMaint
// exactly like a hired mechanic (YouJoin - up to the job's most; a waiting job starts at once if yours was the hand it
// lacked), a mechanic who was still on his way to that place goes to another free one or stands by. At the place your arms
// do the mechanics' own work loop (MechHands: hold or fasten). E / X again, walking away, the aircraft moving or the job
// ending leaves it (YouLeave: the check goes on with its hired crew). Every take and leave is logged.
#include "GSE.h"
#include "Tugs.h"
#include "ADPWalker.h"
#include "BabylonModel.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPMaint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

FString AGSE::MechTip;

namespace
{
	constexpr float MyReach = 150.f;    // (cm: a work place within this of your feet is yours to take; further, you have left it)
	constexpr float MyApart = 75.f;     // (cm: as GSEMechWork's - two people's places never closer)
	constexpr float MyNear = 6000.f;    // (cm: aircraft further than this are not looked at)

	/** The job an aircraft of yours has on it now, or waits for crew for (its fault table key); empty: none. */
	FString MyJob(const FADPWorld& W, const FADPAircraft& A, const FDateTime& Now)
	{
		if (A.AirlineId != UADPSim::Me() || A.Status == TEXT("flying")) return FString();
		if (A.Status == TEXT("maintenance")) return A.CheckFault;
		if (!A.FaultDue.IsEmpty()) return A.FaultDue;
		if (A.MaintWait.IsEmpty()) return FString();   // (a routine check only once it waits for its mechanics)
		double F = 0.0; const FString K = ADPMaint::NextDue(W, A, Now, F);
		return K == TEXT("A") ? FString(TEXT("acheck")) : K == TEXT("C") ? FString(TEXT("ccheck")) : K == TEXT("D") ? FString(TEXT("dcheck")) : FString();
	}
}

void AGSE::MechYouLeave(const TCHAR* Why)
{
	if (MechMe.Reg.IsEmpty()) return;
	const FString Reg = MechMe.Reg;
	if (UADPSim* Sim = UADPSim::Get(GetWorld()))
		if (FADPAircraft* A = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; })) ADPMaint::YouLeave(Sim->World, *A, Sim->Now());
	if (AADPWalker* Me = Cast<AADPWalker>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0))) { Me->SetArm(0.f, FVector::ZeroVector, FVector(1, 0, 0), FVector(0, 0, 1)); Me->SetArmL(0.f, FVector::ZeroVector); }
	ADP_LOG(TEXT("GSE: you leave the %s on %s - %s"), *MechMe.S.What, *Reg, Why);
	MechMe = FMechYou(); MechTip.Reset(); MechLookT = 0.f;
}

void AGSE::MechYouTick(float Dt)
{
	UWorld* Wd = GetWorld(); if (!Wd) return;
	AADPWalker* Me = Cast<AADPWalker>(UGameplayStatics::GetPlayerPawn(Wd, 0));
	const UADPSim* Sim = UADPSim::Get(Wd);
	// ---- at work: held there until E / X, walking away, the aircraft moving or the job over
	if (!MechMe.Reg.IsEmpty())
	{
		const AActor* Pl = MechMe.Plane.Get();
		const FADPAircraft* A = Sim ? Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == MechMe.Reg; }) : nullptr;
		if (!Me || Me->IsLent()) MechYouLeave(TEXT("you went off it"));
		else if (!Pl || FVector::Dist2D(Pl->GetActorLocation(), MechMe.PlaneAt) > 50.f) MechYouLeave(TEXT("the aircraft is moved"));
		else if (FVector::Dist2D(Me->GetActorLocation(), MechMe.S.At) > MyReach) MechYouLeave(TEXT("you walked away"));
		else if (!A || !Sim || MyJob(Sim->World, *A, Sim->Now()).IsEmpty() || !ADPMaint::YouOn(MechMe.Reg)) MechYouLeave(TEXT("the job is over"));
		else
		{
			// the mechanics' own work loop on your arms (MechHands), from where you stand
			const FMechSpot& S = MechMe.S;
			const FVector Feet(Me->GetActorLocation().X, Me->GetActorLocation().Y, S.At.Z);
			FVector L, R; MechHands(Feet, S.Face, S.Hands, S.bHold, float(Wd->GetTimeSeconds()), L, R);
			Me->SetArm(1.f, R, S.Face, FVector(0, 0, 1)); Me->SetArmL(1.f, L);
			MechTip = FString::Printf(TEXT("E / X  Stop work (%s, %s)"), *S.What, S.bHold ? TEXT("holding") : TEXT("fastening"));
		}
		return;
	}
	MechLookT -= Dt; MechFindT -= Dt;
	if (MechLookT > 0.f) return;
	MechLookT = 0.2f;
	ADP_PERF_SCOPE("gse: you by a work place", 0.5f);
	MechTip.Reset(); MechNearReg.Reset(); MechNearPlane.Reset(); bMechNearOk = false;
	if (!Me || Me->IsLent() || !Sim) return;
	const FVector Feet = Me->GetActorLocation();
	ATugs* G = Tg();
	float Best = MyReach;
	auto Consider = [&](const FString& Reg, AActor* Pl, const TArray<FMechSpot>& All, const FMechCrew* K)
	{
		for (const FMechSpot& S : All)
		{
			const float D = FVector::Dist2D(Feet, S.At);
			if (D >= Best || FMath::Abs(Feet.Z - S.At.Z) > 200.f) continue;
			// (a mechanic at it, or anyone standing on it: not free - one on his way there gives it up to you)
			bool bTaken = false;
			if (K) for (const TSharedRef<FMechSpot>& P : K->Places) if (P->bThere && FVector::Dist2D(P->At, S.At) < MyApart) bTaken = true;
			if (K && G) for (const int32 Ci : K->Men) if (G->Crew.IsValidIndex(Ci) && G->Crew[Ci].A.IsValid() && FVector::Dist2D(G->Crew[Ci].A->GetActorLocation(), S.At) < 60.f) bTaken = true;
			if (bTaken) continue;
			Best = D; MechNear = S; MechNearReg = Reg; MechNearPlane = Pl;
		}
	};
	// a check running in a hangar hall, its crew going in or at work
	for (const TPair<FString, FMechCrew>& K : MechCrews)
	{
		if (K.Value.Phase >= 3 || !MechCheckOn(K.Key)) continue;
		const FHallStair* H = nullptr; for (const FHallStair& X : HallStairs) if (X.Hall == K.Value.Hall) H = &X;
		AActor* Pl = H ? H->Plane.Get() : nullptr;
		if (Pl && FVector::Dist2D(Pl->GetActorLocation(), Feet) < MyNear) Consider(K.Key, Pl, K.Value.All, &K.Value);
	}
	// a job waiting for its crew where the aircraft stands (its model tagged SIM/<reg>): looked for every 2 s
	if (MechFindT <= 0.f)
	{
		MechFindT = 2.f;
		TMap<FString, FString> Jobs;
		for (const FADPAircraft& A : Sim->World.Aircraft)
			if (A.Status != TEXT("maintenance") && !MechCrews.Contains(A.Reg)) { const FString J = MyJob(Sim->World, A, Sim->Now()); if (!J.IsEmpty()) Jobs.Add(A.Reg, J); }
		for (auto It = MechWaiting.CreateIterator(); It; ++It) if (!Jobs.Contains(It.Key()) || !It.Value().IsValid()) { MechWaitSpots.Remove(It.Key()); It.RemoveCurrent(); }
		for (const TPair<FString, FString>& J : Jobs)
		{
			if (MechWaiting.Contains(J.Key)) continue;
			const FName Tag(*(TEXT("SIM/") + J.Key));
			for (TActorIterator<ABabylonModel> It(Wd); It; ++It)
				if (It->Tags.Contains(Tag))
				{
					TArray<FMechSpot> Sp; TArray<FOutline> Kp;
					if (MechSpotsAt(J.Key, *It, J.Value, 64, Sp, Kp)) { MechWaiting.Add(J.Key, *It); MechWaitSpots.Add(J.Key, Sp); ADP_LOG(TEXT("GSE: %s's %s waits for crew - %d work places you can take"), *J.Key, *J.Value, Sp.Num()); }
					break;
				}
		}
	}
	for (const TPair<FString, TWeakObjectPtr<AActor>>& Wt : MechWaiting)
		if (AActor* Pl = Wt.Value.Get(); Pl && FVector::Dist2D(Pl->GetActorLocation(), Feet) < MyNear)
			if (const TArray<FMechSpot>* Sp = MechWaitSpots.Find(Wt.Key)) Consider(Wt.Key, Pl, *Sp, nullptr);
	if (MechNearReg.IsEmpty()) return;
	const FADPAircraft* A = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == MechNearReg; });
	FString Why;
	bMechNearOk = A && ADPMaint::YouMay(Sim->World, *A, Sim->Now(), Why);
	MechTip = bMechNearOk ? FString::Printf(TEXT("E / X  Work here (%s, %s)"), *MechNear.What, MechNear.bHold ? TEXT("hold") : TEXT("fasten"))
		: FString::Printf(TEXT("%s: %s"), *MechNearReg, *Why);
}

bool AGSE::MechUse(APawn* Me, FString& Say)
{
	Say.Reset();
	if (!MechMe.Reg.IsEmpty()) { MechYouLeave(TEXT("you stopped (E / X)")); Say = TEXT("You stop work - the crew go on"); return true; }
	if (MechNearReg.IsEmpty() || !Me || FVector::Dist2D(Me->GetActorLocation(), MechNear.At) > MyReach) return false;
	if (!bMechNearOk) { Say = MechTip; return true; }
	UADPSim* Sim = UADPSim::Get(GetWorld()); AActor* Pl = MechNearPlane.Get(); if (!Sim || !Pl) return false;
	const FString Reg = MechNearReg;
	FADPAircraft* A = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; }); if (!A) return false;
	FString Said;
	if (!ADPMaint::YouJoin(Sim->World, *A, Sim->Now(), Said)) { Say = Said; return true; }
	MechMe.Reg = Reg; MechMe.S = MechNear; MechMe.Plane = Pl; MechMe.PlaneAt = Pl->GetActorLocation();
	ADP_LOG(TEXT("GSE: you take the %s on %s (%s)"), *MechMe.S.What, *Reg, MechMe.S.bHold ? TEXT("hold") : TEXT("fasten"));
	// a hired mechanic still on his way to that place: another free one of the job's, else he stands by in the row
	if (FMechCrew* K = MechCrews.Find(Reg))
		for (int32 m = 0; m < K->Places.Num(); ++m)
		{
			FMechSpot& P = K->Places[m].Get();
			if (P.bThere || !P.bWork || FVector::Dist2D(P.At, MechMe.S.At) >= MyApart) continue;
			ATugs* G = Tg(); const FString Who = G && K->Men.IsValidIndex(m) && G->Crew.IsValidIndex(K->Men[m]) ? G->Crew[K->Men[m]].Name : FString(TEXT("a mechanic"));
			const FMechSpot* Alt = nullptr;
			for (const FMechSpot& S : K->All)
			{
				bool bFree = FVector::Dist2D(S.At, MechMe.S.At) >= MyApart;
				for (const TSharedRef<FMechSpot>& O : K->Places) if (&O.Get() != &P && FVector::Dist2D(O->At, S.At) < MyApart) bFree = false;
				if (bFree) { Alt = &S; break; }
			}
			if (Alt) { P = *Alt; ADP_LOG(TEXT("GSE: %s was on his way to it - goes to the %s instead"), *Who, *P.What); }
			else if (const FHallStair* H = HallNamed(K->Hall))
			{
				P.At = HallP(*H, -H->Side * FMath::Min(7.f, H->Half - 2.5f), 1.5f * m - 1.f); P.bWork = false; P.What = TEXT("stand-by row");
				ADP_LOG(TEXT("GSE: %s was on his way to it - no other free place, he stands by beside the nose"), *Who);
			}
		}
	Say = Said.IsEmpty() ? FString::Printf(TEXT("You work at the %s"), *MechMe.S.What) : FString::Printf(TEXT("You work at the %s - %s"), *MechMe.S.What, *Said);
	MechTip = TEXT("E / X  Stop work");
	return true;
}
