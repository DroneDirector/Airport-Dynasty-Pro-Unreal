// The passengers' own animation instance: the walk blend space plus a hand closed round a carried bag's handle, and the
// idle movements with their IK (PaxAnimInstance.h).
#include "PaxAnimInstance.h"
#include "ADPLog.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "ReferenceSkeleton.h"
#include "TwoBoneIK.h"

namespace
{
	/** Every bone's component-space transform in the reference pose. */
	void RefCS(const FReferenceSkeleton& RS, TArray<FTransform>& CS)
	{
		const TArray<FTransform>& L = RS.GetRefBonePose();
		CS.SetNum(L.Num());
		for (int32 I = 0; I < L.Num(); ++I) { const int32 Pa = RS.GetParentIndex(I); CS[I] = Pa == INDEX_NONE ? L[I] : L[I] * CS[Pa]; }
	}
	/** A hand's frame in the reference pose (Sd "l" / "r"): along the fingers, and the palm's side (the thumb's side). */
	bool HandFrame(const FReferenceSkeleton& RS, const TArray<FTransform>& CS, const TCHAR* Sd, FVector& Along, FVector& Palm)
	{
		auto B = [&](const TCHAR* N) { return RS.FindBoneIndex(FName(*FString::Printf(TEXT("%s_%s"), N, Sd))); };
		const int32 H = B(TEXT("hand")), Idx = B(TEXT("index_01")), Pin = B(TEXT("pinky_01")), Mid = B(TEXT("middle_01"));
		if (H == INDEX_NONE || Idx == INDEX_NONE || Pin == INDEX_NONE || Mid == INDEX_NONE) return false;
		const FVector HandAt = CS[H].GetLocation();
		Along = (CS[Mid].GetLocation() - HandAt).GetSafeNormal();
		const FVector Across = (CS[Idx].GetLocation() - CS[Pin].GetLocation()).GetSafeNormal();
		Palm = FVector::CrossProduct(Along, Across).GetSafeNormal();
		// (the same rule as the fire truck's rig, TruckDriverRig.cpp: the thumb sits on the palm's side of the hand)
		const int32 Th = B(TEXT("thumb_02"));
		if (Th != INDEX_NONE && FVector::DotProduct(CS[Th].GetLocation() - HandAt, Palm) < 0.f) Palm = -Palm;
		return !Along.IsNearlyZero() && !Palm.IsNearlyZero();
	}
	/** The compact pose's component-space rotation of a bone's parent (walked up from the root). */
	FQuat ParentRot(const FCompactPose& Pose, FCompactPoseBoneIndex I)
	{
		const FBoneContainer& BC = Pose.GetBoneContainer();
		FQuat Q = FQuat::Identity;
		for (FCompactPoseBoneIndex P = BC.GetParentBoneIndex(I); P != INDEX_NONE; P = BC.GetParentBoneIndex(P)) Q = Pose[P].GetRotation() * Q;
		return Q;
	}
	FTransform CSOf(const FCompactPose& Pose, FCompactPoseBoneIndex I)
	{
		const FBoneContainer& BC = Pose.GetBoneContainer();
		FTransform T = Pose[I];
		for (FCompactPoseBoneIndex P = BC.GetParentBoneIndex(I); P != INDEX_NONE; P = BC.GetParentBoneIndex(P)) T = T * Pose[P];
		return T;
	}
	/** Turn a bone (by a rotation given in component space) in its local transform. */
	void TurnCS(FCompactPose& Pose, FCompactPoseBoneIndex I, const FQuat& Delta)
	{
		const FQuat P = ParentRot(Pose, I);
		Pose[I].SetRotation((P.Inverse() * Delta * P * Pose[I].GetRotation()).GetNormalized());
	}
	/** In, held, out: a movement that reaches its pose by a fifth of the way and lets go over the last fifth. */
	float PaxRamp(float K) { return FMath::SmoothStep(0.f, 0.2f, K) * (1.f - FMath::SmoothStep(0.8f, 1.f, K)); }
	/** The finger curl rule (both hands): each joint closes towards the palm round its own axis, kept in the parent's space. */
	void PaxCurlsFor(const FReferenceSkeleton& RS, const TArray<FTransform>& CS, const TCHAR* Sd, const FVector& Palm, TArray<FVector>& Axes, TArray<int32>& Bones, TArray<float>& Angles)
	{
		// (the angles a hand needs to wrap round a ~3 cm bar: the knuckle and the middle joint well bent, the tip a little
		//  less; the thumb over the fingers)
		static const float Deg[2][3] = {{60.f, 75.f, 45.f}, {15.f, 22.f, 28.f}};
		for (const TCHAR* F : {TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky"), TEXT("thumb")})
			for (int32 J = 1; J <= 3; ++J)
			{
				const int32 Bn = RS.FindBoneIndex(FName(*FString::Printf(TEXT("%s_%02d_%s"), F, J, Sd)));
				if (Bn == INDEX_NONE || RS.GetParentIndex(Bn) == INDEX_NONE) continue;
				const int32 Pa = RS.GetParentIndex(Bn);
				const int32 Next = J < 3 ? RS.FindBoneIndex(FName(*FString::Printf(TEXT("%s_%02d_%s"), F, J + 1, Sd))) : INDEX_NONE;
				const FVector Dir = Next != INDEX_NONE ? (CS[Next].GetLocation() - CS[Bn].GetLocation()).GetSafeNormal() : (CS[Bn].GetLocation() - CS[Pa].GetLocation()).GetSafeNormal();
				const FVector Axis = FVector::CrossProduct(Dir, Palm).GetSafeNormal();
				if (Axis.IsNearlyZero()) continue;
				const bool bThumb = F[0] == TEXT('t');
				Bones.Add(Bn); Axes.Add(CS[Pa].GetRotation().UnrotateVector(Axis)); Angles.Add(FMath::DegreesToRadians(Deg[bThumb ? 1 : 0][J - 1]));
			}
	}
}

void FPaxAnimProxy::PreUpdate(UAnimInstance* In, float Dt)
{
	FAnimSingleNodeInstanceProxy::PreUpdate(In, Dt);
	if (const UPaxAnimInstance* A = Cast<UPaxAnimInstance>(In))
	{
		// (in and out over a quarter of a second)
		Grip = FMath::FInterpConstantTo(Grip, A->bGripRight ? 1.f : 0.f, Dt, 4.f);
		ArmOut = A->ArmOut;
		// the idle movement: its time from the world clock; a new one only once the last has blended out
		const UWorld* W = In->GetWorld();
		const double Now = W ? W->GetTimeSeconds() : 0.0;
		const bool bRun = A->bFidOn && A->FidKind >= 0 && A->FidDur > 0.f && Now < A->FidStart + A->FidDur;
		// (a device held for as long as Device says - the props, PaxProps: the phone / tablet pose held at its middle)
		const int32 DevK = A->Device == 1 ? int32(EPaxFidget::Phone) : A->Device == 2 ? int32(EPaxFidget::Tablet) : int32(EPaxFidget::None);
		if (!bRun && DevK >= 0 && (FidKind == DevK || FidFade <= 0.001f))
		{
			FidKind = DevK; FidDur = 4.f; FidSide = 1.f; bFidSeated = false; FidKt = 0.5f;
			FidFade = FMath::FInterpConstantTo(FidFade, 1.f, Dt, 3.f);
		}
		else if (bRun && (FidKind == A->FidKind || FidFade <= 0.001f))
		{
			FidKind = A->FidKind; FidDur = A->FidDur; FidSide = A->FidSide; bFidSeated = A->bFidSeated;
			FidKt = FMath::Clamp(float((Now - A->FidStart) / A->FidDur), 0.f, 1.f);
			FidFade = FMath::FInterpConstantTo(FidFade, 1.f, Dt, 3.f);
		}
		else
		{
			FidFade = FMath::FInterpConstantTo(FidFade, 0.f, Dt, 3.f);
			if (FidFade <= 0.001f && !bRun) FidKind = EPaxFidget::None;
		}
	}
}

void FPaxAnimProxy::PostEvaluate(UAnimInstance* In)
{
	FAnimSingleNodeInstanceProxy::PostEvaluate(In);
	// (game thread: the device's frame for the props, PaxProps)
	if (UPaxAnimInstance* A = Cast<UPaxAnimInstance>(In)) { A->DeviceCS = DeviceCS; A->DeviceW = DeviceW; A->DeviceKind = DeviceW > 0.f ? FidKind : int32(EPaxFidget::None); }
}

void FPaxAnimProxy::Rig(const FBoneContainer& BC)
{
	const FReferenceSkeleton& RS = BC.GetReferenceSkeleton();
	RigFor = &RS; Curl.Reset(); CurlL.Reset(); Upper = Lower = INDEX_NONE; Out = FVector::ZeroVector; bFidRig = false;
	TArray<FTransform> CS; RefCS(RS, CS);
	Upper = RS.FindBoneIndex(TEXT("upperarm_r")); Lower = RS.FindBoneIndex(TEXT("lowerarm_r"));
	const int32 UpperL = RS.FindBoneIndex(TEXT("upperarm_l"));
	if (Upper != INDEX_NONE && UpperL != INDEX_NONE) { const FVector D = CS[Upper].GetLocation() - CS[UpperL].GetLocation(); Out = FVector(D.X, D.Y, 0.f).GetSafeNormal(); }
	// the fingers of both hands (the right one: the bag's grip; the left one too for the idle movements)
	const TCHAR* Sd[2] = {TEXT("l"), TEXT("r")};
	for (int32 S = 0; S < 2; ++S)
	{
		FVector Along, Palm;
		PalmL[S] = AlongL[S] = FVector::ZeroVector;
		if (!HandFrame(RS, CS, Sd[S], Along, Palm)) continue;
		const int32 H = RS.FindBoneIndex(FName(*FString::Printf(TEXT("hand_%s"), Sd[S])));
		PalmL[S] = CS[H].GetRotation().UnrotateVector(Palm); AlongL[S] = CS[H].GetRotation().UnrotateVector(Along);
		TArray<FVector> Ax; TArray<int32> Bn; TArray<float> An;
		PaxCurlsFor(RS, CS, Sd[S], Palm, Ax, Bn, An);
		TArray<FCurl>& Into = S == 0 ? CurlL : Curl;
		for (int32 I = 0; I < Bn.Num(); ++I) Into.Add({Bn[I], Ax[I], An[I]});
	}
	// the idle movements' bones and the body's own frame (its left, front and up in component space)
	auto B = [&](const TCHAR* N) { return RS.FindBoneIndex(FName(N)); };
	Pelvis = B(TEXT("pelvis")); Head = B(TEXT("head"));
	Spine.Reset(); Neck.Reset();
	for (int32 I = 1; I <= 5; ++I) { const int32 X = RS.FindBoneIndex(FName(*FString::Printf(TEXT("spine_%02d"), I))); if (X != INDEX_NONE) Spine.Add(X); }
	for (int32 I = 1; I <= 2; ++I) { const int32 X = RS.FindBoneIndex(FName(*FString::Printf(TEXT("neck_%02d"), I))); if (X != INDEX_NONE) Neck.Add(X); }
	if (Head != INDEX_NONE) Neck.Add(Head);
	bool bAll = Pelvis != INDEX_NONE && Head != INDEX_NONE && Spine.Num() > 0;
	for (int32 S = 0; S < 2; ++S)
	{
		auto Bs = [&](const TCHAR* N) { return RS.FindBoneIndex(FName(*FString::Printf(TEXT("%s_%s"), N, Sd[S]))); };
		Arm[S][0] = Bs(TEXT("upperarm")); Arm[S][1] = Bs(TEXT("lowerarm")); Arm[S][2] = Bs(TEXT("hand"));
		Leg[S][0] = Bs(TEXT("thigh")); Leg[S][1] = Bs(TEXT("calf")); Leg[S][2] = Bs(TEXT("foot"));
		Clav[S] = Bs(TEXT("clavicle"));
		for (int32 K = 0; K < 3; ++K) bAll &= Arm[S][K] != INDEX_NONE && Leg[S][K] != INDEX_NONE;
	}
	if (!bAll) { ADP_LOG(TEXT("PaxAnim: the skeleton lacks the bones for the idle movements - none on this body")); return; }
	BU = (CS[Head].GetLocation() - CS[Pelvis].GetLocation()).GetSafeNormal();
	FVector L = CS[Arm[0][0]].GetLocation() - CS[Arm[1][0]].GetLocation();
	L = (L - FVector::DotProduct(L, BU) * BU).GetSafeNormal();
	if (BU.IsNearlyZero() || L.IsNearlyZero()) return;
	BL = L; BF = FVector::CrossProduct(BU, BL).GetSafeNormal();
	// (the front is where the toes point; without a ball bone, the reference pose faces +y)
	const int32 Ball = B(TEXT("ball_l"));
	const FVector Toe = Ball != INDEX_NONE ? CS[Ball].GetLocation() - CS[Leg[0][2]].GetLocation() : FVector(0, 1, 0);
	if (FVector::DotProduct(Toe, BF) < 0.f) BF = -BF;
	bFidRig = true;
}

void FPaxAnimProxy::Fidget(FCompactPose& Pose)
{
	DeviceW = 0.f;
	const float W = PaxRamp(FidKt) * FMath::SmoothStep(0.f, 1.f, FidFade);
	if (!bFidRig || FidKind < 0 || W <= 0.001f) return;
	const FBoneContainer& BC = Pose.GetBoneContainer();
	const int32 N = Pose.GetNumBones();
	// the component-space pose (parents always come before their children), kept up to date as bones are set
	TArray<FTransform> CS; CS.SetNumUninitialized(N);
	TArray<int32> Par; Par.SetNumUninitialized(N);
	for (int32 I = 0; I < N; ++I) { const FCompactPoseBoneIndex P = BC.GetParentBoneIndex(FCompactPoseBoneIndex(I)); Par[I] = P.GetInt(); }
	auto Build = [&](int32 From) { for (int32 I = From; I < N; ++I) CS[I] = Par[I] < 0 ? Pose[FCompactPoseBoneIndex(I)] : Pose[FCompactPoseBoneIndex(I)] * CS[Par[I]]; };
	Build(0);
	auto Ix = [&](int32 Mesh) -> int32 { if (Mesh == INDEX_NONE) return INDEX_NONE; const FCompactPoseBoneIndex C = BC.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh)); return C.GetInt(); };
	auto SetCS = [&](int32 I, const FTransform& T)
	{
		FTransform Lo = Par[I] < 0 ? T : T.GetRelativeTransform(CS[Par[I]]);
		Lo.SetScale3D(Pose[FCompactPoseBoneIndex(I)].GetScale3D()); Lo.NormalizeRotation();
		Pose[FCompactPoseBoneIndex(I)] = Lo; Build(I);
	};
	auto Turn = [&](int32 I, const FQuat& Q) { if (I < 0 || Q.Equals(FQuat::Identity, 1e-6f)) return; SetCS(I, FTransform(Q * CS[I].GetRotation(), CS[I].GetLocation(), CS[I].GetScale3D())); };
	auto Loc = [&](int32 I) { return CS[I].GetLocation(); };
	auto TwoBone = [&](int32 A, int32 Bb, int32 C, const FVector& Eff, const FVector& JointTgt)
	{
		FVector OJ, OE;
		AnimationCore::SolveTwoBoneIK(Loc(A), Loc(Bb), Loc(C), JointTgt, Eff, OJ, OE, false, 1.0, 1.0);
		Turn(A, FQuat::FindBetweenVectors(Loc(Bb) - Loc(A), OJ - Loc(A)));
		Turn(Bb, FQuat::FindBetweenVectors(Loc(C) - Loc(Bb), OE - Loc(Bb)));
	};
	const int32 Pv = Ix(Pelvis), Hd = Ix(Head);
	int32 A3[2][3], L3[2][3], Cl[2];
	for (int32 S = 0; S < 2; ++S) { for (int32 K = 0; K < 3; ++K) { A3[S][K] = Ix(Arm[S][K]); L3[S][K] = Ix(Leg[S][K]); } Cl[S] = Ix(Clav[S]); }
	if (Pv < 0 || Hd < 0) return;
	for (int32 S = 0; S < 2; ++S) for (int32 K = 0; K < 3; ++K) if (A3[S][K] < 0 || L3[S][K] < 0) return;   // (a low LOD without them)
	TArray<int32, TInlineAllocator<8>> Sp, Nk;
	for (const int32 X : Spine) { const int32 C = Ix(X); if (C >= 0) Sp.Add(C); }
	for (const int32 X : Neck) { const int32 C = Ix(X); if (C >= 0) Nk.Add(C); }
	const bool bBag = Grip > 0.001f;           // (a bag in the right hand: the right arm is the bag's)
	const float Sd = FidSide >= 0.f ? 1.f : -1.f, Ts = FidKt * FidDur;
	auto OutOf = [&](int32 S) { return S == 0 ? BL : -BL; };

	// 1. the weight from one foot to the other: the pelvis moves, both feet stay exactly where they stood (two-bone IK)
	if (FidKind == EPaxFidget::WeightShift && !bFidSeated)
	{
		const float Wv = FMath::Sin(2.f * PI * FidKt) * Sd;
		const FTransform Feet[2] = {CS[L3[0][2]], CS[L3[1][2]]};
		SetCS(Pv, FTransform(CS[Pv].GetRotation(), Loc(Pv) + (BL * (6.f * Wv) - BU * (1.5f * FMath::Abs(Wv))) * W, CS[Pv].GetScale3D()));
		for (int32 S = 0; S < 2; ++S)
		{
			TwoBone(L3[S][0], L3[S][1], L3[S][2], Feet[S].GetLocation(), Loc(L3[S][1]) + BF * 30.f);
			SetCS(L3[S][2], FTransform(Feet[S].GetRotation(), Loc(L3[S][2]), CS[L3[S][2]].GetScale3D()));
		}
	}

	// 2. what the hands are to do (places on the body's own bones as it stands now), and the look
	bool bHand[2] = {false, false}; FVector Tgt[2], Pole[2], Palm[2], Fing[2]; float CurlW[2] = {0.f, 0.f};
	float HY = 0.f, HP = 0.f, HR = 0.f, Tw = 0.f, ShUp[2] = {0.f, 0.f}, ShFw[2] = {0.f, 0.f};
	bool bLook = false; FVector LookAt = FVector::ZeroVector;
	FVector DevC = FVector::ZeroVector; FQuat DevQ = FQuat::Identity; bool bDev = false;
	const int32 Chest = Sp.Num() ? Sp.Last() : Pv;
	auto Hand = [&](int32 S, const FVector& T, const FVector& Po, const FVector& Pa, const FVector& Fi, float Cu)
	{
		if (S == 1 && bBag) return;   // (never the bag's hand)
		bHand[S] = true; Tgt[S] = T; Pole[S] = Po.GetSafeNormal(); Palm[S] = Pa.GetSafeNormal(); Fing[S] = Fi.GetSafeNormal(); CurlW[S] = Cu;
	};
	switch (FidKind)
	{
	case EPaxFidget::Stretch:
		for (int32 S = 0; S < 2; ++S) { Hand(S, Loc(Hd) + BU * 30.f + OutOf(S) * 6.f + BF * 3.f, OutOf(S) - BF * 0.2f, BU, -OutOf(S), 0.35f); ShUp[S] = 14.f; }
		HP = -12.f; break;
	case EPaxFidget::Watch:
		Hand(0, Loc(Chest) + BF * 28.f - BU * 14.f + BL * 1.f, OutOf(0) - BF * 0.2f - BU * 0.6f, -BU, -BL + BF * 0.3f, 0.3f);
		bLook = true; LookAt = Loc(Chest) + BF * 28.f - BU * 14.f; break;
	case EPaxFidget::LookAround:
		HY = 50.f * Sd * FMath::Sin(2.f * PI * FidKt); Tw = 6.f * Sd * FMath::Sin(2.f * PI * FidKt); HP = -3.f; break;
	case EPaxFidget::WeightShift:
		HR = -2.f * Sd * FMath::Sin(2.f * PI * FidKt); break;
	case EPaxFidget::NeckRoll:
	{
		const float Th = 2.f * PI * 1.25f * FidKt * Sd;
		HR = 20.f * FMath::Sin(Th); HP = 6.f + 10.f * FMath::Cos(Th); break;
	}
	case EPaxFidget::ArmsCrossed:
		for (int32 S = 0; S < 2; ++S) Hand(S, Loc(A3[1 - S][0]) + BF * (S == 0 ? 9.f : 5.f) - BU * 15.f + OutOf(S) * 9.f, OutOf(S) + BF * 0.5f - BU * 0.8f, -OutOf(S) * 0.8f - BF * 0.6f, -OutOf(S) - BF * 0.3f, 0.35f);
		ShFw[0] = ShFw[1] = 4.f; break;
	case EPaxFidget::Pockets:
		for (int32 S = 0; S < 2; ++S) Hand(S, Loc(L3[S][0]) + OutOf(S) * 6.f + BF * 8.f - BU * 12.f, OutOf(S) * 0.6f - BF * 0.6f - BU * 0.3f, -OutOf(S), -BU + BF * 0.3f, 0.5f);
		break;
	case EPaxFidget::HandOnHip:
		Hand(0, Loc(L3[0][0]) + OutOf(0) * 11.f - BF * 6.f + BU * 15.f, OutOf(0) - BF * 0.6f, -OutOf(0), BF * 0.6f - BU * 0.8f, 0.25f);
		HY = 8.f * Sd; break;
	case EPaxFidget::RubNeck:
	{
		const int32 Nb = Nk.Num() > 1 ? Nk[0] : Hd;
		Hand(0, Loc(Nb) + OutOf(0) * 6.f - BF * 9.f - BU * (7.f - 1.5f * FMath::Sin(Ts * 9.f)), OutOf(0) + BF * 0.1f + BU * 0.4f, BF - OutOf(0) * 0.3f, -OutOf(0) * 0.6f + BU * 0.8f, 0.25f);
		HP = 16.f; HR = -5.f; break;
	}
	case EPaxFidget::Phone:
	case EPaxFidget::Tablet:
	{
		// the device at chest height in front, tilted to the eyes; the hands hold its sides from below, the head looks at it
		const bool bTab = FidKind == EPaxFidget::Tablet;
		DevC = Loc(Chest) + BF * (bTab ? 32.f : 30.f) - BU * (bTab ? 18.f : 16.f);
		const FVector Eye = Loc(Hd) + BF * 10.f + BU * 6.f;
		const FVector Z = (Eye - DevC).GetSafeNormal();
		const FVector X = (BF - FVector::DotProduct(BF, Z) * Z).GetSafeNormal();
		DevQ = FRotationMatrix::MakeFromXZ(X, Z).ToQuat();
		const FVector Ls = (BL - FVector::DotProduct(BL, Z) * Z - FVector::DotProduct(BL, X) * X).GetSafeNormal();
		const float HalfW = bTab ? 12.5f : 3.5f;
		for (int32 S = 0; S < 2; ++S)
		{
			const FVector O = S == 0 ? Ls : -Ls;
			Hand(S, DevC + O * (HalfW + 3.f) - X * (bTab ? 3.f : 4.5f) - Z * 3.f, OutOf(S) - BU * 0.8f, Z * 0.7f - O * 0.7f, X * 0.7f - O * 0.6f, 0.45f);
		}
		bLook = true; LookAt = DevC; bDev = !bBag; break;
	}
	default: break;
	}

	// 3. the look and the head, the spine's share and the shoulders - before the arms, so they reach from where the
	//    shoulders now are. FQuat(Cross(A, B), +a) turns A towards B; every angle within natural limits
	if (bLook)
	{
		const FVector D = (LookAt - (Loc(Hd) + BF * 10.f + BU * 6.f)).GetSafeNormal();
		HY += FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(D, BL), FVector::DotProduct(D, BF)));
		HP += FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-FVector::DotProduct(D, BU), -1.f, 1.f)));
	}
	HY = FMath::Clamp(HY, -75.f, 75.f) * W; HP = FMath::Clamp(HP, -35.f, 50.f) * W; HR = FMath::Clamp(HR, -30.f, 30.f) * W;
	Tw = FMath::Clamp(Tw * W + HY * 0.2f, -40.f, 40.f);
	const FVector YawAx = FVector::CrossProduct(BF, BL).GetSafeNormal();
	if (Sp.Num() && FMath::Abs(Tw) > 0.01f) { const FQuat Q(YawAx, FMath::DegreesToRadians(Tw) / Sp.Num()); for (const int32 X : Sp) Turn(X, Q); }
	for (int32 S = 0; S < 2; ++S)
		if (Cl[S] >= 0 && (ShUp[S] != 0.f || ShFw[S] != 0.f) && !(S == 1 && bBag))
		{
			const FVector O = (Loc(A3[S][0]) - Loc(Cl[S])).GetSafeNormal();
			FQuat Q = FQuat::Identity;
			const FVector AxU = FVector::CrossProduct(O, BU).GetSafeNormal(), AxF = FVector::CrossProduct(O, BF).GetSafeNormal();
			if (!AxU.IsNearlyZero()) Q = FQuat(AxU, FMath::DegreesToRadians(FMath::Clamp(ShUp[S], -8.f, 18.f)) * W) * Q;
			if (!AxF.IsNearlyZero()) Q = FQuat(AxF, FMath::DegreesToRadians(FMath::Clamp(ShFw[S], -14.f, 14.f)) * W) * Q;
			Turn(Cl[S], Q);
		}
	if (Nk.Num() && (FMath::Abs(HY - Tw) > 0.01f || FMath::Abs(HP) > 0.01f || FMath::Abs(HR) > 0.01f))
	{
		const float Nn = float(Nk.Num());
		const FQuat SpQ(YawAx, FMath::DegreesToRadians(Tw));
		const FVector HUp = SpQ.RotateVector(BU), HFw = SpQ.RotateVector(BF), HLeft = SpQ.RotateVector(BL);
		const FQuat Q = FQuat(YawAx, FMath::DegreesToRadians(HY - Tw) / Nn)
			* FQuat(FVector::CrossProduct(HFw, -HUp).GetSafeNormal(), FMath::DegreesToRadians(HP) / Nn)     // (the face towards down)
			* FQuat(FVector::CrossProduct(HUp, HLeft).GetSafeNormal(), FMath::DegreesToRadians(HR) / Nn);   // (the crown towards the left)
		for (const int32 X : Nk) Turn(X, Q);
	}

	// 4. the arms: two-bone IK to the targets (the elbow towards its pole), the palm turned onto what it touches (the
	//    wrist at most 70 degrees), the fingers lightly curled
	FVector WristNow[2], WristWant[2];
	for (int32 S = 0; S < 2; ++S)
	{
		if (!bHand[S]) continue;
		const int32 U = A3[S][0], Lw = A3[S][1], H = A3[S][2];
		WristWant[S] = Tgt[S];
		const FVector Eff = FMath::Lerp(Loc(H), Tgt[S], W);
		const FVector JT = FMath::Lerp(Loc(Lw), (Loc(U) + Eff) * 0.5f + Pole[S] * 40.f, W);
		TwoBone(U, Lw, H, Eff, JT);
		if (!PalmL[S].IsNearlyZero() && !Palm[S].IsNearlyZero())
		{
			const FQuat HQ = CS[H].GetRotation();
			const FVector NowP = HQ.RotateVector(PalmL[S]), NowA = HQ.RotateVector(AlongL[S]);
			FQuat Q = FQuat::FindBetweenNormals(NowP, Palm[S]);
			FVector WantA = Fing[S] - FVector::DotProduct(Fing[S], Palm[S]) * Palm[S];
			if (WantA.Normalize())
			{
				const FQuat Q0 = FRotationMatrix::MakeFromXZ(NowA, NowP).ToQuat(), Q1 = FRotationMatrix::MakeFromXZ(WantA, Palm[S]).ToQuat();
				Q = Q1 * Q0.Inverse();
			}
			FVector Ax; float Ang; Q.ToAxisAndAngle(Ax, Ang);
			if (Ang > PI) { Ang = 2.f * PI - Ang; Ax = -Ax; }
			Turn(H, FQuat(Ax, FMath::Min(Ang, FMath::DegreesToRadians(70.f)) * W));
		}
		WristNow[S] = Loc(H);
		// (the fingers: a light curl, resting on the surface)
		const TArray<FCurl>& Cu = S == 0 ? CurlL : Curl;
		for (const FCurl& C : Cu)
		{
			const int32 I = Ix(C.Bone);
			if (I < 0) continue;
			FTransform& Tr = Pose[FCompactPoseBoneIndex(I)];
			Tr.SetRotation((FQuat(C.Axis, C.Angle * CurlW[S] * W) * Tr.GetRotation()).GetNormalized());
		}
	}

	// 5. the device, where the hands really are (it rides with them if the IK fell a little short)
	if (bDev && bHand[0] && bHand[1])
	{
		const FVector Shift = (WristNow[0] + WristNow[1]) * 0.5f - (WristWant[0] + WristWant[1]) * 0.5f;
		DeviceCS = FTransform(DevQ, DevC + Shift); DeviceW = W;
	}
}

bool FPaxAnimProxy::Evaluate(FPoseContext& Output)
{
	const bool bOk = FAnimSingleNodeInstanceProxy::Evaluate(Output);
	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& BC = Pose.GetBoneContainer();
	if (RigFor != &BC.GetReferenceSkeleton()) Rig(BC);
	// the idle movement first (layered on the idle pose), then the bag's grip
	if (FidKind >= 0) Fidget(Pose); else DeviceW = 0.f;
	if (Grip <= 0.001f) return bOk;
	const float W = FMath::SmoothStep(0.f, 1.f, Grip);
	auto Ix = [&](int32 Mesh) { return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : BC.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh)); };
	// the arm a little out from the body (the case clear of the thigh), the forearm a touch further
	const FCompactPoseBoneIndex U = Ix(Upper), L = Ix(Lower);
	if (U != INDEX_NONE && L != INDEX_NONE && !Out.IsNearlyZero())
	{
		const FTransform UC = CSOf(Pose, U);
		const FVector Dir = (UC.TransformPosition(Pose[L].GetTranslation()) - UC.GetLocation()).GetSafeNormal();   // (the upper arm's way, shoulder to elbow)
		const FVector Ax = FVector::CrossProduct(Dir, Out).GetSafeNormal();   // (turning Dir towards Out)
		if (!Ax.IsNearlyZero())
		{
			TurnCS(Pose, U, FQuat(Ax, FMath::DegreesToRadians(ArmOut) * W));
			TurnCS(Pose, L, FQuat(Ax, FMath::DegreesToRadians(ArmOut * 0.3f) * W));
		}
	}
	// the fingers and thumb round the handle
	for (const FCurl& C : Curl)
	{
		const FCompactPoseBoneIndex I = Ix(C.Bone);
		if (I == INDEX_NONE) continue;
		Pose[I].SetRotation((FQuat(C.Axis, C.Angle * W) * Pose[I].GetRotation()).GetNormalized());
	}
	return bOk;
}

// ------------------------------------------------------------------ the idle movements (game thread)

void UPaxAnimInstance::PlayFidget(int32 Kind, float Seconds, float Side)
{
	if (Kind < 0 || Kind >= EPaxFidget::Num) return;
	const UWorld* W = GetWorld();
	FidKind = Kind; FidDur = FMath::Clamp(Seconds, 1.f, 10.f); FidSide = Side >= 0.f ? 1.f : -1.f;
	FidStart = W ? W->GetTimeSeconds() : 0.0; bFidOn = true; LastFid = Kind;
}

void UPaxAnimInstance::StopFidget() { bFidOn = false; }

bool UPaxAnimInstance::IsFidgeting() const
{
	const UWorld* W = GetWorld();
	return bFidOn && FidKind >= 0 && W && W->GetTimeSeconds() < FidStart + FidDur;
}

bool UPaxAnimInstance::FidgetUsesRightArm(int32 K)
{
	return K == EPaxFidget::Stretch || K == EPaxFidget::ArmsCrossed || K == EPaxFidget::Phone || K == EPaxFidget::Tablet;
}

bool UPaxAnimInstance::FidgetSeatedOk(int32 K)
{
	return K == EPaxFidget::LookAround || K == EPaxFidget::Phone || K == EPaxFidget::NeckRoll;
}

int32 UPaxAnimInstance::PickFidget(bool bSeated, bool bRightHandBusy) const
{
	TArray<int32, TInlineAllocator<16>> Can;
	for (int32 K = 0; K < EPaxFidget::Num; ++K)
	{
		if (K == LastFid) continue;                                       // (never the same twice running)
		if (bSeated && !FidgetSeatedOk(K)) continue;                      // (seated: only the subtle ones)
		if (bRightHandBusy && FidgetUsesRightArm(K)) continue;            // (a bag in the right hand)
		if (K == EPaxFidget::Tablet && FMath::FRand() < 0.7f) continue;   // (fewer tablets than phones)
		Can.Add(K);
		if (K == EPaxFidget::Phone || K == EPaxFidget::LookAround) Can.Add(K);   // (the commonest)
	}
	return Can.Num() ? Can[FMath::RandRange(0, Can.Num() - 1)] : int32(EPaxFidget::None);
}

void UPaxAnimInstance::IdleFidgets(float Dt, bool bStill, bool bSeated, bool bRightHandBusy)
{
	if (Device != 0) { if (bFidOn) StopFidget(); return; }   // (a device in the hands: that is what they are doing)
	if (!bStill) { StillT = 0.f; NextFidT = FMath::FRandRange(0.f, 8.f); if (bFidOn) StopFidget(); return; }
	StillT += Dt;
	if (StillT < 30.f) return;   // (only after half a minute of standing / sitting still)
	if (IsFidgeting())
	{
		if ((bRightHandBusy && FidgetUsesRightArm(FidKind)) || (bSeated && !FidgetSeatedOk(FidKind))) StopFidget();
		return;
	}
	bFidOn = false;
	if ((NextFidT -= Dt) > 0.f) return;
	bFidSeated = bSeated;
	const int32 K = PickFidget(bSeated, bRightHandBusy);
	if (K < 0) { NextFidT = 5.f; return; }
	static const float Lo[EPaxFidget::Num] = {2.8f, 2.2f, 3.5f, 4.f, 4.f, 4.f, 4.f, 4.f, 3.5f, 3.f, 4.f};
	static const float Hi[EPaxFidget::Num] = {3.6f, 3.2f, 5.f, 6.f, 5.5f, 6.f, 6.f, 6.f, 5.5f, 4.f, 6.f};
	const float Dur = FMath::FRandRange(Lo[K], Hi[K]);
	PlayFidget(K, Dur, FMath::RandBool() ? 1.f : -1.f);
	NextFidT = Dur + FMath::FRandRange(6.f, 20.f);   // (plain idle between)
}

bool UPaxAnimInstance::GetDeviceGrip(FTransform& OutWorld, float& OutWeight) const
{
	OutWeight = 0.f;
	if ((DeviceKind != EPaxFidget::Phone && DeviceKind != EPaxFidget::Tablet) || DeviceW <= 0.f) return false;
	const USkeletalMeshComponent* B = GetSkelMeshComponent();
	if (!B) return false;
	OutWorld = DeviceCS * B->GetComponentTransform(); OutWeight = DeviceW;
	return true;
}

bool UPaxAnimInstance::HandAxes(const USkeletalMeshComponent* Body, bool bRight, FVector& OutAlong, FVector& OutPalm)
{
	OutAlong = OutPalm = FVector::ZeroVector;
	if (!Body || !Body->GetSkinnedAsset()) return false;
	const FReferenceSkeleton& RS = Body->GetSkinnedAsset()->GetRefSkeleton();
	TArray<FTransform> CS; RefCS(RS, CS);
	FVector Along, Palm;
	if (!HandFrame(RS, CS, bRight ? TEXT("r") : TEXT("l"), Along, Palm)) return false;
	const int32 H = RS.FindBoneIndex(bRight ? TEXT("hand_r") : TEXT("hand_l"));
	OutAlong = CS[H].GetRotation().UnrotateVector(Along); OutPalm = CS[H].GetRotation().UnrotateVector(Palm);
	return true;
}

// ------------------------------------------------------------------

UPaxAnimInstance* UPaxAnimInstance::Install(USkeletalMeshComponent* Body)
{
	if (!Body) return nullptr;
	if (UPaxAnimInstance* Have = Cast<UPaxAnimInstance>(Body->GetAnimInstance())) return Have;
	UAnimSingleNodeInstance* One = Body->GetSingleNodeInstance();
	UAnimationAsset* Asset = One ? One->GetAnimationAsset() : nullptr;
	if (!Asset) return nullptr;   // (not walking on the blend space - leave it be)
	Body->SetAnimInstanceClass(UPaxAnimInstance::StaticClass());
	UPaxAnimInstance* Mine = Cast<UPaxAnimInstance>(Body->GetAnimInstance());
	if (!Mine) return nullptr;
	Body->SetAnimation(Asset); Body->Play(true);
	return Mine;
}

FVector UPaxAnimInstance::GripInHand(const USkeletalMeshComponent* Body)
{
	if (!Body || !Body->GetSkinnedAsset()) return FVector::ZeroVector;
	const FReferenceSkeleton& RS = Body->GetSkinnedAsset()->GetRefSkeleton();
	TArray<FTransform> CS; RefCS(RS, CS);
	FVector Along, Palm;
	if (!HandFrame(RS, CS, TEXT("r"), Along, Palm)) return FVector::ZeroVector;
	const int32 H = RS.FindBoneIndex(TEXT("hand_r")), Mid = RS.FindBoneIndex(TEXT("middle_01_r"));
	// (just past the middle knuckle along the finger, and a bar's radius plus the finger's half thickness to the palm's
	//  side: the middle of a ~3 cm handle bar inside the closed fingers)
	const FVector G = CS[Mid].GetLocation() + Along * 2.f + Palm * 2.5f;
	return CS[H].InverseTransformPosition(G);
}
