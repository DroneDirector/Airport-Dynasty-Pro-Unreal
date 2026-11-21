// The passengers' own animation instance: the walk blend space plus a hand closed round a carried bag's handle
// (PaxAnimInstance.h).
#include "PaxAnimInstance.h"
#include "ADPLog.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

namespace
{
	/** Every bone's component-space transform in the reference pose. */
	void RefCS(const FReferenceSkeleton& RS, TArray<FTransform>& CS)
	{
		const TArray<FTransform>& L = RS.GetRefBonePose();
		CS.SetNum(L.Num());
		for (int32 I = 0; I < L.Num(); ++I) { const int32 Pa = RS.GetParentIndex(I); CS[I] = Pa == INDEX_NONE ? L[I] : L[I] * CS[Pa]; }
	}
	/** The right hand's frame in the reference pose: along the fingers, and the palm's side (the thumb's side). */
	bool HandFrame(const FReferenceSkeleton& RS, const TArray<FTransform>& CS, FVector& Along, FVector& Palm)
	{
		const int32 H = RS.FindBoneIndex(TEXT("hand_r")), Idx = RS.FindBoneIndex(TEXT("index_01_r")), Pin = RS.FindBoneIndex(TEXT("pinky_01_r")), Mid = RS.FindBoneIndex(TEXT("middle_01_r"));
		if (H == INDEX_NONE || Idx == INDEX_NONE || Pin == INDEX_NONE || Mid == INDEX_NONE) return false;
		const FVector HandAt = CS[H].GetLocation();
		Along = (CS[Mid].GetLocation() - HandAt).GetSafeNormal();
		const FVector Across = (CS[Idx].GetLocation() - CS[Pin].GetLocation()).GetSafeNormal();
		Palm = FVector::CrossProduct(Along, Across).GetSafeNormal();
		// (the same rule as the fire truck's rig, TruckDriverRig.cpp: the thumb sits on the palm's side of the hand)
		const int32 Th = RS.FindBoneIndex(TEXT("thumb_02_r"));
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
}

void FPaxAnimProxy::PreUpdate(UAnimInstance* In, float Dt)
{
	FAnimSingleNodeInstanceProxy::PreUpdate(In, Dt);
	if (const UPaxAnimInstance* A = Cast<UPaxAnimInstance>(In))
	{
		// (in and out over a quarter of a second)
		Grip = FMath::FInterpConstantTo(Grip, A->bGripRight ? 1.f : 0.f, Dt, 4.f);
		ArmOut = A->ArmOut;
	}
}

void FPaxAnimProxy::Rig(const FBoneContainer& BC)
{
	const FReferenceSkeleton& RS = BC.GetReferenceSkeleton();
	RigFor = &RS; Curl.Reset(); Upper = Lower = INDEX_NONE; Out = FVector::ZeroVector;
	TArray<FTransform> CS; RefCS(RS, CS);
	Upper = RS.FindBoneIndex(TEXT("upperarm_r")); Lower = RS.FindBoneIndex(TEXT("lowerarm_r"));
	const int32 UpperL = RS.FindBoneIndex(TEXT("upperarm_l"));
	if (Upper != INDEX_NONE && UpperL != INDEX_NONE) { const FVector D = CS[Upper].GetLocation() - CS[UpperL].GetLocation(); Out = FVector(D.X, D.Y, 0.f).GetSafeNormal(); }
	FVector Along, Palm;
	if (!HandFrame(RS, CS, Along, Palm)) return;
	// each finger joint closes towards the palm round its own axis (found in the reference pose, kept in the parent's
	// space so it turns with the hand) - the angles a hand needs to wrap round a ~3 cm bar: the knuckle and the middle
	// joint well bent, the tip a little less; the thumb over the fingers
	static const float Deg[2][3] = {{60.f, 75.f, 45.f}, {15.f, 22.f, 28.f}};
	for (const TCHAR* F : {TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky"), TEXT("thumb")})
		for (int32 J = 1; J <= 3; ++J)
		{
			const int32 Bn = RS.FindBoneIndex(FName(*FString::Printf(TEXT("%s_%02d_r"), F, J)));
			if (Bn == INDEX_NONE || RS.GetParentIndex(Bn) == INDEX_NONE) continue;
			const int32 Pa = RS.GetParentIndex(Bn);
			const int32 Next = J < 3 ? RS.FindBoneIndex(FName(*FString::Printf(TEXT("%s_%02d_r"), F, J + 1))) : INDEX_NONE;
			const FVector Dir = Next != INDEX_NONE ? (CS[Next].GetLocation() - CS[Bn].GetLocation()).GetSafeNormal() : (CS[Bn].GetLocation() - CS[Pa].GetLocation()).GetSafeNormal();
			const FVector Axis = FVector::CrossProduct(Dir, Palm).GetSafeNormal();
			if (Axis.IsNearlyZero()) continue;
			const bool bThumb = F[0] == TEXT('t');
			Curl.Add({Bn, CS[Pa].GetRotation().UnrotateVector(Axis), FMath::DegreesToRadians(Deg[bThumb ? 1 : 0][J - 1])});
		}
}

bool FPaxAnimProxy::Evaluate(FPoseContext& Output)
{
	const bool bOk = FAnimSingleNodeInstanceProxy::Evaluate(Output);
	if (Grip <= 0.001f) return bOk;
	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& BC = Pose.GetBoneContainer();
	if (RigFor != &BC.GetReferenceSkeleton()) Rig(BC);
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
	if (!HandFrame(RS, CS, Along, Palm)) return FVector::ZeroVector;
	const int32 H = RS.FindBoneIndex(TEXT("hand_r")), Mid = RS.FindBoneIndex(TEXT("middle_01_r"));
	// (just past the middle knuckle along the finger, and a bar's radius plus the finger's half thickness to the palm's
	//  side: the middle of a ~3 cm handle bar inside the closed fingers)
	const FVector G = CS[Mid].GetLocation() + Along * 2.f + Palm * 2.5f;
	return CS[H].InverseTransformPosition(G);
}
