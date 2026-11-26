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
	/** A roller door anywhere: the middle of its sill at Foot (world cm), its curtain running along the actor's x turned
	 *  YawDeg, Width x Height metres clear. The curtain's slat and bottom bar are their own meshes (empty paths: the
	 *  terminal's roller_slat / roller_bar), so a later pass can give a door other leaves without touching its wiring.
	 *  PanelPitch > 0: a sectional (garage) door - the slat is one insulated panel made to the door's width (its own
	 *  bottom gap built in), stacked PanelPitch apart and sliding up into the head; WinPath: the panel with the row of
	 *  vision windows, put in place of the panel at eye height; the bar is its bottom rail with seal and handle. */
	static ARollerDoor* PlaceAt(UWorld* W, const FString& Id, const FVector& Foot, float YawDeg, float Width, float Height,
		const FString& SlatPath = FString(), const FString& BarPath = FString(), float PanelPitch = 0.f, const FString& WinPath = FString());
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
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Win;   // sectional: the vision-window panel (or none)
	float Panel = 0.f;                    // sectional panel pitch (m); 0: a roller curtain of Pitch slats
	float PanelLo = 0.f;                  // sectional: the panel mesh's own bottom above its origin (m, the seal's room)
	int32 WinIdx = INDEX_NONE;            // sectional: which panel (from the bottom) has the windows
	FQuat LeafTurn = FQuat::Identity;     // sectional: turns the panel mesh so its width runs along the door
	float Width = 8.f, Height = 4.f;      // metres
	float K = 0.f, Want = 0.f;            // openness now, and where it is going
	float HoldOpen = 0.f;                 // seconds a vehicle asked it to stay open
	float Quiet = 0.f;                    // seconds with nobody near while open
	/** Its sound (step B2): the motor and the curtain rolling (the fire station's door recording), down to the bottom bar's
	 *  thud as it lands; heard from where it is, muffled through walls. Dir: which way it runs (0 standing). */
	UPROPERTY() TObjectPtr<class UAudioComponent> Hum;
	int8 Dir = 0;
	static constexpr float Pitch = 0.1f;  // one slat's height (m)
	static TArray<TWeakObjectPtr<ARollerDoor>> All;
};
