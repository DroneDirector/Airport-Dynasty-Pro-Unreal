// Every aircraft's own lights, as in real life (to-do 243, owner 2026-11-24): one table per type (Data/aircraft-lights.json)
// and this one component for every aircraft - the AI's moving ones (SimTraffic), the parked ones (ParkedAircraft) and,
// when it exists in 3D, the player's own. Who switches what stays where it was: SimTraffic's Blink works out the real
// procedure each frame and hands it over as an FADPLightState; a parked aircraft runs ADPAircraftLights::Parked.
//  - the lamp's face: a soft glowing disc at the lens that always faces you (M_LampGlow on a billboard, as the trucks'
//    LensGlow), growing a little with distance so a lamp stays a visible point far away; strobes flash it hot
//  - its light on the aircraft's own skin and what is right round it, at real reach (owner 2026-11-28: nav lights tint the
//    skin within about a metre, the beacon a soft red pool near itself, strobes a short sharp flash, landing / taxi lights
//    the ground ahead): on the nearest aircraft real lights (candela, inverse-square, a short attenuation radius - they
//    shade by the surface's facing, so nothing behind or round the fuselage is painted); on every other aircraft a light
//    decal (M_AircraftLampLight, Tools/make_aircraft_lamp_light.py), faded over slowly so the change is not seen
//  - all of it at the real ratio to the light round it: the game's sun (10 lux) and its lit night apron are not real
//    lux, so the lamps are scaled by the sun's height (full at night, about 1/20 at sunset, 1/400 by day). No flares.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AircraftLights.generated.h"

class UMaterialBillboardComponent;
class UDecalComponent;
class ULocalLightComponent;
class UMaterialInstanceDynamic;

/** Which lamp groups are switched on now (the flashing of beacons and strobes is the component's). */
struct FADPLightState
{
	bool bPowered = true;   // electrics on (engines, APU or ground power); off = cold and dark: every lamp off
	bool bBeacon = false, bStrobe = false, bNav = false, bLogo = false, bLanding = false, bTakeoff = false, bTaxi = false,
		bTurnoff = false, bWingScan = false, bEngineScan = false, bCabin = false;
};

namespace ADPAircraftLights
{
	/** Night by the real rule (sunset to sunrise): the sun light's angle below the horizon. */
	bool Night(const UWorld* W);
	/** Standing at a stand with the engines off: beacon, strobes and every taxi / runway lamp off; navigation and logo
	 *  lights (and the cabin's windows) on at night while it is powered; all dark when it is shut down. */
	FADPLightState Parked(const UWorld* W, bool bPowered);
}

UCLASS()
class AIRPORTDYNASTYPRO_API UADPAircraftLights : public UActorComponent
{
	GENERATED_BODY()
public:
	UADPAircraftLights();
	/** The lamps of that type (Data/aircraft-lights.json) on this aircraft; null when the type has no table (logged). */
	static UADPAircraftLights* Fit(AActor* Plane, const FString& Type);
	/** Moving: the state worked out this frame by the traffic. */
	void Set(const FADPLightState& S) { State = S; bParked = false; }
	/** Parked: it works out its own state (night, power) from now on. */
	void SetParked(bool bIsPowered) { bParked = true; bParkPower = bIsPowered; }
	/** The nose gear's strut now (its retraction turn in the model's own cm, identity when down) and whether it is down and
	 *  locked: the lamps on it ("on": "nose-gear") follow it and are lit only when down. */
	void SetNoseGear(const FTransform& Fold, bool bDownLocked);
	const FADPLightState& Now() const { return State; }
	virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnUnregister() override;

private:
	struct FLampRt
	{
		int32 Lamp = INDEX_NONE;
		TObjectPtr<UMaterialBillboardComponent> Face; TObjectPtr<UMaterialInstanceDynamic> FaceM;
		TObjectPtr<UDecalComponent> Decal; TObjectPtr<UMaterialInstanceDynamic> DecalM;
		TObjectPtr<ULocalLightComponent> Real;
		float Was = -1.f, WasDecal = -1.f, WasReal = -1.f;
	};
	TArray<FLampRt> Rt;
	int32 TypeIx = INDEX_NONE;
	FADPLightState State;
	bool bParked = false, bParkPower = true;
	float Phase = 0.f;        // its own beacon timing (no two aircraft flash together)
	float Near = 0.f;         // 0..1: how much the real lights have taken over from the decals (the nearest aircraft)
	float NightPoll = 0.f; bool bNight = false;
	float Amb = 1.f;          // the lamps' share against the light round them (1 at night, less as the sun rises)
	FTransform LastXf; bool bPlaced = false;
	FTransform NoseXf = FTransform::Identity; bool bNoseDown = true, bNoseMoved = false;
	UPROPERTY() TArray<TObjectPtr<UObject>> Keep;
	void MakeReal(FLampRt& R);
	float NearWanted() const;
};
