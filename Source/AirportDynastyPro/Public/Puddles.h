// Puddles and wet ground (owner, 2026-11-13; HANDOVER.md "NEWEST", Tools/set_puddles.py): the puddles live in the
// ground's own material, driven by one global wetness in /Game/Weather/MPC_Weather. This keeps that wetness: it rises
// while it rains (the Rainy day switch now; the real rain later drives the same value) and falls slowly after, as real
// ground dries in a cool climate like Iceland's (most of a puddle gone in hours; the drying speed is tunable); the
// puddle tuning (amount, size, shine, ripples, drying speed) comes from Content/Settings/Puddles.ini and the in-game
// panel writes it back there.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Puddles.generated.h"

class UMaterialParameterCollection;

UCLASS()
class AIRPORTDYNASTYPRO_API APuddles : public AActor
{
	GENERATED_BODY()

public:
	APuddles();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	/** The one in the world (or none). */
	static APuddles* Get();

	/** The Rainy day switch: on, the ground wets over about a minute and the puddles fill and ripple; off, it dries. */
	void SetRainyDay(bool bOn);
	bool IsRainyDay() const { return bRainyDay; }
	/** Wet or dry at once (the panel's wetness slider). */
	void SetWetness(float W) { Wetness = FMath::Clamp(W, 0.f, 1.f); }
	float GetWetness() const { return Wetness; }
	/** How hard it rains now (0 none .. 1). */
	float GetRain() const { return Rain; }

	/** The ground's weather (to-do 44): snow builds up while it snows and it is cold enough (heavier snow faster) and
	 *  melts above 0 C (faster in rain and sun); the grass keeps collecting while the paving is ploughed and stays nearly
	 *  free; puddles fill with how much it has rained. Its inputs come from the weather theme and the month and hour for
	 *  now (a south-Iceland climate); the live weather feed will give them with SetLiveWeather. */
	/** The weather is the sky theme's again, not the live report's (a theme chosen by a test). */
	void ClearLiveWeather() { bLive = false; }
	void SetLiveWeather(float RainMmPerHour, float SnowCmPerHour, float AirTempC) { bLive = true; LiveRain = RainMmPerHour; LiveSnow = SnowCmPerHour; LiveTemp = AirTempC; }
	float GetAirTemp() const { return AirTemp; }
	float GetSnowOnGrassCm() const { return SnowGrassCm; }
	float GetSnowOnPavingCm() const { return SnowPaveCm; }
	/** How many times faster the ground's weather runs than the clock (tests: -UIWeatherGround). */
	static float WeatherSpeed;
	static bool bGroundWeatherOn;   // Settings > Effects: off = dry paving, no puddles or snow (Effects.h)

	/** The tuning: Amount, Size, Shine, Ripples, DryingHours (by name; the panel and Puddles.ini use the same names). */
	void SetTuning(const FString& Name, float Value);
	float GetTuning(const FString& Name) const;
	/** The tuning as JSON for the panel: {"wetness":..,"rainy":..,"amount":..,...}. */
	FString TuningJson() const;
	/** Writes the tuning to Content/Settings/Puddles.ini (the release keeps the owner's values as the default). */
	void Save() const;

private:
	void Load();
	void Push() const;

	UPROPERTY() TObjectPtr<UMaterialParameterCollection> Mpc;
	bool bRainyDay = false;
	float Wetness = 0.f;
	float Rain = 0.f;
	float SnowDay = -1.f;   // the -SnowDay test's snow (unset: the Snow setting)
	void GroundWeather(float Dt, float& OutRainK);
	bool bLive = false; float LiveRain = 0.f, LiveSnow = 0.f, LiveTemp = 5.f;
	float AirTemp = 5.f, SnowGrassCm = 0.f, SnowPaveCm = 0.f, Water = 0.f;
	TMap<FString, float> Tuning;
};
