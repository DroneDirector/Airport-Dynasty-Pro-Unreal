// The potable water truck and the toilet truck at work (to-do 212; the vehicles, their driving and their people are the
// shared ones of GSEService.cpp). Each has one hired driver. It comes from behind, turns round behind the tail and backs under
// it to the aircraft's service panel; the driver chocks it, goes up the steps onto the work platform and does the work with
// his hands: water - the coupling off its holder and up onto the panel, the hose off its reel behind it, the valve, the pump,
// 200 l, back; toilets - the splash tray, the cap, both couplings, the platform raised with its held lever, waste hose and
// rinse hose on, down, the drain lever, 4 minutes drained, 2 rinsed, and all of it back. One stage after the other
// (FSvcJob::Stage): whoever takes over goes on from the last one finished.
// Under the tail there is room for one of the two (and the toilet truck stands where the catering truck stands at the rear
// right door): each claims the place when it leaves its box (WetBusy), and gives up in the end if it never comes free.
// The panels: 26.2 m behind the nose wheel, water 0.30 m left, 2.93 m up, toilets 0.80 m right, 3.13 m up - on the game's
// A320 (the real ones stand a third of a metre lower); it has no modelled panels, so the couplings go to these points.
#include "GSEWater.h"
#include "GSE.h"
#include "GSECater.h"
#include "ServiceVehicles.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "VehicleControls.h"
#include "VehicleRiders.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "CableComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	const FVector WaUp(0, 0, 1);
	const FVector WaPanel[2] = {FVector(-30.f, 2620.f, 327.f), FVector(80.f, 2620.f, 347.f)};   // (the aircraft's root, at the nose wheel: x its right, y aft)
	const FVector WaWant[2] = {FVector(-240.f, -30.f, 327.f), FVector(-212.f, 5.f, 347.f)};     // (where they are from the truck in its place)
	const FVector WaWork[2] = {FVector(-240.f, 6.f, 0.f), FVector(-246.f, 4.f, 0.f)};           // (where the driver stands on the platform for them)
	const float WaRinseY = 22.f, WaGiveUp = 2400.f;   // (the rinse nipple beside the drain; 40 min of waiting for the place)
	const float ToRinse = 550.f, ToWaste = 1500.f, ToDrainL = 0.7f, ToRinseL = 1.f / 3.f;   // (its tanks; 168 l drained in 240 s, 40 l rinsed with in 120 s)
	const float WaRate = 50.f / 60.f, WaFill = 240.f, WaDrain = 240.f, WaRinseS = 120.f;   // (200 l at 50 l a minute)
	float WaFloor(const FSvcWet& F) { return F.bToilet ? 62.f + F.Lift : 100.f; }
	FQuat WaRot(const FVector& Ax, float Deg) { return FQuat(Ax, FMath::DegreesToRadians(Deg)); }
}

// ------------------------------------------------------------------ its parts

void AGSE::WetDress(FTractor& T)
{
	ABabylonModel* M = T.M.Get(); if (!M) return;
	T.Wa = MakeShared<FSvcWet>(); FSvcWet& F = *T.Wa; F.bToilet = ADPService::All()[T.Svc].Wet == 2; const bool bT = F.bToilet;
	auto Pre = [&](const FString& N, const FVector& P) { return M->BindGroup({N}, true, FTransform(P)); };
	auto One = [&](const FString& N, const FVector& P) { return M->BindGroupWhere([N](const FString& A, int32, const FVector&) { return A == N; }, FTransform(P)); };   // (Coupling, not CouplingHolder)
	auto Thing = [&](int32 i, const TCHAR* N, const TCHAR* At, const TCHAR* Hand) { FSvcWet::FThing& X = F.Th[i]; X.Piv = X.Now = SvcP(T, At); X.Grip = Hand ? X.Piv - SvcP(T, Hand) : FVector::ZeroVector; X.G = One(N, X.Piv); };
	Thing(FSvcWet::Chock0, TEXT("Chock_0"), TEXT("IK_Chock_0"), nullptr); Thing(FSvcWet::Chock1, TEXT("Chock_1"), TEXT("IK_Chock_1"), nullptr);
	const FString Turns = bT ? TEXT("HoseTurn_RinseReel_") : TEXT("HoseTurn_HoseReel_"); const FVector ReelP = SvcP(T, bT ? TEXT("PIV_RinseReel") : TEXT("PIV_HoseReel"));
	if (bT)
	{
		Thing(FSvcWet::Main, TEXT("WasteCoupling"), TEXT("PIV_WasteCoupling"), nullptr); Thing(FSvcWet::Rinse, TEXT("RinseCoupling"), TEXT("PIV_RinseCoupling"), TEXT("IK_RinseCoupling_Hand")); Thing(FSvcWet::Cap, TEXT("WasteCap"), TEXT("PIV_WasteCap"), nullptr);
		F.GLift = M->BindGroupWhere([](const FString& N, int32, const FVector&) { for (const TCHAR* P : {TEXT("Platform"), TEXT("Lift"), TEXT("RailPlatform"), TEXT("PlatCtl"), TEXT("NamePanel_Rear")}) if (N.StartsWith(P)) return true; return false; }, FTransform::Identity);
		F.GPLever = Pre(TEXT("Lever_Platform"), SvcP(T, TEXT("PIV_Lever_Platform"))); F.GTray = Pre(TEXT("SplashTray"), SvcP(T, TEXT("PIV_SplashTray"))); F.GValve = Pre(TEXT("Lever_Vent"), SvcP(T, TEXT("PIV_Lever_Vent")));
		F.GLower = Pre(TEXT("Lever_Lower"), SvcP(T, TEXT("PIV_Lever_Lower"))); F.GFill = Pre(TEXT("Lever_Dump"), SvcP(T, TEXT("PIV_Lever_Dump")));
		F.GReel = Pre(TEXT("RinseReel"), ReelP); F.GTail = Pre(TEXT("RinseTail"), SvcP(T, TEXT("IK_RinseOut"))); F.GHose = One(TEXT("WasteHose"), SvcP(T, TEXT("IK_WasteHose_In")));
	}
	else
	{
		Thing(FSvcWet::Main, TEXT("Coupling"), TEXT("PIV_Coupling"), TEXT("IK_Coupling_Hand"));
		F.GFill = Pre(TEXT("Lever_Fill"), SvcP(T, TEXT("PIV_Lever_Fill")));
		F.GValve = Pre(TEXT("Lever_Valve"), SvcP(T, TEXT("PIV_Lever_Valve"))); F.GReel = Pre(TEXT("HoseReel"), ReelP); F.GTail = Pre(TEXT("HoseTail"), SvcP(T, TEXT("IK_HoseOut")));
	}
	for (int32 i = 0; i < (bT ? 15 : 14); ++i) F.GTurn.Add(Pre(FString::Printf(TEXT("%s%02d"), *Turns, i), ReelP));
	F.GGate = Pre(TEXT("GatePlatform"), SvcP(T, TEXT("PIV_GatePlatform")));
	auto Cable = [&](const TCHAR* Name, float Width, const FString& Like)
	{
		UCableComponent* C = NewObject<UCableComponent>(M, Name); C->SetupAttachment(M->GetRootComponent());
		C->CableWidth = Width; C->NumSegments = 30; C->NumSides = 12; C->SolverIterations = 16; C->SubstepTime = 0.01f; C->bEnableCollision = false; C->bEnableStiffness = true; C->CableLength = 100.f;
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> G; M->InstancesNamed(*Like, true, G); if (G.Num()) C->SetMaterial(0, G[0].Get<0>()->GetMaterial(0));
		C->SetVisibility(false); C->bAutoActivate = false; C->RegisterComponent(); M->AddInstanceComponent(C); C->SetActive(false); C->SetComponentTickEnabled(false);
		return C;
	};
	if (bT) { F.Hose = Cable(TEXT("WasteHoseOut"), 11.f, TEXT("WasteHose")); F.RinseHose = Cable(TEXT("RinseHoseOut"), 3.2f, Turns + TEXT("00")); }
	else F.Hose = Cable(TEXT("WaterHoseOut"), 2.8f, Turns + TEXT("00"));
}

void AGSE::WetParts(FTractor& T, float Dt)
{
	ABabylonModel* M = T.M.Get(); if (!M || !T.Wa) return;
	FSvcWet& F = *T.Wa; const bool bT = F.bToilet; if (!T.bUsed) { F.Tank = bT ? ToRinse : 2500.f; F.Waste = 0.f; }   // (filled up and emptied in its box)
	if (!F.bLive) return;
	auto Put = [M](int32 G, const FTransform& X) { if (G != INDEX_NONE) M->MoveGroup(G, X); };
	auto Show = [](const FVector& P, bool b) { return FTransform(FQuat::Identity, P, b ? FVector::OneVector : FVector(0.001f)); };
	const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At); const FVector Side = FVector::RightVector;
	// the platform, its gate, levers, the splash tray
	const float WasLift = F.Lift; if (!bT || !WetLiftHeld(T, F.LiftWant)) F.Lift = FMath::FInterpConstantTo(F.Lift, F.LiftWant, Dt, 8.f); F.Gate = FMath::FInterpConstantTo(F.Gate, F.GateWant, Dt, 1.1f);
	F.Valve = FMath::FInterpConstantTo(F.Valve, F.ValveWant, Dt, 1.8f); F.Tray = FMath::FInterpConstantTo(F.Tray, F.TrayWant, Dt, 1.2f);
	if (bT)
	{
		Put(F.GLift, FTransform(WaUp * F.Lift)); Put(F.GTray, FTransform(WaRot(Side, -85.f * F.Tray), SvcP(T, TEXT("PIV_SplashTray"))));
		Put(F.GPLever, FTransform(WaRot(Side, F.Lift > WasLift ? -15.f : F.Lift < WasLift ? 15.f : 0.f), SvcP(T, TEXT("PIV_Lever_Platform")) + WaUp * F.Lift));
		if (F.Lift != WasLift) for (UActorComponent* C : M->GetComponents()) if (C && C->ComponentHasTag(TEXT("SvcRear"))) Cast<USceneComponent>(C)->AddRelativeLocation(WaUp * (F.Lift - WasLift));
	}
	Put(F.GGate, FTransform(WaRot(WaUp, 80.f * FMath::SmoothStep(0.f, 1.f, F.Gate)), SvcP(T, TEXT("PIV_GatePlatform")) + WaUp * F.Lift));
	Put(F.GValve, FTransform(WaRot(Side, 60.f * F.Valve), SvcP(T, bT ? TEXT("PIV_Lever_Vent") : TEXT("PIV_Lever_Valve"))));
	if (bT)
	{
		F.Fill = FMath::FInterpConstantTo(F.Fill, F.FillWant, Dt, 1.8f); Put(F.GFill, FTransform(WaRot(FVector(1, 0, 0), 45.f * F.Fill), SvcP(T, TEXT("PIV_Lever_Dump"))));
		Put(F.GLower, FTransform(WaRot(FVector(1, 0, 0), F.Lift > WasLift ? -15.f : F.Lift < WasLift ? 15.f : 0.f), SvcP(T, TEXT("PIV_Lever_Lower"))));
	}
	else
	{
		// (the fill valve swings out through its door: that stays open as long as it does)
		F.Fill = FMath::FInterpConstantTo(F.Fill, F.FillWant, Dt, 1.8f); Put(F.GFill, FTransform(WaRot(WaUp, 70.f * F.Fill), SvcP(T, TEXT("PIV_Lever_Fill"))));
		if (F.FillWant > 0.5f && T.SvcDoors.IsValidIndex(1)) T.SvcDoors[1].Want = 1.f;
	}
	// the carried things: in their place, in a hand, or put somewhere
	const ATugs* G = Tg(); const USkeletalMeshComponent* B = F.Ci != INDEX_NONE ? Body(F.Ci) : nullptr;
	const bool bBody = B && G && G->Crew[F.Ci].A.IsValid() && !G->Crew[F.Ci].A->IsHidden();
	bool bAll = true; if (T.Job == EJob::None) { if (bT) YouToilet(T, Dt); else YouWater(T, Dt); }
	for (int32 i = 0; i < FSvcWet::NThings; ++i)
	{
		FSvcWet::FThing& X = F.Th[i]; if (X.G == INDEX_NONE) continue;
		FVector Want = X.Piv; float Tip = 0.f;
		if (X.Mode == FSvcWet::Put) { Want = X.At + (X.bRide ? WaUp * F.Lift : FVector::ZeroVector); Tip = X.Tip; }
		else if (X.Mode == FSvcWet::YourHand) { Want = T.YouHand; Tip = bT && i == FSvcWet::Main ? 90.f : 0.f; }
		else if (X.Mode != FSvcWet::InPlace)
		{
			// (its man not in sight: it stays where it is)
			Want = bBody ? Root.InverseTransformPosition(B->GetBoneLocation(X.Mode == FSvcWet::RightHand ? TEXT("hand_r") : TEXT("hand_l"))) + X.Grip : X.Now; Tip = bBody ? X.Tip : X.TipNow;
		}
		X.Now = FMath::VInterpTo(X.Now, Want, Dt, 14.f); X.TipNow = FMath::FInterpTo(X.TipNow, Tip, Dt, 6.f);
		if (X.Mode != FSvcWet::InPlace) bAll = false;
		else { X.Turn = 0.f; if (FVector::Dist(X.Now, X.Piv) < 1.f) { X.Now = X.Piv; X.TipNow = 0.f; } else bAll = false; }
		Put(X.G, FTransform(WaRot(Side, X.TipNow) * WaRot(WaUp, X.Turn), X.Now));
	}
	auto Run = [&](UCableComponent* C, const FVector& From, const FVector& To, float Least, bool bOn)
	{
		if (!C) return 0.f;
		if (!bOn) { if (C->IsVisible()) { C->SetVisibility(false); C->SetActive(false); C->SetComponentTickEnabled(false); } return 0.f; }
		const FVector A = Root.TransformPosition(From), E = Root.TransformPosition(To); C->SetWorldLocation(A);
		C->EndLocation = C->GetComponentTransform().InverseTransformPosition(E); C->CableLength = FMath::Max(float(FVector::Dist(A, E)) * 1.04f, Least);
		if (!C->IsVisible()) { C->SetVisibility(true); C->SetActive(true); C->SetComponentTickEnabled(true); }
		return C->CableLength;
	};
	// the hoses: the reel turns and its turns go from the outside in as far as the coupling goes
	const FSvcWet::FThing &Mn = F.Th[FSvcWet::Main], &Rl = F.Th[bT ? FSvcWet::Rinse : FSvcWet::Main];
	auto Out = [](const FSvcWet::FThing& X) { return !(X.Mode == FSvcWet::InPlace && X.Now == X.Piv); };
	if (bT)
	{
		const FVector In = SvcP(T, TEXT("IK_WasteHose_In"));
		Run(F.Hose, In, Mn.Now + WaRot(Side, Mn.TipNow).RotateVector(SvcP(T, TEXT("IK_WasteCoupling_Hose")) - Mn.Piv), 270.f, Out(Mn)); Put(F.GHose, Show(In, !Out(Mn)));
	}
	const FVector From = SvcP(T, bT ? TEXT("IK_RinseOut") : TEXT("IK_HoseOut"));
	// (the water hose you pulled off stays out until it is wound in)
	const FVector HoseEnd = Rl.Now + SvcP(T, bT ? TEXT("IK_RinseCoupling_Hose") : TEXT("IK_Coupling_Hose")) - Rl.Piv; F.Line.Need = FMath::Max(30.f, float(FVector::Dist(From, HoseEnd)) * 1.04f);
	const float Least = F.Line.Tick(Dt, Rl.Mode == FSvcWet::YourHand, !Out(Rl)); const bool bOut = Out(Rl) || F.Line.bHand;
	const float L = Run(bT ? F.RinseHose : F.Hose, From, HoseEnd, FMath::Max(30.f, Least), bOut);
	Put(F.GTail, Show(From, !bOut));
	if (!FMath::IsNearlyEqual(L, F.OutWas, 0.05f))
	{
		F.Reel += (L - F.OutWas) / 17.f; F.OutWas = L;
		const FTransform X0(FQuat(Side, -F.Reel), SvcP(T, bT ? TEXT("PIV_RinseReel") : TEXT("PIV_HoseReel"))); Put(F.GReel, X0);
		float Left = L; const float Per = bT ? 75.f : 98.f;
		for (int32 i = F.GTurn.Num() - 1; i >= 0; --i) { FTransform X = X0; if (Left >= Per * 0.5f) X.SetScale3D(FVector(0.001f)); Left -= Per; Put(F.GTurn[i], X); }
	}
	const int32 Me = int32(&T - Tractors.GetData()); bool bJob = false; for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Phase == 2 && P.Value.Ti == Me) bJob = true;
	if (!bJob && bAll && F.Lift == F.LiftWant && F.Gate == F.GateWant && F.Valve == F.ValveWant && F.Tray == F.TrayWant && F.Fill == 0.f && F.FillWant == 0.f && L == 0.f) F.bLive = false;
}

void AGSE::WetTick(FSvcJob& J, float Dt)
{
	if (J.Phase != 2 || !Tractors.IsValidIndex(J.Ti) || !Tractors[J.Ti].Wa || !Tractors[J.Ti].Wa->bRun) return;
	J.Pumped = FMath::Min(J.Litres, J.Pumped + Dt / J.Slow);   // (seconds)
	if (FSvcWet& F = *Tractors[J.Ti].Wa; F.bToilet) { const float R = J.Pumped > J.Litres - WaRinseS ? ToRinseL * Dt : 0.f; Tractors[J.Ti].bUsed = true; F.Tank = FMath::Max(0.f, F.Tank - R); F.Waste = FMath::Min(ToWaste, F.Waste + (R > 0.f ? R : ToDrainL * Dt)); }
	else { Tractors[J.Ti].bUsed = true; Tractors[J.Ti].Wa->Tank = FMath::Max(0.f, Tractors[J.Ti].Wa->Tank - Dt * WaRate); }
}

// ------------------------------------------------------------------ its place under the tail: one at a time

const AActor* AGSE::WetPlane(float StandX) const
{
	if (const FHallStair* H = HallOf(StandX)) return H->Plane.Get();   // (in a hangar hall: the towed one)
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(StandX, S, N, Sp); const AActor* Pl = nullptr; double Bd = 900.0; const FVector Nose = PlanT(S, S, N);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		for (const FName& Tg2 : It->Tags)
		{
			const FString Ts = Tg2.ToString(); if (!Ts.StartsWith(TEXT("SIM/")) && !Ts.StartsWith(TEXT("MOVE/"))) continue;
			const double D = FVector::Dist2D(It->GetActorLocation(), Nose); if (D < Bd) { Bd = D; Pl = *It; }
		}
	return Pl;
}

bool AGSE::WetAt(float StandX, bool bToiletOnly, int32 NotTi) const
{
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		const FTractor& O = Tractors[i];
		if (i != NotTi && O.Wa && (O.Wa->bToilet || !bToiletOnly) && FMath::Abs(O.StandX - StandX) < 1.f && (O.Job == EJob::Out || O.Job == EJob::AtStand)) return true;
		if (i != NotTi && O.Wa && (O.Wa->bToilet || !bToiletOnly) && O.Job == EJob::None && FMath::Abs(O.Wa->YouStandX - StandX) < 1.f && !WaterStowed(O)) return true;   // (you are working there)
	}
	return false;
}

bool AGSE::WetBusy(int32 Ti, float StandX, FString& Why) const
{
	if (WetAt(StandX, false, Ti)) { Why = Tractors[Ti].Wa->bToilet ? TEXT("the water truck is under the tail") : TEXT("the toilet truck is under the tail"); return true; }
	if (Tractors[Ti].Wa->bToilet)
		for (const TPair<int32, FSvcJob>& P : SvcJobs)
		{
			// (the catering truck: before its first trolley at the front door it waits for the toilet truck, from then on the other way round)
			const FSvcJob& J = P.Value; if (FMath::Abs(J.StandX - StandX) > 1.f || !Tractors.IsValidIndex(J.Ti) || !Tractors[J.Ti].Ct) continue;
			if ((J.Phase == 2 && (J.CtDoor == 1 || J.RowsDone > 0)) || (J.Phase == 3 && Tractors[J.Ti].Job == EJob::AtStand)) { Why = TEXT("the catering truck is at the rear right door first"); return true; }
		}
	return false;
}

bool AGSE::WetMay(int32 Job)
{
	// in its box: may it go now? (no: asked again, and in the end given up - nobody waits for good)
	FSvcJob* J = SvcJ(Job); if (!J) return false;
	const int32 Ti = J->Ti; FString Why; const bool bPlane = WetPlane(J->StandX) != nullptr;
	if (bPlane && !WetBusy(Ti, J->StandX, Why)) { J->Why.Reset(); return true; }
	J->Why = Why;
	if ((!bPlane || J->T > WaGiveUp) && !J->bFetch)
	{
		ADP_WARN(TEXT("GSE: the %s does not go to %s: %s"), *Tractors[Ti].Name, *J->Reg, bPlane ? *Why : TEXT("the aircraft is gone"));
		J->Phase = 3; Tractors[Ti].Job = EJob::Back; SvcHome(Job); return false;
	}
	RetryIn(Ti, 6.f, [this, Job] { SvcApron(Job); });
	return false;
}

void AGSE::WetIn(int32 Job)
{
	// behind the tail: down beside its line, round to face away, and straight back under the tail (2.85 m and more over its 2.52)
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti; FTractor& T = Tractors[Ti]; const FServiceSpec& Sv = ADPService::All()[J->Svc];
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp);
	const float X = S + Sv.Lat, Yn = N + Sv.Far + 2.f, X0 = X + 2.f * T.KTurnR / 100.f;
	TArray<FVector> W; LegT(S, W, X0, Yn + 4.f, 0.f, -1.f, X0, Yn, 0.f, -1.f, T.KTurnR); LegT(S, W, X0, Yn, 0.f, -1.f, X, Yn, 0.f, 1.f, T.KTurnR * 0.98f); LegT(S, W, X, Yn, 0.f, 1.f, X, Yn + 2.f, 0.f, 1.f, T.KTurnR);
	const float Y = N + Sv.Aft;
	Go(T, MoveTemp(W), false, [this, Job, Ti, S, X, Y]
	{
		TArray<FVector> R; const FVector A = Tractors[Ti].At, E = PlanT(S, X, Y); for (int32 i = 1; i <= 40; ++i) R.Add(FMath::Lerp(A, E, i / 40.f));
		Go(Tractors[Ti], MoveTemp(R), true, [this, Job] { SvcWork(Job); });
	});
}

// ------------------------------------------------------------------ the work

TFunction<void(float, float, FDriverPose&)> AGSE::WetClimbFn(int32 Ti, bool bUp) const
{
	// up the left side's steps, a hand on each handle, through the gate (facing the truck's right: the left hand is the front one)
	return [this, Ti, bUp](float K0, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; if (!T.Wa) return;
		const bool bT = T.Wa->bToilet; const float K = bUp ? K0 : 1.f - K0, Fl = WaFloor(*T.Wa); const FVector Fc = FRotator(0.f, T.Yaw + 90.f, 0.f).Vector();
		const FVector In = SvcP(T, TEXT("IK_Platform_In")), S0 = SvcP(T, TEXT("IK_Step_Platform_0")), S1 = bT ? FVector(S0.X, In.Y - 14.f, Fl) : SvcP(T, TEXT("IK_Step_Platform_1"));
		const FVector GF = SvcP(T, TEXT("IK_Grab_Platform_F")), GR = SvcP(T, TEXT("IK_Grab_Platform_R")), Lf(11.f, 0.f, 0.f), G0(S0.X, S0.Y - 38.f, 0.f), Fi(In.X, In.Y, Fl);
		auto Key = [&](int32 i)
		{
			if (i == 0) return ADPPose::Standing(TW(T, SvcP(T, TEXT("IK_Stand_Steps"))), Fc, PH);
			if (i > 3) return ADPPose::Standing(TW(T, Fi), Fc, PH);
			FDriverPose Q; Q.Facing = Fc; Q.Lean = 10.f; Q.bHand[0] = Q.bHand[1] = true; Q.Grip[0] = Q.Grip[1] = 0.9f; const float Hz = bT ? 0.f : FMath::Min(24.f * i, 60.f);
			Q.Hand[0] = TW(T, GF + WaUp * Hz); Q.Hand[1] = TW(T, GR + WaUp * Hz);
			if (i == 1) { Q.Pelvis = TW(T, FVector(S0.X, S0.Y - 26.f, PH + 3.f)); Q.Foot[0] = TW(T, S0 + Lf); Q.Foot[1] = TW(T, G0 - Lf); }
			else if (i == 2) { Q.Pelvis = TW(T, FVector(S0.X, S0.Y - 4.f, S0.Z + PH - 5.f)); Q.Foot[0] = TW(T, S0 + Lf); Q.Foot[1] = TW(T, S1 - Lf); }
			else { Q.Pelvis = TW(T, FVector(S0.X, S1.Y + (bT ? 2.f : 8.f), S1.Z + PH - 5.f)); Q.Foot[0] = TW(T, Fi + Lf); Q.Foot[1] = TW(T, S1 - Lf); }
			return Q;
		};
		const float X = K * 4.f; const int32 i = FMath::Clamp(FMath::FloorToInt(X), 0, 3);
		P = ADPPose::Lerp(Key(i), Key(i + 1), FMath::SmoothStep(0.f, 1.f, X - i));
	};
}

void AGSE::WetArrive(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	FTractor& T = Tractors[J->Ti]; if (!T.Wa) return;
	FSvcWet& F = *T.Wa; F.bLive = true; const int32 k = F.bToilet ? 1 : 0;
	const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At); const AActor* Pl = WetPlane(J->StandX); float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J->StandX, S, N, Sp);
	const FVector C = Root.InverseTransformPosition(Pl ? Pl->GetActorTransform().TransformPosition(WaPanel[k]) : FVector(PlanT(S, S - float(WaPanel[k].X) / 100.f, N + float(WaPanel[k].Y) / 100.f) + WaUp * WaPanel[k].Z));
	F.Couple = WaWant[k] + (C - WaWant[k]).GetClampedToMaxSize(22.f);   // (so far he reaches from his place)
	if (FVector::Dist(C, WaWant[k]) > 22.f) ADP_WARN(TEXT("GSE: the %s stands %.0f cm off the panel of %s"), *T.Name, FVector::Dist(C, WaWant[k]), *J->Reg);
	ADP_LOG(TEXT("GSE: the %s at %s - the panel at (%.0f, %.0f, %.0f) of it"), *T.Name, *J->Reg, C.X, C.Y, C.Z);
}

void AGSE::WetWork(int32 Job)
{
	FSvcJob* J = SvcJ(Job); ATugs* Gt = Tg(); if (!Gt || !J || J->Phase != 2 || !J->Men.Num() || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Wa) return;
	const int32 Ci = J->Men[0].Ci, Ti = J->Ti; FSvcWet& F0 = *Tractors[Ti].Wa; F0.Ci = Ci; F0.bLive = true; const bool bT = F0.bToilet, bIsUp = Gt->Crew[Ci].bUp;
	if (J->Men[0].bSeated) ClimbOut(Ci, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) { X->bSeated = false; X->bOut = true; } });
	auto Wt = [this, Ti]() -> FSvcWet& { return *Tractors[Ti].Wa; };
	auto Pt = [this, Ti](const TCHAR* N) { return SvcP(Tractors[Ti], N); };
	auto At = [this, Ci, Ti](const FVector& L, float Stop = 15.f) { WalkTo(Ci, [this, Ti, L] { return TW(Tractors[Ti], L); }, Stop); };
	auto On = [this, Ti](const FVector& L) { const FTractor& T = Tractors[Ti]; return TW(T, FVector(L.X, L.Y, WaFloor(*T.Wa))); };   // (on the platform as it is now)
	auto Dir = [this, Ti](float Yaw) { return FRotator(0.f, Tractors[Ti].Yaw + Yaw, 0.f).Vector(); };
	auto Free = [this, Ci] { Do(Ci, [this, Ci] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = nullptr; }); };
	auto Stay = [this, Ci, On, Dir](const FVector& L, float Yaw) { Do(Ci, [this, Ci, On, Dir, L, Yaw] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = [On, Dir, L, Yaw](float, float PH, FDriverPose& P) { P = ADPPose::Standing(On(L), Dir(Yaw), PH); }; }); };
	// a hand to a place and back, what it does done while it is there (bPl: from the platform; bRide: the place goes up with it; Keep: held there until it is true)
	auto Hand = [this, Ci, Ti, On](const FVector& St, int32 Side, const FVector& To, float Dur, TFunction<void()> Fn, bool bLow = false, bool bPl = false, bool bRide = false, TFunction<bool()> Keep = nullptr)
	{
		auto Ps = [this, Ti, On, St, Side, To, bLow, bPl, bRide](float E, float PH, FDriverPose& P)
		{
			const FTractor& T = Tractors[Ti]; const FVector A = bPl ? On(St) : TW(T, St), H = TW(T, To + (bRide ? WaUp * T.Wa->Lift : FVector::ZeroVector)), Fc = (H - A).GetSafeNormal2D();
			P = bLow ? ADPPose::Stoop(A, Fc, Side, H, 0.9f, PH, E) : ADPPose::Reach(A, Fc, Side, H, 0.9f, PH, E);
		};
		PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(FMath::SmoothStep(0.f, 1.f, K), PH, P); }, Dur * 0.5f);
		if (Fn) Do(Ci, MoveTemp(Fn));
		if (Keep)
		{
			Do(Ci, [this, Ci, Ps] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = [Ps](float, float PH, FDriverPose& P) { Ps(1.f, PH, P); }; });
			WaitFor(Ci, [Keep](float E) { return E > 25.f || Keep(); });
		}
		PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(1.f - FMath::SmoothStep(0.f, 1.f, K), PH, P); }, Dur * 0.5f);
	};
	// a step on the platform: one foot after the other, turning
	auto Step = [this, Ci, On, Dir, Stay](const FVector& A, float YawA, const FVector& B, float YawB)
	{
		PoseAct(Ci, [On, Dir, A, YawA, B, YawB](float K, float PH, FDriverPose& P)
		{
			const FDriverPose S = ADPPose::Standing(On(A), Dir(YawA), PH), E = ADPPose::Standing(On(B), Dir(YawB), PH);
			P = ADPPose::Lerp(S, E, FMath::SmoothStep(0.f, 1.f, K));
			for (int32 f = 0; f < 2; ++f) { const float U = FMath::Clamp(K * 2.f - f, 0.f, 1.f); P.Foot[f] = FMath::Lerp(S.Foot[f], E.Foot[f], FMath::SmoothStep(0.f, 1.f, U)) + WaUp * (7.f * FMath::Sin(PI * U)); }
		}, FMath::Max(0.8f, float(FVector::Dist2D(A, B)) / 55.f), true);
		Stay(B, YawB);
	};
	auto Next = [this, Job, Ci](int32 n) { Do(Ci, [this, Job, n] { if (FSvcJob* X = SvcJ(Job)) X->Stage = n; Later([this, Job] { WetWork(Job); }); }); Wait(Ci, 0.4f); };
	// onto the platform and off it, the gate shut behind him by hand
	const FVector PIn = Pt(TEXT("IK_Platform_In")), Wk = WaWork[bT ? 1 : 0]; const float WkYaw = bT ? 0.f : -90.f;
	auto Up = [&](bool bUp)
	{
		const FVector GH = Pt(TEXT("IK_Handle_GatePlatform")), GP = Pt(TEXT("PIV_GatePlatform")), GO = GP + FRotator(0.f, 80.f, 0.f).RotateVector(GH - GP);
		if (bUp)
		{
			At(Pt(TEXT("IK_Stand_Steps")), 12.f);
			Do(Ci, [this, Ci, Wt] { Wt().GateWant = 1.f; if (ATugs* G = Tg()) G->Crew[Ci].bUp = true; });
			PoseAct(Ci, WetClimbFn(Ti, true), bT ? 3.8f : 4.8f, true); Stay(PIn, 90.f);
			Hand(PIn, 1, GO, 1.2f, [Wt] { Wt().GateWant = 0.f; }, false, true, true);
			return;
		}
		Step(Wk, WkYaw, PIn, 90.f);
		Hand(PIn, 1, GH, 1.2f, [Wt] { Wt().GateWant = 1.f; }, false, true, true);
		Wait(Ci, 0.7f); Free();
		PoseAct(Ci, WetClimbFn(Ti, false), bT ? 3.6f : 4.6f, true);
		Do(Ci, [this, Ci, Wt] { Wt().GateWant = 0.f; if (ATugs* G = Tg()) G->Crew[Ci].bUp = false; });
	};
	auto Upon = [&](const FVector& L, float Yaw) { if (bIsUp) return; Up(true); Step(PIn, 90.f, L, Yaw); };   // (a stage on the platform: whoever took over comes up first)
	// a coupling by both hands from the chest up onto the panel and turned fast, or back
	auto Both = [this, Ci, Ti, On, Dir, Wk, WkYaw, bT](int32 i, bool bOn)
	{
		PoseAct(Ci, [this, Ti, On, Dir, Wk, WkYaw, bT, i, bOn](float K0, float PH, FDriverPose& P)
		{
			FTractor& T = Tractors[Ti]; if (!T.Wa) return;
			FSvcWet& F = *T.Wa; FSvcWet::FThing& X = F.Th[i]; const float K = bOn ? K0 : 1.f - K0; const bool bW = bT && i == FSvcWet::Main;
			const FVector Cp = F.Couple + FVector(0.f, i == FSvcWet::Rinse ? WaRinseY : 0.f, bW ? -14.8f : -7.5f), Fw = FRotator(0.f, WkYaw, 0.f).Vector();
			const FVector Ch = FVector(Wk.X, Wk.Y, WaFloor(F) + 118.f) + Fw * 34.f, Un = Cp - WaUp * 12.f;
			const float A = FMath::SmoothStep(0.f, 0.55f, K), U = FMath::SmoothStep(0.55f, 0.8f, K);
			X.Mode = FSvcWet::Put; X.bRide = false; X.At = FMath::Lerp(FMath::Lerp(Ch, Un, A), Cp, U); X.Tip = bW ? 90.f : 0.f; X.Turn = bW ? 0.f : 35.f * FMath::SmoothStep(0.8f, 1.f, K);
			const FVector HL = bW ? X.At - FVector(0.f, 12.5f, 0.f) : X.At + WaUp * 2.f - Fw * 3.f, HR = bW ? X.At + FVector(0.f, 12.5f, 0.f) : X.At - WaUp * 8.f - Fw * 2.f;
			P = ADPPose::Both(On(Wk), Dir(WkYaw), TW(T, HL), TW(T, HR), 1.f, PH, 1.f, 4.f);
		}, 4.5f);
	};
	auto Set = [Wt](int32 i, uint8 Mode) { return [Wt, i, Mode] { Wt().Th[i].Mode = Mode; }; };
	auto Lay = [Wt](int32 i, const FVector& P, bool bRide) { return [Wt, i, P, bRide] { FSvcWet::FThing& X = Wt().Th[i]; X.Mode = FSvcWet::Put; X.At = P; X.bRide = bRide; X.Tip = 0.f; }; };
	auto Chocks = [&](bool bOn)
	{
		// its own two, before and behind the left drive wheel (round its front; the point is on the grip)
		const FVector StCh = Pt(TEXT("IK_Stand_Chocks")), StPl = Pt(TEXT("IK_Stand_ChockPlace")) + FVector(0.f, 22.f, 0.f), FrR(350.f, 165.f, 0.f), FrL(350.f, -180.f, 0.f);
		const FVector C0 = Pt(TEXT("IK_Chock_0")), C1 = Pt(TEXT("IK_Chock_1")), P0 = Pt(TEXT("IK_ChockPlace_0")) + WaUp * 21.7f, P1 = Pt(TEXT("IK_ChockPlace_1")) + WaUp * 21.7f;
		auto Tray = [&] { At(FrL, 40.f); At(FrR, 40.f); At(StCh); };
		if (bOn) Tray(); else At(StPl);
		Hand(bOn ? StCh : StPl, 1, bOn ? C1 : P1, 1.6f, Set(FSvcWet::Chock1, FSvcWet::RightHand), true); Hand(bOn ? StCh : StPl, 0, bOn ? C0 : P0, 1.6f, Set(FSvcWet::Chock0, FSvcWet::LeftHand), true);
		if (bOn)
		{
			At(FrR, 40.f); At(FrL, 40.f); At(StPl);
			Hand(StPl, 1, P1, 1.8f, Lay(FSvcWet::Chock1, P1, false), true); Hand(StPl, 0, P0, 1.8f, Lay(FSvcWet::Chock0, P0, false), true);
			return;
		}
		Tray(); Hand(StCh, 1, C1, 1.6f, Set(FSvcWet::Chock1, FSvcWet::InPlace), true); Hand(StCh, 0, C0, 1.6f, Set(FSvcWet::Chock0, FSvcWet::InPlace), true);
		At(FrR, 40.f); At(FrL, 40.f);
	};
	auto Stowed = [this, Job, Ci, Ti] { Do(Ci, [this, Job, Ti] { Tractors[Ti].bUsed = true; if (FSvcJob* X = SvcJ(Job)) { if (X->Men.Num()) X->Men[0].bDown = true; ADP_LOG(TEXT("GSE: the %s is done at %s, the hose stowed"), *Tractors[Ti].Name, *X->Reg); } }); };
	auto RunFor = [&](const FVector& St, float Yaw, bool bPl, float To)
	{
		Do(Ci, [this, Ci, Ti, Wt, On, Dir, St, Yaw, bPl] { Wt().bRun = true; if (ATugs* G = Tg()) G->Crew[Ci].Hold = [this, Ti, On, Dir, St, Yaw, bPl](float, float PH, FDriverPose& P) { P = ADPPose::Standing(bPl ? On(St) : TW(Tractors[Ti], St), Dir(Yaw), PH); }; });
		WaitFor(Ci, [this, Job, To](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Pumped >= FMath::Min(To, X->Litres); });
		Do(Ci, [this, Job, Wt] { Wt().bRun = false; if (const FSvcJob* X = SvcJ(Job)) ADP_LOG(TEXT("GSE: %s - %s"), *X->Reg, !Wt().bToilet ? TEXT("its 200 l of water are in") : X->Pumped >= X->Litres ? TEXT("toilets rinsed") : TEXT("toilets drained")); });
		if (!bPl) Free();
	};
	const int32 Last = bT ? 7 : 5;
	if (J->Stage >= Last)
	{
		for (FSvcWet::FThing& X : F0.Th) X.Mode = FSvcWet::InPlace;   // (nothing may be left out when it drives off)
		F0.TrayWant = F0.ValveWant = F0.LiftWant = F0.GateWant = 0.f; SvcLeave(Job); return;
	}
	if (J->Stage == 0) { Chocks(true); Next(1); return; }
	if (!bT)
	{
		const FVector PnAt = Pt(TEXT("IK_PanelStand")), RlAt = Pt(TEXT("IK_ReelStand")), CpH = Pt(TEXT("IK_Coupling_Hand"));
		switch (J->Stage)
		{
		case 1:   // up, the panel, the coupling off its holder and onto it
			if (bIsUp) { Next(2); break; }
			Up(true); Step(PIn, 90.f, RlAt, 0.f);
			Hand(RlAt, 1, CpH, 1.8f, Set(FSvcWet::Main, FSvcWet::RightHand), true, true);
			Step(RlAt, 0.f, Wk, WkYaw); Both(FSvcWet::Main, true);
			Do(Ci, [this, Job, Wt] { Wt().bConn = true; if (const FSvcJob* X = SvcJ(Job)) ADP_LOG(TEXT("GSE: the water hose is on %s's panel"), *X->Reg); });
			Next(2); break;
		case 2:   // the valve, the pump, 200 l, the pump, the valve
			Upon(Wk, WkYaw); Step(Wk, WkYaw, PnAt, 0.f);
			Hand(PnAt, 1, Pt(TEXT("IK_Lever_Valve")), 1.6f, [Wt] { Wt().ValveWant = 1.f; }, true, true); Hand(PnAt, 1, Pt(TEXT("IK_Button_PumpOn")), 1.4f, nullptr, true, true);
			RunFor(PnAt, 0.f, true, WaFill);
			Hand(PnAt, 1, Pt(TEXT("IK_Button_PumpOff")), 1.4f, nullptr, true, true); Hand(PnAt, 1, Pt(TEXT("IK_Lever_Valve")), 1.6f, [Wt] { Wt().ValveWant = 0.f; }, true, true);
			Step(PnAt, 0.f, Wk, WkYaw); Next(3); break;
		case 3:   // off, back in its cup, the hose wound in
			Upon(Wk, WkYaw);
			if (F0.bConn) { Do(Ci, [Wt] { Wt().bConn = false; }); Both(FSvcWet::Main, false); Do(Ci, Set(FSvcWet::Main, FSvcWet::RightHand)); }
			Step(Wk, WkYaw, RlAt, 0.f);
			Hand(RlAt, 1, CpH, 1.8f, Set(FSvcWet::Main, FSvcWet::InPlace), true, true); Hand(RlAt, 0, Pt(TEXT("IK_Button_Rewind")), 2.4f, nullptr, true, true);
			Step(RlAt, 0.f, Wk, WkYaw); Stowed(); Up(false); Next(4); break;
		default: Chocks(false); Next(5); break;
		}
		return;
	}
	const FVector HsAt = Pt(TEXT("IK_Platform_Hose")), RsAt = Pt(TEXT("IK_Platform_RinseStand")), CapH = Pt(TEXT("IK_WasteCap")), RsH = Pt(TEXT("IK_RinseCoupling_Hand")), Rail = Pt(TEXT("IK_Rail_R")) + FVector(8.f, -5.f, -4.f), TkAt = Pt(TEXT("IK_Stand_TankPanel")) + FVector(0.f, 4.f, 0.f), CapAt(-250.f, 40.f, 66.f);
	// the platform's lever held all the way (the right hand; what the left one has stays in it)
	auto Lift = [&](bool bHigh)
	{
		const FVector Sl(-234.f, 20.f, 0.f);
		Step(Wk, 0.f, Sl, 45.f);
		Hand(Sl, 1, Pt(TEXT("IK_Lever_Platform")), 1.4f, [Wt, bHigh] { FSvcWet& F = Wt(); F.LiftWant = bHigh ? FMath::Clamp(float(F.Couple.Z) - 240.f, 0.f, 95.f) : 0.f; }, false, true, true, [Wt] { return Wt().Lift == Wt().LiftWant; });
		Step(Sl, 45.f, Wk, 0.f);
	};
	// the waste coupling by both hands out of its rack to the chest, or back
	auto Rack = [this, Ci, Ti, On, HsAt](bool bOut)
	{
		PoseAct(Ci, [this, Ti, On, HsAt, bOut](float K0, float PH, FDriverPose& P)
		{
			FTractor& T = Tractors[Ti]; if (!T.Wa) return;
			FSvcWet::FThing& X = T.Wa->Th[FSvcWet::Main]; const float K = bOut ? K0 : 1.f - K0, M = FMath::SmoothStep(0.45f, 1.f, K), E = FMath::SmoothStep(0.f, 0.45f, K);
			X.Mode = FSvcWet::InPlace; if (K > 0.45f) { X.Mode = FSvcWet::Put; X.bRide = false; X.At = FMath::Lerp(X.Piv, FVector(HsAt.X + 32.f, HsAt.Y - 6.f, 174.f), M); X.Tip = 90.f * M; }
			const FVector C = K > 0.45f ? X.At : X.Piv; const float Z = 3.f * (1.f - M);
			P = ADPPose::Both(On(HsAt), FRotator(0.f, T.Yaw - 32.f * (1.f - M), 0.f).Vector(), TW(T, C + FVector(0.f, -12.5f, Z)), TW(T, C + FVector(0.f, 12.5f, Z)), 1.f, PH, E, 6.f + 14.f * (1.f - M));
		}, 3.f);
	};
	auto Carry = [&] { Do(Ci, [Wt] { FSvcWet::FThing& X = Wt().Th[FSvcWet::Main]; X.Grip = FVector(0.f, 12.f, 0.f); X.Tip = 90.f; X.Mode = FSvcWet::LeftHand; }); };
	switch (J->Stage)
	{
	case 1:   // up, the tray down under the mouth, the cap off, the rinse coupling hung on the rail
		Upon(HsAt, 0.f);
		Hand(HsAt, 0, Pt(TEXT("IK_SplashTray")), 1.4f, [Wt] { Wt().TrayWant = 1.f; }, true, true);
		Hand(HsAt, 0, CapH, 1.6f, Set(FSvcWet::Cap, FSvcWet::LeftHand), true, true);
		Step(HsAt, 0.f, RsAt, 0.f); Hand(RsAt, 0, CapAt, 1.6f, Lay(FSvcWet::Cap, CapAt, true), true, true, true);
		Hand(RsAt, 1, RsH, 1.8f, Set(FSvcWet::Rinse, FSvcWet::RightHand), true, true); Hand(RsAt, 1, Rail, 1.6f, Lay(FSvcWet::Rinse, Rail, true), false, true, true);
		Step(RsAt, 0.f, HsAt, 0.f); Next(2); break;
	case 2:   // the waste coupling out, up with it, both hoses on, down
		Upon(HsAt, 0.f); Rack(true); Carry();
		Step(HsAt, 0.f, Wk, 0.f); Lift(true); Both(FSvcWet::Main, true); Do(Ci, [Wt] { Wt().bConn = true; });
		Hand(Wk, 1, Rail, 1.6f, Set(FSvcWet::Rinse, FSvcWet::RightHand), false, true, true); Both(FSvcWet::Rinse, true);
		Do(Ci, [this, Job, Wt] { Wt().bRinseConn = true; if (const FSvcJob* X = SvcJ(Job)) ADP_LOG(TEXT("GSE: the waste and rinse hoses are on %s's panel"), *X->Reg); });
		Lift(false); Up(false); Next(3); break;
	case 3:   // drained, rinsed
		At(TkAt); Hand(TkAt, 1, Pt(TEXT("IK_Lever_Vent")), 1.6f, [Wt] { Wt().ValveWant = 1.f; });
		RunFor(TkAt, 90.f, false, FMath::Max(0.f, J->Litres - WaRinseS));   // (you may have drained it already)
		Hand(TkAt, 1, Pt(TEXT("IK_Button_RinsePump")), 1.4f, nullptr); RunFor(TkAt, 90.f, false, WaDrain + WaRinseS); Hand(TkAt, 1, Pt(TEXT("IK_Button_RinseStop")), 1.4f, nullptr);
		Hand(TkAt, 1, Pt(TEXT("IK_Lever_Vent")), 1.6f, [Wt] { Wt().ValveWant = 0.f; }); Next(4); break;
	case 4:   // up again, both off, down with them, the waste coupling in its rack
		Upon(Wk, 0.f);
		if (F0.bRinseConn || F0.bConn) Lift(true);
		if (F0.bRinseConn) { Do(Ci, [Wt] { Wt().bRinseConn = false; }); Both(FSvcWet::Rinse, false); Do(Ci, Set(FSvcWet::Rinse, FSvcWet::RightHand)); Hand(Wk, 1, Rail, 1.6f, Lay(FSvcWet::Rinse, Rail, true), false, true, true); }
		if (F0.bConn) { Do(Ci, [Wt] { Wt().bConn = false; }); Both(FSvcWet::Main, false); Carry(); Lift(false); }
		Step(Wk, 0.f, HsAt, 0.f);
		if (F0.bConn) { Rack(false); Do(Ci, Set(FSvcWet::Main, FSvcWet::InPlace)); }
		Next(5); break;
	case 5:   // the rinse coupling, the cap, the tray
		Upon(HsAt, 0.f); Step(HsAt, 0.f, RsAt, 0.f);
		if (F0.Th[FSvcWet::Rinse].Mode == FSvcWet::Put) { Hand(RsAt, 1, Rail, 1.6f, Set(FSvcWet::Rinse, FSvcWet::RightHand), false, true, true); Hand(RsAt, 1, RsH, 1.8f, Set(FSvcWet::Rinse, FSvcWet::InPlace), true, true); }
		if (F0.Th[FSvcWet::Cap].Mode == FSvcWet::Put) Hand(RsAt, 0, CapAt, 1.6f, Set(FSvcWet::Cap, FSvcWet::LeftHand), true, true, true);
		Step(RsAt, 0.f, HsAt, 0.f);
		Hand(HsAt, 0, CapH, 1.6f, Set(FSvcWet::Cap, FSvcWet::InPlace), true, true); Hand(HsAt, 0, Pt(TEXT("IK_SplashTray")), 1.4f, [Wt] { Wt().TrayWant = 0.f; }, true, true);
		Step(HsAt, 0.f, Wk, 0.f); Stowed(); Up(false); Next(6); break;
	default: At(TkAt); Hand(TkAt, 1, Pt(TEXT("IK_Button_ReelIn")), 2.2f, nullptr); Chocks(false); Next(7); break;
	}
}

void AGSE::WetLost(int32 Job)
{
	// its man vanished at work: what he had in his hands is back in its place, what is connected stays; whoever takes over
	// goes on from the last stage finished (the platform down, so he comes up by the steps)
	FSvcJob* J = SvcJ(Job); if (!J || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Wa) return;
	FSvcWet& F = *Tractors[J->Ti].Wa; F.bRun = false; F.LiftWant = F.GateWant = 0.f;
	for (FSvcWet::FThing& X : F.Th) if (X.Mode == FSvcWet::RightHand || X.Mode == FSvcWet::LeftHand) X.Mode = FSvcWet::InPlace;
	auto In = [&](int32 i) { F.Th[i].Mode = FSvcWet::InPlace; };
	if (J->Phase != 2) return;
	if (!F.bConn) In(FSvcWet::Main);
	if (!F.bToilet) { if (J->Stage == 1 && F.bConn) J->Stage = 2; }
	else if (J->Stage == 2 && F.bConn && F.bRinseConn) J->Stage = 3;
	else if (J->Stage == 1 || J->Stage == 2) { In(FSvcWet::Main); In(FSvcWet::Rinse); In(FSvcWet::Cap); F.bConn = F.bRinseConn = false; F.TrayWant = 0.f; J->Stage = 1; }
	ADP_LOG(TEXT("GSE: the %s stands at %s until somebody takes over (stage %d)"), *Tractors[J->Ti].Name, *J->Reg, J->Stage);
}

// ------------------------------------------------------------------ the water truck in your hands (to-do 238, VehicleControls.h)

namespace
{
	enum : int32 { WaMValve, WaMGate, WaMDoor, WaMFill, WaMWind, WaMPull };
	enum : int32 { WaCChassis, WaCGate, WaCDoor, WaCCouple, WaCPut0, WaCPut1, WaCStep, WaCFill };
	enum : int32 { WaSCoupHome, WaSCoupPlane, WaSChock0, WaSChock1, WaSPut0, WaSPut1, WaSClimb, WaSOn, WaSOff, WaSStop, WaSLamp };
	FTransform WaCTurn(const FVector& P, float Deg) { return FTransform(FQuat::Identity, -P) * FTransform(WaRot(FVector(0, 0, 1), Deg), P); }
	bool WaInBox(const AGSE::FTractor& T) { return T.Job == AGSE::EJob::None && !T.Player.IsValid() && FVector::Dist2D(T.At, T.Home) < 200.f; }
	const TCHAR* WaCHeld(const FSvcWet& F) { return F.Th[FSvcWet::Main].Mode == FSvcWet::YourHand ? (F.bToilet ? TEXT("the waste coupling") : TEXT("the water coupling")) : F.Th[FSvcWet::Rinse].Mode == FSvcWet::YourHand ? TEXT("the rinse coupling") : F.Th[FSvcWet::Cap].Mode == FSvcWet::YourHand ? TEXT("the waste cap") : F.Th[FSvcWet::Chock0].Mode == FSvcWet::YourHand || F.Th[FSvcWet::Chock1].Mode == FSvcWet::YourHand ? TEXT("a wheel chock") : nullptr; }
}

bool AGSE::WaterStowed(const FTractor& T) const
{
	if (!T.Wa || T.Job != EJob::None) return true; const FSvcWet& F = *T.Wa;
	for (const int32 i : {FSvcWet::Chock0, FSvcWet::Chock1, FSvcWet::Main, FSvcWet::Rinse, FSvcWet::Cap}) if (F.Th[i].Mode != FSvcWet::InPlace || FVector::Dist(F.Th[i].Now, F.Th[i].Piv) > 2.f) return false;
	if (F.Line.bHand || F.Fill > 0.02f || F.FillWant > 0.02f || (T.SvcDoors.IsValidIndex(1) && T.SvcDoors[1].Now > 0.02f)) return false;
	if (F.Lift > 0.5f || F.LiftWant > 0.5f || F.Tray > 0.02f || F.TrayWant > 0.02f || F.bRinseConn || F.bPump) return false;
	return F.Gate < 0.02f && F.GateWant < 0.02f && F.Valve < 0.02f && F.ValveWant < 0.02f && !F.bRun && !F.bConn && !F.bStop;
}

void AGSE::YouWater(FTractor& T, float Dt)
{
	// (50 l a minute, counted in seconds as its crew's)
	FSvcWet& F = *T.Wa;
	// (in its box, its fill valve open: full in the half minute its crew's filling takes there)
	if (T.bUsed && F.Fill > 0.95f && WaInBox(T) && !F.bRun) { F.Tank += Dt * 2500.f / 30.f; if (F.Tank >= 2500.f) { F.Tank = 2500.f; SvcRestock(T); } }
	if (F.YouStandX < -1e5f) { F.bRun = false; return; }
	if (F.bRun && (!F.bConn || F.Valve < 0.95f || F.bStop || F.Fill > 0.02f || F.Tank <= 0.f)) F.bRun = false;
	if (F.bRun) { T.bUsed = true; F.Tank = FMath::Max(0.f, F.Tank - Dt * WaRate); F.Litres += Dt * WaRate; if (!YouDone(T, F.YouStandX, Dt)) F.bRun = false; }
	else if (WaterStowed(T)) { YouDone(T, F.YouStandX, 0.f); F.YouStandX = -1e6f; }
}

TSharedPtr<FVehTable> AGSE::WaterControls()
{
	using K = FVehControl; using FW = FSvcWet;
	const FVector L(0, -1, 0), B(-1, 0, 0), No(0, 0, 0);
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>(); TArray<K>& C = Tb->Controls;
	auto Mo = [&C](const TCHAR* Part, const TCHAR* Pt, K::EKind Kind, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 M, float A, float Bv, bool bTurns = true) { K X; X.Part = Part; X.Point = Pt; X.Kind = Kind; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Motion = M; X.A = A; X.B = Bv; X.bTurns = bTurns; C.Add(X); };
	auto Sw = [&C](const TCHAR* Part, const TCHAR* Pt, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 S) { K X; X.Part = Part; X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Switch = S; C.Add(X); };
	auto Box = [&Tb](const FVector& Mn, const FVector& Mx, int32 Fr, TFunction<bool(const FTractor&)> On = nullptr) { FVehSolid S; S.Min = Mn; S.Max = Mx; S.Frame = Fr; S.On = MoveTemp(On); Tb->Solids.Add(S); };

	// the panel at the platform
	Mo(TEXT("Lever_Valve"), TEXT("IK_Lever_Valve"), K::Lever, TEXT("Water valve open / shut"), WaCChassis, B, WaMValve, 0.f, 1.f);
	Sw(TEXT("IK_Button_PumpOn"), TEXT("IK_Button_PumpOn"), TEXT("Pump on"), WaCChassis, B, WaSOn); Sw(TEXT("IK_Button_PumpOff"), TEXT("IK_Button_PumpOff"), TEXT("Pump off"), WaCChassis, B, WaSOff);
	Sw(TEXT("IK_Button_Light"), TEXT("IK_Button_Light"), TEXT("Work light"), WaCChassis, B, WaSLamp); Sw(TEXT("IK_Button_EStop"), TEXT("IK_Button_EStop"), TEXT("Emergency stop"), WaCChassis, B, WaSStop);
	// its reel: the hose pulled off by hand at its guide, wound in by the motor's button
	Mo(TEXT("IK_Button_Rewind"), TEXT("IK_Button_Rewind"), K::Hold, TEXT("Reel the hose in"), WaCChassis, B, WaMWind, 1.f, 0.f, false);
	Mo(TEXT("IK_HoseOut"), TEXT("IK_HoseOut"), K::Hold, TEXT("Pull hose off the reel"), WaCChassis, No, WaMPull, 0.f, 1300.f, false); C.Last().bGrip = true;
	// the filling point behind its door; what is carried; the platform
	Mo(TEXT("Lever_Fill"), TEXT("IK_Lever_Fill"), K::Lever, TEXT("Fill valve open / shut"), WaCFill, No, WaMFill, 0.f, 1.f);
	Mo(TEXT("PanelDoorL"), TEXT("IK_Handle_PanelDoorL"), K::Hand, TEXT("Filling door"), WaCDoor, No, WaMDoor, 0.f, 1.f);
	Sw(TEXT("Chock_0"), TEXT("IK_Chock_0"), TEXT("Chock"), WaCChassis, No, WaSChock0); Sw(TEXT("Chock_1"), TEXT("IK_Chock_1"), TEXT("Chock"), WaCChassis, No, WaSChock1);
	Sw(TEXT("Chock_0"), TEXT("IK_Chock_0"), TEXT("Chock"), WaCPut0, No, WaSPut0); Sw(TEXT("Chock_1"), TEXT("IK_Chock_1"), TEXT("Chock"), WaCPut1, No, WaSPut1);
	Sw(TEXT("Coupling"), TEXT("PIV_Coupling"), TEXT("Coupling"), WaCChassis, No, WaSCoupHome); Sw(TEXT("Coupling"), TEXT("PIV_Coupling"), TEXT("Coupling"), WaCCouple, No, WaSCoupPlane);
	Mo(TEXT("GatePlatform"), TEXT("IK_Handle_GatePlatform"), K::Hand, TEXT("Platform gate"), WaCGate, No, WaMGate, 0.f, 1.f);
	Sw(TEXT("IK_Step_Platform_1"), TEXT("IK_Step_Platform_1"), TEXT("Steps"), WaCStep, No, WaSClimb);

	Tb->Frame = [](const AGSE& G, const FTractor& T, int32 Fr) -> FTransform
	{
		const FW& F = *T.Wa;
		switch (Fr)
		{
		case WaCGate: return WaCTurn(G.SvcP(T, TEXT("PIV_GatePlatform")), 80.f * FMath::SmoothStep(0.f, 1.f, F.Gate));
		case WaCDoor: return T.SvcDoors.Num() > 1 ? WaCTurn(T.SvcDoors[1].Piv, ADPService::All()[T.Svc].Doors[1].Deg * T.SvcDoors[1].Now) : FTransform::Identity;
		case WaCCouple: return FTransform(F.Couple - WaUp * 7.5f - G.SvcP(T, TEXT("PIV_Coupling")));
		case WaCPut0: case WaCPut1: return FTransform(G.SvcP(T, Fr == WaCPut1 ? TEXT("IK_ChockPlace_1") : TEXT("IK_ChockPlace_0")) + WaUp * 21.7f - G.SvcP(T, Fr == WaCPut1 ? TEXT("IK_Chock_1") : TEXT("IK_Chock_0")));
		case WaCFill: return WaCTurn(G.SvcP(T, TEXT("PIV_Lever_Fill")), 70.f * F.Fill);
		case WaCStep: return FTransform(WaUp * (T.YouOn ? 70.f : 0.f));
		default: return FTransform::Identity;
		}
	};
	Tb->Now = [](const FTractor& T, int32 M) { return M == WaMValve ? T.Wa->Valve : M == WaMGate ? T.Wa->Gate : M == WaMFill ? T.Wa->Fill : M >= WaMWind ? T.Wa->Line.Slack() : T.SvcDoors.Num() > 1 ? T.SvcDoors[1].Now : 0.f; };
	Tb->Want = [](FTractor& T, int32 M, float To) { T.Wa->bLive = true; if (M == WaMValve) T.Wa->ValveWant = To; else if (M == WaMGate) T.Wa->GateWant = To; else if (M == WaMFill) T.Wa->FillWant = To; else if (M >= WaMWind) T.Wa->Line.Pull(To); else if (T.SvcDoors.Num() > 1) T.SvcDoors[1].Want = To; };
	Tb->May = [](const AGSE& G, const FTractor& T, int32 M, float To, FString& Why) 
	{
		const FW& F = *T.Wa; const bool bMoves = T.Player.IsValid() || T.Speed > 1.f || T.Path.Num(); const TCHAR* H = M >= WaMWind ? WaCHeld(F) : nullptr;
		if (M == WaMGate && T.YouOn && T.YouAt.Y < -6.f) Why = TEXT("you stand in its way");
		else if (M == WaMDoor && To < 0.5f && F.Fill > 0.02f) Why = TEXT("the fill valve is open");
		else if (M == WaMFill && To > 0.5f && F.bConn) Why = TEXT("the coupling is on the aircraft");
		else if (((M == WaMFill && To > 0.5f) || M == WaMPull) && bMoves) Why = TEXT("it is not parked");
		else if (H) Why = FString::Printf(TEXT("you have %s in your hand"), H);
		else if (M == WaMWind && F.bStop) Why = TEXT("the emergency stop is pressed");
		return Why.IsEmpty();
	};
	Tb->End = [](const FTractor& T, int32 M, float E) { return M == WaMPull ? FMath::Max(0.f, E - T.Wa->Line.Need) : E; };
	Tb->Say = [](const AGSE& G, const FTractor& T, const K& X) -> FString
	{
		const FW& F = *T.Wa; auto Two = [](uint8 M, bool bHome, const TCHAR* Take, const TCHAR* Back) { return FString(M == (bHome ? FW::InPlace : FW::Put) ? Take : M == FW::YourHand ? Back : TEXT("")); };
		if (X.Motion == WaMPull) return F.Th[FW::Main].Mode == FW::InPlace ? FString(X.Label) : FString();
		if (X.Motion == WaMWind) return F.Line.Slack() > 0.5f ? FString(X.Label) : FString();
		if (X.Motion == WaMFill) return T.SvcDoors.IsValidIndex(1) && T.SvcDoors[1].Now > 0.5f ? FString::Printf(TEXT("%s (tank %.0f of 2500 l%s)"), X.Label, F.Tank, F.Tank > 2499.5f ? TEXT(", full") : WaInBox(T) ? TEXT("") : TEXT(", filled only in its box")) : FString();
		switch (X.Switch)
		{
		case WaSOn: case WaSOff: return FString::Printf(TEXT("%s (%.0f l given, tank %.0f l)"), X.Label, F.Litres, F.Tank);
		case WaSCoupHome: return Two(F.Th[FW::Main].Mode, true, TEXT("Take the water coupling, the hose comes with it"), TEXT("Put the coupling back"));
		case WaSCoupPlane: return F.YouStandX > -1e5f ? Two(F.Th[FW::Main].Mode, false, TEXT("Take the coupling off"), TEXT("Connect the coupling")) : FString();
		case WaSChock0: case WaSChock1: return Two(F.Th[FW::Chock0 + X.Switch - WaSChock0].Mode, true, TEXT("Take the chock"), TEXT("Put the chock back"));
		case WaSPut0: case WaSPut1: return Two(F.Th[FW::Chock0 + X.Switch - WaSPut0].Mode, false, TEXT("Take the chock away"), TEXT("Put the chock at the wheel"));
		case WaSClimb: return T.YouOn ? TEXT("Climb down") : TEXT("Climb onto the platform");
		case WaSStop: return F.bStop ? TEXT("Pull the emergency stop out") : FString(X.Label);
		default: return X.Label;
		}
	};
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 S, FString& Why)
	{
		const FTractor& T = G.Tractors[Ti]; const FW& F = *T.Wa; const TCHAR* H = WaCHeld(F); const uint8 Cp = F.Th[FW::Main].Mode, Hk = S == WaSChock1 || S == WaSPut1 ? F.Th[FW::Chock1].Mode : F.Th[FW::Chock0].Mode;
		const bool bTake = (S == WaSCoupHome && Cp == FW::InPlace) || (S == WaSCoupPlane && Cp == FW::Put) || ((S == WaSChock0 || S == WaSChock1) && Hk == FW::InPlace) || ((S == WaSPut0 || S == WaSPut1) && Hk == FW::Put);
		if (bTake && H) Why = FString::Printf(TEXT("you have %s in your hand"), H);
		else if (bTake && (T.Player.IsValid() || T.Speed > 1.f || T.Path.Num())) Why = TEXT("it is not parked");
		else if (S == WaSCoupPlane && Cp == FW::Put && F.bRun) Why = TEXT("the pump is on");
		else if (S == WaSCoupPlane && Cp == FW::Put && F.Valve > 0.05f) Why = TEXT("the valve is open");
		else if (S == WaSCoupPlane && Cp != FW::Put && (F.Th[FW::Chock0].Mode != FW::Put || F.Th[FW::Chock1].Mode != FW::Put)) Why = TEXT("the chocks at the wheel first");
		else if ((S == WaSPut0 || S == WaSPut1) && Hk == FW::Put && Cp != FW::InPlace) Why = TEXT("the coupling back in its place first");
		else if (S == WaSOn && F.bStop) Why = TEXT("the emergency stop is pressed");
		else if (S == WaSOn && !F.bConn) Why = TEXT("the coupling is not on the aircraft");
		else if ((S == WaSOn || (S == WaSCoupPlane && Cp != FW::Put)) && F.Fill > 0.02f) Why = TEXT("the fill valve is open");
		else if (S == WaSOn && F.Tank <= 0.f) Why = TEXT("the tank is empty, it is filled up in its box");
		else if (S == WaSClimb && T.YouOn && T.YouAt.Z < 85.f) Why = TEXT("you stand on the steps");
		else if (S == WaSOn && F.Valve < 0.95f) Why = TEXT("the valve is shut");
		else if (S == WaSClimb && F.Gate < 0.9f) Why = TEXT("the gate is shut");
		return Why.IsEmpty();
	};
	Tb->Switch = [](AGSE& G, int32 Ti, int32 S)
	{
		FTractor& T = G.Tractors[Ti]; FW& F = *T.Wa; F.bLive = true; FW::FThing& M = F.Th[FW::Main];
		switch (S)
		{
		case WaSCoupHome:
			if (M.Mode == FW::YourHand) { M.Mode = FW::InPlace; break; }
			M.Mode = FW::YourHand; F.Line.Pull(0.f); if (!F.bConn) { FVector P; const int32 J = G.YouJob(T, WaPanel[0], WaWant[0], 22.f, P); if (J != INDEX_NONE) { F.Couple = P; if (F.YouStandX != G.SvcJobs[J].StandX) F.Litres = 0.f; F.YouStandX = G.SvcJobs[J].StandX; } else ADP_LOG(TEXT("GSE: the %s stands under no aircraft waiting for water"), *T.Name); }
			break;
		case WaSCoupPlane: F.bConn = M.Mode == FW::YourHand; M.Mode = F.bConn ? FW::Put : FW::YourHand; M.bRide = false; M.Tip = 0.f; M.At = F.Couple - WaUp * 7.5f; M.Turn = F.bConn ? 35.f : 0.f; ADP_LOG(TEXT("GSE: you %s the water coupling of the %s"), F.bConn ? TEXT("connect") : TEXT("take off"), *T.Name); break;
		case WaSChock0: case WaSChock1: { uint8& X = F.Th[FW::Chock0 + S - WaSChock0].Mode; X = X == FW::InPlace ? FW::YourHand : FW::InPlace; break; }
		case WaSPut0: case WaSPut1: { FW::FThing& X = F.Th[FW::Chock0 + S - WaSPut0]; X.bRide = false; X.Tip = 0.f; X.At = G.SvcP(T, S == WaSPut1 ? TEXT("IK_ChockPlace_1") : TEXT("IK_ChockPlace_0")) + WaUp * 21.7f; X.Mode = X.Mode == FW::Put ? FW::YourHand : FW::Put; break; }
		case WaSOn: F.bRun = true; break;
		case WaSOff: F.bRun = false; break;
		case WaSStop: F.bStop = !F.bStop; if (F.bStop) F.bRun = false; break;
		case WaSLamp: T.bSwWork = !T.bSwWork; break;
		default: return false;
		}
		return true;
	};
	Tb->Held = [](const FTractor& T) { return T.Wa ? WaCHeld(*T.Wa) : nullptr; };
	Tb->Leash = [](const AGSE& G, const FTractor& T, FVector& From) { From = G.SvcP(T, TEXT("IK_HoseOut")); return 1300.f; };
	Tb->Drop = [](FTractor& T) { for (FW::FThing& X : T.Wa->Th) if (X.Mode == FW::YourHand) X.Mode = FW::InPlace; };
	Tb->Climb = [](const AGSE& G, int32 Ti, int32 S, float& Secs) -> TFunction<void(float, float, FDriverPose&)> { if (S != WaSClimb) return nullptr; const bool bUp = !G.Tractors[Ti].YouOn; Secs = bUp ? 4.8f : 4.6f; return G.WetClimbFn(Ti, bUp); };
	Tb->Aboard = [](const FTractor& T, const FVector& P) -> uint8 { return P.X > -285.f && P.X < -190.f && P.Y > -50.f && P.Y < 104.f && FMath::Abs(P.Z - 100.f) < 40.f ? 1 : P.X > -266.f && P.X < -196.f && P.Y > -102.f && P.Y <= -50.f && P.Z > 25.f && P.Z < 110.f ? 2 : 0; };
	Tb->Stowed = [](const AGSE& G, const FTractor& T) { return G.WaterStowed(T); };
	Tb->Stow = [](AGSE&, FTractor& T)
	{
		FW& F = *T.Wa; F.bRun = F.bConn = F.bStop = false; for (const int32 i : {FW::Chock0, FW::Chock1, FW::Main}) F.Th[i].Mode = FW::InPlace; F.ValveWant = F.GateWant = F.FillWant = 0.f; F.Line.bHand = false; F.bLive = true;
		T.bSwWork = false; for (FTractor::FSvcDoor& D : T.SvcDoors) D.Want = 0.f;
	};
	// measured on the model (Art/WaterTruck/watertruck.glb): cab and its nose, frame with the wheels, tank with its boards, cabinet, its open door
	Box(FVector(118, -100, 34), FVector(258, 100, 236), WaCChassis); Box(FVector(258, -90, 34), FVector(291, 90, 182), WaCChassis);
	Box(FVector(-205, -98, 16), FVector(118, 98, 88), WaCChassis); Box(FVector(-136, -93, 88), FVector(118, 93, 167), WaCChassis); Box(FVector(-136, -58, 167), FVector(107, 58, 198), WaCChassis);
	Box(FVector(-198, -93.5, 87), FVector(-141.5, 93.5, 197), WaCChassis); Box(FVector(-189.6, -95.1, 100.4), FVector(-148.7, -91.6, 177.6), WaCDoor, [](const FTractor& T) { return T.SvcDoors.Num() > 1 && T.SvcDoors[1].Now > 0.98f; });
	// the platform: floor, underrun bar, both treads and their grab rails, rails, gate (shut, wide open)
	Box(FVector(-274, -46, 86), FVector(-198, 95, 100), WaCChassis); Box(FVector(-287, -97, 41), FVector(-266, 97, 70), WaCChassis);
	Box(FVector(-261, -97, 33), FVector(-201, -72, 37.3), WaCChassis); Box(FVector(-261, -72, 66), FVector(-201, -46, 69.8), WaCChassis);
	for (const float X : {-261.f, -205.f}) Box(FVector(X, -97, 86), FVector(X + 4.f, -60, 150), WaCChassis);
	Box(FVector(-274, -46, 100), FVector(-270, 95, 201), WaCChassis); Box(FVector(-274, 91, 100), FVector(-201, 95, 201), WaCChassis);
	Box(FVector(-272.2, -46.4, 143), FVector(-202, -41.2, 195), WaCGate, [](const FTractor& T) { return T.Wa->Gate < 0.1f || T.Wa->Gate > 0.98f; });
	return Tb;
}

// ------------------------------------------------------------------ the toilet truck in your hands (to-do 238)

namespace
{
	enum : int32 { TwMVent, TwMGate, TwMTray, TwMLift, TwMDump, TwMWind, TwMPull };
	enum : int32 { TwCChassis, TwCLift, TwCGate, TwCTray, TwCVent, TwCDump, TwCWaste, TwCRinse, TwCRail, TwCCap, TwCPut0, TwCPut1, TwCStep };
	enum : int32 { TwSWasteHome, TwSWastePlane, TwSRinseHome, TwSRinseRail, TwSRinsePlane, TwSCapHome, TwSCapLaid, TwSChock0, TwSChock1, TwSPut0, TwSPut1, TwSClimb, TwSOn, TwSOff, TwSStop };
	const FVector TwRail(8.f, -5.f, -4.f), TwCapAt(-250.f, 40.f, 66.f), TwRinseAt(0.f, WaRinseY, -7.5f); const float TwWasteZ = 14.8f;   // (where its crew put them)
	float TwHigh(const FSvcWet& F) { return F.YouStandX > -1e5f ? FMath::Clamp(float(F.Couple.Z) - 240.f, 0.f, 95.f) : 95.f; }
	bool TwHome(const FSvcWet::FThing& X) { return X.Mode == FSvcWet::InPlace && FVector::Dist(X.Now, X.Piv) < 2.f; }
}

bool AGSE::WetLiftHeld(const FTractor& T, float To) const
{
	if (!T.Wa || T.Wa->Lift == To) return false; const FSvcWet& F = *T.Wa; const FVector& P = T.YouAt; if (F.bStop) return true;
	if (To < F.Lift) return T.YouOn != 1 && P.X > -316.f && P.X < -146.f && FMath::Abs(P.Y) < 94.f && P.Z > -90.f && P.Z < 40.f + F.Lift;   // (you are under it, or on its step)
	const UWorld* W = GetWorld(); const APlayerController* PC = T.YouOn == 1 && W ? W->GetFirstPlayerController() : nullptr; if (!PC) return false;
	FCollisionQueryParams Q; Q.AddIgnoredActor(T.M.Get()); Q.AddIgnoredActor(PC->GetPawn()); FHitResult Hit; const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
	return W->LineTraceSingleByChannel(Hit, Root.TransformPosition(P + WaUp * 150.f), Root.TransformPosition(P + WaUp * 186.f), ECC_Pawn, Q);   // (your head is at the aircraft)
}

float AGSE::YouLeft(const FTractor& T, float StandX) const
{
	for (const TPair<int32, FSvcJob>& J : SvcJobs) if (J.Value.Phase == 0 && J.Value.StandX == StandX && ADPService::All()[J.Value.Svc].Wet == ADPService::All()[T.Svc].Wet && ADPService::All()[T.Svc].Wet) return J.Value.Litres;
	return 0.f;
}

void AGSE::YouToilet(FTractor& T, float Dt)
{
	// (drained through the waste hose by its own weight, rinsed by the pump: seconds, as its crew's)
	FSvcWet& F = *T.Wa; F.bRun = false;
	// (in its box, its emptying valve open: empty, rinse tank filled, in the half minute its crew's takes there)
	if (T.bUsed && F.Fill > 0.95f && WaInBox(T)) { F.Waste = FMath::Max(0.f, F.Waste - Dt * ToWaste / 30.f); if (F.Waste <= 0.f) SvcRestock(T); }
	if (F.YouStandX < -1e5f) { F.bPump = false; return; }
	const float Left = YouLeft(T, F.YouStandX);
	if (F.bConn && F.Valve > 0.95f && !F.bStop && F.Waste < ToWaste && Left > WaRinseS) { F.bRun = T.bUsed = true; F.Waste = FMath::Min(ToWaste, F.Waste + ToDrainL * Dt); YouDone(T, F.YouStandX, FMath::Min(Dt, Left - WaRinseS)); }
	if (F.bPump && (!F.bRinseConn || !F.bConn || F.bStop || F.Valve < 0.95f || F.Tank <= 0.f || F.Waste >= ToWaste || Left <= 0.f)) F.bPump = false;
	if (F.bPump)
	{
		F.bRun = T.bUsed = true; const float L = FMath::Min(F.Tank, Dt * ToRinseL); F.Tank -= L; F.Waste = FMath::Min(ToWaste, F.Waste + L); F.Litres += L;
		if (!YouDone(T, F.YouStandX, Dt)) F.bPump = false;
	}
	else if (!F.bRun && WaterStowed(T)) { YouDone(T, F.YouStandX, 0.f); F.YouStandX = -1e6f; }
}

TSharedPtr<FVehTable> AGSE::ToiletControls()
{
	using K = FVehControl; using FW = FSvcWet;
	const FVector L(0, -1, 0), No(0, 0, 0);
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>(); TArray<K>& C = Tb->Controls;
	auto Mo = [&C](const TCHAR* Part, const TCHAR* Pt, K::EKind Kind, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 M, float A, float Bv, bool bTurns = true) { K X; X.Part = Part; X.Point = Pt; X.Kind = Kind; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Motion = M; X.A = A; X.B = Bv; X.bTurns = bTurns; C.Add(X); };
	auto Sw = [&C](const TCHAR* Part, const TCHAR* Pt, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 S) { K X; X.Part = Part; X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Switch = S; C.Add(X); };
	auto Box = [&Tb](const FVector& Mn, const FVector& Mx, int32 Fr, TFunction<bool(const FTractor&)> On = nullptr) { FVehSolid S; S.Min = Mn; S.Max = Mx; S.Frame = Fr; S.On = MoveTemp(On); Tb->Solids.Add(S); };

	// the panel at the tanks (left side): the drain lever, the rinse pump, the reel's motor; the platform's lever at the mast
	Mo(TEXT("Lever_Vent"), TEXT("IK_Lever_Vent"), K::Lever, TEXT("Drain valve open / shut"), TwCVent, L, TwMVent, 0.f, 1.f);
	Sw(TEXT("IK_Button_RinsePump"), TEXT("IK_Button_RinsePump"), TEXT("Rinse pump on"), TwCChassis, L, TwSOn); Sw(TEXT("IK_Button_RinseStop"), TEXT("IK_Button_RinseStop"), TEXT("Rinse pump off"), TwCChassis, L, TwSOff);
	Mo(TEXT("IK_Button_ReelIn"), TEXT("IK_Button_ReelIn"), K::Hold, TEXT("Reel the rinse hose in"), TwCChassis, L, TwMWind, 1.f, 0.f, false);
	Mo(TEXT("IK_RinseOut"), TEXT("IK_RinseOut"), K::Hold, TEXT("Pull rinse hose off the reel"), TwCChassis, No, TwMPull, 0.f, 1120.f, false); C.Last().bGrip = true;
	Mo(TEXT("Lever_Lower"), TEXT("IK_Lever_Lower"), K::Lever, TEXT("Platform up / down"), TwCChassis, L, TwMLift, 0.f, 95.f);
	// on the platform: its lever and stop, its gate, the splash tray, the emptying valve behind the mast
	Mo(TEXT("Lever_Platform"), TEXT("IK_Lever_Platform"), K::Lever, TEXT("Platform up / down"), TwCLift, No, TwMLift, 0.f, 95.f);
	Sw(TEXT("IK_Button_PlatEStop"), TEXT("IK_Button_PlatEStop"), TEXT("Emergency stop"), TwCLift, No, TwSStop);
	Mo(TEXT("GatePlatform"), TEXT("IK_Handle_GatePlatform"), K::Hand, TEXT("Platform gate"), TwCGate, No, TwMGate, 0.f, 1.f);
	Mo(TEXT("SplashTray"), TEXT("IK_SplashTray"), K::Hand, TEXT("Splash tray"), TwCTray, No, TwMTray, 0.f, 1.f);
	Mo(TEXT("Lever_Dump"), TEXT("IK_Lever_Dump"), K::Lever, TEXT("Emptying valve open / shut"), TwCDump, No, TwMDump, 0.f, 1.f);
	// what is carried
	Sw(TEXT("Chock_0"), TEXT("IK_Chock_0"), TEXT("Chock"), TwCChassis, No, TwSChock0); Sw(TEXT("Chock_1"), TEXT("IK_Chock_1"), TEXT("Chock"), TwCChassis, No, TwSChock1);
	Sw(TEXT("Chock_0"), TEXT("IK_Chock_0"), TEXT("Chock"), TwCPut0, No, TwSPut0); Sw(TEXT("Chock_1"), TEXT("IK_Chock_1"), TEXT("Chock"), TwCPut1, No, TwSPut1);
	Sw(TEXT("WasteCoupling"), TEXT("PIV_WasteCoupling"), TEXT("Waste coupling"), TwCChassis, No, TwSWasteHome); Sw(TEXT("WasteCoupling"), TEXT("PIV_WasteCoupling"), TEXT("Waste coupling"), TwCWaste, No, TwSWastePlane);
	Sw(TEXT("RinseCoupling"), TEXT("PIV_RinseCoupling"), TEXT("Rinse coupling"), TwCChassis, No, TwSRinseHome); Sw(TEXT("RinseCoupling"), TEXT("PIV_RinseCoupling"), TEXT("Rinse coupling"), TwCRail, No, TwSRinseRail); Sw(TEXT("RinseCoupling"), TEXT("PIV_RinseCoupling"), TEXT("Rinse coupling"), TwCRinse, No, TwSRinsePlane);
	Sw(TEXT("WasteCap"), TEXT("IK_WasteCap"), TEXT("Waste cap"), TwCChassis, No, TwSCapHome); Sw(TEXT("WasteCap"), TEXT("IK_WasteCap"), TEXT("Waste cap"), TwCCap, No, TwSCapLaid);
	Sw(TEXT("IK_Step_Platform_0"), TEXT("IK_Step_Platform_0"), TEXT("Steps"), TwCStep, No, TwSClimb);

	Tb->Frame = [](const AGSE& G, const FTractor& T, int32 Fr) -> FTransform
	{
		const FW& F = *T.Wa; const FTransform Up(WaUp * F.Lift); auto Round = [&G, &T](const TCHAR* Pv, const FVector& Ax, float Deg) { const FVector P = G.SvcP(T, Pv); return FTransform(FQuat::Identity, -P) * FTransform(WaRot(Ax, Deg), P); };
		switch (Fr)
		{
		case TwCLift: return Up;
		case TwCGate: return Round(TEXT("PIV_GatePlatform"), WaUp, 80.f * FMath::SmoothStep(0.f, 1.f, F.Gate)) * Up;
		case TwCTray: return Round(TEXT("PIV_SplashTray"), FVector::RightVector, -85.f * F.Tray);
		case TwCVent: return Round(TEXT("PIV_Lever_Vent"), FVector::RightVector, 60.f * F.Valve);
		case TwCDump: return Round(TEXT("PIV_Lever_Dump"), FVector::ForwardVector, 45.f * F.Fill);
		case TwCWaste: return FTransform(F.Couple - WaUp * TwWasteZ - G.SvcP(T, TEXT("PIV_WasteCoupling")));
		case TwCRinse: return FTransform(F.Couple + TwRinseAt - G.SvcP(T, TEXT("PIV_RinseCoupling")));
		case TwCRail: return FTransform(G.SvcP(T, TEXT("IK_Rail_R")) + TwRail - G.SvcP(T, TEXT("PIV_RinseCoupling"))) * Up;
		case TwCCap: return FTransform(TwCapAt - G.SvcP(T, TEXT("IK_WasteCap"))) * Up;
		case TwCPut0: case TwCPut1: return FTransform(G.SvcP(T, Fr == TwCPut1 ? TEXT("IK_ChockPlace_1") : TEXT("IK_ChockPlace_0")) + WaUp * 21.7f - G.SvcP(T, Fr == TwCPut1 ? TEXT("IK_Chock_1") : TEXT("IK_Chock_0")));
		case TwCStep: return FTransform(WaUp * (T.YouOn ? 70.f : 0.f));
		default: return FTransform::Identity;
		}
	};
	Tb->Now = [](const FTractor& T, int32 M) { const FW& F = *T.Wa; return M == TwMVent ? F.Valve : M == TwMGate ? F.Gate : M == TwMTray ? F.Tray : M == TwMLift ? F.Lift : M == TwMDump ? F.Fill : F.Line.Slack(); };
	Tb->Want = [](FTractor& T, int32 M, float To) { FW& F = *T.Wa; F.bLive = true; if (M == TwMVent) F.ValveWant = To; else if (M == TwMGate) F.GateWant = To; else if (M == TwMTray) F.TrayWant = To; else if (M == TwMLift) F.LiftWant = To; else if (M == TwMDump) F.FillWant = To; else F.Line.Pull(To); };
	Tb->May = [](const AGSE& G, const FTractor& T, int32 M, float To, FString& Why)
	{
		const FW& F = *T.Wa; const bool bMoves = T.Player.IsValid() || T.Speed > 1.f || T.Path.Num(), bOpen = To > 0.5f; const TCHAR* H = M != TwMLift ? WaCHeld(F) : nullptr;
		const FVector Gp = G.SvcP(T, TEXT("PIV_GatePlatform"));
		if (H) Why = FString::Printf(TEXT("you have %s in your hand"), H);
		else if (bMoves && (M == TwMLift || M == TwMPull || ((M == TwMVent || M == TwMDump || M == TwMTray || M == TwMGate) && bOpen))) Why = TEXT("it is not parked");
		else if (M == TwMGate && T.YouOn == 1 && FVector::Dist2D(T.YouAt, Gp) < 88.f) Why = TEXT("you stand in its way");
		else if (M == TwMGate && bOpen && F.Lift > 1.f) Why = TEXT("the platform is up");
		else if (M == TwMLift && F.bStop) Why = TEXT("the emergency stop is pressed");
		else if (M == TwMLift && (F.Gate > 0.05f || F.GateWant > 0.05f)) Why = TEXT("the gate is open");
		else if (M == TwMLift && G.WetLiftHeld(T, To)) Why = To < F.Lift ? TEXT("you stand under it") : TEXT("your head is at the aircraft");
		else if (M == TwMTray && !bOpen && !TwHome(F.Th[FW::Main])) Why = TEXT("the waste coupling back in its rack first");
		else if (M == TwMTray && !bOpen && F.Th[FW::Cap].Mode != FW::InPlace) Why = TEXT("the cap back on first");
		else if (M == TwMVent && bOpen && !F.bConn) Why = TEXT("the waste coupling is not on the aircraft");
		else if (M == TwMVent && !bOpen && F.bPump) Why = TEXT("the rinse pump is on");
		else if (M == TwMDump && bOpen && !WaInBox(T)) Why = TEXT("it is emptied only in its box");
		else if (M == TwMDump && bOpen && (F.bConn || F.bRinseConn || F.Valve > 0.02f)) Why = TEXT("it is working at the aircraft");
		else if (M == TwMWind && F.bStop) Why = TEXT("the emergency stop is pressed");
		return Why.IsEmpty();
	};
	Tb->End = [](const FTractor& T, int32 M, float E) { return M == TwMPull ? FMath::Max(0.f, E - T.Wa->Line.Need) : M == TwMLift && E > 1.f ? TwHigh(*T.Wa) : E; };
	Tb->Say = [](const AGSE& G, const FTractor& T, const K& X) -> FString
	{
		const FW& F = *T.Wa; const bool bJob = F.YouStandX > -1e5f, bRail = F.Th[FW::Rinse].Mode == FW::Put && !F.bRinseConn;
		auto Two = [](uint8 M, bool bHome, const TCHAR* Take, const TCHAR* Back) { return FString(M == (bHome ? FW::InPlace : FW::Put) ? Take : M == FW::YourHand ? Back : TEXT("")); };
		if (X.Motion == TwMPull) return F.Th[FW::Rinse].Mode == FW::InPlace ? FString(X.Label) : FString();
		if (X.Motion == TwMWind) return F.Line.Slack() > 0.5f ? FString(X.Label) : FString();
		if (X.Motion == TwMDump) return T.YouOn == 1 && F.Lift < 1.f ? FString::Printf(TEXT("%s (waste tank %.0f of 1500 l%s)"), X.Label, F.Waste, F.Waste < 0.5f ? TEXT(", empty") : TEXT("")) : FString();
		if (X.Motion == TwMVent) { const float Left = bJob && F.bConn ? G.YouLeft(T, F.YouStandX) : 0.f; return Left > WaRinseS ? FString::Printf(TEXT("%s (%.0f s left to drain)"), X.Label, Left - WaRinseS) : FString(X.Label); }
		switch (X.Switch)
		{
		case TwSOn: case TwSOff: return FString::Printf(TEXT("%s (%.0f l given, rinse tank %.0f of 550 l, waste tank %.0f of 1500 l)"), X.Label, F.Litres, F.Tank, F.Waste);
		case TwSWasteHome: return Two(F.Th[FW::Main].Mode, true, TEXT("Take the waste coupling, the hose comes with it"), TEXT("Put the waste coupling back in its rack"));
		case TwSWastePlane: return bJob ? Two(F.Th[FW::Main].Mode, false, TEXT("Take the waste coupling off"), TEXT("Connect the waste coupling")) : FString();
		case TwSRinseHome: return Two(F.Th[FW::Rinse].Mode, true, TEXT("Take the rinse coupling, the hose comes with it"), TEXT("Put the rinse coupling back"));
		case TwSRinseRail: return bRail ? TEXT("Take the rinse coupling off the rail") : F.Th[FW::Rinse].Mode == FW::YourHand ? TEXT("Hang the rinse coupling on the rail") : TEXT("");
		case TwSRinsePlane: return !bJob ? FString() : FString(F.bRinseConn ? TEXT("Take the rinse coupling off") : F.Th[FW::Rinse].Mode == FW::YourHand ? TEXT("Connect the rinse coupling") : TEXT(""));
		case TwSCapHome: return Two(F.Th[FW::Cap].Mode, true, TEXT("Take the waste cap off"), TEXT("Put the waste cap on"));
		case TwSCapLaid: return Two(F.Th[FW::Cap].Mode, false, TEXT("Pick the waste cap up"), TEXT("Lay the waste cap down"));
		case TwSChock0: case TwSChock1: return Two(F.Th[FW::Chock0 + X.Switch - TwSChock0].Mode, true, TEXT("Take the chock"), TEXT("Put the chock back"));
		case TwSPut0: case TwSPut1: return Two(F.Th[FW::Chock0 + X.Switch - TwSPut0].Mode, false, TEXT("Take the chock away"), TEXT("Put the chock at the wheel"));
		case TwSClimb: return T.YouOn ? TEXT("Climb down") : TEXT("Climb onto the platform");
		case TwSStop: return F.bStop ? TEXT("Pull the emergency stop out") : FString(X.Label);
		default: return X.Label;
		}
	};
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 S, FString& Why)
	{
		const FTractor& T = G.Tractors[Ti]; const FW& F = *T.Wa; const TCHAR* H = WaCHeld(F); const uint8 Cp = F.Th[FW::Main].Mode, Rn = F.Th[FW::Rinse].Mode, Cq = F.Th[FW::Cap].Mode, Hk = S == TwSChock1 || S == TwSPut1 ? F.Th[FW::Chock1].Mode : F.Th[FW::Chock0].Mode;
		const bool bWaste = S == TwSWasteHome || S == TwSWastePlane, bRinse = S == TwSRinseHome || S == TwSRinseRail || S == TwSRinsePlane, bCap = S == TwSCapHome || S == TwSCapLaid, bChock = S >= TwSChock0 && S <= TwSPut1, bPlane = S == TwSWastePlane || S == TwSRinsePlane;
		const bool bThing = bWaste || bRinse || bCap || bChock, bMine = (bWaste ? Cp : bRinse ? Rn : bCap ? Cq : Hk) == FW::YourHand;   // (in your hand: put somewhere; else taken)
		if (bThing && !bMine && H) Why = FString::Printf(TEXT("you have %s in your hand"), H);
		else if (bThing && (T.Player.IsValid() || T.Speed > 1.f || T.Path.Num())) Why = TEXT("it is not parked");
		// on and off the aircraft: from the raised platform, waste first on and last off
		else if (bPlane && (T.YouOn != 1 || float(F.Couple.Z) - WaFloor(F) > 215.f)) Why = T.YouOn == 1 ? TEXT("raise the platform") : TEXT("it is done from the platform");
		else if (bPlane && bMine && (F.Th[FW::Chock0].Mode != FW::Put || F.Th[FW::Chock1].Mode != FW::Put)) Why = TEXT("the chocks at the wheel first");
		else if (S == TwSRinsePlane && bMine && !F.bConn) Why = TEXT("the waste coupling first");
		else if (S == TwSRinsePlane && !bMine && F.bPump) Why = TEXT("the rinse pump is on");
		else if (S == TwSWastePlane && !bMine && F.bRinseConn) Why = TEXT("the rinse coupling off first");
		else if (S == TwSWastePlane && !bMine && F.Valve > 0.05f) Why = TEXT("the drain valve is open");
		else if (S == TwSWastePlane && bMine && F.Fill > 0.02f) Why = TEXT("the emptying valve is open");
		// in and out of the rack, the cap: from the lowered platform, over the tray
		else if ((S == TwSWasteHome || S == TwSCapHome) && T.YouOn != 1) Why = TEXT("it is done from the platform");
		else if (S == TwSCapLaid && T.YouOn != 1) Why = TEXT("it is done from the platform");
		else if ((S == TwSWasteHome || S == TwSCapHome) && F.Lift > 1.f) Why = TEXT("lower the platform");
		else if ((S == TwSWasteHome || S == TwSCapHome) && F.Tray < 0.95f) Why = TEXT("the splash tray down first");
		else if (S == TwSWasteHome && Cq == FW::InPlace) Why = TEXT("the cap is on");
		else if (S == TwSCapHome && bMine && !TwHome(F.Th[FW::Main])) Why = TEXT("the waste coupling back in its rack first");
		else if ((S == TwSPut0 || S == TwSPut1) && !bMine && (Cp != FW::InPlace || Rn != FW::InPlace)) Why = TEXT("both couplings back in their places first");
		// the rinse pump
		else if (S == TwSOn && F.bStop) Why = TEXT("the emergency stop is pressed");
		else if (S == TwSOn && !F.bRinseConn) Why = TEXT("the rinse coupling is not on the aircraft");
		else if (S == TwSOn && F.Valve < 0.95f) Why = TEXT("the drain valve is shut");
		else if (S == TwSOn && F.Waste >= ToWaste) Why = TEXT("the waste tank is full, it is emptied in its box");
		else if (S == TwSOn && F.Tank <= 0.f) Why = TEXT("the rinse tank is empty, it is filled up in its box");
		else if (S == TwSOn && G.YouLeft(T, F.YouStandX) > WaRinseS + 0.01f) Why = TEXT("it is still draining");
		else if (S == TwSOn && G.YouLeft(T, F.YouStandX) <= 0.f) Why = TEXT("the aircraft is rinsed");
		// the steps
		else if (S == TwSClimb && H) Why = FString::Printf(TEXT("you have %s in your hand"), H);
		else if (S == TwSClimb && F.Lift > 1.f) Why = TEXT("the platform is up");
		else if (S == TwSClimb && T.YouOn == 2) Why = TEXT("you stand on the step");
		else if (S == TwSClimb && F.Gate < 0.9f) Why = TEXT("the gate is shut");
		return Why.IsEmpty();
	};
	Tb->Switch = [](AGSE& G, int32 Ti, int32 S)
	{
		FTractor& T = G.Tractors[Ti]; FW& F = *T.Wa; F.bLive = true; FW::FThing &M = F.Th[FW::Main], &R = F.Th[FW::Rinse], &Cq = F.Th[FW::Cap];
		auto Job = [&] { if (F.bConn || F.bRinseConn) return; FVector P; const int32 J = G.YouJob(T, WaPanel[1], WaWant[1], 45.f, P); if (J != INDEX_NONE) { F.Couple = P; if (F.YouStandX != G.SvcJobs[J].StandX) F.Litres = 0.f; F.YouStandX = G.SvcJobs[J].StandX; } else ADP_LOG(TEXT("GSE: the %s stands under no aircraft waiting for it"), *T.Name); };
		auto Lay = [&](FW::FThing& X, const FVector& At, bool bRide, float Tip, float Turn) { const bool bOn = X.Mode == FW::YourHand; X.Mode = bOn ? FW::Put : FW::YourHand; X.At = At; X.bRide = bRide; X.Tip = Tip; X.Turn = bOn ? Turn : 0.f; return bOn; };
		switch (S)
		{
		case TwSWasteHome: if (M.Mode == FW::YourHand) M.Mode = FW::InPlace; else { M.Mode = FW::YourHand; Job(); } break;
		case TwSWastePlane: F.bConn = Lay(M, F.Couple - WaUp * TwWasteZ, false, 90.f, 0.f); ADP_LOG(TEXT("GSE: you %s the waste coupling of the %s"), F.bConn ? TEXT("connect") : TEXT("take off"), *T.Name); break;
		case TwSRinseHome: if (R.Mode == FW::YourHand) R.Mode = FW::InPlace; else { R.Mode = FW::YourHand; F.Line.Pull(0.f); Job(); } break;
		case TwSRinseRail: Lay(R, G.SvcP(T, TEXT("IK_Rail_R")) + TwRail, true, 0.f, 0.f); break;
		case TwSRinsePlane: F.bRinseConn = Lay(R, F.Couple + TwRinseAt, false, 0.f, 35.f); ADP_LOG(TEXT("GSE: you %s the rinse coupling of the %s"), F.bRinseConn ? TEXT("connect") : TEXT("take off"), *T.Name); break;
		case TwSCapHome: Cq.Mode = Cq.Mode == FW::YourHand ? FW::InPlace : FW::YourHand; break;
		case TwSCapLaid: Lay(Cq, TwCapAt, true, 0.f, 0.f); break;
		case TwSChock0: case TwSChock1: { uint8& X = F.Th[FW::Chock0 + S - TwSChock0].Mode; X = X == FW::InPlace ? FW::YourHand : FW::InPlace; break; }
		case TwSPut0: case TwSPut1: Lay(F.Th[FW::Chock0 + S - TwSPut0], G.SvcP(T, S == TwSPut1 ? TEXT("IK_ChockPlace_1") : TEXT("IK_ChockPlace_0")) + WaUp * 21.7f, false, 0.f, 0.f); break;
		case TwSOn: F.bPump = true; break;
		case TwSOff: F.bPump = false; break;
		case TwSStop: F.bStop = !F.bStop; if (F.bStop) { F.bPump = false; F.LiftWant = F.Lift; } break;
		default: return false;
		}
		return true;
	};
	Tb->Held = [](const FTractor& T) { return T.Wa ? WaCHeld(*T.Wa) : nullptr; };
	Tb->Leash = [](const AGSE& G, const FTractor& T, FVector& From) { const bool bW = T.Wa->Th[FW::Main].Mode == FW::YourHand; From = G.SvcP(T, bW ? TEXT("IK_WasteHose_In") : TEXT("IK_RinseOut")); return bW ? 460.f : T.Wa->Th[FW::Rinse].Mode == FW::YourHand ? 1130.f : 1300.f; };
	Tb->Drop = [](FTractor& T) { for (FW::FThing& X : T.Wa->Th) if (X.Mode == FW::YourHand) X.Mode = FW::InPlace; };
	Tb->Climb = [](const AGSE& G, int32 Ti, int32 S, float& Secs) -> TFunction<void(float, float, FDriverPose&)> { if (S != TwSClimb) return nullptr; const bool bUp = !G.Tractors[Ti].YouOn; Secs = bUp ? 3.8f : 3.6f; return G.WetClimbFn(Ti, bUp); };
	Tb->Aboard = [](const FTractor& T, const FVector& P) -> uint8 { return T.Wa && P.X > -272.f && P.X < -186.f && FMath::Abs(P.Y) < 58.f && FMath::Abs(P.Z - WaFloor(*T.Wa)) < 40.f ? 1 : P.X > -262.f && P.X < -198.f && P.Y > -100.f && P.Y <= -58.f && P.Z > 22.f && P.Z < 60.f ? 2 : 0; };
	Tb->Stowed = [](const AGSE& G, const FTractor& T) { return G.WaterStowed(T); };
	Tb->Stow = [](AGSE&, FTractor& T) { FW& F = *T.Wa; F.bRun = F.bPump = F.bConn = F.bRinseConn = F.bStop = false; for (FW::FThing& X : F.Th) X.Mode = FW::InPlace; F.ValveWant = F.GateWant = F.FillWant = F.LiftWant = F.TrayWant = 0.f; F.Line.bHand = false; F.bLive = true; };
	// measured on the model (Art/ToiletTruck/toilettruck.glb): cab and nose, frame with the wheels, tanks, hose racks, the coupling's rack and post
	Box(FVector(118, -100, 34), FVector(258, 100, 236), TwCChassis); Box(FVector(258, -90, 34), FVector(291, 90, 182), TwCChassis);
	Box(FVector(-175, -98.5, 16), FVector(118, 98.5, 88), TwCChassis); Box(FVector(-126, -92.4, 88), FVector(108, 92.4, 169), TwCChassis); Box(FVector(-128, -78, 167), FVector(98.3, -4, 180.9), TwCChassis);
	Box(FVector(-155, -82, 150), FVector(-124, -50, 190), TwCChassis); Box(FVector(-149, -74, 87.5), FVector(-131, -58, 166), TwCChassis);
	// the reel and its guide, the rinse holder's post, the panel, underrun bar and lamps, the frame's end, the mast, the step
	Box(FVector(-161.1, 48, 87.5), FVector(-122.9, 77.5, 139.1), TwCChassis); Box(FVector(-165, 56, 88), FVector(-149, 68, 119), TwCChassis); Box(FVector(-163.8, 83, 87.5), FVector(-150, 93, 144.7), TwCChassis);
	Box(FVector(-144, -93, 99), FVector(-122, -87, 135), TwCChassis); Box(FVector(-287, -97, 41), FVector(-275, 97, 53), TwCChassis); Box(FVector(-287, -97, 52.5), FVector(-279, -61, 70.5), TwCChassis); Box(FVector(-287, 61, 52.5), FVector(-279, 97, 70.5), TwCChassis);
	Box(FVector(-279, -40.4, 43), FVector(-150, 40.4, 57), TwCChassis); Box(FVector(-178, -38.5, 42), FVector(-166, 38.5, 236.5), TwCChassis); Box(FVector(-260, -96, 31.2), FVector(-200, -66, 34.7), TwCChassis);
	// the platform: floor, carriage, rails, control box, gate (shut, wide open)
	Box(FVector(-272, -58, 54.5), FVector(-186, 58, 62.4), TwCLift); Box(FVector(-184, -28, 60), FVector(-179, 28, 128), TwCLift); Box(FVector(-271.7, -57.6, 61), FVector(-267.9, 57.6, 163.8), TwCLift); Box(FVector(-271, 53.9, 61), FVector(-187, 57.7, 163.8), TwCLift);
	Box(FVector(-190.9, 30, 61), FVector(-187, 57.6, 163.4), TwCLift); Box(FVector(-190.9, -57.6, 61), FVector(-187, -30, 163.4), TwCLift); Box(FVector(-214, 49, 157), FVector(-190, 60, 171), TwCLift);
	Box(FVector(-264.5, -57.3, 110.6), FVector(-191, -54.3, 159.4), TwCGate, [](const FTractor& T) { return T.Wa->Gate < 0.1f || T.Wa->Gate > 0.98f; });
	return Tb;
}
