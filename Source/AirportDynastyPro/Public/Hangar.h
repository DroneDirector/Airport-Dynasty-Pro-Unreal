// An aircraft hangar with its crew annex, in four sizes made from real aircraft (small: A318 or Learjet, medium: A320,
// large: A321, extra large: A380), built at run time from the owner's confirmed design (Blueprints/Hangars: the renders
// and his photos of a real hangar). The hall changes with the size; the two-storey crew annex against its west wall is
// the same in every size (lobby with steel stairs, the corridor from the hall with the toilets, the crew lounge, the
// store bay with its roll-up doors; upstairs the kitchen, an office and the mezzanine).
// The hall: dark grey steel portal frames (lattice trusses on the extra large), girts and X-bracing, white lining, a
// flat-looking roof behind a parapet with a glazed rooflight down the middle, twin-strip LED lamps lit as the terminal's
// hall is, the polished concrete floor, the multi-leaf sliding main door (leaves on floor rails, held upright by guide
// rollers in channels under the door head, driven by geared motors in their bogies, fed by festoon cables) stacking
// into its pockets, two personnel doors on each side wall, the kit of a maintenance hangar. Textures from the photos
// (Tools/make_hangar_textures.py, Tools/set_hangar.py).
// Where hangars stand comes from the airfield (Data/airfield.json "hangars"): each one's door line sits on the apron's
// edge and its centre line on its taxi lead-in line, so any size swapped in meets the same line.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Hangar.generated.h"

class UStaticMesh;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class ULocalLightComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;

#pragma region Sizes
/** One hangar size: clear inside width W and depth D (metres), door opening W x DH, the header over it, the door's leaves,
 *  the frame spacing and column depth, lattice trusses or portal frames, the rooflight's half width. */
struct AIRPORTDYNASTYPRO_API FHangarSpec
{
	FString Id, Name, ForTypes;
	float W = 46, D = 48, DH = 15, Header = 1.8f;
	int32 Leaves = 8;
	float Bay = 6, Col = 0.6f;
	bool bTruss = false;
	float Sky = 2.4f;

	static const TArray<FHangarSpec>& All();
	static const FHangarSpec& Get(const FString& Id);

	// derived measurements (metres), as the confirmed renders
	float Eave() const { return DH + Header; }
	float Ridge() const;
	float Top() const { return Ridge() + 0.78f; }                       // the parapet's coping
	float WallX() const { return W / 2 + Col + 0.12f; }                 // the side walls' outer face
	float LeafW() const { return W / Leaves + 0.15f; }
	int32 Tracks() const { return Leaves / 2; }
	float TrackZ(int32 T) const { return -0.45f - T * 0.34f; }
	float Front() const { return TrackZ(Tracks() - 1) - 0.45f; }       // the door head's front
	float Lip() const { return -(Front() - 0.3f); }                     // the threshold's front edge: this far in front of the door line
	float PocketX() const { return WallX() + LeafW() + 0.5f; }          // the door pockets' outer face
	float Back() const { return D + Col + 0.12f; }                      // the back wall's outer face
};
#pragma endregion

UCLASS()
class AIRPORTDYNASTYPRO_API AHangar : public AActor
{
	GENERATED_BODY()

public:
	AHangar();
	/** Builds a hangar of the given size; X is its centre line and ApronEdge the apron's edge its threshold meets (plan
	 *  metres, the airfield's frame); the main door open or shut. */
	void Build(const FString& SizeId, float X, float ApronEdge, bool bDoorOpen);
	virtual void Tick(float Dt) override;
	/** A press on one of the door's control buttons (OPEN, CLOSE, STOP; inside by the west pocket and outside on the
	 *  pocket's front): true if C was one. The door then runs as a real one: OPEN or CLOSE starts it, STOP holds it
	 *  where it is; fully open or shut takes 10 minutes. */
	bool Press(const UPrimitiveComponent* C);
	/** How far the main door is open (0 shut, 1 open), and where it is going (-1 closing, 0 still, 1 opening). */
	float DoorOpen() const { return DoorK; }
	int32 DoorMoving() const { return DoorRun; }

	/** The annex's size (the same in every hangar). */
	static constexpr float AnnexW = 13.f;
	static constexpr float AnnexL = 32.4f;
	static constexpr float AnnexTop = 9.2f;

private:
	#pragma region Pieces
	/** hangar metres (x across, y up, z from the door line into the hall) to the actor's own space (cm) */
	static FVector U(float X, float Y, float Z) { return FVector(X * 100.f, -Z * 100.f, Y * 100.f); }
	UMaterialInterface* Mat(const FName Name);
	UInstancedStaticMeshComponent* Kit(UStaticMesh* Mesh, const FName MatName, bool bCollide);
	/** An axis-aligned box between two corners. */
	void Box(const FName M, float X0, float Y0, float Z0, float X1, float Y1, float Z1, bool bCollide = true);
	/** A beam (box) from A to B, W wide and H deep, metres (its depth kept upright). */
	void Beam(const FName M, const FVector& A, const FVector& B, float W, float H, bool bCollide = true);
	/** A round bar or pipe from A to B. */
	void Rod(const FName M, const FVector& A, const FVector& B, float R, bool bCollide = false);
	/** An upright cylinder standing at (X, Y0, Z), R across, H tall. */
	void Cyl(const FName M, float X, float Y0, float Z, float R, float H, bool bCollide = true);
	/** A light: the twin-strip LED fitting's light, pointing down, as bright on the floor as the terminal's hall. */
	void Lamp(float X, float Y, float Z, float Reach);
	/** A door leaf on its hinge, opening by itself when someone comes within reach and shutting behind them: the hinge
	 *  at (HX, Z0) and the leaf along +x (bAlongX) or +z, W wide, H tall; the frosted pane of a wood-and-glass door. */
	void Swing(float HX, float Y0, float HZ, bool bAlongX, float W, float H, float Th, const FName M, bool bFrosted = false);
	/** A control button (a short cylinder pointing along Dir) */
	UStaticMeshComponent* Button(const FName M, const FVector& At, const FVector& Dir, float R, float Len, int32 What);
	/** the nearest bay's middle (between two frames) to Want, kept within Lo..Hi: where a wall door goes */
	float BayMid(float Want, float Lo, float Hi) const;
	/** the parts that follow: a node the next pieces are built on (a door leaf), or none (the building) */
	USceneComponent* Target = nullptr;
	#pragma endregion

	#pragma region The building
	void BuildFloor();
	void BuildFrame();
	void BuildSkin();
	void BuildDoor(bool bOpen);
	void BuildPersonnelDoors();
	void BuildLampsAndServices();
	void BuildKit();
	void BuildAnnex();
	void BuildAnnexRooms();
	#pragma endregion

	FHangarSpec S;
	float AZ0 = 0;                          // where the annex starts (hangar z)
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Kits;
	UPROPERTY() TArray<TObjectPtr<ULocalLightComponent>> Lights;
	// the main door: each leaf's node and how far it travels (cm, along x) from shut to open; the buttons; the beacons
	struct FLeaf { USceneComponent* Node; float Travel; };
	TArray<FLeaf> Leaves;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> LeafNodes;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Buttons;
	TArray<int32> ButtonWhat;                       // 1 open, -1 close, 0 stop
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Beacons;
	float DoorK = 0.f;                              // how far open
	int32 DoorRun = 0;                              // -1 closing, 0 still, 1 opening
	float Blink = 0.f;
	// the swinging doors: hinge node, which way it opened, how far (0..1), its middle (for who comes near)
	struct FSwingDoor { USceneComponent* Hinge; FVector Mid; float Open = 0, Sign = 1; };
	TArray<FSwingDoor> Swings;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> SwingNodes;
	float HallDoorZ = 0, RollupZ = 0;
	TArray<float> DoorZE, DoorZW;                   // the side walls' personnel doors (east, west), hangar z
	struct FSwingSide { bool bAlongX; };
	TArray<bool> SwingAlongX;               // where the wall door into the annex and the store's roll-up door are (hangar z)
	TArray<float> LightBase;
	float LightScale = -1.f, Since = 0.f;
};
