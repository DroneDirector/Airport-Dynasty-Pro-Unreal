// The body you see climbing into a crash tender and driving it (owner, 2026-11-11: "a real animation ... you step up
// into the truck, lift yourself up and into the seat ... feet actually touch ... hands actually touch ... hands on the
// steering wheel while you're driving", like the big open-world games, no black or loading screens).
// The player's own mannequin (SKM_Manny_Simple) on a poseable mesh, posed every frame from where its pelvis, feet and
// hands must be: two-bone IK for each leg and arm (the knee and elbow bend the way they should), the feet set flat or
// tipped onto a pedal, the fingers closing round what the hands hold. Everything in the poseable mesh's own space (cm).
#pragma once

#include "CoreMinimal.h"

class UPoseableMeshComponent;

struct FDriverPose
{
	FVector Pelvis = FVector::ZeroVector;      // the pelvis bone
	FVector Facing = FVector(0, 1, 0);         // where the body faces (flat)
	float Lean = 0.f;                          // forward lean of the whole body, degrees (back for negative)
	FVector Foot[2] = {FVector::ZeroVector, FVector::ZeroVector};      // where each foot's sole stands (left, right)
	float FootPitch[2] = {0.f, 0.f};           // toes up (+) or down, degrees
	bool bHand[2] = {false, false};            // a hand holding something (else the arm hangs)
	FVector Hand[2] = {FVector::ZeroVector, FVector::ZeroVector};      // the wrist's place
	float Grip[2] = {0.f, 0.f};                // 0 open, 1 the fingers closed round a bar
	// the idle movements (owner 2026-11-21: natural idle body movement with IK where it is needed) - all neutral by default.
	// Directions and offsets below are in the BODY's frame: X its left, Y its front, Z up (cm / unit vectors).
	float HeadYaw = 0.f, HeadPitch = 0.f, HeadRoll = 0.f;   // degrees: turned to its left (+), nodded down (+), the left ear down (+)
	float Twist = 0.f;                         // the upper body turned to its left (+), degrees, spread over the spine
	float ShoulderUp[2] = {0.f, 0.f};          // the shoulder (clavicle) raised (+), degrees
	float ShoulderFwd[2] = {0.f, 0.f};         // the shoulder brought forward (+) or back, degrees
	/** A hand placed on the body: 0 none (Hand is the target), else the landmark it lies on (EHandOn), the wrist HandOff
	 *  from it; HandOnW blends from Hand (0) to the landmark (1). */
	uint8 HandOn[2] = {0, 0};
	FVector HandOff[2] = {FVector::ZeroVector, FVector::ZeroVector};
	float HandOnW[2] = {0.f, 0.f};
	FVector Pole[2] = {FVector::ZeroVector, FVector::ZeroVector};      // where the elbow points (zero: down, back and out)
	FVector PalmDir[2] = {FVector::ZeroVector, FVector::ZeroVector};   // which way the palm faces (zero: as the forearm leaves it)
	FVector FingerDir[2] = {FVector::ZeroVector, FVector::ZeroVector}; // which way the fingers point (optional, with PalmDir)
	float PalmW[2] = {0.f, 0.f};
	int8 LookHand = -1;                        // the head (then the neck, a little of the spine) looks at this hand's target
	float LookW = 0.f;
	/** The landmarks a hand can lie on, found on the body's own bones as it stands posed. */
	enum EHandOn : uint8 { OnNone = 0, OnHeadTop, OnHeadSide, OnMouth, OnNeckBack, OnLowerBack, OnHip, OnOtherUpperArm, OnOtherWrist, OnChest };
};

class FTruckDriverRig
{
public:
	bool Init(UPoseableMeshComponent* Mesh);
	void Pose(const FDriverPose& P);
	bool IsReady() const { return Mesh != nullptr; }
	/** The eyes' place in the last pose (between the eyes, a little in front of the face). */
	FVector Eyes() const { return EyesAt; }
	float PelvisHeight() const { return PelvisH; }
	/** The last pose: every bone's local transform in the mesh's own order (handed to your MetaHuman, DriverPoseAnim.h). */
	const TArray<FTransform>& LocalPose() const { return Local; }
	/** The hip joint and the knee of side S in the last pose (the mesh's own space), and the hips in its rest pose - for
	 *  resting the hands on the thighs and the thighs on a seat (People.cpp). */
	FVector HipAt(int32 S) const { return CS.IsValidIndex(Thigh[S]) ? CS[Thigh[S]].GetLocation() : FVector::ZeroVector; }
	FVector KneeAt(int32 S) const { return CS.IsValidIndex(Calf[S]) ? CS[Calf[S]].GetLocation() : FVector::ZeroVector; }
	/** The sitting IK step (owner, 2026-11-19: every sitting pose, yours too): each holding hand of the pose just made moved
	 *  onto the top of the thigh it rests on (ThighR thick), by Weight, and the body posed again with them there. */
	void RestHandsOnThighs(FDriverPose& P, float ThighR, float Weight);
	/** A body's thigh thickness (half its thigh's width) from its own hips. */
	/** The sitting IK step, the seat under the thighs (owner, 2026-11-19: her leg through the cushion's front): how deep (cm)
	 *  the thighs' undersides go into what is below them, sampled from hip to knee, the mesh placed at Frame (world). */
	float ThighsInSeat(UWorld* W, const FTransform& Frame, float ThighR, const AActor* Ignore) const;
	float ThighRadius() const { return FMath::Clamp(HipWidth() * 0.42f, 6.f, 12.f); }
	float HipWidth() const { return RefCS.IsValidIndex(Thigh[0]) && RefCS.IsValidIndex(Thigh[1]) ? FVector::Dist(RefCS[Thigh[0]].GetLocation(), RefCS[Thigh[1]].GetLocation()) : 18.f; }

private:
	UPoseableMeshComponent* Mesh = nullptr;
	TArray<FTransform> RefLocal, RefCS, Local, CS;
	TArray<int32> Parent;
	int32 Pelvis = INDEX_NONE, Head = INDEX_NONE;
	int32 Thigh[2], Calf[2], Foot[2], Upper[2], Lower[2], Hand[2];
	TArray<int32> HandPts[2];
	TArray<int32> Spine, Neck;  // spine_01.. and neck_01.. (the idle movements: the upper body's turn, the head's look)
	int32 Clav[2] = {INDEX_NONE, INDEX_NONE};
	FVector PalmL[2], AlongL[2];   // the palm's side and the fingers' way in each hand bone's own space
	float LeftSign = 1.f;   // (the palm and the fingers' joints: the sitting IK step keeps them out of the thighs)
	float AnkleH = 8.f, PelvisH = 95.f;
	struct FCurl { int32 Bone; FVector Axis; float Weight; };
	TArray<FCurl> Curl[2];
	FVector EyesAt = FVector::ZeroVector;

	void Rebuild();
	void SetCS(int32 Bone, const FTransform& T);
	void Limb(int32 A, int32 B, int32 C, const FVector& Target, const FVector& Bend, const FVector& RefBend);
	FVector Body(const FQuat& Yaw, const FVector& V) const;   // a body-frame vector (X left, Y front, Z up) in mesh space
	FVector Landmark(uint8 On, int32 S, const FVector& Off, const FQuat& Yaw) const;
	void TurnCS(int32 Bone, const FQuat& Q);   // turn a bone about its own joint (mesh space), its children with it
};
