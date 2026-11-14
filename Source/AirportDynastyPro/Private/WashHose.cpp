// The fire station's wash hose (WashHose.h).
#include "WashHose.h"
#include "ADPLog.h"
#include "ADPWalker.h"
#include "BabylonModel.h"
#include "FireTruck.h"
#include "CableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
	TArray<TWeakObjectPtr<AWashHose>> AllHoses;
	constexpr float JetSpeed = 2000.f, HoseGravity = 980.f, DropsPerSecond = 260.f, MaxDropLife = 2.f;
	constexpr int32 MaxWet = 40;
	constexpr float WetDries = 180.f;   // a wet patch dries off in three minutes (cool, damp air)
	constexpr int32 HoseRate = 44100;

	// the hiss of the jet: noise, a body of lower rush, a little crackle of droplets, eased in and out
	struct FHissState { float Level = 0.f, Lp1 = 0.f, Lp2 = 0.f; uint32 Seed = 12345u; };
	float HoseNoise(FHissState& S) { S.Seed = S.Seed * 1664525u + 1013904223u; return float((S.Seed >> 9) & 0xFFFF) / 32768.f - 1.f; }
}

AWashHose::AWashHose()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AWashHose* AWashHose::Held()
{
	for (const TWeakObjectPtr<AWashHose>& H : AllHoses) if (H.IsValid() && H->Holder.IsValid() && H->Phase != EPhase::Hung) return H.Get();
	return nullptr;
}

TArray<AWashHose*> AWashHose::All()
{
	TArray<AWashHose*> L;
	for (const TWeakObjectPtr<AWashHose>& H : AllHoses) if (H.IsValid()) L.Add(H.Get());
	return L;
}

AWashHose* AWashHose::First()
{
	for (const TWeakObjectPtr<AWashHose>& H : AllHoses) if (H.IsValid()) return H.Get();
	return nullptr;
}

bool AWashHose::NearHook(const FVector& At)
{
	for (const TWeakObjectPtr<AWashHose>& H : AllHoses) if (H.IsValid() && FVector::Dist2D(At, H->Hook) < 150.f) return true;
	return false;
}

FVector AWashHose::HookSpot() const
{
	// 1.3 m out from the wall in front of the hook, at standing height (the model's front: its -Y under the root)
	const FVector Out = Model ? -Model->GetActorRightVector() : FVector::ForwardVector;
	return FVector(Hook.X, Hook.Y, Hook.Z - 111.f + 100.f) + Out * 130.f;
}

void AWashHose::Setup(ABabylonModel* InModel)
{
	Model = InModel;
	if (!Model) return;
	AllHoses.Add(this);
	SetActorTransform(Model->GetActorTransform());
	// the model's pieces: a sketch point (x, y, z) is (-x, z, y) under its root, metres to centimetres
	const FTransform Root = Model->GetActorTransform();
	Outlet = Root.TransformPosition(FVector(0.f, -0.12f, 0.57f) * 100.f);
	Hook = Root.TransformPosition(FVector(-0.235f, -0.051f, 1.11f) * 100.f);
	TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Found;
	Model->InstancesNamed(TEXT("WashHoseGun"), false, Found);
	if (Found.Num()) { GunIsm = Found[0].Get<0>(); GunItem = Found[0].Get<1>(); GunIsm->GetInstanceTransform(GunItem, GunHome, true); }
	Found.Reset();
	Model->InstancesNamed(TEXT("HoseLoop"), true, Found);
	UMaterialInterface* HoseMat = Found.Num() ? Found[0].Get<0>()->GetMaterial(0) : nullptr;
	// the loops, front (furthest from the wall) first; then the second layer over them from the same pieces, a little
	// larger, resting on the first (the top of each 2.6 cm higher). Each loop's own length: an oval 23 x 28.75 cm
	{
		const FVector Out = -Model->GetActorRightVector();
		Found.Sort([&](const auto& A, const auto& B) { FTransform Ta, Tb; A.Get<0>()->GetInstanceTransform(A.Get<1>(), Ta, true); B.Get<0>()->GetInstanceTransform(B.Get<1>(), Tb, true);
			return FVector::DotProduct(Ta.GetLocation(), Out) > FVector::DotProduct(Tb.GetLocation(), Out); });
		auto Oval = [](float A, float B) { return PI * (3.f * (A + B) - FMath::Sqrt((3.f * A + B) * (A + 3.f * B))); };
		constexpr float Up = 1.113f;
		TArray<FLoop> Inner, Outer;
		for (const auto& F : Found)
		{
			FLoop L{F.Get<0>(), F.Get<1>(), FTransform::Identity, Oval(23.f, 28.75f), true};
			L.Ism->GetInstanceTransform(L.Item, L.Home, true);
			Inner.Add(L);
			FLoop O = L; O.Home.SetScale3D(L.Home.GetScale3D() * Up);
			O.Home.AddToTranslation(FVector(0.f, 0.f, 2.6f - 28.75f * (Up - 1.f)));
			O.Len = Oval(23.f * Up, 28.75f * Up);
			O.Item = O.Ism->AddInstance(O.Home, true);
			Outer.Add(O);
		}
		Loops = Outer; Loops.Append(Inner);
		for (const FLoop& L : Loops) HoseLen += L.Len;
	}
	// the gun as it comes into your hand (the hook's own copy hides meanwhile)
	Gun = NewObject<UStaticMeshComponent>(this, TEXT("Gun"));
	Gun->SetupAttachment(RootComponent);
	Gun->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Gun->SetCastShadow(true);
	if (GunIsm)
	{
		Gun->SetStaticMesh(GunIsm->GetStaticMesh());
		for (int32 i = 0; i < GunIsm->GetNumMaterials(); ++i) Gun->SetMaterial(i, GunIsm->GetMaterial(i));
	}
	Gun->SetWorldTransform(GunHome);
	Gun->SetVisibility(false);
	Gun->RegisterComponent();

	// the hose laid out from the tap to the gun: a hanging cable that lies along the ground and over what it meets
	Cable = NewObject<UCableComponent>(this, TEXT("Hose"));
	Cable->SetupAttachment(RootComponent);
	Cable->SetWorldLocation(Outlet);
	Cable->CableWidth = 2.6f;   // (the coil's own hose: 13 mm radius, Art/WashHose/make_washhose.py)
	Cable->NumSegments = 80;   // (owner, 2026-11-14: fully flexible, like a real hose)
	Cable->NumSides = 6;
	Cable->SolverIterations = 8;
	Cable->SubstepTime = 0.01f;
	Cable->CollisionFriction = 0.8f;
	Cable->bEnableCollision = true;
	Cable->bEnableStiffness = false;
	Cable->CableLength = 200.f;
	if (HoseMat) Cable->SetMaterial(0, HoseMat);
	Cable->SetVisibility(false);
	Cable->RegisterComponent();
	if (Gun->GetStaticMesh())
	{
		const FBox B = Gun->GetStaticMesh()->GetBoundingBox();
		Cable->SetAttachEndToComponent(Gun);
		Cable->EndLocation = FVector(0.f, B.Max.Y, B.GetCenter().Z);   // into the back of the gun
	}

	// the water: little drops, stretched along their flight so the jet reads as a stream
	Drops = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Water"));
	Drops->SetupAttachment(RootComponent);
	Drops->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_WaterDrop.M_WaterDrop"))) Drops->SetMaterial(0, M);
	Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drops->SetCastShadow(false);
	Drops->SetNumCustomDataFloats(2);   // how white, how much (Tools/set_water_materials.py)
	Drops->SetUsingAbsoluteLocation(true); Drops->SetUsingAbsoluteRotation(true); Drops->SetUsingAbsoluteScale(true);
	Drops->SetWorldTransform(FTransform::Identity);
	Drops->RegisterComponent();
	Mist = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Spray"));
	Mist->SetupAttachment(RootComponent);
	Mist->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_WaterMist.M_WaterMist"))) Mist->SetMaterial(0, M);
	Mist->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mist->SetCastShadow(false);
	Mist->SetNumCustomDataFloats(2);
	Mist->SetUsingAbsoluteLocation(true); Mist->SetUsingAbsoluteRotation(true); Mist->SetUsingAbsoluteScale(true);
	Mist->SetWorldTransform(FTransform::Identity);
	Mist->RegisterComponent();
	ADP_LOG(TEXT("Wash hose: ready at the fire station (gun %s, hose material %s, %d loops on the coil, %.1f m of hose)"), GunIsm ? TEXT("found") : TEXT("MISSING"), HoseMat ? *HoseMat->GetName() : TEXT("none"), Loops.Num(), HoseLen / 100.f);
}

bool AWashHose::Interact(APawn* Who, AActor* Aimed)
{
	if (!Who) return false;
	// (owner, 2026-11-14: you walk to the wall mount to take it, and back to it to hang it up)
	if (AWashHose* H = Held())
	{
		if (H->Holder.Get() != Who) return false;
		if (H->Phase != EPhase::Hold) return true;   // (mid-reach)
		if (FVector::Dist2D(Who->GetActorLocation(), H->Hook) < 150.f) { H->HangUp(); return true; }
		ADP_LOG(TEXT("Wash hose: E pressed away from the wall mount - walk back to it to hang the hose up"));
		return false;
	}
	for (const TWeakObjectPtr<AWashHose>& H : AllHoses)
	{
		if (!H.IsValid() || !H->GunIsm) continue;
		if (FVector::Dist2D(Who->GetActorLocation(), H->Hook) < 150.f) { H->PickUp(Who); return true; }
	}
	return false;
}

void AWashHose::PickUp(APawn* Who)
{
	// (owner, 2026-11-14: your arm reaches out and takes it - never popping into your hand)
	Holder = Who; Phase = EPhase::Reach; PhaseT = 0.f;
	if (AADPWalker* Wk = Cast<AADPWalker>(Who)) { Wk->SetLed(true); AddTickPrerequisiteActor(Wk); }
	if (!Hiss) MakeSound();
	ADP_LOG(TEXT("Wash hose: reaching for the gun"));
}

void AWashHose::HangUp()
{
	if (Phase != EPhase::Hold) return;
	Phase = EPhase::Back; PhaseT = 0.f; bSpray = false;
	if (AADPWalker* Wk = Cast<AADPWalker>(Holder.Get())) Wk->SetLed(true);
	ADP_LOG(TEXT("Wash hose: hanging the gun back up"));
}

void AWashHose::GunToHook(bool bOnHook)
{
	if (bGunOnHook == bOnHook) return;
	bGunOnHook = bOnHook;
	if (GunIsm && GunItem != INDEX_NONE)
	{
		FTransform T = GunHome; if (!bOnHook) T.SetScale3D(FVector(0.001f));
		GunIsm->UpdateInstanceTransform(GunItem, T, true, true, true);
	}
	Gun->SetVisibility(!bOnHook);
	Cable->SetVisibility(!bOnHook);
	if (bOnHook) ShowCoil(true); else PayOut(0.f);
}

void AWashHose::ShowCoil(bool bOn)
{
	for (FLoop& L : Loops)
	{
		if (!L.Ism || L.bShown == bOn) continue;
		L.bShown = bOn;
		FTransform T = L.Home; if (!bOn) T.SetScale3D(FVector(0.001f));
		L.Ism->UpdateInstanceTransform(L.Item, T, true, true, true);
	}
}

void AWashHose::PayOut(float Paid)
{
	// whole loops off the coil while that much hose is out; the hose leaves from the bottom of the next one still on
	float Off = 0.f;
	Leaves = Outlet;
	for (FLoop& L : Loops)
	{
		const bool bOn = Off >= Paid - 1.f || Off + L.Len * 0.5f > Paid;
		if (bOn && Leaves == Outlet) Leaves = L.Home.GetLocation() - FVector(0.f, 0.f, 28.75f * L.Len / FMath::Max(Loops.Last().Len, 1.f));   // (the bottom of its oval)
		Off += L.Len;
		if (L.bShown != bOn && L.Ism)
		{
			L.bShown = bOn;
			FTransform T = L.Home; if (!bOn) T.SetScale3D(FVector(0.001f));
			L.Ism->UpdateInstanceTransform(L.Item, T, true, true, true);
		}
	}
	Cable->SetWorldLocation(Leaves);
}

void AWashHose::MakeSound()
{
	HissLevel = MakeShared<float, ESPMode::ThreadSafe>(0.f);
	TSharedPtr<FHissState, ESPMode::ThreadSafe> St = MakeShared<FHissState, ESPMode::ThreadSafe>();
	TSharedPtr<float, ESPMode::ThreadSafe> Want = HissLevel;
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
	W->SetSampleRate(HoseRate); W->NumChannels = 1; W->Duration = INDEFINITELY_LOOPING_DURATION; W->bLooping = true;
	W->SoundGroup = SOUNDGROUP_Effects;
	W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([St, Want](USoundWaveProcedural* Wv, int32 Samples)
	{
		const int32 N = FMath::Max(Samples, HoseRate / 20);
		TArray<int16> Buf; Buf.SetNumUninitialized(N);
		FHissState& S = *St;
		for (int32 i = 0; i < N; ++i)
		{
			S.Level += (*Want - S.Level) * 0.0004f;
			const float Nz = HoseNoise(S);
			S.Lp1 += (Nz - S.Lp1) * 0.30f;            // the rush
			S.Lp2 += (Nz - S.Lp2) * 0.02f;
			const float Hi = Nz - S.Lp1;               // the hiss
			const float Crack = (HoseNoise(S) > 0.995f) ? HoseNoise(S) * 0.6f : 0.f;   // drops spattering
			const float V = (0.55f * S.Lp1 + 0.35f * Hi + 0.4f * S.Lp2 + Crack) * S.Level;
			Buf[i] = int16(FMath::Clamp(V * 14000.f, -32000.f, 32000.f));
		}
		Wv->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Buf.Num() * sizeof(int16));
	});
	USoundAttenuation* Att = NewObject<USoundAttenuation>(this);
	Att->Attenuation.bAttenuate = true; Att->Attenuation.bSpatialize = true;
	Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Att->Attenuation.AttenuationShapeExtents = FVector(150.f);
	Att->Attenuation.FalloffDistance = 3500.f;
	Hiss = UGameplayStatics::SpawnSoundAttached(W, Gun, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, 1.f, 1.f, 0.f, Att, nullptr, false);
}

void AWashHose::Tick(float Dt)
{
	Super::Tick(Dt);
	APawn* Who = Holder.Get();
	if (Holder.IsValid() && (!Who || Who->IsPendingKillPending() || !Who->GetController()))
	{
		GunToHook(true); Phase = EPhase::Hung; Holder.Reset(); Who = nullptr; bSpray = false;
	}
	FVector Tip = FVector::ZeroVector, Aim = FVector::ForwardVector;
	if (Who && Phase != EPhase::Hung)
	{
		PhaseT += Dt;
		AADPWalker* Wk = Cast<AADPWalker>(Who);
		// the hose reaches so far: no further from the tap than that
		FVector Me = Who->GetActorLocation();
		const FVector D(Me.X - Outlet.X, Me.Y - Outlet.Y, 0.f);
		const float MaxOut = FMath::Max((HoseLen - 150.f) / 1.12f, 300.f);
		if (D.Size() > MaxOut) { const FVector Back = FVector(Outlet.X, Outlet.Y, Me.Z) + D.GetSafeNormal() * MaxOut; Who->SetActorLocation(Back, true); Me = Who->GetActorLocation(); }

		// where the gun is held: out in front, at your right side, pointing where you look
		const FRotator Look = Who->GetControlRotation();
		Aim = Look.Vector();
		const FVector Flat = FRotator(0.f, Look.Yaw, 0.f).Vector(), Right = FRotationMatrix(FRotator(0.f, Look.Yaw, 0.f)).GetUnitAxis(EAxis::Y);
		FVector HoldAt;
		if (Wk && Wk->IsThirdPerson()) HoldAt = Me + Flat * 36.f + Right * 20.f + FVector(0, 0, 28.f);
		else
		{
			const FVector Up = FRotationMatrix(Look).GetUnitAxis(EAxis::Z), Side = FRotationMatrix(Look).GetUnitAxis(EAxis::Y);
			HoldAt = Who->GetPawnViewLocation() + Aim * 40.f + Side * 17.f - Up * 22.f;
		}
		// (the gun's own barrel runs along its -Y, the grip down its -Z: Art/WashHose/make_washhose.py)
		const FVector UpW = (FVector::UpVector - Aim * FVector::DotProduct(FVector::UpVector, Aim)).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		const FQuat HeldRot = FMatrix(FVector::CrossProduct(-Aim, UpW), -Aim, UpW, FVector::ZeroVector).ToQuat();
		const FVector AimRight = FVector::CrossProduct(UpW, Aim).GetSafeNormal();
		const FBox B = Gun->GetStaticMesh() ? Gun->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-1), FVector(1));
		// the grip in the gun's own units (it stands 19.2 cm tall: the grip's middle 2 cm behind, 8 cm under its origin)
		const float K = (B.Max.Z - B.Min.Z) / 19.2f;
		const FVector Grip(0.f, 2.f * K, -8.f * K);
		const FVector HookGrip = GunHome.TransformPosition(Grip);
		const FVector Shoulder = Wk ? Wk->ShoulderNow() : Me + FVector(0, 0, 55.f);

		// the phases: reach out to the hook (0.55 s), take the gun and bring it in (0.5 s); hanging up the same backwards
		constexpr float TReach = 0.55f, TBring = 0.5f;
		float Out = 0.f;        // 0 = the gun in your hold, 1 = on the hook
		float ArmW = 1.f;
		if (Phase == EPhase::Reach)
		{
			// turning to face the hook while you reach
			if (APlayerController* PC = Cast<APlayerController>(Who->GetController()))
			{
				FRotator R = PC->GetControlRotation();
				const float WantYaw = (Hook - Me).Rotation().Yaw;
				R.Yaw = FMath::FixedTurn(R.Yaw, WantYaw, 240.f * Dt);
				R.Pitch = FMath::FInterpTo(FRotator::NormalizeAxis(R.Pitch), -12.f, Dt, 6.f);
				PC->SetControlRotation(R);
			}
			ArmW = FMath::SmoothStep(0.f, TReach * 0.8f, PhaseT);
			if (PhaseT >= TReach) GunToHook(false);
			Out = PhaseT < TReach ? 1.f : 1.f - FMath::SmoothStep(TReach, TReach + TBring, PhaseT);
			if (PhaseT >= TReach + TBring)
			{
				Phase = EPhase::Hold; Out = 0.f;
				if (Wk) Wk->SetLed(false);
				ADP_LOG(TEXT("Wash hose: the gun in your hand"));
			}
		}
		else if (Phase == EPhase::Back)
		{
			Out = FMath::SmoothStep(0.f, TBring, PhaseT);
			if (PhaseT >= TBring) { GunToHook(true); Out = 1.f; }
			ArmW = PhaseT < TBring ? 1.f : 1.f - FMath::SmoothStep(TBring, TBring + TReach * 0.8f, PhaseT);
			if (PhaseT >= TBring + TReach)
			{
				Phase = EPhase::Hung; ArmW = 0.f;
				if (Wk) { Wk->SetArm(0.f, Shoulder, Aim, UpW); Wk->SetLed(false); }
				Holder.Reset();
				ADP_LOG(TEXT("Wash hose: hung back up"));
			}
		}
		if (Phase != EPhase::Hung)
		{
			// the hand's target: the grip on the hook, or where you hold it, and the way in between
			const FVector GripAt = FMath::Lerp(HoldAt, HookGrip, Out);
			const FVector ToHook = (HookGrip - Shoulder).GetSafeNormal();
			const FVector Fingers = (FMath::Lerp(Aim, ToHook, Out)).GetSafeNormal();
			const FVector Thumb = FMath::Lerp((UpW * 0.6f - AimRight * 0.5f + Aim * 0.2f), FVector::UpVector, Out);
			// the hand's wrist sits 5 cm back from the palm
			if (Wk) Wk->SetArm(ArmW, GripAt - Fingers * 5.f, Fingers, Thumb);
			// the gun: on its hook, or in the palm - where the hand really is, turned between hanging and pointing
			if (!bGunOnHook)
			{
				FTransform Palm;
				const FVector At = (Wk && Wk->PalmNow(Palm)) ? Palm.GetLocation() : GripAt;
				const FQuat Rot = FQuat::Slerp(HeldRot, GunHome.GetRotation(), Out);
				const FTransform G(Rot, FVector::ZeroVector, GunHome.GetScale3D());
				Gun->SetWorldTransform(FTransform(Rot, At - G.TransformVector(Grip), GunHome.GetScale3D()));
				Tip = Gun->GetComponentTransform().TransformPosition(FVector(0.f, B.Min.Y, B.GetCenter().Z));
				Aim = (Tip - Gun->GetComponentTransform().TransformPosition(FVector(0.f, B.Max.Y, B.GetCenter().Z))).GetSafeNormal();
				// as much hose out as reaches your hand with some slack on the ground; the rest stays on the coil
				const float Paid = FMath::Clamp(float(FVector::Dist(Outlet, At)) * 1.12f + 150.f, 0.f, HoseLen);
				PayOut(Paid);
				Cable->CableLength = float(FVector::Dist(Leaves, At)) * 1.05f + 60.f;
			}
		}
		// spraying: the left mouse button or the right trigger held, with the gun in your hand
		bool bDown = bTestSpray && Phase == EPhase::Hold;
		if (Phase == EPhase::Hold)
			if (APlayerController* PC = Cast<APlayerController>(Who->GetController()))
				bDown = bDown || PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) > 0.3f;
		if (bDown != bSpray) { bSpray = bDown; ADP_LOG(TEXT("Wash hose: spraying %s"), bSpray ? TEXT("on") : TEXT("off")); }
	}
	else bSpray = false;
	if (HissLevel.IsValid()) *HissLevel = bSpray ? 1.f : 0.f;
	Spray(Dt, Tip, Aim);
	// the wet patches drying
	for (int32 i = Wet.Num() - 1; i >= 0; --i)
	{
		WetAge[i] += Dt;
		const float A = 1.f - FMath::SmoothStep(0.f, 1.f, WetAge[i] / WetDries);
		if (WetMids[i]) WetMids[i]->SetScalarParameterValue(TEXT("Amount"), A * 0.85f);
		if (A <= 0.f) { if (Wet[i]) Wet[i]->DestroyComponent(); Wet.RemoveAt(i); WetMids.RemoveAt(i); WetAge.RemoveAt(i); }
	}
}

void AWashHose::Spray(float Dt, const FVector& Tip, const FVector& Aim)
{
	UWorld* W = GetWorld();
	if (!W) return;
	if (bSpray)
	{
		Emit += Dt * DropsPerSecond;
		const int32 NEm = int32(Emit); int32 Ek = 0;
		while (Emit >= 1.f)
		{
			Emit -= 1.f; ++Ek;
			const FVector Dir = FMath::VRandCone(Aim, FMath::DegreesToRadians(2.2f));
			FDrop Dp; Dp.V = Dir * JetSpeed * FMath::FRandRange(0.92f, 1.05f); Dp.P = Tip + Dp.V * (Dt * float(NEm - Ek) / FMath::Max(NEm, 1)); Dp.Life = 0.f; Dp.bSplash = false;   // (in the order let go: the older ones further out, so the jet joins up into one line)
			Live.Add(Dp);
		}
	}
	else Emit = 0.f;
	if (Live.Num() == 0) { if (Drops->GetInstanceCount() > 0) Drops->ClearInstances(); if (Mist->GetInstanceCount() > 0) Mist->ClearInstances(); return; }
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPWashHose), false, this);
	if (APawn* Who = Holder.Get())
	{
		Q.AddIgnoredActor(Who);
		if (AADPWalker* Wk = Cast<AADPWalker>(Who)) if (Wk->HumanActor()) Q.AddIgnoredActor(Wk->HumanActor());
	}
	TArray<FDrop> Splash;
	TArray<FTransform> Shown; Shown.Reserve(Live.Num());
	for (int32 i = Live.Num() - 1; i >= 0; --i)
	{
		FDrop& Dp = Live[i];
		Dp.Life += Dt;
		const FVector From = Dp.P;
		Dp.V.Z -= HoseGravity * Dt;
		Dp.P += Dp.V * Dt;
		bool bGone = Dp.Life > (Dp.bMist ? 0.9f : Dp.bSplash ? 0.35f : MaxDropLife);
		if (Dp.bMist) Dp.V *= FMath::Max(0.f, 1.f - 3.f * Dt);   // (the spray slows in the air and hangs)
		if (!Dp.bSplash && !bGone)
		{
			FHitResult H;
			if (W->LineTraceSingleByChannel(H, From, Dp.P, ECC_Visibility, Q))
			{
				bGone = true;
				// the jet washes a truck where it lands
				static float NextHitLog = 0.f; if (W->GetTimeSeconds() > NextHitLog) { NextHitLog = W->GetTimeSeconds() + 1.f; ADP_LOG(TEXT("Wash hose: jet lands on %s (%s) %.0f m away, %d drops in the air"), H.GetActor() ? *H.GetActor()->GetName() : TEXT("-"), H.GetComponent() ? *H.GetComponent()->GetName() : TEXT("-"), FVector::Dist(H.ImpactPoint, Tip) / 100.f, Live.Num()); }
				if (AFireTruck* T = AFireTruck::FromActor(H.GetActor())) T->Wash(H.ImpactPoint, 0.004f);
				if (H.ImpactNormal.Z > 0.6f) WetAt(H.ImpactPoint, H.ImpactNormal);
				// a puff of white spray where it lands (now and then), and splashes off it
				if (FMath::FRand() < 0.3f) { FDrop M; M.bSplash = true; M.bMist = true; M.Life = 0.f; M.Seed = FMath::FRand(); M.P = H.ImpactPoint + H.ImpactNormal * 6.f; M.V = H.ImpactNormal * 90.f + FMath::VRand() * 50.f + FVector(0, 0, 25.f); Splash.Add(M); }
				if (FMath::FRand() < 0.7f)
					for (int32 k = 0; k < 3; ++k)
					{
						FDrop S; S.bSplash = true; S.Life = 0.f; S.P = H.ImpactPoint + H.ImpactNormal * 2.f;
						S.V = (FMath::GetReflectionVector(Dp.V, H.ImpactNormal) * 0.18f + FMath::VRand() * 120.f + H.ImpactNormal * 110.f);
						Splash.Add(S);
					}
			}
		}
		if (bGone) Dp.Life = -1.f;
	}
	Live.RemoveAll([](const FDrop& D) { return D.Life < 0.f; });   // (in order: the jet is drawn drop to drop)
	Live.Append(Splash);
	// the jet as one stream: each drop joined to the one let go just before it (a fast thin drop on its own is lost in the
	// picture's smoothing), widening a little as it flies; drops that have parted from the rest, and the splashes, alone
	const FDrop* Prev = nullptr;
	TArray<float> Data; Data.Reserve(Live.Num() * 2);
	TArray<FTransform> MistShown; TArray<float> MistData;
	FVector EyeAt = Tip;
	if (APlayerController* PCm = W->GetFirstPlayerController()) if (PCm->PlayerCameraManager) EyeAt = PCm->PlayerCameraManager->GetCameraLocation();
	for (const FDrop& Dp : Live)
	{
		if (Dp.bMist)
		{
			// the spray cloud: a card turned to your eye, growing as it spreads, thinning out
			const float K = Dp.Life / 0.9f;
			const FVector ToEye = (EyeAt - Dp.P).GetSafeNormal();
			const FQuat MQ = FQuat(ToEye, Dp.Seed * 2.f * PI) * FRotationMatrix::MakeFromZ(ToEye).ToQuat();
			MistShown.Add(FTransform(MQ, Dp.P, FVector(0.25f + 0.55f * FMath::Sqrt(K))));
			MistData.Add(Dp.Seed); MistData.Add(0.55f * FMath::Square(1.f - K) * FMath::Min(1.f, Dp.Life * 12.f));
			continue;
		}
		if (Dp.bSplash) { Shown.Add(FTransform(FRotator::ZeroRotator, Dp.P, FVector(0.012f))); Data.Add(1.f); Data.Add(1.f - Dp.Life / 0.35f); continue; }
		// the jet: clear and tight by the nozzle, whiter and wider as it breaks up on its way
		const float Wd = (2.0f + Dp.Life * 3.f) / 100.f;
		const float White = FMath::Clamp(0.45f + Dp.Life * 1.4f, 0.f, 0.9f);
		if (Prev && FVector::Dist(Prev->P, Dp.P) < 70.f)
		{
			const FVector D = Prev->P - Dp.P;
			Shown.Add(FTransform(D.Rotation(), (Prev->P + Dp.P) * 0.5f, FVector((D.Size() + 2.f) / 100.f, Wd, Wd)));
		}
		else Shown.Add(FTransform(Dp.V.Rotation(), Dp.P, FVector(0.05f, Wd, Wd)));
		Data.Add(White); Data.Add(1.f);
		Prev = &Dp;
	}
	Drops->ClearInstances();
	if (Shown.Num())
	{
		Drops->AddInstances(Shown, false, true, false);
		for (int32 i = 0; i < Shown.Num(); ++i) Drops->SetCustomData(i, TArrayView<const float>(&Data[i * 2], 2), false);
		Drops->MarkRenderStateDirty();
	}
	Mist->ClearInstances();
	if (MistShown.Num())
	{
		Mist->AddInstances(MistShown, false, true, false);
		for (int32 i = 0; i < MistShown.Num(); ++i) Mist->SetCustomData(i, TArrayView<const float>(&MistData[i * 2], 2), false);
		Mist->MarkRenderStateDirty();
	}
}

void AWashHose::WetAt(const FVector& At, const FVector& Normal)
{
	// one patch per spot: a fresh hit near an old one wets it again
	for (int32 i = 0; i < Wet.Num(); ++i)
		if (Wet[i] && FVector::Dist(Wet[i]->GetComponentLocation(), At) < 70.f) { WetAge[i] = FMath::Min(WetAge[i], 0.f); return; }
	WetClock -= 1.f;
	if (Wet.Num() >= MaxWet)
	{
		int32 Old = 0; for (int32 i = 1; i < Wet.Num(); ++i) if (WetAge[i] > WetAge[Old]) Old = i;
		if (Wet[Old]) Wet[Old]->DestroyComponent();
		Wet.RemoveAt(Old); WetMids.RemoveAt(Old); WetAge.RemoveAt(Old);
	}
	static UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_WetPatch.M_WetPatch"));
	if (!Base) return;
	UDecalComponent* Dc = NewObject<UDecalComponent>(this);
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetScalarParameterValue(TEXT("Amount"), 0.85f);
	Dc->SetDecalMaterial(M);
	const float R = FMath::FRandRange(55.f, 95.f);
	Dc->DecalSize = FVector(40.f, R, R);
	Dc->SetUsingAbsoluteLocation(true); Dc->SetUsingAbsoluteRotation(true);
	Dc->SetupAttachment(RootComponent);
	Dc->RegisterComponent();
	Dc->SetWorldLocationAndRotation(At, FRotationMatrix::MakeFromXZ(-Normal, FVector::ForwardVector).Rotator() + FRotator(0.f, 0.f, FMath::FRandRange(0.f, 360.f)));
	Wet.Add(Dc); WetMids.Add(M); WetAge.Add(0.f);
}
