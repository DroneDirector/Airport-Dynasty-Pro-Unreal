// The town coach hub (to-do 373 step B, owner 2026-11-27): departing passengers come from town by coach.
//  - PaxFlow makes a departing passenger at the hub's shelter (Data/Sites/<site>/roads.json, road TOWN-HUB: "shelter",
//    "bays"), unseen, instead of at the airport (Take); they wait along the shelter
//  - the timetable (owner 2026-11-27): each passenger is booked on one coach run worked back from their flight - at the
//    terminal adp.TownHubBufferMin before it (120), a 7 min drive and 5 min margin before that; each run a small random
//    delay and a traffic speed. They appear 10-18 min before their run leaves and must be at its bay before it shuts; who
//    misses it is logged (why) and rebooked on a later run in time, or goes home unseen
//  - a coach at its terminal stand gets its driver (the coach operator's: appears unseen by the door, climbs in with the
//    riders' IK), drives the one-way loop (ACoaches::SendOnRoute) to a free hub bay ~16 min before its run leaves
//  - at the hub: the driver gets out, opens the door-side luggage bays, the passengers put their own bags in and board
//    (VehicleRiders: same doors, steps, seats, sitting IK), the driver shuts the bays and climbs back in
//  - it leaves at its time (or once all booked are on) and drives the loop back into its own terminal stand (real time)
//  - at the terminal: the driver opens the bays, everyone gets off and takes their bag, then PaxFlow takes them on
//    exactly as before (APaxFlow::FromCoach: the check-in bag, the way in, desks, security, gates)
//  - TAXIS: not driving yet - TaxiFor() is the one hook (returns none); when the taxi drives, waiters can be given one there
//  - the one switch: adp.TownHub (1 default). Off, or the hub's roads / loop missing, or no coach that can drive: the old
//    airport spawn (logged once, with the reason)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TownHub.generated.h"

class ACoaches;
class UVehicleRiders;

UCLASS()
class AIRPORTDYNASTYPRO_API ATownHub : public AActor
{
	GENERATED_BODY()
public:
	ATownHub();
	static ATownHub* Get(UWorld* W);
	/** May departing passengers come from the hub now? (the switch, the hub's data, a coach that can drive; logged once when not) */
	static bool Use(UWorld* W);
	/** A free place along the shelter nobody sees (world, on the ground). False: none now (try again shortly). */
	bool SpawnSpot(FVector& At);
	/** The timetable (owner 2026-11-27): books one passenger of Flight (departing at DepT, PaxFlow's sim clock) onto a coach
	 *  run worked back from it - the run reaches the terminal adp.TownHubBufferMin before (120 default), leaving the hub with a
	 *  margin for a late start and traffic. SpawnT: when they appear at the shelter (early enough to walk to the bay and board).
	 *  INDEX_NONE (logged): no run can make it any more. */
	int32 Book(const FString& Flight, double DepT, bool bBag, double Now, double& SpawnT);
	/** The run's latest moment to appear and still make it (sim s). */
	double LatestSpawn(int32 Run) const;
	/** Not at the shelter before this (sim s): a run moved later with its delayed flight. */
	double EarliestSpawn(int32 Run) const;
	/** Step C: an arriving passenger of Flight (out of the terminal) booked onto the next town coach run from the terminal
	 *  (it leaves 8-14 min after its first booking, sooner when full). INDEX_NONE: none. */
	int32 BookArrival(const FString& Flight, bool bBag);
	/** PaxFlow hands them over (St Coach, standing where they left the hall): they walk to the coach when it boards. */
	void TakeArrival(AActor* Who, bool bBag, const FString& Flight, int32 Run);
	/** A passenger PaxFlow just made at SpawnSpot for their booked run: they wait for its coach. */
	void Take(AActor* Who, bool bBag, const FString& Flight, int32 Run, double DepT);
	/** A booked one who will never come (PaxFlow could not make them in time): logged, the seat given back. */
	void Unbook(int32 Run, bool bBag, const FString& Why);
	/** The arrival buffer before departure, minutes (adp.TownHubBufferMin). */
	static float BufferMin();
	/** Tests (PerfViews "coach out ... pax"): coach Ci, standing out on the way in, filled with N seated passengers (made out of
	 *  sight, a few each think; bags in its door-side bays while they fit). Its next drive to its terminal stand is their run:
	 *  parked square, its driver out and the door open, they get off one by one, take their bags and go into the Grand Hall
	 *  (PaxFlow::FromCoach); the door shut once the last one is off. */
	bool TestFill(int32 Ci, int32 N);
	/** A test-filled coach: how many are off so far and how many still aboard. False: not one, or not parked yet. */
	bool Unloading(int32 Ci, int32& Off, int32& Aboard) const;
	/** The test-filled coach parked first (INDEX_NONE: none parked yet). */
	int32 TestLead() const;
	/** Tests (PerfViews "coach board N"): coach Ci, parked in its town hub bay (sent there by "coach town"), takes a departure
	 *  run of N passengers made unseen along the shelter. They walk to its door, put their bags in and board through the
	 *  same stages as the game (HubOpen .. HubBoard); the door shuts once all are on, it drives to its Grand Hall bay and
	 *  they get off there (ToTerminal .. TermShut). False (logged): not at the hub, or busy. */
	bool TestTownRun(int32 Ci, int32 N);
	/** Tests: how many test runs are still boarding at the hub, and how many have left it. */
	void TestBoarding(int32& Boarding, int32& Left) const;
	/** Tests: why no test coach is unloading (empty: nothing known to be wrong - still on its way). */
	FString TestWhy() const;
	virtual void Tick(float Dt) override;

private:
	enum class EStage : uint8 { Idle, DriverComing, ToTown, HubOpen, HubBags, HubShut, HubBoard, ToTerminal, TermOpen, TermOff, TermShut, Broken, ToHome };
	/** One coach run of the timetable: leaves the hub at LeaveT (+ Late: its small real-life delay), reaches the terminal for
	 *  its flight(s). */
	struct FTrip { FString Flight; double DepT = 0.0, LeaveT = 0.0; float Late = 0.f, LimitMs = 13.9f; int32 People = 0, BagsN = 0, Coach = INDEX_NONE; bool bGone = false, bNoCoachSaid = false, bArr = false; double RateAt = 1.0; };   // (bArr: terminal to hub; RateAt: the sim speed its times were worked out with)
	TArray<FTrip> Trips;
	struct FHubCoach
	{
		int32 Trip = INDEX_NONE;
		int32 Ci = INDEX_NONE; EStage St = EStage::Idle; float T = 0.f, Said = -1000.f;
		TWeakObjectPtr<AActor> Driver; int32 Bay = INDEX_NONE;
		TArray<int32> Riders;          // (Waiters indices assigned to it)
		TArray<int32> BaysOpen;        // (the door-side luggage bays its driver opens)
		TArray<int32> BagsIn;          // (bags in each of those)
		int32 Fails = 0; bool bSlow = false;
		bool bAtHub = false;           // (parked in hub bay Bay after an arrival run: waits there for a departure run, or drives back)
		bool bTestRide = false, bRode = false, bParkSaid = false;   // (a test-filled coach: its next drive in is the run; it has driven)
		int32 Fill = 0, OffN = 0, ParkSeq = 0;                       // (still to seat; off so far; the order it parked in)
		int32 HubFill = 0;                                           // (test town run: still to be made at the shelter)
		bool bTownRun = false; FString FillWhy;                      // (a test town run; why its passengers are not being made just now)
	};
	struct FWaiter
	{
		TWeakObjectPtr<AActor> A; bool bBag = false; FString Flight; int32 Coach = INDEX_NONE;
		uint8 Phase = 0;   // 0 at the shelter, 1 walking to the coach's stop, 2 bag going in, 3 boarding, 4 aboard, 5 getting off, 6 taking the bag, 7 handed back
		int32 Bay = INDEX_NONE; bool bTaxi = false; bool bSaidNoWalk = false; int32 Trip = INDEX_NONE; double DepT = 0.0;
		int32 Slot = INDEX_NONE; bool bIn = false;   // (its place by the coach's door; its bag in the bay)
		bool bArr = false; int32 Kind = 1;            // (an arrival, terminal to hub; its bag's model)   (Phase 9: walking to the stop on the walking map)
	};
	TArray<FHubCoach> Fleet; TArray<FWaiter> Waiters;
	int32 ParkSeqN = 0; bool bTestAny = false; FString TestWhyS;
	bool TakeWaitingArrival(FHubCoach& C);
	void TestFills();
	bool bLoaded = false, bDataOk = false;
	TArray<FVector> Bays;                       // (hub coach bays: front bumper, world xy; nearest the shelter first)
	FVector ShelterA = FVector::ZeroVector, ShelterB = FVector::ZeroVector;   // (the shelter's line, world xy)
	float Think = 0.f; int32 NextSpot = 0;
	double Rate = 1.0, LastSim = -1.0, LastReal = 0.0;   // (sim seconds a real second: the drive is real time)
	TArray<int32> BayOrder;                     // (hub bay indices, nearest the shelter first)
	static FString NoWhy;

	bool Load();
	float GroundAt(const FVector& Xy) const;
	ACoaches* Co() const;
	void Fleetup();
	void Dispatch();
	void Step(FHubCoach& C, float Dt);
	bool DriverFor(FHubCoach& C);
	void Say(FHubCoach& C, const FString& What, bool bWarn = false);
	int32 SeatsOf(const FHubCoach& C) const;
	double Now() const;
	int32 SeatCap() const;
	/** Puts a waiter who missed their run on the next run in time for their flight (false: none - logged). */
	bool Rebook(FWaiter& W, const FString& Why);
	/** The taxi hook (to-do 373: taxis join the moment they drive): a taxi for a waiter, INDEX_NONE while there is none. */
	int32 TaxiFor(FWaiter& W);
	int32 NewTrip(const FString& Flight, double DepT, double At);
	void Missed(FWaiter& W, const FString& Why);
	/** Delayed flights and a changed sim speed: runs not yet boarded moved to fit (logged). */
	void Retime();
	/** An idle coach parked at the hub: waits there for a departure run due soon, else drives back to its stand. */
	void HubIdle(FHubCoach& C);
};
