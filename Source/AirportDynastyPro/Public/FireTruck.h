// The airport's crash tenders, drivable (owner, 2026-11-11: "driving physics that of a fire truck", the driving phase).
// Each truck is a physics body (a box the size of the truck, 28 t, its weight low) on six raycast wheels: a spring and
// damper each (about 10 cm of sag standing, 15 cm of bump, 10 cm of droop), anti-roll on every axle, tyres that grip by
// their load (a friction circle), all six wheels driven (an electric drive's power curve, about 520 kW: some 25 s to
// 80 km/h, 115 km/h top), brakes and a handbrake on the rear axles, the front wheels steering up to 25 deg about their
// kingpins (Ackermann, less at speed) and the rearmost axle the other way, up to 10 deg (rear-axle steering).
// Its model (Art/FireTruck, Content/Babylon/Firetruck<n>) rides on the body: the wheels roll, steer and travel, the
// arms swing, the springs compress, the dampers slide, the drive shafts follow, the steering wheel turns, the pedals go
// down, the cab doors open as you get in and out. Get in with E / pad X beside it, out with E / X; V / D-pad up switches
// between the driver's seat and the view from behind. Specs: docs/fire-station/SPEC.md.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"
#include "FireTruck.generated.h"

class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class ABabylonModel;

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
	/** You get in (your body hidden and carried along) / out (beside the driver's door). */
	void Enter(APawn* Walker);
	void Exit();
	/** The truck an actor belongs to (the truck or its model), or null. */
	static AFireTruck* FromActor(AActor* A);
	float SpeedKmh() const;
	/** How fast the body turns (deg/s): test drives check it doesn't shake. */
	float TurnRate() const;
	bool IsDriven() const { return Driver != nullptr; }
	/** Test drives (-UIDrive): the inputs set from outside. */
	void SetTestInput(float Throttle, float Brake, float SteerIn) { bTestInput = true; InThrottle = Throttle; InBrake = Brake; InSteer = SteerIn; }
	void SetView(bool bChase);
	/** Every truck in the world (the station's bay doors open for them). */
	static TArray<TWeakObjectPtr<AFireTruck>> All;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> ChaseCam;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> DriverCam;
	UPROPERTY() TObjectPtr<ABabylonModel> Model;
	UPROPERTY() TObjectPtr<APawn> Driver;

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
	FTransform BSteeringWheel, BAccel, BBrake;
	FVector HingeRoot[2], BrakePin = FVector::ZeroVector;
	float SpinSign = 1.f, SteerSign = 1.f, DoorSign = 1.f;

	// the controls and the state
	float InThrottle = 0, InBrake = 0, InSteer = 0, KeyFwd = 0, KeySteer = 0, PadThrottle = 0, PadBrake = 0;
	bool bHandbrake = false, bChase = false, bSim = false;
	float Steer = 0, LookYaw = 0, LookPitch = 0, DoorK = 0, DoorClock = -1, Still = 0;
	bool bReverse = false, bTestInput = false;
	float StopHold = 0.f;   // how long the brake has been held at a standstill (reverse after about a second)

	void Simulate(float Dt);
	void Animate(float Dt);
	void SetPhysics(bool bOn);
	void Fwd(float V) { KeyFwd = V; }
	void Right(float V) { KeySteer = V; }
	void Turn(float V) { LookYaw = FMath::Clamp(LookYaw + V * 2.f, -150.f, 150.f); }
	void LookUp(float V) { LookPitch = FMath::Clamp(LookPitch + V * 2.f, -60.f, 50.f); }
	void PadT(float V) { PadThrottle = V; }
	void PadB(float V) { PadBrake = V; }
	void HandOn() { bHandbrake = true; }
	void HandOff() { bHandbrake = false; }
	void ToggleView() { SetView(!bChase); }
};

/** The fire station's bay doors (owner: shut in the game): up-and-over doors that tilt up and in under the ceiling when a
 *  truck comes up to them, from inside or outside, and close again once it has gone. */
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

private:
	UPROPERTY() TObjectPtr<ABabylonModel> Station;
	struct FBay { int32 Group = -1; FVector Pivot = FVector::ZeroVector; FVector World = FVector::ZeroVector; float K = 0.f; };
	TArray<FBay> Bays;
};
