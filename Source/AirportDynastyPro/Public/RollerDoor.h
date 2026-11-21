// A roller door (owner 2026-11-21, Departure-Baggage-v2): a roll-up shutter of curved steel slats running in two side
// guides, winding up into the coil hood over the opening - the departure baggage block's 8 m door onto the baggage hall
// and the two 4 m doors of ramp services 13 (the departures vehicle passage). Its guides and hood are part of the
// building (Art/TerminalAnnex/make_bag_block.py); this actor is the moving curtain: its slats (one instanced list of
// /Game/Terminal/Meshes/roller_slat, Nanite) and the bottom bar. The door rule: it opens and closes with the use key
// (E / X) when you face it close by; tractors and other vehicles ask it to open (OpenFor). It never closes on anyone:
// while it runs down, anything in the doorway sends it back up. It closes by itself when nobody has been near it for a
// while.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RollerDoor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API ARollerDoor : public AActor
{
	GENERATED_BODY()
public:
	ARollerDoor();
	/** A roller door in a wall along the plan's x: the opening from X0 to X1 (plan metres), the curtain's plane at Z,
	 *  Height metres clear (floor at plan y 0.01). */
	static ARollerDoor* Place(UWorld* W, const FString& Id, float X0, float X1, float Z, float Height);
	virtual void Tick(float Dt) override;

	/** A roller door within reach that you face (feet position, the way you look): its open state. */
	static bool DoorNear(UWorld* W, const FVector& Feet, const FVector& Facing, bool& bIsOpen);
	/** E / X at a roller door you face: runs it up or down. True when there was one. */
	static bool UseDoor(const APawn* Me);
	/** A vehicle (or anyone) asks every roller door within Radius cm of At (world) to open and stay open for Seconds. */
	static void OpenFor(UWorld* W, const FVector& At, float Radius = 1500.f, float Seconds = 20.f);
	/** How far open (0 shut .. 1 fully up). */
	float Openness() const { return K; }
	float DoorWidth() const { return Width; }
	FString Id;

private:
	void Layout();
	bool SomethingInDoorway() const;
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Slats;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Bar;
	float Width = 8.f, Height = 4.f;      // metres
	float K = 0.f, Want = 0.f;            // openness now, and where it is going
	float HoldOpen = 0.f;                 // seconds a vehicle asked it to stay open
	float Quiet = 0.f;                    // seconds with nobody near while open
	static constexpr float Pitch = 0.1f;  // one slat's height (m)
	static TArray<TWeakObjectPtr<ARollerDoor>> All;
};
