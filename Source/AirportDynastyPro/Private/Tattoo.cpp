// Tattoos on the MetaHumans (Tattoo.h).
#include "Tattoo.h"
#include "ADPLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Math/RandomStream.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ADPTattoo
{
	static const int32 Layers = 6;                 // (the skin master's tattoo layers, Tools/set_tattoos.py)

	static TArray<FDesign> GDesigns;
	static TMap<FString, TArray<FString>> GPlaces;
	static TArray<FString> GHeadPlaces;

	static void LoadList()
	{
		if (GDesigns.Num() || GPlaces.Num()) return;
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Data/tattoos.json");
		if (!FFileHelper::LoadFileToString(Text, *Path)) { ADP_WARN(TEXT("Tattoo: %s missing"), *Path); return; }
		TSharedPtr<FJsonObject> J;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J) { ADP_WARN(TEXT("Tattoo: tattoos.json unreadable")); return; }
		const TSharedPtr<FJsonObject>* P = nullptr;
		if (J->TryGetObjectField(TEXT("places"), P))
			for (const auto& KV : (*P)->Values)
			{
				TArray<FString>& L = GPlaces.Add(KV.Key);
				for (const TSharedPtr<FJsonValue>& V : KV.Value->AsArray()) L.Add(V->AsString());
			}
		const TArray<TSharedPtr<FJsonValue>>* H = nullptr;
		if (J->TryGetArrayField(TEXT("head_places"), H))
			for (const TSharedPtr<FJsonValue>& V : *H) GHeadPlaces.Add(V->AsString());
		const TArray<TSharedPtr<FJsonValue>>* D = nullptr;
		if (J->TryGetArrayField(TEXT("designs"), D))
			for (const TSharedPtr<FJsonValue>& V : *D)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject(); if (!O) continue;
				FDesign X; X.Id = O->GetStringField(TEXT("id")); X.Name = O->GetStringField(TEXT("name"));
				X.Kind = O->GetStringField(TEXT("kind")); X.Picture = O->GetStringField(TEXT("picture")); X.bColour = O->GetBoolField(TEXT("colour"));
				GDesigns.Add(X);
			}
		ADP_LOG(TEXT("Tattoo: %d designs, %d kinds of place"), GDesigns.Num(), GPlaces.Num());
	}

	const TArray<FDesign>& Designs() { LoadList(); return GDesigns; }
	TArray<FString> PlacesFor(const FString& Kind) { LoadList(); const TArray<FString>* L = GPlaces.Find(Kind); return L ? *L : TArray<FString>(); }
	bool IsHeadPlace(const FString& Place) { LoadList(); return GHeadPlaces.Contains(Place); }

	static const FDesign* FindDesign(const FString& Id)
	{
		for (const FDesign& D : Designs()) if (D.Id == Id) return &D;
		return nullptr;
	}

	// the person's own copies of their skin materials (not the eyes, teeth, lashes)
	static TArray<UMaterialInstanceDynamic*> SkinOf(USkeletalMeshComponent* Mesh)
	{
		TArray<UMaterialInstanceDynamic*> Out;
		if (!Mesh) return Out;
		for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
		{
			UMaterialInterface* M = Mesh->GetMaterial(i);
			const UMaterial* Base = M ? M->GetMaterial() : nullptr;
			if (!Base || !Base->GetName().Equals(TEXT("M_skin_unified_baked"))) continue;
			UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(M);
			if (!D) D = Mesh->CreateDynamicMaterialInstance(i, M);
			if (D) Out.Add(D);
		}
		return Out;
	}

	static UTexture2D* LoadTex(const FString& PackagePath)
	{
		return LoadObject<UTexture2D>(nullptr, *(PackagePath + TEXT(".") + FPaths::GetBaseFilename(PackagePath)));
	}

	static int32 LayOn(USkeletalMeshComponent* Mesh, const TArray<FWorn>& List)
	{
		const TArray<UMaterialInstanceDynamic*> Skin = SkinOf(Mesh);
		if (!Skin.Num()) return 0;
		static UTexture2D* NoSpot = LoadTex(TEXT("/Game/People/Tattoos/Spots/T_TatSpot_None"));
		static UTexture2D* NoDesign = LoadTex(TEXT("/Game/People/Tattoos/Designs/T_TatDesign_None"));
		int32 N = 0;
		for (const FWorn& W : List)
		{
			if (N >= Layers) { ADP_WARN(TEXT("Tattoo: more than %d on %s - %s left off"), Layers, *Mesh->GetName(), *W.Place); break; }
			const FDesign* D = FindDesign(W.Design);
			UTexture2D* Spot = LoadTex(TEXT("/Game/People/Tattoos/Spots/T_TatSpot_") + W.Place);
			UTexture2D* Pic = D ? LoadTex(D->Picture) : nullptr;
			if (!Spot || !Pic) { ADP_WARN(TEXT("Tattoo: %s:%s - %s missing"), *W.Place, *W.Design, !Spot ? TEXT("place") : TEXT("design")); continue; }
			for (UMaterialInstanceDynamic* M : Skin)
			{
				M->SetTextureParameterValue(*FString::Printf(TEXT("TatSpot_%d"), N), Spot);
				M->SetTextureParameterValue(*FString::Printf(TEXT("TatDesign_%d"), N), Pic);
			}
			++N;
		}
		for (UMaterialInstanceDynamic* M : Skin)
		{
			for (int32 i = N; i < Layers; ++i)
			{
				if (NoSpot) M->SetTextureParameterValue(*FString::Printf(TEXT("TatSpot_%d"), i), NoSpot);
				if (NoDesign) M->SetTextureParameterValue(*FString::Printf(TEXT("TatDesign_%d"), i), NoDesign);
			}
			M->SetScalarParameterValue(TEXT("TattooOn"), N > 0 ? 1.f : 0.f);
		}
		return N;
	}

	int32 Wear(USkeletalMeshComponent* Body, USkeletalMeshComponent* Face, const TArray<FWorn>& List)
	{
		TArray<FWorn> OnBody, OnHead;
		for (const FWorn& W : List) (IsHeadPlace(W.Place) ? OnHead : OnBody).Add(W);
		const int32 N = LayOn(Body, OnBody) + LayOn(Face, OnHead);
		ADP_LOG(TEXT("Tattoo: %s wears %d (%s)"), (Body && Body->GetOwner()) ? *Body->GetOwner()->GetName() : TEXT("-"), N, *Format(List));
		return N;
	}

	TArray<FWorn> Parse(const FString& Text)
	{
		TArray<FWorn> Out; TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& P : Parts)
		{
			FString A, B;
			if (P.TrimStartAndEnd().Split(TEXT(":"), &A, &B)) Out.Add({ A.TrimStartAndEnd(), B.TrimStartAndEnd() });
		}
		return Out;
	}

	FString Format(const TArray<FWorn>& List)
	{
		TArray<FString> S;
		for (const FWorn& W : List) S.Add(W.Place + TEXT(":") + W.Design);
		return FString::Join(S, TEXT(","));
	}

	TArray<FWorn> Saved()
	{
		FString S;
		GConfig->GetString(TEXT("ADP.Player"), TEXT("Tattoos"), S, GGameUserSettingsIni);
		return Parse(S);
	}

	void Save(const TArray<FWorn>& List)
	{
		GConfig->SetString(TEXT("ADP.Player"), TEXT("Tattoos"), *Format(List), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	TArray<FWorn> Add(const FString& Place, const FString& Design)
	{
		// (a place can hold several designs - a rose on the upper arm and ferns on the forearm of the same arm)
		TArray<FWorn> L = Saved();
		if (!L.ContainsByPredicate([&](const FWorn& W) { return W.Place == Place && W.Design == Design; })) L.Add({ Place, Design });
		Save(L);
		return L;
	}

	TArray<FWorn> Remove(const FString& Place, const FString& Design)
	{
		TArray<FWorn> L = Saved();
		L.RemoveAll([&](const FWorn& W) { return W.Place == Place && (Design.IsEmpty() || W.Design == Design); });
		Save(L);
		return L;
	}

	TArray<FWorn> ForPerson(uint32 Seed)
	{
		TArray<FWorn> Out;
		FRandomStream R(int32(Seed * 2654435761u ^ 0x7A77u));
		if (R.FRand() >= 0.3f) return Out;                             // (3 in 10 have tattoos)
		TArray<const FDesign*> Arm, Neck;
		for (const FDesign& D : Designs()) { if (D.Kind == TEXT("arm")) Arm.Add(&D); else if (D.Kind == TEXT("neck")) Neck.Add(&D); }
		const float What = R.FRand();                                  // (an arm, the neck, or both)
		if (Arm.Num() && What < 0.75f)
			Out.Add({ R.FRand() < 0.5f ? FString(TEXT("arm_l")) : FString(TEXT("arm_r")), Arm[R.RandHelper(Arm.Num())]->Id });
		if (Neck.Num() && What >= 0.5f)
		{
			static const TCHAR* NeckPlaces[] = { TEXT("neck_l"), TEXT("neck_r"), TEXT("neck_back"), TEXT("neck_throat") };
			Out.Add({ FString(NeckPlaces[R.RandHelper(4)]), Neck[R.RandHelper(Neck.Num())]->Id });
		}
		return Out;
	}

	int32 Apply(USkeletalMeshComponent* Mesh, const FString& Picture)
	{
		if (!Mesh) return 0;
		UTexture2D* T = LoadObject<UTexture2D>(nullptr, *Picture);
		if (!T) { ADP_WARN(TEXT("Tattoo: %s missing"), *Picture); return 0; }
		int32 N = 0;
		for (UMaterialInstanceDynamic* D : SkinOf(Mesh)) { D->SetTextureParameterValue(TEXT("Tattoo"), T); ++N; }
		ADP_LOG(TEXT("Tattoo: %s on %d skin materials of %s"), *FPaths::GetBaseFilename(Picture), N, *Mesh->GetName());
		return N;
	}
}
