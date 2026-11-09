// The fittings in the rooms, placed to the drawings as in the Android edition (furnish.ts): in the Grand Hall the two
// check-in islands (desks 1–24 with their belts, banners, chairs and queue lines), the eight bag drop points, the trees
// in their planters, benches, bins and signs; at every gate the rows of seats, the gate desk and the gate's sign.
// Everything is instanced (one list per shape and material), so a thousand chairs cost little to draw.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerminalPlan.h"
#include "TerminalFittings.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

UCLASS()
class AIRPORTDYNASTYPRO_API ATerminalFittings : public AActor
{
	GENERATED_BODY()

public:
	ATerminalFittings();

	/** Places the fittings of every room the plan has. */
	void Build(const FTerminalPlan& Plan);

private:
	#pragma region Placing
	/** Where an object stands: plan metres (x, height, z) and its turn (radians, 0 faces +z, as in the Android edition). */
	struct FAt { float X = 0, Y = 0, Z = 0, Yaw = 0; };
	enum class EShape : uint8 { Box, Cyl, Ball };
	/** One part of an object: its size and its centre in the object's own frame (x across, y up, z forward), in metres. */
	void Part(const FAt& O, EShape Shape, const FName Mat, float W, float H, float D, float X, float Y, float Z);
	void Box(const FAt& O, const FName Mat, float W, float H, float D, float X, float Y, float Z) { Part(O, EShape::Box, Mat, W, H, D, X, Y, Z); }
	void Cyl(const FAt& O, const FName Mat, float R, float H, float X, float Y, float Z) { Part(O, EShape::Cyl, Mat, R * 2, H, R * 2, X, Y, Z); }
	void Ball(const FAt& O, const FName Mat, float R, float X, float Y, float Z, float SY = 1) { Part(O, EShape::Ball, Mat, R * 2, R * 2 * SY, R * 2, X, Y, Z); }
	/** Words on a sign, facing the object's forward (+1) or back (-1). */
	void Text(const FAt& O, const FString& Words, float X, float Y, float Z, float Facing, float Size, FColor Colour);
	UInstancedStaticMeshComponent* Kit(EShape Shape, const FName Mat);
	#pragma endregion

	#pragma region Objects
	void OfficeChair(const FAt& O);
	void Seats(const FAt& O, int32 N);
	void Plant(const FAt& O, bool bBig);
	void TreePlanter(const FAt& O);
	void Bin(const FAt& O);
	void Sign(const FAt& O, const FString& Words, float W, float H, const FName Back, FColor Colour);
	#pragma endregion

	#pragma region Rooms
	void Hall(const FPlanRoom& R);
	void Gate(const FPlanRoom& R);
	#pragma endregion

	float F2Y = 6;
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Kits;
	UPROPERTY() TObjectPtr<UStaticMesh> Meshes[3];
};
