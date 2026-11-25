// The bag vehicles and your hired people (to-do 206 part 2b, docs/Hired-Crew-Plan.md; StaffPeople.h, TugStaff.cpp): with
// adp.HiredCrew on, your own airline's bags are worked by the ground crew you have hired - the tractor's driver, the belt
// loader's operator, and whoever no waiting job needs as a second handler and up at the hold. A bag job is asked for in the
// same queue as the pushbacks and starts with its whole smallest team (driver + operator, so no driver ever waits for an
// operator another driver waits for) or waits; the flight waits with it and says who is missing. The other airlines'
// flights keep the stand-ins. A vehicle's helpers are of the same people as its driver.
#include "GSE.h"
#include "Tugs.h"
#include "StaffPeople.h"
#include "Turnaround.h"
#include "DepartureBelts.h"
#include "ADPLog.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSchedule.h"
#include "Engine/World.h"

bool AGSE::HiredFlight(const FString& Key, float StandX, FDateTime* When, float* Stand) const
{
	const UADPSim* Sim = UADPSim::Get(GetWorld());
	const UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
	if (bTest || !Sim || !Sim->IsRunning() || !Sp || !Sp->Hires()) return false;
	const ADPTurnaround::FTurn* T = Key.IsEmpty() ? nullptr : ADPTurnaround::ForFlight(Key);
	if (!T && StandX != 0.f) T = ADPTurnaround::AtStand(StandX);
	if (T) { if (When) *When = T->OffBlocks; if (Stand) *Stand = T->StandX; return T->bMine; }
	if (Key.IsEmpty()) return false;
	// (its aircraft is not here yet)
	const FADPPlanned* P = Sim->World.Planned.FindByPredicate([&](const FADPPlanned& X) { return X.FlightNo == Key && X.State == TEXT("planned"); });
	if (P && When) *When = P->Est;
	return P && P->AirlineId == UADPSim::Me();
}

bool AGSE::IsHired(int32 Ci) const { const ATugs* G = Tg(); return G && G->Crew.IsValidIndex(Ci) && G->Crew[Ci].bHired; }
bool AGSE::DriverLost(const FTractor& T) const { const ATugs* G = Tg(); return IsHired(T.Driver) && (!G->Crew[T.Driver].A.IsValid() || G->Crew[T.Driver].bRelief); }

bool AGSE::FindTrain(int32& Ti, int32& A, int32& B) const
{
	// (as StartFor: a free tractor, two free carts coupled-ready, the front one's eye clear)
	const FVector Eye(201.f, 0.f, 40.f), Jaw(-127.f, 0.f, 40.f);
	Ti = INDEX_NONE;
	for (int32 t = 0; t < Tractors.Num() && Ti == INDEX_NONE; ++t) if (TractorFree(Tractors[t])) Ti = t;
	if (Ti == INDEX_NONE) return false;
	for (A = 0; A < Carts.Num(); ++A)
	{
		if (!CartFree(A)) continue;
		bool bFronted = false; for (int32 o = 0; o < Carts.Num(); ++o) if (o != A && FVector::Dist2D(CW(Carts[A], Eye), CW(Carts[o], Jaw)) < 30.f) bFronted = true;
		if (bFronted) continue;
		for (B = 0; B < Carts.Num(); ++B) if (A != B && CartFree(B) && FVector::Dist2D(CW(Carts[B], Eye), CW(Carts[A], Jaw)) < 30.f) return true;
	}
	return false;
}

void AGSE::AddWant(EWant Kind, const FString& Key, float StandX)
{
	for (FWant& W : Wants) if (W.Kind == Kind && W.Key == Key) { if (StandX != 0.f) W.StandX = StandX; return; }
	FWant W; W.Kind = Kind; W.Key = Key; W.StandX = StandX; Wants.Add(W);
	if (Kind == EWant::Dep) if (ADepartureBelts* D = ADepartureBelts::Get(GetWorld())) D->NoAutoClear.Add(Key);   // (its bags wait for your people)
}

FString AGSE::WantKey(const FWant& W) const { return FString(W.Kind == EWant::Arr ? TEXT("arr:") : W.Kind == EWant::Dep ? TEXT("bags:") : TEXT("loader:")) + W.Key; }

bool AGSE::TakeTeam(const TArray<UStaffPeople::FTaken>& Team)
{
	ATugs* G = Tg(); PreDriver = PreLoader = INDEX_NONE;
	bool bAll = G != nullptr;
	for (const UStaffPeople::FTaken& K : Team)
	{
		const int32 Ci = G ? G->Enlist(K.A.Get(), K.Id, UStaffPeople::DutyName(K.Duty)) : INDEX_NONE;
		if (Ci == INDEX_NONE) { bAll = false; continue; }
		G->Crew[Ci].bGse = true;
		(K.Duty == EStaffDuty::LoaderOperator ? PreLoader : PreDriver) = Ci;
	}
	if (!bAll) DropPre();   // (no room in the list: never half a team)
	return bAll;
}

void AGSE::DropPre()
{
	if (ATugs* G = Tg()) for (const int32 Ci : {PreDriver, PreLoader}) if (Ci != INDEX_NONE) G->Discharge(Ci);
	PreDriver = PreLoader = INDEX_NONE;
}

void AGSE::WantTick(float Dt)
{
	if ((WantClock += Dt) < 2.f) return;
	WantClock = 0.f;
	UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); if (!Sp || !Wants.Num()) return;
	for (int32 k = 0; k < Wants.Num(); ++k)
	{
		FWant& W = Wants[k];
		W.Why.Reset();
		FDateTime When; float Stand = 0.f;
		const bool bMine = HiredFlight(W.Kind == EWant::Arr ? FString() : W.Key, W.StandX, &When, &Stand);
		if (Stand != 0.f) W.StandX = Stand;
		bool bOff = !bMine || (W.Kind == EWant::Dep && !ADPTurnaround::ForFlight(W.Key));
		// (a loader alone: only while one of your trains is for that stand)
		if (W.Kind == EWant::Loader) bOff = bOff || LoaderAt(W.StandX) != INDEX_NONE || !Tractors.ContainsByPredicate([&](const FTractor& T) { return T.IsTrain() && T.Job != EJob::None && IsHired(T.Driver) && FMath::Abs(T.StandX - W.StandX) < 1.f; });
		if (bOff)
		{
			if (W.Kind != EWant::Loader)
			{
				ADP_LOG(TEXT("GSE: %s's %s off the list of what your people do - the flight is gone, or you employ too few"), *W.Key, W.Kind == EWant::Arr ? TEXT("arriving bags are") : TEXT("bags are"));
				if (W.Kind == EWant::Dep && !MadeUp.Contains(W.Key)) if (ADepartureBelts* D = ADepartureBelts::Get(GetWorld())) D->NoAutoClear.Remove(W.Key);
			}
			Wants.RemoveAt(k--); continue;
		}
		int32 Ti = INDEX_NONE, A = INDEX_NONE, B = INDEX_NONE;
		if (W.Kind != EWant::Loader && !FindTrain(Ti, A, B)) { W.Why = Tractors.ContainsByPredicate([](const FTractor& T) { return T.IsTrain() && (T.Player.IsValid() || (T.Job == EJob::None && T.Train.Num())); }) ? TEXT("you have the baggage tractor or its carts, no other is free") : TEXT("no baggage tractor and carts free"); continue; }
		int32 Li = LoaderAt(W.StandX); const bool bNeedLoader = Li == INDEX_NONE;
		if (bNeedLoader) for (int32 i = 0; i < Tractors.Num() && Li == INDEX_NONE; ++i) if (LoaderFree(Tractors[i])) Li = i;
		if (Li == INDEX_NONE) { W.Why = TEXT("no belt loader free"); continue; }
		const EStaffTask Task = W.Kind == EWant::Loader ? EStaffTask::Loader : bNeedLoader ? EStaffTask::Bags : W.Kind == EWant::Arr ? EStaffTask::BagsIn : EStaffTask::BagsOut;
		// (ahead of the same flight's pushback in the queue: the bags come first, a loader its train waits for before all)
		const FString What = FString::Printf(TEXT("%s %s"), *W.Key, W.Kind == EWant::Loader ? TEXT("belt loader") : W.Kind == EWant::Arr ? TEXT("arriving bags") : TEXT("bags"));
		TArray<UStaffPeople::FTaken> Team;
		if (!Sp->AskTeam(WantKey(W), Task, When - FTimespan::FromMinutes(W.Kind == EWant::Loader ? 60.0 : 1.0), Tractors[W.Kind == EWant::Loader ? Li : Ti].At, What, Team))
		{ W.Why = Sp->WaitsFor(WantKey(W)); if (W.Why.IsEmpty()) W.Why = TEXT("its people are being called"); continue; }
		if (!TakeTeam(Team)) { W.Why = TEXT("no place for more people"); continue; }
		const FWant X = W; Wants.RemoveAt(k--);
		if (PreLoader != INDEX_NONE) EnsureLoader(X.StandX, X.Key);
		if (PreDriver != INDEX_NONE)
		{
			if (X.Kind == EWant::Arr) StartArrival(Ti, {A, B}, X.StandX, X.Key);
			else { FTractor& T = Tractors[Ti]; T.Key = X.Key; T.StandX = X.StandX; BagKey.Add(Ti, X.Key); StartTrain(Ti, {A, B}); }
		}
		DropPre();   // (both are on the way to their vehicles now; anyone left over is free again)
	}
}

bool AGSE::HiredWaits(const FString& Flight, float StandX, FString& Why) const
{
	for (const FWant& W : Wants)
		if (W.Key == Flight || (W.StandX != 0.f && FMath::Abs(W.StandX - StandX) < 1.f))
		{ Why = (W.Kind == EWant::Arr ? TEXT("the arriving bags wait: ") : TEXT("its bags wait: ")) + (W.Why.IsEmpty() ? FString(TEXT("your people are being called")) : W.Why); return true; }
	if (SvcWaits(Flight, StandX, Why)) return true;
	// (your people are getting its train ready: the flight's bags are not left behind - 25 minutes at most)
	for (const FTractor& T : Tractors)
		if (T.IsTrain() && IsHired(T.Driver) && T.JobT < 1500.f && FMath::Abs(T.StandX - StandX) < 1.f && (T.Job == EJob::Boarding || T.Job == EJob::ToCarts || T.Job == EJob::Coupling))
		{ Why = TEXT("its bag train is being got ready"); return true; }
	return false;
}

bool AGSE::WaitingAt(float StandX) const { return Wants.ContainsByPredicate([&](const FWant& W) { return W.StandX != 0.f && FMath::Abs(W.StandX - StandX) < 1.f; }); }

void AGSE::HiredWay(int32 Ci, const FVector& Near, float StandX)
{
	// from anywhere on the apron: in to the terminal's front and along it (TugStaff.cpp SafeWay), then to the Ramp services 13
	// passage, the vehicle, or the aircraft's right side - in behind the wing, outside its tip
	ATugs* G = Tg(); if (!G || !G->Crew.IsValidIndex(Ci) || !G->Crew[Ci].A.IsValid()) return;
	ATugs::FCrew& C = G->Crew[Ci];
	if (DepotAt(Near)) { DepotGo(Ci, Near); return; }   // (the vehicle area: by the bus road's leg, GSESpare.cpp)
	const bool bFar = Near.Y < -24000.f;
	const FVector Me = DepotBack(Ci, bFar) ? Plan(-247.6f, bFar ? 331.f : 85.45f) : C.A->GetActorLocation();
	const float Nz = -Near.Y / 100.f;
	auto Walk = [&](const FVector& P, float Stop) { ATugs::FCrewAct A; A.Kind = ATugs::FCrewAct::Walk; A.To = [P] { return P; }; A.Stop = Stop; A.Max = 160.f; C.Q.Add(A); };
	if (Nz > 96.f && StandX != 0.f)
	{
		float S = 0.f, N = 0.f, Sp = 36.f;
		if (FVector::Dist2D(Me, Near) < 1500.f || !StandGeo(StandX, S, N, Sp)) return;
		G->SafeWay(C, S);
		Walk(PlanT(S, S - Sp * 0.5f - 4.f, N + 21.f), 60.f);
		return;
	}
	if (-Me.Y / 100.f < 83.f) return;   // (inside already)
	const float X = Nz < 82.6f ? 272.5f : Near.X / 100.f;
	if (FMath::Abs(Me.X / 100.f - X) < 8.f) return;
	Walk(Plan(Me.X / 100.f, 85.45f), 35.f);
	Walk(Plan(X, 85.45f), 35.f);
}

int32 AGSE::BorrowHired(const FVector& Near, int32 Boss, EStaffDuty Duty)
{
	// a second pair of hands: whoever no waiting job needs
	ATugs* G = Tg(); UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
	if (!G || !Sp || !Tractors.IsValidIndex(Boss)) return INDEX_NONE;
	const FTractor& T = Tractors[Boss];
	UStaffPeople::FTaken K;
	if (!Sp->AskExtra(Duty, Near, T.Key + TEXT(" bags"), K)) { ADP_LOG(TEXT("GSE: no %s free for %s - goes on without"), UStaffPeople::DutyName(Duty), *T.Key); return INDEX_NONE; }
	const int32 Ci = G->Enlist(K.A.Get(), K.Id, UStaffPeople::DutyName(Duty));
	if (Ci == INDEX_NONE) return Ci;
	G->Crew[Ci].bGse = true;
	ADP_LOG(TEXT("GSE: %s (%s) comes for %s's bags"), *G->Crew[Ci].Name, UStaffPeople::DutyName(Duty), *T.Key);
	HiredWay(Ci, Near, T.StandX);
	return Ci;
}

void AGSE::GiveBackHired(int32 Ci, float StandX)
{
	ATugs* G = Tg(); if (!G || !G->Crew.IsValidIndex(Ci)) return;
	ATugs::FCrew& C = G->Crew[Ci];
	float S = 0.f, N = 0.f, Sp = 36.f;
	if (DepotBack(Ci)) { G->LeaveAct(C); return; }   // (from the vehicle area back to the terminal's front first)
	if (StandX != 0.f && StandGeo(StandX, S, N, Sp))
	{
		// out past the wingtip, then up the stand's crew side (the same way they came)
		const float X = S - Sp * 0.5f - 4.f;
		WalkTo(Ci, [this, S, X, N] { return PlanT(S, X, N + 21.f); }, 60.f);
		WalkTo(Ci, [this, G, S, X] { return G->GateT(S, X, 104.f); }, 100.f);
	}
	else if (C.A.IsValid() && -C.A->GetActorLocation().Y / 100.f < 83.f) WalkTo(Ci, [this] { return Plan(271.6f, 88.f); }, 40.f);   // (never left in the building: out by Ramp services 13)
	G->LeaveAct(C);
}

bool AGSE::Relieve(int32 Ci, bool& bDrop)
{
	// somebody of a bag job's people has vanished (their body is gone). True: one of these vehicles' people (bDrop: the job
	// goes on without them - the second pairs of hands)
	bDrop = false;
	if (SvcLost(Ci, bDrop)) return true;
	for (int32 Ti = 0; Ti < Tractors.Num(); ++Ti)
	{
		FTractor& T = Tractors[Ti];
		if (T.Driver == Ci) return true;   // (the vehicle stands until somebody takes over: Drive)
		if (T.bLoader && T.TopCrew == Ci) { T.TopCrew = INDEX_NONE; T.bTopUp = false; bDrop = true; return true; }
	}
	for (const TPair<int32, int32>& H : Helper)
		if (H.Value == Ci)
		{
			HelperDone.Add(H.Key); bDrop = true;
			const float Sx = Tractors[H.Key].StandX;
			const int32 Li = Sx != 0.f ? LoaderAt(Sx) : INDEX_NONE;
			if (Li != INDEX_NONE) { Tractors[Li].bBottomHeld = false; for (FTractor::FBeltBag& B : Tractors[Li].BeltBags) if (B.Stage == 6) B.Stage = 4; }
			return true;
		}
	return false;
}

void AGSE::LeftBehind(int32 Ti)
{
	// (a helper lost on the way: what was theirs to carry goes back to the baggage block with the train)
	int32 N = 0; ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
	for (const int32 c : Tractors[Ti].Train) for (int32& b : Carts[c].Bags) if (b != INDEX_NONE) { if (D) D->RemoveBag(b); b = INDEX_NONE; ++N; }
	if (N) ADP_WARN(TEXT("GSE: %d bags of %s stay behind - the handler who carried them is gone"), N, *Tractors[Ti].Key);
}
