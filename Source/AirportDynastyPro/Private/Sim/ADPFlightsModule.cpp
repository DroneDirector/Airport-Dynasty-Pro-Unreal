// Hired pilots flying the routes - see Sim/ADPFlightsModule.h.
#include "Sim/ADPFlightsModule.h"
#include "Sim/ADPRoster.h"
#include "Sim/ADPMaint.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPNetwork.h"
#include "Sim/ADPTown.h"
#include "Sim/ADPBoards.h"
#include "ADPLog.h"
#include "Sim/ADPWeather.h"
#include "Sim/ADPDemand.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPStaff.h"
#include "Sim/ADPEvents.h"
#include "Sim/ADPSchedule.h"

static FString AirlineCode(const FADPWorld& W, const FString& AirlineId)
{
	for (const FADPAirline& A : W.Airlines) if (A.Id == AirlineId) return A.Icao.IsEmpty() ? A.Name.Left(3).ToUpper() : A.Icao;
	return TEXT("XX");
}

const FADPProfile* FADPFlightsModule::ProfileOf(const FADPFlight& F)
{
	if (const FADPProfile* P = Profiles.Find(F.Id)) return P;
	FADPProfile P;
	if (!ADPWorldData::MakeProfile(F.TypeId, F.From, F.To, P)) return nullptr;
	return &Profiles.Add(F.Id, P);
}

void FADPFlightsModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
	if (!ADPWorldData::IsReady()) return;
	FADPWorld& W = Sim.World;
	// the planned legs (Sim/ADPSchedule.h): made, sold, their expected times
	if (LastPlan > To || (To - LastPlan).GetTotalSeconds() >= 30.0) { LastPlan = To; ADPTown::UseWorld(W); ADPSchedule::Step(W, To); }
	// the flights in the air: their phase and position now; landed ones come in
	for (int32 i = W.Flights.Num() - 1; i >= 0; --i)
	{
		FADPFlight& F = W.Flights[i];
		const FADPProfile* P = ProfileOf(F);
		if (!P) { W.Flights.RemoveAt(i); continue; }
		double Min = (To - F.DepAt).GetTotalMinutes();
		// holding: its minutes are spent at the start of the descent (circling at that height); the destination's weather is
		// looked at once, as the descent begins
		const double DescentAt = P->PhaseStart[FADPProfile::Descent];
		if (!F.bWxChecked && Min >= DescentAt) { F.bWxChecked = true; OnTheWayDown(W, F, To); P = ProfileOf(F); if (!P) { W.Flights.RemoveAt(i); continue; } }
		const double DescentAt2 = P->PhaseStart[FADPProfile::Descent];
		Min = (To - F.DepAt).GetTotalMinutes();
		const bool bHolding = Min >= DescentAt2 && Min < DescentAt2 + F.HoldMin;
		if (Min >= DescentAt2) Min = FMath::Max(DescentAt2, Min - F.HoldMin);
		if (Min >= P->BlockMin()) { Land(W, i, F.DepAt + FTimespan::FromMinutes(P->BlockMin() + F.HoldMin)); continue; }
		const FADPProfile::FState S = P->At(FMath::Max(0.0, Min));
		F.Phase = bHolding ? FString(TEXT("Holding")) : FString(FADPProfile::PhaseName(S.Phase)); F.Lat = S.Lat; F.Lon = S.Lon; F.AltFt = S.AltFt; F.GsKt = S.GsKt; F.HeadingDeg = S.HeadingDeg;
	}
	// the aircraft on the ground with a route: off when they're ready
	for (FADPAircraft& A : W.Aircraft)
		if (A.Status == TEXT("parked") && !A.RouteId.IsEmpty() && A.ReadyAt <= To) TryDepart(W, A, To, bCatchUp);
}

void FADPFlightsModule::TryDepart(FADPWorld& W, FADPAircraft& A, const FDateTime& Now, bool bCatchUp)
{
	// its planned leg: your timetable's (an aircraft in it flies nothing else), or the round of the airline's route
	FADPPlanned* Pl = ADPSchedule::Next(W, A.Reg, A.Location);
	if (ADPSchedule::HasSlots(W, A.Reg))
	{
		if (!Pl) return;
		A.RouteId = Pl->RouteId; A.LegIdx = Pl->bReturn ? 1 : 0;
	}
	const bool bMineHere = A.AirlineId == UADPSim::Me() && A.Location == ADPBoards::HereIcao();
	if (Pl && Now < Pl->Est - FTimespan::FromMinutes(bMineHere ? 60.0 : 0.0)) return;
	const FADPRoute* R = W.Routes.FindByPredicate([&](const FADPRoute& X) { return X.Id == A.RouteId; });
	if (!R || R->Stops.Num() < 2) return;
	FADPPerson* Pilot = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == R->PilotId; });
	auto Wait = [&](const TCHAR* Why, double Min)
	{
		if (A.WaitingSince == FDateTime()) A.WaitingSince = Now;
		if (Pl)
		{
			Pl->Why = Why; Pl->Hold = FMath::Max(Pl->Hold, FMath::Max(Now, Pl->Sched) + FTimespan::FromMinutes(Now >= Pl->Sched ? Min : 0.0)); Pl->Est = FMath::Max(Pl->Est, Pl->Hold);
			A.ReadyAt = Now + FTimespan::FromMinutes(Min);
			// (five hours late: off - the way home is flown however late it gets)
			if (!Pl->bReturn && (Now - Pl->Sched).GetTotalHours() >= CancelAfterH) { ADPSchedule::Cancel(W, *Pl, Now, Why); A.WaitingSince = FDateTime(); A.ReadyAt = Pl->SlotId.IsEmpty() ? Now + FTimespan::FromHours(19.0) : Now; }
			return;
		}
		if ((Now - A.WaitingSince).GetTotalHours() >= CancelAfterH)
		{
			// (waited 5 hours: the leg is cancelled and tried again at the same time tomorrow)
			A.ReadyAt = A.WaitingSince + FTimespan::FromDays(1.0); A.WaitingSince = FDateTime();
			FADPNotice N; N.At = Now; N.Kind = TEXT("flight"); N.Text = FString::Printf(TEXT("%s: the leg is cancelled after 5 hours (%s) - tried again tomorrow"), *A.Reg, Why);
			W.Notices.Add(N);
			return;
		}
		A.ReadyAt = Now + FTimespan::FromMinutes(Min);
	};
	// your airline: the whole crew rostered on the EASA flight time limitations (to-do 177, Sim/ADPRoster.h);
	// the AI airlines: one pilot per route and enough cabin crew at the base (their simpler rule)
	const bool bRoster = A.AirlineId == UADPSim::Me();
	if (!bRoster)
	{
		if (!Pilot || Pilot->Status != TEXT("on")) { Wait(TEXT("no pilot"), 60); return; }
		if (Pilot->RestUntil > Now) { A.ReadyAt = Pilot->RestUntil; return; }
		if (ADPStaff::OnStrike(W, A.AirlineId, Pilot->Role) || ADPStaff::OnStrike(W, A.AirlineId, TEXT("fa"))) { Wait(TEXT("a strike"), 180); return; }
		if (!Pilot->Types.Contains(A.TypeId)) { Wait(TEXT("the pilot has no type rating for this aircraft"), 240); return; }
	}
	else if (ADPStaff::OnStrike(W, A.AirlineId, TEXT("captain")) || ADPStaff::OnStrike(W, A.AirlineId, TEXT("fo")) || ADPStaff::OnStrike(W, A.AirlineId, TEXT("fa"))) { Wait(TEXT("a strike"), 180); return; }
	if (A.Condition < 20.0) { Wait(TEXT("the aircraft needs repair"), 120); return; }
	if (!bRoster)
		if (const FADPTypeInfo* Tc = ADPWorldData::Type(A.TypeId))
			if (ADPStaff::CabinCrewAt(W, A.AirlineId, A.Base) < ADPStaff::CabinCrewFor(Tc->Seats)) { ++CrewWaits; Wait(TEXT("not enough cabin crew"), 60); return; }
	const FString Next = Pl ? Pl->To : R->Stops[(A.LegIdx + 1) % R->Stops.Num()];
	FADPProfile P;
	if (!ADPWorldData::MakeProfile(A.TypeId, A.Location, Next, P)) { Wait(TEXT("no flight plan"), 240); return; }
	if (bRoster)
	{
		const FADPTypeInfo* Tc = ADPWorldData::Type(A.TypeId);
		FString Why;
		if (!ADPRoster::Crew(W, A, P.BlockMin(), ADPStaff::CabinCrewFor(Tc ? Tc->Seats : 150), R->PilotId, Now, Why))
		{
			++CrewWaits;
			if (A.WaitingSince == FDateTime()) { FADPNotice N; N.At = Now; N.Kind = TEXT("flight"); N.Priority = 1; N.Title = A.Reg + TEXT(" waiting for crew"); N.Text = FString::Printf(TEXT("%s to %s can't leave: %s. Hire more crew or give it time to rest."), *A.Reg, *Next, *Why); W.Notices.Add(N); }
			Wait(*Why, 30); return;
		}
		Pilot = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == A.Crew[0]; });
		if (!Pilot) { Wait(TEXT("no captain"), 30); return; }
		if (Pl && !Pl->Why.IsEmpty() && Pl->Why.Contains(TEXT(" free"))) { Pl->Why.Empty(); A.WaitingSince = FDateTime(); }   // (a crew came free in time)
	}
	// your people on the ground here: without them the steps they do don't happen (owner 2026-11-23: the step waits, and
	// the game says why) - told an hour ahead, held at its time
	if (bMineHere)
	{
		const FString Miss = ADPSchedule::MissingStaff(W, Now);
		if (!Miss.IsEmpty())
		{
			ADPSchedule::Tell(W, Now, TEXT("staff"), TEXT("Nobody to do the work"), FString::Printf(TEXT("%s to %s waits: %s. Hire them under Crew."), Pl ? *Pl->FlightNo : *A.Reg, *Next, *Miss));
			if (!Pl || Now >= Pl->Est) { ++CrewWaits; Wait(*Miss, 5); return; }
		}
	}
	if (Pl && Now < Pl->Est) return;   // (reported, rostered: on time now)
	// the weather at both ends (the real world's now, or the built-in climate): crosswind on the best runway, visibility,
	// storms; snow or freezing rain means de-icing first
	double WxDelay = 0.0;
	{
		const FADPAirportInfo* Ap = ADPWorldData::Airport(A.Location); const FADPAirportInfo* Bp = ADPWorldData::Airport(Next);
		const FADPTypeInfo* T = ADPWorldData::Type(A.TypeId);
		if (Ap && Bp && T)
		{
			const FADPWx Wa = ADPWeather::At(Ap->Icao.IsEmpty() ? Ap->Ident : Ap->Icao, Ap->Lat, Ap->Lon, Now);
			const FADPWx Wb = ADPWeather::At(Bp->Icao.IsEmpty() ? Bp->Ident : Bp->Icao, Bp->Lat, Bp->Lon, Now + FTimespan::FromMinutes(P.BlockMin()));
			const double MaxCross = T->Class == TEXT("bush") ? 20.0 : T->Class == TEXT("turboprop") ? 32.0 : T->Class == TEXT("regional") ? 33.0 : 38.0;
			auto BestCross = [&](const FADPAirportInfo& Apt, const FADPWx& W)
			{
				double Best = 1e9;
				for (const FADPRunwayInfo& R : Apt.Runways) for (const double H : {R.LeHdg, R.LeHdg + 180.0})
				{ double C, Tl; ADPWeather::Components(W, H, C, Tl); if (Tl <= 10.0) Best = FMath::Min(Best, C); }
				return Apt.Runways.Num() ? Best : 0.0;
			};
			const TCHAR* Why = nullptr;
			if (BestCross(*Ap, Wa) > MaxCross) Why = TEXT("too much crosswind here");
			else if (BestCross(*Bp, Wb) > MaxCross) Why = TEXT("too much crosswind at the destination");
			else if (Wa.VisM < 175.0 || Wb.VisM < 175.0) Why = TEXT("fog below the landing minimums");
			else if (Wa.bStorm) Why = TEXT("a thunderstorm over the field");
			else if (Wb.bStorm && P.DistNm < 600.0) Why = TEXT("a thunderstorm at the destination");
			if (Why) { ++WxWaits; Wait(Why, 30); return; }
			if ((Wa.bSnow || Wa.bFreezing || (Wa.TempC <= 3.0 && (Wa.bRain || Wa.bFog))) && T->Class != TEXT("helicopter")) { WxDelay = Rng.FRandRange(20.0, 35.0); ++DeIcings; }
		}
	}
	// fire cover at our airport (to-do 197): the station must cover the aircraft's rescue and fire-fighting category
	{
		const FString Here = ADPBoards::HereIcao();
		if ((A.Location == Here || Next == Here) && ADPTown::FireCategoryAt(W, Here, Now) < ADPTown::FireCategoryOf(A.TypeId))
		{
			if (A.AirlineId == UADPSim::Me() && A.WaitingSince == FDateTime())
			{ FADPNotice N; N.At = Now; N.Kind = TEXT("authority"); N.Priority = 2; N.Title = TEXT("Fire cover too low"); N.Text = FString::Printf(TEXT("%s may not use %s: the fire station covers category %d, the aircraft needs %d. Hire fire-fighters or buy a crash tender."), *A.Reg, *Here, ADPTown::FireCategoryAt(W, Here, Now), ADPTown::FireCategoryOf(A.TypeId)); W.Notices.Add(N); }
			Wait(TEXT("fire cover"), 30); return;
		}
	}
	ADPTown::UseWorld(W);   // (our airport's curfew as the career has it now - not only after the town's hourly step)
	// night curfews: no departure while the field is closed, nor one that would land at a closed destination
	{
		const FADPAirportInfo* Ap = ADPWorldData::Airport(A.Location); const FADPAirportInfo* Bp = ADPWorldData::Airport(Next);
		if ((Ap && InCurfew(Ap->Icao, Ap->Lon, Now)) || (Bp && InCurfew(Bp->Icao, Bp->Lon, Now + FTimespan::FromMinutes(P.BlockMin()))))
		{ ++Curfews; A.ReadyAt = Now + FTimespan::FromMinutes(30); return; }
	}
	// closed by an event (ash, a strike) at either end: wait
	{
		const FADPAirportInfo* Ap = ADPWorldData::Airport(A.Location); const FADPAirportInfo* Bp = ADPWorldData::Airport(Next);
		const FADPWorldEvent* E = Ap ? ADPEvents::ClosedBy(W, *Ap, Now) : nullptr;
		if (!E && Bp) E = ADPEvents::ClosedBy(W, *Bp, Now + FTimespan::FromMinutes(P.BlockMin()));
		if (E) { ++Closures; Wait(*E->Title, 60); return; }
	}
	// the duty limit: the pilot rests 12 hours first if this leg would break it (the AI airlines; yours: the roster above)
	if (!bRoster && Pilot->DutyMin + P.BlockMin() > MaxDutyH * 60.0)
	{
		Pilot->RestUntil = Now + FTimespan::FromHours(RestH); Pilot->DutyMin = 0.0; A.ReadyAt = Pilot->RestUntil; ++Rests;
		return;
	}
	// delays at random: at the gate mostly short, now and then long (weather-driven delays come with the weather)
	// (your own flights out of your own airport, its 3D world running: nothing at random - they leave when the real work is
	// done; every planned leg: the delay is drawn once and waited out on the stand, the flight made as it leaves)
	const bool bReal = bMineHere && Pl && ADPSchedule::Ready3D && !bCatchUp;
	double Delay = bReal ? 0.0 : (Rng.FRand() < 0.88 ? Rng.FRandRange(0.0, 8.0) : Rng.FRandRange(15.0, 74.0)) + WxDelay;
	if (Pl)
	{
		if (!Pl->bRolled) { Pl->bRolled = true; if (Delay >= 1.0) { Pl->Hold = FMath::Max(Pl->Hold, Now + FTimespan::FromMinutes(Delay)); Pl->Est = FMath::Max(Pl->Est, Pl->Hold); return; } }
		// (from our airport, live: everyone aboard, the bags in the hold, the jetway at rest - the 3D world says so)
		if (A.Location == ADPBoards::HereIcao() && ADPSchedule::Ready3D && !bCatchUp)
		{
			FString Why;
			if (!ADPSchedule::Ready3D(*Pl, Now, Why)) { Wait(*Why, 0.25); return; }
		}
		Delay = FMath::Max(0.0, (Now - Pl->Sched).GetTotalMinutes());
	}
	if (Delay >= 15.0) ++Delayed;
	FADPFlight F;
	F.Id = Pl ? Pl->Id : FString::Printf(TEXT("%s-%lld"), *A.Reg, Now.GetTicks() / ETimespan::TicksPerSecond);
	F.FlightNo = Pl ? Pl->FlightNo : ADPBoards::FlightNumber(AirlineCode(W, A.AirlineId), A.RouteId, A.LegIdx);
	F.RouteId = A.RouteId; F.Reg = A.Reg; F.AirlineId = A.AirlineId; F.PilotId = Pilot->Id; F.TypeId = A.TypeId; F.From = A.Location; F.To = Next;
	F.DepAt = Pl ? Now : Now + FTimespan::FromMinutes(Delay); F.DelayMin = Delay; F.DurMin = P.BlockMin();
	F.Lat = P.FromLat; F.Lon = P.FromLon;
	F.Crew = A.Crew;
	F.FromStand = A.Stand;
	// who flies: the passengers, classes and cargo this airline gets on this pair today (part 4)
	{
		const FADPAirportInfo* Ap = ADPWorldData::Airport(A.Location); const FADPAirportInfo* Bp = ADPWorldData::Airport(Next); const FADPTypeInfo* T = ADPWorldData::Type(A.TypeId);
		if (ADPEvents::JobLoad(W, A.RouteId, Next, F)) {}
		else if (Pl && Pl->bTarget)
		{
			// (who booked: worked out as it was sold - less those who didn't show up at the gate)
			F.Pax = Pl->PaxT; F.PaxJ = Pl->PaxJT; F.PaxF = Pl->PaxFT; F.PaxConnect = Pl->PaxConnectT; F.CargoKg = Pl->CargoT; F.Revenue = Pl->RevenueT; F.LoadFactor = Pl->LoadT;
			F.Pax = FMath::Max(0, F.Pax - Pl->NoShow);
			PaxCarried += F.Pax + F.PaxJ + F.PaxF; RevenueSum += F.Revenue; LoadSum += F.LoadFactor;
		}
		else if (Ap && Bp && T)
		{
			const FADPBooking K = ADPDemand::Book(W, A.AirlineId, A.RouteId, *T, *Ap, *Bp, Now, Rng);
			F.Pax = K.Pax; F.PaxJ = K.PaxJ; F.PaxF = K.PaxF; F.CargoKg = K.CargoKg; F.LoadFactor = K.LoadFactor; F.Revenue = K.Revenue;
			ADPNetwork::Boost(W, F, T->Seats);   // (connections, frequent flyers, code-shares: to-do 184)
			PaxCarried += F.Pax + F.PaxJ + F.PaxF; RevenueSum += F.Revenue; LoadSum += F.LoadFactor;
		}
	}
	Profiles.Add(F.Id, P);
	// a technical failure now and then - likelier the worse the aircraft's condition: a minor one noted and fixed at the next
	// stop; a hydraulic fault checked on arrival; an engine failure: the crew turns for the nearest suitable airport
	{
		double Over = 0.0; ADPMaint::NextDue(W, A, Now, Over);   // (an overdue check: more failures - to-do 181)
		const double K = (0.6 + FMath::Max(0.0, 85.0 - A.Condition) / 25.0) * (Over > 1.0 ? 1.0 + (Over - 1.0) * 6.0 : 1.0);
		const double Roll = Rng.FRand();
		auto Note = [&](const FString& Text) { FADPNotice N; N.At = Now; N.Kind = TEXT("mechanic"); N.Text = Text; W.Notices.Add(N); };
		if (Roll < 0.000025 * K) { F.Emergency = TEXT("an engine failure"); ++Failures; }
		else if (Roll < 0.0004 * K) { A.Condition = FMath::Max(0.0, A.Condition - 4.0); ++Failures; Note(FString::Printf(TEXT("%s: a hydraulic fault on the way to %s - checked on arrival"), *A.Reg, *Next)); }
		else if (Roll < 0.004 * K) { A.Condition = FMath::Max(0.0, A.Condition - 1.0); ++Failures; Note(FString::Printf(TEXT("%s: a minor fault logged (cabin system) - fixed at the next stop"), *A.Reg)); }
	}
	// wear from every flight
	A.Condition = FMath::Max(0.0, A.Condition - (P.BlockMin() / 60.0 * 0.12 + 0.35));
	W.Flights.Add(F);
	if (Pl) { Pl->State = TEXT("gone"); Pl->Ended = Now; Pl->Why.Empty(); }
	if (A.AirlineId == UADPSim::Me()) ADP_LOG(TEXT("Schedule: %s %s %s-%s off blocks%s - %d passengers, %d no-shows"), *F.FlightNo, *F.Reg, *F.From, *F.To, F.DelayMin >= 1.0 ? *FString::Printf(TEXT(", %.0f min late"), F.DelayMin) : TEXT(" on time"), F.Pax + F.PaxJ + F.PaxF, Pl ? Pl->NoShow : 0);
	A.Status = TEXT("flying"); A.WaitingSince = FDateTime();
	ADPGround::FreeStand(A);   // (pushed back: the stand is free - to-do 183)
	++Departures;
}

void FADPFlightsModule::Land(FADPWorld& W, int32 Idx, const FDateTime& At)
{
	const FADPFlight F = W.Flights[Idx];
	W.Flights.RemoveAt(Idx);
	ADPEconomy::BookLeg(W, F, At);   // (the leg's money: part 5)
	Profiles.Remove(F.Id);
	++Landings;
	if (FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == F.Reg; }))
	{
		A->HoursMin += F.DurMin; ++A->Cycles; A->Location = F.To; A->Status = TEXT("parked");
		if (const FADPRoute* R = W.Routes.FindByPredicate([&](const FADPRoute& X) { return X.Id == A->RouteId; }))
			A->LegIdx = (A->LegIdx + 1) % FMath::Max(1, R->Stops.Num());
		A->ReadyAt = At + FTimespan::FromMinutes(ADPGround::Turnaround(W, *A, At));   // (the ground teams' work: to-do 182)
		if (F.To == ADPBoards::HereIcao()) { A->ArrFlight = F.FlightNo; A->ArrFrom = F.From; A->ArrPax = F.Pax + F.PaxJ + F.PaxF; A->ArrAt = At; }
		if (A->AirlineId == UADPSim::Me()) ADP_LOG(TEXT("Schedule: %s %s landed at %s - %d passengers"), *F.FlightNo, *F.Reg, *F.To, F.Pax + F.PaxJ + F.PaxF);
	}
	if (F.Crew.Num()) ADPRoster::Landed(W, F, At);   // (your crews' flight time and duty: to-do 177)
	else if (FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == F.PilotId; }))
	{ P->DutyMin += F.DurMin + TurnMin; P->Xp += F.DurMin / 60.0 + 20.0; }
	ADPEvents::OnLanded(W, F, At);   // (punctuality, reputation, a job paid, your career: part 8)
}

bool FADPFlightsModule::InCurfew(const FString& Icao, double Lon, const FDateTime& Utc)
{
	// well-known night curfews (local hours closed .. open); local time from the longitude (real time zones come later)
	struct FC { const TCHAR* Icao; double From, To; };
	static const FC List[] = {{TEXT("EDDF"), 23.0, 5.0}, {TEXT("LFPO"), 23.5, 6.0}, {TEXT("EGLC"), 22.5, 6.5}, {TEXT("KSNA"), 22.0, 7.0},
		{TEXT("YSSY"), 23.0, 6.0}, {TEXT("EDDL"), 23.0, 6.0}, {TEXT("LSZH"), 23.5, 6.0}};
	// our airport: its own curfew, as the town and its council have set it (to-do 199)
	if (Icao == ADPBoards::HereIcao())
	{
		double From = 0, To = 6; ADPTown::HereCurfew(From, To);
		if (From == To) return false;
		const double H = ADPBoards::Local(Utc).GetHour() + ADPBoards::Local(Utc).GetMinute() / 60.0;
		return From > To ? (H >= From || H < To) : (H >= From && H < To);
	}
	for (const FC& C : List)
		if (Icao == C.Icao)
		{
			const double H = FMath::Fmod(Utc.GetHour() + Utc.GetMinute() / 60.0 + Lon / 15.0 + 48.0, 24.0);
			return C.From > C.To ? (H >= C.From || H < C.To) : (H >= C.From && H < C.To);
		}
	return false;
}

void FADPFlightsModule::OnTheWayDown(FADPWorld& W, FADPFlight& F, const FDateTime& Now)
{
	// an emergency on board: down at the nearest suitable airport
	if (!F.Emergency.IsEmpty() && F.Diverted.IsEmpty()) { Divert(W, F, Now, *F.Emergency); return; }
	const FADPAirportInfo* B = ADPWorldData::Airport(F.To); const FADPTypeInfo* T = ADPWorldData::Type(F.TypeId);
	if (!B || !T) return;
	if (const FADPWorldEvent* E = ADPEvents::ClosedBy(W, *B, Now)) { Divert(W, F, Now, *E->Title); return; }
	const FADPWx Wx = ADPWeather::At(B->Icao.IsEmpty() ? B->Ident : B->Icao, B->Lat, B->Lon, Now);
	const double MaxCross = T->Class == TEXT("bush") ? 20.0 : T->Class == TEXT("turboprop") ? 32.0 : 38.0;
	double Best = 1e9;
	for (const FADPRunwayInfo& R : B->Runways) for (const double H : {R.LeHdg, R.LeHdg + 180.0}) { double C, Tl; ADPWeather::Components(Wx, H, C, Tl); if (Tl <= 10.0) Best = FMath::Min(Best, C); }
	if (!B->Runways.Num()) Best = 0.0;
	const bool bBelowMins = Wx.VisM < 175.0 || Best > MaxCross;
	if (Wx.bStorm || bBelowMins)
	{
		// a storm passing: hold and wait it out (7 in 10); otherwise - and below the minimums - on to the alternate
		if (!bBelowMins && Rng.FRand() < 0.7) { F.HoldMin += Rng.FRandRange(15.0, 35.0); ++Holds; return; }
		Divert(W, F, Now, bBelowMins ? TEXT("below the landing minimums at the destination") : TEXT("thunderstorms at the destination"));
	}
	else if (Rng.FRand() < 0.006) { F.HoldMin += Rng.FRandRange(5.0, 25.0); ++Holds; }   // (traffic: a short hold now and then)
}

bool FADPFlightsModule::Divert(FADPWorld& W, FADPFlight& F, const FDateTime& Now, const TCHAR* Why)
{
	// the alternate: the nearest large or medium airport 20..230 nm from the destination (from the aircraft itself in an
	// emergency) with a runway long enough for the type and weather it can land in
	const FADPAirportInfo* B = ADPWorldData::Airport(F.To); const FADPTypeInfo* T = ADPWorldData::Type(F.TypeId);
	if (!B || !T) return false;
	const bool bEm = !F.Emergency.IsEmpty();
	const double RefLat = bEm ? F.Lat : B->Lat, RefLon = bEm ? F.Lon : B->Lon;
	const double NeedFt = FMath::Max(T->LdRunwayM, T->ToRunwayM) * 3.281;
	const FADPAirportInfo* Alt = nullptr; double BestD = bEm ? 400.0 : 230.0;
	ADPWorldData::ForEachAirport([&](const FADPAirportInfo& Ap)
	{
		if ((Ap.Kind != 'L' && Ap.Kind != 'M') || Ap.Icao.IsEmpty() || Ap.Icao == F.To) return;
		const double D = ADPWorldData::DistanceNm(RefLat, RefLon, Ap.Lat, Ap.Lon);
		if (D >= BestD || (!bEm && D < 20.0) || Ap.LongestFt() < NeedFt) return;
		const FADPWx X = ADPWeather::At(Ap.Icao, Ap.Lat, Ap.Lon, Now);
		if (X.bStorm || X.VisM < 800.0 || ADPEvents::ClosedBy(W, Ap, Now)) return;
		BestD = D; Alt = &Ap;
	});
	if (!Alt) { F.HoldMin += 30.0; ++Holds; return false; }   // (nowhere better: hold, come in when it clears)
	// on to the alternate: the plan from the departure airport to it, the time flown kept (the path bends a little)
	FADPProfile P;
	if (!ADPWorldData::MakeProfile(F.TypeId, F.From, Alt->Icao, P)) return false;
	const double Flown = (Now - F.DepAt).GetTotalMinutes();
	if (Flown > P.PhaseStart[FADPProfile::Descent]) F.DepAt = Now - FTimespan::FromMinutes(P.PhaseStart[FADPProfile::Descent]);
	F.Diverted = F.To; F.To = Alt->Icao; F.bWxChecked = true;
	Profiles.Add(F.Id, P);
	++Diversions;
	FADPNotice N; N.At = Now; N.Kind = TEXT("flight"); N.Text = FString::Printf(TEXT("%s diverts to %s (%s) - %s"), *F.Reg, *Alt->Name, *Alt->Icao, Why); W.Notices.Add(N);
	return true;
}
