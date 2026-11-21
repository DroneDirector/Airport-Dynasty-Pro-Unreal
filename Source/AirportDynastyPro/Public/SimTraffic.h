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
		TWeakObjectPtr<ABabylonModel> Model;
		// its lights (glows by material, no real lights - the lights rule): navigation, beacons, strobes
		TArray<TPair<class UStaticMeshComponent*, class UMaterialInstanceDynamic*>> Lights;   // nav L, nav R, tail, beacon top, beacon belly, strobe L, strobe R
	};
	TArray<FMove> Moves;
	FAirfieldData F;
	float Poll = 0.f;
	TSet<FString> Seen;
	TSet<FString> ParkedRegs;   // the parked aircraft standing now (their SIM/<reg> tags), looked up once a second
	float ParkPoll = 1.f;
	void Sync();
	bool Advance(FMove& M, float Dt);
	void Pose(FMove& M);
	void AddLights(FMove& M);
	void Blink(FMove& M);
	bool RunwayToX1() const;   // the take-off and landing direction from the real wind (true: towards the runway's x1 end)
};
