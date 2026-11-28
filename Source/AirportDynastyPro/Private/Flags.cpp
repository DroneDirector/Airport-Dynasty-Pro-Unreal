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
	const int32 FlN = 33;                 // particles along each side (32 cells: the cross's edges fall on cell lines)
	const float FlSizeCm = 150.f;         // a 1.5 m square flag
	const float FlCell = FlSizeCm / (FlN - 1);
	const float FlSubDt = 1.f / 120.f;
	const FColor FlRed(213, 43, 30), FlWhite(255, 255, 255);
	/** The Swiss cross: the cross 20/32 of the flag, its arms 6/32 wide (official proportions). */
	bool FlInCross(int32 i, int32 j)
	{
		const bool bH = j >= 13 && j < 19 && i >= 6 && i < 26, bV = i >= 13 && i < 19 && j >= 6 && j < 26;
		return bH || bV;
	}
	int32 FlId(int32 i, int32 j) { return j * FlN + i; }
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
	C.P.SetNum(FlN * FlN);
	for (int32 j = 0; j < FlN; ++j) for (int32 i = 0; i < FlN; ++i)
		C.P[FlId(i, j)] = C.Top + FVector(6.f + i * FlCell * 0.2f, i * FlCell * 0.05f, -j * FlCell - i * FlCell * 0.95f);
	C.Prev = C.P;
	C.Mesh = NewObject<UProceduralMeshComponent>(this);
	C.Mesh->SetupAttachment(GetRootComponent()); C.Mesh->SetCastShadow(true); C.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C.Mesh->RegisterComponent(); AddInstanceComponent(C.Mesh);
	Cloths.Add(C);
	Draw(Cloths.Last(), true);
}

void AFlags::Step(FCloth& C, float H)
{
	// the wind: the windsocks' (the live weather's), blowing TOWARDS where it goes; gusts and eddies on top
	const float From = FMath::DegreesToRadians(AWindSocks::WindFrom), Ms = AWindSocks::WindKt * 0.5144f;
	// (a bearing b in the world: plan x points along the runway's true heading; world = (plan x, -plan y))
	const float Ang = From + PI - FMath::DegreesToRadians(RunwayHdg);
	const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
	const FVector Wind = Dir * Ms * 100.f;
	const FVector G(0, 0, -980.f);
	// the air pushes on the cloth's FACE (pressure along its normal, quadratic in the speed across it - like real cloth,
	// which is what makes a flag ripple and snap: a face turned edge-on feels nothing, one turned across it a lot);
	// along the face only a light skin friction. In calm air only gravity and the still air's drag: it hangs limp.
	TArray<FVector>& Nrm = C.Nrm; Nrm.SetNumUninitialized(C.P.Num());
	for (int32 j = 0; j < FlN; ++j) for (int32 i = 0; i < FlN; ++i)
	{
		const FVector Dx = C.P[FlId(FMath::Min(i + 1, FlN - 1), j)] - C.P[FlId(FMath::Max(i - 1, 0), j)];
		const FVector Dy = C.P[FlId(i, FMath::Min(j + 1, FlN - 1))] - C.P[FlId(i, FMath::Max(j - 1, 0))];
		Nrm[FlId(i, j)] = FVector::CrossProduct(Dy, Dx).GetSafeNormal();
	}
	const float Gust = Ms > 0.2f ? 1.f : Ms / 0.2f;   // (eddies only where there is wind)
	for (int32 j = 0; j < FlN; ++j) for (int32 i = 1; i < FlN; ++i)   // (i = 0: the hoist edge, on the rope)
	{
		const int32 k = FlId(i, j);
		const FVector V = (C.P[k] - C.Prev[k]);
		// eddies travel down the flag with the wind (coherent across it, not per particle noise)
		const float Tw = Clock * (0.9f + Ms * 0.35f) - i * 0.09f;
		const FVector Turb(FMath::PerlinNoise1D(Tw + j * 0.02f), FMath::PerlinNoise1D(Tw * 1.3f + j * 0.03f + 7.f), FMath::PerlinNoise1D(Tw * 1.7f + 3.f));
		const FVector Air = Wind * (1.f + 0.3f * Turb.X) + Turb * Ms * 35.f * Gust;
		const FVector Rel = Air - V / H;
		const FVector& N = Nrm[k];
		const float Rn = FVector::DotProduct(Rel, N);
		// 0.5 rho Cd / (cloth 0.15 kg/m2) = 4 /m: in cm units 0.04 |Rn| Rn; held below what the step can take
		const float Kn = FMath::Min(0.04f * FMath::Abs(Rn) + 1.5f, 0.45f / H);
		const FVector Acc = G + N * (Rn * Kn) + (Rel - N * Rn) * 0.6f;
		const FVector Nw = C.P[k] + V * 0.998f + Acc * H * H;
		C.Prev[k] = C.P[k]; C.P[k] = Nw;
	}
	// the hoist: pinned to the rope beside the pole
	for (int32 j = 0; j < FlN; ++j) { const int32 k = FlId(0, j); C.P[k] = C.Top + FVector(5.f, 0, -j * FlCell); C.Prev[k] = C.P[k]; }
	// the springs: stretch (neighbours), shear (diagonals), bend (two apart) - a few passes
	auto Hold = [&](int32 a, int32 b, float Rest, float K)
	{
		FVector D = C.P[b] - C.P[a]; const float L = D.Size(); if (L < 1e-3f) return;
		D *= (L - Rest) / L * 0.5f * K;
		const bool PinA = a % FlN == 0, PinB = b % FlN == 0;
		if (!PinA) C.P[a] += PinB ? D * 2.f : D;
		if (!PinB) C.P[b] -= PinA ? D * 2.f : D;
	};
	for (int32 It = 0; It < 6; ++It)
		for (int32 j = 0; j < FlN; ++j) for (int32 i = 0; i < FlN; ++i)
		{
			if (i + 1 < FlN) Hold(FlId(i, j), FlId(i + 1, j), FlCell, 1.f);
			if (j + 1 < FlN) Hold(FlId(i, j), FlId(i, j + 1), FlCell, 1.f);
			if (It % 2 == 0)
			{
				if (i + 1 < FlN && j + 1 < FlN) { Hold(FlId(i, j), FlId(i + 1, j + 1), FlCell * 1.4142f, 0.6f); Hold(FlId(i + 1, j), FlId(i, j + 1), FlCell * 1.4142f, 0.6f); }
				if (i + 2 < FlN) Hold(FlId(i, j), FlId(i + 2, j), FlCell * 2.f, 0.15f);
				if (j + 2 < FlN) Hold(FlId(i, j), FlId(i, j + 2), FlCell * 2.f, 0.15f);
			}
		}
	// never through the pole
	for (FVector& P : C.P) { FVector2D D(P.X - C.Top.X, P.Y - C.Top.Y); if (D.Size() < 6.f) { D = D.GetSafeNormal() * 6.f; P.X = C.Top.X + D.X; P.Y = C.Top.Y + D.Y; } }
}

void AFlags::Draw(FCloth& C, bool bFirst)
{
	// each cell its own four corners (a crisp edge between the red and the white), normals from the particles
	TArray<FVector> V; TArray<int32> T; TArray<FVector> Nn; TArray<FVector2D> UV; TArray<FColor> Col; TArray<FProcMeshTangent> Tg;
	V.Reserve((FlN - 1) * (FlN - 1) * 4);
	auto Normal = [&](int32 i, int32 j)
	{
		const FVector Dx = C.P[FlId(FMath::Min(i + 1, FlN - 1), j)] - C.P[FlId(FMath::Max(i - 1, 0), j)];
		const FVector Dy = C.P[FlId(i, FMath::Min(j + 1, FlN - 1))] - C.P[FlId(i, FMath::Max(j - 1, 0))];
		return FVector::CrossProduct(Dy, Dx).GetSafeNormal();
	};
	for (int32 j = 0; j < FlN - 1; ++j) for (int32 i = 0; i < FlN - 1; ++i)
	{
		const int32 B = V.Num();
		const FColor K = FlInCross(i, j) ? FlWhite : FlRed;
		for (const FIntPoint Q : {FIntPoint(i, j), FIntPoint(i + 1, j), FIntPoint(i + 1, j + 1), FIntPoint(i, j + 1)})
		{ V.Add(C.P[FlId(Q.X, Q.Y)]); Nn.Add(Normal(Q.X, Q.Y)); UV.Add(FVector2D(float(Q.X) / (FlN - 1), float(Q.Y) / (FlN - 1))); Col.Add(K); }
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
	const int32 Steps = FMath::FloorToInt(Accum / FlSubDt);
	Accum -= Steps * FlSubDt;
	for (FCloth& C : Cloths)
	{
		// always simulated (it never freezes: the old rest-while-unseen left it stuck where it stopped); only its mesh
		// is redrawn less often while nobody sees it (every half second, so the bounds stay right and it is seen again)
		for (int32 s = 0; s < Steps; ++s) Step(C, FlSubDt);
		const bool bSeen = C.Mesh && C.Mesh->WasRecentlyRendered(1.f);
		C.Pause += Dt;
		if (Steps > 0 && (bSeen || C.Pause > 0.5f)) { Draw(C, false); C.Pause = 0.f; }
	}
}
