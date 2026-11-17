#include "LampFlares.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "Effects.h"
#include "ADPKeep.h"
#include "Components/LocalLightComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

ALampFlares::ALampFlares()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

ALampFlares* ALampFlares::Get(UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<ALampFlares> It(W); It; ++It) return *It;
	return W->SpawnActor<ALampFlares>();
}

void ALampFlares::Add(ULocalLightComponent* Light, const FLinearColor& Color, float Size)
{
	static UMaterialInterface* M = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Effects/M_LampFlare.M_LampFlare")));
	if (!M || !Light) return;
	UMaterialBillboardComponent* B = NewObject<UMaterialBillboardComponent>(this);
	B->SetupAttachment(GetRootComponent());
	// just in front of the lamp's face (out of its housing), along the way it shines
	B->SetWorldLocation(Light->GetComponentLocation() + Light->GetForwardVector() * 18.f);
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(M, this);
	D->SetVectorParameterValue(TEXT("Color"), Color); D->SetScalarParameterValue(TEXT("Intensity"), 0.f);
	B->AddElement(D, nullptr, true, Size, Size, nullptr);
	B->SetCastShadow(false); B->SetVisibility(false);
	B->RegisterComponent();
	FFlare F; F.Light = Light; F.Sprite = B; F.Mat = D;
	Flares.Add(F); Keep.Add(B); Keep.Add(D);
}

void ALampFlares::Tick(float Dt)
{
	ADP_PERF_SCOPE("the lamp flares (every 0.1 s)", 2.f);
	Super::Tick(Dt);
	if ((Check -= Dt) <= 0.f) { Check = 1.f; bOn = ADPEffects::On(ADPEffects::LampFlares); }   // (Settings > Effects; off unless switched on - it was on whenever the setting was missing: the night lag of 2026-11-17, 36 -> 88 fps)   // (off unless switched on in Settings - it was on whenever the setting was missing: the night lag of 2026-11-17, 36 -> 88 fps)
	static TWeakObjectPtr<ADirectionalLight> Sun;
	if (!Sun.IsValid()) Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	const float El = Sun.IsValid() ? -Sun->GetActorRotation().Pitch : 30.f;
	const float Dark = 1.f - FMath::SmoothStep(-8.f, 3.f, El);
	FVector Eye = FVector::ZeroVector;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) { FRotator R; PC->GetPlayerViewPoint(Eye, R); }
	for (FFlare& F : Flares)
	{
		ULocalLightComponent* L = F.Light.Get();
		if (!F.Sprite || !F.Mat) continue;
		float Want = 0.f;
		// only the flares within 600 m: each is its own sprite, prepared every frame - the hundreds along the road to town
		// cost the processor a lot at night and are a speck from here anyway
		const bool bNear = FVector::DistSquared(Eye, F.Sprite->GetComponentLocation()) < FMath::Square(ADPEffects::FlareReachCm());
		if (bOn && bNear && L && L->IsVisible() && Dark > 0.01f)
		{
			// how much the lamp faces you: a floodlight seen from below and in front flares, from behind hardly at all
			const FVector To = (Eye - F.Sprite->GetComponentLocation()).GetSafeNormal();
			const float Face = FMath::Max(0.f, (float)FVector::DotProduct(L->GetForwardVector(), To));
			Want = Dark * (0.08f + 0.92f * FMath::Pow(Face, 2.f));
		}
		const bool bShow = Want > 0.01f;
		if (F.Sprite->IsVisible() != bShow) F.Sprite->SetVisibility(bShow);
		if (bShow && FMath::Abs(Want - F.Now) > 0.03f) { F.Now = Want; F.Mat->SetScalarParameterValue(TEXT("Intensity"), Want); }
	}
}
