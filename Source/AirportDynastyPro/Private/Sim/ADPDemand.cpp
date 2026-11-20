// Passengers and demand - see Sim/ADPDemand.h.
#include "Sim/ADPDemand.h"
#include "Sim/ADPSimData.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPEvents.h"

namespace
{
	// how much an airport generates: large hubs a lot, small fields little (a scheduled-service field more than one without)
	double Pull(const FADPAirportInfo& A)
	{
		const double K = A.Kind == 'L' ? 6.5 : A.Kind == 'M' ? 2.6 : A.Kind == 'S' ? 0.9 : 0.3;
		return K * (A.bScheduled ? 1.0 : 0.45);
	}
	// travel by distance: few take a plane for a short hop, most trips are a few hundred to 1,500 nm, fewer go very far
	double ByDistance(double Nm)
	{
		const double L = FMath::Loge(FMath::Max(Nm, 20.0)) - FMath::Loge(700.0);
		return FMath::Clamp(1.12 * FMath::Exp(-L * L / 3.2), 0.35, 1.12);
	}
	// the season: summer busiest, the depth of winter quietest (the southern half of the world the other way round)
	double Season(double Lat, const FDateTime& When)
	{
		const double Peak = Lat >= 0 ? 200.0 : 17.0;   // (day of year: about 19 July / 17 January)
		return 1.0 + 0.18 * FMath::Cos((When.GetDayOfYear() - Peak) / 365.25 * 2.0 * PI);
	}
}

double ADPDemand::MarketPax(const FADPAirportInfo& A, const FADPAirportInfo& B, const FDateTime& When)
{
	const double D = ADPWorldData::DistanceNm(A.Lat, A.Lon, B.Lat, B.Lon);
	// (a stable per-pair character: some city pairs simply travel more)
	FRandomStream Pair(int32(GetTypeHash(A.Ident) ^ (GetTypeHash(B.Ident) * 7)));
	return 25.0 * Pull(A) * Pull(B) * ByDistance(D) * Season((A.Lat + B.Lat) / 2.0, When) * Pair.FRandRange(0.85, 1.15);
}

double ADPDemand::EconomyFare(double Nm, const FString& Class, bool bIntl)
{
	double F;
	if (Class == TEXT("turboprop")) F = 58.0 + 0.33 * Nm;
	else if (Class == TEXT("bush") || Class == TEXT("helicopter")) F = 85.0 + 1.05 * Nm;
	else if (Class == TEXT("regional")) F = 52.0 + 0.24 * Nm;
	else if (Class == TEXT("widebody")) F = 140.0 + 0.135 * Nm;
	else F = 42.0 + 0.155 * Nm;
	return F * (bIntl ? 1.12 : 1.0);
}

FADPBooking ADPDemand::Book(const FADPWorld& W, const FString& AirlineId, const FString& RouteId, const FADPTypeInfo& T,
	const FADPAirportInfo& A, const FADPAirportInfo& B, const FDateTime& When, FRandomStream& Rng)
{
	FADPBooking K;
	const double Nm = ADPWorldData::DistanceNm(A.Lat, A.Lon, B.Lat, B.Lon);
	const FADPRoute* Mine = W.Routes.FindByPredicate([&](const FADPRoute& R) { return R.Id == RouteId; });
	const double MyFare = Mine ? FMath::Clamp(Mine->FareIndex, 0.4, 3.0) : 1.0;
	const FADPAirline* Me = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == AirlineId; });
	const double MyQ = Me ? Me->Quality : 0.75;
	// who else flies this pair: the other airlines' routes with both airports in them (each counts once)
	double Others = 0.0; TSet<FString> Seen;
	for (const FADPRoute& R : W.Routes)
		if (R.AirlineId != AirlineId && R.Stops.Contains(A.Ident) && R.Stops.Contains(B.Ident) && !Seen.Contains(R.AirlineId))
		{
			Seen.Add(R.AirlineId);
			const FADPAirline* O = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == R.AirlineId; });
			Others += ADPEvents::RepFactor(O) * (0.4 + (O ? O->Quality : 0.75)) * FMath::Pow(FMath::Clamp(R.FareIndex, 0.4, 3.0), -1.6);
		}
	const double MyPull = ADPEvents::RepFactor(Me) * (0.4 + MyQ) * FMath::Pow(MyFare, -1.6) * (1.0 + ADPEconomy::CampaignBoost(W, AirlineId, RouteId, When));
	// the market reacts to price too: cheap fares bring more people out
	const double Market = MarketPax(A, B, When) * FMath::Pow(MyFare, -0.6) * ADPEvents::DemandFactor(W, A, B, When);
	const double Share = MyPull / (MyPull + Others);
	// my departures on this pair a day (each aircraft on it does about two a day) split my share between them
	int32 Planes = 0;
	for (const FADPAircraft& X : W.Aircraft) if (X.AirlineId == AirlineId && X.RouteId == RouteId) ++Planes;
	const double PerDeparture = Market * Share / FMath::Max(1.0, Planes * 2.0);
	const int32 Seats = FMath::Max(1, T.Seats);
	K.LoadFactor = FMath::Clamp(1.0 - FMath::Exp(-1.2 * PerDeparture / Seats), 0.08, 0.97) * Rng.FRandRange(0.93, 1.05);
	K.LoadFactor = FMath::Clamp(K.LoadFactor, 0.05, 1.0);
	const int32 All = FMath::RoundToInt(Seats * K.LoadFactor);
	// classes: business more on long and international legs, first only on long widebody legs
	const bool bIntl = A.Country != B.Country;
	const double JShare = (Nm > 2500 ? 0.11 : Nm > 900 ? 0.07 : 0.035) * (bIntl ? 1.2 : 1.0) * (T.Class == TEXT("turboprop") || T.Class == TEXT("bush") ? 0.0 : 1.0);
	const double FShare = T.Class == TEXT("widebody") && Nm > 3000 ? 0.02 : 0.0;
	K.PaxF = FMath::RoundToInt(All * FShare); K.PaxJ = FMath::RoundToInt(All * JShare); K.Pax = All - K.PaxJ - K.PaxF;
	// cargo in the hold: a share of what fits, more on widebodies
	const double HoldKg = Seats * (T.Class == TEXT("widebody") ? 55.0 : 14.0);
	K.CargoKg = FMath::RoundToDouble(HoldKg * Rng.FRandRange(0.2, 0.8));
	// the ticket money (the economy books it with the costs, part 5)
	K.FareY = EconomyFare(Nm, T.Class, bIntl) * MyFare;
	const double JMult = Nm > 2500 ? 3.6 : Nm > 900 ? 2.9 : 2.3, FMult = Nm > 4000 ? 6.5 : 5.0;
	K.Revenue = K.FareY * (K.Pax + K.PaxJ * JMult + K.PaxF * FMult) + K.CargoKg * FMath::Max(0.6, 0.5 + 0.0006 * Nm);
	return K;
}
