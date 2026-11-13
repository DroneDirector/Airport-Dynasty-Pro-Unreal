#include "Puddles.h"
#include "ADPLog.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "EngineUtils.h"

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
	Wetness = FMath::Clamp(Wetness + (bRainyDay ? WetRate : -DryRate) * Dt, 0.f, 1.f);
	Rain = FMath::FInterpTo(Rain, bRainyDay ? 1.f : 0.f, Dt, 0.6f);
	Push();
}

void APuddles::Push() const
{
	if (!Mpc || !GetWorld()) return;
	UWorld* W = GetWorld();
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("Wetness"), Wetness);
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("Rain"), Rain);
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("PuddleAmount"), GetTuning(TEXT("Amount")));
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("PuddleSize"), GetTuning(TEXT("Size")));
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("PuddleShine"), GetTuning(TEXT("Shine")));
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("Ripples"), GetTuning(TEXT("Ripples")));
	UKismetMaterialLibrary::SetScalarParameterValue(W, Mpc, TEXT("Snow"), SnowDay >= 0.f ? SnowDay : GetTuning(TEXT("Snow")));
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
