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
	ABabylonModel();

	/** Places the model called Name (Data/babylon-<lowercase name>.json, Content/Babylon/<Name>). False when it isn't there. */
	bool Place(const FString& Name);
	/** The model's lights as Babylon has them (Data/babylon-<name>-lights.json: points and spots, their colours, strengths
	 *  and reach), put in the same places. */
	void AddLights(const FString& Name);

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> ByMesh;
};
