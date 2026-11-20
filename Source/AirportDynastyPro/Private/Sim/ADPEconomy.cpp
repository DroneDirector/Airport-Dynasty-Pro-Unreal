// The money - see Sim/ADPEconomy.h.
#include "Sim/ADPEconomy.h"
#include "Sim/ADPWorldData.h"

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
	// income
	Pay(W, F.AirlineId, At, TEXT("revenue"), F.Revenue * KI, Leg + TEXT(": tickets and cargo"), F.Id);
	// fuel: the type's burn for the time flown (taxiing burns little), at the departure airport's price
	const double FuelKg = T->BurnKgH * BlockH * 0.92;
	Pay(W, F.AirlineId, At, TEXT("fuel"), -FuelKg * FuelPrice(F.From, At) * KC, FString::Printf(TEXT("%s: fuel %.0f kg"), *Leg, FuelKg), F.Id);
	// fees: landing by weight and airport size, parking, handling per passenger and per turnaround, air navigation by
	// distance and weight
	const double Landing = MtowT * SizeFee(B, 9.0, 6.5, 4.5);
	const double Parking = MtowT * SizeFee(B, 1.8, 1.2, 0.7) + SizeFee(B, 120.0, 60.0, 30.0);
	const double Handling = Pax * (bIntl ? 9.5 : 6.5) + SizeFee(B, 850.0, 400.0, 160.0);
	const double Nav = Nm * 1.852 / 100.0 * FMath::Sqrt(MtowT / 50.0) * 78.0;
	Pay(W, F.AirlineId, At, TEXT("fees"), -(Landing + Parking + Handling + Nav) * KC, FString::Printf(TEXT("%s: landing $%.0f, parking $%.0f, handling $%.0f, navigation $%.0f"), *Leg, Landing, Parking, Handling, Nav), F.Id);
	// the maintenance reserve, crew allowances, booking costs and overhead
	Pay(W, F.AirlineId, At, TEXT("maint"), -MaintPerHour(T->Class) * BlockH * KC, Leg + TEXT(": maintenance reserve"), F.Id);
	const int32 Crew = 2 + FMath::CeilToInt(T->Seats / 50.0);
	Pay(W, F.AirlineId, At, TEXT("crew"), -Crew * (BlockH > 6.0 ? 250.0 : 115.0) * KC, FString::Printf(TEXT("%s: allowances for %d crew"), *Leg, Crew), F.Id);
	Pay(W, F.AirlineId, At, TEXT("fees"), -F.Revenue * (0.055 + 0.07) * KC, Leg + TEXT(": bookings and overhead"), F.Id);
}

void FADPEconomyModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
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
