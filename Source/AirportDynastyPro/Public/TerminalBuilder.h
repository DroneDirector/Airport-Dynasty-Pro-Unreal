// Builds the grey-box terminal at run time from the shared layout: floors, walls with door openings,
// glass frames, columns, stairs, lifts and stand markings. Everything is instanced so the terminal can
// grow in play (new pieces are just more instances); proper art replaces each kit piece later.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerminalPlan.h"
#include "TerminalBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

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
	UInstancedStaticMeshComponent* Kit(const TCHAR* Name, UStaticMesh* Mesh, const FLinearColor& Colour);
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
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Ground;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Floors;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Ceilings;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Walls;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Frames;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Pillars;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Steps;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Markings;
};
