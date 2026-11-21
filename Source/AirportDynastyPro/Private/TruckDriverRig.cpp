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
	for (int32 S = 0; S < 2; ++S)
	{
		HandPts[S].Reset(); HandPts[S].Add(Hand[S]);
		for (const TCHAR* F : {TEXT("middle_01"), TEXT("middle_02"), TEXT("middle_03"), TEXT("index_02"), TEXT("index_03"), TEXT("ring_02"), TEXT("ring_03"), TEXT("pinky_02"), TEXT("pinky_03"), TEXT("thumb_03")})
		{ const int32 X = B(*FString::Printf(TEXT("%s_%s"), F, Sd[S])); if (X != INDEX_NONE) HandPts[S].Add(X); }
	}
	// the idle movements' bones (the upper body's turn, the shoulders, the head's look)
	Spine.Reset(); Neck.Reset();
	for (int32 I = 1; I <= 5; ++I) { const int32 X = B(*FString::Printf(TEXT("spine_%02d"), I)); if (X != INDEX_NONE) Spine.Add(X); }
	for (int32 I = 1; I <= 2; ++I) { const int32 X = B(*FString::Printf(TEXT("neck_%02d"), I)); if (X != INDEX_NONE) Neck.Add(X); }
	Neck.Add(Head);
	Clav[0] = B(TEXT("clavicle_l")); Clav[1] = B(TEXT("clavicle_r"));
	LeftSign = FMath::Sign(FVector::DotProduct(RefCS[Thigh[0]].GetLocation() - RefCS[Pelvis].GetLocation(), FVector(1, 0, 0)));
	if (LeftSign == 0.f) LeftSign = 1.f;
	for (int32 S = 0; S < 2; ++S) { PalmL[S] = FVector::ZeroVector; AlongL[S] = FVector::ZeroVector; }
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
		PalmL[S] = RefCS[Hand[S]].GetRotation().UnrotateVector(Palm); AlongL[S] = RefCS[Hand[S]].GetRotation().UnrotateVector(Along);
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

void FTruckDriverRig::TurnCS(int32 Bone, const FQuat& Q)
{
	if (Bone == INDEX_NONE || Q.Equals(FQuat::Identity, 1e-5f)) return;
	SetCS(Bone, FTransform(Q * CS[Bone].GetRotation(), CS[Bone].GetLocation()));
}

FVector FTruckDriverRig::Body(const FQuat& Yaw, const FVector& V) const
{
	// (the reference pose faces +y, its left on LeftSign x; turned by the pose's yaw)
	return Yaw.RotateVector(FVector(V.X * LeftSign, V.Y, V.Z));
}

FVector FTruckDriverRig::Landmark(uint8 On, int32 S, const FVector& Off, const FQuat& Yaw) const
{
	// a place on the body, found on its own bones as they stand posed just now: the reference place (bone + a fixed offset
	// in the upright reference body) carried by that bone's present transform - so a hand on the head goes with the head,
	// a hand on the back with the spine. Then the wrist's own offset Off (body frame, X towards the hand's own side).
	const float Sd = S == 0 ? 1.f : -1.f;   // (the hand's own side: +1 its left)
	auto At = [&](int32 Bone, const FVector& RefOff) -> FVector
	{
		if (Bone == INDEX_NONE) return CS[Pelvis].GetLocation();
		const FVector RefP = RefCS[Bone].GetLocation() + FVector(RefOff.X * LeftSign, RefOff.Y, RefOff.Z);
		return CS[Bone].TransformPosition(RefCS[Bone].InverseTransformPosition(RefP));
	};
	const int32 Sp0 = Spine.Num() ? Spine[0] : Pelvis, SpTop = Spine.Num() ? Spine.Last() : Pelvis, Nk = Neck.Num() > 1 ? Neck[0] : Head;
	const int32 O = 1 - S;
	FVector P;
	switch (On)
	{
	case FDriverPose::OnHeadTop:       P = At(Head, FVector(0.f, 1.f, 17.f)); break;                // (the crown)
	case FDriverPose::OnHeadSide:      P = At(Head, FVector(Sd * 8.f, -1.f, 9.f)); break;           // (above the ear, its own side)
	case FDriverPose::OnMouth:         P = At(Head, FVector(0.f, 11.f, 1.f)); break;
	case FDriverPose::OnNeckBack:      P = At(Nk, FVector(Sd * 2.f, -6.f, 2.f)); break;
	case FDriverPose::OnLowerBack:     P = At(Sp0, FVector(Sd * 9.f, -13.f, 2.f)); break;
	case FDriverPose::OnHip:           P = At(Thigh[S], FVector(Sd * 8.f, 0.f, 9.f)); break;      // (the top of the hip bone, outside)
	case FDriverPose::OnOtherUpperArm: P = At(Upper[O], FVector(0.f, 5.f, -15.f)); break;         // (the other arm's upper arm, its front)
	case FDriverPose::OnOtherWrist:    P = CS[Hand[O]].GetLocation(); break;
	case FDriverPose::OnChest:         P = At(SpTop, FVector(0.f, 14.f, -4.f)); break;
	default:                           return CS[Pelvis].GetLocation();
	}
	return P + Body(Yaw, FVector(Off.X * Sd, Off.Y, Off.Z));
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
	const FVector Left = Yaw.RotateVector(FVector(LeftSign, 0, 0));
	// the idle movements (owner 2026-11-21), each within natural limits: the upper body's turn spread over the spine, the
	// shoulders raised / rolled, then the head's turn and look spread over the neck - all before the arms, so the hands
	// reach from where the shoulders now are
	{
		const FVector BUp = LeanQ.RotateVector(Up), BFw = LeanQ.RotateVector(Fw), BLeft = LeanQ.RotateVector(Left);
		// (the look at a hand: where its target will be, from the body's frame before the arms are solved)
		float LookYaw = 0.f, LookPitch = 0.f;
		if (P.LookHand >= 0 && P.LookHand < 2 && P.LookW > 0.f && P.bHand[P.LookHand])
		{
			const int32 Lh = P.LookHand;
			const FVector Eye = CS[Head].GetLocation() + BFw * 10.f;
			const FVector Tgt = P.HandOn[Lh] != FDriverPose::OnNone && P.HandOn[Lh] != FDriverPose::OnOtherWrist
				? FMath::Lerp(P.Hand[Lh], Landmark(P.HandOn[Lh], Lh, P.HandOff[Lh], Yaw), FMath::Clamp(P.HandOnW[Lh], 0.f, 1.f)) : P.Hand[Lh];
			const FVector D = (Tgt - Eye).GetSafeNormal();
			LookYaw = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(D, BLeft), FVector::DotProduct(D, BFw))) * P.LookW;
			LookPitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-FVector::DotProduct(D, BUp), -1.f, 1.f))) * P.LookW;
		}
		const float HY = FMath::Clamp(P.HeadYaw + LookYaw, -75.f, 75.f), HP = FMath::Clamp(P.HeadPitch + LookPitch, -35.f, 50.f), HR = FMath::Clamp(P.HeadRoll, -30.f, 30.f);
		// (the spine: its own turn plus a fifth of the look's; at most 40 degrees. FQuat(Cross(A, B), +a) turns A towards B)
		const float Tw = FMath::Clamp(P.Twist + HY * 0.2f, -40.f, 40.f);
		const FVector YawAx = FVector::CrossProduct(BFw, BLeft).GetSafeNormal();
		if (Spine.Num() && FMath::Abs(Tw) > 0.01f)
		{
			const FQuat Q(YawAx, FMath::DegreesToRadians(Tw) / Spine.Num());
			for (const int32 X : Spine) TurnCS(X, Q);
		}
		for (int32 S = 0; S < 2; ++S)
			if (Clav[S] != INDEX_NONE && (FMath::Abs(P.ShoulderUp[S]) > 0.01f || FMath::Abs(P.ShoulderFwd[S]) > 0.01f))
			{
				const FVector Out = (CS[Upper[S]].GetLocation() - CS[Clav[S]].GetLocation()).GetSafeNormal();
				const FVector AxU = FVector::CrossProduct(Out, BUp).GetSafeNormal(), AxF = FVector::CrossProduct(Out, BFw).GetSafeNormal();
				FQuat Q = FQuat::Identity;
				if (!AxU.IsNearlyZero()) Q = FQuat(AxU, FMath::DegreesToRadians(FMath::Clamp(P.ShoulderUp[S], -8.f, 18.f))) * Q;
				if (!AxF.IsNearlyZero()) Q = FQuat(AxF, FMath::DegreesToRadians(FMath::Clamp(P.ShoulderFwd[S], -14.f, 14.f))) * Q;
				TurnCS(Clav[S], Q);
			}
		// (the head: the rest of the turn, the nod and the tilt over the neck's joints, in the frame the spine left it in)
		if (Neck.Num() && (FMath::Abs(HY - Tw) > 0.01f || FMath::Abs(HP) > 0.01f || FMath::Abs(HR) > 0.01f))
		{
			const float N = float(Neck.Num());
			const FQuat Sp = Spine.Num() ? FQuat(YawAx, FMath::DegreesToRadians(Tw)) : FQuat::Identity;
			const FVector HUp = Sp.RotateVector(BUp), HFw = Sp.RotateVector(BFw), HLeft = Sp.RotateVector(BLeft);
			const FQuat QY(YawAx, FMath::DegreesToRadians(HY - Tw) / N);
			const FQuat QP(FVector::CrossProduct(HFw, -HUp).GetSafeNormal(), FMath::DegreesToRadians(HP) / N);   // (the face towards down)
			const FQuat QR(FVector::CrossProduct(HUp, HLeft).GetSafeNormal(), FMath::DegreesToRadians(HR) / N);  // (the crown towards the left)
			const FQuat Q = QY * QP * QR;
			for (const int32 X : Neck) TurnCS(X, Q);
		}
	}
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
		FVector Target = P.bHand[S] ? P.Hand[S] : Sh + (-Up * 0.97f + Out * 0.16f + Fw * 0.05f).GetSafeNormal() * Arm * 0.97f;
		// (a hand on the body: the landmark on its own bones, blended in)
		if (P.bHand[S] && P.HandOn[S] != FDriverPose::OnNone && P.HandOnW[S] > 0.f)
			Target = FMath::Lerp(Target, Landmark(P.HandOn[S], S, P.HandOff[S], Yaw), FMath::Clamp(P.HandOnW[S], 0.f, 1.f));
		FVector Bend = -Up * 0.6f - Fw * 0.3f + Out * 0.6f;
		if (!P.Pole[S].IsNearlyZero()) Bend = Body(Yaw, FVector(P.Pole[S].X * (S == 0 ? 1.f : -1.f), P.Pole[S].Y, P.Pole[S].Z)).GetSafeNormal();   // (X: out to its own side)
		Limb(Upper[S], Lower[S], Hand[S], Target, Bend, FVector(0, -1, 0));
		// the palm the way it must face on what it lies on (the wrist turns, at most 70 degrees - the forearm took the rest)
		if (P.PalmW[S] > 0.f && !P.PalmDir[S].IsNearlyZero() && !PalmL[S].IsNearlyZero())
		{
			const float Sx = S == 0 ? 1.f : -1.f;
			const FVector WantP = Body(Yaw, FVector(P.PalmDir[S].X * Sx, P.PalmDir[S].Y, P.PalmDir[S].Z)).GetSafeNormal();
			const FQuat HQ = CS[Hand[S]].GetRotation();
			const FVector NowP = HQ.RotateVector(PalmL[S]), NowA = HQ.RotateVector(AlongL[S]);
			FQuat Q = FQuat::FindBetweenNormals(NowP, WantP);
			if (!P.FingerDir[S].IsNearlyZero())
			{
				FVector WantA = Body(Yaw, FVector(P.FingerDir[S].X * Sx, P.FingerDir[S].Y, P.FingerDir[S].Z));
				WantA = (WantA - FVector::DotProduct(WantA, WantP) * WantP).GetSafeNormal();
				if (!WantA.IsNearlyZero()) Q = FrameMap(NowA, NowP, WantA, WantP);
			}
			FVector Ax; float Ang; Q.ToAxisAndAngle(Ax, Ang);
			if (Ang > PI) { Ang = 2.f * PI - Ang; Ax = -Ax; }
			Q = FQuat(Ax, FMath::Min(Ang, FMath::DegreesToRadians(70.f)) * FMath::Clamp(P.PalmW[S], 0.f, 1.f));
			TurnCS(Hand[S], Q);
		}
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

void FTruckDriverRig::RestHandsOnThighs(FDriverPose& P, float ThighR, float Weight)
{
	if (!Mesh || Weight <= 0.f) return;
	bool bAny = false;
	for (int32 h = 0; h < 2; ++h)
	{
		if (!P.bHand[h]) continue;
		int32 Side = 0; float Bd = 1e9f;
		for (int32 k = 0; k < 2; ++k) { const float D = FMath::Abs(HipAt(k).X - P.Hand[h].X) + FMath::Abs(KneeAt(k).X - P.Hand[h].X); if (D < Bd) { Bd = D; Side = k; } }
		const FVector Hip = HipAt(Side), Knee = KneeAt(Side), Ax = Knee - Hip;
		const float L2 = FMath::Max(Ax.SizeSquared(), 1.f);
		const float T = FMath::Clamp(FVector::DotProduct(P.Hand[h] - Hip, Ax) / L2, 0.35f, 0.95f);
		const FVector On = Hip + Ax * T;
		// (beside the thigh's top line by the hand's own offset, a little at most; the wrist a palm over the skin)
		const FVector Across = FVector::CrossProduct(Ax.GetSafeNormal(), FVector::UpVector).GetSafeNormal();
		const float Off = FMath::Clamp(FVector::DotProduct(P.Hand[h] - On, Across), -ThighR * 0.6f, ThighR * 0.6f);
		P.Hand[h] = FMath::Lerp(P.Hand[h], On + Across * Off + FVector(0, 0, ThighR + 3.f), FMath::Clamp(Weight, 0.f, 1.f));
		bAny = true;
	}
	if (!bAny) return;
	Pose(P);
	// (then the whole hand, not just the wrist: every palm and finger joint measured against both thighs - a joint inside
	// one lifts its hand by exactly that much and the body is posed again, a few passes, until the hand lies on the skin)
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		bool bMoved = false;
		for (int32 h = 0; h < 2; ++h)
		{
			if (!P.bHand[h]) continue;
			float Pen = 0.f;
			for (const int32 X : HandPts[h])
			{
				if (!CS.IsValidIndex(X)) continue;
				const FVector Pt = CS[X].GetLocation();
				for (int32 k = 0; k < 2; ++k) Pen = FMath::Max(Pen, ThighR + 1.2f - FMath::PointDistToSegment(Pt, HipAt(k), KneeAt(k)));
			}
			if (Pen > 0.2f) { P.Hand[h].Z += Pen * FMath::Clamp(Weight, 0.f, 1.f); bMoved = true; }
		}
		if (!bMoved) break;
		Pose(P);
	}
}

float FTruckDriverRig::ThighsInSeat(UWorld* W, const FTransform& Frame, float ThighR, const AActor* Ignore) const
{
	if (!Mesh || !W) return 0.f;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ThighSeat), false, Ignore);
	if (Ignore) { TArray<AActor*> On; Ignore->GetAttachedActors(On, true, true); for (AActor* X : On) Q.AddIgnoredActor(X); }   // (your own body is its own actor on you)
	float Deep = 0.f;
	for (int32 k = 0; k < 2; ++k)
		for (int32 i = 2; i <= 17; ++i)   // (along the thigh to 85 % of the way to the knee, in 5 % steps: the back of the knee may rest on the seat's edge, as people's do)
		{
			// a point on the thigh's middle line, its underside a thigh's thickness under it; what is right below
			const FVector P = Frame.TransformPosition(FMath::Lerp(HipAt(k), KneeAt(k), i / 20.f));
			FHitResult H;
			if (!W->LineTraceSingleByChannel(H, P + FVector(0, 0, 4.f), P - FVector(0, 0, ThighR + 25.f), ECC_Visibility, Q)) continue;
			const float D = H.ImpactPoint.Z - (P.Z - ThighR);
			if (D > Deep) { Deep = D; static int32 Said = 0; if (D > 3.f && Said++ < 6) ADP_LOG(TEXT("Sitting IK: thigh %d at %d/10 in %.1f cm - under it: %s (%s)"), k, i, D, H.GetActor() ? *H.GetActor()->GetName() : TEXT("-"), H.GetComponent() ? *H.GetComponent()->GetName() : TEXT("-")); }
		}
	return Deep;
}
