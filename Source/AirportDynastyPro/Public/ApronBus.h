// The apron buses (to-do 277): the electric buses that take passengers from the bus gate to the remote stands
// (Tools/make_apron_bus.py, Content/Babylon/ApronBus, sounds in Content/ApronBus/Sounds).
//  - parked where Data/apron-buses.json says (a table: one row a bus), only where the airport has remote stands
//    (built, or -RemoteStands)
//  - your airline's colours on their paint (as the other ground vehicles), the airport's own name on their sides written
//    at run time, road dirt and the wash hose (VehicleDirt.h), every lamp's lens ready to be lit on its own
//  - solid: nobody walks through one; their doors are shut (nothing drives or opens yet)
// The driving, the doors and the riders are the next steps: FBus holds what they need.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VehicleDirt.h"
#include "ApronBus.generated.h"

class ABabylonModel;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTextRenderComponent;

enum class EApronBusLamp : uint8 { Head, Drl, Indicator, TailBrake, ThirdBrake, Reverse, Marker, Beacon, DoorWarn, Destination, Inside };

UCLASS()
class AIRPORTDYNASTYPRO_API AApronBuses : public AActor
{
	GENERATED_BODY()
public:
	AApronBuses();
	/** The manager: spawned the first time it is asked for. */
	static AApronBuses* Get(UWorld* W);
	/** The manager when there is one (nothing spawned). */
	static AApronBuses* Find() { return One.Get(); }
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	struct FLamp { TWeakObjectPtr<UInstancedStaticMeshComponent> K; int32 Ix = INDEX_NONE; EApronBusLamp Kind = EApronBusLamp::Head; int8 Side = 0; };   // (Side: 1 left, -1 right, 0 none)
	struct FBus
	{
		FString Name, Bay;
		float X = 0.f, Y = 0.f, Heading = 0.f;   // (its own place: plan metres and degrees)
		TWeakObjectPtr<ABabylonModel> M;
		FVehicleDirt Dirt;
		TArray<FLamp> Lamps;
		TArray<TWeakObjectPtr<UTextRenderComponent>> Names;
		FString Livery;
	};
	const TArray<FBus>& All() const { return Buses; }
	/** A bus's lamps of one kind lit (0 off .. 1 full); Side 1 left, -1 right, 0 both. */
	void SetLamps(FBus& B, EApronBusLamp Kind, float On, int32 Side = 0);

private:
	void Setup();
	bool SpawnBus(FBus& B);
	void Materials(FBus& B);
	void AirportName(FBus& B);
	void PutLivery(FBus& B);
	TArray<FBus> Buses;
	float LiveryIn = 0.f;
	static TWeakObjectPtr<AApronBuses> One;
};
