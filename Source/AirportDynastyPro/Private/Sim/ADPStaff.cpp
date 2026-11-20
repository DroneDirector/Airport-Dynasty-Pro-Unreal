// Staff - see Sim/ADPStaff.h.
#include "Sim/ADPStaff.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPWorldData.h"

namespace
{
	struct FNames { const TCHAR* Countries; TArray<const TCHAR*> Men, Women, Last; };
	const TArray<FNames>& Names()
	{
		static const TArray<FNames> N = {
			{TEXT("FO IS DK NO SE FI GL"), {TEXT("Jóan"), TEXT("Rógvi"), TEXT("Sigurd"), TEXT("Gunnar"), TEXT("Einar"), TEXT("Magnus"), TEXT("Lars"), TEXT("Mikkel"), TEXT("Jónas"), TEXT("Óli")},
				{TEXT("Guðrun"), TEXT("Sigrid"), TEXT("Ragna"), TEXT("Elin"), TEXT("Hanna"), TEXT("Katrin"), TEXT("Sunneva"), TEXT("Ingrid"), TEXT("Birna"), TEXT("Anna")},
				{TEXT("Joensen"), TEXT("Hansen"), TEXT("Jacobsen"), TEXT("Olsen"), TEXT("Petersen"), TEXT("Sigurðsson"), TEXT("Jónsdóttir"), TEXT("Dam"), TEXT("Nielsen"), TEXT("Larsen")}},
			{TEXT("GB IE US CA AU NZ"), {TEXT("James"), TEXT("Oliver"), TEXT("Thomas"), TEXT("Daniel"), TEXT("Michael"), TEXT("Ryan"), TEXT("Liam"), TEXT("Jack")},
				{TEXT("Emily"), TEXT("Sarah"), TEXT("Grace"), TEXT("Chloe"), TEXT("Olivia"), TEXT("Megan"), TEXT("Hannah"), TEXT("Kate")},
				{TEXT("Smith"), TEXT("Jones"), TEXT("Taylor"), TEXT("Brown"), TEXT("Walker"), TEXT("Murphy"), TEXT("Campbell"), TEXT("Evans")}},
			{TEXT("DE AT CH NL BE LU"), {TEXT("Lukas"), TEXT("Jonas"), TEXT("Felix"), TEXT("Daan"), TEXT("Luca"), TEXT("Stefan")},
				{TEXT("Lena"), TEXT("Anna"), TEXT("Mia"), TEXT("Sophie"), TEXT("Lotte"), TEXT("Julia")},
				{TEXT("Müller"), TEXT("Schmidt"), TEXT("de Vries"), TEXT("Meier"), TEXT("Weber"), TEXT("Janssen")}},
			{TEXT("FR ES IT PT"), {TEXT("Lucas"), TEXT("Pablo"), TEXT("Marco"), TEXT("João"), TEXT("Hugo"), TEXT("Matteo")},
				{TEXT("Camille"), TEXT("Lucía"), TEXT("Giulia"), TEXT("Inês"), TEXT("Léa"), TEXT("Sofia")},
				{TEXT("Martin"), TEXT("García"), TEXT("Rossi"), TEXT("Silva"), TEXT("Bernard"), TEXT("Ferrari")}},
		};
		return N;
	}
	// what a job pays a week (USD), before the region's factor
	double BaseWage(const FString& Role)
	{
		static const TMap<FString, double> W = {{TEXT("captain"), 2950}, {TEXT("fo"), 1550}, {TEXT("fa"), 640}, {TEXT("mech"), 1150}, {TEXT("ground"), 580},
			{TEXT("security"), 700}, {TEXT("gate"), 620}, {TEXT("checkin"), 560}, {TEXT("cleaning"), 480}, {TEXT("catering"), 520}, {TEXT("fuel"), 650},
			{TEXT("bus"), 600}, {TEXT("fire"), 900}, {TEXT("tower"), 1700}, {TEXT("ops"), 1100}};
		const double* V = W.Find(Role); return V ? *V : 600.0;
	}
	double Region(const FString& C)
	{
		if (FString(TEXT("NO CH IS FO DK LU US")).Contains(C)) return 1.12;
		if (FString(TEXT("SE FI DE NL AT BE GB IE CA AU NZ GL")).Contains(C)) return 1.0;
		if (FString(TEXT("FR ES IT PT")).Contains(C)) return 0.88;
		return 0.7;
	}
}

double ADPStaff::GoingRate(const FString& Role, const FString& Country) { return BaseWage(Role) * Region(Country); }

FADPPerson ADPStaff::MakePerson(const FString& Role, const FString& Country, FRandomStream& Rng)
{
	const FNames* Pool = &Names()[0];
	for (const FNames& N : Names()) if (FString(N.Countries).Contains(Country)) { Pool = &N; break; }
	FADPPerson P;
	// (some jobs lean one way, as in real life; none is closed to anyone)
	const double WomanShare = Role == TEXT("fa") ? 0.75 : Role == TEXT("captain") || Role == TEXT("fo") ? 0.12 : Role == TEXT("mech") || Role == TEXT("fire") ? 0.1 : 0.45;
	P.bWoman = Rng.FRand() < WomanShare;
	const TArray<const TCHAR*>& First = P.bWoman ? Pool->Women : Pool->Men;
	P.Name = FString::Printf(TEXT("%s %s"), First[Rng.RandRange(0, First.Num() - 1)], Pool->Last[Rng.RandRange(0, Pool->Last.Num() - 1)]);
	P.Id = FString::Printf(TEXT("p%08x"), Rng.GetUnsignedInt());
	P.Role = Role; P.Age = Rng.RandRange(Role == TEXT("captain") ? 32 : 21, 60);
	P.Rating = FMath::RoundToDouble(Rng.FRandRange(3.6, 5.0) * 10.0) / 10.0;
	P.Salary = FMath::RoundToDouble(GoingRate(Role, Country) * Rng.FRandRange(0.9, 1.15));
	P.Mood = Rng.FRandRange(60.0, 85.0);
	P.Status = TEXT("pool");
	if (Role == TEXT("captain") || Role == TEXT("fo")) P.Types.Add(TEXT("A320"));
	return P;
}

bool ADPStaff::Hire(FADPWorld& W, const FString& PersonId, const FString& AirlineId, const FString& Base, const FDateTime& At)
{
	FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == PersonId && X.AirlineId.IsEmpty(); });
	if (!P) return false;
	P->AirlineId = AirlineId; P->Base = Base; P->Status = TEXT("on");
	ADPEconomy::Pay(W, AirlineId, At, TEXT("training"), -P->Salary * (P->Role == TEXT("captain") ? 1.5 : 1.0), FString::Printf(TEXT("Hiring %s (%s)"), *P->Name, *P->Role), P->Id);
	return true;
}

FString ADPStaff::CountryOf(const FADPWorld& W, const FString& AirlineId)
{
	const FADPAirline* A = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return AirlineId.IsEmpty() ? X.bPlayer : X.Id == AirlineId; });
	const FADPAirportInfo* H = A ? ADPWorldData::Airport(A->Hub) : nullptr;
	return H ? H->Country : FString(TEXT("GB"));
}

int32 ADPStaff::CabinCrewFor(int32 Seats) { return Seats < 20 ? 0 : FMath::CeilToInt(Seats / 50.0); }

int32 ADPStaff::CabinCrewAt(const FADPWorld& W, const FString& AirlineId, const FString& Base)
{
	int32 N = 0;
	for (const FADPPerson& P : W.People) if (P.AirlineId == AirlineId && P.Role == TEXT("fa") && P.Base == Base && P.Status == TEXT("on")) ++N;
	return N;
}

void FADPStaffModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
	FADPWorld& W = Sim.World;
	// the hiring pool: every few hours some leave it and new people come (12 cabin crew and ground staff, 8 of the rest)
	if (Rng.FRand() < 0.2)
	{
		static const TCHAR* Roles[] = {TEXT("captain"), TEXT("fo"), TEXT("fa"), TEXT("mech"), TEXT("ground"), TEXT("security")};
		const FString Country = ADPStaff::CountryOf(W, FString());   // (the player's airline's hub country)
		for (const TCHAR* R : Roles)
		{
			int32 InPool = 0; for (const FADPPerson& P : W.People) if (P.AirlineId.IsEmpty() && P.Role == R) ++InPool;
			const int32 Want = FString(R) == TEXT("fa") || FString(R) == TEXT("ground") ? 12 : 8;
			if (InPool > 3 && Rng.FRand() < 0.3) { const int32 I = W.People.IndexOfByPredicate([&](const FADPPerson& P) { return P.AirlineId.IsEmpty() && P.Role == R; }); if (I != INDEX_NONE) W.People.RemoveAt(I); }
			for (int32 k = InPool; k < Want; ++k) W.People.Add(ADPStaff::MakePerson(R, Country, Rng));
		}
		++PoolChanges;
	}
	// the day's events at midnight UTC: sickness, getting better, morale, notice and leaving
	if (!(To.GetHour() == 0 && From.GetHour() != 0)) return;
	for (int32 i = W.People.Num() - 1; i >= 0; --i)
	{
		FADPPerson& P = W.People[i];
		if (P.AirlineId.IsEmpty()) continue;
		if (P.Status == TEXT("sick")) { if (P.RestUntil <= To) P.Status = TEXT("on"); continue; }
		// sickness: about 7% a month, more when unhappy; 1-14 days
		if (P.Status == TEXT("on") && Rng.FRand() < 0.0024 * (P.Mood < 35 ? 1.6 : 1.0))
		{ P.Status = TEXT("sick"); P.RestUntil = To + FTimespan::FromDays(Rng.RandRange(1, 14)); ++SickDays; continue; }
		// morale: pay above all (paid above the going rate: happier), drifting a little each day
		const double PayMood = FMath::Clamp(55.0 + (P.Salary / FMath::Max(1.0, ADPStaff::GoingRate(P.Role, ADPStaff::CountryOf(W, P.AirlineId))) - 1.0) * 160.0, 0.0, 100.0);
		P.Mood = FMath::Clamp(P.Mood + (PayMood - P.Mood) * 0.05 + Rng.FRandRange(-1.5, 1.5), 0.0, 100.0);
		// very unhappy: notice (4% a day under 20), gone after 14 days unless their mood is back to 30
		if (P.Status == TEXT("on") && P.Mood < 20.0 && Rng.FRand() < 0.04)
		{
			P.Status = TEXT("notice"); P.RestUntil = To + FTimespan::FromDays(14.0);
			FADPNotice N; N.At = To; N.Kind = TEXT("staff"); N.Text = FString::Printf(TEXT("%s (%s) has handed in their notice - 14 days"), *P.Name, *P.Role); W.Notices.Add(N);
		}
		else if (P.Status == TEXT("notice"))
		{
			if (P.Mood >= 30.0) P.Status = TEXT("on");
			else if (P.RestUntil <= To) { ++Quits; FADPNotice N; N.At = To; N.Kind = TEXT("staff"); N.Text = FString::Printf(TEXT("%s (%s) has left"), *P.Name, *P.Role); W.Notices.Add(N); W.People.RemoveAt(i); }
		}
	}
}
