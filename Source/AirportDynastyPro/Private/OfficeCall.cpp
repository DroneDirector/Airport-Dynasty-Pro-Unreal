#include "OfficeCall.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "People.h"
#include "Seats.h"
#include "StaffPeople.h"
#include "BabylonModel.h"
#include "TerminalPlan.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"

// the office (Pets.cpp's GOfficeThings): room metres x = plan x - 114, y = plan z; Floor 2 at 6.01 m. The door
// (D-F2-OFFICE-1-1) is at room x 9.9..11.1 on y 9, swinging in; the staff corridor beyond it (y > 9).
namespace
{
	constexpr float FY = 6.01f;
	const FVector2D CorridorFar(19.5f, 10.0f), OutsideDoor(10.5f, 9.7f), InDoorway(10.5f, 9.0f), InsideDoor(10.45f, 7.6f);
	// round the furniture to the visitor's chair: clear of the armchair, desk and meeting table (a body 20 cm each side)
	const FVector2D ToChair[] = {FVector2D(7.6f, 6.6f), FVector2D(5.0f, 6.6f), FVector2D(4.3f, 5.9f)};
	const FVector2D SitStep(4.45f, 5.0f);          // (beside the chair's front: its usual 60 cm front is inside the desk)
	const FVector VCSit(118.25f, 6.52f, 4.30f);    // Data/owner-office-points.json VC_SitPoint (plan)
}

FVector UOfficeCall::Room(float X, float Y) { return FTerminalPlan::ToWorld(X + 114.f, FY, Y); }

UOfficeCall* UOfficeCall::Get(UWorld* W) { return W ? W->GetSubsystem<UOfficeCall>() : nullptr; }

bool UOfficeCall::Call(const FString& StaffId)
{
	if (Phase != EPhase::None) { ADP_LOG(TEXT("OfficeCall: %s is already called in"), *Name); return false; }
	UStaffPeople* SP = UStaffPeople::Get(GetWorld());
	if (!SP || !SP->CallIn(StaffId, Face, Name)) { ADP_LOG(TEXT("OfficeCall: %s can't come just now"), *StaffId); return false; }
	Id = StaffId; Seat = INDEX_NONE; bTalk = false; bToldWait = false; Waited = 0.f;
	// the visitor's chair (the seat within 60 cm of its point) and the dog (both found once here, never per frame)
	const FVector VC = FTerminalPlan::ToWorld(VCSit.X, VCSit.Y, VCSit.Z);
	for (int32 i = 0, N = ASeats::Count(); i < N; ++i) { FVector At, F; float H; if (ASeats::SeatOf(i, At, F, H) && FVector::Dist(At, VC) < 60.f) { Seat = i; break; } }
	Pet = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->ActorHasTag(TEXT("HomePet"))) { Pet = *It; break; }
	ADP_LOG(TEXT("OfficeCall: %s called into your office (visitor's chair: seat %d)"), *Name, Seat);
	Go(EPhase::Spawn);
	return true;
}

bool UOfficeCall::InYourView(const FVector& At) const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return false;
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation(), P = At + FVector(0, 0, 150.f);
	if (FVector::DotProduct(PC->PlayerCameraManager->GetCameraRotation().Vector(), (P - Eye).GetSafeNormal()) < 0.3f) return false;
	FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(OfficeCallSeen), false, PC->GetPawn());
	if (V.IsValid()) Q.AddIgnoredActor(V.Get());
	return !GetWorld()->LineTraceSingleByChannel(H, Eye, P, ECC_Visibility, Q);
}

bool UOfficeCall::WalkLegs(float Dt)
{
	AADPPeople* People = AADPPeople::Get(GetWorld());
	AActor* A = V.Get();
	if (!People || !A) return false;
	while (Legs.Num())
	{
		const FVector Me = A->GetActorLocation(), Dir = (Legs[0] - Me).GetSafeNormal2D();
		// (you or the dog just ahead: they stop and wait - never through anyone)
		const AActor* You = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
		const AActor* Others[] = {You, Pet.Get()};
		for (const AActor* O : Others)
		{
			if (!O) continue;
			const FVector To = O->GetActorLocation() - Me;
			if (FMath::Abs(To.Z) < 200.f && To.Size2D() < 90.f && FVector::DotProduct(Dir, To.GetSafeNormal2D()) > 0.2f)
			{
				Waited += Dt;
				if (Waited > 3.f && !bToldWait) { bToldWait = true; ADP_LOG(TEXT("OfficeCall: %s waits for %s to step aside"), *Name, O == You ? TEXT("you") : TEXT("the dog")); }
				return false;
			}
		}
		Waited = 0.f;
		if (!People->Walk(A, Legs[0], Dt, 15.f)) return false;
		Legs.RemoveAt(0);
	}
	return true;
}

void UOfficeCall::Knock(const FVector& At)
{
	// three knocks on wood (there is no knock recording in the project yet: made here, 0.75 s)
	const int32 Rate = 22050, N = Rate * 3 / 4;
	TArray<int16> Buf; Buf.SetNumZeroed(N);
	FRandomStream R(7);
	for (int32 k = 0; k < 3; ++k)
	{
		const int32 S0 = int32(k * 0.24f * Rate);
		for (int32 i = 0; i < Rate / 20 && S0 + i < N; ++i)
		{
			const float t = float(i) / Rate, E = FMath::Exp(-t / 0.011f);
			const float s = 0.7f * FMath::Sin(2.f * PI * 170.f * t) + 0.35f * FMath::Sin(2.f * PI * 410.f * t) + 0.25f * R.FRandRange(-1.f, 1.f);
			Buf[S0 + i] = int16(FMath::Clamp(s * E, -1.f, 1.f) * 20000.f);
		}
	}
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
	W->SetSampleRate(Rate); W->NumChannels = 1; W->Duration = float(N) / Rate; W->bLooping = false; W->SoundGroup = SOUNDGROUP_Effects;
	W->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Buf.Num() * sizeof(int16));
	UGameplayStatics::SpawnSoundAtLocation(GetWorld(), W, At);
}

void UOfficeCall::Finish(const TCHAR* Why)
{
	if (AActor* A = V.Get()) { ASeats::Release(A); A->Destroy(); }
	if (UStaffPeople* SP = UStaffPeople::Get(GetWorld())) SP->CallOver(Id);
	ADP_LOG(TEXT("OfficeCall: %s - %s"), *Name, Why);
	V = nullptr; Legs.Reset(); Phase = EPhase::None;
}

void UOfficeCall::Tick(float Dt)
{
	if (Phase == EPhase::None) return;
	ADP_PERF_SCOPE("OfficeCall", 0.2f);
	AADPPeople* People = AADPPeople::Get(GetWorld());
	if (!People) return;
	T += Dt;
	AActor* A = V.Get();
	if (Phase != EPhase::Spawn && !A) { Finish(TEXT("their body went - back to work")); return; }
	const FVector Door = Room(10.5f, 9.0f);
	auto FaceTo = [&](const FVector& P) { const FVector D = (P - A->GetActorLocation()).GetSafeNormal2D(); if (!D.IsNearlyZero()) A->SetActorRotation(FRotator(0.f, D.Rotation().Yaw - 90.f, 0.f)); };
	switch (Phase)
	{
	case EPhase::Spawn:
	{
		// they come from along the staff corridor: made there only while you can't see the spot
		const FVector At = Room(CorridorFar.X, CorridorFar.Y);
		if (T < 1.f) break;
		if (InYourView(At)) { T = 0.f; if (!bToldWait) { bToldWait = true; ADP_LOG(TEXT("OfficeCall: %s waits out of your view"), *Name); } break; }
		bToldWait = false;
		V = People->Spawn(Face, At, (Room(OutsideDoor.X, OutsideDoor.Y) - At).Rotation().Yaw - 90.f);
		if (!V.IsValid()) { Finish(TEXT("couldn't be made in 3D")); return; }
		if (UStaffPeople* SP = UStaffPeople::Get(GetWorld())) SP->Visitor(Id, V.Get());
		if (AADPPeople::FWho* W = People->Find(V.Get())) W->First = Name;
		Legs = {Room(OutsideDoor.X, OutsideDoor.Y)};
		Go(EPhase::In);
		break;
	}
	case EPhase::In: if (WalkLegs(Dt)) { FaceTo(Door); Go(EPhase::Knock); } break;
	case EPhase::Knock:
		if (T - Dt < 0.3f && T >= 0.3f) { Knock(Door); ADP_LOG(TEXT("OfficeCall: %s knocks"), *Name); }
		if (T > 1.8f) { ABabylonModel::StaffDoor(Door, true); Go(EPhase::Opening); }
		break;
	case EPhase::Opening:
		if (ABabylonModel::StaffDoor(Door, true) > 0.9f || T > 3.f) { Legs = {Room(InDoorway.X, InDoorway.Y), Room(InsideDoor.X, InsideDoor.Y)}; Go(EPhase::Through); }
		break;
	case EPhase::Through: ABabylonModel::StaffDoor(Door, true); if (WalkLegs(Dt)) { FaceTo(Door); Go(EPhase::Shut); } break;
	case EPhase::Shut:
		if (T > 0.6f)
		{
			ABabylonModel::StaffDoorShut(Door);
			Legs.Reset(); for (const FVector2D& P : ToChair) Legs.Add(Room(P.X, P.Y));
			Go(EPhase::ToChair);
		}
		break;
	case EPhase::ToChair:
	{
		if (!WalkLegs(Dt)) break;
		const FVector Lo = Room(0.36f, 0.f), Hi = Room(12.3f, 9.f);
		const TArray<FBox2D> Area = {FBox2D(FVector2D(FMath::Min(Lo.X, Hi.X), FMath::Min(Lo.Y, Hi.Y)), FVector2D(FMath::Max(Lo.X, Hi.X), FMath::Max(Lo.Y, Hi.Y)))};
		const FVector Step = Room(SitStep.X, SitStep.Y);
		if (Seat == INDEX_NONE || !People->SitOn(A, Seat, Area, Step.Z, &Step)) { ADP_WARN(TEXT("OfficeCall: %s couldn't take the visitor's chair (seat %d) - talks standing"), *Name, Seat); Go(EPhase::Seated); break; }
		Go(EPhase::Sitting);
		break;
	}
	case EPhase::Sitting: if ((ASeats::IsSatOn(Seat) && T > 2.f) || T > 15.f) Go(EPhase::Seated); break;
	case EPhase::Seated:
	{
		// the talk: once you're in the office with them (the same talk card, camera and blur as anywhere)
		const AActor* You = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
		if (!bTalk)
		{
			if (You && FVector::Dist(You->GetActorLocation(), A->GetActorLocation()) < 700.f && !People->MenuOpen()) { People->Open(A); bTalk = People->MenuPerson() == A; }
			break;
		}
		if (People->MenuPerson() == A) break;
		ADP_LOG(TEXT("OfficeCall: talk with %s over - back to work"), *Name);
		if (People->IsHeld(A)) People->StandFrom(A);
		Go(EPhase::Up);
		break;
	}
	case EPhase::Up:
		if (People->IsHeld(A) && T < 10.f) break;
		ASeats::Release(A);
		Legs.Reset(); Legs.Add(Room(ToChair[2].X, ToChair[2].Y)); Legs.Add(Room(ToChair[1].X, ToChair[1].Y)); Legs.Add(Room(ToChair[0].X, ToChair[0].Y)); Legs.Add(Room(InsideDoor.X, InsideDoor.Y));
		Go(EPhase::Out);
		break;
	case EPhase::Out: if (WalkLegs(Dt)) { FaceTo(Door); ABabylonModel::StaffDoor(Door, true); Go(EPhase::OpenOut); } break;
	case EPhase::OpenOut:
		if (ABabylonModel::StaffDoor(Door, true) > 0.9f || T > 3.f) { Legs = {Room(InDoorway.X, InDoorway.Y), Room(OutsideDoor.X, OutsideDoor.Y)}; Go(EPhase::ThroughOut); }
		break;
	case EPhase::ThroughOut: ABabylonModel::StaffDoor(Door, true); if (WalkLegs(Dt)) { FaceTo(Door); Go(EPhase::ShutOut); } break;
	case EPhase::ShutOut: if (T > 0.6f) { ABabylonModel::StaffDoorShut(Door); Legs = {Room(CorridorFar.X, CorridorFar.Y)}; Go(EPhase::Away); } break;
	case EPhase::Away:
		if (Legs.Num()) { WalkLegs(Dt); T = 0.f; break; }
		if (T > 1.f) { if (!InYourView(A->GetActorLocation())) Finish(TEXT("back at their job")); else T = 0.f; }
		break;
	default: break;
	}
}
