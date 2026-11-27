// Head and eyes to yours while you talk (TalkLook.h).
#include "TalkLook.h"
#include "ADPLog.h"
#include "Animation/AnimNodeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "PaxAnimInstance.h"
#include "PerfLog.h"
#include "ReferenceSkeleton.h"
#include "TalkCamera.h"

namespace
{
	constexpr float HeadSide = 60.f, HeadUp = 25.f, HeadDown = 30.f;   // (degrees from the body)
	constexpr float EyeSide = 25.f, EyeUp = 18.f, EyeDown = 22.f;
	constexpr float EaseTime = 0.4f, HeadLag = 0.1f;
	// (how far the face's rig turns an eye at a full control, measured: adp.TalkLookTest)
	constexpr float FullOut = 42.2f, FullIn = 37.6f, FullUp = 29.8f, FullDown = 39.9f;   // (out: away from the nose)

	struct FEyes
	{
		TWeakObjectPtr<USkeletalMeshComponent> Face, Body; TWeakObjectPtr<UTalkFaceAnim> Anim;
		FQuat HeadRef = FQuat::Identity;   // the head bone as made, component space (its left +X, front +Y, up +Z there)
		FVector2f Eye[2] = {FVector2f::ZeroVector, FVector2f::ZeroVector}, Dart = FVector2f::ZeroVector;
		float W = 0.f, DartIn = 1.f, BlinkIn = 1.f, BlinkT = -1.f; bool bTwice = false;
	};
	TWeakObjectPtr<AActor> GWho; TWeakObjectPtr<USkeletalMeshComponent> GBody;
	FVector GAt = FVector::ZeroVector; bool bGAt = false;
	TArray<FEyes> GEyes;   // (the one you talk to, and any whose eyes are still going back)

	/** One side of a conversation between two people (NpcChat.h): this body's head and eyes to the other one's head. */
	struct FPairLook
	{
		TWeakObjectPtr<AActor> Who, At; TWeakObjectPtr<USkeletalMeshComponent> Body, AtBody;
		FVector Point = FVector::ZeroVector; bool bPoint = false;
	};
	TArray<FPairLook> GPairs;

	/** Where this body looks just now: at you, or at the one it talks with. */
	bool LookPtFor(const USkeletalMeshComponent* B, FVector& Out)
	{
		if (bGAt && GBody.Get() == B) { Out = GAt; return true; }
		for (const FPairLook& P : GPairs) if (P.bPoint && P.Body.Get() == B) { Out = P.Point; return true; }
		return false;
	}

	FTransform LookRefOf(const FReferenceSkeleton& RS, int32 B)
	{
		FTransform T = FTransform::Identity;
		for (; B != INDEX_NONE; B = RS.GetParentIndex(B)) T = T * RS.GetRefBonePose()[B];
		return T;
	}
	FTransform LookPoseOf(const FCompactPose& Pose, FCompactPoseBoneIndex I)
	{
		const FBoneContainer& BC = Pose.GetBoneContainer();
		FTransform T = Pose[I];
		for (FCompactPoseBoneIndex P = BC.GetParentBoneIndex(I); P != INDEX_NONE; P = BC.GetParentBoneIndex(P)) T = T * Pose[P];
		return T;
	}
	void LookTurnBone(FCompactPose& Pose, FCompactPoseBoneIndex I, const FQuat& Q)
	{
		const FBoneContainer& BC = Pose.GetBoneContainer();
		const FCompactPoseBoneIndex Pa = BC.GetParentBoneIndex(I);
		const FQuat P = Pa == INDEX_NONE ? FQuat::Identity : LookPoseOf(Pose, Pa).GetRotation();
		Pose[I].SetRotation((P.Inverse() * Q * P * Pose[I].GetRotation()).GetNormalized());
	}
	float LookDeg(double R) { return float(FMath::RadiansToDegrees(R)); }

	/** Take the face's eyes over: only a face with eyes of its own and no animation of its own. */
	void TakeFace(AActor* Who, USkeletalMeshComponent* Body)
	{
		if (GEyes.ContainsByPredicate([Body](const FEyes& E) { return E.Body == Body; })) return;
		TInlineComponentArray<USkeletalMeshComponent*> Sk(Who);
		for (USkeletalMeshComponent* S : Sk)
		{
			if (S == Body || !S->GetName().StartsWith(TEXT("Face")) || !S->IsRegistered() || !S->GetSkeletalMeshAsset()) continue;
			if (S->GetBoneIndex(TEXT("FACIAL_L_Eye")) == INDEX_NONE || S->GetBoneIndex(TEXT("FACIAL_R_Eye")) == INDEX_NONE || S->LeaderPoseComponent.IsValid()) return;
			if (S->GetAnimInstance() || S->IsRunningParallelEvaluation() || S->IsPostEvaluatingAnimation()) return;   // (one of its own; or the next frame)
			const int32 Hd = Body->GetBoneIndex(TEXT("head"));
			if (Hd == INDEX_NONE || !Body->GetSkeletalMeshAsset()) return;
			S->SetAnimInstanceClass(UTalkFaceAnim::StaticClass());
			UTalkFaceAnim* A = Cast<UTalkFaceAnim>(S->GetAnimInstance());
			if (!A) return;
			FEyes E; E.Face = S; E.Body = Body; E.Anim = A;
			E.HeadRef = LookRefOf(Body->GetSkeletalMeshAsset()->GetRefSkeleton(), Hd).GetRotation();
			E.BlinkIn = FMath::FRandRange(0.15f, 1.2f);
			GEyes.Add(E);
			return;
		}
	}

	/** The face as it was: no animation of its own. False: not possible this moment. */
	bool GiveBack(FEyes& E)
	{
		USkeletalMeshComponent* S = E.Face.Get();
		if (!S || !E.Anim.IsValid() || S->GetAnimInstance() != E.Anim.Get()) return true;
		if (S->IsRunningParallelEvaluation() || S->IsPostEvaluatingAnimation()) return false;
		S->SetAnimInstanceClass(nullptr);
		return true;
	}

	/** Where an eye has to turn (degrees to their left, and up), from the head as it stands. */
	FVector2f EyeTo(const FQuat& HeadNow, const FVector& From, const FVector& At)
	{
		const FVector D = HeadNow.UnrotateVector(At - From).GetSafeNormal();
		return FVector2f(FMath::Clamp(LookDeg(FMath::Atan2(D.X, D.Y)), -EyeSide, EyeSide), FMath::Clamp(LookDeg(FMath::Asin(FMath::Clamp(D.Z, -1.0, 1.0))), -EyeDown, EyeUp));
	}

	void StepEyes(FEyes& E, float Dt)
	{
		USkeletalMeshComponent* S = E.Face.Get(); USkeletalMeshComponent* B = E.Body.Get(); UTalkFaceAnim* A = E.Anim.Get();
		if (!S || !B || !A) return;
		FVector EyePt = FVector::ZeroVector;
		const bool bOn = LookPtFor(B, EyePt);
		E.W = FMath::FInterpConstantTo(E.W, bOn ? 1.f : 0.f, Dt, bOn ? 1.f / 0.12f : 1.f / EaseTime);
		const FQuat Head = B->GetSocketTransform(TEXT("head")).GetRotation() * E.HeadRef.Inverse();
		// (nobody's eyes rest on one point: a very small move now and then)
		if ((E.DartIn -= Dt) <= 0.f) { E.DartIn = FMath::FRandRange(0.7f, 2.4f); E.Dart = FVector2f(FMath::FRandRange(-0.9f, 0.9f), FMath::FRandRange(-0.6f, 0.6f)); }
		static const FName Bn[2] = {TEXT("FACIAL_L_Eye"), TEXT("FACIAL_R_Eye")};
		const float K = 1.f - FMath::Exp(-Dt * 22.f);
		for (int32 I = 0; I < 2; ++I)
		{
			const FVector2f Want = bOn ? EyeTo(Head, S->GetSocketLocation(Bn[I]), EyePt) + E.Dart : FVector2f::ZeroVector;
			E.Eye[I] += (Want - E.Eye[I]) * K;
			A->Eye[I] = E.Eye[I] * E.W;
		}
		// the blink: 0.08 s shut, 0.04 held, 0.15 open; every 2 to 6 s, now and then twice
		float L = 0.f;
		if (E.BlinkT >= 0.f)
		{
			E.BlinkT += Dt;
			L = E.BlinkT < 0.08f ? E.BlinkT / 0.08f : E.BlinkT < 0.12f ? 1.f : FMath::Max(0.f, 1.f - (E.BlinkT - 0.12f) / 0.15f);
			if (E.BlinkT > 0.27f) { E.BlinkT = -1.f; E.bTwice = !E.bTwice && FMath::FRand() < 0.12f; E.BlinkIn = E.bTwice ? 0.06f : FMath::FRandRange(2.f, 6.f); }
		}
		else if (bOn && (E.BlinkIn -= Dt) <= 0.f) E.BlinkT = 0.f;
		A->Blink = FMath::SmoothStep(0.f, 1.f, L);
	}
}

// ------------------------------------------------------------------ the head

void FTalkLookLayer::Update(const USkeletalMeshComponent* Body, float Dt, bool bArms)
{
	FVector World;
	const bool bNow = ADPTalkLook::For(Body, World);
	if (!bNow && !bWant && W <= 0.f) return;
	if (bNow)
	{
		if (!bWant) Delay = HeadLag;
		At = Body->GetComponentTransform().InverseTransformPosition(World);
	}
	bWant = bNow; bArmsBusy = bArms; Step += Dt;
}

void FTalkLookLayer::Rig(const FReferenceSkeleton& RS)
{
	RigFor = &RS; HeadF = FVector::ZeroVector;
	static const FName Nm[4] = {TEXT("spine_05"), TEXT("neck_01"), TEXT("neck_02"), TEXT("head")};
	for (int32 I = 0; I < 4; ++I) Bone[I] = RS.FindBoneIndex(Nm[I]);
	const int32 Al = RS.FindBoneIndex(TEXT("upperarm_l")), Ar = RS.FindBoneIndex(TEXT("upperarm_r"));
	if (Bone[3] == INDEX_NONE || Al == INDEX_NONE || Ar == INDEX_NONE) return;
	// (the body's own left, front and up, from the pose it was made in)
	const FTransform Hd = LookRefOf(RS, Bone[3]);
	FVector L = LookRefOf(RS, Al).GetLocation() - LookRefOf(RS, Ar).GetLocation(); L.Z = 0.f;
	if (!L.Normalize()) return;
	BU = FVector(0, 0, 1); BL = L; BF = FVector::CrossProduct(BU, BL);
	const int32 Ft = RS.FindBoneIndex(TEXT("foot_l")), Tb = RS.FindBoneIndex(TEXT("ball_l"));
	const FVector Toe = Ft != INDEX_NONE && Tb != INDEX_NONE ? LookRefOf(RS, Tb).GetLocation() - LookRefOf(RS, Ft).GetLocation() : FVector(0, 1, 0);
	if (FVector::DotProduct(Toe, BF) < 0.f) BF = -BF;
	HeadF = Hd.GetRotation().UnrotateVector(BF);
	EyeAt = Hd.GetRotation().UnrotateVector(BF * 8.5f + BU * 6.f);
}

void FTalkLookLayer::Turn(FCompactPose& Pose)
{
	const FBoneContainer& BC = Pose.GetBoneContainer();
	if (RigFor != &BC.GetReferenceSkeleton()) Rig(BC.GetReferenceSkeleton());
	float Dt = FMath::Min(Step, 0.1f); Step = 0.f;
	FCompactPoseBoneIndex Ix[4] = {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)};
	for (int32 I = 0; I < 4; ++I) if (Bone[I] != INDEX_NONE) Ix[I] = BC.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bone[I]));
	if (HeadF.IsNearlyZero() || Ix[3] == INDEX_NONE) { W = 0.f; bBase = false; return; }   // (no head to turn, or a low LOD without it)
	// where the pose itself looks: followed slowly, so breathing and swaying stay on the head (the eyes make up for them)
	const FTransform Hd = LookPoseOf(Pose, Ix[3]);
	const FVector F = Hd.GetRotation().RotateVector(HeadF);
	auto YawOf = [&](const FVector& V) { return LookDeg(FMath::Atan2(FVector::DotProduct(V, BL), FVector::DotProduct(V, BF))); };
	auto PitchOf = [&](const FVector& V) { return LookDeg(FMath::Asin(FMath::Clamp(-FVector::DotProduct(V, BU), -1.0, 1.0))); };   // (+ down)
	const float Py = YawOf(F), Pp = PitchOf(F);
	if (!bBase) { BaseY = Py; BaseP = Pp; }
	else { const float K = 1.f - FMath::Exp(-Dt * 1.2f); BaseY += FRotator::NormalizeAxis(Py - BaseY) * K; BaseP += (Pp - BaseP) * K; }
	if (bWant)
	{
		const FVector D = (At - (Hd.GetLocation() + Hd.GetRotation().RotateVector(EyeAt))).GetSafeNormal();
		float Ty = YawOf(D); const float Tp = FMath::Clamp(PitchOf(D), -HeadUp, HeadDown);
		// (behind them: the side they already look to)
		if (bBase && FMath::Abs(Ty) > 120.f && FMath::Abs(BaseY + Yaw) > 5.f) Ty = FMath::Abs(Ty) * FMath::Sign(BaseY + Yaw);
		Ty = FMath::Clamp(Ty, -HeadSide, HeadSide);
		// (the head leaves the last few degrees to the eyes, as people do)
		float Wy = FRotator::NormalizeAxis(Ty - BaseY), Wp = Tp - BaseP;
		Wy -= FMath::Clamp(Wy * 0.12f, -5.f, 5.f); Wp -= FMath::Clamp(Wp * 0.12f, -3.f, 3.f);
		if (!bBase) { Yaw = Wy; Pitch = Wp; }   // (the weight brings it in)
		else
		{
			const float K = 1.f - FMath::Exp(-Dt * 6.f), Most = 150.f * Dt;   // (never a snap)
			Yaw += FMath::Clamp((Wy - Yaw) * K, -Most, Most); Pitch += FMath::Clamp((Wp - Pitch) * K, -Most, Most);
		}
		bBase = true;
		if (Delay > 0.f) { const float Use = FMath::Min(Delay, Dt); Delay -= Use; Dt -= Use; }
	}
	W = FMath::FInterpConstantTo(W, bWant ? 1.f : 0.f, Dt, 1.f / EaseTime);
	SpineW = FMath::FInterpConstantTo(SpineW, bArmsBusy ? 0.f : 1.f, Dt, 1.f / EaseTime);
	if (W <= 0.f) { if (!bWant) bBase = false; return; }
	const float S = FMath::InterpEaseInOut(0.f, 1.f, W, 2.f), Y = Yaw * S, P = Pitch * S;
	// the shares: the upper spine a little, the neck, the head the most
	float Sh[4] = {0.14f * SpineW, 0.24f, 0.24f, 0.38f}, Sum = 0.f;
	for (int32 I = 0; I < 4; ++I) { if (Ix[I] == INDEX_NONE) Sh[I] = 0.f; Sum += Sh[I]; }
	const FVector YawAx = FVector::CrossProduct(BF, BL).GetSafeNormal();
	float Done = 0.f;
	for (int32 I = 0; I < 4; ++I)
	{
		if (Sh[I] <= 0.f) continue;
		const float Part = Sh[I] / Sum;
		Done += Y * Part;
		// the nod about the head's own side, wherever it has turned to by now
		const FVector Fw = FQuat(YawAx, FMath::DegreesToRadians(BaseY + Done)).RotateVector(BF);
		LookTurnBone(Pose, Ix[I], FQuat(FVector::CrossProduct(Fw, -BU).GetSafeNormal(), FMath::DegreesToRadians(P * Part)) * FQuat(YawAx, FMath::DegreesToRadians(Y * Part)));
	}
}

// ------------------------------------------------------------------ the eyes

void FTalkFaceProxy::PreUpdate(UAnimInstance* In, float Dt)
{
	FAnimInstanceProxy::PreUpdate(In, Dt);
	if (const UTalkFaceAnim* A = Cast<UTalkFaceAnim>(In)) { Eye[0] = A->Eye[0]; Eye[1] = A->Eye[1]; Blink = A->Blink; }
}

bool FTalkFaceProxy::Evaluate(FPoseContext& Output)
{
	Output.ResetToRefPose();   // (the body's pose is copied on by the face's post process, as always)
	static const FName Nm[2][5] = {
		{TEXT("CTRL_expressions_eyeLookLeftL"), TEXT("CTRL_expressions_eyeLookRightL"), TEXT("CTRL_expressions_eyeLookUpL"), TEXT("CTRL_expressions_eyeLookDownL"), TEXT("CTRL_expressions_eyeBlinkL")},
		{TEXT("CTRL_expressions_eyeLookLeftR"), TEXT("CTRL_expressions_eyeLookRightR"), TEXT("CTRL_expressions_eyeLookUpR"), TEXT("CTRL_expressions_eyeLookDownR"), TEXT("CTRL_expressions_eyeBlinkR")}};
	for (int32 I = 0; I < 2; ++I)
	{
		const float V[5] = {Eye[I].X / (I == 0 ? FullOut : FullIn), -Eye[I].X / (I == 0 ? FullIn : FullOut), Eye[I].Y / FullUp, -Eye[I].Y / FullDown, Blink};
		for (int32 K = 0; K < 5; ++K) Output.Curve.Set(Nm[I][K], FMath::Clamp(V[K], 0.f, 1.f));   // (always all ten)
	}
	return true;
}

// ------------------------------------------------------------------ the talk

namespace ADPTalkLook
{
void Begin(AActor* Who, USkeletalMeshComponent* Body)
{
	if (!Who || !Body) return;
	if (GBody != Body) bGAt = false;
	GWho = Who; GBody = Body;
}

void End() { GWho = nullptr; GBody = nullptr; bGAt = false; }

void Drop()
{
	End();
	GPairs.Reset();
	for (FEyes& E : GEyes) GiveBack(E);
	GEyes.Reset();
}

bool For(const USkeletalMeshComponent* Body, FVector& OutAt)
{
	return LookPtFor(Body, OutAt);
}

void Pair(AActor* Who, USkeletalMeshComponent* Body, AActor* At, USkeletalMeshComponent* AtBody)
{
	if (!Who || !Body || !At || !AtBody || Who == At) return;
	for (FPairLook& P : GPairs)
		if (P.Who.Get() == Who) { P.Body = Body; P.At = At; P.AtBody = AtBody; return; }
	if (GPairs.Num() >= 8) return;   // (never more than a handful: NpcChat caps the conversations)
	FPairLook P; P.Who = Who; P.Body = Body; P.At = At; P.AtBody = AtBody;
	GPairs.Add(P);
}

void Unpair(const AActor* Who)
{
	for (int32 I = GPairs.Num() - 1; I >= 0; --I)
		if (!GPairs[I].Who.IsValid() || GPairs[I].Who.Get() == Who) GPairs.RemoveAtSwap(I);
}

void Tick(UWorld* W, float Dt)
{
	if (GEyes.Num() == 0 && !GBody.IsValid() && GPairs.Num() == 0) { if (bGAt) End(); return; }
	ADP_PERF_SCOPE("talk: their head and eyes to yours", 1.f);
	if (!GWho.IsValid() || !GBody.IsValid() || GWho->IsHidden()) End();
	if (USkeletalMeshComponent* B = GBody.Get())
	{
		const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			// the talk camera as far as it is in, else your own eyes (never the camera behind your back)
			const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
			const FVector Me = PC->GetPawn() ? PC->GetPawn()->GetPawnViewLocation() : Cam;
			GAt = FMath::Lerp(Me, Cam, double(FMath::Clamp(ADPTalkCam::Amount(), 0.f, 1.f)));
			bGAt = true;
			TakeFace(GWho.Get(), B);
		}
		else bGAt = false;
	}
	// the conversations between people: each one's point is the other one's head (the one you talk to keeps you)
	for (int32 I = GPairs.Num() - 1; I >= 0; --I)
	{
		FPairLook& P = GPairs[I];
		AActor* Who = P.Who.Get(); USkeletalMeshComponent* Body = P.Body.Get();
		const AActor* At = P.At.Get(); const USkeletalMeshComponent* AtBody = P.AtBody.Get();
		if (!Who || !Body || !At || !AtBody || Who->IsHidden() || At->IsHidden()) { GPairs.RemoveAtSwap(I); continue; }
		if (Body == GBody.Get()) { P.bPoint = false; continue; }   // (you are talking to this one)
		P.Point = AtBody->DoesSocketExist(TEXT("head")) ? AtBody->GetSocketLocation(TEXT("head")) : At->GetActorLocation() + FVector(0.f, 0.f, 160.f);
		P.bPoint = true;
		TakeFace(Who, Body);
	}
	for (int32 I = GEyes.Num() - 1; I >= 0; --I)
	{
		FEyes& E = GEyes[I];
		if (!E.Face.IsValid() || !E.Body.IsValid() || !E.Anim.IsValid() || E.Face->GetAnimInstance() != E.Anim.Get()) { GEyes.RemoveAtSwap(I); continue; }
		StepEyes(E, Dt);
		FVector Pt; const bool bKeep = GBody == E.Body || LookPtFor(E.Body.Get(), Pt);
		if (!bKeep && E.W <= 0.f && E.BlinkT < 0.f && GiveBack(E)) GEyes.RemoveAtSwap(I);
	}
}
}

#if !UE_BUILD_SHIPPING
// What the head and the eyes really do, in degrees, into the log - no game needed (the headless editor).
static FAutoConsoleCommandWithWorldAndArgs CmdTalkLookTest(TEXT("adp.TalkLookTest"), TEXT("adp.TalkLookTest <built person>: measures how far the eyes turn at the eyes' controls, and the head to a point"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
{
	using namespace ADPTalkLook;
	const FString N = Args.Num() ? Args[0] : TEXT("MH_000_passenger");
	UClass* C = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Game/People/Built/%s/BP_%s.BP_%s_C"), *N, *N, *N));
	AActor* A = W && C ? W->SpawnActor<AActor>(C, FVector::ZeroVector, FRotator::ZeroRotator) : nullptr;
	if (!A) { ADP_WARN(TEXT("TalkLookTest: no %s"), *N); return; }
	USkeletalMeshComponent* B = nullptr; USkeletalMeshComponent* F = nullptr;
	TInlineComponentArray<USkeletalMeshComponent*> Sk(A);
	for (USkeletalMeshComponent* S : Sk) { if (S->GetName().StartsWith(TEXT("Body"))) B = S; else if (S->GetName().StartsWith(TEXT("Face"))) F = S; }
	if (B && F)
	{
		B->SetAnimInstanceClass(UPaxAnimInstance::StaticClass());
		auto Pose = [](USkeletalMeshComponent* S, float Dt) { S->TickAnimation(Dt, false); S->RefreshBoneTransforms(); };
		const FQuat HeadRef = LookRefOf(B->GetSkeletalMeshAsset()->GetRefSkeleton(), B->GetBoneIndex(TEXT("head"))).GetRotation();
		const FReferenceSkeleton& FR = F->GetSkeletalMeshAsset()->GetRefSkeleton();
		const FName En[2] = {TEXT("FACIAL_L_Eye"), TEXT("FACIAL_R_Eye")};
		const FQuat Ref[2] = {LookRefOf(FR, F->GetBoneIndex(En[0])).GetRotation(), LookRefOf(FR, F->GetBoneIndex(En[1])).GetRotation()};
		auto Dir = [&](int32 I) { return (F->GetSocketTransform(En[I]).GetRotation() * Ref[I].Inverse()).RotateVector(FVector(0, 1, 0)); };
		auto HeadQ = [&]() { return B->GetSocketTransform(TEXT("head")).GetRotation() * HeadRef.Inverse(); };
		Pose(B, 0.03f); Pose(F, 0.03f);
		F->SetAnimInstanceClass(UTalkFaceAnim::StaticClass());
		UTalkFaceAnim* T = Cast<UTalkFaceAnim>(F->GetAnimInstance());
		const FVector2f Try[] = {{0, 0}, {10, 0}, {-10, 0}, {20, 0}, {40, 0}, {-40, 0}, {0, 10}, {0, -10}, {0, 18}, {0, -22}, {0, 30}, {0, -30}};
		for (const FVector2f& V : Try)
		{
			if (T) T->Eye[0] = T->Eye[1] = V;
			Pose(B, 0.03f); Pose(F, 0.03f);
			FString O;
			for (int32 I = 0; I < 2; ++I) { const FVector D = HeadQ().UnrotateVector(Dir(I)); O += FString::Printf(TEXT(" %s %.1f %.1f"), I ? TEXT("R") : TEXT("L"), LookDeg(FMath::Atan2(D.X, D.Y)), LookDeg(FMath::Asin(D.Z))); }
			ADP_LOG(TEXT("TalkLookTest: eyes asked %.0f %.0f, are%s"), V.X, V.Y, *O);
		}
		F->SetAnimInstanceClass(nullptr);
		Pose(F, 0.03f);
		ADP_LOG(TEXT("TalkLookTest: the face's animation given back: %s"), F->GetAnimInstance() ? TEXT("NO") : TEXT("yes"));
		auto Err = [&](int32 I) { return LookDeg(FMath::Acos(FMath::Clamp(FVector::DotProduct(Dir(I), (GAt - F->GetSocketLocation(En[I])).GetSafeNormal()), -1.0, 1.0))); };
		const FVector From = B->GetSocketLocation(TEXT("head"));
		const FVector To[] = {{30, 80, 10}, {-60, 60, -5}, {90, -10, 0}, {0, 70, 60}, {20, 80, -60}};
		for (const FVector& P : To)
		{
			Begin(A, B); GAt = From + P; bGAt = true; TakeFace(A, B);
			FQuat Was = HeadQ(); float Jump = 0.f;
			for (int32 K = 0; K < 60; ++K)
			{
				Pose(B, 0.033f); for (FEyes& E : GEyes) StepEyes(E, 0.033f); Pose(F, 0.033f);
				const FQuat Now = HeadQ(); Jump = FMath::Max(Jump, LookDeg(Now.AngularDistance(Was))); Was = Now;
			}
			const FVector Hf = HeadQ().RotateVector(FVector(0, 1, 0));
			ADP_LOG(TEXT("TalkLookTest: point %s: head side %.1f up %.1f (largest step %.1f), eyes miss it by L %.1f R %.1f"), *P.ToCompactString(), LookDeg(FMath::Atan2(Hf.X, Hf.Y)), LookDeg(FMath::Asin(Hf.Z)), Jump, Err(0), Err(1));
		}
		End();
		for (int32 K = 0; K < 30; ++K) { Pose(B, 0.033f); for (FEyes& E : GEyes) StepEyes(E, 0.033f); Pose(F, 0.033f); }
		const FVector Hf = HeadQ().RotateVector(FVector(0, 1, 0));
		ADP_LOG(TEXT("TalkLookTest: after: head side %.1f up %.1f"), LookDeg(FMath::Atan2(Hf.X, Hf.Y)), LookDeg(FMath::Asin(Hf.Z)));
		Drop();
	}
	A->Destroy();
}));
#endif
