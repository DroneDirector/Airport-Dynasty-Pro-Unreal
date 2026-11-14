// The interaction outline (owner, 2026-11-14): a very thin soft glow round everything within reach that you can pick up
// or use - the wash hose's gun, your phone, the office computers and planners, the operations monitors, the wardrobe,
// the hangar door buttons, the screens with the game on them, and the seats you can sit on - never doors, never
// vehicles. What's within 4 m of you is drawn into the custom-depth stencil (an instanced piece through a stand-in copy
// that draws nothing else), and a post-process material (Tools/set_outline_material.py) lays the line round it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractOutline.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API AInteractOutline : public AActor
{
	GENERATED_BODY()
public:
	AInteractOutline();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	/** Seats and other things that list themselves (the seats: Seats.h). An instance of an instanced mesh, or a whole
	 *  component (Item INDEX_NONE). */
	static void Add(UPrimitiveComponent* C, int32 Item = INDEX_NONE, uint64 Key = 0);
	/** Outlines on or off (Settings). */
	static bool bOn;
	static constexpr float Reach = 400.f;

private:
	// (Key: the pieces of one thing share it - a chair's legs and seat, a phone's parts - and light up together)
	struct FThing { TWeakObjectPtr<UPrimitiveComponent> C; int32 Item = INDEX_NONE; uint64 Key = 0; };
	static TArray<FThing> Listed;
	TArray<FThing> Found;               // everything there is, gathered now and then
	void Gather();
	float GatherIn = 1.f, PickIn = 0.f;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> StandIns;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Marked;   // whole components drawn into the stencil now
	UPROPERTY() TObjectPtr<class APostProcessVolume> Volume;
};
