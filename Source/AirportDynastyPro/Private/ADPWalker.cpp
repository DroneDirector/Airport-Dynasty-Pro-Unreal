// The player walking through the terminal in first person: walk, run, jump and crouch.
#include "ADPWalker.h"
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
	Eyes->SetFieldOfView(80.f);

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
