// The electric buses' DC fast chargers at the vehicle area's bays 1-3 (to-do 277 E3a): the models in their places, nothing more.
// They stand with the remote stands' area and go with it. Parts and sockets: docs/Hired-Crew-Plan.md "E3a as built".
#pragma once
#include "CoreMinimal.h"

class ABabylonModel;

namespace ADPBusCharger
{
	/** Puts them up when the area is there, takes them away when it is not. Pose: the plan's place and direction to the world's. */
	void Keep(UWorld* World, TFunctionRef<FVector(float X, float Y, float Hx, float Hy, float& Yaw)> Pose);
	/** The charger of a bay ("1" to "3"); null without one. */
	ABabylonModel* Of(UWorld* World, const FString& Bay);
}
