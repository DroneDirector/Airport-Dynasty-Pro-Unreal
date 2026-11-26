// Maintenance in the simulation - see Sim/ADPMaint.h.
#include "Sim/ADPMaint.h"
#include "Sim/ADPCrew.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPBoards.h"
#include "Site.h"
#include "ADPLog.h"
#include "Misc/ConfigCacheIni.h"
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

const TArray<ADPMaint::FFault>& ADPMaint::Faults()
{
	// Fast: the owner's 1.5-3 h by fault (2026-11-26). Real: an A320-sized aircraft's usual time on the ground for it -
	// an engine change with its ground run about 36 h, a gear leg or actuator 16 h, a wheel and brake change 3 h, a flap or
	// slat drive 10 h, a spoiler actuator 6 h, a cargo door seal or latch 8 h, a tail strike's inspection and skin repair
	// about 4 days; the A, C and D checks their rule's length (Rules: about 10 h, 14 days, 42 days for a narrowbody).
	static const TArray<FFault> MxFaultList = {
		{TEXT("engine"), TEXT("an engine fault"), 3.0, 36.0, 400000.0},
		{TEXT("gear"), TEXT("a landing gear fault"), 2.5, 16.0, 60000.0},
		{TEXT("brakes"), TEXT("worn wheels and brakes"), 1.5, 3.0, 15000.0},
		{TEXT("flaps"), TEXT("a flaps or slats fault"), 2.25, 10.0, 40000.0},
		{TEXT("spoilers"), TEXT("a spoiler fault"), 1.75, 6.0, 20000.0},
		{TEXT("cargodoor"), TEXT("a cargo bay door fault"), 2.0, 8.0, 15000.0},
		{TEXT("tailstrike"), TEXT("a tail strike"), 3.0, 96.0, 250000.0},
		{TEXT("acheck"), TEXT("the routine A-check"), 2.0, 0.0, 0.0},
		{TEXT("ccheck"), TEXT("the C-check"), 3.0, 0.0, 0.0},
		{TEXT("dcheck"), TEXT("the D-check"), 3.0, 0.0, 0.0}};
	return MxFaultList;
}

const ADPMaint::FFault* ADPMaint::Fault(const FString& Key)
{
	for (const FFault& F : Faults()) if (Key == F.Key) return &F;
	return nullptr;
}

int32 ADPMaint::MaintTime()
{
	int32 Mx = 1;
	if (GConfig) GConfig->GetInt(TEXT("ADP.Realism"), TEXT("MaintTime"), Mx, GGameUserSettingsIni);
	return FMath::Clamp(Mx, 0, 2);
}

void ADPMaint::SetMaintTime(int32 Mode)
{
	if (!GConfig) return;
	GConfig->SetInt(TEXT("ADP.Realism"), TEXT("MaintTime"), FMath::Clamp(Mode, 0, 2), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	ADP_LOG(TEXT("Maintenance: time set to %s - for the checks that begin from now on"), Mode == 0 ? TEXT("Quick") : Mode == 2 ? TEXT("Real") : TEXT("Fast"));
}

double ADPMaint::CheckHours(const FString& FaultKey, double RealH, int32 Mode)
{
	if (Mode == 0) return 0.5;
	const FFault* F = Fault(FaultKey);
	if (Mode == 1) return F ? F->FastH : 2.0;
	return FMath::Max(0.5, RealH);
}

FString ADPMaint::RollFault(const TCHAR* What, double Roll01)
{
	const FString Wk(What);
	if (Wk == TEXT("engine")) return TEXT("engine");
	if (Wk == TEXT("tailstrike")) return TEXT("tailstrike");
	if (Wk == TEXT("hydraulic"))   // (the hydraulic users: gear, brakes, flaps and slats, spoilers)
		return Roll01 < 0.3 ? TEXT("gear") : Roll01 < 0.6 ? TEXT("brakes") : Roll01 < 0.8 ? TEXT("flaps") : TEXT("spoilers");
	return Roll01 < 0.5 ? TEXT("cargodoor") : TEXT("brakes");   // (a minor one that keeps it on the ground: a door latch, worn brakes)
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
	const FCheckRule* R = nullptr; TArray<FCheckRule> Rs = Rules(A.TypeId);
	// a fault check (F, to-do 311): the fault found in flight put right - 2 mechanics, in a hangar where there is one
	const FFault* Ft = Kind == TEXT("F") ? Fault(A.FaultDue) : nullptr;
	if (Kind == TEXT("F"))
	{
		if (!Ft) { Why = TEXT("no fault to put right"); return false; }
		const FADPTypeInfo* Ti = ADPWorldData::Type(A.TypeId);
		const double Kv = (Ti ? FMath::Max(2.0, Ti->ValueM) : 100.0) / 100.0;
		Rs.Add({TEXT("F"), 0.0, 0.0, 0.0, Ft->RealH, 2, Ft->PartsUsd * Kv, true});
	}
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
		if (Mech > 0 && A.AirlineId == UADPSim::Me()) Hours *= ADPCrew::SlowTeam(W, TEXT("mech"), A.Location, R->Mechanics);   // (beginners: to-do 301)
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
	// its length from Settings > Realism at this moment (to-do 311): kept on the check, so changing the setting later
	// changes only the checks that begin after it; game time, so it follows the game's speed
	const int32 Mode = MaintTime();
	A.CheckFault = Ft ? FString(Ft->Key) : Kind == TEXT("A") ? FString(TEXT("acheck")) : Kind == TEXT("C") ? FString(TEXT("ccheck")) : FString(TEXT("dcheck"));
	if (Ft) A.FaultDue.Empty();
	A.CheckLenH = CheckHours(A.CheckFault, Hours, Mode); A.CheckStart = Now;
	A.Status = TEXT("maintenance"); A.CheckKind = R->Kind; A.CheckUntil = Now + FTimespan::FromHours(A.CheckLenH);
	const FFault* Fn = Fault(A.CheckFault);
	ADP_LOG(TEXT("Maintenance: %s - %s (%s check) at %s, %.2f h (%s)"), *A.Reg, Fn ? Fn->Name : TEXT("?"), R->Kind, *A.Location, A.CheckLenH, Mode == 0 ? TEXT("Quick") : Mode == 2 ? TEXT("Real") : TEXT("Fast"));
	if (bMine)
	{
		const FString What = Ft ? FString::Printf(TEXT("%s: %s being put right"), *A.Reg, Ft->Name) : FString::Printf(TEXT("%s in its %s-check"), *A.Reg, R->Kind);
		MxNote(W, Now, Ft ? 1 : 0, What, FString::Printf(TEXT("%s at %s until %s."), *What, *A.Location, *ADPBoards::Local(A.CheckUntil).ToString(TEXT("%d.%m %H:%M"))));
	}
	return true;
}

void FADPMaintModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
	FADPWorld& W = Sim.World;
	for (FADPAircraft& A : W.Aircraft)
	{
		if (A.InService == FDateTime()) A.InService = To;
		// checks finishing
		if (A.Status == TEXT("maintenance") && A.CheckUntil <= To && A.CheckKind == TEXT("F"))   // (a fault put right: no check record)
		{
			const ADPMaint::FFault* Fd = ADPMaint::Fault(A.CheckFault);
			A.Condition = FMath::Min(100.0, A.Condition + 2.0); A.Status = TEXT("parked"); A.ReadyAt = FMath::Max(A.ReadyAt, To);
			if (A.AirlineId == UADPSim::Me()) MxNote(W, To, 0, FString::Printf(TEXT("%s back in service"), *A.Reg), FString::Printf(TEXT("%s: %s put right."), *A.Reg, Fd ? Fd->Name : TEXT("its fault")));
			A.CheckKind.Empty(); A.CheckFault.Empty(); ++Checks;
			continue;
		}
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
			A.CheckKind.Empty(); A.CheckFault.Empty(); ++Checks;
			continue;
		}
		if (A.Status == TEXT("flying") || A.Status == TEXT("maintenance")) continue;
		// a fault found in flight: put right where it stands before it flies again (a hangar at ours; tried each half hour)
		if (!A.FaultDue.IsEmpty())
		{
			FString Why;
			if (!ADPMaint::StartCheck(W, A, TEXT("F"), To, Why) && A.Status != TEXT("grounded")) { A.Status = TEXT("grounded"); ADP_LOG(TEXT("Maintenance: %s waits on the ground with %s - %s"), *A.Reg, *A.FaultDue, *Why); }
			continue;
		}
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
