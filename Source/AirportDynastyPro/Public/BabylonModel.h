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
	/** The model's name as placed (Office, Ops, Smartphone, Monitor329...). */
	FString Model;
	/** Which of the model's objects an instance is (its node's name, e.g. "laptop~b-131": what you click; the Babylon
	 *  edition's own click name before the ~ where it has one). Empty when unknown. */
	FString NodeAt(const UPrimitiveComponent* C, int32 Item) const;
	/** Where a sliding door is in the world (for the door check, -UIDoor); false when the model has none. */
	bool SlidingDoorAt(FVector& Out) const;

	#pragma region MovingParts
	// Any of the model's parts moved by code (the fire trucks' wheels, suspension, steering wheel, pedals and doors; the
	// fire station's bay doors): a group is a set of the model's nodes with every mesh instance under them, held relative
	// to a pivot; moving the pivot moves them all. Everything in the model's root space (its own frame: a model made in
	// Blender has Blender's (x, y, z) metres at (x, -y, z) * 100 in it).
	/** A group of the nodes named Names (exactly, or Blender's numbered copies "Name.001"; or every name starting so when
	 *  bPrefix), held relative to Pivot. INDEX_NONE when none matched. */
	int32 BindGroup(const TArray<FString>& Names, bool bPrefix, const FTransform& Pivot);
	/** A group of the nodes a test picks (by name, glTF mesh number and place in root space). */
	int32 BindGroupWhere(TFunctionRef<bool(const FString& Name, int32 Mesh, const FVector& At)> Pick, const FTransform& Pivot);
	/** Moves a group's pivot to NewPivot; its parts follow. Call FlushMoves once after a frame's moves. */
	void MoveGroup(int32 Group, const FTransform& NewPivot);
	void FlushMoves();
	/** A node's place in root space; false when the model has no node so named. */
	bool NodeBase(const FString& Name, FTransform& Out) const;
	/** Every node called Name or Name.### (copies of one part): their places in the model's root space. */
	void NodesNamed(const FString& Name, TArray<FTransform>& Out) const;
	/** The model's root (its frame). */
	USceneComponent* GetModelRoot() const { return Root; }
	/** Collision on or off for all the model's parts (a fire truck collides with its physics body instead). */
	void SetPartsCollision(bool bOn);
	#pragma endregion

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> ByMesh;
	TMap<const UInstancedStaticMeshComponent*, TArray<FString>> InstNames;

	#pragma region Doors
	// The model's doors move as in the Babylon edition (terminal/doors.ts): sliding leaves part, swing leaves turn on their
	// hinges, as you come up to them (the same floor, within reach), and shut behind you; revolving doors turn all the
	// time. Each moving part is a node of the model (a sliding leaf, a hinge, the revolving wings) with the mesh instances
	// under it; moving it moves them (and their collision).
	struct FInst { UInstancedStaticMeshComponent* K; int32 I; FTransform Rel; };
	struct FPart { FTransform Base, Parent; TArray<FInst> Inst; float Slide = 0; float Swing = 0; bool bSpin = false; };
	struct FDoor { FVector At; float Reach = 2.6f; float K = 0; TArray<FPart> Parts; bool bSpin = false; };
	TArray<FDoor> Doors;
	// every node kept after placing (for the moving parts): its name, glTF mesh, place in root space, children, instance
	TArray<FString> NName; TArray<int32> NMesh; TArray<FTransform> NRoot; TArray<TArray<int32>> NKids;
	TArray<TPair<UInstancedStaticMeshComponent*, int32>> NInst;
	struct FGroup { TArray<FInst> Inst; };
	TArray<FGroup> Groups;
	TSet<UInstancedStaticMeshComponent*> MovedLists;
	int32 MakeGroup(const TArray<int32>& Picked, const FTransform& Pivot);
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
	/** The entrance canopies' undersides matte (not lamps), the entrance doors' glass as the facade's. */
	void MatchSurfaces();
	/** The gates' (and every other) row of seats: the Babylon boxes hidden, our detailed Nanite seating in their place
	 *  (Tools/make_gate_seat.py: a seat per place on the beam, X-legs with armrests at the ends and every third seat,
	 *  armrests between the others). */
	void ReplaceSeats(const TArray<TSharedPtr<FJsonValue>>& Nodes, const TArray<FTransform>& NodeWorld, const TArray<TPair<UInstancedStaticMeshComponent*, int32>>& NodeInst);
	void LightFittings(const TArray<TSharedPtr<FJsonValue>>& Nodes, const TArray<FTransform>& NodeWorld, const TArray<TPair<UInstancedStaticMeshComponent*, int32>>& NodeInst);
	#pragma endregion
};
