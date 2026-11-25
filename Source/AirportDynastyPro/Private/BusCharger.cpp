// The bus chargers - see BusCharger.h.
#include "BusCharger.h"
#include "BabylonModel.h"
#include "Sim/ADPVehicleArea.h"
#include "ADPLog.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

namespace
{
	struct FChargerSet { TWeakObjectPtr<UWorld> World; TMap<FString, TWeakObjectPtr<ABabylonModel>> Models; bool bTried = false; };
	FChargerSet& ChargerSet(UWorld* World)
	{
		static FChargerSet S;
		if (S.World.Get() != World) { S = FChargerSet(); S.World = World; }
		return S;
	}
	void ChargerSolid(ABabylonModel* M, const FVector& At, const FVector& Half)   // (cm, the charger's own axes: x its front, y its right)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(M);
		B->SetupAttachment(M->GetRootComponent()); B->SetBoxExtent(Half); B->SetRelativeLocation(At);
		B->SetCollisionProfileName(TEXT("BlockAll")); B->SetGenerateOverlapEvents(false); B->SetHiddenInGame(true); B->RegisterComponent();
		M->AddInstanceComponent(B);
	}
}

void ADPBusCharger::Keep(UWorld* World, TFunctionRef<FVector(float, float, float, float, float&)> Pose)
{
	if (!World) return;
	FChargerSet& S = ChargerSet(World);
	if (!ADPVehicleArea::Built())
	{
		for (auto& P : S.Models) if (ABabylonModel* M = P.Value.Get()) M->Destroy();
		S.Models.Reset(); S.bTried = false; return;
	}
	if (S.bTried) return;
	S.bTried = true;
	for (const ADPVehicleArea::FCharger& C : ADPVehicleArea::Chargers())
	{
		float Yaw = 0.f;
		const float R = FMath::DegreesToRadians(C.Heading);
		const FVector At = Pose(C.X, C.Y, FMath::Cos(R), FMath::Sin(R), Yaw);
		ABabylonModel* M = World->SpawnActor<ABabylonModel>(At, FRotator(0.f, Yaw, 0.f));
		if (!M || !M->Place(TEXT("BusCharger"))) { if (M) M->Destroy(); ADP_WARN(TEXT("Bus charger: the model BusCharger is not there")); return; }
		M->SetActorRotation(FRotator(0.f, Yaw, 0.f));   // (Place turns the root by half a turn)
		ChargerSolid(M, FVector(0.f, 0.f, 7.5f), FVector(37.5f, 50.f, 7.5f));
		ChargerSolid(M, FVector(0.f, 0.f, 108.f), FVector(27.f, 42.f, 94.f));
		ChargerSolid(M, FVector(-4.f, 50.f, 100.f), FVector(22.f, 9.f, 84.f));      // (the saddle and the hanging cable, on its right)
		for (const float Y : { -55.f, 55.f }) ChargerSolid(M, FVector(62.f, Y, 50.f), FVector(7.f, 7.f, 50.f));
		S.Models.Add(C.Bay, M);
	}
	ADP_LOG(TEXT("Bus chargers: %d at the vehicle area"), S.Models.Num());
}

ABabylonModel* ADPBusCharger::Of(UWorld* World, const FString& Bay)
{
	const TWeakObjectPtr<ABabylonModel>* P = World ? ChargerSet(World).Models.Find(Bay) : nullptr;
	return P ? P->Get() : nullptr;
}
