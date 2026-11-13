// Drone mode (DroneCam.h).
#include "DroneCam.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"

const float ADroneCam::Steps[ADroneCam::NumSteps] = {0.5f, 1.f, 2.f, 4.f, 8.f, 15.f, 25.f, 40.f, 60.f};

ADroneCam::ADroneCam()
{
	PrimaryActorTick.bCanEverTick = true;
	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Cam"));
	RootComponent = Cam;
	Cam->bUsePawnControlRotation = true;
	Cam->SetFieldOfView(90.f);
	SetActorEnableCollision(false);
	bUseControllerRotationYaw = bUseControllerRotationPitch = false;
}

void ADroneCam::SetupPlayerInputComponent(UInputComponent* In)
{
	Super::SetupPlayerInputComponent(In);
	In->BindAxis(TEXT("MoveForward"), this, &ADroneCam::Fwd);
	In->BindAxis(TEXT("MoveRight"), this, &ADroneCam::Right);
	In->BindAxis(TEXT("Turn"), this, &ADroneCam::Turn);
	In->BindAxis(TEXT("LookUp"), this, &ADroneCam::Look);
	In->BindAxis(TEXT("TruckThrottle"), this, &ADroneCam::UpPad);     // right trigger: up
	In->BindAxis(TEXT("TruckBrake"), this, &ADroneCam::DownPad);      // left trigger: down
	In->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ADroneCam::UpOn);
	In->BindKey(EKeys::SpaceBar, IE_Released, this, &ADroneCam::UpOff);
	In->BindKey(EKeys::LeftControl, IE_Pressed, this, &ADroneCam::DownOn);
	In->BindKey(EKeys::LeftControl, IE_Released, this, &ADroneCam::DownOff);
	In->BindKey(EKeys::LeftShift, IE_Pressed, this, &ADroneCam::BoostOn);
	In->BindKey(EKeys::LeftShift, IE_Released, this, &ADroneCam::BoostOff);
	In->BindKey(EKeys::Gamepad_LeftThumbstick, IE_Pressed, this, &ADroneCam::BoostOn);
	In->BindKey(EKeys::Gamepad_LeftThumbstick, IE_Released, this, &ADroneCam::BoostOff);
	In->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ADroneCam::StepUp);
	In->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ADroneCam::StepDown);
	In->BindKey(EKeys::Gamepad_DPad_Up, IE_Pressed, this, &ADroneCam::StepUp);
	In->BindKey(EKeys::Gamepad_DPad_Down, IE_Pressed, this, &ADroneCam::StepDown);
	In->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &ADroneCam::Leave);
}

void ADroneCam::Tick(float Dt)
{
	Super::Tick(Dt);
	const FRotator View = GetControlRotation();
	const FRotator Level(0.f, View.Yaw, 0.f);
	const float F = bTest ? TF : InF, R = bTest ? TR : InR;
	const float U = bTest ? TU : FMath::Clamp(InUpKey - InDownKey + InUpPad - InDownPad, -1.f, 1.f);
	FVector Want = Level.RotateVector(FVector(F, R, 0.f));
	Want.Z = U;
	if (Want.SizeSquared() > 1.f) Want.Normalize();
	// the speed: the wheel's step, or the boost; reaching it takes about a second at the step, a few seconds boosting
	const float Top = (bBoost ? BoostSpeed : Steps[Step]) * 100.f;
	const float Rate = bBoost ? 1.2f : 4.f;
	Vel = FMath::VInterpTo(Vel, Want * Top, Dt, Rate);
	if (Want.IsNearlyZero() && Vel.Size() < 5.f) Vel = FVector::ZeroVector;
	SetActorLocation(GetActorLocation() + Vel * Dt);
	// how high above the ground (or roof) below you: for the panel
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPDrone), true, this);
	const FVector At = GetActorLocation();
	Above = GetWorld()->LineTraceSingleByChannel(H, At, At - FVector(0, 0, 2000000.f), ECC_Visibility, Q) ? H.Distance / 100.f : -1.f;
}
