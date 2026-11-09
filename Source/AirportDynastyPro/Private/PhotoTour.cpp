// Photo mode: set views of the airport, each a picture without the interface, then the game closes.
#include "PhotoTour.h"
#include "TerminalPlan.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"

#pragma region Views
namespace
{
	struct FView { const TCHAR* Name; FVector From, To; }; // plan metres: x, height, z
	const FView Views[] = {
		{TEXT("Terminal_Back"),  FVector(300, 110, 330), FVector(80, 5, 60)},
		{TEXT("Terminal_Front"), FVector(-90, 75, -170), FVector(75, 8, 15)},
		{TEXT("Jetways"),        FVector(58, 12, 120),   FVector(28, 4, 95)},
	};
	constexpr float Settle = 25.f, Each = 6.f;
}
#pragma endregion

#pragma region Taking the pictures

APhotoTour::APhotoTour()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APhotoTour::Tick(float Dt)
{
	Super::Tick(Dt);
	// wait until every material's shaders are compiled (new materials show a grey stand-in until then)
	if (Shot < 0 && GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0) { Clock = 0; return; }
	Clock += Dt;
	const int32 Want = Clock < Settle ? -1 : FMath::FloorToInt((Clock - Settle) / Each);
	if (Want == Shot) return;
	Shot = Want;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	if (Shot >= UE_ARRAY_COUNT(Views)) { FGenericPlatformMisc::RequestExit(false); return; }
	// the camera at this view
	const FView& V = Views[Shot];
	const FVector From = FTerminalPlan::ToWorld(V.From.X, V.From.Y, V.From.Z), To = FTerminalPlan::ToWorld(V.To.X, V.To.Y, V.To.Z);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(From, (To - From).Rotation(), P))
	{
		PC->SetViewTargetWithBlend(Cam, 0.f);
		// the picture a moment later, once the view has rendered (lighting and streaming settled)
		FTimerHandle T;
		const FString Name = V.Name;
		GetWorldTimerManager().SetTimer(T, [Name]() { FScreenshotRequest::RequestScreenshot(Name, false, false); }, Each * 0.6f, false);
	}
}

#pragma endregion
