#include "GateSigns.h"
#include "TerminalPlan.h"
#include "ADPKeep.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"

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
	// the TAX FREE (16 gates, owner 2026-11-17): over its way in (east) and its way out (west) on the gate walkway, as
	// the walkway's other shop signs are (a black plate, its name in gold as THE BAR's); inside, its zones' hanging
	// boards (both faces) and the pay lounge's wall, as in his renders
	auto Plate = [&](float X, float Y, float Z, float W, float H, float Yaw, const FString& Txt, float Size, const FColor& C, bool bTwoFaces)
	{
		UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(A);
		P->SetStaticMesh(Cube); if (Black) P->SetMaterial(0, Black);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		const bool bAlongX = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(Yaw))) > 0.5f;   // facing along plan z: the plate lies along x
		P->SetWorldLocation(FTerminalPlan::ToWorld(X, Y, Z));
		P->SetWorldScale3D(bAlongX ? FVector(W, 0.06f, H) : FVector(0.06f, W, H));
		P->SetupAttachment(Root); P->RegisterComponent();
		for (int32 F = 0; F < (bTwoFaces ? 2 : 1); ++F)
		{
			const float Yw = Yaw + F * 180.f;
			const FVector Dir = FRotator(0.f, Yw, 0.f).Vector();
			UTextRenderComponent* T = NewObject<UTextRenderComponent>(A);
			T->SetText(FText::FromString(Txt)); T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter);
			T->SetWorldSize(Size); T->SetTextRenderColor(C);
			T->SetWorldLocationAndRotation(FTerminalPlan::ToWorld(X, Y, Z) + Dir * 4.f, FRotator(0.f, Yw, 0.f));
			T->SetupAttachment(Root); T->RegisterComponent();
		}
		++N;
	};
	if (Plan.Rooms.ContainsByPredicate([](const FPlanRoom& R) { return R.Id == TEXT("F2-TAXFREE-W"); }))
	{
		const FColor White(240, 242, 245);
		// (its name over the ways in and out is the neon sign in the model: 3D letters and its logo, owner 2026-11-17)
		Plate(-134.93f, 9.3f, 33.f, 7.f, 1.1f, -90.f, TEXT("BEAUTY"), 70.f, White, true);
		Plate(-163.19f, 9.3f, 31.f, 7.f, 1.1f, -90.f, TEXT("LIQUOR"), 70.f, White, true);
		Plate(-183.81f, 9.6f, 26.f, 9.f, 1.1f, -90.f, TEXT("AURORA PLAZA"), 70.f, White, true);
		Plate(-201.36f, 9.3f, 36.f, 9.f, 1.1f, -90.f, TEXT("ICELAND MARKET"), 70.f, White, true);
		Plate(-226.f, 9.4f, 23.47f, 13.f, 1.0f, -90.f, TEXT("LUXURY & PAY LOUNGE"), 70.f, White, false);
		Plate(-226.f, 8.1f, 23.47f, 4.f, 0.6f, -90.f, TEXT("PAY HERE"), 40.f, White, false);
	}
	// the tax free's neon sign and logo flicker as the title screen's neon does (owner 2026-11-17): steady, then every 4 s two
	// quick dips (styles.css tsFlicker: 35 % at 93 % of the cycle, 50 % at 96 %); each sign on its own beat
	// (the dips are the same pink at 50 % and 35 %: ready-made, swapped in - Tools/make_neon_materials_tf.py)
	UMaterialInterface* Lv[3] = { ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terminal/Materials/MI_T_neonPink.MI_T_neonPink"))),
		ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terminal/Materials/MI_T_neonPink50.MI_T_neonPink50"))),
		ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terminal/Materials/MI_T_neonPink35.MI_T_neonPink35"))) };
	TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> Neons;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		TArray<UInstancedStaticMeshComponent*> Ks; It->GetComponents(Ks);
		for (UInstancedStaticMeshComponent* K : Ks)
			if (K && K->GetStaticMesh() && K->GetStaticMesh()->GetName() == TEXT("tf_neon_sign")) { Neons.Add(K); K->SetAffectDynamicIndirectLighting(false); }   // (its glow is the material and its bloom; in Lumen it lay as a pink patch 10 m wide in gate 9's window glass, 2026-11-23)
	}
	if (Neons.Num() && Lv[0] && Lv[1] && Lv[2])
	{
		static FTimerHandle H;
		TWeakObjectPtr<UWorld> WW(W);
		TArray<TWeakObjectPtr<UMaterialInterface>> L3 = { Lv[0], Lv[1], Lv[2] };
		W->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WW, Neons, L3]()
		{
			if (!WW.IsValid()) return;
			static int32 Last = -1;
			const double P = FMath::Fmod(WW->GetTimeSeconds(), 4.0) / 4.0;
			const int32 Want = (P >= 0.93 && P < 0.94) ? 2 : (P >= 0.96 && P < 0.97) ? 1 : 0;
			if (Want == Last) return;
			Last = Want;
			for (const TWeakObjectPtr<UInstancedStaticMeshComponent>& K : Neons)
				if (K.IsValid() && L3[Want].IsValid()) K->SetMaterial(0, L3[Want].Get());
		}), 0.02f, true);
	}
	UE_LOG(LogTemp, Log, TEXT("Gate signs placed: %d (neon flicker on %d)"), N, Neons.Num());
}
