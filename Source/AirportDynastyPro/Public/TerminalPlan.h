// Reads the terminal layout (Data/terminal-plan.json, real metres) shared with the Android edition.
#pragma once

#include "CoreMinimal.h"

#pragma region Plan data

/** One room: a box on a floor, in metres (x along the building, z north / airside). */
struct FPlanRoom
{
	FString Id, Name, Kind;
	int32 Floor = 1;
	float X0 = 0, Z0 = 0, X1 = 0, Z1 = 0;
	FString Glass; // sides that are glass: any of n s w e
};

struct FPlanDoor
{
	FString Id, Type;
	FString A, B; // the room it opens from, and the room (or outside area) on its other side
	int32 Floor = 1;
	float X = 0, Z = 0, Width = 2;
	bool bAlongX = true; // the opening runs along x (wall at a fixed z)

	/** Height of the opening in metres, as in the Android edition (plan.ts doorH). */
	float Height() const
	{
		if (Type == TEXT("open")) return 3.2f;
		if (Type == TEXT("museum")) return 3.0f;
		return Width >= 5.f ? 3.0f : Width >= 2.4f ? 2.7f : 2.25f;
	}
};

struct FPlanStair
{
	FString Id, Dir; // x+ x- z+ z-: the way the steps rise
	FString From, To; // the room at the bottom and the room at the top
	float X0 = 0, Z0 = 0, X1 = 0, Z1 = 0;
};

struct FPlanPoint
{
	FString Id;
	float X = 0, Z = 0;
	float Face = 0; // which way it faces (radians, 0 faces +z, as in the Android edition): the lifts' doors
};

struct FTerminalPlan
{
	float F2Y = 6, F1Ceil = 5.4f, F2Ceil = 11.4f, Roof = 12;
	FPlanPoint Spawn;
	TArray<FPlanRoom> Rooms;
	TArray<FPlanDoor> Doors;
	TArray<FPlanPoint> Columns, Lifts, Stands;
	TArray<FPlanStair> Stairs;

	/** Height of a floor's walking surface in metres. */
	float FloorY(int32 Floor) const { return Floor >= 2 ? F2Y : 0.f; }
	/** Ceiling height of a floor in metres. */
	float CeilY(int32 Floor) const { return Floor >= 2 ? F2Ceil : F1Ceil; }

	/** Loads the plan from the project's Data folder. False when the file is missing or broken. */
	bool Load();

	/** Plan metres to Unreal centimetres. Plan z (north) becomes Unreal -Y so nothing is mirrored. */
	static FVector ToWorld(float X, float Y, float Z) { return FVector(X * 100.f, -Z * 100.f, Y * 100.f); }
};

#pragma endregion
