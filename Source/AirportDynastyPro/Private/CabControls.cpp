// The shared cab controls (CabControls.h): park brake, gears, power take-off, horn, lights, indicators and wipers of every service
// vehicle - for your hand, your keys and its hired driver alike.
#include "CabControls.h"
#include "GSE.h"
#include "GSEAmbulift.h"
#include "GSEDeicer.h"
#include "ServiceDrive.h"
#include "ServiceVehicles.h"
#include "VehicleControls.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const TCHAR* CabPiv[] = {TEXT("PIV_ParkBrake"), TEXT("PIV_GearSelect"), TEXT("PIV_PtoLever")};
	const TCHAR* CabPart[] = {TEXT("Lever_ParkBrake"), TEXT("Lever_GearSelect"), TEXT("Lever_PtoLever")};
	// (the steering wheel's axis, towards the driver: every one of these wheels lies at 45 degrees)
	const FVector CabAxis(-0.7071, 0., 0.7071);
}

bool AGSE::CabMine(const FTractor& T) const { return T.Player.IsValid() && T.PlayerSeat == 0; }

void AGSE::CabDress(FTractor& T)
{
	ABabylonModel* M = T.M.Get(); if (!M) return;
	for (int32 i = 0; i < 3; ++i) if (T.Pt.Contains(CabPiv[i])) T.GCab[i] = M->BindGroup({FString(CabPart[i])}, true, FTransform(SvcP(T, CabPiv[i])));
	// the column's stalks and the wheel's hub (no points of their own in the models; all eight have the same column)
	if (T.Pt.Contains(TEXT("PIV_SteeringWheel")))
	{
		const FVector C = SvcP(T, TEXT("PIV_SteeringWheel"));
		T.Pt.Add(TEXT("CAB_Horn"), C + CabAxis * 3.); T.Pt.Add(TEXT("CAB_StalkL"), C - CabAxis * 5. + FVector(0., -16., 0.)); T.Pt.Add(TEXT("CAB_StalkR"), C - CabAxis * 5. + FVector(0., 16., 0.));
	}
}

void AGSE::CabAdd(FVehTable& Tb, int32 Svc)
{
	using K = FVehControl;
	auto Add = [&Tb](const TCHAR* Part, const TCHAR* Pt, const TCHAR* Lb, int32 Code, bool bGrip)
	{
		K X; X.Part = Part; X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Switch = Code; X.bSeat = true; X.bGrip = bGrip; Tb.Controls.Add(X);
	};
	bool bHorn = false; for (const K& C : Tb.Controls) if (C.Switch == ADPCab::Horn) bHorn = true;
	Add(CabPart[0], TEXT("IK_ParkBrake"), TEXT("Park brake"), ADPCab::Park, true);
	Add(CabPart[1], TEXT("IK_GearSelect"), TEXT("Gear selector"), ADPCab::Gear, true);
	if (Svc >= 0 && (ADPService::All()[Svc].bAmbu || ADPService::All()[Svc].bDeicer)) Add(CabPart[2], TEXT("IK_PtoLever"), TEXT("Power take-off"), ADPCab::Pto, true);
	if (!bHorn) Add(TEXT(""), TEXT("CAB_Horn"), TEXT("Horn"), ADPCab::Horn, false);
	Add(TEXT(""), TEXT("CAB_StalkL"), TEXT("Indicators"), ADPCab::Ind, false);
	Add(TEXT(""), TEXT("CAB_StalkR"), TEXT("Wipers"), ADPCab::Wiper, false);
	if (!Tb.Frame) Tb.Frame = [](const AGSE&, const FTractor&, int32) { return FTransform::Identity; };
	if (!Tb.Now) Tb.Now = [](const FTractor&, int32) { return 0.f; };
	if (!Tb.Want) Tb.Want = [](FTractor&, int32, float) {};
	if (!Tb.May) Tb.May = [](const AGSE&, const FTractor&, int32, float, FString&) { return true; };
	if (!Tb.Stowed) Tb.Stowed = [](const AGSE&, const FTractor&) { return true; };
	if (!Tb.Stow) Tb.Stow = [](AGSE&, FTractor&) {};
}

FString AGSE::CabSay(const FTractor& T, const FVehControl& C) const
{
	if (!T.Pt.Contains(C.Point)) return FString();
	switch (C.Switch)
	{
	case ADPCab::Park: return T.bPark ? TEXT("Park brake: release") : TEXT("Park brake: put on");
	case ADPCab::Gear: return FString::Printf(TEXT("Gear selector: %s (now %s)"), T.Gear > 0 ? TEXT("to neutral") : T.Gear == 0 ? TEXT("to reverse") : TEXT("to drive"), T.Gear > 0 ? TEXT("D") : T.Gear == 0 ? TEXT("N") : TEXT("R"));
	case ADPCab::Pto: return T.bPto ? TEXT("Power take-off: off") : TEXT("Power take-off: on");
	case ADPCab::Ind: return T.Sig == 0 ? TEXT("Indicator: left") : T.Sig > 0 ? TEXT("Indicator: right") : TEXT("Indicator: off");
	case ADPCab::Wiper: return T.WipMode == 0 ? TEXT("Wipers: slow (now by the rain)") : T.WipMode == 1 ? TEXT("Wipers: fast") : T.WipMode == 2 ? TEXT("Wipers: off") : TEXT("Wipers: by the rain");
	default: return C.Label;
	}
}

bool AGSE::CabMay(int32 Ti, int32 Code, FString& Why) const
{
	const FTractor& T = Tractors[Ti]; const bool bGear = Code == ADPCab::Gear || Code == ADPCab::GearUp || Code == ADPCab::GearDown;
	if (Code == ADPCab::Park && !T.bPark && T.Speed > 300.f) Why = TEXT("too fast - brake first");
	else if (bGear && T.Speed > 10.f) Why = TEXT("stop first");
	else if (bGear && T.bPto) Why = TEXT("the power take-off is on");
	else if (Code == ADPCab::Pto && !T.bPto && (!T.bPark || T.Speed > 1.f)) Why = TEXT("the park brake first");
	else if (Code == ADPCab::Pto && !T.bPto && T.Gear != 0) Why = TEXT("the gear into neutral first");
	else if (Code == ADPCab::Pto && T.bPto && !(AmbuStowed(T) && DeicerStowed(T))) Why = TEXT("stow it first");
	return Why.IsEmpty();
}

void AGSE::CabSwitch(int32 Ti, int32 Code)
{
	FTractor& T = Tractors[Ti];
	auto Shift = [&T](int32 G) { T.Gear = int8(FMath::Clamp(G, -1, 1)); T.bGearHand = T.Gear != 1; T.bReverse = T.Gear < 0; T.BackT = 0.f; };
	switch (Code)
	{
	case ADPCab::Park: T.bPark = !T.bPark; break;
	case ADPCab::Gear: Shift(T.Gear > -1 ? T.Gear - 1 : 1); break;
	case ADPCab::GearUp: Shift(T.Gear + 1); break;
	case ADPCab::GearDown: Shift(T.Gear - 1); break;
	case ADPCab::Pto: T.bPto = !T.bPto; T.PtoT = 0.f; break;
	case ADPCab::Horn: Sound(TEXT("BTHorn"), TW(T, FVector(T.KCentreX + T.KHalfL, 0.f, 80.f))); break;   // (the electric tractor's: no other one yet)
	case ADPCab::Ind: T.Sig = T.Sig == 0 ? 1 : T.Sig > 0 ? -1 : 0; T.bSigArmed = false; break;
	case ADPCab::IndL: T.Sig = T.Sig > 0 ? 0 : 1; T.bSigArmed = false; break;
	case ADPCab::IndR: T.Sig = T.Sig < 0 ? 0 : -1; T.bSigArmed = false; break;
	case ADPCab::Beam: T.Beam = uint8((T.Beam + 1) % 3); break;
	case ADPCab::Hazard: T.bSwHaz = !T.bSwHaz; break;
	case ADPCab::Wiper: T.WipMode = uint8((T.WipMode + 1) % 4); break;
	case ADPCab::Beacon: T.bSwNoBeacon = !T.bSwNoBeacon; break;
	case ADPCab::Work: T.bSwWork = !T.bSwWork; break;
	default: break;
	}
}

void AGSE::CabDrive(FTractor& T, float& Go, float& Br)
{
	// what holds it: the park brake, neutral, the power take-off
	const TCHAR* Why = T.bPark ? TEXT("The park brake is on (Space / A releases it)") : T.bPto ? TEXT("The power take-off is on (its lever in the cab, U)") : T.Gear == 0 ? TEXT("The gear is in neutral (Page Up / B + A)") : nullptr;
	if (Why && Go > 0.1f && T.Speed < 30.f) AVehicleControls::Note(GetWorld(), Why);
	if (Why) Go = 0.f;
	if (T.bPark) Br = FMath::Max(Br, 1.f);
	// an indicator cancels itself once the wheel has gone over that way and come back
	if (T.Sig != 0)
	{
		const float Over = T.SteerWant * (T.Sig > 0 ? -1.f : 1.f);
		if (Over > 0.45f) T.bSigArmed = true;
		else if (T.bSigArmed && Over < 0.1f) { T.Sig = 0; T.bSigArmed = false; }
	}
}

void AGSE::CabLeave(FTractor& T)
{
	// (with the power take-off on, its drive runs on for the lift: never left rolling)
	T.bPark = true; T.Sig = 0; T.bSigArmed = false; T.bReverse = false;
	if (!T.bPto) { T.Gear = 1; T.bGearHand = false; }
}

void AGSE::CabTick(FTractor& T, int32 Ti, float Dt)
{
	if (T.IsTrain() && !CabMine(T)) return;   // (a baggage tractor: only with you in it)
	if (CabMine(T)) { if (!T.bGearHand) T.Gear = T.bReverse ? -1 : 1; }
	else if (T.Job != EJob::None || T.Path.Num())
	{
		// its hired driver: the brake on at the stand and off to drive, the beacon on, lights and wipers as they belong (the indicators: Lamps, as it turns)
		const bool bGo = T.Path.Num() > 0;
		T.CabStand = bGo ? 0.f : T.CabStand + Dt;
		const bool bHold = !bGo && T.CabStand > 0.5f;
		if (T.bPark != bHold) CabSwitch(Ti, ADPCab::Park);
		T.Gear = bHold ? 0 : T.bReverse ? -1 : 1; T.bGearHand = false;
		T.bSwNoBeacon = T.bSwHaz = false; T.Beam = 0; T.WipMode = 0; T.Sig = 0; T.PtoT = 0.f;
		T.bPto = bHold && ((T.Am && T.Am->bLive) || (T.Di && T.Di->Ph >= FSvcDeicer::OpIn && T.Di->Ph <= FSvcDeicer::OpOut));
	}
	else
	{
		T.bPark = true; if (!T.bPto) { T.Gear = 1; T.bGearHand = false; }
		// left with its drive running: off by itself after a while without you (it has stowed itself by then)
		const APlayerController* PC = T.bPto ? GetWorld()->GetFirstPlayerController() : nullptr; const APawn* Me = PC ? PC->GetPawn() : nullptr;
		T.PtoT = T.bPto && !T.Player.IsValid() && (!Me || FVector::DistSquared2D(Me->GetActorLocation(), T.At) > FMath::Square(4000.f)) ? T.PtoT + Dt : 0.f;
		if (T.PtoT > 150.f && AmbuStowed(T) && DeicerStowed(T)) { T.bPto = false; T.PtoT = 0.f; Motor(T, false); ADP_LOG(TEXT("Vehicle: the %s was left running - its drive is off"), *T.Name); }
	}
	ABabylonModel* M = T.M.Get(); if (!M) return;
	const float Want[] = {T.bPark ? -22.f : 0.f, 12.f * T.Gear, T.bPto ? 20.f : 0.f};
	for (int32 i = 0; i < 3; ++i)
		if (T.GCab[i] != INDEX_NONE && T.CabNow[i] != Want[i])
		{
			T.CabNow[i] = FMath::FInterpConstantTo(T.CabNow[i], Want[i], Dt, 90.f);
			M->MoveGroup(T.GCab[i], FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(T.CabNow[i])), SvcP(T, CabPiv[i])));
		}
}
