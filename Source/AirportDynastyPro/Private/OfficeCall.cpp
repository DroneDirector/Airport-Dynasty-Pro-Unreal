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
#include "PaxAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"

// the office (Pets.cpp's GOfficeThings): room metres x = plan x - 114, y = plan z; Floor 2 at 6.01 m. The door
// (D-F2-OFFICE-1-1) is at room x 9.9..11.1 on y 9, swinging in; the staff corridor beyond it (y > 9).
namespace
{
	constexpr float FY = 6.01f;
	// (the leaf: hinged at room x 9.9 on y 9, 1.2 m wide, swinging into the room; its handles Door_HandleIn / Door_HandleOut
	//  in Data/owner-office-points.json, plan (124.98, 7.01, 8.93 / 9.07))
	const FVector2D CorridorFar(19.5f, 10.0f), OutsideDoor(10.65f, 9.5f), InDoorway(10.5f, 9.0f), InsideDoor(10.45f, 7.6f), Hinge(9.9f, 9.0f);
	// round the open leaf's free edge (at 9.9, 7.8) with 45 cm to spare: between the doorway side and the leaf's room face
	const FVector2D RoundLeaf[] = {FVector2D(10.4f, 7.45f), FVector2D(9.9f, 7.3f), FVector2D(9.4f, 7.75f)};
	// the knock: the knuckles on the leaf's corridor face, at 1.45 m, in front of the right shoulder (plan)
	const FVector KnockPlan(124.45f, 7.46f, 9.03f);
	// the knocks in Art/Sounds/Office/Knock.wav (Tools/make_office_knock_sound.py)
	const float KnockHits[] = {0.085f, 0.30f, 0.51f};
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

bool UOfficeCall::Ask(const FString& StaffId, FString& Line)
{
	UStaffPeople* SP = UStaffPeople::Get(GetWorld());
	const FString Who = SP ? SP->NameOf(StaffId) : FString();
	FString Doing, Where;
	if (!SP || StaffId.IsEmpty() || Who.IsEmpty()) { Line = TEXT("they don't work here in the 3D airport"); return false; }
	if (Phase != EPhase::None && Id == StaffId) { Line = FString::Printf(TEXT("%s is already on the way to your office"), *Who); return true; }
	if (Queue.Contains(StaffId)) { Line = FString::Printf(TEXT("%s is busy - they come to your office as soon as they are free"), *Who); return true; }
	if (!SP->Status(StaffId, Doing, Where)) { Line = FString::Printf(TEXT("%s is not at the airport just now"), *Who); return false; }
	if (Phase == EPhase::None && Queue.Num() == 0 && Call(StaffId)) { Line = FString::Printf(TEXT("%s is on the way to your office"), *Who); return true; }
	Queue.Add(StaffId); QueueT = 0.f;
	Line = Phase != EPhase::None
		? FString::Printf(TEXT("%s is called in next - %s is with you first"), *Who, *Name)
		: FString::Printf(TEXT("%s is busy (%s) - they come to your office as soon as they are free"), *Who, *Doing);
	ADP_LOG(TEXT("OfficeCall: %s queued (%d waiting)"), *Who, Queue.Num());
	return true;
}

FString UOfficeCall::StateOf(const FString& StaffId) const
{
	if (Phase != EPhase::None && Id == StaffId) return TEXT("coming");
	return Queue.Contains(StaffId) ? TEXT("queued") : FString();
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
	// three knocks on the wood: a real recording (CC0, Art/Sounds/Office/SOURCES.md), at the door, heard in the office
	static USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/Office/Sounds/Knock.Knock"));
	if (!S) { static bool bTold = false; if (!bTold) { bTold = true; ADP_WARN(TEXT("OfficeCall: /Game/Office/Sounds/Knock is missing (Tools/import_office_sounds.py) - the knock is silent")); } return; }
	static USoundAttenuation* Att = nullptr;
	if (!Att)
	{
		Att = NewObject<USoundAttenuation>(GetTransientPackage()); Att->AddToRoot();
		Att->Attenuation.bAttenuate = true; Att->Attenuation.bSpatialize = true;
		Att->Attenuation.AttenuationShapeExtents = FVector(300.f); Att->Attenuation.FalloffDistance = 1500.f;
	}
	UGameplayStatics::SpawnSoundAtLocation(GetWorld(), S, At, FRotator::ZeroRotator, 1.f, 1.f, 0.f, Att);
}

void UOfficeCall::Hand(float Dt, bool bOn, int32 Side, const FVector& At, const FVector& Palm, const FVector& Fingers, float Curl, bool bKnuckles)
{
	if (bOn) { HandSide = Side; HandAt = At; HandPalm = Palm; HandFing = Fingers; HandCurl = Curl; bHandKnuck = bKnuckles; }
	HandW = FMath::FInterpConstantTo(HandW, bOn ? 1.f : 0.f, Dt, 3.f);
	AADPPeople* People = AADPPeople::Get(GetWorld());
	AADPPeople::FWho* W = People && V.IsValid() ? People->Find(V.Get()) : nullptr;
	UPaxAnimInstance* An = W && W->Body ? Cast<UPaxAnimInstance>(W->Body->GetAnimInstance()) : nullptr;
	if (!An) { static bool bTold = false; if (!bTold && bOn) { bTold = true; ADP_WARN(TEXT("OfficeCall: %s's body has no PaxAnimInstance - no hand IK on the door"), *Name); } return; }
	An->SetReach(HandSide, HandAt, HandPalm, HandFing, HandCurl, bHandKnuck, HandW);
}

bool UOfficeCall::DoorByHand(float Dt, bool bIn, float KTo)
{
	AADPPeople* People = AADPPeople::Get(GetWorld());
	AActor* A = V.Get();
	if (!People || !A) return true;
	const FVector Door = Room(InDoorway.X, InDoorway.Y);
	const FVector Rest = FTerminalPlan::ToWorld(124.98f, 7.01f, bIn ? 8.93f : 9.07f);
	FVector H; float K = 0.f;
	if (!ABabylonModel::StaffDoorPoint(Door, Rest, H, K))
	{
		ADP_WARN(TEXT("OfficeCall: no hinged door at the office (D-F2-OFFICE-1-1) - %s goes on without it"), *Name);
		return true;
	}
	// the leaf's way from the hinge to the handle, and the face the handle is on (world: the room face is the leaf turned
	// a quarter to the left of it); they stand half a metre off that face, three quarters of the way along
	const FVector Hg = Room(Hinge.X, Hinge.Y);
	FVector D = H - Hg; D.Z = 0.f;
	if (!D.Normalize()) D = FVector(1.f, 0.f, 0.f);
	const FVector N = FVector(-D.Y, D.X, 0.f) * (bIn ? 1.f : -1.f);
	const FVector S = Hg + D * 75.f + N * 50.f;
	People->Walk(A, S, Dt, 8.f);
	const FVector Me = A->GetActorLocation();
	const FVector F = (H - Me).GetSafeNormal2D();
	if (!F.IsNearlyZero()) A->SetActorRotation(FRotator(0.f, F.Rotation().Yaw - 90.f, 0.f));
	const FVector Right = FVector::CrossProduct(FVector::UpVector, F);
	const int32 Side = FVector::DotProduct(H - Me, Right) >= 0.f ? 1 : 0;
	const FVector Shoulder = Me + FVector(0.f, 0.f, 140.f) + Right * (Side ? 18.f : -18.f);
	const bool bReach = FVector::Dist(Shoulder, H) < 68.f;
	// (the palm down on the lever, the fingers round it into the leaf)
	Hand(Dt, bReach, Side, H, -FVector::UpVector, -N, 0.9f, false);
	float Kn = K;
	if (bReach && HandW > 0.95f && FVector::Dist2D(Me, S) < 30.f) Kn = FMath::FInterpConstantTo(K, KTo, Dt, 0.6f);
	if (HandW > 0.f) ABabylonModel::StaffDoorHold(Door, Kn);   // (held: the leaf goes where the hand takes it, else it stays)
	if (FMath::Abs(Kn - KTo) < 0.01f) return true;
	if (T > 15.f)
	{
		ADP_WARN(TEXT("OfficeCall: %s couldn't get the door to %.1f by hand (at %.2f, reach %d) - lets it go"), *Name, KTo, Kn, bReach ? 1 : 0);
		if (KTo > 0.5f) ABabylonModel::StaffDoor(Door, true); else ABabylonModel::StaffDoorShut(Door);
		return true;
	}
	return false;
}

void UOfficeCall::Finish(const TCHAR* Why)
{
	if (AActor* A = V.Get()) { ASeats::Release(A); A->Destroy(); }
	if (UStaffPeople* SP = UStaffPeople::Get(GetWorld())) SP->CallOver(Id);
	ADP_LOG(TEXT("OfficeCall: %s - %s"), *Name, Why);
	V = nullptr; Legs.Reset(); Phase = EPhase::None; HandW = 0.f;
}

void UOfficeCall::Tick(float Dt)
{
	if (Phase == EPhase::None)
	{
		// (the queue: the first who is free now comes; anyone no longer at the airport drops out of it)
		if (!Queue.Num() || (QueueT += Dt) < 10.f) return;
		QueueT = 0.f;
		UStaffPeople* SP = UStaffPeople::Get(GetWorld());
		for (int32 i = 0; i < Queue.Num(); ++i)
		{
			FString Doing, Where;
			if (!SP || !SP->Status(Queue[i], Doing, Where)) { ADP_LOG(TEXT("OfficeCall: %s left the queue (not at the airport now)"), *Queue[i]); Queue.RemoveAt(i--); continue; }
			if (Call(Queue[i])) { Queue.RemoveAt(i); break; }
		}
		return;
	}
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
	case EPhase::In:
		Hand(Dt, false, HandSide, HandAt, HandPalm, HandFing, HandCurl, false);
		if (WalkLegs(Dt)) { FaceTo(Door); KnockAt = -1.f; Go(EPhase::Knock); }
		break;
	case EPhase::Knock:
	{
		// the right fist up to the leaf (0.5 s), the recording started, each knock's knuckles on the wood at its sound,
		// 4 cm back between them; the hand down again
		const FVector Fw = (Door - A->GetActorLocation()).GetSafeNormal2D(), Left = FVector::CrossProduct(Fw, FVector::UpVector);
		const FVector On = FTerminalPlan::ToWorld(KnockPlan.X, KnockPlan.Y, KnockPlan.Z);
		if (T >= 0.5f && KnockAt < 0.f) { KnockAt = T; Knock(Room(10.55f, 8.9f) + FVector(0.f, 0.f, 140.f)); ADP_LOG(TEXT("OfficeCall: %s knocks"), *Name); }
		float Off = 5.f;
		if (KnockAt >= 0.f)
		{
			float Near = 1.f;
			for (const float Hh : KnockHits) Near = FMath::Min(Near, FMath::Abs(T - KnockAt - Hh));
			Off = 4.f * FMath::Pow(FMath::Clamp(Near / 0.09f, 0.f, 1.f), 0.7f);
		}
		Hand(Dt, T < 1.35f, 1, On - Fw * Off, Left, Fw, 1.f, true);
		if (T > 2.2f) Go(EPhase::Opening);
		break;
	}
	case EPhase::Opening:
		// the corridor handle: pushed, the leaf swinging in, they follow it into the doorway
		if (DoorByHand(Dt, false, 1.f)) { Legs.Reset(); for (const FVector2D& P : RoundLeaf) Legs.Add(Room(P.X, P.Y)); Go(EPhase::Through); }
		break;
	case EPhase::Through:
		Hand(Dt, false, HandSide, HandAt, HandPalm, HandFing, HandCurl, false);
		ABabylonModel::StaffDoor(Door, true);
		if (WalkLegs(Dt)) Go(EPhase::Shut);
		break;
	case EPhase::Shut:
		// round the leaf, the inside handle: pushed shut, following it to the doorway
		if (DoorByHand(Dt, true, 0.f))
		{
			ABabylonModel::StaffDoorShut(Door);
			Legs.Reset(); Legs.Add(Room(10.4f, 7.6f)); for (const FVector2D& P : ToChair) Legs.Add(Room(P.X, P.Y));
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
		Legs.Reset(); Legs.Add(Room(ToChair[2].X, ToChair[2].Y)); Legs.Add(Room(ToChair[1].X, ToChair[1].Y)); Legs.Add(Room(ToChair[0].X, ToChair[0].Y)); Legs.Add(Room(10.4f, 7.6f));
		Go(EPhase::Out);
		break;
	case EPhase::Out: if (WalkLegs(Dt)) Go(EPhase::OpenOut); break;
	case EPhase::OpenOut:
		// the inside handle: pulled open, stepping back round behind the leaf
		if (DoorByHand(Dt, true, 1.f))
		{
			Legs.Reset(); for (int32 i = UE_ARRAY_COUNT(RoundLeaf) - 1; i >= 0; --i) Legs.Add(Room(RoundLeaf[i].X, RoundLeaf[i].Y));
			Legs.Add(Room(10.4f, 8.25f));
			Go(EPhase::ThroughOut);
		}
		break;
	case EPhase::ThroughOut:
		Hand(Dt, false, HandSide, HandAt, HandPalm, HandFing, HandCurl, false);
		ABabylonModel::StaffDoor(Door, true);
		if (WalkLegs(Dt)) Go(EPhase::ShutOut);
		break;
	case EPhase::ShutOut:
		// the corridor handle: pulled shut behind them as they go out
		if (DoorByHand(Dt, false, 0.f)) { ABabylonModel::StaffDoorShut(Door); Legs = {Room(CorridorFar.X, CorridorFar.Y)}; Go(EPhase::Away); }
		break;
	case EPhase::Away:
		Hand(Dt, false, HandSide, HandAt, HandPalm, HandFing, HandCurl, false);
		if (Legs.Num()) { WalkLegs(Dt); T = 0.f; break; }
		if (T > 1.f) { if (!InYourView(A->GetActorLocation())) Finish(TEXT("back at their job")); else T = 0.f; }
		break;
	default: break;
	}
}
