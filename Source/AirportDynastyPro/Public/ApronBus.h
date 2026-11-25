// The apron buses (to-do 277): the electric buses that take passengers from the bus gate to the remote stands
// (Tools/make_apron_bus.py, Content/Babylon/ApronBus, sounds in Content/ApronBus/Sounds).
//  - parked where Data/apron-buses.json says (a table: one row a bus), only where the airport has remote stands
//    (built, or -RemoteStands)
//  - your airline's colours on their paint (as the other ground vehicles), the airport's own name on their sides written
//    at run time, road dirt and the wash hose (VehicleDirt.h), every lamp's lens ready to be lit on its own
//  - the vehicles themselves are AGSE's (GSEBus.cpp: driving, driver, doors, what is solid); this keeps their looks:
//    AGSE places one a row and gives it to Adopt
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
		TArray<FLamp> Lamps;
		TArray<TWeakObjectPtr<UTextRenderComponent>> Names;
		FString Livery;
	};
	const TArray<FBus>& All() const { return Buses; }
	/** The table read (once); the buses have no models until AGSE gives them (Adopt). */
	void Load();
	/** A bus's model, placed by AGSE (its road dirt already on it): its lamps, name and colours. */
	void Adopt(int32 Ix, ABabylonModel* M);
	/** A bought one, at the vehicle area (GSESpare.cpp): its row, for Adopt. */
	int32 AddBus(const FString& Name, const FString& Bay, float X, float Y, float Heading);
	FBus* Of(const ABabylonModel* M) { return Buses.FindByPredicate([M](const FBus& B) { return B.M.Get() == M; }); }
	/** A bus's lamps of one kind lit (0 off .. 1 full); Side 1 left, -1 right, 0 both. */
	void SetLamps(FBus& B, EApronBusLamp Kind, float On, int32 Side = 0);

private:
	bool bLoaded = false;
	void Materials(FBus& B);
	void AirportName(FBus& B);
	void PutLivery(FBus& B);
	TArray<FBus> Buses;
	float LiveryIn = 0.f;
	static TWeakObjectPtr<AApronBuses> One;
};
