// The jetways move (owner, 2026-11-16; docs/Jetway-Animation-Plan.md): his own jetways (the "Jetways" model, J-1 ...)
// drive out to the aircraft's front door and back, exactly as his generator moves them (jetway.ts setPose / ik / path):
// the wheels lead along a gentle curve from the rest point - pulled HALF WAY BACK, his rule - to the door; the tunnel's
// turn, tilt and length follow from the wheels' point; its three sections telescope keeping their overlaps; the cab sits
// at the tunnel's end and turns square to the fuselage in the final crawl. Every moving part is moved from its docked
// place in the export (so nothing drifts from his model): its turn and slide since then are worked out each frame.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JetwayRigs.generated.h"

class ABabylonModel;
class UInstancedStaticMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API AJetwayRigs : public AActor
{
	GENERATED_BODY()
public:
	AJetwayRigs();
	/** Finds the jetways in the world and rigs them (once they are placed). */
	static AJetwayRigs* Get(UWorld* W);
	virtual void Tick(float Dt) override;

	enum class EState : uint8 { Parked, Docking, Docked, Undocking };
	int32 Num() const { return Rigs.Num(); }
	/** The jetway nearest a point (world), INDEX_NONE if none within MaxCm. */
	int32 Nearest(const FVector& At, float MaxCm = 6000.f) const;
	EState State(int32 J) const;
	FString Name(int32 J) const;
	/** Whether an aircraft stands at this jetway's stand (a parked one within reach of its door). */
	bool AircraftAt(int32 J) const;
	/** The aircraft at this jetway's stand, if any (its actor and its type, from its tag "<stand>/<TYPE>"). */
	AActor* AircraftAtStand(int32 J, FString* Type = nullptr) const;
	/** Why the last Dock / Undock was refused (shown on screen), empty if it went. */
	static FString LastNote;
	bool Dock(int32 J);
	bool Undock(int32 J);
	/** Out if parked, back if docked (the on-screen button). */
	void Toggle(int32 J);
	/** Where its console is (world), for E. */
	FVector Console(int32 J) const;
	/** The rotunda's pivot and the docked wheels' point (world), for cameras and tests. */
	bool Frame(int32 J, FVector& Rotunda, FVector& Door) const;
	/** The cab's middle and the way its doorway faces, now (world) - for cameras. */
	bool CabView(int32 J, FVector& Mid, FVector& Front) const;
	/** Every jetway's beacons flashing (tests and pictures), as if they were all moving. */
	static bool bBeaconTest;
	/** How strongly the gate-number signs glow (their own colours; faint). */
	static float SignGlow;
	/** Pictures: the beacons held lit (1), dark (0), or flashing as usual (-1). */
	static int32 BeaconForce;
	/** How fast the moves run (1 = real jetway speeds; tests speed them up). */
	static float Speed;

private:
	struct FPart { UInstancedStaticMeshComponent* K = nullptr; int32 Ix = INDEX_NONE; FTransform Docked; };
	struct FGroup { FTransform Docked; TArray<FPart> Parts; };
	/** A part stretched between two moving parts (bellows, their linings, the floor plates across them): its ends ride
	 *  with group A and group B, it turns to lie between them and grows or shrinks along its length. */
	/** A cab door leaf: turned about its hinge (vertical) from open (as exported, docked) to shut. */
	struct FLeaf { FVector H; float Close = 0.f; TArray<FPart> Parts; };
	/** Someone standing in a moving jetway: the jetway part under them and where it was before this frame's move. */
	struct FRider { TWeakObjectPtr<class ACharacter> Ch; UInstancedStaticMeshComponent* K = nullptr; int32 Ix = INDEX_NONE; FTransform Before; };
	struct FStretch { FPart P; int32 A = 0, B = 0, Axis = 0; FVector E1, E2; };
	enum EGroup { Boom, Pitch, Tun1, Tun2, Tun3, Cab, CabSleeve, Bogie, Sleeves, NumGroups };
	struct FRig
	{
		FString Id; TWeakObjectPtr<ABabylonModel> Model;
		FGroup G[NumGroups];
		TArray<FStretch> Stretch;
		TArray<FLeaf> Leaves; float Shut = 1.f;   // (the cab's front doors: 1 open, 0 shut)
		// the warning beacons (two on poles on the cab's roof at its front corners, one hanging under it): their copies,
		// their glow and the light each throws, which side's flash each follows
		UInstancedStaticMeshComponent *Bcn = nullptr, *BcnRed = nullptr, *Bar = nullptr; TArray<UInstancedStaticMeshComponent*> BcnK; TArray<int32> BcnIx; TArray<uint8> BcnKind; TArray<USceneComponent*> BcnGlow, BcnLight, BcnFill; TArray<uint8> BcnSide; bool bBcnOn = false, bBarOn = false;
		TArray<FQuat> BcnRel; TArray<bool> BcnFixed; uint32 LampKey = 0xffffffff; float Wait = 0.f, Env = 1.f, EnvLp = 20000.f;
		class UAudioComponent *SBeep = nullptr, *SMotor = nullptr, *STyres = nullptr, *SLift = nullptr;
		// the gate-number sign on the cab's roof, softly lit on both faces (owner: far weaker than the beacons)
		TArray<USceneComponent*> SignLight; TArray<FVector> SignAt;
		TArray<FPart> Stair; TArray<bool> StairLevel; FVector StairTop, StairU; float StairSl = 0.f, Ground = 0.f;
		FVector BoomO, PD, Wd, Wp, Ctl, CabOff;     // (model space, cm)
		FVector CabMidD = FVector::ZeroVector, FrontD = FVector::ForwardVector;   // the cab body's middle and the way its door faces (docked, model space)
		FVector DirD, AxisD; float LenD = 0.f, PitchD = 0.f, Turn0 = 0.f;
		EState St = EState::Docked; float T = 0.f, S = 1.f, Turn = 0.f;
		float D = 0.f, V = 0.f, PathL = 0.f; TArray<float> Arc;
		FVector WdBase = FVector::ZeroVector; FString DockedType = TEXT("A320");   // (the export's docked point: the A320's door; per type it moves from there)   // (metres driven along the path, speed m/s, the path's length, S by distance)
	};
	TArray<FRig> Rigs;
	bool bBuilt = false; float Look = 0.f, BcnClock = 0.f;
	void AddBeacons(FRig& R, ABabylonModel* M, const FString& Id);
	void Beacons(FRig& R, bool bMoving);
	void Sounds(FRig& R, bool bMoving, float Dt);
	void GatherRiders(const FRig& R, TArray<FRider>& Out) const;
	void CarryRiders(const TArray<FRider>& Riders) const;
	void AddSignLights(FRig& R, ABabylonModel* M, const FString& Id);
	void Build();
	void Apply(FRig& R);
	FVector PathAt(const FRig& R, float S) const;
	void SetDoor(FRig& R, AActor* Plane, const FString& Type);
	void MakeArc(FRig& R) const;
	float SAt(const FRig& R, float Dm) const;
};
