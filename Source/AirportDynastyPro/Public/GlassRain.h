// Rain on the glass (to-do 295): the weather's part (how much water stands on an open pane, how far it has dried, the runners'
// clock - MPC_GlassRain, read by MF_GlassRain on any glass) and each vehicle's part (under a roof or not, the airflow, its wipers).
// The drops themselves: Tools/make_glass_rain.py. For other glass: docs/Glass-Rain-Plan.md.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GlassRain.generated.h"

class ABabylonModel;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UMaterialParameterCollection;

UCLASS()
class AIRPORTDYNASTYPRO_API UGlassRain : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(const EWorldType::Type T) const override { return T == EWorldType::Game || T == EWorldType::PIE; }
	virtual void Tick(float Dt) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGlassRain, STATGROUP_Tickables); }
	/** How full an open pane is (0..1; 0 also with adp.GlassRain 0), how fast it fills just now (per second, 0: no rain). */
	static float Fill;
	static float FillRate;
	/** Is there a roof over this place? One ray straight up (40 m), past Not and what it is attached to. */
	static bool Roofed(UWorld* W, const FVector& At, const AActor* Not);
private:
	float F = 0.f, Dry = 0.f, Time = 0.f, Run = 0.f;
	bool bStarted = false, bZeroSent = false;
	UPROPERTY() TObjectPtr<UMaterialParameterCollection> Mpc;
	/** A building with rain glass (Tools/set_building_rain.py): what stands above each place is looked up once, a little each frame,
	 *  the first time it gets wet; then its panes read it (under a roof and indoors: dry) and the rain fades in. */
	struct FBuilding
	{
		TWeakObjectPtr<AActor> A;
		TArray<TPair<TWeakObjectPtr<UMeshComponent>, int32>> Panes;
		TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> Mids;
		FBox Box = FBox(ForceInit); float Cell = 75.f; int32 W = 0, H = 0, Next = 0; TArray<float> Top; float On = 0.f; bool bReady = false;
	};
	TArray<FBuilding> Buildings;
	TArray<TWeakObjectPtr<AActor>> NotRoofs;
	float LookIn = 0.f, DryFor = 0.f;
	void FindBuildings();
	void TickBuildings(float Dt);
	void Finish(FBuilding& B);
};

/** One wiper blade, in the model root's frame at rest (cm): it turns from Rest about Axis by Sweep (radians). */
struct FGlassBlade
{
	FVector Pivot = FVector::ZeroVector, Rest = FVector::ForwardVector, Axis = FVector::UpVector;
	float Sweep = 1.f, RIn = 5.f, ROut = 60.f;
};

/** The windscreen's panes and their wipers (two blades at most, moving together). */
struct AIRPORTDYNASTYPRO_API FGlassWipers
{
	TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> Panes;
	float Age = 1000.f, Prev = 1000.f, Last = 0.f, Since = 0.f, SweepS = 1.5f; int32 No = 0; bool bWired = false;
	/** Once: the panes the blades stand on (each gets its own material instance, added to NewGlass and Rain's panes). H: how far the blades' line passes beside the pivot. */
	void Wire(ABabylonModel* M, struct FGlassRainVehicle& Rain, const TArray<FGlassBlade>& Blades, float H, const TCHAR* What, TArray<UMaterialInstanceDynamic*>& NewGlass);
	/** Every frame: T the time since the sweep began (0 lying down), Ts the seconds there and back. */
	void Tick(float Dt, float T, float Ts);
};

struct AIRPORTDYNASTYPRO_API FGlassRainVehicle
{
	struct FPane { TWeakObjectPtr<UMaterialInstanceDynamic> D; FQuat ToLocal = FQuat::Identity; };
	TArray<FPane> Panes;
	float Own = 1.f, OwnSent = 1.f, Lean = 0.f, LeanSent = 0.f, CheckIn = 0.f; bool bRoof = false, bFresh = true;
	/** Once, when the model has its glass: finds its panes (material M_TruckGlass), seeds them, tells them which way is out. */
	void Dress(ABabylonModel* M, TArray<UMaterialInstanceDynamic*>& NewGlass);
	/** Every frame: BackRoot where the airflow of driving goes (root frame). */
	void Tick(ABabylonModel* M, float Dt, float SpeedMs, const FVector& BackRoot);
};
