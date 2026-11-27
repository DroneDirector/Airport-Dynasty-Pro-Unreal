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
#include "Airfield.h"
#include "VehicleDirt.h"
#include "StaffPeople.h"
#include "ServiceVehicles.h"
#include "RunwayRadio.h"
#include "GSE.generated.h"

class ABabylonModel;
class ATugs;
class UAudioComponent;
class UInstancedStaticMeshComponent;
struct FDriverPose;
struct FSvcRefuel;
struct FSvcCater;
struct FSvcClean;
struct FSvcBus;
struct FSvcWet;
struct FSvcStairs;
struct FSvcDeicer;
struct FSvcAmbu;
struct FVehTable;

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
	/** For people on foot only: the vehicles' outlines and, in a hall with a check on, the aircraft's gear and engines
	 *  (Data/aircraft-workpoints.json keepout, GSEMechWork.cpp) - the vehicles keep to Outlines. */
	void WalkOutlines(TArray<FOutline>& Out) const;

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
		FVehicleDirt Dirt; FGlassWipers GlassWip;   // (GlassWip: its wipers on the rain, GlassRain.h)
		FString Name;
		// its own measures (root cm; the tractor's or the belt loader's - Art/<model>/<model>-parts.json)
		bool bLoader = false;
		int32 Svc = INDEX_NONE;           // a service vehicle (GSEService.cpp): its line in the table
		int32 TugIx = INDEX_NONE;         // not a vehicle of its own: the seat line of that pushback tug (TugYou.cpp)
		bool IsTrain() const { return !bLoader && Svc == INDEX_NONE && TugIx == INDEX_NONE; }   // (a baggage tractor)
		float KDoorDeg = 80.f;
		TWeakObjectPtr<class AServiceDrive> Player; int32 PlayerSeat = INDEX_NONE;   // (you in it: ServiceDrive.h; 0 the driver's seat, then the crew's, then the Cab seats)
		float SteerWant = 0.f, BackT = 0.f, PedalA = 0.f, PedalB = 0.f, WipeT = 0.f, WipeNow = 0.f, RainIn = 0.f, BoxT = 0.f; bool bRain = false, bUsed = false;   // (bUsed: full bags or used rolls in it - exchanged shut in its box)
		int32 GPedal[2] = {INDEX_NONE, INDEX_NONE}, LoadWant = INDEX_NONE; TArray<int32> GWiper; FString Livery;
		TSharedPtr<FSvcRefuel> Fu;        // (the refueller's own parts: GSEFuel.cpp)
		// an apron bus let down on its air springs (GSEBus.cpp): how far, 0..1, its left and right side; your switch's wish; the pole or strap in your hand
		float Knelt[2] = {0.f, 0.f}; bool KneelAsk[2] = {false, false}; int32 YouHang = INDEX_NONE;
		FTransform KneelX() const { return FTransform(FRotator(0.f, 0.f, FMath::RadiansToDegrees(FMath::Atan(7.f * (Knelt[1] - Knelt[0]) / 220.f))), FVector(0.f, 0.f, -3.5f * (Knelt[0] + Knelt[1]))); }
		uint8 YouOn = 0; FVector YouAt = FVector(1e6); float LeftOpen = 0.f; mutable double YouStopAt = -10.; FVector YouHand = FVector::ZeroVector;   // (when it last waited because of you; what you carry of it, root cm)
		bool bSwHaz = false, bSwNoBeacon = false, bSwWork = false, bSwLight = false;   // (VehicleControls.h: you on one of its floors, your feet in its frame; the switches)
		// the shared cab controls (CabControls.h): gear 1 D / 0 N / -1 R (by hand: it doesn't change by itself), beam 0 dipped / 1 main / 2 off, signal +1 left / -1 right, wipers 0 by the rain / 1 slow / 2 fast / 3 off
		bool bPark = true, bPto = false, bGearHand = false, bSigArmed = false; int8 Gear = 1, Sig = 0; uint8 Beam = 0, WipMode = 0; float PtoT = 0.f, CabStand = 0.f, CabNow[3] = {0.f, 0.f, 0.f}; int32 GCab[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
		TSharedPtr<FSvcAmbu> Am;          // (the ambulift's: GSEAmbulift.cpp)
		TSharedPtr<FSvcDeicer> Di;        // (the de-icing truck's: GSEDeicer.cpp)
		TSharedPtr<FSvcStairs> Ps;        // (the passenger stairs': GSEStairs.cpp)
		TSharedPtr<FSvcWet> Wa;           // (the water and the toilet truck's: GSEWater.cpp)
		TSharedPtr<FSvcClean> Cl;         // (the cleaning van's kit in your hands: GSECleanYou.cpp)
		TSharedPtr<FSvcCater> Ct;         // (the catering high-loader's: GSECater.cpp)
		// (breaking down, GSEBreak.cpp: 0 well, 1 failing - on to where it may stop, 2 stopped for repair, 3 written off, 4 gone, 5 repaired, its driver getting in again)
		uint8 Brk = 0; bool bBrkFix = true, bBrkHome = false, bBrkBack = false, bBrkHaz = false; FString BrkId, BrkFault; FDateTime BrkUntil, BrkLast; double BrkHours = 0.0, BrkKm = 0.0; float BrkT = 0.f; int32 BrkJob = INDEX_NONE;
		bool bSpare = false, bAway = false; FString SpareId, Owner;   // (a bought one, kept at the vehicle area: kind@place and whose - GSESpare.cpp)
		TSharedPtr<FSvcBus> Bu; TSharedPtr<struct FBusCharge> Ch;           // (an apron bus's doors, driver and sounds: GSEBus.cpp)
		FVector KWheelAx = FVector(-0.7071f, 0.f, 0.7071f);   // (its steering wheel's axis)
		TMap<FString, FVector> Pt;        // (a service vehicle's named points, root cm: Data/<vehicle>-riders.json)
		struct FSvcDoor { int32 G = INDEX_NONE; FVector Piv = FVector::ZeroVector; float Now = 0.f, Want = 0.f; };
		TArray<FSvcDoor> SvcDoors;        // (as the table's doors: 0 shut .. 1 open)
		struct FSvcItem { int32 G = INDEX_NONE; FVector Piv = FVector::ZeroVector; int32 Ci = INDEX_NONE; bool bShow = true, bMoved = true; };
		TArray<FSvcItem> SvcItems;        // (as the table's items: in their place, or in somebody's right hand)
		FVector KWheel[4], KKing[2], KHinge, KWheelC, KStand, KHandle, KStep, KGrabOut, KGrabIn, KSeat, KBrake, KAccel, KHandL, KHandR;
		float KRadF = 26.f, KRadR = 31.f, KWB = 158.f, KTrack = 112.f, KFloor = 62.f, KHalfL = 171.f, KHalfW = 72.f, KCentreX = 3.f, KTurnR = 300.f, KMaxSteer = 32.f;
		FString SndStart, SndLoop, SndStop, SndDoorOpen, SndDoorClose, SndBeep, SndHorn = TEXT("BTHorn");
		// the real lights its lenses throw (AGSE::BuildBeams): one per lamp group (kind, side, front/back), on with the group's lens
		struct FBeam { TWeakObjectPtr<class ULocalLightComponent> L; int32 Lens = INDEX_NONE; float Full = 0.f; float Now = -1.f; };
		TArray<FBeam> Beams;
		// the belt loader: its boom (0 down .. -20.5 deg up), the rams, the bumper, the belt running
		float Boom = 0.f, BoomWant = 0.f; bool bBelt = false;
		// (worked by you, GSELoaderYou.cpp: the belt back, a stop pressed; the stand whose bulk door it is at, how far that is)
		bool bBeltBack = false, bLStop = false, bLNear = false; int32 GJoy = INDEX_NONE; float JoyNow = 0.f;
		int32 YBar = INDEX_NONE; float YLeftT = 0.f, YNoteT = 0.f, YTryT = 0.f; float YStandX = -1e6f, YJobT = 0.f, YAskT = 0.f, YAtT = 0.f;   // a cart whose drawbar you laid in, not pinned yet; left alone
		float LYouX = -1e6f, LLeftT = 0.f, LDockT = 0.f, LWaitT = 0.f, LAhead = 0.f, LAside = 0.f, LTurn = 0.f;
		int32 CtlKey() const { return TugIx != INDEX_NONE ? -4 : bLoader ? -2 : Svc == INDEX_NONE ? -3 : Svc; }   // (its line of controls)
		int32 GBoom = INDEX_NONE, GBumper = INDEX_NONE, GRam[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
		UAudioComponent* BeltSnd = nullptr; UAudioComponent* BoomSnd = nullptr;
		int32 TopCrew = INDEX_NONE;       // the loader up at the hold door
		bool bBottomHeld = false;         // (a handler about to lay a bag on the belt's bottom end)
		struct FBeltBag { int32 Id = 0; float S = 0.f, T = 0.f; uint8 Stage = 0; };   // (up: 0 riding, 1 at the top, 2 in the hold loader's hands; arrivals down: 5 in his hands, 3 riding down, 4 at the bottom)
		bool bArrival = false;            // a train for an arriving aircraft's bags (hold -> carts -> the arrivals conveyor)
		int32 ArrivalsLeft = 0;           // (the belt loader: arriving bags still in the hold)
		float ArrivalIn = 0.f; FString ArrKey;
		// (the belt loader: its hold person really up; the job over; the bag in your hands: from 1 a cart, 2 the belt's foot, 3 its top, 4 the hold;
		// to 0 carried, 1 the foot, 2 a cart, 3 the hold, 4 the top; on its way there 0..1, from where)
		bool bTopUp = false, bLOver = false; float LHoldAsk = 0.f, LAskT = 0.f;
		int32 LYouBag = INDEX_NONE, LYouCart = INDEX_NONE, LYouSlot = INDEX_NONE; uint8 LYouFrom = 0, LYouTo = 0; float LYouMov = 1.f; FTransform LYouA = FTransform::Identity;
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
		uint8 StrapOff[2] = {0, 0};                     // (unhooked by you: 1 the rear two, 2 the front two)
		bool bLoose = false; float YLeftT = 0.f;       // (set down by you beside its place, not in it)
		bool bOut = false; FVector Place = FVector::ZeroVector; float PlaceYaw = 0.f;   // taken by you from there: no crew's cart till it is back
		int32 Tractor = INDEX_NONE;                     // hitched in that tractor's train
		FString Flight;                                 // (the train carries its flight's tag: never the wrong aircraft)
		UAudioComponent* Roll = nullptr;
		FVehicleDirt Dirt;
		bool bMoved = true;
		TArray<int32> Bags;                             // its load slots (18: 3 along x 2 across x 3 high): the bag in each, or INDEX_NONE
	};
	/** A gate GPU being towed: its moving parts (bound when first towed). */
	struct FGpuRig { int32 GWheel[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE}, GAxle = INDEX_NONE; float AxleYaw = 0.f, Spin = 0.f; FVector Home = FVector::ZeroVector; float HomeYaw = 0.f; bool bBound = false; FVector PrevAt = FVector::ZeroVector; };
	/** Every vehicle there is (the tests' camera). */
	const TArray<FTractor>& Vehicles() const { return Tractors; }
	/** Vehicles owned (to-do 318): yours of these, with their condition from the wear (GSEBreak.cpp), state and place. */
	void OwnedList(TArray<struct FOwnedVehicle>& Out) const;
	/** Its condition 0..100 from the hours and km behind it (the rates of Data/vehicle-breakdowns.json: worn out at oldAtHours / oldAtKm). */
	static int32 BreakCondition(double Hours, double Km);
	/** The kinds with no place at the vehicle area (GSEExtra.cpp): a box on the plan (metres, heading degrees, half length and width). */
	struct FExtraBox { FVector2f P = FVector2f::ZeroVector; float H = 0.f, L = 0.f, W = 0.f, StandX = 0.f; };
	/** A free spot for one more of this kind beside its own (ADPOwned::ExtraSpot). */
	bool ExtraSpot(const struct FADPWorld& Wd, const FString& Key, struct FADPSpare& Out, FString& Why) const;
	/** The condition 0..100 of a tug or fire truck from its wear record (GSEBreak.cpp WearOthers); -1: none yet. */
	static int32 WearCondition(const UObject* Ctx, const FString& Id);
	/** A fire truck's wear record: bought ones by their purchase, the airport's own by their order. */
	static FString FireWearId(const AActor* F);

private:
	friend class ATugs;
	friend class AServiceDrive;
	friend class AVehicleControls;
	// your hands on a vehicle's own controls (VehicleControls.h): its table (none: no controls yet)
	static const FVehTable* CtlTable(int32 Svc);
	// the shared cab controls (CabControls.h)
	static void CabAdd(struct FVehTable& Tb, int32 Svc);
	void CabDress(FTractor& T);
	FString CabSay(const FTractor& T, const struct FVehControl& C) const;
	bool CabMay(int32 Ti, int32 Code, FString& Why) const;
	void CabSwitch(int32 Ti, int32 Code);
	void CabDrive(FTractor& T, float& Go, float& Br);
	void CabLeave(FTractor& T);
	void CabTick(FTractor& T, int32 Ti, float Dt);
	bool CabMine(const FTractor& T) const;   // (you at its wheel)
	static TSharedPtr<FVehTable> AmbuControls();
	static TSharedPtr<FVehTable> FuelControls();
	static TSharedPtr<FVehTable> WaterControls();
	static TSharedPtr<FVehTable> ToiletControls();
	static TSharedPtr<FVehTable> CaterControls();
	static TSharedPtr<FVehTable> CleanControls();
	static TSharedPtr<FVehTable> BusControls();   // (the apron buses, GSEBus.cpp)
	static TSharedPtr<FVehTable> TaxiControls();   // (the taxi bus, TaxiBus.cpp)
	static TSharedPtr<FVehTable> StairsControls();
	/** The belt loader worked by you (GSELoaderYou.cpp). Held: the boom stands (somebody on it or under it). Free: a crew may have it. */
	static TSharedPtr<FVehTable> LoaderControls();
	/** The baggage tractor and its carts worked by you (GSETractorYou.cpp). */
	static TSharedPtr<FVehTable> TractorControls();
	/** The pushback tugs with you at the wheel, and at the nose gear (TugYou.cpp). */
	static TSharedPtr<FVehTable> TugControls();
	bool bTugSeats = false;
	void TugSeats();
	const TCHAR* TowHit(const FTractor& T, const FVector2f& N1, const FVector2f& M1, const FCollisionQueryParams& Q, bool bPlan = false) const;   // (the aircraft on your tow bar)
	void YouTug(int32 Ti, float Dt);
	FVector TugSpot(const FTractor& T, int32 S) const;
	FString TugSay(const FTractor& T, int32 S) const;
	bool TugMay(int32 Ti, int32 S, FString& Why) const;
	TFunction<void(float, float, FDriverPose&)> TugAct(int32 Ti, int32 S, float& Secs) const;
	// the gate's ground power unit by hand (to-do 238, GpuYou.cpp): lines of its tug's table
	static void GpuAdd(FVehTable& Tb);
	static bool GpuIs(int32 S) { return S >= 311 && S <= 324; }
	FVector GpuSpot(const FTractor& T, int32 S) const;
	FString GpuSay(const FTractor& T, int32 S) const;
	bool GpuMay(int32 Ti, int32 S, FString& Why) const;
	bool GpuSwitch(int32 Ti, int32 S);
	TFunction<void(float, float, FDriverPose&)> GpuAct(int32 Ti, int32 S, float& Secs) const;
	bool GpuHand(int32 TugIx, FVector& At) const;
	TFunction<void(float, float, FDriverPose&)> HitchPinPose(int32 Ti, int32 Front, float Grip) const;
	TFunction<void(float, float, FDriverPose&)> CartBarPose(int32 Cart) const;
	TFunction<void(float, float, FDriverPose&)> GpuBarPose(int32 Gate) const;
	TFunction<void(float, float, FDriverPose&)> StrapPose(int32 c, int32 s, float X, float Sx) const;
	TFunction<void(float, float, FDriverPose&)> CurtainPose(int32 c, int32 s) const;
	FVector CartBarHandle(int32 Cart) const;
	FVector GpuPoint(int32 Gate, const FVector& L) const;
	void CartFollow(FCart& C, const FVector& Hitch);
	bool TractorFree(const FTractor& T) const;   // a crew may have it: at home, nothing behind it, not yours
	bool CartFree(int32 c) const;
	bool TractorShut(FTractor& T, bool bSay);
	bool TractorBack(FTractor& T, bool bSay);
	bool TrainOwn(const FTractor& T, const FVector& At) const;
	bool TrainDrag(FTractor& T, const struct FCollisionQueryParams& Q, float Dt);
	FVector TrainHitch(const FTractor& T) const;
	int32 TrainLast(const FTractor& T) const { return T.Train.Num() ? T.Train.Last() : INDEX_NONE; }
	int32 HitchCart(const FTractor& T) const;
	int32 HitchGpu(const FTractor& T) const;
	bool CurtainNear(const FTractor& T, int32& c, int32& s) const;
	FString TractorSay(int32 Ti, int32 S) const;
	bool TractorMay(int32 Ti, int32 S, FString& Why) const;
	FVector TractorSpot(int32 Ti, int32 S) const;
	TFunction<void(float, float, FDriverPose&)> TractorAct(int32 Ti, int32 S, float& Secs) const;
	void TractorDo(int32 Ti, int32 S, int32 c, int32 s, float Want, bool bEnd);
	void YouTractors(float Dt);
	int32 TractorWhere(const FTractor& T) const;
	void TractorGpu(int32 Ti);
	FString TrainFlight(const FTractor& T) const;
	void TractorTake(int32 Ti);
	void TrainTidy(int32 Ci, int32 Ti);
	// (its bags by your hands, your train as a flight's own, ways from anywhere indoors: GSETractorBags.cpp)
	FString BagOf(int32 Id) const;
	FString TrainTakes(int32 Ti) const;
	bool TrainHands(int32 Ti) const;
	int32 TrainYouBag() const;
	int32 TrainYours() const;
	bool TrainPlace(bool bBag, int32& Ti, int32& Cart, int32& Slot) const;
	int32 CarouselBag() const;
	bool AtConveyor() const;
	FString TrainBagSay(int32 Ti, int32 S) const;
	bool TrainBagMay(int32 Ti, int32 S, FString& Why) const;
	FVector TrainBagSpot(int32 Ti, int32 S) const;
	bool TrainBagDo(int32 Ti, int32 S);
	void TrainBagDrop(FTractor& T);
	void TrainBags(int32 Ti, float Dt);
	int32 YouTrainFor(const FString& F) const;
	bool YouAtCarousel(const FString& F) const;
	bool TrainInPlace(const FTractor& T, float StandX) const;
	void YouTrain(int32 Ti, float Dt);
	bool TrainByConveyor(const FTractor& T) const;
	bool TrainJoin(int32 Ti, const FString& F, float StandX, TFunction<void()> Then);
	bool YouArrTrain(float StandX, const FString& Key);
	bool WayClear(const FTractor& T, const FVector& At, float Yaw, const TArray<FOutline>& Ol, const struct FCollisionQueryParams& Q) const;
	bool WayTo(const FTractor& T, float x, float y, float hx, float hy, TArray<FVector>& Out) const;
	bool WayIn(const FTractor& T, int32 Kind, TArray<FVector>& W) const;
	void TrainAway(int32 Ti, int32 Kind);
	bool TowHit(const FTractor* T, const FVector& Cc, float Yaw, float Hl, float Hw, int32 OwnGpu, const struct FCollisionQueryParams& Q) const;
	bool CartHit(const FCart& C, const FTractor* T) const;
	void CartFetches(float Dt);
	mutable float YCarT = 0.f; mutable int32 YCarId = INDEX_NONE; int32 YFetch = INDEX_NONE; bool bYFetchOn = false; float YFetchT = 0.f;
	void TractorHome(int32 Ti);
	void TractorIn(int32 Ti);
	void TractorDrop(int32 Ti);
	bool LoaderHeld(const FTractor& T, float To) const;
	bool LoaderFree(const FTractor& T) const;
	void YouLoader(int32 Li, float Dt);
	void LoaderTake(int32 Li, const FString& Key, bool bTrain);
	void LoaderBelt(FTractor& T, int32 Way);
	// (your hands on its bags, its hold person, what its flight waits for: GSELoaderYou.cpp, GSELoader.cpp)
	void HoldUp(int32 Li);
	TFunction<void(float, float, FDriverPose&)> BeltClimb(int32 Li, bool bUp) const;
	int32 LoaderTrain(const FTractor& L, bool* bArr = nullptr) const;
	bool LoaderCartBag(const FTractor& L, int32& Cart, int32& Slot) const;
	bool LoaderCartFree(const FTractor& L, int32& Cart, int32& Slot, bool bMine = false) const;
	FTransform LoaderBagAt(const FTractor& L) const;
	void LoaderHand(FTractor& L, int32 Id, uint8 From);
	void LoaderLay(FTractor& L, uint8 To);
	void LoaderDrop(FTractor& L);
	void LoaderBags(int32 Li, float Dt);
	void LoadBy(int32 Ci, int32 Ti, int32 Li, int32 Who);
	void LoadEnd(int32 Ci, int32 Ti, int32 Li);
	void ArrBy(int32 Ci, int32 Ti, int32 Who);
	static TSharedPtr<FVehTable> DeicerControls();
	void YouCater(FTractor& T, float Dt);
	bool CaterStowed(const FTractor& T) const;
	bool CaterHeld(const FTractor& T, int32 Part, float To) const;   // (that part can't move that way: somebody under it, the aircraft, the stop)
	bool CaterUnder(const FTractor& T, const FVector& Mn, const FVector& Mx) const;   // (you, off it, or one of the ground crew in that box, root cm)
	bool CaterFree(const FTractor& T, int32 Tr, const FVector2D& From, const FVector2D& To) const;   // (a trolley's way: walls, rails, the others, you)
	bool CaterTake(const FTractor& T, int32 Tr, float Dir, FString& Why) const;
	void YouFuel(FTractor& T, float Dt);
	void YouWater(FTractor& T, float Dt);
	void YouToilet(FTractor& T, float Dt);
	float YouLeft(const FTractor& T, float StandX) const;   // (what that aircraft still needs of it, 0 none)
	bool WetLiftHeld(const FTractor& T, float To) const;   // (the platform can't move that way: you under it, your head at the aircraft, the stop)
	bool FuelStowed(const FTractor& T) const;
	bool WaterStowed(const FTractor& T) const;
	/** The waiting turn-round job (fuel / water / toilets) whose aircraft has Pt (its root) within Max of Want (truck root): its place in C. */
	int32 YouJob(const FTractor& T, const FVector& Pt, const FVector& Want, float Max, FVector& C, const AActor** Pl = nullptr) const;   // (its key; none: INDEX_NONE)
	/** What you fuelled or filled by hand, taken off what that aircraft still needs; nothing left: the job is done. */
	bool YouDone(FTractor& T, float StandX, float Amount);   // (false: it needs no more)
	bool YouServe(int32 Svc, float StandX) const;
	void YouSee(FTractor& T);
	FString AmbuWhy(const FTractor& T, int32 Part, float To) const;
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
	/** The same in a stand's frame (Airfield.h FStandFrame: Side, Aft and the way as on a terminal stand). */
	FVector PlanPose(const FAirfieldData::FStandFrame& Fr, float Side, float Aft, float Hx, float Hy, float& Yaw) const;
	FVector PlanAt(const FAirfieldData::FStandFrame& Fr, float Side, float Aft) const;
	bool SpawnTractor(FTractor& T);
	void BuildBeams(FTractor& T);
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
	/** Worked out terminal-way round StandGeo's S and N (nose to -y), placed as the stand at StandX really faces (FStandFrame::Turn):
	 *  a terminal stand's come out exactly as Plan, PlanPose and Leg. AftY: a world point's terminal-way plan y. */
	FVector PlanT(float StandX, float X, float Y) const;
	FVector PoseT(float StandX, float X, float Y, float Hx, float Hy, float& Yaw) const;
	void LegT(float StandX, TArray<FVector>& Out, float X0, float Y0, float Hx0, float Hy0, float X1, float Y1, float Hx1, float Hy1, float R) const;
	double AftY(float StandX, const FVector& W) const;
	FAirfieldData::FStandFrame StandFrame(float StandX) const;
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
	void LoaderHome(int32 Li);
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
	void ArrivalIn(int32 Ti);
	void ArrUnloadBy(int32 Ci, int32 Ti, int32 Count, int32 Belt);
	void ArrUnloadEnd(int32 Ci, int32 Ti, int32 Count);
	FString ArrLabel(const FString& Key, float StandX);
	int32 ArrBelt(const FString& Key, float StandX);
	TMap<FString, float> ArrStand;   // (where each arriving flight's bags came from)
	bool ArrivalFor(float StandX, const FString& Key);
	TArray<TPair<double, int32>> PendingRemove;   // (bags handed over to the arrivals conveyor: gone when its own bag appears)
public:
	/** Arrival at a stand (the turnaround's bulk task): the belt loader and an arrivals train for its hold's bags. */
	void ArrivalAt(float StandX, const FString& Key)
	{
		ArrStand.Add(Key, StandX);
		if (HiredFlight(FString(), StandX)) { float S = 0.f, N = 0.f, Sp = 36.f; if (StandGeo(StandX, S, N, Sp) && Sp <= 40.f) AddWant(EWant::Arr, Key, StandX); return; }
		EnsureLoader(StandX, Key); ArrivalFor(StandX, Key);
	}
	/** To-do 206: your own flight's bags wait for your people (Why: who is missing), or its train is being got ready. */
	bool HiredWaits(const FString& Flight, float StandX, FString& Why) const;
	bool LoaderWaits(float StandX, FString& Why) const;
	/** The taxi bus's fare (to-do 239b piece 3, GSEService.cpp): on - the roof sign goes dark and the taximeter runs; off -
	 *  the fare is settled, the sign lit again while it is on duty. Who: for the log. Called by the meter's own button and by
	 *  the passenger hand-over (piece 4, TaxiBoard / TaxiTripTick). */
	void TaxiFare(bool bOn, const TCHAR* Who);
	bool TaxiFareOn() const { return bTaxiFare; }
	FString TaxiMeterSay() const;
	/** (to-do 239b piece 4) an arriving passenger at the hall's doors asks for the taxi bus: true = it is at its kerb with its
	 *  driver at the wheel, free, and now kept for Who (Stand: its sliding door's stand point, world - where they walk to). */
	bool TaxiClaim(const FString& Who, FVector& Stand);
	/** Who stands at the sliding door: true = taken over (A is the crew's now: in, to a rear seat, the fare runs, driven off and
	 *  gone unseen); false = gone or kept for someone else, they walk on themselves. */
	bool TaxiBoard(AActor* A, const FString& Who, const FString& Name);
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
	bool SvcAsk(const TCHAR* Task, float StandX, const FString& Reg, const FString& Flight, const FDateTime& When, float Litres = 0.f, const FDateTime& Want = FDateTime(0), bool bRear = true);   // (Want: the stairs' order - on or off blocks)
	void SvcGo(const TCHAR* Task, float StandX);
	bool SvcBusy(const TCHAR* Task, float StandX) const;
	/** The passenger stairs (GSEStairs.cpp): at a stand without a jetway. Has: they are asked for, on their way or there. Busy:
	 *  the flight waits for them - bLeaving false: not yet ready for people; true: not yet away from the aircraft. */
	bool StairsHas(float StandX) const;
	bool StairsQueued(float StandX) const;   // (asked for, and had to wait for another aircraft's)
	bool StairsReach(float StandX, int32 Door, float& SillCm) const;
	bool StairsBusy(float StandX, bool bLeaving, FString& Why) const;
	/** Ready at that stand's door (0 front left, 1 rear left): where to queue, the treads and the platform (feet), the door. */
	struct FStairsWay { int32 Ti = INDEX_NONE; FVector Foot = FVector::ZeroVector, Platform = FVector::ZeroVector, Door = FVector::ZeroVector, Up = FVector::ForwardVector; TArray<FVector> Steps; };
	bool StairsReady(float StandX, int32 Door, FStairsWay& Out) const;
	/** (to-do 296, StandAsk.cpp) Could the airport ever bring them; 0 not coming (Why), 1 on their way, 2 ready; an urgent request. */
	bool StairsCan(float StandX, FString& Why) const;
	int32 StairsWhere(float StandX, FString& Why) const;
	void StairsUrge(float StandX, int32 Level);
	/** Anybody on them, step by step with a hand on the rail, keeping to the right (K 0..1, from the ground to the platform or
	 *  back, over StairsSeconds); Enter +1 before the first step, -1 after the last: they never move under people. */
	void StairsPose(int32 Ti, bool bUp, float K, float PH, FDriverPose& P) const;
	float StairsSeconds(int32 Ti) const;
	void StairsEnter(int32 Ti, int32 By);
	/** The hangar tow's brake rider (to-do 311 B, GSEMaint.cpp), every tick while the tow wants him: one hired man drives the
	 *  maintenance van from the hangar apron to the stand, goes up the jetway's service stair (docked for him) or the passenger
	 *  stairs (called to the front left door) and sits in the cockpit (hidden, carried by Plane). 0 waits (Why), 1 on his
	 *  way, 2 aboard - the jetway or stairs still at the door, 3 aboard and clear: the brakes may come off. */
	int32 MaintRider(float StandX, const FString& Reg, AActor* Plane, FString& Why);
	bool MaintAboard(const FString& Reg) const;
	/** The tow back is on its stand (StandX): he gets off by its jetway or stairs and drives the van home. */
	void MaintOff(float StandX, const FString& Reg);
	/** (to-do 311, GSEHangarStair.cpp) The hangar's own passenger stair: called while the tow brings the aircraft in (bIn false),
	 *  the rider down once it stands in the hall (bIn true); up again for the tow back (3: aboard, the stair parked again). */
	void MaintHall(const FString& Reg, const FString& Hall, AActor* Plane, bool bIn);
	int32 MaintBack(const FString& Reg, const FString& Hall, AActor* Plane, FString& Why);
	bool HallBusy(const FString& Hall) const;   // (its stair or rider still on the move: the hall door stays open)
	/** De-icing (GSEDeicer.cpp), every tick of your own departure's turnaround: bSoon - its doors are shut in ten minutes, bDoors -
	 *  they are shut and the jetway is back. The weather decides; it is asked for, sent and asked for again by itself. */
	bool DeiceLooks(const FString& Flight) const;
	void DeiceTick(float StandX, const FString& Reg, const FString& Flight, const FDateTime& Now, const FDateTime& OffBlocks, bool bSoon, bool bDoors, const FDateTime& OnBlocks = FDateTime(0));
	/** The ambulift (GSEAmbuliftWork.cpp): how many passengers of that flight come in a wheelchair (always the same for a flight of a
	 *  day), its call - every tick while the flight wants it; bGo: the passengers get on or off now -, and the flight waiting for it. */
	static int32 AmbuPax(const FString& Flight, const FDateTime& Day, int32 Pax);
	bool AmbuCall(float StandX, const FString& Reg, const FString& Flight, const FDateTime& When, int32 N, bool bArr, bool bGo);
	bool AmbuBusy(float StandX, bool bArr, FString& Why, bool bMayWait = false) const;   // (bMayWait: the caterers first - they stay in their seats)
	/** The aircraft is gone: what had not started is dropped. */
	void SvcGone(float StandX);
private:
	// Seat 0: the driver; bOut: on their feet by the vehicle; bIn: in the cabin; bDown: off the aircraft again; bGone: lost
	struct FSvcMan { int32 Ci = INDEX_NONE, Seat = 0, Item = INDEX_NONE; bool bSeated = false, bOut = false, bIn = false, bDown = false, bStowed = false, bGone = false, bNoKit = false; };
	struct FSvcJob
	{
		int32 Svc = 0, Ti = INDEX_NONE, Jet = INDEX_NONE; float StandX = 0.f, RowT = 0.f; FString Reg, Flight, Why, WhyLog; FDateTime When, Want; bool bQueued = false;
		uint8 Phase = 0;                  // 0 waits for its people, 1 on the way, 2 at the stand, 3 going back
		bool bGo = false, bFetch = false; FVector Place = FVector::ZeroVector; float PlaceYaw = 0.f; int32 Rows = 30, RowsDone = 0; float T = 0.f; TArray<FSvcMan> Men;
		int32 Urgent = 0; float StillT = 0.f; FVector StillAt = FVector::ZeroVector; double PullT = 0.0;   // (how often asked for; on its way: how long it has stood)
		int32 Stage = 0; float Slow = 1.f, Litres = 0.f, Pumped = 0.f; TArray<FVector> InWay; int32 CtDoor = 0, CtTrip = 0, CtDone = 0, CtYou[2] = {0, 0};   // (Ct*: the catering: at which door, its next trip, the doors done as bits) (the refueller's work; the way in, backed out again)
	};
	TMap<int32, FSvcJob> SvcJobs; int32 SvcNext = 1; float SvcClock = 0.f;
	TMap<int32, TPair<int8, int32>> SvcStairUse;   // (a jetway's service stair: which way it is walked now, and by how many)
	static const TCHAR* SvcModel(int32 Svc);
	void SvcSetup();
	void SvcDress(FTractor& T);
	void SvcParts(FTractor& T, float Dt);
	void SvcTick(float Dt);
	// the bought vehicles at the vehicle area (GSESpare.cpp; to-do 277 E1)
	struct FSpareAt { FString Id, Owner, Name; float X = 0.f, Y = 0.f, Heading = 0.f; };
	bool SvcSpawn(int32 i, int32 n, const FSpareAt* Sp);
	void SpareTick(float Dt);
	float SpareT = 0.f; TSet<FString> SpareSaid, SpareOld; int32 SpareLoads = -1;
	// (the kinds kept beside their own, bought: GSEExtra.cpp; the tugs' and fire trucks' wear: GSEBreak.cpp)
	void ExtraTick();
	bool ExtraAnchors(const FString& Key, TArray<FExtraBox>& Out) const;
	bool ExtraDims(const FString& Key, float& L, float& W) const;
	void ExtraBoxes(TArray<FExtraBox>& Out, const struct FADPWorld* Wd, const FString& SkipId) const;
	bool ExtraFits(const FExtraBox& B, const TArray<FExtraBox>& Others, FString& Why) const;
	void WearOthers(float Dt);
	TSet<FString> ExtraDone; float WearT = 0.f; TMap<FString, FVector> WearLast; int32 TugSeatN = 0;
	// (your crews with them, E1b: whose job one may do, its way out to the road and back onto its place, the way there on foot)
	bool SpareOk(const FTractor& T, const FString& AirlineId) const;
	FString SpareAirline(const FString& Reg) const;
	int32 SparePick(int32 Svc, const FString& Reg) const;
	bool SpareIdle(const FTractor& T) const;
	void SpareShow(FTractor& T, bool bShow);
	static bool DepotAt(const FVector& At);
	void HomeOut(const FTractor& T, TArray<FVector>& W, const FVector& ToW) const;
	FVector HomeMouth(const FTractor& T) const;
	void SpareHome(int32 Ti, TFunction<void()> Then);
	// breaking down (to-do 277 E2, GSEBreak.cpp)
	float BrkSlow = 0.f, BrkSave = 0.f; int32 BrkLoads = -1;
	void BreakTick(float Dt);
	static float BreakLimp();
	void BreakLoad(FTractor& T);
	void BreakStore(const FTractor& T);
	bool BreakSafe(const FTractor& T) const;
	int32 BreakJob(int32 Ti) const;
	bool BreakRoad(int32 Ti) const;
	void BreakStop(int32 Ti);
	bool BreakSpare(const FTractor& T, int32 Job) const;
	void BreakHand(int32 Ti, int32 Job);
	void BreakCrew(int32 Ti, int32 Job);
	void BreakMend(int32 Ti);
	void BreakGone(FTractor& T, bool bGone);
	void BreakRound();
public:
	/** adp.BreakVehicle: the first vehicle whose name holds this fails now. */
	bool BreakNow(const FString& Name, bool bTotal);
private:
	void DepotGo(int32 Ci, const FVector& Near);
	bool DepotBack(int32 Ci, bool bNorth = false);
	// the taxi bus's hired driver (to-do 239b, GSEService.cpp): from their place on the pavement round its front to the wheel, out
	// and back to the pavement when their shift is over. Stage: 0 none, 1 on the way in, 2 at the wheel, 3 getting out
	// ---- the apron buses (GSEBus.cpp)
public:
	/** A bus's vehicle by its name in Data/apron-buses.json (none: INDEX_NONE). */
	int32 BusOf(const FString& Name) const;
	/** Its hired driver gets in (if nobody is at its wheel), shuts the doors, drives it to a plan point and stands with that
	 *  heading (degrees: 0 plan +x, 90 plan +y); Done is told. False: it can't now, Why says so (ask again). */
	bool BusDriveTo(int32 Ti, const FVector2f& To, float Heading, TFunction<void()> Done, FString& Why);
	/** The passengers' side of it (PaxApron.cpp). Free: standing in nobody's job, not yours. Try: is there a way to that place and
	 *  straight onto it (an apron search: not every frame). Slots: its free seats and the standing places with a pole or a strap,
	 *  bus cm (x its front, y its right). PoseW: a pose in bus cm, standing on the floor or the ground beside it, into the world. */
	struct FBusSee { FTransform X; bool bStands = false, bAtWheel = false, bYou = false, bShut = false, bKnelt = false; };
	struct FBusSlot { int32 Seat = INDEX_NONE; FVector At = FVector::ZeroVector, Face = FVector(1, 0, 0), Hold = FVector::ZeroVector; bool bStrap = false; };
	void BusFree(TArray<int32>& Out) const;
	int32 ChargeOwner(int32 Ti) const;   // (a bus out at work whose charging bay this one may have)
	void ChargePick(TArray<int32>& Free) const;
	bool BusInPlace(const FTractor& T) const;
	bool BusCan(FString& Why) const;
	bool BusHalt(int32 Ti);
	TMap<int32, int32> BusOrder;   // (StandAsk.cpp: an order withdrawn is never driven)
	bool BusSee(int32 Ti, FBusSee& Out) const;
	bool BusOk(int32 Ti, const FString& AirlineId) const;   // (the airport's own, or a bought one of that airline's that its crew may take)
	void BusOwnFirst(TArray<int32>& Free) const;
	bool BusTry(int32 Ti, const FVector2f& To, float Heading) const;
	void BusSlots(int32 Ti, TArray<FBusSlot>& Out) const;
	void BusPoseW(int32 Ti, FDriverPose& P) const;
	FVector BusW(int32 Ti, const FVector& Local) const;
	static FVector2f BusDoorAt(int32 Door);      // (x, and y: which side)
	/** A vehicle on the move over that point (nobody steps in front of it). */
	bool Moving(const FVector& At) const;
	FVector ApronAt(float PlanX, float PlanY) const { return Plan(PlanX, PlanY); }
	/** Back to its own bay, nose out; the driver stays at the wheel (BusLeave). */
	bool BusHome(int32 Ti, TFunction<void()> Done, FString& Why);
	/** Standing: its driver shuts it, gets out and is free again. */
	bool BusLeave(int32 Ti);
	/** A door (0..6: left front, middle, rear; right front, second, third, rear) opened or shut; false with the reason. Never shut on anyone. */
	bool BusDoor(int32 Ti, int32 Door, bool bOpen, FString& Why);
	bool BusDoorIs(int32 Ti, int32 Door, bool bOpen) const;
	bool BusDoorsShut(const FTractor& T) const;
	bool BusSomebodyIn(const FTractor& T, int32 Door) const;   // (in a door's opening)
	/** Somebody who gets on or off this bus (a passenger's own body): its doors never shut on them, it never drives off with them in a doorway. */
	void BusRider(int32 Ti, AActor* Who, bool bOn);
	/** The pole or strap you hold (T.YouHang): where your hand is on it, world. False: none, or out of your reach. */
	bool BusHang(const FTractor& T, const FVector& Feet, FVector& At, FVector& Fingers, FVector& Thumb) const;
	bool BusKnelt(const FTractor& T) const { return T.Bu && (T.Knelt[0] > 0.f || T.Knelt[1] > 0.f); }
private:
	void BusDress(FTractor& T);
	void BusParts(FTractor& T, float Dt);
	void BusKneel(FTractor& T, float Dt);
	void BusSounds(FTractor& T, float Dt);
	class UAudioComponent* SndOn(FTractor& T, const TCHAR* Name, const FVector& At, float Vol, bool bKeep);   // (a sound that goes with it)
	void BusTick(float Dt);
	void BusSetAll(int32 Ti, bool bOpen);
	bool BusMan(int32 Ti, TFunction<void()> Then, FString& Why);
	void BusRoute(int32 Ti, FVector2f To, float Heading, TFunction<void()> Done);
	void BusRouteHome(int32 Ti, TFunction<void()> Done);
	void BusWalkRound(int32 Ci, int32 Ti);
	TFunction<void(float, float, FDriverPose&)> BusClimbFn(int32 Ti, bool bIn) const;
	float BusPoll = 0.f;
	// the apron buses' battery and charging (to-do 277 E3b, GSECharge.cpp)
	void ChargeDress(FTractor& T);
	void ChargeParts(FTractor& T, float Dt);
	void ChargeTick(float Dt);
	void ChargeLoad(FTractor& T, double Soc);
	double ChargeSoc(const FTractor& T) const;
	bool ChargeHolds(const FTractor& T) const;
	bool ChargeEmpty(const FTractor& T) const;
	bool ChargeLow(const FTractor& T) const;
	bool ChargeFor(const FTractor& T, double Cm) const;
	bool ChargeAway(const FTractor& T) const;
	FVector ChargeBay(const FTractor& T, float& Yaw) const;
	void ChargeNeed(const FTractor& T) const;
	FString ChargerBy(const FTractor& T) const;
	bool ChargeGo(int32 Ti, FString& Why);
	void ChargeDrive(int32 Ti);
	bool ChargeCrew(int32 Ti, bool bPlug, FString& Why);
	bool ChargeSpot(const FTractor& T, int32 Code, FVector& Out) const;
	FString ChargeSay(const FTractor& T, int32 Code) const;
	bool ChargeMay(const FTractor& T, int32 Code, FString& Why) const;
	bool ChargeSwitch(int32 Ti, int32 Code);
	const TCHAR* ChargeHeld(const FTractor& T) const;
	float ChargeLeash(const FTractor& T, FVector& From) const;
	static void ChargeDrop(FTractor& T);
	static float ChargeLimp();
	int32 BusStage(const FTractor& T) const;
	float ChargePoll = 1.f;
	void TaxiTick(float Dt);
	float TaxiPoll = 0.f; int32 TaxiCi = INDEX_NONE, TaxiTi = INDEX_NONE; uint8 TaxiStage = 0;
	// its fare (the same tick owns it): on board or not, the distance (m), the time and the time standing or crawling (s)
	bool bTaxiFare = false; float TaxiFareM = 0.f, TaxiFareS = 0.f, TaxiFareWaitS = 0.f; FVector TaxiFareAt = FVector::ZeroVector;
	bool IsTaxi(const FTractor& T) const { return Tractors.IsValidIndex(TaxiTi) && &Tractors[TaxiTi] == &T; }
	bool TaxiOnDuty(const FTractor& T) const;
	float TaxiFareChf() const;
	// its passenger (piece 4): kept for (TaxiFor, since), their crew place, the trip: 0 none, 1 getting in, 2 seated - pulling out,
	// 3 along the forecourt's bus lane, 4 out of sight (hidden, the fare runs for TaxiTripT s), 5 back to its kerb when unseen
	FString TaxiFor, TaxiWho; double TaxiForT = 0.0; int32 TaxiPaxCi = INDEX_NONE, TaxiLegs = 0; uint8 TaxiTrip = 0; float TaxiTripT = 0.f; bool bTaxiHidden = false;
	void TaxiTripTick(float Dt);
	void TaxiShow(bool bOn);
	void TaxiPaxGone(const TCHAR* Why);
	bool TaxiUnseen(const FVector& At) const;
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
	TFunction<void(float, float, FDriverPose&)> SvcDeepFn(int32 Ti, const FVector& At, bool bIn);
	TFunction<void(float, float, FDriverPose&)> SvcHandFn(int32 Ti, const FVector& At, const FVector& Place);
	void SvcReach(int32 Ci, int32 Ti, int32 It, TFunction<void()> Pass);
	// ---- the cleaning van by hand (GSECleanYou.cpp)
	void CleanDress(FTractor& T);
	void CleanParts(FTractor& T, float Dt);
	bool CleanYours(const FTractor& T, int32 It) const;
	bool CleanYouIn(const FTractor& T) const;
	bool CleanStowed(const FTractor& T) const;
	bool CleanHeld(const FTractor& T, FString& Why) const;
	bool CleanInWay(const FTractor& T, int32 Door) const;
	bool CleanItem(const FTractor& T, int32 It, FTransform& X) const;
	bool CleanMay(const FTractor& T, int32 It, FString& Why) const;
	bool CleanDoorAt(const FTractor& T, FVector& At) const;
	int32 CleanDoorAct(const FTractor& T, FString& Say, FString& Why) const;
	void CleanLay(FTractor& T, int32 It, const FVector& Feet, float Yaw);
	void CleanPass(FTractor& T, int32 It);
	void CleanGet(int32 Ci, int32 Ti, int32 It);
	void CleanStow(FTractor& T);
	bool CleanLost(int32 Ci);
	void CleanLast(int32 Job);
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
	// ---- the brake rider (GSEMaint.cpp)
	TMap<int32, FStairsWay> MaintWay;   // (a rider's job: the passenger stairs he climbs - none: the jetway's service stair)
	TMap<int32, float> MaintTo;         // (the stand the tow back brings him to)
	void MaintAt(int32 Job);
	void MaintStairs(int32 Job, float StandX, bool bUp);
	void MaintClimb(int32 Job, float StandX, bool bUp);
	void MaintHold(int32 Job);
	void MaintDown(int32 Job);
	// ---- the maintenance vans' rides across the runway (GSEMaint.cpp, to-do 311): nobody of the hangar tow walks across the strip
public:   // (the maintenance vans' rides: asked for by the tugs' wing walkers too)
	struct FVanRider { int32 Ti = INDEX_NONE, Seat = 0; FVector Drop = FVector::ZeroVector; bool bAboard = false, bOff = false, bDoor = false; FString Who; };
	TMap<int32, FVanRider> VanRiders;  // (by crew index: one ride at a time each)
	TMap<int32, TPair<int32, EJob>> VanWas;   // (a van taken for a ride: its job's driver and state, given back after)
	TMap<int32, uint8> VanTrip;        // (a van: 1 driving its riders, 2 they get out)
	TMap<int32, double> VanClear;      // (a van holding short: since when the runway is free)
	TMap<int32, ADPRunwayRadio::FCall> VanRadio;   // (a van's runway crossing on the tower frequency, hangar step 6: from the holding point to "vacated")
	ADPRunwayRadio::FWho VanWho(int32 Ti) const;    // (who talks for that van: its driver, its radio)
	TMap<int32, FVector> VanSouth;     // (a van: where it was taken from on the stands' side - the drop back there)
	TSet<int32> VanStrip;              // (vans on the runway strip now: departures hold)
	static bool Across(const FVector& A, const FVector& B);
	int32 VanFor(const FVector& Near) const;   // a free maintenance van on Near's side of the runway (else any free one)
	/** Ci waits for a free maintenance van on his side (logged, never walks across), walks to it, gets in by its door, rides
	 *  to Drop (across at the tows' crossing), gets out; then Then. False: there is no maintenance van at all. */
	bool VanRide(int32 Ci, const FVector& Drop, const FString& Who, TFunction<void()> Then);
	void VanBoard(int32 Ci);
	void VanAlight(int32 Ci, TFunction<void()> Then);
	void VanTick();
	void VanCross(int32 Ti, const FVector& To, TFunction<void()> Done);
	bool VanOnStrip() const { return VanStrip.Num() > 0; }
	FVector VanDropSouth(int32 Ti, float StandX) const;
	void MaintToVan(int32 Job);
private:
	// ---- the hangar halls' own stairs (GSEHangarStair.cpp): a copy of the passenger stairs, one per hall, its job's StandX a key
	struct FHallStair { FString Hall; int32 Ti = INDEX_NONE; float Key = 0.f, Hx = 0.f, Yn = 0.f, Front = 0.f, Half = 23.f, Side = 1.f; FVector2f Park = FVector2f::ZeroVector; TWeakObjectPtr<AActor> Plane; mutable TWeakObjectPtr<AActor> Door; };
	TArray<FHallStair> HallStairs;
	TMap<int32, float> MaintHallX;   // (a rider's job: down in that hall, the hall stair's key)
	// ---- a check's hired mechanics in the hall (GSEMechanics.cpp, to-do 311): by van to the hall, they stand by the aircraft, back by van
	struct FMechSpot { FVector At = FVector::ZeroVector, Face = FVector(1, 0, 0); float Hands = 100.f; bool bHold = true; FString What; bool bWork = true, bThere = false; };   // (bWork false: standing by; bThere: arrived)
	struct FMechCrew { FString Hall; TArray<TSharedRef<FMechSpot>> Places; TArray<FMechSpot> All; TArray<int32> Men; uint8 Phase = 1; int32 There = 0, Out = 0, Home = 0; double OutAt = 0.0; TArray<FOutline> Keep; };   // (Phase 1 going in, 2 by the aircraft, 3 walking out, 4 out of the hall; Keep: its aircraft's gear and engines)
	// ---- where they work on it (GSEMechWork.cpp, hangar step 4): the fault's own place from Data/aircraft-workpoints.json
	bool MechSpots(const FString& Reg, const FHallStair& H, int32 Want, TArray<FMechSpot>& Spots, TArray<FOutline>& Keep) const;
	/** The same for any aircraft model Pl and job FaultKey (ADPMaint's fault table key): a job still waiting for its crew too. */
	bool MechSpotsAt(const FString& Reg, const AActor* Pl, const FString& FaultKey, int32 Want, TArray<FMechSpot>& Spots, TArray<FOutline>& Keep) const;
	void MechWork(int32 Ci, const FString& Reg, TSharedRef<FMechSpot> S);
	/** The work pose's hands at a place (the same loop for the hired mechanics and for you): holder both on the part, fastener
	 *  the left on the part and the right turning a tool beside it. */
	static void MechHands(const FVector& Me, const FVector& Face, float Hands, bool bHold, float T, FVector& L, FVector& R);
	// ---- you at work beside them (GSEMechYou.cpp, hangar step 5): E / X at a free work place of a job
	struct FMechYou { FString Reg; FMechSpot S; TWeakObjectPtr<AActor> Plane; FVector PlaneAt = FVector::ZeroVector; };
	FMechYou MechMe;                       // (Reg empty: not at work)
	static FString MechTip;                // (the prompt now, read by the HUD each frame: empty when there is nothing to take or leave)
	FMechSpot MechNear; FString MechNearReg; TWeakObjectPtr<AActor> MechNearPlane; bool bMechNearOk = false;
	float MechLookT = 0.f, MechFindT = 0.f;
	TMap<FString, TWeakObjectPtr<AActor>> MechWaiting;   // (your aircraft whose job waits for crew, by registration: their models here)
	TMap<FString, TArray<FMechSpot>> MechWaitSpots;      // (and their work places, found with them every 2 s)
	void MechYouTick(float Dt);
	void MechYouLeave(const TCHAR* Why);
public:
	/** E / X by a work place: take it, or leave yours. Say: a short line to show. */
	bool MechUse(APawn* Me, FString& Say);
	static const FString& MechPrompt() { return MechTip; }
private:
	TMap<FString, FMechCrew> MechCrews;   // (by registration)
	TMap<FString, double> MechTry;        // (by registration: when a team was last asked for - every 5 s)
	TMap<FString, FString> MechWhy;       // (why it has none: logged once each)
	void MechStart(const FString& Reg, const FHallStair& H);
	bool MechCheckOn(const FString& Reg) const;
	bool MechHolds(const FString& Reg, FString& Why);
	void MechDoor(const FHallStair& H, const FString& Who) const;
	const FHallStair* HallOf(float StandX) const;
	FHallStair* HallNamed(const FString& Hall);
	bool HallTi(int32 Ti) const;
	float HallOpen(const FHallStair& H) const;
	FVector HallP(const FHallStair& H, float X, float Y) const;
	void HallStairsSetup();
	void HallStart(int32 Job);
	void HallLeave(int32 Job);
	bool HallCrowd(int32 Job) const;
	// ---- the passenger stairs (GSEStairs.cpp)
	void StairsDress(FTractor& T);
	void StairsParts(FTractor& T, float Dt);
	void StairsTick(FSvcJob& J, float Dt);
	bool StairsDock(float StandX, int32 Door, float GroundZ, FVector2f& At, FVector2f& Hd, float& N) const;
	bool StairsIn(FSvcJob& J, float FromY, TArray<FVector>& W);
	bool StairsLane(int32 Ti, float StandX) const;
	bool StairsStowed(const FTractor& T) const;
	void StairsWork(int32 Job);
	void StairsHelper(int32 Job);
	void StairsLost(int32 Job, int32 m);
	void StairsLever(int32 Ci, int32 Ti, int32 Lever, float Tip, TFunction<void()> Fn, TFunction<bool()> Done);
	/** You at its ground panel. */
	// yours (to-do 238): not that way now (0 legs, 1 stairs: somebody on it, somebody under a leg, the aircraft); its state at a door;
	// the vehicle of yours at that door (-1: either door), the job waiting there, its crew go on with what you left there
	bool StairsHeld(const FTractor& T, int32 M, float To) const;
	void YouStairs(FTractor& T, float Dt);
	int32 StairsYou(float StandX, int32 Door) const;
	const struct FSvcJob* StairsJob(float StandX, int32 Door) const;
	void StairsTake(int32 Job);
	void StairsSteps(const FTractor& T, TArray<FVector>& Out) const;
	// ---- the ambulift (GSEAmbulift.cpp, GSEAmbuliftWork.cpp)
	TSet<FString> AmbuDone; TMap<int32, FString> AmbuFlown;   // (flights done; a job's flight)
	void AmbuDress(FTractor& T);
	void AmbuParts(FTractor& T, float Dt);
	void AmbuChairs(FTractor& T, float Dt);
	void AmbuRolls(FTractor& T, float Dt);
	void AmbuRestock(FTractor& T);
	bool AmbuStowed(const FTractor& T) const;
	bool AmbuMay(const FTractor& T, int32 Part, float To) const;
	static FVector AmbuTip(const FSvcAmbu& C);
	static FVector AmbuL(const FSvcAmbu& C, const FVector& P);
	FVector AmbuOn(int32 Ti, const TArray<FVector>& P, bool bLocal, float S) const;
	FTransform AmbuChairX(int32 Ti, int32 i) const;
	float AmbuBlue(const FTractor& T, int8 Side) const;
	void AmbuPress(int32 Ti, const TCHAR* Key);
	void AmbuGo(int32 Ci, int32 Ti, int32 Chair, TFunction<TArray<FVector>()> Line, bool bLocal);
	void AmbuHold(int32 Ci, int32 Ti, FVector At, FVector Face);
	void AmbuHand(int32 Ci, int32 Ti, FVector At, TFunction<FVector()> To, TFunction<void()> Fn, TFunction<bool()> Is, bool bBoth, float MaxT);
	bool AmbuSeat(int32 Ti, int32 i);
	void AmbuUnseat(int32 Ti, int32 i);
	bool AmbuRoute(const FVector& From, const FVector& To, TArray<FVector>& Out) const;
	bool AmbuIs(const FSvcJob& J) const;
	bool AmbuAtFront(float StandX, bool bSoon) const;
	bool CaterAtFront(float StandX) const;
	void AmbuArrive(int32 Job);
	void AmbuStage(int32 Job);
	void AmbuTail(int32 C0, int32 C1, int32 Ti, float To);
	void AmbuFrontRails(int32 Job, bool bOut);
	void AmbuDoor(int32 Job, int32 i, bool bFetch);
	void AmbuTerminal(int32 Job, int32 i, bool bFetch);
	void AmbuSpin(int32 Ci, int32 Ti, int32 i);
	void AmbuEnd(int32 Job);
	void AmbuLost(int32 Job, int32 m);
	// ---- the de-icing truck (GSEDeicer.cpp)
	struct FDeiced { FDateTime Until = FDateTime(0); int32 Rounds = 0; bool bFrost = false; };   // (until when the fluid holds; how often it was done; bFrost: no holdover, looked over when it goes)
	struct FOnIt { float Mm = 0.f; bool bSnow = false; FDateTime On = FDateTime(0), Last = FDateTime(0); };   // (what lies on an aircraft: mm of water, snow among it; its on-blocks, when last counted)
	TMap<FString, FOnIt> OnIt; TMap<FString, FDateTime> DeiceLook;   // (by registration; by registration/flight#round: until when it is looked over)
	TMap<FString, FDeiced> Deiced; FDateTime DeiceNow = FDateTime(0);
	void DeicerDress(FTractor& T);
	void DeicerParts(FTractor& T, float Dt);
	void DeicerSpray(FTractor& T, float Dt, const FTransform& Noz);
	void DeicerRestock(FTractor& T);
	bool DeicerStowed(const FTractor& T) const;
	FTransform DeicerCab(const FTractor& T) const;
	bool DeicerLane(int32 Ti, float StandX, int32 Stop, FString& Why) const;
	void DeicerApron(int32 Job);
	bool DeiceSnow(const FString& Reg) const;
	bool DeicerFree(int32 Ti, TArray<FVector>& Out, bool& bRev) const;
	bool DeicerGets(int32 Ti);
	bool DeicerBack(int32 Ti, TFunction<void()> Then);
	void DeicerIn(int32 Job, float Y1);
	void DeicerOut(int32 Job, bool bEnd);
	void DeicerEnd(int32 Job);
	void DeicerGone(int32 Job);
	void DeicerPlan(FSvcJob& J);
	void DeicerTick(int32 Job, float Dt);
	TFunction<void(float, float, FDriverPose&)> DeicerClimbFn(int32 Ti, bool bUp) const;
	TFunction<void(float, float, FDriverPose&)> DeicerSeatFn(int32 Ti) const;
	void DeicerOpIn(int32 Job);
	void DeicerOpOut(int32 Job);
	void DeicerLost(int32 Job, int32 m);
	void DeicerYours(FTractor& T, float Dt);
	bool DeicerShut(const FTractor& T) const;
	bool DeicerYou(float StandX) const;
	void DeicerNear(FTractor& T, float Dt);
	void DeicerPanel(FTractor& T, float Dt);
	void DeicerNote(FTractor& T, const FString& Reg, const FString& Flight, bool bOk, const FString& How);
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
	/** The cab's seat in the one record (ASeats, to-do 293): claimed before they walk, again at the door; never while you are on it. */
	AActor* CabOf(int32 Ti) const;
	void ClaimSeat(int32 Ci, int32 Ti, int32 Seat);
	void SeatTaken(int32 Ci);
	void SeatLeft(int32 Ci);
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
	void PoseAct(int32 Ci, TFunction<void(float, float, FDriverPose&)> Fn, float Dur, bool bMove = false);   // (bMove: on the move - a beginner is no slower)
	void Hold(int32 Ci, TFunction<void(float, float, FDriverPose&)> Fn);
	FVector GpuEye(int32 Gate) const;
	void GpuFollow(int32 Gate, const FVector& Eye, bool bReverse, float Dt);
};
