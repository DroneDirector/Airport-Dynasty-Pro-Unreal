// Sitting down (Seats.h).
#include "Seats.h"
#include "PerfLog.h"
#include "ADPLog.h"
#include "ADPWalker.h"
#include "DriverPoseAnim.h"
#include "InteractOutline.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

TWeakObjectPtr<ASeats> ASeats::One;

ASeats::ASeats()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

int32 ASeats::Count() { return One.IsValid() ? One->Spots.Num() : 0; }
bool ASeats::IsSitting() { return One.IsValid() && One->State != EState::None; }

int32 ASeats::NearestTo(const FVector& At)
{
	// the seat in front of you: within reach on your own floor, the one nearest the middle of your view (owner,
	// 2026-11-14: a taken one is simply taken - you find a free one yourself; no other picked for you)
	if (!One.IsValid()) return INDEX_NONE;
	FVector Look = FVector::ForwardVector, Eye = At;
	if (APlayerController* PC = One->GetWorld() ? One->GetWorld()->GetFirstPlayerController() : nullptr) if (PC->PlayerCameraManager) { Eye = PC->PlayerCameraManager->GetCameraLocation(); Look = PC->PlayerCameraManager->GetCameraRotation().Vector(); }
	int32 Best = INDEX_NONE; float BS = -1e9f;
	for (int32 i = 0; i < One->Spots.Num(); ++i)
	{
		const ABabylonModel::FSeatSpot& S = One->Spots[i];
		const float D = FVector::Dist2D(S.At, At);
		if (D > Reach || FMath::Abs(S.At.Z - S.SeatH - (At.Z - 90.f)) > 60.f) continue;   // (on your own floor)
		const float Score = FVector::DotProduct(Look.GetSafeNormal2D(), (S.At - Eye).GetSafeNormal2D()) * 2.f - D / Reach;
		if (Score > BS) { BS = Score; Best = i; }
	}
	return Best;
}

bool ASeats::NearSeat(const FVector& At) { const int32 I = NearestTo(At); return I != INDEX_NONE && (!IsTaken(I) || One->Spots[I].bYours); }

bool ASeats::IsTaken(int32 Seat)
{
	if (!One.IsValid() || !One->Spots.IsValidIndex(Seat)) return false;
	const FTaken* T = One->Taken.Find(SpotKey(One->Spots[Seat].At));
	return T && T->Who.IsValid();
}

int32 ASeats::Claim(AActor* Who, const FVector& At, TFunction<void()> AskToLeave)
{
	if (!One.IsValid() || !Who) return INDEX_NONE;
	ASeats* S = One.Get();
	int32 Best = INDEX_NONE; float BD = 150.f;
	for (int32 i = 0; i < S->Spots.Num(); ++i)
	{
		const float D = FVector::Dist(S->Spots[i].At, At);
		if (D < BD && !IsTaken(i) && !(S->State != EState::None && S->Seat == i)) { BD = D; Best = i; }
	}
	if (Best != INDEX_NONE) S->Taken.Add(SpotKey(S->Spots[Best].At), FTaken{Who, MoveTemp(AskToLeave)});
	return Best;
}

void ASeats::Follow(const ABabylonModel* Vehicle, bool bAway)
{
	if (!One.IsValid() || !Vehicle) return;
	TArray<ABabylonModel::FSeatSpot> Now; Vehicle->SeatSpots(Now);
	TArray<FTaken> Moved; TArray<FVector> To;
	for (ABabylonModel::FSeatSpot& S : One->Spots)
	{
		if (!S.Parts.Num() || !S.Parts[0].Key || S.Parts[0].Key->GetOwner() != Vehicle) continue;
		const ABabylonModel::FSeatSpot* N = Now.FindByPredicate([&](const ABabylonModel::FSeatSpot& X) { return X.Parts.Num() && X.Parts[0] == S.Parts[0]; });
		if (!N) continue;
		const FVector At = N->At - FVector(0, 0, bAway ? 1.0e6 : 0.0);
		FTaken T; if (One->Taken.RemoveAndCopyValue(SpotKey(S.At), T)) { Moved.Add(T); To.Add(At); }
		S.At = At; S.Facing = N->Facing;
	}
	for (int32 i = 0; i < Moved.Num(); ++i) One->Taken.Add(SpotKey(To[i]), Moved[i]);
}

void ASeats::Release(AActor* Who)
{
	if (!One.IsValid()) return;
	for (auto It = One->Taken.CreateIterator(); It; ++It) if (!It->Value.Who.IsValid() || It->Value.Who.Get() == Who) It.RemoveCurrent();
}

bool ASeats::SeatOf(int32 I, FVector& At, FVector& Facing, float& SeatH)
{
	if (!One.IsValid() || !One->Spots.IsValidIndex(I)) return false;
	const ABabylonModel::FSeatSpot& S = One->Spots[I]; At = S.At; Facing = S.Facing; SeatH = S.SeatH; return true;
}

void ASeats::LogFacing(const TCHAR* Who, int32 I, const FVector& At, const FVector& Facing, float SeatH, const FVector& BodyDir, const AActor* Sitting)
{
	if (!One.IsValid() || !One->Spots.IsValidIndex(I)) return;
	const ABabylonModel::FSeatSpot& S = One->Spots[I];
	if (FVector::Dist(S.At, At) > 40.f) return;   // (that number is another seat now)
	const float Yaw = FRotator::ClampAxis(Facing.Rotation().Yaw), Sq = FMath::RoundToFloat(Yaw / 90.f) * 90.f;
	// (the kind: its mesh, its floor, the way it faces - the curved windows' seats are the ones not square to the building)
	uint32 Kind = HashCombine(GetTypeHash(S.Parts.Num() && S.Parts[0].Key ? S.Parts[0].Key->GetStaticMesh() : nullptr), GetTypeHash(Who[0]));
	const bool bCurve = FMath::Abs(Yaw - Sq) > 0.5f;
	Kind = HashCombine(Kind, GetTypeHash((At.Z > 500.f ? 1000 : 0) + (bCurve ? 720 : int32(FRotator::ClampAxis(Sq)))));
	static TSet<uint32> Said;
	if (Said.Num() >= 24 || Said.Contains(Kind)) return;
	Said.Add(Kind);
	// what stands highest over each side of the seat (straight down, 10-46 cm from its point; the sitter not counted)
	UWorld* W = One->GetWorld();
	auto Top = [&](const FVector& Dir)
	{
		float H = -SeatH; FCollisionQueryParams Q(SCENE_QUERY_STAT(SeatBackSide), true, Sitting);
		for (float D = 10.f; D <= 46.f; D += 6.f)
		{
			FHitResult R; const FVector P = At + Dir * D;
			if (W->LineTraceSingleByChannel(R, P + FVector(0, 0, 85.f), P - FVector(0, 0, SeatH), ECC_Visibility, Q)
				&& S.Parts.ContainsByPredicate([&](const TPair<UInstancedStaticMeshComponent*, int32>& X) { return X.Key == R.GetComponent(); })) H = FMath::Max(H, float(R.ImpactPoint.Z - At.Z));
		}
		return H;
	};
	const FVector F = Facing.GetSafeNormal2D();
	const float Fr = Top(F), Bk = Top(-F);
	const bool bBack = FMath::Max(Fr, Bk) > 12.f;                                  // (something rises over the pan at all)
	const bool bSeatOk = !bBack || Bk > Fr, bBodyOk = FVector::DotProduct(BodyDir.GetSafeNormal2D(), F) > 0.7f;
	ADP_LOG(TEXT("Seats: facing %s on %s%s, floor %d (seat %d at %.0f, %.0f): the seat %.0f, the sitter's thighs %.0f; the seat rises %.0f cm ahead, %.0f cm behind its point - back %s - %s"), Who,
		S.Parts.Num() && S.Parts[0].Key ? *GetNameSafe(S.Parts[0].Key->GetStaticMesh()) : TEXT("?"), bCurve ? TEXT(" in a curve") : TEXT(""), At.Z > 500.f ? 2 : 1, I, At.X, At.Y,
		Yaw, FRotator::ClampAxis(BodyDir.Rotation().Yaw), Fr, Bk, !bBack ? TEXT("none") : Bk > Fr ? TEXT("behind") : TEXT("AHEAD"),
		bSeatOk && bBodyOk ? TEXT("OK") : TEXT("FLIPPED"));
}

bool ASeats::StandSpot(int32 I, FVector& At, float& Yaw)
{
	if (!One.IsValid() || !One->Spots.IsValidIndex(I)) return false;
	const ABabylonModel::FSeatSpot& S = One->Spots[I];
	At = S.At + S.Facing * 70.f + FVector(0, 0, 92.f - S.SeatH);
	Yaw = (-S.Facing).Rotation().Yaw;
	return true;
}

void ASeats::Gather()
{
	ADP_PERF_SCOPE("the seats: gathering every place to sit", 4.f);
	Spots.Reset();
	for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It) It->SeatSpots(Spots);
	// a seat's pieces outlined when you're near
	for (int32 Si = 0; Si < Spots.Num(); ++Si) for (const auto& P : Spots[Si].Parts) { bool bNew = false; Outlined.Add(P, &bNew); if (!bNew) AInteractOutline::Add(static_cast<UPrimitiveComponent*>(P.Key), P.Value, (uint64(1) << 62) + uint64(Si)); }
	static int32 Was = -1; if (Spots.Num() != Was) { Was = Spots.Num(); ADP_LOG(TEXT("Seats: %d places to sit"), Spots.Num()); }
}

bool ASeats::Interact(APawn* Who)
{
	if (!One.IsValid() || !Who) return false;
	ASeats* S = One.Get();
	if (S->State == EState::Sat) { S->Stand(); return true; }
	if (S->State != EState::None) return true;   // (on the way down or up)
	AADPWalker* Me = Cast<AADPWalker>(Who);
	if (!Me) return false;
	const int32 I = NearestTo(Me->GetActorLocation());
	if (I == INDEX_NONE) return false;
	if (IsTaken(I) && !S->Spots[I].bYours) { ADP_LOG(TEXT("Seats: someone's sitting there")); return false; }
	if (IsTaken(I))
	{
		// your own chair with someone on it: they're asked to get up and move aside; you sit as soon as it's free
		if (FTaken* T = S->Taken.Find(SpotKey(S->Spots[I].At))) if (T->AskToLeave) T->AskToLeave();
		S->Waiting = Me; S->WaitSeat = I; S->WaitT = 0.f;
		ADP_LOG(TEXT("Seats: your chair is taken - asking them to get up"));
		return true;
	}
	S->Sit(Me, I);
	return true;
}

void ASeats::Sit(AADPWalker* Me, int32 I)
{
	const ABabylonModel::FSeatSpot& Sp = Spots[I];
	USkeletalMesh* Body = Me->BodyMesh();
	if (!Rider) { Rider = NewObject<UPoseableMeshComponent>(this); Rider->SetupAttachment(RootComponent); Rider->SetVisibility(false); Rider->RegisterComponent(); }
	if (!Frame) { Frame = NewObject<USceneComponent>(this); Frame->SetupAttachment(RootComponent); Frame->RegisterComponent(); }
	if (!Body) { ADP_LOG(TEXT("Seats: no body of your own to sit with")); return; }
	Rider->SetSkinnedAssetAndUpdate(Body);
	if (!Rig.Init(Rider)) { ADP_WARN(TEXT("Seats: your body couldn't be rigged")); return; }
	// the seat's frame: on the floor under the seat's middle, its +Y the way it faces (the body's own forward)
	// (the seat's point is near its front edge: the frame 20 cm back into the seat, so you sit on it, not on its edge)
	const FVector Floor = Sp.At - Sp.Facing.GetSafeNormal2D() * 20.f - FVector(0, 0, Sp.SeatH);
	Frame->SetWorldLocationAndRotation(Floor, FRotator(0.f, Sp.Facing.Rotation().Yaw - 90.f, 0.f));
	// where you start: standing just in front of it, facing away from it; where you end: on it
	const float PH = Rig.PelvisHeight();
	Standing = FDriverPose(); Standing.Facing = FVector(0, 1, 0);
	Standing.Pelvis = FVector(0.f, 30.f, PH);
	// (left and right: side 0 is the body's right, its foot and hand on the frame's +x - with -x the legs and arms crossed)
	Standing.Foot[0] = FVector(12.f, 32.f, 0.f); Standing.Foot[1] = FVector(-12.f, 32.f, 0.f);
	Sitting = FDriverPose(); Sitting.Facing = FVector(0, 1, 0); Sitting.Lean = -6.f;
	// (owner, 2026-11-19: the sitting IK step on your own avatar too - the seat's real surface measured straight down at
	// the seat, the hips one thigh's thickness (your own body's) over it; the hands on your thighs in Place)
	float Surf = Sp.SeatH;
	{
		FHitResult Hs; FCollisionQueryParams Qs(SCENE_QUERY_STAT(SeatTop), false, Me);
		if (GetWorld()->LineTraceSingleByChannel(Hs, Sp.At + FVector(0, 0, 80.f), Sp.At - FVector(0, 0, 60.f), ECC_Visibility, Qs)) Surf = Hs.ImpactPoint.Z - Floor.Z;
	}
	Sitting.Pelvis = FVector(0.f, -4.f, Surf + Rig.ThighRadius());
	Sitting.Foot[0] = FVector(15.f, 40.f, 0.f); Sitting.Foot[1] = FVector(-15.f, 40.f, 0.f);
	for (int32 s = 0; s < 2; ++s) { Sitting.bHand[s] = true; Sitting.Hand[s] = FVector(s == 0 ? 13.f : -13.f, 22.f, Sp.SeatH + 14.f); Sitting.Grip[s] = 0.25f; }
	Pose = Me->LendBody(Frame);
	if (!Pose) { ADP_WARN(TEXT("Seats: your body couldn't be lent to the seat")); return; }
	Sitter = Me; Seat = I; State = EState::Down; T = 0.f; Before = Me->GetActorLocation();
	Fwd = 0.f; Lift = 0.f; bCushionOk = false; CushionTries = 0;
	Taken.Add(SpotKey(Sp.At), FTaken{Me, nullptr});
	Me->GetCharacterMovement()->DisableMovement();
	Me->SetActorEnableCollision(false);
	if (APlayerController* PC = Cast<APlayerController>(Me->GetController())) { FRotator R = PC->GetControlRotation(); R.Yaw = Sp.Facing.Rotation().Yaw; PC->SetControlRotation(R); }
	Place(0.f);
	ADP_LOG(TEXT("Seats: sitting down (seat %d, %.0f cm high)"), I, Sp.SeatH);
}

void ASeats::Stand()
{
	if (State != EState::Sat) return;
	State = EState::Up; T = 0.f;
	ADP_LOG(TEXT("Seats: standing up"));
}

void ASeats::Place(float K)
{
	AADPWalker* Me = Sitter.Get();
	if (!Me || !Frame) return;
	// the pose between standing and sitting, eased; the hips travel back and down over the feet
	const float E = FMath::SmoothStep(0.f, 1.f, K);
	FDriverPose P = Sitting;
	P.Pelvis = FMath::Lerp(Standing.Pelvis, Sitting.Pelvis, E);
	P.Pelvis.Y = FMath::Lerp(Standing.Pelvis.Y, Sitting.Pelvis.Y, FMath::SmoothStep(0.f, 1.f, FMath::Min(1.f, K * 1.3f)));
	P.Lean = FMath::Lerp(0.f, Sitting.Lean, E) + 22.f * FMath::Sin(PI * E);   // (leaning forward on the way, as you do)
	for (int32 s = 0; s < 2; ++s) { P.Foot[s] = FMath::Lerp(Standing.Foot[s], Sitting.Foot[s], E); P.bHand[s] = E > 0.35f; }
	P.Pelvis.Z += Lift * E;
	Rig.Pose(P);
	// (the seat under the thighs, the same IK step as the passengers': forward a little, then up, until they lie on it)
	if (K >= 1.f && !bCushionOk && Frame)
	{
		const float Deep = Rig.ThighsInSeat(GetWorld(), Frame->GetComponentTransform(), Rig.ThighRadius(), Me);
		if (Deep <= 0.4f || CushionTries > 12) { bCushionOk = true; ADP_LOG(TEXT("Seats: on the cushion - forward %.0f cm, up %.1f cm"), Fwd, Lift); }
		else { ++CushionTries; if (Fwd < 12.f) { Fwd += 2.f; Frame->AddWorldOffset(Spots[Seat].Facing.GetSafeNormal2D() * 2.f); } else if (Lift < 8.f) Lift = FMath::Min(8.f, Lift + Deep); else bCushionOk = true; }
	}
	if (E > 0.35f) Rig.RestHandsOnThighs(P, Rig.ThighRadius(), FMath::Clamp((E - 0.35f) / 0.3f, 0.f, 1.f));   // (the sitting IK step)
	if (Pose) Pose->Local = Rig.LocalPose();
	if (K >= 1.f && Spots.IsValidIndex(Seat)) LogFacing(TEXT("you"), Seat, Spots[Seat].At, Spots[Seat].Facing, Spots[Seat].SeatH, Frame->GetComponentTransform().TransformVector(Rig.KneeAt(0) + Rig.KneeAt(1) - Rig.HipAt(0) - Rig.HipAt(1)), Me);
	// your view goes with your head: the capsule kept so its eyes are where the body's are
	const FVector Eyes = Frame->GetComponentTransform().TransformPosition(Rig.Eyes());
	const float Yaw = Me->GetControlRotation().Yaw;
	Me->SetActorLocationAndRotation(Eyes - FRotator(0.f, Yaw, 0.f).RotateVector(FVector(14.f, 0.f, 70.f)), FRotator(0.f, Yaw, 0.f));
	Me->ShowHead(Me->IsThirdPerson());
}

void ASeats::Tick(float Dt)
{
	ADP_PERF_SCOPE("the seats (each frame)", 4.f);
	Super::Tick(Dt);
	One = this;
	// the seats are gathered again only when the buildings change (one added or gone - the town comes in late, say):
	// gathering walks every piece of every building and took over 100 ms - done every 15 s it was the game's
	// stutter (owner 2026-11-16, found by the performance log's stall watch: ASeats::Gather -> SeatSpots)
	if ((GatherIn -= Dt) <= 0.f && State == EState::None)
	{
		GatherIn = 2.f;
		int32 Models = 0; for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It) ++Models;
		if (Models != GatheredModels) { GatheredModels = Models; Gather(); ++GatherRounds; }
	}
	if (AADPWalker* W = Waiting.Get())
	{
		WaitT += Dt;
		if (!IsTaken(WaitSeat) && State == EState::None) { Waiting.Reset(); Sit(W, WaitSeat); }
		else if (WaitT > 20.f || FVector::Dist2D(W->GetActorLocation(), Spots.IsValidIndex(WaitSeat) ? Spots[WaitSeat].At : W->GetActorLocation()) > 250.f) Waiting.Reset();   // (gave up, or walked off)
	}
	if (State == EState::None) return;
	AADPWalker* Me = Sitter.Get();
	if (!Me) { State = EState::None; return; }
	T += Dt;
	if (State == EState::Down) { Place(FMath::Min(1.f, T / SitTime)); if (T >= SitTime) { State = EState::Sat; ADP_LOG(TEXT("Seats: seated")); } }
	else if (State == EState::Sat) Place(1.f);
	else if (State == EState::Up)
	{
		Place(FMath::Max(0.f, 1.f - T / SitTime));
		if (T >= SitTime)
		{
			// back on your feet, a step in front of the seat
			FVector At; float Yaw;
			Me->TakeBodyBack();
			Me->SetActorEnableCollision(true);
			// (never into a table or a wall: the nearest clear spot to stand, else straight back where you were before)
			if (StandSpot(Seat, At, Yaw))
			{
				FRotator R(0.f, Yaw, 0.f);
				if (!GetWorld()->FindTeleportSpot(Me, At, R)) At = Before;
				Me->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			}
			Me->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Release(Me);
			State = EState::None; Sitter.Reset(); Pose = nullptr;
			ADP_LOG(TEXT("Seats: on your feet"));
		}
	}
}
