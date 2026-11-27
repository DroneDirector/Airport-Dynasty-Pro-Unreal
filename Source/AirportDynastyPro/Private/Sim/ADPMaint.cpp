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
	// hired mechanics free at a place for a new check: those on duty there, less the crews of the checks already running there
	int32 MxFreeMechanics(const FADPWorld& W, const FADPAircraft& A)
	{
		int32 Busy = 0;
		for (const FADPAircraft& X : W.Aircraft)
			if (&X != &A && X.AirlineId == A.AirlineId && X.Location == A.Location && X.Status == TEXT("maintenance")) Busy += FMath::Max(0, X.CheckCrew);
		return FMath::Max(0, MxMechanicsAt(W, A.AirlineId, A.Location) - Busy);
	}
	FString MxWaitWhy(int32 Need, int32 Free)
	{
		return Free <= 0 ? FString::Printf(TEXT("waits for %d mechanics: none free here - hire mechanics in Staff"), Need)
			: FString::Printf(TEXT("waits for %d mechanics: only %d free here - hire more in Staff"), Need, Free);
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
		return {{TEXT("A"), 1000, 600, 120, 14, 6, 60000 * K, false}, {TEXT("C"), 9000, 4000, 730, 480, 12, 900000 * K, true}, {TEXT("D"), 40000, 15000, 365 * 6, 1440, 16, 5000000 * K, true}};
	return {{TEXT("A"), 750, 750, 120, 10, 4, 25000 * K, false}, {TEXT("C"), 7500, 5000, 730, 336, 8, 600000 * K, true}, {TEXT("D"), 30000, 20000, 365 * 6, 1008, 12, 3000000 * K, true}};
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
	// Crew (owner, hangar step 4, 2026-11-27): a wheel change 2, a flap 2-4, an engine change 4-6, an A-check 4-8 (a
	// narrowbody's; other checks and classes from their rule, CrewFor); the areas: where they work on the aircraft.
	static const TArray<FFault> MxFaultList = {
		{TEXT("engine"), TEXT("an engine fault"), 3.0, 36.0, 400000.0, 4, 6, TEXT("engine")},
		{TEXT("gear"), TEXT("a landing gear fault"), 2.5, 16.0, 60000.0, 2, 4, TEXT("maingear")},
		{TEXT("brakes"), TEXT("worn wheels and brakes"), 1.5, 3.0, 15000.0, 2, 2, TEXT("wheels")},
		{TEXT("flaps"), TEXT("a flaps or slats fault"), 2.25, 10.0, 40000.0, 2, 4, TEXT("flaps")},
		{TEXT("spoilers"), TEXT("a spoiler fault"), 1.75, 6.0, 20000.0, 2, 3, TEXT("spoilers")},
		{TEXT("cargodoor"), TEXT("a cargo bay door fault"), 2.0, 8.0, 15000.0, 2, 3, TEXT("cargodoor")},
		{TEXT("tailstrike"), TEXT("a tail strike"), 3.0, 96.0, 250000.0, 3, 6, TEXT("tail")},
		{TEXT("acheck"), TEXT("the routine A-check"), 2.0, 0.0, 0.0, 4, 8, TEXT("nosegear,avionics,wheels,engine")},
		{TEXT("ccheck"), TEXT("the C-check"), 3.0, 0.0, 0.0, 8, 16, TEXT("nosegear,avionics,maingear,wheels,engine,flaps,spoilers,cargodoor,tail")},
		{TEXT("dcheck"), TEXT("the D-check"), 3.0, 0.0, 0.0, 12, 24, TEXT("nosegear,avionics,maingear,wheels,engine,flaps,spoilers,cargodoor,tail")}};
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

double ADPMaint::CheckHours(const FString& FaultKey, double RealH, int32 Mode, double Hands)
{
	if (Mode == 0) return 0.5;
	const FFault* F = Fault(FaultKey);
	if (Mode == 1) return FMath::Max(0.5, (F ? F->FastH : 2.0) * FMath::Clamp(Hands, 0.25, 1.0));
	return FMath::Max(0.5, RealH);
}

void ADPMaint::CrewFor(const FString& FaultKey, int32 RuleMechanics, int32& Least, int32& Most)
{
	const FFault* F = Fault(FaultKey);
	const bool bRoutine = FaultKey == TEXT("acheck") || FaultKey == TEXT("ccheck") || FaultKey == TEXT("dcheck");
	Least = FMath::Max(1, F && !bRoutine ? F->CrewMin : RuleMechanics);
	const double Spread = F && F->CrewMin > 0 ? double(F->CrewMax) / double(F->CrewMin) : 1.0;
	Most = FMath::Max(Least, F && !bRoutine ? F->CrewMax : FMath::RoundToInt(Least * Spread));
}

int32 ADPMaint::HandsFor(const FString& FaultKey, int32 RuleMechanics, int32 Free, double& Factor)
{
	int32 Least = 1, Most = 1; CrewFor(FaultKey, RuleMechanics, Least, Most);
	Factor = 1.0;
	if (Free < Least) return 0;
	const int32 Hands = FMath::Min(Free, Most);
	Factor = double(Least) / double(Hands);   // (more hands up to the job's limit: the same work shared)
	return Hands;
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
	// a fault check (F, to-do 311): the fault found in flight put right - its crew from the fault table (CrewMin), in a hangar where there is one
	const FFault* Ft = Kind == TEXT("F") ? Fault(A.FaultDue) : nullptr;
	if (Kind == TEXT("F"))
	{
		if (!Ft) { Why = TEXT("no fault to put right"); return false; }
		const FADPTypeInfo* Ti = ADPWorldData::Type(A.TypeId);
		const double Kv = (Ti ? FMath::Max(2.0, Ti->ValueM) : 100.0) / 100.0;
		Rs.Add({TEXT("F"), 0.0, 0.0, 0.0, Ft->RealH, Ft->CrewMin, Ft->PartsUsd * Kv, true});
	}
	for (const FCheckRule& X : Rs) if (Kind == X.Kind) R = &X;
	if (!R) { Why = TEXT("no such check"); return false; }
	if (A.Status == TEXT("flying")) { Why = TEXT("in the air"); return false; }
	if (A.Status == TEXT("maintenance")) { Why = TEXT("already in a check"); return false; }
	const bool bMine = A.AirlineId == UADPSim::Me();
	double Labour = 1.0, Hours = R->DurationH;
	const FString Key = Ft ? FString(Ft->Key) : Kind == TEXT("A") ? FString(TEXT("acheck")) : Kind == TEXT("C") ? FString(TEXT("ccheck")) : FString(TEXT("dcheck"));
	double Hf = 1.0; int32 Hands = R->Mechanics;
	if (bMine)
	{
		// the check needs its hired mechanics (to-do 311): too few free here, it waits and says why; more free than its
		// least: up to the job's most of them (CrewFor), the time shorter with the hands at work (hangar step 4)
		const int32 Free = MxFreeMechanics(W, A);
		Hands = HandsFor(Key, R->Mechanics, Free, Hf);
		if (Hands <= 0)
		{
			Why = MxWaitWhy(R->Mechanics, Free);
			if (A.MaintWait != Why)
			{
				A.MaintWait = Why;
				ADP_LOG(TEXT("Maintenance: %s's %s check %s"), *A.Reg, R->Kind, *Why);
				MxNote(W, Now, 1, FString::Printf(TEXT("%s: check waits for mechanics"), *A.Reg), FString::Printf(TEXT("%s's %s-check at %s %s."), *A.Reg, R->Kind, *A.Location, *Why));
			}
			return false;
		}
		Hours *= ADPCrew::SlowTeam(W, TEXT("mech"), A.Location, R->Mechanics) * Hf;   // (beginners: to-do 301)
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
	A.CheckFault = Key;
	if (Ft) A.FaultDue.Empty();
	A.CheckLenH = CheckHours(A.CheckFault, Hours, Mode, Hf); A.CheckStart = Now;
	A.Status = TEXT("maintenance"); A.CheckKind = R->Kind; A.CheckUntil = Now + FTimespan::FromHours(A.CheckLenH);
	A.MaintWait.Empty(); A.CheckCrew = bMine ? Hands : 0;
	const FFault* Fn = Fault(A.CheckFault);
	ADP_LOG(TEXT("Maintenance: %s - %s (%s check) at %s, %.2f h (%s), %d mechanics (least %d)"), *A.Reg, Fn ? Fn->Name : TEXT("?"), R->Kind, *A.Location, A.CheckLenH, Mode == 0 ? TEXT("Quick") : Mode == 2 ? TEXT("Real") : TEXT("Fast"), A.CheckCrew, R->Mechanics);
	if (bMine)
	{
		const FString What = Ft ? FString::Printf(TEXT("%s: %s being put right"), *A.Reg, Ft->Name) : FString::Printf(TEXT("%s in its %s-check"), *A.Reg, R->Kind);
		MxNote(W, Now, Ft ? 1 : 0, What, FString::Printf(TEXT("%s at %s until %s."), *What, *A.Location, *ADPBoards::Local(A.CheckUntil).ToString(TEXT("%d.%m %H:%M"))));
	}
	return true;
}

bool ADPMaint::Quote(const FADPWorld& W, const FADPAircraft& A, const FString& Kind, double& Usd, double& Hours, FString& Why)
{
	// (StartCheck's own sums, nothing paid or changed: keep the two the same)
	Usd = 0.0; Hours = 0.0;
	TArray<FCheckRule> Rs = Rules(A.TypeId); const FFault* Ft = nullptr;
	if (Kind == TEXT("F"))
	{
		Ft = Fault(A.FaultDue); if (!Ft) { Why = TEXT("no fault to put right"); return false; }
		const FADPTypeInfo* Ti = ADPWorldData::Type(A.TypeId);
		Rs.Add({TEXT("F"), 0.0, 0.0, 0.0, Ft->RealH, Ft->CrewMin, Ft->PartsUsd * (Ti ? FMath::Max(2.0, Ti->ValueM) : 100.0) / 100.0, true});
	}
	const FCheckRule* R = Rs.FindByPredicate([&](const FCheckRule& X) { return Kind == X.Kind; });
	if (!R) { Why = TEXT("no such check"); return false; }
	if (A.Status == TEXT("flying")) { Why = TEXT("in the air"); return false; }
	if (A.Status == TEXT("maintenance")) { Why = TEXT("already in a check"); return false; }
	double Labour = 1.0, H = R->DurationH;
	const FString Key = Ft ? FString(Ft->Key) : Kind == TEXT("A") ? FString(TEXT("acheck")) : Kind == TEXT("C") ? FString(TEXT("ccheck")) : FString(TEXT("dcheck"));
	const int32 Free = MxFreeMechanics(W, A); double Hf = 1.0;
	if (HandsFor(Key, R->Mechanics, Free, Hf) <= 0) { Why = MxWaitWhy(R->Mechanics, Free); return false; }
	H *= ADPCrew::SlowTeam(W, TEXT("mech"), A.Location, R->Mechanics) * Hf;
	if (R->bHangar)
	{
		int32 Busy = 0; for (const FADPAircraft& X : W.Aircraft) if (X.Location == A.Location && X.Status == TEXT("maintenance") && X.CheckKind != TEXT("A")) ++Busy;
		if (Busy >= HangarBays(A.Location)) { Why = TEXT("no free hangar bay"); return false; }
	}
	const FADPAirline* Al = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == A.AirlineId; });
	const double Short = FMath::Max(0.0, R->PartsUsd - (Al ? Al->PartsStock : 0.0));
	Usd = (Short > 1.0 ? Short * W.PartsIndex * 1.3 : 0.0) + R->DurationH * R->Mechanics * 85.0 * (Labour - 1.0 + 0.25);
	Hours = CheckHours(Key, H, MaintTime(), Hf);
	if (!Al || Al->Balance < Usd) { Why = TEXT("not enough money"); return false; }
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
			A.CheckKind.Empty(); A.CheckFault.Empty(); A.CheckCrew = 0; ++Checks;
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
			A.CheckKind.Empty(); A.CheckFault.Empty(); A.CheckCrew = 0; ++Checks;
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
		if (Due.IsEmpty()) { A.MaintWait.Empty(); continue; }
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
