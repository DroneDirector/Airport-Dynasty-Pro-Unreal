// The airport coaches (owner, 2026-11-19, to-do 144): the parked coaches' working parts - one manager for every coach the
// road network parks in the forecourt stands (RoadNetwork.cpp; the model: Tools/make_coach.py -- --assembled, imported as
// AirportCoach). Per coach:
//  - the front and middle doors: E / X by a door opens or closes it; each leaf swings out on its outer edge (a plug door's
//    swing), the coach kneels a little on the door side as it opens, with the air hiss (Art/Sounds/Coach)
//  - the six luggage bays (three a side, through-loading): E / X by a bay lifts its door up or shuts it
//  - its lamps: low / high beams, turn signals and hazards, tail and brake, reverse - each lens glows by its material
//    (M_CoachLamp); a real light only for the low / high beams of the coach you're nearest, short reach, no shadows
//    (owner: real lights only if the frame rate holds - the light rule: glow by material, one real light)
//  - its mirrors reflect: one capture each, only on the coach you're nearest (within 25 m), a few times a second
//  - its sounds: the doors' air, the kneel hiss; the drive's hum muffled inside and full outside (for the traffic job)
//  - the airport's own name on the rear name plate, written at run time from Data/airfield.json (never baked)
//  - inside it your body's capsule is hip-wide (the aisle is 0.45 m: real people pass with their shoulders over the seats)
// The seats are sittable through ASeats (ABabylonModel::SeatSpots knows the coach's cushions; never the driver's seat).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Coaches.generated.h"

class ABabylonModel;
class UAudioComponent;
class USoundBase;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class USpotLightComponent;
class UTextRenderComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API ACoaches : public AActor
{
	GENERATED_BODY()
public:
	ACoaches();
	static ACoaches* Get(UWorld* W);
	/** The manager when there is one (nothing spawned). */
	static ACoaches* Find() { return One.Get(); }
	/** A coach the road network parked (its model placed as AirportCoach). */
	void Add(ABabylonModel* Coach);
	virtual void Tick(float Dt) override;
	/** E / X: a coach door or luggage bay you stand by and face - opened or closed. True when it did. Your hand reaches it
	 *  first (to-do 238): the fingertip to the door's button (inside: the door's inner jamb), the palm round the bay's catch;
	 *  it moves only once the hand is there. Walk off before that and nothing happens (the arm comes back). */
	bool Use(const APawn* Who);
	/** Something to use within reach (the outline and the button hint). */
	bool NearUse(const FVector& Feet, const FVector& Facing) const;

	// ---- for the traffic job and the tests
	enum ELamp : uint8 { LowBeam = 1, HighBeam = 2, TurnLeft = 4, TurnRight = 8, Tail = 16, Brake = 32, Reverse = 64, Hazard = TurnLeft | TurnRight };
	void SetLamps(int32 Coach, uint8 Lamps);
	void SetDoor(int32 Coach, int32 Door, bool bOpen);
	void SetBay(int32 Coach, int32 Bay, bool bOpen);
	/** The drive's hum (0 off .. 1 full), heard muffled inside the coach and full outside. */
	void SetDrive(int32 Coach, float Amount);
	int32 Num() const { return Coaches.Num(); }
	/** Tests: the beams' two real lights and the mirrors' captures off (their cost measured against them on). */
	static inline bool bNoRealBeams = false;
	static inline bool bNoMirrors = false;
	/** Tests: a coach's door / bay middle (world), the way out of it. */
	bool DoorSpot(int32 Coach, int32 Door, FVector& At, FVector& Out) const;
	bool BaySpot(int32 Coach, int32 Bay, FVector& At, FVector& Out) const;
	/** By a door's outside, on the ground beside it (world): Along m from the door's middle towards the rear, Out m out from the
	 * body side. ButtonStand: where you stand to press its outside button - behind the rear leaf's swing, within arm's reach. */
	FVector DoorSide(int32 Coach, int32 Door, float Along, float Out) const;
	bool ButtonStand(int32 Coach, int32 Door, FVector& Stand) const;
	bool IsInside(const FVector& At, int32* Which = nullptr) const;
	/** A coach's frame (world; Blender's (x, y, z) m at (x, -y, z) * 100 in it). */
	FTransform RootOf(int32 Coach) const { return Coaches.IsValidIndex(Coach) ? RootW(Coaches[Coach]) : FTransform::Identity; }

	// ---- its driver and riders (VehicleRiders.h): all behind adp.CoachLine, 0 by default - null then, and nothing here is used
	/** A coach's riders component (made on the first call); null while the switch is off. */
	class UVehicleRiders* Riders(int32 Coach);
	struct FDriveParts { bool bOn = false; int32 Wheel[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE}, Knuckle[2] = {INDEX_NONE, INDEX_NONE}, Hand = INDEX_NONE, Accel = INDEX_NONE, Brake = INDEX_NONE, Arm = INDEX_NONE; FVector WheelAt[4]; FTransform HandAt, ArmAt, PedalAt; float LeanRoll = 0.f, LeanPitch = 0.f, SteerDeg = 0.f, RollRad = 0.f; };
	bool DriveReady(int32 Coach);
	void DrivePlace(int32 Coach, const FTransform& Road, float LeanRoll, float LeanPitch, float SteerDeg, float RollRad, float WheelTurn, float Accel, float Brake, float SeatArm);
	/** Every driving part back as the parked coach has it. */
	void DriveOff(int32 Coach);
	FTransform RoadOf(int32 Coach) const { return Coaches.IsValidIndex(Coach) ? Coaches[Coach].RootBase : FTransform::Identity; }
	float DoorKOf(int32 Coach, int32 Door) const { return Coaches.IsValidIndex(Coach) ? Coaches[Coach].DoorK[Door & 1] : 0.f; }
	float BayKOf(int32 Coach, int32 Bay) const { return Coaches.IsValidIndex(Coach) && Bay >= 0 && Bay < 6 ? Coaches[Coach].BayK[Bay] : 0.f; }
	float KneelOf(int32 Coach) const { return Coaches.IsValidIndex(Coach) ? Coaches[Coach].Kneel : 0.f; }
	ABabylonModel* ModelOf(int32 Coach) const { return Coaches.IsValidIndex(Coach) ? Coaches[Coach].M.Get() : nullptr; }

	// ---- the town hub's one-way loop (to-do 373; roads.json "routes"), behind adp.CoachLine like all driving
	/** Sends a coach (its driver at the wheel, everyone seated) along the loop: out of its terminal stand forward to the town
	 *  hub (bToTown), or from the hub back to its own stand, arriving the way the parked coaches face. Forward only: if it
	 *  cannot pull out without touching the coach in front it first backs up only as far as it needs, the reverse beep
	 *  sounding. Bay: the hub bay (INDEX_NONE: the first free one). False (logged) when it cannot go. */
	bool SendOnRoute(int32 Coach, bool bToTown, int32 Bay = INDEX_NONE, float LimitMs = 13.9f);   // (LimitMs: its top speed, m/s - the town hub's traffic)
	/** Being sent (backing up, or its riders driving the loop)? */
	bool OnRoute(int32 Coach) const { return RouteGo.Contains(Coach); }
	/** The loop's path from a root place (world cm, the front axle's way; tests). Empty, Why said, when there is none. */
	TArray<FVector> RoutePath(int32 Coach, bool bToTown, int32 Bay, const FTransform& From, FString& Why) const;
	/** Views test: every coach not yet driving put on the loop's way in, Metres short of the terminal, GapM apart (the
	 *  stands left empty, each one's stand kept as its home). Only before their riders exist. The number put out. */
	int32 TestOut(float Metres, float GapM);
	/** Views test: the coach operator's driver for it (appears unseen beside the coach, climbs in). True once at the wheel. */
	bool TestDriver(int32 Coach, FString* Why = nullptr);
	/** Views test: could it set off along the loop now without touching another coach? */
	bool RouteFree(int32 Coach, bool bToTown, FString* Why = nullptr) const;
	/** The road's limit at each point of a loop path (m/s): the open road 85 km/h, the town hub's streets and the airport
	 *  (inside the airfield's boundary) TownMs, at most 50 km/h. */
	TArray<float> RouteCaps(const TArray<FVector>& Path, float TownMs) const;

private:
	static TWeakObjectPtr<ACoaches> One;
	struct FMover { int32 Group = INDEX_NONE; FTransform Pivot; FQuat OpenTurn = FQuat::Identity; FVector OpenShift = FVector::ZeroVector; };
	struct FCoach
	{
		TWeakObjectPtr<ABabylonModel> M;
		FMover Leaf[2][2];            // [door][leaf]
		float DoorK[2] = {0, 0}; bool bDoorOpen[2] = {false, false};
		FMover Bay[6]; float BayK[6] = {0}; bool bBayOpen[6] = {false};
		float Kneel = 0.f;             // 0 up .. 1 knelt (a door open)
		FTransform RootBase;           // the model root's own place (the kneel tilts it)
		int32 Wheels = INDEX_NONE;     // the wheels, held still while the body kneels
		uint8 Lamps = 0; uint8 LampsShown = 0xFF; float Blink = 0.f;
		TMap<uint8, TArray<UMaterialInstanceDynamic*>> LampMats;   // (per lamp kind: its lenses' own glow)
		TArray<USpotLightComponent*> Beams;          // (all of these also held in Keep)
		TArray<USceneCaptureComponent2D*> Mirrors;
		TArray<UMaterialInstanceDynamic*> MirrorMats;
		UAudioComponent* Hum = nullptr;
		UTextRenderComponent* Plate = nullptr;
		float Drive = 0.f;
		FDriveParts Line;              // (its driving parts: only once adp.CoachLine is on)
		TWeakObjectPtr<class UVehicleRiders> Riders;
	};
	TArray<FCoach> Coaches;
	UPROPERTY() TArray<TObjectPtr<UObject>> Keep;   // (dynamic materials, render targets, components held for the GC)
	USoundBase* SndDoor = nullptr; USoundBase* SndKneel = nullptr; USoundBase* SndBay = nullptr; USoundBase* SndHum = nullptr;
	int32 MirrorsOn = INDEX_NONE; float MirrorClock = 0.f;
	bool bCapsuleSmall = false;
	FString AirportName;

	void Setup(FCoach& C);
	void Lamp(FCoach& C, float Dt);
	void ApplyBody(FCoach& C);
	void Mirror(float Dt, const FVector& Me);
	bool Pick(const FVector& Feet, const FVector& Facing, int32& Ci, int32& Kind, int32& Index) const;
	/** Where your hand goes for a door (Kind 0) or bay (1) from Feet (world), and the part's outward side there. */
	bool HandSpot(int32 Ci, int32 Kind, int32 Index, const FVector& Feet, FVector& Hand, FVector& Out) const;
	// your hand on its way to a door's button or a bay's catch (to-do 238)
	struct FReach { TWeakObjectPtr<APawn> Who; int32 Ci = INDEX_NONE, Kind = 0, Index = 0; bool bOpen = false, bDone = false, bPoint = false; float T = 0.f; FVector From = FVector::ZeroVector; };
	FReach Reach;
	void ReachTick(float Dt);
	void ReachEnd();
	// the loop (to-do 373)
	struct FRouteGo { TArray<FVector> Path; float BackLeft = 0.f, Backed = 0.f; UAudioComponent* Beep = nullptr; bool bToTown = false; bool bDriving = false; int32 Bay = INDEX_NONE; float LimitMs = 13.9f; TArray<float> Caps; float SayIn = 0.f; };
	TMap<int32, FRouteGo> RouteGo;
	TMap<int32, FTransform> HomeStand;     // a coach's terminal stand (world root): where it was when first sent to town
	TMap<int32, int32> HubBayOf;           // the hub bay a coach went to
	TMap<int32, TWeakObjectPtr<AActor>> TestDriverOf;   // views test drivers
	USoundBase* SndBeep = nullptr;
	void RouteTick(float Dt);
	/** The first other coach the body touches driving the first stretch of Path from From (INDEX_NONE: none). */
	int32 RouteHits(int32 Coach, const FTransform& From, const TArray<FVector>& Path) const;
	FTransform RootW(const FCoach& C) const;
};
