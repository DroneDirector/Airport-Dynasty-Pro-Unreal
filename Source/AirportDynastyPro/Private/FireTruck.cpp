// The airport's crash tenders, drivable (FireTruck.h); the fire station's bay doors.
#include "FireTruck.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "FireStation.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

TArray<TWeakObjectPtr<AFireTruck>> AFireTruck::All;

namespace
{
	// the truck's own measures (Art/FireTruck/make_panther.py, Blender metres) and its physics
	constexpr float Axle[3] = {2.85f, -2.55f, -4.2f};
	constexpr float TrackHalf = 1.22f, Rw = 0.75f, Kingpin = 0.12f;
	constexpr float Mass = 28000.f, G = 9.81f;
	constexpr float KSpring = 458000.f, CDamp = 42000.f, AntiRoll = 180000.f;   // N/m, N s/m
	constexpr float BumpCm = 15.f, DroopCm = 10.f, StrokeCm = BumpCm + DroopCm, RwCm = Rw * 100.f;
	constexpr float Mu = 0.9f, Power = 520000.f, TopSpeed = 32.f, ReverseTop = 4.2f;
	constexpr float MaxSteer = 25.f, Wheelbase = 5.4f, RearOff = 1.65f, Track = 2.44f;
	FVector BlenderToRoot(float X, float Y, float Z) { return FVector(X, -Y, Z) * 100.f; }
	FTransform About(const FVector& P, const FQuat& Q) { return FTransform(-P) * FTransform(Q) * FTransform(P); }
}

AFireTruck::AFireTruck()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetBoxExtent(FVector(605.f, 150.f, 140.f));
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetSimulatePhysics(false);
	Body->SetLinearDamping(0.02f);
	Body->SetAngularDamping(0.6f);
	Body->BodyInstance.SetMassOverride(Mass, true);
	Body->BodyInstance.COMNudge = FVector(0, 0, -80.f);   // the weight low (the tank, the chassis)
	Body->SetHiddenInGame(true);
	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(Body);
	Boom->SetRelativeLocation(FVector(0, 0, 130.f));
	Boom->SetRelativeRotation(FRotator(-12.f, 0, 0));
	Boom->TargetArmLength = 1700.f;
	Boom->bDoCollisionTest = false;   // kept in front of walls by Tick's own trace instead (the buildings' meshes collide by their triangles)
	Boom->SetTickGroup(TG_PostPhysics);   // follows the body after physics has moved it (no lag jitter)
	Boom->bUsePawnControlRotation = false;
	Boom->bEnableCameraLag = true; Boom->CameraLagSpeed = 5.f;
	Boom->bEnableCameraRotationLag = true; Boom->CameraRotationLagSpeed = 6.f;
	ChaseCam = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCam"));
	ChaseCam->SetupAttachment(Boom);
	ChaseCam->SetFieldOfView(80.f);
	DriverCam = CreateDefaultSubobject<UCameraComponent>(TEXT("DriverCam"));
	DriverCam->SetupAttachment(Body);
	DriverCam->SetFieldOfView(90.f);
	ChaseCam->SetAutoActivate(false);
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

bool AFireTruck::Setup(const FString& ModelName)
{
	UWorld* W = GetWorld();
	if (!W) return false;
	FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; S.Owner = this;
	Model = W->SpawnActor<ABabylonModel>(GetActorLocation(), GetActorRotation(), S);
	if (!Model || !Model->Place(ModelName)) { if (Model) Model->Destroy(); Model = nullptr; return false; }
	Model->SetPartsCollision(false);   // the body collides for it
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	// the body round the model: forward from the rear axle to the front, its middle at the truck's middle
	const FVector Front = RX.TransformPosition(BlenderToRoot(0, Axle[0], Rw)), Back = RX.TransformPosition(BlenderToRoot(0, Axle[2], Rw));
	const FVector Fw = (Front - Back).GetSafeNormal2D();
	SetActorLocationAndRotation(RX.TransformPosition(BlenderToRoot(0.f, 0.025f, 2.0f)), FRotationMatrix::MakeFromXZ(Fw, FVector::UpVector).Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
	Model->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform);
	const FTransform BT = Body->GetComponentTransform();
	// which way the turns go in the model's root space (its forward is root -y, its right root +x)
	SpinSign = FQuat(FVector::ForwardVector, 0.1f).RotateVector(FVector::UpVector).Y < 0.f ? 1.f : -1.f;
	SteerSign = FQuat(FVector::UpVector, 0.1f).RotateVector(FVector(0, -1, 0)).X > 0.f ? 1.f : -1.f;
	DoorSign = FQuat(FVector::UpVector, 0.1f).RotateVector(FVector(0, 1, 0)).X < 0.f ? 1.f : -1.f;
	int32 Bound = 0;
	for (int32 A = 0; A < 3; ++A)
		for (const float Sd : {-1.f, 1.f})
		{
			FWheel Wh; Wh.Axle = A; Wh.Side = Sd;
			const FString T = FString::Printf(TEXT("%s%d"), Sd < 0 ? TEXT("L") : TEXT("R"), A + 1);
			Wh.CentreRoot = BlenderToRoot(Sd * TrackHalf, Axle[A], Rw);
			Wh.Rest = BT.InverseTransformPosition(RX.TransformPosition(Wh.CentreRoot));
			auto Bind = [&](const TArray<FString>& Names, FTransform& Base, int32& Group)
			{
				if (Model->NodeBase(Names[0], Base)) { Group = Model->BindGroup(Names, false, Base); Bound += Group != INDEX_NONE; }
			};
			Bind({TEXT("Wheel_") + T}, Wh.BWheel, Wh.GWheel);
			Bind({TEXT("LowerArm_") + T}, Wh.BLower, Wh.GLower);
			Bind({TEXT("UpperArm_") + T}, Wh.BUpper, Wh.GUpper);
			Bind({TEXT("Spring_") + T}, Wh.BSpring, Wh.GSpring);
			Bind({TEXT("DamperBody_") + T, TEXT("SpringSeat_") + T}, Wh.BDamper, Wh.GDamper);
			Bind({TEXT("DamperRod_") + T}, Wh.BRod, Wh.GRod);
			Bind({TEXT("DriveShaft_") + T, TEXT("InnerJoint_") + T}, Wh.BShaft, Wh.GShaft);
			// what rides on the wheel's upright (steering with it)
			Wh.GFollow = Model->BindGroup({TEXT("Knuckle_") + T, TEXT("BrakeDisc_") + T, TEXT("Caliper_") + T, TEXT("BrakeHose_") + T,
				TEXT("WheelSensor_") + T, TEXT("SteerArm_") + T, TEXT("TieRodEnd_") + T}, false, FTransform(Wh.CentreRoot));
			if (A == 0 && Sd < 0) ADP_LOG(TEXT("Fire truck: wheel L1 node at %s, expected %s (root space)"), *Wh.BWheel.GetLocation().ToString(), *Wh.CentreRoot.ToString());
			Wheels.Add(Wh);
		}
	// the cab doors (hinged at their front edge), the steering wheel, the pedals
	HingeRoot[0] = BlenderToRoot(-1.53f, 5.0f, 0.f); HingeRoot[1] = BlenderToRoot(1.53f, 5.0f, 0.f);
	GDoor[0] = Model->BindGroup({TEXT("DoorL"), TEXT("Glass_DoorL"), TEXT("DoorHandleL"), TEXT("DoorStripeL")}, false, FTransform(HingeRoot[0]));
	GDoor[1] = Model->BindGroup({TEXT("DoorR"), TEXT("Glass_DoorR"), TEXT("DoorHandleR"), TEXT("DoorStripeR")}, false, FTransform(HingeRoot[1]));
	if (Model->NodeBase(TEXT("SteeringWheel"), BSteeringWheel)) GSteeringWheel = Model->BindGroup({TEXT("SteeringWheel")}, false, BSteeringWheel);
	if (Model->NodeBase(TEXT("Pedal_Accel"), BAccel)) GAccel = Model->BindGroup({TEXT("Pedal_Accel")}, false, BAccel);
	FTransform Pin;
	if (Model->NodeBase(TEXT("Pedal_BrakePin"), Pin)) { BrakePin = Pin.GetLocation(); GBrake = Model->BindGroup({TEXT("Pedal_Brake"), TEXT("Pedal_BrakeBracket"), TEXT("Pedal_BrakeArm")}, false, FTransform(BrakePin)); }
	// the driver's eyes: in the middle seat, looking out through the windscreen
	DriverCam->SetWorldLocation(RX.TransformPosition(BlenderToRoot(0.f, 3.95f, 2.33f)));
	DriverCam->SetRelativeRotation(FRotator::ZeroRotator);
	All.Add(this);
	ADP_LOG(TEXT("Fire truck %s ready: %d moving parts bound, doors %d/%d, steering wheel %d, pedals %d/%d"), *ModelName, Bound, GDoor[0], GDoor[1], GSteeringWheel, GAccel, GBrake);
	return true;
}

void AFireTruck::EndPlay(const EEndPlayReason::Type Why)
{
	All.RemoveAll([this](const TWeakObjectPtr<AFireTruck>& P) { return !P.IsValid() || P.Get() == this; });
	Super::EndPlay(Why);
}

AFireTruck* AFireTruck::FromActor(AActor* A)
{
	if (!A) return nullptr;
	if (AFireTruck* T = Cast<AFireTruck>(A)) return T;
	if (AFireTruck* T = Cast<AFireTruck>(A->GetOwner())) return T;
	return Cast<AFireTruck>(A->GetAttachParentActor());
}

float AFireTruck::SpeedKmh() const { return bSim ? Body->GetPhysicsLinearVelocity().Size() * 0.036f : 0.f; }

void AFireTruck::SetPhysics(bool bOn)
{
	if (bOn == bSim) return;
	bSim = bOn;
	Body->SetSimulatePhysics(bOn);
	if (bOn) Body->WakeRigidBody();
}

void AFireTruck::SetView(bool bNewChase)
{
	bChase = bNewChase;
	DriverCam->SetActive(!bChase);
	ChaseCam->SetActive(bChase);
	if (APlayerController* PC = Cast<APlayerController>(GetController())) if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();   // no smear across the switch
	LookYaw = LookPitch = 0.f;
}

void AFireTruck::SetupPlayerInputComponent(UInputComponent* In)
{
	Super::SetupPlayerInputComponent(In);
	In->BindAxis(TEXT("MoveForward"), this, &AFireTruck::Fwd);
	In->BindAxis(TEXT("MoveRight"), this, &AFireTruck::Right);
	In->BindAxis(TEXT("Turn"), this, &AFireTruck::Turn);
	In->BindAxis(TEXT("LookUp"), this, &AFireTruck::LookUp);
	In->BindAxis(TEXT("TruckThrottle"), this, &AFireTruck::PadT);
	In->BindAxis(TEXT("TruckBrake"), this, &AFireTruck::PadB);
	In->BindAction(TEXT("Jump"), IE_Pressed, this, &AFireTruck::HandOn);
	In->BindAction(TEXT("Jump"), IE_Released, this, &AFireTruck::HandOff);
	In->BindAction(TEXT("ChangeView"), IE_Pressed, this, &AFireTruck::ToggleView);
}

void AFireTruck::Enter(APawn* Walker)
{
	if (Driver || !Walker || !Model) return;
	APlayerController* PC = Cast<APlayerController>(Walker->GetController());
	if (!PC) return;
	Driver = Walker;
	Walker->SetActorHiddenInGame(true);
	Walker->SetActorEnableCollision(false);
	if (ACharacter* Ch = Cast<ACharacter>(Walker)) Ch->GetCharacterMovement()->DisableMovement();
	Walker->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform);
	PC->Possess(this);
	SetView(false);
	SetPhysics(true);
	DoorClock = 0.f; Still = 0.f; bTestInput = false;
	ADP_LOG(TEXT("Fire truck: you got in (%s)"), *GetName());
}

void AFireTruck::Exit()
{
	if (!Driver || !Model) return;
	APlayerController* PC = Cast<APlayerController>(GetController());
	APawn* Wk = Driver;
	Driver = nullptr;
	Wk->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// out on the ground beside the driver's door (left), turned half towards the truck so you see it
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	FVector At = RX.TransformPosition(BlenderToRoot(-3.1f, 4.35f, 1.0f));
	const FVector ToTruck = (GetActorLocation() - At).GetSafeNormal2D();
	FRotator Rot(-10.f, ToTruck.IsNearlyZero() ? GetActorRotation().Yaw : ToTruck.Rotation().Yaw, 0.f);
	GetWorld()->FindTeleportSpot(Wk, At, Rot);
	Wk->TeleportTo(At, FRotator(0.f, Rot.Yaw, 0.f));
	Wk->SetActorEnableCollision(true);
	Wk->SetActorHiddenInGame(false);
	if (ACharacter* Ch = Cast<ACharacter>(Wk)) Ch->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (PC) { PC->Possess(Wk); PC->SetControlRotation(Rot); }
	InThrottle = InBrake = InSteer = KeyFwd = KeySteer = PadThrottle = PadBrake = 0.f;
	bHandbrake = true;   // left with the handbrake on
	DoorClock = 0.f;
	ADP_LOG(TEXT("Fire truck: you got out (%s), %.0f km/h"), *GetName(), SpeedKmh());
}

void AFireTruck::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Model || Dt <= 0.f) return;
	if (Driver && !bTestInput)
	{
		InThrottle = FMath::Max(FMath::Max(KeyFwd, 0.f), PadThrottle);
		InBrake = FMath::Max(FMath::Max(-KeyFwd, 0.f), PadBrake);
		InSteer = FMath::Clamp(KeySteer, -1.f, 1.f);
	}
	if (bSim) Simulate(FMath::Min(Dt, 1.f / 20.f));
	if (bSim || DoorClock >= 0.f) Animate(Dt);
	DriverCam->SetRelativeRotation(FRotator(LookPitch - 11.f, LookYaw, 0.f));   // a driver's eyes rest a little down the road, the wheel's rim in view
	Boom->SetRelativeRotation(FRotator(-12.f + LookPitch * 0.5f, LookYaw, 0.f));
	if (bChase)
	{
		// the chase camera stays on this side of walls and ceilings (in the bay it would sit in the back wall)
		const FVector From = Boom->GetComponentLocation(), To = From - Boom->GetForwardVector() * 1750.f;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPTruckCam), true, this);
		Q.AddIgnoredActor(Model);
		if (Driver) Q.AddIgnoredActor(Driver);
		FHitResult H;
		const float Want = GetWorld()->LineTraceSingleByChannel(H, From, To, ECC_Visibility, Q) ? FMath::Max(250.f, H.Distance - 60.f) : 1700.f;
		// in at once (never through the wall), back out gently
		Boom->TargetArmLength = Want < Boom->TargetArmLength ? Want : FMath::FInterpTo(Boom->TargetArmLength, Want, Dt, 2.f);
	}
	// once you've got out and it stands still a while, it rests (no physics running; it stays exactly where it stopped)
	if (bSim && !Driver) { Still = SpeedKmh() < 0.5f ? Still + Dt : 0.f; if (Still > 3.f) SetPhysics(false); }
}

float AFireTruck::TurnRate() const { return Body ? Body->GetPhysicsAngularVelocityInDegrees().Size() : 0.f; }

void AFireTruck::Simulate(float Dt)
{
	const FTransform BT = Body->GetComponentTransform();
	const FVector Up = BT.GetUnitAxis(EAxis::Z), F = BT.GetUnitAxis(EAxis::X), R = BT.GetUnitAxis(EAxis::Y);
	const FVector V = Body->GetPhysicsLinearVelocity() / 100.f;
	const float Vf = FVector::DotProduct(V, F), Speed = FMath::Abs(Vf);
	// the steering: towards the wheel's target at a steady rate, less lock the faster it goes
	const float Lock = MaxSteer / (1.f + Speed / 12.f);
	Steer = FMath::FInterpConstantTo(Steer, InSteer * Lock, Dt, 45.f);
	// forward on the throttle; reverse once stopped with the brake held; the other pedal brakes
	StopHold = (InBrake > 0.1f && InThrottle < 0.1f && Vf < 0.6f) ? StopHold + Dt : 0.f;
	if (StopHold > 0.9f) bReverse = true;
	if (InThrottle > 0.1f && Vf > -0.6f) bReverse = false;
	const float Drive = bReverse ? -InBrake : InThrottle, BrakeIn = bReverse ? InThrottle : InBrake;
	float DriveForce = 0.f;
	if (Drive > 0.f) DriveForce = Drive * FMath::Min(Power / FMath::Max(Speed, 2.f), 0.7f * Mass * G) * FMath::Clamp((TopSpeed - Vf) / 3.f, 0.f, 1.f);
	else if (Drive < 0.f) DriveForce = Drive * 0.45f * Mass * G * FMath::Clamp((ReverseTop + Vf) / 1.5f, 0.f, 1.f);
	const float BrakeForce = BrakeIn * 0.75f * Mass * G;

	// the suspension: a ray down from each wheel's top of travel
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPTruckWheel), false, this);
	Q.AddIgnoredActor(Model);
	if (Driver) Q.AddIgnoredActor(Driver);
	TArray<FVector> Mount, Contact; Mount.SetNum(Wheels.Num()); Contact.SetNum(Wheels.Num());
	for (int32 I = 0; I < Wheels.Num(); ++I)
	{
		FWheel& Wh = Wheels[I];
		Mount[I] = BT.TransformPosition(Wh.Rest + FVector(0, 0, BumpCm));
		FHitResult H;
		float S = StrokeCm;
		Wh.bContact = GetWorld()->LineTraceSingleByChannel(H, Mount[I], Mount[I] - Up * (StrokeCm + RwCm + 5.f), ECC_Visibility, Q);
		if (Wh.bContact) { S = FMath::Clamp(H.Distance - RwCm, 0.f, StrokeCm); Contact[I] = H.ImpactPoint; Wh.bContact = H.Distance - RwCm <= StrokeCm; }
		const float X = StrokeCm - S;                       // compression, cm
		const float Vx = (X - Wh.X) / Dt; Wh.X = X;
		Wh.Travel = BumpCm - S;                             // up from rest, cm
		float Fs = KSpring * X / 100.f + CDamp * Vx / 100.f;
		if (S < 2.f) Fs += KSpring * 4.f * (2.f - S) / 100.f;   // the bump stop
		Wh.Load = Wh.bContact ? FMath::Max(Fs, 0.f) : 0.f;
	}
	// anti-roll: across each axle, the more compressed side carries more
	for (int32 A = 0; A < 3; ++A)
	{
		FWheel& L = Wheels[A * 2]; FWheel& Rt = Wheels[A * 2 + 1];
		const float Far = AntiRoll * (L.X - Rt.X) / 100.f;
		if (L.bContact) L.Load = FMath::Max(L.Load + Far, 0.f);
		if (Rt.bContact) Rt.Load = FMath::Max(Rt.Load - Far, 0.f);
	}
	float SumLoad = 0.f;
	for (int32 I = 0; I < Wheels.Num(); ++I) if (Wheels[I].Load > 0.f) { SumLoad += Wheels[I].Load; Body->AddForceAtLocation(Up * Wheels[I].Load * 100.f, Mount[I]); }

	// the tyres
	for (int32 I = 0; I < Wheels.Num(); ++I)
	{
		FWheel& Wh = Wheels[I];
		// Ackermann: each wheel turned to the same turning centre (on the middle axle's line); the rear axle the other way
		float Delta = 0.f;
		if (FMath::Abs(Steer) > 0.05f && Wh.Axle != 1)
		{
			const float Rr = Wheelbase / FMath::Tan(FMath::DegreesToRadians(FMath::Abs(Steer)));
			const float Inside = FMath::Sign(Steer) * Wh.Side;   // +1 on the inside of the turn
			const float Rw2 = FMath::Max(1.f, Rr - Inside * Track * 0.5f);
			Delta = FMath::Sign(Steer) * FMath::RadiansToDegrees(FMath::Atan((Wh.Axle == 0 ? Wheelbase : RearOff) / Rw2)) * (Wh.Axle == 0 ? 1.f : -1.f);
		}
		Wh.Steer = Delta;
		const float Sn = FMath::Sin(FMath::DegreesToRadians(Delta)), Cs = FMath::Cos(FMath::DegreesToRadians(Delta));
		const FVector WF = F * Cs + R * Sn, WR = R * Cs - F * Sn;
		const FVector At = Wh.bContact ? Contact[I] : BT.TransformPosition(Wh.Rest);
		const FVector Vp = Body->GetPhysicsLinearVelocityAtPoint(At) / 100.f;
		const float VF = FVector::DotProduct(Vp, WF), VL = FVector::DotProduct(Vp, WR);
		Wh.Spin += SpinSign * VF / Rw * Dt;
		if (!Wh.bContact || Wh.Load <= 0.f) continue;
		const float Fz = Wh.Load, Share = SumLoad > 0.f ? Fz / SumLoad : 0.f;
		// the most a resisting force may do in one step is stop this wheel's share of the truck: any more and it pushes
		// back the other way the next frame, over and over (the truck shook standing and at low speed)
		const float Stop = Mass * Share / Dt;
		float Resist = BrakeForce * Share * FMath::Tanh(VF / 0.5f);
		Resist += 0.012f * Fz * FMath::Tanh(VF / 0.3f);                                 // rolling
		if (bHandbrake && Wh.Axle > 0) Resist += Mu * Fz * FMath::Tanh(VF / 0.3f);       // the handbrake on the rear axles
		if (FMath::IsNearlyZero(Drive) && Speed < 0.8f) Resist += 0.6f * Mu * Fz * FMath::Tanh(VF / 0.15f);   // standing still holds
		Resist = FMath::Clamp(Resist, -Stop * FMath::Abs(VF), Stop * FMath::Abs(VF));
		float Fl = DriveForce * Share - Resist;
		const float Slip = FMath::Atan2(VL, FMath::Max(FMath::Abs(VF), 1.f));
		float Fy = -Mu * Fz * FMath::Tanh(Slip * 6.f);
		Fy = FMath::Clamp(Fy, -Stop * FMath::Abs(VL), Stop * FMath::Abs(VL));
		const float Mag = FMath::Sqrt(Fl * Fl + Fy * Fy), Lim = Mu * Fz;
		if (Mag > Lim) { Fl *= Lim / Mag; Fy *= Lim / Mag; }
		Body->AddForceAtLocation((WF * Fl + WR * Fy) * 100.f, At);
	}
	// the air
	Body->AddForce(-V.GetSafeNormal() * 0.5f * 1.2f * 0.9f * 8.5f * V.SizeSquared() * 100.f);
}

void AFireTruck::Animate(float Dt)
{
	// the wheels and everything that moves with them (all in the model's root space)
	for (FWheel& Wh : Wheels)
	{
		const FVector C0 = Wh.CentreRoot, Kp = C0 - FVector(Wh.Side * Kingpin * 100.f, 0, 0);
		const FTransform SteerT = About(Kp, FQuat(FVector::UpVector, FMath::DegreesToRadians(SteerSign * Wh.Steer)));
		const FTransform Lift(FVector(0, 0, Wh.Travel));
		if (Wh.GWheel >= 0) Model->MoveGroup(Wh.GWheel, Wh.BWheel * About(C0, FQuat(FVector::ForwardVector, Wh.Spin)) * SteerT * Lift);
		if (Wh.GFollow >= 0) Model->MoveGroup(Wh.GFollow, FTransform(C0) * SteerT * Lift);
		auto Swing = [&](const FVector& P, const FVector& O0) { return FQuat::FindBetweenNormals((O0 - P).GetSafeNormal(), (O0 + FVector(0, 0, Wh.Travel) - P).GetSafeNormal()); };
		const FVector PL = Wh.BLower.GetLocation(), PU = Wh.BUpper.GetLocation();
		const FQuat QL = Swing(PL, C0 + FVector(-Wh.Side * 41.f, 0, -19.f));
		if (Wh.GLower >= 0) Model->MoveGroup(Wh.GLower, Wh.BLower * About(PL, QL));
		if (Wh.GUpper >= 0) Model->MoveGroup(Wh.GUpper, Wh.BUpper * About(PU, Swing(PU, C0 + FVector(-Wh.Side * 41.f, 0, 25.f))));
		// the coil-over: its top fixed, its lower eye riding on the lower arm
		const FVector Top = Wh.BSpring.GetLocation(), E0 = Wh.BDamper.GetLocation(), E1 = PL + QL.RotateVector(E0 - PL);
		const FQuat QS = FQuat::FindBetweenNormals((E0 - Top).GetSafeNormal(), (E1 - Top).GetSafeNormal());
		const float Sc = (E1 - Top).Size() / FMath::Max(1.f, float((E0 - Top).Size()));
		if (Wh.GSpring >= 0) Model->MoveGroup(Wh.GSpring, FTransform(FQuat::Identity, FVector::ZeroVector, FVector(1, 1, Sc)) * Wh.BSpring * About(Top, QS));
		if (Wh.GDamper >= 0) Model->MoveGroup(Wh.GDamper, Wh.BDamper * FTransform(-E0) * FTransform(QS) * FTransform(E1));
		if (Wh.GRod >= 0) Model->MoveGroup(Wh.GRod, Wh.BRod * About(Top, QS));
		const FVector D = Wh.BShaft.GetLocation();
		if (Wh.GShaft >= 0) Model->MoveGroup(Wh.GShaft, Wh.BShaft * About(D, FQuat::FindBetweenNormals((C0 - D).GetSafeNormal(), (C0 + FVector(0, 0, Wh.Travel) - D).GetSafeNormal())));
	}
	// the steering wheel (one and a half turns lock to lock), the pedals
	if (GSteeringWheel >= 0) Model->MoveGroup(GSteeringWheel, FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(-Steer / MaxSteer * 270.f))) * BSteeringWheel);
	if (GAccel >= 0) Model->MoveGroup(GAccel, BAccel * About(BAccel.TransformPosition(FVector(0, 0, -11.f)), FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-InThrottle * 14.f))));
	if (GBrake >= 0) Model->MoveGroup(GBrake, FTransform(BrakePin) * About(BrakePin, FQuat(FVector::ForwardVector, FMath::DegreesToRadians(InBrake * 16.f))));
	// the driver's door as you get in or out: open, a moment, shut again
	if (DoorClock >= 0.f)
	{
		DoorClock += Dt;
		const float K = DoorClock < 0.5f ? DoorClock / 0.5f : DoorClock < 1.1f ? 1.f : FMath::Max(0.f, 1.f - (DoorClock - 1.1f) / 0.6f);
		if (GDoor[0] >= 0) Model->MoveGroup(GDoor[0], FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(DoorSign * 70.f * FMath::SmoothStep(0.f, 1.f, K))), HingeRoot[0]));
		if (DoorClock > 1.7f) DoorClock = -1.f;
	}
	Model->FlushMoves();
}

#pragma region StationDoors
AFireStationDoors::AFireStationDoors()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool AFireStationDoors::bTestOpen = false;

void AFireStationDoors::Setup(ABabylonModel* S)
{
	Station = S;
	if (!S) return;
	const FTransform RX = S->GetModelRoot()->GetComponentTransform();
	int32 Found = 0;
	for (int32 I = 0; I < 5; ++I)
	{
		const float Bx = ADPFireStation::BayX(float(I));
		FBay B;
		B.Pivot = FVector(-Bx * 100.f, 0.f, 520.f);   // the leaf's top edge (the sketch: doors 5.2 m high on the front line)
		// the leaf, its window band, glass and bars: the sketch names them BayDoor<bay>_<n> (docs/fire-station/sketch/station.ts)
		const FString Prefix = FString::Printf(TEXT("BayDoor%d_"), I);
		B.Group = S->BindGroupWhere([&Prefix](const FString& Name, int32, const FVector&) { return Name.StartsWith(Prefix); }, FTransform(B.Pivot));
		B.World = RX.TransformPosition(FVector(-Bx * 100.f, 0.f, 200.f));
		Found += B.Group != INDEX_NONE;
		Bays.Add(B);
	}
	ADP_LOG(TEXT("Fire station: %d bay doors that open"), Found);
}

void AFireStationDoors::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Station) return;
	bool bMoved = false;
	for (FBay& B : Bays)
	{
		if (B.Group < 0) continue;
		// open for a truck being driven (or still moving) within 16 m of it, inside or out
		bool bOpen = bTestOpen;
		for (const TWeakObjectPtr<AFireTruck>& T : AFireTruck::All)
			if (T.IsValid() && (T->IsDriven() || T->SpeedKmh() > 1.f) && FVector::Dist2D(T->GetActorLocation(), B.World) < 1600.f) { bOpen = true; break; }
		const float K = FMath::FInterpConstantTo(B.K, bOpen ? 1.f : 0.f, Dt, 0.3f);
		if (K == B.K) continue;
		B.K = K;
		// up and over: the leaf tilts up about its top edge, in under the ceiling
		Station->MoveGroup(B.Group, FTransform(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(90.f * FMath::SmoothStep(0.f, 1.f, K))), B.Pivot));
		bMoved = true;
	}
	if (bMoved) Station->FlushMoves();
}
#pragma endregion
