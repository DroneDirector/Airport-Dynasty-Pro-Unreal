// The grass round the airport (see GrassField.h).
#include "GrassField.h"
#include "LandCover.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "TerminalPlan.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "CollisionQueryParams.h"

float AGrassField::NearDistance = 45.f;
bool AGrassField::bOn = true;

namespace
{
	constexpr float MidReach = 30000.f;     // the simpler swaying clumps: to 300 m
	constexpr float FarStep = 250.f;        // the far tufts: one in every 2.5 m square (wide, so the field reads as grass)
	constexpr int32 FarRowsPerTick = 2;     // 4 m rows of the whole airfield laid per frame

	/** Is what the look down found grass? The airfield's grass slab (its "grass" list) or the model's lawns. */
	bool IsGrass(const FHitResult& H)
	{
		const UPrimitiveComponent* C = H.GetComponent();
		if (!C || H.ImpactNormal.Z < 0.85f) return false;
		if (C->GetFName() == TEXT("grass")) return true;
		if (const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(C))
			if (const UStaticMesh* M = S->GetStaticMesh()) if (M->GetName() == TEXT("terminal_mesh_2611")) return true;
		// the real ground (the globe's terrain): grass where the land cover says the land is grassy
		if (C->GetOwner() && C->GetOwner()->IsA<ACesium3DTileset>())
			if (ACesiumGeoreference* G = ACesiumGeoreference::GetDefaultGeoreference(C->GetWorld()))
			{
				const FVector LL = G->TransformUnrealPositionToLongitudeLatitudeHeight(H.ImpactPoint);
				return ADPLandCover::Grows(ADPLandCover::ClassAt(LL.X, LL.Y));
			}
		return false;
	}
}

AGrassField::AGrassField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AGrassField::Start(float X0, float Y0, float X1, float Y1)
{
	Near = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Materials/Grass/SM_GrassNear.SM_GrassNear"));
	Mid = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Materials/Grass/SM_GrassFar.SM_GrassFar"));
	Tuft = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Materials/Grass/SM_GrassTuft.SM_GrassTuft"));
	const FVector A = FTerminalPlan::ToWorld(X0, 0, Y0), B = FTerminalPlan::ToWorld(X1, 0, Y1);
	Field = FBox2D(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)));
	FarY = Field.Min.Y;
	{ bool B0 = true; GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("Grass"), B0, GGameUserSettingsIni); bOn = B0; }
	GConfig->GetFloat(TEXT("ADP.Graphics"), TEXT("GrassNear"), NearDistance, GGameUserSettingsIni);
	UE_LOG(LogTemp, Log, TEXT("Grass: %s, field %.0f x %.0f m"), Near && Mid && Tuft ? TEXT("ready") : TEXT("meshes missing"), Field.GetSize().X / 100, Field.GetSize().Y / 100);
}

bool AGrassField::GroundAt(float X, float Y, float& Z) const
{
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(GrassLook), true, this);   // complex: the globe's terrain has only its own shape to hit
	if (!GetWorld()->LineTraceSingleByChannel(H, FVector(X, Y, 30000.f), FVector(X, Y, -5000.f), ECC_Visibility, Q) || !IsGrass(H)) return false;
	Z = H.ImpactPoint.Z;
	return true;
}

const AGrassField::FPatch& AGrassField::Patch(const FIntPoint& C)
{
	if (const FPatch* P = Patches.Find(C)) return *P;
	FPatch& P = Patches.Add(C);
	const int32 N = int32(Cell / 100.f);
	P.H.SetNumUninitialized(N * N);
	for (int32 j = 0; j < N; ++j)
		for (int32 i = 0; i < N; ++i)
		{
			float Z;
			P.H[j * N + i] = GroundAt(C.X * Cell + (i + 0.5f) * 100.f, C.Y * Cell + (j + 0.5f) * 100.f, Z) ? Z : TNumericLimits<float>::Lowest();
		}
	return P;
}

UHierarchicalInstancedStaticMeshComponent* AGrassField::NewLayer(UStaticMesh* Mesh, float WpoUntil, float CullFrom, float CullTo)
{
	UHierarchicalInstancedStaticMeshComponent* K = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	K->SetupAttachment(Root);
	K->SetStaticMesh(Mesh);
	K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	K->SetCastShadow(false);
	K->bAffectDistanceFieldLighting = false;
	K->SetCanEverAffectNavigation(false);
	K->WorldPositionOffsetDisableDistance = int32(WpoUntil / 100.f);   // the wind only close by (metres)
	K->SetEvaluateWorldPositionOffset(WpoUntil > 0);
	K->SetCullDistances(int32(CullFrom), int32(CullTo));
	K->SetVisibility(bOn);
	K->RegisterComponent();
	return K;
}

void AGrassField::FillCell(UHierarchicalInstancedStaticMeshComponent* K, const FIntPoint& C, float PerM2, float S0, float S1, int32 Seed)
{
	const FPatch& P = Patch(C);
	const int32 N = int32(Cell / 100.f);
	FRandomStream R(Seed ^ (C.X * 73856093) ^ (C.Y * 19349663));
	TArray<FTransform> T;
	for (int32 j = 0; j < N; ++j)
		for (int32 i = 0; i < N; ++i)
		{
			const float Z = P.H[j * N + i];
			if (Z == TNumericLimits<float>::Lowest()) continue;
			// a whole number of clumps per square metre on average, scattered inside it
			int32 Count = int32(PerM2); if (R.FRand() < PerM2 - Count) ++Count;
			for (int32 k = 0; k < Count; ++k)
			{
				const float S = R.FRandRange(S0, S1);
				T.Add(FTransform(FRotator(0, R.FRandRange(0, 360), 0), FVector(C.X * Cell + (i + R.FRand()) * 100.f, C.Y * Cell + (j + R.FRand()) * 100.f, Z - 1.f), FVector(S, S, S * R.FRandRange(0.85f, 1.2f))));
			}
		}
	if (T.Num()) K->AddInstances(T, false, true);
}

void AGrassField::ApplyOn()
{
	for (auto& It : NearCells) if (It.Value) It.Value->SetVisibility(bOn);
	for (auto& It : MidCells) if (It.Value) It.Value->SetVisibility(bOn);
	for (auto& K : FarRows) if (K) K->SetVisibility(bOn);
	bLastOn = bOn;
}

void AGrassField::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Near || !Mid || !Tuft || !Field.bIsValid) return;
	if (bOn != bLastOn) ApplyOn();

	// the whole airfield's tufts, a few rows each frame until it's all laid (once)
	if (FarY < Field.Max.Y)
	{
		UHierarchicalInstancedStaticMeshComponent* K = NewLayer(Tuft, 0.f, 0.f, 0.f);
		FRandomStream R{int32(FarY)};
		TArray<FTransform> T;
		for (int32 r = 0; r < FarRowsPerTick && FarY < Field.Max.Y; ++r, FarY += FarStep)
			for (float X = Field.Min.X; X < Field.Max.X; X += FarStep)
			{
				const float PX = X + R.FRand() * FarStep, PY = FarY + R.FRand() * FarStep;
				float Z;
				if (GroundAt(PX, PY, Z)) { const float S = R.FRandRange(2.2f, 3.2f); T.Add(FTransform(FRotator(0, R.FRandRange(0, 360), 0), FVector(PX, PY, Z - 2.f), FVector(S, S, S * 0.8f))); }
			}
		if (T.Num()) { K->AddInstances(T, false, true); FarRows.Add(K); } else K->DestroyComponent();
	}

	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me) return;
	const FVector L = Me->GetActorLocation();
	const FIntPoint At(FMath::FloorToInt(L.X / Cell), FMath::FloorToInt(L.Y / Cell));
	const float NearReach = bOn ? FMath::Clamp(NearDistance, 0.f, 150.f) * 100.f : 0.f;

	// the layers' cells round you: dropped when out of reach, the nearest missing one added (one a frame, no stutter)
	auto Keep = [&](TMap<FIntPoint, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Cells, float Reach)
	{
		for (auto It = Cells.CreateIterator(); It; ++It)
		{
			const FVector2D Mid2((It.Key().X + 0.5f) * Cell, (It.Key().Y + 0.5f) * Cell);
			if (FVector2D::Distance(Mid2, FVector2D(L)) > Reach + Cell) { if (It.Value()) It.Value()->DestroyComponent(); It.RemoveCurrent(); }
		}
	};
	auto Missing = [&](const TMap<FIntPoint, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Cells, float Reach, FIntPoint& Out)
	{
		const int32 R = FMath::CeilToInt(Reach / Cell);
		float Best = TNumericLimits<float>::Max();
		for (int32 y = -R; y <= R; ++y)
			for (int32 x = -R; x <= R; ++x)
			{
				const FIntPoint C(At.X + x, At.Y + y);
				const FVector2D Mid2((C.X + 0.5f) * Cell, (C.Y + 0.5f) * Cell);
				const float D = FVector2D::Distance(Mid2, FVector2D(L));
				if (D > Reach + Cell * 0.7f || Cells.Contains(C)) continue;
				if (D < Best) { Best = D; Out = C; }
			}
		return Best < TNumericLimits<float>::Max();
	};
	Keep(NearCells, NearReach);
	Keep(MidCells, bOn ? MidReach : 0.f);
	FIntPoint C;
	if (NearReach > 0 && Missing(NearCells, NearReach, C))
	{
		UHierarchicalInstancedStaticMeshComponent* K = NewLayer(Near, 5000.f, NearReach * 0.8f, NearReach);
		FillCell(K, C, 14.f, 0.85f, 1.2f, 1);
		NearCells.Add(C, K);
	}
	else if (bOn && Missing(MidCells, MidReach, C))
	{
		UHierarchicalInstancedStaticMeshComponent* K = NewLayer(Mid, 12000.f, MidReach * 0.85f, MidReach);
		FillCell(K, C, 3.f, 1.3f, 1.8f, 2);
		MidCells.Add(C, K);
	}
}
