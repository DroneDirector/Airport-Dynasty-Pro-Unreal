// The airport's crash tenders, drivable (FireTruck.h); the fire station's bay doors.
#include "FireTruck.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "FireStation.h"
#include "ADPWalker.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
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
	FVector DirToRoot(float X, float Y) { return FVector(X, -Y, 0.f).GetSafeNormal(); }

	// ---- the sounds, made here (no recordings): 44.1 kHz mono
	constexpr int32 Rate = 44100;
	float Noise(uint32& Seed) { Seed = Seed * 1664525u + 1013904223u; return ((Seed >> 9) & 0xFFFF) / 32767.5f - 1.f; }
	TArray<int16> ToPcm(const TArray<float>& F) { TArray<int16> P; P.SetNumUninitialized(F.Num()); for (int32 I = 0; I < F.Num(); ++I) P[I] = int16(FMath::Clamp(FMath::Tanh(F[I] * 1.2f) * 30000.f, -32767.f, 32767.f)); return P; }
	/** the door opening: the handle's latch letting go (two quick metal clicks), the seal's soft pop, the hinge */
	const TArray<int16>& DoorOpenPcm()
	{
		static TArray<int16> P;
		if (P.Num()) return P;
		TArray<float> F; F.SetNumZeroed(Rate * 45 / 100);
		uint32 Sd = 7u; float Hp = 0.f, Prev = 0.f, Lp = 0.f;
		for (int32 I = 0; I < F.Num(); ++I)
		{
			const float T = float(I) / Rate, N = Noise(Sd);
			Hp = 0.7f * (Hp + N - Prev); Prev = N;                              // bright noise (the metal)
			Lp += 0.08f * (N - Lp);                                             // dull noise (the rubber)
			float V = 0.f;
			for (const float C : {0.f, 0.034f}) if (T >= C) V += 0.75f * Hp * FMath::Exp(-(T - C) / 0.0045f) + 0.35f * FMath::Sin(2.f * PI * 2350.f * (T - C)) * FMath::Exp(-(T - C) / 0.006f);
			if (T >= 0.05f) V += 0.55f * Lp * FMath::Exp(-(T - 0.05f) / 0.035f) + 0.3f * FMath::Sin(2.f * PI * 150.f * (T - 0.05f)) * FMath::Exp(-(T - 0.05f) / 0.03f);
			if (T >= 0.12f) { const float H = T - 0.12f; V += 0.05f * FMath::Sin(2.f * PI * (330.f * H + 120.f * H * H)) * FMath::Sin(PI * FMath::Min(1.f, H / 0.3f)) * (0.6f + 0.4f * FMath::Sin(2.f * PI * 23.f * H)); }
			F[I] = V;
		}
		P = ToPcm(F);
		return P;
	}
	/** the door shutting: a heavy cab door's low thump, the latch catching, a little rattle */
	const TArray<int16>& DoorShutPcm()
	{
		static TArray<int16> P;
		if (P.Num()) return P;
		TArray<float> F; F.SetNumZeroed(Rate * 7 / 10);
		uint32 Sd = 11u; float Lp = 0.f, Hp = 0.f, Prev = 0.f;
		for (int32 I = 0; I < F.Num(); ++I)
		{
			const float T = float(I) / Rate, N = Noise(Sd);
			Lp += 0.05f * (N - Lp); Hp = 0.7f * (Hp + N - Prev); Prev = N;
			float V = 0.95f * FMath::Sin(2.f * PI * 62.f * T) * FMath::Exp(-T / 0.11f) + 0.5f * FMath::Sin(2.f * PI * 124.f * T + 0.6f) * FMath::Exp(-T / 0.07f);
			V += 1.6f * Lp * FMath::Exp(-T / 0.025f);
			if (T >= 0.014f) V += 0.55f * Hp * FMath::Exp(-(T - 0.014f) / 0.004f) + 0.25f * FMath::Sin(2.f * PI * 1900.f * (T - 0.014f)) * FMath::Exp(-(T - 0.014f) / 0.008f);
			V += 0.08f * FMath::Sin(2.f * PI * 870.f * T) * FMath::Exp(-T / 0.06f) * (0.5f + 0.5f * FMath::Sin(2.f * PI * 37.f * T));
			F[I] = V * FMath::Min(1.f, T / 0.002f);
		}
		P = ToPcm(F);
		return P;
	}
	/** the siren: an electronic wail, its tone rising and falling every 4.4 s, the horn speaker's hard edge */
	struct FSirenState { double Phase = 0.0, T = 0.0; };
	void SirenFill(FSirenState& S, TArray<int16>& Out, int32 N)
	{
		Out.SetNumUninitialized(N);
		for (int32 I = 0; I < N; ++I)
		{
			const double C = FMath::Fmod(S.T, 4.4) / 4.4;
			const double Hz = 640.0 + 820.0 * (C < 0.55 ? FMath::Sin(C / 0.55 * HALF_PI) : FMath::Cos((C - 0.55) / 0.45 * HALF_PI));
			S.Phase = FMath::Fmod(S.Phase + 2.0 * PI * Hz / Rate, 2.0 * PI);
			const double Ph = S.Phase;
			const double V = 0.6 * FMath::Sin(Ph) + 0.24 * FMath::Sin(3 * Ph) + 0.12 * FMath::Sin(5 * Ph) + 0.05 * FMath::Sin(7 * Ph);
			Out[I] = int16(FMath::Tanh(1.8 * V) * 22000.0);
			S.T += 1.0 / Rate;
		}
	}

	/** a bay door's motor: the start (relay click, the motor's kick, the sections jerking), the stop (the brake catching,
	 *  the sections' heavy clank, a rattle dying away) */
	TArray<int16> GarageClunk(bool bStop)
	{
		TArray<float> F; F.SetNumZeroed(Rate * (bStop ? 70 : 45) / 100);
		uint32 Sd = bStop ? 23u : 19u; float Lp = 0.f, Hp = 0.f, Prev = 0.f;
		for (int32 I = 0; I < F.Num(); ++I)
		{
			const float T = float(I) / Rate, N = Noise(Sd);
			Lp += 0.06f * (N - Lp); Hp = 0.75f * (Hp + N - Prev); Prev = N;
			float V = 0.f;
			if (!bStop) V += 0.4f * Hp * FMath::Exp(-T / 0.003f);                                    // the relay
			const float T0 = bStop ? 0.f : 0.03f;
			if (T >= T0)
			{
				const float U = T - T0;
				V += (bStop ? 0.9f : 0.6f) * FMath::Sin(2.f * PI * (bStop ? 52.f : 68.f) * U) * FMath::Exp(-U / (bStop ? 0.16f : 0.1f));
				V += (bStop ? 1.3f : 0.8f) * Lp * FMath::Exp(-U / 0.03f);
				V += (bStop ? 0.35f : 0.2f) * Hp * FMath::Exp(-U / 0.012f) * (0.6f + 0.4f * FMath::Sin(2.f * PI * 9.f * U));
				V += 0.12f * FMath::Sin(2.f * PI * 610.f * U) * FMath::Exp(-U / (bStop ? 0.12f : 0.05f)) * (0.5f + 0.5f * FMath::Sin(2.f * PI * 27.f * U));   // the sections' rattle
			}
			F[I] = V;
		}
		return ToPcm(F);
	}
	/** the motor running: its hum, the chain, the sections' rollers knocking past the track's joints */
	struct FHumState { double T = 0.0, Ph = 0.0; uint32 Seed = 5u; float Hp = 0.f, Prev = 0.f; };
	void HumFill(FHumState& S, TArray<int16>& Out, int32 N)
	{
		Out.SetNumUninitialized(N);
		for (int32 I = 0; I < N; ++I)
		{
			S.Ph = FMath::Fmod(S.Ph + 2.0 * PI * 96.0 / Rate, 2.0 * PI);
			const float Nz = Noise(S.Seed); S.Hp = 0.8f * (S.Hp + Nz - S.Prev); S.Prev = Nz;
			const float Knock = FMath::Pow(0.5f + 0.5f * float(FMath::Sin(2.0 * PI * 3.1 * S.T)), 18.f);   // a roller over a joint, three a second
			const float V = 0.22f * float(FMath::Sin(S.Ph) + 0.45 * FMath::Sin(2 * S.Ph) + 0.2 * FMath::Sin(3 * S.Ph)) + 0.07f * S.Hp + 0.35f * Knock * S.Hp;
			Out[I] = int16(FMath::Tanh(V * 1.4f) * 20000.f);
			S.T += 1.0 / Rate;
		}
	}

	// ---- getting in: where your pelvis, feet and hands are (Blender metres, the truck's own measures) at each moment.
	// The driver's door is on the left: the hung step outside it (top 0.40 m, x -1.6, y 4.3-4.9), the step inside the
	// doorway (top 0.75 m), the cab floor at 1.04 m; a handle either side of the doorway inside (rear y 3.78, front
	// y 5.12, about 1.25-1.9 m up); the middle seat's cushion at 1.6 m (y 4.08); the pedals.
	enum class EHand : uint8 { Free, At, Wheel };
	struct FKey { float T; FVector Pelvis; FVector2D Face; float Lean; FVector Foot[2]; EHand HM[2]; FVector Hand[2]; float Grip[2]; };
	const FKey Keys[] = {
		// standing at the foot of the steps (replaced by where you really are when you press E)
		{0.00f, {-2.15f, 4.60f, 0.95f}, {1, 0}, 0.f,  {{-2.15f, 4.72f, 0.f}, {-2.15f, 4.48f, 0.f}}, {EHand::Free, EHand::Free}, {}, {0.f, 0.f}},
		// hands up to the handles either side of the doorway
		{0.55f, {-2.02f, 4.60f, 0.95f}, {1, 0}, 8.f,  {{-2.08f, 4.72f, 0.f}, {-2.08f, 4.48f, 0.f}}, {EHand::At, EHand::At}, {{-1.42f, 5.12f, 1.45f}, {-1.42f, 3.78f, 1.40f}}, {1.f, 1.f}},
		// the right foot on the step outside
		{1.05f, {-1.95f, 4.58f, 1.10f}, {1, 0}, 14.f, {{-2.08f, 4.72f, 0.f}, {-1.62f, 4.50f, 0.40f}}, {EHand::At, EHand::At}, {{-1.42f, 5.12f, 1.55f}, {-1.42f, 3.78f, 1.50f}}, {1.f, 1.f}},
		// pulling up: the left foot on the step inside the doorway
		{1.60f, {-1.72f, 4.60f, 1.48f}, {1, 0}, 16.f, {{-1.30f, 4.74f, 0.75f}, {-1.62f, 4.50f, 0.40f}}, {EHand::At, EHand::At}, {{-1.42f, 5.12f, 1.70f}, {-1.42f, 3.78f, 1.70f}}, {1.f, 1.f}},
		// the right foot onto the cab floor
		{2.10f, {-1.36f, 4.58f, 1.88f}, {1, 0.1f}, 14.f, {{-1.30f, 4.74f, 0.75f}, {-0.98f, 4.48f, 1.04f}}, {EHand::At, EHand::At}, {{-1.42f, 5.12f, 1.85f}, {-1.42f, 3.78f, 1.85f}}, {1.f, 1.f}},
		// in: both feet on the floor, turning to the front, letting go
		{2.60f, {-0.88f, 4.62f, 1.98f}, {0.6f, 0.8f}, 8.f, {{-0.92f, 4.78f, 1.04f}, {-0.98f, 4.48f, 1.04f}}, {EHand::At, EHand::Free}, {{-1.38f, 5.12f, 1.75f}, {}}, {1.f, 0.f}},
		// past the left seat's front, to the middle
		{3.10f, {-0.40f, 4.66f, 1.90f}, {0.3f, 1.f}, 4.f, {{-0.40f, 4.82f, 1.04f}, {-0.52f, 4.56f, 1.04f}}, {EHand::Free, EHand::Free}, {}, {0.f, 0.f}},
		// sat down in the middle, feet to the pedals, hands on the wheel
		{3.70f, {0.00f, 4.02f, 1.70f}, {0, 1}, -8.f, {{-0.14f, 4.70f, 1.04f}, {0.20f, 4.62f, 1.04f}}, {EHand::Wheel, EHand::Wheel}, {}, {1.f, 1.f}},
	};
	constexpr int32 NumKeys = UE_ARRAY_COUNT(Keys);
	constexpr float SeqLen = 3.70f;
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
	MoveCam = CreateDefaultSubobject<UCameraComponent>(TEXT("MoveCam"));
	MoveCam->SetupAttachment(Body);
	MoveCam->SetFieldOfView(85.f);
	MoveCam->SetAutoActivate(false);
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
	FTransform PadT;
	if (Model->NodeBase(TEXT("Pedal_Brake"), PadT)) BrakePad = PadT.GetLocation();
	WheelNow = BSteeringWheel; AccelNow = BAccel; BrakeNow = FTransform(BrakePin);
	// your body for getting in and driving: the walker's own mannequin, posed by hand (TruckDriverRig.h)
	if (USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")))
	{
		Rider = NewObject<UPoseableMeshComponent>(this, TEXT("Rider"));
		Rider->SetSkinnedAssetAndUpdate(Manny);
		Rider->SetupAttachment(Model->GetModelRoot());
		Rider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rider->SetCastShadow(true);
		Rider->RegisterComponent();
		Rider->SetRelativeTransform(FTransform::Identity);
		Rider->SetVisibility(false);
		if (!Rig.Init(Rider)) ADP_ERROR(TEXT("Fire truck: the driver's body couldn't be rigged"));
	}
	else ADP_ERROR(TEXT("Fire truck: the driver's body (SKM_Manny_Simple) is missing"));
	// how far its sounds carry: the doors across the forecourt, the siren far over the airport
	auto Att = [this](float Inner, float Falloff)
	{
		USoundAttenuation* A = NewObject<USoundAttenuation>(this);
		A->Attenuation.bAttenuate = true; A->Attenuation.bSpatialize = true;
		A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		A->Attenuation.AttenuationShapeExtents = FVector(Inner, 0, 0);
		A->Attenuation.FalloffDistance = Falloff;
		A->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		return A;
	};
	NearSound = Att(150.f, 3500.f);
	FarSound = Att(1500.f, 120000.f);
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
	LookYaw = LookPitch = 0.f;
	if (Seq != ESeq::None) return;   // (climbing in or out: the view you'll have once in)
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
	In->BindAction(TEXT("Siren"), IE_Pressed, this, &AFireTruck::ToggleSiren);
}

void AFireTruck::Enter(APawn* Walker)
{
	if (Driver || Seq != ESeq::None || !Walker || !Model) return;
	APlayerController* PC = Cast<APlayerController>(Walker->GetController());
	if (!PC) return;
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	if (FVector::Dist2D(RX.InverseTransformPosition(Walker->GetActorLocation()), BlenderToRoot(Keys[0].Pelvis.X, Keys[0].Pelvis.Y, 0.f)) > 450.f)
	{
		ADP_LOG(TEXT("Fire truck: too far from the driver's door to get in (%s)"), *GetName());
		return;
	}
	// where you are and look from, in the truck's own space: the climb starts there
	FVector EyeAt; FRotator EyeRot;
	Walker->GetActorEyesViewPoint(EyeAt, EyeRot);
	if (PC->PlayerCameraManager) { EyeAt = PC->PlayerCameraManager->GetCameraLocation(); EyeRot = PC->PlayerCameraManager->GetCameraRotation(); }
	CamFrom = FTransform(EyeRot, EyeAt).GetRelativeTransform(RX);
	StartPelvis = RX.InverseTransformPosition(Walker->GetActorLocation());
	StartPelvis.Z = Rig.IsReady() ? Rig.PelvisHeight() : 95.f;
	StartFacing = RX.InverseTransformVector(Walker->GetActorForwardVector()); StartFacing.Z = 0.f; StartFacing.Normalize();
	Driver = Walker;
	Walker->SetActorHiddenInGame(true);
	Walker->SetActorEnableCollision(false);
	if (ACharacter* Ch = Cast<ACharacter>(Walker)) { Ch->GetCharacterMovement()->DisableMovement(); if (Ch->GetMesh()) Ch->GetMesh()->bCastHiddenShadow = false; }
	Walker->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform);
	PC->Possess(this);
	DriverCam->SetActive(false); ChaseCam->SetActive(false); MoveCam->SetActive(true);
	MoveCam->SetWorldTransform(CamFrom * RX);
	if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
	SetPhysics(true);
	Still = 0.f; bTestInput = false; bHandbrake = true;
	Seq = ESeq::In; SeqT = 0.f;
	DoorTarget = 1.f;
	if (Rider) Rider->SetVisibility(true);
	Sequence(0.f);
	ADP_LOG(TEXT("Fire truck: getting in (%s)"), *GetName());
}

FVector AFireTruck::DoorSpot() const
{
	return Model ? Model->GetModelRoot()->GetComponentTransform().TransformPosition(BlenderToRoot(Keys[0].Pelvis.X - 0.5f, Keys[0].Pelvis.Y - 0.3f, 1.0f)) : GetActorLocation();
}

void AFireTruck::Exit()
{
	if (!Driver || !Model || Seq != ESeq::None) return;
	// at a standstill you climb out; moving, it stops first
	if (SpeedKmh() > 4.f) { bExitPending = true; return; }
	StartExit();
}

void AFireTruck::ExitNow()
{
	if (!Driver || !Model) return;
	StartExit();
	FinishExit();
	MoveCam->SetActive(false);
}

void AFireTruck::StartExit()
{
	bExitPending = false;
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	UCameraComponent* From = bChase ? ChaseCam.Get() : DriverCam.Get();
	CamFrom = From->GetComponentTransform().GetRelativeTransform(RX);
	DriverCam->SetActive(false); ChaseCam->SetActive(false); MoveCam->SetActive(true);
	MoveCam->SetWorldTransform(From->GetComponentTransform());
	InThrottle = InBrake = InSteer = KeyFwd = KeySteer = PadThrottle = PadBrake = 0.f;
	bHandbrake = true;   // left with the handbrake on
	// you end a step out from the foot of the steps, turned along the truck towards its front (it stays in view)
	StartPelvis = BlenderToRoot(Keys[0].Pelvis.X - 0.25f, Keys[0].Pelvis.Y - 0.1f, 0.f); StartPelvis.Z = Rig.IsReady() ? Rig.PelvisHeight() : 95.f;
	StartFacing = DirToRoot(-0.35f, 1.f);
	Seq = ESeq::Out; SeqT = 0.f;
	DoorTarget = 1.f;
	ADP_LOG(TEXT("Fire truck: getting out (%s)"), *GetName());
}

void AFireTruck::FinishExit()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	APawn* Wk = Driver;
	Driver = nullptr;
	Seq = ESeq::None;
	if (Rider) Rider->SetVisibility(false);
	DoorTarget = 0.f;
	if (!Wk) return;
	Wk->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// you stand where the climb ended, facing the truck, looking the way the camera looked
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	ACharacter* Ch = Cast<ACharacter>(Wk);
	const float Half = Ch ? Ch->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	FVector At = RX.TransformPosition(FVector(StartPelvis.X, StartPelvis.Y, Half + 2.f));
	const FRotator Look = MoveCam->GetComponentRotation();
	FRotator Rot(0.f, RX.TransformVectorNoScale(StartFacing).Rotation().Yaw, 0.f);
	GetWorld()->FindTeleportSpot(Wk, At, Rot);
	Wk->TeleportTo(At, Rot);
	Wk->SetActorEnableCollision(true);
	Wk->SetActorHiddenInGame(false);
	if (Ch) { Ch->GetCharacterMovement()->SetMovementMode(MOVE_Walking); if (Ch->GetMesh()) Ch->GetMesh()->bCastHiddenShadow = true; }
	if (PC) { PC->Possess(Wk); PC->SetControlRotation(FRotator(Look.Pitch, Rot.Yaw, 0.f)); }
	MoveCam->SetActive(false); DriverCam->SetActive(false); ChaseCam->SetActive(false);
	ADP_LOG(TEXT("Fire truck: you got out (%s), %.0f km/h"), *GetName(), SpeedKmh());
}

void AFireTruck::ToggleSiren()
{
	if (!Model) return;
	bSiren = !bSiren;
	if (bSiren)
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
		W->SetSampleRate(Rate); W->NumChannels = 1; W->Duration = INDEFINITELY_LOOPING_DURATION; W->bLooping = false;
		W->SoundGroup = SOUNDGROUP_Effects;
		TSharedPtr<FSirenState> St = MakeShared<FSirenState>();
		W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([St](USoundWaveProcedural* Wv, int32 Samples)
		{
			TArray<int16> Buf; SirenFill(*St, Buf, FMath::Max(Samples, Rate / 20));
			Wv->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Buf.Num() * sizeof(int16));
		});
		// (the speakers on the cab's roof, at the front)
		SirenOut = UGameplayStatics::SpawnSoundAttached(W, Model->GetModelRoot(), NAME_None, BlenderToRoot(0.f, 5.4f, 3.2f), EAttachLocation::KeepRelativeOffset, false, 0.9f, 1.f, 0.f, FarSound);
	}
	else if (SirenOut) { SirenOut->Stop(); SirenOut = nullptr; }
	ADP_LOG(TEXT("Fire truck: siren %s"), bSiren ? TEXT("on") : TEXT("off"));
}

void AFireTruck::Sound(const TArray<int16>& Pcm, const FVector& RootAt, bool bFar)
{
	if (!Model || Pcm.Num() == 0) return;
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
	W->SetSampleRate(Rate); W->NumChannels = 1; W->Duration = Pcm.Num() / float(Rate); W->bLooping = false;
	W->SoundGroup = SOUNDGROUP_Effects;
	W->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
	UGameplayStatics::SpawnSoundAttached(W, Model->GetModelRoot(), NAME_None, RootAt, EAttachLocation::KeepRelativeOffset, true, 1.f, 1.f, 0.f, bFar ? FarSound.Get() : NearSound.Get());
}

FDriverPose AFireTruck::PoseAt(float T) const
{
	// the keys either side, eased between; the first one where you really stood
	auto KeyPelvis = [this](int32 I) { return I == 0 ? StartPelvis : BlenderToRoot(Keys[I].Pelvis.X, Keys[I].Pelvis.Y, Keys[I].Pelvis.Z); };
	auto KeyFace = [this](int32 I) { return I == 0 ? StartFacing : DirToRoot(Keys[I].Face.X, Keys[I].Face.Y); };
	T = FMath::Clamp(T, 0.f, SeqLen);
	int32 A = 0;
	while (A < NumKeys - 2 && T > Keys[A + 1].T) ++A;
	const int32 B = A + 1;
	const float U = FMath::Clamp((T - Keys[A].T) / (Keys[B].T - Keys[A].T), 0.f, 1.f), E = FMath::SmoothStep(0.f, 1.f, U);
	FDriverPose P;
	// the pelvis on a smooth curve through the keys
	const FVector P0 = KeyPelvis(FMath::Max(A - 1, 0)), P1 = KeyPelvis(A), P2 = KeyPelvis(B), P3 = KeyPelvis(FMath::Min(B + 1, NumKeys - 1));
	P.Pelvis = 0.5f * ((2.f * P1) + (-P0 + P2) * E + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * E * E + (-P0 + 3.f * P1 - 3.f * P2 + P3) * E * E * E);
	P.Facing = FMath::Lerp(KeyFace(A), KeyFace(B), E).GetSafeNormal();
	P.Lean = FMath::Lerp(Keys[A].Lean, Keys[B].Lean, E);
	const FVector Up(0, 0, 1);
	for (int32 S = 0; S < 2; ++S)
	{
		// which side is this one's: the rig's own left (a vector across the body)
		const FVector Across = FVector::CrossProduct(Up, P.Facing).GetSafeNormal();
		const FVector StartAcross = FVector::CrossProduct(Up, StartFacing).GetSafeNormal();
		const float Sgn = S == 0 ? -1.f : 1.f;
		// a foot that moves lifts on its way (the start: under your hips)
		auto KeyFoot = [&](int32 I) { return I == 0 ? StartPelvis * FVector(1, 1, 0) + Sgn * 12.f * StartAcross : BlenderToRoot(Keys[I].Foot[S].X, Keys[I].Foot[S].Y, Keys[I].Foot[S].Z); };
		const FVector Fa = KeyFoot(A), Fb = KeyFoot(B);
		P.Foot[S] = FMath::Lerp(Fa, Fb, E) + (FVector::Dist(Fa, Fb) > 3.f ? Up * 14.f * FMath::Sin(PI * E) : FVector::ZeroVector);
		// the hands: what they hold, or hanging by your side
		const FVector Rest = P.Pelvis + Sgn * Across * 24.f + P.Facing * 4.f - Up * 10.f;
		auto KeyHand = [&](int32 I) -> FVector
		{
			if (Keys[I].HM[S] == EHand::At) return BlenderToRoot(Keys[I].Hand[S].X, Keys[I].Hand[S].Y, Keys[I].Hand[S].Z);
			if (Keys[I].HM[S] == EHand::Wheel)
			{
				// on the rim at ten to two, carried round as the wheel turns
				const FVector WC(0.f, 4.6f, 1.87f), Rt(1.f, 0.f, 0.f), Top(0.f, 0.866f, 0.5f), Ax(0.f, -0.5f, 0.866f);
				const float Ang = FMath::DegreesToRadians(S == 0 ? -62.f : 62.f);
				const FVector Rim = WC + (Rt * FMath::Sin(Ang) + Top * FMath::Cos(Ang)) * 0.23f;
				const FVector Wrist = Rim + (Rim - WC).GetSafeNormal() * 0.045f + Ax * 0.03f;
				return WheelNow.TransformPosition(BSteeringWheel.InverseTransformPosition(BlenderToRoot(Wrist.X, Wrist.Y, Wrist.Z)));
			}
			return Rest;
		};
		P.Hand[S] = FMath::Lerp(KeyHand(A), KeyHand(B), E);
		P.bHand[S] = true;
		P.Grip[S] = FMath::Lerp(Keys[A].Grip[S], Keys[B].Grip[S], E);
	}
	return P;
}

FTransform AFireTruck::CamPath(float T) const
{
	// from where you looked, out to behind your left shoulder (the door and steps in view), and in to where you'll look
	const FVector TpAt = BlenderToRoot(-5.3f, 1.3f, 2.75f), Aim = BlenderToRoot(-1.35f, 4.3f, 1.35f);
	const FTransform Tp(FRotationMatrix::MakeFromX(Aim - TpAt).ToQuat(), TpAt);
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	FTransform To;
	if (Seq == ESeq::In) To = (bChase ? ChaseCam.Get() : DriverCam.Get())->GetComponentTransform().GetRelativeTransform(RX);
	else
	{
		// exactly where your own view will be once you stand there (your eyes, or behind you)
		const FRotator Look(-6.f, StartFacing.Rotation().Yaw, 0.f);
		const AADPWalker* Wk = Cast<AADPWalker>(Driver.Get());
		const float Half = 90.f;
		To = Wk ? Wk->ViewFrom(FVector(StartPelvis.X, StartPelvis.Y, Half), Look) : FTransform(Look, FVector(StartPelvis.X, StartPelvis.Y, 160.f));
	}
	const float L = SeqLen + (Seq == ESeq::Out ? 0.25f : 0.f);
	auto Blend = [](const FTransform& A, const FTransform& B, float K)
	{
		const float E = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(K, 0.f, 1.f));
		return FTransform(FQuat::Slerp(A.GetRotation(), B.GetRotation(), E).GetNormalized(), FMath::Lerp(A.GetLocation(), B.GetLocation(), E));
	};
	if (T < 0.8f) return Blend(CamFrom, Tp, T / 0.8f);
	if (T < L - 0.8f) return Tp;
	return Blend(Tp, To, (T - (L - 0.8f)) / 0.8f);
}

void AFireTruck::Sequence(float Dt)
{
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	if (Seq != ESeq::None)
	{
		SeqT += Dt;
		MoveCam->SetWorldTransform(CamPath(SeqT) * RX);
		if (Seq == ESeq::In && SeqT >= 3.2f) DoorTarget = 0.f;     // pulled shut behind you
		if (Seq == ESeq::In && SeqT >= SeqLen)
		{
			Seq = ESeq::None;
			MoveCam->SetActive(false);
			SetView(bChase);
			ADP_LOG(TEXT("Fire truck: you got in (%s)"), *GetName());
		}
		else if (Seq == ESeq::Out && SeqT >= SeqLen + 0.25f) { FinishExit(); return; }
	}
	if (!Rider || !Rig.IsReady() || !Driver) return;
	// the body: climbing (in: forwards through the keys, out: backwards), or at the wheel
	const float KeyT = Seq == ESeq::Out ? SeqLen - SeqT : Seq == ESeq::In ? SeqT : SeqLen;
	FDriverPose P = PoseAt(KeyT);
	const float SitT = Keys[NumKeys - 2].T;
	if (KeyT > SitT)
	{
		// at the pedals: the right foot's ball on the accelerator, or across on the brake; the left foot on the floor
		const bool bOnBrake = (bReverse ? InThrottle : InBrake) > 0.05f;
		const FVector Pad = bOnBrake ? BrakeNow.TransformPosition(FTransform(BrakePin).InverseTransformPosition(BrakePad)) : AccelNow.GetLocation();
		const float Pitch = bOnBrake ? 18.f : 32.f - InThrottle * 12.f;
		const FVector Up(0, 0, 1);
		const FVector Toe = (P.Facing * FMath::Cos(FMath::DegreesToRadians(Pitch)) + Up * FMath::Sin(FMath::DegreesToRadians(Pitch))).GetSafeNormal();
		const FVector Sole = Pad - Toe * 13.f + Up * 1.5f;
		const float K = FMath::SmoothStep(0.f, 1.f, (KeyT - SitT) / (SeqLen - SitT));
		P.Foot[1] = FMath::Lerp(P.Foot[1], Sole, K);
		P.FootPitch[1] = Pitch * K;
	}
	Rig.Pose(P);
	// in the driver's seat your own head stays out of the way of your eyes
	APlayerController* PC = Cast<APlayerController>(GetController());
	const bool bHide = PC && PC->PlayerCameraManager && FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), RX.TransformPosition(Rig.Eyes())) < 45.f;
	if (bHide != bHeadHidden) { bHeadHidden = bHide; if (bHide) Rider->HideBoneByName(TEXT("head"), PBO_None); else Rider->UnHideBoneByName(TEXT("head")); }
}

void AFireTruck::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Model || Dt <= 0.f) return;
	if (bExitPending)
	{
		InThrottle = 0.f; InBrake = 1.f; InSteer = 0.f; StopHold = 0.f;
		if (SpeedKmh() < 4.f) StartExit();
	}
	else if (Seq != ESeq::None) { InThrottle = InBrake = InSteer = 0.f; }
	else if (Driver && !bTestInput)
	{
		InThrottle = FMath::Max(FMath::Max(KeyFwd, 0.f), PadThrottle);
		InBrake = FMath::Max(FMath::Max(-KeyFwd, 0.f), PadBrake);
		InSteer = FMath::Clamp(KeySteer, -1.f, 1.f);
	}
	// the parking brake lets itself off as you pull away (as on today's trucks); held with Space / pad A it stays on
	if (InThrottle > 0.1f && !bHandHeld) bHandbrake = false;
	if (bSim) Simulate(FMath::Min(Dt, 1.f / 20.f));
	if (bSim || DoorK > 0.f || DoorTarget > 0.f) Animate(Dt);
	if (Driver || Seq != ESeq::None) Sequence(Dt);
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
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPTruckWheel), true, this);   // (the buildings' floors by their own triangles)
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
	WheelNow = FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(-Steer / MaxSteer * 270.f))) * BSteeringWheel;
	AccelNow = BAccel * About(BAccel.TransformPosition(FVector(0, 0, -11.f)), FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-InThrottle * 14.f)));
	BrakeNow = FTransform(BrakePin) * About(BrakePin, FQuat(FVector::ForwardVector, FMath::DegreesToRadians(InBrake * 16.f)));
	if (GSteeringWheel >= 0) Model->MoveGroup(GSteeringWheel, WheelNow);
	if (GAccel >= 0) Model->MoveGroup(GAccel, AccelNow);
	if (GBrake >= 0) Model->MoveGroup(GBrake, BrakeNow);
	// the driver's door: swung open as you reach for it, pulled shut behind you (with their sounds)
	if (DoorK != DoorTarget)
	{
		const float Was = DoorK;
		DoorK = FMath::FInterpConstantTo(DoorK, DoorTarget, Dt, DoorTarget > DoorK ? 2.2f : 1.9f);
		const FVector DoorAt = BlenderToRoot(-1.55f, 4.35f, 1.5f);
		if (Was == 0.f && DoorK > 0.f) Sound(DoorOpenPcm(), DoorAt, false);
		if (Was > 0.f && DoorK == 0.f) Sound(DoorShutPcm(), DoorAt, false);
		// opening: quick, then easing out; shutting: swung, fastest at the end
		const float A = DoorTarget > 0.f ? FMath::Sin(DoorK * HALF_PI) : DoorK * DoorK * (2.f - DoorK);
		if (GDoor[0] >= 0) Model->MoveGroup(GDoor[0], FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(DoorSign * 70.f * A)), HingeRoot[0]));
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

void AFireStationDoors::Sound(const FBay& B, const TArray<int16>& Pcm)
{
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
	W->SetSampleRate(Rate); W->NumChannels = 1; W->Duration = Pcm.Num() / float(Rate); W->bLooping = false;
	W->SoundGroup = SOUNDGROUP_Effects;
	W->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
	UGameplayStatics::SpawnSoundAtLocation(this, W, B.World + FVector(0, 0, 300.f), FRotator::ZeroRotator, 1.f, 1.f, 0.f, Heard);
}

void AFireStationDoors::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Station) return;
	if (!Heard)
	{
		Heard = NewObject<USoundAttenuation>(this);
		Heard->Attenuation.bAttenuate = true; Heard->Attenuation.bSpatialize = true;
		Heard->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Heard->Attenuation.AttenuationShapeExtents = FVector(400.f, 0, 0);
		Heard->Attenuation.FalloffDistance = 6000.f;
	}
	const FTransform SX = Station->GetModelRoot()->GetComponentTransform();
	bool bMoved = false;
	for (FBay& B : Bays)
	{
		if (B.Group < 0) continue;
		// open for a truck in this bay's own lane (owner, 2026-11-11: only the door in front of your truck, as soon as
		// you sit at the wheel; the others stay shut; as many open as there are trucks going), within 20 m of the door,
		// inside or out. Shut otherwise: shut when you start, shut as you walk or drive up.
		bool bWant = bTestOpen;
		for (const TWeakObjectPtr<AFireTruck>& T : AFireTruck::All)
		{
			if (!T.IsValid() || !((T->IsDriven() && !T->IsBusy()) || T->SpeedKmh() > 1.f)) continue;
			const FVector L = SX.InverseTransformPosition(T->GetActorLocation());
			if (FMath::Abs(L.X - B.Pivot.X) < 300.f && FMath::Abs(L.Y) < 2000.f) { bWant = true; break; }
		}
		// never down onto anything (owner: not while a truck or anyone is under it): the door comes down only in its
		// opening (its top runs along the ceiling, high over the trucks), so anything across the opening holds it
		bool bBlocked = false;
		for (const TWeakObjectPtr<AFireTruck>& T : AFireTruck::All)
		{
			if (!T.IsValid()) continue;
			const FVector F = T->GetActorForwardVector() * 620.f;
			const FVector A = SX.InverseTransformPosition(T->GetActorLocation() - F), C = SX.InverseTransformPosition(T->GetActorLocation() + F);
			const float Mx = 0.5f * (A.X + C.X);
			if (FMath::Min(A.Y, C.Y) < 90.f && FMath::Max(A.Y, C.Y) > -90.f && FMath::Abs(Mx - B.Pivot.X) < 290.f + 160.f) { bBlocked = true; break; }
		}
		if (!bBlocked)
			for (TActorIterator<APawn> It(GetWorld()); It; ++It)
			{
				if (Cast<AFireTruck>(*It) || It->IsHidden()) continue;
				const FVector L = SX.InverseTransformPosition(It->GetActorLocation());
				if (FMath::Abs(L.Y) < 110.f && FMath::Abs(L.X - B.Pivot.X) < 330.f) { bBlocked = true; break; }
			}
		B.Hold = bWant ? 3.f : FMath::Max(0.f, B.Hold - Dt);
		// where it's going: up for a truck (and a few seconds after); down when nothing wants it, unless something is in
		// the opening (then it stops where it is, or stays up)
		const float Target = (bWant || B.Hold > 0.f) ? 1.f : (bBlocked ? B.K : 0.f);
		const int32 Want = Target > B.K + 1e-4f ? 1 : Target < B.K - 1e-4f ? -1 : 0;
		auto StopMotor = [&]()
		{
			Sound(B, GarageClunk(true));
			if (B.Hum.IsValid()) B.Hum->Stop();
			B.Hum = nullptr; B.Dir = 0;
		};
		if (B.Dir != 0 && Want != B.Dir) { StopMotor(); B.Pause = Want != 0 ? 0.6f : 0.f; }   // stopped, or turning back: a hard stop first
		if (B.Pause > 0.f) { B.Pause -= Dt; continue; }
		if (Want != 0 && B.Dir == 0)
		{
			// the motor starts: its clunk, then its hum and the sections' rattle while it runs
			Sound(B, GarageClunk(false));
			USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
			W->SetSampleRate(Rate); W->NumChannels = 1; W->Duration = INDEFINITELY_LOOPING_DURATION; W->bLooping = false;
			W->SoundGroup = SOUNDGROUP_Effects;
			TSharedPtr<FHumState> St = MakeShared<FHumState>();
			W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([St](USoundWaveProcedural* Wv, int32 Samples)
			{
				TArray<int16> Buf; HumFill(*St, Buf, FMath::Max(Samples, Rate / 20));
				Wv->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Buf.Num() * sizeof(int16));
			});
			B.Hum = UGameplayStatics::SpawnSoundAtLocation(this, W, B.World + FVector(0, 0, 300.f), FRotator::ZeroRotator, 0.8f, 1.f, 0.f, Heard, nullptr, false);
			B.Dir = Want;
		}
		if (B.Dir == 0) continue;
		// about 5 s from shut to open (a fire station's fast doors)
		B.K = FMath::Clamp(B.K + B.Dir * Dt / 5.f, 0.f, 1.f);
		if ((B.Dir > 0 && B.K >= Target) || (B.Dir < 0 && B.K <= Target)) { B.K = Target; StopMotor(); }
		// a sectional door: its foot rises in the opening, its top runs back along the ceiling track (it never swings
		// down into the hall)
		const float Phi = HALF_PI * B.K;
		Station->MoveGroup(B.Group, FTransform(FQuat(FVector::ForwardVector, -Phi), B.Pivot + FVector(0.f, 520.f * FMath::Sin(Phi), 0.f)));
		bMoved = true;
	}
	if (bMoved) Station->FlushMoves();
}
#pragma endregion
