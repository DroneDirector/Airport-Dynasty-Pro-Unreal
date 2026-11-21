// Events, jobs, reputation, career, news and notifications - see Sim/ADPEvents.h.
#include "Sim/ADPEvents.h"
#include "Sim/ADPNetwork.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPWeather.h"

namespace
{
	struct FEvFamily { const TCHAR* Id; const TCHAR* Title; TArray<double> Tiers; };
	const TArray<FEvFamily>& EvFamilies()
	{
		static const TArray<FEvFamily> F = {
			{TEXT("legs"), TEXT("legs flown"), {1, 5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000, 25000, 50000, 100000}},
			{TEXT("pax"), TEXT("passengers carried"), {100, 500, 1e3, 5e3, 1e4, 2.5e4, 5e4, 1e5, 2.5e5, 5e5, 1e6, 2.5e6, 5e6, 1e7, 2.5e7, 5e7}},
			{TEXT("nm"), TEXT("nautical miles flown"), {1e3, 5e3, 1e4, 2.5e4, 5e4, 1e5, 2.5e5, 5e5, 1e6, 2.5e6, 5e6, 1e7}},
			{TEXT("hours"), TEXT("hours in the air"), {10, 50, 100, 250, 500, 1000, 2500, 5000, 10000, 25000, 50000, 100000}},
			{TEXT("airports"), TEXT("airports served"), {2, 5, 10, 25, 50, 100, 150, 200, 300, 400, 500}},
			{TEXT("countries"), TEXT("countries served"), {2, 5, 10, 20, 30, 50, 75, 100, 150}},
			{TEXT("revenue"), TEXT("dollars in tickets and cargo"), {1e6, 5e6, 1e7, 5e7, 1e8, 5e8, 1e9, 5e9, 1e10}},
			{TEXT("fleet"), TEXT("aircraft in the fleet"), {1, 2, 5, 10, 25, 50, 100, 200}},
			{TEXT("streak"), TEXT("on-time legs in a row"), {10, 25, 50, 100, 250, 500, 1000}},
			{TEXT("jobs"), TEXT("jobs from the board done"), {1, 5, 10, 25, 50, 100, 250}},
		};
		return F;
	}
	struct FEvAch { const TCHAR* Id; const TCHAR* Title; };
	const FEvAch EvAchievements[] = {
		{TEXT("first"), TEXT("Wheels Up - your first departure")}, {TEXT("fullhouse"), TEXT("Full House - every seat taken")},
		{TEXT("longhaul"), TEXT("Long Hauler - a leg of 5,000 nm")}, {TEXT("globe"), TEXT("Globetrotter - 50 countries")},
		{TEXT("punctual"), TEXT("Like Clockwork - 100 on-time legs in a row")}, {TEXT("rich"), TEXT("Deep Pockets - $100M in the bank")},
		{TEXT("debtfree"), TEXT("Debt Free - no loans and $50M in the bank")}, {TEXT("fleet10"), TEXT("Ten Tails - 10 aircraft")},
		{TEXT("fleet50"), TEXT("Fifty Tails - 50 aircraft")}, {TEXT("routes10"), TEXT("Network Builder - 10 routes")},
		{TEXT("weathered"), TEXT("Weathered It - a diversion landed safely")}, {TEXT("engineout"), TEXT("Calm Hands - an emergency landed safely")},
		{TEXT("rivaldown"), TEXT("Last One Standing - a rival went bankrupt")}, {TEXT("charter1"), TEXT("For Hire - your first job from the board")},
		{TEXT("charter10"), TEXT("Charter Pro - 10 jobs done")}, {TEXT("millionpax"), TEXT("A Million Smiles - 1,000,000 passengers")},
		{TEXT("rep80"), TEXT("Well Liked - reputation 80")}, {TEXT("rep95"), TEXT("Legendary - reputation 95")},
		{TEXT("widebody"), TEXT("Big Iron - a widebody in the fleet")}, {TEXT("polar"), TEXT("Midnight Sun - a landing north of the Arctic Circle")},
		{TEXT("equator"), TEXT("Line Crosser - a leg across the equator")}, {TEXT("anniversary"), TEXT("One Year On - a year at the helm")},
	};
	const double EvRankXp[] = {0, 500, 2000, 6000, 15000, 40000, 100000, 250000, 600000, 1500000};
	const TCHAR* EvRankNames[] = {TEXT("Trainee"), TEXT("Junior Manager"), TEXT("Station Manager"), TEXT("Operations Manager"), TEXT("Director"),
		TEXT("Vice President"), TEXT("Chief Operating Officer"), TEXT("Chief Executive"), TEXT("Tycoon"), TEXT("Dynasty")};
	// active volcanoes that close airspace now and then (name, lat, lon)
	struct FEvVolcano { const TCHAR* Name; double Lat, Lon; };
	const FEvVolcano EvVolcanoes[] = {{TEXT("the Reykjanes peninsula"), 63.9, -22.3}, {TEXT("Etna"), 37.75, 15.0}, {TEXT("Klyuchevskoy"), 56.06, 160.64},
		{TEXT("Merapi"), -7.54, 110.44}, {TEXT("Popocatepetl"), 19.02, -98.62}, {TEXT("Sakurajima"), 31.58, 130.66}, {TEXT("Mount Spurr"), 61.3, -152.25},
		{TEXT("Villarrica"), -39.42, -71.93}, {TEXT("Ruapehu"), -39.28, 175.57}, {TEXT("Taal"), 14.0, 120.99}, {TEXT("Katla"), 63.63, -19.05}};

	FADPAirline* EvPlayer(FADPWorld& W) { return W.Airlines.FindByPredicate([](const FADPAirline& A) { return A.bPlayer; }); }
	const FADPAirline* EvPlayer(const FADPWorld& W) { return W.Airlines.FindByPredicate([](const FADPAirline& A) { return A.bPlayer; }); }
	bool EvHits(const FADPWorldEvent& E, const FADPAirportInfo& Ap)
	{
		return !E.Country.IsEmpty() ? Ap.Country == E.Country : ADPWorldData::DistanceNm(E.Lat, E.Lon, Ap.Lat, Ap.Lon) <= E.RadiusNm;
	}
	bool EvActive(const FADPWorldEvent& E, const FDateTime& At) { return E.From <= At && At < E.Until; }
	FString EvMoney(double V) { return V >= 1e6 ? FString::Printf(TEXT("$%.1fM"), V / 1e6) : FString::Printf(TEXT("$%.0fk"), V / 1e3); }

	void EvAward(FADPWorld& W, const FDateTime& At, const FString& Id, const FString& Title, bool bAchievement)
	{
		TArray<FString>& List = bAchievement ? W.Career.Achievements : W.Career.Badges;
		if (List.Contains(Id)) return;
		List.Add(Id);
		W.Career.Xp += bAchievement ? 500.0 : 50.0;
		ADPEvents::Notify(W, At, bAchievement ? TEXT("achievement") : TEXT("badge"), bAchievement ? 1 : 0, bAchievement ? TEXT("Achievement") : TEXT("New badge"), Title);
	}
	void EvCheckRank(FADPWorld& W, const FDateTime& At, int32& Gifts)
	{
		int32 R = 0; for (int32 i = 0; i < UE_ARRAY_COUNT(EvRankXp); ++i) if (W.Career.Xp >= EvRankXp[i]) R = i;
		while (W.Career.Rank < R)
		{
			++W.Career.Rank;
			FADPAirline* Me = EvPlayer(W);
			const double Bonus = 10000.0 * W.Career.Rank;
			if (Me) ADPEconomy::Pay(W, Me->Id, At, TEXT("gift"), Bonus, FString::Printf(TEXT("The board's bonus: promoted to %s"), EvRankNames[W.Career.Rank]));
			ADPEvents::Notify(W, At, TEXT("phone"), 1, TEXT("The board"), FString::Printf(TEXT("Congratulations - you are now %s. A bonus of %s is on its way."), EvRankNames[W.Career.Rank], *EvMoney(Bonus)));
			++Gifts;
		}
	}
	double EvFamilyValue(const FADPWorld& W, const FString& Id)
	{
		const FADPCareer& C = W.Career;
		if (Id == TEXT("legs")) return C.Legs; if (Id == TEXT("pax")) return C.Pax; if (Id == TEXT("nm")) return C.Nm; if (Id == TEXT("hours")) return C.Hours;
		if (Id == TEXT("airports")) return C.Airports.Num(); if (Id == TEXT("countries")) return C.Countries.Num(); if (Id == TEXT("revenue")) return C.Revenue;
		if (Id == TEXT("streak")) return C.BestStreak; if (Id == TEXT("jobs")) return C.JobsDone;
		if (Id == TEXT("fleet")) { const FADPAirline* Me = EvPlayer(W); int32 N = 0; if (Me) for (const FADPAircraft& A : W.Aircraft) if (A.AirlineId == Me->Id) ++N; return N; }
		return 0.0;
	}
	void EvCheckWorld(FADPWorld& W, const FDateTime& At)
	{
		for (const FEvFamily& F : EvFamilies())
		{
			const double V = EvFamilyValue(W, F.Id);
			for (const double T : F.Tiers)
				if (V >= T) EvAward(W, At, FString::Printf(TEXT("%s-%.0f"), F.Id, T), FString::Printf(TEXT("%s %s"), *FText::AsNumber(int64(T)).ToString(), F.Title), false);
		}
		const FADPAirline* Me = EvPlayer(W);
		if (!Me) return;
		int32 Fleet = 0; bool bWide = false;
		for (const FADPAircraft& A : W.Aircraft) if (A.AirlineId == Me->Id) { ++Fleet; if (const FADPTypeInfo* T = ADPWorldData::Type(A.TypeId)) bWide |= T->Class == TEXT("widebody"); }
		int32 Routes = 0; for (const FADPRoute& R : W.Routes) if (R.AirlineId == Me->Id && !R.Id.StartsWith(TEXT("job-"))) ++Routes;
		const bool bLoans = W.Loans.ContainsByPredicate([&](const FADPLoan& L) { return L.AirlineId == Me->Id && L.Owed > 0.0; });
		auto Ach = [&](const TCHAR* Id, bool bIf) { if (bIf) for (const FEvAch& A : EvAchievements) if (FCString::Strcmp(A.Id, Id) == 0) EvAward(W, At, Id, A.Title, true); };
		Ach(TEXT("first"), W.Career.Legs >= 1); Ach(TEXT("globe"), W.Career.Countries.Num() >= 50); Ach(TEXT("punctual"), W.Career.BestStreak >= 100);
		Ach(TEXT("rich"), Me->Balance >= 100e6); Ach(TEXT("debtfree"), !bLoans && Me->Balance >= 50e6); Ach(TEXT("fleet10"), Fleet >= 10); Ach(TEXT("fleet50"), Fleet >= 50);
		Ach(TEXT("routes10"), Routes >= 10); Ach(TEXT("charter1"), W.Career.JobsDone >= 1); Ach(TEXT("charter10"), W.Career.JobsDone >= 10);
		Ach(TEXT("millionpax"), W.Career.Pax >= 1e6); Ach(TEXT("rep80"), Me->Reputation >= 80.0); Ach(TEXT("rep95"), Me->Reputation >= 95.0); Ach(TEXT("widebody"), bWide);
		Ach(TEXT("rivaldown"), W.Airlines.ContainsByPredicate([](const FADPAirline& A) { return A.Id.StartsWith(TEXT("gone:")); }));
		Ach(TEXT("anniversary"), W.Career.Started != FDateTime() && (At - W.Career.Started).GetTotalDays() >= 365.0);
	}
	int32 GEvBadgeCount = 0;
}

void ADPEvents::Notify(FADPWorld& W, const FDateTime& At, const FString& Kind, int32 Priority, const FString& Title, const FString& Text)
{
	FADPNotice N; N.At = At; N.Kind = Kind; N.Priority = Priority; N.Title = Title; N.Text = Text;
	W.Notices.Add(N);
	while (W.Notices.Num() > 400)
	{
		const int32 Read = W.Notices.IndexOfByPredicate([](const FADPNotice& X) { return X.bRead; });
		W.Notices.RemoveAt(Read != INDEX_NONE ? Read : 0);
	}
}

void ADPEvents::News(FADPWorld& W, const FDateTime& At, const FString& Topic, const FString& Headline)
{
	FADPNews N; N.At = At; N.Topic = Topic; N.Headline = Headline;
	W.News.Add(N);
	if (W.News.Num() > 300) W.News.RemoveAt(0, W.News.Num() - 300);
}

double ADPEvents::DemandFactor(const FADPWorld& W, const FADPAirportInfo& A, const FADPAirportInfo& B, const FDateTime& At)
{
	double K = 1.0;
	for (const FADPWorldEvent& E : W.Events) if (EvActive(E, At) && (EvHits(E, A) || EvHits(E, B))) K *= E.DemandMul;
	return K;
}

const FADPWorldEvent* ADPEvents::ClosedBy(const FADPWorld& W, const FADPAirportInfo& Ap, const FDateTime& At)
{
	for (const FADPWorldEvent& E : W.Events) if (E.bClosed && EvActive(E, At) && EvHits(E, Ap)) return &E;
	return nullptr;
}

double ADPEvents::RepFactor(const FADPAirline* A) { return 0.75 + (A ? FMath::Clamp(A->Reputation, 0.0, 100.0) : 50.0) / 200.0; }

FString ADPEvents::RankName(int32 Rank) { return EvRankNames[FMath::Clamp(Rank, 0, int32(UE_ARRAY_COUNT(EvRankNames)) - 1)]; }

int32 ADPEvents::NumBadges()
{
	if (GEvBadgeCount || !ADPWorldData::IsReady()) return GEvBadgeCount;
	int32 N = 0; for (const FEvFamily& F : EvFamilies()) N += F.Tiers.Num();
	TSet<FString> Countries; int32 Large = 0;
	ADPWorldData::ForEachAirport([&](const FADPAirportInfo& Ap) { if (Ap.bScheduled) Countries.Add(Ap.Country); if (Ap.Kind == 'L' && !Ap.Icao.IsEmpty()) ++Large; });
	GEvBadgeCount = N + Countries.Num() + Large + ADPWorldData::Types().Num() * 3;
	return GEvBadgeCount;
}

bool ADPEvents::TakeJob(FADPWorld& W, const FString& JobId, const FString& Reg, const FDateTime& At)
{
	FADPJob* J = W.Jobs.FindByPredicate([&](const FADPJob& X) { return X.Id == JobId; });
	FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; });
	const FADPAirline* Me = EvPlayer(W);
	if (!J || !A || !Me || !J->TakenBy.IsEmpty() || J->bDone || J->Expires < At || A->AirlineId != Me->Id || A->Status != TEXT("parked")) return false;
	const FADPTypeInfo* T = ADPWorldData::Type(A->TypeId);
	const FADPRoute* Was = W.Routes.FindByPredicate([&](const FADPRoute& R) { return R.Id == A->RouteId; });
	if (!T || !Was || T->Seats < J->Pax) return false;   // (the pilot comes from the aircraft's own route)
	FADPProfile P; if (!ADPWorldData::MakeProfile(A->TypeId, J->From, J->To, P)) return false;
	FADPRoute R; R.Id = TEXT("job-") + J->Id; R.AirlineId = Me->Id; R.TypeId = A->TypeId; R.PilotId = Was->PilotId; R.OpenedAt = At;
	R.Stops = A->Location == J->From ? TArray<FString>{J->From, J->To} : TArray<FString>{A->Location, J->From, J->To};
	W.Routes.Add(R);
	A->PrevRouteId = A->RouteId; A->RouteId = R.Id; A->LegIdx = 0; if (A->ReadyAt < At) A->ReadyAt = At;
	J->TakenBy = Reg;
	Notify(W, At, TEXT("company"), 0, TEXT("Job taken"), FString::Printf(TEXT("%s flies %s: %s to %s, %s on arrival"), *Reg, *J->Title, *J->From, *J->To, *EvMoney(J->Pay)));
	return true;
}

bool ADPEvents::JobLoad(const FADPWorld& W, const FString& RouteId, const FString& To, FADPFlight& F)
{
	if (!RouteId.StartsWith(TEXT("job-"))) return false;
	const FString Id = RouteId.Mid(4);
	const FADPJob* J = W.Jobs.FindByPredicate([&](const FADPJob& X) { return X.Id == Id; });
	F.Pax = 0; F.PaxJ = 0; F.PaxF = 0; F.CargoKg = 0; F.Revenue = 0.0; F.LoadFactor = 0.0;
	if (J && To == J->To)
	{
		F.Pax = J->Pax; F.CargoKg = J->CargoKg;
		const FADPTypeInfo* T = ADPWorldData::Type(F.TypeId);
		F.LoadFactor = T && T->Seats ? FMath::Min(1.0, double(J->Pax) / T->Seats) : 0.0;
	}
	return true;   // (an empty positioning leg otherwise; the job is paid on arrival)
}

void ADPEvents::OnLanded(FADPWorld& W, const FADPFlight& F, const FDateTime& At)
{
	FADPAirline* Al = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == F.AirlineId; });
	if (!Al) return;
	ADPNetwork::Landed(W, F);   // (the league's and the challenges' counters: to-do 186)
	const bool bOnTime = F.DelayMin < 15.0 && F.Diverted.IsEmpty();
	Al->OnTimeEma = Al->OnTimeEma * 0.98 + (bOnTime ? 0.02 : 0.0);
	if (!F.Emergency.IsEmpty()) Al->Reputation = FMath::Max(0.0, Al->Reputation - 1.0);
	else if (!F.Diverted.IsEmpty()) Al->Reputation = FMath::Max(0.0, Al->Reputation - 0.3);
	// a job done: paid (half if late), the aircraft back to its own route (next leg: home)
	if (F.RouteId.StartsWith(TEXT("job-")))
	{
		FADPJob* J = W.Jobs.FindByPredicate([&](const FADPJob& X) { return X.Id == F.RouteId.Mid(4); });
		if (J && F.To == J->To && !J->bDone)
		{
			const bool bLate = At > J->Deadline;
			ADPEconomy::Pay(W, Al->Id, At, TEXT("charter"), J->Pay * (bLate ? 0.5 : 1.0), FString::Printf(TEXT("%s: %s"), *F.Reg, *J->Title), F.Id);
			J->bDone = true; J->DoneAt = At;
			if (Al->bPlayer) ++W.Career.JobsDone;
			Notify(W, At, TEXT("finance"), 1, TEXT("Job done"), FString::Printf(TEXT("%s delivered %s to %s - %s paid%s"), *F.Reg, *J->Title, *J->To, *EvMoney(J->Pay * (bLate ? 0.5 : 1.0)), bLate ? TEXT(" (half: late)") : TEXT("")));
			if (FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == F.Reg; }))
			{
				W.Routes.RemoveAll([&](const FADPRoute& R) { return R.Id == A->RouteId; });
				A->RouteId = A->PrevRouteId; A->PrevRouteId.Empty();
				if (const FADPRoute* R = W.Routes.FindByPredicate([&](const FADPRoute& X) { return X.Id == A->RouteId; })) A->LegIdx = R->Stops.Num() - 1;
			}
		}
	}
	if (!Al->bPlayer) return;
	// your career
	FADPCareer& C = W.Career;
	if (C.Started == FDateTime()) C.Started = At;
	const FADPAirportInfo* A = ADPWorldData::Airport(F.From); const FADPAirportInfo* B = ADPWorldData::Airport(F.To);
	const double Nm = A && B ? ADPWorldData::DistanceNm(A->Lat, A->Lon, B->Lat, B->Lon) : 0.0;
	const int32 Pax = F.Pax + F.PaxJ + F.PaxF;
	++C.Legs; C.Pax += Pax; C.Nm += Nm; C.Revenue += F.Revenue; C.Hours += F.DurMin / 60.0;
	C.OnTimeStreak = bOnTime ? C.OnTimeStreak + 1 : 0; C.BestStreak = FMath::Max(C.BestStreak, C.OnTimeStreak);
	++C.DayLegs; C.DayPax += Pax; C.DayRevenue += F.Revenue; if (bOnTime) ++C.DayOnTime;
	C.TypeLegs.FindOrAdd(F.TypeId) += 1;
	C.Xp += 10.0 + Nm / 100.0 + Pax / 20.0;
	for (const FADPAirportInfo* Ap : {A, B})
	{
		if (!Ap) continue;
		const FString Id = Ap->Icao.IsEmpty() ? Ap->Ident : Ap->Icao;
		if (!C.Airports.Contains(Id)) { C.Airports.Add(Id); if (Ap->Kind == 'L') EvAward(W, At, TEXT("airport-") + Id, FString::Printf(TEXT("Served %s"), *Ap->Name), false); }
		if (!C.Countries.Contains(Ap->Country)) { C.Countries.Add(Ap->Country); EvAward(W, At, TEXT("country-") + Ap->Country, FString::Printf(TEXT("First flight to country %s"), *Ap->Country), false); }
	}
	for (const int32 T : {1, 100, 1000})
		if (C.TypeLegs[F.TypeId] >= T) EvAward(W, At, FString::Printf(TEXT("type-%s-%d"), *F.TypeId, T), FString::Printf(TEXT("%d legs on the %s"), T, *F.TypeId), false);
	auto Ach = [&](const TCHAR* Id, bool bIf) { if (bIf) for (const FEvAch& X : EvAchievements) if (FCString::Strcmp(X.Id, Id) == 0) EvAward(W, At, Id, X.Title, true); };
	Ach(TEXT("fullhouse"), F.LoadFactor >= 0.995); Ach(TEXT("longhaul"), Nm >= 5000.0); Ach(TEXT("weathered"), !F.Diverted.IsEmpty() && F.Emergency.IsEmpty());
	Ach(TEXT("engineout"), !F.Emergency.IsEmpty()); Ach(TEXT("polar"), B && B->Lat > 66.56); Ach(TEXT("equator"), A && B && (A->Lat > 0) != (B->Lat > 0));
	EvCheckWorld(W, At);
	int32 Dummy = 0; EvCheckRank(W, At, Dummy);
}

FString ADPEvents::Briefing(const FADPWorld& W, const FDateTime& At)
{
	const FADPAirline* Me = EvPlayer(W);
	if (!Me) return FString();
	const FADPCareer& C = W.Career;
	FString S = FString::Printf(TEXT("Good morning. Yesterday: %d legs, %s passengers, %s in tickets and cargo, %d%% on time.\n"),
		C.DayLegs, *FText::AsNumber(C.DayPax).ToString(), *EvMoney(C.DayRevenue), C.DayLegs ? FMath::RoundToInt(100.0 * C.DayOnTime / C.DayLegs) : 100);
	S += FString::Printf(TEXT("Balance %s, reputation %.0f, rank %s (%s XP).\n"), *EvMoney(Me->Balance), Me->Reputation, *RankName(C.Rank), *FText::AsNumber(int64(C.Xp)).ToString());
	if (const FADPAirportInfo* Hub = ADPWorldData::Airport(Me->Hub))
	{
		const FADPWx X = ADPWeather::At(Hub->Icao, Hub->Lat, Hub->Lon, At);
		S += FString::Printf(TEXT("Weather at %s: %.0f C, wind %03.0f at %.0f kt%s, visibility %.1f km%s%s.\n"), *Hub->Name, X.TempC, X.WindDir, X.WindKt,
			X.GustKt > X.WindKt ? *FString::Printf(TEXT(" gusting %.0f"), X.GustKt) : TEXT(""), X.VisM / 1000.0, X.bSnow ? TEXT(", snow") : X.bRain ? TEXT(", rain") : TEXT(""), X.bStorm ? TEXT(", thunderstorms") : TEXT(""));
	}
	for (const FADPWorldEvent& E : W.Events) if (E.Until > At) S += FString::Printf(TEXT("%s: %s.\n"), E.From > At ? TEXT("Coming") : TEXT("Now"), *E.Title);
	int32 Open = 0; for (const FADPJob& J : W.Jobs) if (J.TakenBy.IsEmpty() && !J.bDone && J.Expires > At) ++Open;
	int32 Unread = 0; for (const FADPNotice& N : W.Notices) if (!N.bRead) ++Unread;
	S += FString::Printf(TEXT("%d jobs on the board, %d unread messages."), Open, Unread);
	return S;
}

FString FADPEventsModule::PostJob(FADPWorld& W, const FDateTime& At)
{
	const FADPAirline* Me = EvPlayer(W);
	const FADPAirportInfo* Hub = Me ? ADPWorldData::Airport(Me->Hub) : nullptr;
	if (!Hub) return FString();
	TArray<const FADPAirportInfo*> Near, Dest;
	ADPWorldData::ForEachAirport([&](const FADPAirportInfo& Ap)
	{
		if ((Ap.Kind != 'L' && Ap.Kind != 'M') || Ap.Icao.IsEmpty() || !Ap.bScheduled) return;
		const double D = ADPWorldData::DistanceNm(Hub->Lat, Hub->Lon, Ap.Lat, Ap.Lon);
		if (D < 400.0) Near.Add(&Ap);
		if (D >= 200.0 && D <= 2200.0) Dest.Add(&Ap);
	});
	if (!Dest.Num()) return FString();
	const FADPAirportInfo* From = (Rng.FRand() < 0.7 || !Near.Num()) ? Hub : Near[Rng.RandRange(0, Near.Num() - 1)];
	const FADPAirportInfo* To = Dest[Rng.RandRange(0, Dest.Num() - 1)];
	if (To->Ident == From->Ident) return FString();
	const double Nm = ADPWorldData::DistanceNm(From->Lat, From->Lon, To->Lat, To->Lon);
	FADPJob J; J.Id = FString::Printf(TEXT("j%d"), W.NextJobId++); J.From = From->Icao; J.To = To->Icao; J.Posted = At;
	J.Expires = At + FTimespan::FromHours(Rng.FRandRange(24.0, 48.0)); J.Deadline = At + FTimespan::FromDays(3.0);
	const double Roll = Rng.FRand();
	if (Roll < 0.6)
	{
		const TCHAR* Who[] = {TEXT("a football team"), TEXT("a tour group"), TEXT("a school trip"), TEXT("a band on tour"), TEXT("a wedding party"), TEXT("a film crew"), TEXT("a choir")};
		J.Kind = TEXT("charter"); J.Pax = Rng.RandRange(30, 170); J.Title = Who[Rng.RandRange(0, UE_ARRAY_COUNT(Who) - 1)];
		J.Pay = (60.0 + Nm * 0.11) * J.Pax * Rng.FRandRange(1.4, 1.9);
	}
	else if (Roll < 0.9)
	{
		const TCHAR* What[] = {TEXT("car parts"), TEXT("medicine"), TEXT("fresh fish"), TEXT("the post"), TEXT("flowers"), TEXT("machine parts")};
		J.Kind = TEXT("cargo"); J.CargoKg = Rng.RandRange(2000, 15000); J.Title = What[Rng.RandRange(0, UE_ARRAY_COUNT(What) - 1)];
		J.Pay = J.CargoKg * Nm * 0.0009 + 5000.0;
	}
	else
	{
		J.Kind = TEXT("medical"); J.Pax = Rng.RandRange(2, 6); J.Title = TEXT("a patient and a medical team");
		J.Pay = 30000.0 + Nm * 40.0; J.Deadline = At + FTimespan::FromHours(24.0); J.Expires = At + FTimespan::FromHours(6.0);
	}
	W.Jobs.Add(J); ++JobsPosted;
	return J.Id;
}

void FADPEventsModule::StartEvent(FADPWorld& W, const FString& Kind, const FDateTime& At)
{
	const FADPAirline* Me = EvPlayer(W);
	const FADPAirportInfo* Hub = Me ? ADPWorldData::Airport(Me->Hub) : nullptr;
	// where: a third near your hub, the rest anywhere (a large airport)
	TArray<const FADPAirportInfo*> Pick; const bool bNear = Hub && Rng.FRand() < 0.35;
	ADPWorldData::ForEachAirport([&](const FADPAirportInfo& Ap)
	{
		if (Ap.Kind != 'L' || Ap.Icao.IsEmpty()) return;
		if (!bNear || ADPWorldData::DistanceNm(Hub->Lat, Hub->Lon, Ap.Lat, Ap.Lon) < 1500.0) Pick.Add(&Ap);
	});
	if (!Pick.Num()) return;
	const FADPAirportInfo* P = Pick[Rng.RandRange(0, Pick.Num() - 1)];
	FADPWorldEvent E; E.Id = FString::Printf(TEXT("e%lld-%d"), At.GetTicks() / ETimespan::TicksPerSecond, Rng.RandRange(0, 999)); E.Kind = Kind; E.Lat = P->Lat; E.Lon = P->Lon;
	if (Kind == TEXT("volcano"))
	{
		const FEvVolcano& V = EvVolcanoes[Rng.RandRange(0, UE_ARRAY_COUNT(EvVolcanoes) - 1)];
		E.Lat = V.Lat; E.Lon = V.Lon; E.RadiusNm = Rng.FRandRange(180.0, 450.0); E.bClosed = true; E.DemandMul = 0.85;
		E.From = At; E.Until = At + FTimespan::FromHours(Rng.FRandRange(24.0, 96.0)); E.Title = FString::Printf(TEXT("Volcanic ash from %s closes the airspace"), V.Name);
	}
	else if (Kind == TEXT("strike"))
	{
		E.Country = P->Country; E.bClosed = true; E.DemandMul = 0.9;
		E.From = At + FTimespan::FromDays(2.0); E.Until = E.From + FTimespan::FromHours(Rng.FRandRange(12.0, 36.0)); E.Title = FString::Printf(TEXT("Air traffic controllers strike in country %s"), *P->Country);
	}
	else if (Kind == TEXT("festival"))
	{
		const TCHAR* What[] = {TEXT("the cup final"), TEXT("a music festival"), TEXT("a world summit"), TEXT("a marathon weekend"), TEXT("a royal wedding"), TEXT("a trade fair")};
		E.RadiusNm = 120.0; E.DemandMul = Rng.FRandRange(1.4, 1.8);
		E.From = At + FTimespan::FromDays(Rng.FRandRange(3.0, 7.0)); E.Until = E.From + FTimespan::FromDays(Rng.FRandRange(3.0, 6.0));
		E.Title = FString::Printf(TEXT("%s in %s - travel there is booming"), What[Rng.RandRange(0, UE_ARRAY_COUNT(What) - 1)], P->Town.IsEmpty() ? *P->Name : *P->Town);
	}
	else
	{
		E.Kind = TEXT("health"); E.RadiusNm = Rng.FRandRange(1500.0, 2500.0); E.DemandMul = Rng.FRandRange(0.7, 0.85);
		E.From = At; E.Until = At + FTimespan::FromDays(Rng.FRandRange(21.0, 42.0)); E.Title = FString::Printf(TEXT("A health scare around %s - fewer people travel"), P->Town.IsEmpty() ? *P->Name : *P->Town);
	}
	W.Events.Add(E); ++WorldEvents;
	ADPEvents::News(W, At, TEXT("world"), E.From > At ? FString::Printf(TEXT("%s (from %s UTC)"), *E.Title, *E.From.ToString(TEXT("%d/%m %H:%M"))) : E.Title);
	// you're told when it touches your network
	if (Me)
	{
		bool bMine = false;
		for (const FADPRoute& R : W.Routes) if (R.AirlineId == Me->Id) for (const FString& S : R.Stops) if (const FADPAirportInfo* Ap = ADPWorldData::Airport(S)) bMine |= EvHits(E, *Ap);
		if (bMine) ADPEvents::Notify(W, At, TEXT("authority"), E.bClosed ? 2 : 1, E.bClosed ? TEXT("Airports closing") : TEXT("Demand change"), E.Title);
	}
}

void FADPEventsModule::Hourly(FADPWorld& W, const FDateTime& At, bool bCatchUp)
{
	// events: begin, end
	for (int32 i = W.Events.Num() - 1; i >= 0; --i)
	{
		FADPWorldEvent& E = W.Events[i];
		if (!E.bStarted && E.From <= At) { E.bStarted = true; if (E.From > At - FTimespan::FromHours(2.0) && E.Kind == TEXT("strike")) ADPEvents::News(W, At, TEXT("world"), E.Title + TEXT(" - from now")); }
		if (E.Until <= At) { ADPEvents::News(W, At, TEXT("world"), FString::Printf(TEXT("Over: %s"), *E.Title)); W.Events.RemoveAt(i); }
	}
	// new ones now and then: ash every few months, a strike a month, a big event a week, a health scare twice a year
	if (Rng.FRand() < 1.0 / (24 * 150)) StartEvent(W, TEXT("volcano"), At);
	if (Rng.FRand() < 1.0 / (24 * 30)) StartEvent(W, TEXT("strike"), At);
	if (Rng.FRand() < 1.0 / (24 * 7)) StartEvent(W, TEXT("festival"), At);
	if (Rng.FRand() < 1.0 / (24 * 180)) StartEvent(W, TEXT("health"), At);
	// the jobs board: old ones off, about 8 open
	for (int32 i = W.Jobs.Num() - 1; i >= 0; --i)
	{
		const FADPJob& J = W.Jobs[i];
		if (J.TakenBy.IsEmpty() && J.Expires <= At) { W.Jobs.RemoveAt(i); ++JobsExpired; }
		else if (J.bDone && J.DoneAt + FTimespan::FromDays(2.0) <= At) W.Jobs.RemoveAt(i);
	}
	int32 Open = 0; for (const FADPJob& J : W.Jobs) if (J.TakenBy.IsEmpty()) ++Open;
	if (Open < 8 && Rng.FRand() < 0.3) PostJob(W, At);
	// the morning briefing at 06:00 at your hub (not for the days caught up on: the day's figures aren't in yet)
	if (bCatchUp) return;
	if (const FADPAirline* Me = EvPlayer(W))
		if (const FADPAirportInfo* Hub = ADPWorldData::Airport(Me->Hub))
		{
			const int32 Local = (At.GetHour() + FMath::RoundToInt(Hub->Lon / 15.0) + 48) % 24;
			if (Local == 6 && (W.Career.LastBriefing == FDateTime() || (At - W.Career.LastBriefing).GetTotalHours() > 20.0))
			{
				ADPEvents::Notify(W, At, TEXT("mail"), 1, TEXT("Morning briefing"), ADPEvents::Briefing(W, At));
				W.Career.LastBriefing = At; W.Career.DayLegs = 0; W.Career.DayPax = 0; W.Career.DayRevenue = 0.0; W.Career.DayOnTime = 0;
				++Briefings;
			}
		}
}

void FADPEventsModule::Daily(FADPWorld& W, const FDateTime& At)
{
	// reputation, for every airline: punctuality most, then quality (incidents took theirs at the time)
	for (FADPAirline& A : W.Airlines)
	{
		if (A.Id.StartsWith(TEXT("gone:"))) continue;
		const double Target = 10.0 + 55.0 * A.OnTimeEma + 30.0 * A.Quality;
		A.Reputation = FMath::Clamp(A.Reputation + (Target - A.Reputation) * 0.08, 0.0, 100.0);
	}
	// a year at the helm: the staff's gift
	if (W.Career.Started != FDateTime())
	{
		const int32 Years = int32((At - W.Career.Started).GetTotalDays() / 365.0);
		if (Years > W.Career.Anniversaries)
		{
			W.Career.Anniversaries = Years; ++Gifts;
			ADPEvents::Notify(W, At, TEXT("gift"), 1, TEXT("A gift from your staff"), FString::Printf(TEXT("%d year%s at the helm - the staff give you a model of your first aircraft and a cake."), Years, Years == 1 ? TEXT("") : TEXT("s")));
		}
	}
	EvCheckWorld(W, At);
	EvCheckRank(W, At, Gifts);
}

void FADPEventsModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
	if (!ADPWorldData::IsReady()) return;
	FADPWorld& W = Sim.World;
	Hourly(W, To, bCatchUp);
	if (To.GetHour() == 0) Daily(W, To);
}
