// Departing passengers by foot or by bus to the aircraft (to-do 277): out of a ground gate's door to the apron, the marked
// straight way to the passenger stairs or an apron bus with its hired driver, up the treads one by one, gone at the top.
// A passenger's phase (FPaxOut): 0 to the bus's door / queueing, 2 getting in, 3 in their place, 4 getting out, 5 to / at the
// foot of the stairs, 6 on the treads, 7 back to the gate, 8 out of the gate (by foot).
// A flight's bus (FRunOut::Stage): 0 none yet, 1 to the gate, 2 boarding, 3 to the aircraft, 4 passengers get out, 5 back at
// the gate, they get out there (the flight is off), 6 done.
#include "PaxFlow.h"
#include "ADPLog.h"
#include "People.h"
#include "Seats.h"
#include "GSE.h"
#include "Airfield.h"
#include "CrewPose.h"
#include "TruckDriverRig.h"
#include "DriverPoseAnim.h"
#include "PaxAnimInstance.h"
#include "TerminalPlan.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPGround.h"
#include "Animation/BlendSpace.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// walking with planted feet from the ground into the bus and to a place, or back (bus cm)
	struct FPaWalk
	{
		TArray<FVector2D> P; float Len = 0.f;
		void Set(TArray<FVector2D>&& In) { P = MoveTemp(In); Len = 0.f; for (int32 i = 1; i < P.Num(); ++i) Len += float(FVector2D::Distance(P[i - 1], P[i])); }
		FVector2D At(float S) const
		{
			S = FMath::Clamp(S, 0.f, Len);
			for (int32 i = 1; i < P.Num(); ++i) { const float D = float(FVector2D::Distance(P[i - 1], P[i])); if (S <= D || i == P.Num() - 1) return FMath::Lerp(P[i - 1], P[i], FMath::Clamp(S / FMath::Max(D, 0.01f), 0.f, 1.f)); S -= D; }
			return P.Num() ? P[0] : FVector2D::ZeroVector;
		}
		FVector2D Dir(float S) const { const FVector2D D = (At(FMath::Min(S + 18.f, Len)) - At(FMath::Max(S - 18.f, 0.f))).GetSafeNormal(); return D.IsNearlyZero() && P.Num() > 1 ? (P.Last() - P[P.Num() - 2]).GetSafeNormal() : D; }
		FVector Foot(float S, int32 Side) const
		{
			const FVector2D A = At(S), D = Dir(S); FVector F(A.X, A.Y, 0.f);
			F += FVector(D.Y, -D.X, 0.f) * (Side == 0 ? 9.f : -9.f);
			const double Sg = F.Y < 0. ? -1. : 1., Y = FMath::Abs(F.Y);
			if (Y < 153.) { F.Y = Sg * FMath::Min(Y, 132.); F.Z = 33.; } else F.Y = Sg * FMath::Max(Y, 166.);
			return F;
		}
		void Pose(float S, float PH, FDriverPose& Q) const
		{
			S = FMath::Clamp(S, 0.f, Len);
			const float Step = Len / FMath::Max(1, FMath::RoundToInt(Len / 52.f));
			const float U = S / FMath::Max(Step, 1.f) + 0.5f; const int32 n = FMath::FloorToInt(U); const float f = U - n;
			Q = FDriverPose(); Q.Facing = FVector(Dir(S), 0.f);
			const FVector Stance = Foot(n * Step, n & 1), From = Foot((n - 1) * Step, (n + 1) & 1), To = Foot((n + 1) * Step, (n + 1) & 1);
			FVector Sw = FMath::Lerp(From, To, FMath::SmoothStep(0.1f, 0.9f, f)); const double Dz = To.Z - From.Z;
			Sw.Z = From.Z + Dz * (Dz >= 0. ? FMath::SmoothStep(0.f, 0.45f, f) : FMath::SmoothStep(0.55f, 1.f, f)) + (From.Equals(To, 1.f) ? 0.f : 8.f * FMath::Sin(PI * f));
			Q.Foot[n & 1] = Stance; Q.Foot[(n + 1) & 1] = Sw;
			const FVector2D C = At(S); const float Zs = FMath::Lerp(33.f, 0.f, FMath::SmoothStep(112.f, 172.f, float(FMath::Abs(C.Y))));
			Q.Pelvis = FVector(C.X, C.Y, 0.f);
			const float D2 = float(FVector::DistSquared2D(Q.Pelvis, Stance)), Reach = FMath::Sqrt(FMath::Max(PH * PH - D2, PH * PH * 0.4f));
			Q.Pelvis.Z = FMath::Min(Zs + PH - 2.f, float(Stance.Z) + Reach - 1.f); Q.Lean = FMath::Abs(Dz) > 5. ? 8.f : 3.f;
		}
	};
	constexpr double PaPoles[7] = {-610., -430., -270., -50., 90., 290., 440.};   // (the aisle's poles, y 62 either side)
	double PaCross(double X) { for (double Px : PaPoles) if (FMath::Abs(Px - X) < 32.) return Px + (X >= Px ? 32. : -32.); return X; }   // (not through a pole)
	float PaYaw(const FVector& Dir) { return float(Dir.Rotation().Yaw) - 90.f; }
	UBlendSpace* PaBS() { static UBlendSpace* B = [] { UBlendSpace* S = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/People/Anims/BS_Idle_Walk_Run_MH.BS_Idle_Walk_Run_MH")); if (S) S->AddToRoot(); return S; }(); return B; }
}

struct FPaxOut
{
	int32 Ph = 0, Door = 0, Bus = INDEX_NONE, K = -1; bool bBack = false, bRider = false;
	int32 Back = 0; float T = 0.f, S = 0.f; AGSE::FBusSlot Slot; FPaWalk W; FDriverPose From;
	TUniquePtr<FTruckDriverRig> Rig; TWeakObjectPtr<UPoseableMeshComponent> Mesh; TWeakObjectPtr<UDriverPoseAnim> Anim; FTransform BodyInActor; bool bPosed = false;
};

struct FRunOut
{
	FString Key, Stand, Why; int32 Gate = 0, Ti = INDEX_NONE, Cand = 0, Trips = 0; float StandX = 0.f; const FAirfieldData::FStand* S = nullptr;
	uint8 Stage = 0; bool bThere = false, bStairs = false, bBack = false, bTold = false, bStuck = false;
	float T = 0.f, StairT = 0.f, UpT = 99.f, SlotT = 0.f, StageT = 0.f, SaidT = 0.f, DoorX = 0.f, Side = 1.f; TArray<float> Doors;
	FVector2f Pick = FVector2f::ZeroVector; AGSE::FStairsWay Way;
	TArray<int32> Q[7], Stair; int32 Lock[7] = {-1, -1, -1, -1, -1, -1, -1}; TArray<AGSE::FBusSlot> Slots;
	// the door on the bus's open side whose part of the bus that place is in
	int32 DoorOf(double X) const
	{
		int32 B = -1; for (int32 d = 0; d < 7; ++d) if (AGSE::BusDoorAt(d).Y * Side > 0.f && (B < 0 || FMath::Abs(AGSE::BusDoorAt(d).X - X) < FMath::Abs(AGSE::BusDoorAt(B).X - X))) B = d;
		return B;
	}
};

int32 APaxFlow::PaxGate(const FString& Stand, const FString& Reg)
{
	const int32 G = Stand.IsEmpty() ? 0 : ADPGround::GateOfStand(Stand);
	if (G == 0) return ADPBoards::GateFor(Reg);
	return ADPGround::PaxWay(Stand) == EADPPaxWay::Bus ? ADPBoards::GateFor(Reg) : G;   // (by bus: a ground gate with a door to the apron, as on the boards)
}

void APaxFlow::SetStand(FRun& R, const FString& Stand)
{
	R.Stand = Stand; R.Way = Stand.IsEmpty() ? 0 : uint8(ADPGround::PaxWay(Stand));
	if (R.Way) ADP_LOG(TEXT("PaxFlow: %s's passengers go %s to stand %s"), *R.Label, EADPPaxWay(R.Way) == EADPPaxWay::Walk ? TEXT("by foot") : TEXT("by bus"), *Stand);
}

int32 APaxFlow::ApronCount(int32 Ri, int32 From, int32 To, int32 Door) const
{
	int32 n = 0; for (const TPair<int32, TSharedPtr<FPaxOut>>& O : Outs) if (Pax.IsValidIndex(O.Key) && Pax[O.Key].Run == Ri && O.Value->Ph >= From && O.Value->Ph <= To && (Door < 0 || O.Value->Door == Door)) ++n;
	return n;
}

FPaxOut* APaxFlow::ApronNew(int32 i) { TSharedPtr<FPaxOut> O = MakeShared<FPaxOut>(); Outs.Add(i, O); return O.Get(); }

void APaxFlow::ApronPose(int32 i, const FDriverPose& W)
{
	TSharedPtr<FPaxOut> Op = Outs.FindRef(i); FPx& P = Pax[i]; AActor* A = P.A.Get(); USkeletalMeshComponent* B = P.Body.Get(); if (!Op || !A || !B) return;
	FPaxOut& O = *Op; P.Speed = 0.f;
	if (!O.Rig)
	{
		UPoseableMeshComponent* M = NewObject<UPoseableMeshComponent>(this); M->SetupAttachment(RootComponent); M->SetVisibility(false); M->RegisterComponent();
		M->SetSkinnedAssetAndUpdate(B->GetSkeletalMeshAsset()); O.Mesh = M; O.Rig = MakeUnique<FTruckDriverRig>();
		if (!O.Rig->Init(M)) ADP_WARN(TEXT("PaxFlow: %s has no posed body (its skeleton)"), *P.Name);
	}
	if (!O.bPosed) { O.bPosed = true; O.BodyInActor = B->GetComponentTransform().GetRelativeTransform(A->GetActorTransform()); B->SetAnimInstanceClass(UDriverPoseAnim::StaticClass()); O.Anim = Cast<UDriverPoseAnim>(B->GetAnimInstance()); }
	const FTransform Fr(FRotator(0.f, PaYaw(W.Facing.GetSafeNormal2D()), 0.f), FVector(W.Pelvis.X, W.Pelvis.Y, FMath::Min(W.Foot[0].Z, W.Foot[1].Z)));
	FDriverPose L = W; L.Facing = FVector(0, 1, 0); L.Pelvis = Fr.InverseTransformPosition(W.Pelvis);
	for (int32 s = 0; s < 2; ++s) { L.Foot[s] = Fr.InverseTransformPosition(W.Foot[s]); L.Hand[s] = Fr.InverseTransformPosition(W.Hand[s]); }
	O.Rig->Pose(L); if (UDriverPoseAnim* An = O.Anim.Get()) An->Local = O.Rig->LocalPose();
	A->SetActorTransform(O.BodyInActor.Inverse() * Fr); P.PrevLoc = A->GetActorLocation();
}

void APaxFlow::ApronUnpose(int32 i)
{
	TSharedPtr<FPaxOut> O = Outs.FindRef(i); if (!O) return; FPx& P = Pax[i]; AActor* A = P.A.Get();
	if (O->bPosed && A) if (USkeletalMeshComponent* B = P.Body.Get())
	{
		B->SetAnimationMode(EAnimationMode::AnimationSingleNode); if (UBlendSpace* S = PaBS()) { B->SetAnimation(S); B->Play(true); } UPaxAnimInstance::Install(B);
		A->SetActorRotation(FRotator(0.f, A->GetActorRotation().Yaw, 0.f)); P.PrevLoc = A->GetActorLocation();
	}
	O->bPosed = false; O->Anim = nullptr; if (UPoseableMeshComponent* M = O->Mesh.Get()) M->DestroyComponent(); O->Mesh = nullptr; O->Rig.Reset();
}

void APaxFlow::ApronGone(int32 i)
{
	TSharedPtr<FPaxOut> O = Outs.FindRef(i); if (!O) return;
	if (AActor* A = Pax[i].A.Get()) { ASeats::Release(A); if (ApronG && O->bRider) ApronG->BusRider(O->Bus, A, false); }
	if (ApronG && O->Ph == 6) ApronG->StairsEnter(O->K, -1);
	if (UPoseableMeshComponent* M = O->Mesh.Get()) M->DestroyComponent();
	for (TPair<FString, TSharedPtr<FRunOut>>& R : RunOuts) { R.Value->Stair.Remove(i); for (int32 d = 0; d < 7; ++d) { R.Value->Q[d].Remove(i); if (R.Value->Lock[d] == i) R.Value->Lock[d] = -1; } }
	Outs.Remove(i);
}

void APaxFlow::ApronWalk(FPx& P, const FVector& To) { FWp W; W.P = To; W.bGround = true; W.bNav = true; P.Way.Add(W); P.St = EPx::Apron; }

bool APaxFlow::ApronStairs(FRunOut& O) const
{
	if (ApronG && O.StairT <= 0.f) { O.StairT = 0.3f; O.bStairs = ApronG->StairsReady(O.StandX, 0, O.Way) && O.Way.Steps.Num() > 2; }
	return O.bStairs;
}

// the queue at the stairs' foot, to the right of the way up
FVector APaxFlow::ApronQueue(const FRunOut& O, int32 k) const
{
	const FVector R(-O.Way.Up.Y, O.Way.Up.X, 0.f), V = O.Way.Steps[0] - O.Way.Up * (30.f + 78.f * k) + R * 26.f;
	return ApronG->ApronAt(float(V.X) / 100.f, float(-V.Y) / 100.f);
}

// beside the bus, out from a door's middle
FVector APaxFlow::ApronDoorSpot(const FRunOut& O, int32 Door, float Out) const
{
	const FVector2f D = AGSE::BusDoorAt(Door); const FVector V = ApronG->BusW(O.Ti, FVector(D.X, FMath::Sign(D.Y) * (130.f + Out), 0.f));
	return ApronG->ApronAt(float(V.X) / 100.f, float(-V.Y) / 100.f);
}

bool APaxFlow::ApronCall(int32 i)
{
	FPx& P = Pax[i]; FRun& R = Runs[P.Run]; AActor* A = P.A.Get(); TSharedPtr<FRunOut> Rp = RunOuts.FindRef(R.Key); if (!A || !Rp || !ApronG) return false;
	FRunOut& O = *Rp; const bool bWalk = EADPPaxWay(R.Way) == EADPPaxWay::Walk; int32 Door = 0;
	if (!O.S || !O.Doors.Num()) return false;   // (said once when the flight was called)
	if (bWalk)
	{
		if (!ApronStairs(O)) { if (!O.bTold) { O.bTold = true; ADP_LOG(TEXT("PaxFlow: %s's boarding waits at gate %d for the passenger stairs on stand %s"), *R.Label, R.Gate, *O.Stand); } return false; }
		if (ApronCount(P.Run, 0, 8) >= 7) return false;
		O.DoorX = O.Doors[0]; for (float X : O.Doors) if (FMath::Abs(X * 100.f - O.Way.Foot.X) < FMath::Abs(O.DoorX * 100.f - O.Way.Foot.X)) O.DoorX = X;
	}
	else
	{
		if (O.Stage != 2 || O.Slots.Num() <= ApronCount(P.Run, 0, 1)) return false;
		int32 Best = 3; Door = -1;
		for (int32 d = 0; d < 7; ++d) { if (AGSE::BusDoorAt(d).Y * O.Side < 0.f || !ApronG->BusDoorIs(O.Ti, d, true)) continue; const int32 n = ApronCount(P.Run, 0, 1, d); int32 Pl = 0; for (const AGSE::FBusSlot& S : O.Slots) if (O.DoorOf(S.At.X) == d) ++Pl; if (n < Best && n < Pl) { Best = n; Door = d; } }
		if (Door < 0) return false;
	}
	FPaxOut& X = *ApronNew(i); X.Door = Door; X.Ph = bWalk ? 8 : 0; X.Bus = O.Ti;
	P.Seat = INDEX_NONE; ASeats::Release(A); P.Way.Reset(); P.Wi = 0; P.PauseT = 0.f;
	AddPlanWay(P, INDEX_NONE, PlanOf(A->GetActorLocation()), INDEX_NONE, FVector(O.DoorX, 0.f, 80.5f), false);
	for (float Z : {82.9f, 84.6f}) { FWp W; W.P = ApronG->ApronAt(O.DoorX, Z); W.bGround = true; W.bNav = true; P.Way.Add(W); }
	P.Do = EDo::ApronOut; P.St = EPx::Walk; P.Stage = bWalk ? TEXT("out of the ground gate, on foot to the aircraft") : TEXT("out of the ground gate to the bus");
	return true;
}

// out of the gate's door
void APaxFlow::ApronArrived(int32 i)
{
	FPx& P = Pax[i]; FRun& R = Runs[P.Run]; TSharedPtr<FPaxOut> X = Outs.FindRef(i); TSharedPtr<FRunOut> O = RunOuts.FindRef(R.Key);
	if (!X || !O) { ApronGone(i); GoHome(i, TEXT("missed the flight")); return; }
	if (R.bOff || R.bGone) { ApronBack(i); return; }
	P.St = EPx::Apron; P.Way.Reset(); P.Wi = 0;
	if (X->Ph == 8)
	{
		if (!ApronStairs(*O)) return;   // (StepApron asks again)
		// (the marked straight way: straight out from the door, then square across to the stairs' foot)
		const FVector F = PlanOf(O->Way.Foot); const float Ax = float(F.X - O->Way.Up.X * 6.5), Az = float(F.Z - 4.0);
		ApronWalk(P, ApronG->ApronAt(O->DoorX, Az)); ApronWalk(P, ApronG->ApronAt(Ax, Az));
		X->Ph = 5; O->Stair.AddUnique(i); X->K = O->Stair.Find(i); ApronWalk(P, ApronQueue(*O, X->K));
		return;
	}
	O->Q[X->Door].AddUnique(i); X->K = O->Q[X->Door].Find(i);
	const FVector Dp = PlanOf(ApronDoorSpot(*O, X->Door, 70.f));
	ApronWalk(P, ApronG->ApronAt(float(Dp.X), 85.f)); ApronWalk(P, ApronDoorSpot(*O, X->Door, 70.f + 75.f * X->K));
}

// back to the gate and in again (the flight is off, or it left)
void APaxFlow::ApronBack(int32 i)
{
	FPx& P = Pax[i]; TSharedPtr<FPaxOut> X = Outs.FindRef(i); TSharedPtr<FRunOut> O = RunOuts.FindRef(Runs[P.Run].Key); if (!X || !O || !P.A.IsValid()) return;
	for (int32 d = 0; d < 7; ++d) O->Q[d].Remove(i);
	O->Stair.Remove(i);
	const FVector L = PlanOf(P.A->GetActorLocation());
	X->Ph = 7; X->bBack = true; P.Way.Reset(); P.Wi = 0; P.bHome = true;
	if (L.Z > 95.) { ApronWalk(P, ApronG->ApronAt(float(L.X), float(L.Z) - 4.f)); ApronWalk(P, ApronG->ApronAt(O->DoorX, float(L.Z) - 4.f)); }
	else ApronWalk(P, ApronG->ApronAt(float(L.X), 85.f));
	for (float Z : {84.6f, 82.9f}) ApronWalk(P, ApronG->ApronAt(O->DoorX, Z));
	FWp W; W.P = PW(O->DoorX, 0.f, 80.5f); P.Way.Add(W); P.Stage = TEXT("back to the gate");
	ADP_LOG(TEXT("PaxFlow: %s goes back to gate %d from the apron (%s)"), *P.Name, O->Gate, Runs[P.Run].bOff ? TEXT("the flight is off") : TEXT("the flight left"));
}

void APaxFlow::StepApron(int32 i, float Dt)
{
	FPx& P = Pax[i]; FRun& R = Runs[P.Run]; AActor* A = P.A.Get(); TSharedPtr<FPaxOut> Xp = Outs.FindRef(i); TSharedPtr<FRunOut> Op = RunOuts.FindRef(R.Key);
	if (!A || !ApronG) return;
	if (!Xp || !Op) { if (!P.bHome) { ADP_WARN(TEXT("PaxFlow: %s stands on the apron without a flight - goes home"), *P.Name); GoHome(i, TEXT("missed the flight")); } return; }
	FPaxOut& X = *Xp; FRunOut& O = *Op; X.T += Dt; const FVector L = A->GetActorLocation(); AGSE* G = ApronG;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController(); const APawn* You = PC ? PC->GetPawn() : nullptr;
	const FVector YouAt = You ? You->GetActorLocation() : FVector(1e9);
	auto YouIn = [&](const FVector& W) { return FVector::Dist2D(W, YouAt) < 62. && FMath::Abs(W.Z - YouAt.Z) < 160.; };
	auto Ground = [&](const FVector& To) { P.Way.Reset(); P.Wi = 0; ApronWalk(P, To); };
	// on their feet, outside: true when there
	auto Go = [&]() -> bool
	{
		if (!P.Way.IsValidIndex(P.Wi)) { Idle(P, A, Dt); return true; }
		const FVector D = (P.Way[P.Wi].P - L).GetSafeNormal2D();
		if (G->Moving(L + D * 150.f)) { Idle(P, A, Dt); return false; }   // (a vehicle comes past)
		const bool bEnd = Follow(P, A, Dt);
		FVector N = A->GetActorLocation(); const FVector Pl = PlanOf(N); if (Pl.Z > 82.4) { N.Z = G->ApronAt(float(Pl.X), float(Pl.Z)).Z; A->SetActorLocation(N); }
		if (bEnd) { P.Way.Reset(); P.Wi = 0; } else Held(i, Dt);
		return bEnd;
	};
	const bool bNo = R.bOff || R.bGone;
	switch (X.Ph)
	{
	case 8: if (bNo) ApronBack(i); else if (ApronStairs(O)) ApronArrived(i); else Idle(P, A, Dt); break;
	case 7: if (Go()) { ApronGone(i); P.Do = EDo::None; GoHome(i, R.bOff ? TEXT("the flight is off") : TEXT("missed the flight")); } break;
	case 0:
	{
		if (bNo) { ApronBack(i); break; }
		if (O.Stage != 2)
		{
			// (it left without room for them: back to the wall, clear of where it comes to stand again)
			if (X.K != -2) { X.K = -2; X.Back = FMath::Max(0, O.Q[X.Door].Find(i)); const FVector Lp = PlanOf(L); Ground(G->ApronAt(float(Lp.X) + 0.8f, 86.9f - 0.8f * X.Back)); }
			Go(); P.Stage = TEXT("waiting: the bus comes back"); break;
		}
		const int32 k = O.Q[X.Door].Find(i); if (k != X.K && k >= 0) { X.K = k; Ground(ApronDoorSpot(O, X.Door, 70.f + 75.f * k)); }
		if (!Go() || k != 0 || O.Lock[X.Door] >= 0) break;
		if (FVector::Dist2D(A->GetActorLocation(), ApronDoorSpot(O, X.Door, 70.f)) > 45.f) { Ground(ApronDoorSpot(O, X.Door, 70.f)); break; }
		AGSE::FBusSee See; const bool bSee = G->BusSee(O.Ti, See); const FVector2f Dr = AGSE::BusDoorAt(X.Door);
		if (!bSee || !See.bStands || !See.bAtWheel || !G->BusDoorIs(O.Ti, X.Door, true) || !See.bKnelt || YouIn(G->BusW(O.Ti, FVector(Dr.X, Dr.Y, 33.)))) { P.Stage = TEXT("waiting at the bus's door"); break; }
		// a place in this door's part of the bus: the window seats first, then the others, then standing with something to hold
		int32 Got = INDEX_NONE;
		for (int32 n = 0; n < O.Slots.Num() && Got == INDEX_NONE; ++n)
		{
			const AGSE::FBusSlot& S = O.Slots[n]; if (O.DoorOf(S.At.X) != X.Door) continue;
			if (S.Seat == INDEX_NONE) { Got = n; break; }
			if (ASeats::IsTaken(S.Seat)) continue;
			if (ASeats::Claim(A, G->BusW(O.Ti, S.At)) == S.Seat) Got = n; else ASeats::Release(A);
		}
		if (Got == INDEX_NONE)
		{
			int32 To = -1; for (const AGSE::FBusSlot& S : O.Slots) { const int32 d = O.DoorOf(S.At.X); if (d != X.Door && d >= 0 && G->BusDoorIs(O.Ti, d, true) && O.Q[d].Num() < 3) { To = d; break; } }
			if (To < 0) { P.Stage = TEXT("waiting: the bus is full"); break; }
			O.Q[X.Door].Remove(i); X.Door = To; O.Q[To].Add(i); X.K = O.Q[To].Num() - 1;
			const FVector Dp = PlanOf(ApronDoorSpot(O, To, 70.f)); P.Way.Reset(); P.Wi = 0;
			ApronWalk(P, G->ApronAt(float(PlanOf(L).X) + 0.6f, 85.f)); ApronWalk(P, G->ApronAt(float(Dp.X), 85.f)); ApronWalk(P, ApronDoorSpot(O, To, 70.f + 75.f * X.K));
			break;
		}
		X.Slot = O.Slots[Got]; O.Slots.RemoveAt(Got); X.Bus = O.Ti; O.Q[X.Door].Remove(i); O.Lock[X.Door] = i; X.Ph = 2; X.S = 0.f; X.T = 0.f;
		const FVector In = X.Slot.Seat != INDEX_NONE ? X.Slot.At + X.Slot.Face * 42.f : X.Slot.At; const double Sg = FMath::Sign(Dr.Y), Ys = FMath::Sign(X.Slot.At.Y), Cx = PaCross(In.X);
		const FVector Me = See.X.InverseTransformPosition(A->GetActorLocation());
		TArray<FVector2D> W; W.Add(FVector2D(Me.X, Me.Y)); W.Add(FVector2D(Dr.X, Sg * 172.)); W.Add(FVector2D(Dr.X, Sg * 112.)); W.Add(FVector2D(Dr.X, 0.));
		if (FMath::Abs(In.Y) > 20.) { W.Add(FVector2D(Cx, 0.)); if (FMath::Abs(Cx - In.X) > 1. && FMath::Abs(In.Y) > 62.) W.Add(FVector2D(Cx, Ys * 84.)); }
		W.Add(FVector2D(In.X, In.Y)); X.W.Set(MoveTemp(W));
		G->BusRider(O.Ti, A, true); X.bRider = true; P.Stage = TEXT("getting on the bus");
		break;
	}
	case 2: case 3: case 4:
	{
		if (!X.Rig) { ApronPose(i, ADPPose::Standing(L, FRotator(0.f, A->GetActorRotation().Yaw + 90.f, 0.f).Vector(), 92.f)); if (!X.Rig) break; }
		const float PH = X.Rig->PelvisHeight(); const FVector Lt = ADPPose::LeftOf(X.Slot.Face); const bool bSeat = X.Slot.Seat != INDEX_NONE;
		// in their place: sitting, or standing with a hand on the pole or the strap
		FDriverPose Rest = ADPPose::Standing(X.Slot.At, X.Slot.Face, PH);
		if (bSeat)
		{
			Rest.Pelvis = X.Slot.At - X.Slot.Face * 4.f + FVector(0, 0, 9.f); const double Fz = FMath::Max(33., X.Slot.At.Z - 46.);
			for (int32 s = 0; s < 2; ++s) { Rest.Foot[s] = X.Slot.At + X.Slot.Face * 42.f + Lt * (s == 0 ? 10.f : -10.f); Rest.Foot[s].Z = Fz; }
		}
		else
		{
			const int32 s = FVector::DotProduct(X.Slot.Hold - X.Slot.At, Lt) > 0. ? 0 : 1; const FVector Sh = X.Slot.At + FVector(0, 0, 143.f) + Lt * (s == 0 ? 18.f : -18.f);
			Rest.bHand[s] = true; Rest.Hand[s] = X.Slot.Hold - (X.Slot.Hold - Sh).GetSafeNormal() * 8.5f; Rest.Grip[s] = 0.9f;
		}
		FDriverPose Q;
		if (X.Ph == 2)
		{
			const FVector2D Nx = X.W.At(X.S + 55.f);
			if (X.S < X.W.Len) { if (!YouIn(G->BusW(X.Bus, FVector(Nx, 33.)))) X.S += Dt * 75.f; X.T = 0.f; }
			if (X.S < X.W.Len) X.W.Pose(X.S, PH, Q);
			else
			{
				FDriverPose E; X.W.Pose(X.W.Len, PH, E); Q = ADPPose::Lerp(E, Rest, FMath::SmoothStep(0.f, 1.f, X.T / 1.3f));
				if (X.T >= 1.3f) { X.Ph = 3; X.T = 0.f; if (O.Lock[X.Door] == i) O.Lock[X.Door] = -1; P.Stage = bSeat ? TEXT("sits in the bus") : TEXT("stands in the bus, holding on"); }
			}
		}
		else if (X.Ph == 3)
		{
			Q = Rest;
			if (O.Stage == 4 || O.Stage == 5)
			{
				// off again, row by row: the nearest of the door's part first, the aisle's side before the window's
				const int32 D = O.DoorOf(X.Slot.At.X);
				bool bMine = D >= 0 && G->BusDoorIs(O.Ti, D, true) && O.Lock[D] < 0 && (O.Stage == 5 || (!bNo && ApronStairs(O) && O.Stair.Num() < 6));
				if (bMine)
				{
					auto Far = [&](const FPaxOut& Y) { return FMath::Abs(Y.Slot.At.X - AGSE::BusDoorAt(D).X) + FMath::Abs(Y.Slot.At.Y) * 0.3 + (Y.Slot.Seat == INDEX_NONE ? -25. : 0.); };
					const FVector Out = ApronDoorSpot(O, D, 75.f);
					for (const TPair<int32, TSharedPtr<FPaxOut>>& Y : Outs)
					{
						if (Y.Key == i || !Pax.IsValidIndex(Y.Key)) continue;
						if (Y.Value->Ph == 3) { if (Pax[Y.Key].Run == P.Run && O.DoorOf(Y.Value->Slot.At.X) == D && Far(*Y.Value) < Far(X) - 0.01) { bMine = false; break; } }
						else if (Pax[Y.Key].A.IsValid() && FVector::Dist2D(Pax[Y.Key].A->GetActorLocation(), Out) < 110.) { bMine = false; break; }   // (the one before has stepped away)
					}
				}
				if (bMine && !YouIn(ApronDoorSpot(O, D, 60.f)) && !G->Moving(ApronDoorSpot(O, D, 60.f)))
				{
					O.Lock[D] = i; X.Door = D; X.Ph = 4; X.S = 0.f; X.T = 0.f; const FVector2f Dr = AGSE::BusDoorAt(D); const double Sg = FMath::Sign(Dr.Y), Ys = FMath::Sign(X.Slot.At.Y);
					const FVector In = bSeat ? X.Slot.At + X.Slot.Face * 42.f : X.Slot.At; const double Cx = PaCross(In.X);
					TArray<FVector2D> W; W.Add(FVector2D(In.X, In.Y)); if (FMath::Abs(In.Y) > 20.) { if (FMath::Abs(Cx - In.X) > 1. && FMath::Abs(In.Y) > 62.) W.Add(FVector2D(Cx, Ys * 84.)); W.Add(FVector2D(Cx, 0.)); }
					W.Add(FVector2D(Dr.X, 0.)); W.Add(FVector2D(Dr.X, Sg * 112.)); W.Add(FVector2D(Dr.X, Sg * 205.)); X.W.Set(MoveTemp(W));
					P.Stage = TEXT("getting off the bus");
				}
			}
		}
		else if (X.T < 1.2f) { FDriverPose E; X.W.Pose(0.f, PH, E); Q = ADPPose::Lerp(Rest, E, FMath::SmoothStep(0.f, 1.f, X.T / 1.2f)); }
		else
		{
			ASeats::Release(A);
			const FVector2D Nx = X.W.At(X.S + 55.f); if (!YouIn(G->BusW(X.Bus, FVector(Nx, FMath::Abs(Nx.Y) < 153. ? 33. : 0.)))) X.S = FMath::Min(X.S + Dt * 75.f, X.W.Len);
			X.W.Pose(X.S, PH, Q);
		}
		G->BusPoseW(X.Bus, Q); ApronPose(i, Q);
		if (X.Ph == 4 && X.T >= 1.2f && X.S >= X.W.Len)
		{
			ApronUnpose(i); if (O.Lock[X.Door] == i) O.Lock[X.Door] = -1;
			G->BusRider(X.Bus, A, false); X.bRider = false;
			if (O.Stage == 5 || bNo) { ApronBack(i); break; }
			X.Ph = 5; X.K = -1; O.Stair.AddUnique(i); P.Way.Reset(); P.Wi = 0; P.Stage = TEXT("to the passenger stairs");
		}
		break;
	}
	case 5:
	{
		if (bNo) { ApronBack(i); break; }
		if (!ApronStairs(O)) { Idle(P, A, Dt); P.Stage = TEXT("waiting for the passenger stairs"); break; }
		const int32 k = O.Stair.Find(i);
		if (k != X.K && k >= 0)
		{
			const FVector To = ApronQueue(O, k); X.K = k;
			if (P.Way.Num() > 1 && P.Wi < P.Way.Num() - 1) P.Way.Last().P = To; else Ground(To);   // (still on the straight way: its end moves up)
		}
		if (!Go() || k != 0) break;
		const FVector At = ApronQueue(O, 0);
		if (FVector::Dist2D(A->GetActorLocation(), At) > 22.f) { Ground(At); break; }
		if (O.UpT < 2.2f || YouIn(O.Way.Steps[0])) break;
		O.UpT = 0.f; O.Stair.Remove(i); X.Ph = 6; X.K = O.Way.Ti; X.S = 0.f; X.T = 0.f; G->StairsEnter(X.K, 1);
		X.From = ADPPose::Standing(A->GetActorLocation(), FRotator(0.f, A->GetActorRotation().Yaw + 90.f, 0.f).Vector(), 92.f); P.Stage = TEXT("up the passenger stairs");
		break;
	}
	case 6:
	{
		// up the treads, the hand on the rail; gone at the top (each waits if you stand in the way, and behind one who waits)
		if (!X.Rig) { ApronPose(i, X.From); if (!X.Rig) break; const FVector F = X.From.Facing; X.From = ADPPose::Standing((X.From.Foot[0] + X.From.Foot[1]) * 0.5, F, X.Rig->PelvisHeight()); X.T = 0.f; }
		const float PH = X.Rig->PelvisHeight(); FDriverPose Q; G->StairsPose(X.K, true, FMath::Min(1.f, X.S + 0.06f), PH, Q);
		bool bHold = X.T < 0.5f || YouIn(Q.Pelvis);
		for (const TPair<int32, TSharedPtr<FPaxOut>>& Y : Outs) if (Y.Key != i && Y.Value->Ph == 6 && Y.Value->K == X.K && Y.Value->S >= X.S && Y.Value->S - X.S < 0.1f && (Y.Value->S > X.S || Y.Key < i)) bHold = true;
		if (!bHold) X.S += Dt / FMath::Max(4.f, G->StairsSeconds(X.K));
		G->StairsPose(X.K, true, FMath::Min(X.S, 1.f), PH, Q);
		if (X.T < 0.5f) Q = ADPPose::Lerp(X.From, Q, FMath::SmoothStep(0.f, 1.f, X.T / 0.5f));
		ApronPose(i, Q);
		if (X.S >= 1.f)
		{
			ApronUnpose(i);   // (Finish lets go of the stairs: ApronGone)
			if (bNo) Finish(i, TEXT("was up the passenger stairs when the flight was taken off"));
			else { P.Do = EDo::Aboard; Arrive(i); }
		}
		break;
	}
	default: break;
	}
}

void APaxFlow::ApronTick(float Dt)
{
	if (!ApronG) { ApronG = AGSE::Find(GetWorld()); if (!ApronG) return; AddTickPrerequisiteActor(ApronG); }   // (the bus has moved before its passengers)
	for (int32 Ri = 0; Ri < Runs.Num(); ++Ri)
	{
		FRun& R = Runs[Ri]; if (R.bArr || !R.Way || !R.bCalled) continue;
		TSharedPtr<FRunOut> Op = RunOuts.FindRef(R.Key);
		if (!Op)
		{
			if (R.bOff || R.bGone || R.bNoShowGo) continue;
			Op = MakeShared<FRunOut>(); RunOuts.Add(R.Key, Op); Op->Key = R.Key; Op->Stand = R.Stand; Op->Gate = R.Gate;
			Op->S = AAirfield::Shared().Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == R.Stand; });
			if (Op->S) Op->StandX = Op->S->X;
			static FTerminalPlan Plan; static const bool bPlan = Plan.Load();
			const FString Room = FString::Printf(TEXT("F1-GATE-%d"), R.Gate);
			if (bPlan) for (const FPlanDoor& D : Plan.Doors) if (D.A == Room && D.B == TEXT("APRON")) Op->Doors.Add(D.X);
			if (!Op->S || !Op->Doors.Num()) ADP_WARN(TEXT("PaxFlow: %s can't board - stand '%s'%s, gate %d has %d doors to the apron"), *R.Label, *R.Stand, Op->S ? TEXT("") : TEXT(" (not built)"), R.Gate, Op->Doors.Num());
			if (Op->Doors.Num()) Op->DoorX = Op->Doors[0];
		}
		FRunOut& O = *Op; O.T -= Dt; O.StairT -= Dt; O.UpT += Dt; O.SlotT -= Dt; O.StageT += Dt; O.SaidT -= Dt;
		if (EADPPaxWay(R.Way) == EADPPaxWay::Bus && O.S && O.Doors.Num()) ApronBus(Ri, O, Dt);
	}
	for (auto It = RunOuts.CreateIterator(); It; ++It)
	{
		if (Runs.ContainsByPredicate([&](const FRun& R) { return R.Key == It.Key(); })) continue;
		FRunOut& O = *It.Value();   // (its run is over)
		if (O.Ti != INDEX_NONE) { FString Why; const int32 Ti = O.Ti; TWeakObjectPtr<AGSE> G = ApronG; if (!ApronG->BusHome(Ti, [G, Ti] { if (G.IsValid()) G->BusLeave(Ti); }, Why)) ADP_WARN(TEXT("PaxFlow: the bus of a finished flight can't go home (%s)"), *Why); }
		It.RemoveCurrent();
	}
}

void APaxFlow::ApronBus(int32 Ri, FRunOut& O, float Dt)
{
	FRun& R = Runs[Ri]; const bool bNo = R.bOff || R.bGone;
	auto Say = [&](const FString& W) { if (W != O.Why) { O.Why = W; ADP_LOG(TEXT("PaxFlow: %s's bus - %s"), *R.Label, *W); } };
	auto To = [&](uint8 S) { O.Stage = S; O.StageT = 0.f; O.T = 0.f; O.Cand = 0; O.SlotT = 0.f; O.bStuck = false; O.Why.Reset(); };
	auto Drive = [&](const FVector2f& At, float Hd) -> bool
	{
		FString Why; const int32 Trip = ++O.Trips; O.bThere = false; TWeakObjectPtr<APaxFlow> Me = this; const FString Key = O.Key;
		if (!ApronG->BusDriveTo(O.Ti, At, Hd, [Me, Key, Trip] { if (Me.IsValid()) if (TSharedPtr<FRunOut> X = Me->RunOuts.FindRef(Key)) if (X->Trips == Trip) X->bThere = true; }, Why)) { Say(Why); return false; }
		return true;
	};
	auto SideTo = [&](const FVector& W) { AGSE::FBusSee See; if (ApronG->BusSee(O.Ti, See)) O.Side = See.X.InverseTransformPosition(W).Y > 0. ? 1.f : -1.f; };
	auto OpenUp = [&]
	{
		if (FMath::Fmod(O.StageT, 1.f) > Dt) return;   // (once a second)
		AGSE::FBusSee See;
		if (!ApronG->BusSee(O.Ti, See) || !See.bStands || !See.bAtWheel) { Say(TEXT("its driver isn't at the wheel")); return; }
		FString Why; for (int32 d = 0; d < 7; ++d) if (AGSE::BusDoorAt(d).Y * O.Side > 0.f && !ApronG->BusDoorIs(O.Ti, d, true)) ApronG->BusDoor(O.Ti, d, true, Why);
	};
	auto Slow = [&] { if (O.StageT > 600.f && !O.bStuck) { O.bStuck = true; ADP_WARN(TEXT("PaxFlow: %s's bus has been on its way for ten minutes - it can't reach its place; its passengers wait"), *R.Label); } };
	auto Home = [&]
	{
		if (O.T > 0.f) return;
		const int32 Ti = O.Ti; TWeakObjectPtr<AGSE> G = ApronG; FString Why;
		if (!ApronG->BusHome(Ti, [G, Ti] { if (G.IsValid()) G->BusLeave(Ti); }, Why)) { Say(Why); O.T = 8.f; return; }
		ADP_LOG(TEXT("PaxFlow: %s's bus goes home"), *R.Label); To(6); O.Ti = INDEX_NONE;
	};
	// the ones of this flight still to come (not aboard, not gone home, not in the bus), those in it, those on the way to it
	int32 Come = R.bNoShowGo ? 0 : FMath::Max(0, R.Visible - R.Spawned), In = 0, Near = 0;
	for (int32 i = 0; i < Pax.Num(); ++i)
	{
		const FPx& P = Pax[i]; if (P.Run != Ri || P.bArr || P.St == EPx::Gone) continue;
		const TSharedPtr<FPaxOut> X = Outs.FindRef(i);
		if (P.bHome && (!X || X->Ph == 0)) continue;
		if (X && X->Ph >= 2 && X->Ph <= 4) ++In; else if (X && X->Ph == 0) { ++Near; ++Come; } else if (!X && !R.bNoShowGo) ++Come;
	}
	switch (O.Stage)
	{
	case 0:
	{
		if (bNo || R.bNoShowGo || (Come == 0 && R.Spawned > 0)) { To(6); return; }
		if (O.T > 0.f) return;
		O.T = 1.5f;
		TArray<int32> Free; ApronG->BusFree(Free);
		Free.RemoveAll([&](int32 Ti) { for (const TPair<FString, TSharedPtr<FRunOut>>& X : RunOuts) if (X.Value.Get() != &O && X.Value->Ti == Ti) return true; return false; });
		if (!Free.Num()) { O.T = 10.f; Say(TEXT("no apron bus is free: its passengers wait at the gate")); return; }
		// (in front of one of the gate's two doors, its door side to it)
		const int32 N = Free.Num() * O.Doors.Num(), c = O.Cand++ % N, Ti = Free[c / O.Doors.Num()]; const FVector2f At(O.Doors[c % O.Doors.Num()], 90.5f);
		for (const TPair<FString, TSharedPtr<FRunOut>>& X : RunOuts) if (X.Value.Get() != &O && X.Value->Ti != INDEX_NONE && X.Value->Stage < 3 && FVector2f::Distance(X.Value->Pick, At) < 18.f) return;   // (another flight's bus stands there)
		if (!ApronG->BusTry(Ti, At, 0.f)) { if (O.Cand >= N) { O.Cand = 0; O.T = 10.f; Say(TEXT("no bus has a clear way to the gate now: its passengers wait")); } return; }
		O.Ti = Ti; O.DoorX = At.X; O.Pick = At;
		if (!Drive(At, 0.f)) { O.Ti = INDEX_NONE; O.T = 10.f; return; }   // (nobody free: Why says so)
		To(1); ADP_LOG(TEXT("PaxFlow: %s's bus is called to gate %d"), *R.Label, O.Gate);
		return;
	}
	case 1:
		Slow(); if (!O.bThere) return;
		SideTo(ApronG->ApronAt(O.DoorX, 84.f)); To(O.bBack ? 5 : 2); ADP_LOG(TEXT("PaxFlow: %s's bus stands at gate %d"), *R.Label, O.Gate);
		return;
	case 2:
	{
		if (bNo) { if (In || Near) To(5); else Home(); return; }
		if (O.SlotT <= 0.f)
		{
			O.SlotT = 2.f; ApronG->BusSlots(O.Ti, O.Slots);   // (not the standing places somebody has)
			O.Slots.RemoveAll([&](const AGSE::FBusSlot& S) { for (const TPair<int32, TSharedPtr<FPaxOut>>& X : Outs) if (X.Value->Bus == O.Ti && X.Value->Ph >= 2 && X.Value->Ph <= 4 && FVector::Dist2D(X.Value->Slot.At, S.At) < 30.) return true; return false; });
		// (the seats first, window to aisle; of each kind the innermost from its door first: nobody has to get past somebody)
		auto Inner = [&](const AGSE::FBusSlot& S) { const int32 d = O.DoorOf(S.At.X); return d < 0 ? 0. : FMath::Abs(S.At.X - AGSE::BusDoorAt(d).X); };
		auto Rank = [&](const AGSE::FBusSlot& S) { return (S.Seat == INDEX_NONE ? 1000 : 0) - FMath::RoundToInt(FMath::Abs(S.At.Y) / 25.) * 10; };
		O.Slots.StableSort([&](const AGSE::FBusSlot& A, const AGSE::FBusSlot& B) { const int32 Ra = Rank(A), Rb = Rank(B); return Ra != Rb ? Ra < Rb : Inner(A) > Inner(B); });
	}
		if (O.SaidT <= 0.f && Come) { O.SaidT = 60.f; ADP_LOG(TEXT("PaxFlow: %s's bus waits for %d passengers (%d in it)"), *R.Label, Come, In); }
		const int32 Getting = ApronCount(Ri, 2, 2); const bool bFull = !O.Slots.Num() && !Getting;
		const bool bGo = In > 0 && !Getting && (Come == 0 || (bFull && (Near == 0 || O.StageT > 180.f)));
		if (!bGo) { OpenUp(); if (In == 0 && Come == 0) Home(); return; }
		// where at the aircraft: clear of the stairs' foot, the doors to it
		if (O.T > 0.f) { return; }
		O.T = 1.5f;
		const FAirfieldData::FStandFrame Fr = O.S->Frame(); const FVector2f S(Fr.K, 0.f), F = Fr.Face(); FVector2f Ft = Fr.P(11.5f, -0.3f);
		if (ApronStairs(O)) { const FVector V = PlanOf(O.Way.Foot); Ft = FVector2f(float(V.X), float(V.Z)); }
		auto Hd = [](const FVector2f& D) { return FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)); };
		const FVector2f At[4] = {Ft + S * 8.5f + F * 3.f, Ft + S * 8.5f + F * 6.f, Ft + S * 8.5f + F * 3.f, Ft + F * 9.f + S * 2.f}; const float Hs[4] = {Hd(F * -1.f), Hd(F * -1.f), Hd(F), Hd(S * -1.f)};
		const int32 c = O.Cand++ % 4;
		if (!ApronG->BusTry(O.Ti, At[c], Hs[c])) { if (c == 3) { O.T = 8.f; Say(TEXT("no clear way to the aircraft now: it waits at the gate with its passengers")); } return; }
		if (!Drive(At[c], Hs[c])) { O.T = 8.f; return; }
		To(3); ADP_LOG(TEXT("PaxFlow: %s's bus leaves gate %d with %d passengers, %d still to come (%s)"), *R.Label, O.Gate, In, Come, Come ? TEXT("full: it comes back for them") : TEXT("everybody in"));
		return;
	}
	case 3:
		Slow(); if (!O.bThere) return;
		{ const FVector2f Ft = O.S->Frame().P(11.5f, -0.3f); SideTo(ApronStairs(O) ? O.Way.Foot : ApronG->ApronAt(Ft.X, Ft.Y)); }
		To(4); ADP_LOG(TEXT("PaxFlow: %s's bus is at the aircraft"), *R.Label);
		return;
	case 4:
		if (bNo && In)
		{
			// (the flight is off: those still in it back to the gate)
			if (ApronCount(Ri, 4, 4) || O.T > 0.f) return;
			O.T = 8.f;
			if (ApronG->BusTry(O.Ti, O.Pick, 0.f) && Drive(O.Pick, 0.f)) { O.bBack = true; To(1); ADP_LOG(TEXT("PaxFlow: %s's bus takes %d passengers back to gate %d"), *R.Label, In, O.Gate); }
			else Say(TEXT("no way back to the gate now: its passengers wait in it"));
			return;
		}
		if (In)
		{
			OpenUp();
			if (!ApronStairs(O)) Say(TEXT("at the aircraft: its passengers wait in it for the passenger stairs"));
			return;
		}
		if (O.T > 0.f) return;
		O.T = 5.f;
		{
			AGSE::FBusSee See; if (!ApronG->BusSee(O.Ti, See)) return;   // (nobody beside it any more)
			for (const TPair<int32, TSharedPtr<FPaxOut>>& X : Outs) if (Pax.IsValidIndex(X.Key) && X.Value->Ph >= 5 && X.Value->Ph != 6 && Pax[X.Key].A.IsValid()) { const FVector L = See.X.InverseTransformPosition(Pax[X.Key].A->GetActorLocation()); if (FMath::Abs(L.X) < 900. && FMath::Abs(L.Y) < 230.) return; }
		}
		O.T = 0.f;
		if (Come && !bNo && !R.bNoShowGo)
		{
			if (!ApronG->BusTry(O.Ti, O.Pick, 0.f)) { O.T = 8.f; Say(TEXT("no way back to the gate for the others now")); return; }
			if (Drive(O.Pick, 0.f)) { To(1); ADP_LOG(TEXT("PaxFlow: %s's bus goes back to gate %d for %d more"), *R.Label, O.Gate, Come); }
			else O.T = 8.f;
			return;
		}
		Home();
		return;
	case 5:
		if (!In && !Near) { Home(); return; }
		OpenUp();
		return;
	default: return;
	}
}
