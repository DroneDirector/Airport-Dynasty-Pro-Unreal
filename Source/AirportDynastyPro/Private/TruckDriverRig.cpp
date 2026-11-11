// The body that climbs into the crash tender and drives it (TruckDriverRig.h).
#include "TruckDriverRig.h"
#include "ADPLog.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

namespace
{
	/** The rotation taking the frame (D0 along, B0 the bend side) onto (D1, B1). */
	FQuat FrameMap(const FVector& D0, const FVector& B0, const FVector& D1, const FVector& B1)
	{
		const FQuat Q0 = FRotationMatrix::MakeFromXZ(D0, B0).ToQuat(), Q1 = FRotationMatrix::MakeFromXZ(D1, B1).ToQuat();
		return Q1 * Q0.Inverse();
	}
	FVector Flat(const FVector& V) { const FVector F(V.X, V.Y, 0.f); return F.IsNearlyZero() ? FVector(0, 1, 0) : F.GetSafeNormal(); }
}

bool FTruckDriverRig::Init(UPoseableMeshComponent* M)
{
	Mesh = nullptr;
	if (!M || !M->GetSkinnedAsset()) return false;
	const FReferenceSkeleton& RS = M->GetSkinnedAsset()->GetRefSkeleton();
	RefLocal = RS.GetRefBonePose();
	Parent.SetNum(RefLocal.Num());
	for (int32 I = 0; I < RefLocal.Num(); ++I) Parent[I] = RS.GetParentIndex(I);
	RefCS.SetNum(RefLocal.Num());
	for (int32 I = 0; I < RefLocal.Num(); ++I) RefCS[I] = Parent[I] == INDEX_NONE ? RefLocal[I] : RefLocal[I] * RefCS[Parent[I]];
	auto B = [&](const TCHAR* N) { return RS.FindBoneIndex(FName(N)); };
	Pelvis = B(TEXT("pelvis")); Head = B(TEXT("head"));
	const TCHAR* Sd[2] = {TEXT("l"), TEXT("r")};
	for (int32 S = 0; S < 2; ++S)
	{
		Thigh[S] = B(*FString::Printf(TEXT("thigh_%s"), Sd[S])); Calf[S] = B(*FString::Printf(TEXT("calf_%s"), Sd[S])); Foot[S] = B(*FString::Printf(TEXT("foot_%s"), Sd[S]));
		Upper[S] = B(*FString::Printf(TEXT("upperarm_%s"), Sd[S])); Lower[S] = B(*FString::Printf(TEXT("lowerarm_%s"), Sd[S])); Hand[S] = B(*FString::Printf(TEXT("hand_%s"), Sd[S]));
		for (int32 X : {Thigh[S], Calf[S], Foot[S], Upper[S], Lower[S], Hand[S]}) if (X == INDEX_NONE) { ADP_ERROR(TEXT("Truck driver: the body's skeleton has no %s-side limb bones"), Sd[S]); return false; }
	}
	if (Pelvis == INDEX_NONE || Head == INDEX_NONE) { ADP_ERROR(TEXT("Truck driver: the body's skeleton has no pelvis or head")); return false; }
	AnkleH = RefCS[Foot[0]].GetLocation().Z;
	PelvisH = RefCS[Pelvis].GetLocation().Z;
	// the fingers: each joint closes towards the palm (the palm's side found from the hand's own shape in the reference pose)
	for (int32 S = 0; S < 2; ++S)
	{
		Curl[S].Reset();
		const int32 Idx = B(*FString::Printf(TEXT("index_01_%s"), Sd[S])), Pin = B(*FString::Printf(TEXT("pinky_01_%s"), Sd[S])), Mid = B(*FString::Printf(TEXT("middle_01_%s"), Sd[S]));
		if (Idx == INDEX_NONE || Pin == INDEX_NONE || Mid == INDEX_NONE) continue;
		const FVector HandAt = RefCS[Hand[S]].GetLocation();
		const FVector Along = (RefCS[Mid].GetLocation() - HandAt).GetSafeNormal(), Across = (RefCS[Idx].GetLocation() - RefCS[Pin].GetLocation()).GetSafeNormal();
		FVector Palm = FVector::CrossProduct(Along, Across).GetSafeNormal();
		// the thumb sits on the palm's side of the hand
		const int32 Th = B(*FString::Printf(TEXT("thumb_02_%s"), Sd[S]));
		if (Th != INDEX_NONE && FVector::DotProduct(RefCS[Th].GetLocation() - HandAt, Palm) < 0.f) Palm = -Palm;
		for (const TCHAR* F : {TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky"), TEXT("thumb")})
			for (int32 J = 1; J <= 3; ++J)
			{
				const int32 Bn = B(*FString::Printf(TEXT("%s_%02d_%s"), F, J, Sd[S]));
				if (Bn == INDEX_NONE || Parent[Bn] == INDEX_NONE) continue;
				// the joint's own direction (to its child, or its parent's way for the tip)
				const int32 Next = J < 3 ? B(*FString::Printf(TEXT("%s_%02d_%s"), F, J + 1, Sd[S])) : INDEX_NONE;
				const FVector Dir = Next != INDEX_NONE ? (RefCS[Next].GetLocation() - RefCS[Bn].GetLocation()).GetSafeNormal() : (RefCS[Bn].GetLocation() - RefCS[Parent[Bn]].GetLocation()).GetSafeNormal();
				const FVector Axis = FVector::CrossProduct(Dir, Palm).GetSafeNormal();
				if (Axis.IsNearlyZero()) continue;
				const bool bThumb = F[0] == TEXT('t');
				// the axis in the parent's own space, so it turns with the hand
				Curl[S].Add({Bn, RefCS[Parent[Bn]].GetRotation().UnrotateVector(Axis), (bThumb ? 0.55f : J == 1 ? 1.15f : 1.f) * FMath::DegreesToRadians(bThumb ? 35.f : 62.f)});
			}
	}
	Mesh = M;
	Local = RefLocal; CS = RefCS;
	return true;
}

void FTruckDriverRig::Rebuild()
{
	for (int32 I = 0; I < Local.Num(); ++I) CS[I] = Parent[I] == INDEX_NONE ? Local[I] : Local[I] * CS[Parent[I]];
}

void FTruckDriverRig::SetCS(int32 Bone, const FTransform& T)
{
	Local[Bone] = Parent[Bone] == INDEX_NONE ? T : T.GetRelativeTransform(CS[Parent[Bone]]);
	Local[Bone].SetScale3D(RefLocal[Bone].GetScale3D());
	Rebuild();
}

void FTruckDriverRig::Limb(int32 A, int32 Bb, int32 C, const FVector& Target, const FVector& Bend, const FVector& RefBend)
{
	const FVector Root = CS[A].GetLocation();
	const float L1 = (RefCS[Bb].GetLocation() - RefCS[A].GetLocation()).Size(), L2 = (RefCS[C].GetLocation() - RefCS[Bb].GetLocation()).Size();
	FVector ToT = Target - Root;
	const float D = FMath::Clamp(float(ToT.Size()), FMath::Abs(L1 - L2) + 0.5f, L1 + L2 - 0.05f);
	const FVector U = ToT.GetSafeNormal();
	FVector V = (Bend - FVector::DotProduct(Bend, U) * U).GetSafeNormal();
	if (V.IsNearlyZero()) V = FVector::CrossProduct(U, FVector::RightVector).GetSafeNormal();
	const float CosA = FMath::Clamp((L1 * L1 + D * D - L2 * L2) / (2.f * L1 * D), -1.f, 1.f);
	const FVector Mid = Root + U * (L1 * CosA) + V * (L1 * FMath::Sqrt(1.f - CosA * CosA));
	const FVector End = Root + U * D;
	// each bone turned from its reference direction onto the new one, its bend side kept to the bend side
	const FVector R0a = (RefCS[Bb].GetLocation() - RefCS[A].GetLocation()).GetSafeNormal(), R0b = (RefCS[C].GetLocation() - RefCS[Bb].GetLocation()).GetSafeNormal();
	const FQuat Qa = FrameMap(R0a, RefBend, (Mid - Root).GetSafeNormal(), V), Qb = FrameMap(R0b, RefBend, (End - Mid).GetSafeNormal(), V);
	SetCS(A, FTransform(Qa * RefCS[A].GetRotation(), Root));
	SetCS(Bb, FTransform(Qb * RefCS[Bb].GetRotation(), Mid));
}

void FTruckDriverRig::Pose(const FDriverPose& P)
{
	if (!Mesh) return;
	Local = RefLocal; Rebuild();
	const FVector Up(0, 0, 1), Fw = Flat(P.Facing);
	const FQuat Yaw(Up, FMath::Atan2(Fw.Y, Fw.X) - HALF_PI);   // the reference pose faces +y (a plain turn about up, even facing -y)
	const FQuat LeanQ = FQuat::FindBetweenNormals(Up, (Up * FMath::Cos(FMath::DegreesToRadians(P.Lean)) + Fw * FMath::Sin(FMath::DegreesToRadians(P.Lean))).GetSafeNormal());
	SetCS(Pelvis, FTransform(LeanQ * Yaw * RefCS[Pelvis].GetRotation(), P.Pelvis));
	// (which way is the body's left: the reference pose's left foot is on its own side)
	const float LeftSign = FMath::Sign(FVector::DotProduct(RefCS[Thigh[0]].GetLocation() - RefCS[Pelvis].GetLocation(), FVector(1, 0, 0)));
	const FVector Left = Yaw.RotateVector(FVector(LeftSign, 0, 0));
	for (int32 S = 0; S < 2; ++S)
	{
		const FVector Out = S == 0 ? Left : -Left;
		// the leg: knee forward and a little out; the foot flat (or tipped), facing the body's way
		const FQuat FootQ = FQuat::FindBetweenNormals(Fw, (Fw * FMath::Cos(FMath::DegreesToRadians(P.FootPitch[S])) + Up * FMath::Sin(FMath::DegreesToRadians(P.FootPitch[S]))).GetSafeNormal()) * Yaw;
		const FVector Ankle = P.Foot[S] + FootQ.RotateVector(FVector(0, 0, AnkleH));
		Limb(Thigh[S], Calf[S], Foot[S], Ankle, Fw + Out * 0.15f, FVector(0, 1, 0));
		SetCS(Foot[S], FTransform(FootQ * RefCS[Foot[S]].GetRotation(), CS[Foot[S]].GetLocation()));
		// the arm: to what it holds, the elbow down, back and out; else hanging by the side
		const FVector Sh = CS[Upper[S]].GetLocation();
		const float Arm = (RefCS[Lower[S]].GetLocation() - RefCS[Upper[S]].GetLocation()).Size() + (RefCS[Hand[S]].GetLocation() - RefCS[Lower[S]].GetLocation()).Size();
		const FVector Target = P.bHand[S] ? P.Hand[S] : Sh + (-Up * 0.97f + Out * 0.16f + Fw * 0.05f).GetSafeNormal() * Arm * 0.97f;
		Limb(Upper[S], Lower[S], Hand[S], Target, -Up * 0.6f - Fw * 0.3f + Out * 0.6f, FVector(0, -1, 0));
		// the fingers round the bar
		const float Gr = FMath::Clamp(P.Grip[S], 0.f, 1.f);
		if (Gr > 0.01f)
		{
			for (const FCurl& C : Curl[S]) Local[C.Bone].SetRotation(FQuat(C.Axis, C.Weight * Gr) * Local[C.Bone].GetRotation());
			Rebuild();
		}
	}
	const FVector HeadAt = CS[Head].GetLocation();
	EyesAt = HeadAt + Fw * 12.f + Up * 6.f;
	for (int32 I = 0; I < Local.Num() && I < Mesh->BoneSpaceTransforms.Num(); ++I) Mesh->BoneSpaceTransforms[I] = Local[I];
	Mesh->MarkRefreshTransformDirty();
}
