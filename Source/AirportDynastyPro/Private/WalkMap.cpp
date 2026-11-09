// The walking map: places from the plan's rooms (and the outside areas), openings from its doors and stairs, and the
// shortest way between two points through them.
#include "WalkMap.h"

#pragma region Building

int32 FWalkMap::Find(const FString& Id) const
{
	return Places.IndexOfByPredicate([&](const FPlace& P) { return P.Id == Id; });
}

void FWalkMap::Build(const FTerminalPlan& Plan)
{
	Places.Reset();
	Portals.Reset();
	for (const FPlanRoom& R : Plan.Rooms)
		Places.Add({R.Id, R.Kind, R.X0, R.Z0, R.X1, R.Z1, Plan.FloorY(R.Floor), false});
	// the outside areas the doors lead to: the kerb in front, the apron behind the pier, the jetways
	auto Outside = [&](const FString& Id, float X0, float Z0, float X1, float Z1, float Y)
	{
		if (Find(Id) == INDEX_NONE) Places.Add({Id, TEXT("outside"), X0, Z0, X1, Z1, Y, true});
	};
	Outside(TEXT("KERB"), 30, -16, 120, -1, 0);
	Outside(TEXT("OUTSIDE"), 30, -16, 120, -1, 0);
	Outside(TEXT("APRON"), -80, 83, 242, 100, 0);
	for (int32 n = 1; n <= 4; ++n) Outside(FString::Printf(TEXT("J-%d"), n), -80, 83, 242, 100, Plan.F2Y);

	for (const FPlanDoor& D : Plan.Doors)
	{
		const int32 A = Find(D.A), B = Find(D.B);
		if (A == INDEX_NONE || B == INDEX_NONE) continue;
		const FVector P(D.X, Plan.FloorY(D.Floor), D.Z);
		Portals.Add({A, B, P, P});
	}
	for (const FPlanStair& S : Plan.Stairs)
	{
		const int32 A = Find(S.From), B = Find(S.To);
		if (A == INDEX_NONE || B == INDEX_NONE) continue;
		// the bottom end and the top end of the run, half a metre clear of the steps
		const float ZC = (S.Z0 + S.Z1) / 2, XC = (S.X0 + S.X1) / 2;
		FVector Bottom, Top;
		if (S.Dir == TEXT("x+"))      { Bottom = FVector(S.X0 - 0.5f, 0, ZC); Top = FVector(S.X1 + 0.5f, Plan.F2Y, ZC); }
		else if (S.Dir == TEXT("x-")) { Bottom = FVector(S.X1 + 0.5f, 0, ZC); Top = FVector(S.X0 - 0.5f, Plan.F2Y, ZC); }
		else if (S.Dir == TEXT("z+")) { Bottom = FVector(XC, 0, S.Z0 - 0.5f); Top = FVector(XC, Plan.F2Y, S.Z1 + 0.5f); }
		else                          { Bottom = FVector(XC, 0, S.Z1 + 0.5f); Top = FVector(XC, Plan.F2Y, S.Z0 - 0.5f); }
		Portals.Add({A, B, Bottom, Top});
	}
	PlacePortals.SetNum(Places.Num());
	for (TArray<int32>& L : PlacePortals) L.Reset();
	for (int32 i = 0; i < Portals.Num(); ++i)
	{
		PlacePortals[Portals[i].A].Add(i);
		PlacePortals[Portals[i].B].Add(i);
	}
}

FVector FWalkMap::RandomIn(int32 Place, FRandomStream& Rng) const
{
	const FPlace& P = Places[Place];
	const float M = FMath::Min(1.f, FMath::Min(P.X1 - P.X0, P.Z1 - P.Z0) * 0.25f);
	return FVector(Rng.FRandRange(P.X0 + M, P.X1 - M), P.Y, Rng.FRandRange(P.Z0 + M, P.Z1 - M));
}

#pragma endregion

#pragma region Finding a way

bool FWalkMap::Way(int32 From, const FVector& P0, int32 To, const FVector& P1, TArray<FVector>& Out) const
{
	Out.Reset();
	if (From == To) { Out.Add(P1); return true; }
	// nodes: each opening crossed one way (even: from A into B, ending at PB; odd: from B into A, ending at PA)
	const int32 N = Portals.Num() * 2;
	TArray<float> Dist;
	TArray<int32> Prev;
	TArray<bool> Done;
	Dist.Init(TNumericLimits<float>::Max(), N);
	Prev.Init(INDEX_NONE, N);
	Done.Init(false, N);
	auto RoomOf = [&](int32 Node) { const FPortal& P = Portals[Node / 2]; return Node % 2 ? P.A : P.B; };
	auto EndOf = [&](int32 Node) { const FPortal& P = Portals[Node / 2]; return Node % 2 ? P.PA : P.PB; };
	auto StartOf = [&](int32 Node) { const FPortal& P = Portals[Node / 2]; return Node % 2 ? P.PB : P.PA; };
	auto Enter = [&](int32 Room, const FVector& At, float Base, int32 Parent)
	{
		for (const int32 Pi : PlacePortals[Room])
		{
			const FPortal& P = Portals[Pi];
			const int32 Node = Pi * 2 + (P.A == Room ? 0 : 1);
			if (Parent != INDEX_NONE && Node / 2 == Parent / 2) continue;
			if (Places[RoomOf(Node)].bOutside && RoomOf(Node) != To) continue; // never walk through an outside area
			const float D = Base + FVector::Dist(At, StartOf(Node)) + FVector::Dist(StartOf(Node), EndOf(Node));
			if (D < Dist[Node]) { Dist[Node] = D; Prev[Node] = Parent; }
		}
	};
	Enter(From, P0, 0, INDEX_NONE);
	float Best = TNumericLimits<float>::Max();
	int32 BestNode = INDEX_NONE;
	for (;;)
	{
		int32 U = INDEX_NONE;
		for (int32 i = 0; i < N; ++i) if (!Done[i] && Dist[i] < TNumericLimits<float>::Max() && (U == INDEX_NONE || Dist[i] < Dist[U])) U = i;
		if (U == INDEX_NONE || Dist[U] >= Best) break;
		Done[U] = true;
		if (RoomOf(U) == To)
		{
			const float D = Dist[U] + FVector::Dist(EndOf(U), P1);
			if (D < Best) { Best = D; BestNode = U; }
			continue;
		}
		Enter(RoomOf(U), EndOf(U), Dist[U], U);
	}
	if (BestNode == INDEX_NONE) return false;
	TArray<int32> Chain;
	for (int32 n = BestNode; n != INDEX_NONE; n = Prev[n]) Chain.Insert(n, 0);
	for (const int32 Node : Chain)
	{
		Out.Add(StartOf(Node));
		if (!StartOf(Node).Equals(EndOf(Node), 0.01f)) Out.Add(EndOf(Node));
	}
	Out.Add(P1);
	return true;
}

#pragma endregion
