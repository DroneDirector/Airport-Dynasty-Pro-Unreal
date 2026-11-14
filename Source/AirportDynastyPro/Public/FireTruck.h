// The airport's crash tenders, drivable (owner, 2026-11-11: "driving physics that of a fire truck", the driving phase).
// Each truck is a physics body (a box the size of the truck, 28 t, its weight low) on six raycast wheels: a spring and
// damper each (about 10 cm of sag standing, 15 cm of bump, 10 cm of droop), anti-roll on every axle, tyres that grip by
// their load (a friction circle), all six wheels driven (an electric drive's power curve, about 780 kW, tops out at 135 km/h: some 17 s to
// 80 km/h), brakes and a handbrake on the rear axles, the front wheels steering up to 25 deg about their
// kingpins (Ackermann, less at speed) and the rearmost axle the other way, up to 10 deg (rear-axle steering).
// Its model (Art/FireTruck, Content/Babylon/Firetruck<n>) rides on the body: the wheels roll, steer and travel, the
// arms swing, the springs compress, the dampers slide, the drive shafts follow, the steering wheel turns, the pedals go
// down, the cab doors open as you get in and out. Get in with E / pad X beside it, out with E / X; V / D-pad up switches
// between the driver's seat and the view from behind. Specs: docs/fire-station/SPEC.md.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"
#include "TruckDriverRig.h"
#include "FireTruck.generated.h"

class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class ABabylonModel;
class UPoseableMeshComponent;
class UAudioComponent;
class USoundAttenuation;

UCLASS()
class AIRPORTDYNASTYPRO_API AFireTruck : public APawn
{
	GENERATED_BODY()

public:
	AFireTruck();
	/** Builds the truck where it stands: places its model (Content/Babylon/<ModelName>) and sets the body round it.
	 *  False when the model is missing. */
	bool Setup(const FString& ModelName);
	virtual void Tick(float Dt) override;
	virtual void SetupPlayerInputComponent(UInputComponent* In) override;
	virtual void EndPlay(const EEndPlayReason::Type Why) override;
	/** You get in (the full pass, owner 2026-11-11): from wherever you pressed E within 9 m you first walk over to the
	 *  driver's door (your keys do nothing meanwhile), reach the door's handle and swing the door open, climb the steps
	 *  holding the handles either side of the doorway, step past the left seat and sit in the middle one, the door pulled
	 *  shut behind you (about 4 s, the camera out behind you and in again). Exit: the same the other way, stopping first,
	 *  the door pulled shut by its handle once you stand outside. */
	void Enter(APawn* Walker);
	void Exit();
	/** Out at once, no climb (the quick-starts that move you elsewhere). */
	void ExitNow();
	void ToggleSiren();
	bool IsBusy() const { return Seq != ESeq::None; }
	/** Where you stand to climb in by the driver's door (world). */
	FVector DoorSpot() const;
	bool SirenOn() const { return bSiren; }
	bool BlueOn() const { return bBlue; }
	void SetTestLights(bool bHigh, bool bBlueOn) { bHighBeam = bHigh; bBlue = bBlueOn; }
	/** Test drives: the indicator (0 off, 1 left, 2 right, 3 hazards) and its state. */
	void SetTestTurn(int32 Way) { Signal = ETurn(FMath::Clamp(Way, 0, 3)); TurnClock = 0.f; bTurnArmed = false; }
	int32 TurnState() const { return int32(Signal); }
	/** The truck an actor belongs to (the truck or its model), or null. */
	static AFireTruck* FromActor(AActor* A);
	/** The wash hose's jet landing on the truck at At (world): that part of it a little cleaner (WashHose.h). */
	void Wash(const FVector& At, float Amount);
	/** How dirty each part still is (0 washed .. 1 as dirty as it gets): 6 lengths of 2 m, rear to front, x 2 sides
	 *  (left 0-5, right 6-11). Driving dirties it again, slowly. */
	float DirtZone[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	/** Where each part's dirt is heading: the jet lowers it, the paint follows over a few seconds (owner, 2026-11-14:
	 *  you see the dirt slowly leave, like the puddles drying). */
	float DirtGoal[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	/** Tests: the whole truck this dirty (1 usual, 2 back from the mud). */
	/** Beads of water the hose left on each part (0 dry .. 1 just washed; owner, 2026-11-14: they stay a while, dry in
	 *  about 20 minutes, and blow off - pushed back along the truck - once you drive, the faster the sooner), and how far
	 *  the airflow has pushed them (cm). */
	float Beads[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	float Blow = 0.f;
	UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> GlassMids;
	void SetDirt(float V) { for (int32 i = 0; i < 12; ++i) DirtZone[i] = DirtGoal[i] = V; }
	FVector DirtCenter = FVector::ZeroVector; bool bDirtCenter = false;
	FVector DirtAxis = FVector::ForwardVector;   // the truck's length, in its own frame (from its parts' extent)
	FVector DirtAxisWorld() const { return GetActorTransform().TransformVectorNoScale(DirtAxis); }
	void PushDirt(float Dt);
	/** The truck right beside you (within 2.5 m of it) that you're facing, or null: E gets you in even when your aim
	 *  misses it, and from behind yourself (third person) too (owner, 2026-11-13: "it doesn't notice, not always"). */
	static AFireTruck* Beside(const APawn* P);
	float SpeedKmh() const;
	/** How fast the body turns (deg/s): test drives check it doesn't shake. */
	float TurnRate() const;
	bool IsDriven() const { return Driver != nullptr; }
	/** Test drives (-UIDrive): the inputs set from outside. */
	void SetTestInput(float Throttle, float Brake, float SteerIn) { bTestInput = true; InThrottle = Throttle; InBrake = Brake; InSteer = SteerIn; TestRoute.Reset(); }
	/** Tests: drive along these plan points (metres) at about Kmh, steering along the road made for it (owner,
	 *  2026-11-14: the test drive keeps to the tarmac, not across the grass). Any SetTestInput ends it. */
	void SetTestRoute(const TArray<FVector2D>& Pts, float Kmh) { TestRoute = Pts; TestKmh = Kmh; bTestInput = true; }
	TArray<FVector2D> TestRoute;
	float TestKmh = 25.f;
	void FollowRoute();
	void SetView(bool bChase);
	/** As pressing V (tests). */
	void PressView() { ToggleView(); }
	int32 ViewStep() const { return !bChase ? 0 : bFarView ? 2 : 1; }
	/** Every truck in the world (the station's bay doors open for them). */
	static TArray<TWeakObjectPtr<AFireTruck>> All;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> ChaseCam;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> DriverCam;
	UPROPERTY() TObjectPtr<ABabylonModel> Model;
	UPROPERTY() TObjectPtr<APawn> Driver;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> MoveCam;    // the camera while you climb in or out
	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Rider;               // your body in and on the truck (the rig; unseen when your MetaHuman rides)
	UPROPERTY() TObjectPtr<class UDriverPoseAnim> HumanPose;            // your own MetaHuman, posed from the rig (DriverPoseAnim.h)
	UPROPERTY() TObjectPtr<USkeletalMesh> MannyMesh;                    // the mannequin, for a walker without a built character
	UPROPERTY() TObjectPtr<UAudioComponent> SirenOut;
	UPROPERTY() TObjectPtr<UAudioComponent> BeeperOut;                  // the reversing beeper
	// the drive's own sound (owner, 2026-11-12), made here as the siren is, nothing recorded: a deep diesel idle
	// (the range extender) that climbs under load, the electric drive's whine rising with speed, the tyres' roar;
	// running from the moment you sit at the wheel until you are out
	UPROPERTY() TObjectPtr<UAudioComponent> EngineOut;
	TSharedPtr<struct FEngineVoice> Engine;
	void EngineOn(bool bOn);
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> ReverseLights;
	bool bReverseLit = false;
	void ReverseGear(bool bOn);
	// the lamps (owner, 2026-11-11): low beams while driven, high beams (L / D-pad down), red tail lights, brake lights
	// far brighter as you brake, amber side markers, the blue warning lights flashing (N / right stick press)
	enum class ELamp : uint8 { Low, High, Tail, Brake, Blue, BlueWide, Marker, Turn };
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Lamps;   // lights and the lenses' glows
	TArray<ELamp> LampKind; TArray<uint8> LampSide; TArray<float> LampFull;
	bool bHighBeam = false, bBlue = false;
	float BlueClock = 0.f;
	void BuildLamps();
	USceneComponent* LensGlow(const FVector& At, const FLinearColor& Col, float Brightness, float Size);
	// the trucks' own materials (Tools/set_truck_materials.py; owner, 2026-11-14): the paint with road dirt, wear and
	// wetness (M_TruckPaint), and every lamp lens lit on its own (M_TruckLens: per-instance brightness 0..1)
	enum class ELens : uint8 { Blue, BlueBar, Low, High, Rear, Turn, Marker, Reverse };
	struct FLens { UInstancedStaticMeshComponent* K = nullptr; int32 Ix = 0; ELens Kind = ELens::Blue; uint8 Side = 0; int32 Bar = 0; float Now = -1.f; };
	TArray<FLens> Lenses;
	int32 BarCount = 0;
	UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> PaintMids;
	void DressMaterials();
	void UpdateLenses(bool bRunning, bool bBraking, bool bTurnOn);
	void UpdateLamps(float Dt);
	void ToggleHighBeam() { if (!bPadMod) bHighBeam = !bHighBeam; }
	void ToggleBlue() { if (!bPadMod) { bBlue = !bBlue; BlueClock = 0.f; } }
	// the indicators (owner, 2026-11-11): left / right / hazards; they cancel themselves once a turn is done; the stalk
	// moves; the relay ticks. Keys: , . F; pad: hold B (circle) + D-pad left / right / down
	enum class ETurn : uint8 { Off, Left, Right, Hazard };
	ETurn Signal = ETurn::Off;
	bool bTurnArmed = false, bPadMod = false, bTurnLit = false;
	float TurnClock = 0.f;
	int32 GStalk = -1; FVector StalkPivot = FVector::ZeroVector, StalkTip = FVector::ZeroVector;
	void SetTurn(ETurn T) { Signal = Signal == T ? ETurn::Off : T; TurnClock = 0.f; bTurnArmed = false; }
	void TurnLeft() { SetTurn(ETurn::Left); }
	void TurnRight() { SetTurn(ETurn::Right); }
	void Hazards() { SetTurn(ETurn::Hazard); }
	void PadModOn() { bPadMod = true; }
	void PadModOff() { bPadMod = false; }
	void PadLeft() { if (bPadMod) TurnLeft(); }
	void PadRight() { if (bPadMod) TurnRight(); }
	void PadDown() { if (bPadMod) Hazards(); }
	UPROPERTY() TObjectPtr<USoundAttenuation> NearSound;
	UPROPERTY() TObjectPtr<USoundAttenuation> FarSound;
	FTruckDriverRig Rig;

	struct FWheel
	{
		int32 Axle = 0; float Side = 1.f;
		FVector Rest = FVector::ZeroVector;          // the wheel's centre at rest, body space (cm)
		FVector CentreRoot = FVector::ZeroVector;    // and in the model's root space
		float Travel = 0.f, X = 0.f, Spin = 0.f, Steer = 0.f, Load = 0.f; bool bContact = false;
		int32 GWheel = -1, GFollow = -1, GLower = -1, GUpper = -1, GSpring = -1, GDamper = -1, GRod = -1, GShaft = -1;
		FTransform BWheel, BLower, BUpper, BSpring, BDamper, BRod, BShaft;
	};
	TArray<FWheel> Wheels;
	int32 GDoor[2] = {-1, -1}, GSteeringWheel = -1, GAccel = -1, GBrake = -1;
	// the gauge cluster (owner, 2026-11-14: needles that move as you drive, the dials true to the truck, a gentle glow in
	// the dark): speed 0-140 km/h and revs 0-3000 over 240 degrees, temperature, oil, fuel and volts over 120; each needle
	// on a little spring so it swings and settles like a real one
	int32 GNeedle[6] = {-1, -1, -1, -1, -1, -1};
	FTransform BNeedle[6];
	float NeedleAng[6] = {0, 0, 0, 0, 0, 0}, NeedleVel[6] = {0, 0, 0, 0, 0, 0};
	float EngineTemp = 15.f, RevShown = 0.f, DashGlow = -1.f;
	TArray<TPair<UInstancedStaticMeshComponent*, int32>> NeedleLens;   // the needles' white and red, lit at night
	UPROPERTY() TObjectPtr<class UPointLightComponent> ClusterLight;
	void UpdateGauges(float Dt);
public:
	/** Tests: what a gauge's needle shows now (0 speed, 1 revs, 2 temp, 3 oil, 4 fuel, 5 volts), degrees from its dial's middle. */
	float NeedleNow(int32 I) const { return I >= 0 && I < 6 ? NeedleAng[I] : 0.f; }
private:
	// the dirt (owner, 2026-11-14: only from bad weather or driving off the paving): what's under the wheels
	float GroundCheck = 0.f; bool bOffRoad = false;
	FTransform BSteeringWheel, BAccel, BBrake;
	FVector HingeRoot[2], BrakePin = FVector::ZeroVector;
	float SpinSign = 1.f, SteerSign = 1.f, DoorSign = 1.f;

	// the controls and the state
	float InThrottle = 0, InBrake = 0, InSteer = 0, KeyFwd = 0, KeySteer = 0, PadThrottle = 0, PadBrake = 0;
	bool bHandbrake = false, bHandHeld = false, bChase = false, bSim = false;
	float Steer = 0, LookYaw = 0, LookPitch = 0, DoorK = 0, Still = 0;
	bool bReverse = false, bTestInput = false;
	float StopHold = 0.f;   // how long the brake has been held at a standstill (reverse after about a second)
	// getting in and out
	enum class ESeq : uint8 { None, Walk, In, Out };
	ESeq Seq = ESeq::None;
	float SeqT = 0.f, WalkClock = 0.f;
	void BeginClimb(APawn* Walker);
	FVector HandleNow() const;   // the door handle's wrist spot now, as the door swings (root space)
	bool bExitPending = false, bSiren = false, bHeadHidden = false;
	FTransform CamFrom;                      // where the camera started (model root space)
	FVector StartPelvis = FVector::ZeroVector, StartFacing = FVector(1, 0, 0);   // you, where you pressed E (root space)
	float DoorTarget = 0.f;
	FTransform WheelNow, AccelNow, BrakeNow; FVector BrakePad = FVector::ZeroVector;
	void StartExit();
	void FinishExit();
	void Sequence(float Dt);
	FDriverPose PoseAt(float T) const;
	FTransform CamPath(float T) const;
	void Sound(const TArray<int16>& Pcm, const FVector& RootAt, bool bFarView);

	void Simulate(float Dt);
	void Animate(float Dt);
	void SetPhysics(bool bOn);
	void Fwd(float V) { KeyFwd = V; }
	void Right(float V) { KeySteer = V; }
	void Turn(float V) { LookYaw = FMath::Clamp(LookYaw + V * 2.f, -150.f, 150.f); }
	void LookUp(float V) { LookPitch = FMath::Clamp(LookPitch + V * 2.f, -60.f, 50.f); }
	void PadT(float V) { PadThrottle = V; }
	void PadB(float V) { PadBrake = V; }
	void HandOn() { bHandbrake = bHandHeld = true; }
	void HandOff() { bHandbrake = bHandHeld = false; }
	// V / the pad's view button: the cab, then outside close behind, then further back, then the cab again (owner,
	// 2026-11-13); the outside distance is kept for the next time you're outside
	bool bFarView = false;
	float ArmWant() const { return bFarView ? 3200.f : 1700.f; }
	void ToggleView() { if (!bChase) { bFarView = false; SetView(true); } else if (!bFarView) { bFarView = true; LookYaw = LookPitch = 0.f; } else SetView(false); }
};

/** The fire station's bay doors (owner: shut in the game): sectional doors whose foot rises in the opening while their top
 *  runs back along the ceiling, opened for a truck in their own lane once its driver sits at the wheel, never shut down
 *  onto anything in the opening; their motor's start and stop clunks and its hum and the sections' rattle as they run. */
UCLASS()
class AIRPORTDYNASTYPRO_API AFireStationDoors : public AActor
{
	GENERATED_BODY()

public:
	AFireStationDoors();
	void Setup(ABabylonModel* Station);
	virtual void Tick(float Dt) override;
	/** Test tours: every door held open. */
	static bool bTestOpen;
	/** A door panel's button (owner, 2026-11-14: each bay door only by the panel beside it, DoorPanelUse.h): the bay door
	 *  nearest At goes up (bOpen) or down and stays so until pressed again - never by itself. Changing its way mid-move
	 *  it stops, waits about a second, then goes. A door still never comes down onto anything in its opening. */
	static void Command(const FVector& At, bool bOpen);
	/** Tests: every bay door sent up or down. */
	static void CommandAll(bool bOpen);
	/** Within 25 m of the station's walls (world position). */
	static bool InReach(const FVector& At);

private:
	UPROPERTY() TObjectPtr<ABabylonModel> Station;
	UPROPERTY() TObjectPtr<USoundAttenuation> Heard;
	struct FBay
	{
		int32 Group = -1; FVector Pivot = FVector::ZeroVector; FVector World = FVector::ZeroVector;
		float K = 0.f, Hold = 0.f, Pause = 0.f; int32 Dir = 0; bool bOpen = false;   // (bOpen: where its panel last sent it)   // open 0-1, kept open a while, the stop before turning back, moving up / down
		TWeakObjectPtr<UAudioComponent> Hum;
	};
	TArray<FBay> Bays;
	static TWeakObjectPtr<AFireStationDoors> One;
	void Sound(const FBay& B, const TArray<int16>& Pcm);
};
