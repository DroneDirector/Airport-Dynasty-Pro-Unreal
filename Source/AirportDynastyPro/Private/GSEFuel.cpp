// The aircraft refueller at work (to-do 212; the vehicle and its crew's part are the shared ones of GSEService.cpp): its hired
// driver sets its two chocks, clamps the bonding cable to the aircraft's main gear leg, takes the left hose's nozzle off its
// holder - the hose comes off the reel behind it, a real flexible hose as the wash hose and the GPU's cable - climbs up the
// back onto the platform, raises it with the held lever, locks the nozzle on the wing's coupling over his head, comes down,
// pumps the flight's own fuel with the dead man in his hand (about 1000 l a minute), and puts it all back in order. One
// stage after the other (FSvcJob::Stage): whoever takes over goes on from the last one finished.
// The coupling: under the right wing's leading edge, 9.9 m out, 11.75 m behind the nose wheel, 3.80 m up - measured on the
// game's A320 (Art/A320/A320.glb: the wing's underside there), where the real one has its refuelling panel.
#include "GSEFuel.h"
#include "GSEWater.h"
#include "GSE.h"
#include "ServiceVehicles.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "VehicleControls.h"
#include "VehicleRiders.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "CableComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	const FVector FuUp(0, 0, 1);
	const float FuFloor = 145.f;                              // (the platform's floor, down)
	const FVector FuWork(-430.f, -62.f, 0.f);
	const FVector FuCouplePlane(990.f, 1175.f, 380.f);        // (the aircraft's root, at the nose wheel: x its right, y aft)
	const FVector FuGear(395.f, 1264.f, 135.f);               // (the right main gear's leg, above the wheels)
	const FVector FuNozStand(-533.f, -106.f, 0.f);
}

void AGSE::FuelDress(FTractor& T)
{
	ABabylonModel* M = T.M.Get(); if (!M) return;
	T.Fu = MakeShared<FSvcRefuel>(); FSvcRefuel& F = *T.Fu;
	auto Pre = [&](const FString& N, const FVector& P) { return M->BindGroup({N}, true, FTransform(P)); };
	for (int32 w = 0; w < 2; ++w)
	{
		const FString K = w ? TEXT("TR") : TEXT("TL");
		F.GWheel[w] = M->BindGroup({TEXT("Tyre_") + K, TEXT("Rim_") + K}, true, FTransform(SvcP(T, *(TEXT("PIV_Wheel_") + K))));
		F.GSteer[w] = Pre(TEXT("Spindle_") + K, SvcP(T, *(TEXT("PIV_Steer_") + K)));
	}
	const FVector ReelP = SvcP(T, TEXT("PIV_HoseReel_L"));
	F.GReel = Pre(TEXT("HoseReel_L"), ReelP); F.GTail = Pre(TEXT("HoseTail_L"), SvcP(T, TEXT("IK_Nozzle_L_Hose"))); F.GNozzle = Pre(TEXT("Nozzle_L"), SvcP(T, TEXT("PIV_Nozzle_L")));
	for (int32 i = 0; i < 15; ++i) F.GTurn.Add(Pre(FString::Printf(TEXT("HoseTurn_L_%02d"), i), ReelP));
	F.GPlat = Pre(TEXT("Platform"), FVector::ZeroVector);
	F.GGate = Pre(TEXT("GatePlatform"), SvcP(T, TEXT("PIV_GatePlatform"))); F.GPLever = Pre(TEXT("LeverPlatform"), SvcP(T, TEXT("PIV_LeverPlatform")));
	static const TCHAR* Sc[] = {TEXT("ScissorA1"), TEXT("ScissorB1"), TEXT("ScissorA2"), TEXT("ScissorB2")};
	for (int32 i = 0; i < 4; ++i) F.GScissor[i] = Pre(Sc[i], SvcP(T, i < 2 ? TEXT("PIV_Scissor1") : TEXT("PIV_Scissor2")));
	F.GBondReel = Pre(TEXT("BondReel"), SvcP(T, TEXT("PIV_BondReel"))); F.GClamp = Pre(TEXT("BondClamp"), SvcP(T, TEXT("IK_BondClamp"))); F.GBondTail = Pre(TEXT("BondTail"), SvcP(T, TEXT("IK_BondOut")));
	F.GChock[0] = Pre(TEXT("Chock_0"), SvcP(T, TEXT("IK_Chock_0"))); F.GChock[1] = Pre(TEXT("Chock_1"), SvcP(T, TEXT("IK_Chock_1")));
	F.GDead = M->BindGroupWhere([](const FString& N, int32, const FVector&) { return N.StartsWith(TEXT("Deadman")) && !N.StartsWith(TEXT("DeadmanLead")); }, FTransform(SvcP(T, TEXT("IK_Deadman"))));
	F.GDeadLead = Pre(TEXT("DeadmanLead"), SvcP(T, TEXT("IK_DeadmanLead")));
	F.GLever[0] = Pre(TEXT("Lever_Pump"), SvcP(T, TEXT("PIV_Lever_Pump"))); F.GLever[1] = Pre(TEXT("Lever_HoseL"), SvcP(T, TEXT("PIV_Lever_HoseL")));
	F.GLever[2] = Pre(TEXT("Lever_HoseR"), SvcP(T, TEXT("PIV_Lever_HoseR"))); F.GLever[3] = Pre(TEXT("Lever_Defuel"), SvcP(T, TEXT("PIV_Lever_Defuel")));
	for (int32 i = 0; i < 3; ++i) { const FString G = FString::Printf(TEXT("GaugeNeedle%d"), i); F.GNeedle[i] = Pre(G, SvcP(T, *(TEXT("PIV_") + G))); }
	// the hose, the bonding cable, the dead man's lead: flexible, the hose as stiff as the GPU's cable
	auto Cable = [&](const TCHAR* Name, float Width, int32 Segs, bool bStiff, const TCHAR* Like)
	{
		UCableComponent* C = NewObject<UCableComponent>(M, Name); C->SetupAttachment(M->GetRootComponent());
		C->CableWidth = Width; C->NumSegments = Segs; C->NumSides = 10; C->SolverIterations = 16; C->SubstepTime = 0.01f;
		C->CollisionFriction = 0.9f; C->bEnableCollision = true; C->bEnableStiffness = bStiff; C->CableLength = 100.f;
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> G; M->InstancesNamed(Like, true, G); if (G.Num()) C->SetMaterial(0, G[0].Get<0>()->GetMaterial(0));
		C->SetVisibility(false); C->bAutoActivate = false; C->RegisterComponent(); M->AddInstanceComponent(C); C->SetActive(false); C->SetComponentTickEnabled(false);
		C->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		return C;
	};
	F.Hose = Cable(TEXT("FuelHose"), 6.4f, 36, true, TEXT("HoseTurn_L_00")); F.Bond = Cable(TEXT("BondCable"), 1.1f, 36, false, TEXT("BondTail")); F.Lead = Cable(TEXT("DeadmanCable"), 0.9f, 20, false, TEXT("DeadmanLead"));
	UTextRenderComponent* X = NewObject<UTextRenderComponent>(M); X->SetupAttachment(M->GetRootComponent()); X->SetText(FText::FromString(TEXT("0 L")));
	X->SetHorizontalAlignment(EHTA_Center); X->SetVerticalAlignment(EVRTA_TextCenter); X->SetWorldSize(6.5f); X->SetTextRenderColor(FColor(20, 28, 20));
	X->SetRelativeLocationAndRotation(SvcP(T, TEXT("IK_Meter_Face")) + FVector(0.f, -0.5f, 0.f), FRotator(0.f, -90.f, 0.f));
	X->SetCollisionEnabled(ECollisionEnabled::NoCollision); X->RegisterComponent(); M->AddInstanceComponent(X); F.Meter = X;
}

void AGSE::FuelParts(FTractor& T, float Dt)
{
	ABabylonModel* M = T.M.Get(); if (!M || !T.Fu) return;
	FSvcRefuel& F = *T.Fu;
	// the third axle: spins with the drive axle, steers against the front one about the same turning point
	if (T.MeasSpeed > 0.005f || F.bLive)
	{
		const float Tag = -FMath::RadiansToDegrees(FMath::Atan(1.35f / 4.9f * FMath::Tan(FMath::DegreesToRadians(T.Steer))));
		const FQuat Qs(FuUp, FMath::DegreesToRadians(Tag)), Spin(FVector::RightVector, T.SpinR);
		for (int32 w = 0; w < 2; ++w)
		{
			const FVector Kp = SvcP(T, w ? TEXT("PIV_Steer_TR") : TEXT("PIV_Steer_TL")), Wp = SvcP(T, w ? TEXT("PIV_Wheel_TR") : TEXT("PIV_Wheel_TL"));
			if (F.GWheel[w] != INDEX_NONE) M->MoveGroup(F.GWheel[w], FTransform(Qs * Spin, Kp + Qs.RotateVector(Wp - Kp)));
			if (F.GSteer[w] != INDEX_NONE) M->MoveGroup(F.GSteer[w], FTransform(Qs, Kp));
		}
	}
	if (!F.bLive) return;
	auto Put = [M](int32 G, const FTransform& X) { if (G != INDEX_NONE) M->MoveGroup(G, X); };
	auto Show = [](const FVector& P, bool b) { return FTransform(FQuat::Identity, P, b ? FVector::OneVector : FVector(0.001f)); };
	const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
	const ATugs* G = Tg(); const USkeletalMeshComponent* B = F.Ci != INDEX_NONE ? Body(F.Ci) : nullptr;
	const bool bBody = B && G && G->Crew[F.Ci].A.IsValid() && !G->Crew[F.Ci].A->IsHidden();
	const float Face = bBody ? float(G->Crew[F.Ci].A->GetActorRotation().Yaw) + 90.f - T.Yaw : 0.f;   // (which way they face, root frame)
	auto HandL = [&](uint8 Mode) { return Root.InverseTransformPosition(B->GetBoneLocation(Mode == FSvcRefuel::RightHand ? TEXT("hand_r") : TEXT("hand_l"))); };
	auto Held = [&](uint8 Mode) { return bBody && (Mode == FSvcRefuel::RightHand || Mode == FSvcRefuel::LeftHand); };
	// the platform: straight up, its scissor, its gate, its lever (pressed while it moves)
	const float WasLift = F.Lift; F.Lift = FMath::FInterpConstantTo(F.Lift, F.LiftWant, Dt, 9.f); F.Gate = FMath::FInterpConstantTo(F.Gate, F.GateWant, Dt, 1.2f);
	Put(F.GPlat, FTransform(FuUp * F.Lift));
	Put(F.GGate, FTransform(FQuat(FuUp, FMath::DegreesToRadians(-90.f * FMath::SmoothStep(0.f, 1.f, F.Gate))), SvcP(T, TEXT("PIV_GatePlatform")) + FuUp * F.Lift));
	Put(F.GPLever, FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(F.Lift > WasLift ? -15.f : F.Lift < WasLift ? 15.f : 0.f)), SvcP(T, TEXT("PIV_LeverPlatform")) + FuUp * F.Lift));
	const float Sa = FMath::Asin((31.f + F.Lift) / 210.f), Turn = Sa - FMath::DegreesToRadians(8.489f);
	for (int32 s = 0; s < 2; ++s)
	{
		const FVector P(-472.f + 52.5f * FMath::Cos(Sa), 0.f, 107.5f + (s + 0.5f) * 105.f * FMath::Sin(Sa));
		Put(F.GScissor[s * 2], FTransform(FQuat(FVector::RightVector, -Turn), P)); Put(F.GScissor[s * 2 + 1], FTransform(FQuat(FVector::RightVector, Turn), P));   // (nose up: a negative turn about +y)
	}
	// the nozzle
	const FVector NozP = SvcP(T, TEXT("PIV_Nozzle_L")); FVector Want = NozP; float WantYaw = 0.f;
	if (Held(F.Noz)) { WantYaw = Face; Want = HandL(F.Noz) + FRotator(0.f, Face, 0.f).RotateVector(FVector(0.f, F.Noz == FSvcRefuel::RightHand ? -15.f : 15.f, 0.f)) + FuUp * 6.f; }
	else if (F.Noz == FSvcRefuel::YourHand) { Want = T.YouHand; WantYaw = F.NozYaw; }
	else if (F.Noz == FSvcRefuel::Put) { Want = F.NozAt; WantYaw = F.NozYaw; }
	else F.Noz = FSvcRefuel::InPlace;
	if (F.NozNow.IsZero()) F.NozNow = NozP;
	F.NozNow = FMath::VInterpTo(F.NozNow, Want, Dt, 14.f); F.NozYawNow += FMath::FindDeltaAngleDegrees(F.NozYawNow, WantYaw) * FMath::Min(1.f, Dt * 8.f);
	const bool bHome = F.Noz == FSvcRefuel::InPlace && FVector::Dist(F.NozNow, NozP) < 1.f;
	if (bHome) { F.NozNow = NozP; F.NozYawNow = 0.f; }
	Put(F.GNozzle, FTransform(FRotator(0.f, F.NozYawNow, 0.f), F.NozNow));
	Put(F.GTail, Show(SvcP(T, TEXT("IK_Nozzle_L_Hose")), bHome));
	auto Run = [&](UCableComponent* C, const FVector& From, const FVector& To, float Slack, bool bOn)
	{
		if (!C) return 0.f;
		if (!bOn) { if (C->IsVisible()) { C->SetVisibility(false); C->SetActive(false); C->SetComponentTickEnabled(false); } return 0.f; }
		const FVector A = Root.TransformPosition(From), E = Root.TransformPosition(To); C->SetWorldLocation(A);
		C->EndLocation = C->GetComponentTransform().InverseTransformPosition(E); C->CableLength = float(FVector::Dist(A, E)) * 1.04f + Slack;
		if (!C->IsVisible()) { C->SetVisibility(true); C->SetActive(true); C->SetComponentTickEnabled(true); }
		return C->CableLength;
	};
	// the hose: off the reel as far as the nozzle goes (its turns gone from the outside in), the reel turning with it
	F.Out = Run(F.Hose, SvcP(T, TEXT("IK_HoseOut_L")), F.NozNow + FRotator(0.f, F.NozYawNow, 0.f).RotateVector(SvcP(T, TEXT("IK_Nozzle_L_Hose")) - NozP), 60.f, !bHome);
	if (!FMath::IsNearlyEqual(F.Out, F.OutWas, 0.05f))
	{
		F.Reel += (F.Out - F.OutWas) / 33.f; F.OutWas = F.Out;
		const FTransform Rl(FQuat(FVector::RightVector, -F.Reel), SvcP(T, TEXT("PIV_HoseReel_L")));
		Put(F.GReel, Rl);
		float Left = F.Out;
		for (int32 i = 14; i >= 0; --i) { const float L = i < 5 ? 145.8f : i < 10 ? 184.7f : 223.7f; FTransform X = Rl; if (Left >= L * 0.5f) X.SetScale3D(FVector(0.001f)); Left -= L; Put(F.GTurn[i], X); }
	}
	// the bonding cable and its clamp, the chocks, the dead man
	const FVector ClampAt = Held(F.Clamp) ? HandL(F.Clamp) : F.Clamp == FSvcRefuel::YourHand ? T.YouHand : F.Clamp == FSvcRefuel::Put ? F.ClampAt : SvcP(T, TEXT("IK_BondClamp"));
	Put(F.GClamp, FTransform(ClampAt)); Put(F.GBondTail, Show(SvcP(T, TEXT("IK_BondOut")), F.Clamp == FSvcRefuel::InPlace));
	const float Bo = Run(F.Bond, SvcP(T, TEXT("IK_BondOut")), ClampAt + FVector(0.f, 0.f, -2.f), 50.f, F.Clamp != FSvcRefuel::InPlace);
	F.BondSpin += (Bo - F.BondOut) / 12.f; F.BondOut = Bo; Put(F.GBondReel, FTransform(FQuat(FVector::RightVector, -F.BondSpin), SvcP(T, TEXT("PIV_BondReel"))));
	for (int32 c = 0; c < 2; ++c)
	{
		const FVector P = SvcP(T, c ? TEXT("IK_Chock_1") : TEXT("IK_Chock_0"));
		Put(F.GChock[c], Held(F.Chock[c]) ? FTransform(FRotator(0.f, Face, 0.f), HandL(F.Chock[c])) : F.Chock[c] == FSvcRefuel::YourHand ? FTransform(T.YouHand) : F.Chock[c] == FSvcRefuel::Put ? FTransform(SvcP(T, c ? TEXT("IK_ChockPlace_1") : TEXT("IK_ChockPlace_0")) + FuUp * 23.2f) : FTransform(P));
	}
	const FVector DeadAt = Held(F.Dead) ? HandL(F.Dead) : SvcP(T, TEXT("IK_Deadman"));
	Put(F.GDead, FTransform(DeadAt)); Put(F.GDeadLead, Show(SvcP(T, TEXT("IK_DeadmanLead")), F.Dead == FSvcRefuel::InPlace));
	Run(F.Lead, SvcP(T, TEXT("IK_DeadmanLead")), DeadAt, 30.f, F.Dead != FSvcRefuel::InPlace);
	// the levers, the gauges, the meter
	static const TCHAR* const FuLevers[4] = {TEXT("PIV_Lever_Pump"), TEXT("PIV_Lever_HoseL"), TEXT("PIV_Lever_HoseR"), TEXT("PIV_Lever_Defuel")};
	for (int32 i = 0; i < 4; ++i) { F.LeverNow[i] = FMath::FInterpConstantTo(F.LeverNow[i], F.LeverWant[i], Dt, 2.f); Put(F.GLever[i], FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(40.f * F.LeverNow[i])), SvcP(T, FuLevers[i]))); }
	F.Flow = FMath::FInterpTo(F.Flow, F.bPump ? 1.f : 0.f, Dt, 0.8f);
	if (T.Job == EJob::None) YouFuel(T, Dt);
	static const float Dial[] = {0.55f, 0.62f, 0.4f};
	for (int32 i = 0; i < 3; ++i) Put(F.GNeedle[i], FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(-135.f + 270.f * Dial[i] * F.Flow)), SvcP(T, *FString::Printf(TEXT("PIV_GaugeNeedle%d"), i))));
	if (F.Meter && FMath::Abs(F.Litres - F.Shown) >= 1.f) { F.Shown = FMath::FloorToFloat(F.Litres); F.Meter->SetText(FText::FromString(FString::Printf(TEXT("%.0f L"), F.Shown))); }
	// (an unseen floor just over the apron under it all, for the hose and the cables only - as the GPU's)
	const bool bOut = !bHome || F.Clamp != FSvcRefuel::InPlace || F.Dead != FSvcRefuel::InPlace;
	if (bOut && !F.Floor)
	{
		F.Floor = NewObject<UBoxComponent>(M, TEXT("FuelHoseFloor")); F.Floor->SetupAttachment(M->GetRootComponent()); F.Floor->SetUsingAbsoluteLocation(true); F.Floor->SetUsingAbsoluteRotation(true);
		F.Floor->SetCollisionObjectType(ECC_WorldStatic); F.Floor->SetCollisionResponseToAllChannels(ECR_Ignore); F.Floor->SetCollisionResponseToChannel(F.Hose->GetCollisionObjectType(), ECR_Block);
		F.Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision); F.Floor->SetCanEverAffectNavigation(false); F.Floor->SetHiddenInGame(true); F.Floor->SetBoxExtent(FVector(1100.f, 1100.f, 5.f)); F.Floor->RegisterComponent();
	}
	const ECollisionEnabled::Type Ce = bOut ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
	if (F.Floor && F.Floor->GetCollisionEnabled() != Ce)
	{
		F.Floor->SetCollisionEnabled(Ce);
		if (bOut)
		{
			// (tilted with the apron, lifted by the most it bulges: never below the visible ground)
			auto Gr = [&](float Lx, float Ly) { const FVector W = Root.TransformPosition(FVector(Lx, Ly, 0.f)); return Plan(W.X / 100.f, -W.Y / 100.f); };
			const FVector A0 = Gr(-900.f, -900.f), A1 = Gr(500.f, -900.f), A2 = Gr(-900.f, 300.f); FVector N = FVector::CrossProduct(A1 - A0, A2 - A0).GetSafeNormal(); if (N.Z < 0.f) N = -N;
			float Bulge = 0.f; for (int32 i = 0; i < 5; ++i) for (int32 k = 0; k < 5; ++k) Bulge = FMath::Max(Bulge, float(FVector::DotProduct(Gr(-900.f + 350.f * i, -900.f + 300.f * k) - A0, N)));
			F.Floor->SetWorldLocationAndRotation((A1 + A2) * 0.5f + N * (Bulge + 0.6f - 5.f), FRotationMatrix::MakeFromZX(N, A1 - A0).Rotator());
		}
	}
	// (all in its place and nothing moving: nothing more to do until the next job)
	const int32 Me = int32(&T - Tractors.GetData()); bool bJob = false; for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Phase == 2 && P.Value.Ti == Me) bJob = true;
	if (!bJob && bHome && !bOut && F.Lift == F.LiftWant && F.Gate == F.GateWant && F.Chock[0] == FSvcRefuel::InPlace && F.Chock[1] == FSvcRefuel::InPlace && F.LeverNow[0] == 0.f && F.LeverNow[1] == 0.f && F.LeverNow[2] == F.LeverWant[2] && F.LeverNow[3] == F.LeverWant[3] && F.Flow < 0.005f && T.bPrev) F.bLive = false;
}

void AGSE::FuelTick(FSvcJob& J, float Dt)
{
	if (J.Phase != 2 || J.Stage != 4 || !Tractors.IsValidIndex(J.Ti) || !Tractors[J.Ti].Fu) return;
	FSvcRefuel& F = *Tractors[J.Ti].Fu; if (!F.bPump || !F.bConn) return;
	// (one hose: about 1000 l a minute once it runs)
	const float Was = J.Pumped; J.Pumped = FMath::Min(J.Litres, J.Pumped + Dt * (1000.f / 60.f) * F.Flow); F.Litres = J.Pumped;
	if (FMath::FloorToInt(J.Pumped / 2000.f) > FMath::FloorToInt(Was / 2000.f) || (J.Pumped >= J.Litres && Was < J.Litres)) ADP_LOG(TEXT("GSE: %s fuelled %.0f of %.0f l"), *J.Reg, J.Pumped, J.Litres);
}

// ------------------------------------------------------------------ the tall cab: two steps

TFunction<void(float, float, FDriverPose&)> AGSE::TallClimbFn(int32 Ti, bool bIn) const
{
	return [this, Ti, bIn](float K0, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const float K = bIn ? K0 : 1.f - K0;
		const FVector In = FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), Fwd = FRotator(0.f, T.Yaw, 0.f).Vector();
		const FVector Lo = SvcP(T, TEXT("IK_Step_DoorDriver_Low")), Hi = T.KStep, GIn = SvcP(T, TEXT("IK_GrabIn_L"));
		const float Xb = float(Hi.X) - 14.f;   // (the body between the two grab handles)
		auto Key = [&](int32 i)
		{
			FDriverPose Q; Q.Facing = In; Q.bHand[0] = Q.bHand[1] = true; Q.Hand[0] = TW(T, GIn); Q.Hand[1] = TW(T, T.KGrabOut + FVector(0.f, 0.f, i > 2 ? 22.f : 0.f)); Q.Grip[0] = Q.Grip[1] = 1.f;
			const bool bFar = T.KGrabOut.X < Xb - 60.f; if (bFar) { Q.Hand[1] = Q.Hand[0]; Q.bHand[1] = false; }   // (the small truck's rear handle is out of reach from its front step)
			switch (i)
			{
			case 0: Q = ADPPose::Standing(TW(T, T.KStand), In, PH); if (!bFar) { Q.bHand[1] = true; Q.Hand[1] = TW(T, T.KGrabOut); Q.Grip[1] = 1.f; } break;
			case 1: Q.Pelvis = TW(T, FVector(Xb - 10.f, Lo.Y - 44.f, PH + 4.f)); Q.Lean = 12.f; Q.Hand[0] = TW(T, GIn - FuUp * 42.f);      // the left foot on the low step
				Q.Foot[0] = TW(T, Lo + FVector(5.f, 0.f, 0.f)); Q.Foot[1] = TW(T, FVector(Xb - 22.f, Lo.Y - 55.f, 0.f)); break;
			case 2: Q.Pelvis = TW(T, FVector(Xb, Lo.Y - 24.f, Lo.Z + PH - 4.f)); Q.Lean = 13.f; Q.Hand[0] = TW(T, GIn - FuUp * 20.f);      // up on it, the right one on the next
				Q.Foot[0] = TW(T, Lo + FVector(5.f, 0.f, 0.f)); Q.Foot[1] = TW(T, Hi + FVector(-9.f, 0.f, 0.f)); break;
			case 3: Q.Pelvis = TW(T, FVector(Xb, Hi.Y - 12.f, Hi.Z + PH - 5.f)); Q.Lean = 16.f;                                              // both on the upper one
				Q.Foot[0] = TW(T, Hi + FVector(6.f, 0.f, 0.f)); Q.Foot[1] = TW(T, Hi + FVector(-11.f, 0.f, 0.f)); break;
			case 4: Q.Pelvis = TW(T, FVector(T.KSeat.X + 22.f, FMath::Lerp(Hi.Y, T.KSeat.Y, 0.35), T.KFloor + PH - 24.f)); Q.Lean = 26.f; Q.bHand[1] = false;   // in, a foot on the cab's floor
				Q.Foot[0] = TW(T, FVector(T.KSeat.X + 36.f, FMath::Lerp(Hi.Y, T.KSeat.Y, 0.5), T.KFloor)); Q.Foot[1] = TW(T, Hi + FVector(-11.f, 2.f, 0.f)); break;
			case 5: Q.Facing = (In + Fwd).GetSafeNormal2D(); Q.Pelvis = TW(T, FVector(T.KSeat.X + 4.f, T.KSeat.Y - 14.f, T.KFloor + PH - 38.f)); Q.Lean = 24.f; Q.bHand[0] = Q.bHand[1] = false;
				Q.Foot[0] = TW(T, FVector(T.KSeat.X + 37.f, T.KSeat.Y - 16.f, T.KFloor)); Q.Foot[1] = TW(T, FVector(T.KSeat.X + 31.f, T.KSeat.Y + 8.f, T.KFloor)); break;
			default: SeatFn(Ti)(1.f, PH, Q); break;
			}
			return Q;
		};
		static const float At[] = {0.f, 0.16f, 0.33f, 0.5f, 0.7f, 0.86f, 1.f};
		int32 i = 0; while (i < 5 && K > At[i + 1]) ++i;
		P = ADPPose::Lerp(Key(i), Key(i + 1), FMath::SmoothStep(0.f, 1.f, (K - At[i]) / (At[i + 1] - At[i])));
	};
}

// ------------------------------------------------------------------ the work

TFunction<void(float, float, FDriverPose&)> AGSE::FuelPlatFn(int32 Ti, bool bUp) const
{
	// up the back, step by step, the left hand on the handles (the right one has the nozzle), through the gate, to the work place
	return [this, Ti, bUp](float K0, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const float K = bUp ? K0 : 1.f - K0; const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector();
		auto S = [&](int32 n, float Y) { return TW(T, SvcP(T, *FString::Printf(TEXT("IK_Step_Platform_%d"), n)) + FVector(n == 3 ? 4.f : 0.f, Y, 0.f)); };
		auto Key = [&](int32 i)
		{
			FDriverPose Q; Q.Facing = Fw; Q.Lean = 14.f; Q.bHand[0] = true; Q.Grip[0] = 0.9f; Q.Hand[0] = TW(T, SvcP(T, TEXT("IK_Grab_RearSteps_L")));
			const FVector Gp = TW(T, SvcP(T, TEXT("IK_Grab_Platform_L")));
			auto Pel = [&](float X, float Z) { Q.Pelvis = TW(T, FVector(X, 0.f, Z)); };
			switch (i)
			{
			case 0: Q = ADPPose::Standing(TW(T, SvcP(T, TEXT("IK_Stand_Platform"))), Fw, PH); break;
			case 1: Pel(-536.f, PH + 6.f); Q.Foot[0] = S(0, -11.f); Q.Foot[1] = TW(T, FVector(-548.f, 11.f, 0.f)); break;
			case 2: Pel(-520.f, 61.5f + PH - 5.f); Q.Foot[0] = S(0, -11.f); Q.Foot[1] = S(1, 11.f); break;
			case 3: Pel(-516.f, 82.f + PH - 5.f); Q.Hand[0] = Gp - FuUp * 45.f; Q.Foot[0] = S(2, -11.f); Q.Foot[1] = S(1, 11.f); break;
			case 4: Pel(-506.f, 114.f + PH - 6.f); Q.Hand[0] = Gp; Q.Foot[0] = S(2, -11.f); Q.Foot[1] = S(3, 11.f); break;
			case 5: Q = ADPPose::Standing(TW(T, FVector(-446.f, 8.f, FuFloor)), Fw, PH); break;
			case 6: Q = ADPPose::Standing(TW(T, FVector(-428.f, 4.f, FuFloor)), FRotator(0.f, T.Yaw - 45.f, 0.f).Vector(), PH); break;
			default: Q = ADPPose::Standing(TW(T, FuWork + FuUp * FuFloor), FRotator(0.f, T.Yaw - 90.f, 0.f).Vector(), PH); break;
			}
			return Q;
		};
		const float X = K * 7.f; const int32 i = FMath::Clamp(FMath::FloorToInt(X), 0, 6);
		P = ADPPose::Lerp(Key(i), Key(i + 1), FMath::SmoothStep(0.f, 1.f, X - i));
	};
}

void AGSE::FuelUpDown(int32 Ci, int32 Ti, bool bUp)
{
	// onto the platform and up with it, or down with it and off it (its lever held all the way)
	auto Hold2 = [this, Ci, Ti](bool bLever)
	{
		if (ATugs* G = Tg()) G->Crew[Ci].Hold = [this, Ti, bLever](float, float PH, FDriverPose& P)
		{
			const FTractor& T = Tractors[Ti]; const float L = T.Fu ? T.Fu->Lift : 0.f; const FVector At = TW(T, FuWork + FuUp * (FuFloor + L)), Fc = FRotator(0.f, T.Yaw - 90.f, 0.f).Vector();
			P = bLever ? ADPPose::Reach(At, Fc, 0, TW(T, SvcP(T, TEXT("IK_LeverPlatform")) + FuUp * L), 0.9f, PH, 1.f) : ADPPose::Standing(At, Fc, PH);
		};
	};
	auto Gate = [this, Ti](float V) { if (Tractors[Ti].Fu) Tractors[Ti].Fu->GateWant = V; };
	auto Drive = [this, Ci, Ti, Hold2](bool bHigh)
	{
		Do(Ci, [Hold2] { Hold2(true); }); Wait(Ci, 0.6f);
		Do(Ci, [this, Ti, bHigh]
		{
			// (so high that the hands lock the nozzle just over the head, the rails never nearer than 35 cm to the wing)
			FSvcRefuel* F = Tractors[Ti].Fu.Get(); if (!F) return;
			F->LiftWant = bHigh ? FMath::Clamp(float(F->Couple.Z) - 13.f - FuFloor - 184.f, 0.f, FMath::Clamp(float(F->Couple.Z) - 35.f - 255.f, 0.f, 90.f)) : 0.f;
		});
		WaitFor(Ci, [this, Ti](float E) { const FSvcRefuel* F = Tractors[Ti].Fu.Get(); return !F || E > 20.f || F->Lift == F->LiftWant; });
		Do(Ci, [Hold2] { Hold2(false); }); Wait(Ci, 0.5f);
	};
	if (bUp)
	{
		WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], SvcP(Tractors[Ti], TEXT("IK_Stand_Platform"))); }, 12.f);
		Do(Ci, [this, Ci, Gate] { Gate(1.f); if (ATugs* G = Tg()) G->Crew[Ci].bUp = true; });
		PoseAct(Ci, FuelPlatFn(Ti, true), 6.5f);
		Do(Ci, [Gate, Hold2] { Gate(0.f); Hold2(false); });
		Drive(true);
		return;
	}
	Drive(false);
	Do(Ci, [Gate] { Gate(1.f); });
	Wait(Ci, 0.9f);
	Do(Ci, [this, Ci] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = nullptr; });
	PoseAct(Ci, FuelPlatFn(Ti, false), 6.f);
	Do(Ci, [this, Ci, Gate] { Gate(0.f); if (ATugs* G = Tg()) G->Crew[Ci].bUp = false; });
}

void AGSE::FuelArrive(int32 Job)
{
	// where the coupling and the gear leg are from here: on the real aircraft standing there
	FSvcJob* J = SvcJ(Job); if (!J) return;
	FTractor& T = Tractors[J->Ti]; if (!T.Fu) return;
	FSvcRefuel& F = *T.Fu; F.bLive = true; F.Litres = J->Pumped;
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp);
	const AActor* Pl = nullptr; double Bd = 900.0; const FVector Nose = Plan(S, N);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		for (const FName& Tg2 : It->Tags)
		{
			const FString Ts = Tg2.ToString(); if (!Ts.StartsWith(TEXT("SIM/")) && !Ts.StartsWith(TEXT("MOVE/"))) continue;
			const double D = FVector::Dist2D(It->GetActorLocation(), Nose); if (D < Bd) { Bd = D; Pl = *It; }
		}
	const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
	auto Loc = [&](const FVector& L) { return Root.InverseTransformPosition(Pl ? Pl->GetActorTransform().TransformPosition(L) : FVector(Plan(S - L.X / 100.f, N + L.Y / 100.f) + FuUp * L.Z)); };
	const FVector C = Loc(FuCouplePlane), Want(FuWork.X, -118.f, 380.f);
	F.Couple = Want + (C - Want).GetClampedToMaxSize(45.f); F.ClampAt = Loc(FuGear);
	if (FVector::Dist(C, Want) > 45.f) ADP_WARN(TEXT("GSE: the %s stands %.0f cm off the coupling of %s"), *T.Name, FVector::Dist(C, Want), *J->Reg);
	ADP_LOG(TEXT("GSE: the %s at %s - %.0f l, the coupling at (%.0f, %.0f, %.0f) of it"), *T.Name, *J->Reg, J->Litres, C.X, C.Y, C.Z);
}

void AGSE::SvcFuelWork(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J || J->Phase != 2 || !J->Men.Num() || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Fu) return;
	const int32 Ci = J->Men[0].Ci, Ti = J->Ti; FSvcRefuel& F0 = *Tractors[Ti].Fu; F0.Ci = Ci; F0.bLive = true;
	if (J->Men[0].bSeated) ClimbOut(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) { X->bSeated = false; X->bOut = true; } });
	auto Fu = [this, Ti]() -> FSvcRefuel& { return *Tractors[Ti].Fu; };
	auto At = [this, Ci, Ti](const FVector& L, float Stop = 15.f) { WalkTo(Ci, [this, Ti, L] { return TW(Tractors[Ti], L); }, Stop); };
	auto Pt = [this, Ti](const TCHAR* N) { return SvcP(Tractors[Ti], N); };
	// a hand to a place and back, what it does done while it is there
	auto Hand = [this, Ci, Ti](const FVector& St, int32 Side, const FVector& To, float Dur, TFunction<void()> Fn, bool bLow = false)
	{
		auto Ps = [this, Ti, St, Side, To, bLow](float E, float PH, FDriverPose& P)
		{
			const FTractor& T = Tractors[Ti]; const FVector A = TW(T, St), H = TW(T, To), Fc = (H - A).GetSafeNormal2D();
			P = bLow ? ADPPose::Stoop(A, Fc, Side, H, 0.9f, PH, E) : ADPPose::Reach(A, Fc, Side, H, 0.9f, PH, E);
		};
		PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(FMath::SmoothStep(0.f, 1.f, K), PH, P); }, Dur * 0.5f);
		Do(Ci, MoveTemp(Fn));
		PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(1.f - FMath::SmoothStep(0.f, 1.f, K), PH, P); }, Dur * 0.5f);
	};
	auto Next = [this, Job, Ci](int32 n) { Do(Ci, [this, Job, n] { if (FSvcJob* X = SvcJ(Job)) X->Stage = n; Later([this, Job] { SvcFuelWork(Job); }); }); Wait(Ci, 0.4f); };
	const FVector StCh = Pt(TEXT("IK_Stand_Chocks")) + FVector(0.f, -15.f, 0.f), StPl = Pt(TEXT("IK_Stand_ChockPlace")) + FVector(0.f, 22.f, 0.f), FrR(575.f, 175.f, 0.f), FrL(575.f, -185.f, 0.f), StBond = Pt(TEXT("IK_Stand_Bond")) + FVector(0.f, 15.f, 0.f);
	const FVector Pl0 = Pt(TEXT("IK_ChockPlace_0")) + FuUp * 23.f, Pl1 = Pt(TEXT("IK_ChockPlace_1")) + FuUp * 23.f, PanAt = Pt(TEXT("IK_PanelStand"));
	const FVector Off(-560.f, -230.f, 0.f);                                          // (behind the engine, beside the truck's back)
	const FVector Gear = F0.ClampAt, GearSt(Gear.X, Gear.Y + 80.f, 0.f);             // (outside the wheels: towards the truck)
	auto Door = [this, Ci, Ti, At, Hand, Pt](bool bOpen)
	{
		const FVector P0 = Pt(TEXT("PIV_PanelDoorL")), H0 = Pt(TEXT("IK_Handle_PanelDoorL_Out")), HO = P0 + FRotator(0.f, -110.f, 0.f).RotateVector(H0 - P0), S0 = bOpen ? Pt(TEXT("IK_Stand_PanelDoorL")) : HO + FVector(45.f, -20.f, -HO.Z);
		At(S0, 15.f);
		Hand(S0, 1, bOpen ? H0 : HO, 1.4f, [this, Ti, bOpen] { FTractor& T = Tractors[Ti]; T.SvcDoors[0].Want = bOpen ? 1.f : 0.f; if (bOpen) Sound(*T.SndDoorOpen, TW(T, T.SvcDoors[0].Piv)); });
		WaitFor(Ci, [this, Ti](float E) { const FTractor::FSvcDoor& D = Tractors[Ti].SvcDoors[0]; return E > 5.f || D.Now == D.Want; });
	};
	// the nozzle moved by both hands on the platform, from the chest over the rail up to the coupling, or back
	auto Both = [this, Ci, Ti](bool bOn)
	{
		PoseAct(Ci, [this, Ti, bOn](float K0, float PH, FDriverPose& P)
		{
			FTractor& T = Tractors[Ti]; if (!T.Fu) return;
			FSvcRefuel& F = *T.Fu; const float K = bOn ? K0 : 1.f - K0;
			const FVector Ch(FuWork.X, -90.f, FuFloor + F.Lift + 128.f), Cp = F.Couple - FuUp * 7.f, Under = Cp - FuUp * 12.f;
			// (up beside the rail, then straight on, then a part turn of its collar: it is locked)
			const float A = FMath::SmoothStep(0.f, 0.6f, K), U = FMath::SmoothStep(0.6f, 0.8f, K), Tn = -14.f * (1.f - FMath::SmoothStep(0.8f, 1.f, K));
			F.Noz = FSvcRefuel::Put; F.NozAt = FMath::Lerp(FMath::Lerp(Ch, Under, A), Cp, U); F.NozYaw = -90.f + Tn;
			const FQuat Q(FuUp, FMath::DegreesToRadians(Tn));
			P = ADPPose::Both(TW(T, FuWork + FuUp * (FuFloor + F.Lift)), FRotator(0.f, T.Yaw - 90.f, 0.f).Vector(), TW(T, F.NozAt + Q.RotateVector(FVector(-15.f, 0.f, -6.f))), TW(T, F.NozAt + Q.RotateVector(FVector(15.f, 0.f, -6.f))), 1.f, PH, 1.f, 6.f);
		}, 5.f);
	};
	switch (J->Stage)
	{
	case 0:   // its own chocks, before and behind the left drive wheel (round its front)
		At(FrL, 40.f); At(FrR, 40.f); At(StCh);
		Hand(StCh, 1, Pt(TEXT("IK_Chock_1")), 1.4f, [Fu] { Fu().Chock[1] = FSvcRefuel::RightHand; }, true);
		Hand(StCh, 0, Pt(TEXT("IK_Chock_0")), 1.4f, [Fu] { Fu().Chock[0] = FSvcRefuel::LeftHand; }, true);
		At(FrR, 40.f); At(FrL, 40.f); At(StPl);
		Hand(StPl, 1, Pl1, 1.8f, [Fu] { Fu().Chock[1] = FSvcRefuel::Put; }, true);
		Hand(StPl, 0, Pl0, 1.8f, [Fu] { Fu().Chock[0] = FSvcRefuel::Put; }, true);
		Next(1); break;
	case 1:   // bonded first: the clamp to the main gear's leg, the cable off its reel behind it
		At(StBond); Hand(StBond, 1, Pt(TEXT("IK_BondClamp")), 1.4f, [Fu] { Fu().Clamp = FSvcRefuel::RightHand; }, true);
		At(Off, 50.f); At(GearSt);
		Hand(GearSt, 1, Gear, 2.f, [this, Job, Fu] { Fu().Clamp = FSvcRefuel::Put; if (const FSvcJob* X = SvcJ(Job)) ADP_LOG(TEXT("GSE: %s bonded to the refueller"), *X->Reg); });
		At(Off, 50.f); Next(2); break;
	case 2:   // the panel: the door, the left hose's valve - then everyone off the aircraft
		Door(true); At(PanAt); Hand(PanAt, 1, Pt(TEXT("IK_Lever_HoseL")), 1.4f, [Fu] { Fu().LeverWant[1] = 1.f; });
		WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->bGo; });
		Next(3); break;
	case 3:   // the nozzle off its holder, up, on
		At(Off, 40.f); At(FuNozStand);
		PoseAct(Ci, [this, Ti](float K, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Both(TW(T, FuNozStand), FRotator(0.f, T.Yaw, 0.f).Vector(), TW(T, SvcP(T, TEXT("IK_Nozzle_L_Hand_L"))), TW(T, SvcP(T, TEXT("IK_Nozzle_L_Hand_R"))), 1.f, PH, FMath::SmoothStep(0.f, 1.f, K), 14.f); }, 1.6f);
		Do(Ci, [Fu] { Fu().Noz = FSvcRefuel::RightHand; });
		FuelUpDown(Ci, Ti, true); Both(true);
		Do(Ci, [this, Job, Fu] { FSvcRefuel& F = Fu(); F.bConn = true; F.Noz = FSvcRefuel::Put; F.NozAt = F.Couple - FuUp * 7.f; F.NozYaw = -90.f; if (const FSvcJob* X = SvcJ(Job)) ADP_LOG(TEXT("GSE: the hose is on %s's coupling"), *X->Reg); });
		FuelUpDown(Ci, Ti, false); At(Off, 40.f); Next(4); break;
	case 4:   // pumping: the pump's lever, the dead man in the hand until it is all in
		At(PanAt); Hand(PanAt, 1, Pt(TEXT("IK_Lever_Pump")), 1.4f, [Fu] { Fu().LeverWant[0] = 1.f; });
		Hand(PanAt, 1, Pt(TEXT("IK_Deadman")), 1.4f, [Fu] { Fu().Dead = FSvcRefuel::RightHand; });
		Do(Ci, [this, Ci, Ti, Fu, PanAt]
		{
			Fu().bPump = true;
			if (ATugs* G = Tg()) G->Crew[Ci].Hold = [this, Ti, PanAt](float, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Reach(TW(T, PanAt), FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), 1, TW(T, PanAt + FVector(-20.f, 34.f, 112.f)), 0.95f, PH, 1.f); };
		});
		WaitFor(Ci, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Pumped >= X->Litres; });
		Do(Ci, [this, Ci, Ti, Fu] { Fu().bPump = false; Tractors[Ti].bUsed = true; if (ATugs* G = Tg()) G->Crew[Ci].Hold = nullptr; });
		Hand(PanAt, 1, Pt(TEXT("IK_Deadman")), 1.4f, [Fu] { Fu().Dead = FSvcRefuel::InPlace; });
		Hand(PanAt, 1, Pt(TEXT("IK_Lever_Pump")), 1.2f, [Fu] { Fu().LeverWant[0] = 0.f; });
		Next(5); break;
	case 5:   // off, down, back on its holder: the hose wound in behind it
		At(Off, 40.f); FuelUpDown(Ci, Ti, true);
		Do(Ci, [Fu] { Fu().bConn = false; });
		Both(false); Do(Ci, [Fu] { Fu().Noz = FSvcRefuel::RightHand; });
		FuelUpDown(Ci, Ti, false); At(FuNozStand);
		PoseAct(Ci, [this, Ti, Fu](float K, float PH, FDriverPose& P)
		{
			const FTractor& T = Tractors[Ti]; if (K > 0.45f) Fu().Noz = FSvcRefuel::InPlace;
			P = ADPPose::Both(TW(T, FuNozStand), FRotator(0.f, T.Yaw, 0.f).Vector(), TW(T, SvcP(T, TEXT("IK_Nozzle_L_Hand_L"))), TW(T, SvcP(T, TEXT("IK_Nozzle_L_Hand_R"))), 1.f, PH, K < 0.45f ? FMath::SmoothStep(0.f, 0.45f, K) : 1.f - FMath::SmoothStep(0.6f, 1.f, K), 14.f);
		}, 2.2f);
		Do(Ci, [this, Job, Fu] { Fu().Noz = FSvcRefuel::InPlace; if (FSvcJob* X = SvcJ(Job)) { ADP_LOG(TEXT("GSE: %s is fuelled, the hose stowed"), *X->Reg); if (X->Men.Num()) X->Men[0].bDown = true; } });
		At(Off, 40.f); Next(6); break;
	case 6:   // the valve, the door
		At(PanAt); Hand(PanAt, 1, Pt(TEXT("IK_Lever_HoseL")), 1.2f, [Fu] { Fu().LeverWant[1] = 0.f; });
		Door(false); Next(7); break;
	case 7:   // unbonded last, its cable wound in
		At(Off, 50.f); At(GearSt); Hand(GearSt, 1, Gear, 1.8f, [Fu] { Fu().Clamp = FSvcRefuel::RightHand; });
		At(Off, 50.f); At(StBond); Hand(StBond, 1, Pt(TEXT("IK_BondClamp")), 1.6f, [Fu] { Fu().Clamp = FSvcRefuel::InPlace; }, true);
		Next(8); break;
	case 8:   // and the chocks
		At(StPl); Hand(StPl, 1, Pl1, 1.6f, [Fu] { Fu().Chock[1] = FSvcRefuel::RightHand; }, true);
		Hand(StPl, 0, Pl0, 1.6f, [Fu] { Fu().Chock[0] = FSvcRefuel::LeftHand; }, true);
		At(FrL, 40.f); At(FrR, 40.f); At(StCh);
		Hand(StCh, 1, Pt(TEXT("IK_Chock_1")), 1.4f, [Fu] { Fu().Chock[1] = FSvcRefuel::InPlace; }, true);
		Hand(StCh, 0, Pt(TEXT("IK_Chock_0")), 1.4f, [Fu] { Fu().Chock[0] = FSvcRefuel::InPlace; }, true);
		At(FrR, 40.f); At(FrL, 40.f); Next(9); break;
	default:
		F0.Chock[0] = F0.Chock[1] = F0.Clamp = F0.Dead = FSvcRefuel::InPlace;   // (nothing may be left out when it drives off)
		SvcLeave(Job); break;
	}
}

void AGSE::FuelLost(int32 Job)
{
	// its man vanished at work: what he had in his hands is back in its place, what is set stays; whoever takes over goes on
	// from the last stage finished
	FSvcJob* J = SvcJ(Job); if (!J || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Fu) return;
	FSvcRefuel& F = *Tractors[J->Ti].Fu;
	for (uint8* V : {&F.Noz, &F.Clamp, &F.Chock[0], &F.Chock[1], &F.Dead}) if (*V == FSvcRefuel::RightHand || *V == FSvcRefuel::LeftHand) *V = FSvcRefuel::InPlace;
	F.bPump = false; F.LiftWant = 0.f; F.GateWant = 0.f;
	if (F.Noz == FSvcRefuel::Put && !F.bConn) F.Noz = FSvcRefuel::InPlace;
	if (J->Phase != 2) return;
	if (J->Stage == 3 && F.bConn) J->Stage = 4;
	if (J->Stage == 5 && !F.bConn) { J->Stage = 6; if (J->Men.Num()) J->Men[0].bDown = true; }
	ADP_LOG(TEXT("GSE: the refueller stands at %s until somebody takes over (stage %d)"), *J->Reg, J->Stage);
}

// ------------------------------------------------------------------ in your hands (to-do 238, VehicleControls.h)

namespace
{
	enum : int32 { FuMPump, FuMHoseL, FuMHoseR, FuMDefuel, FuMLift, FuMGate, FuMDoor, FuMDead };
	enum : int32 { FuCChassis, FuCPlat, FuCGate, FuCDoor, FuCCouple, FuCGear, FuCPut0, FuCPut1, FuCStep };
	enum : int32 { FuSNozHome, FuSNozPlane, FuSClampHome, FuSClampGear, FuSChock0, FuSChock1, FuSPut0, FuSPut1, FuSClimb, FuSStop, FuSLamp };
	FTransform FuCMove(const FVector& V) { return FTransform(V); }
	FTransform FuCTurn(const FVector& P, float Deg) { return FTransform(FQuat::Identity, -P) * FTransform(FQuat(FuUp, FMath::DegreesToRadians(Deg)), P); }
	const TCHAR* FuCHeld(const FSvcRefuel& F)
	{
		return F.Noz == FSvcRefuel::YourHand ? TEXT("the fuel nozzle") : F.Clamp == FSvcRefuel::YourHand ? TEXT("the bonding clamp") : F.Chock[0] == FSvcRefuel::YourHand || F.Chock[1] == FSvcRefuel::YourHand ? TEXT("a wheel chock") : nullptr;
	}
	float FuCTop(const FSvcRefuel& F) { return FMath::Clamp(float(F.Couple.Z) - 13.f - FuFloor - 184.f, 0.f, FMath::Clamp(float(F.Couple.Z) - 35.f - 255.f, 0.f, 90.f)); }   // (as its crew raise it)
	bool FuCFlow(const FSvcRefuel& F, FString& Why)
	{
		if (F.bStop) Why = TEXT("the emergency stop is pressed"); else if (F.Clamp != FSvcRefuel::Put) Why = TEXT("the bonding clamp is not on the aircraft");
		else if (!F.bConn) Why = TEXT("the nozzle is not on the aircraft"); else if (F.LeverNow[1] < 0.95f) Why = TEXT("the left hose's lever is shut"); else if (F.LeverNow[0] < 0.95f) Why = TEXT("the pump's lever is off");
		else if (F.LeverNow[3] > 0.05f) Why = TEXT("the defuel lever is open"); else return true;
		return false;
	}
}

bool AGSE::FuelStowed(const FTractor& T) const
{
	if (!T.Fu || T.Job != EJob::None) return true; const FSvcRefuel& F = *T.Fu;
	for (int32 i = 0; i < 4; ++i) if (F.LeverNow[i] > 0.01f || F.LeverWant[i] > 0.01f) return false;
	return F.Noz == FSvcRefuel::InPlace && F.Clamp == FSvcRefuel::InPlace && F.Chock[0] == FSvcRefuel::InPlace && F.Chock[1] == FSvcRefuel::InPlace && F.Out == 0.f && F.Lift < 0.5f && F.LiftWant < 0.5f && F.Gate < 0.02f && F.GateWant < 0.02f && !F.bPump && !F.bConn && !F.bStop;
}

int32 AGSE::YouJob(const FTractor& T, const FVector& Pt, const FVector& Want, float Max, FVector& C, const AActor** Pl) const
{
	const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At); const FServiceSpec& S = ADPService::All()[T.Svc]; int32 Key = INDEX_NONE; double Bd = Max;
	for (const TPair<int32, FSvcJob>& J : SvcJobs)
	{
		const FServiceSpec& X = ADPService::All()[J.Value.Svc]; if (J.Value.Phase != 0 || X.bFuel != S.bFuel || X.Wet != S.Wet || (!S.bFuel && S.Wet != 1)) continue;
		const AActor* A = WetPlane(J.Value.StandX); if (!A) continue;
		const FVector P = Root.InverseTransformPosition(A->GetActorTransform().TransformPosition(Pt)); const double D = FVector::Dist(P, Want);
		if (D < Bd) { Bd = D; Key = J.Key; C = P; if (Pl) *Pl = A; }
	}
	return Key;
}

bool AGSE::YouServe(int32 Svc, float StandX) const
{
	const FServiceSpec& S = ADPService::All()[Svc]; if (!S.bFuel && S.Wet != 1) return false;
	for (const FTractor& T : Tractors) if (T.Job == EJob::None && ((S.bFuel && T.Fu && T.Fu->YouStandX == StandX && !FuelStowed(T)) || (S.Wet == 1 && T.Wa && !T.Wa->bToilet && T.Wa->YouStandX == StandX && !WaterStowed(T)))) return true;
	return false;
}

bool AGSE::YouDone(FTractor& T, float StandX, float Amount)
{
	const FServiceSpec& S = ADPService::All()[T.Svc];
	for (auto It = SvcJobs.CreateIterator(); It; ++It)
	{
		FSvcJob& J = It.Value(); const FServiceSpec& X = ADPService::All()[J.Svc]; if (J.Phase != 0 || X.bFuel != S.bFuel || X.Wet != S.Wet || J.StandX != StandX) continue;
		if (Amount > 0.f) { J.Litres = FMath::Max(0.f, J.Litres - Amount); if (J.Litres <= 0.f) ADP_LOG(TEXT("GSE: %s has from you all it asked for"), *J.Reg); return J.Litres > 0.f; }
		// (everything back in its place: what it needed is done; else a crew does the rest)
		if (J.Litres <= 1.f) { ADP_LOG(TEXT("GSE: %s served by you"), *J.Reg); It.RemoveCurrent(); }
		return false;
	}
	return false;
}

void AGSE::YouSee(FTractor& T)
{
	// (on a press only: the aircraft you stand under)
	FVector C; const AActor* Pl = nullptr; const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
	if (T.Fu && !T.Fu->bConn)
	{
		FSvcRefuel& F = *T.Fu; const FVector Want(FuWork.X, -118.f, 380.f); const int32 J = YouJob(T, FuCouplePlane, Want, 45.f, C, &Pl); F.bSeen = J != INDEX_NONE; F.SeenAt = GetWorld()->GetTimeSeconds();
		if (F.bSeen) { F.Couple = C; F.ClampAt = Root.InverseTransformPosition(Pl->GetActorTransform().TransformPosition(FuGear)); F.YouStandX = SvcJobs[J].StandX; }
	}
}

void AGSE::YouFuel(FTractor& T, float Dt)
{
	FSvcRefuel& F = *T.Fu; FString Why; if (F.YouStandX < -1e5f) { F.bPump = false; return; }
	if (F.bPump && !FuCFlow(*T.Fu, Why)) F.bPump = false;
	if (F.bPump) { const float L = Dt * (1000.f / 60.f) * F.Flow; T.bUsed = true; F.Litres += L; if (!YouDone(T, F.YouStandX, FMath::Max(L, 0.001f))) F.bPump = false; }
	else if (FuelStowed(T)) { YouDone(T, F.YouStandX, 0.f); F.YouStandX = -1e6f; F.bSeen = false; }
}

TSharedPtr<FVehTable> AGSE::FuelControls()
{
	using K = FVehControl; using FR = FSvcRefuel;
	const FVector L(0, -1, 0), R(0, 1, 0), No(0, 0, 0);
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>(); TArray<K>& C = Tb->Controls;
	auto Mo = [&C](const TCHAR* Part, const TCHAR* Pt, K::EKind Kind, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 M, float A, float Bv, bool bTurns = true) { K X; X.Part = Part; X.Point = Pt; X.Kind = Kind; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Motion = M; X.A = A; X.B = Bv; X.bTurns = bTurns; C.Add(X); };
	auto Sw = [&C](const TCHAR* Part, const TCHAR* Pt, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 S) { K X; X.Part = Part; X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Switch = S; C.Add(X); };
	auto Box = [&Tb](const FVector& Mn, const FVector& Mx, int32 Fr, TFunction<bool(const FTractor&)> On = nullptr) { FVehSolid S; S.Min = Mn; S.Max = Mx; S.Frame = Fr; S.On = MoveTemp(On); Tb->Solids.Add(S); };

	// the panel behind the left door
	Mo(TEXT("PanelDoorL"), TEXT("IK_Handle_PanelDoorL_Out"), K::Hand, TEXT("Panel door"), FuCDoor, No, FuMDoor, 0.f, 1.f);
	Mo(TEXT("Lever_Pump"), TEXT("IK_Lever_Pump"), K::Lever, TEXT("Pump on / off"), FuCChassis, L, FuMPump, 0.f, 1.f);
	Mo(TEXT("Lever_HoseL"), TEXT("IK_Lever_HoseL"), K::Lever, TEXT("Left hose open / shut"), FuCChassis, L, FuMHoseL, 0.f, 1.f);
	Mo(TEXT("Lever_HoseR"), TEXT("IK_Lever_HoseR"), K::Lever, TEXT("Right hose open / shut"), FuCChassis, L, FuMHoseR, 0.f, 1.f);
	Mo(TEXT("Lever_Defuel"), TEXT("IK_Lever_Defuel"), K::Lever, TEXT("Defuel open / shut"), FuCChassis, L, FuMDefuel, 0.f, 1.f);
	Mo(TEXT("Deadman_Button"), TEXT("IK_Deadman"), K::Hold, TEXT("Dead man: fuel flows while you hold it"), FuCChassis, L, FuMDead, 0.f, 1.f, false);
	Sw(TEXT("IK_Button_EStop"), TEXT("IK_Button_EStop"), TEXT("Emergency stop"), FuCChassis, L, FuSStop); Sw(TEXT("IK_Button_LampOn"), TEXT("IK_Button_LampOn"), TEXT("Work lights"), FuCChassis, L, FuSLamp);
	Sw(TEXT("ShutL_Button"), TEXT("IK_EmergencyL"), TEXT("Emergency shut-off"), FuCChassis, L, FuSStop); Sw(TEXT("ShutR_Button"), TEXT("IK_EmergencyR"), TEXT("Emergency shut-off"), FuCChassis, R, FuSStop);
	// what is carried: from its place, to where it goes
	Sw(TEXT("Chock_0"), TEXT("IK_Chock_0"), TEXT("Chock"), FuCChassis, No, FuSChock0); Sw(TEXT("Chock_1"), TEXT("IK_Chock_1"), TEXT("Chock"), FuCChassis, No, FuSChock1);
	Sw(TEXT("Chock_0"), TEXT("IK_Chock_0"), TEXT("Chock"), FuCPut0, No, FuSPut0); Sw(TEXT("Chock_1"), TEXT("IK_Chock_1"), TEXT("Chock"), FuCPut1, No, FuSPut1);
	Sw(TEXT("BondClamp"), TEXT("IK_BondClamp"), TEXT("Bonding clamp"), FuCChassis, No, FuSClampHome); Sw(TEXT("BondClamp"), TEXT("IK_BondClamp"), TEXT("Bonding clamp"), FuCGear, No, FuSClampGear);
	Sw(TEXT("Nozzle_L"), TEXT("PIV_Nozzle_L"), TEXT("Nozzle"), FuCChassis, No, FuSNozHome); Sw(TEXT("Nozzle_L"), TEXT("PIV_Nozzle_L"), TEXT("Nozzle"), FuCCouple, No, FuSNozPlane);
	// the platform
	Mo(TEXT("GatePlatform"), TEXT("IK_Handle_GatePlatform"), K::Hand, TEXT("Platform gate"), FuCGate, No, FuMGate, 0.f, 1.f);
	Mo(TEXT("LeverPlatform"), TEXT("IK_LeverPlatform"), K::Lever, TEXT("Platform up / down"), FuCPlat, No, FuMLift, 0.f, 90.f);
	Sw(TEXT("IK_Step_Platform_3"), TEXT("IK_Step_Platform_3"), TEXT("Steps"), FuCStep, No, FuSClimb);

	Tb->Frame = [](const AGSE& G, const FTractor& T, int32 Fr) -> FTransform
	{
		const FR& F = *T.Fu;
		switch (Fr)
		{
		case FuCPlat: return FuCMove(FuUp * F.Lift);
		case FuCGate: return FuCTurn(G.SvcP(T, TEXT("PIV_GatePlatform")), -90.f * FMath::SmoothStep(0.f, 1.f, F.Gate)) * FuCMove(FuUp * F.Lift);
		case FuCDoor: return T.SvcDoors.Num() ? FuCTurn(T.SvcDoors[0].Piv, ADPService::All()[T.Svc].Doors[0].Deg * T.SvcDoors[0].Now) : FTransform::Identity;
		case FuCCouple: return FuCMove((F.Noz == FR::Put ? F.NozNow : F.Couple - FuUp * 7.f) - G.SvcP(T, TEXT("PIV_Nozzle_L")));
		case FuCGear: return FuCMove(F.ClampAt - G.SvcP(T, TEXT("IK_BondClamp")));
		case FuCPut0: case FuCPut1: return FuCMove(G.SvcP(T, Fr == FuCPut1 ? TEXT("IK_ChockPlace_1") : TEXT("IK_ChockPlace_0")) + FuUp * 23.2f - G.SvcP(T, Fr == FuCPut1 ? TEXT("IK_Chock_1") : TEXT("IK_Chock_0")));
		case FuCStep: return FuCMove(FuUp * (T.YouOn ? 80.f : 0.f));   // (from up there you take hold above it)
		default: return FTransform::Identity;
		}
	};
	Tb->Now = [](const FTractor& T, int32 M) { const FR& F = *T.Fu; return M <= FuMDefuel ? F.LeverNow[M] : M == FuMLift ? F.Lift : M == FuMGate ? F.Gate : M == FuMDoor && T.SvcDoors.Num() ? T.SvcDoors[0].Now : 0.f; };
	Tb->Want = [](FTractor& T, int32 M, float To)
	{
		FR& F = *T.Fu; F.bLive = true;
		if (M <= FuMDefuel) F.LeverWant[M] = To; else if (M == FuMLift) F.LiftWant = To; else if (M == FuMGate) F.GateWant = To; else if (M == FuMDoor) { if (T.SvcDoors.Num()) T.SvcDoors[0].Want = To; } else F.bPump = To > 0.5f;
	};
	Tb->May = [](const AGSE& G, const FTractor& T, int32 M, float To, FString& Why)
	{
		const FR& F = *T.Fu;
		if (M == FuMDead) return FuCFlow(*T.Fu, Why);
		if (M == FuMLift) { if (F.bStop) Why = TEXT("the emergency stop is pressed"); else if (F.Gate > 0.02f) Why = TEXT("the gate is open"); else if (!T.YouOn) Why = TEXT("only from on the platform"); else if (F.bConn) Why = TEXT("the nozzle is on the aircraft"); return Why.IsEmpty(); }
		if (M == FuMGate) { if (F.Lift > 0.5f) Why = TEXT("the platform is up"); else if (T.YouOn && T.YouAt.X < -455.f) Why = TEXT("you stand in its way"); return Why.IsEmpty(); }
		return true;
	};
	Tb->End = [](const FTractor& T, int32 M, float E) { return M == FuMLift && T.Fu->bSeen ? FMath::Min(E, FuCTop(*T.Fu)) : E; };   // (never up into the wing)
	Tb->Press = [](AGSE& G, int32 Ti, const K& X) { FTractor& T = G.Tractors[Ti]; if (X.Motion == FuMLift && G.GetWorld()->GetTimeSeconds() - T.Fu->SeenAt > 5.) G.YouSee(T); };
	Tb->Say = [](const AGSE& G, const FTractor& T, const K& X) -> FString
	{
		const FR& F = *T.Fu; const bool bPanel = FCString::Strncmp(X.Point, TEXT("IK_Lever_"), 9) == 0 || FCString::Strncmp(X.Point, TEXT("IK_Button_"), 10) == 0 || X.Motion == FuMDead;
		if (bPanel && T.SvcDoors.Num() && T.SvcDoors[0].Now < 0.5f) return FString();   // (behind its door)
		auto Two = [](uint8 M, bool bHome, const TCHAR* Take, const TCHAR* Back) { return FString(M == (bHome ? FR::InPlace : FR::Put) ? Take : M == FR::YourHand ? Back : TEXT("")); };
		switch (X.Switch)
		{
		case FuSNozHome: return Two(F.Noz, true, TEXT("Take the nozzle, the hose comes with it"), TEXT("Put the nozzle back"));
		case FuSNozPlane: return F.bSeen || F.bConn ? Two(F.Noz, false, TEXT("Take the nozzle off"), TEXT("Connect the nozzle")) : FString();
		case FuSClampHome: return Two(F.Clamp, true, TEXT("Take the bonding clamp"), TEXT("Put the bonding clamp back"));
		case FuSClampGear: return F.bSeen || F.Clamp == FR::Put ? Two(F.Clamp, false, TEXT("Take the bonding clamp off"), TEXT("Clamp it on the gear leg")) : FString();
		case FuSChock0: case FuSChock1: return Two(F.Chock[X.Switch - FuSChock0], true, TEXT("Take the chock"), TEXT("Put the chock back"));
		case FuSPut0: case FuSPut1: return Two(F.Chock[X.Switch - FuSPut0], false, TEXT("Take the chock away"), TEXT("Put the chock at the wheel"));
		case FuSClimb: return T.YouOn ? TEXT("Climb down") : TEXT("Climb onto the platform");
		case FuSStop: return F.bStop ? TEXT("Pull the emergency stop out") : FString(X.Label);
		default: return X.Label;
		}
	};
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 S, FString& Why)
	{
		const FTractor& T = G.Tractors[Ti]; const FR& F = *T.Fu; const TCHAR* H = FuCHeld(F); const bool bChocked = F.Chock[0] == FR::Put && F.Chock[1] == FR::Put;
		const bool bTake = (S == FuSNozHome && F.Noz == FR::InPlace) || (S == FuSNozPlane && F.Noz == FR::Put) || (S == FuSClampHome && F.Clamp == FR::InPlace) || (S == FuSClampGear && F.Clamp == FR::Put) || ((S == FuSChock0 || S == FuSChock1) && F.Chock[S - FuSChock0] == FR::InPlace) || ((S == FuSPut0 || S == FuSPut1) && F.Chock[S - FuSPut0] == FR::Put);
		if (bTake && H) Why = FString::Printf(TEXT("you have %s in your hand"), H);
		else if (bTake && (T.Player.IsValid() || T.Speed > 1.f || T.Path.Num())) Why = TEXT("it is not parked");
		else if (S == FuSNozPlane && F.Noz == FR::Put && (F.bPump || F.Flow > 0.05f)) Why = TEXT("fuel is flowing");
		else if (S == FuSNozPlane && F.Noz != FR::Put && F.Clamp != FR::Put) Why = TEXT("the bonding clamp first");
		else if (S == FuSClampGear && F.Clamp == FR::Put && (F.bConn || F.Noz != FR::InPlace)) Why = TEXT("the nozzle back in its place first");
		else if (S == FuSClampGear && F.Clamp != FR::Put && !bChocked) Why = TEXT("the chocks at the wheel first");
		else if ((S == FuSPut0 || S == FuSPut1) && F.Chock[S - FuSPut0] == FR::Put && (F.Clamp != FR::InPlace || F.Noz != FR::InPlace)) Why = TEXT("nozzle and bonding clamp back in their places first");
		else if (S == FuSClimb && F.Gate < 0.9f) Why = TEXT("the gate is shut");
		else if (S == FuSClimb && F.Lift > 0.5f) Why = TEXT("the platform is up");
		return Why.IsEmpty();
	};
	Tb->Switch = [](AGSE& G, int32 Ti, int32 S)
	{
		FTractor& T = G.Tractors[Ti]; FR& F = *T.Fu; F.bLive = true;
		auto Turn = [](uint8& M, uint8 Place) { const bool bGet = M == Place; M = bGet ? FR::YourHand : Place; return bGet; };
		switch (S)
		{
		case FuSNozHome: if (Turn(F.Noz, FR::InPlace)) { F.NozYaw = 0.f; G.YouSee(T); } break;
		case FuSNozPlane: F.bConn = !Turn(F.Noz, FR::Put); F.NozYaw = -90.f; if (F.bConn) { F.NozAt = F.Couple - FuUp * 7.f; F.Litres = 0.f; } ADP_LOG(TEXT("GSE: you %s the nozzle of the %s"), F.bConn ? TEXT("connect") : TEXT("take off"), *T.Name); break;
		case FuSClampHome: if (Turn(F.Clamp, FR::InPlace)) G.YouSee(T); break;
		case FuSClampGear: Turn(F.Clamp, FR::Put); break;
		case FuSChock0: case FuSChock1: if (Turn(F.Chock[S - FuSChock0], FR::InPlace)) G.YouSee(T); break;
		case FuSPut0: case FuSPut1: Turn(F.Chock[S - FuSPut0], FR::Put); break;
		case FuSStop: F.bStop = !F.bStop; if (F.bStop) { F.bPump = false; F.LiftWant = F.Lift; } break;
		case FuSLamp: T.bSwWork = !T.bSwWork; break;
		default: return false;
		}
		return true;
	};
	Tb->Held = [](const FTractor& T) { return T.Fu ? FuCHeld(*T.Fu) : nullptr; };
	Tb->Leash = [](const AGSE& G, const FTractor& T, FVector& From) { const bool bNoz = T.Fu->Noz == FR::YourHand; From = G.SvcP(T, bNoz ? TEXT("IK_HoseOut_L") : TEXT("IK_BondOut")); return bNoz ? 2600.f : 2500.f; };
	Tb->Drop = [](FTractor& T) { FR& F = *T.Fu; for (uint8* M : {&F.Noz, &F.Clamp, &F.Chock[0], &F.Chock[1]}) if (*M == FR::YourHand) *M = FR::InPlace; };
	Tb->Climb = [](const AGSE& G, int32 Ti, int32 S, float& Secs) -> TFunction<void(float, float, FDriverPose&)> { if (S != FuSClimb) return nullptr; const bool bUp = !G.Tractors[Ti].YouOn; Secs = bUp ? 6.5f : 6.f; return G.FuelPlatFn(Ti, bUp); };
	Tb->Aboard = [](const FTractor& T, const FVector& P) -> uint8 { return P.X > -500.f && P.X < -385.f && FMath::Abs(P.Y) < 104.f && FMath::Abs(P.Z - FuFloor - T.Fu->Lift) < 40.f ? 1 : 0; };
	Tb->Stowed = [](const AGSE& G, const FTractor& T) { return G.FuelStowed(T); };
	Tb->Stow = [](AGSE&, FTractor& T)
	{
		FR& F = *T.Fu; F.bPump = F.bConn = F.bStop = false; F.Noz = F.Clamp = F.Chock[0] = F.Chock[1] = FR::InPlace; for (float& V : F.LeverWant) V = 0.f; F.LiftWant = F.GateWant = 0.f; F.bLive = true;
		T.bSwWork = false; for (FTractor::FSvcDoor& D : T.SvcDoors) D.Want = 0.f;
	};
	// chassis, cab and tank in one; the platform: its floor, rails, gate
	Box(FVector(-394, -107, 10), FVector(500, 107, 330), FuCChassis); Box(FVector(-480, -100, 10), FVector(-394, 100, 60), FuCChassis);
	Box(FVector(-498, -106, 139), FVector(-394, 106, 145), FuCPlat);
	for (const float S : {-1.f, 1.f}) { Box(FVector(-498, S > 0 ? 100 : -106, 145), FVector(-394, S > 0 ? 106 : -100, 255), FuCPlat); Box(FVector(-498, S > 0 ? 36 : -106, 145), FVector(-492, S > 0 ? 106 : -36, 255), FuCPlat); }
	Box(FVector(-498, -36, 145), FVector(-492, 36, 255), FuCPlat, [](const FTractor& T) { return T.Fu->Gate < 0.9f; });
	return Tb;
}
