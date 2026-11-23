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
#include "Engine/PostProcessVolume.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

float ASunClock::HourOverride = -1.f;
int32 ASunClock::DayOverride = -1;
int32 ASunClock::Month = 0;
int32 ASunClock::Day = 0;
bool ASunClock::bRealWeather = false;

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

// ---- the sky light, a TRIAL (owner to decide): off unless -SkyLightTrial or adp.SkyLight 1 ----
// The game's sky light gives no light at all: the game mode spawns it deferred, which registers the component (and makes
// its render proxy) before bRealTimeCapture is set; setting the flag by hand does not make the proxy again, so the
// renderer never captures the sky in real time, while the component, believing it does, skips the ordinary capture too.
// The trial has the proxy made again (MarkRenderStateDirty): it then captures the sky in real time, as meant.
namespace ADPSkyTrial
{
	static TAutoConsoleVariable<int32> CVarOn(TEXT("adp.SkyLight"), 0, TEXT("The sky light trial: 1 = the sky lights the world (0 = as shipped)"));
	static TAutoConsoleVariable<float> CVarIntensity(TEXT("adp.SkyLightIntensity"), 1.f, TEXT("Trial: the sky light's strength (1 = what the sky really gives)"));
	static TAutoConsoleVariable<float> CVarWarm(TEXT("adp.SkyLightWarm"), 0.7f, TEXT("Trial: how much of the sky's blue is balanced away, as a camera does (0..1)"));
	static TAutoConsoleVariable<float> CVarSoffit(TEXT("adp.SkyLightSoffit"), 0.4f, TEXT("Trial: the share of the daylight panels under the wave roof that is left"));
	static TAutoConsoleVariable<float> CVarLeak(TEXT("adp.SkyLightLeak"), 0.f, TEXT("Trial: the sky's leak indoors (0.12 as shipped)"));
	static TAutoConsoleVariable<int32> CVarGlass(TEXT("adp.SkyLightGlass"), 2, TEXT("Trial: the terminal's panes: 0 = as shipped, 1 = only smoother, 2 = the window glass (M_WindowGlass: clear straight on, mirroring less at a slant)"));
	static TAutoConsoleVariable<float> CVarGlassRough(TEXT("adp.SkyLightGlassRough"), 0.02f, TEXT("Trial: the panes' roughness"));
	static TAutoConsoleVariable<float> CVarGlassStraight(TEXT("adp.SkyLightGlassStraight"), 0.06f, TEXT("Trial, glass 2: how solid a pane is seen straight on"));
	static TAutoConsoleVariable<float> CVarGlassSlant(TEXT("adp.SkyLightGlassSlant"), 0.35f, TEXT("Trial, glass 2: how solid at a slant"));
	static TAutoConsoleVariable<float> CVarGlassSpec(TEXT("adp.SkyLightGlassSpec"), 0.5f, TEXT("Trial, glass 2: how much it mirrors"));
	static TAutoConsoleVariable<int32> CVarWeather(TEXT("adp.TestWeather"), -1, TEXT("Tests only (a -UI test run): the weather theme, 0 clear .. 3 overcast .. 7 snow; -1 = leave it"));
	static FString Last(TEXT("0"));
	static TMap<TWeakObjectPtr<UMaterialInterface>, TPair<TStrongObjectPtr<UMaterialInstanceDynamic>, TWeakObjectPtr<UMaterialInterface>>> Panes;   // the pane -> the trial's copy of it
	static TMap<TWeakObjectPtr<APostProcessVolume>, float> Leaks;

	static void Glass(UWorld* W, bool bOn)
	{
		static int32 LastMode = 0;
		const int32 Mode = bOn ? CVarGlass.GetValueOnGameThread() : 0;
		if (LastMode && Mode && Mode != LastMode) Glass(W, false);   // (the other kind: the shipped panes back first)
		LastMode = Mode; bOn = Mode > 0;
		UMaterialInterface* Window = Mode == 2 ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Glass/M_WindowGlass.M_WindowGlass")) : nullptr;
		TMap<UMaterialInterface*, UMaterialInterface*> Back;
		for (auto& P : Panes) if (P.Value.Key.IsValid() && P.Value.Value.IsValid()) Back.Add(P.Value.Key.Get(), P.Value.Value.Get());
		int32 n = 0;
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			if (It->GetWorld() != W || !It->IsRegistered()) continue;
			for (int32 i = 0; i < It->GetNumMaterials(); ++i)
			{
				UMaterialInterface* M = It->GetMaterial(i);
				if (!M) continue;
				UMaterialInterface** Own = Back.Find(M);
				if (!bOn) { if (Own) { It->SetMaterial(i, *Own); ++n; } continue; }
				UMaterialInterface* Src = Own ? *Own : M;
				if (!Src->GetName().StartsWith(TEXT("MI_T_glass"))) continue;
				auto& P = Panes.FindOrAdd(Src);
				if (!P.Key.IsValid())
				{
					P.Key = TStrongObjectPtr<UMaterialInstanceDynamic>(UMaterialInstanceDynamic::Create(Window ? Window : Src, GetTransientPackage()));
					P.Value = Src; Back.Add(P.Key.Get(), Src);
				}
				P.Key->SetScalarParameterValue(Window ? TEXT("Roughness") : TEXT("R"), CVarGlassRough.GetValueOnGameThread());
				if (Window)
				{
					P.Key->SetScalarParameterValue(TEXT("OpacityStraight"), CVarGlassStraight.GetValueOnGameThread());
					P.Key->SetScalarParameterValue(TEXT("OpacitySlant"), CVarGlassSlant.GetValueOnGameThread());
					P.Key->SetScalarParameterValue(TEXT("Specular"), CVarGlassSpec.GetValueOnGameThread());
					FLinearColor C; if (Src->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("BC")), C)) P.Key->SetVectorParameterValue(TEXT("Tint"), FLinearColor(C.R * 0.06f, C.G * 0.06f, C.B * 0.06f, 1.f));
				}
				if (M != P.Key.Get()) { It->SetMaterial(i, P.Key.Get()); ++n; }
			}
		}
		if (!bOn) Panes.Reset();
		ADP_LOG(TEXT("Sky light trial: %d glass slots %s"), n, bOn ? TEXT("tuned") : TEXT("put back"));
	}

	static void Check(UWorld* W)
	{
		static bool bFirst = true;
		if (bFirst) { bFirst = false; if (FParse::Param(FCommandLine::Get(), TEXT("SkyLightTrial"))) CVarOn->Set(1); }
		const int32 Wx = CVarWeather.GetValueOnGameThread();
		if (Wx >= 0 && Wx != ASunClock::Theme && ASunClock::ManualWeatherAllowed()) { ASunClock::Theme = FMath::Min(Wx, 7); ASunClock::Refresh(); }
		const bool bOn = CVarOn.GetValueOnGameThread() > 0;
		if (!bOn && Last == TEXT("0")) return;   // (as shipped: nothing touched)
		const FString Now = !bOn ? FString(TEXT("0")) : FString::Printf(TEXT("%.3f %.3f %.3f %.3f glass %d %.3f %.3f %.3f %.3f"), CVarIntensity.GetValueOnGameThread(), CVarWarm.GetValueOnGameThread(), CVarSoffit.GetValueOnGameThread(),
			CVarLeak.GetValueOnGameThread(), CVarGlass.GetValueOnGameThread(), CVarGlassRough.GetValueOnGameThread(), CVarGlassStraight.GetValueOnGameThread(), CVarGlassSlant.GetValueOnGameThread(), CVarGlassSpec.GetValueOnGameThread());
		if (Now == Last) return;
		Last = Now;
		ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(W, ASkyLight::StaticClass()));
		USkyLightComponent* S = Sky ? Sky->GetLightComponent() : nullptr;
		if (!S) return;
		const float Wm = FMath::Clamp(CVarWarm.GetValueOnGameThread(), 0.f, 1.f);
		S->SetIntensity(bOn ? CVarIntensity.GetValueOnGameThread() : 1.8f);
		S->SetLightColor(bOn ? FLinearColor(1.f, 1.f - 0.18f * Wm, 1.f - 0.42f * Wm) : FLinearColor::White);
		S->SetVisibility(bOn);   // (off again after it was on: gives nothing, as shipped)
		S->MarkRenderStateDirty();
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("adp.SoffitDay"))) V->Set(bOn ? CVarSoffit.GetValueOnGameThread() : 1.f);
		for (TActorIterator<APostProcessVolume> It(W); It; ++It)
			if (It->Settings.bOverride_LumenSkylightLeaking)
			{
				float& Was = Leaks.FindOrAdd(*It, It->Settings.LumenSkylightLeaking);
				It->Settings.LumenSkylightLeaking = bOn ? CVarLeak.GetValueOnGameThread() : Was;
			}
		Glass(W, bOn);
		ADP_LOG(TEXT("Sky light trial: %s (%s)"), bOn ? TEXT("on") : TEXT("off"), *Now);
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
	// the sky's own light fills the shadows (as outdoors, where a shadow is never black); captured from the sky in real time
	if (ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(this, ASkyLight::StaticClass())))
		if (USkyLightComponent* S = Sky->GetLightComponent()) { S->SetIntensity(1.8f); S->bLowerHemisphereIsBlack = false; S->RecaptureSky(); }
	ApplyWeather();
}

void ASunClock::Tick(float Dt)
{
	ADP_PERF_SCOPE("the sun clock (each frame)", 2.f);
	Super::Tick(Dt);
	ADPSkyTrial::Check(GetWorld());
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

void ASunClock::ApplyLive()
{
	if (!bRealWeather) return;
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
