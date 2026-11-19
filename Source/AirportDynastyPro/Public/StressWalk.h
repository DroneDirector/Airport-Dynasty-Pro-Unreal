// The stress walk (owner, 2026-11-16): the game walks the airport by itself as a player would, to measure how smoothly
// it runs - outside past the road lanes towards the departures entrance and turning round near it, the Grand Hall, up
// the airside stairs, the escalator, Floor 2 and the gate walkway, gate 1, along its jetway's middle line to the cab,
// down the service stair and out on the apron looking back at the terminal. It first waits until the world is fully
// loaded plus a countdown (a note in the middle of the screen says so - never shown in normal play), looks ahead and
// steers round walls, tables and chairs, and marks each part in ADP-Perf.log, ending with a summary of the whole walk.
// Started from the title screen ("Stress walk") or -UIPerfWalk (Tools/perf_walk.sh).
#pragma once
#include "CoreMinimal.h"

class UWorld;
class APlayerController;

namespace ADPStressWalk
{
	/** Start it (the caller has put you in the airport on foot). bQuitWhenDone: close the game at the end (test runs). */
	void Begin(bool bQuitWhenDone);
	/** The coaches' own walk (owner, 2026-11-19, to-do 146; -UICoachWalk): from the car park nearer the terminal over the
	 *  zebra crossing, looking at the coaches, up close along them, into one through its front door (E) and down its aisle,
	 *  out again, then the coaches seen from the main hall through the glass - by day, then again at night. */
	void BeginCoaches(bool bQuitWhenDone);
	bool Active();
	/** The note in the middle of the screen while it waits (empty when there is none). */
	FText Note();
	void Tick(UWorld* W, APlayerController* PC, float Dt);
}
