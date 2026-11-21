// The passengers' own animation instance (owner 2026-11-21: a carried suitcase held by its handle, the fingers visibly
// wrapped round it, the hand fixed there all the way). Still the single-node blend space (idle-walk-run) they always
// walked with - SetBlendSpacePosition / SetPlayRate / SetPlaying keep working - with one layer on top while a bag is
// carried in the right hand: the right fingers and thumb closed round a ~3 cm handle bar, and the right arm held a
// little out from the body so the case clears the thigh. Blended in and out over a quarter of a second.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "PaxAnimInstance.generated.h"

class USkeletalMeshComponent;

/** The proxy: the blend space evaluated as always, then the grip layer (worker thread). */
struct FPaxAnimProxy : public FAnimSingleNodeInstanceProxy
{
	FPaxAnimProxy() {}
	FPaxAnimProxy(UAnimInstance* In) : FAnimSingleNodeInstanceProxy(In) {}
	virtual void PreUpdate(UAnimInstance* In, float Dt) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	float Grip = 0.f;            // 0 open hand, hanging arm .. 1 closed round the handle, arm out
	float ArmOut = 10.f;         // degrees the upper arm is held out from the body at full grip
private:
	struct FCurl { int32 Bone; FVector Axis; float Angle; };
	const void* RigFor = nullptr;   // the skeletal mesh asset the bones below were found on
	TArray<FCurl> Curl;
	int32 Upper = INDEX_NONE, Lower = INDEX_NONE;   // mesh bone indices
	FVector Out = FVector::ZeroVector;              // the body's right side, component space
	void Rig(const FBoneContainer& BC);
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

	/** Swap a body that walks with the plain single-node instance onto this one, keeping its blend space playing. */
	static UPaxAnimInstance* Install(USkeletalMeshComponent* Body);
	/** The grip point in hand_r's own space: where the closed fingers wrap round a handle bar (between the palm and the
	 *  curled fingers, at the middle finger's first joint). Zero when the skeleton has no such bones. */
	static FVector GripInHand(const USkeletalMeshComponent* Body);
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FPaxAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* P) override { delete P; }
};
