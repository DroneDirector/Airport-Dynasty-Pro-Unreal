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
	/** The pushback: Fitting the aircraft's tow fitting (world, on the ground), Fwd its nose direction (world, flat).
	 *  True once a tug is hooked on (it drives in first); then it is posed on the aircraft. */
	bool Push(const FString& Key, float StandX, const FVector& Fitting, const FVector& Fwd);
	/** The pushback is done: the tug unhooks, backs away and drives back to the park. */
	void Release(const FString& Key);
	/** True when no tug is on or in front of that aircraft any more (it may taxi). */
	bool IsClear(const FString& Key) const;
	int32 NumAtWork() const;
	/** Tests: where to look - the first tug at work (else the park); its facing yaw. */
	bool WatchSpot(FVector& At, float& Yaw, bool* bHooked = nullptr) const;

private:
	enum class EState : uint8 { Parked, ToStand, Waiting, ToNose, Hooked, Unhook, Backing, Home };
	struct FLens { UInstancedStaticMeshComponent* K = nullptr; int32 Ix = 0; uint8 Kind = 0; float Now = -1.f; };
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
		float Spin = 0.f, Steer = 0.f; FVector PrevAt = FVector::ZeroVector; float PrevYaw = 0.f; bool bPrev = false;
	};
	TArray<FTug> Tugs;
	TArray<FVector> Planes;   // the moving aircraft (taxiing, pushed), looked up once a second: vehicles always give way to them
	float PlaneClock = 1.f;
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
};
