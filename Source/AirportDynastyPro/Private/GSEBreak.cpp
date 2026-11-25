// The ground vehicles break down (to-do 277 E2): wear by the hours and km really driven, now and then a failure (the rates:
// Data/vehicle-breakdowns.json). A service vehicle on its way with nobody but its crew aboard creeps on to where it is in
// nobody's way and stops there with its hazard lights on; everything else is found failed on its place, after its work. Its
// flight gets another vehicle. Mended by its driver where it stands, or written off: taken away when nobody sees it.
#include "GSE.h"
#include "ServiceVehicles.h"
#include "Tugs.h"
#include "Airfield.h"
#include "CrewPose.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSimData.h"
#include "Sim/ADPEvents.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "BabylonModel.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FBrkFault { FString What; bool bFix = true; float Weight = 1.f; };
	struct FBrkRules
	{
		double HNew = 1500., HOld = 120., HAt = 8000., KNew = 20000., KOld = 2500., KAt = 90000., OwnMin = 200., OwnMax = 7500., OwnKm = 9., GapAny = 6., GapSame = 72.;
		double MinM = 20., MaxM = 60., NoMech = 2., DayMin = 3., DayMax = 6., Limp = 220.;
		TArray<FBrkFault> Faults;
	};
	const FBrkRules& BrkRules()
	{
		static FBrkRules R; static bool bRead = false;
		if (bRead) return R;
		bRead = true;
		FString Text; TSharedPtr<FJsonObject> J;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/vehicle-breakdowns.json"))) && !FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/vehicle-breakdowns.json"))))
		{
			ADP_WARN(TEXT("Breakdowns: Data/vehicle-breakdowns.json not found - the built-in rates"));
		}
		else if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
		{
			ADP_WARN(TEXT("Breakdowns: Data/vehicle-breakdowns.json is not readable - the built-in rates"));
		}
		else
		{
			auto N = [&](const TCHAR* K, double& V) { J->TryGetNumberField(K, V); };
			N(TEXT("mtbfNewHours"), R.HNew); N(TEXT("mtbfOldHours"), R.HOld); N(TEXT("oldAtHours"), R.HAt); N(TEXT("mkbfNewKm"), R.KNew); N(TEXT("mkbfOldKm"), R.KOld); N(TEXT("oldAtKm"), R.KAt);
			N(TEXT("ownHoursMin"), R.OwnMin); N(TEXT("ownHoursMax"), R.OwnMax); N(TEXT("ownKmPerHour"), R.OwnKm); N(TEXT("gapAnyGameHours"), R.GapAny); N(TEXT("gapSameGameHours"), R.GapSame);
			N(TEXT("repairMinMinutes"), R.MinM); N(TEXT("repairMaxMinutes"), R.MaxM); N(TEXT("noMechanicFactor"), R.NoMech); N(TEXT("deliveryDaysMin"), R.DayMin); N(TEXT("deliveryDaysMax"), R.DayMax); N(TEXT("limpCmPerS"), R.Limp);
			const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
			if (J->TryGetArrayField(TEXT("faults"), L)) for (const TSharedPtr<FJsonValue>& V : *L)
			{
				const TSharedPtr<FJsonObject>* O = nullptr; if (!V->TryGetObject(O)) continue;
				FBrkFault F; double We = 1.; (*O)->TryGetStringField(TEXT("what"), F.What); (*O)->TryGetBoolField(TEXT("fix"), F.bFix); (*O)->TryGetNumberField(TEXT("weight"), We); F.Weight = float(We);
				if (!F.What.IsEmpty()) R.Faults.Add(F);
			}
		}
		if (!R.Faults.Num()) { FBrkFault F; F.What = TEXT("a fault in the drive"); R.Faults.Add(F); }
		for (double* V : {&R.HNew, &R.HOld, &R.HAt, &R.KNew, &R.KOld, &R.KAt}) *V = FMath::Max(*V, 1.);
		return R;
	}
	bool BrkMechanic(const FADPWorld& W)
	{
		for (const FADPPerson& P : W.People) if (P.AirlineId == UADPSim::Me() && P.Role == TEXT("mech") && P.Status == TEXT("on")) return true;
		return false;
	}
	// (any fault, or one of the sort asked for)
	void BrkPick(bool bWantFix, bool bAny, bool& bFix, FString& What)
	{
		float Sum = 0.f; for (const FBrkFault& F : BrkRules().Faults) if (bAny || F.bFix == bWantFix) Sum += F.Weight;
		float Left = FMath::FRand() * Sum; What = TEXT("a fault in the drive"); bFix = bAny || bWantFix;
		for (const FBrkFault& F : BrkRules().Faults)
			if (bAny || F.bFix == bWantFix) { What = F.What; bFix = F.bFix; if ((Left -= F.Weight) <= 0.f) return; }
	}
	FString BrkClock(const FDateTime& T) { return T.ToString(TEXT("%H:%M")); }

	FAutoConsoleCommandWithWorldAndArgs CmdBreakVehicle(TEXT("adp.BreakVehicle"), TEXT("adp.BreakVehicle <part of its name> [total]: that ground vehicle breaks down now (total: not to be mended)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W)
		{
			AGSE* G = W ? AGSE::Get(W) : nullptr; if (!G || !A.Num()) return;
			TArray<FString> N = A; const bool bTotal = N.Num() > 1 && N.Last() == TEXT("total"); if (bTotal) N.Pop();
			const FString Name = FString::Join(N, TEXT(" "));
			if (!G->BreakNow(Name, bTotal)) ADP_WARN(TEXT("Breakdowns: no running vehicle called %s"), *Name);
		}));
}

bool AGSE::BreakNow(const FString& Name, bool bTotal)
{
	for (FTractor& T : Tractors)
	{
		if (!T.M.IsValid() || T.TugIx != INDEX_NONE || IsTaxi(T) || T.bAway || T.Brk || !T.Name.Contains(Name)) continue;
		BrkPick(!bTotal, false, T.bBrkFix, T.BrkFault); T.Brk = 1; T.BrkT = 0.f;
		ADP_LOG(TEXT("Breakdowns: %s fails (asked for): %s, %s"), *T.Name, *T.BrkFault, T.bBrkFix ? TEXT("can be mended") : TEXT("cannot be mended"));
		return true;
	}
	return false;
}

float AGSE::BreakLimp() { return float(BrkRules().Limp); }

void AGSE::BreakLoad(FTractor& T)
{
	UADPSim* Sim = UADPSim::Get(this); if (!Sim) return;
	T.BrkId = T.bSpare ? T.Owner + TEXT(":") + T.SpareId : TEXT("own:") + T.Name;
	// (two of the same name: the second, the third one)
	if (!T.bSpare) { int32 n = 1; for (const FTractor& O : Tractors) { if (&O == &T) break; if (!O.bSpare && O.Name == T.Name) ++n; } if (n > 1) T.BrkId += FString::Printf(TEXT("#%d"), n); }
	const FBrkRules& R = BrkRules();
	const FADPVehWear* E = Sim->World.VehWear.FindByPredicate([&](const FADPVehWear& X) { return X.Id == T.BrkId; });
	const uint8 Was = T.Brk;
	if (T.Ch) ChargeLoad(T, E ? E->Soc : -1.0);
	if (E) { T.BrkHours = E->Hours; T.BrkKm = E->Km; T.BrkLast = E->LastFail; }
	else
	{
		// (an airport's own one has years behind it, always the same; a bought one is new)
		const double K = (GetTypeHash(T.BrkId) % 1000) / 999.;
		T.BrkHours = T.bSpare ? 0. : FMath::Lerp(R.OwnMin, R.OwnMax, K); T.BrkKm = T.BrkHours * R.OwnKm; T.BrkLast = FDateTime();
	}
	const int32 St = E ? E->State : 0;
	const bool bPlace = T.Job == EJob::None && !T.Path.Num() && T.Driver == INDEX_NONE && !T.Player.IsValid() && FVector::Dist2D(T.At, T.Home) < 300.;
	T.bBrkBack = false;
	if (St == 0)
	{
		// (broken in the career before: mended here; a wreck on the apron or one still gone: back on its place when nobody looks)
		if (Was == 4) T.BrkUntil = Sim->Now();
		else if (Was == 3 && !T.bSpare) { T.bBrkBack = true; T.BrkUntil = Sim->Now(); }
		else if (Was == 2) T.BrkUntil = FDateTime(1);
		else if (Was == 1 || Was == 3) T.Brk = 0;
		return;
	}
	T.BrkFault = E->Fault; T.BrkUntil = E->Until; T.bBrkFix = St == 2;
	if (Was == 4) { if (St == 2) T.BrkUntil = Sim->Now(); return; }
	if (!bPlace) { if (!Was) T.Brk = 1; return; }
	T.Brk = uint8(St == 4 ? 3 : St); T.bSwHaz = true; T.bBrkHaz = false; T.BrkJob = INDEX_NONE; T.BrkT = 0.f;
	ADP_LOG(TEXT("Breakdowns: %s is still broken down (%s)"), *T.Name, *T.BrkFault);
}

void AGSE::BreakStore(const FTractor& T)
{
	UADPSim* Sim = UADPSim::Get(this); if (!Sim || T.BrkId.IsEmpty()) return;
	TArray<FADPVehWear>& L = Sim->World.VehWear;
	FADPVehWear* E = L.FindByPredicate([&](const FADPVehWear& X) { return X.Id == T.BrkId; });
	if (!E) { E = &L.AddDefaulted_GetRef(); E->Id = T.BrkId; }
	E->Hours = T.BrkHours; E->Km = T.BrkKm; E->Soc = ChargeSoc(T); E->LastFail = T.BrkLast; E->Fault = T.BrkFault; E->Until = T.BrkUntil;
	E->State = T.bBrkBack || T.Brk == 1 || T.Brk == 5 ? 0 : T.Brk;
}

int32 AGSE::BreakJob(int32 Ti) const
{
	for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Ti == Ti && P.Value.Phase > 0 && P.Value.Phase < 9) return P.Key;
	return INDEX_NONE;
}

bool AGSE::BreakRoad(int32 Ti) const
{
	// the ones that fail on the way: a service vehicle with its crew only, going out to a stand or coming back
	const FTractor& T = Tractors[Ti];
	if (T.Svc == INDEX_NONE || T.Bu || IsTaxi(T)) return false;
	const FServiceSpec& S = ADPService::All()[T.Svc];
	if (S.bStairs || S.bAmbu || S.bDeicer || S.bBus) return false;
	const FSvcJob* J = SvcJobs.Find(BreakJob(Ti));
	return J && T.Path.Num() > 0 && !T.bReverse && !T.Player.IsValid() && T.Driver != INDEX_NONE && ((J->Phase == 1 && T.Job == EJob::Out) || (J->Phase == 3 && T.Job == EJob::Back));
}

bool AGSE::BreakSafe(const FTractor& T) const
{
	// the apron between two stands' ways in: not on a taxiway, a road or a stand's line, not in front of anybody's place
	const FVector2f P(float(T.At.X / 100.), float(-T.At.Y / 100.));
	if (P.Y < 84.f || P.Y > 178.f || T.bReverse || DepotAt(T.At) || FVector::Dist2D(T.At, T.Home) < 2500.) return false;
	for (const FAirfieldData::FStand& S : AAirfield::Shared().Stands)
	{
		const FVector2f L = S.Frame().Local(P);
		if (FMath::Abs(L.X) < S.Span * 0.5f + 3.f && L.Y > -12.f && L.Y < 120.f) return false;
	}
	for (const FTractor& O : Tractors)
		if (&O != &T && O.M.IsValid() && !O.bAway && (FVector::Dist2D(T.At, O.Home) < 1500. || FVector::Dist2D(T.At, O.At) < 900.)) return false;
	return true;
}

bool AGSE::BreakSpare(const FTractor& T, int32 Job) const
{
	const FSvcJob* J = SvcJobs.Find(Job); if (!J) return false;
	for (const FTractor& O : Tractors)
		if (&O != &T && O.Svc == J->Svc && !O.bSpare && !O.Brk && O.M.IsValid() && O.Job == EJob::None && !O.Player.IsValid() && O.Driver == INDEX_NONE) return true;
	return SparePick(J->Svc, J->Reg) != INDEX_NONE;
}

void AGSE::BreakHand(int32 Ti, int32 Job)
{
	// its flight's job starts anew with another vehicle and its own people; this one only comes home again
	FSvcJob* J = SvcJ(Job); if (!J || J->Reg.IsEmpty()) return;
	FSvcJob X = *J; X.Men.Reset(); X.Phase = 0; X.Ti = INDEX_NONE; X.T = 0.f; X.StillT = 0.f; X.Why.Reset(); X.WhyLog.Reset(); X.bQueued = false; ++X.Urgent;
	const FString Reg = J->Reg;
	J->Reg.Reset(); J->Flight.Reset(); J->StandX = 0.f; J->Why.Reset(); J->Phase = 3;
	FTractor& T = Tractors[Ti]; T.StandX = 0.f; T.Key.Reset(); T.bBrkHome = true;
	SvcJobs.Add(SvcNext++, MoveTemp(X));
	ADP_LOG(TEXT("Breakdowns: the job of %s at %s goes to another vehicle"), *T.Name, *Reg);
}

void AGSE::BreakStop(int32 Ti)
{
	FTractor& T = Tractors[Ti]; UADPSim* Sim = UADPSim::Get(this); if (!Sim) return;
	const FBrkRules& R = BrkRules(); const FDateTime Now = Sim->Now();
	const int32 Job = BreakJob(Ti); const bool bMech = BrkMechanic(Sim->World);
	T.Brk = T.bBrkFix ? 2 : 3; T.BrkT = 0.f; T.BrkLast = Now; T.BrkJob = Job; T.bBrkHome = false; T.bBrkBack = false;
	T.bBrkHaz = T.bSwHaz; T.bSwHaz = true;
	FString What;
	if (T.bBrkFix)
	{
		const double Min = FMath::RoundToDouble(FMath::FRandRange(R.MinM, R.MaxM) * (bMech ? 1. : R.NoMech));
		T.BrkUntil = Now + FTimespan::FromMinutes(Min);
		What = FString::Printf(TEXT("It can be mended: about %.0f minutes, running again at %s%s."), Min, *BrkClock(T.BrkUntil), bMech ? TEXT("") : TEXT(" (it takes longer without a mechanic on duty)"));
	}
	else
	{
		const double Days = FMath::RoundToDouble(FMath::FRandRange(R.DayMin, R.DayMax));
		T.BrkUntil = Now + FTimespan::FromDays(Days);
		What = T.bSpare ? FString(TEXT("It cannot be mended and is written off; it is gone from your vehicles.")) : FString::Printf(TEXT("It cannot be mended and is written off; the airport gets a new one in %.0f days."), Days);
	}
	ADP_LOG(TEXT("Breakdowns: %s stopped with %s at plan (%.0f, %.0f) after %.0f h, %.0f km. %s"), *T.Name, *T.BrkFault, T.At.X / 100., -T.At.Y / 100., T.BrkHours, T.BrkKm, *What);
	FString Who = T.Name; if (Who.Len()) Who[0] = FChar::ToUpper(Who[0]);
	ADPEvents::Notify(Sim->World, Now, TEXT("mechanic"), 2, FString::Printf(TEXT("%s has broken down"), *Who), FString::Printf(TEXT("%s: %s. %s"), *Who, *T.BrkFault, *What));
	BreakStore(T);
	const FSvcJob* J = SvcJ(Job);
	if (J && !T.Player.IsValid() && J->Phase == 1 && (!T.bBrkFix || BreakSpare(T, Job))) BreakHand(Ti, Job);
	if (Job != INDEX_NONE && !T.Player.IsValid()) BreakCrew(Ti, Job);
}

void AGSE::BreakCrew(int32 Ti, int32 Job)
{
	FTractor& T = Tractors[Ti]; FSvcJob* J = SvcJ(Job); ATugs* G = Tg();
	if (!J || !G || !J->Men.Num() || J->Men[0].bGone || !G->Crew.IsValidIndex(J->Men[0].Ci)) return;
	const int32 Ci = J->Men[0].Ci;
	// (its motor off once it has rolled to its stop)
	Wait(Ci, 4.f);
	Do(Ci, [this, Ti] { Motor(Tractors[Ti], false); });
	if (T.Brk == 3)
	{
		// its people get out and walk back; nothing of it goes on
		T.Path.Reset(); T.OnArrive = nullptr; T.Until = nullptr; T.AfterUntil = nullptr; T.Job = EJob::Home; J->Phase = 3;
		for (int32 m = 0; m < J->Men.Num(); ++m)
		{
			const int32 Cm = J->Men[m].Ci; if (J->Men[m].bGone) continue;
			if (m > 0) { Wait(Cm, 4.f); SvcAlight(Job, m); Do(Cm, [this, Job, m] { if (FSvcMan* Y = SvcM(Job, m)) Y->bGone = true; }); GiveBack(Cm); continue; }
			ClimbOut(Cm, Ti, [this, Job] { if (FSvcMan* Y = SvcM(Job, 0)) { Y->bSeated = false; Y->bOut = true; } });
			WaitFor(Cm, [this, Job](float E) { return E > 120.f || SvcAll(Job, [](const FSvcMan& Y) { return Y.Seat == 0; }); });
			Do(Cm, [this, Job, Ti] { FTractor& Y = Tractors[Ti]; Y.Job = EJob::None; Y.Key.Reset(); Y.StandX = 0.f; Y.Driver = INDEX_NONE; if (FSvcJob* Z = SvcJ(Job)) Z->Phase = 9; });
			GiveBack(Cm);
		}
		return;
	}
	// its driver gets out, round to the front, and works there on one knee until it runs
	ClimbOut(Ci, Ti, nullptr);
	WalkTo(Ci, [this, Ti] { const FTractor& X = Tractors[Ti]; return TW(X, FVector(X.KHalfL + 110.f, X.KStand.Y, 0.f)); }, 25.f);
	WalkTo(Ci, [this, Ti] { const FTractor& X = Tractors[Ti]; return TW(X, FVector(X.KHalfL + 75.f, 0.f, 0.f)); }, 15.f);
	TSharedRef<double> Begin = MakeShared<double>(-1.);
	Hold(Ci, [this, Ti, Begin](float, float PH, FDriverPose& P)
	{
		const FTractor& X = Tractors[Ti]; const double Now = GetWorld()->GetTimeSeconds(); if (*Begin < 0.) *Begin = Now;
		// (down for a while, a look from above, down again)
		const float S = float(FMath::Fmod(Now - *Begin, 26.)), E = FMath::Min(FMath::SmoothStep(0.f, 1.6f, S), 1.f - FMath::SmoothStep(21.f, 22.6f, S));
		const FVector At = TW(X, FVector(X.KHalfL + 75.f, 0.f, 0.f)), Hands = TW(X, FVector(X.KHalfL + 25.f, 6.f * FMath::Sin(float(Now) * 1.7f), 55.f + 4.f * FMath::Sin(float(Now) * 2.6f)));
		P = ADPPose::Squat(At, (Hands - At).GetSafeNormal2D(), Hands, PH, E);
	});
	WaitFor(Ci, [this, Ti](float) { return Tractors[Ti].Brk != 2; });
	Do(Ci, [this, Ci] { if (ATugs* G2 = Tg()) G2->Crew[Ci].Hold = nullptr; });
	Wait(Ci, 1.f);
	WalkTo(Ci, [this, Ti] { const FTractor& X = Tractors[Ti]; return TW(X, FVector(X.KHalfL + 110.f, X.KStand.Y, 0.f)); }, 25.f);
	ClimbIn(Ci, Ti, [this, Ti] { if (Tractors[Ti].Brk == 5) BreakMend(Ti); });
}

void AGSE::BreakMend(int32 Ti)
{
	FTractor& T = Tractors[Ti];
	T.Brk = 0; T.bSwHaz = T.bBrkHaz; T.BrkFault.Reset(); T.BrkT = 0.f;
	ADP_LOG(TEXT("Breakdowns: %s runs again"), *T.Name);
	BreakStore(T);
	const int32 Job = T.BrkJob; T.BrkJob = INDEX_NONE;
	FSvcJob* J = SvcJ(Job); if (!J || J->Ti != Ti) return;
	J->Why.Reset(); J->T = 0.f;
	Motor(T, T.Path.Num() > 0 || T.bBrkHome);
	if (!T.bBrkHome) return;
	T.bBrkHome = false; T.Path.Reset(); T.OnArrive = nullptr; T.Until = nullptr; T.AfterUntil = nullptr; T.Job = EJob::Back;
	SvcHome(Job);
}

void AGSE::BreakGone(FTractor& T, bool bGone)
{
	T.Brk = bGone ? 4 : 0; T.bAway = bGone; T.bPrev = false; T.Speed = 0.f; T.Path.Reset(); T.bSwHaz = false; T.bBrkBack = false;
	T.At = bGone ? T.Home + FVector(400000., -400000., 0.) : T.Home; T.Yaw = T.HomeYaw;
	if (bGone) Motor(T, false);
	if (ABabylonModel* M = T.M.Get()) { M->SetActorHiddenInGame(bGone); M->SetActorEnableCollision(!bGone); }
	Pose(T);
	if (!bGone) { T.BrkHours = 0.; T.BrkKm = 0.; T.BrkT = 0.f; T.BrkFault.Reset(); T.bUsed = false; }
	ADP_LOG(TEXT("Breakdowns: %s %s"), *T.Name, bGone ? TEXT("was taken away") : TEXT("is there again, on its place"));
}

void AGSE::BreakRound()
{
	// who waits behind a broken-down one goes round it
	for (const FTractor& B : Tractors)
	{
		if (B.Brk < 2 || B.Brk == 4 || B.bAway || FVector::Dist2D(B.At, B.Home) < 300.) continue;
		for (FTractor& O : Tractors)
		{
			if (&O == &B || O.Brk || O.Hold < 5.f || O.Path.Num() < 2 || O.bReverse || O.Bu || O.Player.IsValid() || FVector::Dist2D(O.At, B.At) > 1600.) continue;
			if (-O.At.Y < 8400. || -O.At.Y > 18800.) continue;
			int32 Last = INDEX_NONE; FVector From(O.At.X, O.At.Y, 0.); double Len = 0.;
			for (int32 i = 0; i < O.Path.Num() && Len < 4000.; ++i)
			{
				const FVector To(O.Path[i].X, O.Path[i].Y, 0.), S(B.At.X, B.At.Y, 0.); Len += FVector::Dist2D(From, To);
				if (FMath::PointDistToSegment(S, From, To) < 550.) Last = i;
				From = To;
			}
			if (Last == INDEX_NONE) continue;
			int32 k = Last; while (k < O.Path.Num() && FVector::Dist2D(O.Path[k], B.At) < 900.) ++k;
			if (k >= O.Path.Num()) continue;
			TArray<FVector> W; if (!Apron(W, O.At, O.Path[k], &O)) continue;
			for (int32 i = k + 1; i < O.Path.Num(); ++i) W.Add(O.Path[i]);
			O.Path = MoveTemp(W); O.Hold = 0.f;
			ADP_LOG(TEXT("Breakdowns: %s goes round the broken-down %s"), *O.Name, *B.Name);
			return;   // (one at a time)
		}
	}
}

void AGSE::BreakTick(float Dt)
{
	UADPSim* Sim = UADPSim::Get(this); if (!Sim || !Sim->World.Airlines.Num()) return;
	ADP_PERF_SCOPE("GSE: breakdowns", 2.f);
	const FBrkRules& R = BrkRules();
	for (FTractor& T : Tractors)
	{
		if (T.TugIx != INDEX_NONE || T.bAway || !T.M.IsValid()) continue;
		const double V = FMath::Abs(T.Speed);
		if (V > 1. || T.bOn) { T.BrkHours += Dt / 3600.; T.BrkKm += V * Dt / 1e5; }
		if (V > 1. || T.Brk > 1) T.BrkT += Dt;   // (driven since its last chance; broken: how long in this state)
	}
	if ((BrkSlow -= Dt) > 0.f) return;
	BrkSlow = 0.5f; BrkSave -= 0.5f;
	if (BrkLoads != Sim->Loads) { BrkLoads = Sim->Loads; for (FTractor& T : Tractors) T.BrkId.Reset(); }
	const FDateTime Now = Sim->Now();
	int32 Broken = 0; FDateTime LastAny;
	for (const FADPVehWear& E : Sim->World.VehWear) if (E.LastFail > LastAny) LastAny = E.LastFail;
	for (const FTractor& T : Tractors) if (T.Brk > 0 && T.Brk != 4) ++Broken;
	bool bRound = false;
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		FTractor& T = Tractors[i]; if (T.TugIx != INDEX_NONE || !T.M.IsValid() || IsTaxi(T)) continue;
		if (T.BrkId.IsEmpty() || (T.bSpare && !T.bAway && T.BrkId != T.Owner + TEXT(":") + T.SpareId))
		{
			// (a career just loaded, or a put-away one shown as another)
			if (!T.BrkId.IsEmpty()) { T.Brk = 0; T.bSwHaz = false; }
			T.BrkT = 0.f; BreakLoad(T); continue;
		}
		const bool bPlace = T.Job == EJob::None && !T.Path.Num() && !T.Until && T.Driver == INDEX_NONE && !T.Player.IsValid() && !T.YouOn && !T.Train.Num() && T.TowGpu == INDEX_NONE && FVector::Dist2D(T.At, T.Home) < 300.;
		switch (T.Brk)
		{
		case 0:
			{
				if (T.bAway || T.BrkT < 10.f) break;
				// (a chance for what it drove since: rare when new, more often when worn; never two close after each other; yours by the console only)
				const double H = T.BrkT / 3600., Km = H * 15.; T.BrkT = 0.f;
				if (Broken || T.Player.IsValid() || (Now - LastAny).GetTotalHours() < R.GapAny || (Now - T.BrkLast).GetTotalHours() < R.GapSame) break;
				const double Mh = FMath::Lerp(R.HNew, R.HOld, FMath::Clamp(T.BrkHours / R.HAt, 0., 1.)), Mk = FMath::Lerp(R.KNew, R.KOld, FMath::Clamp(T.BrkKm / R.KAt, 0., 1.));
				if (FMath::FRand() >= H / Mh + Km / Mk) break;
				BrkPick(true, true, T.bBrkFix, T.BrkFault); T.Brk = 1; ++Broken;
				ADP_LOG(TEXT("Breakdowns: %s fails (%s) after %.0f h, %.0f km"), *T.Name, *T.BrkFault, T.BrkHours, T.BrkKm);
				break;
			}
		case 1:
			if (bPlace || (T.Player.IsValid() && FMath::Abs(T.Speed) < 1.f && !T.Path.Num()) || (BreakRoad(i) && BreakSafe(T))) BreakStop(i);
			break;
		case 2:
			{
				bRound = true;
				const FSvcJob* J = SvcJobs.Find(T.BrkJob); const bool bCrew = J && J->Ti == i && J->Phase < 9 && !T.Player.IsValid();
				if (Now >= T.BrkUntil)
				{
					if (bCrew) { T.Brk = 5; T.BrkT = 0.f; } else BreakMend(i);
					break;
				}
				// (its flight's job waits for it only as long as there is no other)
				if (bCrew && J->Phase == 1 && !T.bBrkHome)
				{
					if (BreakSpare(T, T.BrkJob)) { BreakHand(i, T.BrkJob); break; }
					SvcJ(T.BrkJob)->Why = FString::Printf(TEXT("the %s has broken down (%s), mended about %s"), ADPService::All()[T.Svc].Label, *T.BrkFault, *BrkClock(T.BrkUntil));
				}
				break;
			}
		case 3:
			{
				bRound = true;
				if (T.Driver != INDEX_NONE || T.Player.IsValid() || T.YouOn || T.Job != EJob::None || !TaxiUnseen(T.At)) break;
				if (!T.bSpare || T.bBrkBack) { const bool bBack = T.bBrkBack; BreakGone(T, true); if (bBack) T.BrkUntil = Now; BreakStore(T); break; }
				// (the bought one: off the list - SpareTick puts it away)
				for (FADPAirline& A : Sim->World.Airlines)
					for (int32 s = A.Spares.Num() - 1; s >= 0 && A.Id == T.Owner; --s)
					{
						if (A.Spares[s].Kind + TEXT("@") + A.Spares[s].Place != T.SpareId) continue;
						if (A.Spares[s].Kind == TEXT("fuel")) A.FuelTrucks = FMath::Max(0, A.FuelTrucks - 1);
						if (A.Spares[s].Kind == TEXT("catering")) A.CateringTrucks = FMath::Max(0, A.CateringTrucks - 1);
						A.Spares.RemoveAt(s); ADP_LOG(TEXT("Breakdowns: %s is gone from the vehicles of %s"), *T.Name, *T.Owner);
					}
				// (its wear goes with it: a new one on the same place starts new; only when it failed is kept)
				Sim->World.VehWear.RemoveAll([](const FADPVehWear& E) { return E.Id == TEXT("gone"); });
				for (FADPVehWear& E : Sim->World.VehWear) if (E.Id == T.BrkId) { const FDateTime Failed = E.LastFail; E = FADPVehWear(); E.Id = TEXT("gone"); E.LastFail = Failed; }
				T.Brk = 6;
				break;
			}
		case 4:
			if (Now >= T.BrkUntil && TaxiUnseen(T.Home) && !Tractors.ContainsByPredicate([&](const FTractor& O) { return &O != &T && O.M.IsValid() && !O.bAway && FVector::Dist2D(O.At, T.Home) < O.KHalfL + T.KHalfL; }))
			{
				BreakGone(T, false); BreakStore(T);
			}
			break;
		case 5:
			{
				bRound = true;
				const ATugs* G = Tg(); const FSvcJob* J = SvcJobs.Find(T.BrkJob); const int32 Ci = J && J->Men.Num() ? J->Men[0].Ci : INDEX_NONE;
				// (its driver went off duty meanwhile: whoever relieves them takes it on)
				if (T.BrkT > 60.f && (!G || !G->Crew.IsValidIndex(Ci) || !G->Crew[Ci].Q.Num())) { ADP_WARN(TEXT("Breakdowns: %s is mended, its driver is not at it - left to their relief"), *T.Name); BreakMend(i); }
				break;
			}
		case 6:
			// (put away: free to be another one)
			if (T.bAway) { T.BrkId = TEXT("-"); T.Brk = 0; T.bSwHaz = false; }
			break;
		default: break;
		}
	}
	if (bRound) BreakRound();
	if (BrkSave > 0.f) return;
	BrkSave = 10.f;
	for (const FTractor& T : Tractors) if (T.TugIx == INDEX_NONE && T.M.IsValid() && !IsTaxi(T) && T.Brk != 6 && T.BrkId != TEXT("-") && (!T.bAway || T.Brk == 4)) BreakStore(T);
}
