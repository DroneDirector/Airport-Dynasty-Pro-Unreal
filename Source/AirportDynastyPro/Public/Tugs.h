// The pushback tugs (owner, 2026-11-21: the airport's ground vehicles, the tug first - Tools/make_pushback_tug.py, imported as
// PushbackTug). One manager for the airport's tugs:
//  - they are always there: parked in the GSE park at the west end of the terminal apron, engines off (they never pop
//    into existence - owner)
//  - a departure coming up (SimTraffic.cpp): the nearest free tug drives out along the head-of-stand road and waits
//    beside the stand OUTSIDE the equipment restraint area (the red line); at off-blocks it drives in to the nose and hooks
//    its towbar's head on the nose gear's tow fitting (the hitch clunk) - only then is the aircraft pushed back, with the
//    tug on it; then it unhooks (the clunk), backs away with its reversing beeper and lamps - the aircraft taxis only once
//    the tug is clear - and drives back to the park, where it stops its engine
//  - tugs give way to each other (none drives within 8 m of another)
//  - its lamps glow by material (the fire truck's lens material: M_TruckLens, one value per lens): low beams, tail lamps
//    and markers while it runs, the reversing lamps when it backs, the amber roof beacon and the two rear flashers flashing
//    as the jetway beacons do while it runs; its windows are the fire truck's glass (M_TruckGlass)
//  - its sounds where it is: the engine (start, idle loop, stop), the hitch, the reversing beeper (Art/Sounds/PushbackTug)
// Next steps (to-do 196/198, the vehicles list): the operator (a MetaHuman walking out, hooking on and climbing in with the
// tug's IK points) and the wing walker, the crew hired in the simulation, driving it yourself (E/X, the outline outside
// only), dirt and washing as the fire truck.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VehicleDirt.h"
#include "Tugs.generated.h"

class ABabylonModel;
class UAudioComponent;
class UPoseableMeshComponent;
class UCableComponent;
class UDriverPoseAnim;
class FTruckDriverRig;
struct FDriverPose;
class UInstancedStaticMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API ATugs : public AActor
{
	GENERATED_BODY()
public:
	ATugs();
	static ATugs* Get(UWorld* W);
	virtual void Tick(float Dt) override;
	/** A departure coming up at the stand whose nose line is at StandX (plan m): a tug drives out and waits beside it,
	 *  outside the red line. */
	void Prepare(const FString& Key, float StandX);
	/** An aircraft due on the stand in 3 minutes (owner 2026-11-21): its tug's driver walks over and sits in it, ready; the
	 *  handler and the walkers come once the aircraft is parked and its departure is near (Prepare). */
	void Arriving(float StandX, bool bMine = false);   // (bMine: your own flight - with your hired crew on (to-do 206) its driver comes for the push)
	/** To-do 206, your own flight and adp.HiredCrew: the ground power is plugged in by one of your hired people. True when one
	 *  of them has taken the job (or there is nothing to do); false: it waits - ask again. When: its departure. */
	bool ArrivedHired(float StandX, const FString& Reg, const FDateTime& When);
	/** To-do 206: may your flight leave - your pushback crew is there (the driver aboard)? False: why not. */
	bool CrewReady(const FString& Key, float StandX, FString& Why);
	/** The pushback: Fitting the aircraft's tow fitting (world, on the ground), Fwd its nose direction (world, flat).
	 *  True once a tug is hooked on (it drives in first); then it is posed on the aircraft. */
	bool Push(const FString& Key, float StandX, const FVector& Fitting, const FVector& Fwd);
	/** The clearance words for a departure (SimTraffic.cpp): its callsign, the way it will face after the push, the
	 *  taxi route, the runway - for the pushback's talk (docs/Pushback-Sequence.md). */
	void Clearance(const FString& Key, const FString& Callsign, const FString& Face, const FString& Route, const FString& Runway);
	/** The chocks and cones (to-do 212, TugChocks.cpp): an aircraft on blocks - somebody sets them. When: its departure; bHired:
	 *  your own flight, one of your hired people; bLong: it stands two hours or more; bAtOnce: they are in already. True once
	 *  the job is taken up (or there is nothing to do). */
	bool ChocksOn(float StandX, const FString& Reg, const FDateTime& When, bool bHired, bool bLong, bool bAtOnce);
	bool ChocksIn(float StandX) const;
	/** The aircraft is gone: whatever is still out is cleared. */
	void ChocksGone(float StandX);
	/** An aircraft has parked on the stand (on blocks): its ground power is plugged in (GpuUnit in TugCrew.cpp). */
	void Arrived(float StandX);
	/** The pushback is done: the tug unhooks, backs away and drives back to the park. */
	void Release(const FString& Key);
	/** True when no tug is on or in front of that aircraft any more (it may taxi). */
	bool IsClear(const FString& Key) const;
	int32 NumAtWork() const;
	/** Tests: where to look - the first tug at work (else the park); its facing yaw. */
	bool WatchSpot(FVector& At, float& Yaw, bool* bHooked = nullptr, bool* bAtGate = nullptr) const;
	/** Tests (owner 2026-11-21: the tested thing always in frame, people and moving things followed): the points that must be
	 *  in view now - the work going on (the GPU, its plug and the person carrying it; the tug, the aircraft's nose, the
	 *  handler; the walkers in a push) - and the side to look from. False if nothing is going on. */
	bool WatchPoints(TArray<FVector>& Pts, FVector& Side) const;
	/** Tests: some work was done and now everything rests again (every tug in its box, every crew member idle, no ground
	 *  power being worked) - the test can end. */
	bool AllDone() const;

private:
	friend class UStaffPeople;   // (your hired people, to-do 206: the way round the vehicles)
	friend class AGSE;   // (the baggage tractors and carts: GSE.h - the same ground crew, the same apron)
	enum class EState : uint8 { Parked, ToStand, Waiting, ToNose, Hooked, Unhook, Backing, Home, Boarding, Coupling, ShowPin, Manned };   // (Manned: the driver in it, engine off, ready for the aircraft coming in)
	// ---- the crew (owner 2026-11-21: "the person driving the pushback truck ... exit the truck and connect"): MetaHumans of
	// the ground staff (Content/Data/people-roster.json, role ground) - per tug a driver and the pushback handler (the headset
	// man); with no handler the driver does it alone. Each works through a queue of acts: walk somewhere, a posed movement
	// (the fire truck's IK rig: feet, hands and hips placed - climbing in by the step and grab rail, sitting at the wheel,
	// squatting at the towbar head and the nose gear, holding the bypass pin up to the cockpit), a wait, a wait for
	// something, or a deed (the door, the hitch, the tug's next state).
public:
	using FPoseFn = TFunction<void(float K, float PH, FDriverPose& P)>;   // world coordinates; K 0..1 through the act
	struct FCrewAct
	{
		enum EKind : uint8 { Walk, Pose, Wait, WaitFor, Do } Kind = Wait;
		TFunction<FVector()> To; float Stop = 25.f, Max = 150.f;
		float Dur = 0.f; FPoseFn PoseFn;
		TFunction<bool(float Elapsed)> Until;
		TFunction<void()> Fn;
	};
private:
	struct FCrew
	{
		TWeakObjectPtr<AActor> A; FString Name; int32 Tug = INDEX_NONE; bool bDriver = false;
		FVector Spot = FVector::ZeroVector; float SpotYaw = 0.f;
		FTruckDriverRig* Rig = nullptr; UPoseableMeshComponent* Rider = nullptr; UDriverPoseAnim* Anim = nullptr; FTransform BodyInActor; bool bPosed = false;
		float Speed = 0.f, T = 0.f; TArray<FCrewAct> Q;
		float BestDist = 1e9f, StuckT = 0.f, SideT = 0.f; FVector StuckTo = FVector::ZeroVector, SideDir = FVector::ZeroVector;   // (being stuck: CrewWalk)
		FPoseFn Hold;                    // kept while the queue is empty (sat at the wheel)
		TFunction<FVector()> Follow;     // walked to while the queue is empty (beside the pushed aircraft's nose)
		float FollowMax = 230.f;         // (cm/s: the wing walkers keep up with the turning wingtip - a jog)
		bool bWalker = false;            // a wing / tail walker (shared by the gates: free when Tug is INDEX_NONE)
		// waiting (owner 2026-11-21: never stiff as a rock, always staying where they are): the place they wait at (where the last
		// walk took them), a small step off it, a look round, and now and then a little movement of their own
		FVector Anchor = FVector::ZeroVector; float AnchorYaw = 0.f; FVector IdleOff = FVector::ZeroVector; float IdleYawOff = 0.f, IdleT = 4.f;
		FPoseFn Fidget; float FidgetT = 0.f, FidgetDur = 0.f; int32 LastFidget = -1;
		bool bHired = false, bLeave = false, bSlotFree = false; FString StaffId; float BlockT = 0.f; bool bLost = false, bRelief = false, bRelClean = false, bRelFuel = false, bRelCater = false, bRelWet = false; float LostAsk = 0.f, LostT = 0.f; FVector LastAt = FVector::ZeroVector; FPoseFn LostHold;   // (to-do 206: one of your hired people for this job; done with it; an empty slot to use again)
		bool bUp = false;                // (up on a jetway's service stair or in its tunnel: on the surface under them, not the apron's)
		bool bGse = false;               // on a baggage tractor / cart job just now (GSE.h): not free for a push
	};
	TArray<FCrew> Crew;
	struct FLens { UInstancedStaticMeshComponent* K = nullptr; int32 Ix = 0; uint8 Kind = 0; int8 Side = 0; float Now = -1.f; };   // Side: indicators -1 right, +1 left
	struct FTug
	{
		TWeakObjectPtr<ABabylonModel> M;
		TArray<FLens> Lenses;
		UAudioComponent* Engine = nullptr; UAudioComponent* Beep = nullptr;
		FVector At = FVector::ZeroVector, Home = FVector::ZeroVector;
		float Yaw = 0.f, HomeYaw = 0.f, Clock = 0.f;
		EState State = EState::Parked;
		FString Key;                                  // the departure it works
		float StandX = 0.f;                           // (plan m)
		TArray<FVector> Path;                         // waypoints (world); bReverse: driven backwards
		bool bReverse = false;
		FVector HookAt = FVector::ZeroVector; float HookYaw = 0.f;
		// its wheels (each a group: tyre, rim, disc, hub, cap and nuts round the hub): rolled by the distance moved, steered in
		// turns - four-wheel steering, the rear wheels opposite the front, as the real tug
		int32 Wheel[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE}; FVector WheelAt[4];
		float Speed = 0.f;              // (cm/s, eased up and down)
		float MeasSpeed = 0.f, LastSpeed = 0.f, BrakeHold = 0.f;   // (m/s)   // brake lamps: on while slowing (and held a moment), and at a standstill with the engine on
		float Spin = 0.f, Steer = 0.f; FVector PrevAt = FVector::ZeroVector; float PrevYaw = 0.f; bool bPrev = false;
		int32 Driver = INDEX_NONE, Handler = INDEX_NONE;           // its crew (Crew[])
		// to-do 206: this departure is worked by your hired people (Driver / Handler: those two while they work it, nobody while it
		// waits for them - never by itself); the gate's stand-ins; the hired person at its ground power
		bool bHired = false; int32 StandInDriver = INDEX_NONE, StandInHandler = INDEX_NONE, GpuCi = INDEX_NONE; float CrewAsk = 0.f;
		FVector Fit = FVector::ZeroVector, Fwd = FVector(1, 0, 0);  // the aircraft's tow fitting and nose direction (world)
		bool bPinShown = true; FString PinKey;                     // the bypass pin shown to the cockpit for that departure
		int32 DoorG = INDEX_NONE; float DoorAng = 0.f, DoorWant = 0.f;
		TArray<FVector> Pending; bool bPendingReverse = false;        // the way on after a short reverse (or pull forward) away from an obstacle
		float RouteCheck = 0.f;                                    // (the way home re-checked for obstacles every 2 s)
		// the pushback's 11 steps (owner 2026-11-21, docs/Pushback-Sequence.md): each waits for the one before, a random
		// 2-5 s pause before each; the talk between the handler (interphone), the pilot and ATC (radio)
		int32 Seq = 0; float SeqT = 0.f, SeqWait = 0.f, EngT = -1.f; bool bEngines = false;
		struct FLine { uint8 Who = 0; FString Text; bool bRadio = false; };   // Who: 0 the ground crew, 1 the pilot, 2 ATC; bRadio: on the ground frequency (else the interphone)
		TArray<FLine> Talk; float TalkT = 0.f;
		FString Callsign, Face, Route, Runway; bool bDeiced = false, bPushReq = false, bCleared = false; int32 PreSeq = 0; double PreAt = 0.0;   // (bPushReq: the cockpit has asked for the push - the tug stays in its box until then)
		float StandNose = 0.f, StandDir = -1.f;                   // (plan m: the stand's nose stop line, the way its aircraft face)
		// the gate's ground power unit (owner 2026-11-21: "you should see the ground crew member actually plug it into the airplane
		// and take it off and put it in its place again"): parked in its box at the head of the stand, its cable to the A320's
		// receptacle (Airbus AC 5-4-4: 2.5 m ahead of the nose tyre, on the centre line, 2.0 m up)
		struct FGpu
		{
			TWeakObjectPtr<ABabylonModel> M; int32 PlugG = INDEX_NONE, PanelG = INDEX_NONE; float PanelAng = 0.f, PanelWant = 0.f;
			UCableComponent* Cable = nullptr; UAudioComponent* Idle = nullptr; float RunT = -1.f;
			class UBoxComponent* CableFloor = nullptr;   // (an unseen floor for the cable alone, a hair above the visible ground - it never sinks in)
			bool bConnected = false, bPower = false, bInHand = false; int32 Holder = INDEX_NONE;
			TArray<TTuple<UInstancedStaticMeshComponent*, int32, FTransform>> Coils, RunPlug;   // (the stowed cable: hidden as it pays out)
			FVehicleDirt Dirt; FVector DirtAt = FVector::ZeroVector;   // (dirt and washing as the fire truck - VehicleDirt.h)
		} Gpu;
		// the chocks and cones of this gate (to-do 212, TugChocks.cpp): pairs 0-2 (nose, right main, left main), cones 3-7
		struct FChkItem { uint8 Where = 0; float Bl = 1.f; bool bDirty = true; FTransform From[2], Now[2], Lie[2]; };   // Where: 0 rack, 1 right hand, 2 left hand, 3 out, 4 lying
		struct FChk
		{
			FChkItem Items[8]; uint8 Stage = 0;   // 0 on the rack, 1 being set, 2 set, 3 the cones being brought in, 4 only the chocks in, 5 the chocks coming off
			int32 Ci = INDEX_NONE; bool bOwn = false, bHired = false, bLong = false, bLoose = false; float Clock = 0.f, WaitT = 0.f;
			FString Reg; FDateTime When;
		} Ck;
		FVehicleDirt Dirt;
		TArray<int32> Walkers; int32 WalkersReady = 0; bool bWalkersOut = false; float WalkerLook = 0.f;              // the wing walkers and tail walker on this push (Crew[])
	};
	TArray<FTug> Tugs;
	TArray<FVector> Planes;   // the moving aircraft (taxiing, pushed), looked up once a second: vehicles always give way to them
	float PlaneClock = 1.f;
	float RadioBusy = 0.f;   // (the ground frequency: one transmission at a time - every aircraft's calls wait for it)
	float Flash = 0.f;
	bool bReady = false;
	void Setup();
	bool SpawnAt(FTug& T);
	int32 Assign(const FString& Key, float StandX);
	FVector Plan(float X, float Y) const;            // plan (x, y) m on the ground -> world
	void Drive(FTug& T, float Dt);
	void Pose(FTug& T);
	void Lamps(FTug& T);
	void Wheels(FTug& T, float Dt);
	void Engine(FTug& T, bool bOn);
	void Sound(const TCHAR* Name, const FVector& At);
	// the crew (TugCrew.cpp)
	void SetupCrew();
	void StepCrew(FCrew& C, float Dt);
	void IdleTick(FCrew& C, float Dt, bool bMayStep, float Room = 1e9f);   // (Room: seconds of the wait left - a movement starts only if it fits)
	bool CrewWalk(FCrew& C, const FVector& To, float Dt, float Stop, float Max);
	/** Where to walk next on the way to To: round the GPUs and the tugs that stand in the straight way (owner 2026-11-21:
	 *  never through them, yet able to come very close - their own outline, a person's half width wider). */
	FVector Around(const FVector& From, const FVector& To) const;
	void ApplyPose(FCrew& C, const FPoseFn& Fn, float K);
	void Unpose(FCrew& C);
	void Door(FTug& T, float Dt);
	float CrewZ(const FCrew& C, const FVector& P) const;
	FVector TugW(const FTug& T, const FVector& Local) const;
	FPoseFn ClimbFn(int32 Tug, bool bIn) const;
	FPoseFn SeatFn(int32 Tug) const;
	void ClimbIn(FCrew& C, int32 Tug, TFunction<void()> Then);
	void ClimbOut(FCrew& C, int32 Tug);
	// your hired people (to-do 206, Private/TugStaff.cpp)
	bool HiredJob(const FString& Key) const;
	bool HiredLive(const FString& Key, FDateTime* When = nullptr, FString* Flight = nullptr) const;
	int32 Enlist(AActor* A, const FString& StaffId, const FString& Label);
	void Discharge(int32 Ci);
	void LeaveAct(FCrew& C);
	void SafeWay(FCrew& C, float StandX);
	bool HiredFree(const FTug& T) const { return T.Driver == INDEX_NONE && T.Handler == INDEX_NONE && T.GpuCi == INDEX_NONE && !T.Walkers.Num(); }
	void HiredEnd(FTug& T);
	void LostTick(int32 Ci, float Dt);
	void HiredTick(int32 Tug, float Dt);
	bool PersonAhead(const FVector& Here, const FVector& Dir, float Reach) const;
	void CrewBoard(int32 Tug, bool bArrival = false);
	void CrewCouple(int32 Tug);
	void CrewUncouple(int32 Tug);
	void CrewParked(int32 Tug);
	void CrewWalkers(int32 Tug);
	void WalkersDone(int32 Tug);
	void GpuSetup(FTug& T);
	void GpuTick(FTug& T, float Dt);
	FVector GpuW(const FTug& T, float X, float Y, float Z) const;
	FVector Receptacle(const FTug& T) const;
	void GpuConnect(int32 Tug, FCrew& C);
	void GpuDisconnect(int32 Tug, FCrew& C);
	// the chocks and cones (TugChocks.cpp)
	using FChk = FTug::FChk; using FChkItem = FTug::FChkItem;
	TArray<UInstancedStaticMeshComponent*> ChkMesh; bool bChkReady = false; float ChkYaw = 0.f;
	FVector ChkAt(const FTug& T, float Lat, float Aft) const;
	void ChkSetup();
	void ChkPlace(const FTug& T, int32 It, FTransform Out[2], uint8& Rope) const;
	void ChkDraw(int32 Tug);
	void ChkMove(int32 Tug, int32 It, uint8 To);
	void ChkTake(int32 Tug, FCrew& C, int32 It, bool bTake, uint8 To);
	void ChkWork(int32 Tug, int32 Ci);
	int32 ChkWorker(int32 Tug);
	void ChkTick(int32 Tug, float Dt);
	bool ChkOut(const FTug& T) const { return T.Ck.Stage == 0; }
	void StartBacking(FTug& T);
	void StartHome(FTug& T);
	void SeqTick(FTug& T, float Dt);
	void Say(FTug& T, uint8 Who, const FString& Text, bool bRadio = false);
	/** Its way home round something in the way that shouldn't be there (owner 2026-11-21) - only then; the normal way
	 *  otherwise. A grid search over the apron (1.5 m) round parked and moving aircraft, other tugs, the jetways' red
	 *  zones, the stand boundary lines and the terminal; first backing off (or pulling forward) when the obstacle is right
	 *  in front of (behind) it. False if there is no way. */
	bool RouteHome(FTug& T);
	bool PathBlocked(const FTug& T, const TArray<FVector>& Path, const FVector& From) const;
};
