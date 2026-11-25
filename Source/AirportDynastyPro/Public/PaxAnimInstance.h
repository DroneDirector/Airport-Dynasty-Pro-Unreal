// The passengers' own animation instance (owner 2026-11-21: a carried suitcase held by its handle, the fingers visibly
// wrapped round it, the hand fixed there all the way). Still the single-node blend space (idle-walk-run) they always
// walked with - SetBlendSpacePosition / SetPlayRate / SetPlaying keep working - with one layer on top while a bag is
// carried in the right hand: the right fingers and thumb closed round a ~3 cm handle bar, and the right arm held a
// little out from the body so the case clears the thigh. Blended in and out over a quarter of a second.
//
// And the idle movements (owner 2026-11-21: natural idle body movement, IK where it is needed): a passenger who has
// stood still a while (a queue, the gate, the belt) now and then stretches, looks at the watch, looks round, shifts the
// weight, rolls the neck, crosses the arms, puts the hands in the pockets, checks a phone or a tablet, puts a hand on
// the hip or rubs the neck - layered on the idle pose in the proxy, blended in and out: two-bone IK for the arms onto
// places found on the body's own bones (the hip, the nape, the other upper arm, the chest), the palm turned onto what it
// touches, the fingers lightly curled; the look a chain (the head, then the neck, a little of the spine) within natural
// limits; a weight shift moves the pelvis while two-bone IK keeps both feet exactly where they stood.
//
// And the feet on the ground (to-do 290, owner 2026-11-25: the soles, their sides too, always above the ground while
// walking - apron, landside, terminal, stairs - but so little that it isn't noticed): everyone who walks, passengers, crew
// and the player. The shoe's real sole (its lowest points, from the mesh) is kept 2.5 mm over the ground found under the heel
// and the ball of each foot; on steps and slopes each foot comes down on its own step by two-bone IK, the hips following the
// lower one. Near the camera only; nobody sees a sole further off.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "TalkLook.h"
#include "PaxAnimInstance.generated.h"

class USkeletalMeshComponent;

/** The idle movements (UPaxAnimInstance::PlayFidget). */
namespace EPaxFidget
{
	enum Type : int32
	{
		None = -1,
		Stretch = 0,     // both arms up overhead (both arms)
		Watch,           // the left wrist up in front, the eyes on the watch (left arm)
		LookAround,      // the head, the neck and a little of the spine, one way then the other
		WeightShift,     // the hips from one foot to the other, the feet planted
		NeckRoll,        // the head tilted to one side, down, to the other
		ArmsCrossed,     // each hand on the other upper arm (both arms)
		Pockets,         // the hands in the front trouser pockets (both arms; the left one alone with a bag in the right)
		Phone,           // a phone (7 x 15 cm) in both hands at chest height, the head down at it (both arms)
		HandOnHip,       // the left hand on the hip (left arm)
		RubNeck,         // the left hand rubbing the nape, the head down (left arm)
		Tablet,          // a tablet (17 x 25 cm, landscape) in both hands, the head down at it (both arms)
		Num
	};
}

/** The soles of a body and what it wears: how far they reach under the body's own zero, and six points of each (the heel's back,
 *  inner and outer edge in the foot bone's space; the ball's inner and outer edge and the toe's tip in the ball bone's). */
struct FPaxSoles { bool bOk = false; float Sole = 0.f; FVector At[2][6]; };

/** The proxy: the blend space evaluated as always, then the idle movement, then the grip layer (worker thread). */
struct FPaxAnimProxy : public FAnimSingleNodeInstanceProxy
{
	FPaxAnimProxy() {}
	FPaxAnimProxy(UAnimInstance* In) : FAnimSingleNodeInstanceProxy(In) {}
	virtual void PreUpdate(UAnimInstance* In, float Dt) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void PostEvaluate(UAnimInstance* In) override;

	float Grip = 0.f;            // 0 open hand, hanging arm .. 1 closed round the handle, arm out
	float ArmOut = 10.f;         // degrees the upper arm is held out from the body at full grip
	float Twist = 0.f;           // degrees the upper body is turned from the way the feet go (+ to the right)
	// the idle movement (copied from the instance each update)
	FTalkLookLayer Look;   // (the one you talk to: the head to you, TalkLook.h)
	int32 FidKind = -1; float FidKt = 0.f, FidDur = 1.f, FidFade = 0.f, FidSide = 1.f; bool bFidSeated = false;
	FTransform DeviceCS = FTransform::Identity; float DeviceW = 0.f;   // (the phone / tablet held, component space; written on the worker)
	// the feet (copied each update; heights over the body's own zero): the soles, the weight, the ground under each point, each
	// foot's and the hips' eased rise, the ground's slope
	FPaxSoles Soles; float FeetW = 0.f, Under[2][6] = {}, Rise[2] = {0.f, 0.f}, HipRise = 0.f;
	FVector Slope[2] = {FVector::UpVector, FVector::UpVector};
private:
	struct FCurl { int32 Bone; FVector Axis; float Angle; };
	const void* RigFor = nullptr;   // the skeletal mesh asset the bones below were found on
	TArray<FCurl> Curl, CurlL;       // the right hand's (the bag's grip) and the left's (the idle movements)
	int32 Upper = INDEX_NONE, Lower = INDEX_NONE;   // mesh bone indices
	FVector Out = FVector::ZeroVector;              // the body's right side, component space
	// the idle movements' bones (mesh indices) and the body's frame in component space (its left, front, up)
	int32 Pelvis = INDEX_NONE, Head = INDEX_NONE;
	int32 Arm[2][3] = {{INDEX_NONE, INDEX_NONE, INDEX_NONE}, {INDEX_NONE, INDEX_NONE, INDEX_NONE}};
	int32 Leg[2][3] = {{INDEX_NONE, INDEX_NONE, INDEX_NONE}, {INDEX_NONE, INDEX_NONE, INDEX_NONE}};
	int32 Clav[2] = {INDEX_NONE, INDEX_NONE}, Ball[2] = {INDEX_NONE, INDEX_NONE};
	TArray<int32> Spine, Neck;
	FVector BL = FVector(1, 0, 0), BF = FVector(0, 1, 0), BU = FVector(0, 0, 1);
	FVector PalmL[2] = {FVector::ZeroVector, FVector::ZeroVector}, AlongL[2] = {FVector::ZeroVector, FVector::ZeroVector};
	bool bFidRig = false;
	void Rig(const FBoneContainer& BC);
	void Fidget(FCompactPose& Pose);
	void Feet(FCompactPose& Pose) const;
};

UCLASS(Transient, NotBlueprintable)
class AIRPORTDYNASTYPRO_API UPaxAnimInstance : public UAnimSingleNodeInstance
{
	GENERATED_BODY()
public:
	UPaxAnimInstance(const FObjectInitializer& OI) : Super(OI) {}
	/** Close the right hand round a carried bag's handle (and hold the arm out a little). */
	bool bGripRight = false;
	/** Degrees the right upper arm is held out from the body while gripping. */
	float ArmOut = 10.f;
	/** Past someone where it is tight: the shoulders turned this far (degrees, + to the right, at most 42) by the spine, the feet
	 *  going on ahead, the head still looking where they go, the arms a little in. */
	float Twist = 0.f;

	/** Play an idle movement (EPaxFidget) for Seconds (2-6 s is natural), blended in and out; Side +1 / -1 picks the way
	 *  (the look's first side, the weight's first foot). Replaces one playing. */
	void PlayFidget(int32 Kind, float Seconds, float Side = 1.f);
	/** Let the one playing blend out now (about a third of a second). */
	void StopFidget();
	bool IsFidgeting() const;
	int32 CurrentFidget() const { return IsFidgeting() ? FidKind : int32(EPaxFidget::None); }
	/** A random idle movement fit for the situation, never the last one again: seated, only the subtle ones (a look round,
	 *  the phone, the neck); with a bag in the right hand, none that needs the right arm. */
	int32 PickFidget(bool bSeated, bool bRightHandBusy) const;
	static bool FidgetUsesRightArm(int32 Kind);
	static bool FidgetSeatedOk(int32 Kind);
	/** The whole rule in one call, every tick of a passenger near the camera: bStill true while they stand (or sit) still
	 *  and nothing forbids it (not walking, not on an escalator or in a revolving door, not setting a bag down). After
	 *  30 s of it, a movement now and then (6-20 s of plain idle between, each 2-6 s); bStill false lets one playing blend
	 *  out at once and starts the 30 s again. */
	void IdleFidgets(float Dt, bool bStill, bool bSeated, bool bRightHandBusy);
	/** The phone or tablet held just now (Phone / Tablet playing): its centre and frame in WORLD space - X the screen's
	 *  "up" (away from the holder; the phone's long 15 cm side, the tablet's short 17 cm side), Y across (the phone's 7 cm,
	 *  the tablet's 25 cm), Z out of the screen towards the eyes - and Weight 0..1 how far the hands have brought it in
	 *  (show the device above ~0.3). False when no device movement is playing. */
	bool GetDeviceGrip(FTransform& OutWorld, float& OutWeight) const;
	/** A device held for as long as this says (the props, PaxProps): 0 none, 1 a phone, 2 a tablet - the Phone / Tablet
	 *  pose blended in and held (no idle movements meanwhile), blended out when set back to 0. */
	int32 Device = 0;
	/** A hand's own axes in its bone's space (along the fingers, out of the palm), from the reference pose. */
	static bool HandAxes(const USkeletalMeshComponent* Body, bool bRight, FVector& OutAlong, FVector& OutPalm);

	/** The feet kept on the ground (off while falling). */
	bool bFeet = true;
	/** Swap a body that walks with the plain single-node instance onto this one, keeping its blend space playing. */
	static UPaxAnimInstance* Install(USkeletalMeshComponent* Body);
	/** The grip point in hand_r's own space: where the closed fingers wrap round a handle bar (between the palm and the
	 *  curled fingers, at the middle finger's first joint). Zero when the skeleton has no such bones. */
	static FVector GripInHand(const USkeletalMeshComponent* Body);

	// (read by the proxy each update; DeviceCS / DeviceW written back after each evaluation, on the game thread)
	int32 FidKind = -1, LastFid = -1;
	double FidStart = -1.0; float FidDur = 0.f, FidSide = 1.f; bool bFidOn = false, bFidSeated = false;
	float StillT = 0.f, NextFidT = 0.f; bool bLooks = false;   // (bLooks: looking at you, TalkLook.h)
	FTransform DeviceCS = FTransform::Identity; float DeviceW = 0.f; int32 DeviceKind = -1;
	/** (the proxy's, each update on the game thread: the ground under the feet, near the camera) */
	void StepFeet(float Dt, FPaxAnimProxy& P);
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FPaxAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* P) override { delete P; }
private:
	FPaxSoles Soles; int32 SolesFor = 0; float SolesT = 0.f;   // (measured again when what they wear changed)
	float FeetW = 0.f, Rise[2] = {0.f, 0.f}, SmZ = 0.f; FVector Slope[2] = {FVector::UpVector, FVector::UpVector};
	FVector FeetAt = FVector::ZeroVector, FootWas[2] = {FVector::ZeroVector, FVector::ZeroVector}; bool bFeetOn = false; float UnevenT = 0.f;
	FTransform FeetXf;   // (where the body was for the pose that is on it)
	void MeasureSoles(const USkeletalMeshComponent* B);
};
