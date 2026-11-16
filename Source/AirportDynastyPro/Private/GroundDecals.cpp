#include "GroundDecals.h"
#include "Airfield.h"
#include "TerminalPlan.h"
#include "ADPLog.h"
#include "ADPKeep.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

AGroundDecals::AGroundDecals() { SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root"))); }

void AGroundDecals::Add(UMaterialInterface* M, float X, float Y, float SizeAlong, float SizeAcross, float Yaw, float Seed, float Strength)
{
	if (!M) return;
	// the ground under it (a look straight down), the decal's box reaching 1.5 m up and down from it
	FVector At = FTerminalPlan::ToWorld(X, 0.f, Y);
	FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(GroundDecal), true);
	if (GetWorld()->LineTraceSingleByChannel(H, At + FVector(0, 0, 3000.f), At - FVector(0, 0, 3000.f), ECC_Visibility, Q)) At.Z = H.ImpactPoint.Z;
	UDecalComponent* D = NewObject<UDecalComponent>(this);
	D->SetupAttachment(GetRootComponent());
	UMaterialInstanceDynamic* MI = UMaterialInstanceDynamic::Create(M, this);
	MI->SetScalarParameterValue(TEXT("Seed"), Seed); MI->SetScalarParameterValue(TEXT("Strength"), Strength);
	D->SetDecalMaterial(MI);
	D->DecalSize = FVector(150.f, SizeAcross * 50.f, SizeAlong * 50.f);   // (depth, half across, half along - in cm)
	D->SetWorldLocationAndRotation(At, FRotator(-90.f, Yaw, 0.f));
	D->SetFadeScreenSize(0.002f);
	D->RegisterComponent();
	++Count;
}

void AGroundDecals::Place(const FAirfieldData& F)
{
	static UMaterialInterface* Stain = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ground/Decals/M_DecalStain.M_DecalStain")));
	static UMaterialInterface* Tyre = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ground/Decals/M_DecalTyre.M_DecalTyre")));
	static UMaterialInterface* Patch = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ground/Decals/M_DecalPatch.M_DecalPatch")));
	if (!Stain || !Tyre || !Patch) { ADP_WARN(TEXT("Ground decals: the materials are missing (Tools/make_ground_decals.py)")); return; }
	FRandomStream R(20261116);
	// oil and fuel stains at every passenger stand: under the two engines (13 m behind the nose line, 5.75 m out) and the
	// nose gear; a world-plan yaw along the stand
	for (const FAirfieldData::FStand& St : F.Stands)
	{
		if (St.bCargo) continue;
		const float B = -St.Dir;
		for (const float Side : {-5.75f, 5.75f})
			Add(Stain, St.X + Side + R.FRandRange(-0.4f, 0.4f), St.Nose + B * (13.f + R.FRandRange(-0.6f, 0.6f)), R.FRandRange(1.6f, 2.8f), R.FRandRange(1.4f, 2.4f), R.FRandRange(0.f, 360.f), R.FRand() * 10.f, R.FRandRange(0.5f, 0.85f));
		Add(Stain, St.X + R.FRandRange(-0.3f, 0.3f), St.Nose + B * 5.6f, 1.1f, 0.9f, R.FRandRange(0.f, 360.f), R.FRand() * 10.f, 0.55f);
	}
	// rubber in both touchdown zones (150 - 650 m in from each end), along the main wheels' two tracks, in pieces
	const float Dx = F.RwyX1 - F.RwyX0;
	for (const int32 End : {0, 1})
		for (int32 k = 0; k < 10; ++k)
		{
			const float Along = 150.f + k * 50.f + R.FRandRange(-10.f, 10.f);
			const float X = End == 0 ? F.RwyX0 + Along : F.RwyX1 - Along;
			for (const float Track : {-3.8f, 3.8f})
				Add(Tyre, X, F.RwyY + Track + R.FRandRange(-1.2f, 1.2f), R.FRandRange(35.f, 55.f), R.FRandRange(2.2f, 3.2f), Dx >= 0 ? 0.f : 180.f, R.FRand() * 10.f, R.FRandRange(0.35f, 0.75f) * (1.f - k * 0.06f));
		}
	// repair patches: a few on each apron
	for (const FAirfieldData::FRect& A : F.Aprons)
		for (int32 k = 0; k < 2; ++k)
			Add(Patch, R.FRandRange(A.X0 + 10.f, A.X1 - 10.f), R.FRandRange(A.Y0 + 10.f, A.Y1 - 10.f), R.FRandRange(3.f, 7.f), R.FRandRange(3.f, 6.f), R.RandRange(0, 1) * 90.f, R.FRand() * 10.f, R.FRandRange(0.5f, 0.9f));
	ADP_LOG(TEXT("Ground decals: %d placed (stand stains, touchdown rubber, apron patches)"), Count);
}
