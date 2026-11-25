// The passengers' own animation instance: the walk blend space plus a hand closed round a carried bag's handle, and the
// idle movements with their IK (PaxAnimInstance.h).
#include "PaxAnimInstance.h"
#include "ADPLog.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationAsset.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PerfLog.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
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
	if (UPaxAnimInstance* A = Cast<UPaxAnimInstance>(In))
	{
		A->StepFeet(Dt, *this);
		// (in and out over a quarter of a second)
		Grip = FMath::FInterpConstantTo(Grip, A->bGripRight ? 1.f : 0.f, Dt, 4.f);
		ArmOut = A->ArmOut;
		Twist = FMath::Clamp(A->Twist, -42.f, 42.f);
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
		Clav[S] = Bs(TEXT("clavicle")); Ball[S] = Bs(TEXT("ball"));
		for (int32 K = 0; K < 3; ++K) bAll &= Arm[S][K] != INDEX_NONE && Leg[S][K] != INDEX_NONE;
	}
	if (!bAll) { ADP_LOG(TEXT("PaxAnim: the skeleton lacks the bones for the idle movements - none on this body")); return; }
	BU = (CS[Head].GetLocation() - CS[Pelvis].GetLocation()).GetSafeNormal();
	FVector L = CS[Arm[0][0]].GetLocation() - CS[Arm[1][0]].GetLocation();
	L = (L - FVector::DotProduct(L, BU) * BU).GetSafeNormal();
	if (BU.IsNearlyZero() || L.IsNearlyZero()) return;
	BL = L; BF = FVector::CrossProduct(BU, BL).GetSafeNormal();
	// (the front is where the toes point; without a ball bone, the reference pose faces +y)
	const FVector Toe = Ball[0] != INDEX_NONE ? CS[Ball[0]].GetLocation() - CS[Leg[0][2]].GetLocation() : FVector(0, 1, 0);
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
	if (FMath::Abs(Twist) > 0.4f)
	{
		auto Cx = [&](int32 Mesh) { return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : BC.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh)); };
		int32 N = 0; for (const int32 X : Spine) if (Cx(X) != INDEX_NONE) ++N;
		const FQuat Q(FVector::UpVector, FMath::DegreesToRadians(Twist) / FMath::Max(1, N));
		for (const int32 X : Spine) { const FCompactPoseBoneIndex I = Cx(X); if (I != INDEX_NONE) TurnCS(Pose, I, Q); }
		int32 Nn = 0; for (const int32 X : Neck) if (Cx(X) != INDEX_NONE) ++Nn;
		const FQuat Qn(FVector::UpVector, -FMath::DegreesToRadians(Twist * 0.75f) / FMath::Max(1, Nn + 1));
		for (const int32 X : Neck) { const FCompactPoseBoneIndex I = Cx(X); if (I != INDEX_NONE) TurnCS(Pose, I, Qn); }
		if (const FCompactPoseBoneIndex I = Cx(Head); I != INDEX_NONE) TurnCS(Pose, I, Qn);
		// (the arms in towards the body: the one with a bag keeps its grip, held less far out)
		ArmOut *= 1.f - 0.7f * FMath::Abs(Twist) / 42.f;
	}
	if (FeetW > 0.001f && Soles.bOk && bFidRig) Feet(Pose);   // (last: the legs as they are by now)
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

// ------------------------------------------------------------------ the feet on the ground (to-do 290)

namespace { constexpr float PaxSoleGap = 0.25f; }   // (cm the sole stays over the ground: not noticed, never below)

void FPaxAnimProxy::Feet(FCompactPose& Pose) const
{
	const FBoneContainer& BC = Pose.GetBoneContainer();
	auto Ix = [&](int32 Mesh) { return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : BC.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh)); };
	const FCompactPoseBoneIndex Pv = Ix(Pelvis); if (Pv == INDEX_NONE) return;
	FCompactPoseBoneIndex Lg[2][4] = {{Pv, Pv, Pv, Pv}, {Pv, Pv, Pv, Pv}};
	for (int32 S = 0; S < 2; ++S)
		for (int32 K = 0; K < 4; ++K) { Lg[S][K] = Ix(K < 3 ? Leg[S][K] : Ball[S]); if (Lg[S][K] == INDEX_NONE) return; }   // (a low LOD without them)
	auto Rel = [&](FCompactPoseBoneIndex I, const FTransform& T, const FTransform& Pa)
	{
		FTransform Lo = T.GetRelativeTransform(Pa); Lo.SetScale3D(Pose[I].GetScale3D()); Lo.NormalizeRotation(); Pose[I] = Lo;
	};
	const FTransform PvO = CSOf(Pose, Pv);
	const FTransform PvPar = Pose[Pv].Inverse() * PvO;
	const float W = FeetW, Up = -Soles.Sole + PaxSoleGap;
	FTransform Th[2], Ca[2], Fo[2]; FTransform BaLo[2];
	for (int32 S = 0; S < 2; ++S) { Th[S] = Pose[Lg[S][0]] * PvO; Ca[S] = Pose[Lg[S][1]] * Th[S]; Fo[S] = Pose[Lg[S][2]] * Ca[S]; BaLo[S] = Pose[Lg[S][3]]; }
	FTransform PvN = PvO; PvN.AddToTranslation(FVector(0.f, 0.f, (Up + HipRise) * W));
	Rel(Pv, PvN, PvPar);
	const FVector Hip(0.f, 0.f, (Up + HipRise) * W);
	for (int32 S = 0; S < 2; ++S)
	{
		FTransform F = Fo[S];
		auto Pt = [&](const FTransform& Foot, int32 I) { return I < 3 ? Foot.TransformPosition(Soles.At[S][I]) : (BaLo[S] * Foot).TransformPosition(Soles.At[S][I]); };
		// how near the ground the animation has it (its own floor: the body's zero, the sole's thickness below)
		float Low = MAX_flt; for (int32 I = 0; I < 6; ++I) Low = FMath::Min(Low, float(Pt(F, I).Z) - Soles.Sole);
		const float On = W * (1.f - FMath::SmoothStep(1.f, 8.f, Low));
		// on the ground: along its slope, and its sides level with it (never on one edge)
		const FVector N = Slope[S];
		FQuat Q = F.GetRotation();
		if (N.Z < 0.9999f) Q = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(FVector::UpVector, N), On) * Q;
		F.SetRotation(Q);
		const FVector Fw = (Pt(F, 5) - Pt(F, 0)).GetSafeNormal(), Sd = (Pt(F, 2) - Pt(F, 1)).GetSafeNormal();
		FVector Flat = FVector::VectorPlaneProject(Sd, N).GetSafeNormal();
		if (!Flat.IsNearlyZero() && !Fw.IsNearlyZero())
		{
			const float Roll = FMath::Atan2(float(FVector::DotProduct(FVector::CrossProduct(Sd, Flat), Fw)), float(FVector::DotProduct(Sd, Flat)));
			F.SetRotation((FQuat(Fw, FMath::Clamp(Roll, -0.35f, 0.35f) * On) * F.GetRotation()).GetNormalized());
		}
		F.AddToTranslation(FVector(0.f, 0.f, (Up + Rise[S]) * W));
		// never below: the lowest of the sole's points over the ground found under it
		float Need = 0.f;
		for (int32 I = 0; I < 6; ++I) Need = FMath::Max(Need, Under[S][I] + PaxSoleGap - float(Pt(F, I).Z));
		F.AddToTranslation(FVector(0.f, 0.f, FMath::Min(Need, 40.f) * W));
		// the leg to it (the knee stays the way it bends)
		const FVector H0 = Th[S].GetLocation() + Hip, K0 = Ca[S].GetLocation() + Hip, A0 = Fo[S].GetLocation() + Hip;
		FVector OJ, OE;
		AnimationCore::SolveTwoBoneIK(H0, K0, A0, K0 + BF * 25.f, F.GetLocation(), OJ, OE, false, 1.0, 1.0);
		const FQuat Qa = FQuat::FindBetweenVectors(K0 - H0, OJ - H0);
		FTransform ThN(Qa * Th[S].GetRotation(), H0, Th[S].GetScale3D());
		const FQuat Qb = FQuat::FindBetweenVectors(Qa.RotateVector(A0 - K0), OE - OJ);
		FTransform CaN(Qb * Qa * Ca[S].GetRotation(), OJ, Ca[S].GetScale3D());
		Rel(Lg[S][0], ThN, PvN); Rel(Lg[S][1], CaN, ThN); F.SetLocation(OE); Rel(Lg[S][2], F, CaN);
	}
}

void UPaxAnimInstance::MeasureSoles(const USkeletalMeshComponent* B)
{
	// the lowest points of a mesh in its reference pose: the soles as they are (the body's own feet, the shoes over them)
	struct FLow { float Z = 0.f; TArray<FVector3f> Pts; };
	static TMap<TWeakObjectPtr<const USkinnedAsset>, FLow> Known;
	auto LowOf = [](const USkinnedAsset* M) -> const FLow&
	{
		if (const FLow* K = Known.Find(M)) return *K;
		FLow& O = Known.Add(M);
		const FSkeletalMeshRenderData* R = M->GetResourceForRendering();
		const int32 N = R ? R->LODRenderData.Num() : 0;
		int32 From = R ? R->GetFirstValidLODIdx(0) : 0;
		for (int32 l = FMath::Max(0, From); l < N; ++l)
		{
			const FPositionVertexBuffer& V = R->LODRenderData[l].StaticVertexBuffers.PositionVertexBuffer;
			if (!V.GetNumVertices() || !V.GetVertexData()) continue;
			O.Z = MAX_flt;
			for (uint32 i = 0; i < V.GetNumVertices(); ++i) O.Z = FMath::Min(O.Z, V.VertexPosition(i).Z);
			if (O.Z < 12.f) for (uint32 i = 0; i < V.GetNumVertices(); ++i) if (const FVector3f& P = V.VertexPosition(i); P.Z < O.Z + 1.5f) O.Pts.Add(P);
			break;
		}
		if (!O.Pts.Num() && O.Z == 0.f) if (const USkeletalMesh* Sm = Cast<USkeletalMesh>(M)) { const FBoxSphereBounds Bb = Sm->GetImportedBounds(); O.Z = float(Bb.Origin.Z - Bb.BoxExtent.Z); }
		ADP_LOG(TEXT("PaxAnim: %s - lowest point %.2f cm, %d points of its soles"), *M->GetName(), O.Z, O.Pts.Num());
		return O;
	};
	Soles = FPaxSoles();
	const USkinnedAsset* Bm = B->GetSkinnedAsset(); if (!Bm) return;
	const FReferenceSkeleton& RS = Bm->GetRefSkeleton();
	TArray<FTransform> CS; RefCS(RS, CS);
	TArray<const FLow*> Lows;
	float Z = 0.f;
	if (const AActor* Own = B->GetOwner())
	{
		TInlineComponentArray<USkinnedMeshComponent*> Sk(Own);
		for (const USkinnedMeshComponent* S : Sk)
			if (S == B || S->LeaderPoseComponent.Get() == B)
				if (const USkinnedAsset* M = S->GetSkinnedAsset()) { LowOf(M); }
		for (const USkinnedMeshComponent* S : Sk)
			if (S == B || S->LeaderPoseComponent.Get() == B)
				if (const USkinnedAsset* M = S->GetSkinnedAsset()) if (const FLow& O = LowOf(M); O.Z < 12.f) { Z = FMath::Min(Z, FMath::Max(O.Z, -8.f)); Lows.Add(&O); }
	}
	Soles.Sole = Z;
	for (int32 S = 0; S < 2; ++S)
	{
		const int32 F = RS.FindBoneIndex(S ? TEXT("foot_r") : TEXT("foot_l")), Ba = RS.FindBoneIndex(S ? TEXT("ball_r") : TEXT("ball_l"));
		if (F == INDEX_NONE || Ba == INDEX_NONE) return;
		const FVector A = CS[F].GetLocation(), T = CS[Ba].GetLocation();
		const FVector Fw = FVector(T.X - A.X, T.Y - A.Y, 0.f).GetSafeNormal(), Sd = FVector::CrossProduct(FVector::UpVector, Fw);
		const float D = FVector::Dist2D(A, T);
		// along the foot and across it, from the ankle: a shoe's usual measures, then what its points say
		float Back = -0.45f * D, Tip = 1.6f * D, Hi = -0.25f * D, Ho = 0.25f * D, Bi = -0.36f * D, Bo = 0.36f * D;
		float Back2 = 0.f, Tip2 = 0.f, Hi2 = 0.f, Ho2 = 0.f, Bi2 = 0.f, Bo2 = 0.f;
		for (const FLow* Lo : Lows)
			for (const FVector3f& P : Lo->Pts)
			{
				if (P.Z > Z + 1.5f) continue;
				const FVector V = FVector(P) - A; const float Al = float(V.X * Fw.X + V.Y * Fw.Y), Ac = float(V.X * Sd.X + V.Y * Sd.Y);
				if (FMath::Abs(Ac) > 0.75f * D || Al < -1.f * D || Al > 2.3f * D) continue;   // (the other foot's)
				Back2 = FMath::Min(Back2, Al); Tip2 = FMath::Max(Tip2, Al);
				if (Al < 0.2f * D) { Hi2 = FMath::Min(Hi2, Ac); Ho2 = FMath::Max(Ho2, Ac); }
				else if (FMath::Abs(Al - D) < 0.35f * D) { Bi2 = FMath::Min(Bi2, Ac); Bo2 = FMath::Max(Bo2, Ac); }
			}
		if (Back2 < -2.f && Tip2 > D) { Back = Back2; Tip = Tip2; if (Ho2 - Hi2 > 2.f) { Hi = Hi2; Ho = Ho2; } if (Bo2 - Bi2 > 3.f) { Bi = Bi2; Bo = Bo2; } }
		auto Pt = [&](float Al, float Ac) { FVector P = A + Fw * Al + Sd * Ac; P.Z = Z; return P; };
		const FVector Pf[6] = {Pt(Back, 0.5f * (Hi + Ho)), Pt(0.f, Hi), Pt(0.f, Ho), Pt(D, Bi), Pt(D, Bo), Pt(Tip, 0.5f * (Bi + Bo))};
		for (int32 I = 0; I < 6; ++I) Soles.At[S][I] = (I < 3 ? CS[F] : CS[Ba]).InverseTransformPosition(Pf[I]);
		static int32 Said = 0;
		if (Said++ < 6) ADP_LOG(TEXT("PaxAnim: the soles of %s (%s foot): %.2f cm under the body's zero, heel %.1f, toe %.1f, heel width %.1f, ball width %.1f"), *Bm->GetName(), S ? TEXT("right") : TEXT("left"), -Z, Back, Tip, Ho - Hi, Bo - Bi);
	}
	Soles.bOk = true;
}

void UPaxAnimInstance::StepFeet(float Dt, FPaxAnimProxy& P)
{
	USkeletalMeshComponent* B = GetSkelMeshComponent();
	UWorld* W = B ? B->GetWorld() : nullptr;
	AActor* Own = B ? B->GetOwner() : nullptr;
	if (!W || !Own || !W->IsGameWorld()) { P.FeetW = 0.f; return; }
	static uint64 CamFrame = 0; static FVector Cam = FVector::ZeroVector;
	if (CamFrame != GFrameCounter) { CamFrame = GFrameCounter; if (const APlayerController* PC = W->GetFirstPlayerController()) if (PC->PlayerCameraManager) Cam = PC->PlayerCameraManager->GetCameraLocation(); }
	const FVector Base = B->GetComponentLocation();
	const bool bMe = Own->IsA<APawn>() || (Own->GetAttachParentActor() && Own->GetAttachParentActor()->IsA<APawn>());
	const float DistSq = float(FVector::DistSquared(Base, Cam));
	const bool bNear = bFeet && DistSq < FMath::Square(bFeetOn ? 2300.f : 2000.f) && (bMe || B->WasRecentlyRendered(0.3f));
	FeetW = FMath::FInterpConstantTo(FeetW, bNear ? 1.f : 0.f, Dt, 5.f);
	const FVector V = Dt > 1e-4f ? (Base - FeetAt) / Dt : FVector::ZeroVector; FeetAt = Base;
	if (FeetW <= 0.001f) { bFeetOn = false; P.FeetW = 0.f; return; }
	ADP_PERF_SCOPE("feet: the ground under them", 2.f);
	if ((SolesT -= Dt) <= 0.f)
	{
		SolesT = 2.f;
		int32 Sum = 0; TInlineComponentArray<USkinnedMeshComponent*> Sk(Own);
		for (const USkinnedMeshComponent* S : Sk) if (S == B || S->LeaderPoseComponent.Get() == B) Sum = HashCombine(Sum, GetTypeHash(S->GetSkinnedAsset()));
		if (Sum != SolesFor) { SolesFor = Sum; MeasureSoles(B); }
	}
	if (!Soles.bOk) { P.FeetW = 0.f; return; }
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AdpFeet), false, Own);
	for (AActor* Pa = Own->GetAttachParentActor(); Pa; Pa = Pa->GetAttachParentActor()) if (Pa->IsA<APawn>()) Q.AddIgnoredActor(Pa);
	const float Bz = float(Base.Z);
	auto Below = [&](const FVector& Pos, FVector* N) -> float
	{
		for (int32 k = 0; k < 2; ++k)
		{
			FHitResult Hi;
			if (!W->LineTraceSingleByChannel(Hi, FVector(Pos.X, Pos.Y, Bz + 42.f), FVector(Pos.X, Pos.Y, Bz - 45.f), ECC_Visibility, Q)) break;
			// (another person's leg, a bag, an edge: not the ground)
			if (Cast<USkinnedMeshComponent>(Hi.GetComponent())) { Q.AddIgnoredActor(Hi.GetActor()); continue; }
			if (Hi.bStartPenetrating || Hi.ImpactNormal.Z < 0.7f) break;
			if (N) *N = Hi.ImpactNormal;
			return float(Hi.ImpactPoint.Z) - Bz;
		}
		return 0.f;
	};
	auto Under = [&](const FVector& Pos, FVector* N) -> float
	{
		const float G = Below(Pos, N);
		// (higher than one step: only with a step before it - else a bag, a seat's edge, something standing there)
		if (G > 22.f && Below(FMath::Lerp(Base, Pos, 0.5f), nullptr) < 6.f) { if (N) *N = FVector::UpVector; return 0.f; }
		return G;
	};
	static const FName Foot[2] = {TEXT("foot_l"), TEXT("foot_r")}, BallN[2] = {TEXT("ball_l"), TEXT("ball_r")};
	const FTransform Xf = B->GetComponentTransform();
	const FTransform Was = bFeetOn ? FeetXf : Xf; FeetXf = Xf;
	float Grd[2][6] = {}, Hi6[2] = {0.f, 0.f}, Ahead[2] = {-100.f, -100.f}; FVector Nm[2] = {FVector::UpVector, FVector::UpVector}; FVector Pw6[2][6]; bool bEven = true;
	const FVector Go = FVector(V.X, V.Y, 0.f).GetClampedToMaxSize(300.f) * 0.14f;
	for (int32 S = 0; S < 2; ++S)
	{
		const FTransform Tf = B->GetSocketTransform(Foot[S], RTS_Component), Tb = B->GetSocketTransform(BallN[S], RTS_Component);
		// (the pose is the last one: where the foot is going to be in this one)
		const FVector Step = bFeetOn ? (Tf.GetLocation() - FootWas[S]).GetClampedToMaxSize(14.f) : FVector::ZeroVector; FootWas[S] = Tf.GetLocation();
		for (int32 I = 0; I < 6; ++I) Pw6[S][I] = Xf.TransformPosition((I < 3 ? Tf : Tb).TransformPosition(Soles.At[S][I]) + Step);
		const float Gb = Under(Xf.TransformPosition(Tb.GetLocation() + Step), &Nm[S]);
		Ahead[S] = Under(Pw6[S][5] + Go, nullptr);   // (the next step: seen, and lifted for, before they are at it)
		if (FMath::Abs(Gb) > 0.5f || FMath::Abs(Ahead[S]) > 1.f || Nm[S].Z < 0.9995f) bEven = false;
		for (int32 I = 0; I < 6; ++I) Grd[S][I] = Gb;
		Hi6[S] = Gb;
	}
	if (!bEven) UnevenT = 0.6f; else UnevenT -= Dt;
	if (UnevenT > 0.f)
	{
		float Least = 0.f;
		for (int32 S = 0; S < 2; ++S)
		{
			const FVector Ankle = Xf.TransformPosition(FootWas[S]), Tip = (Pw6[S][5] - Pw6[S][0]).GetSafeNormal2D();
			Hi6[S] = -100.f;
			for (int32 I = 0; I < 6; ++I)
			{
				const FVector& Pp = Pw6[S][I];
				Grd[S][I] = Under(Pp, nullptr);
				if (I == 0 || I == 5) Grd[S][I] = FMath::Max(Grd[S][I], Under(Pp + Tip * (I ? 3.f : -3.f), nullptr));
				Least = FMath::Min(Least, Grd[S][I]);
				// (what the slope the foot is laid along gives by itself is no rise)
				const float Own6 = Nm[S].Z > 0.5f ? float(-(Nm[S].X * (Pp.X - Ankle.X) + Nm[S].Y * (Pp.Y - Ankle.Y)) / Nm[S].Z) : 0.f;
				Hi6[S] = FMath::Max(Hi6[S], Grd[S][I] - Own6);
			}
		}
		// ground lower than they stand: only where the ground right under them is what they stand on (the apron's paving and
		// others are followed by their own heights, the collision can be the earth under them)
		if (Least < -0.5f && Under(Base, nullptr) < -2.f)
			for (int32 S = 0; S < 2; ++S) { for (int32 I = 0; I < 6; ++I) Grd[S][I] = FMath::Max(Grd[S][I], 0.f); Hi6[S] = FMath::Max(Hi6[S], 0.f); Nm[S] = FVector::UpVector; }
	}
	if (!bFeetOn || FMath::Abs(SmZ - Bz) > 60.f) { SmZ = Bz; for (int32 S = 0; S < 2; ++S) { Rise[S] = Hi6[S]; Slope[S] = Nm[S]; } }
	bFeetOn = true;
	// (the body goes up and down steps all at once: the hips follow it eased - not the player's, whose eyes go with them)
	SmZ = FMath::FInterpTo(SmZ, Bz, Dt, V.SizeSquared2D() > 400.f ? 12.f : 40.f);
	for (int32 S = 0; S < 2; ++S)
	{
		const float Want = FMath::Clamp(FMath::Max(Hi6[S], UnevenT > 0.f && Go.SizeSquared() > 100.f ? Ahead[S] : -100.f), -35.f, 40.f);
		Rise[S] = FMath::FInterpTo(Rise[S], Want, Dt, Want > Rise[S] ? 18.f : 10.f);
		Slope[S] = FMath::VInterpTo(Slope[S], Nm[S], Dt, 10.f).GetSafeNormal();
		for (int32 I = 0; I < 6; ++I) P.Under[S][I] = Grd[S][I];
		P.Rise[S] = Rise[S]; P.Slope[S] = B->GetComponentTransform().InverseTransformVectorNoScale(Slope[S]);
	}
	P.HipRise = FMath::Clamp(FMath::Min(Rise[0], Rise[1]) + (bMe ? 0.f : FMath::Clamp(SmZ - Bz, -20.f, 4.f)), -30.f, 15.f);
	P.Soles = Soles; P.FeetW = FeetW;
	// (tests: the soles as they were last drawn against the ground under them, every 10 s in the log)
	static const bool bCheck = FParse::Param(FCommandLine::Get(), TEXT("FeetCheck"));
	if (!bCheck || FeetW < 0.999f || bMe) return;   // (the player's own body hangs under the camera of a flown view)
	static int32 N = 0, Under0 = 0, Deep = 0; static float Worst = 100.f, SumGap = 0.f; static double SaidAt = 0.0; static FString Who;
	for (int32 S = 0; S < 2; ++S)
	{
		const FTransform Tf = B->GetSocketTransform(Foot[S], RTS_Component) * Was, Tb = B->GetSocketTransform(BallN[S], RTS_Component) * Was;
		float Gap = 100.f; FVector Low = FVector::ZeroVector;
		for (int32 I = 0; I < 6; ++I)
		{
			const FVector Pw = (I < 3 ? Tf : Tb).TransformPosition(Soles.At[S][I]);
			float G = Under(Pw, nullptr);
			if (G < 0.f && Under(Base, nullptr) < -2.f) G = 0.f;
			if (const float G2 = float(Pw.Z) - Bz - G; G2 < Gap) { Gap = G2; Low = Pw; }
		}
		if (Gap < 6.f) { ++N; SumGap += FMath::Max(Gap, 0.f); if (Gap < -0.05f) ++Under0; if (Gap < -2.f) ++Deep; if (Gap < Worst) { Worst = Gap; FHitResult Hw; W->LineTraceSingleByChannel(Hw, FVector(Low.X, Low.Y, Bz + 42.f), FVector(Low.X, Low.Y, Bz - 45.f), ECC_Visibility, Q); Who = FString::Printf(TEXT("%s at %.0f, %.0f, %.0f, %.1f m/s, sole %.0f %.0f over the body, on %s of %s"), *Own->GetName(), Base.X, Base.Y, Base.Z, V.Size2D() / 100.f, Low.Z - Bz, Hw.ImpactPoint.Z - Bz, *GetNameSafe(Hw.GetComponent()), *GetNameSafe(Hw.GetActor())); } }
	}
	if (const double Now = FPlatformTime::Seconds(); Now - SaidAt > 10.0)
	{
		if (N > 0) ADP_LOG(TEXT("Feet: %d soles near the ground, %d under it (%.2f %%; %d more than 2 cm), on average %.2f cm over it, the lowest %.2f cm (%s)"), N, Under0, 100.f * Under0 / N, Deep, SumGap / N, Worst, *Who);
		N = Under0 = Deep = 0; Worst = 100.f; SumGap = 0.f; SaidAt = Now;
	}
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
