// The airport fire station (FireStation.h).
#include "FireStation.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "TerminalPlan.h"
#include "Engine/World.h"

namespace ADPFireStation
{
	void Place(UWorld* W)
	{
		if (!W) return;
		FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABabylonModel* Station = W->SpawnActor<ABabylonModel>(FTerminalPlan::ToWorld(X, 0.02f, Y), FRotator(0, Yaw, 0), S);
		if (!Station || !Station->Place(TEXT("Firestation"))) { if (Station) Station->Destroy(); ADP_ERROR(TEXT("Fire station: the model is missing (Content/Babylon/Firestation)")); return; }
		Station->AddLights(TEXT("Firestation"));
		// the crash tenders in bays 2 and 3, noses to the doors (the sketch's (bayX, 0, 11.5)); a sketch point (x, y, z) is
		// (-x, z, y) under the model's root, metres to centimetres
		int32 Trucks = 0;
		for (int32 i = 1; i <= 2; ++i)
		{
			const FVector At = Station->GetActorTransform().TransformPosition(FVector(-BayX(i), 11.5f, 0.f) * 100.f);
			ABabylonModel* T = W->SpawnActor<ABabylonModel>(At, FRotator(0, Yaw, 0), S);
			if (T && T->Place(FString::Printf(TEXT("Firetruck%d"), i))) ++Trucks;
			else { if (T) T->Destroy(); ADP_ERROR(TEXT("Fire station: truck %d missing (Content/Babylon/Firetruck%d)"), i, i); }
		}
		// the wash hose (owner, 2026-11-11: part of the fire station, wherever it stands): on the front wall's outside, on
		// the pier between bay doors 4 and 5 (the sketch's (50.5, 0, -0.05)), facing out over the forecourt
		const FVector HoseAt = Station->GetActorTransform().TransformPosition(FVector(-(BayX(3) + BayX(4)) * 0.5f, -0.05f, 0.f) * 100.f);
		ABabylonModel* Hose = W->SpawnActor<ABabylonModel>(HoseAt, FRotator(0, Yaw, 0), S);
		if (!Hose || !Hose->Place(TEXT("WashHose"))) { if (Hose) Hose->Destroy(); ADP_ERROR(TEXT("Fire station: the wash hose is missing (Content/Babylon/WashHose)")); }
		ADP_LOG(TEXT("Fire station at plan (%.0f, %.0f) with %d trucks and the wash hose"), X, Y, Trucks);
	}

	void MapOutline(TArray<FVector4f>& Walls, TArray<FVector4f>& Doors)
	{
		auto Seg = [](TArray<FVector4f>& L, float X0, float Z0, float X1, float Z1) { const FVector2D A = ToPlan(X0, Z0), B = ToPlan(X1, Z1); L.Add(FVector4f(A.X, A.Y, B.X, B.Y)); };
		auto Rect = [&](float X0, float Z0, float X1, float Z1) { Seg(Walls, X0, Z0, X1, Z0); Seg(Walls, X1, Z0, X1, Z1); Seg(Walls, X1, Z1, X0, Z1); Seg(Walls, X0, Z1, X0, Z0); };
		// the apparatus hall: its back and sides, the bay doors along its front
		Seg(Walls, 18.f, 0.f, 18.f, 28.f); Seg(Walls, 18.f, 28.f, 62.5f, 28.f); Seg(Walls, 62.5f, 28.f, 62.5f, 0.f);
		for (int32 i = 0; i < 5; ++i) Seg(Doors, BayX(i) - 2.8f, 0.f, BayX(i) + 2.8f, 0.f);
		for (int32 i = 0; i <= 5; ++i) { const float A = i == 0 ? 18.f : BayX(i - 1) + 2.8f, B = i == 5 ? 62.5f : BayX(i) - 2.8f; if (B > A) Seg(Walls, A, 0.f, B, 0.f); }
		Rect(0.f, 0.f, 18.f, 30.f);            // the two-storey crew block
		Rect(-14.f, 4.f, 0.f, 34.f);           // the charcoal wing
		Rect(-1.5f, -1.4f, 3.5f, 3.6f);        // the glass stair tower at the entrance
		Rect(65.f, 17.f, 71.f, 23.f);          // the training tower's drill shaft
		Rect(71.f, 18.2f, 74.f, 22.4f);        // and its stair shaft
	}
}
