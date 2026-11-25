// The ground vehicles you have bought, on their places at the vehicle area (to-do 277 E1; Sim/ADPVehicleArea.h): each comes when
// nobody looks at its place and the place is clear, at the start and when bought. Never popped in view, never over another.
#include "GSE.h"
#include "ServiceVehicles.h"
#include "Tugs.h"
#include "Airfield.h"
#include "ApronBus.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPVehicleArea.h"
#include "ADPLog.h"
#include "BabylonModel.h"

namespace
{
	// its lane and the bus road's leg beside it (plan metres: Data/vehicle-area.json, Airfield.cpp), and where people walk along the leg
	constexpr float DpLane = 273.5f, DpRoad = -245.f, DpRoadBus = -244.f, DpWalk = -247.6f;
	bool SpareBought(const FADPWorld& W, const AGSE::FTractor& T)
	{
		for (const FADPAirline& A : W.Airlines) if (A.Id == T.Owner) for (const FADPSpare& S : A.Spares) if (S.Kind + TEXT("@") + S.Place == T.SpareId) return true;
		return false;
	}
	bool SpareIn(const AGSE::FTractor& T) { return FVector::Dist2D(T.At, T.Home) < 200.f && FMath::Abs(FMath::FindDeltaAngleDegrees(T.Yaw, T.HomeYaw)) < 20.f; }
	float SpareR(const AGSE::FTractor& T) { return T.Bu ? 11.6f : T.KTurnR / 100.f + 0.4f; }
}

// ------------------------------------------------------------------ your crews with them (E1b)

bool AGSE::DepotAt(const FVector& At) { return ADPVehicleArea::Built() && At.X > -30000. && At.X < -24900. && At.Y < -24000. && At.Y > -31000.; }

FString AGSE::SpareAirline(const FString& Reg) const
{
	const UADPSim* Sim = UADPSim::Get(GetWorld()); if (!Sim) return FString();
	for (const FADPAircraft& A : Sim->World.Aircraft) if (A.Reg == Reg) return A.AirlineId;
	return FString();
}

bool AGSE::SpareOk(const FTractor& T, const FString& AirlineId) const
{
	// (only its owner's flights, only your hired people; still bought; on its place, or out of the vehicle area - never beside its place there)
	const UADPSim* Sim = UADPSim::Get(GetWorld());
	return Sim && T.bSpare && !T.bAway && T.M.IsValid() && T.Owner == UADPSim::Me() && T.Owner == AirlineId && !SpareOld.Contains(T.SpareId) && SpareBought(Sim->World, T) && (SpareIn(T) || !DepotAt(T.At));
}

int32 AGSE::SparePick(int32 Svc, const FString& Reg) const
{
	const FString Al = SpareAirline(Reg); int32 Best = INDEX_NONE;
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		const FTractor& T = Tractors[i]; if (!T.bSpare || T.Svc != Svc || T.Job != EJob::None || T.Player.IsValid() || !SpareOk(T, Al)) continue;
		const bool bIn = SpareIn(T); if (bIn && T.bUsed) continue;
		if (bIn) return i;
		if (Best == INDEX_NONE) Best = i;
	}
	return Best;
}

bool AGSE::SpareIdle(const FTractor& T) const
{
	if (T.Job != EJob::None || T.Player.IsValid() || T.Driver != INDEX_NONE || T.Path.Num() || T.Until) return false;
	const FServiceSpec& S = ADPService::All()[T.Svc];
	if ((S.bStairs && !StairsStowed(T)) || (S.bAmbu && !AmbuStowed(T)) || (S.bCater && !CaterStowed(T)) || (S.bFuel && !FuelStowed(T)) || (S.Wet && !WaterStowed(T)) || (S.bDeicer && !DeicerShut(T))) return false;
	FString Why; return !CleanHeld(T, Why);
}

void AGSE::SpareShow(FTractor& T, bool bShow)
{
	if (!bShow) { T.At = T.Home = Plan(T.Home.X / 100.f - 4000.f, -T.Home.Y / 100.f + 4000.f); }
	T.bAway = !bShow; T.bPrev = false; T.Speed = 0.f;
	if (ABabylonModel* M = T.M.Get()) { M->SetActorHiddenInGame(!bShow); M->SetActorEnableCollision(bShow); }
	Pose(T);
	ADP_LOG(TEXT("GSE: the bought vehicle %s %s"), *T.SpareId, bShow ? TEXT("stands on its place") : TEXT("is put away: not in this career, or not where it stood"));
}

bool AGSE::BusOk(int32 Ti, const FString& AirlineId) const { return Tractors.IsValidIndex(Ti) && (!Tractors[Ti].bSpare || SpareOk(Tractors[Ti], AirlineId)); }

void AGSE::BusOwnFirst(TArray<int32>& Free) const
{
	if (Free.ContainsByPredicate([&](int32 Ti) { return !Tractors[Ti].bSpare; })) Free.RemoveAll([&](int32 Ti) { return Tractors[Ti].bSpare; });
}

void AGSE::HomeOut(const FTractor& T, TArray<FVector>& W, const FVector& ToW) const
{
	if (!T.bSpare)
	{
		const FServiceSpec& S = ADPService::All()[T.Svc];
		Leg(W, S.HomeX, S.HomeY, S.HomeHx, S.HomeHy, S.HomeX + S.HomeHx * 9.f, S.HomeY + S.HomeHy * 9.f, S.HomeHx, S.HomeHy, T.KTurnR); return;
	}
	// straight out, onto the lane to the east, onto the road the way it goes
	const float x = T.Home.X / 100.f, y = -T.Home.Y / 100.f, s = y > DpLane ? -1.f : 1.f, r = SpareR(T), d = -ToW.Y / 100.f > DpLane ? 1.f : -1.f, k = T.KTurnR;
	Leg(W, x, y, 0.f, s, x, DpLane - s * r, 0.f, s, k);
	Leg(W, x, DpLane - s * r, 0.f, s, x + r + 0.4f, DpLane, 1.f, 0.f, k);
	Leg(W, x + r + 0.4f, DpLane, 1.f, 0.f, DpRoad - r, DpLane, 1.f, 0.f, k);
	Leg(W, DpRoad - r, DpLane, 1.f, 0.f, DpRoad, DpLane + d * (r + 2.f), 0.f, d, k);
}

FVector AGSE::HomeMouth(const FTractor& T) const
{
	if (T.bSpare) return Plan(DpRoad, DpLane - SpareR(T) - 2.f);
	const FServiceSpec& S = ADPService::All()[T.Svc]; return Plan(S.HomeX + S.HomeHx * 34.f, S.HomeY + S.HomeHy * 34.f);
}

void AGSE::SpareHome(int32 Ti, TFunction<void()> Then)
{
	// along the road to its lane, west along it, round to point into its place, straight back in: nose out
	FTractor& T = Tractors[Ti]; TArray<FVector> H;
	const float x = T.Home.X / 100.f, y = -T.Home.Y / 100.f, s = y > DpLane ? -1.f : 1.f, r = SpareR(T), d = -T.At.Y / 100.f > DpLane ? 1.f : -1.f, k = T.KTurnR, Rx = T.Bu ? DpRoadBus : DpRoad;
	if (!Apron(H, T.At, Plan(Rx, DpLane + d * (r + 2.f)), &T))
	{
		ADP_LOG(TEXT("GSE: %s has no clear way to the vehicle area now - tried again"), *T.Name);
		RetryIn(Ti, 8.f, [this, Ti, Then] { SpareHome(Ti, Then); }); return;
	}
	Leg(H, Rx, DpLane + d * (r + 2.f), 0.f, -d, Rx - r, DpLane, -1.f, 0.f, k);
	Leg(H, Rx - r, DpLane, -1.f, 0.f, x + r + 0.4f, DpLane, -1.f, 0.f, k);
	Leg(H, x + r + 0.4f, DpLane, -1.f, 0.f, x, DpLane + s * r, 0.f, s, k);
	ADP_LOG(TEXT("GSE: %s drives back to the vehicle area"), *T.Name);
	Go(T, MoveTemp(H), false, [this, Ti, Then]
	{
		FTractor& U = Tractors[Ti]; TArray<FVector> B; const FVector A = U.At;
		for (int32 i = 1; i <= 50; ++i) B.Add(FMath::Lerp(A, U.Home, i / 50.f));
		Go(U, MoveTemp(B), true, Then);
	});
}

void AGSE::DepotGo(int32 Ci, const FVector& Near)
{
	// on foot: along the terminal's front, the bus road's leg across taxilane T (waiting for its aircraft), the lane, up the gap on
	// the side of the door they use; from the remote stands by the bus road, waiting at each crossing. Slow until the crew bus.
	ATugs* G = Tg(); if (!G || !G->Crew.IsValidIndex(Ci) || !G->Crew[Ci].A.IsValid()) return;
	ATugs::FCrew& C = G->Crew[Ci]; const FVector Me = C.A->GetActorLocation(); const float Mx = float(Me.X / 100.), My = float(-Me.Y / 100.);
	if (FVector::Dist2D(Me, Near) < 600.f) return;   // (at it)
	auto Walk = [&](TFunction<FVector()> To) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Walk; A.To = MoveTemp(To); A.Stop = 45.f; A.Max = 160.f; C.Q.Add(A); };
	auto On = [&](float X, float Y) { const FVector P = Plan(X, Y); Walk([P] { return P; }); };
	auto Clear = [&](float X, float Y, float R, const TCHAR* Where)
	{
		const FVector P = Plan(X, Y); TSharedRef<bool> bSaid = MakeShared<bool>(false); const FString N = C.Name, W = Where;
		WaitFor(Ci, [this, P, R, bSaid, N, W](float) { if (!PlaneNear(P, R)) return true; if (!*bSaid) { *bSaid = true; ADP_LOG(TEXT("GSE: %s waits at %s for an aircraft"), *N, *W); } return false; });
	};
	float Off = 0.f;
	for (const FTractor& T : Tractors)
		if (FVector::Dist2D(T.At, Near) < 50.f)
		{
			const bool bDrv = T.Driver == INDEX_NONE || T.Driver == Ci || T.Svc == INDEX_NONE; float Sy = float(T.KStand.Y);
			if (!bDrv && ADPService::All()[T.Svc].Doors.Num()) if (const FVector* V = T.Pt.Find(ADPService::All()[T.Svc].Doors[0].Stand)) Sy = float(V->Y);
			Off = float(FRotator(0.f, T.Yaw, 0.f).RotateVector(FVector(0.f, (Sy < 0.f ? -1.f : 1.f) * (T.Bu ? 2.8f : 1.85f), 0.f)).X); break;
		}
	const float To = float(Near.X / 100.) + Off;
	if (DepotAt(Me)) Walk([this, Ci] { const ATugs* Q = Tg(); const FVector L = Q && Q->Crew.IsValidIndex(Ci) && Q->Crew[Ci].A.IsValid() ? Q->Crew[Ci].A->GetActorLocation() : FVector::ZeroVector; return Plan(float(L.X / 100.), DpLane); });
	else if (My < 240.f)
	{
		ADP_LOG(TEXT("GSE: %s walks to the vehicle area, %.0f m: no crew bus yet"), *C.Name, FMath::Abs(Mx - DpWalk) + DpLane - 85.45f);
		float X = Mx; if (My < 83.f) { X = 271.6f; On(X, 88.f); }   // (out of the building by Ramp services 13)
		if (My < 187.f || Mx > -240.f) { On(X, 85.45f); On(DpWalk, 85.45f); On(DpWalk, 187.f); }
		Clear(DpRoad, 218.f, 12000.f, TEXT("taxilane T")); On(DpWalk, 247.f); On(DpWalk, DpLane);
	}
	else
	{
		ADP_LOG(TEXT("GSE: %s walks from the remote stands to the vehicle area, %.0f m: no crew bus yet"), *C.Name, Mx - DpWalk);
		const float Road = 338.75f, H = AAirfield::Shared().Remote.LinkHalf + 4.f;
		if (Mx > -237.f)
		{
			On(Mx, Road); TArray<float> Lk = AAirfield::Shared().Remote.LinkX; Lk.Sort(); Algo::Reverse(Lk);
			for (const float Lx : Lk) if (Lx < Mx) { On(Lx + H, Road); Clear(Lx, Road, 12000.f, TEXT("a taxiway link")); On(Lx - H, Road); }
			On(-237.f, Road);
		}
		On(DpWalk, 331.f); On(DpWalk, DpLane);
	}
	On(To, DpLane);
}

bool AGSE::DepotBack(int32 Ci, bool bNorth)
{
	// from a place there back to the terminal's front (or out to the bus road), the same way; false: not there
	ATugs* G = Tg(); if (!G || !G->Crew.IsValidIndex(Ci) || !G->Crew[Ci].A.IsValid() || !DepotAt(G->Crew[Ci].A->GetActorLocation())) return false;
	ATugs::FCrew& C = G->Crew[Ci];
	auto Walk = [&](TFunction<FVector()> To) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Walk; A.To = MoveTemp(To); A.Stop = 45.f; A.Max = 160.f; C.Q.Add(A); };
	auto On = [&](float X, float Y) { const FVector P = Plan(X, Y); Walk([P] { return P; }); };
	Walk([this, Ci] { const ATugs* Q = Tg(); const FVector L = Q && Q->Crew.IsValidIndex(Ci) && Q->Crew[Ci].A.IsValid() ? Q->Crew[Ci].A->GetActorLocation() : FVector::ZeroVector; return Plan(float(L.X / 100.), DpLane); });   // (out of the gap)
	On(DpWalk, DpLane);
	if (bNorth) { On(DpWalk, 331.f); On(-237.f, 338.75f); return true; }
	On(DpWalk, 247.f);
	const FVector P = Plan(DpRoad, 218.f); WaitFor(Ci, [this, P](float) { return !PlaneNear(P, 12000.f); });
	On(DpWalk, 187.f); On(DpWalk, 85.45f);
	return true;
}

// ------------------------------------------------------------------ standing on their places (E1)

void AGSE::SpareTick(float Dt)
{
	if ((SpareT -= Dt) > 0.f) return;
	SpareT = 2.f;
	UADPSim* Sim = UADPSim::Get(this); if (!Sim || !ADPVehicleArea::Built()) return;
	if (!Sim->World.Airlines.Num()) return;
	// (another career came without a new level: what stands away from its place, and what nobody there has bought, goes when nobody
	//  looks - put away, not destroyed, and used again when one of its kind is bought; what is being used first finishes)
	if (SpareLoads != Sim->Loads)
	{
		if (SpareLoads >= 0) for (const FTractor& T : Tractors) if (T.bSpare && !T.bAway && FVector::Dist2D(T.At, T.Home) > 100.f) SpareOld.Add(T.SpareId);
		SpareLoads = Sim->Loads;
	}
	for (FTractor& T : Tractors)
	{
		if (!T.bSpare || T.bAway || (!SpareOld.Contains(T.SpareId) && SpareBought(Sim->World, T)) || !SpareIdle(T) || !TaxiUnseen(T.At)) continue;
		SpareOld.Remove(T.SpareId); SpareShow(T, false); return;
	}
	for (const FADPAirline& A : Sim->World.Airlines)
		for (const FADPSpare& S : A.Spares)
		{
			const FString Id = S.Kind + TEXT("@") + S.Place;
			if (Tractors.ContainsByPredicate([&](const FTractor& T) { return T.bSpare && !T.bAway && T.SpareId == Id; })) continue;
			auto Say = [&](const TCHAR* Why) { bool bWas = false; SpareSaid.Add(Id + Why, &bWas); if (!bWas) ADP_LOG(TEXT("GSE: the bought vehicle %s waits: %s"), *Id, Why); };
			const ADPVehicleArea::FKind* K = ADPVehicleArea::Kind(S.Kind); const ADPVehicleArea::FPlace* P = ADPVehicleArea::Place(S.Place);
			if (!K || !P) { Say(TEXT("no such kind or place in Data/vehicle-area.json")); continue; }
			const int32 i = ADPService::All().IndexOfByPredicate([&](const FServiceSpec& X) { return K->Points == X.Points && K->bRoof == X.bRoof; });
			if (i == INDEX_NONE) { Say(TEXT("no such service vehicle")); continue; }
			const FServiceSpec& Sv = ADPService::All()[i];
			const FVector At = Plan(P->X, P->Y);
			if (!TaxiUnseen(At)) { Say(TEXT("somebody sees its place")); continue; }
			// (a vehicle's middle within the place's own strip, with both half sizes and half a metre)
			const float a = FMath::DegreesToRadians(P->Heading); const FVector Fw(FMath::Cos(a), -FMath::Sin(a), 0.f), Sd(-Fw.Y, Fw.X, 0.f);
			if (Tractors.ContainsByPredicate([&](const FTractor& T) { const FVector D = T.At - At; return T.M.IsValid() && FMath::Abs(FVector::DotProduct(D, Fw)) < T.KHalfL + Sv.HalfL && FMath::Abs(FVector::DotProduct(D, Sd)) < T.KHalfW + Sv.HalfW + 50.f; }))
			{ Say(TEXT("another vehicle stands in its place")); continue; }
			// (one put away, of this kind: a bus keeps its row)
			if (FTractor* Old = Tractors.FindByPredicate([&](const FTractor& T) { return T.bSpare && T.bAway && T.M.IsValid() && T.Svc == i && (!Sv.bBus || T.SpareId == Id); }))
			{
				Old->SpareId = Id; Old->Owner = A.Id; Old->Name = FString::Printf(TEXT("%s, place %s"), *K->Label, *P->Name); Old->bUsed = false;
				Old->Home = Old->At = PlanPose(P->X, P->Y, FMath::Cos(a), FMath::Sin(a), Old->Yaw); Old->HomeYaw = Old->Yaw;
				SpareShow(*Old, true); SvcLivery(*Old); return;
			}
			if (Tractors.Num() >= Tractors.Max()) { Say(TEXT("no room for another vehicle")); continue; }   // (the entries never move: Setup reserves them)
			FSpareAt Sa; Sa.Id = Id; Sa.Owner = A.Id; Sa.X = P->X; Sa.Y = P->Y; Sa.Heading = P->Heading; Sa.Name = FString::Printf(TEXT("%s, place %s"), *K->Label, *P->Name);
			int32 n = 0;
			if (Sv.bBus)
			{
				AApronBuses* Bs = AApronBuses::Get(GetWorld()); if (!Bs) { Say(TEXT("no apron buses here")); continue; }
				Bs->Load(); n = Bs->All().IndexOfByPredicate([&](const AApronBuses::FBus& B) { return B.Name == Sa.Name; });
				if (n == INDEX_NONE) n = Bs->AddBus(Sa.Name, TEXT("depot ") + P->Name, P->X, P->Y, P->Heading);
			}
			if (!SvcSpawn(i, n, &Sa)) { Say(TEXT("its model is missing")); continue; }
			return;   // (one at a time)
		}
}
