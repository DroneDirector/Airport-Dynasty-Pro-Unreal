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
#include "Tugs.generated.h"

class ABabylonModel;
class UAudioComponent;
class UPoseableMeshComponent;
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
	void Arriving(float StandX);
	/** The pushback: Fitting the aircraft's tow fitting (world, on the ground), Fwd its nose direction (world, flat).
	 *  True once a tug is hooked on (it drives in first); then it is posed on the aircraft. */
	bool Push(const FString& Key, float StandX, const FVector& Fitting, const FVector& Fwd);
	/** The clearance words for a departure (SimTraffic.cpp): its callsign, the way it will face after the push, the
	 *  taxi route, the runway - for the pushback's talk (docs/Pushback-Sequence.md). */
	void Clearance(const FString& Key, const FString& Callsign, const FString& Face, const FString& Route, const FString& Runway);
	/** The pushback is done: the tug unhooks, backs away and drives back to the park. */
	void Release(const FString& Key);
	/** True when no tug is on or in front of that aircraft any more (it may taxi). */
	bool IsClear(const FString& Key) const;
	int32 NumAtWork() const;
	/** Tests: where to look - the first tug at work (else the park); its facing yaw. */
	bool WatchSpot(FVector& At, float& Yaw, bool* bHooked = nullptr, bool* bAtGate = nullptr) const;

private:
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
		FPoseFn Hold;                    // kept while the queue is empty (sat at the wheel)
		TFunction<FVector()> Follow;     // walked to while the queue is empty (beside the pushed aircraft's nose)
		float FollowMax = 230.f;         // (cm/s: the wing walkers keep up with the turning wingtip - a jog)
		bool bWalker = false;            // a wing / tail walker (shared by the gates: free when Tug is INDEX_NONE)
		// waiting (owner 2026-11-21: never stiff as a rock, always staying where they are): the place they wait at (where the last
		// walk took them), a small step off it, a look round, and now and then a little movement of their own
		FVector Anchor = FVector::ZeroVector; float AnchorYaw = 0.f; FVector IdleOff = FVector::ZeroVector; float IdleYawOff = 0.f, IdleT = 4.f;
		FPoseFn Fidget; float FidgetT = 0.f, FidgetDur = 0.f; int32 LastFidget = -1;
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
		FString Callsign, Face, Route, Runway; bool bDeiced = false;
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
	void IdleTick(FCrew& C, float Dt, bool bMayStep);
	bool CrewWalk(FCrew& C, const FVector& To, float Dt, float Stop, float Max);
	void ApplyPose(FCrew& C, const FPoseFn& Fn, float K);
	void Unpose(FCrew& C);
	void Door(FTug& T, float Dt);
	FVector TugW(const FTug& T, const FVector& Local) const;
	FPoseFn ClimbFn(int32 Tug, bool bIn) const;
	FPoseFn SeatFn(int32 Tug) const;
	void ClimbIn(FCrew& C, int32 Tug, TFunction<void()> Then);
	void ClimbOut(FCrew& C, int32 Tug);
	void CrewBoard(int32 Tug, bool bArrival = false);
	void CrewCouple(int32 Tug);
	void CrewUncouple(int32 Tug);
	void CrewParked(int32 Tug);
	void CrewWalkers(int32 Tug);
	void WalkersDone(int32 Tug);
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
