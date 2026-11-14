// The fire station's wash hose (owner, 2026-11-14): take the spray gun off its hook with E / X, drag the hose about
// (25 m of it), hold the left mouse button / right trigger to spray - a jet of water you see, that splashes and leaves
// wet patches on the ground - and wash the dirt off a vehicle where the jet hits it (the fire trucks: FireTruck.h).
// E / X again hangs the gun back up.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WashHose.generated.h"

class ABabylonModel;
class UCableComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UDecalComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;

UCLASS()
class AIRPORTDYNASTYPRO_API AWashHose : public AActor
{
	GENERATED_BODY()
public:
	AWashHose();
	virtual void Tick(float Dt) override;

	/** Hooks the hose up to its model on the station wall (the WashHose model: its gun, hook and the tap's outlet). */
	void Setup(ABabylonModel* Model);

	/** The hose in someone's hand, or none. */
	static AWashHose* Held();
	/** The first hose there is (tests). */
	static AWashHose* First();
	/** Every hose there is (the fire station's, each hangar's). */
	static TArray<AWashHose*> All();
	/** The coil's loops still on the wall (the outline). */
	TArray<TPair<UInstancedStaticMeshComponent*, int32>> CoilPieces() const { TArray<TPair<UInstancedStaticMeshComponent*, int32>> L; for (const FLoop& Lp : Loops) if (Lp.bShown) L.Add(TPair<UInstancedStaticMeshComponent*, int32>(Lp.Ism, Lp.Item)); return L; }
	/** Within reach of a hose's hook (to take it: listed in the Buttons panel). */
	static bool NearHook(const FVector& At);
	/** Where its hook is, and a spot to stand in front of it (tests). */
	FVector HookAt() const { return Hook; }
	FVector HookSpot() const;
	/** E / X: take the gun when you aim at the hose or stand by its hook; hang it back up when you hold it.
	 *  Returns true when it did something. */
	static bool Interact(APawn* Who, AActor* Aimed);
	/** Spraying now (the left mouse button / right trigger held with the gun in your hand). */
	bool IsSpraying() const { return bSpray; }
	/** The gun in your hand, ready to spray (not reaching for it or hanging it up). */
	bool IsReady() const { return Phase == EPhase::Hold; }
	/** The gun's piece on its hook, while it hangs there (the interaction outline). */
	bool GunOnHook(UInstancedStaticMeshComponent*& K, int32& Item) const { K = GunIsm; Item = GunItem; return bGunOnHook && GunIsm && GunItem != INDEX_NONE; }
	/** Tests: hold the trigger without input. */
	bool bTestSpray = false;

	static constexpr float Reach = 2400.f;   // (the most there is: set from the coil's own length)

private:
	void PickUp(APawn* Who);
	void HangUp();
	void Spray(float Dt, const FVector& Tip, const FVector& Aim);
	void WetAt(const FVector& At, const FVector& Normal);
	void MakeSound();

	UPROPERTY() TObjectPtr<ABabylonModel> Model;
	UPROPERTY() TObjectPtr<UCableComponent> Cable;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Gun;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Drops;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Mist;   // the spray clouds where it lands
	UPROPERTY() TArray<TObjectPtr<UDecalComponent>> Wet;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> WetMids;
	UPROPERTY() TObjectPtr<UAudioComponent> Hiss;
	TArray<float> WetAge;

	TWeakObjectPtr<APawn> Holder;
	enum class EPhase : uint8 { Hung, Reach, Hold, Back };
	EPhase Phase = EPhase::Hung;
	float PhaseT = 0.f;
	bool bGunOnHook = true;
	void GunToHook(bool bOnHook);
	UInstancedStaticMeshComponent* GunIsm = nullptr; int32 GunItem = INDEX_NONE; FTransform GunHome;
	/** The coil on the wall saddle (owner, 2026-11-14: the same hose, paying out of it): its own 7 loops and a second
	 *  layer wound over them from the same pieces, unwinding loop by loop (top layer first, front first) as you walk out,
	 *  winding back on as you come in. The free hose leaves from the bottom of the last loop still on. */
	struct FLoop { UInstancedStaticMeshComponent* Ism; int32 Item; FTransform Home; float Len; bool bShown; };
	TArray<FLoop> Loops;
	float HoseLen = 0.f;
	void ShowCoil(bool bOn);
	void PayOut(float Paid);
	FVector Leaves = FVector::ZeroVector;   // where the hose comes off the coil now
	FVector Outlet = FVector::ZeroVector, Hook = FVector::ZeroVector;

	struct FDrop { FVector P, V; float Life; bool bSplash; bool bMist = false; float Seed = 0.f; };
	TArray<FDrop> Live;
	float Emit = 0.f, WetClock = 0.f, SprayVol = 0.f;
	bool bSpray = false;
	TSharedPtr<float, ESPMode::ThreadSafe> HissLevel;
};
