// Crew rostering on the EASA flight time limitations - see Sim/ADPRoster.h.
#include "Sim/ADPRoster.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPStaff.h"
#include "Sim/ADPBoards.h"
#include "Site.h"

namespace
{
	const double PostFlightMin = 30.0;   // (the FDP ends at on-blocks; duty goes on ~30 min after the last leg)
	const double PreFlightMin = 60.0;    // (report time: one hour before the first departure)
	bool IsPilot(const FString& R) { return R == TEXT("captain") || R == TEXT("fo"); }
	FADPPerson* Find(FADPWorld& W, const FString& Id) { return W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == Id; }); }
	/** Rolls the flight-time records forward to today (each new day a new slot; each new week a new slot). */
	void Roll(FADPPerson& P, int32 Today)
	{
		if (P.DayKey == 0) { P.DayKey = Today; P.FlyDays.Init(0.0, 28); P.FlyWeeks.Init(0.0, 52); return; }
		if (P.FlyDays.Num() != 28) P.FlyDays.Init(0.0, 28);
		if (P.FlyWeeks.Num() != 52) P.FlyWeeks.Init(0.0, 52);
		const int32 Days = Today - P.DayKey;
		if (Days <= 0) return;
		for (int32 d = 0; d < FMath::Min(Days, 28); ++d) { P.FlyDays.RemoveAt(0); P.FlyDays.Add(0.0); }
		const int32 Weeks = Today / 7 - P.DayKey / 7;
		for (int32 w = 0; w < FMath::Min(Weeks, 52); ++w) { P.FlyWeeks.RemoveAt(0); P.FlyWeeks.Add(0.0); }
		P.DayKey = Today;
	}
}

int32 ADPRoster::DayOf(const FDateTime& T) { return int32((T - FDateTime(2000, 1, 1)).GetTotalDays()); }

double ADPRoster::MaxFdpMin(int32 H, int32 M, int32 Sectors)
{
	// ORO.FTL.205 table 2 (acclimatised crew), 1-2 sectors
	const int32 T = H * 60 + M;
	double Base;
	if (T >= 360 && T < 810) Base = 13 * 60;            // 06:00-13:29
	else if (T >= 810 && T < 1020) Base = 13 * 60 - 15 * (1 + (T - 810) / 30);   // 13:30-16:59: 12:45 .. 11:15
	else if (T >= 300 && T < 360) Base = 12 * 60 + 15 * ((T - 300) / 15);         // 05:00-05:59: 12:00 .. 12:45
	else Base = 11 * 60;                                // 17:00-04:59
	// minus 30 minutes for every sector from the third, never under 9 h
	return FMath::Max(9.0 * 60.0, Base - 30.0 * FMath::Max(0, Sectors - 2));
}

FDateTime ADPRoster::LocalAt(const FString& Icao, const FDateTime& Utc)
{
	if (Icao == ADPBoards::HereIcao()) return ADPBoards::Local(Utc);
	const FADPAirportInfo* A = ADPWorldData::Airport(Icao);
	return Utc + FTimespan::FromHours(A ? FMath::RoundToDouble(A->Lon / 15.0) : 0.0);
}

double ADPRoster::Fly28(const FADPPerson& P) { double S = 0; for (double D : P.FlyDays) S += D; return S; }
double ADPRoster::FlyYear(const FADPPerson& P) { double S = 0; for (double D : P.FlyWeeks) S += D; return S; }

bool ADPRoster::CanFly(const FADPWorld& W, const FADPPerson& P, const FString& TypeId, const FString& Where, double LegMin, const FDateTime& Now, FString& Why)
{
	if (P.Status != TEXT("on")) { Why = P.Status; return false; }
	if (P.OffUntil > Now) { Why = TEXT("days off"); return false; }
	if (P.RestUntil > Now) { Why = TEXT("resting"); return false; }
	const FString Loc = P.Location.IsEmpty() ? P.Base : P.Location;
	if (Loc != Where) { Why = TEXT("not at this airport"); return false; }
	if (IsPilot(P.Role) && !P.Types.Contains(TypeId)) { Why = TEXT("no type rating"); return false; }
	// the duty this leg would be part of
	const bool bOnDuty = P.DutyStart != FDateTime();
	const FDateTime Start = bOnDuty ? P.DutyStart : Now - FTimespan::FromMinutes(PreFlightMin);
	const FDateTime L = LocalAt(Loc, Start);
	const double Fdp = MaxFdpMin(L.GetHour(), L.GetMinute(), (bOnDuty ? P.Sectors : 0) + 1);
	const double Used = (Now - Start).GetTotalMinutes() + LegMin;
	if (Used > Fdp) { Why = TEXT("the flight duty limit"); return false; }
	// flight time limits: 100 h in 28 days, 900 h a year
	// (the records as of today: days and weeks older than the window drop out)
	const int32 Today = DayOf(Now), DShift = FMath::Max(0, Today - P.DayKey), WShift = FMath::Max(0, Today / 7 - P.DayKey / 7);
	double F28 = 0, FYr = 0;
	for (int32 i = DShift; i < P.FlyDays.Num(); ++i) F28 += P.FlyDays[i];
	for (int32 i = WShift; i < P.FlyWeeks.Num(); ++i) FYr += P.FlyWeeks[i];
	if (F28 + LegMin > 100.0 * 60.0) { Why = TEXT("100 hours in 28 days"); return false; }
	if (FYr + LegMin > 900.0 * 60.0) { Why = TEXT("900 hours in a year"); return false; }
	// days off due: six days with duty in a row means two days off first (a new duty on a new day only)
	if (!bOnDuty && P.DaysOn >= (P.Contract == TEXT("part") ? 3 : 6) && P.LastDutyDay != DayOf(Now) && Loc == P.Base) { Why = TEXT("days off due"); return false; }
	return true;
}

void ADPRoster::Release(FADPWorld& W, FADPAircraft& A, const FDateTime& At)
{
	for (const FString& Id : A.Crew)
	{
		FADPPerson* P = Find(W, Id);
		if (!P) continue;
		const FString Loc = P->Location.IsEmpty() ? P->Base : P->Location;
		// the duty ended 30 minutes after the last on-blocks (a crew that never flew: when it was released)
		const FDateTime End = P->DutyStart != FDateTime() && P->Sectors > 0 ? P->DutyStart + FTimespan::FromMinutes(P->DutyMin + PostFlightMin) : At;
		const double DutyH = P->DutyStart != FDateTime() ? (End - P->DutyStart).GetTotalHours() : 0.0;
		// rest: at least as long as the duty, at home base at least 12 h, away at least 10 h
		const double RestHrs = FMath::Max(DutyH, Loc == P->Base ? 12.0 : 10.0);
		P->RestUntil = FMath::Max(At, End + FTimespan::FromHours(RestHrs));
		// days off when due (two after six days with duty), at home base
		if (P->DaysOn >= (P->Contract == TEXT("part") ? 3 : 6) && Loc == P->Base) { P->OffUntil = At + FTimespan::FromHours(FMath::Max(RestHrs, 60.0)); P->DaysOn = 0; }
		P->DutyStart = FDateTime(); P->Sectors = 0; P->WithReg.Empty(); P->DutyMin = 0.0;
	}
	A.Crew.Reset();
}

bool ADPRoster::Crew(FADPWorld& W, FADPAircraft& A, double LegMin, int32 CabinNeeded, const FString& PreferCaptain, const FDateTime& Now, FString& Why)
{
	// the crew on board goes on if every one of them may fly this leg too
	if (A.Crew.Num())
	{
		bool bAll = true;
		for (const FString& Id : A.Crew) { FADPPerson* P = Find(W, Id); FString Y; if (!P || !CanFly(W, *P, A.TypeId, A.Location, LegMin, Now, Y)) { bAll = false; break; } }
		int32 Fa = 0, Cap = 0, Fo = 0;
		for (const FString& Id : A.Crew) if (FADPPerson* P = Find(W, Id)) { Fa += P->Role == TEXT("fa"); Cap += P->Role == TEXT("captain"); Fo += P->Role == TEXT("fo"); }
		if (bAll && Cap >= 1 && Fo >= 1 && Fa >= CabinNeeded) return true;
		Release(W, A, Now);   // (this crew's duty ends here; a fresh one is rostered)
	}
	// a fresh crew from the people at this airport, the most rested first (the fewest hours in 28 days)
	TArray<FADPPerson*> Cap, Fo, Fa;
	FString LastWhy;
	for (FADPPerson& P : W.People)
	{
		if (P.AirlineId != A.AirlineId || !P.WithReg.IsEmpty()) continue;
		if (P.Role != TEXT("captain") && P.Role != TEXT("fo") && P.Role != TEXT("fa")) continue;
		FString Y;
		if (!CanFly(W, P, A.TypeId, A.Location, LegMin, Now, Y)) { if (Y != TEXT("not at this airport")) LastWhy = Y; continue; }
		(P.Role == TEXT("captain") ? Cap : P.Role == TEXT("fo") ? Fo : Fa).Add(&P);
	}
	auto Rested = [](const FADPPerson& X, const FADPPerson& Y) { return ADPRoster::Fly28(X) < ADPRoster::Fly28(Y); };
	Cap.Sort(Rested); Fo.Sort(Rested); Fa.Sort(Rested);
	// a captain may fly as first officer when no first officer is free (never the other way round)
	if (Cap.Num() && !PreferCaptain.IsEmpty()) { const int32 I = Cap.IndexOfByPredicate([&](const FADPPerson* X) { return X->Id == PreferCaptain; }); if (I > 0) Cap.Swap(0, I); }
	if (!Fo.Num() && Cap.Num() >= 2) Fo.Add(Cap[1]);
	if (!Cap.Num()) { Why = FString::Printf(TEXT("no captain free (%s)"), LastWhy.IsEmpty() ? TEXT("none at this airport") : *LastWhy); return false; }
	if (!Fo.Num() || Fo[0] == Cap[0]) { Why = FString::Printf(TEXT("no first officer free (%s)"), LastWhy.IsEmpty() ? TEXT("none at this airport") : *LastWhy); return false; }
	if (Fa.Num() < CabinNeeded) { Why = FString::Printf(TEXT("%d of %d cabin crew free"), Fa.Num(), CabinNeeded); return false; }
	TArray<FADPPerson*> Picked = {Cap[0], Fo[0]};
	for (int32 i = 0; i < CabinNeeded; ++i) Picked.Add(Fa[i]);
	const int32 Today = DayOf(Now);
	for (FADPPerson* P : Picked)
	{
		P->WithReg = A.Reg; P->DutyStart = Now - FTimespan::FromMinutes(PreFlightMin); P->Sectors = 0;
		if (P->LastDutyDay != Today) { ++P->DaysOn; P->LastDutyDay = Today; }
		if (P->Location.IsEmpty()) P->Location = P->Base;
		A.Crew.Add(P->Id);
	}
	return true;
}

void ADPRoster::Landed(FADPWorld& W, const FADPFlight& F, const FDateTime& At)
{
	const int32 Today = DayOf(At);
	for (const FString& Id : F.Crew)
		if (FADPPerson* P = Find(W, Id))
		{
			Roll(*P, Today);
			P->FlyDays.Last() += F.DurMin + F.HoldMin; P->FlyWeeks.Last() += F.DurMin + F.HoldMin;
			++P->Sectors; P->Location = F.To; P->Xp += F.DurMin / 60.0 + 20.0; P->HoursTotal += F.DurMin / 60.0;
			P->DutyMin = (At - P->DutyStart).GetTotalMinutes();
		}
}

bool ADPRoster::GiveDaysOff(FADPWorld& W, const FString& PersonId, int32 Days, const FDateTime& At)
{
	FADPPerson* P = Find(W, PersonId);
	if (!P || P->AirlineId.IsEmpty()) return false;
	const FDateTime From = P->WithReg.IsEmpty() ? FMath::Max(At, P->RestUntil) : At + FTimespan::FromHours(13.0);
	P->OffUntil = From + FTimespan::FromDays(FMath::Clamp(Days, 1, 14));
	P->DaysOn = 0;
	return true;
}
