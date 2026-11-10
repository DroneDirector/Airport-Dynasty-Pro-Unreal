// The apron light masts (LightMasts.h).
#include "LightMasts.h"
#include "TerminalPlan.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "EngineUtils.h"
#include "SunClock.h"

namespace
{
	const float MastHeadY = 29.4f;      // the floodlights' height, metres above the apron
	const float LampLumens = 2600.f;    // each floodlight: bright enough for the apron, never glaring
	const float FaceGlow = 6.f;         // the LED faces' glow at full night
	const float BeamDistance = 50000.f; // the haze the beams show in reaches 500 m from you (Unreal's default is 60 m)
}

ALightMasts::ALightMasts()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

const TArray<FVector2D>& ALightMasts::Spots()
{
	// each 19 m east of its stand's centre line (the jetways all turn west), 8 m out from the terminal front
	static const TArray<FVector2D> S = {
		{-43.0f, 90.f}, {-2.14f, 90.f}, {38.71f, 90.f}, {79.57f, 90.f},
		{120.43f, 90.f}, {161.29f, 90.f}, {202.14f, 90.f}, {243.0f, 90.f}};
	return S;
}

void ALightMasts::Build()
{
	UStaticMesh* Main = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/LightMast/lightmast/StaticMeshes/LightMast.LightMast"));
	UStaticMesh* Fine = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/LightMast/lightmast/StaticMeshes/LightMastThin.LightMastThin"));
	if (!Main) { UE_LOG(LogTemp, Warning, TEXT("Light masts: mesh missing")); return; }
	auto Make = [this](UStaticMesh* M, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
		C->SetupAttachment(Root);
		C->SetStaticMesh(M);
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetMobility(EComponentMobility::Movable);
		C->RegisterComponent();
		AddInstanceComponent(C);
		return C;
	};
	Masts = Make(Main, TEXT("LightMasts"));
	if (Fine) { Thin = Make(Fine, TEXT("LightMastsThin")); Thin->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	int32 MastNo = 0;
	for (const FVector2D& P : Spots())
	{
		const FTransform T(FRotator::ZeroRotator, FTerminalPlan::ToWorld(P.X, 0.f, P.Y));
		Masts->AddInstance(T, true);
		if (Thin) Thin->AddInstance(T, true);
		// a light from each of the 8 floodlights, pointed as the lamp is (out and down): their beams fan out in the
		// evening haze and meet in one wide pool round the foot. No shadows of their own (cheap), visible in the fog.
		for (int32 k = 0; k < 8; ++k)
		{
			const float A = FMath::DegreesToRadians(k * 45.f + 22.5f), Tilt = (k % 2) ? 48.f : 40.f, Zo = (k % 2) ? 0.06f : -0.06f;
			const FVector At = T.GetLocation() + FVector(FMath::Cos(A) * 283.f, FMath::Sin(A) * 283.f, (MastHeadY + Zo) * 100.f);
			USpotLightComponent* L = NewObject<USpotLightComponent>(this);
			L->SetupAttachment(Root);
			L->SetMobility(EComponentMobility::Movable);
			L->SetWorldLocationAndRotation(At, FRotator(-Tilt, FMath::RadiansToDegrees(A), 0.f));
			L->SetIntensityUnits(ELightUnits::Lumens);
			L->SetIntensity(0.f);
			L->SetUseTemperature(true); L->SetTemperature(4300.f);
			L->SetOuterConeAngle(34.f); L->SetInnerConeAngle(16.f);
			L->SetAttenuationRadius(7000.f);
			L->SetSourceRadius(35.f); L->SetSourceLength(50.f);
			L->SetCastShadows(false);
			L->SetVolumetricScatteringIntensity(9.f);   // the beams stay visible in the haze while the ground stays soft
			L->MaxDrawDistance = 80000.f; L->MaxDistanceFadeRange = 12000.f;
			L->SetVisibility(false);
			L->RegisterComponent();
			AddInstanceComponent(L);
			Lights.Add(L);
			LightMast.Add(MastNo);
		}
		++MastNo;
	}
	// the LED faces: their own material copy, so the glow can follow the night
	const int32 Slot = Masts->GetMaterialIndex(TEXT("MI_LM_LEDFace"));
	if (Slot != INDEX_NONE) if (UMaterialInterface* F = Masts->GetMaterial(Slot)) { Faces = UMaterialInstanceDynamic::Create(F, this); Masts->SetMaterial(Slot, Faces); }
	UE_LOG(LogTemp, Log, TEXT("Light masts: %d placed (LED slot %d)"), Spots().Num(), Slot);
}

void ALightMasts::Tick(float Dt)
{
	Super::Tick(Dt);
	// switched as real aprons are, by a light sensor: on at sunset (the sun at the horizon), off just after sunrise
	// (3 degrees up; a sensor turns off at more light than it turns on at), and on all day in fog and storms (low
	// visibility). Masts come on one after another, a second or so apart, as their circuits close.
	static TWeakObjectPtr<ADirectionalLight> Sun;
	if (!Sun.IsValid()) Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	const float El = Sun.IsValid() ? -Sun->GetActorRotation().Pitch : 30.f;
	const bool bLowVis = ASunClock::Theme == 4 || ASunClock::Theme == 6;
	const bool bWant = bLowVis || (bOn ? El < 3.f : El < 0.f);
	if (bWant != bOn) { bOn = bWant; Since = 0.f; }
	Since += Dt;
	// the night's depth (dusk to full dark): the haze and glow grow as the sky darkens
	const float Dark = bLowVis ? 0.6f : 1.f - FMath::SmoothStep(-10.f, 3.f, El);
	bool bChanged = false;
	for (int32 i = 0; i < Lights.Num(); ++i)
	{
		USpotLightComponent* L = Lights[i];
		if (!L) continue;
		const bool bLit = bOn && Since >= LightMast[i] * 1.3f;
		const float Want = bLit ? LampLumens * FMath::Max(Dark, 0.35f) : 0.f;
		if (L->IsVisible() != bLit) { L->SetVisibility(bLit); bChanged = true; }
		if (FMath::Abs(L->Intensity - Want) > 50.f) L->SetIntensity(Want);
	}
	if (Faces) Faces->SetScalarParameterValue(TEXT("Glow"), bOn ? FaceGlow * FMath::Max(Dark, 0.35f) : 0.f);
	// the haze the beams show in: volumetric fog while the masts are lit
	if (bChanged)
		for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
			if (UExponentialHeightFogComponent* F = It->GetComponent())
			{
				F->SetVolumetricFog(bOn);
				// far enough to show every mast's beams along the whole row of gates (about 290 m end to end)
				F->SetVolumetricFogDistance(BeamDistance);
			}
}
