#include "AirfieldLights.h"
#include "Airfield.h"
#include "TerminalPlan.h"
#include "SunClock.h"
#include "ADPLog.h"
#include "ADPKeep.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AAirfieldLights::AAirfieldLights()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

namespace
{
	float SegDist(const FVector2f& P, const FVector2f& A, const FVector2f& B)
	{
		const FVector2f D = B - A; const float L2 = D.SizeSquared();
		const float T = L2 > 0.f ? FMath::Clamp(FVector2f::DotProduct(P - A, D) / L2, 0.f, 1.f) : 0.f;
		return FVector2f::Distance(P, A + D * T);
	}
}

void AAirfieldLights::Place(const FAirfieldData& F)
{
	UStaticMesh* EdgeM = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/AirfieldLights/SM_TwyEdge.SM_TwyEdge")));
	UStaticMesh* CentreM = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/AirfieldLights/SM_TwyCentre.SM_TwyCentre")));
	if (!EdgeM || !CentreM) { ADP_WARN(TEXT("Taxiway lights: the fittings are missing (Tools/import_airfield_lights.py)")); return; }
	auto MakeList = [this](UStaticMesh* M, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this, Name);
		K->SetupAttachment(GetRootComponent()); K->SetStaticMesh(M);
		K->SetCollisionEnabled(ECollisionEnabled::NoCollision); K->SetCastShadow(false);
		K->SetNumCustomDataFloats(1);
		K->RegisterComponent();
		const int32 Slot = K->GetMaterialIndex(TEXT("JwBcnLens"));
		if (Slot != INDEX_NONE)
			if (UMaterialInterface* L = K->GetMaterial(Slot))
			{
				UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(L, this); K->SetMaterial(Slot, D);
				float G = 10.f; L->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Glow")), G);
				Lenses.Add(D); LensGlow.Add(G);
			}
		return K;
	};
	Edge = MakeList(EdgeM, TEXT("TwyEdge")); Centre = MakeList(CentreM, TEXT("TwyCentre"));

	// what is paved besides a taxiway's own edge: the runway with its shoulders, the aprons, the other taxiways
	auto OnRunway = [&F](const FVector2f& P, float Margin)
	{
		return P.X > FMath::Min(F.RwyX0, F.RwyX1) - 5.f && P.X < FMath::Max(F.RwyX0, F.RwyX1) + 5.f && FMath::Abs(P.Y - F.RwyY) < F.RwyW * 0.5f + 7.5f + Margin;
	};
	auto OnApron = [&F](const FVector2f& P, float Margin)
	{
		for (const FAirfieldData::FRect& A : F.Aprons) if (P.X > A.X0 - Margin && P.X < A.X1 + Margin && P.Y > A.Y0 - Margin && P.Y < A.Y1 + Margin) return true;
		return false;
	};
	auto InsideTaxiway = [&F](const FVector2f& P)
	{
		for (const FAirfieldData::FTwy& T : F.Taxiways)
			for (int32 i = 0; i + 1 < T.Pts.Num(); ++i)
				if (SegDist(P, T.Pts[i], T.Pts[i + 1]) < T.W * 0.5f - 1.f) return true;
		return false;
	};
	TArray<FVector2f> DoneE, DoneC;
	auto Taken = [](const TArray<FVector2f>& Done, const FVector2f& P, float R) { for (const FVector2f& Q : Done) if (FVector2f::DistSquared(P, Q) < R * R) return true; return false; };
	FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(TwyLight), true);
	auto Put = [&](UInstancedStaticMeshComponent* K, const FVector2f& P, const FVector2f& Dir)
	{
		FVector At = FTerminalPlan::ToWorld(P.X, 0.f, P.Y);
		if (GetWorld()->LineTraceSingleByChannel(H, At + FVector(0, 0, 3000.f), At - FVector(0, 0, 3000.f), ECC_Visibility, Q)) At.Z = H.ImpactPoint.Z;
		const FVector W = FTerminalPlan::ToWorld(Dir.X, 0.f, Dir.Y);
		const int32 Ix = K->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(W.Y, W.X)) - 90.f, 0.f), At), true);
		K->SetCustomDataValue(Ix, 0, 1.f, false);
	};
	int32 NE = 0, NC = 0;
	for (const FAirfieldData::FTwy& T : F.Taxiways)
	{
		const bool bCurve = T.Kind == TEXT("curve"), bLane = T.Kind == TEXT("taxilane");
		for (int32 i = 0; i + 1 < T.Pts.Num(); ++i)
		{
			const FVector2f A = T.Pts[i], B = T.Pts[i + 1]; const float L = FVector2f::Distance(A, B);
			if (L < 0.1f) continue;
			const FVector2f Dir = (B - A) / L, N(-Dir.Y, Dir.X);
			// the green centreline (on taxilanes too), never on the runway
			{
				const float Gap = bCurve ? 7.5f : 30.f; const int32 n = FMath::Max(1, FMath::CeilToInt(L / Gap));
				for (int32 k = 0; k <= n; ++k)
				{
					const FVector2f P = A + Dir * (L * k / n);
					if (OnRunway(P, 0.f) || Taken(DoneC, P, bCurve ? 4.f : 10.f)) continue;
					Put(Centre, P, Dir); DoneC.Add(P); ++NC;
				}
			}
			// the blue edges (taxilanes are edged by their aprons: none), both sides, skipping where other paving joins
			if (bLane) continue;
			const float Gap = bCurve ? 15.f : 50.f; const int32 n = FMath::Max(1, FMath::CeilToInt(L / Gap));
			for (const float S : {-1.f, 1.f})
				for (int32 k = 0; k <= n; ++k)
				{
					const FVector2f P = A + Dir * (L * k / n) + N * (S * (T.W * 0.5f + 0.75f));   // (75 cm out from the paving's edge)
					if (OnRunway(P, 1.f) || OnApron(P, 1.f) || InsideTaxiway(P) || Taken(DoneE, P, 8.f)) continue;
					Put(Edge, P, Dir); DoneE.Add(P); ++NE;
				}
		}
	}
	Edge->MarkRenderStateDirty(); Centre->MarkRenderStateDirty();
	ADP_LOG(TEXT("Taxiway lights: %d blue edge, %d green centreline"), NE, NC);
}

void AAirfieldLights::Tick(float Dt)
{
	Super::Tick(Dt);
	// on at dusk like the masts (and all day in fog or storms); by day they burn too, far weaker than the sun
	static TWeakObjectPtr<ADirectionalLight> Sun;
	if (!Sun.IsValid()) Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	const float El = Sun.IsValid() ? -Sun->GetActorRotation().Pitch : 30.f;
	const bool bLowVis = ASunClock::Theme == 4 || ASunClock::Theme == 6;
	const float Night = bLowVis ? 1.f : 1.f - FMath::SmoothStep(-8.f, 4.f, El);
	const float Want = 0.12f + 0.88f * Night;
	if (FMath::Abs(Want - Level) < 0.01f) return;
	Level = Want;
	for (int32 i = 0; i < Lenses.Num(); ++i) if (Lenses[i]) Lenses[i]->SetScalarParameterValue(TEXT("Glow"), LensGlow[i] * Level);
}
