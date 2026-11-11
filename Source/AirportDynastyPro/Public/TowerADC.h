// The control tower (ADC, aerodrome control) at your airport, as in the Android edition's tower.ts: at site A, west of
// the pier at the apron's west edge, in the terminal's design (white cladding, timber fins, the same glass). Walked
// into: the entrance hall's sliding glass doors, the lift from the ground floor to the lift lobby (+13.5 m), the spiral
// stair up into the cab (+17.6 m), the continuous ring of control desks with their screens and chairs (each chair its
// own object, ADC_CHAIR_nn, to swap for a better model), the desk's lift-up flap and the glass door out onto the
// balcony (the walkway round the cab with its glass railing). Everything is solid; the doors, the flap and the lift move
// as you come to them.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TowerADC.generated.h"

class UStaticMesh;
class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInterface;

UCLASS()
class AIRPORTDYNASTYPRO_API ATowerADC : public AActor
{
	GENERATED_BODY()

public:
	ATowerADC();

	/** Builds the tower at its place (plan metres, the terminal's frame) and its lift. */
	void Build();
	virtual void Tick(float DeltaSeconds) override;

	/** Where it stands (plan metres) and its levels (metres), as in tower.ts TOWER_AT / TOWER. */
	static const float AtX;
	static const float AtZ;
	static const float CabY;
	static const float LobbyY;

private:
	#pragma region Mesh building
	/** One material's triangles for a procedural shell (both faces, so it looks and stops you the same from either side). */
	struct FBuf { TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; };
	/** solid shells, see-through glass (solid), trim that is walked through */
	TMap<FName, FBuf> Solid, Glass, Trim;
	FBuf& Buf(TMap<FName, FBuf>& Set, const FName MatName) { return Set.FindOrAdd(MatName); }
	static FVector U(double X, double Y, double Z) { return FVector(X * 100.0, -Z * 100.0, Y * 100.0); }
	static void Tri(FBuf& B, const FVector& A, const FVector& Bp, const FVector& C);
	/** A ring surface from radius R0 at height Y0 to R1 at Y1, over the angles A0..A1 (a wall, a floor ring, a frustum). */
	static void Ring(FBuf& B, float R0, float Y0, float R1, float Y1, float A0, float A1, int32 N);
	/** A flat convex floor piece: the disc of radius R cut by half-planes (keep nx·x + nz·z ≤ d), at height Y. */
	static void DiscPiece(FBuf& B, float R, float Y, const TArray<FVector>& Cuts);
	/** A tube along a path (tower metres), for rails, the desk's lip and the mullions. */
	static void Tube(FBuf& B, const TArray<FVector>& Path, float Radius, int32 Sides = 8);
	UProceduralMeshComponent* Shells(USceneComponent* Parent, TMap<FName, FBuf>& Set, bool bCollide, bool bShadow, const FName Name);
	#pragma endregion

	#pragma region Pieces
	UMaterialInterface* Mat(const FName Name);
	UInstancedStaticMeshComponent* Kit(UStaticMesh* Mesh, const FName MatName, bool bCollide);
	/** A box (tower metres: width along x, height, depth along z; centre; turned by Yaw radians as in tower.ts). */
	void Box(const FName M, float W, float H, float D, float X, float Y, float Z, float Yaw = 0, bool bCollide = true, float PitchDeg = 0);
	/** A box placed in a turned frame: its origin (OX, OY, OZ) turned by Yaw, the box at (LX, LY, LZ) in that frame. */
	void BoxIn(const FName M, float W, float H, float D, float OX, float OY, float OZ, float Yaw, float LX, float LY, float LZ, bool bCollide = false);
	void Cyl(const FName M, float R, float H, float X, float Y, float Z, bool bCollide = true);
	/** A box as its own component under Parent (things that move or are swapped), in Parent's frame (metres). */
	UStaticMeshComponent* Part(USceneComponent* Parent, UStaticMesh* Mesh, const FName M, float W, float H, float D, float X, float Y, float Z, bool bCollide);
	USceneComponent* Node(USceneComponent* Parent, const FVector& At, const FRotator& Rot, const FName Name = NAME_None);
	#pragma endregion

	#pragma region The tower parts
	void BuildFoot();
	void BuildShaftAndLobby();
	void BuildStair();
	void BuildCab();
	void BuildDesk();
	void BuildRoof();
	/** Night lights outside (owner, 2026-11-11): downlights round the balcony under the roof's overhang, aimed away from
	 *  the cab's glass, and six over the entrance under its canopy; on from sunset as the light masts are. */
	void BuildNightLights();
	UPROPERTY() TArray<TObjectPtr<class USpotLightComponent>> NightLights;
	bool bNightOn = false;
	#pragma endregion

	#pragma region Moving parts
	TArray<TObjectPtr<USceneComponent>> EntranceDoors;   // two leaves, sliding apart (north and south)
	UPROPERTY() TObjectPtr<USceneComponent> BalconyLeaf;  // the balcony door's leaf, on its hinge
	UPROPERTY() TObjectPtr<USceneComponent> FlapHinge;    // the desk's flap, on its hinge at the desk's back
	FQuat FlapOpen = FQuat::Identity;
	float DoorSign = -1.f;
	float EK = 0, BK = 0, FK = 0;                         // entrance, balcony door, flap: 0 shut … 1 open
	#pragma endregion

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Frame;
	/** The glass's outward faces: not drawn while you are inside the tower (only the clear inner faces then, owner
	 *  2026-11-10: no white haze on the windows from inside). */
	UPROPERTY() TObjectPtr<class UProceduralMeshComponent> GlassOutside;
	bool bInsideLast = false;       // the tower's own frame, at its place
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Kits;
};
