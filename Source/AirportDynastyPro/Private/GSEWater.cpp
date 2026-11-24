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
	const FVector WaPanel[2] = {FVector(-30.f, 2620.f, 293.f), FVector(80.f, 2620.f, 313.f)};   // (the aircraft's root, at the nose wheel: x its right, y aft)
	const FVector WaWant[2] = {FVector(-240.f, -30.f, 293.f), FVector(-212.f, 5.f, 313.f)};     // (where they are from the truck in its place)
	const FVector WaWork[2] = {FVector(-240.f, 6.f, 0.f), FVector(-246.f, 4.f, 0.f)};           // (where the driver stands on the platform for them)
	const float WaRinseY = 22.f, WaGiveUp = 2400.f;   // (the rinse nipple beside the drain; 40 min of waiting for the place)
	const float WaFill = 240.f, WaDrain = 240.f, WaRinseS = 120.f;   // (200 l at 50 l a minute)
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
		F.GReel = Pre(TEXT("RinseReel"), ReelP); F.GTail = Pre(TEXT("RinseTail"), SvcP(T, TEXT("IK_RinseOut"))); F.GHose = One(TEXT("WasteHose"), SvcP(T, TEXT("IK_WasteHose_In")));
	}
	else
	{
		Thing(FSvcWet::Main, TEXT("Coupling"), TEXT("PIV_Coupling"), TEXT("IK_Coupling_Hand"));
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
	ABabylonModel* M = T.M.Get(); if (!M || !T.Wa || !T.Wa->bLive) return;
	FSvcWet& F = *T.Wa; const bool bT = F.bToilet;
	auto Put = [M](int32 G, const FTransform& X) { if (G != INDEX_NONE) M->MoveGroup(G, X); };
	auto Show = [](const FVector& P, bool b) { return FTransform(FQuat::Identity, P, b ? FVector::OneVector : FVector(0.001f)); };
	const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At); const FVector Side = FVector::RightVector;
	// the platform, its gate, levers, the splash tray
	const float WasLift = F.Lift; F.Lift = FMath::FInterpConstantTo(F.Lift, F.LiftWant, Dt, 8.f); F.Gate = FMath::FInterpConstantTo(F.Gate, F.GateWant, Dt, 1.1f);
	F.Valve = FMath::FInterpConstantTo(F.Valve, F.ValveWant, Dt, 1.8f); F.Tray = FMath::FInterpConstantTo(F.Tray, F.TrayWant, Dt, 1.2f);
	if (bT)
	{
		Put(F.GLift, FTransform(WaUp * F.Lift)); Put(F.GTray, FTransform(WaRot(Side, -85.f * F.Tray), SvcP(T, TEXT("PIV_SplashTray"))));
		Put(F.GPLever, FTransform(WaRot(Side, F.Lift > WasLift ? -15.f : F.Lift < WasLift ? 15.f : 0.f), SvcP(T, TEXT("PIV_Lever_Platform")) + WaUp * F.Lift));
		if (F.Lift != WasLift) for (UActorComponent* C : M->GetComponents()) if (C && C->ComponentHasTag(TEXT("SvcRear"))) Cast<USceneComponent>(C)->AddRelativeLocation(WaUp * (F.Lift - WasLift));
	}
	Put(F.GGate, FTransform(WaRot(WaUp, 80.f * FMath::SmoothStep(0.f, 1.f, F.Gate)), SvcP(T, TEXT("PIV_GatePlatform")) + WaUp * F.Lift));
	Put(F.GValve, FTransform(WaRot(Side, 60.f * F.Valve), SvcP(T, bT ? TEXT("PIV_Lever_Vent") : TEXT("PIV_Lever_Valve"))));
	// the carried things: in their place, in a hand, or put somewhere
	const ATugs* G = Tg(); const USkeletalMeshComponent* B = F.Ci != INDEX_NONE ? Body(F.Ci) : nullptr;
	const bool bBody = B && G && G->Crew[F.Ci].A.IsValid() && !G->Crew[F.Ci].A->IsHidden();
	bool bAll = true;
	for (int32 i = 0; i < FSvcWet::NThings; ++i)
	{
		FSvcWet::FThing& X = F.Th[i]; if (X.G == INDEX_NONE) continue;
		FVector Want = X.Piv; float Tip = 0.f;
		if (X.Mode == FSvcWet::Put) { Want = X.At + (X.bRide ? WaUp * F.Lift : FVector::ZeroVector); Tip = X.Tip; }
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
	const float L = Run(bT ? F.RinseHose : F.Hose, From, Rl.Now + SvcP(T, bT ? TEXT("IK_RinseCoupling_Hose") : TEXT("IK_Coupling_Hose")) - Rl.Piv, 30.f, Out(Rl));
	Put(F.GTail, Show(From, !Out(Rl)));
	if (!FMath::IsNearlyEqual(L, F.OutWas, 0.05f))
	{
		F.Reel += (L - F.OutWas) / 17.f; F.OutWas = L;
		const FTransform X0(FQuat(Side, -F.Reel), SvcP(T, bT ? TEXT("PIV_RinseReel") : TEXT("PIV_HoseReel"))); Put(F.GReel, X0);
		float Left = L; const float Per = bT ? 75.f : 98.f;
		for (int32 i = F.GTurn.Num() - 1; i >= 0; --i) { FTransform X = X0; if (Left >= Per * 0.5f) X.SetScale3D(FVector(0.001f)); Left -= Per; Put(F.GTurn[i], X); }
	}
	const int32 Me = int32(&T - Tractors.GetData()); bool bJob = false; for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Phase == 2 && P.Value.Ti == Me) bJob = true;
	if (!bJob && bAll && F.Lift == F.LiftWant && F.Gate == F.GateWant && F.Valve == F.ValveWant && F.Tray == F.TrayWant && L == 0.f) F.bLive = false;
}

void AGSE::WetTick(FSvcJob& J, float Dt)
{
	if (J.Phase != 2 || !Tractors.IsValidIndex(J.Ti) || !Tractors[J.Ti].Wa || !Tractors[J.Ti].Wa->bRun) return;
	J.Pumped = FMath::Min(J.Litres, J.Pumped + Dt);   // (seconds)
}

// ------------------------------------------------------------------ its place under the tail: one at a time

const AActor* AGSE::WetPlane(float StandX) const
{
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(StandX, S, N, Sp); const AActor* Pl = nullptr; double Bd = 900.0; const FVector Nose = Plan(S, N);
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
	TArray<FVector> W; Leg(W, X0, Yn + 4.f, 0.f, -1.f, X0, Yn, 0.f, -1.f, T.KTurnR); Leg(W, X0, Yn, 0.f, -1.f, X, Yn, 0.f, 1.f, T.KTurnR * 0.98f); Leg(W, X, Yn, 0.f, 1.f, X, Yn + 2.f, 0.f, 1.f, T.KTurnR);
	const float Y = N + Sv.Aft;
	Go(T, MoveTemp(W), false, [this, Job, Ti, X, Y]
	{
		TArray<FVector> R; const FVector A = Tractors[Ti].At, E = Plan(X, Y); for (int32 i = 1; i <= 40; ++i) R.Add(FMath::Lerp(A, E, i / 40.f));
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
	const FVector C = Root.InverseTransformPosition(Pl ? Pl->GetActorTransform().TransformPosition(WaPanel[k]) : FVector(Plan(S - float(WaPanel[k].X) / 100.f, N + float(WaPanel[k].Y) / 100.f) + WaUp * WaPanel[k].Z));
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
		}, FMath::Max(0.8f, float(FVector::Dist2D(A, B)) / 55.f));
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
			PoseAct(Ci, WetClimbFn(Ti, true), bT ? 3.8f : 4.8f); Stay(PIn, 90.f);
			Hand(PIn, 1, GO, 1.2f, [Wt] { Wt().GateWant = 0.f; }, false, true, true);
			return;
		}
		Step(Wk, WkYaw, PIn, 90.f);
		Hand(PIn, 1, GH, 1.2f, [Wt] { Wt().GateWant = 1.f; }, false, true, true);
		Wait(Ci, 0.7f); Free();
		PoseAct(Ci, WetClimbFn(Ti, false), bT ? 3.6f : 4.6f);
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
		RunFor(TkAt, 90.f, false, WaDrain);
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
