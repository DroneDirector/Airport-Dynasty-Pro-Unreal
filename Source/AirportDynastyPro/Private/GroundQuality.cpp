#include "GroundQuality.h"
#include "ADPLog.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectIterator.h"

namespace ADPGroundQuality
{
	int32 Level()
	{
		int32 L = 2; GConfig->GetInt(TEXT("ADP.Graphics"), TEXT("AirportGround"), L, GGameUserSettingsIni);
		return FMath::Clamp(L, 0, 3);
	}

	const TCHAR* Name(int32 L)
	{
		static const TCHAR* N[] = {TEXT("LOW (2K)"), TEXT("MEDIUM"), TEXT("HIGH (4K)"), TEXT("ULTRA (4K + FINE RELIEF)")};
		return N[FMath::Clamp(L, 0, 3)];
	}

	void Apply(UWorld* W)
	{
		const int32 L = Level();
		// the scans (/Game/Ground/Scans: 4K pictures) one step smaller where the level asks for it
		int32 N = 0;
		for (TObjectIterator<UTexture2D> It; It; ++It)
		{
			const FString P = It->GetPathName();
			if (!P.StartsWith(TEXT("/Game/Ground/Scans/"))) continue;
			const bool bColour = P.Contains(TEXT("_diff"));
			const int32 Bias = L == 0 ? 1 : L == 1 ? (bColour ? 0 : 1) : 0;
			if (It->LODBias != Bias) { It->LODBias = Bias; It->UpdateCachedLODBias(); It->UpdateResource(); }
			++N;
		}
		if (W)
			if (UMaterialParameterCollection* C = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Weather/MPC_Weather.MPC_Weather")))
			{
				UKismetMaterialLibrary::SetScalarParameterValue(W, C, TEXT("Relief"), 0.6f);
				UKismetMaterialLibrary::SetScalarParameterValue(W, C, TEXT("FineRelief"), L >= 3 ? 1.f : 0.f);
			}
		ADP_LOG(TEXT("Airport ground: %s (%d ground pictures)"), Name(L), N);
		// once more a little later: some ground pictures only load when their part of the airport comes into view
		static bool bAgain = false;
		if (W && !bAgain)
		{
			bAgain = true; FTimerHandle T; TWeakObjectPtr<UWorld> WW(W);
			W->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([WW]() { if (WW.IsValid()) Apply(WW.Get()); }), 20.f, false);
		}
	}

	void Set(int32 L, UWorld* W)
	{
		GConfig->SetInt(TEXT("ADP.Graphics"), TEXT("AirportGround"), FMath::Clamp(L, 0, 3), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		Apply(W);
	}
}
