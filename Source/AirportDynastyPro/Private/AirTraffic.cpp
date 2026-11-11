// The airport's traffic: airliners turning round at the stands, taxiing, taking off and landing on runway 08, with
// their ground vehicles. Simple shapes for the aircraft and vehicles until proper models replace them.
#include "AirTraffic.h"
#include "ADPLog.h"
#include "TerminalBuilder.h"
#include "TerminalPlan.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UI/ADPHUD.h"

#pragma region Sizes
namespace
{
	constexpr float NoseToCentre = 18.5f;   // an A320-sized airliner: 37.6 m long
	constexpr float BodyY = 3.9f;           // the fuselage's middle above the ground
	constexpr float TaxiSpeed = 10.f, PushSpeed = 1.5f, RotateSpeed = 75.f, ApproachSpeed = 70.f;
	constexpr float Glide = 3.f;            // degrees
	const FName Liveries[] = {"navy", "red", "tealUI", "fabricRust"};
}
#pragma endregion

#pragma region Models

AAirTraffic::AAirTraffic()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UStaticMeshComponent* AAirTraffic::Part(USceneComponent* Parent, UStaticMesh* Mesh, const FName Mat, const FVector& At, const FRotator& Rot, const FVector& Size)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Parent);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, ATerminalBuilder::KitMaterial(this, Mat));
	C->SetRelativeLocationAndRotation(At * 100.f, Rot);
	C->SetRelativeScale3D(Size);
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

USceneComponent* AAirTraffic::MakeAirliner(int32 Livery)
{
	// in its own frame: x forward (the nose), y right, z up; metres
	USceneComponent* R = NewObject<USceneComponent>(this);
	R->SetupAttachment(Root);
	R->SetMobility(EComponentMobility::Movable);
	R->RegisterComponent();
	AddInstanceComponent(R);
	const FName Tail = Liveries[Livery % 4];
	Part(R, Cyl, "white", FVector(0, 0, BodyY), FRotator(90, 0, 0), FVector(3.95f, 3.95f, 33));           // the fuselage
	Part(R, Ball, "white", FVector(16.4f, 0, BodyY - 0.2f), FRotator::ZeroRotator, FVector(4.4f, 3.9f, 3.6f)); // the nose
	Part(R, Ball, "white", FVector(-17.2f, 0, BodyY + 0.5f), FRotator::ZeroRotator, FVector(7.f, 3.f, 2.8f)); // the tail cone
	Part(R, Cube, "black", FVector(15.6f, 0, BodyY + 0.9f), FRotator::ZeroRotator, FVector(1.f, 2.4f, 0.5f)); // the cockpit windows
	for (const float S : {-1.f, 1.f})
	{
		// the swept wings and tailplane, the engines under the wings
		const float Sweep = FMath::DegreesToRadians(25.f);
		const FVector Wd(-FMath::Sin(Sweep), S * FMath::Cos(Sweep), 0);
		Part(R, Cube, "liftClad", FVector(-1.5f, 0, BodyY - 1.1f) + Wd * 9.f, FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(Wd.Y, Wd.X)), 0), FVector(17.5f, 4.2f, 0.45f));
		const FVector Td(-FMath::Sin(Sweep * 1.2f), S * FMath::Cos(Sweep * 1.2f), 0);
		Part(R, Cube, "liftClad", FVector(-15.8f, 0, BodyY + 0.9f) + Td * 3.4f, FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(Td.Y, Td.X)), 0), FVector(6.5f, 2.4f, 0.3f));
		Part(R, Cyl, "steel", FVector(1.2f, S * 5.75f, BodyY - 2.1f), FRotator(90, 0, 0), FVector(2.f, 2.f, 4.f));
		Part(R, Cyl, "black", FVector(-2.f, S * 2.f, 0.55f), FRotator(0, 0, 90), FVector(1.1f, 1.1f, 0.9f)); // main wheels
	}
	Part(R, Cube, Tail, FVector(-15.f, 0, BodyY + 4.6f), FRotator(-12, 0, 0), FVector(5.5f, 0.4f, 6.f));     // the fin, in the airline's colour
	Part(R, Cyl, "black", FVector(12.6f, 0, 0.4f), FRotator(0, 0, 90), FVector(0.75f, 0.75f, 0.6f));          // nose wheel
	return R;
}

AAirTraffic::FVehicle AAirTraffic::MakeVehicle(int32 Kind, const FVector2f& Home, const FVector2f& Spot)
{
	FVehicle V;
	V.Home = Home;
	V.Spot = Spot;
	V.Root = NewObject<USceneComponent>(this);
	V.Root->SetupAttachment(Root);
	V.Root->SetMobility(EComponentMobility::Movable);
	V.Root->RegisterComponent();
	AddInstanceComponent(V.Root);
	if (Kind == 0)
	{
		// the fuel truck: cab and tank
		Part(V.Root, Cube, "white", FVector(3.2f, 0, 1.4f), FRotator::ZeroRotator, FVector(2.2f, 2.5f, 2.6f));
		Part(V.Root, Cyl, "steel", FVector(-1.f, 0, 1.9f), FRotator(90, 0, 0), FVector(2.3f, 2.3f, 6.f));
	}
	else if (Kind == 1)
	{
		// the catering truck: cab and the box on its scissor lift
		Part(V.Root, Cube, "white", FVector(3.f, 0, 1.4f), FRotator::ZeroRotator, FVector(2.f, 2.4f, 2.4f));
		Part(V.Root, Cube, "liftClad", FVector(-1.f, 0, 3.f), FRotator::ZeroRotator, FVector(5.f, 2.4f, 2.6f));
	}
	else
	{
		// the baggage tug and two carts
		Part(V.Root, Cube, "yellow", FVector(2.f, 0, 0.8f), FRotator::ZeroRotator, FVector(2.6f, 1.6f, 1.3f));
		for (int32 c = 0; c < 2; ++c) Part(V.Root, Cube, "steel", FVector(-1.6f - c * 3.2f, 0, 0.9f), FRotator::ZeroRotator, FVector(2.8f, 1.6f, 1.4f));
	}
	Place(V.Root, FVector(Home.X, 0, Home.Y), -HALF_PI);
	return V;
}

void AAirTraffic::Place(USceneComponent* C, const FVector& P, float Heading)
{
	// the plan's (x, y) heading h points along Unreal's (cos h, -sin h): yaw -h
	C->SetRelativeLocationAndRotation(FTerminalPlan::ToWorld(P.X, P.Y, P.Z), FRotator(0, -FMath::RadiansToDegrees(Heading), 0));
}

#pragma endregion

#pragma region Routes

FVector AAirTraffic::StandCentre(const FPlane& P) const
{
	// parked nose-in towards the pier: the nose at the stand's nose line, the body behind it
	return FVector(P.Stand.X, 0, P.Stand.Nose - P.Stand.Dir * NoseToCentre);
}

void AAirTraffic::Plan(FPlane& P, EState S)
{
	P.State = S;
	P.Path.Reset();
	P.Step = 0;
	P.bBackwards = false;
	const float XW = F.RwyX0 + 20, RY = F.RwyY;
	const FAirfieldData::FTwy* Curve = F.Find(TEXT("CURVE-A-A1"));
	switch (S)
	{
	case EState::Parked:
		P.Pos = StandCentre(P);
		P.Heading = P.Stand.Dir > 0 ? HALF_PI : -HALF_PI;
		P.Timer = P.Turn = Rng.FRandRange(150, 280);
		P.Speed = 0;
		break;
	case EState::Pushback:
		// pushed straight back onto the taxilane, nose still to the pier
		P.Path.Add(FVector(P.Stand.X, 0, F.TY));
		P.bBackwards = true;
		break;
	case EState::TaxiOut:
		// west along T to P1, up P1 to A, west along A, round the curve onto A1, up to the holding position
		P.Path.Add(FVector(-130, 0, F.TY));
		P.Path.Add(FVector(-130, 0, F.AY));
		if (Curve) for (int32 i = Curve->Pts.Num() - 1; i >= 0; --i) P.Path.Add(FVector(Curve->Pts[i].X, 0, Curve->Pts[i].Y));
		P.Path.Add(FVector(XW, 0, RY - 107.5f));
		break;
	case EState::TakeOff:
		// lined up on 08, the roll, then climbing away east
		P.Path.Add(FVector(XW, 0, RY));
		P.Path.Add(FVector(XW + 30, 0, RY));
		P.Path.Add(FVector(F.RwyX1 + 9000, 900, RY + 3600));
		P.Speed = 5;
		break;
	case EState::Away:
		P.Root->SetVisibility(false, true);
		P.Timer = Rng.FRandRange(90, 240);
		break;
	case EState::Approach:
	{
		// a straight-in 3° approach onto 08: touching down 550 m past the runway's end
		const float TD = F.RwyX0 + 550, From = 7000;
		P.Pos = FVector(TD - From, From * FMath::Tan(FMath::DegreesToRadians(Glide)), RY);
		P.Heading = 0;
		P.Speed = ApproachSpeed;
		P.Path.Add(FVector(TD, 0, RY));
		P.Root->SetVisibility(true, true);
		break;
	}
	case EState::Rollout:
		// slowing down to leave at A2
		P.Path.Add(FVector(81, 0, RY));
		break;
	case EState::TaxiIn:
		// off at A2, east along A to P3, down to T, west along T and nose in to the stand
		P.Path.Add(FVector(81, 0, F.AY));
		P.Path.Add(FVector(362, 0, F.AY));
		P.Path.Add(FVector(362, 0, F.TY));
		P.Path.Add(FVector(P.Stand.X, 0, F.TY));
		P.Path.Add(StandCentre(P));
		break;
	default: break;
	}
}

bool AAirTraffic::Follow(FPlane& P, float Dt, float MaxSpeed)
{
	// along the path's points, the nose turning towards the way it goes (or away from it, pushed back)
	P.Speed = FMath::FInterpTo(P.Speed, MaxSpeed, Dt, 0.8f);
	float Left = P.Speed * Dt;
	while (Left > 0 && P.Step < P.Path.Num())
	{
		const FVector To = P.Path[P.Step] - P.Pos;
		const float D = To.Size();
		if (D > 0.01f)
		{
			const float H = FMath::Atan2(To.Z, To.X) + (P.bBackwards ? PI : 0);
			P.Heading = H;
		}
		if (D <= Left) { P.Pos = P.Path[P.Step++]; Left -= D; continue; }
		P.Pos += To / D * Left;
		Left = 0;
	}
	Place(P.Root, P.Pos, P.Heading);
	return P.Step >= P.Path.Num();
}

#pragma endregion

#pragma region Ground vehicles

void AAirTraffic::TurnaroundVehicles(FPlane& P, float Dt)
{
	// they drive up in the first 25 s of the turnaround and away in its last 25 s
	const float Done = P.Turn - P.Timer;
	const bool bThere = P.State == EState::Parked && Done > 2 && P.Timer > 25;
	for (FVehicle& V : P.Vehicles)
	{
		const FVector Now = V.Root->GetRelativeLocation() / 100.f; // Unreal metres: the plan's x, and -y
		const FVector2f Cur(Now.X, -Now.Y);
		const FVector2f Goal = bThere ? V.Spot : V.Home;
		const FVector2f To = Goal - Cur;
		const float D = To.Size();
		if (D < 0.05f) continue;
		const FVector2f N = Cur + To / D * FMath::Min(D, 8.f * Dt);
		Place(V.Root, FVector(N.X, 0, N.Y), FMath::Atan2(To.Y, To.X));
	}
}

#pragma endregion

#pragma region Running

void AAirTraffic::Start(const FAirfieldData& Field)
{
	F = Field;
	Rng.Initialize(68);
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Cube || !Cyl || !Ball) return;
	int32 n = 0;
	for (const FAirfieldData::FStand& S : F.Stands)
	{
		if (S.bCargo || S.Id == TEXT("STAND-9")) continue;
		FPlane P;
		P.Stand = S;
		P.Root = MakeAirliner(n++);
		// the ground vehicles wait between the stands by the pier, and drive up to the aircraft's sides
		const float Base = S.Nose - S.Dir * 2;
		P.Vehicles.Add(MakeVehicle(0, FVector2f(S.X - 22, F.TY - 12), FVector2f(S.X - 8.5f, S.Nose - S.Dir * 20)));
		P.Vehicles.Add(MakeVehicle(1, FVector2f(S.X + 22, F.TY - 12), FVector2f(S.X + 4.5f, S.Nose - S.Dir * 31)));
		P.Vehicles.Add(MakeVehicle(2, FVector2f(S.X + 20, Base + 2), FVector2f(S.X + 5, S.Nose - S.Dir * 26)));
		Plan(P, EState::Parked);
		P.Timer = Rng.FRandRange(10, P.Turn); // spread out, so they don't all leave together
		Place(P.Root, P.Pos, P.Heading);
		Planes.Add(MoveTemp(P));
	}
	ADP_LOG(TEXT("Air traffic started: %d aircraft"), Planes.Num());
}

void AAirTraffic::Tick(float Dt)
{
	Super::Tick(Dt);
	for (int32 i = 0; i < Planes.Num(); ++i)
	{
		FPlane& P = Planes[i];
		TurnaroundVehicles(P, Dt);
		switch (P.State)
		{
		case EState::Parked:
		{
			// the calls for this stand's flight: boarding a minute before it goes, then the final call
			static const TCHAR* To[] = {TEXT("Copenhagen"), TEXT("London"), TEXT("Oslo"), TEXT("Reykjavík"), TEXT("Amsterdam"), TEXT("Barcelona"), TEXT("Frankfurt"), TEXT("Vágar")};
			const int32 Gate = FCString::Atoi(*P.Stand.Id.RightChop(6));
			const float Before = P.Timer;
			P.Timer -= Dt;
			const int32 Flight = 200 + Gate * 37;
			if (Before > 60 && P.Timer <= 60) AADPHUD::Announce(FString::Printf(TEXT("Flight VT %d to %s is now boarding at gate %d."), Flight, To[(Gate - 1) % 8], Gate));
			if (Before > 20 && P.Timer <= 20) AADPHUD::Announce(FString::Printf(TEXT("Final call for flight VT %d to %s, gate %d is closing."), Flight, To[(Gate - 1) % 8], Gate));
			if (P.Timer <= 0) Plan(P, EState::Pushback);
			break;
		}
		case EState::Pushback:
			if (Follow(P, Dt, PushSpeed)) { P.Speed = 0; Plan(P, EState::TaxiOut); }
			break;
		case EState::TaxiOut:
			if (Follow(P, Dt, TaxiSpeed)) { P.Speed = 0; P.State = EState::Holding; }
			break;
		case EState::Holding:
			// waits at the holding position until the runway is free
			if (RunwayUser == INDEX_NONE) { RunwayUser = i; Plan(P, EState::TakeOff); }
			break;
		case EState::TakeOff:
		{
			// onto the runway and lined up, then the roll: on the ground to about 75 m/s, then climbing away
			if (P.Step == 0) { Follow(P, Dt, 6.f); if (P.Step >= 1) { P.Heading = 0; P.Speed = 5; } break; }
			P.Speed = FMath::Min(95.f, P.Speed + 2.3f * Dt);
			const FVector Far = P.Path.Last();
			const FVector Dir = P.Speed < RotateSpeed ? FVector(1, 0, 0) : (Far - P.Pos).GetSafeNormal();
			P.Pos += Dir * P.Speed * Dt;
			P.Heading = FMath::Atan2(Dir.Z, Dir.X);
			Place(P.Root, P.Pos, P.Heading);
			if (P.Pos.X > F.RwyX0 + 2600 && RunwayUser == i) RunwayUser = INDEX_NONE; // clear of the runway
			if (FVector::Dist(P.Pos, Far) < 200) Plan(P, EState::Away);
			break;
		}
		case EState::Away:
			if ((P.Timer -= Dt) <= 0 && RunwayUser == INDEX_NONE) { RunwayUser = i; Plan(P, EState::Approach); }
			break;
		case EState::Approach:
			if (Follow(P, Dt, ApproachSpeed)) Plan(P, EState::Rollout);
			break;
		case EState::Rollout:
		{
			// braking hard, then taxi speed to the exit
			const float Left = FVector::Dist(P.Pos, P.Path.Last());
			const float Want = FMath::Max(12.f, FMath::Sqrt(2.f * 2.4f * FMath::Max(0.f, Left - 150)) + 12.f);
			P.Speed = FMath::Min(P.Speed, Want);
			if (Follow(P, Dt, FMath::Min(P.Speed, Want))) { if (RunwayUser == i) RunwayUser = INDEX_NONE; Plan(P, EState::TaxiIn); }
			break;
		}
		case EState::TaxiIn:
			if (Follow(P, Dt, P.Step >= P.Path.Num() - 1 ? 3.f : TaxiSpeed)) Plan(P, EState::Parked);
			break;
		}
	}
}

#pragma endregion
