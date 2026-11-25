// The vehicle area: the depot of the remote stands, where bought ground vehicles are kept (to-do 277 E1, Data/vehicle-area.json,
// proved by Tools/check_vehicle_area.py). The plan and the kinds only; AGSE places the vehicles (GSESpare.cpp).
#pragma once
#include "CoreMinimal.h"

struct FADPWorld;

namespace ADPVehicleArea
{
	struct FPlace { FString Name, For, Fixed; float X = 0.f, Y = 0.f, Heading = 0.f; bool bCharge = false; };   // (plan metres, degrees; Fixed: somebody's own already)
	struct FKind { FString Key, Label, Points; bool bRoof = false; double Price = 0.0; TArray<FString> Places; };
	const TArray<FPlace>& Places();
	const TArray<FKind>& Kinds();
	const FKind* Kind(const FString& Key);
	const FPlace* Place(const FString& Name);
	/** The area exists here: the remote stands are in use. */
	bool Built();
	/** The first free place a vehicle of this kind may stand on, best sort first; null when all are taken. */
	const FPlace* FirstFree(const FADPWorld& W, const FKind& K);
}
