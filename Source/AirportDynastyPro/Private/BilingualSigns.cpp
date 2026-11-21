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
			TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> In; T->InstancesNamed(Wd.Node, false, In);
			for (const auto& I : In)
			{
				UInstancedStaticMeshComponent* C = I.Get<0>(); FTransform Xf;
				if (!C || !C->GetStaticMesh() || !C->GetInstanceTransform(I.Get<1>(), Xf, true)) continue;
				const FBox Local = C->GetStaticMesh()->GetBoundingBox();
				const FString En = Wd.En, Lo = ADPSite::Translate(En);
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
