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
	TArray<int32> HandPts[2];   // (the palm and the fingers' joints: the sitting IK step keeps them out of the thighs)
	float AnkleH = 8.f, PelvisH = 95.f;
	struct FCurl { int32 Bone; FVector Axis; float Weight; };
	TArray<FCurl> Curl[2];
	FVector EyesAt = FVector::ZeroVector;

	void Rebuild();
	void SetCS(int32 Bone, const FTransform& T);
	void Limb(int32 A, int32 B, int32 C, const FVector& Target, const FVector& Bend, const FVector& RefBend);
};
