// Your MetaHuman's own body posed by hand (DriverPoseAnim.h).
#include "DriverPoseAnim.h"
#include "Animation/AnimNodeBase.h"

void FDriverPoseProxy::PreUpdate(UAnimInstance* In, float Dt)
{
	FAnimInstanceProxy::PreUpdate(In, Dt);
	if (const UDriverPoseAnim* A = Cast<UDriverPoseAnim>(In)) Local = A->Local;
}

bool FDriverPoseProxy::Evaluate(FPoseContext& Output)
{
	const FBoneContainer& BC = Output.Pose.GetBoneContainer();
	for (FCompactPoseBoneIndex I : Output.Pose.ForEachBoneIndex())
	{
		const int32 M = BC.MakeMeshPoseIndex(I).GetInt();
		Output.Pose[I] = Local.IsValidIndex(M) ? Local[M] : BC.GetRefPoseTransform(I);
	}
	return true;
}
