// A road vehicle's driver, passengers and driving (VehicleRiders.h) - all of it behind adp.CoachLine, 0 by default.
#include "VehicleRiders.h"
#include "ADPLog.h"
#include "CrewPose.h"
#include "DriverPoseAnim.h"
#include "PaxAnimInstance.h"
#include "People.h"
#include "RoadNetwork.h"
#include "Seats.h"
#include "Site.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Algo/Reverse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static TAutoConsoleVariable<int32> CVarCoachLine(TEXT("adp.CoachLine"), 0,
	TEXT("The airport coaches' driver, passengers and driving: 0 (default) off - the coaches stay parked and nobody boards them; 1 the features may be used."));

namespace VehRide
{
	const FVector VrX(1, 0, 0), VrY(0, 1, 0), VrZ(0, 0, 1);
	FVector VrFromBlender(const FVector& V) { return FVector(V.X * 100.f, -V.Y * 100.f, V.Z * 100.f); }
	FVector VrTurned(float YawDeg) { return FRotator(0.f, YawDeg, 0.f).Vector(); }
	float VrSm(float K) { return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(K, 0.f, 1.f)); }
	const FQuat VrUpright(FVector::XAxisVector, -HALF_PI);   // (a bag lying on its back stood up, its end handle on top)
	const FVector VrBagHandle(0.f, -22.f, 14.4f);            // (SM_Bag1's own: DepartureBelts.cpp GRIP)
	UBlendSpace* VrMoveBS()
	{
		static UBlendSpace* B = [] { UBlendSpace* S = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/People/Anims/BS_Idle_Walk_Run_MH.BS_Idle_Walk_Run_MH")); if (S) S->AddToRoot(); return S; }();
		return B;
	}
	// a hand's plain hanging place beside the hip
	FVector VrHang(const FDriverPose& P, int32 s) { return P.Pelvis + ADPPose::LeftOf(P.Facing) * (s == 0 ? 22.f : -22.f) - VrZ * 15.f; }
	// standing at A moving to B, the body facing one way: the leading foot first, the other after it
	FDriverPose VrSlide(const FVector& A, const FVector& B, const FVector& Face, float K, float PH)
	{
		const FDriverPose Pa = ADPPose::Standing(A, Face, PH), Pb = ADPPose::Standing(B, Face, PH);
		FDriverPose P = ADPPose::Lerp(Pa, Pb, VrSm(K));
		const int32 Lead = FVector::DotProduct(B - A, ADPPose::LeftOf(Face)) >= 0.f ? 0 : 1;
		for (int32 s = 0; s < 2; ++s)
		{
			const float F = FMath::Clamp(s == Lead ? K * 2.f : K * 2.f - 1.f, 0.f, 1.f);
			P.Foot[s] = FMath::Lerp(Pa.Foot[s], Pb.Foot[s], VrSm(F)) + VrZ * (5.f * FMath::Sin(PI * F));
			P.bHand[s] = false;
		}
		P.Pelvis.Z -= 2.f;
		return P;
	}
	// standing on one spot, turning from one way to another
	FDriverPose VrTurnOn(const FVector& At, float Y0, float Y1, float K, float PH)
	{
		const float D = FMath::FindDeltaAngleDegrees(Y0, Y1), Sk = VrSm(K);
		FDriverPose P = ADPPose::Standing(At, VrTurned(Y0 + D * Sk), PH);
		const int32 Lead = D < 0.f ? 0 : 1;
		for (int32 s = 0; s < 2; ++s)
		{
			const float F = FMath::Clamp(s == Lead ? K * 2.f : K * 2.f - 1.f, 0.f, 1.f);
			const FVector L0 = ADPPose::LeftOf(VrTurned(Y0)), L1 = ADPPose::LeftOf(VrTurned(Y1));
			P.Foot[s] = FMath::Lerp(At + L0 * (s == 0 ? 12.f : -12.f), At + L1 * (s == 0 ? 12.f : -12.f), VrSm(F)) + VrZ * (5.f * FMath::Sin(PI * F));
		}
		P.Pelvis.Z -= 2.f;
		return P;
	}
}
using namespace VehRide;

// ------------------------------------------------------------------------------------------------ the points, the lane
bool FVehicleSpec::Load(const FString& Name)
{
	FString Text; TSharedPtr<FJsonObject> J;
	const FString File = FString::Printf(TEXT("Data/%s-riders.json"), *Name.ToLower());
	if (!(FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / File)) || FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / File)))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J) { ADP_WARN(TEXT("Riders: no %s"), *File); return false; }
	auto Vec = [](const TArray<TSharedPtr<FJsonValue>>& A) { return A.Num() >= 3 ? VrFromBlender(FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber())) : FVector::ZeroVector; };
	const TSharedPtr<FJsonObject>* O = nullptr;
	if (J->TryGetObjectField(TEXT("points"), O)) for (const auto& E : (*O)->Values) P.Add(FString(E.Key), Vec(E.Value->AsArray()));
	if (J->TryGetObjectField(TEXT("lists"), O)) for (const auto& E : (*O)->Values) { TArray<FVector> A; for (const auto& V : E.Value->AsArray()) A.Add(Vec(V->AsArray())); L.Add(FString(E.Key), MoveTemp(A)); }
	if (J->TryGetObjectField(TEXT("numbers"), O)) for (const auto& E : (*O)->Values) N.Add(FString(E.Key), float(E.Value->AsNumber()));
	return P.Num() > 0;
}

bool FVehicleLane::FromRoad(const FString& Road, bool bRight, int32 Lane, FVehicleLane& Out)
{
	FString Text; TSharedPtr<FJsonObject> J; const TArray<TSharedPtr<FJsonValue>>* Rs = nullptr;
	if (!FFileHelper::LoadFileToString(Text, *ADPSite::File(TEXT("roads.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J || !J->TryGetArrayField(TEXT("roads"), Rs)) return false;
	for (const auto& V : *Rs)
	{
		const TSharedPtr<FJsonObject> R = V->AsObject();
		FString Id; if (!R || !R->TryGetStringField(TEXT("id"), Id) || Id != Road) continue;
		TArray<FVector> Mid; for (const auto& Q : R->GetArrayField(TEXT("pts"))) { const auto& A = Q->AsArray(); Mid.Add(FVector(A[0]->AsNumber() * 100.0, -A[1]->AsNumber() * 100.0, 0.0)); }
		const TSharedPtr<FJsonObject>* Ls = nullptr; const TArray<TSharedPtr<FJsonValue>>* Side = nullptr;
		if (!R->TryGetObjectField(TEXT("lanes"), Ls) || !(*Ls)->TryGetArrayField(bRight ? TEXT("right") : TEXT("left"), Side) || !Side->IsValidIndex(Lane)) return false;
		auto Width = [&](int32 K, double D)
		{
			const TSharedPtr<FJsonValue> W = (*Side)[K]->AsObject()->TryGetField(TEXT("w"));
			if (!W) return 0.0;
			if (W->Type == EJson::Number) return W->AsNumber();
			const auto& A = W->AsArray(); double Was = A[0]->AsArray()[1]->AsNumber(), At = A[0]->AsArray()[0]->AsNumber();
			for (const auto& E : A)
			{
				const double D1 = E->AsArray()[0]->AsNumber(), W1 = E->AsArray()[1]->AsNumber();
				if (D <= D1) return D1 > At ? FMath::Lerp(Was, W1, (D - At) / (D1 - At)) : W1;
				At = D1; Was = W1;
			}
			return Was;
		};
		double D = 0.0; Out.Pts.Reset();
		for (int32 k = 0; k + 1 < Mid.Num(); ++k)
		{
			const double Len = FVector::Dist2D(Mid[k], Mid[k + 1]) / 100.0; const FVector Fw = (Mid[k + 1] - Mid[k]).GetSafeNormal2D(), Rt(-Fw.Y, Fw.X, 0.0);
			for (double d = 0.0; d < Len; d += 2.0)
			{
				double Off = Width(Lane, D + d) / 2.0; for (int32 i = 0; i < Lane; ++i) Off += Width(i, D + d);
				Out.Pts.Add(Mid[k] + Fw * (d * 100.0) + Rt * ((bRight ? 1.0 : -1.0) * Off * 100.0));
			}
			D += Len;
		}
		bool bInv = false; (*Side)[Lane]->AsObject()->TryGetBoolField(TEXT("invert"), bInv);
		if (bInv) Algo::Reverse(Out.Pts);
		return Out.Pts.Num() > 1;
	}
	return false;
}

TArray<FVector> FVehicleLane::PullOut(const FVector& At, const FVector& Dir, float Length, float Run) const
{
	TArray<FVector> Out;
	if (Pts.Num() < 2) return Out;
	const FVector D = Dir.GetSafeNormal2D();
	// the lane's point beside Length cm ahead
	const FVector Want = At + D * Length;
	int32 Best = 0; double Bd = 1e18;
	for (int32 i = 0; i < Pts.Num(); ++i) { const double X = FVector::DistSquared2D(Pts[i], Want); if (X < Bd) { Bd = X; Best = i; } }
	const FVector To(Pts[Best].X, Pts[Best].Y, At.Z);
	const float Ahead = FMath::Max(300.f, float(FVector::DotProduct(To - At, D))), Off = float(FVector::DotProduct(To - At, FVector(-D.Y, D.X, 0.f)));
	for (float s = 0.f; s <= Ahead; s += 25.f) { const float t = s / Ahead; Out.Add(At + D * s + FVector(-D.Y, D.X, 0.f) * (Off * t * t * (3.f - 2.f * t))); }
	float Left = Run;
	for (int32 i = Best + 1; i < Pts.Num() && Left > 0.f; ++i)
	{
		FVector Q(Pts[i].X, Pts[i].Y, At.Z); const FVector Was = Out.Last(); float L = float(FVector::Dist2D(Was, Q));
		if (L < 1.f || FVector::DotProduct(Q - Was, D) < 0.f) continue;
		if (L > Left) { Q = FMath::Lerp(Was, Q, Left / L); L = Left; }
		for (float s = 50.f; s < L; s += 50.f) Out.Add(FMath::Lerp(Was, Q, s / L));
		Out.Add(Q); Left -= L;
	}
	return Out;
}

// ------------------------------------------------------------------------------------------------ the component
UVehicleRiders::UVehicleRiders() { PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.TickGroup = TG_PrePhysics; }
UVehicleRiders::~UVehicleRiders() {}

bool UVehicleRiders::LineOn()
{
	static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("CoachIKTest"));
	return bTest || CVarCoachLine.GetValueOnGameThread() != 0;
}

UVehicleRiders* UVehicleRiders::Make(AActor* Vehicle, const FString& SpecName, TUniquePtr<IVehicleBody> InBody)
{
	if (!LineOn() || !Vehicle || !InBody) return nullptr;
	FVehicleSpec S; if (!S.Load(SpecName)) return nullptr;
	UVehicleRiders* R = NewObject<UVehicleRiders>(Vehicle);
	R->Spec = MoveTemp(S); R->Body = MoveTemp(InBody);
	R->State.Road = R->Body->Road(); R->Parked = R->State;
	R->RegisterComponent(); Vehicle->AddInstanceComponent(R);
	ADP_LOG(TEXT("Riders: %s ready for its driver and passengers (%d points)"), *SpecName, R->Spec.P.Num());
	return R;
}

UVehicleRiders::FRider* UVehicleRiders::Find(const AActor* Who) const
{
	for (const TSharedPtr<FRider>& R : Riders) if (R->A.Get() == Who && Who) return R.Get();
	return nullptr;
}

UVehicleRiders::FRider* UVehicleRiders::Join(AActor* Who)
{
	if (!LineOn() || !Body || !Who || Cast<APawn>(Who)) return nullptr;   // (off: nobody; never the player)
	if (FRider* Have = Find(Who)) return Have;
	AADPPeople* P = AADPPeople::Get(GetWorld()); AADPPeople::FWho* W = P ? P->Find(Who) : nullptr;
	if (!W || !W->Body) return nullptr;
	TSharedPtr<FRider> R = MakeShared<FRider>(); R->A = Who; R->Skel = W->Body;
	R->SitKind = int32(GetTypeHash(W->Name) % 5u);
	Riders.Add(R);
	return R.Get();
}

bool UVehicleRiders::IsSeated(const AActor* W) const { const FRider* R = Find(W); return R && R->bSeated; }
bool UVehicleRiders::IsBusy(const AActor* W) const { const FRider* R = Find(W); return R && R->Q.Num() > 0; }
bool UVehicleRiders::IsAboard(const AActor* W) const { const FRider* R = Find(W); return R && R->bAboard; }

float UVehicleRiders::GroundZ(const FVector& W) const
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(RidersGround), true);
	Q.AddIgnoredActor(Body->Actor());
	for (const TSharedPtr<FRider>& R : Riders) if (R->A.IsValid()) Q.AddIgnoredActor(R->A.Get());
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (PC->GetPawn()) Q.AddIgnoredActor(PC->GetPawn());
	FHitResult H;
	const double Z = Body->Road().GetLocation().Z;
	return GetWorld()->LineTraceSingleByChannel(H, FVector(W.X, W.Y, Z + 160.0), FVector(W.X, W.Y, Z - 400.0), ECC_Visibility, Q) ? float(H.ImpactPoint.Z) : float(Z);
}

FVector UVehicleRiders::GroundUnder(const FVector& L) const
{
	FVector W = Wo(FVector(L.X, L.Y, 0.f)); W.Z = GroundZ(W);
	return Lo(W);
}

FDriverPose UVehicleRiders::StandAt(const FVector& Local, const FVector& Face, float PH, bool bOutside) const
{
	return ADPPose::Standing(bOutside ? GroundUnder(Local) : Local, Face, PH);
}

// ------------------------------------------------------------------------------------------------ posing, walking
void UVehicleRiders::Unpose(FRider& R)
{
	if (!R.bPosed) return;
	R.bPosed = false;
	if (R.bLast)
	{
		R.At = FVector(R.Last.Pelvis.X, R.Last.Pelvis.Y, FMath::Min(R.Last.Foot[0].Z, R.Last.Foot[1].Z)); R.Yaw = float(R.Last.Facing.Rotation().Yaw);
		if (AActor* A = R.A.Get())
		{
			const FVector W = Wo(R.At), F = Body->Body().TransformVectorNoScale(R.Last.Facing);
			if (R.bAboard) A->SetActorTransform(FTransform(FRotator(0.f, R.Yaw - 90.f, 0.f), R.At) * Body->Body());
			else A->SetActorLocationAndRotation(W, FRotator(0.f, F.Rotation().Yaw - 90.f, 0.f));
		}
	}
	if (R.Skel)
	{
		R.Skel->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		if (VrMoveBS()) { R.Skel->SetAnimation(VrMoveBS()); R.Skel->Play(true); }
	}
	R.Anim = nullptr; R.Speed = 0.f; R.bLast = false;
}

void UVehicleRiders::ApplyPose(FRider& R, const FPoseFn& Fn, float K, float Dt)
{
	AActor* A = R.A.Get();
	if (!A || !R.Skel || !Fn) return;
	if (!R.Rig)
	{
		R.Rig = new FTruckDriverRig();
		R.Mesh = NewObject<UPoseableMeshComponent>(A); R.Mesh->SetupAttachment(A->GetRootComponent()); R.Mesh->SetVisibility(false); R.Mesh->RegisterComponent();
		R.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		R.Mesh->SetSkinnedAssetAndUpdate(R.Skel->GetSkeletalMeshAsset());
		Keep.Add(R.Mesh);
		if (!R.Rig->Init(R.Mesh)) { delete R.Rig; R.Rig = nullptr; return; }
	}
	const float PH = R.Rig->PelvisHeight();
	if (!R.bPosed)
	{
		R.BodyInActor = R.Skel->GetComponentTransform().GetRelativeTransform(A->GetActorTransform());
		const FVector F = Body->Body().InverseTransformVectorNoScale(FRotator(0.f, A->GetActorRotation().Yaw + 90.f, 0.f).Vector());
		R.From = ADPPose::Standing(Lo(A->GetActorLocation()), F, PH); R.bFrom = true; R.BlendT = 0.f;
		R.Skel->SetAnimInstanceClass(UDriverPoseAnim::StaticClass());
		R.Anim = Cast<UDriverPoseAnim>(R.Skel->GetAnimInstance());
		R.bPosed = true;
	}
	R.RestW = 0.f;
	FDriverPose P = Fn(FMath::Clamp(K, 0.f, 1.f), PH);
	if (R.bFrom && R.BlendT < 1.f) { R.BlendT = FMath::Min(1.f, R.BlendT + Dt / 0.4f); P = ADPPose::Lerp(R.From, P, VrSm(R.BlendT)); }
	R.Last = P; R.bLast = true;
	FVector Fw = P.Facing.GetSafeNormal2D(); if (Fw.IsNearlyZero()) Fw = VrX;
	const FTransform FrameL(FRotator(0.f, Fw.Rotation().Yaw - 90.f, 0.f), FVector(P.Pelvis.X, P.Pelvis.Y, FMath::Min(P.Foot[0].Z, P.Foot[1].Z)));
	FDriverPose L = P; L.Facing = VrY; L.Pelvis = FrameL.InverseTransformPosition(P.Pelvis);
	for (int32 s = 0; s < 2; ++s) { L.Foot[s] = FrameL.InverseTransformPosition(P.Foot[s]); L.Hand[s] = FrameL.InverseTransformPosition(P.Hand[s]); }
	const FTransform FrameW = FrameL * Body->Body();
	R.Rig->Pose(L);
	if (R.RestW > 0.f)
	{
		const float Th = R.Rig->ThighRadius();
		if (R.SitKind == 4 && R.RestW >= 1.f)   // (legs crossed: the foot lifted until the thighs lie on, not in, each other)
			for (int32 Pass = 0; Pass < 5; ++Pass)
			{
				float D = 1e9f;
				for (int32 i = 0; i <= 12; ++i) D = FMath::Min(D, float(FMath::PointDistToSegment(FMath::Lerp(R.Rig->HipAt(1), R.Rig->KneeAt(1), i / 12.f), R.Rig->HipAt(0), R.Rig->KneeAt(0))));
				const float Need = 2.f * Th * 0.95f;
				if (D >= Need || R.CrossLift > 22.f) break;
				R.CrossLift += (Need - D) * 1.5f; L.Foot[0].Z += (Need - D) * 1.5f; R.Rig->Pose(L);
			}
		// the seat under the thighs: forward a little at a time, then up, until they lie on the cushion (People's own step)
		if (R.RestW >= 1.f && !R.bCushionOk)
		{
			R.Deep = R.Rig->ThighsInSeat(GetWorld(), FrameW, Th, A);
			if (R.Deep <= 0.4f || ++R.Tries > 12) R.bCushionOk = true;
			else if (R.Fwd < 8.f) R.Fwd += 2.f; else if (R.Lift < 6.f) R.Lift = FMath::Min(6.f, R.Lift + R.Deep); else R.bCushionOk = true;
		}
		R.Rig->RestHandsOnThighs(L, Th, R.RestW);
	}
	if (R.Anim) R.Anim->Local = R.Rig->LocalPose();
	A->SetActorTransform(R.BodyInActor.Inverse() * FrameW);
}

void UVehicleRiders::WalkAnim(FRider& R, float Speed)
{
	if (!R.Skel) return;
	const bool bGrip = R.BagState == 1 && R.BagBy == R.A;
	UPaxAnimInstance* Mine = Cast<UPaxAnimInstance>(R.Skel->GetAnimInstance());
	if (!Mine && bGrip && !R.Skel->IsRunningParallelEvaluation() && !R.Skel->IsPostEvaluatingAnimation()) Mine = UPaxAnimInstance::Install(R.Skel);
	if (Mine) Mine->bGripRight = bGrip;
	if (UAnimSingleNodeInstance* One = R.Skel->GetSingleNodeInstance())
	{
		One->SetBlendSpacePosition(FVector(0.f, FMath::Min(Speed * 1.5f, 300.f), 0.f));
		One->SetPlayRate(Speed > 20.f ? FMath::Clamp(Speed / FMath::Max(Speed * 1.5f, 100.f), 0.5f, 1.4f) : 1.f);
	}
}

bool UVehicleRiders::WalkWorld(FRider& R, const FVector& To, float Dt, float Max)
{
	AActor* A = R.A.Get(); if (!A) return true;
	FVector D = To - A->GetActorLocation(); D.Z = 0.f;
	const float Dist = float(D.Size()); const bool bThere = Dist < 6.f;
	R.Speed = FMath::FInterpTo(R.Speed, bThere ? 0.f : FMath::Min(Max, FMath::Max(45.f, Dist * 1.6f)), Dt, 4.f);
	if (!bThere)
	{
		FVector P = A->GetActorLocation() + D.GetSafeNormal() * FMath::Min(Dist, R.Speed * Dt);
		P.Z = FMath::FInterpTo(A->GetActorLocation().Z, double(GroundZ(P)), Dt, 18.f);
		A->SetActorLocationAndRotation(P, FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, D.Rotation().Yaw - 90.f, 0.f), Dt, 7.f));
	}
	WalkAnim(R, R.Speed);
	return bThere;
}

bool UVehicleRiders::WalkAisle(FRider& R, FAct& Ac, float Dt)
{
	if (!Ac.Pts.Num()) { R.Speed = 0.f; WalkAnim(R, 0.f); return true; }
	FVector D = Ac.Pts[0] - R.At; const float Dist = float(D.Size2D());
	R.Speed = FMath::FInterpTo(R.Speed, Ac.Pts.Num() > 1 ? Ac.Speed : FMath::Min(Ac.Speed, FMath::Max(30.f, Dist * 2.f)), Dt, 4.f);
	const float Go1 = FMath::Min(Dist, R.Speed * Dt);
	if (Dist > 0.5f) { R.At += D * (Go1 / Dist); R.Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(R.Yaw, float(D.Rotation().Yaw)), -260.f * Dt, 260.f * Dt); }
	if (Dist <= FMath::Max(3.f, Go1 + 0.1f)) Ac.Pts.RemoveAt(0);
	if (AActor* A = R.A.Get()) A->SetActorTransform(FTransform(FRotator(0.f, R.Yaw - 90.f, 0.f), R.At) * Body->Body());
	WalkAnim(R, R.Speed);
	return false;
}

FDriverPose UVehicleRiders::HoldPole(const FRider& R, float PH) const
{
	// standing in the aisle, the hand on the nearest grab handle or pole, the feet a little apart
	FDriverPose P = ADPPose::Standing(R.At, VrTurned(R.Yaw), PH);
	const FVector L = ADPPose::LeftOf(P.Facing);
	P.Foot[0] += L * 5.f; P.Foot[1] -= L * 5.f; P.Pelvis.Z -= 2.f;
	FVector Best = FVector::ZeroVector; float Bd = 1e9f;
	for (const FVector& Q : Spec.List(TEXT("poles"))) { const float D = float(FVector::Dist2D(Q, R.At)); if (D < Bd) { Bd = D; Best = Q; } }
	if (Bd < 95.f)
	{
		const int32 s = FVector::DotProduct(Best - R.At, L) >= 0.f ? 0 : 1;
		P.bHand[s] = true; P.Hand[s] = Best - (Best - P.Pelvis).GetSafeNormal2D() * 7.f; P.Grip[s] = 0.9f; P.Lean = 2.f;
	}
	return P;
}

void UVehicleRiders::Stay(FRider& R, float Dt)
{
	if (R.Hold) { ApplyPose(R, R.Hold, 1.f, Dt); return; }
	if (R.bAboard)
	{
		// (aboard on their feet: a hand on a pole while the vehicle moves)
		if (Go.V > 5.f || R.HoldT > 0.f)
		{
			if (!R.bPosed) { R.bFrom = true; }
			R.HoldT = Go.V > 5.f ? 1.2f : R.HoldT - Dt;
			ApplyPose(R, [this, &R](float, float PH) { return HoldPole(R, PH); }, 1.f, Dt);
			return;
		}
		Unpose(R);
		if (AActor* A = R.A.Get()) A->SetActorTransform(FTransform(FRotator(0.f, R.Yaw - 90.f, 0.f), R.At) * Body->Body());
		WalkAnim(R, 0.f);
		return;
	}
	Unpose(R); R.Speed = FMath::FInterpTo(R.Speed, 0.f, Dt, 5.f); WalkAnim(R, R.Speed);
}

void UVehicleRiders::Step(FRider& R, float Dt)
{
	if (!R.A.IsValid()) return;
	if (!R.Q.Num()) { Stay(R, Dt); TickBag(R); return; }
	FAct& Ac = R.Q[0]; bool bDone = false;
	R.T += Dt;
	switch (Ac.Kind)
	{
	case FAct::Walk: Unpose(R); bDone = WalkWorld(R, Ac.To(), Dt, Ac.Speed); break;
	case FAct::Aisle:
		if (Go.V > 5.f || R.HoldT > 0.f) Stay(R, Dt);   // (never walking about while it moves: a hand on a pole)
		else { Unpose(R); bDone = WalkAisle(R, Ac, Dt); }
		break;
	case FAct::Pose: ApplyPose(R, Ac.Fn, Ac.Dur > 0.f ? R.T / Ac.Dur : 1.f, Dt); bDone = R.T >= Ac.Dur; break;
	case FAct::Until: Stay(R, Dt); bDone = Ac.Till(R.T); break;
	case FAct::Do: { const TFunction<void()> F = Ac.Fire; R.Q.RemoveAt(0); R.T = 0.f; if (F) F(); TickBag(R); return; }
	}
	if (bDone)
	{
		R.Q.RemoveAt(0); R.T = 0.f;
		if (R.bPosed && R.bLast) { R.From = R.Last; R.bFrom = true; R.BlendT = 0.f; }
	}
	TickBag(R);
}

void UVehicleRiders::AddPose(FRider& R, float Dur, FPoseFn Fn) { FAct A; A.Kind = FAct::Pose; A.Dur = Dur; A.Fn = MoveTemp(Fn); R.Q.Add(MoveTemp(A)); }
void UVehicleRiders::AddWalk(FRider& R, TFunction<FVector()> To, float Speed) { FAct A; A.Kind = FAct::Walk; A.To = MoveTemp(To); A.Speed = Speed; R.Q.Add(MoveTemp(A)); }
void UVehicleRiders::AddAisle(FRider& R, const TArray<FVector>& Pts, float Speed) { FAct A; A.Kind = FAct::Aisle; A.Pts = Pts; A.Speed = Speed; R.Q.Add(MoveTemp(A)); }
void UVehicleRiders::AddDo(FRider& R, TFunction<void()> Fn) { FAct A; A.Kind = FAct::Do; A.Fire = MoveTemp(Fn); R.Q.Add(MoveTemp(A)); }
void UVehicleRiders::AddUntil(FRider& R, TFunction<bool(float)> Fn) { FAct A; A.Kind = FAct::Until; A.Till = MoveTemp(Fn); R.Q.Add(MoveTemp(A)); }

// ------------------------------------------------------------------------------------------------ steps
void UVehicleRiders::RailHands(FDriverPose& P, const TArray<FVector>& Rails) const
{
	// each hand on the rail of its own side while it is within reach, else hanging
	const FVector L = ADPPose::LeftOf(P.Facing);
	for (int32 s = 0; s < 2; ++s)
	{
		float W = 0.f; FVector At = FVector::ZeroVector;
		for (const FVector& Rl : Rails)
		{
			const FVector Sh = P.Pelvis + VrZ * 46.f + L * (s == 0 ? 17.f : -17.f);
			if ((FVector::DotProduct(Rl - P.Pelvis, L) >= 0.f) != (s == 0)) continue;
			const FVector Q(Rl.X, Rl.Y, FMath::Clamp(Sh.Z - 14.f, 75.f, 290.f));
			const float D = float(FVector::Dist(Sh, Q)), Wi = 1.f - FMath::SmoothStep(62.f, 88.f, D);
			if (Wi > W) { W = Wi; At = Q - (Q - P.Pelvis).GetSafeNormal2D() * 6.f; }
		}
		if (W > 0.02f) { P.bHand[s] = true; P.Hand[s] = FMath::Lerp(VrHang(P, s), At, VrSm(W)); P.Grip[s] = W * 0.95f; }
	}
}

FDriverPose UVehicleRiders::Stairs(const TArray<FVector>& T, const FVector& Face, float K, float PH, const TArray<FVector>& Rails) const
{
	// foot by foot up (or down) T: T[0] where both feet stand, then a tread a step, the last foot joining the other
	const int32 n = T.Num() - 1, Steps = n + 1;
	const float F = FMath::Clamp(K, 0.f, 0.9999f) * Steps; const int32 s = int32(F) + 1; const float t = F - int32(F);
	const FVector L = ADPPose::LeftOf(Face);
	auto C = [&](int32 j) { return T[FMath::Clamp(j, 0, n)]; };
	auto FootAt = [&](int32 j, int32 Side) { return C(j) + L * (Side == 0 ? 9.f : -9.f); };
	const int32 Sw = s & 1, St = 1 - Sw;
	const FVector A = FootAt(s - 2, Sw), B = FootAt(s, Sw), Base = FootAt(s - 1, St);
	const bool bUp = B.Z > A.Z + 1.f;
	FVector Foot = FMath::Lerp(A, B, VrSm(bUp ? (t - 0.22f) / 0.78f : t / 0.8f));
	Foot.Z = FMath::Lerp(A.Z, B.Z, VrSm(bUp ? t * 1.7f : (t - 0.35f) / 0.65f)) + 7.f * FMath::Sin(PI * t);
	FDriverPose P; P.Facing = Face.GetSafeNormal2D();
	P.Foot[Sw] = Foot; P.Foot[St] = Base;
	const FVector M0 = (C(s - 2) + C(s - 1)) / 2.f, M1 = (C(s - 1) + C(s)) / 2.f;
	P.Pelvis = FMath::Lerp(M0, M1, VrSm(t));
	P.Pelvis.Z = PH * 0.97f + FMath::Lerp(FMath::Min(C(s - 2).Z, C(s - 1).Z), FMath::Min(C(s - 1).Z, C(s).Z), VrSm(t));
	P.FootPitch[Sw] = (bUp ? 12.f : -10.f) * FMath::Sin(PI * t);
	P.Lean = (bUp ? 11.f : 3.f) * VrSm(FMath::Min(K, 1.f - K) * 6.f);
	RailHands(P, Rails);
	return P;
}

void UVehicleRiders::AddClimb(FRider& R, bool bUp)
{
	AddDo(R, [this, &R, bUp] { if (bUp) R.bAboard = true; });
	const float Dur = 0.72f * (Spec.List(TEXT("treads_front")).Num() + 1);
	AddPose(R, Dur, [this, bUp](float K, float PH)
	{
		TArray<FVector> T = Spec.List(TEXT("treads_front"));
		T.Insert(GroundUnder(Outside(TEXT("door_stand"), 0.f, -8.f)), 0);
		if (!bUp) Algo::Reverse(T);
		return Stairs(T, bUp ? -VrY : VrY, K, PH, Spec.List(TEXT("rails_front")));
	});
	AddDo(R, [this, &R, bUp] { if (!bUp) R.bAboard = false; R.At = bUp ? Spec.List(TEXT("treads_front")).Last() : GroundUnder(Spec.At(TEXT("door_stand"))); R.Yaw = bUp ? -90.f : 90.f; });
}

// ------------------------------------------------------------------------------------------------ the driver
bool UVehicleRiders::HireDriver(AActor* Who)
{
	FRider* R = Join(Who); if (!R || (Driver && Driver != R)) return false;
	Driver = R; R->bDriver = true;
	return true;
}

bool UVehicleRiders::DriverGetIn()
{
	if (!LineOn() || !Driver || bAtWheel || Driver->bAboard || IsDriving()) return false;
	FRider& R = *Driver;
	AddWalk(R, [this] { return Wo(Spec.At(TEXT("door_stand"))); });
	// the door: the palm on it, it swings out, a step back clear of it meanwhile
	if (Body->DoorK(0) < 0.99f)
	{
		auto Press = [this](float K, float PH) { return ADPPose::Reach(GroundUnder(Spec.At(TEXT("door_stand"))), -VrY, 1, Spec.At(TEXT("door_press")) + VrY * 8.f, 0.f, PH, VrSm(K)); };
		AddPose(R, 0.8f, Press);
		AddDo(R, [this] { Body->SetDoor(0, true); });
		AddPose(R, 0.9f, [this](float K, float PH) { return VrSlide(GroundUnder(Spec.At(TEXT("door_stand"))), GroundUnder(Spec.At(TEXT("door_back"))), -VrY, K, PH); });
		AddUntil(R, [this](float) { return Body->DoorK(0) >= 0.99f; });
		AddPose(R, 0.9f, [this](float K, float PH) { return VrSlide(GroundUnder(Spec.At(TEXT("door_back"))), GroundUnder(Spec.At(TEXT("door_stand"))), -VrY, K, PH); });
	}
	AddClimb(R, true);
	AddDo(R, [this] { ArmWant = 1.f; });
	// down to the driver's own floor, a turn, the left leg in under the wheel, down onto the seat
	AddPose(R, 1.5f, [this](float K, float PH) { return Stairs({Spec.List(TEXT("treads_front")).Last(), Spec.At(TEXT("driver_step"))}, -VrY, K, PH, Spec.List(TEXT("poles"))); });
	AddPose(R, 0.9f, [this](float K, float PH) { return ADPPose::Lerp(ADPPose::Standing(Spec.At(TEXT("driver_step")), -VrY, PH), ADPPose::Standing(Spec.At(TEXT("driver_turned")), VrX, PH), VrSm(K)); });
	auto Sit = [this](float K, float PH)
	{
		const FDriverPose A = ADPPose::Standing(Spec.At(TEXT("driver_turned")), VrX, PH), C = DriverPose(PH);
		FDriverPose B = C; B.Pelvis = FVector(C.Pelvis.X - 2.f, (A.Pelvis.Y + C.Pelvis.Y) * 0.5f, FMath::Lerp(A.Pelvis.Z, C.Pelvis.Z, 0.45f) + 6.f); B.Lean = 20.f;
		B.Foot[0] = FMath::Lerp(A.Foot[0], C.Foot[0], 0.7f) + VrZ * 14.f; B.Foot[1] = A.Foot[1]; B.FootPitch[0] = B.FootPitch[1] = 0.f;
		FDriverPose P = K < 0.5f ? ADPPose::Lerp(A, B, VrSm(K * 2.f)) : ADPPose::Lerp(B, C, VrSm(K * 2.f - 1.f));
		if (K > 0.5f) P.Foot[1] += VrZ * (10.f * FMath::Sin(PI * (K * 2.f - 1.f)));
		return P;
	};
	AddPose(R, 2.0f, Sit);
	AddDo(R, [this, &R] { bAtWheel = true; R.bSeated = true; ArmWant = 0.f; R.Hold = [this](float, float PH) { return DriverPose(PH); }; HandBase[0] = HandBase[1] = State.WheelTurn; RegT[0] = RegT[1] = 1.f; ADP_LOG(TEXT("Riders: the driver is at the wheel")); });
	return true;
}

bool UVehicleRiders::DriverGetOut()
{
	if (!LineOn() || !Driver || !bAtWheel || IsDriving()) return false;
	FRider& R = *Driver;
	if (Body->DoorK(0) < 0.99f) { Gest = GButton; GestT = 0.f; GestDur = 1.5f; GestSide = 1; }
	AddUntil(R, [this](float) { return Body->DoorK(0) >= 0.99f && GestT >= GestDur; });
	AddDo(R, [this, &R] { ArmWant = 1.f; bAtWheel = false; R.bSeated = false; R.Hold = nullptr; });
	AddPose(R, 0.8f, [this](float, float PH) { return DriverPose(PH); });
	AddPose(R, 2.0f, [this](float K, float PH)
	{
		const FDriverPose A = ADPPose::Standing(Spec.At(TEXT("driver_turned")), VrX, PH), C = DriverPose(PH);
		FDriverPose B = C; B.Pelvis = FVector(C.Pelvis.X - 2.f, (A.Pelvis.Y + C.Pelvis.Y) * 0.5f, FMath::Lerp(A.Pelvis.Z, C.Pelvis.Z, 0.45f) + 6.f); B.Lean = 20.f;
		B.Foot[0] = FMath::Lerp(A.Foot[0], C.Foot[0], 0.7f) + VrZ * 14.f; B.Foot[1] = A.Foot[1]; B.FootPitch[0] = B.FootPitch[1] = 0.f;
		return K < 0.5f ? ADPPose::Lerp(C, B, VrSm(K * 2.f)) : ADPPose::Lerp(B, A, VrSm(K * 2.f - 1.f));
	});
	AddPose(R, 0.9f, [this](float K, float PH) { return VrTurnOn(Spec.At(TEXT("driver_step")), 0.f, 90.f, K, PH); });
	AddPose(R, 1.5f, [this](float K, float PH) { return Stairs({Spec.At(TEXT("driver_step")), Spec.List(TEXT("treads_front")).Last()}, VrY, K, PH, Spec.List(TEXT("poles"))); });
	AddDo(R, [this] { ArmWant = 0.f; });
	AddClimb(R, false);
	AddWalk(R, [this] { return Wo(Outside(TEXT("stop_wait"), -260.f, -60.f)); });
	AddDo(R, [] { ADP_LOG(TEXT("Riders: the driver is out again")); });
	return true;
}

bool UVehicleRiders::DriverDoor(bool bOpen)
{
	if (!LineOn() || !bAtWheel || IsDriving() || GestT < GestDur) return false;
	Gest = GButton; GestT = 0.f; GestDur = 1.5f; GestSide = bOpen ? 1 : -1;
	return true;
}

FDriverPose UVehicleRiders::DriverPose(float PH)
{
	// at the wheel: the hips on the seat, the hands on the rim where they hold it, the right foot on its pedal
	const FVector Seat = Spec.At(TEXT("driver_seat")), C = Spec.At(TEXT("wheel"));
	const float Th = Driver && Driver->Rig ? Driver->Rig->ThighRadius() : 8.f;
	FDriverPose P; P.Facing = VrX; P.Lean = -5.f;
	P.Pelvis = Seat + VrZ * Th;
	// the rim: its upper point ahead and up, its right point to the right (angles clockwise for the driver)
	const float Tilt = FMath::DegreesToRadians(Spec.Num(TEXT("wheel_tilt"), 30.f)), Rr = Spec.Num(TEXT("wheel_radius"), 0.245f) * 100.f;
	const FVector U(FMath::Cos(Tilt), 0.f, FMath::Sin(Tilt)), N(-FMath::Sin(Tilt), 0.f, FMath::Cos(Tilt));
	for (int32 s = 0; s < 2; ++s)
	{
		const float Home = s == 0 ? -78.f : 78.f;
		const float Was = Home + State.WheelTurn - HandBase[s];
		const float K = VrSm(RegT[s]);
		const float Ang = FMath::DegreesToRadians(RegT[s] < 1.f ? FMath::Lerp(RegFrom[s], Home, K) : Was);
		const FVector Out = U * FMath::Cos(Ang) + VrY * FMath::Sin(Ang);
		const float Off = RegT[s] < 1.f ? FMath::Sin(PI * RegT[s]) : 0.f;   // (off the rim and back on while re-gripping)
		P.bHand[s] = true;
		P.Hand[s] = C + Out * (Rr + 1.5f) - U * 6.5f + N * (3.f + 5.f * Off);
		P.Grip[s] = 0.9f - 0.6f * Off;
	}
	// a door button (the right hand) or the indicator (the left hand to the column), there and back
	if ((Gest == GButton || Gest == GIndicate) && GestT < GestDur)
	{
		const int32 s = Gest == GButton ? 1 : 0;
		const float W = VrSm(FMath::Sin(PI * FMath::Clamp(GestT / GestDur, 0.f, 1.f)) * 1.5f);
		const FVector To = Gest == GButton ? Spec.At(TEXT("door_button_0")) + FVector(-10.f, -4.f, 6.f) : Spec.At(TEXT("wheel_column")) - U * 8.f;
		P.Hand[s] = FMath::Lerp(P.Hand[s], To, W); P.Grip[s] = FMath::Lerp(P.Grip[s], 0.35f, W);
		if (Gest == GButton) { P.Lean += 13.f * W; P.Twist = -10.f * W; P.LookHand = 1; P.LookW = W * 0.8f; }
	}
	P.HeadYaw = HeadYaw; P.HeadPitch = HeadPitch;
	// the feet: the left one on the floor, the right one on the accelerator or the brake, pressing as it does
	P.Foot[0] = Spec.At(TEXT("left_foot"));
	const FVector A = Spec.At(TEXT("accelerator")), B = Spec.At(TEXT("brake"));
	P.Foot[1] = FMath::Lerp(A, B, VrSm(FootMix)) + VrZ * (4.f * FMath::Sin(PI * FootMix));
	P.FootPitch[1] = FMath::Lerp(34.f - 14.f * AccelK, 36.f - 12.f * BrakeK, VrSm(FootMix));
	P.FootPitch[0] = 8.f;
	return P;
}

void UVehicleRiders::TickDriver(float Dt)
{
	State.SeatArm = FMath::FInterpConstantTo(State.SeatArm, ArmWant, Dt, 1.6f);
	// the hands: one at a time lets go and takes the rim anew once it has been carried too far round
	if (bAtWheel)
		for (int32 s = 0; s < 2; ++s)
		{
			const float Rel = State.WheelTurn - HandBase[s];
			if (RegT[s] < 1.f) { RegT[s] += Dt / 0.32f; HandBase[s] = State.WheelTurn; if (RegT[s] > 1.f) RegT[s] = 1.f; }
			else if (FMath::Abs(Rel) > 62.f && RegT[1 - s] >= 1.f) { RegT[s] = 0.f; RegFrom[s] = (s == 0 ? -78.f : 78.f) + Rel; }
		}
	FootMix = FMath::FInterpTo(FootMix, AccelK > 0.04f ? 0.f : 1.f, Dt, 7.f);
	State.Accel = AccelK; State.Brake = BrakeK;
	// the looks: the mirror of the side it signals to, and now and then each one while driving
	if (GestT < GestDur)
	{
		const float Was = GestT; GestT += Dt;
		if (Gest == GButton && Was < GestDur * 0.5f && GestT >= GestDur * 0.5f) Body->SetDoor(0, GestSide > 0);
	}
	float Wy = 0.f, Wp = 0.f;
	if (bAtWheel)
	{
		if (IsDriving() && (MirrorIn -= Dt) <= 0.f && GestT >= GestDur) { MirrorIn = FMath::FRandRange(6.f, 10.f); Gest = GMirror; GestT = 0.f; GestDur = 1.3f; GestSide = FMath::RandBool() ? 1 : -1; }
		if ((Gest == GMirror || Gest == GIndicate) && GestT < GestDur && GestSide != 0)
		{
			const FVector D = (Spec.At(GestSide < 0 ? TEXT("mirror_left") : TEXT("mirror_right")) - (Spec.At(TEXT("driver_seat")) + VrZ * 80.f)).GetSafeNormal();
			Wy = -FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)); Wp = -FMath::RadiansToDegrees(FMath::Asin(D.Z));
		}
		else if (IsDriving()) Wy = -State.SteerDeg * 0.5f;   // (into the turn)
	}
	HeadYaw = FMath::FInterpTo(HeadYaw, FMath::Clamp(Wy, -70.f, 70.f), Dt, 6.f); HeadPitch = FMath::FInterpTo(HeadPitch, Wp, Dt, 6.f);
}

// ------------------------------------------------------------------------------------------------ the passengers
bool UVehicleRiders::WaitAtStop(AActor* Who, int32 Place, bool bBag)
{
	FRider* R = Join(Who); if (!R || R->bAboard || R->bDriver) return false;
	if (bBag && R->BagState == 0) GiveBag(*R);
	AddWalk(*R, [this, Place] { return Wo(Outside(TEXT("stop_wait"), -130.f - 75.f * Place, 0.f)); });
	TSharedPtr<FVector4> At = MakeShared<FVector4>(0, 0, 0, 1e9);   // (where it stands and the way it faces as it starts to turn)
	AddPose(*R, 0.9f, [this, R, At](float K, float PH)
	{
		if (At->W > 1e8) { const FVector A = Lo(R->A->GetActorLocation()); *At = FVector4(A, Body->Body().InverseTransformVectorNoScale(FRotator(0.f, R->A->GetActorRotation().Yaw + 90.f, 0.f).Vector()).Rotation().Yaw); }
		return VrTurnOn(FVector(*At), float(At->W), -90.f, K, PH);
	});
	return true;
}

bool UVehicleRiders::PickSeat(FRider& R)
{
	// a free seat - nobody on it or on its way to it, you included (ASeats), its row's aisle seat free to pass
	TArray<int32> Free; const FTransform B = Body->Body();
	TArray<TPair<int32, FVector>> All;
	for (int32 i = 0; i < ASeats::Count(); ++i)
	{
		FVector At, F; float H; if (!ASeats::SeatOf(i, At, F, H)) continue;
		const FVector L = B.InverseTransformPosition(At);
		if (FMath::Abs(L.Y) > 125.f || FMath::Abs(L.Z - 185.f) > 40.f || L.X < Spec.Num(TEXT("seat_x_min"), -3.f) * 100.f || L.X > Spec.Num(TEXT("seat_x_max"), 4.f) * 100.f) continue;
		if (FMath::Abs(L.Y) < 20.f || FVector::DotProduct(B.InverseTransformVectorNoScale(F), VrX) < 0.9f) continue;
		All.Emplace(i, L);
	}
	for (const auto& S : All)
	{
		if (ASeats::IsTaken(S.Key)) continue;
		bool bPass = true;
		for (const auto& O : All) if (O.Key != S.Key && FMath::Abs(O.Value.X - S.Value.X) < 25.f && O.Value.Y * S.Value.Y > 0.f && FMath::Abs(O.Value.Y) < FMath::Abs(S.Value.Y) && ASeats::IsTaken(O.Key)) bPass = false;
		if (bPass) Free.Add(S.Key);
	}
	if (!Free.Num()) return false;
	const int32 Pick = Free[int32(GetTypeHash(R.A->GetName()) % uint32(Free.Num()))];
	FVector At, F; float H; ASeats::SeatOf(Pick, At, F, H);
	R.Seat = ASeats::Claim(R.A.Get(), At);   // (the nearest free one to it: itself)
	if (R.Seat == INDEX_NONE || !ASeats::SeatOf(R.Seat, At, F, H)) return false;
	R.SeatL = B.InverseTransformPosition(At); R.SurfZ = -1.f; R.Fwd = R.Lift = R.CrossLift = 0.f; R.bCushionOk = false; R.Tries = 0;
	ADP_LOG(TEXT("Riders: %s takes seat %d of %d free (x %.0f y %.0f z %.0f)"), *R.A->GetName(), R.Seat, Free.Num(), R.SeatL.X, R.SeatL.Y, R.SeatL.Z);
	return true;
}

FDriverPose UVehicleRiders::Passenger(FRider& R, float E, float PH)
{
	// the sitting IK step (People.cpp, Seats.cpp): the hips a thigh's thickness over the seat's real surface, the feet on the
	// floor, the hands on the thighs, one of five ways to sit
	const FVector S = R.SeatL; const float Floor = S.Z - 45.f + 0.5f;
	if (R.SurfZ < 0.f)
	{
		R.SurfZ = float(S.Z);
		FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(RiderSeatTop), true, R.A.Get());
		for (const TSharedPtr<FRider>& O : Riders) if (O->A.IsValid()) Q.AddIgnoredActor(O->A.Get());
		if (GetWorld()->LineTraceSingleByChannel(H, Wo(S + FVector(-4.f, 0.f, 40.f)), Wo(S - VrZ * 40.f), ECC_Visibility, Q)) R.SurfZ = float(Lo(H.ImpactPoint).Z);
		R.SurfZ = FMath::Clamp(R.SurfZ, float(S.Z) - 8.f, float(S.Z) + 6.f);
	}
	const float Th = R.Rig ? R.Rig->ThighRadius() : 8.f, Row = Spec.Num(TEXT("row_stand"), 0.33f) * 100.f;
	const FVector L = ADPPose::LeftOf(VrX);
	FDriverPose St = ADPPose::Standing(FVector(S.X + Row, S.Y, Floor), VrX, PH);
	FDriverPose P; P.Facing = VrX;
	P.Pelvis = FVector(S.X - Spec.Num(TEXT("seat_back"), 0.09f) * 100.f + R.Fwd, S.Y, R.SurfZ + Th + R.Lift);
	const bool bNarrow = (R.SitKind & 1) != 0;
	const float Fx = FMath::Min(38.f, 24.f + PH * 0.12f), Fy = bNarrow ? 8.f : 14.f;
	P.Foot[0] = FVector(S.X + Fx, S.Y, Floor) + L * Fy; P.Foot[1] = FVector(S.X + Fx, S.Y, Floor) - L * Fy;
	P.bHand[0] = P.bHand[1] = true; P.Grip[0] = P.Grip[1] = 0.15f;
	const FVector Lap = P.Pelvis + FVector(26.f, 0.f, 4.f);
	switch (R.SitKind)
	{
	case 0: P.Lean = -4.f; P.Hand[0] = Lap + L * 12.f; P.Hand[1] = Lap - L * 12.f; break;                      // each hand on its thigh
	case 1: P.Lean = -8.f; P.Hand[0] = Lap + L * 5.f - VrX * 8.f; P.Hand[1] = Lap - L * 5.f - VrX * 6.f; break;   // back in the seat, the hands in the lap
	case 2: P.Lean = -3.f; P.Hand[0] = Lap + L * 14.f + VrX * 6.f; P.Hand[1] = Lap - L * 10.f; break;
	case 3: P.Lean = -6.f; P.Hand[0] = Lap + L * 11.f - VrX * 4.f; P.Hand[1] = Lap - L * 13.f + VrX * 3.f; break;
	default:                                                                                                   // legs crossed, the left over the right
		P.Lean = -5.f; P.Foot[1] = FVector(S.X + Fx - 2.f, S.Y, Floor) - L * 7.f;
		P.Foot[0] = FVector(S.X + Fx + 6.f, S.Y, Floor + 26.f + R.CrossLift) - L * 10.f;
		P.Hand[0] = Lap + VrZ * 10.f + L * 4.f; P.Hand[1] = Lap + VrZ * 9.f - L * 6.f; break;
	}
	const float K = VrSm(E);
	if (E < 1.f)
	{
		const FDriverPose Sat = P;
		P = ADPPose::Lerp(St, Sat, K);
		P.Lean += 24.f * FMath::Sin(PI * K);
	}
	// (the idle looks: out of the window, along the aisle, now and then)
	else { const float W = float(FMath::Fmod(GetWorld()->GetTimeSeconds() + R.SitKind * 3.7, 17.0)); if (W < 4.f) P.HeadYaw = (R.SeatL.Y < 0.f ? 1.f : -1.f) * 42.f * FMath::Sin(PI * W / 4.f); }
	R.RestW = FMath::Clamp((K - 0.4f) / 0.4f, 0.f, 1.f);
	return P;
}

bool UVehicleRiders::Board(AActor* Who)
{
	FRider* R = Join(Who); if (!R || R->bAboard || R->bDriver || IsDriving() || R->BagState == 1) return false;
	AddDo(*R, [this] { if (Body->DoorK(0) < 0.5f && bAtWheel && GestT >= GestDur) { Gest = GButton; GestT = 0.f; GestDur = 1.5f; GestSide = 1; } });
	AddWalk(*R, [this] { return Wo(Outside(TEXT("door_back"), 0.f, 20.f)); });
	AddUntil(*R, [this](float) { return Body->DoorK(0) >= 0.99f; });
	AddWalk(*R, [this] { return Wo(Spec.At(TEXT("door_stand"))); });
	AddClimb(*R, true);
	AddDo(*R, [this, R]
	{
		if (!PickSeat(*R))
		{
			// (no seat left: standing by a pole)
			AddAisle(*R, {Spec.At(TEXT("aisle_front")), Spec.At(TEXT("aisle_turn"))});
			R->bSeated = true;
			return;
		}
		const float Row = Spec.Num(TEXT("row_stand"), 0.33f) * 100.f, Ay = -Spec.Num(TEXT("aisle_y"), 0.f) * 100.f;
		const FVector In(R->SeatL.X + Row, Ay, R->SeatL.Z - 44.5f);
		AddAisle(*R, {Spec.At(TEXT("aisle_front")), Spec.At(TEXT("aisle_turn")), Spec.At(TEXT("aisle_start")), In});
		AddPose(*R, 1.0f, [this, In](float K, float PH) { return VrTurnOn(In, 180.f, 0.f, K, PH); });
		AddPose(*R, 1.4f, [this, R, In](float K, float PH)
		{
			// into the row sideways, a hand on the seat in front
			FDriverPose P = VrSlide(In, FVector(In.X, R->SeatL.Y, In.Z), VrX, K, PH);
			const FVector H(R->SeatL.X + 62.f, FMath::Lerp(In.Y, R->SeatL.Y * 0.7f, K), In.Z + 96.f);
			const int32 s = R->SeatL.Y < 0.f ? 0 : 1; P.bHand[s] = true; P.Hand[s] = FMath::Lerp(VrHang(P, s), H - VrX * 6.f, VrSm(FMath::Sin(PI * K) * 1.6f)); P.Grip[s] = 0.5f;
			return P;
		});
		AddPose(*R, 1.3f, [this, R](float K, float PH) { return Passenger(*R, K, PH); });
		AddDo(*R, [this, R] { R->bSeated = true; R->Hold = [this, R](float, float PH) { return Passenger(*R, 1.f, PH); }; ADP_LOG(TEXT("Riders: %s sits"), *R->A->GetName()); });
	});
	return true;
}

bool UVehicleRiders::Alight(AActor* Who)
{
	FRider* R = Find(Who); if (!LineOn() || !R || !R->bAboard || R->bDriver || R->Q.Num()) return false;
	R->bSeated = false; R->Hold = nullptr;
	const float Row = Spec.Num(TEXT("row_stand"), 0.33f) * 100.f, Ay = -Spec.Num(TEXT("aisle_y"), 0.f) * 100.f;
	if (R->Seat != INDEX_NONE)
	{
		const FVector In(R->SeatL.X + Row, Ay, R->SeatL.Z - 44.5f);
		AddPose(*R, 1.3f, [this, R](float K, float PH) { return Passenger(*R, 1.f - K, PH); });
		AddPose(*R, 1.4f, [R, In](float K, float PH) { return VrSlide(FVector(In.X, R->SeatL.Y, In.Z), In, VrX, K, PH); });
		AddDo(*R, [this, R, In] { ASeats::Release(R->A.Get()); R->Seat = INDEX_NONE; R->At = In; R->Yaw = 0.f; });
		AddAisle(*R, {Spec.At(TEXT("aisle_start")), Spec.At(TEXT("aisle_turn"))});
	}
	AddUntil(*R, [this](float) { return Go.V < 1.f && !IsDriving() && Body->DoorK(0) >= 0.99f; });
	AddAisle(*R, {Spec.At(TEXT("aisle_front")), Spec.List(TEXT("treads_front")).Last() - VrX * 2.f});
	AddPose(*R, 0.7f, [this, R](float K, float PH) { return VrTurnOn(Spec.List(TEXT("treads_front")).Last(), 0.f, 90.f, K, PH); });
	AddClimb(*R, false);
	AddWalk(*R, [this, R] { return Wo(Outside(TEXT("stop_wait"), -80.f - 60.f * (R->SitKind % 3), -60.f)); });
	AddDo(*R, [R] { ADP_LOG(TEXT("Riders: %s is off"), *R->A->GetName()); });
	return true;
}

// ------------------------------------------------------------------------------------------------ the luggage
float UVehicleRiders::BaySide(int32 Bay) const { const TArray<FVector>& B = Spec.List(TEXT("bays")); return B.IsValidIndex(Bay) && B[Bay].Y < 0.f ? -1.f : 1.f; }
FVector UVehicleRiders::BaySpot(int32 Bay, float Out) const { const TArray<FVector>& B = Spec.List(TEXT("bays")); return B.IsValidIndex(Bay) ? FVector(B[Bay].X, B[Bay].Y + BaySide(Bay) * Out, 0.f) : FVector::ZeroVector; }

FTransform UVehicleRiders::BagInBay(int32 Bay) const
{
	// lying on its back on the bay's floor, its handle to the door; one beside the other
	int32 N = 0; for (const TSharedPtr<FRider>& R : Riders) if (R->BagState == 3 && R->Bay == Bay) ++N;
	const float Sd = BaySide(Bay); FVector P = BaySpot(Bay, 0.f);
	P.X += (N % 2 == 0 ? -1.f : 1.f) * 30.f; P.Y -= Sd * 18.f; P.Z = Spec.At(TEXT("bay_floor")).Z + VrBagHandle.Z;
	return FTransform(FRotator(0.f, Sd > 0.f ? 180.f : 0.f, 0.f), P);
}

void UVehicleRiders::GiveBag(FRider& R)
{
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Baggage/SM_Bag1.SM_Bag1"));
	if (!M) { ADP_WARN(TEXT("Riders: no SM_Bag1")); return; }
	R.BagHold = NewObject<USceneComponent>(GetOwner()); R.BagHold->RegisterComponent(); R.BagHold->SetMobility(EComponentMobility::Movable);
	R.Bag = NewObject<UStaticMeshComponent>(GetOwner()); R.Bag->SetStaticMesh(M); R.Bag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	R.Bag->SetupAttachment(R.BagHold); R.Bag->SetRelativeLocation(-VrBagHandle); R.Bag->RegisterComponent();
	Keep.Add(R.BagHold); Keep.Add(R.Bag);
	BagToHand(R, R.A.Get());
}

void UVehicleRiders::BagToHand(FRider& R, AActor* Who)
{
	FRider* H = Find(Who); if (!H || !R.BagHold || !H->Skel) return;
	R.BagState = 1; R.BagBy = Who;
	R.BagHold->AttachToComponent(H->Skel, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
	R.BagHold->SetUsingAbsoluteRotation(true); R.BagHold->SetUsingAbsoluteScale(true);
	R.BagHold->SetRelativeLocation(UPaxAnimInstance::GripInHand(H->Skel));
}

void UVehicleRiders::TickBag(FRider& R)
{
	if (!R.BagHold || R.BagState == 0) return;
	if (R.BagState == 1) { if (const AActor* A = R.BagBy.Get()) R.BagHold->SetWorldRotation(FRotator(0.f, A->GetActorRotation().Yaw + 90.f, 0.f).Quaternion() * VrUpright); return; }
	if (R.BagHold->GetAttachParent()) { R.BagHold->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform); R.BagHold->SetUsingAbsoluteRotation(false); R.BagHold->SetUsingAbsoluteScale(false); }
	R.BagHold->SetWorldTransform(R.BagState == 2 ? R.BagAt : R.BagAt * Body->Body());
}

bool UVehicleRiders::PutBagDown(AActor* Who, int32 Bay)
{
	FRider* R = Find(Who); if (!LineOn() || !R || R->BagState != 1 || R->BagBy != Who) return false;
	const FVector Spot = BaySpot(Bay, 175.f) + FVector(115.f, 0.f, 0.f);
	const float Sd = BaySide(Bay);
	AddWalk(*R, [this, Spot, Sd] { return Wo(Spot + VrY * (Sd * 45.f)); });
	auto Low = [this, R, Spot, Sd](float K, float PH)
	{
		const FVector G = GroundUnder(Spot + VrY * (Sd * 45.f));
		return ADPPose::Stoop(G, -VrY * Sd, 1, G + FVector(0.f, -Sd * 24.f, 50.f), 0.9f, PH, VrSm(K));
	};
	AddPose(*R, 0.8f, Low);
	AddDo(*R, [this, R] { FVector W = R->BagHold->GetComponentLocation(); W.Z = GroundZ(W) + 47.f; R->BagAt = FTransform(R->BagHold->GetComponentQuat(), W); R->BagState = 2; R->BagBy = nullptr; });
	AddPose(*R, 0.6f, [Low](float K, float PH) { return Low(1.f - K, PH); });
	return true;
}

void UVehicleRiders::AddLoad(FRider& R, FRider& O, int32 Bay, bool bIn)
{
	const float Sd = BaySide(Bay); const FVector Face = -VrY * Sd;
	const FVector Spot = BaySpot(Bay, 92.f);
	// (on the haunches beside the open bay, both hands at the bag's handle end; the head below the bay's raised door)
	auto Haunch = [this, Spot, Face](const FVector& Hands, float E, float PH, float In)
	{
		FDriverPose P = ADPPose::Squat(GroundUnder(Spot), Face, Hands, PH, E);
		P.Lean += 12.f * E + 8.f * In; P.Pelvis += Face * (14.f * E * In);
		return P;
	};
	AddWalk(R, [this, Spot] { return Wo(Spot); }, 110.f);
	AddPose(R, 0.5f, [this, Spot, Face, &R](float K, float PH) { return StandAt(Spot, Face, PH, true); });
	TSharedPtr<FTransform> From = MakeShared<FTransform>(), To = MakeShared<FTransform>();
	if (bIn)
	{
		AddDo(R, [this, &O, Bay, From, To] { *From = O.BagHold->GetComponentTransform().GetRelativeTransform(Body->Body()); *To = BagInBay(Bay); O.BagState = 4; O.BagBy = nullptr; O.BagAt = *From; });
		// down with it to the bay's edge, then pushed in
		auto Edge = [this, To, Sd, Bay] { FTransform E = *To; E.AddToTranslation(FVector(0.f, Sd * 60.f, 8.f)); return E; };
		AddPose(R, 1.1f, [Haunch, From, Edge, &O](float K, float PH) { O.BagAt.Blend(*From, Edge(), VrSm(K)); return Haunch(O.BagAt.GetLocation() - VrZ * 2.f, VrSm(K), PH, 0.f); });
		AddPose(R, 1.2f, [Haunch, To, Edge, &O](float K, float PH) { O.BagAt.Blend(Edge(), *To, VrSm(K)); return Haunch(O.BagAt.GetLocation(), 1.f, PH, VrSm(K)); });
		AddDo(R, [&O, Bay, To] { O.BagState = 3; O.Bay = Bay; O.BagAt = *To; });
		AddPose(R, 1.0f, [Haunch, To](float K, float PH) { return Haunch(To->GetLocation() + VrZ * (30.f * K), 1.f - VrSm(K), PH, 1.f - K); });
		AddDo(R, [&O, Bay] { ADP_LOG(TEXT("Riders: %s's bag is in bay %d"), *O.A->GetName(), Bay); });
	}
	else
	{
		AddDo(R, [&O, From, To, Sd] { *From = O.BagAt; *To = *From; To->AddToTranslation(FVector(0.f, Sd * 60.f, 8.f)); });
		AddPose(R, 1.0f, [Haunch, From](float K, float PH) { return Haunch(From->GetLocation(), VrSm(K), PH, VrSm(K)); });
		AddDo(R, [&O] { O.BagState = 4; });
		AddPose(R, 1.1f, [Haunch, From, To, &O](float K, float PH) { O.BagAt.Blend(*From, *To, VrSm(K)); return Haunch(O.BagAt.GetLocation(), 1.f, PH, 1.f - VrSm(K)); });
		AddDo(R, [this, &O, &R] { BagToHand(O, R.A.Get()); });
		AddPose(R, 0.9f, [Haunch, To](float K, float PH) { FDriverPose P = Haunch(To->GetLocation(), 1.f - VrSm(K), PH, 0.f); P.bHand[0] = false; P.Hand[1] = FMath::Lerp(P.Hand[1], VrHang(P, 1), VrSm(K)); return P; });
		AddDo(R, [&O] { O.Bay = INDEX_NONE; ADP_LOG(TEXT("Riders: %s's bag is out of its bay"), *O.A->GetName()); });
	}
}

bool UVehicleRiders::LoadBag(AActor* Loader, AActor* Owner, int32 Bay)
{
	FRider* R = Join(Loader); FRider* O = Find(Owner);
	if (!R || !O || !O->BagHold || R->bAboard || (O->BagState != 1 && O->BagState != 2)) return false;
	if (O->BagState == 2)
	{
		// someone else's, standing on the ground: picked up by its handle first
		AddWalk(*R, [this, O] { const FVector P = O->BagAt.GetLocation(), Out = Body->Body().TransformVectorNoScale(VrY * BaySide(0)); return P + Out * 45.f; });
		auto Low = [this, O, R](float K, float PH)
		{
			const FVector H = Lo(O->BagAt.GetLocation()); const float Sd = H.Y >= 0.f ? 1.f : -1.f;
			return ADPPose::Stoop(GroundUnder(H + VrY * (Sd * 45.f)), -VrY * Sd, 1, H + FVector(0.f, Sd * 5.f, 4.f), 0.95f, PH, VrSm(K));
		};
		AddPose(*R, 0.9f, Low);
		AddDo(*R, [this, O, Loader] { BagToHand(*O, Loader); });
		AddPose(*R, 0.7f, [Low](float K, float PH) { return Low(1.f - K, PH); });
	}
	AddLoad(*R, *O, Bay, true);
	return true;
}

bool UVehicleRiders::TakeBag(AActor* Who)
{
	FRider* R = Find(Who); if (!LineOn() || !R || R->bAboard || R->BagState != 3 || R->Bay == INDEX_NONE) return false;
	const int32 Bay = R->Bay;
	AddUntil(*R, [this, Bay](float) { return Body->BayK(Bay) >= 0.99f; });
	AddLoad(*R, *R, Bay, false);
	return true;
}

bool UVehicleRiders::UseBay(AActor* Who, int32 Bay, bool bOpen)
{
	FRider* R = Join(Who); if (!R || R->bAboard || IsDriving() || !Spec.List(TEXT("bays")).IsValidIndex(Bay)) return false;
	const float Sd = BaySide(Bay); const FVector Face = -VrY * Sd, Near = BaySpot(Bay, bOpen ? 52.f : 112.f) + VrX * (bOpen ? 0.f : 35.f), Far = BaySpot(Bay, 125.f) + VrX * 35.f;
	AddWalk(*R, [this, Near] { return Wo(Near); });
	auto Reach = [this, Near, Face, Bay, bOpen, Sd](float K, float PH)
	{
		const FVector G = GroundUnder(Near);
		return bOpen ? ADPPose::Stoop(G, Face, 1, BaySpot(Bay, 4.f) + VrZ * 52.f, 0.8f, PH, VrSm(K)) : ADPPose::Reach(G, Face, 1, BaySpot(Bay, 74.f) + FVector(22.f, 0.f, 128.f), 0.7f, PH, VrSm(K));
	};
	AddPose(*R, 0.9f, Reach);
	AddDo(*R, [this, Bay, bOpen] { Body->SetBay(Bay, bOpen); });
	if (bOpen) AddPose(*R, 1.1f, [this, Near, Far, Face](float K, float PH) { return VrSlide(GroundUnder(Near), GroundUnder(Far), Face, K, PH); });
	else AddPose(*R, 0.8f, [Reach](float K, float PH) { return Reach(1.f - K, PH); });
	AddUntil(*R, [this, Bay, bOpen](float) { return bOpen ? Body->BayK(Bay) >= 0.99f : Body->BayK(Bay) <= 0.01f; });
	return true;
}

// ------------------------------------------------------------------------------------------------ driving
FVector UVehicleRiders::OnPath(float S, FVector* Dir) const
{
	int32 i = 1; while (i + 1 < Go.Pts.Num() && Go.Len[i] < S) ++i;
	const FVector D = (Go.Pts[i] - Go.Pts[i - 1]).GetSafeNormal2D();
	if (Dir) *Dir = D;
	return Go.Pts[i - 1] + D * (S - Go.Len[i - 1]);   // (past the end: straight on)
}

bool UVehicleRiders::Drive(const TArray<FVector>& Path, float LimitMs)
{
	if (!LineOn() || !bAtWheel || IsDriving() || Path.Num() < 2) return false;
	for (const TSharedPtr<FRider>& R : Riders) if (!R->bDriver && R->bAboard && !R->bSeated) return false;   // (everyone sits first)
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (const APawn* Me = PC->GetPawn())   // (never with you aboard: your own walking and sitting stay as they are)
		{ const FVector L = Lo(Me->GetActorLocation()); if (FMath::Abs(L.X) < 660.f && FMath::Abs(L.Y) < 135.f && L.Z > 0.f && L.Z < 400.f) { ADP_LOG(TEXT("Riders: you're aboard - it stays")); return false; } }
	Go = FGo(); Go.Pts = Path; Go.Len.SetNum(Path.Num()); Go.Len[0] = 0.f;
	for (int32 i = 1; i < Path.Num(); ++i) Go.Len[i] = Go.Len[i - 1] + float(FVector::Dist2D(Path[i], Path[i - 1]));
	Go.Limit = LimitMs * 100.f; Go.Rear = Body->Road().TransformPosition(Spec.At(TEXT("rear_axle")));
	Go.Stage = EGo::Shut;
	if (Body->DoorK(0) > 0.f) { Gest = GButton; GestT = 0.f; GestDur = 1.5f; GestSide = -1; }
	for (int32 b = 0; b < Spec.List(TEXT("bays")).Num(); ++b) Body->SetBay(b, false);
	Body->SetDoor(1, false);
	ADP_LOG(TEXT("Riders: driving off - %.0f m, at most %.0f km/h"), Go.Len.Last() / 100.f, LimitMs * 3.6f);
	return true;
}

float UVehicleRiders::Clear(const FVector& Front, float Need)
{
	// how far the way ahead of the bumper is free (cm): people and vehicles in it, people on a zebra it crosses
	AADPPeople* P = AADPPeople::Get(GetWorld());
	const APlayerController* PC = GetWorld()->GetFirstPlayerController(); const APawn* Me = PC ? PC->GetPawn() : nullptr;
	auto Someone = [&](const FVector& At, float R)
	{
		if (Me && Me->GetActorEnableCollision() && FVector::Dist2D(Me->GetActorLocation(), At) < R + 20.f && FMath::Abs(Me->GetActorLocation().Z - At.Z) < 250.f) return true;
		return P && P->AnyoneNear(At, R);
	};
	const float Ahead = float(Spec.At(TEXT("front_bumper")).X - Spec.At(TEXT("front_axle")).X);
	FCollisionObjectQueryParams Ob; Ob.AddObjectTypesToQuery(ECC_Vehicle); Ob.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(RidersAhead), false, Body->Actor());
	const float End = FMath::Min(Need, Go.Len.Last() - Go.S + 200.f);
	for (float d = 60.f; d <= End; d += 90.f)
	{
		FVector D; FVector At = OnPath(Go.S + Ahead + d, &D); At.Z = Front.Z;
		const FVector Rt(-D.Y, D.X, 0.f);
		if (Someone(At, 150.f)) return d - 60.f;
		for (const float s : {-105.f, 0.f, 105.f}) if (Body->OtherVehicleAt(At + Rt * s, 12.f)) return d - 60.f;
		TArray<FOverlapResult> Hit;
		if (GetWorld()->OverlapMultiByObjectType(Hit, At + VrZ * 120.f, FQuat::Identity, Ob, FCollisionShape::MakeSphere(95.f), Q)) return d - 60.f;
		for (const TPair<FVector, FVector>& Z : ARoadNetwork::ZebraCrossings)
		{
			if (FMath::PointDistToSegment(FVector(At.X, At.Y, 0.f), FVector(Z.Key.X, Z.Key.Y, 0.f), FVector(Z.Value.X, Z.Value.Y, 0.f)) > 110.f) continue;
			// (someone on it, or at its very edge, within a lane of us either side)
			const int32 N = FMath::Max(2, int32(FVector::Dist2D(Z.Key, Z.Value) / 100.f));
			for (int32 i = 0; i <= N; ++i)
			{
				const FVector On = FMath::Lerp(Z.Key, Z.Value, float(i) / N);
				if (FVector::Dist2D(On, At) < 620.f && Someone(FVector(On.X, On.Y, At.Z), 140.f)) return FMath::Max(0.f, d - 200.f);
			}
		}
	}
	return 1e9f;
}

void UVehicleRiders::TickDrive(float Dt)
{
	if (Go.Stage == EGo::Parked)
	{
		Go.V = 0.f; AccelK = FMath::FInterpTo(AccelK, 0.f, Dt, 5.f); BrakeK = FMath::FInterpTo(BrakeK, bAtWheel ? 0.6f : 0.f, Dt, 4.f);
		if (bAtWheel || State.SeatArm > 0.f || FMath::Abs(State.LeanPitch) > 0.01f) { State.LeanPitch = FMath::FInterpTo(State.LeanPitch, 0.f, Dt, 4.f); State.LeanRoll = FMath::FInterpTo(State.LeanRoll, 0.f, Dt, 4.f); Body->Place(State); }
		return;
	}
	Go.T += Dt;
	if (Go.Stage == EGo::Shut)
	{
		Body->SetLamps(true, 0, true); Body->SetDrive(0.15f); Body->Place(State);
		if (GestT >= GestDur && Body->DoorK(0) <= 0.01f && Body->DoorK(1) <= 0.01f && !Body->Knelt())
		{
			FVector D; OnPath(FMath::Min(700.f, Go.Len.Last()), &D);
			const float Turn = FMath::FindDeltaAngleDegrees(float(Body->Road().Rotator().Yaw), float(D.Rotation().Yaw));
			Go.Indicate = Turn < -3.f ? -1 : Turn > 3.f ? 1 : 0;
			Gest = GIndicate; GestT = 0.f; GestDur = 1.6f; GestSide = Go.Indicate;
			Go.Stage = EGo::Signal; Go.T = 0.f;
			ADP_LOG(TEXT("Riders: doors shut, signalling %s"), Go.Indicate < 0 ? TEXT("left") : Go.Indicate > 0 ? TEXT("right") : TEXT("nothing"));
		}
		return;
	}
	if (Go.Stage == EGo::Signal)
	{
		Body->SetLamps(true, Go.Indicate, true); Body->Place(State);
		if (Go.T > 2.4f) { Go.Stage = EGo::Run; Body->SetAway(true); }
		return;
	}
	const float Wb = float(Spec.At(TEXT("front_axle")).X - Spec.At(TEXT("rear_axle")).X), Acc = Spec.Num(TEXT("accel"), 0.8f) * 100.f, Brk = Spec.Num(TEXT("brake"), 1.1f) * 100.f;
	const float Left = Go.Len.Last() - Go.S;
	// the speed wanted: the limit, the bends ahead, the stop, what is in the way
	float Want = Go.Limit;
	for (float d = 0.f; d <= 1400.f; d += 200.f)
	{
		FVector A, B; OnPath(FMath::Min(Go.S + d, Go.Len.Last()), &A); OnPath(FMath::Min(Go.S + d + 200.f, Go.Len.Last()), &B);
		const float Ang = FMath::Acos(FMath::Clamp(float(FVector::DotProduct(A, B)), -1.f, 1.f));
		if (Ang > 0.004f) { const float Bend = FMath::Sqrt(110.f * 200.f / Ang); Want = FMath::Min(Want, FMath::Sqrt(Bend * Bend + 2.f * Brk * 0.6f * d)); }
	}
	Want = FMath::Min(Want, FMath::Max(Left > 4.f ? 12.f : 0.f, FMath::Sqrt(FMath::Max(0.f, 2.f * Brk * 0.75f * (Left - 5.f)))));
	if ((Go.LookIn -= Dt) <= 0.f)
	{
		Go.LookIn = 0.12f;
		Go.Free = Clear(Body->Road().GetLocation(), Go.V * Go.V / (2.f * Brk * 0.6f) + 700.f);
		const bool bGive = Go.Free < 1e8f;
		if (bGive != Go.bGiveWay) { Go.bGiveWay = bGive; ADP_LOG(TEXT("Riders: %s (%.1f m ahead, at %.1f km/h)"), bGive ? TEXT("giving way") : TEXT("the way is clear"), bGive ? Go.Free / 100.f : 0.f, Go.V * 0.036f); }
	}
	else if (Go.Free < 1e8f) Go.Free -= Go.V * Dt;
	float Hard = Brk;
	if (Go.Free < 1e8f) { const float Room = Go.Free - 120.f; Want = FMath::Min(Want, Room > 0.f ? FMath::Sqrt(2.f * Brk * 0.7f * Room) : 0.f); if (Go.V * Go.V > 2.f * Brk * FMath::Max(Room, 1.f)) Hard = 380.f; }
	const float WantA = FMath::Clamp((Want - Go.V) / FMath::Max(Dt, 0.05f), -Hard, Acc);
	Go.A = FMath::FInterpTo(Go.A, WantA, Dt, WantA < Go.A ? 9.f : 2.5f);
	Go.V = FMath::Max(0.f, Go.V + Go.A * Dt);
	if (Want <= 0.f && Go.V < 6.f) { Go.V = 0.f; Go.A = 0.f; }
	const float Ds = FMath::Min(Go.V * Dt, FMath::Max(0.f, Left));
	Go.S += Ds; Go.Roll += Ds / (Spec.Num(TEXT("tyre_radius"), 0.52f) * 100.f);
	// the front axle on the path, the rear one trailing after it
	FVector Dir; FVector F = OnPath(Go.S, &Dir);
	FVector Back = Go.Rear - F; Back.Z = 0.f; Back = Back.GetSafeNormal() * Wb;
	Go.Rear = F + Back;
	const FVector Head = -Back.GetSafeNormal2D(), Rt(-Head.Y, Head.X, 0.f);
	const float WasYaw = float(State.Road.Rotator().Yaw);
	const float Steer = FMath::Clamp(FMath::FindDeltaAngleDegrees(float(Head.Rotation().Yaw), float(Dir.Rotation().Yaw)), -Spec.Num(TEXT("max_steer"), 46.f), Spec.Num(TEXT("max_steer"), 46.f));
	if (Go.V > 1.f) State.SteerDeg = FMath::FInterpTo(State.SteerDeg, Steer, Dt, 8.f);
	State.WheelTurn = State.SteerDeg * Spec.Num(TEXT("steer_ratio"), 16.f);
	State.RollRad = float(FMath::Fmod(double(Go.Roll), double(2.f * PI)));
	// every wheel on the road: the ground under each, the vehicle lying on the four
	const FVector W[4] = {F - Rt * 107.f, F + Rt * 107.f, Go.Rear - Rt * 92.f, Go.Rear + Rt * 92.f};
	float Z[4]; for (int32 i = 0; i < 4; ++i) Z[i] = GroundZ(W[i]);
	const FVector Fm(F.X, F.Y, (Z[0] + Z[1]) / 2.f), Rm(Go.Rear.X, Go.Rear.Y, (Z[2] + Z[3]) / 2.f);
	const FVector X3 = (Fm - Rm).GetSafeNormal(), Y3 = (Rt * 199.f + VrZ * ((Z[1] + Z[3] - Z[0] - Z[2]) / 2.f)).GetSafeNormal();
	const FQuat Rot = FRotationMatrix::MakeFromXY(X3, Y3).ToQuat();
	FVector O = Fm - X3 * Spec.At(TEXT("front_axle")).X;
	O.Z = FMath::FInterpTo(State.Road.GetLocation().Z, O.Z, Dt, 14.f);
	State.Road = FTransform(FQuat::Slerp(State.Road.GetRotation(), Rot, FMath::Clamp(Dt * 14.f, 0.f, 1.f)), O);
	// the lean: out of the bend, the nose down braking and up pulling away
	const float YawRate = Dt > 0.f ? FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(WasYaw, float(State.Road.Rotator().Yaw))) / Dt : 0.f;
	State.LeanRoll = FMath::FInterpTo(State.LeanRoll, FMath::Clamp(-YawRate * Go.V / 100.f * 1.0f, -2.f, 2.f), Dt, 2.5f);
	State.LeanPitch = FMath::FInterpTo(State.LeanPitch, FMath::Clamp(Go.A / 100.f * 0.55f, -1.2f, 0.7f), Dt, 3.f);
	const bool bBrake = Go.A < -25.f || Go.V < 1.f;
	AccelK = FMath::FInterpTo(AccelK, Go.A > 5.f ? FMath::Clamp(Go.A / Acc, 0.15f, 1.f) : Go.V > 60.f && Go.A > -10.f ? 0.15f : 0.f, Dt, 6.f);
	BrakeK = FMath::FInterpTo(BrakeK, bBrake ? FMath::Clamp(-Go.A / Hard, 0.3f, 1.f) : 0.f, Dt, 8.f);
	const int32 Ind = Left < 2500.f && Go.S > 1800.f ? 1 : Go.S < 1700.f ? Go.Indicate : 0;
	if (Ind != 0 && Ind != Go.Indicate && GestT >= GestDur) { Go.Indicate = Ind; Gest = GIndicate; GestT = 0.f; GestDur = 1.4f; GestSide = Ind; }
	Body->SetLamps(true, Ind, bBrake); Body->SetDrive(FMath::Clamp(0.15f + Go.V / 1100.f, 0.f, 1.f));
	Body->Place(State);
	if (Left < 5.f && Go.V < 15.f)
	{
		Go.Stage = EGo::Parked; Go.V = 0.f; Go.A = 0.f; Go.bGiveWay = false;
		Body->SetAway(false); Body->SetLamps(true, 0, false); Body->SetDrive(0.f);
		ADP_LOG(TEXT("Riders: stopped at its stop"));
	}
}

void UVehicleRiders::TickComponent(float Dt, ELevelTick Type, FActorComponentTickFunction* Fn)
{
	Super::TickComponent(Dt, Type, Fn);
	if (!Body || !Body->Actor()) return;
	Dt = FMath::Min(Dt, 0.1f);
	Riders.RemoveAll([this](const TSharedPtr<FRider>& R) { if (R->A.IsValid() || R.Get() == Driver) return false; if (R->BagHold) { R->Bag->DestroyComponent(); R->BagHold->DestroyComponent(); } return true; });
	TickDriver(Dt); TickDrive(Dt);
	for (int32 i = 0; i < Riders.Num(); ++i) Step(*Riders[i], Dt);
}

void UVehicleRiders::PutBack()
{
	if (!Body) return;
	for (const TSharedPtr<FRider>& R : Riders)
	{
		R->Q.Reset(); R->Hold = nullptr; R->bAboard = false;
		if (AActor* A = R->A.Get()) { ASeats::Release(A); Unpose(*R); }
		if (R->Bag) R->Bag->DestroyComponent();
		if (R->BagHold) R->BagHold->DestroyComponent();
		if (R->Mesh) R->Mesh->DestroyComponent();
	}
	Riders.Reset(); Keep.Reset(); Driver = nullptr; bAtWheel = false; ArmWant = 0.f; Go = FGo();
	State = Parked; Body->Place(State);
	for (int32 d = 0; d < 2; ++d) Body->SetDoor(d, false);
	for (int32 b = 0; b < Spec.List(TEXT("bays")).Num(); ++b) Body->SetBay(b, false);
	Body->SetAway(false); Body->SetLamps(false, 0, false); Body->SetDrive(0.f);
	SetComponentTickEnabled(false);
}

FString UVehicleRiders::Report(const AActor* Who) const
{
	const FRider* R = Find(Who);
	if (!R || !R->Skel || !R->bPosed) return TEXT("(not posed)");
	auto Off = [&](const TCHAR* Bone, const FVector& Want) { return float(FVector::Dist(Lo(R->Skel->GetBoneLocation(Bone)), Want)); };
	FString S = FString::Printf(TEXT("hips %.1f cm from theirs"), Off(TEXT("pelvis"), R->Last.Pelvis));
	if (R->bDriver) S += FString::Printf(TEXT(", hands %.1f / %.1f cm from the rim's grips, ankles %.1f / %.1f cm over their soles' places, wheel turned %.0f, front wheels %.0f deg"), Off(TEXT("hand_l"), R->Last.Hand[0]), Off(TEXT("hand_r"), R->Last.Hand[1]), Off(TEXT("foot_l"), R->Last.Foot[0]), Off(TEXT("foot_r"), R->Last.Foot[1]), State.WheelTurn, State.SteerDeg);
	else if (R->Seat != INDEX_NONE) S += FString::Printf(TEXT(", way to sit %d, seat top %.1f, forward %.0f, up %.1f, thighs %.1f cm into it, crossing foot +%.0f"), R->SitKind, R->SurfZ, R->Fwd, R->Lift, R->Deep, R->CrossLift);
	return S;
}
