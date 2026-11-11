// The player walking through the terminal in first person: walk, run, jump and crouch.
#include "ADPWalker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#pragma region Setup
namespace { constexpr float WalkSpeed = 150.f, RunSpeed = 450.f; } // cm/s

AADPWalker::AADPWalker()
{
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	Eyes = CreateDefaultSubobject<UCameraComponent>(TEXT("Eyes"));
	Eyes->SetupAttachment(GetCapsuleComponent());
	Eyes->SetRelativeLocation(FVector(0, 0, 70.f)); // eye height about 1.6 m
	Eyes->bUsePawnControlRotation = true;
	Eyes->SetFieldOfView(90.f);
	PrimaryActorTick.bCanEverTick = true;

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
	Input->BindAxis(TEXT("Turn"), this, &APawn::AddControllerYawInput);
	Input->BindAxis(TEXT("LookUp"), this, &APawn::AddControllerPitchInput);
	Input->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
	Input->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
	Input->BindAction(TEXT("Run"), IE_Pressed, this, &AADPWalker::RunOn);
	Input->BindAction(TEXT("Run"), IE_Released, this, &AADPWalker::RunOff);
	Input->BindAction(TEXT("Crouch"), IE_Pressed, this, &AADPWalker::ToggleCrouch);
}

void AADPWalker::MoveForward(float V) { if (V != 0.f) AddMovementInput(GetActorForwardVector(), V); }
void AADPWalker::MoveRight(float V) { if (V != 0.f) AddMovementInput(GetActorRightVector(), V); }
void AADPWalker::RunOn() { GetCharacterMovement()->MaxWalkSpeed = RunSpeed; }
void AADPWalker::RunOff() { GetCharacterMovement()->MaxWalkSpeed = WalkSpeed; }
void AADPWalker::ToggleCrouch() { if (bIsCrouched) UnCrouch(); else Crouch(); }

#pragma endregion

float AADPWalker::Fov = 90.f;

void AADPWalker::Tick(float Dt)
{
	Super::Tick(Dt);
	// the angle up and down held to what Fov gives on a 16:9 screen; the side angle opened up for wider screens (Unreal
	// keeps the side angle otherwise, so a 32:9 screen saw only a thin strip up and down: everything felt too close)
	static bool bRead = false;
	if (!bRead) { bRead = true; GConfig->GetFloat(TEXT("ADP.Graphics"), TEXT("Fov"), Fov, GGameUserSettingsIni); Fov = FMath::Clamp(Fov, 60.f, 120.f); }
	FVector2D Size(16, 9);
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Size);
	const float Aspect = Size.Y > 1 ? FMath::Max(Size.X / Size.Y, 1.f) : 16.f / 9.f;
	const float V = 2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Fov) * 0.5f) / (16.f / 9.f));
	const float H = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(V * 0.5f) * Aspect));
	if (Eyes && FMath::Abs(Eyes->FieldOfView - H) > 0.05f) Eyes->SetFieldOfView(FMath::Min(H, 170.f));
}
