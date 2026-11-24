// The wheel chocks and the apron safety cones of the gates (to-do 212, docs/Blueprints/Apron/ApronSafety.png; the models:
// Art/ApronSafety). They live on a rack at the head of the stand, beside the ground power unit. An aircraft on blocks, its
// beacon off: one of the ground crew takes the pair of chocks by its rope, crouches at the nose wheel and sets them fore
// and aft of it, then a pair at the right main wheel (the left one too when it stands two hours or more); then the cones,
// all five in one round: engine, wingtip, tail, wingtip, engine. Before the push, the vehicles gone, a wing walker (else
// the pushback handler) brings the cones in; the chocks come off last, with the tug hooked on (the mains first, the nose
// wheel's at the very end) - the push waits for that (ATugs::Push, ChkOut). Your own flights: one of your hired people,
// from the same queue as the ground power; the other airlines: a free walker, else the gate's tug crew.
// Every place is in stand metres: lat to the aircraft's right, aft from the nose wheel.
#include "Tugs.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "CrewPose.h"
#include "GSE.h"
#include "People.h"
#include "StaffPeople.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
	enum : uint8 { CkRack = 0, CkRight = 1, CkLeft = 2, CkOut = 3, CkLying = 4 };   // where an item is
	enum : uint8 { CkChock, CkStow, CkCarry, CkNose, CkMain, CkCone, CkRackM };     // the meshes
	const TCHAR* const CkNames[] = {TEXT("Chock"), TEXT("RopeStow"), TEXT("RopeCarry"), TEXT("RopeNose"), TEXT("RopeMain"), TEXT("Cone"), TEXT("Rack")};
	const int32 CkPer[] = {6, 3, 3, 1, 2, 5, 1};   // how many at each gate
	constexpr float CkRackLat = 11.6f, CkRackAft = -7.1f;
	struct FCkPlace { float Lat, Aft; };
	// the chocks: the right nose wheel, the right and the left main gear's outer wheel (Art/A320 measured)
	const FCkPlace CkPair[3] = {{0.35f, 0.f}, {4.45f, 12.80f}, {-4.45f, 12.80f}};
	const float CkGap[3] = {37.8f, 45.4f, 45.4f};   // a chock's middle from the wheel's (Art/ApronSafety/sizes.json)
	// the cones: the right engine's inlet, the right wingtip, behind the tail, the left wingtip, the left engine's inlet
	const FCkPlace CkCones[5] = {{5.6f, 4.9f}, {16.9f, 14.9f}, {0.f, 33.4f}, {-16.9f, 14.9f}, {-5.6f, 4.9f}};
	const FVector CkUp(0, 0, 1);
	ATugs::FCrewAct CkWalk(const FVector& To, float Stop = 18.f) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Walk; A.To = [To] { return To; }; A.Stop = Stop; A.Max = 150.f; return A; }
	ATugs::FCrewAct CkPose(ATugs::FPoseFn Fn, float Dur) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Pose; A.PoseFn = MoveTemp(Fn); A.Dur = Dur; return A; }
	ATugs::FCrewAct CkWait(float Dur) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Wait; A.Dur = Dur; return A; }
	ATugs::FCrewAct CkDo(TFunction<void()> F) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Do; A.Fn = MoveTemp(F); return A; }
	struct FCkReach { FVector At, Face, Hl, Hr; bool bL = false, bR = true; float Drop = 0.f, Lean = 12.f; };
	/** Down by Drop (0 standing .. 0.45 on the haunches), one hand or both at their places; E 0..1 how far. */
	FDriverPose CkReach(const FCkReach& R, float PH, float E)
	{
		const FDriverPose S = ADPPose::Standing(R.At, R.Face, PH); FDriverPose D = S; const FVector L = ADPPose::LeftOf(S.Facing);
		D.Pelvis = R.At + CkUp * (PH * (1.f - R.Drop)) - S.Facing * (R.Drop * 30.f); D.Lean = R.Lean;
		const float Wd = R.Drop > 0.2f ? 19.f : 13.f; D.Foot[0] = R.At + L * Wd + S.Facing * 4.f; D.Foot[1] = R.At - L * Wd + S.Facing * 4.f;
		if (R.bL) { D.bHand[0] = true; D.Hand[0] = R.Hl; D.Grip[0] = 0.9f; }
		if (R.bR) { D.bHand[1] = true; D.Hand[1] = R.Hr; D.Grip[1] = 0.9f; }
		return ADPPose::Lerp(S, D, E);
	}
}

FVector ATugs::ChkAt(const FTug& T, float Lat, float Aft) const
{
	const float B = -T.StandDir;
	return Plan(T.StandX - B * Lat, T.StandNose + B * (5.1f + Aft));
}

void ATugs::ChkSetup()
{
	bChkReady = true;
	for (int32 m = 0; m < 7; ++m)
	{
		UStaticMesh* Sm = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Babylon/ApronSafety/apronsafety/StaticMeshes/%s.%s"), CkNames[m], CkNames[m]));
		if (!Sm) { ADP_WARN(TEXT("Chocks: the model %s is missing (Tools/import_model.py - Art/ApronSafety)"), CkNames[m]); ChkMesh.Reset(); return; }
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this);
		K->SetStaticMesh(Sm); K->SetupAttachment(RootComponent); K->SetUsingAbsoluteLocation(true); K->SetUsingAbsoluteRotation(true); K->SetUsingAbsoluteScale(true);
		if (m == CkRackM) K->SetCollisionProfileName(TEXT("BlockAll")); else K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		K->SetCanEverAffectNavigation(false);
		K->RegisterComponent(); K->SetWorldTransform(FTransform::Identity);
		// (the rope lies on the models' outer side: which way that is after the import)
		if (m == CkNose) { const FVector O = Sm->GetBounds().Origin; ChkYaw = FVector(O.X, O.Y, 0.f).Rotation().Yaw; }
		for (int32 i = 0; i < Tugs.Num() * CkPer[m]; ++i)
		{
			FTransform X(FQuat::Identity, FVector::ZeroVector, FVector(0.0001f));
			if (m == CkRackM)
			{
				const FTug& T = Tugs[i]; const FVector Aft = (ChkAt(T, 0.f, 1.f) - ChkAt(T, 0.f, 0.f)).GetSafeNormal2D();
				X = FTransform(FRotator(0.f, Aft.Rotation().Yaw - ChkYaw, 0.f), ChkAt(T, CkRackLat, CkRackAft));
			}
			K->AddInstance(X, true);
		}
		ChkMesh.Add(K);
	}
	ADP_LOG(TEXT("Chocks: racks with three pairs of chocks and five cones at %d gates"), Tugs.Num());
}

void ATugs::ChkPlace(const FTug& T, int32 It, FTransform Out[2], uint8& Rope) const
{
	const FChk& Ck = T.Ck; const FChkItem& I = Ck.Items[It];
	const FVector Right = (ChkAt(T, 1.f, 0.f) - ChkAt(T, 0.f, 0.f)).GetSafeNormal2D(), Aft = (ChkAt(T, 0.f, 1.f) - ChkAt(T, 0.f, 0.f)).GetSafeNormal2D();
	auto Flat = [this](const FVector& OutDir) { return FRotator(0.f, OutDir.Rotation().Yaw - ChkYaw, 0.f).Quaternion(); };
	const bool bCone = It >= 3; Rope = CkStow;
	const FVector Rk = ChkAt(T, CkRackLat, CkRackAft);
	if (I.Where == CkRack)
	{
		if (bCone) { Out[0] = FTransform(Flat(Aft), Rk + Aft * 77.f + CkUp * (2.f + 6.f * (It - 3))); return; }
		const FVector C = Rk + Aft * (-72.f + 48.f * It) + CkUp * 26.f;
		for (int32 s = 0; s < 2; ++s) Out[s] = FTransform(Flat(-Right), C + Aft * (s ? 11.5f : -11.5f));
		return;
	}
	if (I.Where == CkOut)
	{
		if (bCone) { Out[0] = FTransform(Flat(Aft), ChkAt(T, CkCones[It - 3].Lat, CkCones[It - 3].Aft)); return; }
		const FVector OutD = CkPair[It].Lat > 0.f ? Right : -Right, C = ChkAt(T, CkPair[It].Lat, CkPair[It].Aft);
		Rope = It == 0 ? CkNose : CkMain;
		for (int32 s = 0; s < 2; ++s) Out[s] = FTransform(Flat(OutD), C + Aft * (s ? CkGap[It] : -CkGap[It]));
		return;
	}
	if (I.Where == CkLying) { Out[0] = I.Lie[0]; Out[1] = I.Lie[1]; return; }
	// in a hand
	const int32 Ci = Ck.Ci; const bool bR = I.Where == CkRight;
	FVector Hand = Crew.IsValidIndex(Ci) ? Crew[Ci].LastAt + CkUp * 80.f : Rk + CkUp * 80.f, Face = Aft; double Ground = Rk.Z;
	if (Crew.IsValidIndex(Ci) && Crew[Ci].A.IsValid())
	{
		AActor* A = Crew[Ci].A.Get(); AADPPeople* Pp = AADPPeople::Get(GetWorld()); const AADPPeople::FWho* W = Pp ? Pp->Find(A) : nullptr;
		Face = A->GetActorRotation().RotateVector(FVector(0, 1, 0)).GetSafeNormal2D(); Ground = A->GetActorLocation().Z;
		Hand = W && W->Body ? W->Body->GetBoneLocation(bR ? TEXT("hand_r") : TEXT("hand_l")) - CkUp * 7.f : A->GetActorLocation() + CkUp * 80.f;
	}
	const FVector Side = ADPPose::LeftOf(Face) * (bR ? -1.f : 1.f);   // (out from the body)
	if (bCone)
	{
		// (by the tip, leaning out from the leg; the ones in the same hand nested)
		int32 Rank = 0; for (int32 k = 3; k < It; ++k) if (Ck.Items[k].Where == I.Where) ++Rank;
		const FVector Ax = (CkUp * 0.978f - Side * 0.208f).GetSafeNormal();
		FVector P = Hand - Ax * 71.f; P.Z = FMath::Max(P.Z, Ground + 6.0);
		Out[0] = FTransform(FQuat::FindBetweenNormals(CkUp, Ax) * Flat(Face), P + Ax * (6.f * Rank));
		return;
	}
	// (hanging by the rope's grip: one before the other, upright, the rings up)
	Rope = CkCarry;
	const FQuat Q = FQuat::FindBetweenNormals(Side, CkUp) * Flat(Side);
	for (int32 s = 0; s < 2; ++s) Out[s] = FTransform(Q, Hand + Face * (s ? 10.4f : -10.4f) - CkUp * (18.f + 32.8f) + Side * 6.2f);
}

void ATugs::ChkDraw(int32 Tug)
{
	FTug& T = Tugs[Tug];
	for (int32 It = 0; It < 8; ++It)
	{
		FChkItem& I = T.Ck.Items[It];
		const bool bHand = I.Where == CkRight || I.Where == CkLeft;
		if (!I.bDirty && !bHand && I.Bl >= 1.f) continue;
		I.bDirty = I.Bl < 1.f;
		FTransform X[2]; uint8 Rope = CkStow; ChkPlace(T, It, X, Rope);
		if (I.Bl < 1.f) for (int32 s = 0; s < 2; ++s) { FTransform Y; Y.Blend(I.From[s], X[s], FMath::SmoothStep(0.f, 1.f, I.Bl)); X[s] = Y; }
		I.Now[0] = X[0]; I.Now[1] = X[1];
		if (It >= 3) { ChkMesh[CkCone]->UpdateInstanceTransform(Tug * 5 + It - 3, X[0], true, true, true); continue; }
		for (int32 s = 0; s < 2; ++s) ChkMesh[CkChock]->UpdateInstanceTransform(Tug * 6 + It * 2 + s, X[s], true, true, true);
		// (its rope: the one shape of the four for where it is, between its two chocks)
		const FVector Mid = (X[0].GetLocation() + X[1].GetLocation()) * 0.5f;
		FTransform R(X[0].GetRotation(), Mid);
		if (Rope == CkCarry)
		{
			const FVector OutD = X[0].GetRotation().RotateVector(FRotator(0.f, ChkYaw, 0.f).Vector());
			FVector Side = X[0].GetRotation().RotateVector(FVector(0, 0, -1)); Side.Z = 0.f;   // (hanging upright: the chock's underside looks out)
			Side = Side.GetSafeNormal2D(); if (Side.IsNearlyZero()) Side = OutD.GetSafeNormal2D();
			R = FTransform(FRotator(0.f, Side.Rotation().Yaw - ChkYaw, 0.f), Mid + OutD * 32.8f + CkUp * 18.f - Side * 6.2f);
		}
		const FTransform Off(FQuat::Identity, Mid, FVector(0.0001f));
		ChkMesh[CkStow]->UpdateInstanceTransform(Tug * 3 + It, Rope == CkStow ? R : Off, true, true, true);
		ChkMesh[CkCarry]->UpdateInstanceTransform(Tug * 3 + It, Rope == CkCarry ? R : Off, true, true, true);
		if (It == 0) ChkMesh[CkNose]->UpdateInstanceTransform(Tug, Rope == CkNose ? R : Off, true, true, true);
		else ChkMesh[CkMain]->UpdateInstanceTransform(Tug * 2 + It - 1, Rope == CkMain ? R : Off, true, true, true);
	}
}

void ATugs::ChkMove(int32 Tug, int32 It, uint8 To)
{
	FChkItem& I = Tugs[Tug].Ck.Items[It];
	I.From[0] = I.Now[0]; I.From[1] = I.Now[1]; I.Where = To; I.Bl = 0.f; I.bDirty = true;
}

void ATugs::ChkTake(int32 Tug, FCrew& C, int32 It, bool bTake, uint8 To)
{
	// at the place it is in now (bTake) or goes to: walk up, down with the hands on it, taken or put down
	FTug& T = Tugs[Tug]; const FChkItem& I = T.Ck.Items[It]; const uint8 At = bTake ? I.Where : To;
	const FVector Right = (ChkAt(T, 1.f, 0.f) - ChkAt(T, 0.f, 0.f)).GetSafeNormal2D(), Aft = (ChkAt(T, 0.f, 1.f) - ChkAt(T, 0.f, 0.f)).GetSafeNormal2D();
	const FVector Rk = ChkAt(T, CkRackLat, CkRackAft);
	FCkReach R; float Dur = 1.6f;
	if (It >= 3)
	{
		// (the cone's tip, in the hand that carries it; at the rack the stack's)
		R.bR = It - 3 < 3; R.bL = !R.bR;
		FVector P = Rk + Aft * 77.f; float Tip = 80.f; R.At = P - Right * 58.f;
		if (At != CkRack)
		{
			Tip = 72.f; const FCkPlace& Pl = CkCones[It - 3];
			P = At == CkLying ? I.Lie[0].GetLocation() : ChkAt(T, Pl.Lat, Pl.Aft);
			R.At = P + (It == 5 ? Aft : Pl.Lat > 0.f ? Right : -Right) * 52.f;
		}
		R.Face = (P - R.At).GetSafeNormal2D(); R.Hl = R.Hr = P + CkUp * Tip; R.Lean = 16.f; R.Drop = 0.06f;
	}
	else if (At == CkRack)
	{
		// (by the red grip in the middle of the rope)
		const FVector P = Rk + Aft * (-72.f + 48.f * It); R.At = P - Right * 66.f; R.Face = Right; R.Hl = R.Hr = P + CkUp * 45.f; R.Drop = 0.24f; R.Lean = 44.f; Dur = 2.0f;
	}
	else if (At == CkLying)
	{
		const FVector P = (I.Lie[0].GetLocation() + I.Lie[1].GetLocation()) * 0.5f; R.At = P - Right * 60.f; R.Face = Right; R.Hl = R.Hr = P + CkUp * 18.f; R.Drop = 0.42f; R.Lean = 40.f; Dur = 2.2f;
	}
	else
	{
		// on the haunches outside the wheel, a hand on each chock: fore and aft of the tyre
		const FVector OutD = CkPair[It].Lat > 0.f ? Right : -Right, P = ChkAt(T, CkPair[It].Lat, CkPair[It].Aft);
		R.At = P + OutD * 82.f; R.Face = -OutD; const FVector L = ADPPose::LeftOf(R.Face);
		R.Hl = P + L * CkGap[It] + OutD * 24.f + CkUp * 14.f; R.Hr = P - L * CkGap[It] + OutD * 24.f + CkUp * 14.f;
		R.bL = true; R.Drop = 0.45f; R.Lean = 42.f; Dur = 3.4f;
	}
	C.Q.Add(CkWalk(R.At, 12.f));
	// (two halves: the hands are on it when it changes hands)
	C.Q.Add(CkPose([R](float K, float PH, FDriverPose& P) { P = CkReach(R, PH, FMath::SmoothStep(0.f, 0.9f, K)); }, Dur * 0.45f));
	C.Q.Add(CkDo([this, Tug, It, To] { ChkMove(Tug, It, To); }));
	C.Q.Add(CkPose([R](float K, float PH, FDriverPose& P) { P = CkReach(R, PH, 1.f - FMath::SmoothStep(0.45f, 1.f, K)); }, Dur * 0.55f));
}

void ATugs::ChkWork(int32 Tug, int32 Ci)
{
	// the worker's acts for the stage it is in, from where each item is now (so another one can go on with it)
	FTug& T = Tugs[Tug]; FChk& Ck = T.Ck; FCrew& C = Crew[Ci]; Ck.Ci = Ci;
	auto Way = [&](float Lat, float Aft) { C.Q.Add(CkWalk(ChkAt(T, Lat, Aft), 30.f)); };
	// (across behind the nose gear, under the fuselage, 2.2 m high there: clear of the towbar and the ground power's cable)
	auto Under = [&](bool bToLeft) { const float S = bToLeft ? 1.f : -1.f; Way(2.8f * S, 2.2f); Way(-2.8f * S, 2.2f); };
	// (round the engines)
	auto Main = [&](bool bGo) { if (bGo) { Way(8.4f, 6.f); Way(7.6f, 11.6f); } else { Way(7.6f, 11.6f); Way(8.4f, 6.f); } };
	auto MainL = [&](bool bGo) { if (bGo) { Under(true); Way(-3.0f, 5.2f); Way(-8.0f, 5.6f); Way(-7.6f, 11.6f); } else { Way(-7.6f, 11.6f); Way(-8.0f, 5.6f); Way(-3.0f, 5.2f); Under(false); } };
	const uint8 Stage = Ck.Stage;
	if (Stage == 1)
	{
		C.Q.Add(CkWait(8.f));   // (the beacon off: 8 s after it stands)
		for (int32 p = 0; p < (Ck.bLong ? 3 : 2); ++p)
		{
			if (Ck.Items[p].Where == CkOut) continue;
			if (Ck.Items[p].Where != CkRight) ChkTake(Tug, C, p, true, CkRight);
			if (p == 1) Main(true);
			if (p == 2) MainL(true);
			ChkTake(Tug, C, p, false, CkOut);
			if (p == 1) Main(false);
			if (p == 2) MainL(false);
		}
		C.Q.Add(CkDo([this, Tug] { FTug& U = Tugs[Tug]; ADP_LOG(TEXT("Chocks: in at x %.0f%s"), U.StandX, U.Ck.bLong ? TEXT(" (both main wheels)") : TEXT("")); }));
		// all five cones in one round, three in the right hand and two in the left
		bool bAny = false;
		for (int32 k = 4; k >= 0; --k)
		{
			const uint8 Wh = Ck.Items[3 + k].Where; if (Wh == CkOut) continue;
			bAny = true; if (Wh == CkRack || Wh == CkLying) ChkTake(Tug, C, 3 + k, true, k >= 3 ? CkLeft : CkRight);
		}
		if (bAny)
		{
			for (int32 k = 0; k < 5; ++k)
			{
				if (k == 2) Way(15.5f, 33.4f);
				if (k == 3) Way(-15.5f, 33.4f);
				if (k == 4) Way(-8.4f, 5.4f);
				if (Ck.Items[3 + k].Where != CkOut) ChkTake(Tug, C, 3 + k, false, CkOut);
			}
			Way(-3.0f, 5.2f); Under(false);
		}
		Way(9.6f, -4.f);
		C.Q.Add(CkDo([this, Tug] { FTug& U = Tugs[Tug]; U.Ck.Stage = 2; U.Ck.Ci = INDEX_NONE; ADP_LOG(TEXT("Chocks: the cones are out at x %.0f"), U.StandX); }));
	}
	else if (Stage == 3)
	{
		Way(9.0f, -3.5f); Way(6.5f, 2.2f); Under(true); Way(-3.0f, 5.2f);
		for (int32 k = 4; k >= 0; --k)
		{
			if (k == 3) Way(-8.4f, 5.4f);
			if (k == 2) Way(-15.5f, 33.4f);
			if (k == 1) Way(15.5f, 33.4f);
			const uint8 Wh = Ck.Items[3 + k].Where;
			if (Wh == CkOut || Wh == CkLying) ChkTake(Tug, C, 3 + k, true, k >= 3 ? CkLeft : CkRight);
		}
		Way(9.6f, -4.f);
		for (int32 k = 0; k < 5; ++k) if (Ck.Items[3 + k].Where != CkRack) ChkTake(Tug, C, 3 + k, false, CkRack);
		C.Q.Add(CkDo([this, Tug] { FTug& U = Tugs[Tug]; U.Ck.Stage = 4; U.Ck.Ci = INDEX_NONE; ADP_LOG(TEXT("Chocks: the cones are in at x %.0f"), U.StandX); }));
	}
	else
	{
		for (int32 p = 2; p >= 0; --p)
		{
			const uint8 Wh = Ck.Items[p].Where; if (Wh == CkRack) continue;
			if (Wh != CkRight)
			{
				Way(8.4f, 0.f);
				if (p == 1) Main(true);
				if (p == 2) MainL(true);
				ChkTake(Tug, C, p, true, CkRight);
				if (p == 1) Main(false);
				if (p == 2) MainL(false);
			}
			ChkTake(Tug, C, p, false, CkRack);
		}
		C.Q.Add(CkDo([this, Tug]
		{
			FTug& U = Tugs[Tug]; U.Ck.Stage = 0; U.Ck.bLoose = false; U.Ck.Ci = INDEX_NONE;
			ADP_LOG(TEXT("Chocks: off and back on the rack at x %.0f"), U.StandX);
			if (AtNose(U)) CrewWalkers(Tug);   // (now the walkers go out to their places)
		}));
	}
	// and what they go on to
	if (Ck.bOwn)
	{
		if (C.bHired) { C.Q.Add(CkWalk(Plan(T.StandX - 18.5f, 104.f), 80.f)); LeaveAct(C); }
		else { C.Q.Add(CkWalk(C.Spot, 30.f)); C.Q.Add(CkDo([this, Ci] { Crew[Ci].bGse = false; Crew[Ci].AnchorYaw = Crew[Ci].SpotYaw; })); }
	}
	else if (Stage == 3) C.Q.Add(CkWalk(T.Handler == Ci ? Plan(T.StandX - 6.f, 110.f) : Plan(T.StandX - 19.5f, 100.f), 40.f));
}

int32 ATugs::ChkWorker(int32 Tug)
{
	FTug& T = Tugs[Tug]; FChk& Ck = T.Ck; Ck.bOwn = false;
	auto Idle = [this](int32 i)
	{
		if (!Crew.IsValidIndex(i)) return false;
		const FCrew& C = Crew[i]; return C.A.IsValid() && !C.bSlotFree && !C.bLeave && !C.Q.Num() && !C.Hold && !C.bLost;
	};
	if (!T.Key.IsEmpty() && Ck.Stage >= 3)
	{
		// the pushback crew: a walker standing by, else the handler
		for (const int32 W : T.Walkers) if (Idle(W)) return W;
		if (!T.Walkers.Num() && Idle(T.Handler)) return T.Handler;
		return INDEX_NONE;
	}
	Ck.bOwn = true;
	if (Ck.bHired)
	{
		UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); if (!Sp) return INDEX_NONE;
		TArray<UStaffPeople::FTaken> Team;
		// (two minutes ahead of the same flight's other jobs: the vehicles come after it)
		if (!Sp->AskTeam(TEXT("chocks:") + Ck.Reg, EStaffTask::Chocks, Ck.When - FTimespan::FromMinutes(2.0), ChkAt(T, CkRackLat, CkRackAft), FString::Printf(TEXT("%s chocks and cones"), *Ck.Reg), Team) || !Team.Num()) return INDEX_NONE;
		const int32 Ci = Enlist(Team[0].A.Get(), Team[0].Id, TEXT("chocks and cones"));
		if (Ci != INDEX_NONE) SafeWay(Crew[Ci], T.StandX);
		return Ci;
	}
	for (int32 i = 0; i < Crew.Num(); ++i)
		if (Crew[i].bWalker && Crew[i].Tug == INDEX_NONE && !Crew[i].bGse && !Crew[i].bHired && Idle(i))
		{
			Crew[i].bGse = true; SafeWay(Crew[i], T.StandX); return i;   // (bGse: at another job)
		}
	// (no walker free: the gate's own handler or driver, when they aren't at anything)
	if (T.State == EState::Parked) for (const int32 i : {T.StandInHandler, T.Handler, T.StandInDriver, T.Driver}) if (Idle(i) && !Crew[i].bGse && !Crew[i].bHired) { Crew[i].bGse = true; return i; }
	return INDEX_NONE;
}

void ATugs::ChkTick(int32 Tug, float Dt)
{
	if (!bChkReady) ChkSetup();
	if (!ChkMesh.Num()) return;
	FTug& T = Tugs[Tug]; FChk& Ck = T.Ck;
	for (FChkItem& I : Ck.Items) if (I.Bl < 1.f) I.Bl = FMath::Min(1.f, I.Bl + Dt / 0.7f);
	ChkDraw(Tug);
	if ((Ck.Clock -= Dt) > 0.f) return;
	Ck.Clock = 2.f;
	// (the worker taken off it: what they carried lies where it is, the next one goes on)
	if (Ck.Ci != INDEX_NONE && (!Crew.IsValidIndex(Ck.Ci) || Crew[Ck.Ci].bSlotFree || (!Crew[Ck.Ci].Q.Num() && !Crew[Ck.Ci].bLost)))
	{
		ADP_WARN(TEXT("Chocks: nobody at the job any more at x %.0f - the next one goes on"), T.StandX);
		const double Z = ChkAt(T, CkRackLat, CkRackAft).Z;
		for (FChkItem& I : Ck.Items) if (I.Where == CkRight || I.Where == CkLeft)
		{
			for (int32 s = 0; s < 2; ++s)
			{
				FVector P = I.Now[s].GetLocation(); P.Z = Z;
				I.Lie[s] = FTransform(FRotator(0.f, -ChkYaw, 0.f), P);
			}
			// (the pair side by side on the ground)
			const FVector Mid = (I.Lie[0].GetLocation() + I.Lie[1].GetLocation()) * 0.5f;
			if (&I - Ck.Items < 3) for (int32 s = 0; s < 2; ++s) I.Lie[s].SetLocation(Mid + FVector(0, s ? 11.5f : -11.5f, 0));
			I.Where = CkLying; I.bDirty = true;
		}
		Ck.Ci = INDEX_NONE;
	}
	if (Ck.Ci != INDEX_NONE) return;
	const bool bCrew = !T.Key.IsEmpty() && (T.State == EState::Waiting || AtNose(T));
	// (never set in time: the departure's crew is here already - nothing more is put out)
	if (Ck.Stage == 1 && bCrew)
	{
		bool bOut = false; for (const FChkItem& I : Ck.Items) if (I.Where != CkRack) bOut = true;
		Ck.Stage = bOut ? 2 : 0;
	}
	if (Ck.Stage == 2)
	{
		const AGSE* G = AGSE::Find(GetWorld());
		if (Ck.bLoose || (bCrew && !(G && G->BusyAt(T.StandX)))) Ck.Stage = 3;   // (the vehicles gone first)
	}
	if (Ck.Stage == 4 && (Ck.bLoose || (bCrew && AtNose(T)))) Ck.Stage = 5;   // (the tug connected)
	if (Ck.Stage != 1 && Ck.Stage != 3 && Ck.Stage != 5) { Ck.WaitT = 0.f; return; }
	const int32 Ci = ChkWorker(Tug);
	if (Ci == INDEX_NONE)
	{
		if ((Ck.WaitT += 2.f) > 60.f) { Ck.WaitT = 0.f; ADP_LOG(TEXT("Chocks: nobody free at x %.0f (%s)"), T.StandX, Ck.Stage == 1 ? TEXT("to set them") : Ck.Stage == 3 ? TEXT("to bring the cones in") : TEXT("to take the chocks off")); }
		return;
	}
	Ck.WaitT = 0.f;
	ADP_LOG(TEXT("Chocks: %s %s at x %.0f"), *Crew[Ci].Name, Ck.Stage == 1 ? TEXT("sets the chocks and cones") : Ck.Stage == 3 ? TEXT("brings the cones in") : TEXT("takes the chocks off"), T.StandX);
	ChkWork(Tug, Ci);
}

bool ATugs::ChocksOn(float StandX, const FString& Reg, const FDateTime& When, bool bHired, bool bLong, bool bAtOnce)
{
	if (bChkReady && !ChkMesh.Num()) return true;   // (the models missing: said once, nothing waits)
	for (FTug& T : Tugs)
	{
		FChk& Ck = T.Ck;
		if (FMath::Abs(T.StandX - StandX) >= 1.f) continue;
		if (Ck.Stage != 0) return (Ck.Stage == 1 || Ck.Stage == 2) && !Ck.bLoose;   // (else: the last ones are still being cleared)
		if (!bHired && (T.bHired ? T.StandInDriver : T.Driver) == INDEX_NONE) return true;   // (a gate without a crew)
		Ck.Reg = Reg; Ck.When = When; Ck.bHired = bHired; Ck.bLong = bLong; Ck.bLoose = false; Ck.Clock = 0.f; Ck.Stage = 1;
		if (bAtOnce)
		{
			// (it has stood here since before you came: they are in)
			for (int32 It = 0; It < 8; ++It) if (It != 2 || bLong) { Ck.Items[It].Where = CkOut; Ck.Items[It].Bl = 1.f; Ck.Items[It].bDirty = true; }
			Ck.Stage = 2;
		}
		return true;
	}
	return bReady;   // (no such gate: nothing to set)
}

bool ATugs::ChocksIn(float StandX) const
{
	for (const FTug& T : Tugs) if (FMath::Abs(T.StandX - StandX) < 1.f && T.Ck.Stage == 1)
		for (int32 p = 0; p < (T.Ck.bLong ? 3 : 2); ++p) if (T.Ck.Items[p].Where != CkOut || T.Ck.Items[p].Bl < 1.f) return false;
	return true;
}

void ATugs::ChocksGone(float StandX)
{
	// (the aircraft left without its pushback crew: somebody clears the stand)
	for (FTug& T : Tugs) if (FMath::Abs(T.StandX - StandX) < 1.f && T.Ck.Stage > 0)
	{
		bool bOut = T.Ck.Ci != INDEX_NONE; for (const FChkItem& I : T.Ck.Items) if (I.Where != CkRack) bOut = true;
		if (bOut) T.Ck.bLoose = true; else T.Ck.Stage = 0;
	}
}
