// The roads' street lights (StreetLights.h).
#include "StreetLights.h"
#include "ADPLog.h"
#include "SunClock.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const float PostLumens = 6500.f;   // a modern LED road light (about 50 W)
	const float StreetFaceGlow = 5.f;        // the LED face's glow at full night
}

AStreetLights::AStreetLights()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

bool AStreetLights::Ready()
{
	if (Posts) return true;
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/StreetLight/streetlight/StaticMeshes/streetlight.streetlight"));
	if (!M) { ADP_WARN(TEXT("Street lights: the model is missing (Tools/set_street_light.py)")); return false; }
	Posts = NewObject<UInstancedStaticMeshComponent>(this, TEXT("StreetLightPosts"));
	Posts->SetupAttachment(Root);
	Posts->SetStaticMesh(M);
	Posts->SetCollisionProfileName(TEXT("BlockAll"));
	Posts->SetMobility(EComponentMobility::Movable);
	Posts->RegisterComponent();
	AddInstanceComponent(Posts);
	// which way the model's arm reaches, and how far, and the head's height: read off its own shape (whatever turn the
	// import gave it) - the arm is the only part far off the pole
	const FBox B = M->GetBoundingBox();
	const FVector2D Far = FMath::Abs(B.Max.X) + FMath::Abs(B.Max.Y) > FMath::Abs(B.Min.X) + FMath::Abs(B.Min.Y) ? FVector2D(B.Max.X, B.Max.Y) : FVector2D(B.Min.X, B.Min.Y);
	const FVector2D Arm = FMath::Abs(Far.X) > FMath::Abs(Far.Y) ? FVector2D(FMath::Sign(Far.X), 0) : FVector2D(0, FMath::Sign(Far.Y));
	ArmLocal = Arm;
	Reach = FMath::Abs(Arm.X != 0 ? Far.X : Far.Y) - 32.f;   // (the lens' middle, a little in from the head's tip)
	HeadZ = B.Max.Z - 6.f;
	const int32 Slot = Posts->GetMaterialIndex(TEXT("LEDFace"));
	if (Slot != INDEX_NONE) if (UMaterialInterface* F = Posts->GetMaterial(Slot)) { Faces = UMaterialInstanceDynamic::Create(F, this); Posts->SetMaterial(Slot, Faces); }
	ADP_LOG(TEXT("Street lights: model ready (arm %s, reach %.0f cm, head %.0f cm, LED slot %d)"), *ArmLocal.ToString(), Reach, HeadZ, Slot);
	return true;
}

bool AStreetLights::AddPost(const FVector& At, const FVector& Toward)
{
	if (!Ready()) return false;
	const FVector2D T = FVector2D(Toward.X, Toward.Y).GetSafeNormal();
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X) - FMath::Atan2(ArmLocal.Y, ArmLocal.X));
	Posts->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), At), true);
	// its light, under the head, pointing down on the road (a little toward the far side, as real road optics throw it)
	USpotLightComponent* L = NewObject<USpotLightComponent>(this);
	L->SetupAttachment(Root);
	L->SetMobility(EComponentMobility::Movable);
	const FVector Head = At + FVector(T.X, T.Y, 0.f) * Reach + FVector(0, 0, HeadZ - 4.f);
	L->SetWorldLocationAndRotation(Head, (FVector(T.X, T.Y, 0.f) * 0.35f + FVector(0, 0, -1.f)).Rotation());
	L->SetIntensityUnits(ELightUnits::Lumens);
	L->SetIntensity(0.f);
	L->SetUseTemperature(true); L->SetTemperature(4000.f);
	L->SetOuterConeAngle(72.f); L->SetInnerConeAngle(42.f);
	L->SetAttenuationRadius(2600.f);
	L->SetSourceRadius(12.f); L->SetSourceLength(30.f);
	L->SetCastShadows(false);
	L->SetVolumetricScatteringIntensity(2.f);
	L->MaxDrawDistance = 25000.f; L->MaxDistanceFadeRange = 8000.f;
	L->SetVisibility(false);
	L->RegisterComponent();
	AddInstanceComponent(L);
	Lights.Add(L);
	++Count;
	return true;
}

void AStreetLights::Tick(float Dt)
{
	Super::Tick(Dt);
	// a light sensor, as the apron's masts (LightMasts.cpp): on at sunset, off when the sun is 3 degrees up, on all day
	// in fog and storms
	static TWeakObjectPtr<ADirectionalLight> Sun;
	if (!Sun.IsValid()) Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	const float El = Sun.IsValid() ? -Sun->GetActorRotation().Pitch : 30.f;
	const bool bLowVis = ASunClock::Theme == 4 || ASunClock::Theme == 6;
	const bool bWas = bOn;
	bOn = bLowVis || (bOn ? El < 3.f : El < 0.f);
	if (bOn != bWas) ADP_LOG(TEXT("Street lights: %s (%d posts, the sun %.1f deg)"), bOn ? TEXT("on") : TEXT("off"), Count, El);
	const float Dark = bLowVis ? 0.6f : 1.f - FMath::SmoothStep(-10.f, 3.f, El);
	const float Want = bOn ? PostLumens * FMath::Max(Dark, 0.35f) : 0.f;
	for (USpotLightComponent* L : Lights)
	{
		if (!L) continue;
		if (L->IsVisible() != bOn) L->SetVisibility(bOn);
		if (FMath::Abs(L->Intensity - Want) > 50.f) L->SetIntensity(Want);
	}
	if (Faces) Faces->SetScalarParameterValue(TEXT("Glow"), bOn ? StreetFaceGlow * FMath::Max(Dark, 0.35f) : 0.f);
}
