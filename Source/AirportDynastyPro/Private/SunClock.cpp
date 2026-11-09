// The sun's place from the time: the low-precision solar position (as in the Astronomical Almanac and NOAA's
// calculator), good to about a hundredth of a degree for many years either side of 2000.
#include "SunClock.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"

float ASunClock::HourOverride = -1.f;
int32 ASunClock::Theme = 1;
TWeakObjectPtr<ASunClock> ASunClock::Current;

const TCHAR* ASunClock::ThemeName(int32 T)
{
	static const TCHAR* N[] = {TEXT("CLEAR"), TEXT("FEW CLOUDS"), TEXT("BROKEN"), TEXT("OVERCAST"), TEXT("FOG"), TEXT("RAIN"), TEXT("STORM"), TEXT("SNOW")};
	return N[FMath::Clamp(T, 0, 7)];
}

void ASunClock::Refresh() { if (ASunClock* S = Current.Get()) { S->Update(); S->ApplyWeather(); } }

ASunClock::ASunClock()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Current = this;
}

void ASunClock::SunAt(const FDateTime& Utc, double Lat, double Lon, double& Elevation, double& Azimuth)
{
	const double D = (Utc - FDateTime(2000, 1, 1, 12)).GetTotalDays();   // days since J2000.0
	auto Rad = [](double Deg) { return FMath::DegreesToRadians(Deg); };
	const double L = FMath::Fmod(280.460 + 0.9856474 * D, 360.0);         // mean longitude
	const double G = Rad(FMath::Fmod(357.528 + 0.9856003 * D, 360.0));   // mean anomaly
	const double Lambda = Rad(L + 1.915 * FMath::Sin(G) + 0.020 * FMath::Sin(2 * G));   // ecliptic longitude
	const double Eps = Rad(23.439 - 0.0000004 * D);                       // the Earth's tilt
	const double Ra = FMath::Atan2(FMath::Cos(Eps) * FMath::Sin(Lambda), FMath::Cos(Lambda));
	const double Dec = FMath::Asin(FMath::Sin(Eps) * FMath::Sin(Lambda));
	const double Gmst = FMath::Fmod(18.697374558 + 24.06570982441908 * D, 24.0);   // sidereal time at Greenwich, hours
	const double Ha = Rad(Gmst * 15.0 + Lon) - Ra;                        // the sun's hour angle here
	const double Phi = Rad(Lat);
	const double SinEl = FMath::Sin(Phi) * FMath::Sin(Dec) + FMath::Cos(Phi) * FMath::Cos(Dec) * FMath::Cos(Ha);
	Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SinEl, -1.0, 1.0)));
	Azimuth = FMath::RadiansToDegrees(FMath::Atan2(-FMath::Sin(Ha), FMath::Tan(Dec) * FMath::Cos(Phi) - FMath::Sin(Phi) * FMath::Cos(Ha)));
	if (Azimuth < 0) Azimuth += 360.0;
}

void ASunClock::Update()
{
	double El = 0, Az = 0;
	FDateTime When = FDateTime::UtcNow();
	if (HourOverride >= 0) { const FDateTime D = When.GetDate(); When = D + FTimespan::FromHours(FMath::Clamp(HourOverride, 0.f, 23.999f)); }
	SunAt(When, Lat, Lon, El, Az);
	ADirectionalLight* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	if (!Sun) return;
	// the world's x runs along the runway (its heading from north), y to its right, z up; the light shines from the sun
	const double Yaw = Az - Heading + 180.0;
	Sun->SetActorRotation(FRotator(-El, Yaw, 0));
	if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
	{
		// full daylight from a few degrees up, fading through dusk; at night the sky's own light only (the moon later)
		const float Day = FMath::Clamp(float((El + 4.0) / 10.0), 0.f, 1.f);
		// clouds and fog let less of the sun through
		static const float Through[] = {1.f, 0.9f, 0.7f, 0.35f, 0.3f, 0.3f, 0.2f, 0.35f};
		C->SetIntensity(10.f * Day * Day * Through[FMath::Clamp(Theme, 0, 7)]);
		C->SetCastShadows(El > -2.0);
	}
}

void ASunClock::ApplyWeather()
{
	// the clouds: coverage, density and the storm (Unreal's own cloud material); the fog: how thick near the ground
	static const float Cover[] = {0.02f, 0.35f, 0.6f, 1.f, 0.85f, 1.f, 1.f, 1.f};
	static const float Dense[] = {0.3f, 0.45f, 0.6f, 0.9f, 0.8f, 1.f, 1.f, 0.85f};
	static const float Fog[] = {0.004f, 0.006f, 0.01f, 0.02f, 0.3f, 0.05f, 0.06f, 0.07f};
	const int32 T = FMath::Clamp(Theme, 0, 7);
	for (TActorIterator<AVolumetricCloud> It(GetWorld()); It; ++It)
		if (UVolumetricCloudComponent* C = It->FindComponentByClass<UVolumetricCloudComponent>())
		{
			if (!CloudMat) { CloudMat = UMaterialInstanceDynamic::Create(C->Material.LoadSynchronous(), this); C->SetMaterial(CloudMat); }
			CloudMat->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), Cover[T]);
			CloudMat->SetScalarParameterValue(TEXT("Cloud_GlobalDensity"), Dense[T]);
			CloudMat->SetScalarParameterValue(TEXT("StormClouds"), T == 6 ? 1.f : 0.f);
		}
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
		if (UExponentialHeightFogComponent* F = It->GetComponent()) F->SetFogDensity(Fog[T]);
}

void ASunClock::BeginPlay()
{
	Super::BeginPlay();
	GConfig->GetInt(TEXT("ADP.Testing"), TEXT("Weather"), Theme, GGameUserSettingsIni);
	// the sky's own light fills the shadows (as outdoors, where a shadow is never black); captured from the sky in real time
	if (ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(this, ASkyLight::StaticClass())))
		if (USkyLightComponent* S = Sky->GetLightComponent()) { S->SetIntensity(1.8f); S->SetLowerHemisphereIsBlack(false); S->RecaptureSky(); }
	ApplyWeather();
}

void ASunClock::Tick(float Dt)
{
	Super::Tick(Dt);
	if ((Clock += Dt) >= 5.f) { Clock = 0; Update(); }
	// the sky light follows the sun's colour now and then (it captures the sky in real time where it can)
	if ((SkyClock += Dt) >= 120.f)
	{
		SkyClock = 0;
		if (ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(this, ASkyLight::StaticClass())))
			if (USkyLightComponent* S = Sky->GetLightComponent()) if (!S->bRealTimeCapture) S->RecaptureSky();
	}
}
