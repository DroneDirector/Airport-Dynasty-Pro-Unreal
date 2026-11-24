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
	bool bLive = true, bReady = false, bYours = false;   // (bYours: it stows itself)
	// yours (to-do 238): the aircraft's door it stands at now (and the treads up to its sill), the one its gates were out at by you;
	// how far that door is (cm ahead, to the right, degrees), how long you are away from it, its emergency stop
	float YouStandX = -1e6f, YouN = 0.f, ServedX = -1e6f, Ahead = 0.f, Aside = 0.f, Turn = 0.f, DockT = 0.f, LeftT = 0.f;
	int32 YouDoor = 0, ServedDoor = 0; bool bNear = false, bServed = false, bStop = false;
	int32 Users = 0;        // people on it or on their way up or down: it does not move
	float UserT = 0.f;
	struct FSolid { TWeakObjectPtr<UBoxComponent> B; FVector At; };
	TArray<FSolid> Up;      // your solid boxes of the upper flight and the platform
	TWeakObjectPtr<UBoxComponent> Front;   // the platform's open front, shut until the gates are out at an aircraft
	bool bHelperUp = false; // its helper is on the treads or the platform
};
