// Bilingual signs at a French-speaking site (to-do 170; owner, 2026-11-20 night: French first, English under it, as at
// real airports in Vaud): the terminal model's own word signs (KERB/DEPARTURES-SIGN, the car rental's) get a face of the
// same size laid over them with both languages, and "Bienvenue / Welcome to <the site's airport>" goes up over the main
// entrance - all at run time from the site (never baked: Iceland's terminal keeps its English signs).
#include "BabylonModel.h"
#include "Site.h"
#include "ADPLog.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace ADPBilingual
{
	struct FWords { const TCHAR* Node; const TCHAR* En; };

	/** A board over the box B (world, its face the thin side), the local words big on top, the English under them. */
	static void Face(AActor* Owner, const FTransform& Xf, const FBox& Local, const FString& Lo, const FString& En, bool bWelcome)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Cube) return;
		const FVector C = Local.GetCenter(), E = Local.GetExtent();
		// the thin axis is the sign's face normal (x or y in the part's frame)
		const bool bThinX = E.X < E.Y;
		const float W = (bThinX ? E.Y : E.X) * 2.f, H = E.Z * 2.f, D = (bThinX ? E.X : E.Y) * 2.f + 2.f;
		UStaticMeshComponent* B = NewObject<UStaticMeshComponent>(Owner);
		B->SetStaticMesh(Cube); if (M) B->SetMaterial(0, M);
		B->SetupAttachment(Owner->GetRootComponent()); B->RegisterComponent(); Owner->AddInstanceComponent(B);
		const FVector Scale = bThinX ? FVector(D, W + 2.f, H + 2.f) / 100.f : FVector(W + 2.f, D, H + 2.f) / 100.f;
		B->SetWorldTransform(FTransform(Xf.GetRotation(), Xf.TransformPosition(C), Xf.GetScale3D() * Scale));
		// the words on both faces (whichever way the sign faces you, it reads)
		for (const float S : {1.f, -1.f})
		{
			const FVector N = Xf.TransformVectorNoScale(bThinX ? FVector(S, 0, 0) : FVector(0, S, 0));
			const FVector At = Xf.TransformPosition(C) + N * (D * 0.5f + 1.5f);
			const FRotator R = (N).Rotation();
			auto Line = [&](const FString& T, float Z, float Size, FColor Col)
			{
				UTextRenderComponent* Tx = NewObject<UTextRenderComponent>(Owner);
				Tx->SetText(FText::FromString(T)); Tx->SetHorizontalAlignment(EHTA_Center); Tx->SetVerticalAlignment(EVRTA_TextCenter);
				Tx->SetWorldSize(Size); Tx->SetTextRenderColor(Col);
				Tx->SetupAttachment(Owner->GetRootComponent()); Tx->RegisterComponent(); Owner->AddInstanceComponent(Tx);
				Tx->SetWorldLocationAndRotation(At + FVector(0, 0, Z), R);
			};
			const FColor Navy(14, 30, 58);
			const float Hs = H * Xf.GetScale3D().Z;
			Line(Lo, Hs * 0.17f, Hs * (bWelcome ? 0.36f : 0.42f), Navy);
			Line(En, -Hs * 0.24f, Hs * 0.27f, FColor(40, 60, 90));
		}
	}

	/** A group node's children's names, from the model's node list (Content/Data or Data/babylon-terminal.json). */
	static TArray<FString> ChildrenOf(const FString& Name)
	{
		static TSharedPtr<FJsonObject> J;
		if (!J)
		{
			FString T;
			for (const FString& P : {FPaths::ProjectContentDir() / TEXT("Data/babylon-terminal.json"), FPaths::ProjectDir() / TEXT("Data/babylon-terminal.json")})
				if (FFileHelper::LoadFileToString(T, *P) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T), J) && J) break;
		}
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* Nodes;
		if (!J || !J->TryGetArrayField(TEXT("nodes"), Nodes)) return Out;
		for (const TSharedPtr<FJsonValue>& V : *Nodes)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject(); FString N;
			if (!O || !O->TryGetStringField(TEXT("name"), N) || N != Name) continue;
			const TArray<TSharedPtr<FJsonValue>>* Ch;
			if (O->TryGetArrayField(TEXT("children"), Ch))
				for (const TSharedPtr<FJsonValue>& C : *Ch) { const int32 I = int32(C->AsNumber()); if (Nodes->IsValidIndex(I)) { FString CN; (*Nodes)[I]->AsObject()->TryGetStringField(TEXT("name"), CN); Out.Add(CN); } }
			break;
		}
		return Out;
	}

	void Apply(UWorld* W)
	{
		if (!W || ADPSite::SignLanguage().IsEmpty()) return;
		ABabylonModel* T = nullptr;
		for (TActorIterator<ABabylonModel> It(W); It; ++It) if (It->Model == TEXT("Terminal")) { T = *It; break; }
		if (!T) return;
		const FWords List[] = {{TEXT("KERB/DEPARTURES-SIGN"), TEXT("DEPARTURES")}, {TEXT("RENTAL/SIGN-CAR-RENTAL"), TEXT("CAR RENTAL")}};
		int32 N = 0;
		for (const FWords& Wd : List)
		{
			// (the sign is a group: its parts are its children in the model's node list - Data/babylon-terminal.json)
			TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> In;
			for (const FString& Child : ChildrenOf(Wd.Node)) T->InstancesNamed(Child, false, In);
			// (the sign's biggest part is its board: one face over it)
			int32 Best = INDEX_NONE; double BestV = 0.0;
			for (int32 k = 0; k < In.Num(); ++k)
			{
				UInstancedStaticMeshComponent* C = In[k].Get<0>(); FTransform X;
				if (!C || !C->GetStaticMesh() || !C->GetInstanceTransform(In[k].Get<1>(), X, true)) continue;
				const FVector Sz = C->GetStaticMesh()->GetBoundingBox().GetSize() * X.GetScale3D().GetAbs();
				const double V = Sz.X * Sz.Y + Sz.Y * Sz.Z + Sz.X * Sz.Z;
				if (V > BestV) { BestV = V; Best = k; }
			}
			if (Best != INDEX_NONE)
			{
				const auto& I = In[Best];
				UInstancedStaticMeshComponent* C = I.Get<0>(); FTransform Xf;
				C->GetInstanceTransform(I.Get<1>(), Xf, true);
				const FBox Local = C->GetStaticMesh()->GetBoundingBox();
				const FString En = Wd.En, Lo = ADPSite::Translate(En);
				// the sign's old English letters (its smaller parts lying on the board's face) tucked away, so only the new
				// two-language face reads - the posts and brackets reaching past the board stay (2026-11-21: "DÉPARTS" showed
				// through behind the old "DEPARTURES")
				{
					const FVector BE = Local.GetExtent(), BC = Local.GetCenter();
					const bool bThinX = BE.X < BE.Y;
					for (int32 k = 0; k < In.Num(); ++k)
					{
						if (k == Best) continue;
						UInstancedStaticMeshComponent* P = In[k].Get<0>(); FTransform Px;
						if (!P || !P->GetStaticMesh() || !P->GetInstanceTransform(In[k].Get<1>(), Px, true)) continue;
						const FVector Pc = Xf.InverseTransformPosition(Px.TransformPosition(P->GetStaticMesh()->GetBoundingBox().GetCenter())) - BC;
						const double Across = bThinX ? FMath::Abs(Pc.Y) : FMath::Abs(Pc.X), Wide = bThinX ? BE.Y : BE.X;
						if (Across > Wide * 1.02 || FMath::Abs(Pc.Z) > BE.Z * 1.02) continue;   // (outside the board: a post or bracket)
						Px.SetScale3D(FVector(0.001f)); P->UpdateInstanceTransform(In[k].Get<1>(), Px, true, true, true);
					}
				}
				Face(T, Xf, Local, Lo, En, false); ++N;
				// the welcome, a little above the departures sign over the main entrance
				if (En == TEXT("DEPARTURES"))
				{
					FBox Up = Local.ShiftBy(FVector(0, 0, Local.GetSize().Z * 1.5f));
					const FString Name = ADPSite::Name(ADPSite::Id()).ToUpper();
					Face(T, Xf, Up, TEXT("BIENVENUE À ") + Name, TEXT("WELCOME TO ") + Name, true); ++N;
				}
			}
		}
		ADP_LOG(TEXT("Signs: %d bilingual faces (%s / English)"), N, *ADPSite::SignLanguage());
	}
}
