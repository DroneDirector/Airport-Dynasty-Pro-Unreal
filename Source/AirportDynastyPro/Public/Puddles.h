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
	TMap<FString, float> Tuning;
};
