// You in a service vehicle (ServiceDrive.h), and the vehicle's side of it (AGSE::Svc*: which seat is free, the drive itself).
#include "ServiceDrive.h"
#include "GSE.h"
#include "Tugs.h"
#include "ADPWalker.h"
#include "ADPFov.h"
#include "ADPLog.h"
#include "ADPKeep.h"
#include "BabylonModel.h"
#include "CrewPose.h"
#include "DriverPoseAnim.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

AServiceDrive::AServiceDrive()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;   // (after AGSE has moved the vehicle)
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Cam"));
	Cam->SetupAttachment(RootComponent);
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	SetCanBeDamaged(false);
}

void AServiceDrive::SetupPlayerInputComponent(UInputComponent* In)
{
	Super::SetupPlayerInputComponent(In);
	In->BindAxis(TEXT("MoveForward"), this, &AServiceDrive::Fwd);
	In->BindAxis(TEXT("MoveRight"), this, &AServiceDrive::Right);
	In->BindAxis(TEXT("Turn"), this, &AServiceDrive::Turn);
	In->BindAxis(TEXT("LookUp"), this, &AServiceDrive::LookUp);
	In->BindAxis(TEXT("TruckThrottle"), this, &AServiceDrive::PadThrottle);
	In->BindAxis(TEXT("TruckBrake"), this, &AServiceDrive::PadBrake);
	In->BindAction(TEXT("ChangeView"), IE_Pressed, this, &AServiceDrive::ToggleView);
}

bool AServiceDrive::Begin(AGSE* G, int32 Vehicle, int32 InSeat, APawn* Walker)
{
	APlayerController* PC = Walker ? Cast<APlayerController>(Walker->GetController()) : nullptr;
	if (!G || !PC || !G->Tractors.IsValidIndex(Vehicle)) return false;
	Gse = G; Ti = Vehicle; Seat = InSeat; Me = Walker; Seq = ESeq::Walk; T = 0.f;
	AGSE::FTractor& V = G->Tractors[Ti]; V.Player = this; V.PlayerSeat = Seat;
	AddTickPrerequisiteActor(G);
	if (AADPWalker* Wk = Cast<AADPWalker>(Walker)) { Wk->SetLed(true); bBehind = Wk->IsThirdPerson(); }
	ADP_LOG(TEXT("Service vehicle: you walk to the %s's door (seat %d)"), *V.Name, Seat);
	return true;
}

void AServiceDrive::Climb()
{
	APawn* W = Me.Get(); APlayerController* PC = W ? Cast<APlayerController>(W->GetController()) : nullptr; AGSE* G = Gse.Get();
	if (!W || !PC || !G) { Finish(); return; }
	AADPWalker* Wk = Cast<AADPWalker>(W); if (Wk) Wk->SetLed(false);
	CamFrom = PC->PlayerCameraManager ? FTransform(PC->PlayerCameraManager->GetCameraRotation(), PC->PlayerCameraManager->GetCameraLocation()) : W->GetActorTransform();
	CamT = 0.f;
	USkeletalMesh* Own = Wk ? Wk->BodyMesh() : nullptr;
	Manny = Own ? Own : ADPKeep(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
	Rider = NewObject<UPoseableMeshComponent>(this);
	Rider->SetupAttachment(RootComponent); Rider->SetUsingAbsoluteLocation(true); Rider->SetUsingAbsoluteRotation(true);
	Rider->SetCollisionEnabled(ECollisionEnabled::NoCollision); Rider->RegisterComponent();
	if (Manny) Rider->SetSkinnedAssetAndUpdate(Manny);
	if (!Manny || !Rig.Init(Rider)) ADP_ERROR(TEXT("Service vehicle: your body couldn't be rigged"));
	W->SetActorHiddenInGame(true); W->SetActorEnableCollision(false);
	if (ACharacter* Ch = Cast<ACharacter>(W)) { Ch->GetCharacterMovement()->DisableMovement(); if (Ch->GetMesh()) Ch->GetMesh()->bCastHiddenShadow = false; }
	SetActorLocation(G->TW(G->Tractors[Ti], FVector(0.f, 0.f, 120.f)));
	PC->Possess(this);
	if (Own && Rig.IsReady()) Human = Wk->LendBody(Rider);
	if (Human) Wk->ShowHead(true);
	Rider->SetVisibility(!Human);
	Seq = ESeq::Handle; T = 0.f;
	Pose(0.f); View(0.f);
}

void AServiceDrive::Pose(float K)
{
	AGSE* G = Gse.Get(); if (!G || !Rider || !Rig.IsReady()) return;
	FDriverPose P; const float PH = Rig.PelvisHeight();
	if (Seq == ESeq::Handle) G->SvcHandFn(Ti, Seat)(K, PH, P);
	else if (Seq == ESeq::In || Seq == ESeq::Out) G->SvcClimbAny(Ti, Seat, Seq == ESeq::In)(K, PH, P);
	else G->SvcSeatFn(Ti, Seat)(1.f, PH, P);
	// (the frame under the hips, as the crew's - TugCrew.cpp ApplyPose)
	const FVector Fw = P.Facing.GetSafeNormal2D().IsNearlyZero() ? FVector(1, 0, 0) : P.Facing.GetSafeNormal2D();
	const FTransform Frame(FRotator(0.f, Fw.Rotation().Yaw - 90.f, 0.f), FVector(P.Pelvis.X, P.Pelvis.Y, FMath::Min(P.Foot[0].Z, P.Foot[1].Z)));
	P.Facing = FVector(0, 1, 0); P.Pelvis = Frame.InverseTransformPosition(P.Pelvis);
	for (int32 s = 0; s < 2; ++s) { P.Foot[s] = Frame.InverseTransformPosition(P.Foot[s]); P.Hand[s] = Frame.InverseTransformPosition(P.Hand[s]); }
	Rider->SetWorldLocationAndRotation(Frame.GetLocation(), Frame.GetRotation());
	Rig.Pose(P);
	if (Human) Human->Local = Rig.LocalPose();
}

void AServiceDrive::View(float Dt)
{
	AGSE* G = Gse.Get(); APlayerController* PC = Cast<APlayerController>(GetController()); if (!G || !Rider) return;
	const AGSE::FTractor& V = G->Tractors[Ti];
	CamT += Dt;
	FVector At; FRotator Rot;
	if (bBehind)
	{
		Rot = FRotator(-12.f + LookPitch * 0.5f, V.Yaw + LookYaw, 0.f);
		const FVector From = G->TW(V, FVector(0.f, 0.f, 190.f)); At = From - Rot.Vector() * 950.f;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPSvcCam), true, this); Q.AddIgnoredActor(V.M.Get()); if (Me.IsValid()) { Q.AddIgnoredActor(Me.Get()); if (AADPWalker* Wk = Cast<AADPWalker>(Me.Get())) if (Wk->HumanActor()) Q.AddIgnoredActor(Wk->HumanActor()); }
		FHitResult H; if (GetWorld()->LineTraceSingleByChannel(H, From, At, ECC_Visibility, Q)) At = From - Rot.Vector() * FMath::Max(120.f, H.Distance - 40.f);
	}
	else
	{
		At = Rider->GetComponentTransform().TransformPosition(Rig.Eyes());
		Rot = FRotator(LookPitch - 6.f, Rider->GetComponentRotation().Yaw + 90.f + LookYaw, 0.f);
	}
	// (in from where you looked; out to where you will stand)
	float K = FMath::SmoothStep(0.f, 0.8f, CamT); FTransform C(FQuat::Slerp(CamFrom.GetRotation(), Rot.Quaternion(), K).GetNormalized(), FMath::Lerp(CamFrom.GetLocation(), At, K));
	if (Seq == ESeq::Out)
		if (const AADPWalker* Wk = Cast<AADPWalker>(Me.Get()))
		{
			const float E = FMath::SmoothStep(0.65f, 1.f, T / G->SvcClimbTime(Ti, Seat, false));
			const FTransform To = Wk->ViewFrom(G->SvcStandW(Ti, Seat) + FVector(0.f, 0.f, 90.f), FRotator(-6.f, G->SvcFaceYaw(Ti, Seat), 0.f));
			C = FTransform(FQuat::Slerp(C.GetRotation(), To.GetRotation(), E).GetNormalized(), FMath::Lerp(C.GetLocation(), To.GetLocation(), E));
		}
	Cam->SetWorldTransform(C);
	const float Fov = ADPFov::ForScreen(bBehind ? ADPFov::Drive3 : ADPFov::Drive1); if (FMath::Abs(Cam->FieldOfView - Fov) > 0.05f) Cam->SetFieldOfView(Fov);
	const bool bHide = FVector::Dist(C.GetLocation(), Rider->GetComponentTransform().TransformPosition(Rig.Eyes())) < 45.f;
	if (bHide != bHead)
	{
		bHead = bHide;
		if (Human) { if (AADPWalker* Wk = Cast<AADPWalker>(Me.Get())) Wk->ShowHead(!bHide); }
		else if (bHide) Rider->HideBoneByName(TEXT("head"), PBO_None); else Rider->UnHideBoneByName(TEXT("head"));
	}
	(void)PC;
}

void AServiceDrive::Tick(float Dt)
{
	Super::Tick(Dt);
	AGSE* G = Gse.Get(); APawn* W = Me.Get();
	if (!G || !W || !G->Tractors.IsValidIndex(Ti)) { Finish(); return; }
	AGSE::FTractor& V = G->Tractors[Ti];
	T += Dt;
	if (Seq == ESeq::Walk)
	{
		const FVector Spot = G->SvcStandW(Ti, Seat); FVector To = Spot - W->GetActorLocation(); To.Z = 0.f; const float Left = To.Size();
		// (there, or given up: something in the way, the vehicle has moved)
		if (Left < 28.f) { Climb(); return; }
		if (T > 6.f || V.Speed > 5.f) { ADP_LOG(TEXT("Service vehicle: you couldn't reach the %s's door"), *V.Name); Finish(); return; }
		W->AddMovementInput(To / Left, Left < 120.f ? 0.6f : 1.f);
		if (AController* C = W->GetController()) { const FRotator R = C->GetControlRotation(); C->SetControlRotation(FRotator(R.Pitch, FMath::FixedTurn(R.Yaw, G->SvcFaceYaw(Ti, Seat), 200.f * Dt), 0.f)); }
		return;
	}
	SetActorLocation(G->TW(V, FVector(0.f, 0.f, 120.f)));
	float K = 1.f;
	if (Seq == ESeq::Handle) { K = T / 1.f; if (T > 0.55f && !bDoor) { bDoor = true; G->SvcSeatDoor(Ti, Seat, true); } if (T >= 1.f && G->SvcSeatDoorOpen(Ti, Seat)) { Seq = ESeq::In; T = 0.f; K = 0.f; } }
	if (Seq == ESeq::In)
	{
		K = T / G->SvcClimbTime(Ti, Seat, true);
		if (K >= 1.f)
		{
			Seq = ESeq::Sat; T = 0.f; G->SvcSeatDoor(Ti, Seat, false);
			if (Seat == 0) G->Motor(V, true);
			ADP_LOG(TEXT("Service vehicle: you sit in the %s (%s)"), *V.Name, Seat == 0 ? TEXT("at the wheel") : TEXT("a passenger"));
		}
	}
	else if (Seq == ESeq::Sat)
	{
		if (bOutWanted && V.Speed < 2.f && V.Path.Num() == 0)
		{
			if (!bDoor) { bDoor = true; G->SvcSeatDoor(Ti, Seat, true); T = 0.f; }
			if (G->SvcSeatDoorOpen(Ti, Seat) || T > 5.f) { Seq = ESeq::Out; T = 0.f; K = 0.f; if (AADPWalker* Wk = Cast<AADPWalker>(W)) Wk->SetThirdPerson(bBehind); }
		}
		else bDoor = false;
	}
	else if (Seq == ESeq::Out)
	{
		K = T / G->SvcClimbTime(Ti, Seat, false);
		if (K >= 1.f) { Finish(); return; }
	}
	Pose(FMath::Clamp(K, 0.f, 1.f)); View(Dt);
}

void AServiceDrive::Exit()
{
	AGSE* G = Gse.Get(); if (!G || Seq != ESeq::Sat) return;
	if (bOutWanted) { bOutWanted = false; return; }   // (E again while it still rolls: you stay)
	if (!G->SvcRoomAt(Ti, Seat, Me.Get(), this)) { ADP_LOG(TEXT("Service vehicle: no room to get out at that door")); return; }
	bOutWanted = true; bDoor = false;
}

void AServiceDrive::ExitNow() { if (AGSE* G = Gse.Get()) if (G->Tractors.IsValidIndex(Ti)) G->Tractors[Ti].Speed = 0.f; Finish(); }

void AServiceDrive::Finish()
{
	AGSE* G = Gse.Get(); APawn* W = Me.Get(); const ESeq Was = Seq;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (G && G->Tractors.IsValidIndex(Ti))
	{
		AGSE::FTractor& V = G->Tractors[Ti];
		if (V.Player.Get() == this) { V.Player = nullptr; V.PlayerSeat = INDEX_NONE; }
		if (Was != ESeq::Walk) G->SvcSeatDoor(Ti, Seat, false);
		if (Seat == 0 && Was != ESeq::Walk) { V.Speed = 0.f; G->Motor(V, false); }
		ADP_LOG(TEXT("Service vehicle: you are out of the %s"), *V.Name);
	}
	if (AADPWalker* Wk = Cast<AADPWalker>(W)) { Wk->SetLed(false); if (Human) { Wk->ShowHead(true); Wk->TakeBodyBack(); } Human = nullptr; }
	if (W && Was != ESeq::Walk)
	{
		ACharacter* Ch = Cast<ACharacter>(W); const float Half = Ch ? Ch->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
		FVector At = W->GetActorLocation(); float Yaw = W->GetActorRotation().Yaw;
		if (G && G->Tractors.IsValidIndex(Ti)) { At = G->SvcStandW(Ti, Seat) + FVector(0.f, 0.f, Half + 2.f); Yaw = G->SvcFaceYaw(Ti, Seat); }
		W->SetActorLocationAndRotation(At, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);   // (where the climb out ended)
		W->SetActorEnableCollision(true); W->SetActorHiddenInGame(false);
		if (Ch) { Ch->GetCharacterMovement()->SetMovementMode(MOVE_Walking); if (Ch->GetMesh()) Ch->GetMesh()->bCastHiddenShadow = true; }
		if (PC) { PC->Possess(W); PC->SetControlRotation(FRotator(-6.f, Yaw, 0.f)); }
	}
	Destroy();
}

// ------------------------------------------------------------------ the vehicle's side

namespace
{
	const FServiceSpec& SdSpec(int32 i) { return ADPService::All()[i]; }
	int32 SdCab(const FServiceSpec& S, int32 Seat) { return Seat - 1 - S.Seats.Num(); }   // (the seat's own door: its line in Cab; below 0: the driver's, the crew's)
	// two boxes on the ground (middle, forward, half sizes), overlapping?
	bool SdCross(const FVector& A, const FVector& Af, float Al, float Aw, const FVector& B, const FVector& Bf, float Bl, float Bw)
	{
		const FVector D = B - A, As(-Af.Y, Af.X, 0.f), Bs(-Bf.Y, Bf.X, 0.f);
		for (const FVector& X : {Af, As, Bf, Bs})
			if (FMath::Abs(FVector::DotProduct(D, X)) > Al * FMath::Abs(FVector::DotProduct(Af, X)) + Aw * FMath::Abs(FVector::DotProduct(As, X)) + Bl * FMath::Abs(FVector::DotProduct(Bf, X)) + Bw * FMath::Abs(FVector::DotProduct(Bs, X))) return false;
		return true;
	}
}

FVector AGSE::SvcStandL(int32 Ti, int32 Seat) const
{
	const FTractor& T = Tractors[Ti]; const FServiceSpec& S = SdSpec(T.Svc); const int32 c = SdCab(S, Seat);
	return Seat == 0 ? T.KStand : S.Cab.IsValidIndex(c) ? SvcP(T, S.Cab[c].Door.Stand) : SvcP(T, S.Doors[0].Stand);
}
FVector AGSE::SvcStandW(int32 Ti, int32 Seat) const { const FVector W = TW(Tractors[Ti], SvcStandL(Ti, Seat)); return Plan(W.X / 100.f, -W.Y / 100.f); }
float AGSE::SvcFaceYaw(int32 Ti, int32 Seat) const { return Tractors[Ti].Yaw + (SvcStandL(Ti, Seat).Y >= 0.f ? -90.f : 90.f); }
float AGSE::SvcClimbTime(int32 Ti, int32 Seat, bool bIn) const { const FServiceSpec& S = SdSpec(Tractors[Ti].Svc); return Seat > 0 && SdCab(S, Seat) < 0 ? (bIn ? 5.f : 4.4f) : IsTall(Tractors[Ti]) ? (bIn ? 4.6f : 4.2f) : (bIn ? 3.2f : 2.8f); }

void AGSE::SvcSeatDoor(int32 Ti, int32 Seat, bool bOpen)
{
	FTractor& T = Tractors[Ti]; const FServiceSpec& S = SdSpec(T.Svc);
	if (Seat == 0) { if (bOpen && T.DoorWant == 0.f) Sound(*T.SndDoorOpen, TW(T, T.KHandle)); T.DoorWant = bOpen ? T.KDoorDeg : 0.f; return; }
	const int32 c = SdCab(S, Seat), d = c >= 0 ? S.Doors.Num() + c : 0; if (!T.SvcDoors.IsValidIndex(d)) return;
	if (bOpen && T.SvcDoors[d].Want < 0.5f) Sound(*T.SndDoorOpen, TW(T, T.SvcDoors[d].Piv));
	T.SvcDoors[d].Want = bOpen ? 1.f : 0.f;
}
bool AGSE::SvcSeatDoorOpen(int32 Ti, int32 Seat) const
{
	const FTractor& T = Tractors[Ti]; const FServiceSpec& S = SdSpec(T.Svc);
	if (Seat == 0) return T.DoorAng > T.KDoorDeg * 0.9f;
	const int32 c = SdCab(S, Seat), d = c >= 0 ? S.Doors.Num() + c : 0; return !T.SvcDoors.IsValidIndex(d) || T.SvcDoors[d].Now > 0.99f;
}

TFunction<void(float, float, FDriverPose&)> AGSE::SvcHandFn(int32 Ti, int32 Seat) const
{
	const FTractor& T0 = Tractors[Ti]; const FServiceSpec& S = SdSpec(T0.Svc); const int32 c = SdCab(S, Seat);
	const FVector At = SvcStandL(Ti, Seat), H = Seat == 0 ? T0.KHandle : S.Cab.IsValidIndex(c) ? SvcP(T0, S.Cab[c].Door.Handle) : SvcP(T0, S.Doors[0].Handle);
	const float Dy = At.Y >= 0.f ? -90.f : 90.f; const int32 Side = At.Y >= 0.f ? 0 : 1;   // (the hand nearer the door's handle)
	return [this, Ti, At, H, Dy, Side](float K, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Reach(TW(T, At), FRotator(0.f, T.Yaw + Dy, 0.f).Vector(), Side, TW(T, H), 0.7f, PH, FMath::Sin(PI * FMath::Clamp(K, 0.f, 1.f))); };
}

TFunction<void(float, float, FDriverPose&)> AGSE::SvcClimbAny(int32 Ti, int32 Seat, bool bIn) const
{
	const FServiceSpec& S = SdSpec(Tractors[Ti].Svc);
	if (Seat == 0) return ClimbFn(Ti, bIn);
	if (SdCab(S, Seat) < 0 && !IsTall(Tractors[Ti])) return SvcClimbFn(Ti, Seat, bIn);
	// the seat beside the driver: the driver's way in, mirrored, the end of it onto its own seat
	return [this, Ti, Seat, bIn](float K0, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const float K = bIn ? K0 : 1.f - K0;
		FDriverPose A; ClimbFn(Ti, true)(FMath::Min(K, 0.85f), PH, A);
		const FVector F = FRotator(0.f, T.Yaw, 0.f).Vector(), R(-F.Y, F.X, 0.f);
		auto Pt = [&](const FVector& V) { return V - R * (2.f * FVector::DotProduct(V - T.At, R)); };
		P = A; P.Pelvis = Pt(A.Pelvis); P.Facing = A.Facing - R * (2.f * FVector::DotProduct(A.Facing, R));
		for (int32 s = 0; s < 2; ++s) { P.Foot[s] = Pt(A.Foot[1 - s]); P.Hand[s] = Pt(A.Hand[1 - s]); P.bHand[s] = A.bHand[1 - s]; P.Grip[s] = A.Grip[1 - s]; P.FootPitch[s] = A.FootPitch[1 - s]; }
		if (K > 0.85f) { FDriverPose E; SvcSeatFn(Ti, Seat)(1.f, PH, E); P = ADPPose::Lerp(P, E, FMath::SmoothStep(0.f, 1.f, (K - 0.85f) / 0.15f)); }
	};
}

bool AGSE::SvcRoomAt(int32 Ti, int32 Seat, const AActor* A, const AActor* B) const
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPSvcRoom), false, A); Q.AddIgnoredActor(B); Q.AddIgnoredActor(Tractors[Ti].M.Get());
	if (const AADPWalker* Wk = Cast<AADPWalker>(A)) if (Wk->HumanActor()) Q.AddIgnoredActor(Wk->HumanActor());
	return !GetWorld()->OverlapBlockingTestByChannel(SvcStandW(Ti, Seat) + FVector(0.f, 0.f, 100.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(30.f, 70.f), Q);
}

bool AGSE::SvcYours(const FTractor& T) const { return T.Player.IsValid() && T.Svc != INDEX_NONE && SdCab(SdSpec(T.Svc), T.PlayerSeat) < 0; }

bool AGSE::SvcSeatFree(int32 Ti, int32 Seat, FString& Why) const
{
	const FTractor& T = Tractors[Ti]; const FServiceSpec& S = SdSpec(T.Svc); const ATugs* G = Tg();
	if (T.Player.IsValid()) { Why = TEXT("you are in it already"); return false; }
	if (T.Speed > 2.f || T.Path.Num()) { Why = TEXT("it is driving"); return false; }
	if (SdCab(S, Seat) >= 0 || T.Job == EJob::None) return true;   // (the seat beside the driver: nobody of its crew sits there)
	// its crew have it: the driver's seat only while every one of them is up in the aircraft
	if (Seat == 0 && T.Job == EJob::AtStand && !S.bFuel && !S.bCater)   // (the catering truck: on its legs, its body up; the refueller: chocked, bonded, its hose on - never while its driver is at work)
		for (const TPair<int32, FSvcJob>& P : SvcJobs)
			if (P.Value.Ti == Ti && P.Value.Phase == 2)
			{
				bool bUp = true; for (const FSvcMan& M : P.Value.Men) if (!M.bGone && !M.bIn && !(G && G->Crew.IsValidIndex(M.Ci) && G->Crew[M.Ci].bUp)) bUp = false;
				if (bUp) return true;
			}
	Why = Seat == 0 ? TEXT("its crew have it - the seat is taken") : TEXT("its crew's seats are taken");
	return false;
}

bool AGSE::SvcUse(APawn* Me, FString& Say)
{
	Say.Reset(); if (!Me || Cast<AServiceDrive>(Me)) return false;
	const FVector At = Me->GetActorLocation(), Fw = FRotator(0.f, (Me->GetController() ? Me->GetController()->GetControlRotation() : Me->GetActorRotation()).Yaw, 0.f).Vector();
	int32 Bt = INDEX_NONE, Bs = INDEX_NONE; float Bd = 190.f; bool bLoad = false;
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		const FTractor& T = Tractors[i]; if (T.Svc == INDEX_NONE || !T.M.IsValid() || FVector::Dist2D(T.At, At) > T.KHalfL + 300.f || FMath::Abs(T.At.Z - At.Z) > 250.f) continue;
		const FServiceSpec& S = SdSpec(T.Svc); const FVector L = FRotator(0.f, T.Yaw, 0.f).UnrotateVector(At - T.At);
		if (FVector::DotProduct((T.At + FRotator(0.f, T.Yaw, 0.f).RotateVector(FVector(FMath::Clamp(L.X, -T.KHalfL, T.KHalfL), 0.f, 0.f)) - At).GetSafeNormal2D(), Fw) < 0.2f) continue;   // (facing it)
		// each door from its own side of the body; the crew's door: its free seat nearest the door
		auto Try = [&](const FVector& St, int32 Seat, bool bLd)
		{
			if (FMath::Abs(St.Y) > T.KHalfW ? L.Y * St.Y < 0.f || FMath::Abs(L.Y) < T.KHalfW : FMath::Abs(L.X) < T.KHalfL || L.X * St.X < 0.f) return;
			const float D = FVector::Dist2D(L, St); if (D < Bd) { Bd = D; Bt = i; Bs = Seat; bLoad = bLd; }
		};
		Try(T.KStand, 0, false);
		for (int32 c = 0; c < S.Cab.Num(); ++c) Try(SvcP(T, S.Cab[c].Door.Stand), 1 + S.Seats.Num() + c, false);
		if (S.Seats.Num() && S.Doors.Num()) Try(SvcP(T, S.Doors[0].Stand), S.Seats.Num(), false);
		for (int32 d = 1; d < S.Doors.Num(); ++d) Try(SvcP(T, S.Doors[d].Stand), 0, true);
	}
	if (Bt == INDEX_NONE) return false;
	FTractor& T = Tractors[Bt];
	if (bLoad)
	{
		// the load space's doors: open or shut (one leaf after the other, SvcParts) - never while its crew are at it
		bool bCrew = false; for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Ti == Bt && P.Value.Phase < 9) { FString Y; bCrew = !SvcSeatFree(Bt, 0, Y) && !SvcYours(T); }
		if (bCrew) { Say = TEXT("Its crew are at work"); return true; }
		T.LoadWant = T.SvcDoors[1].Want < 0.5f ? 1 : 0; return true;
	}
	FString Why; if (!SvcSeatFree(Bt, Bs, Why)) { Say = FString::Printf(TEXT("The %s: %s"), SdSpec(T.Svc).Label, *Why); return true; }
	AServiceDrive* D = GetWorld()->SpawnActor<AServiceDrive>(T.At, FRotator::ZeroRotator);
	if (!D || !D->Begin(this, Bt, Bs, Me)) { if (D) D->Destroy(); return false; }
	return true;
}

void AGSE::SvcPlayerDrive(FTractor& T, float Dt)
{
	// as AGSE's own Drive (the same reference point, turning circle and wheels), by your pedals and wheel; a solid box: walls,
	// aircraft, vehicles stop it, people ahead stop it in time; only on the airfield's own surface
	AServiceDrive* D = T.Player.Get(); if (!D || Dt <= 0.f) return;
	const FServiceSpec& S = SdSpec(T.Svc);
	bool bShut = true; for (int32 d = 0; d < T.SvcDoors.Num(); ++d) if (T.SvcDoors[d].Now > 0.01f) bShut = false;
	if (T.DoorAng > 1.f) bShut = false;
	float Go = bShut ? D->Throttle() : 0.f, Br = D->Brake();
	// (backwards: the brake held at a standstill)
	if (T.Speed < 3.f) { if (Go > 0.1f) T.bReverse = false; else if (Br > 0.1f && bShut && !D->Leaving()) { T.BackT += Dt; if (T.BackT > 0.35f) T.bReverse = true; } else T.BackT = 0.f; }
	if (T.bReverse && !D->Leaving()) { Swap(Go, Br); if (!bShut) Go = 0.f; }
	const float Max = T.bReverse ? 250.f : S.PlayerSpeed;
	const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector(), Move = Fw * (T.bReverse ? -1.f : 1.f);
	float Acc = Go * 160.f - Br * 520.f - (Go < 0.05f ? 45.f : 0.f);
	// people: the stopping way ahead kept free
	if (T.Speed > 1.f || Go > 0.f)
	{
		const ATugs* G = Tg(); const FVector Sd(-Move.Y, Move.X, 0.f); const float Stop = FMath::Square(T.Speed) / 1400.f;
		for (float r = 60.f; G && r < 130.f + Stop; r += 65.f) for (const float y : {-95.f, 0.f, 95.f}) if (G->PersonAhead(T.At + Move * T.KHalfL + Sd * y, Move, r)) { Acc = -900.f; r = 1e9f; break; }
	}
	T.Speed = FMath::Clamp(T.Speed + Acc * Dt, 0.f, Max);
	T.SteerWant = FMath::FInterpTo(T.SteerWant, D->SteerIn(), Dt, 3.f);
	T.PedalA = FMath::FInterpTo(T.PedalA, Go, Dt, 10.f); T.PedalB = FMath::FInterpTo(T.PedalB, Br, Dt, 10.f);
	if (T.Speed < 0.5f) return;
	const float NewYaw = T.Yaw + (T.bReverse ? -1.f : 1.f) * T.SteerWant * FMath::RadiansToDegrees(T.Speed / T.KTurnR) * FMath::Clamp(500.f / FMath::Max(T.Speed, 1.f), 0.35f, 1.f) * Dt;
	const FVector NewAt = T.At + Move * T.Speed * Dt;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPSvcDrive), true, D); Q.AddIgnoredActor(T.M.Get());
	if (D->GetOwner()) Q.AddIgnoredActor(D->GetOwner());
	for (TActorIterator<AADPWalker> It(GetWorld()); It; ++It) { Q.AddIgnoredActor(*It); if (It->HumanActor()) Q.AddIgnoredActor(It->HumanActor()); }
	auto Blocked = [&](const FVector& At, float Yaw) -> bool
	{
		const FRotator R(0.f, Yaw, 0.f); const FVector F = R.Vector(), C = At + F * T.KCentreX;
		TArray<FOutline> Ol; Outlines(Ol);
		for (const FOutline& O : Ol) if (FVector::Dist2D(O.C, T.At + Fw * T.KCentreX) > 5.f && SdCross(C, F, T.KHalfL, T.KHalfW, O.C, O.X, O.Hx, O.Hy)) return true;
		if (const ATugs* G = Tg())
			for (const ATugs::FTug& U : G->Tugs)
			{
				if (U.M.IsValid() && SdCross(C, F, T.KHalfL, T.KHalfW, U.At, FRotator(0.f, U.Yaw, 0.f).Vector(), 255.f, 126.f)) return true;
				if (U.Gpu.M.IsValid() && SdCross(C, F, T.KHalfL, T.KHalfW, U.Gpu.M->GetActorTransform().TransformPosition(FVector(40.f, 0.f, 0.f)), U.Gpu.M->GetActorForwardVector(), 170.f, 90.f)) return true;
			}
		return GetWorld()->OverlapBlockingTestByChannel(FVector(C.X, C.Y, At.Z + 145.f), R.Quaternion(), ECC_Pawn, FCollisionShape::MakeBox(FVector(T.KHalfL, T.KHalfW, 85.f)), Q);
	};
	// (the surface under the leading wheels: the airfield's, not a drop or a bank)
	auto NoGround = [&](const FVector& At)
	{
		const FVector N = At + Move * T.KHalfL, P = Plan(N.X / 100.f, -N.Y / 100.f); FHitResult H;
		return !GetWorld()->LineTraceSingleByChannel(H, P + FVector(0, 0, 60.f), P - FVector(0, 0, 90.f), ECC_Visibility, Q);
	};
	// (already touching something where it stands: only out of it, never stuck for good)
	if (NoGround(NewAt) || (Blocked(NewAt, NewYaw) && !Blocked(T.At, T.Yaw))) { T.Speed = 0.f; return; }
	T.At = NewAt; T.Yaw = NewYaw; Pose(T);
}

void AGSE::MapShapes(TArray<FVector4f>& Out) const
{
	// every vehicle and cart as it stands now, with its nose (plan metres, as the jetways)
	TArray<FOutline> Ol; Outlines(Ol);
	auto P = [](const FVector& V) { return FVector2f(V.X / 100.f, -V.Y / 100.f); };
	auto Ln = [&](const FVector& A, const FVector& B) { const FVector2f a = P(A), b = P(B); Out.Add(FVector4f(a.X, a.Y, b.X, b.Y)); };
	for (const FOutline& O : Ol)
	{
		const FVector F = O.X * O.Hx, S = O.Y * O.Hy;
		Ln(O.C + F + S, O.C + F - S); Ln(O.C + F - S, O.C - F - S); Ln(O.C - F - S, O.C - F + S); Ln(O.C - F + S, O.C + F + S);
		Ln(O.C + F * 0.4f + S, O.C + F); Ln(O.C + F, O.C + F * 0.4f - S);
	}
}
