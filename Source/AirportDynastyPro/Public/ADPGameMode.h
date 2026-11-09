// Sets up the terminal test when play starts: sun, sky, fog, the terminal, its passengers and
// the player standing at the plan's spawn point.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ADPGameMode.generated.h"

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
