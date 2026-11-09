// Crowd test: about a hundred simple walkers wandering inside the terminal's rooms.
#include "CrowdTest.h"
#include "TerminalPlan.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#pragma region Setup

ACrowdTest::ACrowdTest()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	auto Make = [&](const TCHAR* Name, const TCHAR* MeshPath)
	{
		UInstancedStaticMeshComponent* K = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		K->SetupAttachment(Root);
		K->SetMobility(EComponentMobility::Movable);
		K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ConstructorHelpers::FObjectFinder<UStaticMesh> M(MeshPath);
		if (M.Succeeded()) K->SetStaticMesh(M.Object);
		return K;
	};
	Bodies = Make(TEXT("Bodies"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Heads = Make(TEXT("Heads"), TEXT("/Engine/BasicShapes/Sphere.Sphere"));
}

void ACrowdTest::Start(const FTerminalPlan& Plan, int32 Count)
{
	Rng.Initialize(20261109);
	Areas.Reset();
	for (const FPlanRoom& R : Plan.Rooms)
		if ((R.X1 - R.X0) * (R.Z1 - R.Z0) > 150.f && R.Kind != TEXT("staff") && R.Kind != TEXT("plant"))
			Areas.Add({ R.X0 + 1.f, R.Z0 + 1.f, R.X1 - 1.f, R.Z1 - 1.f, Plan.FloorY(R.Floor) });
	if (Areas.Num() == 0) return;

	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* B = UMaterialInstanceDynamic::Create(Base, this);
		B->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 0.25f, 0.45f));
		Bodies->SetMaterial(0, B);
		UMaterialInstanceDynamic* H = UMaterialInstanceDynamic::Create(Base, this);
		H->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.80f, 0.62f, 0.50f));
		Heads->SetMaterial(0, H);
	}

	Walkers.SetNum(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		FWalker& W = Walkers[i];
		W.Room = Rng.RandRange(0, Areas.Num() - 1);
		W.Pos = RandomPointIn(W.Room);
		W.Target = RandomPointIn(W.Room);
		W.Speed = Rng.FRandRange(1.0f, 1.5f);
		W.Height = Rng.FRandRange(1.55f, 1.90f);
		Bodies->AddInstance(FTransform::Identity);
		Heads->AddInstance(FTransform::Identity);
		Place(i, i == Count - 1);
	}
}

#pragma endregion

#pragma region Walking

FVector2f ACrowdTest::RandomPointIn(int32 Room) const
{
	const FArea& A = Areas[Room];
	return FVector2f(Rng.FRandRange(A.X0, A.X1), Rng.FRandRange(A.Z0, A.Z1));
}

void ACrowdTest::Place(int32 i, bool bLast)
{
	const FWalker& W = Walkers[i];
	const float Floor = Areas[W.Room].Y;
	const float BodyH = W.Height - 0.25f;
	const FVector Body = FTerminalPlan::ToWorld(W.Pos.X, Floor + BodyH * 0.5f, W.Pos.Y);
	const FVector Head = FTerminalPlan::ToWorld(W.Pos.X, Floor + W.Height - 0.12f, W.Pos.Y);
	Bodies->UpdateInstanceTransform(i, FTransform(FRotator::ZeroRotator, Body, FVector(0.45f, 0.45f, BodyH)), true, bLast, true);
	Heads->UpdateInstanceTransform(i, FTransform(FRotator::ZeroRotator, Head, FVector(0.24f)), true, bLast, true);
}

void ACrowdTest::Tick(float Dt)
{
	Super::Tick(Dt);
	for (int32 i = 0; i < Walkers.Num(); ++i)
	{
		FWalker& W = Walkers[i];
		if (W.Wait > 0.f) { W.Wait -= Dt; continue; }
		const FVector2f To = W.Target - W.Pos;
		const float D = To.Size();
		if (D < 0.2f)
		{
			W.Target = RandomPointIn(W.Room);
			W.Wait = Rng.FRandRange(0.f, 6.f); // stop for a moment, like looking at a board
			continue;
		}
		W.Pos += To / D * FMath::Min(D, W.Speed * Dt);
		Place(i, false);
	}
	if (Walkers.Num() > 0)
	{
		Bodies->MarkRenderStateDirty();
		Heads->MarkRenderStateDirty();
	}
}

#pragma endregion
