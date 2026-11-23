// The functional test of the whole passenger and crew chain (owner, 2026-11-23): in a real career, without test switches
// for the passengers - the career step after the new game test (UI/NewGameTest.cpp; Tools/new_game_test.sh --career).
// By the same calls the Overview's screens make (UADPBridge::Sim): two A320s, the people, the timetable; then the time runs
// faster and every step of every passenger and crew task is read from the game and logged ("CHAIN TEST: ..."). Stages, each
// with a save point at its start (Saved/Careers/chaintest-<n>.json), so a fix goes on from the stage that failed:
//   1  bought, hired, the timetable made; the steps without staff wait and say why; the bookings come in
//   2  two departures (15 min apart): car park -> desks -> security -> Floor 2 -> gate -> aboard; the crew's turn-round,
//      pushback, taxi, take-off
//   3  the first aircraft back: off the aircraft -> claim belt, their own bag -> out -> the car park
//   4  its turn-round and second departure of the day
// Nothing is measured. -ChainTest=<stage to start at>.
#include "ChainTest.h"
#include "ADPLog.h"
#include "PaxFlow.h"
#include "Turnaround.h"
#include "SimTraffic.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSchedule.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPWorldData.h"
#include "UI/ADPBridge.h"
#include "UI/ADPHUD.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FChain
	{
		TWeakObjectPtr<UWorld> World;
		int32 Stage = 1, Sub = 0, Problems = 0;
		double T0 = 0.0, SubT0 = 0.0, SayAt = 0.0;
		FString A, B, To;                     // the two aircraft, the place
		FDateTime DepA, DepB;
		float Speed = 1.f;
		TSet<FString> Seen;
	};
	TSharedPtr<FChain> GChain;

	UADPSim* ChSim() { return GChain->World.IsValid() ? UADPSim::Get(GChain->World.Get()) : nullptr; }
	void ChProblem(const FString& S) { ++GChain->Problems; ADP_WARN(TEXT("CHAIN TEST: PROBLEM at stage %d: %s"), GChain->Stage, *S); }
	/** The time: the simulation's and the 3D world's together (Walk), or the simulation alone (nothing to see: waiting). */
	void ChSpeed(float Sim, float World3D)
	{
		if (UADPSim* S = ChSim()) S->SetSpeed(Sim);
		if (UWorld* W = GChain->World.Get()) UGameplayStatics::SetGlobalTimeDilation(W, World3D);
		GChain->Speed = Sim;
	}
	/** The Overview's own call. */
	TSharedPtr<FJsonObject> ChCall(const TCHAR* Action, const FString& Arg)
	{
		UWorld* W = GChain->World.Get(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; AADPHUD* H = PC ? Cast<AADPHUD>(PC->GetHUD()) : nullptr;
		TSharedPtr<FJsonObject> J; if (!H) { ChProblem(TEXT("no HUD")); return J; }
		UADPBridge* Br = NewObject<UADPBridge>(); Br->Hud = H;
		const FString S = Br->Sim(Action, Arg);
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S), J);
		return J;
	}
	bool ChOk(const TCHAR* Action, const FString& Arg, bool bMust = true)
	{
		TSharedPtr<FJsonObject> J = ChCall(Action, Arg);
		const bool b = J && J->GetBoolField(TEXT("ok")); FString Why; if (J) J->TryGetStringField(TEXT("why"), Why);
		ADP_LOG(TEXT("CHAIN TEST: %s '%s' - %s%s%s"), Action, *Arg, b ? TEXT("done") : TEXT("refused"), Why.IsEmpty() ? TEXT("") : TEXT(": "), *Why);
		if (!b && bMust) ChProblem(FString::Printf(TEXT("%s %s refused: %s"), Action, *Arg, *Why));
		return b;
	}
	int32 ChHire(const TCHAR* Role, int32 N)
	{
		// (the Crew screen's list: the first ones in the pool with that job)
		TSharedPtr<FJsonObject> J = ChCall(TEXT("crew"), FString()); int32 Got = 0;
		const TArray<TSharedPtr<FJsonValue>>* Pool = nullptr;
		if (J && J->TryGetArrayField(TEXT("pool"), Pool))
			for (const TSharedPtr<FJsonValue>& V : *Pool)
			{
				const TSharedPtr<FJsonObject> P = V->AsObject();
				if (Got >= N) break;
				if (P->GetStringField(TEXT("role")) != Role) continue;
				TSharedPtr<FJsonObject> R = ChCall(TEXT("hire"), P->GetStringField(TEXT("id")) + TEXT("|"));
				if (R && R->GetBoolField(TEXT("ok"))) ++Got;
			}
		if (Got < N) ChProblem(FString::Printf(TEXT("only %d of %d %s could be hired"), Got, N, Role));
		return Got;
	}
	FString ChHm(const FDateTime& Local) { return Local.ToString(TEXT("%H:%M")); }
	const FADPPlanned* ChPlan(const FString& Reg, bool bReturn, int32 Nth = 0)
	{
		TArray<const FADPPlanned*> L;
		if (const UADPSim* S = ChSim()) for (const FADPPlanned& P : S->World.Planned) if (P.Reg == Reg && P.bReturn == bReturn && P.State != TEXT("cancelled")) L.Add(&P);
		L.Sort([](const FADPPlanned& X, const FADPPlanned& Y) { return X.Sched < Y.Sched; });
		return L.IsValidIndex(Nth) ? L[Nth] : nullptr;
	}
	const FADPFlight* ChFlight(const FString& Reg) { const UADPSim* S = ChSim(); return S ? S->World.Flights.FindByPredicate([&](const FADPFlight& F) { return F.Reg == Reg; }) : nullptr; }
	bool ChCount(const FString& Label, bool bArr, APaxFlow::FCount& Out)
	{
		const APaxFlow* P = APaxFlow::Find(GChain->World.Get()); if (!P) return false;
		TArray<APaxFlow::FCount> L; P->Counts(L);
		for (int32 i = L.Num() - 1; i >= 0; --i) if (L[i].Label == Label && L[i].bArr == bArr) { Out = L[i]; return true; }
		return false;
	}
	void ChSave(int32 N)
	{
		if (UADPSim* S = ChSim()) { const FString Slot = S->World.Slot; S->SaveCareer(FString::Printf(TEXT("chaintest-%d"), N)); S->World.Slot = Slot; ADP_LOG(TEXT("CHAIN TEST: save point of stage %d (%s)"), N, *ADPBoards::Local(S->Now()).ToString(TEXT("%H:%M"))); }
	}
	void ChNext(int32 Stage) { ADP_LOG(TEXT("CHAIN TEST: STAGE %d %s (%d problems so far)"), GChain->Stage, GChain->Problems ? TEXT("DONE") : TEXT("PASSED"), GChain->Problems); { int32 Stop = 99; if (FParse::Value(FCommandLine::Get(), TEXT("ChainStop="), Stop) && GChain->Stage >= Stop) { GChain->Stage = 99; return; } } GChain->Stage = Stage; GChain->Sub = 0; GChain->SubT0 = FPlatformTime::Seconds(); ADP_LOG(TEXT("CHAIN TEST: stage %d"), Stage); }
	void ChSub() { ++GChain->Sub; GChain->SubT0 = FPlatformTime::Seconds(); }
	void ChEnd(const FString& Why)
	{
		if (const APaxFlow* P = APaxFlow::Find(GChain->World.Get()))
		{
			ADP_LOG(TEXT("CHAIN TEST: RESULT stuck: %d in jetways, %d elsewhere"), P->StuckInJetways(), P->StuckElsewhere());
			if (P->StuckInJetways()) ChProblem(FString::Printf(TEXT("%d times someone was stuck in a jetway"), P->StuckInJetways()));
			if (P->StuckElsewhere()) ChProblem(FString::Printf(TEXT("%d times someone was stuck elsewhere"), P->StuckElsewhere()));
		}
		if (UADPSim* S = ChSim()) S->SetSpeed(1.0);
		ADP_LOG(TEXT("CHAIN TEST: %s - %d problems, %.0f min%s%s"), GChain->Problems ? TEXT("FAILED") : TEXT("ALL PASSED"), GChain->Problems, (FPlatformTime::Seconds() - GChain->T0) / 60.0, Why.IsEmpty() ? TEXT("") : TEXT(" - "), *Why);
		FPlatformMisc::RequestExit(false);
	}
	void ChSay(double Every)
	{
		if (FPlatformTime::Seconds() - GChain->SayAt < Every) return;
		GChain->SayAt = FPlatformTime::Seconds();
		const UADPSim* S = ChSim(); const APaxFlow* P = APaxFlow::Find(GChain->World.Get());
		if (!S) return;
		ADP_LOG(TEXT("CHAIN TEST: --- %s local, x%.0f"), *ADPBoards::Local(S->Now()).ToString(TEXT("%d.%m %H:%M")), GChain->Speed);
		for (const FADPPlanned& Q : S->World.Planned) if (Q.AirlineId == UADPSim::Me() && Q.State == TEXT("planned") && FMath::Abs((Q.Est - S->Now()).GetTotalMinutes()) < 200.0)
			ADP_LOG(TEXT("CHAIN TEST: plan %s %s %s-%s, expected %s (%+.0f min), %d booked, %s%s%s"), *Q.FlightNo, *Q.Reg, *Q.From, *Q.To, *ChHm(ADPBoards::Local(Q.Est)), (Q.Est - Q.Sched).GetTotalMinutes(), Q.Booked, *Q.Stand, Q.Why.IsEmpty() ? TEXT("") : TEXT(" - waits: "), *Q.Why);
		if (P) { TArray<APaxFlow::FCount> L; P->Counts(L); for (const APaxFlow::FCount& C : L) if (C.About || C.Spawned < C.Walkers) ADP_LOG(TEXT("CHAIN TEST: %s %s: %d booked, %d walkers - %d out of their cars / off, %d of the %d with a bag checked in (the others: hand luggage, checked in on the phone), %d screened, %d up, %d aboard, %d bags taken, %d out, %d late, %d gone home; %d about now: %s"), *C.Label, C.bArr ? TEXT("arrival") : TEXT("departure"), C.Pax, C.Walkers, C.Spawned, C.Desk, C.Bags, C.Lane, C.F2, C.Aboard, C.Bag, C.Out, C.Late, C.Home, C.About, *C.Stages); }
		for (const ADPTurnaround::FTurn& T : ADPTurnaround::All())
		{
			FString D; for (const ADPTurnaround::FTask& K : T.Tasks) if (K.bStarted && !K.bDone) D += K.Id + TEXT(" ");
			if (T.bMine) ADP_LOG(TEXT("CHAIN TEST: turn-round %s at %s: working on %s"), *T.Reg, *T.Stand, D.IsEmpty() ? TEXT("nothing just now") : *D);
		}
	}
	/** A departure's passengers: what isn't as it should be when it has gone. */
	void ChCheckDeparture(const FString& Label)
	{
		APaxFlow::FCount C;
		if (!ChCount(Label, false, C)) { ChProblem(FString::Printf(TEXT("nobody walked for %s"), *Label)); return; }
		ADP_LOG(TEXT("CHAIN TEST: RESULT %s departure: %d booked, %d walkers: %d came, %d of the %d with a bag checked it in, %d screened, %d up on Floor 2, %d aboard, %d late, %d gone home"), *Label, C.Pax, C.Walkers, C.Spawned, C.Desk, C.Bags, C.Lane, C.F2, C.Aboard, C.Late, C.Home);
		if (C.Spawned < C.Walkers) ChProblem(FString::Printf(TEXT("%s: only %d of %d came out of the car park"), *Label, C.Spawned, C.Walkers));
		ADP_LOG(TEXT("CHAIN TEST: RESULT %s: boarding called %.0f min before its departure"), *Label, C.CallBefore);
		if (C.CallBefore < 20.f) ChProblem(FString::Printf(TEXT("%s: boarding called only %.0f min before"), *Label, C.CallBefore));
		if (C.Desk < C.Bags - C.Late) ChProblem(FString::Printf(TEXT("%s: %d of %d bags checked in"), *Label, C.Desk, C.Bags));
		const int32 Due = FMath::Max(0, C.Spawned - C.Late);   // (too late for the check-in: home, not missing aboard)
		if (C.Lane < Due) ChProblem(FString::Printf(TEXT("%s: %d of %d passed security"), *Label, C.Lane, Due));
		if (C.F2 < Due) ChProblem(FString::Printf(TEXT("%s: %d of %d came up to Floor 2"), *Label, C.F2, Due));
		if (C.Aboard < Due) ChProblem(FString::Printf(TEXT("%s: %d of %d aboard - left behind: %s"), *Label, C.Aboard, Due, *C.Stages));
	}

	bool Tick(float)
	{
		if (!GChain) return false;
		UWorld* W = GChain->World.Get(); UADPSim* S = ChSim();
		if (!W || !S) { GChain.Reset(); return false; }
		const double Now = FPlatformTime::Seconds(), T = Now - GChain->SubT0;
		const FDateTime Sn = S->Now();
		const FString Here = ADPBoards::HereIcao();
		FChain& C = *GChain;
		if (Now - C.T0 > 3.0 * 3600.0) { ChProblem(TEXT("three hours")); ChEnd(TEXT("time")); return false; }
		// (the aircraft and the flights, also after a save point)
		if (C.A.IsEmpty()) for (const FADPAircraft& X : S->World.Aircraft) if (X.AirlineId == UADPSim::Me()) { if (C.A.IsEmpty()) C.A = X.Reg; else if (C.B.IsEmpty()) C.B = X.Reg; }

		switch (C.Stage)
		{
		case 1:
			if (C.Sub == 0)
			{
				if (T < 5.0) return true;
				// (the morning: the whole day before the curfew)
				{ const FDateTime L = ADPBoards::Local(Sn); const FDateTime Eight = L.GetDate() + FTimespan::FromHours(L.GetHour() >= 8 ? 32.0 : 8.0); S->SetClock(Sn + (Eight - L)); }
				if (const APaxFlow* P = APaxFlow::Find(W)) if (P->Walking() > 0) ChProblem(FString::Printf(TEXT("%d passengers about with no flight planned"), P->Walking()));
				ChOk(TEXT("buy"), TEXT("A320|lease")); ChOk(TEXT("buy"), TEXT("A320|lease"));
				C.A.Empty(); C.B.Empty();
				ChSub(); return true;
			}
			if (C.Sub == 1)
			{
				if (T < 4.0) return true;
				TSharedPtr<FJsonObject> J = ChCall(TEXT("places"), C.A + TEXT("|"));
				const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
				double Best = 1e9;
				if (J && J->TryGetArrayField(TEXT("places"), L)) for (const TSharedPtr<FJsonValue>& V : *L) { const TSharedPtr<FJsonObject> P = V->AsObject(); const double Blk = P->GetNumberField(TEXT("block")); if (P->GetBoolField(TEXT("schengen")) && Blk >= 70.0 && Blk < Best && P->GetNumberField(TEXT("pax")) > 60.0) { Best = Blk; C.To = P->GetStringField(TEXT("icao")); } }
				if (C.To.IsEmpty()) { ChProblem(TEXT("no Schengen place to fly to")); ChEnd(TEXT("no place")); return false; }
				ADP_LOG(TEXT("CHAIN TEST: flying to %s (block %.0f min)"), *C.To, Best);
				const FDateTime L0 = ADPBoards::Local(S->Now());
				auto Round5 = [](const FDateTime& D) { return D + FTimespan::FromMinutes(5 - D.GetMinute() % 5); };
				const FDateTime Da = Round5(L0 + FTimespan::FromMinutes(158.0)), Db = Da + FTimespan::FromMinutes(15.0), Dc = Da + FTimespan::FromMinutes(2.0 * Best + 50.0 + 75.0);
				ChOk(TEXT("slot"), C.A + TEXT("|") + C.To + TEXT("|") + ChHm(Da) + TEXT("|127"));
				ChOk(TEXT("slot"), C.B + TEXT("|") + C.To + TEXT("|") + ChHm(Db) + TEXT("|127"));
				ChOk(TEXT("slot"), C.A + TEXT("|") + C.To + TEXT("|") + ChHm(Dc) + TEXT("|127"));
				if (ChOk(TEXT("slot"), C.A + TEXT("|") + C.To + TEXT("|") + ChHm(Da + FTimespan::FromMinutes(30.0)) + TEXT("|127"), false)) ChProblem(TEXT("two lines of one aircraft at once were accepted"));
				const FString Miss = ADPSchedule::MissingStaff(S->World, S->Now());
				ADP_LOG(TEXT("CHAIN TEST: before hiring the ground staff the game says: %s"), *Miss);
				if (Miss.IsEmpty()) ChProblem(TEXT("nobody hired, and nothing is missing"));
				ChHire(TEXT("captain"), 4); ChHire(TEXT("fo"), 4); ChHire(TEXT("fa"), 12); ChHire(TEXT("ground"), 10); ChHire(TEXT("checkin"), 8); ChHire(TEXT("security"), 6); ChHire(TEXT("mech"), 2); ChHire(TEXT("fuel"), 3); ChHire(TEXT("catering"), 4); ChHire(TEXT("cleaning"), 6);
				const FString Miss2 = ADPSchedule::MissingStaff(S->World, S->Now());
				if (!Miss2.IsEmpty()) ChProblem(TEXT("still missing after hiring: ") + Miss2);
				ChSub(); return true;
			}
			if (C.Sub == 2)
			{
				if (T < 10.0) return true;
				int32 Own = 0; for (const FADPPlanned& P : S->World.Planned) if (P.AirlineId == UADPSim::Me()) { ++Own; if (Own <= 8) ADP_LOG(TEXT("CHAIN TEST: planned %s %s %s-%s %s, target %d"), *P.FlightNo, *P.Reg, *P.From, *P.To, *ADPBoards::Local(P.Sched).ToString(TEXT("%d.%m %H:%M")), P.Pax()); }
				if (Own < 6) ChProblem(FString::Printf(TEXT("only %d flights planned from the timetable"), Own));
				const FADPPlanned* Pa = ChPlan(C.A, false);
				if (!Pa) { ChProblem(TEXT("no departure planned")); ChEnd(TEXT("nothing to fly")); return false; }
				if (Pa->Pax() < 20) ChProblem(FString::Printf(TEXT("only %d passengers will book %s"), Pa->Pax(), *Pa->FlightNo));
				for (const FADPAircraft& X : S->World.Aircraft) if (X.AirlineId == UADPSim::Me()) { ADP_LOG(TEXT("CHAIN TEST: %s stands at %s"), *X.Reg, *X.Stand); if (X.Stand.IsEmpty()) ChProblem(X.Reg + TEXT(" has no stand")); }
				int32 Rows = 0; for (const FADPBoardRow& R : ADPBoards::Rows(W, false, 20)) if (R.AirlineId == UADPSim::Me()) ++Rows;
				if (Rows < 2) ChProblem(FString::Printf(TEXT("%d of your flights on the departures board"), Rows));
				ChNext(2);
				return true;
			}
			return true;
		case 2:
		{
			const FADPPlanned* Pa = ChPlan(C.A, false); const FADPPlanned* Pb = ChPlan(C.B, false);
			if (!Pa || !Pb) { ChProblem(TEXT("the two departures aren't planned")); ChEnd(TEXT("nothing to fly")); return false; }
			ChSay(20.0);
			if (C.Sub == 0)
			{
				// on to two hours before the first one
				if ((Pa->Sched - Sn).GetTotalMinutes() > 124.0) { if (C.Speed != 60.f) ChSpeed(60.f, 1.f); return true; }
				ChSpeed(1.f, 1.f);
				if (Pa->Booked < Pa->Pax()) ChProblem(FString::Printf(TEXT("the booking isn't closed: %d of %d"), Pa->Booked, Pa->Pax()));
				ChSave(2); C.DepA = Pa->Sched; C.DepB = Pb->Sched;
				ChSpeed(5.f, 5.f); ChSub(); return true;
			}
			if (C.Sub == 1)
			{
				const bool bGone = Pa->State == TEXT("gone") && Pb->State == TEXT("gone");
				if (!bGone) { if ((Sn - Pb->Sched).GetTotalMinutes() > 150.0) { ChProblem(FString::Printf(TEXT("not gone 2.5 h after its time: %s (%s), %s (%s)"), *Pa->FlightNo, *Pa->Why, *Pb->FlightNo, *Pb->Why)); ChEnd(TEXT("stuck")); return false; } return true; }
				ChCheckDeparture(Pa->FlightNo); ChCheckDeparture(Pb->FlightNo);
				ChSub(); return true;
			}
			// pushed, taxied, away: both taken off (no moving aircraft of theirs left) - at most 40 minutes
			{
				const FADPFlight* Fa = ChFlight(C.A); const FADPFlight* Fb = ChFlight(C.B);
				const bool bUp = ASimTraffic::Airborne(W, C.A) && ASimTraffic::Airborne(W, C.B);
				if (bUp) { ADP_LOG(TEXT("CHAIN TEST: RESULT both in the air, %.0f and %.0f min late"), Fa ? Fa->DelayMin : -1.0, Fb ? Fb->DelayMin : -1.0); ChNext(3); return true; }
				if (T * C.Speed > 40.0 * 60.0) { ChProblem(TEXT("not in the air 40 min after their doors closed")); ChNext(3); }
			}
			return true;
		}
		case 3:
		{
			const FADPFlight* Fa = ChFlight(C.A);
			const FADPPlanned* Pr = ChPlan(C.A, true);
			ChSay(20.0);
			if (C.Sub == 0)
			{
				// the flight out, the turn-round there, the flight back: the second departure's passengers come two hours before it,
				// and the aircraft is 12 minutes from its stand: whichever is first
				const FADPPlanned* P2 = ChPlan(C.A, false, 1);
				const bool bNear = Fa && Fa->To == Here && (Fa->DepAt + FTimespan::FromMinutes(Fa->DurMin + Fa->HoldMin) - Sn).GetTotalMinutes() < 12.0;
				const bool bComing = P2 && (P2->Sched - Sn).GetTotalMinutes() < 124.0;
				if (!bNear && !bComing)
				{
					if (C.Speed != 60.f) ChSpeed(60.f, 1.f);
					if (T > 1500.0) { ChProblem(FString::Printf(TEXT("the aircraft didn't come back: %s"), Pr ? *Pr->Why : TEXT("no way back planned"))); ChEnd(TEXT("stuck")); return false; }
					return true;
				}
				ChSpeed(1.f, 1.f); ChSave(3); ChSpeed(5.f, 5.f); ChSub(); return true;
			}
			{
				// (a save point 12 minutes before its stand: for a run that starts just before the passengers get off)
				if (!C.Seen.Contains(TEXT("33")) && Fa && Fa->To == Here && (Fa->DepAt + FTimespan::FromMinutes(Fa->DurMin + Fa->HoldMin) - Sn).GetTotalMinutes() < 12.0) { C.Seen.Add(TEXT("33")); ChSave(33); }
				const FADPAircraft* Ac = S->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == C.A; });
				if (!Ac || Ac->Location != Here || Ac->Status == TEXT("flying") || Ac->ArrAt < Sn - FTimespan::FromHours(3.0)) { if (T > 1500.0) { ChProblem(TEXT("not landed")); ChEnd(TEXT("stuck")); return false; } return true; }
				APaxFlow::FCount K;
				const bool bHave = ChCount(Ac->ArrFlight, true, K);
				const bool bOver = bHave && K.Spawned >= K.Walkers && K.Out >= K.Spawned;
				if (!bOver && (Sn - Ac->ArrAt).GetTotalMinutes() < 75.0) return true;
				if (!bHave) ChProblem(FString::Printf(TEXT("nobody came off %s"), *Ac->ArrFlight));
				else
				{
					ADP_LOG(TEXT("CHAIN TEST: RESULT %s arrival: %d aboard, %d walkers: %d off, %d took their own bag, %d at the car park, %.0f min after on-blocks"), *K.Label, K.Pax, K.Walkers, K.Spawned, K.Bag, K.Out, (Sn - Ac->ArrAt).GetTotalMinutes());
					if (K.Spawned < K.Walkers || K.Out < K.Spawned) ChProblem(FString::Printf(TEXT("%s: %d off, %d out of %d; still about: %s"), *K.Label, K.Spawned, K.Out, K.Walkers, *K.Stages));
					const int32 Bags = K.Spawned - K.Spawned / 5; if (K.Bag < Bags) ChProblem(FString::Printf(TEXT("%s: %d of %d got their bag"), *K.Label, K.Bag, Bags));
				}
				ChNext(4);
			}
			return true;
		}
		case 4:
		{
			const FADPPlanned* P2 = ChPlan(C.A, false, 1);
			ChSay(20.0);
			if (!P2) { ChProblem(TEXT("no second departure planned")); ChEnd(TEXT("nothing to fly")); return false; }
			if (P2->State != TEXT("gone")) { if ((Sn - P2->Sched).GetTotalMinutes() > 150.0) { ChProblem(FString::Printf(TEXT("%s not gone 2.5 h after its time (%s)"), *P2->FlightNo, *P2->Why)); ChEnd(TEXT("stuck")); return false; } return true; }
			if (C.Sub == 0) { ChCheckDeparture(P2->FlightNo); ChSub(); return true; }
			if (ASimTraffic::Airborne(W, C.A)) { ADP_LOG(TEXT("CHAIN TEST: RESULT %s in the air again after its turn-round"), *C.A); ChNext(5); ChEnd(TEXT("the whole chain")); return false; }
			if (T * C.Speed > 40.0 * 60.0) { ChProblem(TEXT("not in the air 40 min after its doors closed")); ChEnd(TEXT("stuck")); return false; }
			return true;
		}
		default: ChEnd(TEXT("stopped there")); return false;
		}
	}
}

namespace
{
	/** Swallows every input before any widget sees it. */
	class FNoInput : public IInputProcessor
	{
	public:
		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent&) override { return true; }
		virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent&) override { return true; }
		virtual bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent&) override { return true; }
		virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent&) override { return true; }
		virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent&) override { return true; }
		virtual bool HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent&) override { return true; }
		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication&, const FPointerEvent&) override { return true; }
		virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&, const FPointerEvent&, const FPointerEvent*) override { return true; }
		virtual bool HandleMotionDetectedEvent(FSlateApplication&, const FMotionEvent&) override { return true; }
	};
}

void ADPChainTest::BlockInput()
{
	static bool bOn = false;
	if (bOn || !FSlateApplication::IsInitialized()) return;
	bOn = true;
	FSlateApplication::Get().RegisterInputPreProcessor(MakeShared<FNoInput>(), 0);
	ADP_LOG(TEXT("Test: the keyboard, the mouse and the pad are ignored"));
}

int32 ADPChainTest::StartStage()
{
	int32 N = 0;
	return FParse::Value(FCommandLine::Get(), TEXT("ChainTest="), N) ? (N == 33 ? 3 : FMath::Max(1, N)) : FParse::Param(FCommandLine::Get(), TEXT("ChainTest")) ? 1 : 0;
}

void ADPChainTest::Begin(UWorld* W)
{
	if (GChain || !StartStage()) return;
	GChain = MakeShared<FChain>();
	GChain->World = W; GChain->Stage = StartStage(); GChain->T0 = GChain->SubT0 = FPlatformTime::Seconds();
	ADP_LOG(TEXT("CHAIN TEST: started at stage %d"), GChain->Stage);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.5f);
	TWeakObjectPtr<UWorld> Ww(W);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Ww](float Dt) { if (!Ww.IsValid() || !GChain) return false; ADPChainTest::Camera(Ww.Get(), FMath::Min(Dt, 0.1f)); return true; }), 0.f);   // (the camera director: ChainCam.cpp)
}
