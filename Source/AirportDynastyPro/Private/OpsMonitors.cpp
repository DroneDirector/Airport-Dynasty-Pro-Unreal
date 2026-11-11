// The operations room's monitor, by the player's screen (OpsMonitors.h).
#include "OpsMonitors.h"
#include "BabylonModel.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

AOpsMonitors::AOpsMonitors()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

int32 AOpsMonitors::KindFor(float Aspect)
{
	// 32:9 is 3.56, 21:9 2.33 to 2.39, 16:9 1.78 (16:10 and 4:3 get the widescreen too)
	return Aspect >= 2.9f ? 2 : Aspect >= 2.1f ? 1 : 0;
}

void AOpsMonitors::Build()
{
	UWorld* W = GetWorld(); if (!W) return;
	FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const TCHAR* Name : { TEXT("Monitor169"), TEXT("Monitor219"), TEXT("Monitor329") })
	{
		ABabylonModel* M = W->SpawnActor<ABabylonModel>(GetActorLocation(), GetActorRotation(), S);
		if (M && !M->Place(Name)) { M->Destroy(); M = nullptr; }
		if (M) { M->SetActorHiddenInGame(true); M->SetActorEnableCollision(false); }
		Kinds.Add(M);
	}
	Tick(0.f);
}

void AOpsMonitors::Tick(float Dt)
{
	Super::Tick(Dt);
	FVector2D Size(16, 9);
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Size);
	if (Size.Y < 1.0) return;
	int32 Want = KindFor(float(Size.X / Size.Y));
	// (tests: -OpsMonitor=0, 1 or 2 shows that one whatever the window)
	int32 Force = -1; if (FParse::Value(FCommandLine::Get(), TEXT("OpsMonitor="), Force) && Force >= 0 && Force <= 2) Want = Force;
	if (Want == Current) return;
	for (int32 i = 0; i < Kinds.Num(); ++i)
		if (ABabylonModel* M = Kinds[i].Get()) { M->SetActorHiddenInGame(i != Want); M->SetActorEnableCollision(i == Want); }
	Current = Want;
	UE_LOG(LogTemp, Log, TEXT("Operations monitor: %s for a %.0fx%.0f screen"), Want == 2 ? TEXT("32:9") : Want == 1 ? TEXT("21:9") : TEXT("16:9"), Size.X, Size.Y);
}
