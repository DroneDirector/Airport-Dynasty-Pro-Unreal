// The hangar tow's brake rider (to-do 311 step B, docs/Hired-Crew-Plan.md "Hangar tow"). One hired mechanic (role mech)
// drives the maintenance van (GSEService.cpp table line "maint") from the hangar apron to the stand through the service
// job's own drive (SvcStart / SvcApron), parks it on the aircraft's right 1.5 m beyond the wingtip, walks round behind the
// tail to its left side and goes in at the front left door: up the jetway's service stair (SvcUp; the jetway docked for him
// first) or, where no jetway is, up the passenger stairs (called to the door if none is there; sent away once he is in).
// In the cockpit he is hidden and carried by the aircraft (no interiors yet); the tow waits for him (HangarTow.cpp). On the
// tow back he gets off at its stand the same ways and walks the hired people's way (HiredWay) to the van and drives it home.
// In the hangar hall he gets off and on by the hall's own stair (GSEHangarStair.cpp). The check crew: GSEMechanics.cpp.
#include "GSE.h"
#include "ServiceVehicles.h"
#include "Tugs.h"
#include "JetwayRigs.h"
#include "Airfield.h"
#include "CrewPose.h"
#include "ADPLog.h"
#include "Sim/ADPSim.h"
#include "HangarTow.h"
#include "SimTraffic.h"
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
		G->Crew[M->Ci].Hold = [](float, float, FDriverPose&) {};   // (carried: no idle steps back to where he went in)
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
			if (const FHallStair* Hh = HallOf(StandX))   // (aboard in the hangar hall for the tow back: carried from here)
			{
				ATugs* G = Tg(); AActor* A = G ? G->Crew[Ci].A.Get() : nullptr; AActor* Pl = Hh->Plane.Get();
				if (A && Pl) { A->AttachToActor(Pl, FAttachmentTransformRules::KeepWorldTransform); G->Crew[Ci].Hold = [](float, float, FDriverPose&) {}; }
				MaintHallX.Remove(Job); if (FSvcJob* X = SvcJ(Job)) X->Stage = 11;
				ADP_LOG(TEXT("GSE: the brake rider is in the cockpit again in hangar %s"), *Hh->Hall);
			}
			SvcGo(TEXT("stairs"), StandX);
		});
		MaintHold(Job);
		return;
	}
	Do(Ci, [this, Job, Ci]
	{
		if (ATugs* G = Tg()) if (AActor* A = G->Crew[Ci].A.Get()) { A->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); A->SetActorHiddenInGame(false); G->Crew[Ci].Hold = nullptr; G->Crew[Ci].Anchor = A->GetActorLocation(); }
		if (FSvcMan* X = SvcM(Job, 0)) X->bIn = false;
	});
	WalkTo(Ci, [W] { return W.Platform; }, 15.f);
	Climb(false); Level(false);
	Do(Ci, [this, Job, StandX] { MaintWay.Remove(Job); SvcGo(TEXT("stairs"), StandX); if (FSvcMan* X = SvcM(Job, 0)) X->bDown = true; });
	if (const FHallStair* Hh = HallOf(StandX))
	{
		// in the hangar hall: out from the stair's foot, along the side wall to behind its place, and waits there through the check
		const float Fy = float(-W.Foot.Y) / 100.f, Sd = Hh->Side, Hf = Hh->Half;
		const FVector P1 = Plan(Hh->Hx + Sd * (Hf - 6.f), Fy), P2 = HallP(*Hh, Sd * (Hf - 1.2f), 4.f), Pw = HallP(*Hh, Sd * (Hf - 1.2f), 18.f);
		WalkTo(Ci, [P1] { return P1; }, 40.f); WalkTo(Ci, [P2] { return P2; }, 40.f); WalkTo(Ci, [Pw] { return Pw; }, 25.f);
		Do(Ci, [this, Job] { if (FSvcJob* X = SvcJ(Job)) { X->Stage = 8; ADP_LOG(TEXT("GSE: %s's brake rider waits in the hall through the check"), *X->Reg); } });
		return;
	}
	Do(Ci, [this, Job] { Later([this, Job] { MaintToVan(Job); }); });
	Wait(Ci, 0.5f);
}

void AGSE::MaintHold(int32 Job)
{
	// in the cockpit through the tow; off in the hangar hall by its own stair (MaintHallX), or at the stand after the tow back
	FSvcJob* J = SvcJ(Job); if (!J || !J->Men.Num()) return;
	const int32 Ci = J->Men[0].Ci;
	WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->bGo || MaintHallX.Contains(Job); });   // (or down in the hangar hall)
	Do(Ci, [this, Job] { Later([this, Job] { MaintDown(Job); }); });
	Wait(Ci, 0.5f);
}

void AGSE::MaintDown(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J || !J->Men.Num()) return;
	if (const float* Hk = MaintHallX.Find(Job)) { J->Stage = 6; MaintStairs(Job, *Hk, false); return; }   // (down in the hangar hall by its own stair)
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
		Do(Ci, [this, Ci] { if (ATugs* G = Tg()) if (AActor* A = G->Crew[Ci].A.Get()) { A->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); G->Crew[Ci].Hold = nullptr; G->Crew[Ci].Anchor = A->GetActorLocation(); } });
		Do(Ci, [this, Job] { Later([this, Job]
		{
			FSvcJob* X = SvcJ(Job); if (!X || !X->Men.Num()) return;
			SvcUp(Job, 0, false);
			const int32 C = X->Men[0].Ci;
			Do(C, [this, Job] { Later([this, Job] { MaintToVan(Job); }); });
			Wait(C, 0.5f);
		}); });
		Wait(Ci, 0.5f);
		return;
	}
	MaintStairs(Job, Sx, false);
}

// ------------------------------------------------------------------ the maintenance vans' rides (to-do 311: nobody walks across the runway)
// Anyone of the hangar tow who has to get to the other side of the runway (the hall stair's crew, the brake rider) waits for a
// free maintenance van on his own side (logged), walks to it and gets in - the first at the wheel by the driver's door, up to
// three behind by the sliding door, one through it at a time - and rides. The van crosses only at the tows' crossing
// (AHangarTow::Crossing): it holds short there until nothing rolls, lands or takes off (ASimTraffic::RunwayBusy) and no tow is
// on the strip, then 10 s more; departures and tows hold while a van is on the strip (VanStrip, AHangarTow::OnRunway).

bool AGSE::Across(const FVector& A, const FVector& B) { return (ATugs::SideOf(A.Y) == 2) != (ATugs::SideOf(B.Y) == 2); }

int32 AGSE::VanFor(const FVector& Near) const
{
	int32 Best = INDEX_NONE; double Bd = 1e18;
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		const FTractor& T = Tractors[i];
		if (T.Svc == INDEX_NONE || !MtSpec(T.Svc).bMaint || T.Player.IsValid() || T.Path.Num() || VanTrip.Contains(i)) continue;
		bool bUsed = false;   // (a brake rider's job holds it: on its way, at the stand, or he sits in it)
		for (const TPair<int32, FSvcJob>& X : SvcJobs) if (X.Value.Ti == i && X.Value.Phase > 0) bUsed = true;
		int32 Bound = 0; for (const TPair<int32, FVanRider>& R : VanRiders) if (R.Value.Ti == i) ++Bound;
		if (bUsed || Bound >= 4) continue;
		const double D = FVector::Dist2D(T.At, Near) + (Across(T.At, Near) ? 1e9 : 0.0);   // (its own side first)
		if (D < Bd) { Bd = D; Best = i; }
	}
	return Best;
}

FVector AGSE::VanDropSouth(int32 Ti, float StandX) const
{
	if (const FVector* V = VanSouth.Find(Ti)) return *V;
	float S = 0.f, N = 0.f, Sp = 36.f;
	if (StandX != 0.f && StandGeo(StandX, S, N, Sp)) return PlanT(S, S - FMath::Max(MtSide, Sp * 0.5f + 1.5f), N + 8.f);
	FVector2f Cs, Cn; if (AHangarTow::Crossing(GetWorld(), Cs, Cn)) return Plan(Cs.X, Cs.Y - 25.f);
	return FVector::ZeroVector;
}

bool AGSE::VanRide(int32 Ci, const FVector& Drop, const FString& Who, TFunction<void()> Then)
{
	ATugs* G = Tg(); if (!G || !G->Crew.IsValidIndex(Ci) || Drop.IsNearlyZero()) return false;
	if (!Tractors.ContainsByPredicate([](const FTractor& T) { return T.Svc != INDEX_NONE && MtSpec(T.Svc).bMaint; })) return false;
	FVanRider& R = VanRiders.Add(Ci); R = FVanRider(); R.Drop = Drop; R.Who = Who;
	ADP_LOG(TEXT("GSE: %s (%s) waits for a maintenance van - nobody walks across the runway"), *G->Crew[Ci].Name, *Who);
	WaitFor(Ci, [this, Ci](float E)
	{
		FVanRider* X = VanRiders.Find(Ci); ATugs* H = Tg(); if (!X || !H || !H->Crew[Ci].A.IsValid()) return true;
		const FVector Me = H->Crew[Ci].A->GetActorLocation();
		const int32 V = VanFor(Me);
		if (V != INDEX_NONE && !Across(Tractors[V].At, Me))
		{
			TSet<int32> Used; for (const TPair<int32, FVanRider>& Y : VanRiders) if (Y.Value.Ti == V) Used.Add(Y.Value.Seat);
			for (int32 s = 0; s < 4; ++s) if (!Used.Contains(s)) { X->Ti = V; X->Seat = s; break; }
			if (X->Ti != INDEX_NONE) { ADP_LOG(TEXT("GSE: %s goes to the %s (seat %d)"), *H->Crew[Ci].Name, *Tractors[V].Name, X->Seat); return true; }
		}
		if (E > 1.f && FMath::Fmod(E, 30.f) < 0.05f) ADP_LOG(TEXT("GSE: %s still waits for a free maintenance van on this side of the runway"), *H->Crew[Ci].Name);
		return false;
	});
	Do(Ci, [this, Ci, Then] { Later([this, Ci, Then]
	{
		const FVanRider* X = VanRiders.Find(Ci); if (!X || X->Ti == INDEX_NONE) { VanRiders.Remove(Ci); if (Then) Then(); return; }
		VanBoard(Ci);
		WaitFor(Ci, [this, Ci](float) { const FVanRider* Y = VanRiders.Find(Ci); return !Y || Y->bOff; });
		Do(Ci, [this, Ci, Then] { Later([this, Ci, Then] { VanAlight(Ci, Then); }); });
	}); });
	return true;
}

void AGSE::VanBoard(int32 Ci)
{
	const FVanRider* X = VanRiders.Find(Ci); if (!X) return;
	const int32 Ti = X->Ti, m = X->Seat;
	if (m == 0) { ClimbIn(Ci, Ti, [this, Ci] { if (FVanRider* Y = VanRiders.Find(Ci)) Y->bAboard = true; }); return; }
	const FVector Stand = SvcP(Tractors[Ti], MtSpec(Tractors[Ti].Svc).Doors[0].Stand);
	ClaimSeat(Ci, Ti, m);
	WalkTo(Ci, [this, Ti, Stand] { return TW(Tractors[Ti], Stand + FVector(-150.f, 0.f, 0.f)); }, 60.f);
	// (one through the sliding door at a time)
	WaitFor(Ci, [this, Ci, Ti](float) { for (const TPair<int32, FVanRider>& Y : VanRiders) if (Y.Key != Ci && Y.Value.Ti == Ti && Y.Value.bDoor) return false; if (FVanRider* Me = VanRiders.Find(Ci)) Me->bDoor = true; return true; });
	WalkTo(Ci, [this, Ti, Stand] { return TW(Tractors[Ti], Stand); }, 15.f);
	ClaimSeat(Ci, Ti, m); Do(Ci, [this, Ci] { SeatTaken(Ci); });
	Do(Ci, [this, Ti] { FTractor& V = Tractors[Ti]; if (V.SvcDoors[0].Want < 0.5f && !V.SndDoorOpen.IsEmpty()) Sound(*V.SndDoorOpen, TW(V, V.SvcDoors[0].Piv)); V.SvcDoors[0].Want = 1.f; });
	WaitFor(Ci, [this, Ti](float E) { return E > 5.f || Tractors[Ti].SvcDoors[0].Now > 0.99f; });
	PoseAct(Ci, SvcClimbFn(Ti, m, true), 5.f, true);
	Do(Ci, [this, Ci, Ti, m] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = SvcSeatFn(Ti, m); if (FVanRider* Y = VanRiders.Find(Ci)) { Y->bAboard = true; Y->bDoor = false; } });
}

void AGSE::VanAlight(int32 Ci, TFunction<void()> Then)
{
	const FVanRider* X = VanRiders.Find(Ci); if (!X) { if (Then) Then(); return; }
	const int32 Ti = X->Ti, m = X->Seat;
	auto Done = [this, Ci, Then] { Do(Ci, [this, Ci, Then] { VanRiders.Remove(Ci); if (Then) Later(Then); }); };
	if (m == 0) { ClimbOut(Ci, Ti, nullptr); Done(); return; }
	const FVector Off = SvcP(Tractors[Ti], MtSpec(Tractors[Ti].Svc).Doors[0].Stand);
	WaitFor(Ci, [this, Ci, Ti](float) { for (const TPair<int32, FVanRider>& Y : VanRiders) if (Y.Key != Ci && Y.Value.Ti == Ti && Y.Value.bDoor) return false; if (FVanRider* Me = VanRiders.Find(Ci)) Me->bDoor = true; return true; });
	Do(Ci, [this, Ti] { FTractor& V = Tractors[Ti]; if (V.SvcDoors[0].Want < 0.5f && !V.SndDoorOpen.IsEmpty()) Sound(*V.SndDoorOpen, TW(V, V.SvcDoors[0].Piv)); V.SvcDoors[0].Want = 1.f; });
	WaitFor(Ci, [this, Ti](float E) { return E > 4.f || Tractors[Ti].SvcDoors[0].Now > 0.99f; });
	Do(Ci, [this, Ci] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = nullptr; });
	PoseAct(Ci, SvcClimbFn(Ti, m, false), 4.4f, true);
	Do(Ci, [this, Ci] { SeatLeft(Ci); });
	WalkTo(Ci, [this, Ti, Off, m] { return TW(Tractors[Ti], Off + FVector(-110.f * m, 0.f, 0.f)); }, 30.f);
	Do(Ci, [this, Ci, Ti]
	{
		if (FVanRider* Y = VanRiders.Find(Ci)) { Y->bDoor = false; Y->bAboard = false; }
		bool bAny = false; for (const TPair<int32, FVanRider>& Y : VanRiders) if (Y.Key != Ci && Y.Value.Ti == Ti && Y.Value.Seat > 0 && Y.Value.bAboard) bAny = true;
		if (!bAny) Tractors[Ti].SvcDoors[0].Want = 0.f;   // (the last one out shuts it)
	});
	Done();
}

void AGSE::VanTick()
{
	// a van whose riders are all in leaves for its driver's drop; once all are out it is free again
	TSet<int32> Vans; for (const TPair<int32, FVanRider>& R : VanRiders) if (R.Value.Ti != INDEX_NONE) Vans.Add(R.Value.Ti);
	for (auto It = VanTrip.CreateIterator(); It; ++It) if (It->Value == 2 && !Vans.Contains(It->Key))
	{
		FTractor& T = Tractors[It->Key];
		if (const TPair<int32, EJob>* W = VanWas.Find(It->Key)) { T.Driver = W->Key; T.Job = W->Value; }
		VanWas.Remove(It->Key);
		ADP_LOG(TEXT("GSE: the %s is free again at plan (%.0f, %.0f)"), *T.Name, T.At.X / 100.f, -T.At.Y / 100.f);
		It.RemoveCurrent();
	}
	for (const int32 Ti : Vans)
	{
		if (VanTrip.Contains(Ti) || !Tractors.IsValidIndex(Ti)) continue;
		int32 Drv = INDEX_NONE; bool bAll = true;
		for (const TPair<int32, FVanRider>& R : VanRiders) if (R.Value.Ti == Ti) { if (!R.Value.bAboard) bAll = false; if (R.Value.Seat == 0) Drv = R.Key; }
		if (!bAll || Drv == INDEX_NONE) continue;
		FTractor& T = Tractors[Ti];
		T.SvcDoors[0].Want = 0.f; if (T.SvcDoors[0].Now > 0.01f) continue;   // (shut first)
		const FVector Drop = VanRiders[Drv].Drop;
		VanWas.Add(Ti, TPair<int32, EJob>(T.Driver, T.Job));
		if (ATugs::SideOf(T.At.Y) != 2) VanSouth.Add(Ti, T.At);   // (taken on the stands' side: back here later)
		T.Driver = Drv; T.Job = EJob::Out; Motor(T, true); VanTrip.Add(Ti, 1);
		int32 N = 0; for (const TPair<int32, FVanRider>& R : VanRiders) if (R.Value.Ti == Ti) ++N;
		ADP_LOG(TEXT("GSE: the %s leaves with %d aboard for plan (%.0f, %.0f)%s"), *T.Name, N, Drop.X / 100.f, -Drop.Y / 100.f, Across(T.At, Drop) ? TEXT(" across the runway") : TEXT(""));
		VanCross(Ti, Drop, [this, Ti]
		{
			FTractor& V = Tractors[Ti]; Motor(V, false); VanTrip.Add(Ti, 2);
			for (TPair<int32, FVanRider>& R : VanRiders) if (R.Value.Ti == Ti) R.Value.bOff = true;
			ADP_LOG(TEXT("GSE: the %s stops - its riders get out"), *V.Name);
		});
	}
}

void AGSE::VanCross(int32 Ti, const FVector& To, TFunction<void()> Done)
{
	FTractor& T = Tractors[Ti]; TArray<FVector> W;
	if (!Across(T.At, To))
	{
		if (FVector::Dist2D(T.At, To) < 300.f) { if (Done) Done(); return; }
		if (FVector::Dist2D(T.At, T.Home) < 200.f) HomeOut(T, W, To);
		if (!Apron(W, W.Num() ? W.Last() : T.At, To, &T)) { ADP_LOG(TEXT("GSE: the %s finds no way to plan (%.0f, %.0f) - tries again"), *T.Name, To.X / 100.f, -To.Y / 100.f); RetryIn(Ti, 8.f, [this, Ti, To, Done] { VanCross(Ti, To, Done); }); return; }
		Go(T, MoveTemp(W), false, Done); return;
	}
	FVector2f Cs, Cn;
	if (!AHangarTow::Crossing(GetWorld(), Cs, Cn)) { ADP_WARN(TEXT("GSE: the %s waits - no tow crossing of the runway is known yet"), *T.Name); RetryIn(Ti, 15.f, [this, Ti, To, Done] { VanCross(Ti, To, Done); }); return; }
	const bool bNorth = ATugs::SideOf(T.At.Y) == 2; const FVector2f A = bNorth ? Cn : Cs, B = bNorth ? Cs : Cn;
	if (FVector::Dist2D(T.At, T.Home) < 200.f) HomeOut(T, W, Plan(A.X, A.Y));
	if (!Apron(W, W.Num() ? W.Last() : T.At, Plan(A.X, A.Y), &T)) { ADP_LOG(TEXT("GSE: the %s finds no way to the runway crossing - tries again"), *T.Name); RetryIn(Ti, 8.f, [this, Ti, To, Done] { VanCross(Ti, To, Done); }); return; }
	Go(T, MoveTemp(W), false, [this, Ti, A, B, To, Done]
	{
		FTractor& V = Tractors[Ti]; VanClear.Add(Ti, -1.0);
		ADP_LOG(TEXT("GSE: the %s holds short of the runway at the tows' crossing"), *V.Name);
		V.Until = [this, Ti]
		{
			const double Now = GetWorld()->GetTimeSeconds(); double& C = VanClear.FindOrAdd(Ti);
			if (ASimTraffic::RunwayBusy(GetWorld()) || AHangarTow::OnRunway(GetWorld(), false)) { C = -1.0; return false; }
			if (C < 0.0) C = Now;
			return Now - C >= 10.0;
		};
		V.AfterUntil = [this, Ti, A, B, To, Done]
		{
			FTractor& U = Tractors[Ti]; VanStrip.Add(Ti); VanClear.Remove(Ti);
			ADP_LOG(TEXT("GSE: the %s crosses the runway - departures hold"), *U.Name);
			TArray<FVector> X; for (int32 i = 1; i <= 24; ++i) { const FVector2f Q = FMath::Lerp(A, B, i / 24.f); X.Add(Plan(Q.X, Q.Y)); }
			Go(U, MoveTemp(X), false, [this, Ti, To, Done]
			{
				VanStrip.Remove(Ti); ADP_LOG(TEXT("GSE: the %s is off the runway"), *Tractors[Ti].Name);
				VanCross(Ti, To, Done);
			});
		};
	});
}

void AGSE::MaintToVan(int32 Job)
{
	// the brake rider back to his van; across the runway from it (it was left there): over in another van, never on foot
	FSvcJob* X = SvcJ(Job); ATugs* G = Tg(); if (!X || !X->Men.Num() || !G) return;
	const int32 Ci = X->Men[0].Ci; const FVector Van = Tractors[X->Ti].At;
	const bool bOver = G->Crew.IsValidIndex(Ci) && G->Crew[Ci].A.IsValid() && Across(G->Crew[Ci].A->GetActorLocation(), Van);
	auto Walk = [this, Job] { if (FSvcJob* Y = SvcJ(Job)) if (Y->Men.Num()) { HiredWay(Y->Men[0].Ci, Tractors[Y->Ti].At, 0.f); Do(Y->Men[0].Ci, [this, Job] { Later([this, Job] { SvcLeave(Job); }); }); } };
	if (bOver && VanRide(Ci, Van + FVector(0.f, 900.f, 0.f), TEXT("brake rider"), Walk)) return;
	Walk();
}
