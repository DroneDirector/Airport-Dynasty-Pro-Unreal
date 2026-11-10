// The terminal's timetable screens (the Babylon model's own boards: the big walls, the totems, the wide screens and the
// Operations wall): each board's picture is drawn by the menus with the Babylon edition's own drawing (departures and
// arrivals from the game, or its sample timetable) and handed over here, then shown on every screen of that kind in
// Unreal's copy of the model (their imported materials, fids-<kind><shape>, get the picture; lit, as screens are).
#include "UI/ADPHUD.h"
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
	const FString Want = TEXT("fids-") + Key;
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
					if (!Mat || !Mat->GetName().StartsWith(Want)) continue;
					UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(Mat);
					if (!D) { D = UMaterialInstanceDynamic::Create(Mat, this); K->SetMaterial(S, D); }
					L.Add(D);
				}
		}
		UE_LOG(LogTemp, Log, TEXT("Board %s: %dx%d on %d screens"), *Key, W->GetWidth(), W->GetHeight(), L.Num());
	}
	for (UMaterialInstanceDynamic* D : BoardMats[Key])
	{
		if (!D) continue;
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		D->GetAllTextureParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& I : Infos) D->SetTextureParameterValueByInfo(I, T);   // the colour and the light: the same picture
	}
}
