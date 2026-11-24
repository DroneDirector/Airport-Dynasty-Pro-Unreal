// The crash tender's cab worked by hand (to-do 238; FireTruck.h): from the driver's seat you look at a switch, a stalk or
// the turret joystick, E / X presses it, the hand nearest it goes there and back to the wheel, the part moves, and the
// same state changes as with the keys (the keys stay). The wipers, the work lights, the pump and foam, and the roof and
// bumper turrets: aimed by the joystick, their jet drawn as the wash hose's is, the tanks running down at the real flow
// and filled again standing at the station.
//
// Real figures (Rosenbauer Panther 6x6, the truck this one is modelled on - Wikipedia "Rosenbauer Panther", its table from
// Rosenbauer's own data): 11,400 L water, 1,400 L foam concentrate, 9,000 L/min from the pump. The turrets: a roof turret
// of 6,000 L/min throwing about 85 m and a bumper turret of 1,500 L/min throwing about 50 m (the RM60 / RM15 class); both
// powered, about 20 deg/s round and 15 deg/s up and down, 90 deg either side, 20 deg down to 45 deg up (NFPA 414's
// minimum arcs). Foam mixed at 3 %. Filling at the station from the hydrant: 2,000 L/min water, 200 L/min foam by its
// transfer pump. (The turret figures and fill rates are the class's usual ones, not checked against one data sheet.)
#include "FireTruck.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "BabylonModel.h"
#include "VehicleControls.h"
#include "VehicleDirt.h"
#include "ADPKeep.h"
#include "UI/InputGlyphs.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	FVector B2R(float X, float Y, float Z) { return FVector(X, -Y, Z) * 100.f; }   // (as FireTruck.cpp: Blender metres to root cm)
	constexpr float WaterFull = 11400.f, FoamFull = 1400.f, FoamPart = 0.03f, FillWater = 2000.f, FillFoam = 200.f;
	constexpr float JetDrag = 0.12f, JetG = 981.f, JetPerSecond = 320.f, JetLife = 4.f;
	constexpr int32 JetMaxWet = 160; constexpr float JetDries = 180.f;   // (as the wash hose's patches)
	// the jet's speed at the nozzle that carries it Throw (cm) at its best angle, with the air's drag as drawn
	float SpeedFor(float Throw)
	{
		auto Range = [](float V)
		{
			const float A = FMath::DegreesToRadians(32.f); FVector2D P(0, 0), U(V * FMath::Cos(A), V * FMath::Sin(A));
			for (int32 i = 0; i < 2000 && P.Y >= 0.f; ++i) { U *= 1.f - JetDrag * 0.02f; U.Y -= JetG * 0.02f; P += U * 0.02f; }
			return P.X;
		};
		float Lo = 500.f, Hi = 15000.f;
		for (int32 i = 0; i < 30; ++i) { const float M = 0.5f * (Lo + Hi); (Range(M) < Throw ? Lo : Hi) = M; }
		return 0.5f * (Lo + Hi);
	}
}

void AFireTruck::SetupYou()
{
	// the cab's parts (Art/FireTruck/make_panther.py), each its own node; where one is missing its press point is kept
	// where the model script puts it, without movement, and logged
	struct FDef { ECab K; const TCHAR* Name; const TCHAR* Extra; FVector Blender; };
	const FDef Defs[] = {
		{ECab::Lights, TEXT("Switch_Lights"), TEXT("Switch_LightsPointer"), FVector(-0.51f, 5.035f, 1.38f)},
		{ECab::Hazard, TEXT("Switch_Hazard"), nullptr, FVector(0.45f, 5.045f, 1.48f)},
		{ECab::Beacons, TEXT("Switch_Beacons"), nullptr, FVector(0.4525f, 5.04f, 1.37f)},
		{ECab::Siren, TEXT("Switch_Siren"), nullptr, FVector(0.5175f, 5.04f, 1.37f)},
		{ECab::Work, TEXT("Switch_WorkLights"), nullptr, FVector(0.5825f, 5.04f, 1.37f)},
		{ECab::Pump, TEXT("Switch_Pump"), nullptr, FVector(0.6475f, 5.04f, 1.37f)},
		{ECab::Foam, TEXT("Switch_Foam"), nullptr, FVector(0.7125f, 5.04f, 1.37f)},
		{ECab::Indicator, TEXT("Stalk_Indicator"), nullptr, FVector(-0.2f, 4.7f, 1.70f)},
		{ECab::Wiper, TEXT("Stalk_Wiper"), TEXT("Stalk_WiperKnob"), FVector(0.2f, 4.7f, 1.70f)},
		{ECab::Joystick, TEXT("Joystick"), nullptr, FVector(0.33f, 4.32f, 1.84f)}};
	FString Missing; int32 Own = 0;
	for (const FDef& D : Defs)
	{
		FCabPart C; C.Kind = D.K;
		FTransform B;
		if (Model->NodeBase(D.Name, B))
		{
			C.bPart = true; C.Base = B; C.At = B.GetLocation();
			// (the indicator stalk is moved by the indicators themselves, FireTruck.cpp; its knob end is where the hand goes)
			if (D.K == ECab::Indicator) { C.At = StalkTip.IsNearlyZero() ? B.GetLocation() : StalkTip; }
			else if (D.K == ECab::Wiper) { C.Base = FTransform(B.GetRotation(), B2R(0.f, 4.68f, 1.731f)); TArray<FString> N = {FString(D.Name), FString(D.Extra)}; C.Group = Model->BindGroup(N, false, C.Base); GWiperStalk = C.Group; }
			else { TArray<FString> N = {FString(D.Name)}; if (D.Extra) N.Add(FString(D.Extra)); C.Group = Model->BindGroup(N, false, B); }
			++Own;
		}
		else { C.At = B2R(D.Blender.X, D.Blender.Y, D.Blender.Z); Missing += FString(D.Name) + TEXT(" "); }
		Cab.Add(C);
	}
	// the wipers: each turns about its spindle, about the windscreen's normal there (the model script's own frame)
	WiperAxis = FVector(0.f, -(2.98f - 1.62f), 0.85f).GetSafeNormal();
	WiperSign = FQuat(WiperAxis, 0.1f).RotateVector(FVector(1, 0, 0)).Z > 0.f ? 1.f : -1.f;
	for (int32 i = 0; i < 2; ++i)
	{
		const FString N = i ? TEXT("Wiper_R") : TEXT("Wiper_L");
		if (Model->NodeBase(N, BWiper[i])) GWiper[i] = Model->BindGroup(TArray<FString>{N}, false, BWiper[i]); else Missing += N + TEXT(" ");
	}
	// the turrets: the body turns about its middle, the nozzle tips up and down about its root
	PitchSign = FQuat(FVector(1, 0, 0), 0.1f).RotateVector(FVector(0, -1, 0)).Z > 0.f ? 1.f : -1.f;
	const TCHAR* TBody[2] = {TEXT("RoofTurretBody"), TEXT("BumperTurretBody")};
	const TCHAR* TNoz[2] = {TEXT("RoofNozzle"), TEXT("BumperNozzle")};
	const float Lens[2] = {50.f, 30.f}, Lpm[2] = {6000.f, 1500.f}, Throw[2] = {8500.f, 5000.f};
	const FVector Fall[2] = {B2R(0.f, 4.96f, 3.32f), B2R(-0.62f, 6.07f, 1.77f)};
	for (int32 i = 0; i < 2; ++i)
	{
		FTurret& T = Tur[i];
		T.Len = Lens[i]; T.Lpm = Lpm[i]; T.Throw = Throw[i]; T.Speed = SpeedFor(Throw[i]);
		FTransform B, Nz;
		const bool bBody = Model->NodeBase(TBody[i], B), bNoz = Model->NodeBase(TNoz[i], Nz);
		T.BodyAt = bBody ? B.GetLocation() : Fall[i];
		T.NozzleBase = (bNoz ? Nz.GetLocation() : Fall[i]) + FVector(0.f, T.Len * 0.5f, 0.f);   // (the nozzle's centre less half its length, back along root +y)
		if (bBody) { T.BBody = FTransform(T.BodyAt); T.GBody = Model->BindGroup(TArray<FString>{FString(TBody[i])}, false, T.BBody); } else Missing += FString(TBody[i]) + TEXT(" ");
		if (bNoz) { T.BNozzle = FTransform(T.NozzleBase); T.GNozzle = Model->BindGroup(TArray<FString>{FString(TNoz[i])}, false, T.BNozzle); } else Missing += FString(TNoz[i]) + TEXT(" ");
		T.bTurns = T.GBody >= 0 && T.GNozzle >= 0;
		T.Slew = 20.f;
	}
	// the work lights (scene lights): on the light columns behind the cab and at the rear, either side, shining out and down
	// onto the ground beside the truck; through the lamp list like every other lamp (ELamp::Work), no lens glow
	for (const float Sd : {-1.f, 1.f})
		for (const FVector& At : {FVector(1.55f, 2.21f, 3.0f), FVector(1.55f, -4.53f, 3.25f)})
		{
			USpotLightComponent* Sp = NewObject<USpotLightComponent>(this);
			Sp->SetupAttachment(Model->GetModelRoot());
			Sp->SetRelativeLocationAndRotation(B2R(Sd * (At.X + 0.1f), At.Y, At.Z), FRotator(-40.f, Sd > 0.f ? 0.f : 180.f, 0.f));
			Sp->SetIntensityUnits(ELightUnits::Lumens); Sp->SetIntensity(4500.f);   // (an LED scene lamp of about 4,500 lm)
			Sp->SetOuterConeAngle(60.f); Sp->SetInnerConeAngle(35.f); Sp->SetAttenuationRadius(2500.f);
			Sp->SetLightColor(FLinearColor(1.f, 0.96f, 0.9f));
			Sp->SetCastShadows(false); Sp->SetVolumetricScatteringIntensity(0.f);
			Sp->MaxDrawDistance = 20000.f;
			Sp->SetVisibility(false);
			Sp->RegisterComponent();
			Lamps.Add(Sp); LampKind.Add(ELamp::Work); LampSide.Add(uint8(Sd > 0.f)); LampFull.Add(1.f);
		}
	// the jet: the wash hose's own water drops and spray clouds (WashHose.cpp)
	auto Ism = [this](const TCHAR* Name, const TCHAR* Mesh, const TCHAR* Mat)
	{
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this, Name);
		K->SetupAttachment(GetRootComponent());
		K->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Mesh));
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Mat)) K->SetMaterial(0, M);
		K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		K->SetCastShadow(false);
		K->SetNumCustomDataFloats(2);
		K->SetUsingAbsoluteLocation(true); K->SetUsingAbsoluteRotation(true); K->SetUsingAbsoluteScale(true);
		K->SetWorldTransform(FTransform::Identity);
		K->RegisterComponent();
		return K;
	};
	JetDrops = Ism(TEXT("TurretWater"), TEXT("/Engine/BasicShapes/Sphere.Sphere"), TEXT("/Game/Truck/M_WaterDrop.M_WaterDrop"));
	JetMist = Ism(TEXT("TurretSpray"), TEXT("/Engine/BasicShapes/Plane.Plane"), TEXT("/Game/Truck/M_WaterMist.M_WaterMist"));
	ADP_LOG(TEXT("Fire truck: cab worked by hand - %d of %d controls their own parts, wipers %d/%d, turrets roof %s bumper %s (jet %.0f / %.0f m/s); missing: %s"),
		Own, Cab.Num(), GWiper[0], GWiper[1], Tur[0].bTurns ? TEXT("turns") : TEXT("fixed"), Tur[1].bTurns ? TEXT("turns") : TEXT("fixed"), Tur[0].Speed / 100.f, Tur[1].Speed / 100.f, Missing.IsEmpty() ? TEXT("none") : *Missing);
}

FTransform AFireTruck::NozzleNow(int32 I) const
{
	// the nozzle's root (root space) and its way: yawed about the body's middle, tipped about its own root
	const FTurret& T = Tur[I];
	const FQuat Qy(FVector::UpVector, FMath::DegreesToRadians(T.Yaw)), Qp(FVector(1, 0, 0), FMath::DegreesToRadians(PitchSign * T.Pitch));
	return FTransform(Qy * Qp, T.BodyAt + Qy.RotateVector(T.NozzleBase - T.BodyAt));
}

bool AFireTruck::CabMay(ECab K, FString& Why) const
{
	const bool bEngine = Driver && Seq == ESeq::None;
	switch (K)
	{
	case ECab::Pump: if (!bEngine && !bPump) { Why = TEXT("The engine is off"); return false; } break;
	case ECab::Foam:
		if (!bFoam && !bPump) { Why = TEXT("The pump is off - switch the pump on first"); return false; }
		if (!bFoam && Foam <= 1.f) { Why = TEXT("The foam tank is empty - fill it at the fire station"); return false; }
		break;
	case ECab::Joystick:
		if (AimS.bOn) break;
		if (bChase) { Why = TEXT("The turrets are worked from the cab - press V for the cab view"); return false; }
		if (!bPump) { Why = TEXT("The pump is off - switch the pump on first"); return false; }
		if (Water <= 1.f) { Why = TEXT("The water tank is empty - fill it at the fire station"); return false; }
		break;
	default: break;
	}
	return true;
}

FString AFireTruck::CabSay(ECab K) const
{
	auto OnOff = [](bool b) { return b ? TEXT("on - switch off") : TEXT("off - switch on"); };
	switch (K)
	{
	case ECab::Lights: return bHeadOn ? TEXT("Light switch: headlights on - turn off") : TEXT("Light switch: off - turn the headlights on");
	case ECab::Hazard: return FString(TEXT("Hazard lights: ")) + OnOff(Signal == ETurn::Hazard);
	case ECab::Beacons: return FString(TEXT("Blue lights: ")) + OnOff(bBlue);
	case ECab::Siren: return FString(TEXT("Siren: ")) + OnOff(bSiren);
	case ECab::Work: return FString(TEXT("Work lights: ")) + OnOff(bWork);
	case ECab::Pump: return FString::Printf(TEXT("Pump: %s (water %.0f L)"), OnOff(bPump), Water);
	case ECab::Foam: return FString::Printf(TEXT("Foam: %s (foam %.0f L)"), OnOff(bFoam), Foam);
	case ECab::Indicator: return Signal == ETurn::Left ? TEXT("Indicator stalk: left - right") : Signal == ETurn::Right ? TEXT("Indicator stalk: right - off") : TEXT("Indicator stalk: off - left");
	case ECab::Wiper: return WiperMode == 0 ? TEXT("Wiper stalk: off - slow") : WiperMode == 1 ? TEXT("Wiper stalk: slow - fast") : TEXT("Wiper stalk: fast - off");
	case ECab::Joystick: return AimS.bOn ? TEXT("Turret joystick - let go") : TEXT("Turret joystick - take it");
	default: return FString();
	}
}

FString AFireTruck::CabPrompt() const
{
	const TCHAR* Key = ADPInput::UsingPad() ? TEXT("X") : TEXT("E");
	if (GetWorld() && CabNoteT > GetWorld()->GetTimeSeconds() - 1.5f && !CabNote.IsEmpty()) return CabNote;
	if (AimS.bOn)
	{
		const bool bPad = ADPInput::UsingPad();
		return FString::Printf(TEXT("%s turret: %s aims, %s sprays, %s the other turret, %s lets go   |   water %.0f L, foam %.0f L%s"),
			AimS.Which == 0 ? TEXT("Roof") : TEXT("Bumper"), bPad ? TEXT("right stick") : TEXT("the mouse"), bPad ? TEXT("RB held") : TEXT("left button held"),
			bPad ? TEXT("LB") : TEXT("right button"), Key, Water, Foam, bFoam ? TEXT(", foam on") : TEXT(""));
	}
	if (!Cab.IsValidIndex(CabLook)) return FString();
	const FCabPart& C = Cab[CabLook];
	FString Why;
	const FString S = CabSay(C.Kind) + (C.bPart ? TEXT("") : TEXT(" (no part of its own)"));
	return CabMay(C.Kind, Why) ? FString::Printf(TEXT("%s   %s"), Key, *S) : FString::Printf(TEXT("%s: %s"), *S, *Why);
}

bool AFireTruck::CabPress()
{
	if (!Driver || Seq != ESeq::None) return false;
	if (AimS.bOn && Cab.IsValidIndex(CabDo) && Cab[CabDo].Kind == ECab::Joystick && CabDone) { CabT = 0.36f; CabDone = false; return true; }   // (letting go of the joystick: the hand already on it)
	if (CabDo >= 0) return true;   // (the hand is on its way)
	int32 I = CabLook;
	if (AimS.bOn) for (int32 k = 0; k < Cab.Num(); ++k) if (Cab[k].Kind == ECab::Joystick) I = k;
	if (!Cab.IsValidIndex(I)) return false;
	FString Why;
	if (!CabMay(Cab[I].Kind, Why)) { AVehicleControls::Note(GetWorld(), Why); CabNote = Why; CabNoteT = GetWorld()->GetTimeSeconds(); ADP_LOG(TEXT("Fire truck: %s refused - %s"), *CabSay(Cab[I].Kind), *Why); return true; }
	CabDo = I; CabT = 0.f; CabDone = false;
	return true;
}

void AFireTruck::CabApply(ECab K)
{
	switch (K)
	{
	case ECab::Lights: bHeadOn = !bHeadOn; break;
	case ECab::Hazard: SetTurn(ETurn::Hazard); break;
	case ECab::Beacons: bBlue = !bBlue; BlueClock = 0.f; break;
	case ECab::Siren: { const bool bWas = bPadMod; bPadMod = false; ToggleSiren(); bPadMod = bWas; } break;
	case ECab::Work: bWork = !bWork; break;
	case ECab::Pump: bPump = !bPump; if (!bPump) { bFoam = false; AimS.bSpray = false; } break;
	case ECab::Foam: bFoam = !bFoam; break;
	case ECab::Indicator: SetTurn(Signal == ETurn::Left ? ETurn::Right : Signal == ETurn::Right ? ETurn::Right : ETurn::Left); break;
	case ECab::Wiper: WiperMode = uint8((WiperMode + 1) % 3); break;
	case ECab::Joystick:
		AimS.bOn = !AimS.bOn; AimS.bSpray = false;
		if (AimS.bOn) { DriverCam->SetActive(false); MoveCam->SetActive(true); }
		else { MoveCam->SetActive(false); SetView(false); }
		if (APlayerController* PC = Cast<APlayerController>(GetController())) if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
		break;
	default: break;
	}
	ADP_LOG(TEXT("Fire truck: by hand - %s"), *CabSay(K));
}

void AFireTruck::CabHand(FDriverPose& P) const
{
	// the hand nearest the part goes to it and back to the wheel: its wrist a little short of the part, toward you
	if (!Cab.IsValidIndex(CabDo) || Seq != ESeq::None) return;
	const FCabPart& C = Cab[CabDo];
	const bool bStick = C.Kind == ECab::Joystick;
	float W;
	if (bStick && AimS.bOn && CabT > 0.35f) W = 1.f;
	else W = CabT < 0.35f ? FMath::SmoothStep(0.f, 1.f, CabT / 0.35f) : CabT < 0.5f ? 1.f : 1.f - FMath::SmoothStep(0.f, 1.f, (CabT - 0.5f) / 0.35f);
	const int32 S = C.At.X < -5.f ? 0 : 1;
	FVector At = C.At;
	if (bStick)
	{
		// the joystick's grip, tilted as you aim
		const FTurret& T = Tur[AimS.Which];
		At += FVector(0, 0, 8.f) + FVector(T.Yaw / FMath::Max(T.YawMax, 1.f) * 2.f, -T.Pitch / 45.f * 2.f, 0.f);
	}
	const FVector Wrist = At + (bStick ? FVector(0.f, 5.f, -1.f) : FVector(0.f, 9.f, -2.f));
	P.Hand[S] = FMath::Lerp(P.Hand[S], Wrist, W);
	P.Grip[S] = FMath::Lerp(P.Grip[S], bStick ? 1.f : 0.25f, W);
	if (!bStick) { P.LookHand = int8(S); P.LookW = 0.6f * W; }
}

void AFireTruck::TickYou(float Dt)
{
	ADP_PERF_SCOPE("the fire truck worked by hand (each frame)", 2.f);
	if (!Model) return;
	UWorld* W = GetWorld();
	APlayerController* PC = Cast<APlayerController>(GetController());
	const bool bSeated = Driver && Seq == ESeq::None;
	if (!bSeated)
	{
		CabLook = -1; CabDo = -1;
		if (AimS.bOn) { AimS.bOn = false; AimS.bSpray = false; MoveCam->SetActive(false); }
		if (!Driver && bPump) { bPump = false; bFoam = false; ADP_LOG(TEXT("Fire truck: engine off - the pump stops")); }
	}
	// what you look at: the part nearest the middle of your view, within 6 degrees and 2 m
	else if (CabDo < 0 && !bChase && !AimS.bOn && PC && PC->PlayerCameraManager)
	{
		const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
		const FVector Eye = PC->PlayerCameraManager->GetCameraLocation(), Fw = PC->PlayerCameraManager->GetCameraRotation().Vector();
		float Best = FMath::Cos(FMath::DegreesToRadians(6.f)); CabLook = -1;
		for (int32 i = 0; i < Cab.Num(); ++i)
		{
			const FVector D = RX.TransformPosition(Cab[i].At) - Eye;
			if (D.Size() > 200.f) continue;
			const float C = FVector::DotProduct(D.GetSafeNormal(), Fw);
			if (C > Best) { Best = C; CabLook = i; }
		}
	}
	// a press under way: the part changes as the hand gets there, the hand goes back
	if (Cab.IsValidIndex(CabDo))
	{
		CabT += Dt;
		if (!CabDone && CabT >= 0.35f) { CabDone = true; CabApply(Cab[CabDo].Kind); }
		const bool bHold = Cab[CabDo].Kind == ECab::Joystick && AimS.bOn;
		if (!bHold && CabT >= 0.85f) CabDo = -1;
	}
	else if (AimS.bOn) { for (int32 k = 0; k < Cab.Num(); ++k) if (Cab[k].Kind == ECab::Joystick) { CabDo = k; CabT = 0.4f; CabDone = true; } }
	// the parts as they stand: the rockers tipped on or off, the light knob turned, the hazard button pressed while touched,
	// the wiper stalk up a notch per speed, the joystick tilted as you aim
	for (int32 i = 0; i < Cab.Num(); ++i)
	{
		const FCabPart& C = Cab[i];
		if (C.Group < 0) continue;
		const bool bTouch = i == CabDo && CabT > 0.3f && CabT < 0.55f;
		FTransform T = C.Base;
		auto Tip = [&](const FVector& Axis, float Deg) { T = FTransform(FQuat(Axis, FMath::DegreesToRadians(Deg)) * C.Base.GetRotation(), C.Base.GetLocation()); };
		switch (C.Kind)
		{
		case ECab::Lights: Tip(FVector(0, 1, 0), bHeadOn ? 0.f : -45.f); break;
		case ECab::Hazard: T.AddToTranslation(FVector(0.f, bTouch || Signal == ETurn::Hazard ? -0.4f : 0.f, 0.f)); break;
		case ECab::Beacons: Tip(FVector(1, 0, 0), bBlue ? 10.f : -10.f); break;
		case ECab::Siren: Tip(FVector(1, 0, 0), bSiren ? 10.f : -10.f); break;
		case ECab::Work: Tip(FVector(1, 0, 0), bWork ? 10.f : -10.f); break;
		case ECab::Pump: Tip(FVector(1, 0, 0), bPump ? 10.f : -10.f); break;
		case ECab::Foam: Tip(FVector(1, 0, 0), bFoam ? 10.f : -10.f); break;
		case ECab::Wiper: T = FTransform(FQuat(FVector(0, 1, 0), FMath::DegreesToRadians(-6.f * WiperMode)), C.Base.GetLocation()); break;
		case ECab::Joystick:
		{
			const FTurret& Tt = Tur[AimS.Which];
			const float Ix = AimS.bOn ? Tt.Yaw / FMath::Max(Tt.YawMax, 1.f) * 12.f : 0.f, Iy = AimS.bOn ? Tt.Pitch / 45.f * 12.f : 0.f;
			const FVector Foot = C.Base.GetLocation() - FVector(0, 0, 8.f);
			const FQuat Q = FQuat(FVector(0, 1, 0), FMath::DegreesToRadians(Ix)) * FQuat(FVector(1, 0, 0), FMath::DegreesToRadians(Iy));
			T = FTransform(Q * C.Base.GetRotation(), Foot + Q.RotateVector(C.Base.GetLocation() - Foot));
			break;
		}
		default: break;
		}
		Model->MoveGroup(C.Group, T);
	}
	// the wipers: slow about 40 sweeps a minute, fast about 65; switched off they finish the sweep and park
	if (WiperMode > 0 || WiperPhase > 0.f)
	{
		const float Period = WiperMode == 2 ? 0.92f : 1.5f, Was = WiperPhase;
		WiperPhase += Dt / Period;
		if (WiperPhase >= 1.f) WiperPhase = WiperMode > 0 ? FMath::Fmod(WiperPhase, 1.f) : 0.f;
		if (WiperMode == 0 && WiperPhase < Was) WiperPhase = 0.f;
		const float Ang = 100.f * (0.5f - 0.5f * FMath::Cos(2.f * PI * WiperPhase));
		for (int32 i = 0; i < 2; ++i)
			if (GWiper[i] >= 0) Model->MoveGroup(GWiper[i], FTransform(FQuat(WiperAxis, FMath::DegreesToRadians(WiperSign * Ang)) * BWiper[i].GetRotation(), BWiper[i].GetLocation()));
	}
	if (AimS.bOn) AimTurret(Dt);
	// the turrets' bodies and nozzles where they point
	for (int32 i = 0; i < 2; ++i)
	{
		const FTurret& T = Tur[i];
		if (T.GBody >= 0) Model->MoveGroup(T.GBody, FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(T.Yaw)), T.BodyAt));
		if (T.GNozzle >= 0) Model->MoveGroup(T.GNozzle, NozzleNow(i));
	}
	Model->FlushMoves();
	// the tanks filled again standing at the station (from its hydrant), not while spraying
	if (!AimS.bSpray && SpeedKmh() < 1.f && (Water < WaterFull || Foam < FoamFull) && AFireStationDoors::InReach(GetActorLocation()))
	{
		Water = FMath::Min(WaterFull, Water + FillWater / 60.f * Dt);
		Foam = FMath::Min(FoamFull, Foam + FillFoam / 60.f * Dt);
		if (W && W->GetTimeSeconds() > RefillLog) { RefillLog = W->GetTimeSeconds() + 10.f; ADP_LOG(TEXT("Fire truck: filling at the station - water %.0f L, foam %.0f L"), Water, Foam); }
	}
	SprayJet(Dt);
	// the wet patches drying (as the wash hose's)
	for (int32 i = JetWet.Num() - 1; i >= 0; --i)
	{
		JetWetAge[i] += Dt;
		const float A = 1.f - FMath::SmoothStep(0.f, 1.f, JetWetAge[i] / JetDries);
		if (JetWetMids[i]) JetWetMids[i]->SetScalarParameterValue(TEXT("Amount"), A * 0.85f);
		if (A <= 0.f) { if (JetWet[i]) JetWet[i]->DestroyComponent(); JetWet.RemoveAt(i); JetWetMids.RemoveAt(i); JetWetAge.RemoveAt(i); }
	}
}

void AFireTruck::AimTurret(float Dt)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	// the other turret: the right mouse button / LB
	if (PC->WasInputKeyJustPressed(EKeys::RightMouseButton) || PC->WasInputKeyJustPressed(EKeys::Gamepad_LeftShoulder)) { AimS.Which = 1 - AimS.Which; ADP_LOG(TEXT("Fire truck: the %s turret"), AimS.Which ? TEXT("bumper") : TEXT("roof")); }
	FTurret& T = Tur[AimS.Which];
	// a powered turret: the stick asks for a speed, up to its slew rate
	float Mx = 0.f, My = 0.f; PC->GetInputMouseDelta(Mx, My);
	const float Ix = FMath::Clamp(Mx * 0.4f + PC->GetInputAnalogKeyState(EKeys::Gamepad_RightX), -1.f, 1.f);
	const float Iy = FMath::Clamp(My * 0.4f + PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY), -1.f, 1.f);
	T.Yaw = FMath::Clamp(T.Yaw + Ix * T.Slew * Dt, -T.YawMax, T.YawMax);
	T.Pitch = FMath::Clamp(T.Pitch + Iy * T.Slew * 0.75f * Dt, T.PitchMin, T.PitchMax);
	// spraying: held; refused with its reason
	const bool bWant = PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::Gamepad_RightShoulder);
	FString Why;
	if (bWant && !bPump) Why = TEXT("The pump is off - switch the pump on first");
	else if (bWant && Water <= 1.f) Why = TEXT("The water tank is empty - fill it at the fire station");
	if (!Why.IsEmpty() && GetWorld() && GetWorld()->GetTimeSeconds() - CabNoteT > 1.5f) { CabNote = Why; CabNoteT = GetWorld()->GetTimeSeconds(); ADP_LOG(TEXT("Fire truck: no jet - %s"), *Why); }
	const bool bNow = bWant && Why.IsEmpty();
	if (bNow != AimS.bSpray) { AimS.bSpray = bNow; ADP_LOG(TEXT("Fire truck: %s turret %s (water %.0f L, foam %.0f L)"), AimS.Which ? TEXT("bumper") : TEXT("roof"), bNow ? TEXT("spraying") : TEXT("stopped"), Water, Foam); }
	// the tanks run down at the turret's flow; foam at 3 % of it
	if (AimS.bSpray)
	{
		const float L = T.Lpm / 60.f * Dt;
		if (bFoam && Foam > 0.f) { Foam = FMath::Max(0.f, Foam - L * FoamPart); Water = FMath::Max(0.f, Water - L * (1.f - FoamPart)); if (Foam <= 0.f) { bFoam = false; CabNote = TEXT("The foam tank is empty - water only"); CabNoteT = GetWorld()->GetTimeSeconds(); } }
		else Water = FMath::Max(0.f, Water - L);
	}
	// the joystick's view: behind the roof turret over the cab, or from the driver's eyes for the bumper turret
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	const FTransform N = NozzleNow(AimS.Which);
	const FVector Dir = RX.TransformVectorNoScale(N.GetRotation().RotateVector(FVector(0, -1, 0)));
	const FVector From = AimS.Which == 0 ? RX.TransformPosition(T.BodyAt + FQuat(FVector::UpVector, FMath::DegreesToRadians(T.Yaw)).RotateVector(FVector(0.f, 260.f, 110.f))) : DriverCam->GetComponentLocation();
	MoveCam->SetWorldLocationAndRotation(From, FRotator(FMath::Clamp(Dir.Rotation().Pitch - 6.f, -60.f, 40.f), Dir.Rotation().Yaw, 0.f));
}

void AFireTruck::SprayJet(float Dt)
{
	UWorld* W = GetWorld();
	if (!W || !JetDrops || !JetMist) return;
	const FTurret& T = Tur[AimS.Which];
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	if (AimS.bSpray)
	{
		const FTransform N = NozzleNow(AimS.Which);
		const FVector Tip = RX.TransformPosition(N.GetLocation() + N.GetRotation().RotateVector(FVector(0.f, -T.Len, 0.f)));
		const FVector Dir = RX.TransformVectorNoScale(N.GetRotation().RotateVector(FVector(0, -1, 0))).GetSafeNormal();
		const FVector Carry = GetVelocity();   // (pump and roll: the jet leaves with the truck's own speed)
		JetEmit += Dt * JetPerSecond;
		const int32 NEm = int32(JetEmit); int32 Ek = 0;
		while (JetEmit >= 1.f)
		{
			JetEmit -= 1.f; ++Ek;
			FJetDrop D; D.V = FMath::VRandCone(Dir, FMath::DegreesToRadians(1.5f)) * T.Speed * FMath::FRandRange(0.94f, 1.03f) + Carry;
			D.P = Tip + D.V * (Dt * float(NEm - Ek) / FMath::Max(NEm, 1)); D.Life = 0.f; D.bSplash = D.bMist = false; D.Seed = FMath::FRand();
			Jet.Add(D);
		}
	}
	else JetEmit = 0.f;
	if (Jet.Num() == 0) { if (JetDrops->GetInstanceCount() > 0) JetDrops->ClearInstances(); if (JetMist->GetInstanceCount() > 0) JetMist->ClearInstances(); return; }
	ADP_PERF_SCOPE("the fire truck's jet", 3.f);
	// every drop's flight tested against the world: the jet stops on walls and roofs, never through a building
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPTurretJet), false, this);
	Q.AddIgnoredActor(Model);
	if (Driver) Q.AddIgnoredActor(Driver);
	TArray<FJetDrop> Splash;
	for (FJetDrop& D : Jet)
	{
		D.Life += Dt;
		const FVector From = D.P;
		D.V.Z -= JetG * Dt * (D.bMist ? 0.3f : 1.f);
		D.V *= FMath::Max(0.f, 1.f - (D.bMist ? 3.f : JetDrag) * Dt);
		D.P += D.V * Dt;
		bool bGone = D.Life > (D.bMist ? 1.2f : D.bSplash ? 0.4f : JetLife);
		if (!D.bSplash && !bGone)
		{
			FHitResult H; FVector VP, VN;
			const bool bVeh = FVehicleDirt::WashSegment(From, D.P, 0.004f, VP, VN);
			if (bVeh) { H.Location = H.ImpactPoint = VP; H.Normal = H.ImpactNormal = VN; }
			if (bVeh || W->LineTraceSingleByChannel(H, From, D.P, ECC_Visibility, Q))
			{
				bGone = true;
				if (AFireTruck* Other = AFireTruck::FromActor(H.GetActor())) Other->Wash(H.ImpactPoint, 0.004f);
				if (H.ImpactNormal.Z > 0.6f && !bVeh && FMath::FRand() < 0.35f) JetWetAt(H.ImpactPoint, H.ImpactNormal);
				if (FMath::FRand() < 0.35f) { FJetDrop M; M.bSplash = M.bMist = true; M.Life = 0.f; M.Seed = FMath::FRand(); M.P = H.ImpactPoint + H.ImpactNormal * 8.f; M.V = H.ImpactNormal * 120.f + FMath::VRand() * 80.f + FVector(0, 0, 40.f); Splash.Add(M); }
				for (int32 k = 0; k < 2; ++k) { FJetDrop S; S.bSplash = true; S.bMist = false; S.Seed = 0.f; S.Life = 0.f; S.P = H.ImpactPoint + H.ImpactNormal * 2.f; S.V = FMath::GetReflectionVector(D.V, H.ImpactNormal) * 0.15f + FMath::VRand() * 150.f + H.ImpactNormal * 130.f; Splash.Add(S); }
			}
		}
		if (bGone) D.Life = -1.f;
	}
	Jet.RemoveAll([](const FJetDrop& D) { return D.Life < 0.f; });
	Jet.Append(Splash);
	// drawn as the wash hose's: each drop joined to the one before into one stream, whiter as it breaks up (foam: white)
	FVector EyeAt = RX.GetLocation();
	if (APlayerController* PCm = W->GetFirstPlayerController()) if (PCm->PlayerCameraManager) EyeAt = PCm->PlayerCameraManager->GetCameraLocation();
	TArray<FTransform> Shown, MistShown; TArray<float> Data, MistData;
	const float Thick = AimS.Which == 0 ? 6.f : 3.5f;
	const FJetDrop* Prev = nullptr;
	for (const FJetDrop& D : Jet)
	{
		if (D.bMist)
		{
			const float K = D.Life / 1.2f; const FVector ToEye = (EyeAt - D.P).GetSafeNormal();
			MistShown.Add(FTransform(FQuat(ToEye, D.Seed * 2.f * PI) * FRotationMatrix::MakeFromZ(ToEye).ToQuat(), D.P, FVector(0.4f + 0.9f * FMath::Sqrt(K))));
			MistData.Add(D.Seed); MistData.Add(0.55f * FMath::Square(1.f - K) * FMath::Min(1.f, D.Life * 12.f));
			continue;
		}
		if (D.bSplash) { Shown.Add(FTransform(FRotator::ZeroRotator, D.P, FVector(0.015f))); Data.Add(1.f); Data.Add(1.f - D.Life / 0.4f); continue; }
		const float Wd = (Thick + D.Life * 6.f) / 100.f;
		const float White = bFoam ? 0.95f : FMath::Clamp(0.45f + D.Life * 0.9f, 0.f, 0.9f);
		if (Prev && FVector::Dist(Prev->P, D.P) < 120.f) { const FVector V = Prev->P - D.P; Shown.Add(FTransform(V.Rotation(), (Prev->P + D.P) * 0.5f, FVector((V.Size() + 2.f) / 100.f, Wd, Wd))); }
		else Shown.Add(FTransform(D.V.Rotation(), D.P, FVector(0.08f, Wd, Wd)));
		Data.Add(White); Data.Add(1.f);
		Prev = &D;
	}
	JetDrops->ClearInstances();
	if (Shown.Num()) { JetDrops->AddInstances(Shown, false, true, false); for (int32 i = 0; i < Shown.Num(); ++i) JetDrops->SetCustomData(i, TArrayView<const float>(&Data[i * 2], 2), false); JetDrops->MarkRenderStateDirty(); }
	JetMist->ClearInstances();
	if (MistShown.Num()) { JetMist->AddInstances(MistShown, false, true, false); for (int32 i = 0; i < MistShown.Num(); ++i) JetMist->SetCustomData(i, TArrayView<const float>(&MistData[i * 2], 2), false); JetMist->MarkRenderStateDirty(); }
}

void AFireTruck::JetWetAt(const FVector& At, const FVector& Normal)
{
	// as the wash hose's: one patch per spot, a fresh hit near an old one wets it again and spreads it
	for (int32 i = 0; i < JetWet.Num(); ++i)
		if (JetWet[i] && FVector::Dist(JetWet[i]->GetComponentLocation(), At) < 90.f)
		{
			JetWetAge[i] = FMath::Min(JetWetAge[i], 0.f);
			FVector S = JetWet[i]->DecalSize; const float G = FMath::Min(S.Y * 1.015f, 160.f) / FMath::Max(S.Y, 1.f);
			S.Y *= G; S.Z *= G; JetWet[i]->DecalSize = S; JetWet[i]->MarkRenderStateDirty();
			return;
		}
	if (JetWet.Num() >= JetMaxWet)
	{
		int32 Old = 0; for (int32 i = 1; i < JetWet.Num(); ++i) if (JetWetAge[i] > JetWetAge[Old]) Old = i;
		if (JetWet[Old]) JetWet[Old]->DestroyComponent();
		JetWet.RemoveAt(Old); JetWetMids.RemoveAt(Old); JetWetAge.RemoveAt(Old);
	}
	static UMaterialInterface* Base = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_WetPatch.M_WetPatch")));
	if (!Base) return;
	UDecalComponent* Dc = NewObject<UDecalComponent>(this);
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetScalarParameterValue(TEXT("Amount"), 1.0f);
	Dc->SetDecalMaterial(M);
	const float R = FMath::FRandRange(100.f, 160.f);
	Dc->DecalSize = FVector(40.f, R, R);
	Dc->SetUsingAbsoluteLocation(true); Dc->SetUsingAbsoluteRotation(true);
	Dc->SetupAttachment(GetRootComponent());
	Dc->RegisterComponent();
	Dc->SetWorldLocationAndRotation(At, FRotationMatrix::MakeFromXZ(-Normal, FVector::ForwardVector).Rotator() + FRotator(0.f, 0.f, FMath::FRandRange(0.f, 360.f)));
	JetWet.Add(Dc); JetWetMids.Add(M); JetWetAge.Add(0.f);
}
