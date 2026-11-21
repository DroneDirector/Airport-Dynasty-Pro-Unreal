// Swiss flags that really wave - see Flags.h.
#include "Flags.h"
#include "ADPLog.h"
#include "Site.h"
#include "WindSocks.h"
#include "Puddles.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"

namespace
{
	const int32 N = 33;                 // particles along each side (32 cells: the cross's edges fall on cell lines)
	const float SizeCm = 150.f;         // a 1.5 m square flag
	const float Cell = SizeCm / (N - 1);
	const float SubDt = 1.f / 120.f;
	const FColor Red(213, 43, 30), White(255, 255, 255);
	/** The Swiss cross: the cross 20/32 of the flag, its arms 6/32 wide (official proportions). */
	bool InCross(int32 i, int32 j)
	{
		const bool bH = j >= 13 && j < 19 && i >= 6 && i < 26, bV = i >= 13 && i < 19 && j >= 6 && j < 26;
		return bH || bV;
	}
	int32 Id(int32 i, int32 j) { return j * N + i; }
}

AFlags::AFlags()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AFlags::Place(UWorld* W)
{
	if (!W || ADPSite::Id() != TEXT("chablais")) return;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AFlags* F = W->SpawnActor<AFlags>(FVector::ZeroVector, FRotator::ZeroRotator, P))
	{
		// (plan metres: by the fire station's landside corner, and on the forecourt facing the terminal's main doors)
		F->Add(FVector(-1180.f * 100.f, 26.f * 100.f, 28.f), 9.f);
		F->Add(FVector(60.f * 100.f, 56.f * 100.f, 0.f), 11.f);   // (the car park's front edge, across the road from the main doors: in the open, seen from the forecourt)
	}
}

void AFlags::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Flag/M_FlagCloth.M_FlagCloth")))
		Mat = UMaterialInstanceDynamic::Create(M, this);
	for (FCloth& C : Cloths) if (C.Mesh && Mat) C.Mesh->SetMaterial(0, Mat);
	if (TSharedPtr<FJsonObject> A = ADPSite::AirportBlock()) { double H = 336; A->TryGetNumberField(TEXT("hdg"), H); RunwayHdg = float(H); }
}

void AFlags::Add(const FVector& Foot, float PoleM)
{
	// the pole: a slim white mast with a ball on top
	if (UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		UStaticMeshComponent* Pole = NewObject<UStaticMeshComponent>(this);
		Pole->SetStaticMesh(Cyl); Pole->SetupAttachment(GetRootComponent());
		Pole->SetWorldLocation(Foot + FVector(0, 0, PoleM * 50.f)); Pole->SetWorldScale3D(FVector(0.09f, 0.09f, PoleM));
		Pole->RegisterComponent();
		if (UMaterialInterface* Wm = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))) Pole->SetMaterial(0, Wm);
	}
	FCloth C;
	C.Top = Foot + FVector(0, 0, PoleM * 100.f - 25.f);
	// hanging down beside the pole at first (it lifts with the wind in its first seconds)
	C.P.SetNum(N * N);
	for (int32 j = 0; j < N; ++j) for (int32 i = 0; i < N; ++i)
		C.P[Id(i, j)] = C.Top + FVector(6.f + i * Cell * 0.2f, i * Cell * 0.05f, -j * Cell - i * Cell * 0.95f);
	C.Prev = C.P;
	C.Mesh = NewObject<UProceduralMeshComponent>(this);
	C.Mesh->SetupAttachment(GetRootComponent()); C.Mesh->SetCastShadow(true); C.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C.Mesh->RegisterComponent(); AddInstanceComponent(C.Mesh);
	Cloths.Add(C);
	Draw(Cloths.Last(), true);
}

void AFlags::Step(FCloth& C, float H)
{
	// the wind: the windsocks' (the live weather's), blowing TOWARDS where it goes; a light turbulence on top
	const float From = FMath::DegreesToRadians(AWindSocks::WindFrom), Ms = AWindSocks::WindKt * 0.5144f;
	// (plan north is the runway's frame turned; the world axes here: x along the plan, -y; the flags stream with the wind)
	// (a bearing b in the world: plan x points along the runway's true heading; world = (plan x, -plan y))
	const float Ang = From + PI - FMath::DegreesToRadians(RunwayHdg);
	const FVector Wind = FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * Ms * 100.f;
	const FVector G(0, 0, -980.f);
	for (int32 k = 0; k < C.P.Num(); ++k)
	{
		if (k % N == 0) continue;   // (the hoist edge: on the rope)
		const FVector V = (C.P[k] - C.Prev[k]);
		const FVector Turb(FMath::PerlinNoise1D(Clock * 1.7f + k * 0.031f), FMath::PerlinNoise1D(Clock * 1.3f + k * 0.017f + 7.f), FMath::PerlinNoise1D(Clock * 2.1f + k * 0.023f + 3.f));
		// air drag towards the wind's speed (cloth is light: a strong pull), gravity
		const FVector Rel = Wind * (1.f + 0.35f * Turb.X) + Turb * Ms * 25.f - V / H;
		const FVector Acc = G + Rel * 2.2f;
		const FVector Nw = C.P[k] + V * 0.992f + Acc * H * H;
		C.Prev[k] = C.P[k]; C.P[k] = Nw;
	}
	// the hoist: pinned to the rope beside the pole
	for (int32 j = 0; j < N; ++j) { const int32 k = Id(0, j); C.P[k] = C.Top + FVector(5.f, 0, -j * Cell); C.Prev[k] = C.P[k]; }
	// the springs: stretch (neighbours), shear (diagonals), bend (two apart) - a few passes
	auto Hold = [&](int32 a, int32 b, float Rest, float K)
	{
		FVector D = C.P[b] - C.P[a]; const float L = D.Size(); if (L < 1e-3f) return;
		D *= (L - Rest) / L * 0.5f * K;
		const bool PinA = a % N == 0, PinB = b % N == 0;
		if (!PinA) C.P[a] += PinB ? D * 2.f : D;
		if (!PinB) C.P[b] -= PinA ? D * 2.f : D;
	};
	for (int32 It = 0; It < 6; ++It)
		for (int32 j = 0; j < N; ++j) for (int32 i = 0; i < N; ++i)
		{
			if (i + 1 < N) Hold(Id(i, j), Id(i + 1, j), Cell, 1.f);
			if (j + 1 < N) Hold(Id(i, j), Id(i, j + 1), Cell, 1.f);
			if (It % 2 == 0)
			{
				if (i + 1 < N && j + 1 < N) { Hold(Id(i, j), Id(i + 1, j + 1), Cell * 1.4142f, 0.6f); Hold(Id(i + 1, j), Id(i, j + 1), Cell * 1.4142f, 0.6f); }
				if (i + 2 < N) Hold(Id(i, j), Id(i + 2, j), Cell * 2.f, 0.15f);
				if (j + 2 < N) Hold(Id(i, j), Id(i, j + 2), Cell * 2.f, 0.15f);
			}
		}
	// never through the pole
	for (FVector& P : C.P) { FVector2D D(P.X - C.Top.X, P.Y - C.Top.Y); if (D.Size() < 6.f) { D = D.GetSafeNormal() * 6.f; P.X = C.Top.X + D.X; P.Y = C.Top.Y + D.Y; } }
}

void AFlags::Draw(FCloth& C, bool bFirst)
{
	// each cell its own four corners (a crisp edge between the red and the white), normals from the particles
	TArray<FVector> V; TArray<int32> T; TArray<FVector> Nn; TArray<FVector2D> UV; TArray<FColor> Col; TArray<FProcMeshTangent> Tg;
	V.Reserve((N - 1) * (N - 1) * 4);
	auto Normal = [&](int32 i, int32 j)
	{
		const FVector Dx = C.P[Id(FMath::Min(i + 1, N - 1), j)] - C.P[Id(FMath::Max(i - 1, 0), j)];
		const FVector Dy = C.P[Id(i, FMath::Min(j + 1, N - 1))] - C.P[Id(i, FMath::Max(j - 1, 0))];
		return FVector::CrossProduct(Dy, Dx).GetSafeNormal();
	};
	for (int32 j = 0; j < N - 1; ++j) for (int32 i = 0; i < N - 1; ++i)
	{
		const int32 B = V.Num();
		const FColor K = InCross(i, j) ? White : Red;
		for (const FIntPoint Q : {FIntPoint(i, j), FIntPoint(i + 1, j), FIntPoint(i + 1, j + 1), FIntPoint(i, j + 1)})
		{ V.Add(C.P[Id(Q.X, Q.Y)]); Nn.Add(Normal(Q.X, Q.Y)); UV.Add(FVector2D(float(Q.X) / (N - 1), float(Q.Y) / (N - 1))); Col.Add(K); }
		T.Append({B, B + 1, B + 2, B, B + 2, B + 3});
	}
	if (bFirst) { C.Mesh->CreateMeshSection(0, V, T, Nn, UV, Col, Tg, false); if (Mat) C.Mesh->SetMaterial(0, Mat); }
	else C.Mesh->UpdateMeshSection(0, V, Nn, UV, Col, Tg);
}

void AFlags::Tick(float Dt)
{
	Super::Tick(Dt);
	Clock += Dt;
	if (Mat) if (APuddles* P = APuddles::Get()) Mat->SetScalarParameterValue(TEXT("Wet"), P->GetWetness());
	Accum = FMath::Min(Accum + Dt, 0.1f);
	const int32 Steps = FMath::FloorToInt(Accum / SubDt);
	Accum -= Steps * SubDt;
	for (FCloth& C : Cloths)
	{
		// (always simulated; only while nobody has seen it for a while does it rest - and it carries on from where it was)
		const bool bSeen = C.Mesh && C.Mesh->WasRecentlyRendered(2.f);
		if (!bSeen && (C.Pause += Dt) > 5.f) continue;
		if (bSeen) C.Pause = 0.f;
		for (int32 s = 0; s < Steps; ++s) Step(C, SubDt);
		if (Steps > 0) Draw(C, false);
	}
}
