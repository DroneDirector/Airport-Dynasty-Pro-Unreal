// Departing passengers by foot or by bus to the aircraft (to-do 277): out of a ground gate's door to the apron, the marked
// straight way to the passenger stairs or an apron bus with its hired driver, up the treads one by one, gone at the top.
// A passenger's phase (FPaxOut): 0 to the bus's door / queueing, 2 getting in, 3 in their place, 4 getting out, 5 to / at the
// foot of the stairs, 6 on the treads, 7 back to the gate, 8 out of the gate (by foot).
// A flight's bus (FRunOut::Stage): 0 none yet, 1 to the gate, 2 boarding, 3 to the aircraft, 4 passengers get out, 5 back at
// the gate, they get out there (the flight is off), 6 done.
// Arriving passengers (FPx::bArr) the other way round with the same phases and stages: 6 down the treads, 0 at the bus's door,
// 2-4 in it, 7 to the ground gate's door and in; the bus 1 to the aircraft, 2 boarding there, 3 to the gate, 4 they get out.
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
#include "Turnaround.h"
#include "StandAsk.h"
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
	bool bArr = false; float TurnT = 0.f; FString Key, Stand, Why; int32 Gate = 0, Ti = INDEX_NONE, Cand = 0, Trips = 0; float StandX = 0.f; const FAirfieldData::FStand* S = nullptr;
	uint8 Stage = 0; bool bThere = false, bStairs = false, bBack = false, bTold = false, bStuck = false;
	bool bRetreat = false; float NoBusT = 0.f, AskT = 0.f, NoWayT = 0.f; int32 Ask = 0, NotTi = INDEX_NONE; double NotT = 0.0;   // (to-do 296: no bus for so long, what it asked for, a bus that did not get there)
	float T = 0.f, StairT = 0.f, UpT = 99.f, SlotT = 0.f, StageT = 0.f, SaidT = 0.f, DoorX = 0.f, Side = 1.f; TArray<float> Doors;
	FVector2f Pick = FVector2f::ZeroVector, Aim = FVector2f::ZeroVector; float AimH = 0.f; AGSE::FStairsWay Way;
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
	if (R.Way) ADP_LOG(TEXT("PaxFlow: %s's passengers %s stand %s %s"), *R.Label, R.bArr ? TEXT("come from") : TEXT("go to"), *Stand, EADPPaxWay(R.Way) == EADPPaxWay::Walk ? TEXT("by foot") : TEXT("by bus"));
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
	ADP_LOG(TEXT("PaxFlow: %s goes back to gate %d from the apron (%s)"), *P.Name, O->Gate, Runs[P.Run].bOff ? TEXT("the flight is off") : Runs[P.Run].bGone ? TEXT("the flight left") : TEXT("nothing can bring them on"));
}

// ---- arriving: out of the aircraft at the top of the stairs, one by one

// still in the aircraft or on its stairs (the turn-round waits for these; DeplaneDone)
int32 APaxFlow::ApronInside(int32 Ri) const { return FMath::Max(0, Runs[Ri].Visible - Runs[Ri].Spawned) + ApronCount(Ri, 5, 6) + ApronCount(Ri, 9, 9); }   // (5: on the way back in, 9: in it again)

// could the airport ever bring what they need: the stairs, and the bus where they go by bus (DeplaneDone, BoardingDone)
bool APaxFlow::ApronCan(int32 Ri, FString& Why) const
{
	const TSharedPtr<FRunOut> O = RunOuts.FindRef(Runs[Ri].Key);
	if (!O || !ApronG || !O->S || !O->Doors.Num()) { Why = TEXT("its stand or gate has no way over the apron"); return false; }
	if (!O->bStairs && !ApronG->StairsCan(O->StandX, Why)) return false;
	return EADPPaxWay(Runs[Ri].Way) == EADPPaxWay::Walk || (O->Stage >= 1 && O->Stage <= 4) || ApronG->BusCan(Why);
}

// departing: they can't get on now, but what they need can come
bool APaxFlow::ApronHolds(int32 Ri) const
{
	const TSharedPtr<FRunOut> O = RunOuts.FindRef(Runs[Ri].Key); FString Why; if (!O || Runs[Ri].bOff || Runs[Ri].bGone) return false;
	return (!O->bStairs || (EADPPaxWay(Runs[Ri].Way) == EADPPaxWay::Bus && O->Stage != 2)) && ApronCan(Ri, Why);
}

// may the next one come out now
bool APaxFlow::ApronMay(int32 Ri, bool bAgain)
{
	if (!bAgain && ApronCount(Ri, 9, 9) > 0) return false;   // (those who went back in come out first)
	FRun& R = Runs[Ri]; TSharedPtr<FRunOut> Op = RunOuts.FindRef(R.Key); if (!Op || !ApronG) return false;
	FRunOut& O = *Op; if (!O.S || !O.Doors.Num()) return false;
	if (!ApronStairs(O)) { if (!O.bTold) { O.bTold = true; ADP_LOG(TEXT("PaxFlow: %s's passengers wait in the aircraft for the passenger stairs on stand %s"), *R.Label, *O.Stand); } return false; }
	const int32 Down = ApronCount(Ri, 6, 6);
	if (O.bRetreat || ApronCount(Ri, 5, 5) > 0) return false;   // (they go back in)
	for (const TPair<int32, TSharedPtr<FPaxOut>>& Y : Outs) if (Y.Value->Ph == 6 && Y.Value->bBack && Y.Value->K == O.Way.Ti) return false;
	if (O.UpT < 2.2f || Down >= 5) return false;
	for (const TPair<int32, TSharedPtr<FPaxOut>>& Y : Outs) if (Y.Value->Ph == 6 && Y.Value->K == O.Way.Ti && (!Pax.IsValidIndex(Y.Key) || Pax[Y.Key].Run != Ri || Y.Value->S < 0.16f)) return false;   // (somebody at the top, or coming up)
	FDriverPose Q; ApronG->StairsPose(O.Way.Ti, false, 0.f, 92.f, Q);
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (const APawn* You = PC ? PC->GetPawn() : nullptr) if (FVector::Dist2D(You->GetActorLocation(), Q.Pelvis) < 75. && FMath::Abs(You->GetActorLocation().Z - Q.Pelvis.Z) < 160.) return false;
	if (EADPPaxWay(R.Way) == EADPPaxWay::Walk) return true;
	// (by bus: only as many as it has places for; the others stay in their seats in the aircraft until it is back)
	const int32 Near = ApronCount(Ri, 0, 1) + Down; AGSE::FBusSee See;
	if (O.Stage != 2 || Near >= 6 || O.Slots.Num() <= Near || !ApronG->BusSee(O.Ti, See) || !See.bStands || !See.bAtWheel) return false;
	for (int32 d = 0; d < 7; ++d) if (AGSE::BusDoorAt(d).Y * O.Side > 0.f && ApronG->BusDoorIs(O.Ti, d, true)) return true;
	return false;
}

int32 APaxFlow::ApronOff(int32 i)
{
	FPx& P = Pax[i]; FRun& R = Runs[P.Run]; TSharedPtr<FRunOut> O = RunOuts.FindRef(R.Key); AActor* A = nullptr; FDriverPose Q;
	if (O && ApronG) { ApronG->StairsPose(O->Way.Ti, false, 0.f, 92.f, Q); A = Body((Q.Foot[0] + Q.Foot[1]) * 0.5, Q.Facing.Rotation().Yaw, P); }
	if (!A) { P.St = EPx::Gone; FreePx.Add(i); return INDEX_NONE; }
	FPaxOut& X = *ApronNew(i); X.Ph = 6; X.K = O->Way.Ti; X.From = Q; O->UpT = 0.f;
	P.St = EPx::Apron; P.Do = EDo::ArrIn; P.Stage = TEXT("out of the aircraft, down the passenger stairs");
	ApronG->StairsEnter(X.K, 1); ApronPose(i, Q);
	if (X.Rig) { ApronG->StairsPose(X.K, false, 0.f, X.Rig->PelvisHeight(), X.From); ApronPose(i, X.From); }
	if (R.Spawned <= 3) ADP_LOG(TEXT("PaxFlow: %s (%s) off %s at stand %s, down the passenger stairs%s"), *P.Id, *P.Name, *R.Label, *O->Stand, P.bArrBag ? TEXT(", a bag in the hold") : TEXT(", hand luggage only"));
	return i;
}

// off the bus at the gate, or down the stairs by foot: in by the ground gate's door
void APaxFlow::ApronIn(int32 i)
{
	FPx& P = Pax[i]; FRun& R = Runs[P.Run]; TSharedPtr<FPaxOut> X = Outs.FindRef(i); TSharedPtr<FRunOut> O = RunOuts.FindRef(R.Key); const AActor* A = P.A.Get(); if (!X || !O || !A) return;
	const FVector L = PlanOf(A->GetActorLocation()); float Dx = O->Doors[0]; const float J = float(P.Jit.X) / 250.f;
	X->Ph = 7; P.Way.Reset(); P.Wi = 0;
	if (EADPPaxWay(R.Way) == EADPPaxWay::Walk)
	{
		// (the departures' straight way backwards, on the other side of it)
		for (float V : O->Doors) if (FMath::Abs(V * 100.f - O->Way.Foot.X) < FMath::Abs(Dx * 100.f - O->Way.Foot.X)) Dx = V;
		const FVector F = PlanOf(O->Way.Foot); const float Ax = float(F.X - O->Way.Up.X * 6.5), Az = float(F.Z - 5.4);
		ApronWalk(P, ApronG->ApronAt(Ax, Az)); ApronWalk(P, ApronG->ApronAt(Dx + J, Az));
	}
	else { Dx = O->DoorX; ApronWalk(P, ApronG->ApronAt(float(L.X), 85.f)); }
	for (float Z : {84.6f, 82.9f}) ApronWalk(P, ApronG->ApronAt(Dx, Z));
	FWp W; W.P = PW(Dx, 0.f, 80.5f); P.Way.Add(W); P.Stage = TEXT("to the ground gate's door");
}

void APaxFlow::StepApron(int32 i, float Dt)
{
	FPx& P = Pax[i]; FRun& R = Runs[P.Run]; AActor* A = P.A.Get(); TSharedPtr<FPaxOut> Xp = Outs.FindRef(i); TSharedPtr<FRunOut> Op = RunOuts.FindRef(R.Key);
	if (!A || !ApronG) return;
	if (!Xp || !Op) { if (P.bArr) P.Do = EDo::None; if (!P.bHome) { ADP_WARN(TEXT("PaxFlow: %s stands on the apron without a flight - goes home"), *P.Name); GoHome(i, TEXT("missed the flight")); } return; }
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
	case 7:
		if (P.bArr) { if (Go()) { ApronGone(i); WayArrivals(P); P.St = EPx::Walk; } break; }   // (in: on as everybody off an aircraft)
		if (Go()) { ApronGone(i); P.Do = EDo::None; GoHome(i, R.bOff ? TEXT("the flight is off") : TEXT("missed the flight")); } break;
	case 0:
	{
		if (bNo) { ApronBack(i); break; }
		// (no bus here, or nobody hired to drive it: told. The gate again; off an aircraft that gave up and has no stairs, on foot)
		if (O.Stage == 0 && O.NoWayT > 600.f && (!P.bArr || (R.bNoShowGo && !ApronStairs(O)))) { ApronBack(i); break; }
		if (P.bArr && O.NoBusT > 300.f && O.bRetreat && ApronStairs(O))
		{
			// (no bus comes: up the stairs and into the aircraft again, until one is there)
			for (int32 d = 0; d < 7; ++d) O.Q[d].Remove(i);
			O.Stair.AddUnique(i); X.Ph = 5; X.bBack = true; X.K = O.Stair.Find(i); Ground(ApronQueue(O, X.K)); P.Stage = TEXT("no bus: back to the aircraft");
			ADP_LOG(TEXT("PaxFlow: %s (%s) goes back to the aircraft - no bus at stand %s for %.0f s"), *P.Name, *R.Label, *O.Stand, O.NoBusT);
			break;
		}
		if (O.Stage != 2 && P.bArr)
		{
			// (its bus was lost: back behind the stairs' foot, clear of where the next one comes to stand)
			if (X.K != -2) { X.K = -2; Ground(ApronQueue(O, 1 + FMath::Max(0, O.Q[X.Door].Find(i)) + 3 * (X.Door % 4))); }
			Go(); P.Stage = TEXT("waiting for the bus"); break;
		}
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
			if (P.bArr) { Ground(ApronDoorSpot(O, To, 70.f + 75.f * X.K)); break; }
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
				bool bMine = D >= 0 && G->BusDoorIs(O.Ti, D, true) && O.Lock[D] < 0 && (O.Stage == 5 || P.bArr || (!bNo && ApronStairs(O) && O.Stair.Num() < 6));
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
			if (P.bArr) { ApronIn(i); break; }
			if (O.Stage == 5 || bNo) { ApronBack(i); break; }
			X.Ph = 5; X.K = -1; O.Stair.AddUnique(i); P.Way.Reset(); P.Wi = 0; P.Stage = TEXT("to the passenger stairs");
		}
		break;
	}
	case 5:
	{
		if (P.bArr ? (bNo || R.bNoShowGo) && !ApronStairs(O) : bNo) { ApronBack(i); break; }
		if (P.bArr && O.Stage == 2 && !R.bNoShowGo)
		{
			// (one has come after all)
			O.Stair.Remove(i); X.Ph = 0; X.bBack = false; X.Bus = O.Ti; X.K = -3; X.Door = -1; int32 Best = 99;
			for (int32 d = 0; d < 7; ++d) if (AGSE::BusDoorAt(d).Y * O.Side > 0.f && O.Q[d].Num() < Best) { Best = O.Q[d].Num(); X.Door = d; }
			X.Door = FMath::Max(0, X.Door); O.Q[X.Door].AddUnique(i); P.Stage = TEXT("to the bus"); break;
		}
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
		if (P.bArr) { bool bDown = false; for (const TPair<int32, TSharedPtr<FPaxOut>>& Y : Outs) if (Y.Value->Ph == 6 && Y.Value->K == O.Way.Ti && !Y.Value->bBack) bDown = true; if (bDown) break; }   // (somebody comes down)
		O.UpT = 0.f; O.Stair.Remove(i); X.Ph = 6; X.K = O.Way.Ti; X.S = 0.f; X.T = 0.f; G->StairsEnter(X.K, 1);
		X.From = ADPPose::Standing(A->GetActorLocation(), FRotator(0.f, A->GetActorRotation().Yaw + 90.f, 0.f).Vector(), 92.f); P.Stage = TEXT("up the passenger stairs");
		break;
	}
	case 6:
	{
		// up the treads, the hand on the rail; gone at the top (each waits if you stand in the way, and behind one who waits)
		if (!X.Rig) { ApronPose(i, X.From); if (!X.Rig) break; const FVector F = X.From.Facing; X.From = ADPPose::Standing((X.From.Foot[0] + X.From.Foot[1]) * 0.5, F, X.Rig->PelvisHeight()); X.T = 0.f; }
		const float PH = X.Rig->PelvisHeight(); FDriverPose Q;
		if (P.bArr && !X.bBack && O.bRetreat && X.T >= 0.5f)
		{
			// (no bus comes: they turn round on the treads and go back up into the aircraft)
			G->StairsPose(X.K, false, FMath::Min(X.S, 1.f), PH, X.From); X.bBack = true; X.S = FMath::Clamp(1.f - X.S, 0.f, 1.f); X.T = 0.f; P.Stage = TEXT("no bus: back up the passenger stairs");
			ADP_LOG(TEXT("PaxFlow: %s (%s) goes back up the passenger stairs - no bus at stand %s for %.0f s"), *P.Name, *R.Label, *O.Stand, O.NoBusT);
		}
		const bool bUp = !P.bArr || X.bBack; G->StairsPose(X.K, bUp, FMath::Min(1.f, X.S + 0.06f), PH, Q);
		bool bHold = X.T < 0.5f || YouIn(Q.Pelvis);
		for (const TPair<int32, TSharedPtr<FPaxOut>>& Y : Outs) if (Y.Key != i && Y.Value->Ph == 6 && Y.Value->K == X.K && Y.Value->S >= X.S && Y.Value->S - X.S < 0.1f && (Y.Value->S > X.S || Y.Key < i)) bHold = true;
		if (!bUp && X.S > 0.84f)
		{
			// down: on only when the foot is clear and (by bus) a door is open with a place behind it
			const FVector Ft = O.Way.Steps[0] - O.Way.Up * 45.f + FVector(O.Way.Up.Y, -O.Way.Up.X, 0.f) * 26.f;
			for (const TPair<int32, TSharedPtr<FPaxOut>>& Y : Outs) if (Y.Key != i && Y.Value->Ph != 6 && Pax.IsValidIndex(Y.Key) && Pax[Y.Key].A.IsValid() && FVector::Dist2D(Pax[Y.Key].A->GetActorLocation(), Ft) < 75.) bHold = true;
			if (G->Moving(Ft)) bHold = true;
			if (EADPPaxWay(R.Way) == EADPPaxWay::Bus)
			{
				int32 Best = 99; X.Door = -1;
				if (O.Stage == 2) for (int32 d = 0; d < 7; ++d)
				{
					if (AGSE::BusDoorAt(d).Y * O.Side < 0.f || !G->BusDoorIs(O.Ti, d, true)) continue;
					const int32 n = O.Q[d].Num(); int32 Pl = 0; for (const AGSE::FBusSlot& S : O.Slots) if (O.DoorOf(S.At.X) == d) ++Pl;
					if (n < Best && n < Pl) { Best = n; X.Door = d; }
				}
				if (X.Door < 0) { bHold = true; P.Stage = TEXT("on the passenger stairs, waiting for the bus"); }
			}
		}
		if (!bHold) X.S += Dt / FMath::Max(4.f, G->StairsSeconds(X.K));
		G->StairsPose(X.K, bUp, FMath::Min(X.S, 1.f), PH, Q);
		if (X.T < 0.5f) Q = ADPPose::Lerp(X.From, Q, FMath::SmoothStep(0.f, 1.f, X.T / 0.5f));
		ApronPose(i, Q);
		if (X.S >= 1.f)
		{
			ApronUnpose(i);   // (Finish lets go of the stairs: ApronGone)
			if (!bUp)
			{
				G->StairsEnter(X.K, -1); P.Way.Reset(); P.Wi = 0;
				if (EADPPaxWay(R.Way) == EADPPaxWay::Walk) { ApronIn(i); break; }
				X.Ph = 0; X.Bus = O.Ti; O.Q[X.Door].AddUnique(i); X.K = O.Q[X.Door].Find(i); P.Stage = TEXT("to the bus");
				ApronWalk(P, ApronDoorSpot(O, X.Door, 70.f + 75.f * X.K));
				break;
			}
			if (P.bArr)
			{
				// (through the door, as everybody who boards; the same person with the same things comes out again: case 9)
				G->StairsEnter(X.K, -1); X.Ph = 9; X.bBack = false; X.S = 0.f; X.T = 0.f; P.Way.Reset(); P.Wi = 0; P.Stage = TEXT("in the aircraft again, waiting for the bus");
				A->SetActorHiddenInGame(true); A->SetActorEnableCollision(false);
				break;
			}
			if (bNo) Finish(i, TEXT("was up the passenger stairs when the flight was taken off"));
			else { P.Do = EDo::Aboard; Arrive(i); }
		}
		break;
	}
	case 9:
	{
		if (bNo || R.bNoShowGo) { Finish(i, TEXT("was in the aircraft again when it gave up")); break; }
		if (X.T < 5.f || !ApronMay(P.Run, true)) break;
		G->StairsPose(O.Way.Ti, false, 0.f, 92.f, X.From); X.Ph = 6; X.K = O.Way.Ti; X.S = 0.f; X.T = 0.f; O.UpT = 0.f; P.Stage = TEXT("out of the aircraft again, down the passenger stairs");
		G->StairsEnter(X.K, 1); A->SetActorHiddenInGame(false); A->SetActorEnableCollision(true); ApronPose(i, X.From);
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
		FRun& R = Runs[Ri]; if (!R.Way || !(R.bArr ? R.bDeplaning : R.bCalled)) continue;
		TSharedPtr<FRunOut> Op = RunOuts.FindRef(R.Key);
		if (!Op)
		{
			if (R.bOff || R.bGone || R.bNoShowGo || (R.bArr && Inside(Ri) == 0)) continue;
			Op = MakeShared<FRunOut>(); RunOuts.Add(R.Key, Op); Op->Key = R.Key; Op->bArr = R.bArr; Op->Stand = R.Stand; Op->Gate = R.Gate;
			Op->S = AAirfield::Shared().Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == R.Stand; });
			if (Op->S) Op->StandX = Op->S->X;
			static FTerminalPlan Plan; static const bool bPlan = Plan.Load();
			const FString Room = FString::Printf(TEXT("F1-GATE-%d"), R.Gate);
			if (bPlan) for (const FPlanDoor& D : Plan.Doors) if (D.A == Room && D.B == TEXT("APRON")) Op->Doors.Add(D.X);
			if (!Op->S || !Op->Doors.Num()) ADP_WARN(TEXT("PaxFlow: %s can't be served on the apron - stand '%s'%s, gate %d has %d doors to the apron"), *R.Label, *R.Stand, Op->S ? TEXT("") : TEXT(" (not built)"), R.Gate, Op->Doors.Num());
			if (Op->Doors.Num()) Op->DoorX = Op->Doors[0];
		}
		FRunOut& O = *Op; O.T -= Dt; O.StairT -= Dt; O.UpT += Dt; O.SlotT -= Dt; O.StageT += Dt; O.SaidT -= Dt;
		if (R.bArr)
		{
			if (!O.S || !O.Doors.Num()) { R.Visible = R.Spawned; continue; }   // (no way off: they stay data)
			// (its turn-round is no more - another career loaded, or the aircraft is gone: nobody else comes out, the ones outside go on in)
			if ((O.TurnT -= Dt) <= 0.f)
			{
				O.TurnT = 2.f; ApronStairs(O);
				if ((R.Spawned < R.Visible || ApronCount(Ri, 9, 9) > 0) && !ADPTurnaround::All().ContainsByPredicate([&](const ADPTurnaround::FTurn& T) { return TEXT("A:") + T.Reg + TEXT(":") + FString::Printf(TEXT("%lld"), T.OnBlocks.GetTicks() / ETimespan::TicksPerMinute) == R.Key; }))
				{
					ADP_LOG(TEXT("PaxFlow: %s's aircraft is gone with %d still in it - they stay data, the others go on in"), *R.Label, R.Visible - R.Spawned);
					R.Visible = R.Spawned; R.bNoShowGo = true;
				}
			}
		}
		if (EADPPaxWay(R.Way) != EADPPaxWay::Bus || !O.S || !O.Doors.Num()) continue;
		ApronBus(Ri, O, Dt);
		// (to-do 296: the bus did not come, or fell out - the flight asks for it and checks back; those on the treads go back in meanwhile)
		O.NoBusT = O.Stage == 2 ? 0.f : O.NoBusT + Dt;
		O.bRetreat = R.bArr && O.NoBusT > 180.f && (O.Stage == 0 || O.Stage >= 5 || O.bStuck);
		if ((O.AskT -= Dt) > 0.f) continue;
		O.AskT = 2.f;
		FString Wy = O.Why; const bool bCan = ApronG->BusCan(Wy); O.NoWayT = bCan ? 0.f : O.NoWayT + 2.f;
		if (R.bOff || R.bGone || R.bNoShowGo || O.Stage >= 5 || !ApronStairs(O)) continue;   // (the stairs first: they ask for themselves)
		O.Ask = ADPStandAsk::Check(GetWorld(), ADPTurnaround::LastNow(), TEXT("bus:") + O.Key, TEXT("Apron bus"), FString::Printf(TEXT("%s (%s) at stand %s"), *R.Label, *R.Reg, *O.Stand), O.Stage >= 2 ? 2 : O.Stage == 1 && !O.bStuck ? 1 : 0, bCan, Wy);
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
		FString Why; const int32 Trip = ++O.Trips; O.bThere = false; O.Aim = At; O.AimH = Hd; TWeakObjectPtr<APaxFlow> Me = this; const FString Key = O.Key;
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
	// (ten minutes on its way: nobody in it - it goes home when it can and another one comes; with passengers in it, its way looked for anew)
	auto Slow = [&]() -> bool
	{
		if (O.StageT > 600.f && O.Stage == 1 && !O.bThere && O.Ti != INDEX_NONE && ApronCount(Ri, 2, 4) == 0 && ApronG->BusHalt(O.Ti))
		{
			ADP_WARN(TEXT("PaxFlow: %s's bus has been on its way for ten minutes - another one is called"), *R.Label);
			const int32 Ti = O.Ti; FString Wh; TWeakObjectPtr<AGSE> Gw = ApronG; AGSE::FBusSee Se; ++O.Trips;
			if (ApronG->BusSee(Ti, Se) && Se.bAtWheel && !ApronG->BusHome(Ti, [Gw, Ti] { if (Gw.IsValid()) Gw->BusLeave(Ti); }, Wh)) ADP_WARN(TEXT("PaxFlow: that bus can't go home (%s)"), *Wh);
			O.NotTi = Ti; O.NotT = RealT + 300.0; To(0); O.Ti = INDEX_NONE; O.bStuck = true;
			return true;
		}
		if (O.StageT > 600.f && O.Stage == 3 && !O.bThere && O.T <= 0.f) { O.T = 120.f; if (ApronG->BusHalt(O.Ti)) { ADP_WARN(TEXT("PaxFlow: %s's bus - its way is looked for anew"), *R.Label); Drive(O.Aim, O.AimH); } }
		if (O.StageT > 600.f && !O.bStuck) { O.bStuck = true; ADP_WARN(TEXT("PaxFlow: %s's bus has been on its way for ten minutes - it can't reach its place; its passengers wait"), *R.Label); }
		return false;
	};
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
		const FPx& P = Pax[i]; if (P.Run != Ri || P.St == EPx::Gone) continue;
		const TSharedPtr<FPaxOut> X = Outs.FindRef(i);
		if (P.bArr) { if (X && X->Ph >= 2 && X->Ph <= 4) ++In; else if (X && (X->Ph == 0 || (X->Ph == 6 && !X->bBack))) { ++Near; ++Come; } else if (X && (X->Ph == 5 || X->Ph == 6 || X->Ph == 9)) ++Come; continue; }
		if (P.bHome && (!X || X->Ph == 0)) continue;
		if (X && X->Ph >= 2 && X->Ph <= 4) ++In; else if (X && X->Ph == 0) { ++Near; ++Come; } else if (!X && !R.bNoShowGo) ++Come;
	}
	// another flight's bus stands there, or comes to
	auto Taken = [&](const FVector2f& At) { for (const TPair<FString, TSharedPtr<FRunOut>>& X : RunOuts) if (X.Value.Get() != &O && X.Value->Ti != INDEX_NONE && X.Value->Stage < (X.Value->bArr ? 5 : 3) && FVector2f::Distance(X.Value->Pick, At) < 18.f) return true; return false; };
	// the places at the aircraft: clear of the stairs' foot, the doors to it
	auto AtPlane = [&](int32 c, FVector2f& At, float& H)
	{
		const FAirfieldData::FStandFrame Fr = O.S->Frame(); const FVector2f S(Fr.K, 0.f), F = Fr.Face(); FVector2f Ft = Fr.P(11.5f, -0.3f);
		if (ApronStairs(O)) { const FVector V = PlanOf(O.Way.Foot); Ft = FVector2f(float(V.X), float(V.Z)); }
		auto Hd = [](const FVector2f& D) { return FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)); };
		const FVector2f Ats[4] = {Ft + S * 8.5f + F * 3.f, Ft + S * 8.5f + F * 6.f, Ft + S * 8.5f + F * 3.f, Ft + F * 9.f + S * 2.f}; const float Hs[4] = {Hd(F * -1.f), Hd(F * -1.f), Hd(F), Hd(S * -1.f)};
		At = Ats[c]; H = Hs[c];
	};
	auto SideToPlane = [&] { const FVector2f Ft = O.S->Frame().P(11.5f, -0.3f); SideTo(ApronStairs(O) ? O.Way.Foot : ApronG->ApronAt(Ft.X, Ft.Y)); };
	auto Beside = [&]
	{
		AGSE::FBusSee See; if (!ApronG->BusSee(O.Ti, See)) return true;
		for (const TPair<int32, TSharedPtr<FPaxOut>>& X : Outs) if (Pax.IsValidIndex(X.Key) && X.Value->Ph >= 5 && X.Value->Ph != 6 && X.Value->Ph != 9 && Pax[X.Key].A.IsValid()) { const FVector L = See.X.InverseTransformPosition(Pax[X.Key].A->GetActorLocation()); if (FMath::Abs(L.X) < 900. && FMath::Abs(L.Y) < 230.) return true; }
		return false;
	};
	// its driver lost on the way (it stands, nobody at the wheel): driven on by the next one
	if ((O.Stage == 1 || O.Stage == 3) && !O.bThere && O.StageT > 30.f && O.T <= 0.f)
	{
		O.T = 8.f; AGSE::FBusSee See;
		if (ApronG->BusSee(O.Ti, See) && See.bStands && !See.bAtWheel && !See.bYou && ApronG->BusTry(O.Ti, O.Aim, O.AimH)) Drive(O.Aim, O.AimH);
	}
	// (standing shut where they get out, its driver gone: the next one brings it to the same place, so its doors open)
	if (O.Stage == 4 && In && O.StageT > 30.f && O.T <= 0.f)
	{
		AGSE::FBusSee See;
		if (ApronG->BusSee(O.Ti, See) && See.bStands && See.bShut && !See.bAtWheel && !See.bYou && !Beside()) { if (ApronG->BusTry(O.Ti, O.Aim, O.AimH) && Drive(O.Aim, O.AimH)) { To(3); return; } O.T = 8.f; }
	}
	// (empty, taken away from its place or its driver lost: another one; passengers beside it step back first)
	bool bLost = false;
	if (O.Stage == 2 && O.StageT > 30.f)
	{
		AGSE::FBusSee See; const bool bSee = ApronG->BusSee(O.Ti, See), bAway = !bSee || FVector::Dist2D(See.X.GetLocation(), ApronG->ApronAt(O.Pick.X, O.Pick.Y)) > 1000.;
		bLost = bSee && See.bStands && !See.bAtWheel && !See.bYou;
		if (!In && (bAway || bLost)) { ADP_LOG(TEXT("PaxFlow: %s's bus %s - another one is called"), *R.Label, bAway ? TEXT("is gone from its place") : TEXT("has lost its driver")); To(0); O.T = 10.f; O.Ti = INDEX_NONE; return; }
	}
	switch (O.Stage)
	{
	case 0:
	{
		if (R.bArr ? Come == 0 : bNo || R.bNoShowGo || (Come == 0 && R.Spawned > 0)) { if (R.bArr && R.Visible == 0) return; To(6); return; }
		if (O.T > 0.f) return;
		O.T = 1.5f;
		TArray<int32> Free; ApronG->BusFree(Free);
		Free.RemoveAll([&](int32 Ti) { for (const TPair<FString, TSharedPtr<FRunOut>>& X : RunOuts) if (X.Value.Get() != &O && X.Value->Ti == Ti) return true; return false; });
		if (O.NotT > RealT && Free.Num() > 1) Free.Remove(O.NotTi);
		// (urgent: a bus that stands waiting for a flight less urgent, nobody in it or on the way to it)
		if (!Free.Num() && O.Ask >= 2)
			for (const TPair<FString, TSharedPtr<FRunOut>>& X : RunOuts)
			{
				FRunOut& V = *X.Value; if (&V == &O || V.Stage != 2 || V.Ask >= O.Ask || V.Ti == INDEX_NONE || (O.NotT > RealT && V.Ti == O.NotTi)) continue;
				const int32 Vr = Runs.IndexOfByPredicate([&](const FRun& Q) { return Q.Key == X.Key; }); if (Vr == INDEX_NONE || ApronCount(Vr, 0, 6) > 0) continue;
				int32 c = 0; const int32 Nc = R.bArr ? 4 : O.Doors.Num();
				for (; c < Nc; ++c) { FVector2f At(0.f, 90.5f); float H = 0.f; if (R.bArr) AtPlane(c, At, H); else At.X = O.Doors[c]; if (!Taken(At) && ApronG->BusTry(V.Ti, At, H)) break; }
				if (c >= Nc) continue;
				ADP_LOG(TEXT("PaxFlow: %s's bus, nobody at it yet, is called to %s, which has asked for one %d times"), *Runs[Vr].Label, *R.Label, O.Ask);
				Free.Add(V.Ti); O.Cand = c; V.Stage = 0; V.StageT = 0.f; V.T = 30.f; V.Cand = 0; V.SlotT = 0.f; V.Ti = INDEX_NONE; ++V.Trips; break;
			}
		if (!Free.Num()) { O.T = 10.f; Say(R.bArr ? TEXT("no apron bus is free: its passengers wait in the aircraft") : TEXT("no apron bus is free: its passengers wait at the gate")); return; }
		if (R.bArr)
		{
			const int32 N = Free.Num() * 4, c = O.Cand++ % N, Ti = Free[c / 4]; FVector2f At; float H; AtPlane(c % 4, At, H);
			if (Taken(At) || !ApronG->BusTry(Ti, At, H)) { if (O.Cand >= N) { O.Cand = 0; O.T = 10.f; Say(TEXT("no bus has a clear way to the aircraft now: its passengers wait in it")); } return; }
			O.Ti = Ti; O.Pick = At;
			if (!Drive(At, H)) { O.Ti = INDEX_NONE; O.T = 10.f; return; }
			To(1); ADP_LOG(TEXT("PaxFlow: %s's bus is called to stand %s"), *R.Label, *O.Stand);
			return;
		}
		// (in front of one of the gate's two doors, its door side to it)
		const int32 N = Free.Num() * O.Doors.Num(), c = O.Cand++ % N, Ti = Free[c / O.Doors.Num()]; const FVector2f At(O.Doors[c % O.Doors.Num()], 90.5f);
		if (Taken(At)) return;
		if (!ApronG->BusTry(Ti, At, 0.f)) { if (O.Cand >= N) { O.Cand = 0; O.T = 10.f; Say(TEXT("no bus has a clear way to the gate now: its passengers wait")); } return; }
		O.Ti = Ti; O.DoorX = At.X; O.Pick = At;
		if (!Drive(At, 0.f)) { O.Ti = INDEX_NONE; O.T = 10.f; return; }   // (nobody free: Why says so)
		To(1); ADP_LOG(TEXT("PaxFlow: %s's bus is called to gate %d"), *R.Label, O.Gate);
		return;
	}
	case 1:
		if (Slow() || !O.bThere) return;
		if (R.bArr) { SideToPlane(); To(2); ADP_LOG(TEXT("PaxFlow: %s's bus stands at the aircraft"), *R.Label); return; }
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
		// (arriving: never while somebody is on the stairs or beside it; with no stairs any more those in it at least)
		const bool bGo = In > 0 && !Getting && (R.bArr ? Near == 0 && (Come == 0 || bFull || bLost || (!ApronStairs(O) && O.StageT > 180.f)) : Come == 0 || (bFull && (Near == 0 || O.StageT > 180.f)));
		if (!bGo) { OpenUp(); if (In == 0 && Come == 0) Home(); return; }
		if (O.T > 0.f) { return; }
		O.T = 1.5f;
		if (R.bArr)
		{
			// (to one of the gate's doors)
			const int32 N = O.Doors.Num(), c = O.Cand++ % N; const FVector2f At(O.Doors[c], 90.5f);
			if (Taken(At) || !ApronG->BusTry(O.Ti, At, 0.f)) { if (c == N - 1) { O.T = 8.f; Say(TEXT("no clear place at the gate now: it waits at the aircraft with its passengers")); } return; }
			if (!Drive(At, 0.f)) { O.T = 8.f; return; }
			O.DoorX = At.X; O.Pick = At; To(3); ADP_LOG(TEXT("PaxFlow: %s's bus leaves stand %s with %d passengers, %d still to come (%s)"), *R.Label, *O.Stand, In, Come, Come ? TEXT("it comes back for them") : TEXT("everybody in"));
			return;
		}
		FVector2f At; float H; const int32 c = O.Cand++ % 4; AtPlane(c, At, H);
		if (Taken(At) || !ApronG->BusTry(O.Ti, At, H)) { if (c == 3) { O.T = 8.f; Say(TEXT("no clear way to the aircraft now: it waits at the gate with its passengers")); } return; }
		if (!Drive(At, H)) { O.T = 8.f; return; }
		To(3); ADP_LOG(TEXT("PaxFlow: %s's bus leaves gate %d with %d passengers, %d still to come (%s)"), *R.Label, O.Gate, In, Come, Come ? TEXT("full: it comes back for them") : TEXT("everybody in"));
		return;
	}
	case 3:
		Slow(); if (!O.bThere) return;
		if (R.bArr) { SideTo(ApronG->ApronAt(O.DoorX, 84.f)); To(4); ADP_LOG(TEXT("PaxFlow: %s's bus stands at gate %d"), *R.Label, O.Gate); return; }
		SideToPlane(); To(4); ADP_LOG(TEXT("PaxFlow: %s's bus is at the aircraft"), *R.Label);
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
			if (!R.bArr && !ApronStairs(O)) Say(TEXT("at the aircraft: its passengers wait in it for the passenger stairs"));
			return;
		}
		if (O.T > 0.f) return;
		O.T = 5.f;
		if (Beside()) return;   // (nobody beside it any more)
		O.T = 0.f;
		if (R.bArr && Come)
		{
			const int32 c = O.Cand++ % 4; FVector2f At; float H; AtPlane(c, At, H);
			if (Taken(At) || !ApronG->BusTry(O.Ti, At, H)) { if (c == 3) { O.T = 8.f; Say(TEXT("no way back to the aircraft for the others now")); } return; }
			if (Drive(At, H)) { O.Pick = At; To(1); ADP_LOG(TEXT("PaxFlow: %s's bus goes back to stand %s for %d more"), *R.Label, *O.Stand, Come); }
			else O.T = 8.f;
			return;
		}
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
