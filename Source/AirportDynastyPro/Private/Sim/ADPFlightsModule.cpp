// Hired pilots flying the routes - see Sim/ADPFlightsModule.h.
#include "Sim/ADPFlightsModule.h"
#include "ADPLog.h"
#include "Sim/ADPWeather.h"

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
		const double Min = (To - F.DepAt).GetTotalMinutes();
		if (Min >= P->BlockMin()) { Land(W, i, F.DepAt + FTimespan::FromMinutes(P->BlockMin())); continue; }
		const FADPProfile::FState S = P->At(FMath::Max(0.0, Min));
		F.Phase = FADPProfile::PhaseName(S.Phase); F.Lat = S.Lat; F.Lon = S.Lon; F.AltFt = S.AltFt; F.GsKt = S.GsKt; F.HeadingDeg = S.HeadingDeg;
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
	Profiles.Add(F.Id, P);
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
