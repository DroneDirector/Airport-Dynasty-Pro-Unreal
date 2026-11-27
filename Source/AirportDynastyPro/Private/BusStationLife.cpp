#include "BusStationLife.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "PerfLog.h"
#include "Site.h"
#include "SunClock.h"
#include "TerminalBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

ABusStationLife::ABusStationLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;   // (a clock with a minute hand: once a second is plenty)
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ABusStationLife* ABusStationLife::Make(ABabylonModel* BusModel)
{
	if (!BusModel || !BusModel->GetWorld() || !BusModel->GetModelRoot())
	{
		ADP_WARN(TEXT("Bus station: no model, so no lights and no clock"));
		return nullptr;
	}
	FString P = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/busstation-lights.json"));
	if (!FPaths::FileExists(P)) P = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/busstation-lights.json"));
	FString Text; TSharedPtr<FJsonObject> J;
	const bool bLights = FFileHelper::LoadFileToString(Text, *P)
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid();
	if (!bLights) ADP_WARN(TEXT("Bus station: no fittings file, so the station stays dark (%s)"), *P);

	USceneComponent* MR = BusModel->GetModelRoot();
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABusStationLife* A = BusModel->GetWorld()->SpawnActor<ABusStationLife>(MR->GetComponentLocation(), MR->GetComponentRotation(), SP);
	if (!A) return nullptr;
	A->AttachToComponent(MR, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	A->Model = BusModel;

	auto Loc = [](double X, double Y, double Z) { return FVector(X, -Y, Z) * 100.0; };   // (model-local Blender metres -> the model's frame, cm)
	auto Num = [](const TSharedPtr<FJsonObject>& O, const TCHAR* F) { return O->GetNumberField(F); };
	const FLinearColor Led(1.f, 0.97f, 0.92f);

	// the lamps: spots down from the vault's discs, the uplights along their own aim, panels over the hall
	int32 NDisc = 0, NUp = 0, NPanel = 0;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (bLights && J->TryGetArrayField(TEXT("fittings"), Arr))
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			const FString Kind = O->GetStringField(TEXT("kind"));
			const FVector At = Loc(Num(O, TEXT("x")), Num(O, TEXT("y")), Num(O, TEXT("z")));
			ULocalLightComponent* L = nullptr;
			float Cd = 0.f, Reach = 0.f, Draw = 0.f;
			if (Kind == TEXT("disc"))
			{
				USpotLightComponent* S = NewObject<USpotLightComponent>(BusModel);
				S->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));   // (straight down on deck, walk and coaches)
				S->SetOuterConeAngle(68.f); S->SetInnerConeAngle(30.f); S->SetSourceRadius(22.f);
				L = S; Cd = 2400.f; Reach = 2200.f; Draw = 25000.f; ++NDisc;
			}
			else if (Kind == TEXT("uplight"))
			{
				USpotLightComponent* S = NewObject<USpotLightComponent>(BusModel);
				FVector Dir(0.0, 0.0, 1.0);
				const TArray<TSharedPtr<FJsonValue>>* Aim = nullptr;
				if (O->TryGetArrayField(TEXT("aim"), Aim) && Aim->Num() >= 3)
					Dir = FVector((*Aim)[0]->AsNumber(), -(*Aim)[1]->AsNumber(), (*Aim)[2]->AsNumber());
				S->SetRelativeRotation(Dir.GetSafeNormal().Rotation());
				S->SetOuterConeAngle(30.f); S->SetInnerConeAngle(11.f); S->SetSourceRadius(6.f);
				L = S; Cd = 650.f; Reach = 1300.f; Draw = 15000.f; ++NUp;
			}
			else if (Kind == TEXT("hallpanel"))
			{
				URectLightComponent* R = NewObject<URectLightComponent>(BusModel);
				R->SetSourceWidth(123.f); R->SetSourceHeight(63.f);
				R->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));   // (facing down into the hall)
				R->SetBarnDoorAngle(88.f);
				L = R; Cd = 900.f; Reach = 1000.f; Draw = 13000.f; ++NPanel;
			}
			else continue;
			BusModel->AddLamp(L, At, Cd, Reach, Led);
			L->MaxDrawDistance = Draw; L->MaxDistanceFadeRange = Draw * 0.25f;
			L->SetVolumetricScatteringIntensity(0.f);   // (no haze cones over the platform)
		}

	// the lamp faces glow (the kit's lamp material on the discs', the uplights' and the panels' lenses)
	if (UMaterialInterface* Lamp = ATerminalBuilder::KitMaterial(A, TEXT("lamp")))
	{
		TArray<UInstancedStaticMeshComponent*> Lists;
		for (const TCHAR* Pre : { TEXT("BusStn_Disc_Lens"), TEXT("BusStn_Uplight_Lens"), TEXT("BusStn_CeilPanel_Lens") })
		{
			TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Found;
			BusModel->InstancesNamed(Pre, true, Found);
			for (const auto& F : Found) if (F.Get<0>()) Lists.AddUnique(F.Get<0>());
		}
		for (UInstancedStaticMeshComponent* C : Lists)
			for (int32 i = 0; i < C->GetNumMaterials(); ++i) C->SetMaterial(i, Lamp);
		ADP_LOG(TEXT("Bus station: %d lamp faces lit"), Lists.Num());
	}
	else ADP_WARN(TEXT("Bus station: no kit lamp material, the lamp faces stay dull"));

	// the clock: the hour and minute hands of both faces, each on its own boss, turned in Tick
	const TCHAR* HandNames[4] = { TEXT("BusStn_Clock_HandHW"), TEXT("BusStn_Clock_HandMW"), TEXT("BusStn_Clock_HandHE"), TEXT("BusStn_Clock_HandME") };
	double MidX = 0.0; int32 NFound = 0;
	FTransform Base[4];
	for (int32 i = 0; i < 4; ++i)
		if (BusModel->NodeBase(HandNames[i], Base[i])) { MidX += Base[i].GetLocation().X; ++NFound; }
	if (NFound > 0) MidX /= (double)NFound;
	for (int32 i = 0; i < 4; ++i)
	{
		FTransform B;
		if (!BusModel->NodeBase(HandNames[i], B)) { ADP_WARN(TEXT("Bus station clock: no %s in the model"), HandNames[i]); continue; }
		const int32 G = BusModel->BindGroup({ FString(HandNames[i]) }, false, FTransform(B.GetLocation()));
		if (G == INDEX_NONE) { ADP_WARN(TEXT("Bus station clock: %s binds to nothing"), HandNames[i]); continue; }
		FHand H;
		H.Group = G;
		H.Pivot = B.GetLocation();
		H.bMinute = (i == 1 || i == 3);
		// which way the face looks (the dial on the +X side of the boss pair reads clockwise the other way round)
		H.Sense = (H.Pivot.X >= MidX) ? -1.f : 1.f;
		A->Hands.Add(H);
	}
	ADP_LOG(TEXT("Bus station: %d disc lamps, %d uplights, %d hall panels, %d clock hands"), NDisc, NUp, NPanel, A->Hands.Num());
	A->Tick(0.f);
	return A;
}

void ABusStationLife::Tick(float Dt)
{
	Super::Tick(Dt);
	ADP_PERF_SCOPE("BusStationLife", 0.1f);
	ABabylonModel* M = Model.Get();
	if (!M || Hands.Num() == 0) return;
	// the game's own time, read fresh every tick (so an overridden or sped-up clock moves the hands)
	const float Hr = ASunClock::HourOverride >= 0.f ? ASunClock::HourOverride
		: (float)ADPSite::LocalNow().GetHour() + ADPSite::LocalNow().GetMinute() / 60.f;
	if (FMath::IsNearlyEqual(Hr, ShownHour, 1.e-4f)) return;
	ShownHour = Hr;
	const float Mins = FMath::Fmod(Hr * 60.f, 60.f);
	const float HourDeg = FMath::Fmod(Hr, 12.f) * 30.f;   // (30 degrees an hour, the minutes' share with it)
	const float MinDeg = Mins * 6.f;                      // (6 degrees a minute)
	for (const FHand& H : Hands)
	{
		const float Deg = H.Sense * (H.bMinute ? MinDeg : HourDeg);
		M->MoveGroup(H.Group, FTransform(FQuat(FVector(1.f, 0.f, 0.f), FMath::DegreesToRadians(Deg)), H.Pivot));
	}
	M->FlushMoves();
}
