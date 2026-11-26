// Sitting down (Seats.h).
#include "Seats.h"
#include "PerfLog.h"
#include "ADPLog.h"
#include "ADPWalker.h"
#include "DriverPoseAnim.h"
#include "InteractOutline.h"
#include "PaxNav.h"
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
		if (S.bAisle && FVector::DotProduct(At - S.At, S.Facing) < -5.f) continue;   // (a bus's seat: never through its side from outside)
		const float Score = FVector::DotProduct(Look.GetSafeNormal2D(), (S.At - Eye).GetSafeNormal2D()) * 2.f - D / Reach;
		if (Score > BS) { BS = Score; Best = i; }
	}
	return Best;
}

bool ASeats::NearSeat(const FVector& At) { const int32 I = NearestTo(At); return I != INDEX_NONE && (!IsSatOn(I) || One->Spots[I].bYours); }

bool ASeats::IsTaken(int32 Seat)
{
	if (!One.IsValid() || !One->Spots.IsValidIndex(Seat)) return false;
	const FTaken* T = One->Taken.Find(SpotKey(One->Spots[Seat].At));
	return T && T->Who.IsValid();
}

bool ASeats::IsSatOn(int32 Seat)
{
	if (!One.IsValid() || !One->Spots.IsValidIndex(Seat)) return false;
	const FTaken* T = One->Taken.Find(SpotKey(One->Spots[Seat].At));
	return T && T->Who.IsValid() && T->bSat;
}

bool ASeats::OwnerOnly(int32 I)
{
	return One.IsValid() && One->Spots.IsValidIndex(I) && One->Spots[I].bOwnerOnly;
}

bool ASeats::ClaimSeat(AActor* Who, int32 I, TFunction<void()> AskToLeave)
{
	if (!One.IsValid() || !Who || !One->Spots.IsValidIndex(I)) return false;
	if (One->Spots[I].bOwnerOnly) return false;   // (the owner's chair: only you, and you never claim)
	ASeats* S = One.Get();
	if (S->State != EState::None && S->Seat == I) return false;   // (you are on it, or on the way down or up)
	const FIntVector Key = SpotKey(S->Spots[I].At);
	const FTaken* T = S->Taken.Find(Key);
	if (T && T->Who.IsValid()) { if (T->Who.Get() != Who) return false; if (!AskToLeave) return true; }
	Release(Who);
	S->Taken.Add(Key, FTaken{Who, MoveTemp(AskToLeave)});
	return true;
}

void ASeats::Sat(AActor* Who)
{
	if (One.IsValid() && Who) for (auto& P : One->Taken) if (P.Value.Who.Get() == Who) P.Value.bSat = true;
}

bool ASeats::HoldsAt(const AActor* Who, const FVector& SeatAt)
{
	const FTaken* T = One.IsValid() ? One->Taken.Find(SpotKey(SeatAt)) : nullptr;
	return T && Who && T->Who.Get() == Who;
}

bool ASeats::Holds(const AActor* Who, int32 I) { return One.IsValid() && One->Spots.IsValidIndex(I) && HoldsAt(Who, One->Spots[I].At); }

void ASeats::SeatsOf(const ABabylonModel* Model, TArray<int32>& Out)
{
	Out.Reset(); if (!One.IsValid() || !Model) return;
	if (const FGot* G = One->Got.FindByPredicate([Model](const FGot& X) { return X.Model.Get() == Model; })) Out = G->Slots;
}

int32 ASeats::Innermost(int32 I)
{
	if (!One.IsValid() || !One->Spots.IsValidIndex(I)) return I;
	const ABabylonModel::FSeatSpot& S = One->Spots[I];
	const ABabylonModel* M = S.Parts.Num() && S.Parts[0].Key ? Cast<ABabylonModel>(S.Parts[0].Key->GetOwner()) : nullptr;
	const FGot* G = M ? One->Got.FindByPredicate([M](const FGot& X) { return X.Model.Get() == M; }) : nullptr; if (!G) return I;
	// its row: the seats beside it, facing the same way, each within 95 cm of the next
	const FVector F = S.Facing.GetSafeNormal2D(), Side(-F.Y, F.X, 0.f);
	TArray<TPair<float, int32>, TInlineAllocator<24>> Row;
	for (const int32 k : G->Slots)
	{
		const ABabylonModel::FSeatSpot& X = One->Spots[k]; const FVector D = X.At - S.At;
		if (FMath::Abs(D.Z) > 30.f || FMath::Abs(FVector::DotProduct(D, F)) > 30.f || FVector::DotProduct(X.Facing, F) < 0.9f) continue;
		Row.Emplace(float(FVector::DotProduct(D, Side)), k);
	}
	Row.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	const int32 At = Row.IndexOfByPredicate([I](const TPair<float, int32>& X) { return X.Value == I; }); if (At == INDEX_NONE) return I;
	int32 A = At, B = At;
	while (A > 0 && Row[A].Key - Row[A - 1].Key < 95.f) --A;
	while (B < Row.Num() - 1 && Row[B + 1].Key - Row[B].Key < 95.f) ++B;
	if (A == B) return I;
	// a wall at one end only (a short look sideways past the end seat, at the shoulders' height)
	auto Wall = [&](int32 k, float Dir)
	{
		const FVector P = One->Spots[Row[k].Value].At - F * 10.f + FVector(0, 0, 45.f); FHitResult R; FCollisionQueryParams Q(SCENE_QUERY_STAT(SeatRowEnd), false);
		if (!One->GetWorld()->LineTraceSingleByChannel(R, P, P + Side * (Dir * 85.f), ECC_Visibility, Q)) return false;
		return !Cast<APawn>(R.GetActor()) && FMath::Abs(R.ImpactNormal.Z) < 0.4f;
	};
	const bool bWa = Wall(A, -1.f), bWb = Wall(B, 1.f); if (bWa == bWb) return I;
	// from the open end in, up to the first one taken
	int32 In = I; bool bAny = false;
	for (int32 k = bWa ? B : A; k >= A && k <= B; k += bWa ? -1 : 1) { if (IsTaken(Row[k].Value)) break; In = Row[k].Value; bAny = true; }
	return bAny ? In : I;
}

bool ASeats::ClaimCab(const AActor* Vehicle, int32 CabSeat, AActor* Who, bool bForce)
{
	if (!One.IsValid() || !Vehicle || !Who) return true;
	TArray<FCab>& C = One->Cabs;
	const bool bYou = Cast<APawn>(Who) != nullptr;
	for (int32 k = C.Num() - 1; k >= 0; --k)
	{
		if (!C[k].Vehicle.IsValid() || !C[k].Who.IsValid()) { C.RemoveAtSwap(k); continue; }
		if (C[k].Vehicle.Get() != Vehicle || C[k].Seat != CabSeat || C[k].Who.Get() == Who) continue;
		const bool bYours = Cast<APawn>(C[k].Who.Get()) != nullptr;
		const bool bAway = !bYours && C[k].bSat && FVector::Dist2D(C[k].Who->GetActorLocation(), Vehicle->GetActorLocation()) > 1500.f;   // (he sat there once and is long somewhere else)
		if (!bAway && (bYours || C[k].bSat || !(bYou || bForce))) return false;
		C.RemoveAtSwap(k);
	}
	if (C.ContainsByPredicate([&](const FCab& X) { return X.Who.Get() == Who && X.Vehicle.Get() == Vehicle && X.Seat == CabSeat; })) return true;
	ReleaseCab(Who);
	FCab N; N.Vehicle = Vehicle; N.Seat = CabSeat; N.Who = Who; N.bSat = bYou;   // (you: blocked from the moment you choose it)
	C.Add(N);
	return true;
}

void ASeats::CabSat(AActor* Who) { if (One.IsValid() && Who) for (FCab& X : One->Cabs) if (X.Who.Get() == Who) X.bSat = true; }

AActor* ASeats::CabBy(const AActor* Vehicle, int32 CabSeat, bool bOnlySat)
{
	if (One.IsValid() && Vehicle) for (const FCab& X : One->Cabs) if (X.Vehicle.Get() == Vehicle && X.Seat == CabSeat && (X.bSat || !bOnlySat)) return X.Who.Get();
	return nullptr;
}

void ASeats::ReleaseCab(AActor* Who)
{
	if (One.IsValid()) One->Cabs.RemoveAllSwap([Who](const FCab& X) { return !X.Who.IsValid() || X.Who.Get() == Who; });
}

void ASeats::Carry(const ABabylonModel* Vehicle, const FTransform& Was, const FTransform& Now, AActor* By)
{
	if (!One.IsValid() || !Vehicle) return;
	ASeats* S = One.Get();
	if (By && S->Mover.Get() != By) { S->Mover = By; S->AddTickPrerequisiteActor(By); }
	const FGot* G = S->Got.FindByPredicate([&](const FGot& X) { return X.Model.Get() == Vehicle; }); if (!G) return;
	const FTransform D = Was.Inverse() * Now; const float Dy = float(FRotator::NormalizeAxis(Now.Rotator().Yaw - Was.Rotator().Yaw));
	TArray<TPair<FVector, FTaken>, TInlineAllocator<12>> Moved;
	for (const int32 I : G->Slots)
	{
		ABabylonModel::FSeatSpot& P = S->Spots[I]; FTaken Tk; const bool bTk = S->Taken.RemoveAndCopyValue(SpotKey(P.At), Tk);
		P.At = D.TransformPosition(P.At); P.Facing = FRotator(0.f, Dy, 0.f).RotateVector(P.Facing);
		if (bTk) Moved.Emplace(P.At, MoveTemp(Tk));
	}
	for (auto& M : Moved) S->Taken.Add(SpotKey(M.Key), MoveTemp(M.Value));
	if (S->State == EState::None || !S->Frame || !G->Slots.Contains(S->Seat)) return;
	S->Frame->SetWorldLocationAndRotation(D.TransformPosition(S->Frame->GetComponentLocation()), FRotator(0.f, S->Frame->GetComponentRotation().Yaw + Dy, 0.f));
	S->Before = D.TransformPosition(S->Before);
	if (AADPWalker* Me = S->Sitter.Get()) if (APlayerController* PC = Cast<APlayerController>(Me->GetController())) { FRotator R = PC->GetControlRotation(); R.Yaw += Dy; PC->SetControlRotation(R); }
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

bool ASeats::FrontOf(int32 I, FVector& At)
{
	if (!One.IsValid() || !One->Spots.IsValidIndex(I)) return false;
	ASeats& O = *One;
	if (O.FrontOk.Num() < O.Spots.Num()) { O.FrontOk.SetNumZeroed(O.Spots.Num()); O.FrontAt.SetNumZeroed(O.Spots.Num()); }
	if (O.FrontOk[I] == 0) O.CheckFront(I);
	if (O.FrontOk[I] == -1) return false;
	const ABabylonModel::FSeatSpot& S = O.Spots[I];
	At = O.FrontOk[I] == 1 ? O.FrontAt[I] : S.At - FVector(0, 0, S.SeatH) + S.Facing.GetSafeNormal2D() * 60.f;
	return true;
}

void ASeats::CheckFront(int32 I)
{
	UWorld* W = GetWorld();
	const ABabylonModel::FSeatSpot& S = Spots[I];
	if (S.At.Z < -1.0e6) { FrontOk[I] = 2; return; }   // (an empty place)
	if (!PaxNav::Started(W)) return;   // (not known yet: the old spot for now)
	if (!PaxNav::Covers(W, S.At) || PaxNav::Failed(W)) { FrontOk[I] = 2; return; }
	if (!PaxNav::Ready(W)) return;
	const FVector F = S.Facing.GetSafeNormal2D(), Floor = S.At - FVector(0, 0, S.SeatH);
	// (the front edge of its footprint, as PaxNav cut it: + the cut's 12 cm, + the map's own 30 cm body radius round a hole)
	float Edge = 30.f; FVector C, H; FQuat Q;
	if (ABabylonModel::SeatBox(S, C, Q, H)) Edge = FVector::DotProduct(C - S.At, F) + H.X;
	const float Hole = Edge + 12.f + 30.f;
	for (const float Out : {Hole + 8.f, Hole + 25.f, Hole + 45.f})
	{
		FVector On;
		if (PaxNav::Nearest(W, Floor + F * Out + FVector(0, 0, 10.f), 30.f, On) && FVector::DotProduct(On - Floor, F) > Hole - 5.f) { FrontOk[I] = 1; FrontAt[I] = On; return; }
	}
	FrontOk[I] = -1; ++Unusable;
	ADP_WARN(TEXT("Seats: seat %d at (%.0f, %.0f, %.0f), facing (%.2f, %.2f), has no front on the walking map (against a window or wall, or the rows too close) - nobody is sent to it"), I, S.At.X, S.At.Y, S.At.Z, F.X, F.Y);
}

bool ASeats::StandSpot(int32 I, FVector& At, float& Yaw)
{
	if (!One.IsValid() || !One->Spots.IsValidIndex(I)) return false;
	const ABabylonModel::FSeatSpot& S = One->Spots[I];
	At = S.At + S.Facing * 70.f + FVector(0, 0, 92.f - S.SeatH);
	Yaw = (-S.Facing).Rotation().Yaw;
	return true;
}

bool ASeats::Gather()
{
	ADP_PERF_SCOPE("the seats: gathering the new places to sit", 4.f);
	const double Until = FPlatformTime::Seconds() + 0.003;
	auto Vacate = [this](TArray<int32>& Slots)
	{
		for (int32 k = Slots.Num() - 1; k >= 0; --k) { const int32 I = Slots[k]; Spots[I] = ABabylonModel::FSeatSpot(); Spots[I].At = FVector(0, 0, -1.0e7); Vacant.Add(I); }
		Slots.Reset();
	};
	TMap<const ABabylonModel*, int32> Has;
	for (int32 G = Got.Num() - 1; G >= 0; --G) if (!Got[G].Model.IsValid()) { Vacate(Got[G].Slots); Got.RemoveAtSwap(G); }
	for (int32 G = 0; G < Got.Num(); ++G) Has.Add(Got[G].Model.Get(), G);
	bool bAll = true;
	for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
	{
		const uint32 Stamp = It->SeatStamp();
		const int32* Old = Has.Find(*It);
		if (Old && Got[*Old].Stamp == Stamp) continue;
		if (FPlatformTime::Seconds() > Until) { bAll = false; break; }
		const int32 G = Old ? *Old : Got.AddDefaulted();
		Got[G].Model = *It; Got[G].Stamp = Stamp;
		TArray<ABabylonModel::FSeatSpot> New; It->SeatSpots(New);
		Vacate(Got[G].Slots);
		for (ABabylonModel::FSeatSpot& N : New)
		{
			const int32 Si = Vacant.Num() ? Vacant.Pop() : Spots.AddDefaulted();
			Spots[Si] = MoveTemp(N); Got[G].Slots.Add(Si);
			if (FrontOk.IsValidIndex(Si)) { if (FrontOk[Si] == -1) --Unusable; FrontOk[Si] = 0; }   // (a new seat here: its front looked at again)
			// a seat's pieces outlined when you're near
			for (const auto& P : Spots[Si].Parts) { bool bKnown = false; Outlined.Add(P, &bKnown); if (!bKnown) AInteractOutline::Add(static_cast<UPrimitiveComponent*>(P.Key), P.Value, (uint64(1) << 62) + uint64(Si)); }
		}
	}
	if (bAll && Spots.Num() - Vacant.Num() != Said) { Said = Spots.Num() - Vacant.Num(); ADP_LOG(TEXT("Seats: %d places to sit"), Said); }
	return bAll;
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
	if (IsSatOn(I) && !S->Spots[I].bYours) { ADP_LOG(TEXT("Seats: someone's sitting there")); return false; }
	if (IsSatOn(I))
	{
		// your own chair with someone on it: they're asked to get up and move aside; you sit as soon as it's free
		if (FTaken* T = S->Taken.Find(SpotKey(S->Spots[I].At))) if (T->AskToLeave) T->AskToLeave();
		S->Waiting = Me; S->WaitSeat = I; S->WaitT = 0.f;
		ADP_LOG(TEXT("Seats: your chair is taken - asking them to get up"));
		return true;
	}
	if (IsTaken(I)) ADP_LOG(TEXT("Seats: seat %d is yours - the one only walking to it finds another"), I);   // (to-do 293)
	S->Sit(Me, I);
	return true;
}

bool ASeats::SitYours(APawn* Who, const FVector& SeatWorld)
{
	AADPWalker* Me = Cast<AADPWalker>(Who);
	if (!One.IsValid() || !Me) { ADP_WARN(TEXT("Seats: no seats or no walker to sit down in your office")); return false; }
	ASeats* S = One.Get();
	if (S->State != EState::None) return false;
	int32 Best = INDEX_NONE; double BestD = 80.0;
	for (int32 i = 0; i < S->Spots.Num(); ++i) { const double D = FVector::Dist(S->Spots[i].At, SeatWorld); if (D < BestD) { BestD = D; Best = i; } }
	if (Best == INDEX_NONE) { ADP_WARN(TEXT("Seats: no seat within 80 cm of %s"), *SeatWorld.ToCompactString()); return false; }
	if (IsSatOn(Best)) { ADP_WARN(TEXT("Seats: seat %d is sat on - not sitting you on top"), Best); return false; }
	S->Sit(Me, Best);
	return S->State != EState::None;
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
	Sitter = Me; Seat = I; State = EState::Down; T = 0.f; bKeysFree = false; Before = Me->GetActorLocation();
	Fwd = 0.f; Lift = 0.f; bCushionOk = false; CushionTries = 0;
	Taken.Add(SpotKey(Sp.At), FTaken{Me, nullptr, true});
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
	// only new buildings are gone through, a few milliseconds a frame: going through every piece of every building again
	// whenever one aircraft or vehicle came took up to 590 ms (2026-11-25, seen by the performance log's stall watch)
	if ((GatherIn -= Dt) <= 0.f && State == EState::None) GatherIn = Gather() ? 2.f : 0.f;
	// each seat's front on the walking map, once it is there: a few milliseconds a frame, only the seats not looked at yet
	if (PaxNav::Ready(GetWorld()))
	{
		if (FrontOk.Num() < Spots.Num()) { FrontOk.SetNumZeroed(Spots.Num()); FrontAt.SetNumZeroed(Spots.Num()); }
		const double Until = FPlatformTime::Seconds() + 0.002;
		bool bAll = true;
		for (int32 i = 0; i < Spots.Num(); ++i) if (FrontOk[i] == 0) { if (FPlatformTime::Seconds() > Until) { bAll = false; break; } CheckFront(i); bAll &= FrontOk[i] != 0; }
		if (bAll && Unusable != FrontSaid) { FrontSaid = Unusable; int32 On = 0; for (const int8 K : FrontOk) On += K == 1 || K == -1; ADP_LOG(TEXT("Seats: %d of the %d seats on the walking map have no front anyone can reach - nobody is sent to them"), Unusable, On); }
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
	else if (State == EState::Sat)
	{
		Place(1.f);
		// walking keys or the stick: up you get, as with E / X (owner, 2026-11-25) - once they were let go since you sat down;
		// not while you use a phone or a screen on the seat (the keys are theirs then)
		if (Me->GetLastMovementInputVector().Size() < 0.2) bKeysFree = true;
		else if (bKeysFree) Stand();
	}
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
				// (in a bus that may drive: straight on its floor, in the aisle before the seat - it carries you from there on)
				if (Spots[Seat].bAisle) At.Z -= 24.f;
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
