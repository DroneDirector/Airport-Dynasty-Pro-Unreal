// The baggage tractors and baggage carts at work (owner 2026-11-21: the new ground vehicles in the game, doing what they
// do, with a ground crew - Tools/make_baggage_tractor.py / make_baggage_cart.py, imported as BaggageTractor and
// BaggageCart; the pushback tug is the standard for every part). One manager, made by the tugs (ATugs::Setup), sharing
// their ground crew (ATugs' crew: the same people switch jobs - ground-crew-staffing):
//  - four tractors in their boxes and eight carts, unhitched, their drawbars standing up (the parking brake), in the
//    departure cart lane in the baggage block (plan x 114-156, z 38-52, the make-up carousels south of it)
//  - a job: a free crew member walks over (through the Ramp services 13 passage when he comes from the apron), opens the
//    driver's door, climbs in by the step and the grab handle, sits, closes the door, switches on; the tractor backs up to
//    the carts and he couples them by hand - the tractor's pin up, the cart's drawbar lowered so its eye drops into the
//    jaw, the pin down (its clunk); the second cart's eye into the first cart's rear jaw the same way
//  - the train tows out through the lane's roller door, along the baggage hall's south side (never across the arrivals
//    belt), north through Ramp services 13 onto the apron and round the stands (behind their tails, across the stand
//    lines only straight, off the jetways' red zones, never under an aircraft) to the departure's stand: parked beside
//    the aircraft's forward hold, ahead of the wing, clear of the tug's way and the GPU; the crew roll the curtains up on
//    the aircraft's side, the bags go in (the belt loader and the bags come next), the curtains down; the train goes
//    back the way it came, the carts are uncoupled in the lane and the tractor parks in its box
//  - the same tractor can tow a gate's GPU instead (its towing eye 0.40 m, the middle slot of the pin hitch)
//  - each cart is a real trailer: its turntable axle steered by its drawbar, its body following its axle, wheels rolling
//  - lamps (low, high, tail / brake, indicators, reverse, amber beacon) as the tug's, its electric motor, the reversing
//    beeper; aircraft always have right of way; dirt and washing as the fire truck (VehicleDirt.h)
// -GseTest: the first job runs at once (a train of 2 carts to the next departure, and a GPU moved a short way and back).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VehicleDirt.h"
#include "StaffPeople.h"
#include "ServiceVehicles.h"
#include "GSE.generated.h"

class ABabylonModel;
class ATugs;
class UAudioComponent;
class UInstancedStaticMeshComponent;
struct FDriverPose;
struct FSvcRefuel;
struct FSvcCater;
struct FSvcWet;

UCLASS()
class AIRPORTDYNASTYPRO_API AGSE : public AActor
{
	GENERATED_BODY()
public:
	AGSE();
	/** The manager (made once, by the tugs). */
	static AGSE* Get(UWorld* W);
	static AGSE* Find(const UWorld* W);
	virtual void Tick(float Dt) override;
	/** A departure coming up at the stand whose nose line is at StandX (ATugs::Prepare): its baggage train goes out. */
	void Departure(const FString& Key, float StandX);
	/** The arriving flights whose bags are out of the hold and away (a departure train waits for that). */
	TSet<FString> ArrAway;
	/** That flight has left: its number may be used again tomorrow. */
	void Gone(const FString& Key) { Served.Remove(Key); MadeUp.Remove(Key); BagFlights.Remove(Key); }
	/** True while a baggage train is at (or going in to) that stand - the push waits for it to leave. */
	bool BusyAt(float StandX) const;
	/** Tests: what must be in view now (the tractor, its carts, the crew at the hitch or the curtains), the side to look from. */
	bool WatchPoints(TArray<FVector>& Pts, FVector& Side) const;
	/** Tests: a job was done and everything rests again. */
	bool AllDone() const;
	bool IsTest() const { return bTest; }
	/** Tests: a train was given a real departure (then the test also waits for that departure's push). */
	bool HadDeparture() const { return bTestStand || Served.Num() > 0; }
	/** The vehicles' outlines (world, flat) for the crew's walking round them (ATugs::Around). */
	struct FOutline { FVector C, X, Y; float Hx, Hy; };
	void Outlines(TArray<FOutline>& Out) const;

	// ---- the vehicles (GSECrew.cpp works their crew)
	struct FLens { UInstancedStaticMeshComponent* K = nullptr; int32 Ix = 0; uint8 Kind = 0; int8 Side = 0; float Now = -1.f; };
	enum class EJob : uint8 { None, Boarding, ToCarts, Coupling, Out, AtStand, Loading, Back, Uncoupling, Home, ToGpu, GpuCouple, GpuMove, GpuUncouple };
	struct FTractor
	{
		TWeakObjectPtr<ABabylonModel> M; TArray<FLens> Lenses;
		UAudioComponent* Motor = nullptr; UAudioComponent* Beep = nullptr;
		FVector At = FVector::ZeroVector, Home = FVector::ZeroVector; float Yaw = 0.f, HomeYaw = 0.f;
		TArray<FVector> Path; bool bReverse = false; float Speed = 0.f, Hold = 0.f;
		float MeasSpeed = 0.f, LastSpeed = 0.f, BrakeHold = 0.f, Steer = 0.f, SpinF = 0.f, SpinR = 0.f, Wheel = 0.f;
		FVector PrevAt = FVector::ZeroVector; float PrevYaw = 0.f; bool bPrev = false;
		bool bZone[2] = {false, false};   // (its turn in a place where only one train fits: the cart lane's door, the passage at Ramp services 13)
		float Orbit = 0.f, OrbitYaw = 0.f; int32 OrbitN = -1;   // (how far round it has turned for the point it drives to)
		int32 GWheel[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE}, GSteer[2] = {INDEX_NONE, INDEX_NONE};
		int32 GDoor = INDEX_NONE, GDoor2 = INDEX_NONE, GWheelS = INDEX_NONE, GPin = INDEX_NONE;
		float DoorAng = 0.f, DoorWant = 0.f, Pin = 0.f, PinWant = 0.f;
		bool bOn = false;
		int32 Driver = INDEX_NONE;          // ATugs' crew member driving it
		TArray<int32> Train;                // the carts hitched behind it, in order
		int32 TowGpu = INDEX_NONE;          // or the gate GPU it tows (ATugs' tug index)
		EJob Job = EJob::None; FString Key; float StandX = 0.f, JobT = 0.f;
		float EndYaw = 0.f; bool bEndYaw = false; TFunction<void()> OnArrive;      // (what comes after the leg it drives)
		TFunction<bool()> Until; TFunction<void()> AfterUntil;                    // (waiting at a holding point until something is so)
		FVehicleDirt Dirt;
		FString Name;
		// its own measures (root cm; the tractor's or the belt loader's - Art/<model>/<model>-parts.json)
		bool bLoader = false;
		int32 Svc = INDEX_NONE;           // a service vehicle (GSEService.cpp): its line in the table
		bool IsTrain() const { return !bLoader && Svc == INDEX_NONE; }   // (a baggage tractor)
		float KDoorDeg = 80.f;
		TWeakObjectPtr<class AServiceDrive> Player; int32 PlayerSeat = INDEX_NONE;   // (you in it: ServiceDrive.h; 0 the driver's seat, then the crew's, then the Cab seats)
		float SteerWant = 0.f, BackT = 0.f, PedalA = 0.f, PedalB = 0.f, WipeT = 0.f, WipeNow = 0.f, RainIn = 0.f, BoxT = 0.f; bool bRain = false, bUsed = false;   // (bUsed: full bags or used rolls in it - exchanged shut in its box)
		int32 GPedal[2] = {INDEX_NONE, INDEX_NONE}, LoadWant = INDEX_NONE; TArray<int32> GWiper; FString Livery;
		TSharedPtr<FSvcRefuel> Fu;        // (the refueller's own parts: GSEFuel.cpp)
		TSharedPtr<FSvcWet> Wa;           // (the water and the toilet truck's: GSEWater.cpp)
		TSharedPtr<FSvcCater> Ct;         // (the catering high-loader's: GSECater.cpp)
		TMap<FString, FVector> Pt;        // (a service vehicle's named points, root cm: Data/<vehicle>-riders.json)
		struct FSvcDoor { int32 G = INDEX_NONE; FVector Piv = FVector::ZeroVector; float Now = 0.f, Want = 0.f; };
		TArray<FSvcDoor> SvcDoors;        // (as the table's doors: 0 shut .. 1 open)
		struct FSvcItem { int32 G = INDEX_NONE; FVector Piv = FVector::ZeroVector; int32 Ci = INDEX_NONE; bool bShow = true, bMoved = true; };
		TArray<FSvcItem> SvcItems;        // (as the table's items: in their place, or in somebody's right hand)
		FVector KWheel[4], KKing[2], KHinge, KWheelC, KStand, KHandle, KStep, KGrabOut, KGrabIn, KSeat, KBrake, KAccel, KHandL, KHandR;
		float KRadF = 26.f, KRadR = 31.f, KWB = 158.f, KTrack = 112.f, KFloor = 62.f, KHalfL = 171.f, KHalfW = 72.f, KCentreX = 3.f, KTurnR = 300.f, KMaxSteer = 32.f;
		FString SndStart, SndLoop, SndStop, SndDoorOpen, SndDoorClose, SndBeep;
		// the belt loader: its boom (0 down .. -20.5 deg up), the rams, the bumper, the belt running
		float Boom = 0.f, BoomWant = 0.f; bool bBelt = false;
		int32 GBoom = INDEX_NONE, GBumper = INDEX_NONE, GRam[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
		UAudioComponent* BeltSnd = nullptr; UAudioComponent* BoomSnd = nullptr;
		int32 TopCrew = INDEX_NONE;       // the loader up at the hold door
		bool bBottomHeld = false;         // (a handler about to lay a bag on the belt's bottom end)
		struct FBeltBag { int32 Id = 0; float S = 0.f, T = 0.f; uint8 Stage = 0; };   // (up: 0 riding, 1 at the top, 2 in the hold loader's hands; arrivals down: 5 in his hands, 3 riding down, 4 at the bottom)
		bool bArrival = false;            // a train for an arriving aircraft's bags (hold -> carts -> the arrivals conveyor)
		int32 ArrivalsLeft = 0;           // (the belt loader: arriving bags still in the hold)
		float ArrivalIn = 0.f; FString ArrKey;
		TArray<FBeltBag> BeltBags;        // the bags pinned on its belt (DepartureBelts' bags)
	};
	struct FCart
	{
		TWeakObjectPtr<ABabylonModel> M; FVector At = FVector::ZeroVector; float Yaw = 0.f, AxleYaw = 0.f;
		float Lift = -80.f, LiftWant = -80.f;           // the drawbar: 0 lowered (towing), -80 standing up (the brake on)
		float Curtain[2] = {0.f, 0.f}, CurtainWant[2] = {0.f, 0.f};   // L, R: 0 down .. 1 rolled up
		float Pin = 0.f, PinWant = 0.f;                 // the rear coupler's pin: 0 in .. 1 lifted
		float Spin = 0.f; FVector PrevAt = FVector::ZeroVector; bool bPrev = false;
		int32 GWheel[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE}, GAxle = INDEX_NONE, GBar = INDEX_NONE, GPin = INDEX_NONE;
		int32 GSheet[2] = {INDEX_NONE, INDEX_NONE}, GHem[2] = {INDEX_NONE, INDEX_NONE}, GStrap[2] = {INDEX_NONE, INDEX_NONE}, GRoll[2] = {INDEX_NONE, INDEX_NONE};
		bool bStraps[2] = {true, true};                 // the curtain's straps hooked
		int32 Tractor = INDEX_NONE;                     // hitched in that tractor's train
		FString Flight;                                 // (the train carries its flight's tag: never the wrong aircraft)
		UAudioComponent* Roll = nullptr;
		FVehicleDirt Dirt;
		bool bMoved = true;
		TArray<int32> Bags;                             // its load slots (18: 3 along x 2 across x 3 high): the bag in each, or INDEX_NONE
	};
	/** A gate GPU being towed: its moving parts (bound when first towed). */
	struct FGpuRig { int32 GWheel[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE}, GAxle = INDEX_NONE; float AxleYaw = 0.f, Spin = 0.f; FVector Home = FVector::ZeroVector; float HomeYaw = 0.f; bool bBound = false; FVector PrevAt = FVector::ZeroVector; };

private:
	friend class ATugs;
	friend class AServiceDrive;
	TArray<FTractor> Tractors;
	TArray<FCart> Carts;
	TMap<int32, FGpuRig> GpuRigs;
	TWeakObjectPtr<ATugs> TugsRef;
	bool bReady = false, bTest = false, bWorked = false, bTestStarted = false, bGpuDone = false;
	float Flash = 0.f, Clock = 0.f, CheckClock = 0.f, SayClock = 0.f;
	float DoorClock = 0.f;
	FString TestKey; float TestStand = 0.f; bool bTestStand = false;
	TSet<FString> Served;

	void Setup();
	ATugs* Tg() const;
	FVector Plan(float X, float Y) const;
	FVector PlanPose(float X, float Y, float Hx, float Hy, float& Yaw) const;
	bool SpawnTractor(FTractor& T);
	bool SpawnCart(FCart& C);
	FVector TW(const FTractor& T, const FVector& Local) const;
	FVector CW(const FCart& C, const FVector& Local) const;
	void Drive(FTractor& T, float Dt);
	void Pose(FTractor& T);
	void PoseCart(FCart& C);
	void Train(FTractor& T, float Dt);
	void TractorParts(FTractor& T, float Dt);
	void CartParts(FCart& C, float Dt);
	void Lamps(FTractor& T);
	void Motor(FTractor& T, bool bOn);
	void Sound(const TCHAR* Name, const FVector& At);
	void Check(const FTractor& T, int32 Index);
	// routes
	void Dubins(TArray<FVector>& Out, const FVector& From, float FromYaw, const FVector& To, float ToYaw, float R) const;
	void Leg(TArray<FVector>& Out, float X0, float Y0, float Hx0, float Hy0, float X1, float Y1, float Hx1, float Hy1, float R) const;
	bool Apron(TArray<FVector>& Out, const FVector& From, const FVector& To, const FTractor* Self) const;
	void Go(FTractor& T, TArray<FVector> Path, bool bReverse, TFunction<void()> Then);
	void RouteOut(FTractor& T, TFunction<void()> Then);       // the lane -> the hall -> Ramp services 13 -> the apron
	void RouteIn(FTractor& T, const FVector& ApronFrom, float FromYaw, TFunction<void()> Then);  // the apron -> the lane
	bool StandGeo(float StandX, float& S, float& N, float& Span) const;
	bool AircraftAt(float StandX) const;
	bool PlaneNear(const FVector& At, float R) const;
	// the jobs (GSECrew.cpp: the crew's part)
	void StartTrain(int32 Ti, const TArray<int32>& Pair);
	void TrainToStand(int32 Ti);
	void TrainApron(int32 Ti);
	void TrainHoldIn(int32 Ti);
	void TrainLoad(int32 Ti);
	void TrainHome(int32 Ti);
	void TrainDrop(int32 Ti);
	void ToBox(int32 Ti);
	void StartGpu(int32 Ti, int32 Gate);
	void GpuApron(int32 Ti, int32 Gate);
	void GpuLeave(int32 Ti, int32 Gate);
	void RetryIn(int32 Ti, float Seconds, TFunction<void()> Fn);
	// the belt loader (Art/BeltLoader/beltloader-parts.json): parked by the Ramp services 13 opening, driven to the
	// aircraft's bulk hold door (right side, aft of the wing), the boom raised to the sill, the belt running while the
	// train's bags go up it, the boom down and away before the push
	void SetKind(FTractor& T, bool bLoader) const;
	void LoaderParts(FTractor& T, float Dt);
	int32 LoaderAt(float StandX) const;
	void EnsureLoader(float StandX, const FString& Key);
	void LoaderApron(int32 Li);
	void LoaderIn(int32 Li);
	void LoaderLeave(int32 Li);
	FVector BoomW(const FTractor& T, const FVector& BoomLocal) const;
	void LoadingCycles(int32 Ci, int32 Ti, int32 Li, float Seconds);
	// the bags (DepartureBelts.h): the crew at the flight's make-up carousel ahead of its bags, each bag lifted off by its
	// grips onto a cart's load slot; at the aircraft off the carts onto the belt loader's bottom end, pinned on the belt up
	// to the hold loader, who takes it off into the hold (GSEBags.cpp)
	class USkeletalMeshComponent* Body(int32 Ci) const;
	FTransform CartSlot(int32 Cart, int32 Slot) const;      // (the cart's root frame)
	void MakeUp(int32 Ci, int32 Ti, float Started);
	void Unload(int32 Ci, int32 Ti, int32 Li);
	void BeltTick(FTractor& L, float Dt);
	bool BeltBottomFree(const FTractor& L) const;
	void PollBags(float Dt);
	FString BagFlight(int32 Ti) const;
	float BagPoll = 0.f; int32 BagScan = 1; TSet<FString> BagFlights, MadeUp; TMap<int32, FString> BagKey;
	TArray<TFunction<void()>> Deferred;   // (crew plans made from inside a crew act: run on the next frame)
	void Later(TFunction<void()> F) { Deferred.Add(MoveTemp(F)); }
	int32 Pickable(int32 Ti, int32 H = 0) const;
	void MakeUpBy(int32 Ci, int32 Ti, float Started, int32 H);
	TSet<int32> SlotTaken; TMap<int32, int32> Helper; TSet<int32> HelperDone;
	bool StartFor(const FString& Key, float StandX);
	FVector PickStand(int32 Ti) const;
	FTransform BeltOffset(const FTractor& L, float S) const;
	// the arriving bags (GSEArrivals.cpp): the hold loader puts each on the belt top, it rides down, the train's driver
	// lifts it onto a cart; the train to the arrivals conveyor's loading point in Ramp services 4, the bags onto it
	void StartArrival(int32 Ti, const TArray<int32>& Pair, float StandX, const FString& Key);
	void ArrivalLoad(int32 Ti);
	void ArrivalToStation(int32 Ti);
	void ArrivalUnload(int32 Ti);
	bool ArrivalFor(float StandX, const FString& Key);
	TArray<TPair<double, int32>> PendingRemove;   // (bags handed over to the arrivals conveyor: gone when its own bag appears)
public:
	/** Arrival at a stand (the turnaround's bulk task): the belt loader and an arrivals train for its hold's bags. */
	void ArrivalAt(float StandX, const FString& Key)
	{
		if (HiredFlight(FString(), StandX)) { float S = 0.f, N = 0.f, Sp = 36.f; if (StandGeo(StandX, S, N, Sp) && Sp <= 40.f) AddWant(EWant::Arr, Key, StandX); return; }
		EnsureLoader(StandX, Key); ArrivalFor(StandX, Key);
	}
	/** To-do 206: your own flight's bags wait for your people (Why: who is missing), or its train is being got ready. */
	bool HiredWaits(const FString& Flight, float StandX, FString& Why) const;
private:
public:
	/** E at a service vehicle's door (ServiceDrive.h): true when it was that; Say: why not, when it can't be. */
	bool SvcUse(APawn* Me, FString& Say);
	/** The vehicles and carts on the holomap (plan metres). */
	void MapShapes(TArray<FVector4f>& Out) const;
	/** The turnaround planner's bulk hold task (Turnaround.cpp): the belt loader to that stand. */
	void BulkHold(float StandX, const FString& Key) { EnsureLoader(StandX, Key); }
	/** The service vehicles (cabin cleaning first; docs/Hired-Crew-Plan.md): your own flight's turnaround task. Ask: from 8 min
	 *  before it is due and then every tick - true once its people are on the way (false: it waits for them). Go: the work
	 *  itself may start. Busy: not finished. SvcFor: this task is worked in 3D by people of yours. */
	bool SvcFor(const TCHAR* Task, float StandX) const;
	bool SvcAsk(const TCHAR* Task, float StandX, const FString& Reg, const FString& Flight, const FDateTime& When, float Litres = 0.f);
	void SvcGo(const TCHAR* Task, float StandX);
	bool SvcBusy(const TCHAR* Task, float StandX) const;
	/** The aircraft is gone: what had not started is dropped. */
	void SvcGone(float StandX);
private:
	// Seat 0: the driver; bOut: on their feet by the vehicle; bIn: in the cabin; bDown: off the aircraft again; bGone: lost
	struct FSvcMan { int32 Ci = INDEX_NONE, Seat = 0, Item = INDEX_NONE; bool bSeated = false, bOut = false, bIn = false, bDown = false, bStowed = false, bGone = false; };
	struct FSvcJob
	{
		int32 Svc = 0, Ti = INDEX_NONE, Jet = INDEX_NONE; float StandX = 0.f, RowT = 0.f; FString Reg, Flight, Why; FDateTime When;
		uint8 Phase = 0;                  // 0 waits for its people, 1 on the way, 2 at the stand, 3 going back
		bool bGo = false, bFetch = false; FVector Place = FVector::ZeroVector; float PlaceYaw = 0.f; int32 Rows = 30, RowsDone = 0; float T = 0.f; TArray<FSvcMan> Men;
		int32 Stage = 0; float Litres = 0.f, Pumped = 0.f; TArray<FVector> InWay; int32 CtDoor = 0, CtTrip = 0, CtDone = 0;   // (Ct*: the catering: at which door, its next trip, the doors done as bits) (the refueller's work; the way in, backed out again)
	};
	TMap<int32, FSvcJob> SvcJobs; int32 SvcNext = 1; float SvcClock = 0.f;
	TMap<int32, TPair<int8, int32>> SvcStairUse;   // (a jetway's service stair: which way it is walked now, and by how many)
	static const TCHAR* SvcModel(int32 Svc);
	void SvcSetup();
	void SvcDress(FTractor& T);
	void SvcParts(FTractor& T, float Dt);
	void SvcTick(float Dt);
	bool SvcWaits(const FString& Flight, float StandX, FString& Why) const;
	bool SvcLost(int32 Ci, bool& bDrop);
	bool SvcPersonAhead(const FTractor& T, const FVector& Move) const;
	FSvcJob* SvcJ(int32 Job) { return SvcJobs.Find(Job); }
	FSvcMan* SvcM(int32 Job, int32 m) { FSvcJob* J = SvcJ(Job); return J && J->Men.IsValidIndex(m) ? &J->Men[m] : nullptr; }
	bool SvcAll(int32 Job, TFunctionRef<bool(const FSvcMan&)> Is);
	void SvcStart(int32 Job);
	void SvcBoard(int32 Job, int32 m);
	void SvcAlight(int32 Job, int32 m);
	FVector SvcP(const FTractor& T, const TCHAR* Name) const;
	TFunction<void(float, float, FDriverPose&)> SvcSeatFn(int32 Ti, int32 Seat) const;
	TFunction<void(float, float, FDriverPose&)> SvcClimbFn(int32 Ti, int32 Seat, bool bIn) const;
	void SvcDoor(int32 Ci, int32 Ti, int32 Door, bool bOpen);
	void SvcApron(int32 Job);
	void SvcWork(int32 Job);
	void SvcManAt(int32 Job, int32 m);
	void SvcCabin(int32 Job, int32 m);
	void SvcUp(int32 Job, int32 m, bool bUp);
	void SvcTake(int32 Job, int32 m, bool bTake);
	void SvcLeave(int32 Job);
	void SvcHome(int32 Job);
	void SvcDriveHome(int32 Job, TFunction<void()> Then);
	void SvcRestock(FTractor& T);
	// ---- the refueller (GSEFuel.cpp)
	void FuelDress(FTractor& T);
	void FuelParts(FTractor& T, float Dt);
	void FuelTick(FSvcJob& J, float Dt);
	void FuelArrive(int32 Job);
	void SvcFuelWork(int32 Job);
	void FuelLost(int32 Job);
	// ---- the catering high-loader (GSECater.cpp)
	void CaterDress(FTractor& T);
	void CaterParts(FTractor& T, float Dt);
	void CaterRestock(FTractor& T);
	void CaterDock(const FSvcJob& J, int32 Door, FVector2f& At, FVector2f& Hd, float& SillZ) const;
	void CaterIn(const FSvcJob& J, int32 Door, float FromY, TArray<FVector>& W) const;
	/** The lane on the aircraft's right is used by another vehicle; the catering truck moves at that stand, is at its rear door, or may go there. */
	bool SvcSideBusy(int32 Ti, float StandX) const;
	bool CaterMoving(float StandX) const;
	bool CaterRear(float StandX) const;
	bool CaterRearFree(float StandX) const;
	void CaterHold(int32 Ci, int32 Ti, FVector2D At, FVector2D Face);
	void CaterWalk(int32 Ci, int32 Ti, TArray<FVector2D> Pts, FVector2D EndFace);
	void CaterHand(int32 Ci, int32 Ti, FVector2D At, int32 Side, TFunction<FVector()> To, float Dur, TFunction<void()> Fn, bool bBothHands);
	TFunction<void(float, float, FDriverPose&)> CaterClimbFn(int32 Ti, bool bUp) const;
	void CaterBoxDoor(int32 Ci, int32 Ti, bool bOpen);
	void CaterTrip(int32 Job, int32 Ci, int32 Ti, int32 W, int32 In, int32 Out);
	void CaterArrive(int32 Job);
	void CaterStage(int32 Job);
	void CaterMove(int32 Job);
	bool CaterLost(int32 Job, int32 m);
	void FuelUpDown(int32 Ci, int32 Ti, bool bUp);
	// ---- the potable water truck and the toilet truck (GSEWater.cpp)
	void WetDress(FTractor& T);
	void WetParts(FTractor& T, float Dt);
	void WetTick(FSvcJob& J, float Dt);
	const AActor* WetPlane(float StandX) const;
	/** One of the two (or only the toilet truck) has the place under the tail, or is on its way to it. */
	bool WetAt(float StandX, bool bToiletOnly, int32 NotTi) const;
	bool WetBusy(int32 Ti, float StandX, FString& Why) const;
	bool WetMay(int32 Job);
	void WetIn(int32 Job);
	TFunction<void(float, float, FDriverPose&)> WetClimbFn(int32 Ti, bool bUp) const;
	void WetArrive(int32 Job);
	void WetWork(int32 Job);
	void WetLost(int32 Job);
	TFunction<void(float, float, FDriverPose&)> FuelPlatFn(int32 Ti, bool bUp) const;
	/** The driver's way into a tall cab (two steps: a point IK_Step_DoorDriver_Low). */
	TFunction<void(float, float, FDriverPose&)> TallClimbFn(int32 Ti, bool bIn) const;
	bool IsTall(const FTractor& T) const { return T.Pt.Contains(TEXT("IK_Step_DoorDriver_Low")); }
	void SvcLivery(FTractor& T);
	/** The vehicle standing at its place for its crew (not with you). */
	bool SvcHere(int32 Job) const;
	void SvcReturn(int32 Job, int32 m);
	void SvcFetch(int32 Job);
	// ---- you in it (ServiceDrive.cpp)
	bool SvcYours(const FTractor& T) const;
	bool SvcSeatFree(int32 Ti, int32 Seat, FString& Why) const;
	FVector SvcStandL(int32 Ti, int32 Seat) const;
	FVector SvcStandW(int32 Ti, int32 Seat) const;
	float SvcFaceYaw(int32 Ti, int32 Seat) const;
	float SvcClimbTime(int32 Ti, int32 Seat, bool bIn) const;
	void SvcSeatDoor(int32 Ti, int32 Seat, bool bOpen);
	bool SvcSeatDoorOpen(int32 Ti, int32 Seat) const;
	bool SvcRoomAt(int32 Ti, int32 Seat, const AActor* A, const AActor* B) const;
	TFunction<void(float, float, FDriverPose&)> SvcHandFn(int32 Ti, int32 Seat) const;
	TFunction<void(float, float, FDriverPose&)> SvcClimbAny(int32 Ti, int32 Seat, bool bIn) const;
	void SvcPlayerDrive(FTractor& T, float Dt);
	/** A crew member for a vehicle's job: Pre - the hired person claimed for it; Boss - the vehicle they help at (hired when its driver is). */
	int32 Borrow(const FVector& Near, int32* Pre = nullptr, int32 Boss = INDEX_NONE, EStaffDuty Duty = EStaffDuty::BaggageHandler);
	void GiveBack(int32 Ci, float StandX = 0.f);
	// your hired people at your own flights' bags (to-do 206, Private/GSEStaff.cpp): the jobs that wait for them
	enum class EWant : uint8 { Arr, Dep, Loader };
	struct FWant { EWant Kind = EWant::Dep; FString Key, Why; float StandX = 0.f; };
	TArray<FWant> Wants; float WantClock = 0.f;
	int32 PreDriver = INDEX_NONE, PreLoader = INDEX_NONE;   // (claimed just now for the vehicle that starts next)
	bool HiredFlight(const FString& Key, float StandX, FDateTime* When = nullptr, float* Stand = nullptr) const;
	bool IsHired(int32 Ci) const;
	bool DriverLost(const FTractor& T) const;
	bool FindTrain(int32& Ti, int32& A, int32& B) const;
	void AddWant(EWant Kind, const FString& Key, float StandX);
	FString WantKey(const FWant& W) const;
	bool TakeTeam(const TArray<UStaffPeople::FTaken>& Team);
	void DropPre();
	void WantTick(float Dt);
	bool WaitingAt(float StandX) const;
	void HiredWay(int32 Ci, const FVector& Near, float StandX);
	int32 BorrowHired(const FVector& Near, int32 Boss, EStaffDuty Duty);
	void GiveBackHired(int32 Ci, float StandX);
	bool Relieve(int32 Ci, bool& bDrop);
	void LeftBehind(int32 Ti);
	void WalkTo(int32 Ci, TFunction<FVector()> To, float Stop = 20.f);
	void ClimbIn(int32 Ci, int32 Ti, TFunction<void()> Then);
	void ClimbOut(int32 Ci, int32 Ti, TFunction<void()> Then);
	TFunction<void(float, float, FDriverPose&)> SeatFn(int32 Ti) const;
	TFunction<void(float, float, FDriverPose&)> ClimbFn(int32 Ti, bool bIn) const;
	void CoupleCart(int32 Ci, int32 Ti, int32 Cart, int32 Front, TFunction<void()> Then);   // Front: the cart it couples to (INDEX_NONE: the tractor)
	void UncoupleCart(int32 Ci, int32 Ti, int32 Cart, int32 Front, TFunction<void()> Then);
	void CoupleGpu(int32 Ci, int32 Ti, int32 Gate, bool bOn, TFunction<void()> Then);
	void Curtains(int32 Ci, int32 Ti, bool bOpen, TFunction<void()> Then, const FVector* Toward = nullptr);
	void Do(int32 Ci, TFunction<void()> Fn);
	void Wait(int32 Ci, float S);
	void WaitFor(int32 Ci, TFunction<bool(float)> Until);
	void PoseAct(int32 Ci, TFunction<void(float, float, FDriverPose&)> Fn, float Dur);
	void Hold(int32 Ci, TFunction<void(float, float, FDriverPose&)> Fn);
	FVector GpuEye(int32 Gate) const;
	void GpuFollow(int32 Gate, const FVector& Eye, bool bReverse, float Dt);
};
