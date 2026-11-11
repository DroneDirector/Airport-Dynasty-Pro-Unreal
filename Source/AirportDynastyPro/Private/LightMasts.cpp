// The apron light masts (LightMasts.h).
#include "LightMasts.h"
#include "Engine/PostProcessVolume.h"
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
#include "Airfield.h"
#include "Hangar.h"

namespace
{
	const float MastHeadY = 29.4f;      // the floodlights' height, metres above the apron
	const float LampLumens = 3900.f;    // each floodlight: bright enough for the apron, never glaring
	const float FaceGlow = 6.f;         // the LED faces' glow at full night
}

ALightMasts::ALightMasts()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

float ALightMasts::BeamDistance = 500.f;
TWeakObjectPtr<ALightMasts> ALightMasts::Current;

void ALightMasts::Refresh()
{
	ALightMasts* M = Current.Get();
	if (!M || !M->bOn) return;
	for (TActorIterator<AExponentialHeightFog> It(M->GetWorld()); It; ++It)
		if (UExponentialHeightFogComponent* F = It->GetComponent()) F->SetVolumetricFogDistance(BeamDistance * 100.f);
}

const TArray<FVector2D>& ALightMasts::Spots()
{
	// at the terminal: each 19 m east of its stand's centre line (the jetways all turn west), 8 m out from the front
	static TArray<FVector2D> S;
	if (S.Num()) return S;
	S = {{-43.0f, 90.f}, {-2.14f, 90.f}, {38.71f, 90.f}, {79.57f, 90.f},
		{120.43f, 90.f}, {161.29f, 90.f}, {202.14f, 90.f}, {243.0f, 90.f}};
	// two in front of each hangar (owner, 2026-11-11): at its front corners, 5 m out past the door pockets and 6 m out
	// on the apron, clear of the door, its lead-in line and the wings of an aircraft going in
	FAirfieldData D;
	if (D.Load())
		for (const FAirfieldData::FHangarSite& H : D.Hangars)
		{
			const float Off = FHangarSpec::Get(H.Size).PocketX() + 5.f;
			S.Add({H.X - Off, H.Y - 6.f}); S.Add({H.X + Off, H.Y - 6.f});
		}
	return S;
}

void ALightMasts::Build()
{
	Current = this;
	GConfig->GetFloat(TEXT("ADP.Graphics"), TEXT("BeamDistance"), BeamDistance, GGameUserSettingsIni);
	BeamDistance = FMath::Clamp(BeamDistance, 60.f, 500.f);
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
	// outside at night the camera brightened the dark so much that the lit terminal washed out white (owner, 2026-11-11):
	// two stops less outdoors after dark (the terminal at about a quarter), the masts a little stronger to keep the
	// apron's ground readable (their own look about 30% of before); indoors keeps its own exposure
	const float Bias = -1.7f * (1.f - FMath::SmoothStep(-10.f, 3.f, El));
	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
		if (It->bUnbound && It->Priority < 1.f && FMath::Abs(It->Settings.AutoExposureBias - Bias) > 0.02f)
		{ It->Settings.bOverride_AutoExposureBias = true; It->Settings.AutoExposureBias = Bias; }
	// the haze the beams show in: volumetric fog while the masts are lit
	if (bChanged)
		for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
			if (UExponentialHeightFogComponent* F = It->GetComponent())
			{
				F->SetVolumetricFog(bOn);
				// far enough to show every mast's beams along the whole row of gates (about 290 m end to end)
				F->SetVolumetricFogDistance(BeamDistance * 100.f);
			}
}
