// The airline's network and money markets - see Sim/ADPNetwork.h.
#include "Sim/ADPNetwork.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPEvents.h"
#include "Sim/ADPWorldData.h"

namespace
{
	FADPAirline* Al(FADPWorld& W, const FString& Id) { return W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Id; }); }
	const FADPAirline* Al(const FADPWorld& W, const FString& Id) { return W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Id; }); }
	void Note(FADPWorld& W, const FDateTime& At, const TCHAR* Kind, int32 Prio, const FString& Title, const FString& Text)
	{ FADPNotice N; N.At = At; N.Kind = Kind; N.Priority = Prio; N.Title = Title; N.Text = Text; W.Notices.Add(N); }
	FADPAlliance* AllianceOf(FADPWorld& W, const FString& AirlineId)
	{ return W.Alliances.FindByPredicate([&](const FADPAlliance& A) { return A.Members.Contains(AirlineId); }); }
	double FleetValue(const FADPWorld& W, const FString& AirlineId)
	{ double V = 0; for (const FADPAircraft& C : W.Aircraft) if (C.AirlineId == AirlineId && C.Ownership == TEXT("owned")) V += C.Value; return V; }
	double Debt(const FADPWorld& W, const FString& AirlineId)
	{ double D = 0; for (const FADPLoan& L : W.Loans) if (L.AirlineId == AirlineId) D += L.Owed; return D; }
}

void ADPNetwork::Boost(FADPWorld& W, FADPFlight& F, int32 Seats)
{
	const FADPAirline* A = Al(W, F.AirlineId);
	if (!A || Seats <= 0) return;
	double K = 0.0;
	// connections at its hub: one more route there, a little more transfer traffic (up to +25%)
	if (F.From == A->Hub || F.To == A->Hub)
	{
		int32 AtHub = 0; for (const FADPRoute& R : W.Routes) if (R.AirlineId == F.AirlineId && R.Stops.Contains(A->Hub)) ++AtHub;
		K += FMath::Min(0.25, 0.03 * FMath::Max(0, AtHub - 1));
	}
	// frequent flyers (members come back: up to +8%, with the scheme's size)
	if (A->bFrequentFlyer) K += FMath::Min(0.08, A->FfMembers / 400000.0);
	// code-shares: a member's hub at either end (+5%)
	if (const FADPAlliance* Ally = W.Alliances.FindByPredicate([&](const FADPAlliance& X) { return X.Members.Contains(F.AirlineId); }))
		for (const FString& M : Ally->Members) if (M != F.AirlineId) if (const FADPAirline* O = Al(W, M)) if (O->Hub == F.From || O->Hub == F.To) { K += 0.05; break; }
	if (K <= 0.0) return;
	const int32 Total = F.Pax + F.PaxJ + F.PaxF;
	const int32 Extra = FMath::Min(Seats - Total, FMath::RoundToInt(Total * K));
	if (Extra <= 0) return;
	const double PerPax = Total > 0 ? F.Revenue / Total * 0.8 : 0.0;   // (transfer fares are prorated: a little less each)
	F.Pax += Extra; F.PaxConnect = Extra; F.Revenue += PerPax * Extra; F.LoadFactor = double(F.Pax + F.PaxJ + F.PaxF) / Seats;
}

double ADPNetwork::FuelCost(FADPWorld& W, const FString& AirlineId, double FuelKg, double SpotPerKg)
{
	FADPAirline* A = Al(W, AirlineId);
	double Cost = 0.0, Left = FuelKg;
	if (A && A->HedgeKg > 0.0)
	{
		// (hedged fuel was paid for when it was bought: it costs nothing more here)
		const double Use = FMath::Min(A->HedgeKg, Left);
		A->HedgeKg -= Use; Left -= Use;
	}
	return Cost + Left * SpotPerKg;
}

void ADPNetwork::Landed(FADPWorld& W, const FADPFlight& F)
{
	FADPAirline* A = Al(W, F.AirlineId);
	if (!A) return;
	A->Pax30 += F.Pax + F.PaxJ + F.PaxF; A->Legs30 += 1.0;
	if (A->bFrequentFlyer) A->FfMembers += (F.Pax + F.PaxJ + F.PaxF) * 0.04;   // (about 1 in 25 passengers joins)
	if (A->bPlayer) { A->WeekPax += F.Pax + F.PaxJ + F.PaxF; A->WeekLegs += 1; if (F.DelayMin < 15.0 && F.Diverted.IsEmpty()) A->WeekOnTime += 1; }
}

bool ADPNetwork::SetFrequentFlyer(FADPWorld& W, const FString& AirlineId, bool bOn)
{ FADPAirline* A = Al(W, AirlineId); if (!A) return false; A->bFrequentFlyer = bOn; return true; }

bool ADPNetwork::JoinAlliance(FADPWorld& W, const FString& AirlineId, const FString& Name, const FDateTime& At, FString& Why)
{
	FADPAirline* A = Al(W, AirlineId);
	FADPAlliance* Ally = W.Alliances.FindByPredicate([&](const FADPAlliance& X) { return X.Name == Name; });
	if (!A || !Ally) { Why = TEXT("no such alliance"); return false; }
	if (AllianceOf(W, AirlineId)) { Why = TEXT("already in an alliance"); return false; }
	if (A->Reputation < 55.0) { Why = TEXT("your reputation must be 55 or more"); return false; }
	const double Fee = 2.0e6;
	if (A->Balance < Fee) { Why = TEXT("the joining fee is $2 million"); return false; }
	ADPEconomy::Pay(W, AirlineId, At, TEXT("fees"), -Fee, FString::Printf(TEXT("Joining fee: %s"), *Name));
	Ally->Members.Add(AirlineId);
	Note(W, At, TEXT("company"), 1, TEXT("Alliance"), FString::Printf(TEXT("%s has joined %s: code-shares with its members, their passengers connect to your flights."), *A->Name, *Name));
	return true;
}

bool ADPNetwork::FoundAlliance(FADPWorld& W, const FString& AirlineId, const FString& Name, const FDateTime& At, FString& Why)
{
	FADPAirline* A = Al(W, AirlineId);
	if (!A || Name.IsEmpty()) { Why = TEXT("a name is needed"); return false; }
	if (AllianceOf(W, AirlineId)) { Why = TEXT("already in an alliance"); return false; }
	if (A->Reputation < 65.0) { Why = TEXT("your reputation must be 65 or more to found one"); return false; }
	FADPAlliance N; N.Name = Name.Left(40); N.Founder = AirlineId; N.Members.Add(AirlineId); W.Alliances.Add(N);
	Note(W, At, TEXT("company"), 1, TEXT("Alliance founded"), FString::Printf(TEXT("%s founded %s. AI airlines without an alliance may ask to join."), *A->Name, *N.Name));
	return true;
}

bool ADPNetwork::LeaveAlliance(FADPWorld& W, const FString& AirlineId)
{
	FADPAlliance* Ally = AllianceOf(W, AirlineId);
	if (!Ally) return false;
	Ally->Members.Remove(AirlineId);
	return true;
}

bool ADPNetwork::Ipo(FADPWorld& W, const FString& AirlineId, const FDateTime& At, FString& Why)
{
	FADPAirline* A = Al(W, AirlineId);
	if (!A) return false;
	if (A->bPublic) { Why = TEXT("already public"); return false; }
	if (A->Legs30 < 60 || A->Reputation < 50.0) { Why = TEXT("an IPO needs a record: 60 legs in 30 days and reputation 50"); return false; }
	const double Money = A->SharePrice * A->SharesM * 1e6 * 0.30 * 0.95;   // (30% of the shares; 5% to the banks)
	ADPEconomy::Pay(W, AirlineId, At, TEXT("loan"), Money, FString::Printf(TEXT("IPO: 30%% of %s's shares sold"), *A->Name));
	A->bPublic = true; A->FloatShare = 0.30;
	Note(W, At, TEXT("finance"), 1, TEXT("IPO"), FString::Printf(TEXT("%s is listed: $%.1f million raised at $%.2f a share."), *A->Name, Money / 1e6, A->SharePrice));
	return true;
}

bool ADPNetwork::TradeShares(FADPWorld& W, const FString& Target, double Shares, const FDateTime& At, FString& Why)
{
	FADPAirline* Me = W.Airlines.FindByPredicate([](const FADPAirline& X) { return X.bPlayer; });
	FADPAirline* T = Al(W, Target);
	if (!Me || !T || T->bPlayer) { Why = TEXT("no such airline"); return false; }
	double& Held = W.Holdings.FindOrAdd(Target);
	if (Shares < 0.0 && Held < -Shares) { Why = TEXT("you hold fewer shares"); return false; }
	const double Cost = Shares * T->SharePrice * (Shares > 0 ? 1.005 : 0.995);   // (0.5% to the broker)
	if (Shares > 0.0 && Me->Balance < Cost) { Why = TEXT("not enough money"); return false; }
	if (Shares > 0.0 && Held + Shares > T->SharesM * 1e6 * 0.49) { Why = TEXT("at most 49% of an airline (ownership rules)"); return false; }
	ADPEconomy::Pay(W, Me->Id, At, TEXT("sale"), -Cost, FString::Printf(TEXT("%s %.0f shares of %s"), Shares > 0 ? TEXT("Bought") : TEXT("Sold"), FMath::Abs(Shares), *T->Name));
	Held += Shares;
	return true;
}

bool ADPNetwork::Hedge(FADPWorld& W, const FString& AirlineId, double Tonnes, int32 Months, const FDateTime& At, FString& Why)
{
	FADPAirline* A = Al(W, AirlineId);
	if (!A || Tonnes <= 0.0) { Why = TEXT("nothing to hedge"); return false; }
	Months = FMath::Clamp(Months, 1, 6);
	const double PerKg = ADPEconomy::FuelPrice(A->Hub, At) * W.FuelIndex * (1.0 + 0.03 * Months);
	const double Cost = Tonnes * 1000.0 * PerKg;
	if (A->Balance < Cost) { Why = TEXT("not enough money"); return false; }
	ADPEconomy::Pay(W, AirlineId, At, TEXT("fuel"), -Cost, FString::Printf(TEXT("Fuel hedge: %.0f t at $%.3f/kg for %d months"), Tonnes, PerKg, Months));
	const double Kg = Tonnes * 1000.0;
	A->HedgePrice = (A->HedgePrice * A->HedgeKg + PerKg * Kg) / FMath::Max(1.0, A->HedgeKg + Kg);
	A->HedgeKg += Kg; A->HedgeUntil = FMath::Max(A->HedgeUntil, At + FTimespan::FromDays(30.0 * Months));
	return true;
}

TArray<ADPNetwork::FRank> ADPNetwork::League(const FADPWorld& W)
{
	TArray<FRank> Out;
	double MaxPax = 1.0, MaxProfit = 1.0;
	for (const FADPAirline& A : W.Airlines) if (!A.Id.StartsWith(TEXT("gone:"))) { MaxPax = FMath::Max(MaxPax, A.Pax30); MaxProfit = FMath::Max(MaxProfit, FMath::Abs(A.ProfitEma30)); }
	for (const FADPAirline& A : W.Airlines)
	{
		if (A.Id.StartsWith(TEXT("gone:"))) continue;
		FRank R; R.AirlineId = A.Id; R.Name = A.Name; R.Pax30 = A.Pax30; R.OnTime = A.OnTimeEma; R.Reputation = A.Reputation; R.Profit30 = A.ProfitEma30;
		R.Score = 35.0 * A.Pax30 / MaxPax + 25.0 * A.OnTimeEma + 25.0 * A.Reputation / 100.0 + 15.0 * FMath::Clamp(0.5 + 0.5 * A.ProfitEma30 / MaxProfit, 0.0, 1.0);
		Out.Add(R);
	}
	Out.Sort([](const FRank& A, const FRank& B) { return A.Score > B.Score; });
	return Out;
}

void FADPNetworkModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
	FADPWorld& W = Sim.World;
	// AI alliances: two or three of them, founded once there are AI airlines
	if (W.Alliances.Num() == 0)
	{
		TArray<FADPAirline*> Ai; for (FADPAirline& A : W.Airlines) if (!A.bPlayer && !A.Id.StartsWith(TEXT("gone:"))) Ai.Add(&A);
		if (Ai.Num() >= 6)
		{
			static const TCHAR* Names[] = {TEXT("SkyCircle"), TEXT("Polar Link"), TEXT("Meridian Alliance")};
			const int32 N = Ai.Num() >= 9 ? 3 : 2;
			for (int32 i = 0; i < N; ++i) { FADPAlliance Al_; Al_.Name = Names[i]; W.Alliances.Add(Al_); }
			for (int32 i = 0; i < Ai.Num(); ++i) if (Rng.FRand() < 0.75) W.Alliances[i % N].Members.Add(Ai[i]->Id);
		}
	}
	const bool bDay = To.GetHour() == 0 && From.GetHour() != 0;
	if (!bDay) return;
	// every day: 30-day counters fade, profit smoothed, share prices move, the frequent-flyer scheme's cost, hedges lapse
	for (FADPAirline& A : W.Airlines)
	{
		A.Pax30 *= FMath::Exp(-1.0 / 30.0); A.Legs30 *= FMath::Exp(-1.0 / 30.0);
		const double DayProfit = A.PrevBalance != 0.0 ? A.Balance - A.PrevBalance : 0.0; A.PrevBalance = A.Balance;
		A.ProfitEma30 = A.ProfitEma30 * (29.0 / 30.0) + DayProfit;   // (about the last 30 days' profit)
		const double Equity = A.Balance + FleetValue(W, A.Id) - Debt(W, A.Id);
		const double Earnings = A.ProfitEma30 * 12.0 * 9.0;   // (a year's profit at about 9 times)
		const double Fair = FMath::Max(0.5, (FMath::Max(0.0, Equity) * 0.6 + FMath::Max(0.0, Earnings)) * (0.7 + A.Reputation / 160.0) / (A.SharesM * 1e6));
		A.SharePrice = FMath::Max(0.1, A.SharePrice + (Fair - A.SharePrice) * 0.08 + A.SharePrice * Rng.FRandRange(-0.015, 0.015));
		if (A.bFrequentFlyer) ADPEconomy::Pay(W, A.Id, To, TEXT("marketing"), -A.FfMembers * 2.0 / 365.0, TEXT("Frequent-flyer scheme"));
		if (A.HedgeKg > 0.0 && A.HedgeUntil < To) { A.HedgeKg = 0.0; if (A.bPlayer) Note(W, To, TEXT("finance"), 0, TEXT("Fuel hedge ended"), TEXT("Your fuel hedge has run out: fuel is bought at the market price again.")); }
	}
	// Monday: three new weekly challenges (the old ones paid if done)
	FADPAirline* Me = W.Airlines.FindByPredicate([](const FADPAirline& X) { return X.bPlayer; });
	if (!Me) return;
	for (FADPChallenge& C : W.Challenges)
	{
		if (C.bDone) continue;
		const double Got = C.Kind == TEXT("pax") ? Me->WeekPax : C.Kind == TEXT("legs") ? Me->WeekLegs : C.Kind == TEXT("ontime") ? (Me->WeekLegs > 0 ? 100.0 * Me->WeekOnTime / Me->WeekLegs : 0.0)
			: C.Kind == TEXT("airports") ? W.Career.Airports.Num() - C.Base : C.Kind == TEXT("profit") ? Me->Balance - C.Base : 0.0;
		C.Progress = Got;
		if (Got >= C.Target && (C.Kind != TEXT("ontime") || Me->WeekLegs >= 10))
		{
			C.bDone = true;
			ADPEconomy::Pay(W, Me->Id, To, TEXT("charter"), C.Money, FString::Printf(TEXT("Challenge won: %s"), *C.Title));
			W.Career.Xp += C.Xp; W.Career.Badges.AddUnique(TEXT("challenge-") + C.Kind);
			Note(W, To, TEXT("company"), 1, TEXT("Challenge won"), FString::Printf(TEXT("%s - $%.0f and %.0f XP."), *C.Title, C.Money, C.Xp));
		}
	}
	if (To.GetDayOfWeek() == EDayOfWeek::Monday || W.Challenges.Num() == 0)
	{
		W.Challenges.Reset(); Me->WeekPax = 0; Me->WeekLegs = 0; Me->WeekOnTime = 0;
		const double Size = FMath::Max(1.0, Me->Legs30 / 30.0 * 7.0);   // (a week's legs at today's pace)
		struct FC { const TCHAR* Kind; FString Title; double Target; double Money; };
		TArray<FC> Pool = {
			{TEXT("pax"), FString::Printf(TEXT("Carry %.0f passengers this week"), FMath::RoundToDouble(FMath::Max(300.0, Size * 140.0) / 50.0) * 50.0), FMath::RoundToDouble(FMath::Max(300.0, Size * 140.0) / 50.0) * 50.0, 150000.0},
			{TEXT("ontime"), TEXT("95% on time this week (10 legs or more)"), 95.0, 200000.0},
			{TEXT("legs"), FString::Printf(TEXT("Fly %.0f legs this week"), FMath::Max(10.0, FMath::RoundToDouble(Size * 1.15))), FMath::Max(10.0, FMath::RoundToDouble(Size * 1.15)), 120000.0},
			{TEXT("airports"), TEXT("Fly to 2 airports you have never served"), 2.0, 180000.0},
			{TEXT("profit"), TEXT("Make $500,000 profit this week"), 500000.0, 250000.0}};
		for (int32 i = 0; i < 3 && Pool.Num(); ++i)
		{
			const int32 k = Rng.RandRange(0, Pool.Num() - 1); const FC P = Pool[k]; Pool.RemoveAt(k);
			FADPChallenge C; C.Kind = P.Kind; C.Title = P.Title; C.Target = P.Target; C.Money = P.Money; C.Xp = P.Money / 1000.0; C.Until = To + FTimespan::FromDays(7.0);
			C.Base = C.Kind == TEXT("airports") ? W.Career.Airports.Num() : C.Kind == TEXT("profit") ? Me->Balance : 0.0;
			W.Challenges.Add(C);
		}
		FString L; for (const FADPChallenge& C : W.Challenges) L += TEXT("\n- ") + C.Title;
		Note(W, To, TEXT("company"), 0, TEXT("This week's challenges"), TEXT("New challenges until next Monday:") + L);
	}
}
