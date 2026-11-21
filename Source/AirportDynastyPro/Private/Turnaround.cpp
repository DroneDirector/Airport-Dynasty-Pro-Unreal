#include "Turnaround.h"
#include "ADPLog.h"
#include "Tugs.h"
#include "GSE.h"
#include "Airfield.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPBoards.h"
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
		// (check-in and the make-up close about 40 minutes before off-blocks - the bags go out then)
		const FTurn* T = ForFlight(Flight); if (!T) T = AtStand(StandX);
		return T && Last.GetTicks() > 0 && Last >= T->OffBlocks - FTimespan::FromMinutes(40.0);
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
		const float BoardEnd = G - 1.5f, BoardStart = BoardEnd - Board;
		Add(TEXT("board"),    TEXT("boarding"),                          TEXT(""), BoardStart, Board, {TEXT("clean"), TEXT("cat2")});
		Add(TEXT("doors"),    TEXT("doors closed, jetway off"),          TEXT("jetway"), BoardEnd, 1.5f, {TEXT("board")});
		Add(TEXT("apu"),      TEXT("APU started"),                       TEXT(""), G - 10.f, 1.f);
		Add(TEXT("gpuoff"),   TEXT("ground power off and stowed"),       TEXT("gpu"), G - 8.f, 4.f, {TEXT("apu")});
		Add(TEXT("tug"),      TEXT("tug and pushback crew ready"),       TEXT("tug"), G - 8.f, 5.f);
		Add(TEXT("push"),     TEXT("pushback"),                          TEXT("tug"), G, 3.f, {TEXT("doors"), TEXT("gpuoff")});
		// cleaning fills the time between deplaning and boarding
		for (FTask& K : T) if (K.Id == TEXT("clean")) K.Dur = FMath::Max(5.f, BoardStart - K.Start);
		// too short a ground time: what doesn't fit before boarding makes it late
		float NeedBoard = 0.f; for (const FTask& K : T) if (K.Id == TEXT("cat2") || K.Id == TEXT("deplane")) NeedBoard = FMath::Max(NeedBoard, K.Start + K.Dur + 5.f);
		LateMin = FMath::Max(0.f, NeedBoard - BoardStart);
		if (LateMin > 0.f) for (FTask& K : T) if (K.Start >= BoardStart - 0.01f && K.Id != TEXT("apu")) K.Start += LateMin;
		return T;
	}

	void Tick(UWorld* W, const FDateTime& Now)
	{
		UADPSim* Sim = UADPSim::Get(W); if (!Sim || !Sim->IsRunning()) return;
		Last = Now;
		const FString Here = ADPBoards::HereIcao();
		// the aircraft standing at a gate here, each with its next departure
		TSet<FString> Seen;
		for (const FADPAircraft& A : Sim->World.Aircraft)
		{
			if (A.Location != Here || A.Stand.IsEmpty() || A.Status == TEXT("flying")) continue;
			Seen.Add(A.Reg);
			FTurn* Tr = Turns.FindByPredicate([&](const FTurn& X) { return X.Reg == A.Reg; });
			const FADPFlight* Dep = nullptr;
			for (const FADPFlight& F : Sim->World.Flights) if (F.Reg == A.Reg && F.From == Here && F.DepAt > Now - FTimespan::FromMinutes(1.0) && (!Dep || F.DepAt < Dep->DepAt)) Dep = &F;
			if (!Tr)
			{
				FTurn N; N.Reg = A.Reg; N.Stand = A.Stand; N.OnBlocks = Now;
				for (const FAirfieldData::FStand& S : AAirfield::Shared().Stands) if (S.Id == A.Stand) N.StandX = S.X;
				N.OffBlocks = Dep ? Dep->DepAt : Now + FTimespan::FromMinutes(60.0);
				N.Flight = Dep ? Dep->Id : FString();
				N.Pax = Dep ? FMath::Max(1, Dep->Pax + Dep->PaxJ + Dep->PaxF) : 150;
				const float G = float((N.OffBlocks - N.OnBlocks).GetTotalMinutes());
				N.Tasks = PlanA320(G, N.Pax, N.FuelL, N.LateMin);
				ADP_LOG(TEXT("Turnaround: %s at %s - %.0f min on the ground, %d passengers%s"), *N.Reg, *N.Stand, G, N.Pax, N.LateMin > 0.f ? *FString::Printf(TEXT(", %.0f min late (tight turn)"), N.LateMin) : TEXT(""));
				Tr = &Turns.Add_GetRef(MoveTemp(N));
			}
			else if (Dep && Dep->DepAt != Tr->OffBlocks)
			{
				// (the departure moved - a delay: the tasks still to come shift with it, the gaps between them kept)
				const float Shift = float((Dep->DepAt - Tr->OffBlocks).GetTotalMinutes());
				for (FTask& K : Tr->Tasks) if (!K.bStarted && K.Start > float((Now - Tr->OnBlocks).GetTotalMinutes()) - 0.01f && (K.Id == TEXT("board") || K.Id == TEXT("doors") || K.Id == TEXT("apu") || K.Id == TEXT("gpuoff") || K.Id == TEXT("tug") || K.Id == TEXT("push"))) K.Start += Shift;
				Tr->OffBlocks = Dep->DepAt; Tr->Flight = Dep->Id;
				ADP_LOG(TEXT("Turnaround: %s off-blocks moved %+.0f min - the departure tasks with it"), *Tr->Reg, Shift);
			}
			const float Tm = float((Now - Tr->OnBlocks).GetTotalMinutes());
			for (FTask& K : Tr->Tasks)
			{
				// (a task waits for the ones before it, however late they run)
				bool bReady = true; float AfterEnd = 0.f;
				for (const FString& Af : K.After) for (const FTask& P : Tr->Tasks) if (P.Id == Af) { if (!P.bDone) bReady = false; AfterEnd = FMath::Max(AfterEnd, P.Start + P.Dur); }
				if (!K.bStarted && bReady && Tm >= K.Start)
				{
					K.bStarted = true; K.Start = FMath::Max(K.Start, Tm);
					const bool bVisual = K.Vehicle == TEXT("gpu") || K.Vehicle == TEXT("jetway") || K.Vehicle == TEXT("tug") || K.Vehicle == TEXT("beltloader");
					ADP_LOG(TEXT("Turnaround: %s %s starts (%.1f min, %s)"), *Tr->Reg, *K.Name, K.Dur, bVisual ? TEXT("in 3D") : K.Vehicle.IsEmpty() ? TEXT("the crew") : *FString::Printf(TEXT("%s - simulated until the vehicle is built"), *K.Vehicle));
					if (K.Id == TEXT("gpu")) if (ATugs* Tg = ATugs::Get(W)) Tg->Arrived(Tr->StandX);   // (the ground power plugged in)
					// (the bulk hold: out first, then in - the belt loader and an empty train for the arriving bags; the departure
					//  train comes once its make-up closes: GSE.h)
					if (K.Id == TEXT("bulk")) if (AGSE* Gs = AGSE::Get(W)) Gs->ArrivalAt(Tr->StandX, Tr->Flight.IsEmpty() ? Tr->Reg : Tr->Flight);
				}
				// (the bulk hold is done when the bags are: the belt loader and the trains gone from the stand)
				const bool bBagsBusy = K.Id == TEXT("bulk") && AGSE::Find(W) && AGSE::Find(W)->BusyAt(Tr->StandX);
				if (K.bStarted && !K.bDone && Tm >= K.Start + K.Dur && !bBagsBusy) { K.bDone = true; ADP_LOG(TEXT("Turnaround: %s %s done"), *Tr->Reg, *K.Name); }
				// (bags still going in when the doors are due: the departure waits - off-blocks moved 2 min at a time through the
				//  delay path above, the whole chain with it; at most 20 min for the bags)
				if (K.Id == TEXT("doors") && !K.bStarted && Dep && Tm >= K.Start - 0.5f && Tr->BagDelayMin < 20.f)
					if (const AGSE* Gs = AGSE::Find(W)) if (Gs->BusyAt(Tr->StandX))
						for (FADPFlight& F : Sim->World.Flights) if (F.Id == Dep->Id)
						{ F.DepAt += FTimespan::FromMinutes(2.0); Tr->BagDelayMin += 2.f; ADP_LOG(TEXT("Turnaround: %s (%s) delayed 2 min (%.0f in all) - its bags are still being loaded into the hold"), *Tr->Reg, *F.Id, Tr->BagDelayMin); break; }
			}
		}
		// gone (pushed back and away): its plan with it
		Turns.RemoveAll([&](const FTurn& X) { return !Seen.Contains(X.Reg); });
	}
}
