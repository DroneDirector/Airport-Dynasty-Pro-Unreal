// Your MetaHuman's own body posed by hand (owner, 2026-11-13: the fire truck's climb, seat and wheel with your MetaHuman,
// not the old mannequin): an animation instance that plays no animation, only the pose it's handed each frame (the
// bones' local transforms in the mesh's own order, from FTruckDriverRig). The body stays a real skeletal mesh, so your
// clothes keep following it and your face keeps riding on its head as when you walk.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "DriverPoseAnim.generated.h"

USTRUCT()
struct FDriverPoseProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()
	FDriverPoseProxy() {}
	FDriverPoseProxy(UAnimInstance* In) : FAnimInstanceProxy(In) {}
	virtual void PreUpdate(UAnimInstance* In, float Dt) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	TArray<FTransform> Local;
};

UCLASS(Transient, NotBlueprintable)
class AIRPORTDYNASTYPRO_API UDriverPoseAnim : public UAnimInstance
{
	GENERATED_BODY()
public:
	/** The pose to show: every bone's local transform, in the skeletal mesh's own bone order. */
	TArray<FTransform> Local;
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FDriverPoseProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* P) override { delete P; }
};
