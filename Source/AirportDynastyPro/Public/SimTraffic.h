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
		bool bYou = false, bYouOff = false; int32 YouRe = INDEX_NONE; float YouBar = 0.f; TArray<FVector2f> YouFwd;   // (pushed by you: off blocks, where the crew's path goes on, the bar's angle)
		TWeakObjectPtr<ABabylonModel> Model;
		// its lights (to-do 243: the type's table, Data/aircraft-lights.json; switched in Blink by the real procedure)
		TWeakObjectPtr<class UADPAircraftLights> Lamps;
		// its gear (Art/A320/make_a320_parts.py): the nose wheels and lower nose leg steer; all wheels roll; the struts hang
		// extended in the air and compress on touchdown with a little settle
		int32 GNoseW = INDEX_NONE, GNoseLeg = INDEX_NONE, GMainW[2] = {INDEX_NONE, INDEX_NONE}, GMainLeg[2] = {INDEX_NONE, INDEX_NONE};
		bool bGear = false, bGearPrev = false; float Steer = 0.f, RollN = 0.f, RollM = 0.f, Ext = 0.f, ExtV = 0.f;
		FVector2f PrevNose = FVector2f::ZeroVector, PrevMain = FVector2f::ZeroVector;
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
	void Pose(FMove& M);
	void AddLights(FMove& M);
	void Blink(FMove& M);
	void Gear(FMove& M, float Dt);
	bool RunwayToX1() const;   // the take-off and landing direction from the real wind (true: towards the runway's x1 end)
};
