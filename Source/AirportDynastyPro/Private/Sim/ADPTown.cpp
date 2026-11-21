// Your airport and its neighbours - see Sim/ADPTown.h.
#include "Sim/ADPTown.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPBoards.h"
#include "Site.h"

namespace
{
	double GCurfewFrom = 0.0, GCurfewTo = 6.0;
	bool GCurfewSet = false;
	void TwNote(FADPWorld& W, const FDateTime& At, int32 Prio, const FString& Title, const FString& Text, const TCHAR* Kind = TEXT("authority"))
	{ FADPNotice N; N.At = At; N.Kind = Kind; N.Priority = Prio; N.Title = Title; N.Text = Text; W.Notices.Add(N); }
	FADPAirline* TwMe(FADPWorld& W) { return W.Airlines.FindByPredicate([](const FADPAirline& X) { return X.bPlayer; }); }
	int32 TwOnWatch(const FADPWorld& W, const FString& Icao)
	{
		int32 N = 0;
		for (const FADPPerson& P : W.People) if (P.AirlineId == UADPSim::Me() && P.Role == TEXT("fire") && P.Base == Icao && P.Status == TEXT("on")) ++N;
		return N == 0 ? 0 : FMath::Max(1, FMath::RoundToInt(N * 0.25 + 0.3));   // (24-hour watches: about a quarter on duty)
	}
	const int32 Trucks[11] = {0, 1, 1, 1, 1, 1, 2, 2, 3, 3, 3};
	const int32 Crew[11] = {0, 2, 2, 3, 4, 5, 6, 8, 10, 12, 14};
}

int32 ADPTown::FireCategoryOf(const FString& TypeId)
{
	const FADPTypeInfo* T = ADPWorldData::Type(TypeId);
	if (!T) return 6;
	if (T->Class == TEXT("bush") || T->Class == TEXT("helicopter")) return 3;
	if (T->Class == TEXT("turboprop")) return T->Seats > 50 ? 5 : 4;
	if (T->Class == TEXT("regional")) return T->Seats > 90 ? 6 : 5;
	if (T->Class == TEXT("widebody")) return T->Seats > 450 ? 10 : T->Seats > 300 ? 9 : 8;
	return T->Seats > 190 ? 7 : 6;
}

int32 ADPTown::FireCategoryAt(const FADPWorld& W, const FString& Icao, const FDateTime& At)
{
	if (Icao != ADPBoards::HereIcao()) return 10;
	const FADPAirline* A = W.Airlines.FindByPredicate([](const FADPAirline& X) { return X.bPlayer; });
	const int32 Have = A ? A->FireTrucks : 0, People = TwOnWatch(W, Icao);
	int32 Cat = 0;
	for (int32 c = 1; c <= 10; ++c) if (Have >= Trucks[c] && People >= Crew[c]) Cat = c;
	return Cat;
}

void ADPTown::HereCurfew(double& From, double& To) { From = GCurfewFrom; To = GCurfewTo; }

FString ADPTown::SyndicName(const FADPWorld& W) { return W.Town.Syndic.IsEmpty() ? FString(TEXT("Claire Dufour")) : W.Town.Syndic; }

bool ADPTown::CommunityFund(FADPWorld& W, double Usd, const FDateTime& At)
{
	FADPAirline* A = TwMe(W); if (!A || Usd <= 0 || A->Balance < Usd) return false;
	ADPEconomy::Pay(W, A->Id, At, TEXT("fees"), -Usd, TEXT("Community fund for the towns round the airport"));
	W.Town.Mood = FMath::Min(100.0, W.Town.Mood + Usd / 50000.0);
	return true;
}

bool ADPTown::Insulate(FADPWorld& W, int32 Homes, const FDateTime& At)
{
	FADPAirline* A = TwMe(W); const double Cost = Homes * 18000.0;   // (sound-proof windows and roof insulation, about CHF 16,000 a home)
	if (!A || Homes <= 0 || A->Balance < Cost) return false;
	ADPEconomy::Pay(W, A->Id, At, TEXT("fees"), -Cost, FString::Printf(TEXT("Noise insulation for %d homes"), Homes));
	W.Town.Insulated += Homes; W.Town.Mood = FMath::Min(100.0, W.Town.Mood + Homes * 0.05);
	return true;
}

bool ADPTown::Compensate(FADPWorld& W, double Usd, const FDateTime& At)
{
	FADPAirline* A = TwMe(W); if (!A || Usd <= 0 || A->Balance < Usd) return false;
	ADPEconomy::Pay(W, A->Id, At, TEXT("fees"), -Usd, TEXT("Compensation to residents for noise"));
	W.Town.Mood = FMath::Min(100.0, W.Town.Mood + Usd / 30000.0); W.Town.Complaints = FMath::Max(0.0, W.Town.Complaints - Usd / 2000.0);
	return true;
}

bool ADPTown::Lobby(FADPWorld& W, const FDateTime& At, FString& Why)
{
	FADPAirline* A = TwMe(W); const double Cost = 250000.0;
	if (!A || A->Balance < Cost) { Why = TEXT("lobbying costs $250,000"); return false; }
	if (W.Town.LobbyUntil > At) { Why = TEXT("already lobbying the council"); return false; }
	ADPEconomy::Pay(W, A->Id, At, TEXT("fees"), -Cost, TEXT("Lobbying the town council (advisers, studies, meetings)"));
	W.Town.LobbyUntil = At + FTimespan::FromDays(21.0);
	TwNote(W, At, 0, TEXT("Lobbying"), TEXT("Your people are working on the council - its vote on the curfew comes in three weeks. A happier town makes a yes likelier."), TEXT("company"));
	return true;
}

bool ADPTown::Bribe(FADPWorld& W, double Usd, const FDateTime& At, FString& Why)
{
	FADPAirline* A = TwMe(W);
	if (!A || Usd < 50000.0) { Why = TEXT("the syndic won't move for less than $50,000"); return false; }
	if (A->Balance < Usd) { Why = TEXT("not enough money"); return false; }
	ADPEconomy::Pay(W, A->Id, At, TEXT("fees"), -Usd, TEXT("Consulting fee"));   // (booked as something else, as bribes are)
	W.Town.Bribes += Usd; W.Town.LastBribe = At;
	// the council bends: an hour off the curfew for every $150,000 (at most all of it)
	const double Hours = FMath::Min(Usd / 150000.0, W.Town.CurfewTo - W.Town.CurfewFrom + (W.Town.CurfewFrom > W.Town.CurfewTo ? 24.0 : 0.0));
	W.Town.CurfewFrom = FMath::Fmod(W.Town.CurfewFrom + Hours / 2.0, 24.0); W.Town.CurfewTo = FMath::Fmod(W.Town.CurfewTo - Hours / 2.0 + 24.0, 24.0);
	if (FMath::Abs(W.Town.CurfewTo - W.Town.CurfewFrom) < 0.25) W.Town.CurfewFrom = W.Town.CurfewTo;
	TwNote(W, At, 1, TEXT("The council"), FString::Printf(TEXT("After a quiet dinner with %s the council relaxes the night curfew - now %02d:%02d-%02d:%02d."), *SyndicName(W),
		int32(W.Town.CurfewFrom), int32(FMath::Fmod(W.Town.CurfewFrom, 1.0) * 60), int32(W.Town.CurfewTo), int32(FMath::Fmod(W.Town.CurfewTo, 1.0) * 60)), TEXT("company"));
	return true;
}

bool ADPTown::BuyFireTruck(FADPWorld& W, const FDateTime& At)
{
	FADPAirline* A = TwMe(W); const double Price = 850000.0;   // (an airport crash tender, 6x6)
	if (!A || A->Balance < Price) return false;
	ADPEconomy::Pay(W, A->Id, At, TEXT("purchase"), -Price, TEXT("A new airport crash tender for the fire station"));
	++A->FireTrucks;
	return true;
}

void FADPTownModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
	FADPWorld& W = Sim.World;
	FADPTownState& T = W.Town;
	if (!T.bStarted)
	{
		// Chablais (and any site): closed 00:00-06:00 local for regular flights, as Geneva is; the town's syndic
		T.bStarted = true; T.CurfewFrom = 0.0; T.CurfewTo = 6.0; T.Mood = 55.0;
		static const TCHAR* Names[] = {TEXT("Claire Dufour"), TEXT("Jean-Marc Rochat"), TEXT("Sylvie Pittet"), TEXT("Olivier Favre")};
		T.Syndic = Names[Rng.RandRange(0, 3)];
	}
	GCurfewFrom = T.CurfewFrom; GCurfewTo = T.CurfewTo; GCurfewSet = true;
	// complaints: each night movement here (22:00-07:00 local) and loud aircraft; fewer with insulated homes
	const FString Here = ADPBoards::HereIcao();
	const int32 H = ADPBoards::Local(To).GetHour();
	const bool bNight = H >= 22 || H < 7;
	for (const FADPFlight& F : W.Flights)
	{
		if (F.From != Here && F.To != Here) continue;
		const double Age = (To - F.DepAt).GetTotalMinutes();
		const bool bMoveNow = (F.From == Here && Age >= 0 && Age < 60) || (F.To == Here && FMath::Abs(Age - F.DurMin) < 30);
		if (!bMoveNow) continue;
		const FADPTypeInfo* Ty = ADPWorldData::Type(F.TypeId);
		const double Loud = Ty && Ty->Class == TEXT("widebody") ? 2.0 : 1.0;
		T.Complaints += (bNight ? 6.0 : 0.4) * Loud * FMath::Clamp(1.0 - T.Insulated / 800.0, 0.2, 1.0);
	}
	if (!(To.GetHour() == 0 && From.GetHour() != 0)) return;
	// every day: complaints fade, the mood follows them; the council's vote when lobbying ends; the bribe's risk
	T.Mood = FMath::Clamp(T.Mood - FMath::Min(4.0, T.Complaints / 40.0) + 0.3, 0.0, 100.0);
	if (T.Complaints > 60.0 && Rng.FRand() < 0.15) TwNote(W, To, 1, TEXT("Noise complaints"), FString::Printf(TEXT("Residents of Villeneuve and Rennaz complain about night flights (%d complaints this month). The town's mood: %.0f / 100."), int32(T.Complaints), T.Mood));
	T.Complaints *= 0.95;
	if (T.LobbyUntil != FDateTime() && T.LobbyUntil <= To)
	{
		T.LobbyUntil = FDateTime();
		if (T.Mood >= 60.0 && Rng.FRand() < 0.35 + (T.Mood - 60.0) / 80.0 && T.CurfewFrom != T.CurfewTo)
		{
			T.CurfewFrom = FMath::Fmod(T.CurfewFrom + 0.5, 24.0);
			TwNote(W, To, 1, TEXT("The council voted yes"), FString::Printf(TEXT("The council shortens the night curfew by 30 minutes: now from %02d:%02d."), int32(T.CurfewFrom), int32(FMath::Fmod(T.CurfewFrom, 1.0) * 60)), TEXT("company"));
		}
		else TwNote(W, To, 1, TEXT("The council voted no"), FString::Printf(TEXT("The council keeps the curfew as it is (the town's mood %.0f / 100 - a happier town makes a yes likelier)."), T.Mood), TEXT("company"));
	}
	// a bribe found out: about 3% a week for each $100,000 paid, more in the first months, never zero
	if (T.Bribes > 0.0 && !T.bScandal)
	{
		const double Weeks = (To - T.LastBribe).GetTotalDays() / 7.0;
		const double PerDay = FMath::Clamp(T.Bribes / 100000.0 * 0.03 / 7.0 * (Weeks < 12.0 ? 1.5 : 0.7), 0.0005, 0.05);
		if (Rng.FRand() < PerDay)
		{
			T.bScandal = true;
			FADPAirline* A = TwMe(W);
			const double Fine = T.Bribes * 2.0 + 1000000.0;
			if (A) { ADPEconomy::Pay(W, A->Id, To, TEXT("fine"), -Fine, TEXT("Fine: bribing a public official")); A->Reputation = FMath::Max(0.0, A->Reputation - 15.0); }
			FADPCase C; C.AirlineId = UADPSim::Me(); C.What = FString::Printf(TEXT("Bribery of the syndic %s"), *ADPTown::SyndicName(W)); C.Hearing = To + FTimespan::FromDays(45.0); W.Cases.Add(C);
			T.CurfewFrom = 23.0; T.CurfewTo = 6.5; T.Mood = FMath::Max(0.0, T.Mood - 40.0);
			TwNote(W, To, 2, TEXT("Scandal"), FString::Printf(TEXT("A journalist has uncovered your payments to the syndic %s. Fine $%.1f million, reputation -15, a court case in 45 days - and the council brings the curfew back harder: 23:00-06:30."), *ADPTown::SyndicName(W), Fine / 1e6));
			FADPNews Nw; Nw.At = To; Nw.Topic = TEXT("airport"); Nw.Headline = TEXT("Airport bribery scandal: syndic under investigation"); W.News.Add(Nw);
		}
	}
}
