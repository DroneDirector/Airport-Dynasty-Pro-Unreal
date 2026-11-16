// The "Airport ground" graphics setting (to-do 40f): how sharp the paving's scanned pictures are drawn and how much
// relief they show. Low: every ground picture at 2K; Medium: its colour at 4K, its relief at 2K; High: all at 4K (the
// default); Ultra: 4K plus a fine relief close by (the same scan's relief again, finer - Tools/set_ground.py, MPC_Weather
// Relief / FineRelief). Saved in GameUserSettings [ADP.Graphics] AirportGround.
#pragma once
#include "CoreMinimal.h"

class UWorld;

namespace ADPGroundQuality
{
	int32 Level();
	const TCHAR* Name(int32 L);
	/** Saves the level and puts it into effect at once. */
	void Set(int32 L, UWorld* W);
	/** Puts the saved level into effect (at the start, after the ground is in). */
	void Apply(UWorld* W);
}
