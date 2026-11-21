// Maintenance in the simulation - see Sim/ADPMaint.h.
#include "Sim/ADPMaint.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPBoards.h"
#include "Site.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	void MxNote(FADPWorld& W, const FDateTime& At, int32 Prio, const FString& Title, const FString& Text)
	{ FADPNotice N; N.At = At; N.Kind = TEXT("mechanic"); N.Priority = Prio; N.Title = Title; N.Text = Text; W.Notices.Add(N); }
	int32 MxMechanicsAt(const FADPWorld& W, const FString& AirlineId, const FString& Icao)
	{
		int32 N = 0;
		for (const FADPPerson& P : W.People) if (P.AirlineId == AirlineId && P.Role == TEXT("mech") && P.Base == Icao && P.Status == TEXT("on")) ++N;
		return N;
	}
}

TArray<ADPMaint::FCheckRule> ADPMaint::Rules(const FString& TypeId)
{
	const FADPTypeInfo* T = ADPWorldData::Type(TypeId);
	const FString C = T ? T->Class : FString(TEXT("narrowbody"));
	const double V = T ? FMath::Max(2.0, T->ValueM) : 100.0;   // (parts cost follows the aircraft's value)
	const double K = V / 100.0;
	if (C == TEXT("bush") || C == TEXT("helicopter"))
		return {{TEXT("A"), 100, 100, 60, 4, 1, 4000 * K, false}, {TEXT("C"), 1500, 1500, 365, 72, 2, 40000 * K, true}, {TEXT("D"), 6000, 6000, 365 * 6, 240, 3, 200000 * K, true}};
	if (C == TEXT("turboprop") || C == TEXT("regional"))
		return {{TEXT("A"), 500, 500, 90, 8, 2, 12000 * K, false}, {TEXT("C"), 5000, 5000, 730, 240, 6, 300000 * K, true}, {TEXT("D"), 20000, 20000, 365 * 6, 720, 8, 1500000 * K, true}};
	if (C == TEXT("widebody"))
		return {{TEXT("A"), 1000, 600, 120, 14, 3, 60000 * K, false}, {TEXT("C"), 9000, 4000, 730, 480, 12, 900000 * K, true}, {TEXT("D"), 40000, 15000, 365 * 6, 1440, 16, 5000000 * K, true}};
	return {{TEXT("A"), 750, 750, 120, 10, 2, 25000 * K, false}, {TEXT("C"), 7500, 5000, 730, 336, 8, 600000 * K, true}, {TEXT("D"), 30000, 20000, 365 * 6, 1008, 12, 3000000 * K, true}};
}

FString ADPMaint::NextDue(const FADPWorld& W, const FADPAircraft& A, const FDateTime& Now, double& Fraction)
{
	FString Best; Fraction = 0.0;
	for (const FCheckRule& R : Rules(A.TypeId))
	{
		const FADPCheckRecord* C = A.Checks.FindByPredicate([&](const FADPCheckRecord& X) { return X.Kind == R.Kind; });
		const double H = (A.HoursMin - (C ? C->AtHoursMin : 0.0)) / 60.0, Cy = A.Cycles - (C ? C->AtCycles : 0);
		const FDateTime Since = C ? C->At : (A.InService != FDateTime() ? A.InService : Now);
		const double F = FMath::Max3(H / R.Hours, Cy / R.Cycles, (Now - Since).GetTotalDays() / R.Days);
		if (F > Fraction) { Fraction = F; Best = R.Kind; }
	}
	return Best;
}

int32 ADPMaint::HangarBays(const FString& Icao)
{
	if (Icao != ADPBoards::HereIcao()) return 1;
	static int32 Bays = -1;
	if (Bays < 0)
	{
		Bays = 1;
		FString T; TSharedPtr<FJsonObject> J;
		if ((FFileHelper::LoadFileToString(T, *ADPSite::File(TEXT("airfield.json"))) || FFileHelper::LoadFileToString(T, *(FPaths::ProjectContentDir() / TEXT("Data/airfield.json"))) || FFileHelper::LoadFileToString(T, *(FPaths::ProjectDir() / TEXT("Data/airfield.json")))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T), J) && J)
		{ const TArray<TSharedPtr<FJsonValue>>* H; if (J->TryGetArrayField(TEXT("hangars"), H)) Bays = FMath::Max(1, H->Num()); }
	}
	return Bays;
}

bool ADPMaint::BuyParts(FADPWorld& W, const FString& AirlineId, double Usd, const FDateTime& Now)
{
	FADPAirline* Al = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == AirlineId; });
	const double Cost = Usd * W.PartsIndex;
	if (!Al || Usd <= 0.0 || Al->Balance < Cost) return false;
	ADPEconomy::Pay(W, AirlineId, Now, TEXT("maint"), -Cost, FString::Printf(TEXT("Parts for the stock ($%.0f at today's prices)"), Usd));
	Al->PartsStock += Usd;
	return true;
}

bool ADPMaint::StartCheck(FADPWorld& W, FADPAircraft& A, const FString& Kind, const FDateTime& Now, FString& Why)
{
	const FCheckRule* R = nullptr; const TArray<FCheckRule> Rs = Rules(A.TypeId);
	for (const FCheckRule& X : Rs) if (Kind == X.Kind) R = &X;
	if (!R) { Why = TEXT("no such check"); return false; }
	if (A.Status == TEXT("flying")) { Why = TEXT("in the air"); return false; }
	if (A.Status == TEXT("maintenance")) { Why = TEXT("already in a check"); return false; }
	const bool bMine = A.AirlineId == UADPSim::Me();
	double Labour = 1.0, Hours = R->DurationH;
	if (bMine)
	{
		const int32 Mech = MxMechanicsAt(W, A.AirlineId, A.Location);
		if (Mech == 0) { Labour = 3.0; }   // (an outside team flown in)
		else if (Mech < R->Mechanics) Hours *= double(R->Mechanics) / Mech;
		if (R->bHangar)
		{
			int32 Busy = 0; for (const FADPAircraft& X : W.Aircraft) if (X.Location == A.Location && X.Status == TEXT("maintenance") && X.CheckKind != TEXT("A")) ++Busy;
			if (Busy >= HangarBays(A.Location)) { Why = TEXT("no free hangar bay"); return false; }
		}
		// parts from the stock; short: bought on the spot (AOG, 1.3x)
		FADPAirline* Al = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == A.AirlineId; });
		const double Need = R->PartsUsd;
		if (Al)
		{
			const double From = FMath::Min(Al->PartsStock, Need); Al->PartsStock -= From;
			if (Need - From > 1.0) ADPEconomy::Pay(W, A.AirlineId, Now, TEXT("maint"), -(Need - From) * W.PartsIndex * 1.3, FString::Printf(TEXT("%s: %s-check parts bought AOG"), *A.Reg, R->Kind), A.Reg);
		}
		ADPEconomy::Pay(W, A.AirlineId, Now, TEXT("maint"), -R->DurationH * R->Mechanics * 85.0 * (Labour - 1.0 + 0.25), FString::Printf(TEXT("%s: %s-check labour%s"), *A.Reg, R->Kind, Labour > 1.0 ? TEXT(" (outside team)") : TEXT("")), A.Reg);
	}
	else ADPEconomy::Pay(W, A.AirlineId, Now, TEXT("maint"), -R->PartsUsd * W.PartsIndex, FString::Printf(TEXT("%s: %s-check"), *A.Reg, R->Kind), A.Reg);
	A.Status = TEXT("maintenance"); A.CheckKind = R->Kind; A.CheckUntil = Now + FTimespan::FromHours(Hours);
	if (bMine) MxNote(W, Now, 0, FString::Printf(TEXT("%s in its %s-check"), *A.Reg, R->Kind), FString::Printf(TEXT("%s is in its %s-check at %s until %s."), *A.Reg, R->Kind, *A.Location, *ADPBoards::Local(A.CheckUntil).ToString(TEXT("%d.%m %H:%M"))));
	return true;
}

void FADPMaintModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
	FADPWorld& W = Sim.World;
	for (FADPAircraft& A : W.Aircraft)
	{
		if (A.InService == FDateTime()) A.InService = To;
		// checks finishing
		if (A.Status == TEXT("maintenance") && A.CheckUntil <= To)
		{
			FADPCheckRecord* C = A.Checks.FindByPredicate([&](const FADPCheckRecord& X) { return X.Kind == A.CheckKind; });
			if (!C) { A.Checks.AddDefaulted(); C = &A.Checks.Last(); C->Kind = A.CheckKind; }
			C->At = To; C->AtHoursMin = A.HoursMin; C->AtCycles = A.Cycles;
			// a heavier check counts as the lighter ones too
			for (FADPCheckRecord& X : A.Checks) if ((A.CheckKind == TEXT("D")) || (A.CheckKind == TEXT("C") && X.Kind == TEXT("A"))) { X.At = To; X.AtHoursMin = A.HoursMin; X.AtCycles = A.Cycles; }
			if (A.CheckKind == TEXT("D")) for (const TCHAR* K : {TEXT("A"), TEXT("C")}) if (!A.Checks.ContainsByPredicate([&](const FADPCheckRecord& X) { return X.Kind == K; })) { FADPCheckRecord N; N.Kind = K; N.At = To; N.AtHoursMin = A.HoursMin; N.AtCycles = A.Cycles; A.Checks.Add(N); }
			A.Condition = A.CheckKind == TEXT("D") ? 100.0 : A.CheckKind == TEXT("C") ? FMath::Max(A.Condition, 96.0) : FMath::Min(100.0, A.Condition + 8.0);
			A.Status = TEXT("parked"); A.ReadyAt = FMath::Max(A.ReadyAt, To);
			if (A.AirlineId == UADPSim::Me()) MxNote(W, To, 0, FString::Printf(TEXT("%s back in service"), *A.Reg), FString::Printf(TEXT("%s finished its %s-check - condition %.0f%%."), *A.Reg, *A.CheckKind, A.Condition));
			A.CheckKind.Empty(); ++Checks;
			continue;
		}
		if (A.Status == TEXT("flying") || A.Status == TEXT("maintenance")) continue;
		double F = 0.0; const FString Due = ADPMaint::NextDue(W, A, To, F);
		if (Due.IsEmpty()) continue;
		// due (95% of the interval) at its base: the check starts; 10% overdue anywhere: grounded until checked there
		const bool bAtBase = A.Location == A.Base;
		if ((F >= 0.95 && bAtBase) || F >= 1.10)
		{
			FString Why;
			if (!ADPMaint::StartCheck(W, A, Due, To, Why) && F >= 1.10 && A.Status != TEXT("grounded"))
			{
				A.Status = TEXT("grounded"); ++Groundings;
				if (A.AirlineId == UADPSim::Me()) MxNote(W, To, 2, FString::Printf(TEXT("%s grounded"), *A.Reg), FString::Printf(TEXT("%s is grounded: its %s-check is overdue (%s). It may not fly until checked."), *A.Reg, *Due, *Why));
			}
		}
		else if (A.Status == TEXT("grounded") && F < 1.10) A.Status = TEXT("parked");
		// a grounded aircraft tries again each half hour (a bay or a mechanic may be free now)
		if (A.Status == TEXT("grounded")) { FString Why; ADPMaint::StartCheck(W, A, Due, To, Why); }
	}
}
