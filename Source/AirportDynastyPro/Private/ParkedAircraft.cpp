// The aircraft parked at the terminal's gates (ParkedAircraft.h).
#include "ParkedAircraft.h"
#include "Components/MaterialBillboardComponent.h"
#include "ADPKeep.h"
#include "ADPLog.h"
#include "Airfield.h"
#include "BabylonModel.h"
#include "TerminalPlan.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

namespace ADPParked
{
	bool HaveModel()
	{
		return FPackageName::DoesPackageExist(TEXT("/Game/Babylon/A320/a320/StaticMeshes/A320"));
	}

	int32 Place(UWorld* W, const FAirfieldData& Field)
	{
		if (!W || !HaveModel()) return 0;
		FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		int32 N = 0;
		for (const FAirfieldData::FStand& St : Field.Stands)
		{
			// the terminal's narrow-body stands (code C: 36 m span), not the wide-body stand or the cargo stands
			if (St.bCargo || St.Span > 40.f) continue;
			// its nose tyre on the stop bar, on the lead-in line; the nose toward the terminal (the stand's Dir)
			const float B = -St.Dir;                             // (from the nose back towards the tail: Airfield.cpp)
			const float Gx = St.X, Gy = St.Nose + B * NoseGearBehindNose;
			// the apron's surface under it: a look straight down at the nose tyre and between the main tyres (12.8 m
			// behind), the higher of the two (its tyres stand on it, never in it)
			auto Surface = [&](float X, float Y)
			{
				FHitResult Hit;
				const FVector From = FTerminalPlan::ToWorld(X, 30.f, Y), To = FTerminalPlan::ToWorld(X, -30.f, Y);
				FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPParked), true);
				return W->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q) ? Hit.ImpactPoint.Z / 100.0 : 0.0;
			};
			double Z = FMath::Max(Surface(Gx, Gy), Surface(Gx, Gy + B * 12.8f));
			if (FMath::Abs(Z) > 0.5) Z = 0.02;   // (something else met - a jetway: the apron is at about 0 there)
			// turned so the model's +y (its nose) runs along the stand toward the terminal: the loader puts a Blender
			// model's +y on the actor's right (BabylonModel.h); plan y is the world's -y
			const FVector Nose(0.0, -St.Dir, 0.0);             // (world: plan (0, Dir))
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Nose.X, Nose.Y));
			ABabylonModel* M = W->SpawnActor<ABabylonModel>(FTerminalPlan::ToWorld(Gx, 0.f, Gy) + FVector(0, 0, Z * 100.0), FRotator(0, Yaw, 0), S);
			if (!M || !M->Place(TEXT("A320"))) { if (M) M->Destroy(); ADP_ERROR(TEXT("Parked aircraft: the A320 is missing (Tools/import_model.py)")); return N; }
			M->Tags.Add(FName(*(St.Id + TEXT("/A320"))));
			// heat haze behind the APU's exhaust at the tip of the tail cone (to-do 22b: the APU runs while it stands at the
			// gate): a shimmer of hot air, rising (M_HeatHaze). The model's nose is the actor's +Y; the A320 is 37.6 m long,
			// its nose tyre 5.1 m behind the nose, so the exhaust is about 32.5 m behind the nose tyre, 5 m up
			static UMaterialInterface* Haze = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Effects/M_HeatHaze.M_HeatHaze")));
			if (Haze)
			{
				UMaterialBillboardComponent* Hz = NewObject<UMaterialBillboardComponent>(M);
				Hz->SetupAttachment(M->GetRootComponent());
				Hz->SetRelativeLocation(FVector(0.f, 3520.f, 500.f));   // (the loader turns the model half round on the actor: its tail is the actor's +Y)
				Hz->AddElement(Haze, nullptr, false, 210.f, 170.f, nullptr);
				Hz->SetCastShadow(false);
				Hz->RegisterComponent();
			}
			++N;
		}
		ADP_LOG(TEXT("Parked aircraft: %d A320s at the terminal's stands, nose tyres on their stop bars"), N);
		return N;
	}
}
