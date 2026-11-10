// The Babylon edition's own model, placed as it is: the terminal (and with it everything the Babylon scene holds: the
// wave roof, the interior, doors, stairs, pillars, jetways, the museum, the rental car centre, the site). Its meshes,
// materials and textures were imported unchanged from the glTF the Babylon edition exported (Content/Babylon/<Name>);
// this actor reads the model's node tree (Data/babylon-<name>.json) and puts every mesh where the model has it, with
// the same rules Unreal's glTF importer uses (positions (x, z, y), rotations (-x, -z, -y, w), metres to centimetres).
// A mesh used many times (seats, lamps, chairs) becomes one instanced list. Geometry is never changed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BabylonModel.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API ABabylonModel : public AActor
{
	GENERATED_BODY()

public:
	/** The lounge fireplace's fire: flame panels, its sound, its lights flickering (Tick). */
	void AddFire(const FVector& LightAt);
	void FlickerFire();
	/** The lounge's fireplace made deeper (owner, 2026-11-10): its front (the dark breast, the golden frames and rays,
	 *  the hearth) out into the room by Depth metres; the fire, its logs and its back wall stay where they are. */
	void DeepenFireplace(float Depth);
	UPROPERTY() TArray<TObjectPtr<class ULocalLightComponent>> FireLights;
	TArray<float> FireBase;
	/** Where the fire is, in the world (for the test pictures). */
	FVector FireWorld = FVector::ZeroVector;
	/** The gate screens' faces by gate number (each its own mesh), for the live gate pictures (Boards.cpp). */
	UPROPERTY() TMap<int32, TObjectPtr<UInstancedStaticMeshComponent>> GateScreens;
	ABabylonModel();
	virtual void Tick(float Dt) override;

	/** Places the model called Name (Data/babylon-<lowercase name>.json, Content/Babylon/<Name>). False when it isn't there. */
	bool Place(const FString& Name);
	/** The model's lights as Babylon has them (Data/babylon-<name>-lights.json: points and spots, their colours, strengths
	 *  and reach), put in the same places. */
	void AddLights(const FString& Name);
	/** Where a sliding door is in the world (for the door check, -UIDoor); false when the model has none. */
	bool SlidingDoorAt(FVector& Out) const;

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> ByMesh;

	#pragma region Doors
	// The model's doors move as in the Babylon edition (terminal/doors.ts): sliding leaves part, swing leaves turn on their
	// hinges, as you come up to them (the same floor, within reach), and shut behind you; revolving doors turn all the
	// time. Each moving part is a node of the model (a sliding leaf, a hinge, the revolving wings) with the mesh instances
	// under it; moving it moves them (and their collision).
	struct FInst { UInstancedStaticMeshComponent* K; int32 I; FTransform Rel; };
	struct FPart { FTransform Base, Parent; TArray<FInst> Inst; float Slide = 0; float Swing = 0; bool bSpin = false; };
	struct FDoor { FVector At; float Reach = 2.6f; float K = 0; TArray<FPart> Parts; bool bSpin = false; };
	TArray<FDoor> Doors;
	float Clock = 0;
	void MovePart(FPart& P, float E, float Spin);
	/** The model's light fittings made to shine (Babylon draws them glowing; here they also light the rooms): the hall's
	 *  downlights under the wave roof as spotlights, the rooms' ceiling lamps as soft lights, the jetways' LED strips as
	 *  rows of lights; and every glowing material brighter so it blooms. */
	/** The lamps' light into the rooms and their base brightness: always on, but by day the daylight outshines them
	 *  (owner, 2026-11-09), so how much they light the walls and floors follows the sun: full in the dark, dusk and
	 *  dawn, about a fifth at midday, changing as smoothly as the daylight does. The lamps' faces stay lit. */
	UPROPERTY() TArray<TObjectPtr<class ULocalLightComponent>> LampLights;
	TArray<float> LampBase;
	float LampScale = -1.f;
	void LampsByDaylight();
	/** Whites as real paint, strong non-lamp glows halved (Unreal lights them far more than Babylon did). */
	void ToneDown();
	void LightFittings(const TArray<TSharedPtr<FJsonValue>>& Nodes, const TArray<FTransform>& NodeWorld, const TArray<TPair<UInstancedStaticMeshComponent*, int32>>& NodeInst);
	#pragma endregion
};
