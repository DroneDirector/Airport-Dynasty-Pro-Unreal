// The effects' own switches and quality (owner, 2026-11-17: "toggles on and off for all of the features, and when on, a
// slider for the quality of that effect"), kept in Settings > Graphics > Effects; saved in ADP.Graphics, applied at the
// start and the moment one changes. Each: on/off, and where it allows it a quality value 0..1 (what it means is in the
// effect's own line in Effects.cpp: how far it reaches, how smooth it moves, how detailed).
#pragma once
#include "CoreMinimal.h"
class UWorld;

namespace ADPEffects
{
	enum EEffect : int32 { LampFlares, Decals, HeatHaze, TaxiwayLights, Adverts, WeatherGround, TextureMemory, ShadowDetail, Upscaler, FastClouds, FastLighting, Count };
	struct FInfo { const TCHAR* Key; const TCHAR* Title; bool bDefaultOn; float DefaultQ; bool bHasSlider; bool bHasSwitch; };
	const FInfo& Info(int32 E);
	bool On(int32 E);
	float Quality(int32 E);                  // 0..1
	FString QualityText(int32 E);            // what the slider's value means, e.g. "600 m", "3000 MB"
	void Set(int32 E, bool bOn, float Q, UWorld* W);
	void Load();
	void Apply(UWorld* W);                    // every effect as set (at the start and after a change)
	// read by the effects themselves
	float FlareReachCm();
	float HazeReachCm();                      // how far the heat haze is drawn (cm)
	float AdvertStep();                       // the LED screens' update interval (s)
}
