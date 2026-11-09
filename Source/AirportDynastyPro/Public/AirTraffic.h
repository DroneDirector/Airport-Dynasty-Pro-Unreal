// The traffic at your airport: an airliner on each of stands 1–8, turned round by its ground vehicles (the fuel
// truck, the catering truck, the baggage tug with its carts), then pushed back and taxied out (T, P1, A, A1) to wait at
// the holding position, lined up on runway 08, off down the runway and climbing away. A while later it comes back: a
// 3° approach onto 08, touchdown, the rollout, off at A2 and in along A, P3 and T to its stand. One aircraft uses the
// runway at a time; departures and arrivals keep to different links so they never meet head on.
// Until the shared world drives it (its real flights and aircraft), the timings are the game's own short turnarounds.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Airfield.h"
#include "AirTraffic.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API AAirTraffic : public AActor
{
	GENERATED_BODY()

public:
	AAirTraffic();
	void Start(const FAirfieldData& Field);
	virtual void Tick(float DeltaSeconds) override;

private:
	#pragma region Aircraft
	enum class EState : uint8 { Parked, Pushback, TaxiOut, Holding, TakeOff, Away, Approach, Rollout, TaxiIn };
	struct FVehicle { USceneComponent* Root = nullptr; FVector2f Home, Spot; };
	struct FPlane
	{
		USceneComponent* Root = nullptr;
		FAirfieldData::FStand Stand;
		EState State = EState::Parked;
		TArray<FVector> Path;   // plan x, height, y (the airfield's y)
		int32 Step = 0;
		FVector Pos = FVector::ZeroVector;
		float Heading = 0;      // radians in the plan: the way the nose points
		float Speed = 0, Timer = 0, Turn = 200;
		bool bBackwards = false;
		TArray<FVehicle> Vehicles;
	};
	TArray<FPlane> Planes;
	FAirfieldData F;
	int32 RunwayUser = INDEX_NONE;
	FRandomStream Rng;

	USceneComponent* MakeAirliner(int32 Livery);
	FVehicle MakeVehicle(int32 Kind, const FVector2f& Home, const FVector2f& Spot);
	UStaticMeshComponent* Part(USceneComponent* Parent, UStaticMesh* Mesh, const FName Mat, const FVector& At, const FRotator& Rot, const FVector& Size);
	void Place(USceneComponent* C, const FVector& PlanPos, float Heading);
	FVector StandCentre(const FPlane& P) const;
	void Plan(FPlane& P, EState S);
	bool Follow(FPlane& P, float Dt, float MaxSpeed);
	void TurnaroundVehicles(FPlane& P, float Dt);
	#pragma endregion

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UStaticMesh> Cyl;
	UPROPERTY() TObjectPtr<UStaticMesh> Ball;
};
