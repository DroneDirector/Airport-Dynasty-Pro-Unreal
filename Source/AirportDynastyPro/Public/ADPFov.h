// The field of view (owner, 2026-11-18): ONE slider for every view - walking or driving, first person or from behind -
// unless "separate for each view" is switched on, then each view has its own slider. Degrees side to side as on a 16:9
// screen; wider screens see more to the sides (the angle up and down stays the same). Saved in ADP.Graphics in
// GameUserSettings: Fov (all views - the old walking slider's key, so its value carries over), FovSplit, FovWalk1,
// FovWalk3, FovDrive1, FovDrive3.
#pragma once
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

namespace ADPFov
{
	enum EView { Walk1, Walk3, Drive1, Drive3, NumViews };
	inline float All = 90.f;
	inline bool bSplit = false;
	inline float Each[NumViews] = { 90.f, 90.f, 90.f, 80.f };
	inline const TCHAR* Key[NumViews] = { TEXT("FovWalk1"), TEXT("FovWalk3"), TEXT("FovDrive1"), TEXT("FovDrive3") };
	inline const TCHAR* Name[NumViews] = { TEXT("Walking, first person"), TEXT("Walking, from behind"), TEXT("Driving, from the cab"), TEXT("Driving, from behind") };
	inline bool bRead = false;
	constexpr float Min = 60.f, Max = 120.f;

	inline void Read()
	{
		if (bRead) return; bRead = true;
		GConfig->GetFloat(TEXT("ADP.Graphics"), TEXT("Fov"), All, GGameUserSettingsIni); All = FMath::Clamp(All, Min, Max);
		GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("FovSplit"), bSplit, GGameUserSettingsIni);
		for (int32 i = 0; i < NumViews; ++i) { if (!GConfig->GetFloat(TEXT("ADP.Graphics"), Key[i], Each[i], GGameUserSettingsIni)) Each[i] = i == Drive3 ? FMath::Min(All, 80.f) : All; Each[i] = FMath::Clamp(Each[i], Min, Max); }
	}
	inline void Save()
	{
		GConfig->SetFloat(TEXT("ADP.Graphics"), TEXT("Fov"), All, GGameUserSettingsIni);
		GConfig->SetBool(TEXT("ADP.Graphics"), TEXT("FovSplit"), bSplit, GGameUserSettingsIni);
		for (int32 i = 0; i < NumViews; ++i) GConfig->SetFloat(TEXT("ADP.Graphics"), Key[i], Each[i], GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	/** The chosen degrees for a view (16:9 side to side). */
	inline float Get(EView V) { Read(); return bSplit ? Each[V] : All; }
	/** What the camera needs on this screen: the up-and-down angle held to the 16:9 one, the sides opened for wider screens. */
	inline float ForScreen(EView V)
	{
		FVector2D Size(16, 9);
		if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Size);
		const float Aspect = Size.Y > 1 ? FMath::Max(Size.X / Size.Y, 1.f) : 16.f / 9.f;
		const float Vt = 2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Get(V)) * 0.5f) / (16.f / 9.f));
		return FMath::Min(170.f, FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(Vt * 0.5f) * Aspect)));
	}
}
