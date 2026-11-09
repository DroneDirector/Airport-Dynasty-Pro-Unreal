// Builds the grey-box terminal at run time from the shared layout: floors, walls with door openings,
// glass frames, columns, stairs, lifts and stand markings.
#include "TerminalBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#pragma region Sizes
namespace
{
	constexpr float WallT = 0.2f;   // wall thickness, m
	constexpr float SlabT = 0.3f;   // floor slab thickness, m
	constexpr float DoorH = 2.8f;   // door opening height, m
	constexpr float MullionStep = 1.5f;
	constexpr float MullionW = 0.08f;
}
#pragma endregion

#pragma region Setup

ATerminalBuilder::ATerminalBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UInstancedStaticMeshComponent* ATerminalBuilder::Kit(const TCHAR* Name, UStaticMesh* Mesh, const FLinearColor& Colour)
{
	UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this, Name);
	K->SetupAttachment(Root);
	K->SetStaticMesh(Mesh);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Color"), Colour);
		K->SetMaterial(0, M);
	}
	K->SetCollisionProfileName(TEXT("BlockAll"));
	K->RegisterComponent();
	AddInstanceComponent(K);
	return K;
}

void ATerminalBuilder::Box(UInstancedStaticMeshComponent* K, float X0, float Y0, float Z0, float X1, float Y1, float Z1)
{
	const FVector A = FTerminalPlan::ToWorld(FMath::Min(X0, X1), FMath::Min(Y0, Y1), FMath::Min(Z0, Z1));
	const FVector B = FTerminalPlan::ToWorld(FMath::Max(X0, X1), FMath::Max(Y0, Y1), FMath::Max(Z0, Z1));
	const FVector Size = (B - A).GetAbs();
	if (Size.X < 1.f || Size.Y < 1.f || Size.Z < 1.f) return;
	// the engine cube is 100 cm across with its pivot in the middle
	K->AddInstance(FTransform(FRotator::ZeroRotator, (A + B) * 0.5f, Size / 100.f));
}

#pragma endregion

#pragma region Build

bool ATerminalBuilder::Build()
{
	for (UInstancedStaticMeshComponent* K : { Ground.Get(), Floors.Get(), Ceilings.Get(), Walls.Get(), Frames.Get(), Pillars.Get(), Steps.Get(), Markings.Get() })
		if (K) K->DestroyComponent();

	Plan = FTerminalPlan();
	if (!Plan.Load()) return false;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!Cube || !Cyl) return false;

	Ground   = Kit(TEXT("Ground"),   Cube, FLinearColor(0.10f, 0.11f, 0.12f));
	Floors   = Kit(TEXT("Floors"),   Cube, FLinearColor(0.62f, 0.60f, 0.56f));
	Ceilings = Kit(TEXT("Ceilings"), Cube, FLinearColor(0.85f, 0.85f, 0.85f));
	Walls    = Kit(TEXT("Walls"),    Cube, FLinearColor(0.78f, 0.78f, 0.76f));
	Frames   = Kit(TEXT("Frames"),   Cube, FLinearColor(0.20f, 0.22f, 0.25f));
	Pillars  = Kit(TEXT("Pillars"),  Cyl,  FLinearColor(0.70f, 0.70f, 0.72f));
	Steps    = Kit(TEXT("Steps"),    Cube, FLinearColor(0.45f, 0.45f, 0.47f));
	Markings = Kit(TEXT("Markings"), Cube, FLinearColor(0.90f, 0.75f, 0.10f));

	// apron and landside ground around the whole plan
	float MinX = 1e9f, MaxX = -1e9f, MinZ = 1e9f, MaxZ = -1e9f;
	for (const FPlanRoom& R : Plan.Rooms)
	{
		MinX = FMath::Min(MinX, R.X0); MaxX = FMath::Max(MaxX, R.X1);
		MinZ = FMath::Min(MinZ, R.Z0); MaxZ = FMath::Max(MaxZ, R.Z1);
	}
	Box(Ground, MinX - 400, -0.52f, MinZ - 300, MaxX + 400, -0.02f, MaxZ + 400);

	for (const FPlanRoom& R : Plan.Rooms) BuildRoom(R);
	for (const FPlanStair& S : Plan.Stairs) BuildStair(S);

	for (const FPlanPoint& C : Plan.Columns)
	{
		const FVector P = FTerminalPlan::ToWorld(C.X, Plan.Roof * 0.5f, C.Z);
		Pillars->AddInstance(FTransform(FRotator::ZeroRotator, P, FVector(0.6f, 0.6f, Plan.Roof)));
	}
	for (const FPlanPoint& L : Plan.Lifts)
		Box(Walls, L.X - 1.2f, 0, L.Z - 1.2f, L.X + 1.2f, Plan.F2Y + 3.f, L.Z + 1.2f);
	for (const FPlanPoint& S : Plan.Stands)
		Box(Markings, S.X - 0.15f, 0, S.Z, S.X + 0.15f, 0.02f, S.Z + 45.f); // stand lead-in line from the nose position

	UE_LOG(LogTemp, Log, TEXT("Terminal built: %d rooms, %d doors"), Plan.Rooms.Num(), Plan.Doors.Num());
	return true;
}

#pragma endregion

#pragma region Rooms and walls

void ATerminalBuilder::BuildRoom(const FPlanRoom& R)
{
	const float Y0 = Plan.FloorY(R.Floor), Y1 = Plan.CeilY(R.Floor);
	Box(Floors, R.X0, Y0 - SlabT, R.Z0, R.X1, Y0, R.Z1);
	Box(Ceilings, R.X0, Y1, R.Z0, R.X1, Y1 + 0.1f, R.Z1);
	if (R.Floor >= 2 || R.Kind == TEXT("hall")) Box(Ceilings, R.X0, Plan.Roof, R.Z0, R.X1, Plan.Roof + 0.4f, R.Z1);

	BuildWall(R, true,  R.Z0, R.X0, R.X1, R.Glass.Contains(TEXT("s")));
	BuildWall(R, true,  R.Z1, R.X0, R.X1, R.Glass.Contains(TEXT("n")));
	BuildWall(R, false, R.X0, R.Z0, R.Z1, R.Glass.Contains(TEXT("w")));
	BuildWall(R, false, R.X1, R.Z0, R.Z1, R.Glass.Contains(TEXT("e")));
}

void ATerminalBuilder::BuildWall(const FPlanRoom& R, bool bAlongX, float Fixed, float From, float To, bool bGlass)
{
	const float Y0 = Plan.FloorY(R.Floor), Y1 = Plan.CeilY(R.Floor);

	// door openings on this wall line
	TArray<FVector2f> Gaps;
	for (const FPlanDoor& D : Plan.Doors)
	{
		if (D.Floor != R.Floor || D.bAlongX != bAlongX) continue;
		const float Line = bAlongX ? D.Z : D.X, At = bAlongX ? D.X : D.Z;
		if (FMath::Abs(Line - Fixed) > 0.35f || At < From || At > To) continue;
		Gaps.Add(FVector2f(At - D.Width * 0.5f, At + D.Width * 0.5f));
	}
	Gaps.Sort([](const FVector2f& A, const FVector2f& B) { return A.X < B.X; });

	auto Piece = [&](UInstancedStaticMeshComponent* K, float A, float B, float Bottom, float Top)
	{
		if (B - A < 0.01f) return;
		if (bAlongX) Box(K, A, Bottom, Fixed - WallT * 0.5f, B, Top, Fixed + WallT * 0.5f);
		else         Box(K, Fixed - WallT * 0.5f, Bottom, A, Fixed + WallT * 0.5f, Top, B);
	};
	auto Solid = [&](float A, float B)
	{
		if (!bGlass) { Piece(Walls, A, B, Y0, Y1); return; }
		// glass: sill, head and slim mullions; the panes themselves come with the real glass material
		Piece(Frames, A, B, Y0, Y0 + 0.1f);
		Piece(Frames, A, B, Y1 - 0.15f, Y1);
		for (float M = A; M <= B; M += MullionStep) Piece(Frames, M - MullionW * 0.5f, M + MullionW * 0.5f, Y0, Y1);
	};

	float Cursor = From;
	for (const FVector2f& G : Gaps)
	{
		Solid(Cursor, G.X);
		Piece(bGlass ? Frames : Walls, G.X, G.Y, Y0 + DoorH, Y1); // lintel
		Cursor = FMath::Max(Cursor, G.Y);
	}
	Solid(Cursor, To);
}

#pragma endregion

#pragma region Stairs

void ATerminalBuilder::BuildStair(const FPlanStair& S)
{
	const int32 N = FMath::Max(1, FMath::RoundToInt(Plan.F2Y / 0.18f));
	const float Rise = Plan.F2Y / N;
	const bool bX = S.Dir.StartsWith(TEXT("x"));
	const bool bUp = S.Dir.EndsWith(TEXT("+"));
	const float A0 = bX ? S.X0 : S.Z0, A1 = bX ? S.X1 : S.Z1;
	const float Run = (A1 - A0) / N;
	for (int32 i = 0; i < N; ++i)
	{
		const int32 k = bUp ? i : N - 1 - i; // step k rises to (k+1)*Rise
		const float From = A0 + Run * i, To = From + Run;
		const float Top = (k + 1) * Rise;
		if (bX) Box(Steps, From, 0, S.Z0, To, Top, S.Z1);
		else    Box(Steps, S.X0, 0, From, S.X1, Top, To);
	}
}

#pragma endregion
