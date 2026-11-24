// Staff - see Sim/ADPStaff.h.
#include "Sim/ADPStaff.h"
#include "Sim/ADPCrew.h"
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
			// (Switzerland round Chablais: French-speaking Vaud and Valais - its own pool first, with the Swiss German and
			// Italian names people there have too)
			{TEXT("CH"), {TEXT("Nicolas"), TEXT("Julien"), TEXT("Mathieu"), TEXT("Olivier"), TEXT("Sébastien"), TEXT("Yann"), TEXT("Loïc"), TEXT("Thomas"), TEXT("Laurent"), TEXT("Pascal"), TEXT("Raphaël"), TEXT("Damien"), TEXT("Reto"), TEXT("Urs"), TEXT("Marco"), TEXT("Luca")},
				{TEXT("Sophie"), TEXT("Camille"), TEXT("Céline"), TEXT("Valérie"), TEXT("Laura"), TEXT("Chloé"), TEXT("Mélanie"), TEXT("Nathalie"), TEXT("Isabelle"), TEXT("Aurélie"), TEXT("Sandrine"), TEXT("Manon"), TEXT("Nadia"), TEXT("Seraina"), TEXT("Giulia"), TEXT("Anna")},
				{TEXT("Favre"), TEXT("Rochat"), TEXT("Pittet"), TEXT("Bonvin"), TEXT("Morand"), TEXT("Dubois"), TEXT("Perrin"), TEXT("Cherix"), TEXT("Chappuis"), TEXT("Michaud"), TEXT("Rey"), TEXT("Fournier"), TEXT("Vuilleumier"), TEXT("Cottier"), TEXT("Pasche"), TEXT("Mottier"), TEXT("Gay"), TEXT("Berthoud"), TEXT("Jaquet"), TEXT("Reymond"), TEXT("Meier"), TEXT("Keller"), TEXT("Bernasconi"), TEXT("Rossi")}},
			{TEXT("DE AT NL BE LU"), {TEXT("Lukas"), TEXT("Jonas"), TEXT("Felix"), TEXT("Daan"), TEXT("Luca"), TEXT("Stefan")},
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
			{TEXT("bus"), 600}, {TEXT("taxi"), 600}, {TEXT("fire"), 900}, {TEXT("tower"), 1700}, {TEXT("ops"), 1100}};
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
	ADPStaff::Describe(P, Country, Rng);   // (hours, languages, home, strengths, what they ask for: to-do 179)
	return P;
}

bool ADPStaff::Hire(FADPWorld& W, const FString& PersonId, const FString& AirlineId, const FString& Base, const FDateTime& At)
{
	FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == PersonId && X.AirlineId.IsEmpty(); });
	if (!P) return false;
	P->AirlineId = AirlineId; P->Base = Base; P->Status = TEXT("on"); P->HiredAt = At; P->Location = Base;
	if (P->Asks > 0.0) P->Salary = P->Asks;
	if (const ADPCrew::FRole* R = ADPCrew::Role(P->Role); R && R->bShift) ADPCrew::GiveShift(W, *P);
	ADPEconomy::Pay(W, AirlineId, At, TEXT("training"), -P->Salary * (P->Role == TEXT("captain") ? 1.5 : 1.0), FString::Printf(TEXT("Hiring %s (%s)"), *P->Name, *ADPCrew::RoleName(P->Role)), P->Id);
	return true;
}

bool ADPStaff::Dismiss(FADPWorld& W, const FString& PersonId, const FDateTime& At)
{
	FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == PersonId && !X.AirlineId.IsEmpty(); });
	if (!P || P->Status == TEXT("dismissed")) return false;
	// severance: a week's pay for every year with you, at least two; their notice period from their contract (to-do 180)
	const double Years = P->HiredAt != FDateTime() ? (At - P->HiredAt).GetTotalDays() / 365.0 : 0.0;
	const double Weeks = FMath::Max(2.0, FMath::FloorToDouble(Years));
	ADPEconomy::Pay(W, P->AirlineId, At, TEXT("salary"), -P->Salary * Weeks, FString::Printf(TEXT("Severance: %s (%s)"), *P->Name, *ADPCrew::RoleName(P->Role)), P->Id);
	P->Status = TEXT("dismissed"); P->RestUntil = At + FTimespan::FromDays(FMath::Max(7, P->NoticeDays)); P->Mood = FMath::Min(P->Mood, 25.0);
	FADPNotice N; N.At = At; N.Kind = TEXT("staff"); N.Title = TEXT("Notice given");
	N.Text = FString::Printf(TEXT("%s (%s) has been given notice: off the roster now, leaves in %d days (their contract), %.0f weeks' pay as severance."), *P->Name, *ADPCrew::RoleName(P->Role), FMath::Max(7, P->NoticeDays), Weeks); W.Notices.Add(N);
	return true;
}

bool ADPStaff::SetPay(FADPWorld& W, const FString& PersonId, double Weekly)
{
	FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == PersonId && !X.AirlineId.IsEmpty(); });
	if (!P || Weekly <= 0.0) return false;
	P->Salary = FMath::Clamp(Weekly, GoingRate(P->Role, CountryOf(W, P->AirlineId)) * 0.5, GoingRate(P->Role, CountryOf(W, P->AirlineId)) * 3.0);
	return true;
}

bool ADPStaff::SetBase(FADPWorld& W, const FString& PersonId, const FString& Base)
{
	FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == PersonId && !X.AirlineId.IsEmpty(); });
	if (!P || !ADPWorldData::Airport(Base)) return false;
	P->Base = Base;
	return true;
}

void ADPStaff::FillPool(FADPWorld& W, FRandomStream& Rng)
{
	const FString Country = CountryOf(W, FString());
	for (const ADPCrew::FRole& Job : ADPCrew::Roles())   // (every job there is: ADPCrew.cpp)
	{
		const FString R = Job.Id;
		int32 InPool = 0; for (const FADPPerson& P : W.People) if (P.AirlineId.IsEmpty() && P.Role == R) ++InPool;
		int32 Want = R == TEXT("fa") || R == TEXT("ground") ? 12 : 8;
		if (W.Difficulty == TEXT("test")) Want *= 5;   // (the test: plenty of people to hire for everything)
		for (int32 k = InPool; k < Want; ++k) W.People.Add(MakePerson(R, Country, Rng));
	}
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
		const FString Country = ADPStaff::CountryOf(W, FString());   // (the player's airline's hub country)
		for (const ADPCrew::FRole& Job : ADPCrew::Roles())   // (every job there is: ADPCrew.cpp)
		{
			const FString R = Job.Id;
			int32 InPool = 0; for (const FADPPerson& P : W.People) if (P.AirlineId.IsEmpty() && P.Role == R) ++InPool;
			const int32 Want = R == TEXT("fa") || R == TEXT("ground") ? 12 : 8;
			if (InPool > 3 && Rng.FRand() < 0.3) { const int32 I = W.People.IndexOfByPredicate([&](const FADPPerson& P) { return P.AirlineId.IsEmpty() && P.Role == R; }); if (I != INDEX_NONE) W.People.RemoveAt(I); }
			for (int32 k = InPool; k < Want; ++k) W.People.Add(ADPStaff::MakePerson(R, Country, Rng));
		}
		++PoolChanges;
	}
	// the day's events at midnight UTC: sickness, getting better, morale, notice and leaving
	if (!(To.GetHour() == 0 && From.GetHour() != 0)) return;
	Daily(W, To);
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
			FADPNotice N; N.At = To; N.Kind = TEXT("staff"); N.Text = FString::Printf(TEXT("%s (%s) has handed in their notice - 14 days"), *P.Name, *ADPCrew::RoleName(P.Role)); W.Notices.Add(N);
		}
		else if (P.Status == TEXT("dismissed"))
		{
			if (P.RestUntil <= To) { FADPNotice N; N.At = To; N.Kind = TEXT("staff"); N.Text = FString::Printf(TEXT("%s (%s) has left after their notice"), *P.Name, *ADPCrew::RoleName(P.Role)); W.Notices.Add(N); W.People.RemoveAt(i); }
		}
		else if (P.Status == TEXT("notice"))
		{
			if (P.Mood >= 30.0) P.Status = TEXT("on");
			else if (P.RestUntil <= To) { ++Quits; FADPNotice N; N.At = To; N.Kind = TEXT("staff"); N.Text = FString::Printf(TEXT("%s (%s) has left"), *P.Name, *ADPCrew::RoleName(P.Role)); W.Notices.Add(N); W.People.RemoveAt(i); }
		}
	}
}

bool ADPStaff::StartTypeRating(FADPWorld& W, const FString& PersonId, const FString& TypeId, const FDateTime& At)
{
	FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == PersonId; });
	const FADPTypeInfo* T = ADPWorldData::Type(TypeId);
	if (!P || !T || P->AirlineId.IsEmpty() || P->Types.Contains(TypeId) || !P->Training.IsEmpty()) return false;
	const double Cost = T->Class == TEXT("widebody") ? 38000.0 : T->Class == TEXT("airliner") ? 22000.0 : T->Class == TEXT("regional") ? 15000.0 : T->Class == TEXT("turboprop") ? 9000.0 : 2500.0;
	const int32 Days = T->Class == TEXT("widebody") ? 4 : T->Class == TEXT("airliner") || T->Class == TEXT("regional") ? 3 : 1;
	P->Training = TypeId; P->TrainingUntil = At + FTimespan::FromDays(Days); P->Status = TEXT("training");
	ADPEconomy::Pay(W, P->AirlineId, At, TEXT("training"), -Cost, FString::Printf(TEXT("%s: %s type rating course (%d days)"), *P->Name, *T->Name, Days), P->Id);
	return true;
}

bool ADPStaff::OnStrike(const FADPWorld& W, const FString& AirlineId, const FString& Role)
{
	for (const FADPUnion& U : W.Unions) if (U.AirlineId == AirlineId && U.Role == Role && U.Stage == 4) return true;
	return false;
}

void ADPStaff::MeetUnion(FADPWorld& W, const FString& AirlineId, const FString& Role, bool bFully, const FDateTime& At)
{
	const FString C = CountryOf(W, AirlineId);
	for (FADPPerson& P : W.People)
		if (P.AirlineId == AirlineId && P.Role == Role)
		{
			const double Target = GoingRate(Role, C) * 1.05;
			if (P.Salary < Target) P.Salary = bFully ? Target : (P.Salary + Target) / 2.0;
			P.Mood = FMath::Min(100.0, P.Mood + (bFully ? 25.0 : 12.0));
		}
	for (FADPUnion& U : W.Unions) if (U.AirlineId == AirlineId && U.Role == Role) { U.Stage = 0; U.Cooldown = At + FTimespan::FromDays(21.0); }
}

void FADPStaffModule::Daily(FADPWorld& W, const FDateTime& To)
{
	ADPStaff::CareerDay(W, To, Rng);   // (promotions, raises, holidays: to-do 178 / 180)
	// type-rating courses ending: pass (likelier the better rated) or fail (a retake costs a third)
	for (FADPPerson& P : W.People)
		if (!P.Training.IsEmpty() && P.TrainingUntil <= To)
		{
			const bool bPass = Rng.FRand() < FMath::Clamp(0.62 + 0.075 * P.Rating, 0.8, 0.97);
			FADPNotice N; N.At = To; N.Kind = TEXT("pilot");
			N.Text = FString::Printf(TEXT("%s %s the %s type rating"), *P.Name, bPass ? TEXT("passed") : TEXT("failed"), *P.Training);
			W.Notices.Add(N);
			if (bPass) { P.Types.AddUnique(P.Training); ++Ratings; }
			P.Training.Empty(); P.Status = TEXT("on");
		}
	// unions: per airline and job (3 people or more) - paid under 86% of the going rate or very unhappy is a grievance;
	// after 3 days a warning (30% a day), 2 days later a final warning, 3 days later a strike (7 days, or until met)
	TMap<FString, TArray<FADPPerson*>> Groups;
	for (FADPPerson& P : W.People) if (!P.AirlineId.IsEmpty()) Groups.FindOrAdd(P.AirlineId + TEXT("|") + P.Role).Add(&P);
	for (auto& G : Groups)
	{
		if (G.Value.Num() < 3) continue;
		FString Al, Role; G.Key.Split(TEXT("|"), &Al, &Role);
		double PayK = 0, Mood = 0; const double Going = ADPStaff::GoingRate(Role, ADPStaff::CountryOf(W, Al));
		for (const FADPPerson* P : G.Value) { PayK += P->Salary / FMath::Max(1.0, Going); Mood += P->Mood; }
		PayK /= G.Value.Num(); Mood /= G.Value.Num();
		FADPUnion* U = W.Unions.FindByPredicate([&](const FADPUnion& X) { return X.AirlineId == Al && X.Role == Role; });
		if (!U) { FADPUnion N; N.AirlineId = Al; N.Role = Role; W.Unions.Add(N); U = &W.Unions.Last(); }
		const bool bGrievance = PayK < 0.86 || Mood < 28.0;
		auto Say = [&](const FString& T) { FADPNotice N; N.At = To; N.Kind = TEXT("staff"); N.Text = T; W.Notices.Add(N); };
		const double Days = (To - U->Since).GetTotalDays();
		if (U->Stage == 0 && bGrievance && To >= U->Cooldown) { U->Stage = 1; U->Since = To; }
		else if (U->Stage > 0 && U->Stage < 4 && !bGrievance) { U->Stage = 0; Say(FString::Printf(TEXT("The %s union's dispute is settled"), *Role)); }
		else if (U->Stage == 1 && Days >= 3 && Rng.FRand() < 0.3) { U->Stage = 2; U->Since = To; Say(FString::Printf(TEXT("The %s union warns of action: pay %.0f%% of the going rate, mood %.0f"), *Role, PayK * 100.0, Mood)); }
		else if (U->Stage == 2 && Days >= 2) { U->Stage = 3; U->Since = To; Say(FString::Printf(TEXT("Final warning from the %s union: a strike in 3 days unless their demands are met"), *Role)); }
		else if (U->Stage == 3 && Days >= 3) { U->Stage = 4; U->Since = To; ++Strikes; Say(FString::Printf(TEXT("The %s are ON STRIKE - their work stops"), *Role)); }
		else if (U->Stage == 4 && Days >= 7) { U->Stage = 0; U->Cooldown = To + FTimespan::FromDays(21.0); Say(FString::Printf(TEXT("The %s strike is over after a week"), *Role)); }
	}
	// conduct: on some days 1-3 reports (how often: Settings > Staff); light, moderate or severe - the person's rating
	// drops; about 3% of reports are crimes: a court case, its hearing in 2-6 weeks
	static const double DayChance[] = {0.0, 0.08, 0.17, 0.30, 0.55};
	if (Rng.FRand() < DayChance[FMath::Clamp(W.ConductLevel, 0, 4)])
	{
		TArray<FADPPerson*> Staff; for (FADPPerson& P : W.People) if (!P.AirlineId.IsEmpty() && P.Status == TEXT("on")) Staff.Add(&P);
		for (int32 n = Rng.RandRange(1, 3); n > 0 && Staff.Num(); --n)
		{
			FADPPerson* P = Staff[Rng.RandRange(0, Staff.Num() - 1)];
			const double Sev = Rng.FRand();
			static const TCHAR* Light[] = {TEXT("late for duty"), TEXT("rude to a passenger"), TEXT("uniform out of order")};
			static const TCHAR* Moderate[] = {TEXT("a heated argument with a colleague"), TEXT("left the post without cover"), TEXT("ignored a safety check")};
			static const TCHAR* Severe[] = {TEXT("came to work drunk"), TEXT("took items from the duty-free stock"), TEXT("threatened a passenger")};
			const TCHAR* What = Sev < 0.72 ? Light[Rng.RandRange(0, 2)] : Sev < 0.94 ? Moderate[Rng.RandRange(0, 2)] : Severe[Rng.RandRange(0, 2)];
			P->Rating = FMath::Max(0.0, P->Rating - (Sev < 0.72 ? 0.08 : Sev < 0.94 ? 0.15 : 0.3)); ++P->Warnings; ++Reports;
			FADPNotice N; N.At = To; N.Kind = TEXT("staff"); N.Text = FString::Printf(TEXT("Conduct report: %s (%s) - %s"), *P->Name, *P->Role, What); W.Notices.Add(N);
			if (Rng.FRand() < 0.03 || (Sev >= 0.94 && Rng.FRand() < 0.15))
			{
				FADPCase C; C.AirlineId = P->AirlineId; C.PersonId = P->Id; C.What = What; C.Hearing = To + FTimespan::FromDays(Rng.RandRange(14, 42)); W.Cases.Add(C); ++Trials;
			}
		}
	}
	// court hearings that are due: the verdict (guilty more likely the worse the record)
	for (FADPCase& C : W.Cases)
		if (C.Outcome.IsEmpty() && C.Hearing <= To)
		{
			const FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == C.PersonId; });
			const bool bGuilty = Rng.FRand() < 0.45 + 0.1 * (P ? FMath::Min(P->Warnings, 4) : 0);
			C.Outcome = bGuilty ? TEXT("guilty: a fine and a criminal record") : TEXT("not guilty");
			FADPNotice N; N.At = To; N.Kind = TEXT("authority"); N.Text = FString::Printf(TEXT("Court: %s - %s"), P ? *P->Name : TEXT("a former employee"), *C.Outcome); W.Notices.Add(N);
		}
}
