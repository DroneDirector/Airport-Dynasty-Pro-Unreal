// The graphics card's memory as Windows sees it (to-do 269): how much this game may use, uses and how much of it had to move
// into the PC's slow main memory - over the limit every frame takes many times as long.
#pragma once
#include "CoreMinimal.h"

namespace ADPGraphicsMemory
{
	struct FNow { int64 UsedMB = 0, BudgetMB = 0, SharedMB = 0; bool bKnown = false; };
	FNow Now();
	FString Line();
	/** The guard: from now on the texture pool gives way (never over MaxPoolMB, never under 1000 MB) while the card is nearly
	 *  full, so what is drawn every frame never has to move into main memory; it comes back when there is room again. */
	void Guard(int32 MaxPoolMB);
}
