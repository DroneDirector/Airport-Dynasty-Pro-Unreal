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
#include "Materials/Material.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#if WITH_EDITOR
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#endif

// A timetable screen sunk inside its own housing (the true-Unreal terminal's copy came out mirrored: the screen 5 cm in front
// of its 12 cm box in the Babylon edition ended up inside it - the departures wall at Chablais showed as a dark panel,
// 2026-11-21): a screen with something right in front of it on BOTH sides is moved to 1.5 cm in front of the nearer one
// (the housing's face), with a copy turned round, so whichever way the flat screen faces, one of them faces the hall.
// A screen that is seen from one side already (in front of a wall) is left as it is.
static void BdBringOut(UWorld* W, UInstancedStaticMeshComponent* K)
{
	if (!W || !K || !K->GetStaticMesh() || K->ComponentTags.Contains(TEXT("BoardOut"))) return;
	K->ComponentTags.Add(TEXT("BoardOut"));
	const FBox Lb = K->GetStaticMesh()->GetBoundingBox();
	const int32 N0 = K->GetInstanceCount();
	int32 Moved = 0;
	for (int32 i = 0; i < N0; ++i)
	{
		FTransform X; if (!K->GetInstanceTransform(i, X, true)) continue;
		const FVector E = Lb.GetExtent() * X.GetScale3D().GetAbs();
		const FVector Ax = E.X <= E.Y && E.X <= E.Z ? FVector::XAxisVector : (E.Y <= E.Z ? FVector::YAxisVector : FVector::ZAxisVector);
		const FVector Nrm = X.TransformVectorNoScale(Ax).GetSafeNormal(), C = X.TransformPosition(Lb.GetCenter());
		FCollisionQueryParams Q(TEXT("BoardOut"), false); Q.AddIgnoredComponent(K);
		float Near[2] = {-1.f, -1.f};
		for (int32 sd = 0; sd < 2; ++sd)
		{
			const FVector Dir = sd ? -Nrm : Nrm; FHitResult H;
			if (W->LineTraceSingleByChannel(H, C + Dir * 40.f, C, ECC_Visibility, Q)) Near[sd] = (H.ImpactPoint - C).Size();
		}
		if (Near[0] < 0.f || Near[1] < 0.f) continue;   // (seen from one side: fine)
		const int32 sd = Near[0] <= Near[1] ? 0 : 1; const FVector Dir = sd ? -Nrm : Nrm;
		const FVector Shift = Dir * (Near[sd] + 1.5f);
		X.AddToTranslation(Shift);
		// (the mirrored copy also turned the picture upside down: the screen's height axis flipped back, round its middle)
		{
			int32 Up = 0; double Best = -1.0;
			for (int32 a = 0; a < 3; ++a) { const double Zc = FMath::Abs(X.TransformVectorNoScale(a == 0 ? FVector::XAxisVector : a == 1 ? FVector::YAxisVector : FVector::ZAxisVector).Z); if (Zc > Best) { Best = Zc; Up = a; } }
			FVector Sc = X.GetScale3D(); Sc[Up] = -Sc[Up]; X.SetScale3D(Sc);
			X.SetTranslation(X.GetTranslation() + (C + Shift) - X.TransformPosition(Lb.GetCenter()));
		}
		K->UpdateInstanceTransform(i, X, true, false, true);
		FTransform Y = X; Y.SetRotation(FQuat(Nrm.GetSafeNormal().Cross(FVector::UpVector).IsNearlyZero() ? FVector::ForwardVector : FVector::UpVector, PI) * X.GetRotation());
		// (the turned copy turns round the screen's own middle)
		Y.SetTranslation(X.GetTranslation() + (C + Shift) - Y.TransformPosition(Lb.GetCenter()));
		K->AddInstance(Y, true); ++Moved;
	}
	K->MarkRenderStateDirty();
	if (Moved) ADP_LOG(TEXT("Boards: %d screen(s) of %s brought out of their housing"), Moved, *K->GetStaticMesh()->GetName());
}

void AADPHUD::OnBoard(const FString& Key, const FString& B64)
{
	TArray<uint8> Png;
	if (!FBase64::Decode(B64, Png)) return;
	// (-BoardDump: every board's picture also written to Saved/Boards/<kind>.png, to check what the menus drew)
	if (FParse::Param(FCommandLine::Get(), TEXT("BoardDump"))) FFileHelper::SaveArrayToFile(Png, *(FPaths::ProjectSavedDir() / TEXT("Boards") / (Key + TEXT(".png"))));
	IImageWrapperModule& M = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> W = M.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> Bgra;
	if (!W.IsValid() || !W->SetCompressed(Png.GetData(), Png.Num()) || !W->GetRaw(ERGBFormat::BGRA, 8, Bgra)) return;
	// the same board again at the same size: its new pixels written into the picture it already has (a new picture every
	// refresh made the game wait for the old one's cleanup - a stall of up to 0.4 s every few seconds, the title film
	// stuttering; owner 2026-11-15)
	if (TObjectPtr<UTexture2D>* Old = BoardTex.Find(Key))
		if (UTexture2D* OT = Old->Get())
			if (OT->GetSizeX() == W->GetWidth() && OT->GetSizeY() == W->GetHeight() && OT->GetResource())
			{
				const int32 Wd = W->GetWidth(), Ht = W->GetHeight();
				uint8* Copy = (uint8*)FMemory::Malloc(Bgra.Num()); FMemory::Memcpy(Copy, Bgra.GetData(), Bgra.Num());
				FUpdateTextureRegion2D* R = new FUpdateTextureRegion2D(0, 0, 0, 0, Wd, Ht);
				OT->UpdateTextureRegions(0, 1, R, Wd * 4, 4, Copy, [](uint8* Data, const FUpdateTextureRegion2D* Rg) { FMemory::Free(Data); delete Rg; });
				return;
			}
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
					// (the true-Unreal terminal's materials are MI_T_<the same name>)
					const auto Plain = [](const FString& N) { return N.StartsWith(TEXT("MI_T_")) ? N.RightChop(5) : N; };
					if (!Plain(Named->GetName()).StartsWith(Want) && !Plain(Mat->GetName()).StartsWith(Want)) continue;
					UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(Mat);
					if (!D) { D = UMaterialInstanceDynamic::Create(Mat, this); K->SetMaterial(S, D); }
					L.Add(D);
					BdBringOut(GetWorld(), K);
#if WITH_EDITOR
					// (-BoardDump: the screen part's texture coordinates - all at one point shows one pixel of the picture)
					if (FParse::Param(FCommandLine::Get(), TEXT("BoardDump")) && K->GetStaticMesh())
						if (const FMeshDescription* MD = K->GetStaticMesh()->GetMeshDescription(0))
						{
							FStaticMeshConstAttributes A(*MD); const auto UV = A.GetVertexInstanceUVs();
							FBox2f B(ForceInit); for (const FVertexInstanceID V : MD->VertexInstances().GetElementIDs()) if (UV.GetNumChannels() > 0) B += UV.Get(V, 0);
							FTransform X0; K->GetInstanceTransform(0, X0, true);
							ADP_LOG(TEXT("Board %s part %s (%s, %d instances, first at %s): uv %s, slot %d of %d"), *Key, *K->GetName(), *K->GetStaticMesh()->GetName(), K->GetInstanceCount(), *X0.GetLocation().ToString(), *B.ToString(), S, K->GetNumMaterials());
							const FBox Lb = K->GetStaticMesh()->GetBoundingBox();
							ADP_LOG(TEXT("Board %s part size %s, rot %s"), *Key, *(Lb.GetSize() * X0.GetScale3D()).ToString(), *X0.Rotator().ToString());
							const FVector C0 = X0.TransformPosition(Lb.GetCenter());
							for (const FVector Dir : {FVector::XAxisVector, -FVector::XAxisVector, FVector::YAxisVector, -FVector::YAxisVector})
							{
								TArray<FHitResult> Hits; GetWorld()->LineTraceMultiByChannel(Hits, C0 + Dir * 900.f, C0 - Dir * 20.f, ECC_Visibility);
								FHitResult H1; const bool b1 = GetWorld()->LineTraceSingleByChannel(H1, C0 + Dir * 900.f, C0 - Dir * 20.f, ECC_Visibility);
								ADP_LOG(TEXT("Board %s seen from %s: first hit %s at %.0f cm from the screen"), *Key, *Dir.ToString(), b1 && H1.GetComponent() ? *(H1.GetComponent()->GetName() + TEXT("/") + (Cast<UStaticMeshComponent>(H1.GetComponent()) && Cast<UStaticMeshComponent>(H1.GetComponent())->GetStaticMesh() ? Cast<UStaticMeshComponent>(H1.GetComponent())->GetStaticMesh()->GetName() : FString())) : TEXT("nothing"), b1 ? (H1.ImpactPoint - C0).Size() : -1.f);
							}
						}
#endif
				}
		}
		ADP_LOG(TEXT("Board %s: %dx%d on %d screens"), *Key, W->GetWidth(), W->GetHeight(), L.Num());
		// (-BoardDump: each screen's material and its picture slots, to see why a screen stays dark)
		if (FParse::Param(FCommandLine::Get(), TEXT("BoardDump")))
			for (UMaterialInstanceDynamic* D : L) if (D)
			{
				TArray<FMaterialParameterInfo> Infos; TArray<FGuid> Ids; D->GetAllTextureParameterInfo(Infos, Ids);
				FString Ns; for (const FMaterialParameterInfo& I : Infos) Ns += I.Name.ToString() + TEXT(" ");
				ADP_LOG(TEXT("Board %s screen: %s (parent %s, base %s) textures: %s"), *Key, *D->GetName(), D->Parent ? *D->Parent->GetName() : TEXT("-"), D->GetBaseMaterial() ? *D->GetBaseMaterial()->GetName() : TEXT("-"), *Ns);
			}
	}
	for (UMaterialInstanceDynamic* D : BoardMats[Key])
	{
		if (!D) continue;
		// (the true-Unreal terminal's lit screens, M_TUnlit: picture x BC + glow x EC - BC was the import's dark "screen off"
		// colour, so the picture showed nearly black (Chablais, 2026-11-21): the picture at its own colour, no second glow)
		if (const UMaterial* Bm = D->GetBaseMaterial()) if (Bm->GetName().StartsWith(TEXT("M_TUnlit")))
		{
			D->SetVectorParameterValue(TEXT("BC"), FLinearColor::White); D->SetVectorParameterValue(TEXT("EC"), FLinearColor::Black);
			float Eb = 1.f; D->GetScalarParameterValue(FMaterialParameterInfo(TEXT("EB")), Eb); if (Eb < 1.f) D->SetScalarParameterValue(TEXT("EB"), 1.f);
		}
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		D->GetAllTextureParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& I : Infos)
		{
			// (the operations monitors: only the colour and the light; the picture as their bumps, metal and shade turned
			// the screen dark in the packaged game)
			const FString P = I.Name.ToString();
			if (Key.StartsWith(TEXT("opsmon")) && !P.Contains(TEXT("baseColor"), ESearchCase::IgnoreCase) && !P.Contains(TEXT("emissive"), ESearchCase::IgnoreCase)) continue;
			if (P == TEXT("Nrm") || P == TEXT("Det")) continue;   // (the true-Unreal materials: the picture is colour and light only)
			D->SetTextureParameterValueByInfo(I, T);   // the colour and the light: the same picture
		}
	}
}
