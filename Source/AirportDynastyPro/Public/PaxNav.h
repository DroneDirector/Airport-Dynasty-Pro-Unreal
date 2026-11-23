// The walkers' navigation mesh (owner 2026-11-21: "they need to know where the walls and floors are, and that they can't
// walk through walls"). Unreal's own navigation, generated once at run time from the real geometry - the terminal's
// floors and walls, both floors, and the landside car park and forecourt; never the whole world. The terminal plan's
// unfinished rooms (kind "empty") are cut out. Moving things (people, bags, belts, vehicles, doors, jetways) never
// affect it, so it is never rebuilt while the game runs; the walls do. PaxFlow walks every leg of its ways along it.
// Since 2026-11-23 it comes ready-made: baked for the release (-PaxNavBake, Tools/bake_paxnav.sh -> the site's paxnav.bin)
// and loaded in a moment; only when the terminal, the roads or the walker changed since (its key) is it generated, and
// then kept in Saved/PaxNav for the next start (-NoPaxNavFile: always generated, to compare).
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
	/** As early as possible (the loading screen, the passenger flow): starts the navmesh once the terminal is in. */
	void Kick(UWorld* W);
	bool Started(UWorld* W);
	/** The map is being made in this run (no ready-made one): the revolving doors stand still meanwhile - every turn of them
	 *  made it start over (1500 changed places a second, 2026-11-23), and it was never finished. */
	bool Making();
	/** Not ready 2 minutes after it started (warned): carry on without it. */
	bool Failed(UWorld* W);
	/** Tile jobs still to do (for the loading screen). */
	int32 JobsLeft(UWorld* W);
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
	/** The check of the map: every leg a passenger walks - the car park, the forecourt, every entrance, check-in, each
	 *  security lane, the airside, Floor 2 and every gate, the arrivals' way out - each in the log ("PaxNav leg OK/FAILED"),
	 *  and the ways that must stay shut for them. How many failed (0: all reached). The escalators and the revolving doors
	 *  are rides of their own (PaxFlow), off the map: the legs end at their feet. */
	int32 Legs(UWorld* W, FString* Summary = nullptr);
	/** The same, as one line: how many legs are reached, how far in all. */
	FString Check(UWorld* W);
}
