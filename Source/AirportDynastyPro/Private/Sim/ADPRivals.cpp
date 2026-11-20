// The AI airlines - see Sim/ADPRivals.h.
#include "Sim/ADPRivals.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPStaff.h"
#include "Sim/ADPDemand.h"
#include "Sim/ADPEvents.h"

namespace
{
	// made-up names (never a real airline's)
	const TCHAR* RivalNames[] = {TEXT("Aurelia Airways"), TEXT("Vantor Air"), TEXT("Celestra"), TEXT("Nordhavn Air"), TEXT("Meridia Airlines"), TEXT("Altavia"),
		TEXT("Corvalis Air"), TEXT("Skyrun"), TEXT("Azurel"), TEXT("Borealis Wings"), TEXT("Halcyon Air"), TEXT("Tessara Airways"), TEXT("Kestrel Atlantic"),
		TEXT("Solmar Air"), TEXT("Avanta"), TEXT("Lumora Air"), TEXT("Ventara"), TEXT("Orvella Airlines")};

	// each hub's markets in reach, worked out once: large and medium airports with scheduled service, 150..4,500 nm away
	TMap<FString, TArray<const FADPAirportInfo*>> GMarkets;
	const TArray<const FADPAirportInfo*>& MarketsFrom(const FADPAirportInfo& Hub)
	{
		if (const TArray<const FADPAirportInfo*>* M = GMarkets.Find(Hub.Ident)) return *M;
		TArray<const FADPAirportInfo*>& M = GMarkets.Add(Hub.Ident);
		ADPWorldData::ForEachAirport([&](const FADPAirportInfo& Ap)
		{
			if ((Ap.Kind != 'L' && Ap.Kind != 'M') || !Ap.bScheduled || Ap.Icao.IsEmpty() || Ap.Ident == Hub.Ident) return;
			const double D = ADPWorldData::DistanceNm(Hub.Lat, Hub.Lon, Ap.Lat, Ap.Lon);
			if (D >= 150.0 && D <= 4500.0) M.Add(&Ap);
		});
		return M;
	}
	FADPAirline* AirlineById(FADPWorld& W, const FString& Id) { return W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Id; }); }
}

void FADPRivalsModule::Found(FADPWorld& W, int32 Count, const FDateTime& At)
{
	int32 Have = 0; for (const FADPAirline& A : W.Airlines) if (!A.bPlayer) ++Have;
	if (Have >= Count || !ADPWorldData::IsReady()) return;
	const FADPAirline* Me = W.Airlines.FindByPredicate([](const FADPAirline& X) { return X.bPlayer; });
	const FADPAirportInfo* MyHub = Me ? ADPWorldData::Airport(Me->Hub) : nullptr;
	// hubs: the large airports with scheduled service - half of them within 1,300 nm of yours
	TArray<const FADPAirportInfo*> Near, Far;
	ADPWorldData::ForEachAirport([&](const FADPAirportInfo& Ap)
	{
		if (Ap.Kind != 'L' || !Ap.bScheduled || Ap.Icao.IsEmpty()) return;
		(MyHub && ADPWorldData::DistanceNm(MyHub->Lat, MyHub->Lon, Ap.Lat, Ap.Lon) < 1300.0 ? Near : Far).Add(&Ap);
	});
	for (int32 i = Have; i < Count; ++i)
	{
		TArray<const FADPAirportInfo*>& From = (i % 2 == 0 && Near.Num()) ? Near : Far;
		if (!From.Num()) break;
		const FADPAirportInfo* Hub = From[Rng.RandRange(0, From.Num() - 1)];
		FADPAirline A; A.Id = FString::Printf(TEXT("ai%d"), i); A.Name = RivalNames[i % UE_ARRAY_COUNT(RivalNames)];
		A.Icao = FString::Printf(TEXT("%c%c"), TCHAR('B' + Rng.RandRange(0, 23)), TCHAR('A' + Rng.RandRange(0, 25)));
		A.Hub = Hub->Icao; A.Quality = Rng.FRandRange(0.6, 0.95); A.Reputation = Rng.FRandRange(40.0, 80.0); A.Balance = Rng.FRandRange(15e6, 60e6);
		W.Airlines.Add(A);
		// a starting fleet (leased, like most airlines), each aircraft put straight on a route
		const int32 Fleet = Rng.RandRange(3, 12);
		for (int32 k = 0; k < Fleet; ++k)
		{
			const FString Type = Rng.FRand() < 0.15 ? TEXT("B789") : Rng.FRand() < 0.5 ? TEXT("A320") : Rng.FRand() < 0.5 ? TEXT("B38M") : TEXT("A321");
			FADPAirline& Al = W.Airlines.Last(); Al.Balance += 5e6;   // (the deposit covered by the founders)
			const FString Reg = ADPEconomy::BuyAircraft(W, Al.Id, Type, true, Hub->Icao, At);
			if (FADPAircraft* C = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; })) OpenRoute(W, W.Airlines.Last(), *C, At);
		}
		FADPNotice N; N.At = At; N.Kind = TEXT("company"); N.Text = FString::Printf(TEXT("%s (%s) flies from %s with %d aircraft"), *A.Name, *A.Icao, *Hub->Name, Fleet); W.Notices.Add(N);
	}
}

bool FADPRivalsModule::OpenRoute(FADPWorld& W, FADPAirline& A, FADPAircraft& Craft, const FDateTime& At)
{
	const FADPAirportInfo* Hub = ADPWorldData::Airport(A.Hub); const FADPTypeInfo* T = ADPWorldData::Type(Craft.TypeId);
	if (!Hub || !T) return false;
	const TArray<const FADPAirportInfo*>& Markets = MarketsFrom(*Hub);
	if (!Markets.Num()) return false;
	// the best of a sample of markets in the type's reach: travellers on the pair, shared with whoever flies it already
	const FADPAirportInfo* Best = nullptr; double BestScore = 0.0;
	const double Reach = T->RangeNm * 0.8, NeedFt = T->ToRunwayM * 3.281;
	for (int32 s = 0; s < 300; ++s)
	{
		const FADPAirportInfo* B = Markets[Rng.RandRange(0, Markets.Num() - 1)];
		const double D = ADPWorldData::DistanceNm(Hub->Lat, Hub->Lon, B->Lat, B->Lon);
		if (D > Reach || B->LongestFt() < NeedFt || (T->Class == TEXT("widebody") && D < 1800.0) || (T->Class != TEXT("widebody") && D > 2600.0)) continue;
		int32 Flying = 0; for (const FADPRoute& R : W.Routes) if (R.Stops.Contains(Hub->Ident) && R.Stops.Contains(B->Ident)) ++Flying;
		const double Score = ADPDemand::MarketPax(*Hub, *B, At) / (1.0 + Flying * 1.5) * Rng.FRandRange(0.8, 1.2);
		if (Score > BestScore) { BestScore = Score; Best = B; }
	}
	if (!Best) return false;
	// its crew: a captain rated on the type and the cabin crew it needs, hired at the hub
	FADPPerson Cap = ADPStaff::MakePerson(TEXT("captain"), Hub->Country, Rng); Cap.Types = {Craft.TypeId};
	W.People.Add(Cap); ADPStaff::Hire(W, Cap.Id, A.Id, Hub->Icao, At);
	const int32 NeedFa = ADPStaff::CabinCrewFor(T->Seats) - ADPStaff::CabinCrewAt(W, A.Id, Hub->Icao) + ADPStaff::CabinCrewFor(T->Seats) / 2;
	for (int32 k = 0; k < NeedFa; ++k) { FADPPerson Fa = ADPStaff::MakePerson(TEXT("fa"), Hub->Country, Rng); W.People.Add(Fa); ADPStaff::Hire(W, Fa.Id, A.Id, Hub->Icao, At); }
	FADPRoute R; R.Id = FString::Printf(TEXT("%s-%s-%s"), *A.Id, *Hub->Icao, *Best->Icao); R.AirlineId = A.Id; R.Stops = {Hub->Icao, Best->Icao}; R.TypeId = Craft.TypeId; R.PilotId = Cap.Id;
	R.OpenedAt = At;
	W.Routes.Add(R);
	Craft.RouteId = R.Id; Craft.LegIdx = 0; Craft.Location = Hub->Icao; Craft.ReadyAt = At + FTimespan::FromHours(Rng.FRandRange(1.0, 20.0));
	++Opened;
	return true;
}

void FADPRivalsModule::Decide(FADPWorld& W, FADPAirline& A, const FDateTime& At)
{
	TArray<FADPAircraft*> Fleet; for (FADPAircraft& C : W.Aircraft) if (C.AirlineId == A.Id) Fleet.Add(&C);
	// in the red: count the days; sell an aircraft when short; bankrupt after 60 days or deep in debt
	double FleetValue = 0.0; for (const FADPAircraft* C : Fleet) FleetValue += C->Ownership == TEXT("owned") ? C->Value : 0.0;
	if (A.Balance < 0.0) { if (A.RedSince == FDateTime()) A.RedSince = At; } else A.RedSince = FDateTime();
	if (A.RedSince != FDateTime() && ((At - A.RedSince).GetTotalDays() > 60.0 || A.Balance < -(FleetValue * 0.5 + 25e6)))
	{
		// bankruptcy: routes closed, aircraft back to their lessors or sold, the staff out into the hiring pool
		W.Routes.RemoveAll([&](const FADPRoute& R) { return R.AirlineId == A.Id; });
		W.Flights.RemoveAll([&](const FADPFlight& F) { return F.AirlineId == A.Id; });
		W.Aircraft.RemoveAll([&](const FADPAircraft& C) { return C.AirlineId == A.Id; });
		for (FADPPerson& P : W.People) if (P.AirlineId == A.Id) { P.AirlineId.Empty(); P.Status = TEXT("pool"); }
		FADPNotice N; N.At = At; N.Kind = TEXT("company"); N.Text = FString::Printf(TEXT("%s has gone bankrupt and stops flying - its staff look for work"), *A.Name); W.Notices.Add(N);
		ADPEvents::News(W, At, TEXT("rival"), FString::Printf(TEXT("%s goes bankrupt"), *A.Name));
		A.Id = TEXT("gone:") + A.Id; ++Bankrupt;
		return;
	}
	if (A.Balance < 0.0 && Fleet.Num() > 1)
	{
		for (FADPAircraft* C : Fleet) if (C->Status != TEXT("flying")) { const FString Reg = C->Reg; W.Routes.RemoveAll([&](const FADPRoute& R) { return R.Id == C->RouteId; }); if (ADPEconomy::SellAircraft(W, Reg, At)) { ++Sold; break; } }
		return;
	}
	// close routes that lose money (given 3 weeks and 10 legs to prove themselves): the aircraft goes idle
	for (FADPAircraft* C : Fleet)
	{
		const int32 RI = W.Routes.IndexOfByPredicate([&](const FADPRoute& R) { return R.Id == C->RouteId; });
		if (RI == INDEX_NONE) continue;
		const FADPRoute& R = W.Routes[RI];
		if (R.Legs >= 10 && R.ProfitEma < 0.0 && (At - R.OpenedAt).GetTotalDays() > 21.0 && C->Status != TEXT("flying"))
		{ W.Routes.RemoveAt(RI); C->RouteId.Empty(); ++Closed; }
	}
	// idle aircraft: a new route
	for (FADPAircraft* C : Fleet) if (C->RouteId.IsEmpty() && C->Status == TEXT("parked")) OpenRoute(W, A, *C, At);
	// growth: full flights and money in the bank -> one more aircraft (leased; bought when rich)
	double Lf = 0.0; int32 N = 0;
	for (const FADPRoute& R : W.Routes) if (R.AirlineId == A.Id && R.Legs > 0) { Lf += R.LoadEma; ++N; }
	Lf = N ? Lf / N : 0.0;
	const FString Type = Rng.FRand() < 0.15 ? TEXT("B789") : TEXT("A320");
	const double Price = ADPWorldData::Type(Type) ? ADPWorldData::Type(Type)->ValueM * 1e6 : 55e6;
	if (Lf > 0.78 && Fleet.Num() < 80 && A.Balance > Price * 0.05 + 5e6 && Rng.FRand() < 0.35)
	{
		const FString Reg = ADPEconomy::BuyAircraft(W, A.Id, Type, A.Balance < Price * 1.5, A.Hub, At);
		if (FADPAircraft* C = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; })) { ++Bought; OpenRoute(W, A, *C, At); }
	}
	// you on one of its pairs: a fare war for 45 days (the better ones fight on price)
	for (FADPRoute& R : W.Routes)
	{
		if (R.AirlineId != A.Id) continue;
		if (R.FareUntil != FDateTime() && R.FareUntil <= At) { R.FareIndex = 1.0; R.FareUntil = FDateTime(); }
		if (R.FareUntil != FDateTime() || A.Quality < 0.6) continue;
		const bool bYou = W.Routes.ContainsByPredicate([&](const FADPRoute& O)
		{ const FADPAirline* Ow = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == O.AirlineId; }); return Ow && Ow->bPlayer && O.Stops.Contains(R.Stops[0]) && O.Stops.Contains(R.Stops.Last()); });
		if (bYou) { ADPEvents::News(W, At, TEXT("rival"), FString::Printf(TEXT("Fare war: %s cuts prices %s-%s"), *A.Name, *R.Stops[0], *R.Stops.Last())); R.FareIndex = 0.82; R.FareUntil = At + FTimespan::FromDays(45.0); ++FareWars;
			FADPNotice Nt; Nt.At = At; Nt.Kind = TEXT("company"); Nt.Text = FString::Printf(TEXT("%s cuts its fares %s-%s to fight your route"), *A.Name, *R.Stops[0], *R.Stops.Last()); W.Notices.Add(Nt); }
	}
}

void FADPRivalsModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
	if (!ADPWorldData::IsReady()) return;
	FADPWorld& W = Sim.World;
	// each airline once a day, at its own hour
	for (int32 i = 0; i < W.Airlines.Num(); ++i)
	{
		FADPAirline& A = W.Airlines[i];
		if (A.bPlayer || A.Id.StartsWith(TEXT("gone:"))) continue;
		if (int32(GetTypeHash(A.Id) % 24) == To.GetHour()) Decide(W, A, To);
	}
}
