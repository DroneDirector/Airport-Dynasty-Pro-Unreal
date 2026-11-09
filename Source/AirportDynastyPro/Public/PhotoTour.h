// Photo mode for screenshots of the airport (started with -PhotoTour): after the world has settled it puts a camera
// at each set view in turn (the terminal from behind at an angle, from the front at an angle, close to the jetways),
// takes a picture without the interface into Saved/Screenshots, and closes the game.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhotoTour.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API APhotoTour : public AActor
{
	GENERATED_BODY()

public:
	APhotoTour();
	virtual void Tick(float DeltaSeconds) override;

private:
	float Clock = 0;
	int32 Shot = -1;
};
