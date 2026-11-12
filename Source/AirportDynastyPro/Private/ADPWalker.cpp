// The player walking through the terminal in first person: walk, run, jump and crouch.
#include "ADPWalker.h"
#include "Wardrobe.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GroomComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "ADPLog.h"

#pragma region Setup
namespace
{
	constexpr float WalkSpeed = 150.f, RunSpeed = 450.f; // cm/s
	// seen from behind: over the right shoulder, a little above the head
	constexpr float ArmLen = 320.f; const FVector ArmPivot(0, 0, 60.f), ArmOffset(0, 45.f, 15.f);
}

AADPWalker::AADPWalker()
{
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	Eyes = CreateDefaultSubobject<UCameraComponent>(TEXT("Eyes"));
	Eyes->SetupAttachment(GetCapsuleComponent());
	Eyes->SetRelativeLocation(FVector(0, 0, 70.f)); // eye height about 1.6 m
	Eyes->bUsePawnControlRotation = true;
	Eyes->SetFieldOfView(90.f);
	// the view from behind (owner, 2026-11-11: a third-person view for your character)
	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(GetCapsuleComponent());
	Boom->SetRelativeLocation(ArmPivot);
	Boom->TargetArmLength = ArmLen;
	Boom->SocketOffset = ArmOffset;
	Boom->bUsePawnControlRotation = true;
	Boom->ProbeSize = 14.f;
	Boom->bEnableCameraLag = true; Boom->CameraLagSpeed = 14.f;
	Behind = CreateDefaultSubobject<UCameraComponent>(TEXT("Behind"));
	Behind->SetupAttachment(Boom);
	Behind->SetFieldOfView(90.f);
	Behind->SetAutoActivate(false);
	PrimaryActorTick.bCanEverTick = true;

	// your body (owner, 2026-11-11: a body for the avatar, so getting in and out, sitting, holding the hose etc. can be
	// animated): Epic's mannequin with its walk / run / idle / jump animation set (the engine's template content,
	// Content/Characters/Mannequins). Feet on the capsule's floor, facing forward; your own view from the eyes sees
	// your body and its shadow but not the inside of your head (the head is hidden from you only)
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> Anim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	USkeletalMeshComponent* B = GetMesh();
	if (Body.Succeeded()) B->SetSkeletalMesh(Body.Object);
	else ADP_ERROR(TEXT("Walker: the body mesh (SKM_Manny_Simple) is missing"));
	if (Anim.Succeeded()) B->SetAnimInstanceClass(Anim.Class);
	else ADP_ERROR(TEXT("Walker: the body's animation blueprint (ABP_Unarmed) is missing"));
	B->SetRelativeLocation(FVector(0, 0, -88.f));
	B->SetRelativeRotation(FRotator(0, -90.f, 0));
	B->SetCastShadow(true); B->bCastHiddenShadow = true;
	B->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Eyes->SetRelativeLocation(FVector(14.f, 0, 70.f)); // just in front of the face

	UCharacterMovementComponent* M = GetCharacterMovement();
	M->MaxWalkSpeed = WalkSpeed;
	M->MaxWalkSpeedCrouched = 80.f;
	M->JumpZVelocity = 380.f;
	M->NavAgentProps.bCanCrouch = true;
	bUseControllerRotationYaw = true;
}
#pragma endregion

#pragma region Input

void AADPWalker::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	Input->BindAxis(TEXT("MoveForward"), this, &AADPWalker::MoveForward);
	Input->BindAxis(TEXT("MoveRight"), this, &AADPWalker::MoveRight);
	Input->BindAxis(TEXT("Turn"), this, &AADPWalker::TurnBy);
	Input->BindAxis(TEXT("LookUp"), this, &AADPWalker::LookUpBy);
	Input->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
	Input->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
	Input->BindAction(TEXT("Run"), IE_Pressed, this, &AADPWalker::RunOn);
	Input->BindAction(TEXT("Run"), IE_Released, this, &AADPWalker::RunOff);
	Input->BindAction(TEXT("Crouch"), IE_Pressed, this, &AADPWalker::ToggleCrouch);
	Input->BindAction(TEXT("ChangeView"), IE_Pressed, this, &AADPWalker::ToggleView);
}

void AADPWalker::MoveForward(float V) { if (V != 0.f && !bLed) AddMovementInput(GetActorForwardVector(), V); }
void AADPWalker::MoveRight(float V) { if (V != 0.f && !bLed) AddMovementInput(GetActorRightVector(), V); }
void AADPWalker::RunOn() { GetCharacterMovement()->MaxWalkSpeed = RunSpeed; }
void AADPWalker::RunOff() { GetCharacterMovement()->MaxWalkSpeed = WalkSpeed; }
void AADPWalker::ToggleCrouch() { if (bIsCrouched) UnCrouch(); else Crouch(); }

#pragma endregion

float AADPWalker::Fov = 90.f;

void AADPWalker::BeginPlay()
{
	Super::BeginPlay();
	WearMetaHuman(AWardrobe::Chosen());
}

bool AADPWalker::WearMetaHuman(const FString& Who)
{
	UClass* C = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Game/People/Built/%s/BP_%s.BP_%s_C"), *Who, *Who, *Who));
	if (!C) { if (Human) return true; ADP_LOG(TEXT("Walker: no built character %s, the mannequin stays"), *Who); return false; }
	if (Human) { Human->Destroy(); Human = nullptr; HumanBody = nullptr; HumanHead.Empty(); }
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; P.Owner = this;
	Human = GetWorld()->SpawnActor<AActor>(C, GetMesh()->GetComponentLocation(), GetMesh()->GetComponentRotation(), P);
	if (!Human) return false;
	// on the capsule where the mannequin stands (feet on the floor, facing forward), nothing of its own in the way
	Human->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepWorldTransform);
	Human->SetActorEnableCollision(false);
	TArray<USkeletalMeshComponent*> Skels; Human->GetComponents<USkeletalMeshComponent>(Skels);
	USkeletalMeshComponent* Face = nullptr;
	for (USkeletalMeshComponent* S : Skels)
	{
		const FString N = S->GetName();
		if (N.StartsWith(TEXT("Body"))) HumanBody = S;
		else if (N.StartsWith(TEXT("Face"))) Face = S;
	}
	if (!HumanBody) { ADP_WARN(TEXT("Walker: %s has no Body"), *Who); Human->Destroy(); Human = nullptr; return false; }
	// the clothes follow the body's pose; the face rides on the body's head bone
	for (USkeletalMeshComponent* S : Skels)
		if (S != HumanBody && S != Face) S->SetLeaderPoseComponent(HumanBody);
	if (Face)
	{
		HumanHead.Add(Face);
		TArray<UGroomComponent*> Grooms; Human->GetComponents<UGroomComponent>(Grooms);
		for (UGroomComponent* G : Grooms) HumanHead.Add(G);
	}
	// the body plays the carried-over blend space itself (no animation blueprint): idle, walk and run by how you move
	if (!MoveBlend) MoveBlend = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/People/Anims/BS_Idle_Walk_Run_MH.BS_Idle_Walk_Run_MH"));
	if (!FallAnim) FallAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/People/Anims/MM_Fall_Loop_MH.MM_Fall_Loop_MH"));
	if (MoveBlend)
	{
		HumanBody->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		HumanBody->SetAnimation(MoveBlend);
		HumanBody->Play(true);
	}
	else ADP_WARN(TEXT("Walker: the carried-over animations are missing (Tools/retarget_player_anims.py)"));
	HumanBody->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	for (USkeletalMeshComponent* S : Skels) S->SetCastShadow(true);
	// the mannequin out of sight (it keeps animating as the rig the rest of the game reads), your head only from behind
	GetMesh()->SetVisibility(false, false);
	GetMesh()->bCastHiddenShadow = false;
	for (USceneComponent* H : HumanHead) H->SetVisibility(bThird, true);
	ADP_LOG(TEXT("Walker: you are %s"), *Who);
	return true;
}

void AADPWalker::DriveHuman(float Dt)
{
	if (!HumanBody) return;
	UAnimSingleNodeInstance* One = HumanBody->GetSingleNodeInstance();
	if (!One) return;
	const bool bFalling = GetCharacterMovement() && GetCharacterMovement()->IsFalling();
	if (bFalling != bHumanFalling)
	{
		bHumanFalling = bFalling;
		if (bFalling && FallAnim) { HumanBody->SetAnimation(FallAnim); HumanBody->Play(true); }
		else if (MoveBlend) { HumanBody->SetAnimation(MoveBlend); HumanBody->Play(true); }
		One = HumanBody->GetSingleNodeInstance();
	}
	if (!bFalling && One && MoveBlend)
	{
		// the blend space's axes: the way you move relative to where you face (degrees) and how fast (cm/s)
		const FVector V = GetVelocity();
		const float Speed = V.Size2D();
		const float Dir = Speed > 5.f ? FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, V.Rotation().Yaw) : 0.f;
		One->SetBlendSpacePosition(FVector(Dir, Speed, 0.f));
	}
}

void AADPWalker::SetThirdPerson(bool bOn)
{
	bThird = bOn;
	Eyes->SetActive(!bThird);
	Behind->SetActive(bThird);
	// your own head: hidden only while you look out from inside it
	if (GetMesh() && GetMesh()->GetSkeletalMeshAsset())
	{
		if (bThird) GetMesh()->UnHideBoneByName(TEXT("head")); else GetMesh()->HideBoneByName(TEXT("head"), PBO_None);
		bHeadHidden = true;
	}
	for (USceneComponent* H : HumanHead) if (H) H->SetVisibility(bThird, true);
	if (APlayerController* PC = Cast<APlayerController>(GetController())) if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
	GConfig->SetBool(TEXT("ADP.Input"), TEXT("ThirdPerson"), bThird, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

FTransform AADPWalker::ViewFrom(const FVector& At, const FRotator& Look) const
{
	if (!bThird) return FTransform(Look, At + FRotator(0.f, Look.Yaw, 0.f).RotateVector(FVector(14.f, 0.f, 70.f)));
	const FVector Pivot = At + ArmPivot;
	return FTransform(Look, Pivot + Look.RotateVector(FVector(-ArmLen, 0.f, 0.f) + ArmOffset));
}

void AADPWalker::Tick(float Dt)
{
	Super::Tick(Dt);
	DriveHuman(Dt);
	// your own head hidden from your eyes (you look out from inside it); the rest of the body stays seen and casts its shadow
	// the view you last chose, first person or from behind (kept between games)
	if (!bViewRead) { bViewRead = true; bool bT = false; GConfig->GetBool(TEXT("ADP.Input"), TEXT("ThirdPerson"), bT, GGameUserSettingsIni); SetThirdPerson(bT); }
	if (!bHeadHidden && !bThird && GetMesh() && GetMesh()->GetSkeletalMeshAsset()) { GetMesh()->HideBoneByName(TEXT("head"), PBO_None); bHeadHidden = true; }
	// the angle up and down held to what Fov gives on a 16:9 screen; the side angle opened up for wider screens (Unreal
	// keeps the side angle otherwise, so a 32:9 screen saw only a thin strip up and down: everything felt too close)
	static bool bRead = false;
	if (!bRead) { bRead = true; GConfig->GetFloat(TEXT("ADP.Graphics"), TEXT("Fov"), Fov, GGameUserSettingsIni); Fov = FMath::Clamp(Fov, 60.f, 120.f); }
	FVector2D Size(16, 9);
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Size);
	const float Aspect = Size.Y > 1 ? FMath::Max(Size.X / Size.Y, 1.f) : 16.f / 9.f;
	const float V = 2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Fov) * 0.5f) / (16.f / 9.f));
	const float H = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(V * 0.5f) * Aspect));
	if (Eyes && FMath::Abs(Eyes->FieldOfView - H) > 0.05f) { Eyes->SetFieldOfView(FMath::Min(H, 170.f)); Behind->SetFieldOfView(FMath::Min(H, 170.f)); }
}
