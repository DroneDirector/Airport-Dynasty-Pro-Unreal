// Vehicles owned (to-do 318): every airport vehicle you have, from the live vehicles - the service vehicles, the baggage
// tractors, belt loaders and maintenance vans (AGSE, wear from GSEBreak.cpp), the pushback and hangar tugs (ATugs) and the
// fire trucks - one list, read by the native Vehicles page on the tablet, the computers and the phone (UI/VehiclesPage.cpp).
#pragma once
#include "CoreMinimal.h"

struct FOwnedVehicle
{
	FString Kind;           // what it is ("apron bus", "pushback tug" ...)
	FString BuyKey;         // the vehicle area's kind to buy one more (Data/vehicle-area.json), empty: no place for one
	FString Name;
	int32 Condition = -1;   // 0..100 from its wear (GSEBreak.cpp); -1: its wear is not kept
	FString State;          // "parked", "at work", "broken down" ...
	FString Place;          // where it is now
	bool bBroken = false, bWrittenOff = false, bWork = false, bBought = false;
	FVector At = FVector::ZeroVector, Home = FVector::ZeroVector;   // (world cm)
};

namespace ADPOwned
{
	/** Every airport vehicle there is now, the ones you own; kinds in the order the list shows them. */
	AIRPORTDYNASTYPRO_API void List(UWorld* W, TArray<FOwnedVehicle>& Out);
	/** Where a vehicle at At is, in words: its own place, a stand, the vehicle area, the hangar apron, the apron. */
	AIRPORTDYNASTYPRO_API FString PlaceOf(UWorld* W, const FVector& At, const FVector& Home);
}
