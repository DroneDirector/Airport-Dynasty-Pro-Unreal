// The sun's place from the time: the low-precision solar position (as in the Astronomical Almanac and NOAA's
// calculator), good to about a hundredth of a degree for many years either side of 2000.
#include "SunClock.h"
#include "ADPLog.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "WindSocks.h"
#include "Puddles.h"
#include "Sim/ADPWeather.h"
#include "Site.h"
#include "PerfLog.h"
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
#include "HAL/IConsoleManager.h"

float ASunClock::HourOverride = -1.f;
int32 ASunClock::DayOverride = -1;
int32 ASunClock::Month = 0;
int32 ASunClock::Day = 0;
bool ASunClock::bRealWeather = false;
bool ASunClock::bTestHold = false;

void ASunClock::SetDate(int32 InMonth, int32 InDay)
{
	Month = FMath::Clamp(InMonth, 0, 12);
	Day = Month ? FMath::Clamp(InDay, 1, FDateTime::DaysInMonth(FDateTime::UtcNow().GetYear(), Month)) : 0;
	DayOverride = Month ? FDateTime(FDateTime::UtcNow().GetYear(), Month, Day).GetDayOfYear() : -1;
	GConfig->SetInt(TEXT("ADP.World"), TEXT("Month"), Month, GGameUserSettingsIni);
	GConfig->SetInt(TEXT("ADP.World"), TEXT("Day"), Day, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	Refresh();
}

FString ASunClock::DateName()
{
	static const TCHAR* Names[] = {TEXT("January"), TEXT("February"), TEXT("March"), TEXT("April"), TEXT("May"), TEXT("June"), TEXT("July"), TEXT("August"), TEXT("September"), TEXT("October"), TEXT("November"), TEXT("December")};
	if (Month < 1 || Month > 12) return TEXT("REAL DATE");
	return FString::Printf(TEXT("%d %s"), Day, Names[Month - 1]);
}

void ASunClock::SetRealWeather(bool bOn)
{
	bRealWeather = bOn;
	GConfig->SetBool(TEXT("ADP.World"), TEXT("RealWeather"), bOn, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
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

float ASunClock::SunElevation = 30.f;

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
	if (DayOverride > 0) When = FDateTime(When.GetYear(), 1, 1) + FTimespan::FromDays(DayOverride - 1) + When.GetTimeOfDay();
	if (HourOverride >= 0) { const FDateTime D = When.GetDate(); When = D + FTimespan::FromHours(FMath::Clamp(HourOverride, 0.f, 23.999f) - ADPSite::UtcOffsetHours(When)); }   // (the hour shown is the site's local time: Site.h)
	SunAt(When, Lat, Lon, El, Az);
	SunElevation = float(El);
	ADirectionalLight* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	if (!Sun) return;
	// the world's x runs along the runway (its heading from north), y to its right, z up; the light shines from the sun
	const double Yaw = Az - Heading + 180.0;
	Sun->SetActorRotation(FRotator(-El, Yaw, 0));
	if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
	{
		// full daylight from a few degrees up, fading through dusk; at night the sky's own light only (the moon later)
		const float Daylight = FMath::Clamp(float((El + 4.0) / 10.0), 0.f, 1.f);
		// clouds and fog let less of the sun through
		static const float Through[] = {1.f, 0.9f, 0.7f, 0.35f, 0.3f, 0.3f, 0.2f, 0.35f};
		C->SetIntensity(10.f * Daylight * Daylight * Through[FMath::Clamp(Theme, 0, 7)]);
		C->SetCastShadows(El > -2.0);
	}
}

// (tests: the clouds' coverage and density by hand; they count from the next change of weather)
static TAutoConsoleVariable<float> CVarCloudCover(TEXT("adp.CloudCover"), -10.f, TEXT("Tests: the clouds' coverage (-10 = by the weather)"));
static TAutoConsoleVariable<float> CVarCloudDense(TEXT("adp.CloudDense"), 0.f, TEXT("Tests: the clouds' density (0 = by the weather)"));

void ASunClock::ApplyWeather()
{
	// the clouds: coverage, density and the storm (Unreal's own cloud material); the fog: how thick near the ground
	// (the engine's material: -0.2 and 0.008 are its fair-weather wisps; coverage adds to the cloud shape, density is per metre.
	// Until 2026-11-23 the numbers here were 0.02..1 and 0.3..1, far outside its range: every theme was the same solid layer)
	static const float Cover[] = {-0.35f, -0.25f, -0.05f, 0.3f, 0.3f, 0.35f, 0.45f, 0.3f};
	static const float Dense[] = {0.006f, 0.008f, 0.012f, 0.0012f, 0.0015f, 0.002f, 0.006f, 0.0015f};   // (a whole grey lid must be thin, or no light gets through it and it looks dark blue)
	static const float Fog[] = {0.004f, 0.006f, 0.01f, 0.02f, 0.3f, 0.05f, 0.06f, 0.07f};
	const int32 T = FMath::Clamp(Theme, 0, 7);
	for (TActorIterator<AVolumetricCloud> It(GetWorld()); It; ++It)
		if (UVolumetricCloudComponent* C = It->FindComponentByClass<UVolumetricCloudComponent>())
		{
			if (!CloudMat) CloudMat = UMaterialInstanceDynamic::Create(C->Material.LoadSynchronous(), this);
			if (C->GetMaterial() != CloudMat) C->SetMaterial(CloudMat);
			const float Cv = CVarCloudCover.GetValueOnGameThread(), Dn = CVarCloudDense.GetValueOnGameThread();
			CloudMat->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), Cv > -9.f ? Cv : Cover[T]);
			CloudMat->SetScalarParameterValue(TEXT("Cloud_GlobalDensity"), Dn > 0.f ? Dn : Dense[T]);
			ADP_LOG(TEXT("Clouds: %s on %s (coverage %.2f, density %.3f)"), ThemeName(T), *It->GetName(), Cv > -9.f ? Cv : Cover[T], Dn > 0.f ? Dn : Dense[T]);
			CloudMat->SetScalarParameterValue(TEXT("StormClouds"), T == 6 ? 1.f : 0.f);
		}
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
		if (UExponentialHeightFogComponent* F = It->GetComponent()) F->SetFogDensity(Fog[T]);
}

// ---- the sky light (owner, 2026-11-23: on for good, a little warmer; tried first in 99ff69df) ----
// It captures the sky in real time (ADPGameMode.cpp) and lights the shade as the sky does. The camera's white balance: the
// sky's blue is balanced away (Warm), so white walls in the shade look white; the sunlit look is the sun's own.
namespace ADPSky
{
	static TAutoConsoleVariable<float> CVarIntensity(TEXT("adp.SkyLightIntensity"), 1.f, TEXT("The sky light's strength (1 = what the sky really gives)"));
	static TAutoConsoleVariable<float> CVarWarm(TEXT("adp.SkyLightWarm"), 1.05f, TEXT("How much of the sky's blue is balanced away in its light, as a camera does (0..1)"));
	static TAutoConsoleVariable<int32> CVarWeather(TEXT("adp.TestWeather"), -1, TEXT("Tests only (a -UI test run): the weather theme, 0 clear .. 3 overcast .. 7 snow; -1 = leave it"));

	static void Check(UWorld* W)
	{
		const int32 Wx = CVarWeather.GetValueOnGameThread();
		if (Wx >= 0 && Wx <= 7 && Wx != ASunClock::Theme && ASunClock::ManualWeatherAllowed()) ASunClock::HoldTheme(Wx);
		static FVector2f LastCloud(-10.f, 0.f);
		const FVector2f Cloud(CVarCloudCover.GetValueOnGameThread(), CVarCloudDense.GetValueOnGameThread());
		if (Cloud != LastCloud) { LastCloud = Cloud; ASunClock::Refresh(); }
		static TWeakObjectPtr<USkyLightComponent> Done;
		static FVector2f Last(-1.f, -1.f);
		const FVector2f Now(CVarIntensity.GetValueOnGameThread(), FMath::Clamp(CVarWarm.GetValueOnGameThread(), 0.f, 1.5f));
		if (Done.IsValid() && Done->GetWorld() == W && Now == Last) return;
		ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(W, ASkyLight::StaticClass()));
		USkyLightComponent* S = Sky ? Sky->GetLightComponent() : nullptr;
		if (!S) return;
		Done = S; Last = Now;
		S->bLowerHemisphereIsBlack = false;
		S->SetIntensity(Now.X);
		S->SetLightColor(FLinearColor(1.f, 1.f - 0.18f * Now.Y, 1.f - 0.42f * Now.Y));
		S->MarkRenderStateDirty();
	}
}

void ASunClock::BeginPlay()
{
	Super::BeginPlay();
	GConfig->GetInt(TEXT("ADP.Testing"), TEXT("Weather"), Theme, GGameUserSettingsIni);
	// the time of year chosen in Settings (0: the real date), and the real-weather switch
	{ int32 M = 0, D = 0; GConfig->GetInt(TEXT("ADP.World"), TEXT("Month"), M, GGameUserSettingsIni); GConfig->GetInt(TEXT("ADP.World"), TEXT("Day"), D, GGameUserSettingsIni); if (M > 0 && DayOverride < 0) { Month = M; Day = D; DayOverride = FDateTime(FDateTime::UtcNow().GetYear(), FMath::Clamp(M, 1, 12), FMath::Clamp(D, 1, FDateTime::DaysInMonth(FDateTime::UtcNow().GetYear(), FMath::Clamp(M, 1, 12)))).GetDayOfYear(); } }
	GConfig->GetBool(TEXT("ADP.World"), TEXT("RealWeather"), bRealWeather, GGameUserSettingsIni);
	if (!ManualWeatherAllowed()) bRealWeather = true;   // (the finished game: always the real weather - to-do 193)
	ADPSky::Check(GetWorld());   // (the sky's own light fills the shadows, as outdoors, where a shadow is never black)
	ApplyWeather();
}

void ASunClock::Tick(float Dt)
{
	ADP_PERF_SCOPE("the sun clock (each frame)", 2.f);
	Super::Tick(Dt);
	ADPSky::Check(GetWorld());
	if ((Clock += Dt) >= 5.f) { Clock = 0; Update(); }
	if ((LiveClock += Dt) >= 60.f) { LiveClock = 0; ApplyLive(); }
	// the sky light follows the sun's colour now and then (it captures the sky in real time where it can)
	if ((SkyClock += Dt) >= 120.f)
	{
		SkyClock = 0;
		if (ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(this, ASkyLight::StaticClass())))
			if (USkyLightComponent* S = Sky->GetLightComponent()) if (!S->bRealTimeCapture) S->RecaptureSky();
	}
}

bool ASunClock::ManualWeatherAllowed()
{
	const TCHAR* C = FCommandLine::Get();
	return FParse::Param(C, TEXT("ManualWeather")) || FCString::Strifind(C, TEXT("-UI")) != nullptr;
}

void ASunClock::HoldTheme(int32 InTheme)
{
	Theme = FMath::Clamp(InTheme, 0, 7); bTestHold = true;
	if (APuddles* P = APuddles::Get()) P->ClearLiveWeather();   // (rain and snow on the ground follow the chosen theme)
	Refresh();
	ADP_LOG(TEXT("Weather: %s held for the test"), ThemeName(Theme));
}

void ASunClock::ReleaseTheme()
{
	bTestHold = false;
	if (ASunClock* S = Current.Get()) { S->LiveClock = 0.f; S->ApplyLive(); }
	ADP_LOG(TEXT("Weather: live again"));
}

void ASunClock::ApplyLive()
{
	if (!bRealWeather || bTestHold) return;
	TSharedPtr<FJsonObject> A = ADPSite::AirportBlock();
	FString Icao = TEXT("BIAP"); double SLat = 63.458, SLon = -18.714;
	if (A) { A->TryGetStringField(TEXT("icao"), Icao); A->TryGetNumberField(TEXT("lat"), SLat); A->TryGetNumberField(TEXT("lon"), SLon); }
	const FADPWx X = ADPWeather::At(Icao, SLat, SLon, FDateTime::UtcNow());
	// the sky: the report's weather first, then its cloud layers (the most covered one)
	int32 T = 0;
	const FString R = TEXT(" ") + X.Raw + TEXT(" ");
	if (R.Contains(TEXT(" OVC")) || R.Contains(TEXT(" VV"))) T = 3; else if (R.Contains(TEXT(" BKN"))) T = 2; else if (R.Contains(TEXT(" SCT")) || R.Contains(TEXT(" FEW"))) T = 1;
	if (!X.bLive) T = X.CeilingFt < 3000 ? 3 : X.CeilingFt < 8000 ? 2 : 1;
	if (X.bFog || X.VisM < 1000) T = 4;
	if (X.bRain) T = 5;
	if (X.bSnow) T = 7;
	if (X.bStorm) T = 6;
	if (T != Theme) { Theme = T; ApplyWeather(); ADP_LOG(TEXT("Live weather: %s at %s - %s"), ThemeName(T), *X.Station, *X.Raw); }
	// rain and snow on the ground: the report's intensity (light / moderate / heavy) as mm or cm an hour
	auto Has = [&](const TCHAR* K) { return R.Contains(K); };
	float Rain = 0.f, Snow = 0.f;
	if (X.bRain) Rain = Has(TEXT("+RA")) || Has(TEXT("+SHRA")) || Has(TEXT("+TSRA")) ? 10.f : Has(TEXT("-RA")) || Has(TEXT("-SHRA")) ? 1.f : Has(TEXT("DZ")) ? 0.3f : 4.f;
	if (X.bSnow) Snow = Has(TEXT("+SN")) || Has(TEXT("+SHSN")) ? 4.f : Has(TEXT("-SN")) || Has(TEXT("-SHSN")) ? 0.5f : 1.5f;
	if (APuddles* P = APuddles::Get()) P->SetLiveWeather(Rain, Snow, float(X.TempC));
	// the wind: windsocks, flags and (with the aircraft in 3D) the aircraft
	AWindSocks::SetLive(float(X.WindDir), float(X.WindKt), float(FMath::Max(X.GustKt, X.WindKt)));
}
