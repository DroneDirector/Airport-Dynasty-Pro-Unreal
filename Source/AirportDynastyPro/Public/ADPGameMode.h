// Sets up the terminal test when play starts: sun, sky, fog, the terminal, its passengers and
// the player standing at the plan's spawn point.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ADPGameMode.generated.h"

/** The world's late part (owner, 2026-11-15: the title screen took 40 s): the roads, the town and our land along the
 *  roads, built one road a frame once Go() is called - when you first go into the 3D world, or behind a new career's
 *  loading screen. Test runs (-UIShot), the photo tour and -LateWorldNow build it straight away. */
namespace ADPLateWorld
{
	void Prepare(class UWorld* W);
	void Go();
	bool Started();
	bool Done();
	float Progress();
	float RoadsProgress();
	float LandProgress();
}

UCLASS()
class AIRPORTDYNASTYPRO_API AADPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AADPGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** Number of passengers in the terminal. */
	UPROPERTY(EditAnywhere, Category = "Airport Dynasty Pro") int32 CrowdSize = 400;

private:
	#pragma region World setup
	void SpawnSkyAndLight();
	#pragma endregion
};
