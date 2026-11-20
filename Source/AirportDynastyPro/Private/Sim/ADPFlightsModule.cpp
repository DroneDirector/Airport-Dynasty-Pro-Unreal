// Hired pilots flying the routes - see Sim/ADPFlightsModule.h.
#include "Sim/ADPFlightsModule.h"
#include "ADPLog.h"
#include "Sim/ADPWeather.h"
#include "Sim/ADPDemand.h"

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
		if (A.Status == TEXT("parked") && !A.RouteId.IsEmpty() && A.ReadyAt <= To) TryDepart(W, A, To);
}

void FADPFlightsModule::TryDepart(FADPWorld& W, FADPAircraft& A, const FDateTime& Now)
{
	const FADPRoute* R = W.Routes.FindByPredicate([&](const FADPRoute& X) { return X.Id == A.RouteId; });
	if (!R || R->Stops.Num() < 2) return;
	FADPPerson* Pilot = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == R->PilotId; });
	auto Wait = [&](const TCHAR* Why, double Min)
	{
		if (A.WaitingSince == FDateTime()) A.WaitingSince = Now;
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
	// the checks before departure (more come with their parts: weather, ground crew, strikes, curfews, stands)
	if (!Pilot || Pilot->Status != TEXT("on")) { Wait(TEXT("no pilot"), 60); return; }
	if (Pilot->RestUntil > Now) { A.ReadyAt = Pilot->RestUntil; return; }
	if (A.Condition < 20.0) { Wait(TEXT("the aircraft needs repair"), 120); return; }
	const FString& Next = R->Stops[(A.LegIdx + 1) % R->Stops.Num()];
	FADPProfile P;
	if (!ADPWorldData::MakeProfile(A.TypeId, A.Location, Next, P)) { Wait(TEXT("no flight plan"), 240); return; }
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
	// night curfews: no departure while the field is closed, nor one that would land at a closed destination
	{
		const FADPAirportInfo* Ap = ADPWorldData::Airport(A.Location); const FADPAirportInfo* Bp = ADPWorldData::Airport(Next);
		if ((Ap && InCurfew(Ap->Icao, Ap->Lon, Now)) || (Bp && InCurfew(Bp->Icao, Bp->Lon, Now + FTimespan::FromMinutes(P.BlockMin()))))
		{ ++Curfews; A.ReadyAt = Now + FTimespan::FromMinutes(30); return; }
	}
	// the duty limit: the pilot rests 12 hours first if this leg would break it
	if (Pilot->DutyMin + P.BlockMin() > MaxDutyH * 60.0)
	{
		Pilot->RestUntil = Now + FTimespan::FromHours(RestH); Pilot->DutyMin = 0.0; A.ReadyAt = Pilot->RestUntil; ++Rests;
		return;
	}
	// delays at random: at the gate mostly short, now and then long (weather-driven delays come with the weather)
	const double Delay = (Rng.FRand() < 0.88 ? Rng.FRandRange(0.0, 8.0) : Rng.FRandRange(15.0, 74.0)) + WxDelay;
	if (Delay >= 15.0) ++Delayed;
	FADPFlight F;
	F.Id = FString::Printf(TEXT("%s-%lld"), *A.Reg, Now.GetTicks() / ETimespan::TicksPerSecond);
	F.Reg = A.Reg; F.AirlineId = A.AirlineId; F.PilotId = Pilot->Id; F.TypeId = A.TypeId; F.From = A.Location; F.To = Next;
	F.DepAt = Now + FTimespan::FromMinutes(Delay); F.DelayMin = Delay; F.DurMin = P.BlockMin();
	F.Lat = P.FromLat; F.Lon = P.FromLon;
	// who flies: the passengers, classes and cargo this airline gets on this pair today (part 4)
	{
		const FADPAirportInfo* Ap = ADPWorldData::Airport(A.Location); const FADPAirportInfo* Bp = ADPWorldData::Airport(Next); const FADPTypeInfo* T = ADPWorldData::Type(A.TypeId);
		if (Ap && Bp && T)
		{
			const FADPBooking K = ADPDemand::Book(W, A.AirlineId, A.RouteId, *T, *Ap, *Bp, Now, Rng);
			F.Pax = K.Pax; F.PaxJ = K.PaxJ; F.PaxF = K.PaxF; F.CargoKg = K.CargoKg; F.LoadFactor = K.LoadFactor; F.Revenue = K.Revenue;
			PaxCarried += K.Pax + K.PaxJ + K.PaxF; RevenueSum += K.Revenue; LoadSum += K.LoadFactor;
		}
	}
	Profiles.Add(F.Id, P);
	// a technical failure now and then - likelier the worse the aircraft's condition: a minor one noted and fixed at the next
	// stop; a hydraulic fault checked on arrival; an engine failure: the crew turns for the nearest suitable airport
	{
		const double K = 0.6 + FMath::Max(0.0, 85.0 - A.Condition) / 25.0;
		const double Roll = Rng.FRand();
		auto Note = [&](const FString& Text) { FADPNotice N; N.At = Now; N.Kind = TEXT("mechanic"); N.Text = Text; W.Notices.Add(N); };
		if (Roll < 0.000025 * K) { F.Emergency = TEXT("an engine failure"); ++Failures; }
		else if (Roll < 0.0004 * K) { A.Condition = FMath::Max(0.0, A.Condition - 4.0); ++Failures; Note(FString::Printf(TEXT("%s: a hydraulic fault on the way to %s - checked on arrival"), *A.Reg, *Next)); }
		else if (Roll < 0.004 * K) { A.Condition = FMath::Max(0.0, A.Condition - 1.0); ++Failures; Note(FString::Printf(TEXT("%s: a minor fault logged (cabin system) - fixed at the next stop"), *A.Reg)); }
	}
	// wear from every flight
	A.Condition = FMath::Max(0.0, A.Condition - (P.BlockMin() / 60.0 * 0.12 + 0.35));
	W.Flights.Add(F);
	A.Status = TEXT("flying"); A.WaitingSince = FDateTime();
	++Departures;
}

void FADPFlightsModule::Land(FADPWorld& W, int32 Idx, const FDateTime& At)
{
	const FADPFlight F = W.Flights[Idx];
	W.Flights.RemoveAt(Idx);
	Profiles.Remove(F.Id);
	++Landings;
	if (FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == F.Reg; }))
	{
		A->HoursMin += F.DurMin; ++A->Cycles; A->Location = F.To; A->Status = TEXT("parked");
		if (const FADPRoute* R = W.Routes.FindByPredicate([&](const FADPRoute& X) { return X.Id == A->RouteId; }))
			A->LegIdx = (A->LegIdx + 1) % FMath::Max(1, R->Stops.Num());
		A->ReadyAt = At + FTimespan::FromMinutes(TurnMin);
	}
	if (FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == F.PilotId; }))
	{ P->DutyMin += F.DurMin + TurnMin; P->Xp += F.DurMin / 60.0 + 20.0; }
}

bool FADPFlightsModule::InCurfew(const FString& Icao, double Lon, const FDateTime& Utc)
{
	// well-known night curfews (local hours closed .. open); local time from the longitude (real time zones come later)
	struct FC { const TCHAR* Icao; double From, To; };
	static const FC List[] = {{TEXT("EDDF"), 23.0, 5.0}, {TEXT("LFPO"), 23.5, 6.0}, {TEXT("EGLC"), 22.5, 6.5}, {TEXT("KSNA"), 22.0, 7.0},
		{TEXT("YSSY"), 23.0, 6.0}, {TEXT("EDDL"), 23.0, 6.0}, {TEXT("LSZH"), 23.5, 6.0}};
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
		if (X.bStorm || X.VisM < 800.0) return;
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
