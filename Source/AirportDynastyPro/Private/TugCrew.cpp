// The tug's crew (owner, 2026-11-21: "do all of these steps ... who gets out depends on the crew ... metahumans ... walking
// into the pushback truck as well, and out of it again. And then going by their daily tasks").
//  - per tug a driver and the pushback handler (the headset man), ground staff MetaHumans; they wait at their gate's crew
//    spot beside the tug's box (never popping in or out)
//    (owner 2026-11-21: this fixed pair per tug is FOR THE TESTS ONLY - in the game the crew are the ground staff you
//    hire; any of them can be assigned to a tug; between jobs they go about the airport and apron where they are needed,
//    and are always at the tug ahead of time, before the aircraft even lands - from far off by a crew vehicle, later)
//  - a departure: the driver walks to the cab, opens the door, climbs in by the step and the grab rail, sits at the wheel,
//    closes the door, starts the engine; the handler walks out ahead of the aircraft's nose
//  - the tug stops 1.5 m short of the nose; the handler waves it in, it creeps up, he puts the towbar head on the tow
//    fitting and pins it (the clunk), fits the steering bypass pin in the nose gear and plugs his headset into the nose's
//    interphone socket - only then is the aircraft pushed (with no handler the driver drives right up, climbs out, does
//    it all himself and climbs back in)
//  - during the push the handler walks beside the nose; at the end he unpins the towbar, the tug backs away, he takes the
//    bypass pin out, unplugs, walks out to the left, well clear of the wing, and holds the pin up to the cockpit with a
//    thumbs up - the aircraft taxis only after that, with everyone clear (docs/Pushback-Sequence.md); once it has gone
//    he walks back to the gate; the driver parks in the box, stops the engine, climbs out and walks back too
// Their bodies are posed by the fire truck's rig (TruckDriverRig.h: two-bone IK for arms and legs, the hips placed, the
// fingers closing round what they hold), at the tug's IK points (Tools/make_pushback_tug.py: IK_Step, IK_Grab_Outside,
// IK_Seat_Driver, IK_Hand_L/R, IK_Foot_Brake ...). Next: the radio and interphone talk and the timed 11 steps.
#include "Tugs.h"
#include "People.h"
#include "TruckDriverRig.h"
#include "DriverPoseAnim.h"
#include "BabylonModel.h"
#include "ADPLog.h"
#include "Airfield.h"
#include "TerminalPlan.h"
#include "Engine/World.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const FVector Up(0, 0, 1);
	// the tug's own points (cm, the model's root: Blender (x, y, z) m -> (x, -y, z) * 100; its left side is -y)
	const FVector DoorStand(8.f, -178.f, 0.f);       // outside the door, behind its swing
	const FVector HingeAt(57.f, -119.25f, 0.f);       // the door's hinge line (x 0.57, y CAB_Y1 + 0.015)
	FVector LeftOf(const FVector& F) { const FVector G = F.GetSafeNormal2D(); return FVector(G.Y, -G.X, 0.f); }
	float Ramp(float K) { return FMath::SmoothStep(0.f, 0.22f, K) * (1.f - FMath::SmoothStep(0.8f, 1.f, K)); }   // in, held, out
	UBlendSpace* MoveBS()
	{
		static UBlendSpace* B = [] { UBlendSpace* S = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/People/Anims/BS_Idle_Walk_Run_MH.BS_Idle_Walk_Run_MH")); if (S) S->AddToRoot(); return S; }();
		return B;
	}
	USkeletalMeshComponent* BodyOf(UWorld* W, AActor* A)
	{
		AADPPeople* P = AADPPeople::Get(W); AADPPeople::FWho* Who = P && A ? P->Find(A) : nullptr;
		return Who ? Who->Body : nullptr;
	}
	FDriverPose Lerp(const FDriverPose& A, const FDriverPose& B, float K)
	{
		FDriverPose P = B;
		P.Pelvis = FMath::Lerp(A.Pelvis, B.Pelvis, K); P.Facing = FMath::Lerp(A.Facing, B.Facing, K).GetSafeNormal2D(); P.Lean = FMath::Lerp(A.Lean, B.Lean, K);
		for (int32 s = 0; s < 2; ++s)
		{
			P.Foot[s] = FMath::Lerp(A.Foot[s], B.Foot[s], K); P.FootPitch[s] = FMath::Lerp(A.FootPitch[s], B.FootPitch[s], K);
			P.bHand[s] = K < 0.5f ? A.bHand[s] : B.bHand[s];
			// (a hand reaching from hanging: from beside the hip)
			const FVector Ha = A.bHand[s] ? A.Hand[s] : A.Pelvis + LeftOf(A.Facing) * (s == 0 ? 22.f : -22.f) - Up * 15.f;
			const FVector Hb = B.bHand[s] ? B.Hand[s] : B.Pelvis + LeftOf(B.Facing) * (s == 0 ? 22.f : -22.f) - Up * 15.f;
			P.Hand[s] = FMath::Lerp(Ha, Hb, K); P.Grip[s] = FMath::Lerp(A.Grip[s], B.Grip[s], K);
			if (!A.bHand[s] && !B.bHand[s]) P.bHand[s] = false; else P.bHand[s] = true;
		}
		return P;
	}
	// standing at a spot facing a way (side 0 is the body's left: its foot and hand on the left of the way it faces)
	FDriverPose Standing(const FVector& At, const FVector& Face, float PH)
	{
		FDriverPose P; P.Facing = Face.GetSafeNormal2D(); const FVector L = LeftOf(P.Facing);
		P.Pelvis = At + Up * PH; P.Foot[0] = At + L * 12.f; P.Foot[1] = At - L * 12.f;
		return P;
	}
	// down on the haunches at a spot, both hands at a place (the towbar's head, the nose gear's steering collar)
	FDriverPose Squat(const FVector& At, const FVector& Face, const FVector& Hands, float PH, float E)
	{
		FDriverPose S = Standing(At, Face, PH); const FVector L = LeftOf(S.Facing);
		FDriverPose D = S; D.Pelvis = At + Up * (PH * 0.55f) - S.Facing * 14.f; D.Lean = 38.f;
		D.Foot[0] = At + L * 19.f + S.Facing * 4.f; D.Foot[1] = At - L * 19.f + S.Facing * 4.f;
		D.bHand[0] = D.bHand[1] = true; D.Hand[0] = Hands + L * 11.f; D.Hand[1] = Hands - L * 11.f; D.Grip[0] = D.Grip[1] = 0.8f;
		return Lerp(S, D, E);
	}
	// standing, one hand up at a place (the headset socket, the pin held up), the other hanging
	FDriverPose Reach(const FVector& At, const FVector& Face, int32 Side, const FVector& Hand, float Grip, float PH, float E)
	{
		FDriverPose S = Standing(At, Face, PH), D = S;
		D.bHand[Side] = true; D.Hand[Side] = Hand; D.Grip[Side] = Grip; D.Lean = 4.f;
		return Lerp(S, D, E);
	}
}

FVector ATugs::TugW(const FTug& T, const FVector& Local) const { return T.At + FRotator(0.f, T.Yaw, 0.f).RotateVector(Local); }

void ATugs::SetupCrew()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("TugNoCrew"))) return;
	const bool bSolo = FParse::Param(FCommandLine::Get(), TEXT("TugSolo"));
	// the ground staff that are built (the roster: role "ground")
	TArray<FString> Names;
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/people-roster.json"))))
	{
		TArray<TSharedPtr<FJsonValue>> Rows; const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Text);
		if (FJsonSerializer::Deserialize(R, Rows))
			for (const TSharedPtr<FJsonValue>& V : Rows)
				if (const TSharedPtr<FJsonObject> O = V->AsObject())
					if (O->GetStringField(TEXT("role")) == TEXT("ground")) Names.Add(O->GetStringField(TEXT("name")));
	}
	Names.Sort();
	AADPPeople* P = AADPPeople::Get(GetWorld());
	if (!P) return;
	// (the test's ground staff are few - 16 built: six wing / tail walkers first, two pushbacks' worth; the rest drive and
	//  hand the tugs, gate by gate, as far as they go - a gate without a crew works as before, by itself)
	TArray<FString> WalkerNames; for (int32 k = 0; k < 6 && Names.Num() > 2; ++k) WalkerNames.Add(Names.Pop());
	int32 Next = 0;
	for (int32 i = 0; i < Tugs.Num(); ++i)
	{
		FTug& T = Tugs[i];
		for (int32 Rl = 0; Rl < (bSolo ? 1 : 2); ++Rl)
		{
			if (!Names.IsValidIndex(Next)) break;
			// (the crew spot: on the terminal side of the tug's box, to the west of it - away from the jetway, off the red lines)
			FCrew C; C.Name = Names[Next++]; C.Tug = i; C.bDriver = Rl == 0; C.IdleT = FMath::FRandRange(2.f, 9.f);
			C.Spot = Plan(T.StandX - 17.5f - Rl * 1.2f, 86.f + Rl * 1.4f); C.SpotYaw = FVector(0.3f, -1.f, 0.f).Rotation().Yaw;   // (looking out at the stand)
			AActor* A = P->Spawn(C.Name, C.Spot, C.SpotYaw);
			if (!A) continue;
			C.A = A; C.Anchor = C.Spot; C.AnchorYaw = C.SpotYaw;
			(Rl == 0 ? T.Driver : T.Handler) = Crew.Add(C);
		}
	}
	// the wing walkers and the tail walker (owner 2026-11-21, his picture docs/Reference/Pushback/Pushback-Crew-Positions.jpg):
	// the rest of the ground staff, shared by the gates - a push takes up to three that are free
	int32 NumW = 0;
	for (int32 Wn = 0; Wn < WalkerNames.Num() && Tugs.Num(); ++Wn)
	{
		const FTug& T = Tugs[NumW % Tugs.Num()];
		FCrew C; C.Name = WalkerNames[Wn]; C.bWalker = true; C.FollowMax = 330.f;
		C.Spot = Plan(T.StandX - 20.5f, 86.f + (NumW / Tugs.Num()) * 1.4f); C.SpotYaw = FVector(0.3f, -1.f, 0.f).Rotation().Yaw;
		AActor* A = P->Spawn(C.Name, C.Spot, C.SpotYaw); if (!A) continue;
		C.A = A; C.Anchor = C.Spot; C.AnchorYaw = C.SpotYaw; C.IdleT = FMath::FRandRange(2.f, 9.f); Crew.Add(C); ++NumW;
	}
	ADP_LOG(TEXT("Tugs: crew of %d (%s, %d wing / tail walkers) at the gates"), Crew.Num(), bSolo ? TEXT("drivers alone") : TEXT("drivers and pushback handlers"), NumW);
}

void ATugs::Unpose(FCrew& C)
{
	if (!C.bPosed) return;
	C.bPosed = false;
	if (USkeletalMeshComponent* B = BodyOf(GetWorld(), C.A.Get()))
	{
		B->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		if (MoveBS()) { B->SetAnimation(MoveBS()); B->Play(true); }
	}
	C.Anim = nullptr; C.Speed = 0.f;
	// (upright again, facing the way it last faced)
	if (AActor* A = C.A.Get()) A->SetActorRotation(FRotator(0.f, A->GetActorRotation().Yaw, 0.f));
}

void ATugs::ApplyPose(FCrew& C, const FPoseFn& Fn, float K)
{
	AActor* A = C.A.Get(); USkeletalMeshComponent* B = BodyOf(GetWorld(), A);
	if (!A || !B || !Fn) return;
	if (!C.Rig)
	{
		C.Rig = new FTruckDriverRig();
		C.Rider = NewObject<UPoseableMeshComponent>(this); C.Rider->SetupAttachment(RootComponent); C.Rider->SetVisibility(false); C.Rider->RegisterComponent();
		C.Rider->SetSkinnedAssetAndUpdate(B->GetSkeletalMeshAsset());
		if (!C.Rig->Init(C.Rider)) { delete C.Rig; C.Rig = nullptr; return; }
	}
	if (!C.bPosed)
	{
		C.BodyInActor = B->GetComponentTransform().GetRelativeTransform(A->GetActorTransform());
		B->SetAnimInstanceClass(UDriverPoseAnim::StaticClass());
		C.Anim = Cast<UDriverPoseAnim>(B->GetAnimInstance());
		C.bPosed = true;
	}
	FDriverPose W; Fn(FMath::Clamp(K, 0.f, 1.f), C.Rig->PelvisHeight(), W);
	// the frame: under the hips on the feet's level, turned the way the body faces (the body faces the mesh's +y)
	const FVector Fw = W.Facing.GetSafeNormal2D().IsNearlyZero() ? FVector(1, 0, 0) : W.Facing.GetSafeNormal2D();
	const FTransform Frame(FRotator(0.f, Fw.Rotation().Yaw - 90.f, 0.f), FVector(W.Pelvis.X, W.Pelvis.Y, FMath::Min(W.Foot[0].Z, W.Foot[1].Z)));
	FDriverPose L = W; L.Facing = FVector(0, 1, 0);
	L.Pelvis = Frame.InverseTransformPosition(W.Pelvis);
	for (int32 s = 0; s < 2; ++s) { L.Foot[s] = Frame.InverseTransformPosition(W.Foot[s]); L.Hand[s] = Frame.InverseTransformPosition(W.Hand[s]); }
	C.Rig->Pose(L);
	if (C.Anim) C.Anim->Local = C.Rig->LocalPose();
	A->SetActorTransform(C.BodyInActor.Inverse() * Frame);
}

bool ATugs::CrewWalk(FCrew& C, const FVector& To, float Dt, float Stop, float Max)
{
	AActor* A = C.A.Get(); if (!A) return true;
	FVector D = To - A->GetActorLocation(); D.Z = 0.f;
	const float Dist = D.Size();
	const bool bThere = Dist < Stop;
	// (eased: a gentle start and stop; a little slower the last 2 m)
	const float Want = bThere ? 0.f : FMath::Min(Max, FMath::Max(60.f, Dist * 0.9f));
	C.Speed = FMath::FInterpTo(C.Speed, Want, Dt, 3.f);
	if (!bThere)
	{
		FVector P = A->GetActorLocation() + D / Dist * FMath::Min(Dist, C.Speed * Dt);
		FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(TugCrewWalk), false, A);
		if (GetWorld()->LineTraceSingleByChannel(H, P + FVector(0, 0, 120.f), P - FVector(0, 0, 200.f), ECC_Visibility, Q)) P.Z = H.ImpactPoint.Z;
		A->SetActorLocation(P);
		A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, D.Rotation().Yaw - 90.f, 0.f), Dt, 6.f));
	}
	if (USkeletalMeshComponent* B = BodyOf(GetWorld(), A))
		if (UAnimSingleNodeInstance* One = B->GetSingleNodeInstance())
		{
			One->SetBlendSpacePosition(FVector(0.f, FMath::Min(C.Speed * 1.5f, 300.f), 0.f));
			One->SetPlayRate(C.Speed > 20.f ? FMath::Clamp(C.Speed / FMath::Max(C.Speed * 1.5f, 100.f), 0.5f, 1.4f) : 1.f);
		}
	return bThere;
}

void ATugs::StepCrew(FCrew& C, float Dt)
{
	AActor* A = C.A.Get(); if (!A) return;
	if (C.Q.Num())
	{
		FCrewAct& Ac = C.Q[0];
		bool bDone = false;
		C.T += Dt;
		switch (Ac.Kind)
		{
		case FCrewAct::Walk:
			C.FidgetDur = 0.f; Unpose(C); bDone = CrewWalk(C, Ac.To(), Dt, Ac.Stop, Ac.Max);
			if (bDone) { C.Anchor = Ac.To(); C.AnchorYaw = A->GetActorRotation().Yaw + 90.f; C.IdleOff = FVector::ZeroVector; C.IdleYawOff = 0.f; }
			break;
		case FCrewAct::Pose: C.FidgetDur = 0.f; ApplyPose(C, Ac.PoseFn, Ac.Dur > 0.f ? C.T / Ac.Dur : 1.f); bDone = C.T >= Ac.Dur; break;
		case FCrewAct::Wait: if (C.Hold) ApplyPose(C, C.Hold, 1.f); else IdleTick(C, Dt, false); bDone = C.T >= Ac.Dur; break;
		case FCrewAct::WaitFor: if (C.Hold) ApplyPose(C, C.Hold, 1.f); else IdleTick(C, Dt, true); bDone = Ac.Until(C.T); break;
		case FCrewAct::Do: if (Ac.Fn) Ac.Fn(); bDone = true; break;
		}
		if (bDone) { C.T = 0.f; if (C.Q.Num()) C.Q.RemoveAt(0); if (Ac.Kind == FCrewAct::Wait || Ac.Kind == FCrewAct::WaitFor) { C.FidgetDur = 0.f; } }
		return;
	}
	if (C.Hold) { ApplyPose(C, C.Hold, 1.f); return; }
	if (C.Follow) { C.FidgetDur = 0.f; }
	if (!C.Follow) { IdleTick(C, Dt, true); return; }
	Unpose(C);
	if (C.Follow)
	{
		CrewWalk(C, C.Follow(), Dt, 40.f, C.FollowMax);
		// (keeping pace: facing the aircraft as they go - their eyes on the wingtip / tail)
		return;
	}
}

void ATugs::Door(FTug& T, float Dt)
{
	ABabylonModel* M = T.M.Get(); if (!M) return;
	if (T.DoorG == INDEX_NONE)
		T.DoorG = M->BindGroup({TEXT("DoorLeaf"), TEXT("WinDoorGlass"), TEXT("DoorHandle"), TEXT("DoorPanelIn")}, true, FTransform(HingeAt));
	if (T.DoorG == INDEX_NONE || FMath::IsNearlyEqual(T.DoorAng, T.DoorWant)) return;
	const float Was = T.DoorAng;
	T.DoorAng = FMath::FInterpTo(T.DoorAng, T.DoorWant, Dt, 5.f);
	if (FMath::Abs(T.DoorAng - T.DoorWant) < 0.5f) T.DoorAng = T.DoorWant;
	// (swung out forward round the front post: a positive turn takes its rear edge out to the left)
	M->MoveGroup(T.DoorG, FTransform(FRotator(0.f, T.DoorAng, 0.f), HingeAt)); M->FlushMoves();
	if (Was > 0.f && T.DoorAng == 0.f) Sound(TEXT("TugDoorClose"), TugW(T, HingeAt + FVector(-40.f, 0.f, 120.f)));
}

ATugs::FPoseFn ATugs::SeatFn(int32 Tug) const
{
	// sat at the wheel: the hips on the seat, the right foot on the brake, the left on the floor, both hands on the wheel
	// (IK_Seat_Driver, IK_Foot_Brake, IK_Hand_L / IK_Hand_R)
	return [this, Tug](float, float PH, FDriverPose& P)
	{
		const FTug& T = Tugs[Tug];
		P.Facing = FRotator(0.f, T.Yaw, 0.f).Vector();
		P.Pelvis = TugW(T, FVector(-14.f, -70.75f, 99.f + 10.f)); P.Lean = -4.f;
		P.Foot[0] = TugW(T, FVector(28.f, -86.f, 61.f)); P.Foot[1] = TugW(T, FVector(33.f, -70.75f, 72.f)); P.FootPitch[1] = 20.f;
		P.bHand[0] = P.bHand[1] = true; P.Hand[0] = TugW(T, FVector(31.8f, -88.6f, 143.6f)); P.Hand[1] = TugW(T, FVector(31.8f, -52.9f, 143.6f));
		P.Grip[0] = P.Grip[1] = 1.f;
	};
}

ATugs::FPoseFn ATugs::ClimbFn(int32 Tug, bool bIn) const
{
	// up the step into the sunk cab and round onto the seat (IK_Step, IK_Grab_Outside, the door's front post, the cab floor)
	return [this, Tug, bIn](float K0, float PH, FDriverPose& P)
	{
		const FTug& T = Tugs[Tug];
		const float K = bIn ? K0 : 1.f - K0;
		const FVector In = FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), Fwd = FRotator(0.f, T.Yaw, 0.f).Vector();
		auto Key = [&](int32 i)
		{
			FDriverPose Q; Q.Facing = In;
			switch (i)
			{
			case 0: Q = Standing(TugW(T, DoorStand), In, PH); Q.bHand[1] = true; Q.Hand[1] = TugW(T, FVector(-36.f, -126.f, 112.f)); Q.Grip[1] = 1.f; break;   // at the step, a hand on the grab rail
			case 1: Q.Pelvis = TugW(T, FVector(4.f, -158.f, PH + 18.f)); Q.Lean = 12.f;
				Q.Foot[0] = TugW(T, FVector(10.f, -136.f, 53.3f)); Q.Foot[1] = TugW(T, FVector(-6.f, -170.f, 0.f));
				Q.bHand[0] = Q.bHand[1] = true; Q.Hand[0] = TugW(T, FVector(52.f, -120.f, 150.f)); Q.Hand[1] = TugW(T, FVector(-36.f, -126.f, 112.f)); Q.Grip[0] = Q.Grip[1] = 1.f; break;   // left foot on the step, hands on the door post and the rail
			case 2: Q.Pelvis = TugW(T, FVector(2.f, -140.f, 53.3f + PH - 6.f)); Q.Lean = 16.f;
				Q.Foot[0] = TugW(T, FVector(10.f, -136.f, 53.3f)); Q.Foot[1] = TugW(T, FVector(-8.f, -136.f, 53.3f));
				Q.bHand[0] = Q.bHand[1] = true; Q.Hand[0] = TugW(T, FVector(52.f, -120.f, 162.f)); Q.Hand[1] = TugW(T, FVector(-36.f, -126.f, 116.f)); Q.Grip[0] = Q.Grip[1] = 1.f; break;   // up on the step
			case 3: Q.Pelvis = TugW(T, FVector(0.f, -116.f, 60.f + PH - 30.f)); Q.Lean = 32.f;
				Q.Foot[0] = TugW(T, FVector(12.f, -98.f, 60.f)); Q.Foot[1] = TugW(T, FVector(-8.f, -136.f, 53.3f));
				Q.bHand[0] = true; Q.Hand[0] = TugW(T, FVector(52.f, -118.f, 170.f)); Q.Grip[0] = 1.f; break;   // the left foot in on the cab floor, ducking under the door top
			case 4: Q.Facing = (In + Fwd).GetSafeNormal2D(); Q.Pelvis = TugW(T, FVector(-8.f, -86.f, 60.f + PH - 40.f)); Q.Lean = 30.f;
				Q.Foot[0] = TugW(T, FVector(16.f, -92.f, 60.f)); Q.Foot[1] = TugW(T, FVector(-4.f, -100.f, 60.f)); break;   // both feet in, turning to the seat
			default: { const FPoseFn S = SeatFn(Tug); S(1.f, PH, Q); } break;   // sat
			}
			return Q;
		};
		static const float At[] = {0.f, 0.22f, 0.45f, 0.68f, 0.85f, 1.f};
		int32 i = 0; while (i < 4 && K > At[i + 1]) ++i;
		const float E = FMath::SmoothStep(0.f, 1.f, (K - At[i]) / (At[i + 1] - At[i]));
		P = Lerp(Key(i), Key(i + 1), E);
	};
}

void ATugs::ClimbIn(FCrew& C, int32 Tug, TFunction<void()> Then)
{
	FCrewAct W; W.Kind = FCrewAct::Walk; W.To = [this, Tug] { return TugW(Tugs[Tug], DoorStand); }; W.Stop = 15.f; C.Q.Add(W);
	// the hand to the door handle, the door swung open (its sound), up and in, sat, the door pulled shut
	FCrewAct R; R.Kind = FCrewAct::Pose; R.Dur = 1.0f;
	R.PoseFn = [this, Tug](float K, float PH, FDriverPose& P) { const FTug& T = Tugs[Tug]; P = Reach(TugW(T, DoorStand), FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), 1, TugW(T, FVector(-5.f, -124.f, 118.f)), 0.7f, PH, FMath::Sin(PI * K)); };
	C.Q.Add(R);
	FCrewAct O; O.Kind = FCrewAct::Do; O.Fn = [this, Tug] { FTug& T = Tugs[Tug]; T.DoorWant = 72.f; Sound(TEXT("TugDoorOpen"), TugW(T, HingeAt + FVector(-40.f, 0.f, 120.f))); }; C.Q.Add(O);
	FCrewAct Wt; Wt.Kind = FCrewAct::Wait; Wt.Dur = 0.6f; C.Q.Add(Wt);
	FCrewAct Cl; Cl.Kind = FCrewAct::Pose; Cl.Dur = 3.4f; Cl.PoseFn = ClimbFn(Tug, true); C.Q.Add(Cl);
	FCrewAct S; S.Kind = FCrewAct::Do; S.Fn = [this, &C, Tug] { C.Hold = SeatFn(Tug); Tugs[Tug].DoorWant = 0.f; }; C.Q.Add(S);
	FCrewAct W2; W2.Kind = FCrewAct::Wait; W2.Dur = 1.2f; C.Q.Add(W2);
	if (Then) { FCrewAct D; D.Kind = FCrewAct::Do; D.Fn = MoveTemp(Then); C.Q.Add(D); }
}

void ATugs::ClimbOut(FCrew& C, int32 Tug)
{
	FCrewAct O; O.Kind = FCrewAct::Do; O.Fn = [this, Tug] { FTug& T = Tugs[Tug]; T.DoorWant = 72.f; Sound(TEXT("TugDoorOpen"), TugW(T, HingeAt + FVector(-40.f, 0.f, 120.f))); }; C.Q.Add(O);
	FCrewAct Wt; Wt.Kind = FCrewAct::Wait; Wt.Dur = 0.8f; C.Q.Add(Wt);
	FCrewAct H; H.Kind = FCrewAct::Do; H.Fn = [&C] { C.Hold = nullptr; }; C.Q.Add(H);
	FCrewAct Cl; Cl.Kind = FCrewAct::Pose; Cl.Dur = 3.0f; Cl.PoseFn = ClimbFn(Tug, false); C.Q.Add(Cl);
	// down on the ground: the door pushed shut behind
	FCrewAct R; R.Kind = FCrewAct::Pose; R.Dur = 0.9f;
	R.PoseFn = [this, Tug](float K, float PH, FDriverPose& P) { const FTug& T = Tugs[Tug]; P = Reach(TugW(T, DoorStand), FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), 1, TugW(T, FVector(-5.f, -128.f, 118.f)), 0.3f, PH, FMath::Sin(PI * K)); if (K > 0.4f) const_cast<FTug&>(T).DoorWant = 0.f; };
	C.Q.Add(R);
}

namespace
{
	ATugs::FCrewAct WalkAct(TFunction<FVector()> To, float Stop = 20.f, float Max = 150.f) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Walk; A.To = MoveTemp(To); A.Stop = Stop; A.Max = Max; return A; }
	ATugs::FCrewAct PoseAct(ATugs::FPoseFn Fn, float Dur) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Pose; A.PoseFn = MoveTemp(Fn); A.Dur = Dur; return A; }
	ATugs::FCrewAct WaitAct(float Dur) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Wait; A.Dur = Dur; return A; }
	ATugs::FCrewAct UntilAct(TFunction<bool(float)> U) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::WaitFor; A.Until = MoveTemp(U); return A; }
	ATugs::FCrewAct DoAct(TFunction<void()> F) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Do; A.Fn = MoveTemp(F); return A; }
	float Pause() { return FMath::FRandRange(2.f, 5.f); }   // (owner: a random 2-5 s before each step - never scripted)
}

void ATugs::CrewBoard(int32 Tug, bool bArrival)
{
	FTug& T = Tugs[Tug];
	FCrew& D = Crew[T.Driver];
	if (bArrival)
	{
		// (owner 2026-11-21: the driver sits in his tug 3 minutes before the aircraft parks at the gate - engine off till its departure)
		D.Q.Add(WaitAct(Pause()));
		ClimbIn(D, Tug, [this, Tug] { FTug& U = Tugs[Tug]; if (U.State == EState::Boarding) U.State = U.Key.IsEmpty() ? EState::Manned : EState::Waiting; if (!U.Key.IsEmpty()) Engine(U, true); ADP_LOG(TEXT("Tugs: the driver is in his tug, ready for the aircraft")); });
		return;
	}
	if (T.State == EState::Boarding)
	{
		D.Q.Add(WaitAct(Pause()));
		ClimbIn(D, Tug, [this, Tug] { FTug& U = Tugs[Tug]; Engine(U, true); if (U.State == EState::Boarding) U.State = EState::Waiting; ADP_LOG(TEXT("Tugs: the driver is in, engine running")); });
	}
	CrewWalkers(Tug);
	if (T.Handler != INDEX_NONE)
	{
		// the handler out ahead of the aircraft's nose, round the west of the tug's box
		FCrew& H = Crew[T.Handler]; const float Sx = T.StandX;
		H.Q.Add(WaitAct(Pause()));
		H.Q.Add(WalkAct([this, Sx] { return Plan(Sx - 18.5f, 104.f); }, 60.f));
		H.Q.Add(WalkAct([this, Sx] { return Plan(Sx - 6.f, 110.f); }));
	}
}

void ATugs::CrewCouple(int32 Tug)
{
	FTug& T = Tugs[Tug];
	auto Geo = [this, Tug](FVector& Fit, FVector& F, FVector& L, FVector& Gear) { const FTug& U = Tugs[Tug]; Fit = U.Fit; F = U.Fwd; L = LeftOf(F); Gear = Fit - F * 60.f; };
	auto HeadSpot = [Geo] { FVector Fit, F, L, G; Geo(Fit, F, L, G); return Fit + F * 40.f + L * 90.f; };
	auto GearSpot = [Geo] { FVector Fit, F, L, G; Geo(Fit, F, L, G); return G + L * 85.f; };
	const FPoseFn PinHead = [Geo](float K, float PH, FDriverPose& P) { FVector Fit, F, L, G; Geo(Fit, F, L, G); P = Squat(Fit + F * 40.f + L * 90.f, -L, Fit + Up * 48.f + F * 8.f, PH, Ramp(K)); };
	const FPoseFn PinGear = [Geo](float K, float PH, FDriverPose& P) { FVector Fit, F, L, G; Geo(Fit, F, L, G); P = Squat(G + L * 85.f, -L, G + L * 22.f + Up * 62.f, PH, Ramp(K)); };
	auto Clunk = [this, Tug] { const FTug& U = Tugs[Tug]; Sound(TEXT("TugHitchClunk"), U.Fit + Up * 45.f); };
	if (T.Handler != INDEX_NONE)
	{
		FCrew& H = Crew[T.Handler];
		H.Q.Add(WalkAct(HeadSpot, 15.f));
		H.Q.Add(WaitAct(Pause()));
		// he waves the tug in (both hands up, beckoning), it creeps the last 1.5 m
		H.Q.Add(DoAct([this, Tug] { FTug& U = Tugs[Tug]; U.Path = {U.HookAt}; U.bReverse = false; ADP_LOG(TEXT("Tugs: the handler waves the tug in")); }));
		H.Q.Add(PoseAct([this, Tug, Geo](float K, float PH, FDriverPose& P)
		{
			FVector Fit, F, L, G; Geo(Fit, F, L, G); const FVector At = Fit + F * 40.f + L * 90.f, Face = (F - L * 0.4f).GetSafeNormal2D();
			const FDriverPose S = Standing(At, Face, PH); FDriverPose D = S; const FVector PL = LeftOf(Face), Bk = Face * (12.f * FMath::Sin(K * 2.f * PI * 4.f));
			D.bHand[0] = D.bHand[1] = true; D.Hand[0] = S.Pelvis + Up * 55.f + PL * 22.f + Face * 28.f - Bk; D.Hand[1] = S.Pelvis + Up * 55.f - PL * 22.f + Face * 28.f - Bk; D.Grip[0] = D.Grip[1] = 0.2f;
			P = Lerp(S, D, Ramp(K));
		}, 3.f));
		H.Q.Add(UntilAct([this, Tug](float) { return Tugs[Tug].Path.Num() == 0; }));
		// the head onto the fitting, pinned
		H.Q.Add(PoseAct(PinHead, 3.0f));
		H.Q.Add(DoAct([Clunk] { Clunk(); }));
		H.Q.Add(WaitAct(Pause()));
		// the steering bypass pin into the nose gear
		H.Q.Add(WalkAct(GearSpot, 15.f));
		H.Q.Add(PoseAct(PinGear, 2.8f));
		// the headset plugged into the interphone socket on the nose gear leg
		H.Q.Add(PoseAct([Geo](float K, float PH, FDriverPose& P) { FVector Fit, F, L, G; Geo(Fit, F, L, G); P = Reach(G + L * 85.f, -L, 0, G + L * 25.f + Up * 135.f, 0.6f, PH, Ramp(K)); }, 2.0f));
		H.Q.Add(DoAct([this, Tug, Geo] { FTug& U = Tugs[Tug]; U.State = EState::Hooked; ADP_LOG(TEXT("Tugs: towbar on, bypass pin in, headset plugged - hooked on %s"), *U.Key); CrewWalkers(Tug);
			// (beside the nose all through the push, on the cable)
			Crew[U.Handler].Follow = [Geo] { FVector Fit, F, L, G; Geo(Fit, F, L, G); return G + L * 230.f; }; }));
		return;
	}
	// the driver alone: out, the towbar's head on, the bypass pin in, back in
	FCrew& D = Crew[T.Driver];
	D.Q.Add(WaitAct(Pause()));
	ClimbOut(D, Tug);
	D.Q.Add(WalkAct(HeadSpot, 15.f));
	D.Q.Add(PoseAct(PinHead, 3.0f));
	D.Q.Add(DoAct([Clunk] { Clunk(); }));
	D.Q.Add(WalkAct(GearSpot, 15.f));
	D.Q.Add(PoseAct(PinGear, 2.8f));
	ClimbIn(D, Tug, [this, Tug] { FTug& U = Tugs[Tug]; U.State = EState::Hooked; ADP_LOG(TEXT("Tugs: the driver hooked on alone - %s"), *U.Key); CrewWalkers(Tug); });
}

void ATugs::CrewUncouple(int32 Tug)
{
	FTug& T = Tugs[Tug];
	T.bPinShown = false; T.PinKey = T.Key;
	auto Geo = [this, Tug](FVector& Fit, FVector& F, FVector& L, FVector& Gear) { const FTug& U = Tugs[Tug]; Fit = U.Fit; F = U.Fwd; L = LeftOf(F); Gear = Fit - F * 60.f; };
	auto HeadSpot = [Geo] { FVector Fit, F, L, G; Geo(Fit, F, L, G); return Fit + F * 40.f + L * 90.f; };
	auto GearSpot = [Geo] { FVector Fit, F, L, G; Geo(Fit, F, L, G); return G + L * 85.f; };
	const FPoseFn PinHead = [Geo](float K, float PH, FDriverPose& P) { FVector Fit, F, L, G; Geo(Fit, F, L, G); P = Squat(Fit + F * 40.f + L * 90.f, -L, Fit + Up * 48.f + F * 8.f, PH, Ramp(K)); };
	const FPoseFn PinGear = [Geo](float K, float PH, FDriverPose& P) { FVector Fit, F, L, G; Geo(Fit, F, L, G); P = Squat(G + L * 85.f, -L, G + L * 22.f + Up * 62.f, PH, Ramp(K)); };
	auto Clunk = [this, Tug] { const FTug& U = Tugs[Tug]; Sound(TEXT("TugHitchClunk"), U.Fit + Up * 45.f); };
	// out to the left of the nose, well clear of the wing (20 m out, 9 m ahead), facing the captain's window: the pin held up,
	// then a thumbs up
	// (the side towards the gates: 20 m out on the other side put him on taxiway A - the test of 2026-11-21; the pilots see
	//  him ahead at an angle through the side and front windows)
	const FVector Gate = Plan(T.StandX, 104.f);
	auto ShowAt = [Geo, Gate]() { FVector Fit, F, L, G; Geo(Fit, F, L, G); const FVector A = Fit + F * 1360.f + L * 2000.f, B = Fit + F * 1360.f - L * 2000.f; return FVector::Dist2D(A, Gate) < FVector::Dist2D(B, Gate) ? A : B; };
	auto ShowSpot = ShowAt;
	const FPoseFn Show = [Geo, ShowAt](float K, float PH, FDriverPose& P)
	{
		FVector Fit, F, L, G; Geo(Fit, F, L, G); const FVector At = ShowAt(), Cockpit = Fit + F * 210.f + Up * 350.f;
		const FVector Face = (Cockpit - At).GetSafeNormal2D(), PL = LeftOf(Face);
		const FDriverPose S = Standing(At, Face, PH); FDriverPose D = S;
		D.bHand[0] = true; D.Hand[0] = S.Pelvis + Up * 95.f + PL * 25.f + Face * 18.f; D.Grip[0] = 1.f;                    // the pin, held high
		if (K > 0.55f) { D.bHand[1] = true; D.Hand[1] = S.Pelvis + Up * 42.f - PL * 16.f + Face * 32.f; D.Grip[1] = 1.f; }  // a thumbs up
		P = Lerp(S, D, Ramp(K));
	};
	auto Gone = [this, Tug](float Elapsed)
	{
		// (once the aircraft has taxied away: no moving aircraft within 50 m of him, or 90 s)
		const FCrew* C = Tugs[Tug].Handler != INDEX_NONE ? &Crew[Tugs[Tug].Handler] : &Crew[Tugs[Tug].Driver];
		const FVector At = C->A.IsValid() ? C->A->GetActorLocation() : FVector::ZeroVector;
		for (const FVector& Pl : Planes) if (FVector::Dist2D(Pl, At) < 5000.f) return Elapsed > 90.f;
		return Elapsed > 6.f;
	};
	const float Sx = T.StandX;
	if (T.Handler != INDEX_NONE)
	{
		FCrew& H = Crew[T.Handler]; H.Follow = nullptr;
		H.Q.Add(WaitAct(Pause()));                                       // (brakes set - the interphone call comes with the 11 steps)
		H.Q.Add(WalkAct(HeadSpot, 15.f));
		H.Q.Add(PoseAct(PinHead, 2.8f));
		H.Q.Add(DoAct([Clunk] { Clunk(); }));
		H.Q.Add(WalkAct([Geo] { FVector Fit, F, L, G; Geo(Fit, F, L, G); return Fit + F * 60.f + L * 420.f; }, 25.f));   // (aside, clear of the bar)
		H.Q.Add(WaitAct(Pause()));
		H.Q.Add(DoAct([this, Tug] { StartBacking(Tugs[Tug]); }));
		H.Q.Add(UntilAct([this, Tug](float) { return Tugs[Tug].State != EState::Unhook && Tugs[Tug].State != EState::Backing; }));
		H.Q.Add(WalkAct(GearSpot, 15.f));
		H.Q.Add(PoseAct(PinGear, 2.8f));                                // the bypass pin out
		H.Q.Add(PoseAct([Geo](float K, float PH, FDriverPose& P) { FVector Fit, F, L, G; Geo(Fit, F, L, G); P = Reach(G + L * 85.f, -L, 0, G + L * 25.f + Up * 135.f, 0.6f, PH, Ramp(K)); }, 1.6f));   // unplugged
		H.Q.Add(WalkAct(ShowSpot, 25.f));
		H.Q.Add(WaitAct(Pause() * 0.5f));
		H.Q.Add(PoseAct(Show, 5.f));
		H.Q.Add(DoAct([this, Tug] { FTug& U = Tugs[Tug]; U.bPinShown = true; ADP_LOG(TEXT("Tugs: bypass pin shown to the cockpit of %s - all clear"), *U.PinKey); }));
		H.Q.Add(UntilAct(Gone));
		H.Q.Add(WalkAct([this, Sx] { return Plan(Sx - 18.5f, 104.f); }, 80.f));
		{ const FVector Home = H.Spot; H.Q.Add(WalkAct([Home] { return Home; }, 30.f)); H.Q.Add(DoAct([&H] { H.AnchorYaw = H.SpotYaw; })); }
		return;
	}
	// the driver alone: out, the towbar unpinned, the bypass pin out, back in, backs away, out again to show the pin
	FCrew& D = Crew[T.Driver];
	D.Q.Add(WaitAct(Pause()));
	ClimbOut(D, Tug);
	D.Q.Add(WalkAct(HeadSpot, 15.f));
	D.Q.Add(PoseAct(PinHead, 2.8f));
	D.Q.Add(DoAct([Clunk] { Clunk(); }));
	D.Q.Add(WalkAct(GearSpot, 15.f));
	D.Q.Add(PoseAct(PinGear, 2.8f));
	ClimbIn(D, Tug, [this, Tug] { StartBacking(Tugs[Tug]); });
	D.Q.Add(UntilAct([this, Tug](float) { return Tugs[Tug].State == EState::ShowPin; }));
	ClimbOut(D, Tug);
	D.Q.Add(PoseAct([this, Tug, Geo](float K, float PH, FDriverPose& P)
	{
		FVector Fit, F, L, G; Geo(Fit, F, L, G); const FTug& U = Tugs[Tug]; const FVector At = TugW(U, DoorStand), Cockpit = Fit + F * 210.f + Up * 350.f;
		const FVector Face = (Cockpit - At).GetSafeNormal2D(), PL = LeftOf(Face);
		const FDriverPose S = Standing(At, Face, PH); FDriverPose Dp = S;
		Dp.bHand[0] = true; Dp.Hand[0] = S.Pelvis + Up * 95.f + PL * 25.f + Face * 18.f; Dp.Grip[0] = 1.f;
		if (K > 0.55f) { Dp.bHand[1] = true; Dp.Hand[1] = S.Pelvis + Up * 42.f - PL * 16.f + Face * 32.f; Dp.Grip[1] = 1.f; }
		P = Lerp(S, Dp, Ramp(K));
	}, 5.f));
	D.Q.Add(DoAct([this, Tug] { FTug& U = Tugs[Tug]; U.bPinShown = true; ADP_LOG(TEXT("Tugs: the driver shows the bypass pin to %s - all clear"), *U.PinKey); }));
	ClimbIn(D, Tug, [this, Tug] { StartHome(Tugs[Tug]); });
}

void ATugs::CrewParked(int32 Tug)
{
	FTug& T = Tugs[Tug];
	if (T.Driver == INDEX_NONE) return;
	FCrew& D = Crew[T.Driver];
	D.Q.Add(WaitAct(Pause()));
	ClimbOut(D, Tug);
	// (back round the tug's rear to the crew spot)
	{ const FVector Home = D.Spot; D.Q.Add(WalkAct([Home] { return Home; }, 30.f)); D.Q.Add(DoAct([&D] { D.AnchorYaw = D.SpotYaw; })); }
}

void ATugs::CrewWalkers(int32 Tug)
{
	// (owner 2026-11-21: the whole pushback crew is at the gate before the aircraft is about to leave - no crew, no push;
	// short of people, it keeps looking for free ones). Called when the departure is assigned (the walkers come to the
	// gate and stand by) and again every few seconds until three are there; once the tug is hooked on, each goes out to
	// its place - off each wingtip (2 m outboard, level with it) and behind the tail - and the push waits for all three
	FTug& T = Tugs[Tug];
	auto Place = [this, Tug](int32 Rr)
	{
		const FTug& U = Tugs[Tug]; const FVector F = U.Fwd, L = LeftOf(F), G = U.Fit - F * 60.f;
		// (the A320: wingtips 17.9 m out, 12 m behind the nose tyre; the tail cone 32.5 m behind it)
		return Rr == 0 ? G - F * 1200.f + L * 1990.f : Rr == 1 ? G - F * 1200.f - L * 1990.f : G - F * 3900.f;
	};
	auto GoOut = [this, Tug, Place](int32 i, int32 Rr)
	{
		FCrew& C = Crew[i];
		C.Q.Add(WaitAct(Pause()));
		C.Q.Add(WalkAct([Place, Rr] { return Place(Rr); }, 60.f, 200.f));
		C.Q.Add(DoAct([this, Tug, i, Place, Rr] { ++Tugs[Tug].WalkersReady; Crew[i].Follow = [Place, Rr] { return Place(Rr); }; }));
	};
	const bool bHooked = T.State == EState::Hooked;
	// out to their places now that it is hooked on (the ones already standing by)
	if (bHooked && !T.bWalkersOut) { T.bWalkersOut = true; for (int32 r = 0; r < T.Walkers.Num(); ++r) GoOut(T.Walkers[r], r); }
	const float Sx = T.StandX;
	for (int32 i = 0; i < Crew.Num() && T.Walkers.Num() < 3; ++i)
	{
		FCrew& C = Crew[i];
		if (!C.bWalker || C.Tug != INDEX_NONE || !C.A.IsValid()) continue;
		const int32 Rk = T.Walkers.Num();
		C.Tug = Tug; T.Walkers.Add(i);
		ADP_LOG(TEXT("Tugs: walker %d to gate at x %.0f (%s)"), Rk, Sx, Rk == 2 ? TEXT("tail") : TEXT("wingtip"));
		if (T.bWalkersOut) GoOut(i, Rk);
		else C.Q.Add(WalkAct([this, Sx, Rk] { return Plan(Sx - 19.5f - Rk * 1.1f, 100.f + Rk * 0.8f); }, 30.f));   // (standing by at the gate, off the red lines)
	}
	if (T.Walkers.Num() < 3) { static float Said = -100.f; if (GetWorld()->GetTimeSeconds() - Said > 20.f) { Said = GetWorld()->GetTimeSeconds(); ADP_LOG(TEXT("Tugs: short of wing / tail walkers for %s (%d of 3) - looking for free crew"), *T.Key, T.Walkers.Num()); } }
}

void ATugs::WalkersDone(int32 Tug)
{
	// the push done: they hold their places (clear of the wings, behind the tail) until the aircraft has taxied away, then
	// walk back to the gate and are free again
	FTug& T = Tugs[Tug];
	for (const int32 i : T.Walkers)
	{
		FCrew& C = Crew[i]; C.Follow = nullptr;
		if (C.A.IsValid()) { C.Anchor = C.A->GetActorLocation(); C.AnchorYaw = C.A->GetActorRotation().Yaw + 90.f; C.IdleOff = FVector::ZeroVector; }   // (waiting where the push left them)
		C.Q.Add(UntilAct([this, i](float Elapsed)
		{
			const FVector At = Crew[i].A.IsValid() ? Crew[i].A->GetActorLocation() : FVector::ZeroVector;
			for (const FVector& Pl : Planes) if (FVector::Dist2D(Pl, At) < 5500.f) return Elapsed > 120.f;
			return Elapsed > 8.f;
		}));
		C.Q.Add(WalkAct([this, i] { return Crew[i].Spot; }, 40.f));
		C.Q.Add(DoAct([this, i] { Crew[i].Tug = INDEX_NONE; Crew[i].AnchorYaw = Crew[i].SpotYaw; }));
	}
	T.Walkers.Reset(); T.WalkersReady = 0; T.bWalkersOut = false;
}

void ATugs::IdleTick(FCrew& C, float Dt, bool bMayStep)
{
	// waiting: never still as a rock (owner 2026-11-21) - the idle breathing and sway of the walk animation's rest, and every
	// 5-12 s something of their own: a head scratch, a look at the watch, hands on hips, arms folded, a stretch, the weight
	// on the other leg, a look round or a small step - always within 1.5 m of where they wait
	AActor* A = C.A.Get(); if (!A) return;
	if (C.FidgetDur > 0.f)
	{
		C.FidgetT += Dt; ApplyPose(C, C.Fidget, C.FidgetT / C.FidgetDur);
		if (C.FidgetT >= C.FidgetDur) { C.FidgetDur = 0.f; Unpose(C); }
		return;
	}
	Unpose(C);
	const FVector Want = bMayStep ? C.Anchor + C.IdleOff : A->GetActorLocation();
	if (!CrewWalk(C, Want, Dt, 25.f, 90.f)) return;
	A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, C.AnchorYaw + C.IdleYawOff - 90.f, 0.f), Dt, 1.5f));
	if ((C.IdleT -= Dt) > 0.f) return;
	C.IdleT = FMath::FRandRange(5.f, 12.f);
	int32 K = FMath::RandRange(0, 8); if (K == C.LastFidget) K = (K + 1) % 9; C.LastFidget = K;
	if (K == 7) { if (bMayStep) { const float a = FMath::FRandRange(0.f, 2.f * PI), r = FMath::FRandRange(0.3f, 1.4f); C.IdleOff = FVector(FMath::Cos(a) * r * 100.f, FMath::Sin(a) * r * 100.f, 0.f); } return; }   // (a small step)
	if (K == 8) { C.IdleYawOff = FMath::FRandRange(-60.f, 60.f); return; }                                                                                                     // (a look round)
	const FVector At = A->GetActorLocation(); const FVector Face = FRotator(0.f, A->GetActorRotation().Yaw + 90.f, 0.f).Vector();
	const float Side = FMath::RandBool() ? 1.f : -1.f;
	C.Fidget = [At, Face, K, Side](float Kt, float PH, FDriverPose& P)
	{
		const FDriverPose S = Standing(At, Face, PH); FDriverPose D = S; const FVector PL = LeftOf(Face); const float E = Ramp(Kt);
		const int32 h = Side > 0.f ? 0 : 1;   // (which hand: 0 the left, 1 the right)
		switch (K)
		{
		case 0: D.bHand[h] = true; D.Hand[h] = S.Pelvis + Up * 78.f + PL * (7.f * Side) + Face * 2.f + PL * (2.5f * Side) * FMath::Sin(Kt * 40.f); D.Grip[h] = 0.5f; D.Lean = 3.f; break;   // scratching the head
		case 1: D.bHand[0] = D.bHand[1] = true; D.Hand[0] = S.Pelvis + Up * 30.f + Face * 28.f + PL * 4.f; D.Hand[1] = S.Pelvis + Up * 26.f + Face * 26.f - PL * 4.f; D.Grip[1] = 0.4f; D.Lean = 6.f; break;   // a look at the watch
		case 2: D.bHand[0] = D.bHand[1] = true; D.Hand[0] = S.Pelvis + Up * 6.f + PL * 21.f - Face * 2.f; D.Hand[1] = S.Pelvis + Up * 6.f - PL * 21.f - Face * 2.f; D.Grip[0] = D.Grip[1] = 0.6f; break;   // hands on the hips
		case 3: D.bHand[0] = D.bHand[1] = true; D.Hand[0] = S.Pelvis + Up * 33.f - PL * 9.f + Face * 16.f; D.Hand[1] = S.Pelvis + Up * 35.f + PL * 9.f + Face * 17.f; D.Grip[0] = D.Grip[1] = 0.3f; D.Lean = -2.f; break;   // arms folded
		case 4: D.bHand[0] = D.bHand[1] = true; D.Hand[0] = S.Pelvis + Up * 112.f + PL * 14.f; D.Hand[1] = S.Pelvis + Up * 112.f - PL * 14.f; D.Grip[0] = D.Grip[1] = 0.5f; D.Lean = -6.f; break;   // a stretch
		case 5: D.Pelvis += PL * (6.f * Side) - Up * 1.5f; D.Foot[h] += Face * 8.f + PL * (5.f * Side); D.Lean = 1.f; break;   // the weight on one leg
		default: D.bHand[h] = true; D.Hand[h] = S.Pelvis + Up * 62.f + PL * (14.f * Side) + Face * 8.f; D.Grip[h] = 0.2f; break;   // a hand to the back of the neck
		}
		P = Lerp(S, D, E);
	};
	C.FidgetT = 0.f; C.FidgetDur = K == 4 ? 3.f : K == 0 ? 2.6f : FMath::FRandRange(3.f, 5.f);
}
