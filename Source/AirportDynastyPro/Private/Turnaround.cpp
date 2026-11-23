#include "Turnaround.h"
#include "ADPLog.h"
#include "Tugs.h"
#include "GSE.h"
#include "Airfield.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSchedule.h"
#include "PaxFlow.h"
#include "SimTraffic.h"
#include "StaffPeople.h"
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
				if ((K.Id == TEXT("board") || K.Id == TEXT("doors") || K.Id == TEXT("push") || K.Id == TEXT("tug") || K.Id == TEXT("apu") || K.Id == TEXT("gpuoff")) && Tr->PlanId.IsEmpty()) bReady = false;   // (no departure planned: nothing to get ready for)
				if (K.Id == TEXT("push")) continue;   // (the pushback itself: the tug and SimTraffic, once the flight may leave)
				// (the doors: everyone aboard first - APaxFlow; without passengers in 3D at once)
				if (K.Id == TEXT("doors") && bReady && !K.bStarted && Tm >= K.Start && !APaxFlow::BoardingDone(W, Tr->Reg)) bReady = false;
				if (bHired && K.Id == TEXT("gpu") && bReady && !K.bStarted && Tm >= K.Start)
				{
					if (!Tr->PlanId.IsEmpty() && (Tr->OffBlocks - Now).GetTotalMinutes() < 12.0) { K.bStarted = K.bDone = true; ADP_LOG(TEXT("Turnaround: %s - no ground power, its pushback crew come now"), *Tr->Reg); continue; }
					ATugs* Tg = ATugs::Get(W);
					if (Tg && !Tg->ArrivedHired(Tr->StandX, Tr->Reg, Tr->OffBlocks)) bReady = false;
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
					const bool bVisual = K.Vehicle == TEXT("gpu") || K.Vehicle == TEXT("jetway") || K.Vehicle == TEXT("tug") || K.Vehicle == TEXT("beltloader");
					ADP_LOG(TEXT("Turnaround: %s %s starts (%.1f min, %s)"), *Tr->Reg, *K.Name, K.Dur, bVisual ? TEXT("in 3D") : K.Vehicle.IsEmpty() ? TEXT("the crew") : *FString::Printf(TEXT("%s - simulated until the vehicle is built"), *K.Vehicle));
					if (K.Id == TEXT("gpu") && !bHired) if (ATugs* Tg = ATugs::Get(W)) Tg->Arrived(Tr->StandX);   // (the ground power plugged in)
					if (K.Id == TEXT("bridge")) ASimTraffic::DockAt(W, Tr->Stand);                      // (the jetway out to the door)
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
				const bool bBridge = K.Id == TEXT("bridge") && K.bStarted && !K.bDone && !ASimTraffic::JetwayReady(W, Tr->Stand, true);
				const bool bBack = K.Id == TEXT("doors") && K.bStarted && !K.bDone && !ASimTraffic::JetwayReady(W, Tr->Stand, false);
				if (K.bStarted && !K.bDone && Tm >= K.Start + K.Dur && !bBagsBusy && !bBoarding && !bOff && !bBridge && !bBack) { K.bDone = true; ADP_LOG(TEXT("Turnaround: %s %s done"), *Tr->Reg, *K.Name); }
			}
			// (its bags go out on the tractor train to it as soon as the stand is its own: 50 min before)
			if (!Tr->PlanId.IsEmpty() && (Tr->OffBlocks - Now).GetTotalMinutes() < 50.0 && (bHired || Crew >= 2)) if (AGSE* Gs = AGSE::Get(W)) Gs->Departure(Tr->Flight, Tr->StandX);
		}
		// gone (pushed back and away): its plan with it
		Turns.RemoveAll([&](const FTurn& X)
		{
			if (Seen.Contains(X.Reg)) return false;
			if (!X.Flight.IsEmpty()) if (AGSE* Gs = AGSE::Find(W)) Gs->Gone(X.Flight);
			return true;
		});
	}
}
