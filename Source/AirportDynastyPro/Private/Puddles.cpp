#include "Puddles.h"
#include "ADPLog.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SunClock.h"

#pragma region Setup

namespace
{
	TWeakObjectPtr<APuddles> GPuddles;
	// the tuning's names, the game's own values (used when Puddles.ini or a line is missing) and the slider ranges
	struct FTune { const TCHAR* Name; float Def; float Min; float Max; const TCHAR* Help; };
	const FTune Tunes[] = {
		{TEXT("Amount"),      0.55f, 0.f,  1.f,  TEXT("how many puddles: 0 none, 1 a great many")},
		{TEXT("Size"),        3.0f,  0.5f, 12.f, TEXT("their size, metres (the spots' spacing)")},
		{TEXT("Shine"),       0.9f,  0.f,  1.f,  TEXT("how mirror-like the water is")},
		{TEXT("Ripples"),     1.0f,  0.f,  2.f,  TEXT("rain ripples' strength")},
		{TEXT("WetMinutes"),  1.0f,  0.1f, 10.f, TEXT("minutes for the ground to get fully wet in rain")},
		{TEXT("DryingHours"), 3.0f,  0.05f, 24.f, TEXT("hours for the ground to dry after the rain (Iceland-like: about 2-6)")},
		{TEXT("Snow"),        0.0f,  0.f,  1.f,  TEXT("snow on the ground: 0 none, 0.3 a light ploughed cover, 1 a lot (owner, 2026-11-13)")},
	};
	FString IniPath() { return FPaths::ProjectContentDir() / TEXT("Settings/Puddles.ini"); }
}

APuddles::APuddles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	Mpc = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Weather/MPC_Weather.MPC_Weather"));
}

APuddles* APuddles::Get() { return GPuddles.Get(); }
float APuddles::WeatherSpeed = 1.f;

void APuddles::BeginPlay()
{
	Super::BeginPlay();
	GPuddles = this;
	if (!Mpc) ADP_WARN(TEXT("Puddles: /Game/Weather/MPC_Weather is missing (Tools/set_puddles.py); the ground stays dry"));
	Load();
	// the test (-UIPuddles) and the command line can start on a rainy day, already soaked
	if (FParse::Param(FCommandLine::Get(), TEXT("RainyDay"))) { bRainyDay = true; Wetness = 1.f; Rain = 1.f; }
	// -SnowDay: the ground under a light ploughed snow cover for the tests (not saved)
	if (FParse::Param(FCommandLine::Get(), TEXT("SnowDay"))) SnowDay = 0.35f;
	Push();
}

#pragma endregion

#pragma region Weather

void APuddles::SetRainyDay(bool bOn)
{
	bRainyDay = bOn;
	ADP_LOG(TEXT("Puddles: rainy day %s"), bOn ? TEXT("on") : TEXT("off"));
}

void APuddles::Tick(float Dt)
{
	Super::Tick(Dt);
	// it wets fast while it rains (WetMinutes to soaked) and dries slowly after (DryingHours), the rain's ripples
	// fading in and out over a few seconds
	const float WetRate = 1.f / FMath::Max(6.f, GetTuning(TEXT("WetMinutes")) * 60.f);
	const float DryRate = 1.f / FMath::Max(60.f, GetTuning(TEXT("DryingHours")) * 3600.f);
	float RainK = 0.f; GroundWeather(Dt, RainK);
	const bool bWet = bRainyDay || RainK > 0.f;
	Wetness = FMath::Clamp(Wetness + (bWet ? WetRate : -DryRate) * Dt * WeatherSpeed, 0.f, 1.f);   // (the test's fast clock wets and dries it too)
	Rain = FMath::FInterpTo(Rain, bRainyDay ? 1.f : RainK, Dt, 0.6f);
	Push();
}

void APuddles::GroundWeather(float Dt, float& OutRainK)
{
	// the air's temperature: the month's mean for the south coast of Iceland (Vik, Icelandic Met Office normals), a
	// day's swing of about 2.5 C (warmest mid-afternoon), clear nights colder; snowing means at or below freezing, rain
	// above it
	static const float Mean[12] = {0.4f, 0.6f, 1.0f, 3.4f, 6.6f, 9.1f, 10.9f, 10.5f, 8.0f, 4.8f, 2.0f, 0.7f};
	const int32 Th = ASunClock::Theme;
	float RainMm = 0.f, SnowCm = 0.f;
	if (bLive) { AirTemp = LiveTemp; RainMm = LiveRain; SnowCm = LiveSnow; }
	else
	{
		const float Hr = ASunClock::HourOverride >= 0.f ? ASunClock::HourOverride : (float)FDateTime::UtcNow().GetHour() + FDateTime::UtcNow().GetMinute() / 60.f;
		float T = Mean[FMath::Clamp(ASunClock::Month, 1, 12) - 1] + 2.5f * FMath::Cos((Hr - 15.f) / 24.f * 2.f * PI);
		if (Th <= 1 && (Hr < 7.f || Hr > 20.f)) T -= 1.5f;
		if (Th == 7) T = FMath::Min(T, -1.5f);
		if (Th == 5 || Th == 6) T = FMath::Max(T, 1.5f);
		AirTemp = T;
		if (Th == 5) RainMm = 2.f; else if (Th == 6) RainMm = 6.f;   // (moderate rain, a storm's downpour: mm an hour)
		if (Th == 7) SnowCm = 1.5f;   // (steady snow, about 1.5 cm an hour)
	}
	if (AirTemp <= 0.5f && RainMm > 0.f) { SnowCm += RainMm * 0.8f; RainMm = 0.f; }   // (at freezing the rain falls as snow: about 1 cm per mm)
	const float H = Dt / 3600.f * WeatherSpeed;   // (hours of weather this frame)
	// melting above 0 C: about 0.4 cm an hour a degree, more in rain, and in the sun by day
	const float Sun = ASunClock::SunElevation > 5.f && Th <= 2 ? 0.5f : 0.f;
	const float Melt = AirTemp > 0.f ? AirTemp * 0.4f + RainMm * 0.25f + Sun : 0.f;
	SnowGrassCm = FMath::Clamp(SnowGrassCm + (SnowCm - Melt) * H, 0.f, 60.f);
	// the paving: ploughed (whatever lies over about 1.5 cm is pushed off), worn by the traffic, melting twice as fast
	SnowPaveCm = FMath::Clamp(SnowPaveCm + (SnowCm * 0.6f - Melt * 2.f - 0.2f) * H, 0.f, 1.5f);
	// the puddles fill with the rain that falls (and the melt water), drain as the ground dries
	const float Inflow = (RainMm + (Melt > 0.f && SnowGrassCm > 0.f ? Melt * 0.5f : 0.f)) / 6.f;
	Water = FMath::Clamp(Water + (Inflow - 1.f / FMath::Max(0.05f, GetTuning(TEXT("DryingHours")))) * H, 0.f, 1.f);
	OutRainK = RainMm > 0.f ? FMath::Clamp(0.5f + RainMm / 12.f, 0.f, 1.f) : 0.f;   // (the ripples: half in light rain, full in a downpour)
}

void APuddles::Push() const
{
	if (!Mpc || !GetWorld()) return;
	UWorld* W = GetWorld();
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("Wetness"), Wetness);
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("Rain"), Rain);
	// (the rainy-day switch and the tests keep their full puddles; the weather fills them by how much it rained)
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("PuddleAmount"), GetTuning(TEXT("Amount")) * (bRainyDay ? 1.f : 0.35f + 0.65f * Water));
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("PuddleSize"), GetTuning(TEXT("Size")));
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("PuddleShine"), GetTuning(TEXT("Shine")));
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("Ripples"), GetTuning(TEXT("Ripples")));
	// snow: the -SnowDay test or the panel's own amount when set; otherwise the weather's (a light ploughed cover on
	// the paving at most, the grass to a full white over about 12 cm)
	const float Manual = SnowDay >= 0.f ? SnowDay : GetTuning(TEXT("Snow"));
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("Snow"), Manual > 0.f ? Manual : FMath::Clamp(SnowPaveCm / 1.5f, 0.f, 1.f) * 0.65f);   // (owner: more snow fill on the paving)
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("SnowGrass"), Manual > 0.f ? FMath::Min(1.f, Manual * 1.6f) : FMath::Clamp(SnowGrassCm / 12.f, 0.f, 1.f));
}

#pragma endregion

#pragma region Tuning

float APuddles::GetTuning(const FString& Name) const
{
	if (const float* V = Tuning.Find(Name)) return *V;
	for (const FTune& T : Tunes) if (Name == T.Name) return T.Def;
	return 0.f;
}

void APuddles::SetTuning(const FString& Name, float Value)
{
	for (const FTune& T : Tunes) if (Name == T.Name) { Tuning.Add(Name, FMath::Clamp(Value, T.Min, T.Max)); Push(); return; }
}

FString APuddles::TuningJson() const
{
	FString S = FString::Printf(TEXT("{\"wetness\":%.3f,\"rainy\":%s"), Wetness, bRainyDay ? TEXT("true") : TEXT("false"));
	for (const FTune& T : Tunes)
		S += FString::Printf(TEXT(",\"%s\":{\"v\":%.3f,\"min\":%.3f,\"max\":%.3f,\"help\":\"%s\"}"), T.Name, GetTuning(T.Name), T.Min, T.Max, T.Help);
	return S + TEXT("}");
}

void APuddles::Load()
{
	// Content/Settings/Puddles.ini: [Puddles] Name=Value lines, as TruckLamps.ini
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *IniPath())) { ADP_LOG(TEXT("Puddles: %s not found; the built-in values are used"), *IniPath()); return; }
	for (FString L : Lines)
	{
		L.TrimStartAndEndInline();
		FString K, V;
		if (L.IsEmpty() || L.StartsWith(TEXT(";")) || L.StartsWith(TEXT("[")) || !L.Split(TEXT("="), &K, &V)) continue;
		K.TrimStartAndEndInline(); V.TrimStartAndEndInline();
		if (V.IsNumeric()) SetTuning(K, FCString::Atof(*V));
	}
	ADP_LOG(TEXT("Puddles: %d settings from %s"), Tuning.Num(), *IniPath());
}

void APuddles::Save() const
{
	FString S = TEXT("; The puddles on the airfield (the game's puddle panel writes this; change a number, save, restart).\n");
	S += TEXT("; (If this file or a line is missing, the game uses its own values.)\n");
	for (const FTune& T : Tunes) S += FString::Printf(TEXT(";   %-12s %s\n"), T.Name, T.Help);
	S += TEXT("\n[Puddles]\n");
	for (const FTune& T : Tunes) S += FString::Printf(TEXT("%s=%.3f\n"), T.Name, GetTuning(T.Name));
	if (FFileHelper::SaveStringToFile(S, *IniPath())) ADP_LOG(TEXT("Puddles: saved to %s"), *IniPath());
	else ADP_WARN(TEXT("Puddles: could not save %s"), *IniPath());
}

#pragma endregion
