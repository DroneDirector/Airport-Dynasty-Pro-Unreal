// The terminal's walking map, made from the shared layout: every room is a place to walk in, joined to its neighbours
// through its doors, and the floors are joined by the stairs. Outside areas (the kerb, the apron, the jetways) are
// places too. A way between two points is the shortest chain of doors and stairs (Dijkstra over the openings).
#pragma once

#include "CoreMinimal.h"
#include "TerminalPlan.h"

#pragma region Walking map

struct AIRPORTDYNASTYPRO_API FWalkMap
{
	/** A place to walk: a room of the plan, or an outside area. Points are plan metres (x, height, z). */
	struct FPlace { FString Id, Kind; float X0, Z0, X1, Z1, Y; bool bOutside = false; };
	/** An opening between two places: a door (both points the same) or a stair (bottom point in A, top point in B). */
	struct FPortal { int32 A, B; FVector PA, PB; };

	TArray<FPlace> Places;
	TArray<FPortal> Portals;

	void Build(const FTerminalPlan& Plan);
	int32 Find(const FString& Id) const;
	/** A random point inside a place, a metre clear of its walls. */
	FVector RandomIn(int32 Place, FRandomStream& Rng) const;
	/** The way from P0 (in place From) to P1 (in place To) as points to walk through; false when there is none. */
	bool Way(int32 From, const FVector& P0, int32 To, const FVector& P1, TArray<FVector>& Out) const;

private:
	TArray<TArray<int32>> PlacePortals;
};

#pragma endregion
