// The walkers' navigation mesh (owner 2026-11-21: "they need to know where the walls and floors are, and that they can't
// walk through walls"). Unreal's own navigation, generated once at run time from the real geometry - the terminal's
// floors and walls, both floors, and the landside car park and forecourt; never the whole world. The terminal plan's
// unfinished rooms (kind "empty") are cut out. Moving things (people, bags, belts, vehicles, doors, jetways) never
// affect it, so it is never rebuilt while the game runs; the walls do. PaxFlow walks every leg of its ways along it.
#pragma once

#include "CoreMinimal.h"

struct FTerminalPlan;
class UWorld;
class AActor;

namespace PaxNav
{
	enum EPath : uint8 { NoWay, Full, Partial, OffMesh };
	/** Once: the walkable bounds (the terminal, the landside box) and the cut-outs; generated in the background. */
	void Start(UWorld* W, const FTerminalPlan& Plan, const FBox& Landside);
	/** True once the first generation is done (logs "navmesh ready" once). Cheap; call every frame. */
	bool Ready(UWorld* W);
	/** Things that move never shape the navmesh: every primitive of every such actor (and every skeletal mesh). Call
	 *  now and then: things spawned later are caught on the next sweep. */
	void Sweep(UWorld* W);
	/** One actor's primitives off the navmesh (a person just spawned). */
	void NoNav(AActor* A);
	/** A walk from one point to another on the navmesh: the corner points (from and to included). OffMesh when either
	 *  end is off the navmesh (a jetway, an escalator), NoWay when both are on it but nothing joins them. */
	EPath Path(UWorld* W, const FVector& From, const FVector& To, TArray<FVector>& Out);
}
