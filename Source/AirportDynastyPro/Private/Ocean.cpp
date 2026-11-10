// The sea round the airport (see Ocean.h).
#include "Ocean.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "CesiumGeoreference.h"
#include "ProceduralMeshComponent.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

AOcean::AOcean()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	Water = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Water"));
	SetRootComponent(Water);
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetCastShadow(false);
}

bool AOcean::Build(ACesiumGeoreference* G, double SeaHeight)
{
	TArray<uint8> B;
	bool bRead = false;
	for (const FString& F : {FPaths::ProjectContentDir() / TEXT("Data/ocean.bin"), FPaths::ProjectDir() / TEXT("Content/Data/ocean.bin")})
		if (FFileHelper::LoadFileToArray(B, *F)) { bRead = true; break; }
	if (!G || !bRead || B.Num() < 16 || FMemory::Memcmp(B.GetData(), "ADPO", 4) != 0) { UE_LOG(LogTemp, Warning, TEXT("Ocean: no ocean.bin")); return false; }
	uint32 Ver, NV, NT;
	FMemory::Memcpy(&Ver, B.GetData() + 4, 4); FMemory::Memcpy(&NV, B.GetData() + 8, 4); FMemory::Memcpy(&NT, B.GetData() + 12, 4);
	const int64 Need = 16 + int64(NV) * 16 + int64(NV) * 4 + int64(NT) * 12;
	if (B.Num() < Need) return false;
	const double* LL = reinterpret_cast<const double*>(B.GetData() + 16);
	const float* Shore = reinterpret_cast<const float*>(B.GetData() + 16 + NV * 16);
	const uint32* Tri = reinterpret_cast<const uint32*>(B.GetData() + 16 + NV * 20);

	// every corner from longitude and latitude to Unreal's world through the georeference (the Earth's curve included),
	// held relative to the middle so the numbers stay small
	TArray<FVector> P; P.SetNumUninitialized(NV);
	FBox Box(ForceInit);
	for (uint32 i = 0; i < NV; ++i) { P[i] = G->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(LL[i * 2], LL[i * 2 + 1], SeaHeight)); Box += P[i]; }
	const FVector Mid = Box.GetCenter();
	SetActorLocation(Mid);
	TArray<FVector> V; V.SetNumUninitialized(NV);
	TArray<FVector> Nrm; Nrm.Init(FVector::UpVector, NV);
	TArray<FColor> Col; Col.SetNumUninitialized(NV);
	TArray<FVector2D> UV; UV.SetNumUninitialized(NV);
	for (uint32 i = 0; i < NV; ++i)
	{
		V[i] = P[i] - Mid;
		Col[i] = FColor(uint8(FMath::Clamp(Shore[i], 0.f, 1.f) * 255.f), 0, 0, 255);   // red: how close to the shore
		UV[i] = FVector2D(P[i].X / 10000.0, P[i].Y / 10000.0);
	}
	TArray<int32> Idx; Idx.SetNumUninitialized(NT * 3);
	// facing up: each triangle turned so its normal points to the sky (the converter's order depends on the hemisphere)
	for (uint32 t = 0; t < NT; ++t)
	{
		int32 A = Tri[t * 3], Bq = Tri[t * 3 + 1], C = Tri[t * 3 + 2];
		if (A >= int32(NV) || Bq >= int32(NV) || C >= int32(NV)) { A = Bq = C = 0; }
		const FVector Nn = FVector::CrossProduct(V[Bq] - V[A], V[C] - V[A]);
		if (Nn.Z > 0) Swap(Bq, C);   // Unreal's front face is clockwise seen from above
		Idx[t * 3] = A; Idx[t * 3 + 1] = Bq; Idx[t * 3 + 2] = C;
	}
	Water->CreateMeshSection(0, V, Idx, Nrm, UV, Col, TArray<FProcMeshTangent>(), false);
	// held to its place on the globe: when the globe's middle moves (the 3D viewer following a flight), the sea moves with it
	UCesiumGlobeAnchorComponent* Anchor = NewObject<UCesiumGlobeAnchorComponent>(this);
	AddInstanceComponent(Anchor);
	Anchor->RegisterComponent();
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Ocean/M_Ocean.M_Ocean"))) Water->SetMaterial(0, M);
	SeaZ = Mid.Z;
	UE_LOG(LogTemp, Log, TEXT("Ocean: %u corners, %u triangles, at %.1f m, %.0f x %.0f km"), NV, NT, SeaHeight, Box.GetSize().X / 1e5, Box.GetSize().Y / 1e5);
	return true;
}

void AOcean::Tick(float Dt)
{
	Super::Tick(Dt);
	// the camera's height over the sea: the detailed water up to 600 m, the terrain's water mask above (a little
	// overlap either way so it never flickers at the line)
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam || !Water) return;
	const float Up = (Cam->GetCameraLocation().Z - SeaZ) / 100.f;
	const bool bShow = Water->IsVisible() ? Up < 650.f : Up < 550.f;
	if (bShow != Water->IsVisible()) Water->SetVisibility(bShow);
}
