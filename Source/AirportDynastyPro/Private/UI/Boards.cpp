// The terminal's timetable screens (the Babylon model's own boards: the big walls, the totems, the wide screens and the
// Operations wall): each board's picture is drawn by the menus with the Babylon edition's own drawing (departures and
// arrivals from the game, or its sample timetable) and handed over here, then shown on every screen of that kind in
// Unreal's copy of the model (their imported materials, fids-<kind><shape>, get the picture; lit, as screens are).
#include "UI/ADPHUD.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/Base64.h"

void AADPHUD::OnBoard(const FString& Key, const FString& B64)
{
	TArray<uint8> Png;
	if (!FBase64::Decode(B64, Png)) return;
	IImageWrapperModule& M = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> W = M.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> Bgra;
	if (!W.IsValid() || !W->SetCompressed(Png.GetData(), Png.Num()) || !W->GetRaw(ERGBFormat::BGRA, 8, Bgra)) return;
	UTexture2D* T = UTexture2D::CreateTransient(W->GetWidth(), W->GetHeight(), PF_B8G8R8A8, FName(*(TEXT("Board_") + Key)));
	if (!T) return;
	T->SRGB = true;
	void* Dst = T->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Dst, Bgra.GetData(), Bgra.Num());
	T->GetPlatformData()->Mips[0].BulkData.Unlock();
	T->UpdateResource();
	BoardTex.Add(Key, T);

	// the screens of this kind: their material slots swapped once for a copy that shows the board
	// (the operations room's monitors: the game's own operations screen, drawn for each monitor's proportions: opsmon169,
	// opsmon219, opsmon329 on the screens MonitorScreen169... of the three kinds, OpsMonitors.h)
	const FString Want = Key.StartsWith(TEXT("opsmon")) ? TEXT("MonitorScreen") + Key.Mid(6) : TEXT("fids-") + Key;
	if (!BoardMats.Contains(Key) || !BoardMats[Key].Num())   // (again until the model is there)
	{
		TArray<TObjectPtr<UMaterialInstanceDynamic>>& L = BoardMats.Add(Key);
		// a gate's own screen (gate1…gate8): its face's material, wherever the model put it
		if (Key.StartsWith(TEXT("gate")))
		{
			const int32 Gate = FCString::Atoi(*Key.Mid(4));
			for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
				if (TObjectPtr<UInstancedStaticMeshComponent>* F = It->GateScreens.Find(Gate))
					if (UInstancedStaticMeshComponent* K = *F)
						for (int32 S = 0; S < K->GetNumMaterials(); ++S)
						{
							UMaterialInterface* Mat = K->GetMaterial(S);
							if (!Mat) continue;
							UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(Mat);
							if (!D) { D = UMaterialInstanceDynamic::Create(Mat, this); K->SetMaterial(S, D); }
							L.Add(D);
						}
		}
		else for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
		{
			TArray<UInstancedStaticMeshComponent*> Parts;
			It->GetComponents(Parts);
			for (UInstancedStaticMeshComponent* K : Parts)
				for (int32 S = 0; S < K->GetNumMaterials(); ++S)
				{
					UMaterialInterface* Mat = K->GetMaterial(S);
					if (!Mat) continue;
					// (a material already copied to be toned down, ToneDown, is known by the one it was copied from)
					const UMaterialInstanceDynamic* Copy = Cast<UMaterialInstanceDynamic>(Mat);
					const UMaterialInterface* Named = Copy && Copy->Parent ? Copy->Parent.Get() : Mat;
					if (!Named->GetName().StartsWith(Want) && !Mat->GetName().StartsWith(Want)) continue;
					UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(Mat);
					if (!D) { D = UMaterialInstanceDynamic::Create(Mat, this); K->SetMaterial(S, D); }
					L.Add(D);
				}
		}
		ADP_LOG(TEXT("Board %s: %dx%d on %d screens"), *Key, W->GetWidth(), W->GetHeight(), L.Num());
	}
	for (UMaterialInstanceDynamic* D : BoardMats[Key])
	{
		if (!D) continue;
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		D->GetAllTextureParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& I : Infos)
		{
			// (the operations monitors: only the colour and the light; the picture as their bumps, metal and shade turned
			// the screen dark in the packaged game)
			const FString P = I.Name.ToString();
			if (Key.StartsWith(TEXT("opsmon")) && !P.Contains(TEXT("baseColor"), ESearchCase::IgnoreCase) && !P.Contains(TEXT("emissive"), ESearchCase::IgnoreCase)) continue;
			D->SetTextureParameterValueByInfo(I, T);   // the colour and the light: the same picture
		}
	}
}
