#include "GateSigns.h"
#include "TerminalPlan.h"
#include "ADPKeep.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

void ADPGateSigns::Place(UWorld* W, const FTerminalPlan& Plan)
{
	if (!W) return;
	UStaticMesh* Cube = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	UMaterialInterface* Black = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terminal/Materials/MI_T_black.MI_T_black")));
	if (!Cube) return;
	AActor* A = W->SpawnActor<AActor>();
	if (!A) return;
	USceneComponent* Root = NewObject<USceneComponent>(A, TEXT("Root"));
	A->SetRootComponent(Root); Root->RegisterComponent();
	int32 N = 0;
	for (const FPlanRoom& R : Plan.Rooms)
	{
		const bool bGate = R.Id.StartsWith(TEXT("F2-GATE-")), bView = R.Id.StartsWith(TEXT("F2-VIEW-"));
		if (!bGate && !bView) continue;
		const int32 Num = FCString::Atoi(*R.Id.RightChop(8));
		if (Num < 9) continue;                                   // gates 1-8: the model's own signs
		// over the opening onto the walkway (gate room: 8 m at x0 + 12; view bay: 10 m at x0 + 17), facing the walkway
		const float X = R.X0 + (bGate ? 12.f : 17.f), Y = 6.f + 3.62f + 0.25f, Z = R.Z0 - 0.13f;
		UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(A);
		P->SetStaticMesh(Cube); if (Black) P->SetMaterial(0, Black);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetWorldLocation(FTerminalPlan::ToWorld(X, Y, Z));
		P->SetWorldScale3D(FVector(bGate ? 0.016f : 0.022f, 0.0006f, 0.0042f) * 100.f);
		P->SetupAttachment(Root); P->RegisterComponent();
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(A);
		T->SetText(FText::FromString(bGate ? FString::Printf(TEXT("Gate %d"), Num) : FString::Printf(TEXT("View bay %d"), Num)));
		T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(22.f); T->SetTextRenderColor(FColor(240, 242, 245));
		T->SetWorldLocationAndRotation(FTerminalPlan::ToWorld(X, Y, Z - 0.04f), FRotator(0.f, 90.f, 0.f));
		T->SetupAttachment(Root); T->RegisterComponent();
		++N;
	}
	UE_LOG(LogTemp, Log, TEXT("Gate signs placed: %d"), N);
}
