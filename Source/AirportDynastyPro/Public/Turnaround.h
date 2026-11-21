// Every aircraft's turnaround at a gate as a plan of real tasks (owner 2026-11-21: "make all of this in order and follow
// real-world guidelines ... how long each of these different steps take ... when I add catering crew and belt loaders and
// stuff later, then automatically make everything work in sync"). The tasks, their times and what each waits for come from
// Airbus's A320 Aircraft Characteristics 5-2-0 (full servicing, 44 min: docs/Turnaround-Plan.md). A task with a vehicle in
// the game calls it when it starts (the jetway, the GPU, the tug - more as they are built: catering, fuel, belt and cargo
// loaders, water, toilet, cleaning); one without runs as data with its real time. Late tasks push on their dependants;
// only that aircraft's departure moves.
#pragma once
#include "CoreMinimal.h"

class UWorld;

namespace ADPTurnaround
{
	struct FTask
	{
		FString Id, Name, Vehicle;      // Vehicle: who does it ("" = the crew on foot / data only)
		float Start = 0.f, Dur = 0.f;   // minutes from on-blocks
		TArray<FString> After;          // the tasks it waits for
		bool bStarted = false, bDone = false;
	};
	struct FTurn
	{
		FString Reg, Stand; float StandX = 0.f;
		FDateTime OnBlocks, OffBlocks;  // (off-blocks: its next departure from here, delay included)
		int32 Pax = 150; float FuelL = 20000.f;
		TArray<FTask> Tasks;
		float LateMin = 0.f;            // how late the plan runs against the departure (0 on time)
	};
	/** The A320's plan for a ground time (minutes): forward from on-blocks, boarding and the door backward from off-blocks. */
	TArray<FTask> PlanA320(float GroundMin, int32 Pax, float FuelL, float& LateMin);
	/** Called by the traffic every couple of seconds: starts / ends the tasks due, calls the vehicles. */
	void Tick(UWorld* W, const FDateTime& Now);
	/** The turnarounds going on (for the screens and tests). */
	const TArray<FTurn>& All();
}
