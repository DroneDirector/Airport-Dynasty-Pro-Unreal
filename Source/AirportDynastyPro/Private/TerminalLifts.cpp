// The lifts: shafts, landing doors, the riding car and its calls, doors and ride, ported from the Android edition's
// lifts.ts with the same sizes, speeds and timings. The car's floor carries the player up and down.
#include "TerminalLifts.h"
#include "TerminalBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#pragma region Sizes
namespace
{
	constexpr float ShaftOut = 1.06f, ShaftT = 0.07f;                              // the shaft: outer half width, wall thickness
	constexpr float LiftDoor = 0.5f, LiftDH = 2.2f, CarHalf = 0.84f, CarH = 2.4f, LiftSpeed = 1.1f;
}
#pragma endregion

#pragma region Setup

ATerminalLifts::ATerminalLifts()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

USceneComponent* ATerminalLifts::Node(USceneComponent* Parent, float X, float Y, float Z, float YawDeg)
{
	USceneComponent* N = NewObject<USceneComponent>(this);
	N->SetupAttachment(Parent);
	N->SetMobility(EComponentMobility::Movable);
	N->SetRelativeLocationAndRotation(FVector(X * 100.f, -Z * 100.f, Y * 100.f), FRotator(0, YawDeg, 0));
	N->RegisterComponent();
	AddInstanceComponent(N);
	return N;
}

UStaticMeshComponent* ATerminalLifts::Box(USceneComponent* Parent, const FName Mat, float W, float H, float D, float X, float Y, float Z, bool bCollide)
{
	UMaterialInterface* M = nullptr;
	if (TObjectPtr<UMaterialInterface>* F = Mats.Find(Mat)) M = *F;
	else { M = ATerminalBuilder::KitMaterial(this, Mat); Mats.Add(Mat, M); }
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Parent);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Cube);
	C->SetMaterial(0, M);
	// the lift's frame: x across, y up, z out of its doors (Unreal's -y); the engine cube is 100 cm, pivot in the middle
	C->SetRelativeLocation(FVector(X * 100.f, -Z * 100.f, Y * 100.f));
	C->SetRelativeScale3D(FVector(W, D, H));
	C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

UTextRenderComponent* ATerminalLifts::Words(USceneComponent* Parent, const FString& Text, float X, float Y, float Z, float Size, FColor Colour)
{
	UTextRenderComponent* W = NewObject<UTextRenderComponent>(this);
	W->SetupAttachment(Parent);
	W->SetText(FText::FromString(Text));
	W->SetHorizontalAlignment(EHTA_Center);
	W->SetVerticalAlignment(EVRTA_TextCenter);
	W->SetWorldSize(Size * 100.f);
	W->SetTextRenderColor(Colour);
	// facing out of the doors (the lift's +z is Unreal's -y)
	W->SetRelativeLocationAndRotation(FVector(X * 100.f, -Z * 100.f, Y * 100.f), FRotator(0, -90.f, 0));
	W->RegisterComponent();
	AddInstanceComponent(W);
	return W;
}

#pragma endregion

#pragma region Build

void ATerminalLifts::Build(const FTerminalPlan& Plan)
{
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) return;
	F2Y = Plan.F2Y;
	F2Ceil = Plan.F2Ceil;

	for (const FPlanPoint& P : Plan.Lifts)
	{
		FLift L;
		L.P = P;
		// the lift's frame, turned as in the Android edition (0 faces +z)
		USceneComponent* O = NewObject<USceneComponent>(this);
		O->SetupAttachment(Root);
		O->SetMobility(EComponentMobility::Movable);
		O->SetRelativeLocationAndRotation(FTerminalPlan::ToWorld(P.X, 0, P.Z), FRotator(0, FMath::RadiansToDegrees(P.Face), 0));
		O->RegisterComponent();
		AddInstanceComponent(O);

		#pragma region Shaft
		// the back and the sides from the ground floor up to Floor 2's ceiling, over the edge of the holes
		Box(O, "liftClad", ShaftOut * 2, F2Ceil, ShaftT, 0, F2Ceil / 2, -ShaftOut + ShaftT / 2);
		Box(O, "liftClad", ShaftT, F2Ceil, ShaftOut * 2, -ShaftOut + ShaftT / 2, F2Ceil / 2, 0);
		Box(O, "liftClad", ShaftT, F2Ceil, ShaftOut * 2, ShaftOut - ShaftT / 2, F2Ceil / 2, 0);
		// its front: panels either side of the doors, and over the doors up to the next floor
		for (const float SX : {-1.f, 1.f}) Box(O, "liftClad", ShaftOut - LiftDoor, F2Ceil, ShaftT, SX * (LiftDoor + (ShaftOut - LiftDoor) / 2), F2Ceil / 2, ShaftOut - ShaftT / 2);
		for (const float FY : {0.f, F2Y})
		{
			const float Top = FY == 0 ? F2Y : F2Ceil;
			Box(O, "liftClad", LiftDoor * 2, Top - FY - LiftDH, ShaftT, 0, FY + LiftDH + (Top - FY - LiftDH) / 2, ShaftOut - ShaftT / 2);
		}
		Box(O, "liftClad", ShaftOut * 2, 0.1f, ShaftOut * 2, 0, F2Ceil - 0.05f, 0); // the cap
		#pragma endregion

		#pragma region Landings
		for (const float FY : {0.f, F2Y})
		{
			Box(O, "chrome", 0.05f, LiftDH + 0.05f, 0.1f, -LiftDoor - 0.02f, FY + LiftDH / 2, ShaftOut + 0.01f, false);
			Box(O, "chrome", 0.05f, LiftDH + 0.05f, 0.1f, LiftDoor + 0.02f, FY + LiftDH / 2, ShaftOut + 0.01f, false);
			Box(O, "chrome", LiftDoor * 2 + 0.09f, 0.05f, 0.1f, 0, FY + LiftDH + 0.02f, ShaftOut + 0.01f, false);
			FDoorPair D;
			D.A = Box(O, "liftDoor", LiftDoor, LiftDH, 0.03f, -LiftDoor / 2, FY + LiftDH / 2, ShaftOut - ShaftT - 0.02f);
			D.B = Box(O, "liftDoor", LiftDoor, LiftDH, 0.03f, LiftDoor / 2, FY + LiftDH / 2, ShaftOut - ShaftT - 0.02f);
			L.Landing.Add(D);
			Box(O, "black", 0.09f, 0.16f, 0.03f, LiftDoor + 0.2f, FY + 1.15f, ShaftOut + 0.02f, false);  // the call button
			Box(O, "green", 0.04f, 0.04f, 0.02f, LiftDoor + 0.2f, FY + 1.18f, ShaftOut + 0.035f, false);
			Box(O, "navy", 0.9f, 0.24f, 0.02f, 0, FY + LiftDH + 0.3f, ShaftOut + 0.01f, false);
			Words(O, P.Id + TEXT(" · LIFT"), 0, FY + LiftDH + 0.3f, ShaftOut + 0.025f, 0.12f, FColor(242, 244, 247));
		}
		#pragma endregion

		#pragma region Car
		USceneComponent* Car = Node(O, 0, 0, 0);
		L.Car = Car;
		Box(Car, "liftFloor", CarHalf * 2, 0.12f, CarHalf * 2, 0, -0.06f, 0);
		Box(Car, "liftBrushed", CarHalf * 2, CarH, 0.04f, 0, CarH / 2, -CarHalf);
		Box(Car, "liftBrushed", 0.04f, CarH, CarHalf * 2, -CarHalf, CarH / 2, 0);
		Box(Car, "liftBrushed", 0.04f, CarH, CarHalf * 2, CarHalf, CarH / 2, 0);
		Box(Car, "liftMirror", CarHalf * 2 - 0.2f, 1.1f, 0.01f, 0, 1.55f, -CarHalf + 0.03f, false);
		Box(Car, "chrome", CarHalf * 2 - 0.2f, 0.04f, 0.04f, 0, 0.95f, -CarHalf + 0.07f, false);
		for (const float SX : {-1.f, 1.f}) Box(Car, "chrome", 0.04f, 0.04f, CarHalf * 2 - 0.4f, SX * (CarHalf - 0.07f), 0.95f, -0.1f, false);
		Box(Car, "liftBrushed", CarHalf * 2, 0.08f, CarHalf * 2, 0, CarH + 0.04f, 0);
		Box(Car, "white", CarHalf * 1.4f, 0.02f, CarHalf * 1.4f, 0, CarH - 0.01f, 0, false); // the lit ceiling
		for (const float SX : {-1.f, 1.f}) Box(Car, "liftBrushed", CarHalf - LiftDoor + 0.04f, CarH, 0.05f, SX * (LiftDoor + (CarHalf - LiftDoor) / 2), CarH / 2, CarHalf);
		Box(Car, "liftBrushed", LiftDoor * 2, CarH - LiftDH, 0.05f, 0, LiftDH + (CarH - LiftDH) / 2, CarHalf);
		// the button panel by the door
		Box(Car, "black", 0.02f, 0.5f, 0.16f, CarHalf - 0.03f, 1.15f, CarHalf - 0.25f, false);
		Box(Car, "green", 0.02f, 0.05f, 0.05f, CarHalf - 0.045f, 1.25f, CarHalf - 0.25f, false);
		Box(Car, "green", 0.02f, 0.05f, 0.05f, CarHalf - 0.045f, 1.1f, CarHalf - 0.25f, false);
		Box(Car, "red", 0.02f, 0.05f, 0.05f, CarHalf - 0.045f, 0.95f, CarHalf - 0.25f, false);
		// the floor shown over the door, inside
		Box(Car, "black", 0.26f, 0.13f, 0.01f, 0, LiftDH + 0.1f, CarHalf - 0.03f, false);
		L.Display = Words(Car, TEXT("1"), 0, LiftDH + 0.1f, CarHalf - 0.04f, 0.09f, FColor(255, 154, 46));
		L.Display->SetRelativeRotation(FRotator(0, 90.f, 0)); // read from inside the car
		L.CarDoor.A = Box(Car, "liftDoor", LiftDoor, LiftDH, 0.03f, -LiftDoor / 2, LiftDH / 2, CarHalf - 0.06f);
		L.CarDoor.B = Box(Car, "liftDoor", LiftDoor, LiftDH, 0.03f, LiftDoor / 2, LiftDH / 2, CarHalf - 0.06f);
		#pragma endregion

		Lifts.Add(MoveTemp(L));
	}
	for (FLift& L : Lifts) { SetDoors(L); ShowFloor(L); }
	UE_LOG(LogTemp, Log, TEXT("Lifts built: %d"), Lifts.Num());
}

#pragma endregion

#pragma region Calls, doors and the ride

void ATerminalLifts::ShowFloor(FLift& L)
{
	const int32 Floor = L.Y > F2Y / 2 ? 2 : 1;
	const FString F = L.bMoving ? FString::Printf(TEXT("%s %d"), L.To > L.Y ? TEXT("UP") : TEXT("DOWN"), Floor) : FString::FromInt(Floor);
	if (F == L.Shown || !L.Display) return;
	L.Shown = F;
	L.Display->SetText(FText::FromString(F));
}

void ATerminalLifts::SetDoors(FLift& L)
{
	const float E = L.K * L.K * (3.f - 2.f * L.K), Off = E * (LiftDoor - 0.04f);
	const int32 Here = L.Y > F2Y / 2 ? 1 : 0;
	auto Put = [](USceneComponent* C, float X) { FVector P = C->GetRelativeLocation(); P.X = X * 100.f; C->SetRelativeLocation(P); };
	for (int32 i = 0; i < L.Landing.Num(); ++i)
	{
		const float O = i == Here && !L.bMoving ? Off : 0.f;
		Put(L.Landing[i].A, -LiftDoor / 2 - O);
		Put(L.Landing[i].B, LiftDoor / 2 + O);
	}
	Put(L.CarDoor.A, -LiftDoor / 2 - Off);
	Put(L.CarDoor.B, LiftDoor / 2 + Off);
}

void ATerminalLifts::Tick(float Dt)
{
	Super::Tick(Dt);
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player) return;
	const FVector P = Player->GetActorLocation();
	const float PX = P.X / 100.f, PZ = -P.Y / 100.f;
	const float PY = (P.Z - Player->GetSimpleCollisionHalfHeight()) / 100.f; // the player's feet

	for (FLift& L : Lifts)
	{
		// where the player is in the lift's own frame (x across, z out of its doors)
		const float DX = PX - L.P.X, DZ = PZ - L.P.Z, C = FMath::Cos(L.P.Face), S = FMath::Sin(L.P.Face);
		const float LX = DX * C - DZ * S, LZ = DX * S + DZ * C;
		const bool bInside = FMath::Abs(LX) < CarHalf - 0.12f && LZ > -CarHalf + 0.1f && LZ < CarHalf - 0.05f && FMath::Abs(PY - L.Y) < 1.0f;
		const int32 At = (FMath::Abs(LX) > 1.4f || LZ < ShaftOut - 0.05f || LZ > ShaftOut + 2.4f) ? -1 : (PY > F2Y / 2 ? 1 : 0);
		const int32 Here = L.Y > F2Y / 2 ? 1 : 0;

		if (L.bMoving)
		{
			// the ride: speeding up, cruising, slowing down to stop level with the floor
			const float Left = L.To - L.Y, Dist = FMath::Abs(Left);
			const float Want = FMath::Sign(Left) * FMath::Min(LiftSpeed, FMath::Sqrt(2.f * 0.9f * Dist) + 0.02f);
			L.V += FMath::Clamp(Want - L.V, -0.9f * Dt, 0.9f * Dt);
			float Step = L.V * Dt;
			if (FMath::Abs(Step) >= Dist) { Step = Left; L.bMoving = false; L.V = 0; L.Hold = 6; }
			L.Y += Step;
			L.Car->SetRelativeLocation(FVector(0, 0, L.Y * 100.f));
		}
		else
		{
			// standing at a floor: open for whoever is there, close after a while, set off once someone is inside
			const bool bSomeone = bInside || At == Here;
			if (At >= 0 && At != Here && !bInside)
			{
				if (L.K > 0) L.K = FMath::Max(0.f, L.K - Dt * 1.6f);
				else { L.To = At ? F2Y : 0.f; L.bMoving = true; }
			}
			else if (bSomeone) { L.K = FMath::Min(1.f, L.K + Dt * 1.4f); L.Hold = FMath::Max(L.Hold, 4.f); }
			else { L.Hold -= Dt; if (L.Hold <= 0) L.K = FMath::Max(0.f, L.K - Dt * 1.2f); }
			if (bInside)
			{
				L.InT += Dt;
				if (L.InT >= 1.6f)
				{
					L.K = FMath::Max(0.f, L.K - Dt * 1.6f);
					if (L.K <= 0) { L.To = Here ? 0.f : F2Y; L.bMoving = true; L.InT = 0; }
				}
			}
			else L.InT = 0;
		}
		SetDoors(L);
		ShowFloor(L);
	}
}

#pragma endregion
