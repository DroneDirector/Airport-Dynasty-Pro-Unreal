// While you talk to someone, their head and eyes meet yours (to-do 298, owner): the one you talk to looks at the real camera
// (the talk camera once it is in, else your own eyes). The head: a layer on whatever pose they have (walking blend space or
// hand-made, standing, seated, working), shared over the upper spine, the neck and the head, within natural limits, eased.
// With the arms in use only the neck and head turn. The eyes: the MetaHuman face's own controls (eyeLook..., eyeBlink...,
// which its rig logic turns into eyes and lids), each eye by itself, quicker than the head, with blinks; the face has an
// animation of its own only while this runs. Nobody talked to: one pointer compared per animated body, nothing else.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "TalkLook.generated.h"

class AActor;
class USkeletalMeshComponent;

/** The head's share, one in each body's animation proxy. */
struct FTalkLookLayer
{
	/** Game thread, each update. bArms: the arms are in use, the spine stays as it is. */
	void Update(const USkeletalMeshComponent* Body, float Dt, bool bArms);
	/** On the pose as it is by now (worker thread). */
	void Apply(FCompactPose& Pose) { if (bWant || W > 0.f) Turn(Pose); }
	/** Someone looks at you: no idle movement that looks elsewhere. */
	bool IsOn() const { return bWant; }
private:
	bool bWant = false, bBase = false, bArmsBusy = false;
	float W = 0.f, Step = 0.f, Delay = 0.f, BaseY = 0.f, BaseP = 0.f, Yaw = 0.f, Pitch = 0.f, SpineW = 0.f;
	FVector At = FVector::ZeroVector;   // (component space)
	const void* RigFor = nullptr;
	int32 Bone[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};   // the upper spine, the neck's two, the head
	FVector BL = FVector(1, 0, 0), BF = FVector(0, 1, 0), BU = FVector(0, 0, 1), HeadF = FVector::ZeroVector, EyeAt = FVector::ZeroVector;
	void Rig(const FReferenceSkeleton& RS);
	void Turn(FCompactPose& Pose);
};

USTRUCT()
struct FTalkFaceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()
	FTalkFaceProxy() {}
	FTalkFaceProxy(UAnimInstance* In) : FAnimInstanceProxy(In) {}
	virtual void PreUpdate(UAnimInstance* In, float Dt) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	FVector2f Eye[2] = {FVector2f::ZeroVector, FVector2f::ZeroVector}; float Blink = 0.f;
};

/** The face's animation while its owner looks at you: no movement but the eyes' controls (the face's own post process does the rest). */
UCLASS(Transient, NotBlueprintable)
class AIRPORTDYNASTYPRO_API UTalkFaceAnim : public UAnimInstance
{
	GENERATED_BODY()
public:
	/** Each eye (left, right): degrees to their left and up, from straight ahead. */
	FVector2f Eye[2] = {FVector2f::ZeroVector, FVector2f::ZeroVector};
	/** The lids, 0 as they are .. 1 shut. */
	float Blink = 0.f;
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FTalkFaceProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* P) override { delete P; }
};

namespace ADPTalkLook
{
	/** The talk begins: this person looks at you until End. */
	void Begin(AActor* Who, USkeletalMeshComponent* Body);
	/** The talk is over, for whatever reason: eased back, then off. */
	void End();
	/** Off at once (the world goes). */
	void Drop();
	/** Each frame (AADPPeople::Tick): where you are, and the eyes. */
	void Tick(UWorld* W, float Dt);
	/** Is this the body that looks at you, and where (world). */
	bool For(const USkeletalMeshComponent* Body, FVector& OutAt);
}
