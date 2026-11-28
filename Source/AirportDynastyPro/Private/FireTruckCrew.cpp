// A fire truck's own crew turning out (2026-11-28): a ground-crew person standing at the driver's door climbs in by the
// truck's own climb (the same keys as yours, on that person's own body: AFireTruck::Sequence), then the truck drives the
// route it was given by itself (FollowRoute). No player, no second system: the views test's "fire out" and, later, a call.
#include "FireTruck.h"
#include "BabylonModel.h"
#include "DriverPoseAnim.h"
#include "ADPLog.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

FVector AFireTruck::CrewSpot() const
{
	// on the ground at the foot of the driver's steps (the climb's first key), at the truck's own floor
	FVector At = DoorSpot();
	At.Z = Model ? Model->GetModelRoot()->GetComponentLocation().Z : GetActorLocation().Z;
	return At;
}

bool AFireTruck::CrewIn(AActor* Person, USkeletalMeshComponent* PersonBody, FString& Why)
{
	if (!Person || !PersonBody || !Model || !Rider) { Why = TEXT("no person, body or truck model"); return false; }
	if (Driver || bCrew || Seq != ESeq::None) { Why = TEXT("someone is already in or climbing"); return false; }
	USkeletalMesh* Own = PersonBody->GetSkeletalMeshAsset();
	if (!Own) { Why = TEXT("the person has no body mesh"); return false; }
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	HumanPose = nullptr;
	Rider->SetSkinnedAssetAndUpdate(Own);
	if (!Rig.Init(Rider)) { Why = FString::Printf(TEXT("the body %s could not be rigged"), *Own->GetName()); return false; }
	// the climb starts where the person stands, facing the way they face (a person's actor faces its yaw + 90)
	CamFrom = FTransform(FRotator(-8.f, Person->GetActorRotation().Yaw + 90.f, 0.f), Person->GetActorLocation() + FVector(0, 0, 160.f)).GetRelativeTransform(RX);
	StartPelvis = RX.InverseTransformPosition(Person->GetActorLocation());
	StartPelvis.Z = Rig.PelvisHeight();
	StartFacing = RX.InverseTransformVector(FRotator(0.f, Person->GetActorRotation().Yaw + 90.f, 0.f).Vector()); StartFacing.Z = 0.f; StartFacing.Normalize();
	// the person's own body lent to the truck's rig: the whole person laid on the rig's frame and carried by it (as
	// AADPWalker::LendBody), posed each frame from the rig; never in anyone's way meanwhile
	Person->SetActorEnableCollision(false);
	const FTransform BodyInActor = PersonBody->GetComponentTransform().GetRelativeTransform(Person->GetActorTransform());
	Person->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Person->SetActorTransform(BodyInActor.Inverse() * Rider->GetComponentTransform());
	Person->AttachToComponent(Rider, FAttachmentTransformRules::KeepWorldTransform);
	PersonBody->SetAnimInstanceClass(UDriverPoseAnim::StaticClass());
	HumanPose = Cast<UDriverPoseAnim>(PersonBody->GetAnimInstance());
	if (!HumanPose)
	{
		Why = TEXT("the person's body would not take the driver's pose");
		Person->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Person->SetActorEnableCollision(true);
		return false;
	}
	Rider->SetVisibility(false);
	Rider->UnHideBoneByName(TEXT("head")); bHeadHidden = false;
	CrewBy = Person; bCrew = true;
	SetPhysics(true);
	Still = 0.f; bHandbrake = true;
	Seq = ESeq::In; SeqT = 0.f;
	Sequence(0.f);
	ADP_LOG(TEXT("Fire truck: its crew climbs in (%s, %s)"), *GetName(), *Own->GetName());
	return true;
}
