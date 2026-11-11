// The passengers: Mass Entities on their trip through the terminal, walking the walking map's ways, drawn as simple
// figures (a body in one of four colours, a head) until they get their MetaHuman bodies.
#include "TerminalCrowd.h"
#include "ADPLog.h"
#include "PassengerFragments.h"
#include "TerminalBuilder.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#pragma region Setup

ATerminalCrowd::ATerminalCrowd()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ATerminalCrowd::Start(const FTerminalPlan& Plan, int32 Count)
{
	UWorld* W = GetWorld();
	UMassEntitySubsystem* Mass = W ? W->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Mass || Count <= 0) return;
	FMassEntityManager& EM = Mass->GetMutableEntityManager();

	Rng.Initialize(20261109);
	F2Y = Plan.F2Y;
	Map.Build(Plan);
	Kerb = Map.Find(TEXT("KERB"));
	Hall = Map.Find(TEXT("F1-HALL"));
	Security = Map.Find(TEXT("F1-SECURITY"));
	for (const TCHAR* Id : {TEXT("F2-FOOD"), TEXT("F2-TAXFREE"), TEXT("F2-BAR"), TEXT("F2-LANDING"), TEXT("F1-AIRSIDE")})
		if (const int32 I = Map.Find(Id); I != INDEX_NONE) Upstairs.Add(I);
	if (Kerb == INDEX_NONE || Hall == INDEX_NONE || Security == INDEX_NONE) return;

	// the gates 1–8, where each boards (the jetway door upstairs, the walk-out door on the ground) and what lies beyond
	for (int32 g = 1; g <= 8; ++g)
	{
		const FString Id = g <= 4 ? FString::Printf(TEXT("F2-GATE-%d"), g) : FString::Printf(TEXT("F1-GATE-%d"), g);
		const int32 Gi = Map.Find(Id);
		Gates.Add(Gi);
		FVector Out = FVector::ZeroVector;
		int32 Beyond = INDEX_NONE;
		for (const FPlanDoor& D : Plan.Doors)
			if (D.A == Id && (D.B.StartsWith(TEXT("J-")) || D.B == TEXT("APRON")))
			{
				Out = FVector(D.X, Plan.FloorY(D.Floor), D.Z);
				Beyond = Map.Find(D.B);
				break;
			}
		GateWayOut.Add(Out);
		GateOut.Add(Beyond);
	}

	// the figures: bodies in four colours, heads
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	auto Make = [&](const FName Name, UStaticMesh* Mesh, const FName Mat)
	{
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this, Name);
		K->SetupAttachment(Root);
		K->SetMobility(EComponentMobility::Movable);
		K->SetStaticMesh(Mesh);
		K->SetMaterial(0, ATerminalBuilder::KitMaterial(this, Mat));
		K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		K->SetCastShadow(true);
		K->RegisterComponent();
		AddInstanceComponent(K);
		return K;
	};
	for (const FName C : {FName("navy"), FName("fabricRust"), FName("fabricGrey"), FName("fabricTeal")}) Bodies.Add(Make(*(TEXT("Bodies_") + C.ToString()), Cyl, C));
	Heads = Make("Heads", Ball, "skin");

	// the passengers as Mass Entities
	const FMassArchetypeHandle Arch = EM.CreateArchetype({FPaxFragment::StaticStruct()});
	TArray<FMassEntityHandle> Handles;
	EM.BatchCreateEntities(Arch, Count, Handles);
	Ways.SetNum(Count);
	BodyKit.SetNum(Count);
	BodyIdx.SetNum(Count);
	for (int32 i = 0; i < Handles.Num(); ++i)
	{
		FPaxFragment& P = EM.GetFragmentDataChecked<FPaxFragment>(Handles[i]);
		P.Num = i;
		P.Speed = Rng.FRandRange(1.0f, 1.5f);
		P.Height = Rng.FRandRange(1.55f, 1.92f);
		BodyKit[i] = Rng.RandRange(0, Bodies.Num() - 1);
		BodyIdx[i] = Bodies[BodyKit[i]]->AddInstance(FTransform::Identity);
		Heads->AddInstance(FTransform::Identity);
		Begin(P, true);
		Draw(P, false);
	}
	for (UInstancedStaticMeshComponent* K : Bodies) K->MarkRenderStateDirty();
	Heads->MarkRenderStateDirty();

	Query = FMassEntityQuery(EM.AsShared());
	Query.AddRequirement<FPaxFragment>(EMassFragmentAccess::ReadWrite);
	bStarted = true;
	ADP_LOG(TEXT("Crowd started: %d passengers, %d places, %d openings"), Count, Map.Places.Num(), Map.Portals.Num());
}

#pragma endregion

#pragma region Trips

void ATerminalCrowd::WalkTo(FPaxFragment& P, int32 Place, const FVector& At)
{
	TArray<FVector>& W = Ways[P.Num];
	if (!Map.Way(P.Place, P.Pos, Place, At, W)) { W.Reset(); W.Add(At); }
	P.Goal = Place;
	P.Step = 0;
	P.bWalking = true;
}

void ATerminalCrowd::Begin(FPaxFragment& P, bool bAnywhere)
{
	P.Gate = Rng.RandRange(1, 8);
	if (!Gates.IsValidIndex(P.Gate - 1) || Gates[P.Gate - 1] == INDEX_NONE) P.Gate = 1;
	if (!bAnywhere)
	{
		// a new passenger: out of a car or a bus on the kerb
		P.Stage = EPaxStage::Arrive;
		P.Place = Kerb;
		P.Pos = Map.RandomIn(Kerb, Rng);
		P.Wait = Rng.FRandRange(0, 4);
		WalkTo(P, Hall, Map.RandomIn(Hall, Rng));
		return;
	}
	// the first crowd: spread over every stage, standing where that stage happens
	const int32 S = Rng.RandRange(0, 4);
	P.Stage = EPaxStage(S);
	const int32 Where = S == 0 ? Hall : S == 1 ? Hall : S == 2 ? Security : S == 3 ? Upstairs[Rng.RandRange(0, Upstairs.Num() - 1)] : Gates[P.Gate - 1];
	P.Place = Where;
	P.Pos = Map.RandomIn(Where, Rng);
	P.bWalking = false;
	P.Wait = Rng.FRandRange(0, 30);
}

void ATerminalCrowd::Next(FPaxFragment& P)
{
	switch (P.Stage)
	{
	case EPaxStage::Arrive:
	{
		// to a check-in desk: in front of one of the two islands (desks 1–12 at x 44–69, 13–24 at x 71–96)
		P.Stage = EPaxStage::CheckIn;
		const float X = Rng.FRandRange(0, 1) < 0.5f ? Rng.FRandRange(45, 68) : Rng.FRandRange(72, 95);
		WalkTo(P, Hall, FVector(X, 0, 15.3f));
		P.Wait = Rng.FRandRange(15, 40);
		break;
	}
	case EPaxStage::CheckIn:
	{
		// through a security lane: up to the walk-through frame, then out the far side
		P.Stage = EPaxStage::Security;
		static const float Lanes[] = {53, 59.2f, 65.3f, 71.5f, 77.7f, 83.8f};
		const float X = Lanes[Rng.RandRange(0, 5)] + 1.675f;
		WalkTo(P, Security, FVector(X, 0, 39.5f));
		Ways[P.Num].Insert(FVector(X, 0, 32.5f), FMath::Max(0, Ways[P.Num].Num() - 1));
		P.Wait = Rng.FRandRange(4, 10);
		break;
	}
	case EPaxStage::Security:
	{
		// a stop on the way: the Food Walk, Tax Free, the bar, or a seat
		P.Stage = EPaxStage::Airside;
		const int32 Where = Upstairs[Rng.RandRange(0, Upstairs.Num() - 1)];
		WalkTo(P, Where, Map.RandomIn(Where, Rng));
		P.Wait = Rng.FRandRange(30, 90);
		break;
	}
	case EPaxStage::Airside:
	{
		// to the gate: a seat in the rows facing the windows
		P.Stage = EPaxStage::Gate;
		const int32 G = Gates[P.Gate - 1];
		const FWalkMap::FPlace& R = Map.Places[G];
		WalkTo(P, G, FVector(Rng.FRandRange(R.X0 + 3, R.X1 - 3), R.Y, Rng.FRandRange(R.Z0 + 3, R.Z0 + 9.5f)));
		P.Wait = Rng.FRandRange(40, 120);
		break;
	}
	case EPaxStage::Gate:
	{
		// aboard: through the jetway door or the walk-out door, and on out of the terminal
		P.Stage = EPaxStage::Board;
		const int32 Gi = P.Gate - 1;
		if (GateOut.IsValidIndex(Gi) && GateOut[Gi] != INDEX_NONE)
			WalkTo(P, GateOut[Gi], GateWayOut[Gi] + FVector(0, 0, 6));
		else
			WalkTo(P, P.Place, P.Pos);
		P.Wait = 0;
		break;
	}
	case EPaxStage::Board:
		// aboard: a new passenger arrives at the kerb in their place
		Begin(P, false);
		break;
	}
}

void ATerminalCrowd::Step(FPaxFragment& P, float Dt)
{
	if (!P.bWalking)
	{
		if (P.Wait > 0) { P.Wait -= Dt; return; }
		Next(P);
		return;
	}
	const TArray<FVector>& W = Ways[P.Num];
	float Left = P.Speed * Dt;
	while (Left > 0 && P.Step < W.Num())
	{
		const FVector To = W[P.Step] - P.Pos;
		const float D = To.Size();
		if (D <= Left) { P.Pos = W[P.Step]; Left -= D; ++P.Step; continue; }
		P.Pos += To / D * Left;
		Left = 0;
	}
	if (P.Step >= W.Num())
	{
		P.bWalking = false;
		P.Place = P.Goal;
	}
}

#pragma endregion

#pragma region Figures

void ATerminalCrowd::Draw(const FPaxFragment& P, bool bLast)
{
	const float BodyH = P.Height - 0.25f;
	const FVector Body = FTerminalPlan::ToWorld(P.Pos.X, P.Pos.Y + BodyH * 0.5f, P.Pos.Z);
	const FVector Head = FTerminalPlan::ToWorld(P.Pos.X, P.Pos.Y + P.Height - 0.12f, P.Pos.Z);
	Bodies[BodyKit[P.Num]]->UpdateInstanceTransform(BodyIdx[P.Num], FTransform(FRotator::ZeroRotator, Body, FVector(0.45f, 0.45f, BodyH)), true, false, true);
	Heads->UpdateInstanceTransform(P.Num, FTransform(FRotator::ZeroRotator, Head, FVector(0.24f)), true, false, true);
}

void ATerminalCrowd::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bStarted) return;
	UMassEntitySubsystem* Mass = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!Mass) return;
	FMassEntityManager& EM = Mass->GetMutableEntityManager();

	// people within 60 m of the player are redrawn every frame, the rest every fourth frame (they keep walking)
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Eye = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	++Frame;
	FMassExecutionContext Ctx(EM, Dt);
	Query.ForEachEntityChunk(Ctx, [&](FMassExecutionContext& C)
	{
		const TArrayView<FPaxFragment> Pax = C.GetMutableFragmentView<FPaxFragment>();
		for (FPaxFragment& P : Pax)
		{
			Step(P, Dt);
			const FVector At = FTerminalPlan::ToWorld(P.Pos.X, P.Pos.Y, P.Pos.Z);
			if (FVector::DistSquared(At, Eye) < FMath::Square(6000.f) || (Frame + P.Num) % 4 == 0) Draw(P, false);
		}
	});
	for (UInstancedStaticMeshComponent* K : Bodies) K->MarkRenderStateDirty();
	Heads->MarkRenderStateDirty();
}

#pragma endregion
