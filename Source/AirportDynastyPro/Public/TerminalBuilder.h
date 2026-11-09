// Builds the terminal at run time from the shared layout: floors, walls with door openings, glass walls, columns,
// stairs, lifts and stand markings, each in the material the Android edition gives it (its kit.ts and shell.ts).
// Everything is instanced (one instanced piece list per shape and material) so the terminal can grow in play (new
// pieces are just more instances); proper models replace each kit piece later.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerminalPlan.h"
#include "TerminalBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS()
class AIRPORTDYNASTYPRO_API ATerminalBuilder : public AActor
{
	GENERATED_BODY()

public:
	ATerminalBuilder();

	/** Reads the layout and places every piece. Safe to call again: old pieces are cleared first. */
	bool Build();

	const FTerminalPlan& GetPlan() const { return Plan; }

private:
	#pragma region Kit
	/** The instanced boxes in one kit material (Content/Materials/Kit/MI_<Mat>), made the first time it is asked for. */
	UInstancedStaticMeshComponent* Mat(const FName Name);
	/** The same for cylinders (the roof columns). */
	UInstancedStaticMeshComponent* MatCyl(const FName Name);
	UInstancedStaticMeshComponent* MakeKit(const FName Key, UStaticMesh* Mesh, UMaterialInterface* Material);
	/** Adds a box between two corners, in plan metres (x, height, z). */
	void Box(UInstancedStaticMeshComponent* K, float X0, float Y0, float Z0, float X1, float Y1, float Z1);
	#pragma endregion

	#pragma region Parts
	void BuildRoom(const FPlanRoom& R);
	void BuildWall(const FPlanRoom& R, bool bAlongX, float Fixed, float From, float To, bool bGlass);
	void BuildStair(const FPlanStair& S);
	#pragma endregion

	FTerminalPlan Plan;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
	/** Every instanced piece list, by mesh and material ("box:terrazzo", "cyl:white", "glass"). */
	UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Kits;
};
