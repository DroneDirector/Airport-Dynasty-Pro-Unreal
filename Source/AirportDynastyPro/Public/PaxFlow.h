// The passengers' trips (owner 2026-11-21: "all the passengers that are going to travel with the airline that I make or
// other airplanes - for now they spawn at the big parking lot behind the terminal - just until the driving mechanism for
// civilian cars and buses is in"). Every step walked and animated, nobody appears or vanishes in sight of a public place:
//   DEPARTING: out of a car in the big landside car park (where the cars would have brought them) -> the pavement, over
//   the forecourt at a zebra crossing -> the main entrance -> the Grand Hall -> the queue at one of their airline's desks
//   (1..20) -> their own bag, carried by its handle, set on the desk's weigh belt (ADepartureBelts: it rides the belts to
//   the make-up carousels) -> a walk-through between the desks -> a security door -> a lane: things on the X-ray belt,
//   through the arch, picked up again -> the airside lobby -> up the stairs, or along the new airside corridor and up the
//   escalators -> Floor 2 -> waiting until their flight is called: on a chair at their own gate, a free chair out on the
//   gate walkway, or leaning on a wall (all with the sitting IK step / the lean) -> the boarding call (the announcer, as
//   the turnaround's "board" task starts) -> at the real boarding rate (Airbus AC 5-2: about 12 a minute at one door) up,
//   to the gate door, through the jetway's rotunda and tunnel to its cab end, aboard (no cabin yet: counted aboard).
//   ARRIVING: as the turnaround's "deplane" starts (the jetway docked) they come off one by one at the cab end (about 20
//   a minute), through the jetway into the gate area, the gate walkway west, down the arrivals escalators into the claim
//   hall, to their flight's claim belt (ABaggageBelts), their own bag taken off when it comes round (tagged with their
//   id), then out through the arrivals doors, over the forecourt at a crossing to the car park, where a car would pick
//   them up.
// The real numbers stay in the simulation (passengers-real-count: never a slider - the schedule decides); of each flight
// a share walks in 3D (up to VisibleDep / VisibleArr), the rest is data. MetaHumans from Content/People/Built (role
// passenger), their walk blend space as AADPPeople's; far away their animation updates only when seen and less often.
// Test: -PaxFlowTest - a departure and an arrival right after the start, so one combined test sees the whole chain both
// ways; WatchPoints() gives the tracking camera its subject (one passenger followed from start to end, then the next).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WalkMap.h"
#include "PaxFlow.generated.h"

class USkeletalMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API APaxFlow : public AActor
{
	GENERATED_BODY()
public:
	APaxFlow();
	/** The manager (made once, when the airport has loaded - AADPPeople::StartWanderers). */
	static APaxFlow* Get(UWorld* W);
	static APaxFlow* Find(const UWorld* W);
	/** True when this flight's departing passengers walk here: the boarding call is made by the passenger flow (at the
	 *  turnaround's board task), not by the announcer's 40-minute rule - never called twice. */
	static bool CallsBoarding(const UWorld* W, const FString& Flight);
	/** Where this passenger flies to (or came from: bArr), from their own flight; false when not known. */
	static bool TripOf(UWorld* W, const AActor* Who, FString& Place, bool& bArr);
	/** How far that flight (by its planned leg) is: -1 nobody walks for it, 0 not called yet, 1 boarding, 2 the final call made,
	 *  3 everyone aboard or it goes without the missing ones (to-do 288: the calls, the boards and the gate agree). */
	static int32 CallsState(const UWorld* W, const FString& PlanId);
	/** May the aircraft Reg close its doors and leave? True when every passenger walking for its departure is aboard (or
	 *  nobody walks for it). A passenger missing: the flight waits up to 15 minutes from the first ask, then goes without
	 *  them (a no-show, logged). The jetway and the push wait on this (SimTraffic.cpp). */
	static bool BoardingDone(const UWorld* W, const FString& Reg);
	/** Is everyone off the aircraft Reg that came on blocks then: the last of its walkers through the gate's door (those without a
	 *  walker: by the turnaround's time). Asked once that time is over; from then at most 15 minutes, then it goes on, logged as a
	 *  problem. Why: who is still inside, in words. */
	static bool DeplaneDone(const UWorld* W, const FString& Reg, const FDateTime& OnBlocks, int32 ArrPax, FString& Why);
	virtual void Tick(float Dt) override;

	/** Tests: what must be in view now - the passenger being followed (feet, head, its bag), and the side to look from.
	 *  False when nobody is walking yet. */
	bool WatchPoints(TArray<FVector>& Pts, FVector& Side) const;
	/** Tests: who is followed and what they are doing (for a caption / the log). */
	FString WatchWhat() const;
	bool IsTest() const { return bTest; }
	/** Tests: both test flights are through (the departure all aboard, the arrival all out at the car park). */
	bool TestDone() const;
	/** How many are walking now, and how many have gone aboard / out so far. */
	int32 Walking() const;
	/** One flight's passengers in numbers (the functional test, the tracking map): its label; false when nobody walks for it. */
	struct FCount { FString Label; bool bArr = false; int32 Pax = 0, Walkers = 0, Spawned = 0, Bags = 0, Desk = 0, Lane = 0, F2 = 0, Aboard = 0, Bag = 0, Out = 0, Late = 0, Home = 0, About = 0; bool bCalled = false, bOff = false; float CallBefore = -1.f; int32 Jetway = 0, Jet = -1; FString Stages; };   // (CallBefore: the boarding call, minutes before its departure as expected then)
	void Counts(TArray<FCount>& Out) const;
	float Floor2Z() const { return F2Z; }
	int32 StuckInJetways() const { return StuckJet; }
	int32 StuckElsewhere() const { return StuckElse; }
	/** What is going on, for the tests' camera director (ChainCam.cpp): who, where, which step (Kind: 0 walking in, 1 the desk, 2 security,
	 *  3 up, 4 waiting at the gate, 5 boarding, 6 off the aircraft, 7 down to the claim, 8 the belt, 9 walking out; 20+ aircraft), News: how
	 *  lately it began. Jet: in that jetway. */
	struct FScene { FString Id, Line; FVector At = FVector::ZeroVector, Face = FVector(1, 0, 0); int32 Kind = 0, Jet = -1; double News = 0.0; TWeakObjectPtr<const AActor> Who; };
	void Scenes(TArray<FScene>& Out) const;
	/** The measured inside of a docked jetway (the latest flight at it): its floor's middle, from the aircraft's door to the gate's. */
	void JetwayLine(int32 Jet, TArray<FVector>& Out) const;

	enum : int32 { VisibleDep = 36, VisibleArr = 30 };

private:
	enum class EPx : uint8 { Walk, DeskQ, Desk, LaneQ, Wait, Seated, Lean, Call, Jetway, Apron, ClaimWait, ToBag, Revolve, Gone };   // (Revolve: at / in a revolving door - waiting, stepping in, riding round, stepping out)
	enum class EDo : uint8 { None, JoinDesk, JoinLane, Screened, Upstairs, AtSeat, AtWall, AtGateSpot, Aboard, AtClaim, OutCarPark, ArrIn, ToTaxi, ApronOut };
	struct FWp { FVector P = FVector::ZeroVector; float Pause = 0.f; FVector Face = FVector::ZeroVector; uint8 Mode = 0; bool bGround = false; bool bNav = false; int8 Rev = -1, RevSide = 0; };   // Mode 0 walk, 1 slow, 2 ride, 3 a revolving door (Rev: ATerminalDoors' revolving door, RevSide: the side they come from, +-1 along its axis; P: the queue's head spot); bNav: the leg to it already walked out on the navmesh
	struct FPx
	{
		TWeakObjectPtr<AActor> A; TWeakObjectPtr<USkeletalMeshComponent> Body; USceneComponent* Holder = nullptr;
		int32 Run = 0; FString Id, Name; bool bArr = false;
		EPx St = EPx::Walk; EDo Do = EDo::None;
		TArray<FWp> Way; int32 Wi = 0; float PauseT = 0.f, Speed = 0.f, Pace = 130.f, Acc = 0.f, T = 0.f;
		FVector PrevLoc = FVector::ZeroVector; double PrevTime = -1.0; float MoveDir = 0.f, MoveSpd = 0.f; bool bRideNow = false;   // (the real motion: legs follow it)
		float TurnYaw = 0.f, YieldT = 0.f; FVector YieldOff = FVector::ZeroVector, YieldWant = FVector::ZeroVector;   // (passing people: the shoulder turn, the step aside)
		FVector2D Jit = FVector2D::ZeroVector;
		int32 Bag = INDEX_NONE; bool bBagDown = false, bHasBag = false, bArrBag = false; FVector Handle = FVector::ZeroVector;
		int32 Desk = 0, Lane = -1, Seat = INDEX_NONE, Order = 0;
		float LeanYaw = 0.f, Fidget = 0.f; double BoardAt = -1.0; bool bEsc = false, bPriority = false;
		FString Stage;   // (in words, for the tests' caption)
		float DevIn = -1.f, DevFor = 0.f; uint8 Dev = 0;   // (waiting at the gate: the phone / tablet out - APaxProps; in how long, for how long, which)
		int32 Rev = INDEX_NONE, RevComp = INDEX_NONE, RevPhase = 0, RevSide = 0; float RevT = 0.f; FVector RevFrom = FVector::ZeroVector; TArray<FWp> RevAfter;   // (a revolving door: which, the compartment, 0 waiting 1 stepping in 2 riding 3 stepping out; the way on after it)
		int32 TunA = 0, TunB = 0;    // (the way points inside the jetway: [TunA, TunB) - held within its walls, APaxFlow::FSlice)
		FVector StuckAt = FVector::ZeroVector; float StuckT = 0.f, ReplanT = 0.f; bool bQueued = false, bDoorWait = false; FString HeldBy;   // (not getting on: where, how long; behind someone - a queue, not stuck; what held them)
		FVector LooseFrom = FVector::ZeroVector, LooseTo = FVector::ZeroVector; float LooseT = -1.f;       // (truly held by geometry: eased back to the nearest free point)
		bool bHome = false;          // (on the way home: too late, or their flight is off)
		float FadeT = -1.f; int32 FadeFrom = INDEX_NONE; bool bFadeSwap = false; TWeakObjectPtr<class UMaterialInstanceDynamic> FadeMid;   // (boarding: faded away halfway along the jetway)
	};
	/** The inside of a docked jetway as it stands now, cut every 40 cm from the aircraft's door to the gate: its middle on the floor,
	 *  the way across, and how far to each side a body's middle may be (the walls and whatever stands in, less a body). */
	struct FSlice { FVector C = FVector::ZeroVector, Rt = FVector(0, 1, 0); float Lim = 0.f; };
	struct FRun
	{
		TArray<FSlice> Walk; bool bMeasured = false;
		FString Key, Label, AirlineId, Reg, PlanId;   // (PlanId: its planned leg - Sim/ADPSchedule.h)
		bool bArr = false, bTest = false, bPlayer = false, bDockNoted = false;
		int32 Gate = 2, Jet = INDEX_NONE;
		double DepT = 0.0;          // departure (seconds on the run's clock: the simulation's, the test's own)
		int32 Pax = 0, Visible = 0, Spawned = 0, Aboard = 0, Out = 0, Late = 0, Home = 0;   // (Late: too late to check in; Home: gone home - late, or the flight is off)
		FString Stand; uint8 Way = 0;   // (its stand; EADPPaxWay: jetway, by foot, by bus)
		bool bOff = false, bGone = false;   // (cancelled; left)
		int32 NBags = 0, NDesk = 0, NLane = 0, NF2 = 0, NBag = 0;   // (how many have a bag to check in, have checked it in, been screened, come up to Floor 2; taken their bag)
		TArray<double> SpawnAt;     // (departures: when each walker gets out of their car; arrivals: off the aircraft)
		TArray<int32> Desks;
		bool bCalled = false, bFinal = false, bClosed = false, bAllLogged = false, bDeplaning = false, bBagsTagged = false;
		float CallBefore = -1.f;
		double CallT = 0.0, BoardSpanS = 0.0, ArrStart = -1.0, NextArr = 0.0, HoldSince = -1.0, SaidAt = -1.0e9; bool bNoShowGo = false, bLateTun = false;   // (arrivals: HoldSince, bNoShowGo - the wait for the last ones off, given up)
		int32 InTunnel = 0, AtDoor = 0; float DoorT = 0.f, DoorSaid = 30.f;   // (counted four times a second: in the jetway or on their way through the gate's door; waiting at that door shut)
		float DockWait = 0.f;
	};
	struct FGate { int32 N = 0; FString Room; FBox2D Box; FVector2f Jw = FVector2f::ZeroVector; int32 Place = INDEX_NONE; bool bF2 = true; };
	struct FLine { TArray<int32> Who; };

	TArray<FPx> Pax; TArray<int32> FreePx;
	TArray<FRun> Runs;
	TArray<FGate> Gates;
	FLine DeskLines[20], LaneLines[6];
	TArray<FLine> RevLines;   // (the short queues outside the revolving doors' openings: door x 2 + side)
	FWalkMap Map;
	TArray<FString> Names;
	FString NextFace; TMap<FString, TSharedPtr<struct FStreamableHandle>> FaceLoads; double NextFaceAt = 0.0;   // (the next passenger's body, loaded in the background before they come)
	bool FaceReady();
	TArray<TPair<FVector2f, float>> NoLean;   // (plan points with their keep-out radius: doors, stairs, escalators)
	TArray<FVector> LeanSpots;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Holders;

	bool bReady = false, bTest = false, bTestMade = false;
	TWeakObjectPtr<AActor> DoorsActor, SeatsActor;   // (ignored by the walkers' wall look: doors open for them, chairs are sat on)
	int32 NavBudget = 0; float NavSweepIn = 0.f; TSet<uint64> NoWayLogged;   // (the navmesh ways: a few new legs a frame; moving things kept off it; each missing way logged once)
	float ReadyIn = 2.f, PollIn = 0.f, SayIn = 20.f, SpawnGap = 0.f, StuckLog = 0.f;
	double RealT = 0.0, SimT = 0.0;
	float F2Z = 600.f, F2Y = 6.f;
	int32 StuckJet = 0, StuckElse = 0;   // (how often someone was truly held, in a jetway / elsewhere: each a fault to be found)
	int32 StaffDesk = 99, StaffLane = 99, StaffGate = 99;   // (your passenger service agents / security officers / gate agents on shift)
	int32 Frame = 0, Watched = INDEX_NONE, TotalAboard = 0, TotalOut = 0;
	int32 PhoneWho = INDEX_NONE, PhoneReturn = INDEX_NONE; float PhoneShowIn = -1.f; bool bPhoneShown = false;   // (tests: the camera on someone using a phone at the gate, a while)
	float HallIn = 0.f, HallQuiet = 99.f;   // (passengers on the Grand Hall's ground floor: next count, how long there were none - the security doors)
	bool bRevShown = false; int32 RevReturn = INDEX_NONE; float RevReturnIn = -1.f;   // (tests: the camera shows one revolving door ride, then goes back)
	FRandomStream Rng;

	// setting up and the flights
	bool Prepare();
	void Poll();
	void MakeTestRuns();
	double Clock(const FRun& R) const { return R.bTest ? RealT : SimT; }
	int32 GateRoom(int32 Gate) const;
	int32 WaitRoom(int32 Gate) const { return GateRoom(Gate >= 5 && Gate <= 8 ? Gate - 4 : Gate); }   // (a ground gate's: the room over it, down its stairs when called)
	// ---- by foot or by bus to the aircraft and up the passenger stairs (PaxApron.cpp, to-do 277)
	TMap<int32, TSharedPtr<struct FPaxOut>> Outs;      // (by passenger)
	TMap<FString, TSharedPtr<struct FRunOut>> RunOuts; // (by FRun::Key)
	static int32 PaxGate(const FString& Stand, const FString& Reg);
	void SetStand(FRun& R, const FString& Stand);
	bool ApronCall(int32 i);
	void ApronArrived(int32 i);
	void StepApron(int32 i, float Dt);
	void ApronTick(float Dt);
	struct FPaxOut* ApronNew(int32 i);
	void ApronPose(int32 i, const struct FDriverPose& W);
	void ApronUnpose(int32 i);
	void ApronBus(int32 Ri, struct FRunOut& O, float Dt);
	void ApronGone(int32 i);
	int32 ApronCount(int32 Ri, int32 From, int32 To, int32 Door = -1) const;
	bool ApronStairs(struct FRunOut& O) const;
	FVector ApronQueue(const struct FRunOut& O, int32 k) const;
	FVector ApronDoorSpot(const struct FRunOut& O, int32 Door, float Out) const;
	class AGSE* ApronG = nullptr;
	void ApronWalk(FPx& P, const FVector& To);
	void ApronBack(int32 i);
	// (arriving)
	int32 ApronInside(int32 Ri) const;
	bool ApronCan(int32 Ri, FString& Why) const; bool ApronHolds(int32 Ri) const;
	bool ApronMay(int32 Ri, bool bAgain = false);
	int32 ApronOff(int32 i);
	void ApronIn(int32 i);
	int32 JetwayFor(int32 GateIx) const;
	void ChooseDesks(FRun& R);
	void StepRun(int32 Ri, float Dt);
	void CallBoarding(int32 Ri);

	// people
	int32 SpawnDep(int32 Ri);
	int32 SpawnArr(int32 Ri);
	int32 NewPx();
	AActor* Body(const FVector& At, float Yaw, FPx& P);
	void StepPx(int32 i, float Dt);
	bool Follow(FPx& P, AActor* A, float Dt);
	void Idle(FPx& P, AActor* A, float Dt, const FVector* Face = nullptr);
	void Anim(FPx& P);
	void Arrive(int32 i);
	void Finish(int32 i, const TCHAR* Why, bool bKeepBody = false);   // (bKeepBody: handed over alive - the taxi bus, AGSE::TaxiBoard)
	void CarryBag(FPx& P, AActor* A);
	void StartFade(FPx& P, AActor* A);
	// the revolving doors (ATerminalDoors): waiting for an empty compartment, the ride round, out at the far side
	class ATerminalDoors* Doors() const;
	FLine& RevLine(int32 Rev, int32 Side);
	void StartRevolve(int32 i);
	void StepRevolve(int32 i, float Dt);
	void EndRevolve(FPx& P, bool bThrough);
	bool StepFade(FPx& P, AActor* A, float Dt);

	// ways (world points)
	void WayToHall(FPx& P, const FVector& From, int32 RevDoor = INDEX_NONE);
	void WayFromHall(FPx& P, const FVector& From);
	bool OutToTaxi(FPx& P);   // (to-do 239b piece 4: instead of WayFromHall, now and then - the taxi bus at the kerb)
	void WayToDesk(FPx& P, int32 Spot);
	void WayToSecurity(FPx& P);
	void WayScreening(FPx& P);
	void WayUpstairs(FPx& P);
	bool WayToWait(FPx& P);
	bool ReSeat(FPx& P);
	void WayToJetway(FPx& P);
	void WayArrivals(FPx& P);
	/** Measures the jetway's inside (once docked; traces across it against the real parts); false: no jetway there. */
	bool Measure(FRun& R);
	int32 SliceAt(const FRun& R, const FVector& At) const;
	bool PastDoor(const FPx& P) const;            // (beyond the gate's jetway door)
	int32 Inside(int32 Ri) const;                 // (an arrival's walkers not yet through the gate's door, the ones still aboard with them)
	void GateDoors(float Dt);                     // (which gates' jetway doors stand open for passengers now - to-do 234)
	bool PlaneAtJetway(const class AJetwayRigs* J, int32 Jet, const FString& Reg) const;   // (the flight's aircraft at that jetway's stand)
	float GateDoorsIn = 0.f;
	void GoHome(int32 i, const TCHAR* Why);
	void Held(int32 i, float Dt);
	void AddPlanWay(FPx& P, int32 FromPlace, const FVector& FromPlan, int32 ToPlace, const FVector& ToPlan, bool bJit = true);
	void Crossing(FPx& P, bool bToTerminal, float NearX);
	FVector DeskSpot(int32 Desk, int32 K) const;
	FVector LaneSpot(int32 Lane, int32 K) const;
	bool WallSpot(const FPx& P, const FBox2D& Region, FVector& Out, float& Yaw);
	/** A road, pavement or car park over that point (up to a person's height) outside: its height there (to-do 287). */
	bool RoadAbove(const FVector& Feet, float& Z) const;
	FVector CarParkSpot();
	FVector PlanOf(const FVector& W) const { return FVector(W.X / 100.f, W.Z / 100.f, -W.Y / 100.f); }
	static FVector PW(float X, float H, float Z);
	float GroundZ(float PlanX, float PlanZ) const;
	FVector Jit(const FPx& P, const FVector& W) const { return W + FVector(P.Jit.X, P.Jit.Y, 0.f); }
};
