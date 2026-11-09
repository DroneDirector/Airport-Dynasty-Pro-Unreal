// Sets up the terminal test when play starts: sun, sky, fog, the grey-box terminal, the crowd test and
// the player standing at the plan's spawn point.
#include "Misc/PackageName.h"
#include "ADPGameMode.h"
#include "ADPWalker.h"
#include "UI/ADPHUD.h"
#include "TerminalBuilder.h"
#include "TerminalDoors.h"
#include "TerminalFittings.h"
#include "TerminalLifts.h"
#include "TerminalJetways.h"
#include "TerminalScreens.h"
#include "TerminalCrowd.h"
#include "Airfield.h"
#include "AirTraffic.h"
#include "Globe.h"
#include "PhotoTour.h"
#include "BabylonModel.h"
#include "Misc/Paths.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

#pragma region Setup

AADPGameMode::AADPGameMode()
{
	DefaultPawnClass = AADPWalker::StaticClass();
	HUDClass = AADPHUD::StaticClass();
}

void AADPGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	UWorld* W = GetWorld();
	if (!W) return;

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (!UGameplayStatics::GetActorOfClass(W, ADirectionalLight::StaticClass())) SpawnSkyAndLight();

	// the terminal is the Babylon edition's own model when it has been imported (Content/Babylon/Terminal, placed in
	// the map): then Unreal's stand-in pieces and moving aircraft step aside, and the airfield keeps only what lies
	// beyond the model's own site
	// (asked of the asset registry, not the disk: in a release build the model is inside the game's pak files)
	const bool bBabylon = FPackageName::DoesPackageExist(TEXT("/Game/Babylon/Terminal/terminal/StaticMeshes/terminal_mesh_0"));
	FTerminalPlan Plan;
	if (!Plan.Load()) return;
	{
		// the Babylon model, placed as it is
		if (bBabylon)
			if (ABabylonModel* Model = W->SpawnActor<ABabylonModel>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Model->Place(TEXT("Terminal"));
		if (!bBabylon)
		{
			if (ATerminalBuilder* Terminal = W->SpawnActor<ATerminalBuilder>(FVector::ZeroVector, FRotator::ZeroRotator, P)) Terminal->Build();
			if (ATerminalDoors* Doors = W->SpawnActor<ATerminalDoors>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Doors->Build(Plan);
			if (ATerminalFittings* Fittings = W->SpawnActor<ATerminalFittings>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Fittings->Build(Plan);
			if (ATerminalLifts* Lifts = W->SpawnActor<ATerminalLifts>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Lifts->Build(Plan);
			if (ATerminalJetways* Jetways = W->SpawnActor<ATerminalJetways>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Jetways->Build(Plan);
			if (ATerminalScreens* Screens = W->SpawnActor<ATerminalScreens>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Screens->Build(Plan);
		}
		// the airfield round the terminal, and its traffic
		if (AAirfield* Field = W->SpawnActor<AAirfield>(FVector::ZeroVector, FRotator::ZeroRotator, P); Field && Field->Build(bBabylon))
		{
			if (!bBabylon)
				if (AAirTraffic* Traffic = W->SpawnActor<AAirTraffic>(FVector::ZeroVector, FRotator::ZeroRotator, P))
					Traffic->Start(Field->Data());
			// the globe round the airport
			if (AGlobe* Globe = W->SpawnActor<AGlobe>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Globe->Build(Field->Data());
		}
		// the passengers are switched off for now (owner, 2026-11-09); -Crowd brings them back
		if (FParse::Param(FCommandLine::Get(), TEXT("Crowd")))
			if (ATerminalCrowd* Crowd = W->SpawnActor<ATerminalCrowd>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Crowd->Start(Plan, CrowdSize);
		// photo mode, for screenshots of the airport
		if (FParse::Param(FCommandLine::Get(), TEXT("PhotoTour"))) W->SpawnActor<APhotoTour>(FVector::ZeroVector, FRotator::ZeroRotator, P);
		if (!UGameplayStatics::GetActorOfClass(W, APlayerStart::StaticClass()))
			W->SpawnActor<APlayerStart>(FTerminalPlan::ToWorld(Plan.Spawn.X, 1.0f, Plan.Spawn.Z), FRotator(0, -90.f, 0), P); // facing airside
	}
}

#pragma endregion

#pragma region Sky and light

void AADPGameMode::SpawnSkyAndLight()
{
	UWorld* W = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(FVector(0, 0, 5000), FRotator(-35.f, 40.f, 0), P))
	{
		Sun->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			L->SetAtmosphereSunLight(true);
			L->SetIntensity(10.f);
		}
	}
	W->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, P);

	FTransform At(FVector(0, 0, 1000));
	if (ASkyLight* Sky = W->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Sky->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		Sky->GetLightComponent()->bRealTimeCapture = true;
		Sky->FinishSpawning(At);
	}
	W->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	// the weather: volumetric clouds over the airport (the shared world's real weather drives them later)
	W->SpawnActor<AVolumetricCloud>(FVector::ZeroVector, FRotator::ZeroRotator, P);

	if (APostProcessVolume* PP = W->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, P))
	{
		PP->bUnbound = true;
		PP->Settings.bOverride_AutoExposureMinBrightness = true;
		PP->Settings.AutoExposureMinBrightness = -1.f;
		PP->Settings.bOverride_AutoExposureMaxBrightness = true;
		PP->Settings.AutoExposureMaxBrightness = 12.f;
	}
}

#pragma endregion
