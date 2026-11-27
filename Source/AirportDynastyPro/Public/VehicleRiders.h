// The people of a road vehicle: its hired driver and its passengers, with full IK, and its driving (owner, 2026-11-23: "make
// sure the blue coaches are working with IK support and everything. But still keep them static. Put the features in place so
// it is possible" - the airport coaches now, the apron bus and the taxi bus with a body and a points file of their own later).
//
// EVERYTHING HERE IS OFF: adp.CoachLine 0 (the default). Off, no component exists, nothing ticks, the coaches stay parked and
// nobody can board them (every call below refuses); PaxFlow knows nothing of coaches. The player's own walking in and sitting
// down (Coaches, Seats) is not part of this. The developer test is -CoachIKTest (command line only, CoachIKTest.cpp).
//
//  - the named points come from Data/<vehicle>-riders.json (FVehicleSpec); what moves - doors, bays, lamps, wheels, pedals -
//    is the vehicle's own (IVehicleBody; ACoaches gives one per coach)
//  - the driver (an NPC, never you): walks to the front door, opens it, climbs the steps with a hand on the rail, steps down
//    to the driver's floor, sits (the seat's arm up, then down), hands on the wheel following its turn and re-gripping, the
//    right foot on the accelerator or the brake, the left hand to the indicator, the right hand to the door buttons, looks
//    into the mirrors; gets out the same way back
//  - passengers: wait at the stop, put a bag into the luggage bay (themselves, or the driver / a handler loads it), climb in,
//    walk the aisle (holding a grab handle while the vehicle moves), pick a free seat - shared with your own sitting (ASeats:
//    taken is blocked) - sit with the sitting IK step, get up, get off, take the bag
//  - driving: a lane's path (FVehicleLane, from roads.json), the front axle on it and the rear one trailing (the front wheels
//    and the steering wheel turn by it), real speeds and braking, a stop point, giving way at zebra crossings and to people
//    and vehicles ahead, every wheel on the road, a slight lean, indicators / brake lights / beams, the drive's hum
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TruckDriverRig.h"
#include "VehicleRiders.generated.h"

class USkeletalMeshComponent;
class UPoseableMeshComponent;
class UStaticMeshComponent;

/** A vehicle's named points (Data/<name>-riders.json), in its root space: cm, x forward, y to the right, z up from the road. */
struct FVehicleSpec
{
	TMap<FString, FVector> P; TMap<FString, TArray<FVector>> L; TMap<FString, float> N;
	bool Load(const FString& Name);
	FVector At(const TCHAR* Name) const { const FVector* V = P.Find(Name); return V ? *V : FVector::ZeroVector; }
	const TArray<FVector>& List(const TCHAR* Name) const { static const TArray<FVector> None; const TArray<FVector>* V = L.Find(Name); return V ? *V : None; }
	float Num(const TCHAR* Name, float Def) const { const float* V = N.Find(Name); return V ? *V : Def; }
};

/** What a vehicle does with its own parts for its riders (one per vehicle, owned by its riders component). */
class IVehicleBody
{
public:
	virtual ~IVehicleBody() {}
	virtual AActor* Actor() const = 0;
	/** The body's frame now (knelt, leaning) and the road frame under it (the wheels' contact plane). */
	virtual FTransform Body() const = 0;
	virtual FTransform Road() const = 0;
	struct FDriveState { FTransform Road; float LeanRoll = 0.f, LeanPitch = 0.f, SteerDeg = 0.f, RollRad = 0.f, WheelTurn = 0.f, Accel = 0.f, Brake = 0.f, SeatArm = 0.f; };
	/** Puts the vehicle there with its wheels, steering wheel, pedals and the driver's seat arm as they are now. */
	virtual void Place(const FDriveState& S) = 0;
	virtual void SetDoor(int32 Door, bool bOpen) = 0;
	virtual float DoorK(int32 Door) const = 0;
	virtual bool Knelt() const = 0;
	virtual void SetBay(int32 Bay, bool bOpen) = 0;
	virtual float BayK(int32 Bay) const = 0;
	virtual void SetLamps(bool bLit, int32 Indicate, bool bBrake) = 0;   // Indicate: -1 left, 1 right, 0 none
	virtual void SetDrive(float Amount) = 0;
	/** On the way (its seats step out of everyone's reach) or standing (its seats at its new place). */
	virtual void SetAway(bool bAway) = 0;
	/** Another vehicle of this kind at this point? (for giving way) */
	virtual bool OtherVehicleAt(const FVector& At, float Radius) const = 0;
};

/** A lane's middle line from the site's roads.json (world cm, in its driving direction). */
struct FVehicleLane
{
	TArray<FVector> Pts;
	static bool FromRoad(const FString& Road, bool bRight, int32 Lane, FVehicleLane& Out);
	/** Pulling out: from a front axle at At going Dir, into this lane within Length cm, then along it for Run cm more. */
	TArray<FVector> PullOut(const FVector& At, const FVector& Dir, float Length, float Run) const;
};

UCLASS()
class AIRPORTDYNASTYPRO_API UVehicleRiders : public UActorComponent
{
	GENERATED_BODY()
public:
	UVehicleRiders();
	virtual ~UVehicleRiders();
	/** The one switch: adp.CoachLine (0 by default), or the developer test's command line. */
	static bool LineOn();
	/** The town coach hub (TownHub.h) runs the coaches: their riders on even with adp.CoachLine 0. */
	static inline bool bTownLine = false;
	/** Makes it for a vehicle (null while the switch is off or its points file is missing). It takes the body over. */
	static UVehicleRiders* Make(AActor* Vehicle, const FString& SpecName, TUniquePtr<IVehicleBody> Body);
	virtual void OnUnregister() override;
	virtual void TickComponent(float Dt, ELevelTick Type, FActorComponentTickFunction* Fn) override;

	// ---- the driver (an NPC; never the player)
	bool HireDriver(AActor* Who);
	bool DriverGetIn();
	bool DriverGetOut();
	/** The right hand to the front door's button. */
	bool DriverDoor(bool bOpen);
	bool DriverAtWheel() const { return bAtWheel; }
	// ---- passengers
	/** Waits at the stop (with a bag in the right hand when bBag). */
	bool WaitAtStop(AActor* Who, int32 Place, bool bBag);
	/** Puts the bag in the hand down on the ground beside a bay (for someone else to load). */
	bool PutBagDown(AActor* Who, int32 Bay);
	/** Loader (the owner of the bag itself, the driver or a handler) puts Owner's bag into a bay. */
	bool LoadBag(AActor* Loader, AActor* Owner, int32 Bay);
	bool UseBay(AActor* Who, int32 Bay, bool bOpen);
	bool Board(AActor* Who);
	/** Gets up now (holding a grab handle while the vehicle moves), gets off once it stands with its door open. */
	bool Alight(AActor* Who);
	bool TakeBag(AActor* Who);
	/** Lets someone standing outside go (the town hub: handed back to PaxFlow) - their pose, seat and bag of this vehicle gone. */
	bool Leave(AActor* Who);
	bool IsSeated(const AActor* Who) const;
	bool IsBusy(const AActor* Who) const;
	bool IsAboard(const AActor* Who) const;
	// ---- driving
	bool Drive(const TArray<FVector>& Path, float LimitMs);
	bool IsDriving() const { return Go.Stage != EGo::Parked; }
	bool IsGivingWay() const { return Go.bGiveWay; }
	float Speed() const { return Go.V; }
	float WheelTurn() const { return State.WheelTurn; }
	float FrontSteer() const { return State.SteerDeg; }
	/** Tests: everyone out of the vehicle's hands, the vehicle back where it was, every part as parked. */
	void PutBack();
	const FVehicleSpec& GetSpec() const { return Spec; }
	FTransform BodyFrame() const { return Body ? Body->Body() : FTransform::Identity; }
	FTransform RoadFrame() const { return Body ? Body->Road() : FTransform::Identity; }
	/** Tests: how well a rider's pose meets what it should touch (hands on the wheel, feet, thighs on the seat). */
	FString Report(const AActor* Who) const;

private:
	using FPoseFn = TFunction<FDriverPose(float K, float PH)>;   // (the vehicle's root space, K 0..1)
	struct FAct
	{
		enum EKind : uint8 { Walk, Aisle, Pose, Until, Do };
		EKind Kind = Do;
		TArray<FVector> Pts; TFunction<FVector()> To; FPoseFn Fn; float Dur = 1.f, Speed = 125.f; TFunction<bool(float)> Till; TFunction<void()> Fire;
	};
	struct FRider
	{
		~FRider() { delete Rig; }
		TWeakObjectPtr<AActor> A; USkeletalMeshComponent* Skel = nullptr; bool bDriver = false, bAboard = false, bSeated = false; uint8 Way = 0;   // (Way: 1 getting on, 2 getting off)
		FTruckDriverRig* Rig = nullptr; UPoseableMeshComponent* Mesh = nullptr; class UDriverPoseAnim* Anim = nullptr; FTransform BodyInActor; bool bPosed = false;
		FVector At = FVector::ZeroVector; float Yaw = 0.f;   // (aboard: in the root space)
		TArray<FAct> Q; float T = 0.f; FPoseFn Hold; FDriverPose Last, From; bool bLast = false, bFrom = false; float BlendT = 1.f;
		float Speed = 0.f, HoldT = 0.f;
		int32 Seat = INDEX_NONE; FVector SeatL = FVector::ZeroVector; float SurfZ = -1.f, Fwd = 0.f, Lift = 0.f, CrossLift = 0.f, Tip = 0.f, Deep = 0.f, RestW = 0.f; bool bCushionOk = false; int32 Tries = 0, SitKind = 0;
		int32 Bay = INDEX_NONE, BagState = 0;   // (0 none, 1 in a hand, 2 on the ground, 3 in the bay, 4 being moved by two hands)
		USceneComponent* BagHold = nullptr; UStaticMeshComponent* Bag = nullptr; FTransform BagAt;   // (BagAt: 2 world, 3 / 4 the root space)
		TWeakObjectPtr<AActor> BagBy;           // (whose hand it is in)
	};
	FVehicleSpec Spec;
	TUniquePtr<IVehicleBody> Body;
	TArray<TSharedPtr<FRider>> Riders;
	UPROPERTY() TArray<TObjectPtr<UObject>> Keep;
	IVehicleBody::FDriveState State, Parked;
	FRider* Driver = nullptr; FRider* OnSteps = nullptr; bool bAtWheel = false; float ArmWant = 0.f;
	// the driver's hands, feet, looks
	enum EGest : uint8 { GNone, GButton, GIndicate, GMirror };
	float HandBase[2] = {0.f, 0.f}, RegT[2] = {1.f, 1.f}, RegFrom[2] = {0.f, 0.f}, FootMix = 1.f, GestT = 1e9f, GestDur = 1.f, MirrorIn = 4.f; EGest Gest = GNone; int32 GestSide = 0; float HeadYaw = 0.f, HeadPitch = 0.f, AccelK = 0.f, BrakeK = 1.f, WheelErr = 0.f;
	enum class EGo : uint8 { Parked, Shut, Signal, Run };
	struct FGo { EGo Stage = EGo::Parked; TArray<FVector> Pts; TArray<float> Len; float S = 0.f, V = 0.f, A = 0.f, Limit = 830.f, T = 0.f, Roll = 0.f; FVector Rear = FVector::ZeroVector; bool bGiveWay = false; float LookIn = 0.f, Free = 1e9f, Still = 0.f, SayAt = 60.f; int32 Indicate = 0;
		// (the people round about, their own motion measured from look to look; who or what it waits for; held because you stepped in)
		struct FSeen { TWeakObjectPtr<AActor> A; FVector At = FVector::ZeroVector, Vel = FVector::ZeroVector; bool bNew = true; };
		TArray<FSeen> Seen; float SeenIn = 0.f, LookDt = 0.f; FString Why; bool bHeld = false; } Go;
	// (while it drives: one box for the body, its thousands of parts' own collision off - each put back as it was once it stands)
	struct FPart { TWeakObjectPtr<class UPrimitiveComponent> C; uint8 Was = 0; };
	UPROPERTY() TObjectPtr<class UBoxComponent> Box; TArray<FPart> Parts; int32 PartsOff = 0;
	void TickParts();
	void PartsBack();

	FRider* Find(const AActor* Who) const;
	FRider* Join(AActor* Who);
	FVector Lo(const FVector& World) const { return Body->Body().InverseTransformPosition(World); }
	FVector Wo(const FVector& Local) const { return Body->Body().TransformPosition(Local); }
	FVector GroundUnder(const FVector& Local) const;     // (a point outside: on the ground under it, root space)
	float GroundZ(const FVector& World) const;
	void Step(FRider& R, float Dt);
	void ApplyPose(FRider& R, const FPoseFn& Fn, float K, float Dt);
	void Unpose(FRider& R);
	void WalkAnim(FRider& R, float Speed);
	bool WalkWorld(FRider& R, const FVector& To, float Dt, float Max);
	bool WalkAisle(FRider& R, FAct& Ac, float Dt);
	void Stay(FRider& R, float Dt);
	// the queue's pieces
	void AddPose(FRider& R, float Dur, FPoseFn Fn);
	void AddWalk(FRider& R, TFunction<FVector()> To, float Speed = 125.f);
	void AddAisle(FRider& R, const TArray<FVector>& Pts, float Speed = 80.f);
	void AddDo(FRider& R, TFunction<void()> Fn);
	void AddUntil(FRider& R, TFunction<bool(float)> Fn);
	void AddClimb(FRider& R, bool bUp);
	void AddSteps(FRider& R);   // (the door's steps: one at a time - waits for them, then they are its own until it is through)
	void AddLoad(FRider& R, FRider& Owner, int32 Bay, bool bIn);
	FVector Outside(const TCHAR* Name, float DX = 0.f, float DY = 0.f) const { FVector V = Spec.At(Name); V.X += DX; V.Y += DY; return V; }
	FVector BaySpot(int32 Bay, float Out) const;
	float BaySide(int32 Bay) const;
	FTransform BagInBay(int32 Bay) const;
	float FloorZ() const { return float(Spec.At(TEXT("aisle_start")).Z); }
	FDriverPose StandAt(const FVector& Local, const FVector& Face, float PH, bool bOutside) const;
	FDriverPose Stairs(const TArray<FVector>& T, const FVector& Face, float K, float PH, const TArray<FVector>& Rails) const;
	FDriverPose DriverPose(float PH);
	FDriverPose Passenger(FRider& R, float E, float PH);
	FDriverPose HoldPole(const FRider& R, float PH) const;
	void RailHands(FDriverPose& P, const TArray<FVector>& Rails) const;
	bool PickSeat(FRider& R);
	void FreeSeats(TArray<TPair<int32, FVector>>& Free, TArray<TPair<int32, FVector>>& All) const;
	void GoSeat(FRider& R, bool bAgain);
	bool MayLeave(const FRider& R) const;
	void TickDriver(float Dt);
	void TickDrive(float Dt);
	bool YouAboard() const;
	void LookAround(float Since);
	float Clear(float Need);
	FVector OnPath(float S, FVector* Dir = nullptr) const;
	void GiveBag(FRider& R);
	void BagToHand(FRider& R, AActor* Who);
	void TickBag(FRider& R);
};
