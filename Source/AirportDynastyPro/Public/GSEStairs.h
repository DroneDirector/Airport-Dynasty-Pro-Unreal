// The passenger stairs' own moving parts (to-do 212, Private/GSEStairs.cpp): what a FTractor of AGSE needs besides the shared ones.
#pragma once

#include "CoreMinimal.h"

class UBoxComponent;

struct FSvcStairs
{
	int32 GUp = INDEX_NONE, GGate = INDEX_NONE, GLever[2] = {INDEX_NONE, INDEX_NONE}, GNeedle = INDEX_NONE;
	int32 GJack = INDEX_NONE, GLift[2][4] = {{INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE}, {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE}};
	TArray<int32> GTread;   // (the lower flight's folding ones, 2 .. 10)
	// its movements and where they are going: treads up (0 .. 10), the jacks and the gates in cm
	float N = 0.f, NWant = 0.f, Jack = 0.f, JackWant = 0.f, Gate = 0.f, GateWant = 0.f, Lev[2] = {0.f, 0.f}, WasSpeed = -1.f;   // (levers: the stairs', the legs')
	float Target = 0.f;     // the crew's: treads up at this aircraft
	bool bLive = true, bReady = false, bYours = false, bGatesWant = false;
	float YourN = 0.f;      // what you asked for at its panel
	int32 Users = 0;        // people on it or on their way up or down: it does not move
	float UserT = 0.f;
	struct FSolid { TWeakObjectPtr<UBoxComponent> B; FVector At; };
	TArray<FSolid> Up;      // your solid boxes of the upper flight and the platform
	TWeakObjectPtr<UBoxComponent> Front;   // the platform's open front, shut until the gates are out at an aircraft
	bool bHelperUp = false; // its helper is on the treads or the platform
};
