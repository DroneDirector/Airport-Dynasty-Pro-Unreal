#include "SoffitLights.h"
#include "SunClock.h"
#include "TerminalPlan.h"
#include "ADPLog.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

static TAutoConsoleVariable<float> CVarSoffitDay(TEXT("adp.SoffitDay"), 0.85f, TEXT("The daylight under the terminal's wave roof (1 = as before the sky light gave its own)"));
static TAutoConsoleVariable<float> CVarSoffitNight(TEXT("adp.SoffitNight"), 1.f, TEXT("The downlights' light under the terminal's wave roof (1 = as built)"));

ASoffitLights::ASoffitLights()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ASoffitLights::Build()
{
	// (plan metres; Len along the building, Wid across it; Tilt: turned out towards the road, degrees)
	auto Add = [this](float X, float Y, float Z, float Len, float Wid, float Tilt, float DayCd, float NightCd)
	{
		URectLightComponent* L = NewObject<URectLightComponent>(this);
		L->SetupAttachment(Root);
		L->SetMobility(EComponentMobility::Movable);
		const float T = FMath::DegreesToRadians(Tilt);
		// facing down (its x), its long side along the building (its z); the plan's -z is the world's +y
		L->SetWorldLocationAndRotation(FTerminalPlan::ToWorld(X, Y, Z), FRotationMatrix::MakeFromXZ(FVector(0, FMath::Sin(T), -FMath::Cos(T)), FVector(1, 0, 0)).Rotator());
		L->SetSourceHeight(Len * 100.f); L->SetSourceWidth(Wid * 100.f);
		L->SetBarnDoorAngle(88.f);
		L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(0.f);
		L->SetAttenuationRadius(5200.f);
		L->SetCastShadows(false);
		L->SetSpecularScale(0.f);
		L->SetVolumetricScatteringIntensity(0.f);
		L->SetAffectTranslucentLighting(false);
		L->MaxDrawDistance = 40000.f; L->MaxDistanceFadeRange = 10000.f;
		L->SetVisibility(false);
		L->RegisterComponent();
		AddInstanceComponent(L);
		Lights.Add(L); Panels.Add({DayCd, NightCd});
	};
	// along the whole front: the pavement and the paving to the doors
	for (int32 k = 0; k < 16; ++k) Add(-62.f + 40.f * k, 10.4f, -5.f, 38.f, 2.f, 20.f, 650.f, 1400.f);
	// the open ground under the roof's west end
	for (int32 k = 0; k < 4; ++k) Add(-224.f + 40.f * k, 10.4f, 9.f, 38.f, 30.f, 0.f, 1000.f, 1000.f);
	Add(566.f, 10.4f, 9.f, 22.f, 30.f, 0.f, 450.f, 500.f); Add(567.f, 10.4f, -6.f, 18.f, 2.f, 20.f, 250.f, 400.f);   // and its east end
	// inside, the roof's downlights over the Grand Hall, the East Hall and the promenade (by day, with the skylights' own
	// daylight, at half strength - owner, 2026-11-23: the hall a little brighter)
	for (const float X : {60.f, 96.f, 174.f, 210.f}) for (const float Z : {9.f, 24.f}) Add(X, 10.4f, Z, 30.f, 3.f, 0.f, 420.f, 700.f);
	for (int32 k = 0; k < 8; ++k) Add(248.f + 40.3f * k, 10.4f, 4.f, 38.f, 2.f, 0.f, 250.f, 700.f);
	ADP_LOG(TEXT("Soffit lights: %d panels under the wave roof"), Lights.Num());
}

void ASoffitLights::Tick(float Dt)
{
	Super::Tick(Dt);
	if ((Check -= Dt) > 0.f) return;
	Check = 0.5f;
	static TWeakObjectPtr<ADirectionalLight> Sun;
	if (!Sun.IsValid()) Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	if (!Sun.IsValid()) return;
	// by day as the daylight itself comes and goes (SunClock.cpp's own fade; a cloudy sky lights the shade a little less);
	// by night on and off with the street lights: at sunset, off when the sun is 3 degrees up, all day in fog and storms
	const float El = -Sun->GetActorRotation().Pitch;
	const float Dl = FMath::Clamp((El + 4.f) / 10.f, 0.f, 1.f);
	static const float Sky[] = {1.f, 1.f, 0.9f, 0.7f, 0.6f, 0.6f, 0.45f, 0.7f};
	const float Day = Dl * Dl * Sky[FMath::Clamp(ASunClock::Theme, 0, 7)] * CVarSoffitDay.GetValueOnGameThread();
	const bool bLowVis = ASunClock::Theme == 4 || ASunClock::Theme == 6;
	bOn = bLowVis || (bOn ? El < 3.f : El < 0.f);
	const float Night = bOn ? FMath::Max(bLowVis ? 0.6f : 1.f - FMath::SmoothStep(-10.f, 3.f, El), 0.35f) * CVarSoffitNight.GetValueOnGameThread() : 0.f;
	if (FMath::Abs(Day - LastDay) < 0.004f && FMath::Abs(Night - LastNight) < 0.004f) return;
	LastDay = Day; LastNight = Night;
	const FLinearColor DayCol(1.f, 0.98f, 0.95f), NightCol(1.f, 0.9f, 0.76f);
	for (int32 i = 0; i < Lights.Num(); ++i)
	{
		URectLightComponent* L = Lights[i];
		const float D = Panels[i].DayCd * Day, N = Panels[i].NightCd * Night, Cd = D + N;
		if (L->IsVisible() != (Cd > 0.5f)) L->SetVisibility(Cd > 0.5f);
		if (Cd <= 0.5f) continue;
		L->SetIntensity(Cd);
		L->SetLightColor(FMath::Lerp(DayCol, NightCol, N / Cd));
	}
}
