// The developer's test of the coaches' driver and riders (CoachIKTest.h).
#include "CoachIKTest.h"
#include "VehicleRiders.h"
#include "ADPLog.h"
#include "ADPWalker.h"
#include "BabylonModel.h"
#include "Coaches.h"
#include "People.h"
#include "RoadNetwork.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Parse.h"

namespace
{
	struct FCoachTest
	{
		int32 Stage = 0, Coach = 4, Sub = 0; float T = 0.f, Was = 0.f, ShotIn = -1.f, LogIn = 0.f, GiveT = 0.f; FString ShotName;
		TWeakObjectPtr<AActor> Driver, Pax[2], Walker; TWeakObjectPtr<ACameraActor> Cam; FVector WalkTo; FTransform Base;
		TFunction<FTransform()> View; TSet<FString> Done; bool bFailed = false, bGave = false; TArray<FVector> Path; float MaxV = 0.f, MaxTurn = 0.f;
	} CT;
}

bool ADPCoachIKTestStep(APlayerController* PC, float Clock)
{
	if (!PC || Clock < 62.f) { CT.Was = Clock; return false; }
	const float Dt = FMath::Clamp(Clock - CT.Was, 0.f, 0.2f); CT.Was = Clock;
	UWorld* W = PC->GetWorld(); ACoaches* Co = ACoaches::Find(); AADPPeople* P = AADPPeople::Get(W);
	FParse::Value(FCommandLine::Get(), TEXT("CoachNo="), CT.Coach);
	if (!Co || !P || CT.Coach >= Co->Num()) { ADP_LOG(TEXT("CoachIKTest: no coaches")); return true; }
	const int32 Ci = CT.Coach;
	UVehicleRiders* V = CT.Stage < 98 ? Co->Riders(Ci) : nullptr;
	if (CT.Stage < 98 && !V) { ADP_LOG(TEXT("CoachIKTest: no riders component")); return true; }
	auto InCoach = [&](float X, float Y, float Z) { return Co->RootOf(Ci).TransformPosition(FVector(X * 100.f, -Y * 100.f, Z * 100.f)); };
	auto Next = [&](int32 S) { ADP_LOG(TEXT("CoachIKTest: stage %d -> %d after %.1f s"), CT.Stage, S, CT.T); CT.Stage = S; CT.T = 0.f; CT.Sub = 0; };
	// a small picture from a place by the coach, once by name, about a second later (the view follows the coach)
	auto Shot = [&](const TCHAR* Name, const FVector& Eye, const FVector& At, float Fov = 60.f)
	{
		if (CT.Done.Contains(Name) || CT.ShotIn >= 0.f) return;
		CT.Done.Add(Name); CT.ShotName = Name; CT.ShotIn = 1.1f;
		CT.View = [Co, Ci, Eye, At] { const FTransform R = Co->RootOf(Ci); auto Q = [&](const FVector& B) { return R.TransformPosition(FVector(B.X * 100.f, -B.Y * 100.f, B.Z * 100.f)); }; return FTransform((Q(At) - Q(Eye)).Rotation(), Q(Eye)); };
		if (CT.Cam.IsValid())
		{
			// (inside, by day: the camera opened up, else the cabin is black)
			UCameraComponent* C = CT.Cam->GetCameraComponent(); C->SetFieldOfView(Fov); C->PostProcessBlendWeight = 1.f;
			C->PostProcessSettings.bOverride_AutoExposureBias = true; C->PostProcessSettings.AutoExposureBias = Fov > 65.f ? 3.f : 0.f;
		}
	};
	if (CT.Cam.IsValid() && CT.View) CT.Cam->SetActorTransform(CT.View());
	if (CT.ShotIn >= 0.f && (CT.ShotIn -= Dt) < 0.f) { CT.ShotIn = -1.f; PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 1280x720 filename=CoachIK-%s"), *CT.ShotName), true); }
	CT.T += Dt;
	AActor* D = CT.Driver.Get(); AActor* A = CT.Pax[0].Get(); AActor* B = CT.Pax[1].Get();
	if (V) { CT.MaxV = FMath::Max(CT.MaxV, V->Speed()); CT.MaxTurn = FMath::Max(CT.MaxTurn, FMath::Abs(V->WheelTurn())); }
	if ((CT.LogIn -= Dt) < 0.f && V && CT.Stage > 2) { CT.LogIn = CT.Stage == 10 || CT.Stage == 3 || (CT.Stage == 7 && CT.Sub == 1) ? 0.6f : 4.f; ADP_LOG(TEXT("CoachIKTest: [%d %.0f s] driver: %s | a: %s | b: %s | %.1f km/h"), CT.Stage, CT.T, *V->Report(D), *V->Report(A), *V->Report(B), V->Speed() * 0.036f); }
	if (CT.Stage > 0 && CT.Stage < 90 && CT.T > 100.f) { ADP_WARN(TEXT("CoachIKTest: STUCK in stage %d"), CT.Stage); CT.bFailed = true; Next(90); }
	switch (CT.Stage)
	{
	case 0:
	{
		CT.Base = Co->RoadOf(Ci);
		// (-CoachFast: five times the speed, for checking that everything happens; looks and driving are judged at real speed)
		if (FParse::Param(FCommandLine::Get(), TEXT("CoachFast"))) UGameplayStatics::SetGlobalTimeDilation(W, 5.f);
		CT.Cam = W->SpawnActor<ACameraActor>(InCoach(9.f, -7.f, 2.5f), FRotator::ZeroRotator); if (CT.Cam.IsValid()) { CT.Cam->GetCameraComponent()->bConstrainAspectRatio = false; PC->SetViewTarget(CT.Cam.Get()); }
		CT.View = [Co, Ci] { const FTransform R = Co->RootOf(Ci); const FVector E = R.TransformPosition(FVector(900, 700, 260)); return FTransform((R.TransformPosition(FVector(300, 120, 120)) - E).Rotation(), E); };
		if (APawn* Me = PC->GetPawn())
		{
			if (AADPWalker* Wk = Cast<AADPWalker>(Me)) if (AActor* H = Wk->HumanActor()) H->SetActorHiddenInGame(true);
			Me->SetActorEnableCollision(false); if (ACharacter* Ch = Cast<ACharacter>(Me)) Ch->GetCharacterMovement()->DisableMovement();
			Me->SetActorLocation(InCoach(0.f, -14.f, 3.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		for (const TCHAR* N : {TEXT("MH_070_ground"), TEXT("MH_071_ground"), TEXT("MH_072_ground"), TEXT("MH_076_ground"), TEXT("MH_080_ground"), TEXT("MH_090_ground"), TEXT("MH_060_pilot")}) if (!CT.Driver.IsValid()) CT.Driver = P->Spawn(N, InCoach(3.2f, -4.6f, 0.6f), Co->RootOf(Ci).Rotator().Yaw - 90.f);
		CT.Pax[0] = P->Spawn(TEXT("MH_003_passenger"), InCoach(0.6f, -4.8f, 0.6f), Co->RootOf(Ci).Rotator().Yaw - 90.f);
		CT.Pax[1] = P->Spawn(TEXT("MH_012_passenger"), InCoach(-0.6f, -5.2f, 0.6f), Co->RootOf(Ci).Rotator().Yaw - 90.f);
		ADP_LOG(TEXT("CoachIKTest: coach %d at %s, people %d %d %d"), Ci, *CT.Base.GetLocation().ToCompactString(), CT.Driver.IsValid(), CT.Pax[0].IsValid(), CT.Pax[1].IsValid());
		if (!CT.Driver.IsValid() || !CT.Pax[0].IsValid() || !CT.Pax[1].IsValid()) return true;
		Next(1); break;
	}
	case 1:
		if (CT.T < 3.f) break;
		ADP_LOG(TEXT("CoachIKTest: hired %d, waiting %d %d"), V->HireDriver(D), V->WaitAtStop(A, 0, true), V->WaitAtStop(B, 1, true));
		V->UseBay(D, 0, true); Next(2); break;
	case 2:
		if (CT.T > 2.f) Shot(TEXT("01-bay-opened"), FVector(6.2f, -5.2f, 1.9f), FVector(2.6f, -2.2f, 1.0f));
		if (V->IsBusy(D) || V->IsBusy(A) || V->IsBusy(B)) break;
		V->LoadBag(A, A, 0); V->PutBagDown(B, 0); Next(3); break;
	case 3:
		if (CT.T > 3.f) Shot(TEXT("02-own-bag-in"), FVector(4.4f, -4.4f, 1.7f), FVector(2.2f, -1.9f, 0.8f));
		if (V->IsBusy(A) || V->IsBusy(B)) break;
		V->WaitAtStop(A, 0, false); V->WaitAtStop(B, 2, false); V->LoadBag(D, B, 0); Next(4); break;
	case 4:
		if (CT.T > 4.5f) Shot(TEXT("03-driver-loads"), FVector(4.6f, -4.4f, 1.7f), FVector(2.2f, -1.9f, 0.8f));
		if (V->IsBusy(D)) break;
		V->UseBay(D, 0, false); V->DriverGetIn(); Next(5); break;
	case 5:
		if (V->IsAboard(D)) { Shot(TEXT("04-driver-climbs"), FVector(6.6f, -5.0f, 1.8f), FVector(5.3f, -0.9f, 1.4f)); if (CT.Done.Contains(TEXT("04-driver-climbs")) && CT.ShotIn < 0.f) Shot(TEXT("05-driver-steps-in"), FVector(6.12f, -0.15f, 2.55f), FVector(5.15f, 0.35f, 1.7f), 88.f); }
		if (!V->DriverAtWheel()) break;
		if (CT.Sub == 0) { CT.Sub = 1; CT.T = 0.f; }
		if (CT.T > 2.f) Shot(TEXT("06-at-the-wheel"), FVector(6.12f, 0.02f, 2.42f), FVector(5.25f, 0.60f, 1.72f), 88.f);
		if (CT.T > 5.f)
		{
			V->Board(A); V->Board(B);
			// someone on the zebra ahead, in the bus lane: it must wait for them (made now, so that nothing is loaded at drive-off)
			FVehicleLane Lane; const bool bLane = FVehicleLane::FromRoad(TEXT("AIRPORT-FORECOURT"), true, 6, Lane);
			const FTransform R = V->RoadFrame(); const FVector F = R.TransformPosition(V->GetSpec().At(TEXT("front_axle")));
			CT.Path = bLane ? Lane.PullOut(F, R.GetUnitAxis(EAxis::X), 1400.f, 4500.f) : TArray<FVector>();
			FVector Z = FVector::ZeroVector; float Zd = 1e9f;
			for (const FVector& Q : CT.Path) for (const auto& Zb : ARoadNetwork::ZebraCrossings) { const float X = float(FMath::PointDistToSegment(FVector(Q.X, Q.Y, 0), FVector(Zb.Key.X, Zb.Key.Y, 0), FVector(Zb.Value.X, Zb.Value.Y, 0))); if (X < 60.f && X < Zd) { Zd = X; Z = Q; } }
			if (!Z.IsZero()) { CT.Walker = P->Spawn(TEXT("MH_021_passenger"), Z + FVector(0, 0, 60.f), R.Rotator().Yaw + 90.f); CT.WalkTo = Z + R.GetUnitAxis(EAxis::Y) * 1100.f; }
			ADP_LOG(TEXT("CoachIKTest: lane %d, path %d points, someone on the zebra %d"), bLane, CT.Path.Num(), CT.Walker.IsValid());
			Next(6);
		}
		break;
	case 6:
		if (V->IsAboard(A)) Shot(TEXT("07-passenger-climbs"), FVector(6.6f, -5.0f, 1.8f), FVector(5.3f, -0.9f, 1.4f));
		if (!V->IsSeated(A) || !V->IsSeated(B) || V->IsBusy(A) || V->IsBusy(B)) break;
		if (CT.Sub == 0) { CT.Sub = 1; CT.T = 0.f; }
		if (CT.T > 1.f) { const FVector L = Co->RootOf(Ci).InverseTransformPosition(A->GetActorLocation()); Shot(TEXT("08-a-seated"), FVector(L.X / 100.f + 0.62f, L.Y > 0 ? 0.42f : -0.42f, 2.75f), FVector(L.X / 100.f + 0.1f, -L.Y / 100.f, 1.95f), 88.f); }
		if (CT.T > 4.f) { const FVector L = Co->RootOf(Ci).InverseTransformPosition(B->GetActorLocation()); Shot(TEXT("09-b-seated"), FVector(L.X / 100.f + 0.62f, L.Y > 0 ? 0.42f : -0.42f, 2.75f), FVector(L.X / 100.f + 0.1f, -L.Y / 100.f, 1.95f), 88.f); }
		if (CT.T > 7.f)
		{
			if (!V->Drive(CT.Path, 6.5f)) { ADP_WARN(TEXT("CoachIKTest: it would not drive")); CT.bFailed = true; Next(90); break; }
			CT.GiveT = 0.f; CT.bGave = false; CT.Done.Remove(TEXT("x")); Next(7);
		}
		break;
	case 7:
	{
		if (CT.T > 0.5f) Shot(TEXT("10-indicating"), FVector(11.5f, 4.5f, 2.2f), FVector(5.f, 0.f, 1.5f));
		// (its stop counts once it stands - from then on the time runs by itself, at any speed of the test)
		if (!CT.bGave && V->IsGivingWay() && V->Speed() < 5.f) { CT.bGave = true; ADP_LOG(TEXT("CoachIKTest: it stands for someone after %.1f s"), CT.T); }
		if (CT.bGave && CT.Sub == 0)
		{
			CT.GiveT += Dt;
			if (CT.GiveT > 1.f) Shot(TEXT("11-giving-way"), FVector(1.f, 9.f, 3.f), FVector(7.f, 0.5f, 1.2f), 64.f);
		}
		if (CT.GiveT > 5.f && CT.Walker.IsValid()) { const FVector At = CT.Walker->GetActorLocation(); FVector To = CT.WalkTo; To.Z = At.Z; if (FVector::Dist2D(At, To) > 20.f) CT.Walker->SetActorLocation(At + (To - At).GetSafeNormal() * 130.f * Dt); }
		if (CT.GiveT > 5.f && FMath::Abs(V->WheelTurn()) > 110.f && V->Speed() > 20.f) { Shot(TEXT("12-wheel-turned"), FVector(6.12f, 0.02f, 2.42f), FVector(5.25f, 0.60f, 1.72f), 88.f); if (CT.Done.Contains(TEXT("12-wheel-turned")) && CT.ShotIn < 0.f) Shot(TEXT("13-front-wheels"), FVector(6.5f, 5.0f, 0.9f), FVector(3.4f, 1.0f, 0.7f)); }
		if (V->Speed() > 450.f) Shot(TEXT("14-driving"), FVector(13.f, 7.f, 2.6f), FVector(1.f, 0.f, 1.6f));
		// (one gets up before the stop: a hand on a pole while it moves)
		if (CT.Sub == 0 && CT.Done.Contains(TEXT("14-driving")) && CT.ShotIn < 0.f) { CT.Sub = 1; ADP_LOG(TEXT("CoachIKTest: b gets up on the way: %d"), V->Alight(B)); CT.GiveT = 100.f; }
		if (CT.Sub == 1 && V->IsDriving() && (CT.GiveT += Dt) > 104.5f) { const FVector L = Co->RootOf(Ci).InverseTransformPosition(B->GetActorLocation()); Shot(TEXT("15-holding-on"), FVector(L.X / 100.f + 1.9f, 0.05f, 2.75f), FVector(L.X / 100.f, -L.Y / 100.f, 2.25f), 75.f); }
		if (!V->IsDriving()) { if (CT.Walker.IsValid() && !CT.bGave) { ADP_WARN(TEXT("CoachIKTest: FAIL - it never stood for the one on the zebra")); CT.bFailed = true; } Next(8); }
		break;
	}
	case 8:
		if (CT.Sub == 0) { CT.Sub = 1; V->DriverDoor(true); V->Alight(A); if (!V->IsBusy(B)) V->Alight(B); }
		if (!V->IsAboard(A) && !V->IsAboard(B) && !V->IsBusy(A) && !V->IsBusy(B)) { V->DriverGetOut(); Next(9); }
		else if (CT.T > 9.f) Shot(TEXT("16-getting-off"), FVector(7.4f, -5.0f, 1.8f), FVector(5.2f, -1.0f, 1.3f));
		break;
	case 9:
		if (V->IsBusy(D)) break;
		V->UseBay(D, 0, true); V->TakeBag(A); Next(10); break;
	case 10:
		if (CT.T > 6.f) Shot(TEXT("17-taking-the-bag"), FVector(4.6f, -4.6f, 1.7f), FVector(2.2f, -1.9f, 0.8f));
		if (V->IsBusy(A) || V->IsBusy(D)) break;
		if (CT.Sub == 0) { CT.Sub = 1; V->TakeBag(B); break; }
		if (V->IsBusy(B)) break;
		ADP_LOG(TEXT("CoachIKTest: all done - fastest %.1f km/h, the steering wheel turned up to %.0f degrees"), CT.MaxV * 0.036f, CT.MaxTurn);
		Next(90); break;
	case 90:
		if (CT.T < 2.5f) break;
		V->PutBack();
		for (TWeakObjectPtr<AActor> X : {CT.Driver, CT.Pax[0], CT.Pax[1], CT.Walker}) if (X.IsValid()) X->Destroy();
		Next(91); break;
	case 91:
		if (CT.T < 5.f) break;
		Co->DriveOff(Ci); V->DestroyComponent();
		Next(98); break;
	default:
		if (CT.T > 1.f) Shot(TEXT("18-put-back"), FVector(11.f, -7.5f, 2.6f), FVector(1.f, 0.f, 1.4f));
		if (CT.T < 5.f) break;
		ADP_LOG(TEXT("CoachIKTest: %s - the coach %.2f cm and %.3f degrees from where it stood"), CT.bFailed ? TEXT("STUCK, put back") : TEXT("OK, put back"), FVector::Dist(Co->RootOf(Ci).GetLocation(), CT.Base.GetLocation()), FMath::RadiansToDegrees(Co->RootOf(Ci).GetRotation().AngularDistance(CT.Base.GetRotation())));
		return true;
	}
	return false;
}
