#include "TerminalLift.h"
#include "ADPLog.h"
#include "PaxNav.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "ADPKeep.h"

namespace
{
	// the plan's metres to the world (FTerminalPlan::ToWorld): x east, y up, z north
	FVector W(float X, float Y, float Z) { return FVector(X * 100.f, -Z * 100.f, Y * 100.f); }
}

ATerminalLift::ATerminalLift()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

UStaticMeshComponent* ATerminalLift::Box(USceneComponent* Parent, const FVector& Lo, const FVector& Hi, const TCHAR* Mat)
{
	static UStaticMesh* Cube = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Cube); C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Parent);
	const FVector A = W(Lo.X, Lo.Y, Lo.Z), B = W(Hi.X, Hi.Y, Hi.Z);
	const FVector Mn(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z)), Mx(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z));
	C->SetWorldLocation((Mn + Mx) / 2.f); C->SetWorldScale3D((Mx - Mn) / 100.f);   // (the engine cube is 1 m)
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Terminal/Materials/%s.%s"), Mat, Mat)))
		C->SetMaterial(0, M);
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->RegisterComponent();
	return C;
}

void ATerminalLift::Build(float X0, float X1, float Z0, float Z1, float D0, float D1)
{
	Door = FVector2D(D0, D1); Across = FVector2D(X0, X1);
	Inside = FBox2D(FVector2D(X0, Z0), FVector2D(X1, Z1));
	Car = NewObject<USceneComponent>(this); Car->SetupAttachment(GetRootComponent()); Car->SetMobility(EComponentMobility::Movable); Car->RegisterComponent();
	Car->SetWorldLocation(W(X0, 0.f, Z0));
	// the car: brushed steel walls, a floor, a ceiling with its light, a handrail; its door on the west side
	Box(Car, FVector(X0, -0.08f, Z0), FVector(X1, 0.01f, Z1), TEXT("MI_T_tileDark"));
	Box(Car, FVector(X0, 2.4f, Z0), FVector(X1, 2.5f, Z1), TEXT("MI_T_liftBrushed"));
	Box(Car, FVector(X1 - 0.05f, 0.01f, Z0), FVector(X1, 2.4f, Z1), TEXT("MI_T_liftBrushed"));
	Box(Car, FVector(X0, 0.01f, Z0), FVector(X1, 2.4f, Z0 + 0.05f), TEXT("MI_T_liftBrushed"));
	Box(Car, FVector(X0, 0.01f, Z1 - 0.05f), FVector(X1, 2.4f, Z1), TEXT("MI_T_liftBrushed"));
	Box(Car, FVector(X0, 0.01f, Z0), FVector(X0 + 0.05f, 2.4f, D0), TEXT("MI_T_liftBrushed"));
	Box(Car, FVector(X0, 0.01f, D1), FVector(X0 + 0.05f, 2.4f, Z1), TEXT("MI_T_liftBrushed"));
	Box(Car, FVector(X0, 2.3f, D0), FVector(X0 + 0.05f, 2.4f, D1), TEXT("MI_T_liftBrushed"));
	Box(Car, FVector(X1 - 0.09f, 0.9f, Z0 + 0.3f), FVector(X1 - 0.05f, 0.94f, Z1 - 0.3f), TEXT("MI_T_steel"));
	const float Mid = (D0 + D1) / 2.f;
	CarDoors.Add(Box(Car, FVector(X0 + 0.06f, 0.01f, D0), FVector(X0 + 0.1f, 2.3f, Mid), TEXT("MI_T_liftBrushed")));
	CarDoors.Add(Box(Car, FVector(X0 + 0.06f, 0.01f, Mid), FVector(X0 + 0.1f, 2.3f, D1), TEXT("MI_T_liftBrushed")));
	for (int32 f = 0; f < 2; ++f)
	{
		const float Y0 = FloorY[f];
		LandDoors[f].Add(Box(GetRootComponent(), FVector(X0 - 0.16f, Y0, D0), FVector(X0 - 0.12f, Y0 + 2.3f, Mid), TEXT("MI_T_liftBrushed")));
		LandDoors[f].Add(Box(GetRootComponent(), FVector(X0 - 0.16f, Y0, Mid), FVector(X0 - 0.12f, Y0 + 2.3f, D1), TEXT("MI_T_liftBrushed")));
	}
	PaxNav::NoNav(this);   // (it moves every frame: in the walkers' map while that is made it was changed under its threads - an engine check in the log, 2026-11-23)
	ADP_LOG(TEXT("Lift: the staff lift to the offices, its car %.1f x %.1f m between Floor 1 and Floor 2"), X1 - X0, Z1 - Z0);
}

void ATerminalLift::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Car) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	FVector P = Me ? Me->GetActorLocation() - FVector(0, 0, Me->GetSimpleCollisionHalfHeight()) : FVector(1e9);
	const float Px = P.X / 100.f, Py = P.Z / 100.f, Pz = -P.Y / 100.f;
	const bool bInCar = Inside.ExpandBy(-0.1f).IsInside(FVector2D(Px, Pz)) && FMath::Abs(Py - Y) < 1.2f;
	int32 Calling = -1;   // a floor where someone waits at the door
	for (int32 f = 0; f < 2; ++f)
		if (Px > Across.X - 2.5f && Px < Across.X && Pz > Door.X - 1.2f && Pz < Door.Y + 1.2f && FMath::Abs(Py - FloorY[f]) < 1.2f) Calling = f;

	if (Going >= 0)
	{
		// riding: doors shut first, then the car moves, then the doors open at the new floor
		if (Open > 0.f) Open = FMath::Max(0.f, Open - Dt / 1.2f);
		else
		{
			const float To = FloorY[Going];
			const float Step = 1.5f * Dt;
			const float Before = Y;
			Y = FMath::Abs(To - Y) <= Step ? To : Y + FMath::Sign(To - Y) * Step;
			if (bInCar && Me) Me->AddActorWorldOffset(FVector(0, 0, (Y - Before) * 100.f), false);
			if (Y == To) { At = Going; Going = -1; Wait = 0.f; }
		}
	}
	else
	{
		const bool bWant = bInCar || Calling == At;
		if (Calling >= 0 && Calling != At && !bInCar) Going = Calling;                  // called from the other floor
		else if (bWant) { Open = FMath::Min(1.f, Open + Dt / 1.2f); }
		else Open = FMath::Max(0.f, Open - Dt / 1.2f);
		// stepped in: a moment, then away to the other floor
		if (bInCar && Open >= 1.f) { Wait += Dt; if (Wait > 2.f) { Going = 1 - At; Wait = 0.f; } }
		else if (!bInCar) Wait = 0.f;
	}
	Car->SetWorldLocation(W(Inside.Min.X, Y - 0.01f, Inside.Min.Y));
	// the leaves slide apart along the wall (plan z), the car's and the floor's together
	const float Half = (Door.Y - Door.X) / 2.f * Open * 100.f * 0.95f;
	for (int32 i = 0; i < CarDoors.Num(); ++i) CarDoors[i]->SetRelativeLocation(CarDoors[i]->GetRelativeLocation() * FVector(1, 0, 1) + FVector(0, 0, 0));
	auto Slide = [&](UStaticMeshComponent* C, int32 Side, float BaseZ, float BaseY)
	{
		C->SetWorldLocation(FVector(C->GetComponentLocation().X, -(BaseZ * 100.f) + (Side ? -Half : Half), BaseY));
	};
	const float Q = (Door.Y - Door.X) / 4.f;
	for (int32 i = 0; i < 2; ++i)
	{
		Slide(CarDoors[i], i, i ? Door.Y - Q : Door.X + Q, (Y + 1.15f) * 100.f);
		for (int32 f = 0; f < 2; ++f)
			Slide(LandDoors[f][i], i, i ? Door.Y - Q : Door.X + Q, (FloorY[f] + 1.15f) * 100.f * 1.f), (void)0;
	}
	// the floors' doors are open only where the car stands
	for (int32 f = 0; f < 2; ++f)
		if (f != At || Going >= 0)
			for (int32 i = 0; i < 2; ++i)
				LandDoors[f][i]->SetWorldLocation(FVector(LandDoors[f][i]->GetComponentLocation().X, -((i ? Door.Y - Q : Door.X + Q) * 100.f), (FloorY[f] + 1.15f) * 100.f));
}
