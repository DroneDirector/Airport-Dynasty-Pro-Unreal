#include "Turnaround.h"
#include "ADPLog.h"
#include "Tugs.h"
#include "GSE.h"
#include "Airfield.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSchedule.h"
#include "PaxFlow.h"
#include "SimTraffic.h"
#include "StaffPeople.h"
#include "StandAsk.h"
#include "Sim/ADPGround.h"
#include "Engine/World.h"

namespace ADPTurnaround
{
	static TArray<FTurn> Turns;
	static FDateTime Last = FDateTime(0);
	const TArray<FTurn>& All() { return Turns; }
	FDateTime LastNow() { return Last; }
	const FTurn* ForFlight(const FString& Flight) { return Flight.IsEmpty() ? nullptr : Turns.FindByPredicate([&](const FTurn& X) { return X.Flight == Flight; }); }
	const FTurn* AtStand(float StandX) { return StandX == 0.f ? nullptr : Turns.FindByPredicate([&](const FTurn& X) { return FMath::Abs(X.StandX - StandX) < 1.f; }); }
	bool BagCutOff(const FString& Flight, float StandX)
	{
		// (check-in closes 45 minutes before off-blocks; the last bags are on the make-up some minutes later - they go out at 32)
		const FTurn* T = ForFlight(Flight); if (!T) T = AtStand(StandX);
		return T && Last.GetTicks() > 0 && Last >= T->OffBlocks - FTimespan::FromMinutes(32.0);
	}

	TArray<FTask> PlanA320(float G, int32 Pax, float FuelL, float& LateMin)
	{
		// (Airbus A320 AC 5-2-0: bridge on + door 2 min; other equipment from t0 + 1; deplaning 20 pax/min at L1; boarding 12
		//  pax/min + 2 min last seating and headcount; door closed + bridge off 1.5 min; containers 1.5 min each way, +2 / +1.5;
		//  bulk 150 kg/min out, 120 in; refuel one hose, +2.5 min, ~1000 l/min; 1 catering truck R1 4 trolleys then R2 7,
		//  1.2 min each, +2 to drive; potable water 200 l; toilets drained and rinsed; cleaning in the time left)
		TArray<FTask> T;
		auto Add = [&T](const TCHAR* Id, const TCHAR* Name, const TCHAR* Veh, float S, float D, std::initializer_list<const TCHAR*> After = {})
		{ FTask K; K.Id = Id; K.Name = Name; K.Vehicle = Veh; K.Start = S; K.Dur = D; for (const TCHAR* A : After) K.After.Add(A); T.Add(K); };
		const float Deplane = Pax / 20.f, Board = Pax / 12.f + 2.f;
		Add(TEXT("bridge"),   TEXT("jetway on, door L1 open"),         TEXT("jetway"), 0.f, 2.f);
		Add(TEXT("gpu"),      TEXT("ground power connected"),           TEXT("gpu"), 1.f, 1.5f);
		Add(TEXT("chocks"),   TEXT("chocks and cones"),                 TEXT(""), 0.5f, 1.f);
		Add(TEXT("deplane"),  TEXT("deplaning"),                        TEXT(""), 2.f, Deplane, {TEXT("bridge")});
		Add(TEXT("fwd"),      TEXT("forward hold: 3 containers out and in"), TEXT("cargoloader"), 1.f, 2.f + 3 * 1.5f * 2.f + 1.5f);
		Add(TEXT("aft"),      TEXT("aft hold: 4 containers out and in"),     TEXT("cargoloader"), 1.f, 2.f + 4 * 1.5f * 2.f + 1.5f);
		Add(TEXT("bulk"),     TEXT("bulk hold: 500 kg out and in"),          TEXT("beltloader"), 1.f, 2.f + 500.f / 150.f + 500.f / 120.f + 1.5f);
		Add(TEXT("water"),    TEXT("potable water 200 l"),              TEXT("watertruck"), 1.f, 2.f + 4.f + 1.5f);
		Add(TEXT("toilet"),   TEXT("toilets drained and rinsed"),       TEXT("toilettruck"), 1.f, 2.f + 6.f + 1.5f);
		const float CatR1 = 2.f + 4 * 1.2f + 1.5f, CatR2 = 2.f + 7 * 1.2f + 1.5f;
		Add(TEXT("cat1"),     TEXT("catering at R1 (4 trolleys)"),      TEXT("catering"), 2.f + Deplane, CatR1, {TEXT("deplane")});
		Add(TEXT("cat2"),     TEXT("catering at R2 (7 trolleys)"),      TEXT("catering"), 2.f + Deplane + CatR1 + 2.f, CatR2, {TEXT("cat1")});
		Add(TEXT("clean"),    TEXT("cabin cleaning"),                    TEXT(""), 2.f + Deplane, 0.f, {TEXT("deplane")});
		Add(TEXT("fuel"),     TEXT("refuelling"),                        TEXT("fueltruck"), 2.f + Deplane, 2.5f + FuelL / 1000.f, {TEXT("deplane")});
		// backward from off-blocks: the door closed and the bridge off 1.5 min before, boarding before that; the APU 10 min
		// before, the ground power off after it; the tug crew 8 min before
		// (with time for it, boarding is called half an hour before, as airlines do: the walk to the gate and through the jetway)
		const float BoardEnd = G - 1.5f, BoardStart = G >= 60.f ? FMath::Min(BoardEnd - Board, G - 30.f) : BoardEnd - Board;
		Add(TEXT("board"),    TEXT("boarding"),                          TEXT(""), BoardStart, Board, {TEXT("clean"), TEXT("cat2")});
		Add(TEXT("doors"),    TEXT("doors closed, jetway off"),          TEXT("jetway"), BoardEnd, 1.5f, {TEXT("board")});
		Add(TEXT("apu"),      TEXT("APU started"),                       TEXT(""), G - 10.f, 1.f);
		Add(TEXT("gpuoff"),   TEXT("ground power off and stowed"),       TEXT("gpu"), G - 8.f, 4.f, {TEXT("apu")});
		Add(TEXT("tug"),      TEXT("tug and pushback crew ready"),       TEXT("tug"), G - 8.f, 5.f);
		Add(TEXT("push"),     TEXT("pushback"),                          TEXT("tug"), G, 3.f, {TEXT("doors"), TEXT("gpuoff")});
		// cleaning fills the time between deplaning and boarding
		for (FTask& K : T) if (K.Id == TEXT("clean")) K.Dur = FMath::Clamp(BoardStart - K.Start, 5.f, 25.f);
		// too short a ground time: what doesn't fit before boarding makes it late
		float NeedBoard = 0.f; for (const FTask& K : T) if (K.Id == TEXT("cat2") || K.Id == TEXT("deplane")) NeedBoard = FMath::Max(NeedBoard, K.Start + K.Dur + 5.f);
		LateMin = FMath::Max(0.f, NeedBoard - BoardStart);
		if (LateMin > 0.f) for (FTask& K : T) if (K.Start >= BoardStart - 0.01f && K.Id != TEXT("apu")) K.Start += LateMin;
		return T;
	}

	bool Done(const FTurn& T, const TCHAR* Task)
	{
		const FTask* K = T.Tasks.FindByPredicate([&](const FTask& X) { return X.Id == Task; });
		return !K || K->bDone;
	}

	void Tick(UWorld* W, const FDateTime& Now)
	{
		UADPSim* Sim = UADPSim::Get(W); if (!Sim || !Sim->IsRunning()) return;
		Last = Now;
		const FString Here = ADPBoards::HereIcao();
		// the aircraft standing at a gate here, each with its next planned leg (Sim/ADPSchedule.h)
		TSet<FString> Seen;
		for (const FADPAircraft& A : Sim->World.Aircraft)
		{
			if (A.Location != Here || A.Stand.IsEmpty() || A.Status == TEXT("flying")) continue;
			Seen.Add(A.Reg);
			if (ASimTraffic::Moving(W, A.Reg)) continue;   // (still taxiing in: on blocks when it stands)
			FTurn* Tr = Turns.FindByPredicate([&](const FTurn& X) { return X.Reg == A.Reg; });
			const FADPPlanned* Dep = ADPSchedule::Next(Sim->World, A.Reg, Here);
			if (!Tr)
			{
				FTurn N; N.Reg = A.Reg; N.Stand = A.Stand; N.bMine = A.AirlineId == UADPSim::Me();
				for (const FAirfieldData::FStand& S : AAirfield::Shared().Stands) if (S.Id == A.Stand) N.StandX = S.X;
				// (just in: its passengers get off and its bags come out; standing here already - new, or a night stop: only its
				//  departure's work, from 75 min before)
				N.bArrival = A.ArrAt > FDateTime(2000, 1, 1) && (Now - A.ArrAt).GetTotalMinutes() < 40.0 && (Now - A.ArrAt).GetTotalMinutes() >= 0.0;
				N.OffBlocks = Dep ? Dep->Est : Now + FTimespan::FromHours(48.0);
				N.OnBlocks = N.bArrival ? Now : FMath::Max(Now, N.OffBlocks - FTimespan::FromMinutes(75.0));
				N.ArrFlight = A.ArrFlight; N.ArrPax = A.ArrPax;
				N.Flight = Dep ? Dep->FlightNo : FString(); N.PlanId = Dep ? Dep->Id : FString();
				N.Pax = Dep ? FMath::Max(1, Dep->Pax()) : 150;
				const float G = float((N.OffBlocks - N.OnBlocks).GetTotalMinutes());
				if (Dep) if (const double Kg = ADPEconomy::LegFuelKg(A.TypeId, Dep->BlockMin); Kg > 0.0) N.FuelL = FMath::Clamp(float(Kg / 0.8), 500.f, 19500.f);   // (its leg's own fuel, 0.8 kg a litre)
				N.Tasks = PlanA320(G, N.Pax, N.FuelL, N.LateMin);
				if (!N.bArrival) for (FTask& K : N.Tasks) if (K.Id == TEXT("deplane") || K.Id == TEXT("fwd") || K.Id == TEXT("aft") || K.Id == TEXT("bulk") || K.Id == TEXT("toilet")) K.bStarted = K.bDone = true;
				ADP_LOG(TEXT("Turnaround: %s at %s%s - %s"), *N.Reg, *N.Stand, N.bArrival ? *FString::Printf(TEXT(", in as %s with %d passengers"), *N.ArrFlight, N.ArrPax) : TEXT(", standing here"),
					Dep ? *FString::Printf(TEXT("%s leaves in %.0f min with %d passengers%s"), *N.Flight, (Dep->Est - Now).GetTotalMinutes(), N.Pax, N.LateMin > 0.f ? *FString::Printf(TEXT(", %.0f min late (tight turn)"), N.LateMin) : TEXT("")) : TEXT("no departure planned"));
				Tr = &Turns.Add_GetRef(MoveTemp(N));
			}
			else if (Dep && (Dep->Id != Tr->PlanId || FMath::Abs((Dep->Est - Tr->OffBlocks).GetTotalMinutes()) > 0.5))
			{
				// (the departure moved - a delay, or another flight: the tasks still to come shift with it, the gaps between them kept)
				const float Shift = float((Dep->Est - Tr->OffBlocks).GetTotalMinutes()), Tn = float((Now - Tr->OnBlocks).GetTotalMinutes());
				for (FTask& K : Tr->Tasks) if (!K.bStarted && (K.Id == TEXT("board") || K.Id == TEXT("doors") || K.Id == TEXT("apu") || K.Id == TEXT("gpuoff") || K.Id == TEXT("tug") || K.Id == TEXT("push") || (!Tr->bArrival && Tn < 0.f))) K.Start += Shift;
				const bool bNew = Dep->Id != Tr->PlanId;
				if (bNew) if (const double Kg = ADPEconomy::LegFuelKg(A.TypeId, Dep->BlockMin); Kg > 0.0)
				{
					Tr->FuelL = FMath::Clamp(float(Kg / 0.8), 500.f, 19500.f);
					for (FTask& K : Tr->Tasks) if (K.Id == TEXT("fuel") && !K.bStarted) K.Dur = 2.5f + Tr->FuelL / 1000.f;
				}
				Tr->OffBlocks = Dep->Est; Tr->Flight = Dep->FlightNo; Tr->PlanId = Dep->Id; Tr->Pax = FMath::Max(1, Dep->Pax());
				if (bNew || FMath::Abs(Shift) >= 1.f) ADP_LOG(TEXT("Turnaround: %s - %s off blocks in %.0f min%s"), *Tr->Reg, *Tr->Flight, (Dep->Est - Now).GetTotalMinutes(), bNew ? TEXT("") : *FString::Printf(TEXT(" (moved %+.1f min)"), Shift));
			}
			else if (!Dep && !Tr->PlanId.IsEmpty()) { Tr->PlanId.Empty(); Tr->Flight.Empty(); ADP_LOG(TEXT("Turnaround: %s - no departure planned any more"), *Tr->Reg); }
			// (how many of your ground crew are on shift: without them the crew's steps wait - the other airlines bring a handling agent)
			// (to-do 206: your own flights' ground power, bags and pushback are done by the people you have hired - the job asks for
			//  them itself and waits in their queue; you employ too few for any job - a new game - and the handling agent's stand-ins
			//  do them, as the other airlines'; what has no crew of theirs yet, the jetway and the doors, still goes by the count)
			const UStaffPeople* Sp = UStaffPeople::Get(W);
			const bool bHired = Tr->bMine && Sp && Sp->Hires(), bAgent = Tr->bMine && Sp && UStaffPeople::On() && !bHired;
			const int32 Crew = Tr->bMine && !bAgent ? ADPSchedule::OnShift(Sim->World, TEXT("ground"), Now) : 99;
			const float Tm = float((Now - Tr->OnBlocks).GetTotalMinutes());
			for (FTask& K : Tr->Tasks)
			{
				// (a task waits for the ones before it, however late they run)
				bool bReady = true;
				for (const FString& Af : K.After) for (const FTask& P : Tr->Tasks) if (P.Id == Af) { if (!P.bDone) bReady = false; }
			// (to-do 212: no vehicle comes to the aircraft before its chocks are in - at most four minutes over its own time)
			if (!K.bStarted && Tm < K.Start + 4.f && (K.Id == TEXT("bulk") || K.Id == TEXT("fwd") || K.Id == TEXT("aft") || K.Id == TEXT("water") || K.Id == TEXT("toilet") || K.Id == TEXT("fuel") || K.Id == TEXT("cat1")) && !Done(*Tr, TEXT("chocks"))) bReady = false;
			if (K.Id == TEXT("chocks") && bReady && !K.bStarted && Tm >= K.Start)
			{
				const double Left = Tr->PlanId.IsEmpty() ? 1e6 : (Tr->OffBlocks - Now).GetTotalMinutes();
				if (Tr->bArrival && Left < 12.0) { K.bStarted = K.bDone = true; ADP_LOG(TEXT("Turnaround: %s - no chocks, its pushback crew come now"), *Tr->Reg); continue; }
				ATugs* Tg = ATugs::Get(W);
				if (Tg && !Tg->ChocksOn(Tr->StandX, Tr->Reg, Tr->OffBlocks, bHired, Left >= 120.0, !Tr->bArrival)) bReady = false;
			}
				if ((K.Id == TEXT("board") || K.Id == TEXT("doors") || K.Id == TEXT("push") || K.Id == TEXT("tug") || K.Id == TEXT("apu") || K.Id == TEXT("gpuoff")) && Tr->PlanId.IsEmpty()) bReady = false;   // (no departure planned: nothing to get ready for)
				if (K.Id == TEXT("push")) continue;   // (the pushback itself: the tug and SimTraffic, once the flight may leave)
				// (the doors: everyone aboard first - APaxFlow; without passengers in 3D at once)
				if (K.Id == TEXT("doors") && bReady && !K.bStarted && Tm >= K.Start && !APaxFlow::BoardingDone(W, Tr->Reg)) bReady = false;
				// (to-do 212: the cabin cleaned by your cleaners and their van - asked for 8 min ahead; nobody free = it and the boarding wait)
				if (K.Id == TEXT("clean") && Tr->bMine && !Tr->PlanId.IsEmpty() && !K.bStarted && Tm >= K.Start - 8.f)
					if (AGSE* Gs = AGSE::Get(W)) if (Gs->SvcFor(TEXT("clean"), Tr->StandX) && !Gs->SvcAsk(TEXT("clean"), Tr->StandX, Tr->Reg, Tr->Flight, Tr->OffBlocks)) bReady = false;
				// (to-do 212: fuelled by your fuel driver and the refueller - asked for 8 min ahead; boarding does not wait for it, the flight does)
				if (K.Id == TEXT("fuel") && Tr->bMine && !Tr->PlanId.IsEmpty() && !K.bStarted && Tm >= K.Start - 8.f)
					if (AGSE* Gs = AGSE::Get(W)) if (Gs->SvcFor(TEXT("fuel"), Tr->StandX) && !Gs->SvcAsk(TEXT("fuel"), Tr->StandX, Tr->Reg, Tr->Flight, Tr->OffBlocks, Tr->FuelL)) bReady = false;
				// (to-do 212: catered by your catering crew and their truck, the front right door then the rear one - asked for 8 min ahead; boarding waits for it, as it did)
				// (water and toilets: asked for as soon as it is chocked; the two trucks see to it themselves that one waits for the other)
				if ((K.Id == TEXT("water") || K.Id == TEXT("toilet")) && Tr->bMine && !Tr->PlanId.IsEmpty() && !K.bStarted && (Done(*Tr, TEXT("chocks")) || Tm >= K.Start + 4.f))
					if (AGSE* Gs = AGSE::Get(W)) if (Gs->SvcFor(*K.Id, Tr->StandX) && !Gs->SvcAsk(*K.Id, Tr->StandX, Tr->Reg, Tr->Flight, Tr->OffBlocks)) bReady = false;
				if (K.Id == TEXT("cat1") && Tr->bMine && !Tr->PlanId.IsEmpty() && !K.bStarted && Tm >= K.Start - 8.f)
					if (AGSE* Gs = AGSE::Get(W)) if (Gs->SvcFor(TEXT("cat1"), Tr->StandX) && !Gs->SvcAsk(TEXT("cat1"), Tr->StandX, Tr->Reg, Tr->Flight, Tr->OffBlocks)) bReady = false;
				if (bHired && K.Id == TEXT("gpu") && bReady && !K.bStarted && Tm >= K.Start)
				{
					if (!Tr->PlanId.IsEmpty() && (Tr->OffBlocks - Now).GetTotalMinutes() < 12.0) { K.bStarted = K.bDone = true; ADP_LOG(TEXT("Turnaround: %s - no ground power, its pushback crew come now"), *Tr->Reg); continue; }
					ATugs* Tg = ATugs::Get(W);
					if (Tg && !Tg->ArrivedHired(Tr->StandX, Tr->Reg, Tr->OffBlocks)) bReady = false;
				}
				// (to-do 212: no jetway at this stand - the passenger stairs, asked for when it is on blocks; the door and everybody off
				//  wait for them, at most half an hour where the airport can't bring them at all (to-do 296: asked for again, below); a departure planned later asks for them again before its boarding)
				if (!K.bStarted && (K.Id == TEXT("bridge") || (K.Id == TEXT("board") && bReady)) && (Tr->bArrival || !Tr->PlanId.IsEmpty()) && Tm >= K.Start - (K.Id == TEXT("board") ? 12.f : 0.f))
					if (AGSE* Gs = AGSE::Get(W)) if (Gs->SvcFor(TEXT("stairs"), Tr->StandX))
					{
						FString Sw; if (!Gs->StairsHas(Tr->StandX) && Tm < K.Start + 20.f) Gs->SvcAsk(TEXT("stairs"), Tr->StandX, Tr->Reg, Tr->Flight, Tr->PlanId.IsEmpty() ? Now + FTimespan::FromMinutes(20.0) : Tr->OffBlocks, 0.f, K.Id == TEXT("bridge") && Tr->bArrival ? Tr->OnBlocks : Tr->OffBlocks, Tr->bMine);
						if (K.Id == TEXT("board") && (Tm < K.Start + 30.f || Gs->StairsCan(Tr->StandX, Sw)) && Gs->StairsBusy(Tr->StandX, false, Sw)) bReady = false;
					}
				const int32 Need = bHired && (K.Id == TEXT("gpu") || K.Id == TEXT("tug") || K.Id == TEXT("bulk")) ? 0 : K.Id == TEXT("gpu") || K.Id == TEXT("bridge") || K.Id == TEXT("doors") ? 1 : K.Id == TEXT("bulk") || K.Id == TEXT("tug") ? 2 : 0;
				if (!K.bStarted && bReady && Tm >= K.Start && Crew < Need)
				{
					ADPSchedule::Tell(Sim->World, Now, TEXT("ground:") + Tr->Reg + K.Id, TEXT("No ground crew"), FString::Printf(TEXT("%s at %s: %s waits - %d of your ground crew on shift, %d needed. Hire ground crew under Crew."), *Tr->Reg, *Tr->Stand, *K.Name, Crew, Need));
					bReady = false;
				}
				if (!K.bStarted && bReady && Tm >= K.Start)
				{
					K.bStarted = true; K.Start = FMath::Max(K.Start, Tm);
					const bool bVisual = K.Vehicle == TEXT("gpu") || K.Vehicle == TEXT("jetway") || K.Vehicle == TEXT("tug") || K.Vehicle == TEXT("beltloader") || (Tr->bMine && AGSE::Find(W) && AGSE::Find(W)->SvcFor(*K.Id, Tr->StandX));
					ADP_LOG(TEXT("Turnaround: %s %s starts (%.1f min, %s)"), *Tr->Reg, *K.Name, K.Dur, bVisual ? TEXT("in 3D") : K.Vehicle.IsEmpty() ? TEXT("the crew") : *FString::Printf(TEXT("%s - simulated until the vehicle is built"), *K.Vehicle));
					if (K.Id == TEXT("gpu") && !bHired) if (ATugs* Tg = ATugs::Get(W)) Tg->Arrived(Tr->StandX);   // (the ground power plugged in)
					if ((K.Id == TEXT("clean") || K.Id == TEXT("fuel") || K.Id == TEXT("cat1")) && Tr->bMine) if (AGSE* Gs = AGSE::Get(W)) Gs->SvcGo(*K.Id, Tr->StandX);
					if (K.Id == TEXT("bridge")) ASimTraffic::DockAt(W, Tr->Stand);                      // (the jetway out to the door)
					if (K.Id == TEXT("doors")) if (AGSE* Gs = AGSE::Find(W)) Gs->SvcGo(TEXT("stairs"), Tr->StandX);   // (everyone aboard, the door closed: the stairs away)
					if (K.Id == TEXT("doors")) ASimTraffic::UndockAt(W, Tr->Stand);                     // (everyone aboard, the door closed: the jetway back to rest)
					// (the bulk hold: out first, then in - the belt loader and an empty train for the arriving bags; the departure
					//  train comes once its make-up closes: GSE.h)
					if (K.Id == TEXT("bulk")) if (AGSE* Gs = AGSE::Get(W)) Gs->ArrivalAt(Tr->StandX, Tr->ArrFlight.IsEmpty() ? Tr->Reg : Tr->ArrFlight);
					// (the tug and its crew get ready: the driver in, the handler unplugs the ground power, the walkers come)
					if (K.Id == TEXT("tug")) if (ATugs* Tg = ATugs::Get(W)) Tg->Prepare(Tr->PlanId, Tr->StandX);
				}
				// (the bulk hold is done when the bags are: the belt loader and the trains gone from the stand)
				const bool bBagsBusy = K.Id == TEXT("bulk") && AGSE::Find(W) && AGSE::Find(W)->BusyAt(Tr->StandX);
				const bool bBoarding = K.Id == TEXT("board") && K.bStarted && !K.bDone && Tm >= K.Start + K.Dur && !APaxFlow::BoardingDone(W, Tr->Reg);   // (boarding is over when the last one is aboard)
				// (off the aircraft: not by the clock alone - the last walker through the gate's door, so nobody boards into arrivals;
				//  at most 15 min longer, then it is a problem in the log)
				FString Inside;
				const bool bOff = K.Id == TEXT("deplane") && K.bStarted && !K.bDone && Tr->bArrival && Tm >= K.Start + K.Dur && !APaxFlow::DeplaneDone(W, Tr->Reg, Tr->OnBlocks, Tr->ArrPax, Inside);
				if (bOff && Tr->bMine && Tm >= K.Start + K.Dur + 3.f) ADPSchedule::Tell(Sim->World, Now, TEXT("deplane:") + Tr->Reg, TEXT("Still getting off"), FString::Printf(TEXT("%s at %s: cleaning and boarding wait - %s."), *Tr->Reg, *Tr->Stand, *Inside));
				// (to-do 212: the passengers in wheelchairs off and on by the ambulift, at the front right door; the flight waits for them)
				FString AmWhy; bool bAmbu = false;
				if (Tr->bMine && bHired && !K.bDone && (K.Id == TEXT("deplane") ? Tr->bArrival : K.Id == TEXT("board") && !Tr->PlanId.IsEmpty()))
				{
					const bool bIn = K.Id == TEXT("deplane"); const int32 Nw = bIn ? AGSE::AmbuPax(Tr->ArrFlight, Tr->OnBlocks, Tr->ArrPax) : AGSE::AmbuPax(Tr->Flight, Tr->OffBlocks, Tr->Pax);
					const bool bStill = !bIn && Tr->bArrival && AGSE::AmbuPax(Tr->ArrFlight, Tr->OnBlocks, Tr->ArrPax) > 0;   // (the arriving ones may still sit in their seats: the caterers were at the door first)
					AGSE* Gs = Nw > 0 || bStill ? AGSE::Get(W) : nullptr; const bool bAsk = Gs && (bIn ? Done(*Tr, TEXT("chocks")) : Tm >= K.Start - 15.f && Done(*Tr, TEXT("cat2")));
					if (bAsk && Nw > 0) Gs->AmbuCall(Tr->StandX, Tr->Reg, bIn ? Tr->ArrFlight : Tr->Flight, Tr->OffBlocks, Nw, bIn, K.bStarted);
					bAmbu = bAsk && K.bStarted && Tm < K.Start + K.Dur + 60.f && (Gs->AmbuBusy(Tr->StandX, bIn, AmWhy, bIn) || (bStill && Gs->AmbuBusy(Tr->StandX, true, AmWhy)));
					if (bAmbu && Tm >= K.Start + K.Dur + 3.f) ADPSchedule::Tell(Sim->World, Now, TEXT("ambulift:") + Tr->Reg + K.Id, TEXT("Wheelchair passengers"), FString::Printf(TEXT("%s at %s: %s."), *Tr->Reg, *Tr->Stand, *AmWhy));
				}
				FString StWhy; const bool bStairs = K.bStarted && !K.bDone && (K.Id == TEXT("bridge") ? Tm < K.Start + 30.f || (AGSE::Find(W) && AGSE::Find(W)->StairsCan(Tr->StandX, StWhy)) : K.Id == TEXT("doors")) && AGSE::Find(W) && AGSE::Find(W)->StairsBusy(Tr->StandX, K.Id == TEXT("doors"), StWhy);
				if (bStairs && Tr->bMine && Tm >= K.Start + K.Dur + 3.f) ADPSchedule::Tell(Sim->World, Now, TEXT("stairs:") + Tr->Reg + K.Id, TEXT("Passenger stairs"), FString::Printf(TEXT("%s at %s: %s."), *Tr->Reg, *Tr->Stand, *StWhy));
				if (K.Id == TEXT("deplane") && K.bDone && Tr->bArrival && Tr->PlanId.IsEmpty() && Tm >= K.Start + K.Dur && Tm < K.Start + K.Dur + 60.f) if (AGSE* Gs = AGSE::Find(W)) Gs->SvcGo(TEXT("stairs"), Tr->StandX);   // (no departure planned: they go home)
				const bool bBridge = K.Id == TEXT("bridge") && K.bStarted && !K.bDone && !ASimTraffic::JetwayReady(W, Tr->Stand, true);
				const bool bBack = K.Id == TEXT("doors") && K.bStarted && !K.bDone && !ASimTraffic::JetwayReady(W, Tr->Stand, false);
				const bool bChocks = K.Id == TEXT("chocks") && K.bStarted && !K.bDone && ATugs::Get(W) && !ATugs::Get(W)->ChocksIn(Tr->StandX);
			const bool bClean = (K.Id == TEXT("clean") || K.Id == TEXT("fuel") || K.Id == TEXT("cat1") || K.Id == TEXT("cat2") || K.Id == TEXT("water") || K.Id == TEXT("toilet")) && K.bStarted && !K.bDone && Tr->bMine && AGSE::Find(W) && AGSE::Find(W)->SvcBusy(*K.Id, Tr->StandX);
			if (K.bStarted && !K.bDone && Tm >= K.Start + K.Dur && !bClean && !bChocks && !bBagsBusy && !bBoarding && !bOff && !bBridge && !bBack && !bStairs && !bAmbu) { K.bDone = true; ADP_LOG(TEXT("Turnaround: %s %s done"), *Tr->Reg, *K.Name); }
			}
			// (to-do 296: nobody is ever left without stairs - they did not come by themselves: it asks for them and checks back)
			if (const FAirfieldData::FStand* Sd = AAirfield::Shared().StandAt(Tr->StandX); Sd && !Sd->bCargo && !Sd->bJetway)
			{
				bool bIn = false, bOut = false;
				for (const FTask& K : Tr->Tasks)
				{
					if (K.Id == TEXT("bridge") && (K.bStarted || Tm >= K.Start) && Tr->bArrival && Tr->ArrPax > 0) bIn = !Done(*Tr, TEXT("deplane"));
					if (K.Id == TEXT("board")) bOut = !Tr->PlanId.IsEmpty() && Tm >= K.Start && !K.bDone;
				}
				if (AGSE* Gs = bIn || bOut ? AGSE::Get(W) : nullptr)
				{
					FString Wy, Cw; const bool bCan = Gs->StairsCan(Tr->StandX, Cw); const int32 Where = Gs->StairsWhere(Tr->StandX, Wy);
					const FString Fl = bIn && !Tr->ArrFlight.IsEmpty() ? Tr->ArrFlight : !bIn && !Tr->Flight.IsEmpty() ? Tr->Flight : Tr->Reg;
					const int32 Lv = ADPStandAsk::Check(W, Now, FString::Printf(TEXT("stairs:%s:%s:%s"), bIn ? TEXT("in") : TEXT("out"), *Tr->Reg, *Tr->OnBlocks.ToString()), TEXT("Passenger stairs"), FString::Printf(TEXT("%s (%s) at stand %s"), *Fl, *Tr->Reg, *Tr->Stand), Where, bCan, bCan ? Wy : Cw);
					if (Lv > 0 && bCan && Where < 2)
					{
						if (!Gs->StairsHas(Tr->StandX)) Gs->SvcAsk(TEXT("stairs"), Tr->StandX, Tr->Reg, Tr->Flight, Tr->PlanId.IsEmpty() ? Now + FTimespan::FromMinutes(20.0) : Tr->OffBlocks, 0.f, bIn ? Tr->OnBlocks : Tr->OffBlocks, false);
						if (Where == 0) Gs->StairsUrge(Tr->StandX, Lv);
					}
				}
			}
			// (to-do 212: de-iced when the weather asks for it, once its doors are shut and the jetway is back; the flight waits)
			if (Tr->bMine)
			{
				bool bSoon = false, bShut = false;   // (what falls on it counts all the time it stands)
				if (bHired && !Tr->PlanId.IsEmpty()) if (const FTask* Dk = Tr->Tasks.FindByPredicate([](const FTask& K) { return K.Id == TEXT("doors"); })) if (Dk->bDone || Dk->bStarted || Tm >= Dk->Start - 12.f) { bSoon = true; bShut = Dk->bDone; }
				if (AGSE* Gs = bSoon ? AGSE::Get(W) : AGSE::Find(W))
				{
					Gs->DeiceTick(Tr->StandX, Tr->Reg, Tr->Flight, Now, Tr->OffBlocks, bSoon, bShut, Tr->bArrival ? Tr->OnBlocks : FDateTime(0));
					// (its look-over in icing weather: a line of its tasks, the flight waits for it - GSE's SvcWaits)
					FTask* Ic = Tr->Tasks.FindByPredicate([](const FTask& K) { return K.Id == TEXT("icecheck"); }); const bool bLk = bSoon && Gs->DeiceLooks(Tr->Flight);
					if (bLk && !Ic) { FTask& K = Tr->Tasks.AddDefaulted_GetRef(); K.Id = TEXT("icecheck"); K.Name = TEXT("looked over for ice"); K.Start = Tm; K.Dur = 2.f; K.bStarted = true; }
					else if (bLk && Ic->bDone) { Ic->bDone = false; Ic->Start = Tm; }   // (once more)
				}
			}
			// (its bags go out on the tractor train to it as soon as the stand is its own: 50 min before)
			if (!Tr->PlanId.IsEmpty() && (Tr->OffBlocks - Now).GetTotalMinutes() < 50.0 && (bHired || Crew >= 2)) if (AGSE* Gs = AGSE::Get(W)) Gs->Departure(Tr->Flight, Tr->StandX);
		}
		// gone (pushed back and away): its plan with it
		Turns.RemoveAll([&](const FTurn& X)
		{
			if (Seen.Contains(X.Reg)) return false;
			if (!X.Flight.IsEmpty()) if (AGSE* Gs = AGSE::Find(W)) Gs->Gone(X.Flight);
		if (ATugs* Tg = ATugs::Get(W)) Tg->ChocksGone(X.StandX);
		if (AGSE* Gs = AGSE::Find(W)) Gs->SvcGone(X.StandX);
			return true;
		});
	}
}
