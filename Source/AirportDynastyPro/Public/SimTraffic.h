// The simulation's flights at our airport in 3D (to-do 191, first working step; owner, 2026-11-20 night - and the ground
// rule of to-do 187): every airline's departures and arrivals at the airport you are at, on his A320 model (it stands in
// for every type until there are more models).
//  - Departures: from the flight's off-blocks time (the simulation's), pushed back tail first at walking pace onto the
//    taxilane, then taxiing on the centre lines to the holding point of the runway end that faces the real wind, lined
//    up, the take-off roll, rotation and the climb away until out of sight.
//  - Arrivals: a straight-in 3 degree approach to the touchdown zone, the roll-out, off at the exit, taxiing to the stand
//    the simulation gave it (the parked aircraft takes over there: ParkedAircraft.h).
//  - On the ground the NOSE WHEEL follows the centre-line waypoints exactly and the main gear trails it (12.6 m behind,
//    as a real A320 steers: the body cuts the corner, the nose wheel stays on the line); never over 25 kt, about 10 kt
//    through turns, 5 kt or less near the stand; smooth speeding up and slowing down.
// Lights, control surfaces and the tug's model come with the later steps of 191 / 196.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Airfield.h"
#include "PaxFlow.h"
#include "RunwayRadio.h"
#include "SimTraffic.generated.h"

class ABabylonModel;

UCLASS()
class AIRPORTDYNASTYPRO_API ASimTraffic : public AActor
{
	GENERATED_BODY()
public:
	ASimTraffic();
	virtual void Tick(float Dt) override;
	static void Start(UWorld* W, const FAirfieldData& Field);
	virtual void EndPlay(const EEndPlayReason::Type Why) override;
	/** The jetway of a stand out to the aircraft's door / back to rest (nothing at a stand without one). */
	static void DockAt(UWorld* W, const FString& Stand);
	static void UndockAt(UWorld* W, const FString& Stand);
	/** It is docked / at rest (a stand without a jetway: always); not yet: asked for again. */
	static bool JetwayReady(UWorld* W, const FString& Stand, bool bDocked);
	/** Tests: the aircraft moving now, for the camera director (ChainCam.cpp). */
	static void Scenes(UWorld* W, TArray<APaxFlow::FScene>& Out);
	/** Tests: it has left the ground (its departure is in the climb or away). */
	static bool Airborne(UWorld* W, const FString& Reg);
	/** True while that aircraft moves in 3D (not on blocks). */
	static bool Moving(const UWorld* W, const FString& Reg);
	/** The runway is in use: something rolling, landing, on final under 400 m or lined up (a hangar tow holds short, to-do 311). */
	static bool RunwayBusy(const UWorld* W);
	/** Views test: an A320 of no flight (tag MOVE/<Reg>): lined up on the runway in use and rolling now (bTakeoff), or on a
	 *  3 degree final SecOut seconds before touchdown. False (logged) without traffic or with the runway busy. */
	static bool TestMove(UWorld* W, bool bTakeoff, float SecOut, FString& Reg);
	/** Views test: an A320 of no flight (HB-TAXI) on the taxilane by a free stand, as just pushed back, taxiing out on the
	 *  centre lines to the holding point of the runway in use, holding short, lining up and taking off. A test landing
	 *  (HB-LAND) gets a free stand the same way and taxis in to it. Its model is made at once (no career needed). */
	static bool TestTaxiOut(UWorld* W, FString& Reg);
	/** Views test: where a test aircraft is now - taxi, hold, lineup, roll, climb, away, final, rollout, vacate, taxiin,
	 *  stand; "gone" when it is not about. */
	static FString TestPhase(const UWorld* W, const FString& Reg);
	/** For the tower's words (RunwayRadio.h): what keeps the runway busy now ("traffic landing", "traffic departing",
	 *  "traffic on the runway"; empty: none of ours) and the runway in use, spoken ("two six"). */
	static FString RunwayTraffic(const UWorld* W);
	static FString RunwaySpoken(const UWorld* W);
	/** May this planned leg leave (ADPSchedule::Ready3D): everyone aboard, the jetway at rest, the bags in. */
	bool Ready(const struct FADPPlanned& P, const FDateTime& Now, FString& Why) const;

private:
	enum class EPhase : uint8 { Push, Taxi, Roll, Climb, Approach, Rollout, TaxiIn, Done };
	struct FMove
	{
		FString FlightId, Reg, Stand;
		bool bDeparture = true;
		EPhase Phase = EPhase::Push;
		TArray<FVector2f> Path;             // plan metres: the waypoints the nose wheel follows
		int32 Step = 0;
		FVector2f Nose, Main;               // the nose wheel and the main gear's middle (plan m)
		float Speed = 0.f, Height = 0.f, Clock = 0.f;
		int32 RoundedNum = -1; FVector2f RoundedEnd = FVector2f::ZeroVector;   // (the taxi path whose corners were rounded last: a new one is rounded once)
		float Pitch = 0.f, RotT = -1.f, Gamma = 0.f;   // (nose-up degrees as drawn; seconds since rotation began (-1 not yet); the climb's path angle - all eased, never jumping)
		bool bYou = false, bYouOff = false; int32 YouRe = INDEX_NONE; float YouBar = 0.f; TArray<FVector2f> YouFwd;   // (pushed by you: off blocks, where the crew's path goes on, the bar's angle)
		TWeakObjectPtr<ABabylonModel> Model;
		// its lights (to-do 243: the type's table, Data/aircraft-lights.json; switched in Blink by the real procedure)
		TWeakObjectPtr<class UADPAircraftLights> Lamps;
		// its gear, moving as FlightGear's A320 (SimTraffic.cpp Gear): the nose wheels and lower nose leg steer; all wheels roll;
		// each strut hangs out in the air and compresses as its own wheels meet the ground (ExtN nose, ExtM main: 1 out, 0 in);
		// PosN / PosM: FlightGear's gear position-norm (1 down, 0 up); GG: every moving part's group (SimTraffic.cpp EGg)
		TArray<int32> GG;
		bool bGear = false, bGearPrev = false; float Steer = 0.f, RollN = 0.f, RollM = 0.f, ExtN = 0.f, ExtNV = 0.f, ExtM = 0.f, ExtMV = 0.f;
		FVector2f PrevNose = FVector2f::ZeroVector, PrevMain = FVector2f::ZeroVector;
		// GearSt 0 down and locked, 1 retracting, 2 up and locked, 3 extending
		uint8 GearSt = 0; float AirT = 0.f, PosN = 1.f, PosM = 1.f;
		// the tower (to-do 378): one exchange at a time (RadFor 1 take-off, 2 landing, 3 taxi-in); what it has been cleared for
		ADPRunwayRadio::FCall Rad; uint8 RadFor = 0; FString Cs, Voice;
		bool bTakeoffOk = false, bLandOk = false, bTaxiInOk = false;
		// the engines' heat haze (to-do 379): three rings of shimmer behind each engine (M_HeatHaze), the far two only with
		// power; Thrust 0 off .. 1 take-off power, eased as the engines spool
		TArray<TWeakObjectPtr<class UMaterialBillboardComponent>> HazeB; TArray<TWeakObjectPtr<class UMaterialInstanceDynamic>> HazeM;
		bool bEnginesOn = false; float Thrust = 0.f, HazeShown = -1.f;
		// its control surfaces (step B, Art/A320/a320_surfaces.py, FlightGear's hinges): flaps (aft on their tracks and down),
		// slats, ailerons, elevators, rudder and the flap track fairings, each about its hinge, at the moments and speeds a
		// real A320 moves them. Conf 0, 1, 1+F, 2, 3, FULL (0..5); degrees + = trailing edge down (slats: leading edge down,
		// rudder: trailing edge left); Bank + = right wing down
		TArray<int32> GSurf; bool bSurf = false; int8 Conf = -1; uint8 ElevSt = 255, AilSt = 255, RudSt = 255;
		float FlapDeg = 0.f, SlatDeg = 0.f, Elev = 0.f, AilL = 0.f, AilR = 0.f, Rud = 0.f, Bank = 0.f, Track = 0.f, TurnRate = 0.f, SurfT = 0.f;
		float SurfShown[6] = {-99.f, -99.f, -99.f, -99.f, -99.f, -99.f};
		// spoilers and reversers (Spoilers(), FlightGear's hinges, Art/A320/a320_surfaces.py SPOIL lines): ground spoilers 1-5
		// at 50 deg on touchdown, roll spoilers 2-5 on the down-going wing; the CFM56's four pivot doors per engine (RevPos 0..1)
		TArray<int32> GSpoil; bool bSpoil = false; uint8 SpoilSt = 255, RevSt = 255;
		float SpoilL[5] = {}, SpoilR[5] = {}, RevPos = 0.f, SpoilShown = -1.f;
	};
	TSet<FString> TugCalled;   // (arrivals whose tug driver was called)
	float SlipPoll = 0.f;
	TArray<FMove> Moves;
	FAirfieldData F;
	float Poll = 0.f;
	bool bTurnDue = false;   // (the turnarounds' look, a second after the flights')
	TSet<FString> Seen;
	TSet<FString> ParkedRegs;   // the parked aircraft standing now (their SIM/<reg> tags), looked up once a second
	float ParkPoll = 1.f;
	void Sync();
	double FleetAt = 0.0;   // (when the late world was in: the ground fleet is placed 5 s later, 2026-11-24)
	bool Advance(FMove& M, float Dt);
	void ModelFor(FMove& M);           // its A320 in 3D (tag MOVE/<Reg>), lights and pose
	void TaxiOutPath(FMove& M);        // the taxi-out's centre-line waypoints from the taxilane to the holding point in use
	FString FreeStand() const;         // (tests) a passenger stand no movement uses and no aircraft stands on
	void Pose(FMove& M);
	void AddLights(FMove& M);
	void Blink(FMove& M);
	void Gear(FMove& M, float Dt);
	void Surfaces(FMove& M, float Dt);   // flaps, slats, ailerons, elevators, rudder: the real schedule (step B)
	void Spoilers(FMove& M, float Dt);   // ground and roll spoilers, the CFM56 reverser doors
	void Haze(FMove& M, float Dt);     // the engines' heat haze: made once, then its strength by the thrust
	bool RunwayToX1() const;   // the take-off and landing direction from the real wind (true: towards the runway's x1 end)
};
