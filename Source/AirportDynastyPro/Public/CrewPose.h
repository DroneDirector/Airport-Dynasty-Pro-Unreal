// The crew's posed movements, shared by every vehicle's crew (the tug's in TugCrew.cpp, the baggage tractors' and carts'
// in GSECrew.cpp): standing, squatting with both hands at a place, reaching with one hand, and the blend between two
// poses - all in world coordinates, for the fire truck's IK rig (TruckDriverRig.h).
#pragma once

#include "CoreMinimal.h"
#include "TruckDriverRig.h"

namespace ADPPose
{
	inline const FVector& Up() { static const FVector U(0, 0, 1); return U; }
	inline FVector LeftOf(const FVector& F) { const FVector G = F.GetSafeNormal2D(); return FVector(G.Y, -G.X, 0.f); }
	/** In, held, out: a movement that reaches its pose by a fifth of the way and lets go over the last fifth. */
	inline float Ramp(float K) { return FMath::SmoothStep(0.f, 0.22f, K) * (1.f - FMath::SmoothStep(0.8f, 1.f, K)); }
	inline FDriverPose Lerp(const FDriverPose& A, const FDriverPose& B, float K)
	{
		FDriverPose P = B;
		P.Pelvis = FMath::Lerp(A.Pelvis, B.Pelvis, K); P.Facing = FMath::Lerp(A.Facing, B.Facing, K).GetSafeNormal2D(); P.Lean = FMath::Lerp(A.Lean, B.Lean, K);
		if (P.Facing.IsNearlyZero()) P.Facing = B.Facing;
		for (int32 s = 0; s < 2; ++s)
		{
			P.Foot[s] = FMath::Lerp(A.Foot[s], B.Foot[s], K); P.FootPitch[s] = FMath::Lerp(A.FootPitch[s], B.FootPitch[s], K);
			// (a hand reaching from hanging: from beside the hip)
			const FVector Ha = A.bHand[s] ? A.Hand[s] : A.Pelvis + LeftOf(A.Facing) * (s == 0 ? 22.f : -22.f) - Up() * 15.f;
			const FVector Hb = B.bHand[s] ? B.Hand[s] : B.Pelvis + LeftOf(B.Facing) * (s == 0 ? 22.f : -22.f) - Up() * 15.f;
			P.Hand[s] = FMath::Lerp(Ha, Hb, K); P.Grip[s] = FMath::Lerp(A.Grip[s], B.Grip[s], K);
			P.bHand[s] = A.bHand[s] || B.bHand[s];
			// (the idle movements: a hand on the body blends onto its landmark, the palm turns, the shoulders move)
			P.ShoulderUp[s] = FMath::Lerp(A.ShoulderUp[s], B.ShoulderUp[s], K); P.ShoulderFwd[s] = FMath::Lerp(A.ShoulderFwd[s], B.ShoulderFwd[s], K);
			const bool bOnB = B.HandOn[s] != FDriverPose::OnNone;
			P.HandOn[s] = bOnB ? B.HandOn[s] : A.HandOn[s]; P.HandOff[s] = bOnB ? B.HandOff[s] : A.HandOff[s];
			P.HandOnW[s] = bOnB ? FMath::Lerp(A.HandOn[s] == B.HandOn[s] ? A.HandOnW[s] : 0.f, B.HandOnW[s], K) : FMath::Lerp(A.HandOnW[s], 0.f, K);
			const bool bPB = !B.PalmDir[s].IsNearlyZero();
			P.PalmDir[s] = bPB ? B.PalmDir[s] : A.PalmDir[s]; P.FingerDir[s] = bPB ? B.FingerDir[s] : A.FingerDir[s];
			P.PalmW[s] = FMath::Lerp(A.PalmW[s], bPB ? B.PalmW[s] : 0.f, K);
			const bool bPoB = !B.Pole[s].IsNearlyZero();
			P.Pole[s] = bPoB && !A.Pole[s].IsNearlyZero() ? FMath::Lerp(A.Pole[s], B.Pole[s], K) : (bPoB ? B.Pole[s] : A.Pole[s]);
		}
		P.HeadYaw = FMath::Lerp(A.HeadYaw, B.HeadYaw, K); P.HeadPitch = FMath::Lerp(A.HeadPitch, B.HeadPitch, K); P.HeadRoll = FMath::Lerp(A.HeadRoll, B.HeadRoll, K);
		P.Twist = FMath::Lerp(A.Twist, B.Twist, K);
		P.LookHand = B.LookHand >= 0 ? B.LookHand : A.LookHand; P.LookW = FMath::Lerp(A.LookW, B.LookHand >= 0 ? B.LookW : 0.f, K);
		return P;
	}
	/** Standing at a spot facing a way (side 0 is the body's left). */
	inline FDriverPose Standing(const FVector& At, const FVector& Face, float PH)
	{
		FDriverPose P; P.Facing = Face.GetSafeNormal2D(); const FVector L = LeftOf(P.Facing);
		P.Pelvis = At + Up() * PH; P.Foot[0] = At + L * 12.f; P.Foot[1] = At - L * 12.f;
		return P;
	}
	/** Down on the haunches at a spot, both hands at a place; E 0..1 how far down. */
	inline FDriverPose Squat(const FVector& At, const FVector& Face, const FVector& Hands, float PH, float E)
	{
		FDriverPose S = Standing(At, Face, PH); const FVector L = LeftOf(S.Facing);
		FDriverPose D = S; D.Pelvis = At + Up() * (PH * 0.55f) - S.Facing * 14.f; D.Lean = 38.f;
		D.Foot[0] = At + L * 19.f + S.Facing * 4.f; D.Foot[1] = At - L * 19.f + S.Facing * 4.f;
		D.bHand[0] = D.bHand[1] = true; D.Hand[0] = Hands + L * 11.f; D.Hand[1] = Hands - L * 11.f; D.Grip[0] = D.Grip[1] = 0.8f;
		return Lerp(S, D, E);
	}
	/** Bent over (not squatting) with one hand at a low place - a pin, a strap's hook. */
	inline FDriverPose Stoop(const FVector& At, const FVector& Face, int32 Side, const FVector& Hand, float Grip, float PH, float E)
	{
		FDriverPose S = Standing(At, Face, PH), D = S;
		D.Pelvis = At + Up() * (PH * 0.86f) - S.Facing * 10.f; D.Lean = 34.f;
		D.bHand[Side] = true; D.Hand[Side] = Hand; D.Grip[Side] = Grip;
		return Lerp(S, D, E);
	}
	/** Standing, one hand at a place, the other hanging. */
	inline FDriverPose Reach(const FVector& At, const FVector& Face, int32 Side, const FVector& Hand, float Grip, float PH, float E)
	{
		FDriverPose S = Standing(At, Face, PH), D = S;
		D.bHand[Side] = true; D.Hand[Side] = Hand; D.Grip[Side] = Grip; D.Lean = 4.f;
		return Lerp(S, D, E);
	}
	/** Standing, both hands at places (a bar, a curtain's hem). */
	inline FDriverPose Both(const FVector& At, const FVector& Face, const FVector& HandL, const FVector& HandR, float Grip, float PH, float E, float Lean = 6.f)
	{
		FDriverPose S = Standing(At, Face, PH), D = S;
		D.bHand[0] = D.bHand[1] = true; D.Hand[0] = HandL; D.Hand[1] = HandR; D.Grip[0] = D.Grip[1] = Grip; D.Lean = Lean;
		// (reaching low: the hips go down and back)
		const float Low = FMath::Clamp((S.Pelvis.Z + 20.f - FMath::Min(HandL.Z, HandR.Z)) / 90.f, 0.f, 1.f);
		D.Pelvis = S.Pelvis - Up() * (PH * 0.3f * Low) - S.Facing * (12.f * Low); D.Lean += 26.f * Low;
		return Lerp(S, D, E);
	}
}
