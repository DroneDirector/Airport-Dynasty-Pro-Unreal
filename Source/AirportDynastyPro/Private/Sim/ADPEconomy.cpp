// The money - see Sim/ADPEconomy.h.
#include "Sim/ADPEconomy.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPNetwork.h"

namespace
{
	// the difficulty: income and costs (cruise .. extreme)
	void Difficulty(const FString& D, double& Income, double& Cost)
	{
		if (D == TEXT("cruise")) { Income = 1.3; Cost = 0.75; }
		else if (D == TEXT("easy")) { Income = 1.15; Cost = 0.88; }
		else if (D == TEXT("hard")) { Income = 0.9; Cost = 1.1; }
		else if (D == TEXT("extreme")) { Income = 0.78; Cost = 1.22; }
		else { Income = 1.0; Cost = 1.0; }
	}
	double MaintPerHour(const FString& Class)
	{
		return Class == TEXT("widebody") ? 2400.0 : Class == TEXT("regional") ? 600.0 : Class == TEXT("turboprop") ? 420.0 : Class == TEXT("bush") ? 160.0 : Class == TEXT("helicopter") ? 520.0 : 950.0;
	}
	double SizeFee(const FADPAirportInfo* A, double L, double M, double S) { return !A ? M : A->Kind == 'L' ? L : A->Kind == 'M' ? M : S; }
}

double ADPEconomy::FuelPrice(const FString& Icao, const FDateTime& Now)
{
	// about 0.80 USD/kg of jet fuel; dearer at small and remote fields, a little cheaper at the big hubs; a small daily wobble
	const FADPAirportInfo* A = ADPWorldData::Airport(Icao);
	const double Size = !A ? 1.04 : A->Kind == 'L' ? 0.97 : A->Kind == 'M' ? 1.03 : 1.13;
	FRandomStream R(int32(GetTypeHash(Icao) ^ Now.GetDayOfYear() * 977));
	return 0.80 * Size * R.FRandRange(0.96, 1.04);
}

void ADPEconomy::Pay(FADPWorld& W, const FString& AirlineId, const FDateTime& At, const TCHAR* Kind, double Amount, const FString& What, const FString& Link)
{
	FADPTxn T; T.At = At; T.Kind = Kind; T.Amount = Amount; T.What = What; T.Link = Link;
	W.Ledger.Add(T);
	if (W.Ledger.Num() > 20000) W.Ledger.RemoveAt(0, W.Ledger.Num() - 20000);   // (the newest 20,000 lines kept; the totals live on in the reports)
	if (FADPAirline* A = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == AirlineId; })) A->Balance += Amount;
}

void ADPEconomy::BookLeg(FADPWorld& W, const FADPFlight& F, const FDateTime& At)
{
	const FADPTypeInfo* T = ADPWorldData::Type(F.TypeId);
	if (!T) return;
	const FADPAirportInfo* A = ADPWorldData::Airport(F.From); const FADPAirportInfo* B = ADPWorldData::Airport(F.To);
	double KI, KC; Difficulty(W.Difficulty, KI, KC);
	const double BlockH = (F.DurMin + F.HoldMin) / 60.0, Nm = A && B ? ADPWorldData::DistanceNm(A->Lat, A->Lon, B->Lat, B->Lon) : 0.0;
	const double MtowT = T->Mtow / 1000.0;
	const int32 Pax = F.Pax + F.PaxJ + F.PaxF;
	const bool bIntl = A && B && A->Country != B->Country;
	const FString Leg = FString::Printf(TEXT("%s %s-%s"), *F.Reg, *F.From, *F.To);
	const double Before = W.Airlines.ContainsByPredicate([&](const FADPAirline& X) { return X.Id == F.AirlineId; }) ? W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == F.AirlineId; })->Balance : 0.0;
	// income
	Pay(W, F.AirlineId, At, TEXT("revenue"), F.Revenue * KI, Leg + TEXT(": tickets and cargo"), F.Id);
	// fuel: the type's burn for the time flown (taxiing burns little), at the departure airport's price
	const double FuelKg = T->BurnKgH * BlockH * 0.92;
	// (hedged fuel first, already paid for - to-do 185)
	Pay(W, F.AirlineId, At, TEXT("fuel"), -ADPNetwork::FuelCost(W, F.AirlineId, FuelKg, FuelPrice(F.From, At) * W.FuelIndex) * KC, FString::Printf(TEXT("%s: fuel %.0f kg"), *Leg, FuelKg), F.Id);
	// fees: landing by weight and airport size, parking, handling per passenger and per turnaround, air navigation by
	// distance and weight
	const double Landing = MtowT * SizeFee(B, 9.0, 6.5, 4.5);
	const double Parking = MtowT * SizeFee(B, 1.8, 1.2, 0.7) + SizeFee(B, 120.0, 60.0, 30.0);
	const double Handling = Pax * (bIntl ? 9.5 : 6.5) + SizeFee(B, 850.0, 400.0, 160.0);
	const double Nav = Nm * 1.852 / 100.0 * FMath::Sqrt(MtowT / 50.0) * 78.0;
	Pay(W, F.AirlineId, At, TEXT("fees"), -(Landing + Parking + Handling + Nav) * KC, FString::Printf(TEXT("%s: landing $%.0f, parking $%.0f, handling $%.0f, navigation $%.0f"), *Leg, Landing, Parking, Handling, Nav), F.Id);
	// the maintenance reserve, crew allowances, booking costs and overhead
	// (45% of the maintenance reserve is parts: it follows the parts market)
	// (your airline: only line maintenance here - the checks, parts and mechanics are paid for themselves: to-do 181)
	Pay(W, F.AirlineId, At, TEXT("maint"), -MaintPerHour(T->Class) * BlockH * (0.55 + 0.45 * W.PartsIndex) * KC * (F.AirlineId == UADPSim::Me() ? 0.3 : 1.0), Leg + (F.AirlineId == UADPSim::Me() ? TEXT(": line maintenance") : TEXT(": maintenance reserve")), F.Id);
	const int32 Crew = 2 + FMath::CeilToInt(T->Seats / 50.0);
	Pay(W, F.AirlineId, At, TEXT("crew"), -Crew * (BlockH > 6.0 ? 250.0 : 115.0) * KC, FString::Printf(TEXT("%s: allowances for %d crew"), *Leg, Crew), F.Id);
	Pay(W, F.AirlineId, At, TEXT("fees"), -F.Revenue * (0.055 + 0.07) * KC, Leg + TEXT(": bookings and overhead"), F.Id);
	if (FADPRoute* R = W.Routes.FindByPredicate([&](const FADPRoute& X) { return X.Id == F.RouteId; }))
	{
		const FADPAirline* Al = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == F.AirlineId; });
		const double Net = (Al ? Al->Balance : 0.0) - Before;
		// (the aircraft's own monthly cost, spread over its legs: about 60 legs a month)
		const FADPAircraft* C = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == F.Reg; });
		const double Own = C ? (C->Ownership == TEXT("leased") ? C->LeaseMonthly : C->Value * 0.0052) / 60.0 : 0.0;
		R->ProfitEma = R->Legs ? R->ProfitEma * 0.85 + (Net - Own) * 0.15 : Net - Own;
		R->LoadEma = R->Legs ? R->LoadEma * 0.85 + F.LoadFactor * 0.15 : F.LoadFactor;
		++R->Legs;
	}
}

double ADPEconomy::LeaseRate(const FString& TypeId)
{
	const FADPTypeInfo* T = ADPWorldData::Type(TypeId);
	return T ? T->ValueM * 1e6 * 0.007 : 0.0;   // (an A320neo: about $385,000 a month)
}

void ADPEconomy::TakeLoan(FADPWorld& W, const FString& AirlineId, double Amount, double RateYear, int32 Months, const FDateTime& At)
{
	FADPLoan L; L.AirlineId = AirlineId; L.Owed = Amount; L.RateYear = RateYear; L.MonthsLeft = FMath::Max(1, Months);
	const double R = RateYear / 12.0;
	L.Monthly = R > 0.0 ? Amount * R / (1.0 - FMath::Pow(1.0 + R, -double(L.MonthsLeft))) : Amount / L.MonthsLeft;
	W.Loans.Add(L);
	Pay(W, AirlineId, At, TEXT("loan"), Amount, FString::Printf(TEXT("Loan of $%.0f at %.1f%% over %d months"), Amount, RateYear * 100.0, Months));
}

namespace
{
	// the 1st of the month: leases, insurance, base rent, loan instalments; owned aircraft lose a little value
	void MonthEnd(FADPWorld& W, const FDateTime& At)
	{
		double KI, KC; Difficulty(W.Difficulty, KI, KC);
		for (FADPAircraft& A : W.Aircraft)
		{
			const FADPTypeInfo* T = ADPWorldData::Type(A.TypeId);
			if (A.Value <= 0.0 && T) A.Value = T->ValueM * 1e6;
			if (A.Ownership == TEXT("leased"))
			{
				if (A.LeaseMonthly <= 0.0) A.LeaseMonthly = ADPEconomy::LeaseRate(A.TypeId);
				ADPEconomy::Pay(W, A.AirlineId, At, TEXT("lease"), -A.LeaseMonthly * KC, FString::Printf(TEXT("%s: lease for the month"), *A.Reg), A.Reg);
			}
			else A.Value *= 0.9948;   // (about 6% a year)
			// insurance (0.05% of the value a month) and the base's hangar and stand rent by size
			ADPEconomy::Pay(W, A.AirlineId, At, TEXT("insurance"), -A.Value * 0.0005 * KC, FString::Printf(TEXT("%s: insurance"), *A.Reg), A.Reg);
			const double Rent = !T ? 12000.0 : T->Class == TEXT("widebody") ? 38000.0 : T->Class == TEXT("turboprop") || T->Class == TEXT("regional") ? 9000.0 : T->Class == TEXT("bush") || T->Class == TEXT("helicopter") ? 3500.0 : 15000.0;
			ADPEconomy::Pay(W, A.AirlineId, At, TEXT("fees"), -Rent * KC, FString::Printf(TEXT("%s: hangar and stand at %s"), *A.Reg, *A.Base), A.Reg);
		}
		for (int32 i = W.Loans.Num() - 1; i >= 0; --i)
		{
			FADPLoan& L = W.Loans[i];
			const double Interest = L.Owed * L.RateYear / 12.0, Repay = FMath::Min(L.Owed, L.Monthly - Interest);
			ADPEconomy::Pay(W, L.AirlineId, At, TEXT("loan"), -(Interest + Repay), FString::Printf(TEXT("Loan instalment: interest $%.0f, repaid $%.0f"), Interest, Repay));
			L.Owed -= Repay; --L.MonthsLeft;
			if (L.MonthsLeft <= 0 || L.Owed < 1.0) W.Loans.RemoveAt(i);
		}
	}
}

void FADPEconomyModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
	FADPWorld& WM = Sim.World;
	// the markets: each hour the fuel and parts prices move 5% of the way to where the market is heading, which itself
	// drifts back to normal; every 3-10 days some news moves it (most rises are announced a day or two ahead)
	WM.FuelIndex += (WM.FuelTarget - WM.FuelIndex) * 0.05; WM.PartsIndex += (WM.PartsTarget - WM.PartsIndex) * 0.05;
	WM.FuelTarget += (1.0 - WM.FuelTarget) * 0.004; WM.PartsTarget += (1.0 - WM.PartsTarget) * 0.004;
	if (WM.NextMarketNews == FDateTime()) WM.NextMarketNews = To + FTimespan::FromDays(Rng.FRandRange(3.0, 10.0));
	if (To >= WM.NextMarketNews)
	{
		static const TCHAR* Up[] = {TEXT("Refinery outage on the Gulf Coast"), TEXT("OPEC+ extends output cuts"), TEXT("Pipeline disruption in the North Sea"), TEXT("Strong summer travel lifts jet fuel demand")};
		static const TCHAR* Down[] = {TEXT("Crude stockpiles build faster than expected"), TEXT("New refinery capacity comes online"), TEXT("Mild winter cuts heating oil demand"), TEXT("Production quotas raised")};
		const bool bUp = Rng.FRand() < 0.5; const bool bParts = Rng.FRand() < 0.3;
		const double Move = bUp ? Rng.FRandRange(1.08, 1.3) : Rng.FRandRange(0.78, 0.94);
		double& Target = bParts ? WM.PartsTarget : WM.FuelTarget;
		Target = FMath::Clamp(Target * Move, 0.55, 2.2);
		FADPNotice N; N.At = To; N.Kind = TEXT("finance");
		N.Text = bParts ? FString::Printf(TEXT("Parts market: suppliers %s prices (%+.0f%%)"), bUp ? TEXT("raise") : TEXT("cut"), (Move - 1.0) * 100.0)
			: FString::Printf(TEXT("Fuel market: %s (%+.0f%%)"), bUp ? Up[Rng.RandRange(0, 3)] : Down[Rng.RandRange(0, 3)], (Move - 1.0) * 100.0);
		WM.Notices.Add(N); ++MarketNews;
		WM.NextMarketNews = To + FTimespan::FromDays(Rng.FRandRange(3.0, 10.0));
	}
	WM.FuelIndex = FMath::Clamp(WM.FuelIndex, 0.55, 2.2); WM.PartsIndex = FMath::Clamp(WM.PartsIndex, 0.55, 2.2);
	// the used market: a new list every Monday
	if (WM.UsedMarket.Num() == 0 || (To.GetDayOfWeek() == EDayOfWeek::Monday && To.GetHour() == 1))
	{
		WM.UsedMarket.Reset();
		const TArray<FADPTypeInfo>& Ts = ADPWorldData::Types();
		for (int32 i = 0; Ts.Num() && i < 8; ++i)
		{
			const FADPTypeInfo& T = Ts[Rng.RandRange(0, Ts.Num() - 1)];
			FADPUsedOffer O; O.TypeId = T.Id; O.AgeYears = Rng.RandRange(2, 22); O.Condition = Rng.FRandRange(55.0, 95.0);
			O.Price = T.ValueM * 1e6 * FMath::Max(0.12, FMath::Pow(0.93, O.AgeYears)) * (0.55 + 0.45 * O.Condition / 100.0);
			WM.UsedMarket.Add(O);
		}
	}
	// campaigns that ended
	WM.Campaigns.RemoveAll([&](const FADPCampaign& C) { return C.Until <= To; });
	if (To.GetDay() == 1 && To.GetHour() == 0 && From.GetDay() != 1) { MonthEnd(Sim.World, To); ++MonthEnds; }
	// payday: the hour that passes Monday 00:00 UTC - every person's week's salary, from their airline
	const bool bPayday = To.GetDayOfWeek() == EDayOfWeek::Monday && To.GetHour() == 0 && From.GetDayOfWeek() != EDayOfWeek::Monday;
	if (!bPayday) return;
	++Paydays;
	FADPWorld& W = Sim.World;
	TMap<FString, double> Total; TMap<FString, int32> Count;
	for (const FADPPerson& P : W.People)
	{
		if (P.AirlineId.IsEmpty()) continue;
		const double Pay = P.Status == TEXT("leave") ? P.Salary * 0.6 : P.Salary;
		Total.FindOrAdd(P.AirlineId) += Pay; ++Count.FindOrAdd(P.AirlineId);
	}
	double KI, KC; Difficulty(W.Difficulty, KI, KC);
	for (const auto& It : Total)
		ADPEconomy::Pay(W, It.Key, To, TEXT("salary"), -It.Value * KC, FString::Printf(TEXT("Wages for the week: %d people"), Count[It.Key]));
}

FString ADPEconomy::BuyAircraft(FADPWorld& W, const FString& AirlineId, const FString& TypeId, bool bLease, const FString& Base, const FDateTime& At)
{
	const FADPTypeInfo* T = ADPWorldData::Type(TypeId);
	FADPAirline* Al = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == AirlineId; });
	if (!T || !Al) return FString();
	const double Price = T->ValueM * 1e6, Rent = LeaseRate(TypeId);
	const double Now = bLease ? Rent * 2.0 : Price;
	if (Al->Balance < Now) return FString();
	FADPAircraft A; A.Reg = FString::Printf(TEXT("%s-%c%c%c"), Al->Icao.IsEmpty() ? TEXT("XX") : *Al->Icao.Left(2), TCHAR('A' + (W.NextReg / 676) % 26), TCHAR('A' + (W.NextReg / 26) % 26), TCHAR('A' + W.NextReg % 26)); ++W.NextReg;
	A.TypeId = TypeId; A.AirlineId = AirlineId; A.Base = A.Location = Base; A.Value = Price; A.Condition = 100.0;
	A.Ownership = bLease ? TEXT("leased") : TEXT("owned"); A.LeaseMonthly = bLease ? Rent : 0.0;
	W.Aircraft.Add(A);
	Pay(W, AirlineId, At, bLease ? TEXT("lease") : TEXT("purchase"), -Now, FString::Printf(TEXT("%s %s (%s)"), bLease ? TEXT("Lease deposit for") : TEXT("Bought"), *T->Name, *A.Reg), A.Reg);
	return A.Reg;
}

FString ADPEconomy::BuyUsed(FADPWorld& W, const FString& AirlineId, int32 Offer, const FString& Base, const FDateTime& At)
{
	if (!W.UsedMarket.IsValidIndex(Offer)) return FString();
	const FADPUsedOffer O = W.UsedMarket[Offer];
	FADPAirline* Al = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == AirlineId; });
	if (!Al || Al->Balance < O.Price) return FString();
	FADPAircraft A; A.Reg = FString::Printf(TEXT("%s-U%02d"), Al->Icao.IsEmpty() ? TEXT("XX") : *Al->Icao.Left(2), W.NextReg++ % 100);
	A.TypeId = O.TypeId; A.AirlineId = AirlineId; A.Base = A.Location = Base; A.Value = O.Price; A.Condition = O.Condition; A.HoursMin = O.AgeYears * 3000.0 * 60.0;
	W.Aircraft.Add(A); W.UsedMarket.RemoveAt(Offer);
	Pay(W, AirlineId, At, TEXT("purchase"), -O.Price, FString::Printf(TEXT("Bought a used %s, %d years old (%s)"), *O.TypeId, O.AgeYears, *A.Reg), A.Reg);
	return A.Reg;
}

bool ADPEconomy::SellAircraft(FADPWorld& W, const FString& Reg, const FDateTime& At)
{
	const int32 I = W.Aircraft.IndexOfByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; });
	if (I == INDEX_NONE || W.Aircraft[I].Status == TEXT("flying")) return false;
	const FADPAircraft A = W.Aircraft[I];
	if (A.Ownership == TEXT("leased")) Pay(W, A.AirlineId, At, TEXT("lease"), -A.LeaseMonthly * 3.0, FString::Printf(TEXT("%s handed back early: three months' rent"), *Reg), Reg);
	else Pay(W, A.AirlineId, At, TEXT("sale"), A.Value * (0.55 + 0.45 * A.Condition / 100.0) * 0.94, FString::Printf(TEXT("%s sold (6%% to the broker)"), *Reg), Reg);
	W.Aircraft.RemoveAt(I);
	return true;
}

void ADPEconomy::StartCampaign(FADPWorld& W, const FString& AirlineId, const FString& RouteId, int32 Weeks, const FDateTime& At)
{
	FADPCampaign C; C.AirlineId = AirlineId; C.RouteId = RouteId; C.Boost = RouteId.IsEmpty() ? 0.12 : 0.25; C.Until = At + FTimespan::FromDays(7.0 * FMath::Clamp(Weeks, 1, 4));
	W.Campaigns.Add(C);
	const double Cost = (RouteId.IsEmpty() ? 60000.0 : 15000.0) * FMath::Clamp(Weeks, 1, 4);
	Pay(W, AirlineId, At, TEXT("marketing"), -Cost, FString::Printf(TEXT("Marketing campaign, %d weeks (%s)"), Weeks, RouteId.IsEmpty() ? TEXT("the whole airline") : *RouteId));
}

double ADPEconomy::CampaignBoost(const FADPWorld& W, const FString& AirlineId, const FString& RouteId, const FDateTime& At)
{
	double B = 0.0;
	for (const FADPCampaign& C : W.Campaigns) if (C.AirlineId == AirlineId && C.Until > At && (C.RouteId.IsEmpty() || C.RouteId == RouteId)) B += C.Boost;
	return FMath::Min(B, 0.35);
}
