// You work a vehicle's own controls (to-do 238, owner 2026-11-24: every feature of every airport vehicle, by its real button, lever
// or handle). The general layer: one table per vehicle (FVehTable: its controls, its solid pieces, a few hooks), no code of its own.
//  - you look at a control in your reach: its prompt, or why it can't now; E / X does it, held for the hold controls (a lever held:
//    the move keys push it the other way); your hand goes to the real part and stays on it; a seat's controls by your rider's hand
//  - what it drives moves by the same motion functions, speeds and interlocks as for the crew (the hooks only set what is wanted)
//  - its crew at work keep it; left standing open and unattended, it stows itself after two minutes
//  - standing near it, it is solid piece by piece: the floors carry you (FVehSolid), doors let you through when they are open
// How to add the next vehicle: docs/Hired-Crew-Plan.md "Vehicle controls (238)".
#pragma once

#include "GSE.h"
#include "TruckDriverRig.h"
#include "VehicleControls.generated.h"

class UBoxComponent;
class UPoseableMeshComponent;
class UDriverPoseAnim;
class UInstancedStaticMeshComponent;

/** One control. Push: a press (a motion toggles between A and B, or Switch is told); Hold: runs to B while held (A to B and back in turn
 *  when bTurns); Lever: as Hold in turn, and the move keys choose the way; Hand: the part itself, by its handle (as Lever, no glow). */
struct FVehControl
{
	enum EKind : uint8 { Push, Hold, Lever, Hand };
	const TCHAR* Part; const TCHAR* Point; EKind Kind; const TCHAR* Label;
	int32 Frame = 0; FVector Out = FVector::ZeroVector;   // the piece it rides on; the way it faces (only worked from that side)
	int32 Motion = INDEX_NONE; float A = 0.f, B = 1.f;
	int32 Switch = INDEX_NONE; bool bSeat = false, bTurns = true, bGrip = false, bCabin = false, bHelp = false;   // (bHelp: also while its crew work)
	//   // (bGrip: held in the hand, not pressed with a finger)
};

/** A solid piece: a box (root cm, in its frame) and when it is there (none: always). */
struct FVehSolid { FVector Min, Max; int32 Frame = 0; TFunction<bool(const AGSE::FTractor&)> On; };

struct FVehTable
{
	TArray<FVehControl> Controls; TArray<FVehSolid> Solids;
	TFunction<FTransform(const AGSE&, const AGSE::FTractor&, int32)> Frame;                     // where a piece is now (root)
	TFunction<float(const AGSE::FTractor&, int32)> Now;
	TFunction<void(AGSE::FTractor&, int32, float)> Want;                                        // a motion, as its crew ask for it
	TFunction<bool(const AGSE&, const AGSE::FTractor&, int32, float, FString&)> May;            // may it, or why not
	TFunction<float(const AGSE::FTractor&, int32, float)> End;                                  // (how far a motion's end is now; none: as the table says)
	TFunction<void(AGSE&, int32, const FVehControl&)> Press;                                    // its part under your finger moves
	TFunction<bool(AGSE&, int32, int32)> Switch;                                                // a light, the horn, a stop
	TFunction<uint8(const AGSE::FTractor&, const FVector&)> Aboard;                             // your feet (root) on it: which floor, 0 none
	TFunction<bool(const AGSE&, const AGSE::FTractor&)> Stowed;
	TFunction<void(AGSE&, AGSE::FTractor&)> Stow;
	// things you carry, steps you climb (all may be none): what a control says now (empty: not there now); may its switch, or why not;
	// what you carry of it; how far its hose lets you go (from a root point); let it go home (you are gone); the climb its crew do
	TFunction<FString(const AGSE&, const AGSE::FTractor&, const FVehControl&)> Say;
	TFunction<bool(AGSE&, int32, int32, FString&)> MaySwitch;
	TFunction<const TCHAR*(const AGSE::FTractor&)> Held;
	float Range = 400.f; bool bAlways = false;                                                   // how far off its things can be; solid piece by piece even stowed
	TFunction<bool(const AGSE&, const AGSE::FTractor&, const FVehControl&, FVector&)> Spot;     // (a control that lies somewhere else each time: where, world)
	TFunction<bool(const AGSE&, const AGSE::FTractor&, FVector&)> GripL;                        // (your left hand on it too, root)
	TFunction<bool(const AGSE&, const AGSE::FTractor&, FVector&)> Grip;   // (its handle, root: what you push stands on its own wheels)
	TFunction<float(const AGSE&, const AGSE::FTractor&, FVector&)> Leash;
	TFunction<void(AGSE::FTractor&)> Drop;
	TFunction<TFunction<void(float, float, FDriverPose&)>(const AGSE&, int32, int32, float&)> Climb;
	// E with the thing in your hand itself (a dead man, a ticket): what it does now or why not; E is held or not; how far it is pocketed
	TFunction<FString(const AGSE&, const AGSE::FTractor&, FString&)> HandSay;
	TFunction<void(AGSE&, AGSE::FTractor&, bool)> HandDo;
	TFunction<float(const AGSE::FTractor&)> Pocket;
	TFunction<void(AGSE&, int32, int32)> Worked;                                                // a motion you brought to its end (work that counts for the turn-round)
};

UCLASS()
class AIRPORTDYNASTYPRO_API AVehicleControls : public AActor
{
	GENERATED_BODY()
public:
	AVehicleControls();
	static AVehicleControls* Get(UWorld* W);
	virtual void Tick(float Dt) override;
	/** E / X: the control you look at; true when there is one. */
	static bool Interact(APawn* Who);
	/** A shared cab control by its key (CabControls.h): your hand goes to its part, where it has one. */
	static void CabKey(class AServiceDrive* Sd, int32 Code);
	/** Why something doesn't go (shown a moment). */
	static void Note(UWorld* W, const FString& Why);
	/** What to show while you look at one or hold one (empty: nothing). */
	static FString Prompt();
	/** The lever or button you look at or hold: it may glow (never the vehicle). */
	static void ChosenParts(TArray<TPair<UInstancedStaticMeshComponent*, int32>>& Out);

private:
	struct FHull { TArray<TWeakObjectPtr<UBoxComponent>> Box; TArray<bool> On; TWeakObjectPtr<UBoxComponent> Whole; bool bOn = false; };
	TMap<int32, FHull> Hulls;
	int32 Ti = INDEX_NONE, Ci = INDEX_NONE, GlowTi = INDEX_NONE, GlowCi = INDEX_NONE;
	enum class EDo : uint8 { None, Reach, Work, Back, Climb } Do = EDo::None;
	float T = 0.f, To = 0.f, W = 0.f; bool bPushed = false, bLedByMe = false, bEnd = false, bSqueeze = false; int32 Way = 0;
	FString Text; double TextAt = -1.0;
	TMap<TPair<int32, int32>, float> Last;   // (the way a motion went last: the other way next)
	TArray<TPair<UInstancedStaticMeshComponent*, int32>> Glow;
	TWeakObjectPtr<APawn> User; int32 KeyCode = INDEX_NONE;
	static FString SayOf(const AGSE& G, const AGSE::FTractor& V, const FVehControl& C);
	static bool MayOf(AGSE& G, int32 V, int32 Code, FString& Why);
	int32 HoldTi = INDEX_NONE; FVector HoldAt = FVector::ZeroVector, HoldF = FVector::ForwardVector;   // (what you carry: whose, where)
	TFunction<void(float, float, FDriverPose&)> ClimbFn; float ClimbS = 0.f; bool bClimbing = false; FTruckDriverRig Rig;
	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Rider; UPROPERTY() TObjectPtr<UDriverPoseAnim> Human;
	void Carry(AGSE& G, APawn* Me);
	bool Climb(AGSE& G, class AADPWalker& Wk, float Dt);
	void ClimbEnd(class AADPWalker& Wk);
	void Find(AGSE& G, APawn& Me);
	FString Label(AGSE& G, FString& No) const;
	float Target(AGSE& G, const FVehControl& C, int32 Dir) const;
	void Hull(AGSE& G, APawn* Me, float Dt);
	void Arm(AGSE& G, APawn* Me);
	void Stop(AGSE& G);
	bool Crew(const AGSE& G, int32 V) const;
	FVector At(const AGSE& G, int32 V, const FVehControl& C, FVector* Out = nullptr) const;
};
