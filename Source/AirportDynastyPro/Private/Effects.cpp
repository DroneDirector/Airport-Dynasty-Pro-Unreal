#include "Effects.h"
#include "GraphicsMemory.h"
#include "ADPLog.h"
#include "GroundDecals.h"
#include "AirfieldLights.h"
#include "Adverts.h"
#include "Puddles.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Scalability.h"

namespace ADPEffects
{
	static const FInfo GInfo[Count] = {
		// key, title, on by default, quality by default, slider, switch
		{TEXT("LampFlares"), TEXT("Lamp flares at night (glow round street lights and masts)"), false, 0.35f, true, true},
		{TEXT("Decals"), TEXT("Ground marks (oil stains, tyre rubber, repair patches)"), true, 0.5f, true, true},
		{TEXT("HeatHaze"), TEXT("Heat haze behind parked aircraft"), true, 0.4f, true, true},
		{TEXT("TaxiwayLights"), TEXT("Taxiway lights (blue edge and green centre lights)"), true, 0.6f, true, true},
		{TEXT("Adverts"), TEXT("Moving LED advert screens"), true, 1.f, true, true},
		{TEXT("WeatherGround"), TEXT("Weather on the ground (wet paving, puddles, snow)"), true, 1.f, false, true},
		{TEXT("TextureMemory"), TEXT("Texture memory (lower on cards with little video memory)"), true, 0.5f, true, false},
		{TEXT("ShadowDetail"), TEXT("Shadow detail (lower saves video memory)"), true, 0.5f, true, false},
		// (owner, 2026-11-20, to-do 149: frame savers, each on or off live - no restart; ON by default, owner: to save frames)
		{TEXT("Upscaler"), TEXT("Upscaler at Quality (draws 67% of the pixels, sharpened to full size; overrides the resolution slider while on) - more frames per second; highly recommended if your system is struggling"), true, 1.f, false, true},
		{TEXT("FastClouds"), TEXT("Faster clouds (fewer samples through the clouds) - more frames per second; highly recommended if your system is struggling"), true, 1.f, false, true},
		{TEXT("FastLighting"), TEXT("Faster lighting (cheaper Lumen: coarser light gathering, no extra lighting inside glass) - more frames per second; highly recommended if your system is struggling"), true, 1.f, false, true},
	};
	static bool GOn[Count];
	static float GQ[Count];
	static bool GLoaded = false;

	const FInfo& Info(int32 E) { return GInfo[FMath::Clamp(E, 0, Count - 1)]; }
	void Load()
	{
		for (int32 E = 0; E < Count; ++E)
		{
			GOn[E] = GInfo[E].bDefaultOn; GQ[E] = GInfo[E].DefaultQ;
			GConfig->GetBool(TEXT("ADP.Graphics"), GInfo[E].Key, GOn[E], GGameUserSettingsIni);
			GConfig->GetFloat(TEXT("ADP.Graphics"), *(FString(GInfo[E].Key) + TEXT("Q")), GQ[E], GGameUserSettingsIni);
			GQ[E] = FMath::Clamp(GQ[E], 0.f, 1.f);
		}
		GLoaded = true;
	}
	bool On(int32 E) { if (!GLoaded) Load(); return GOn[E]; }
	float Quality(int32 E) { if (!GLoaded) Load(); return GQ[E]; }

	// what each slider means (low end .. high end)
	static float FlareM(float Q) { return 100.f + Q * 1400.f; }        // 100 .. 1500 m
	static float DecalM(float Q) { return 50.f + Q * 750.f; }          // 50 .. 800 m
	static float HazeM(float Q) { return 40.f + Q * 400.f; }           // 40 .. 440 m
	static float TaxiM(float Q) { return 500.f + Q * 4500.f; }         // 500 .. 5000 m
	static float AdHz(float Q) { return 5.f + Q * 25.f; }              // 5 .. 30 updates a second
	static int32 PoolMB(float Q) { return 1000 + FMath::RoundToInt(Q * 4.f) * 500; }   // 1000 .. 3000 MB in 500 steps
	static int32 Pages(float Q) { return Q < 0.34f ? 2048 : Q < 0.67f ? 4096 : 8192; }  // low / medium / high

	FString QualityText(int32 E)
	{
		const float Q = Quality(E);
		switch (E)
		{
		case LampFlares: return FString::Printf(TEXT("%.0f m"), FlareM(Q));
		case Decals: return FString::Printf(TEXT("%.0f m"), DecalM(Q));
		case HeatHaze: return FString::Printf(TEXT("%.0f m"), HazeM(Q));
		case TaxiwayLights: return FString::Printf(TEXT("%.1f km"), TaxiM(Q) / 1000.f);
		case Adverts: return FString::Printf(TEXT("%.0f / s"), AdHz(Q));
		case TextureMemory: return FString::Printf(TEXT("%d MB"), PoolMB(Q));
		case ShadowDetail: return Pages(Q) == 2048 ? TEXT("LOW") : Pages(Q) == 4096 ? TEXT("MEDIUM") : TEXT("HIGH");
		default: return FString();
		}
	}
	float FlareReachCm() { return FlareM(Quality(LampFlares)) * 100.f; }
	float AdvertStep() { return 1.f / AdHz(Quality(Adverts)); }

	void Set(int32 E, bool bOn, float Q, UWorld* W)
	{
		if (!GLoaded) Load();
		GOn[E] = bOn; GQ[E] = FMath::Clamp(Q, 0.f, 1.f);
		GConfig->SetBool(TEXT("ADP.Graphics"), GInfo[E].Key, bOn, GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("ADP.Graphics"), *(FString(GInfo[E].Key) + TEXT("Q")), GQ[E], GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		Apply(W);
	}

	// a console value set while a saver is on; the value it had before is kept and put back when it goes off
	static TMap<FString, FString> GSaved;
	static void Saver(bool bOn, const TCHAR* Name, const TCHAR* Value)
	{
		IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name);
		if (!V) { ADP_WARN(TEXT("Effects: no setting %s in this engine - skipped"), Name); return; }
		if (bOn) { if (!GSaved.Contains(Name)) GSaved.Add(Name, V->GetString()); V->Set(Value, ECVF_SetByGameSetting); }
		else if (const FString* Was = GSaved.Find(Name)) { V->Set(**Was, ECVF_SetByGameSetting); GSaved.Remove(Name); }
	}

	void Apply(UWorld* W)
	{
		if (!GLoaded) Load();
		// the frame savers (live: console values, no restart)
		if (GOn[Upscaler]) Saver(true, TEXT("r.ScreenPercentage"), TEXT("67"));
		else if (GSaved.Contains(TEXT("r.ScreenPercentage"))) { Saver(false, TEXT("r.ScreenPercentage"), nullptr); if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))) V->Set(Scalability::GetQualityLevels().ResolutionQuality, ECVF_SetByGameSetting); }
		static const TCHAR* const Clouds[][2] = {{TEXT("r.VolumetricCloud.ViewRaySampleMaxCount"), TEXT("256")}, {TEXT("r.VolumetricCloud.ReflectionRaySampleMaxCount"), TEXT("24")},
			{TEXT("r.VolumetricCloud.ShadowMap.RaySampleMaxCount"), TEXT("64")}, {TEXT("r.VolumetricCloud.SkyAO"), TEXT("0")}};
		for (const auto& P : Clouds) Saver(GOn[FastClouds], P[0], P[1]);
		static const TCHAR* const Lighting[][2] = {{TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor"), TEXT("32")}, {TEXT("r.Lumen.ScreenProbeGather.TracingOctahedronResolution"), TEXT("6")},
			{TEXT("r.Lumen.TranslucencyVolume.Enable"), TEXT("0")}};
		for (const auto& P : Lighting) Saver(GOn[FastLighting], P[0], P[1]);
		// video memory: the texture pool and the shadow pages (the Cinematic setting reserves 3000 MB and 8192 pages)
		ADPGraphicsMemory::Guard(PoolMB(GQ[TextureMemory]));   // (your setting is the most it may be; less while the card is nearly full - to-do 269)
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.MaxPhysicalPages"))) V->Set(Pages(GQ[ShadowDetail]), ECVF_SetByGameSetting);
		if (!W) return;
		for (TActorIterator<AGroundDecals> It(W); It; ++It)
		{
			TArray<UDecalComponent*> Ds; It->GetComponents(Ds);
			for (UDecalComponent* D : Ds)
			{
				D->SetVisibility(GOn[Decals]);
				// (how far: the screen size it fades out at - 0.002 is about 400 m for a 3 m stain)
				D->SetFadeScreenSize(0.002f * 400.f / DecalM(GQ[Decals]));
			}
		}
		for (TActorIterator<AAirfieldLights> It(W); It; ++It)
		{
			TArray<UInstancedStaticMeshComponent*> Ks; It->GetComponents(Ks);
			const int32 End = FMath::RoundToInt(TaxiM(GQ[TaxiwayLights]) * 100.f);
			for (UInstancedStaticMeshComponent* K : Ks) { K->SetVisibility(GOn[TaxiwayLights]); K->SetCullDistances(FMath::RoundToInt(End * 0.85f), End); }
		}
		for (TActorIterator<AAdverts> It(W); It; ++It)
		{
			TArray<UInstancedStaticMeshComponent*> Ks; It->GetComponents(Ks);
			for (UInstancedStaticMeshComponent* K : Ks) K->SetVisibility(GOn[Adverts]);
			It->SetActorTickEnabled(GOn[Adverts]);
			It->SetActorTickInterval(AdvertStep());
		}
		// the heat haze: the billboards behind the parked aircraft (their material is M_HeatHaze)
		for (TObjectIterator<UMaterialBillboardComponent> It; It; ++It)
		{
			if (It->GetWorld() != W || !It->Elements.Num() || !It->Elements[0].Material || !It->Elements[0].Material->GetName().Contains(TEXT("HeatHaze"))) continue;
			It->SetVisibility(GOn[HeatHaze]);
			It->SetCullDistance(HazeM(GQ[HeatHaze]) * 100.f);
		}
		APuddles::bGroundWeatherOn = GOn[WeatherGround];
		ADP_LOG(TEXT("Effects applied: flares %d, marks %d, haze %d, taxiway lights %d, adverts %d, weather ground %d, textures %d MB, shadow pages %d, upscaler %d, faster clouds %d, faster lighting %d"),
			GOn[LampFlares], GOn[Decals], GOn[HeatHaze], GOn[TaxiwayLights], GOn[Adverts], GOn[WeatherGround], PoolMB(GQ[TextureMemory]), Pages(GQ[ShadowDetail]), GOn[Upscaler], GOn[FastClouds], GOn[FastLighting]);
	}
}
