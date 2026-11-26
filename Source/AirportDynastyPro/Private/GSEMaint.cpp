// The hangar tow's brake rider (to-do 311 step B, docs/Hired-Crew-Plan.md "Hangar tow"). One hired man of the ground crew
// drives the maintenance van (GSEService.cpp table line "maint") from the hangar apron to the stand through the service
// job's own drive (SvcStart / SvcApron), parks it on the aircraft's right 1.5 m beyond the wingtip, walks round behind the
// tail to its left side and goes in at the front left door: up the jetway's service stair (SvcUp; the jetway docked for him
// first) or, where no jetway is, up the passenger stairs (called to the door if none is there; sent away once he is in).
// In the cockpit he is hidden and carried by the aircraft (no interiors yet); the tow waits for him (HangarTow.cpp). On the
// tow back he gets off at its stand the same ways and walks the hired people's way (HiredWay) to the van and drives it home.
// Not yet: the hangar's own mobile stair (he stays aboard through the check), a mechanic role (ground crew for now).
#include "GSE.h"
#include "ServiceVehicles.h"
#include "Tugs.h"
#include "JetwayRigs.h"
#include "Airfield.h"
#include "CrewPose.h"
#include "ADPLog.h"
#include "Sim/ADPSim.h"
#include "Engine/World.h"

namespace
{
	const FServiceSpec& MtSpec(int32 i) { return ADPService::All()[i]; }
	constexpr float MtSide = 19.5f, MtTail = 45.f;   // (m: his walk round the aircraft - beyond the wingtips, behind the tail)
	// (Stage of a rider's job: 0 at the van, 4 in the cockpit, 5 the jetway / stairs away again - the tow may go, 6 getting off)
}

int32 AGSE::MaintRider(float StandX, const FString& Reg, AActor* Plane, FString& Why)
{
	TPair<int32, FSvcJob>* P = nullptr;
	for (TPair<int32, FSvcJob>& X : SvcJobs) if (X.Value.Reg == Reg && MtSpec(X.Value.Svc).bMaint && X.Value.Phase < 3) P = &X;
	if (!P)
	{
		if (!ADPService::All().ContainsByPredicate([](const FServiceSpec& S) { return S.bMaint; })) { Why = TEXT("there is no maintenance van for its brake rider"); return 0; }
		const UADPSim* Sim = UADPSim::Get(GetWorld()); const FDateTime Now = Sim ? Sim->Now() : FDateTime::UtcNow();
		SvcAsk(TEXT("maint"), StandX, Reg, TEXT("TOW/") + Reg, Now + FTimespan::FromMinutes(20.0));
		ADP_LOG(TEXT("GSE: %s's hangar tow asks for a brake rider (maintenance van, stand x %.0f)"), *Reg, StandX);
		Why = TEXT("its brake rider is being called"); return 0;
	}
	const int32 Job = P->Key; FSvcJob& J = P->Value;
	if (J.Phase == 0) { Why = J.Why.IsEmpty() ? FString(TEXT("its brake rider is being called")) : TEXT("its brake rider waits: ") + J.Why; return 0; }
	const FSvcMan* M = J.Men.IsValidIndex(0) ? &J.Men[0] : nullptr;
	if (!M || M->bGone) { Why = TEXT("its brake rider is lost"); return 0; }
	if (!M->bIn) { Why = J.Phase == 1 ? TEXT("its brake rider drives the maintenance van to the stand") : TEXT("its brake rider is on his way to the cockpit"); return 1; }
	// in the cockpit: out of sight, carried by the aircraft
	ATugs* G = Tg(); AActor* A = G && G->Crew.IsValidIndex(M->Ci) ? G->Crew[M->Ci].A.Get() : nullptr;
	if (A && Plane && A->GetAttachParentActor() != Plane)
	{
		A->SetActorHiddenInGame(true); A->AttachToActor(Plane, FAttachmentTransformRules::KeepWorldTransform);
		ADP_LOG(TEXT("GSE: %s, the brake rider, is in %s's cockpit - lights on"), *G->Crew[M->Ci].Name, *Reg);
	}
	if (J.Stage < 4) J.Stage = 4;
	if (J.Stage == 4)
	{
		// the way he came in goes away before the brakes come off
		if (!MaintWay.Contains(Job))
		{
			AJetwayRigs* R = AJetwayRigs::Get(GetWorld());
			const AJetwayRigs::EState Js = R && J.Jet != INDEX_NONE ? R->State(J.Jet) : AJetwayRigs::EState::Parked;
			if (Js == AJetwayRigs::EState::Docked && !R->Undock(J.Jet)) ADP_WARN(TEXT("GSE: %s's jetway does not pull back for the hangar tow: %s"), *Reg, *AJetwayRigs::LastNote);
			if (Js == AJetwayRigs::EState::Parked || Js == AJetwayRigs::EState::Manual) J.Stage = 5;
			else Why = TEXT("its jetway pulls back");
		}
		else
		{
			FString Sw; if (!StairsBusy(J.StandX, true, Sw) && !StairsHas(J.StandX)) J.Stage = 5;
			else { SvcGo(TEXT("stairs"), J.StandX); Why = TEXT("the passenger stairs leave its door"); }
		}
		if (J.Stage == 5) ADP_LOG(TEXT("GSE: %s - its door clear, the brake rider ready for the tow"), *Reg);
	}
	return J.Stage >= 5 ? 3 : 2;
}

bool AGSE::MaintAboard(const FString& Reg) const
{
	for (const TPair<int32, FSvcJob>& X : SvcJobs) if (X.Value.Reg == Reg && MtSpec(X.Value.Svc).bMaint && X.Value.Men.Num() && X.Value.Men[0].bIn && !X.Value.Men[0].bGone) return true;
	return false;
}

void AGSE::MaintOff(float StandX, const FString& Reg)
{
	for (TPair<int32, FSvcJob>& X : SvcJobs)
		if (X.Value.Reg == Reg && MtSpec(X.Value.Svc).bMaint && X.Value.Phase == 2 && !X.Value.bGo)
		{
			MaintTo.Add(X.Key, StandX); X.Value.bGo = true;
			ADP_LOG(TEXT("GSE: %s back on its stand (x %.0f) - the brake rider gets off"), *Reg, StandX);
		}
}

void AGSE::MaintAt(int32 Job)
{
	// out of the van, round behind the tail to the aircraft's left side, in at its front left door
	FSvcJob* J = SvcJ(Job); if (!J || !J->Men.Num()) return;
	const int32 Ci = J->Men[0].Ci, Ti = J->Ti; const float Sx = J->StandX; J->Stage = 0;
	ClimbOut(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) { X->bSeated = false; X->bOut = true; } });
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(Sx, S, N, Sp);   // (as the van's place: plan x S + its side, y N + aft)
	const float Sd2 = FMath::Max(MtSide, Sp * 0.5f + 1.5f);
	for (const FVector2f& Pt : {FVector2f(-Sd2, MtTail), FVector2f(Sd2, MtTail), FVector2f(Sd2, 6.f)})
		WalkTo(Ci, [this, S, N, Pt] { return PlanT(S, S + Pt.X, N + Pt.Y); }, 60.f);
	const AJetwayRigs* R = AJetwayRigs::Get(GetWorld()); TArray<FVector> St; float Hw = 0.f; FVector Md, Out;
	const FAirfieldData::FStand* Sd = AAirfield::Shared().StandAt(Sx);
	if (Sd && Sd->bJetway && R && J->Jet != INDEX_NONE && R->ServiceSteps(J->Jet, St, Hw) && R->ServiceDoorFrame(J->Jet, Md, Out))
	{
		MaintWay.Remove(Job);
		const int32 Jet = J->Jet;
		// (the jetway to the front left door for him, as for the passengers)
		WaitFor(Ci, [this, Jet, Reg = J->Reg](float E)
		{
			AJetwayRigs* Rg = AJetwayRigs::Get(GetWorld()); if (!Rg) return true;
			if (Rg->State(Jet) == AJetwayRigs::EState::Docked) return true;
			if (Rg->State(Jet) == AJetwayRigs::EState::Parked && FMath::Fmod(E, 30.f) < 0.05f && !Rg->Dock(Jet)) ADP_WARN(TEXT("GSE: the brake rider of %s waits - its jetway does not dock: %s"), *Reg, *AJetwayRigs::LastNote);
			return false;
		});
		Do(Ci, [this, Job] { Later([this, Job] { SvcUp(Job, 0, true); MaintHold(Job); }); });
		Wait(Ci, 0.5f);
		return;
	}
	MaintStairs(Job, Sx, true);
}

void AGSE::MaintStairs(int32 Job, float StandX, bool bUp)
{
	// the passenger stairs to the front left door, called if none are there; up or down once they stand ready
	FSvcJob* J = SvcJ(Job); if (!J || !J->Men.Num()) return;
	const int32 Ci = J->Men[0].Ci;
	Do(Ci, [this, Job, StandX]
	{
		const FSvcJob* X = SvcJ(Job); if (!X || StairsHas(StandX)) return;
		const UADPSim* Sim = UADPSim::Get(GetWorld()); const FDateTime Now = Sim ? Sim->Now() : FDateTime::UtcNow();
		SvcAsk(TEXT("stairs"), StandX, X->Reg, TEXT("TOW/") + X->Reg, Now + FTimespan::FromMinutes(20.0), 0.f, Now, false);
		ADP_LOG(TEXT("GSE: the brake rider of %s calls the passenger stairs to its front left door (stand x %.0f)"), *X->Reg, StandX);
	});
	WaitFor(Ci, [this, Job, StandX](float E)
	{
		FStairsWay W; if (StairsReady(StandX, 0, W)) { MaintWay.Add(Job, W); return true; }
		if (FMath::Fmod(E, 30.f) < 0.05f) { FString Sw; StairsBusy(StandX, false, Sw); const FSvcJob* X = SvcJ(Job); ADP_LOG(TEXT("GSE: the brake rider of %s waits for the passenger stairs%s%s"), X ? *X->Reg : TEXT("?"), Sw.IsEmpty() ? TEXT("") : TEXT(" - "), *Sw); }
		return false;
	});
	Do(Ci, [this, Job, StandX, bUp] { Later([this, Job, StandX, bUp] { MaintClimb(Job, StandX, bUp); }); });
	Wait(Ci, 0.5f);
}

void AGSE::MaintClimb(int32 Job, float StandX, bool bUp)
{
	FSvcJob* J = SvcJ(Job); const FStairsWay* Wp = MaintWay.Find(Job); if (!J || !J->Men.Num() || !Wp) return;
	const int32 Ci = J->Men[0].Ci; const FStairsWay W = *Wp; const int32 Ti = W.Ti;
	auto Level = [this, Ci](bool b) { Do(Ci, [this, Ci, b] { if (ATugs* G = Tg()) if (G->Crew.IsValidIndex(Ci)) G->Crew[Ci].bUp = b; }); };
	auto Climb = [this, Ci, Ti](bool bUpward)
	{
		Do(Ci, [this, Ti] { StairsEnter(Ti, 1); });
		PoseAct(Ci, [this, Ti, bUpward](float K, float PH, FDriverPose& P) { StairsPose(Ti, bUpward, K, PH, P); }, StairsSeconds(Ti), true);
		Do(Ci, [this, Ti] { StairsEnter(Ti, -1); });
	};
	if (bUp)
	{
		WalkTo(Ci, [W] { return W.Foot; }, 15.f);
		Level(true); Climb(true);
		WalkTo(Ci, [W] { return W.Door; }, 15.f);
		// (no cabin yet, to-do 209: out of sight in its door; the stairs go once he is in)
		Do(Ci, [this, Job, Ci, StandX]
		{
			if (ATugs* G = Tg()) if (AActor* A = G->Crew[Ci].A.Get()) A->SetActorHiddenInGame(true);
			if (FSvcMan* X = SvcM(Job, 0)) X->bIn = true;
			SvcGo(TEXT("stairs"), StandX);
		});
		MaintHold(Job);
		return;
	}
	Do(Ci, [this, Job, Ci]
	{
		if (ATugs* G = Tg()) if (AActor* A = G->Crew[Ci].A.Get()) { A->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); A->SetActorHiddenInGame(false); }
		if (FSvcMan* X = SvcM(Job, 0)) X->bIn = false;
	});
	WalkTo(Ci, [W] { return W.Platform; }, 15.f);
	Climb(false); Level(false);
	Do(Ci, [this, Job, StandX] { MaintWay.Remove(Job); SvcGo(TEXT("stairs"), StandX); if (FSvcMan* X = SvcM(Job, 0)) X->bDown = true; });
	Do(Ci, [this, Job] { Later([this, Job] { if (FSvcJob* X = SvcJ(Job)) if (X->Men.Num()) { HiredWay(X->Men[0].Ci, Tractors[X->Ti].At, 0.f); Do(X->Men[0].Ci, [this, Job] { Later([this, Job] { SvcLeave(Job); }); }); } }); });
	Wait(Ci, 0.5f);
}

void AGSE::MaintHold(int32 Job)
{
	// in the cockpit through the tow, the check and the tow back (the hangar's own stair is not there yet): off at the stand
	FSvcJob* J = SvcJ(Job); if (!J || !J->Men.Num()) return;
	const int32 Ci = J->Men[0].Ci;
	WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->bGo; });
	Do(Ci, [this, Job] { Later([this, Job] { MaintDown(Job); }); });
	Wait(Ci, 0.5f);
}

void AGSE::MaintDown(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J || !J->Men.Num()) return;
	const int32 Ci = J->Men[0].Ci; const float* To = MaintTo.Find(Job); const float Sx = To ? *To : J->StandX; MaintTo.Remove(Job);
	J->Stage = 6;
	const FAirfieldData::FStand* Sd = AAirfield::Shared().StandAt(Sx);
	AJetwayRigs* R = AJetwayRigs::Get(GetWorld());
	const int32 Jet = R && Sd && Sd->bJetway && !Sd->bRemote ? R->Nearest(PlanAt(Sd->Frame(), 5.f, 2.f)) : INDEX_NONE;
	TArray<FVector> St; float Hw = 0.f; FVector Md, Out;
	if (Jet != INDEX_NONE && R->ServiceSteps(Jet, St, Hw) && R->ServiceDoorFrame(Jet, Md, Out))
	{
		J->Jet = Jet; MaintWay.Remove(Job);
		WaitFor(Ci, [this, Jet, Reg = J->Reg](float E)
		{
			AJetwayRigs* Rg = AJetwayRigs::Get(GetWorld()); if (!Rg) return true;
			if (Rg->State(Jet) == AJetwayRigs::EState::Docked) return true;
			if (Rg->State(Jet) == AJetwayRigs::EState::Parked && FMath::Fmod(E, 30.f) < 0.05f && !Rg->Dock(Jet)) ADP_WARN(TEXT("GSE: the brake rider of %s waits in the cockpit - its jetway does not dock: %s"), *Reg, *AJetwayRigs::LastNote);
			return false;
		});
		Do(Ci, [this, Ci] { if (ATugs* G = Tg()) if (AActor* A = G->Crew[Ci].A.Get()) A->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); });
		Do(Ci, [this, Job] { Later([this, Job]
		{
			FSvcJob* X = SvcJ(Job); if (!X || !X->Men.Num()) return;
			SvcUp(Job, 0, false);
			const int32 C = X->Men[0].Ci;
			Do(C, [this, Job] { Later([this, Job] { if (FSvcJob* Y = SvcJ(Job)) if (Y->Men.Num()) { HiredWay(Y->Men[0].Ci, Tractors[Y->Ti].At, 0.f); Do(Y->Men[0].Ci, [this, Job] { Later([this, Job] { SvcLeave(Job); }); }); } }); });
			Wait(C, 0.5f);
		}); });
		Wait(Ci, 0.5f);
		return;
	}
	MaintStairs(Job, Sx, false);
}
