// The ambulift's moving parts (to-do 212): they follow pose() in Art/Ambulift/ambulift_checks.py. Whoever works a part - its crew
// (GSEAmbuliftWork.cpp) or you - only sets FSvcAmbu::Want; AmbuParts moves it at its real speed and AmbuMay keeps the interlocks,
// so nobody can do what the vehicle does not allow. And its two wheelchairs.
#include "GSEAmbulift.h"
#include "GSE.h"
#include "VehicleControls.h"
#include "ServiceVehicles.h"
#include "Tugs.h"
#include "VehicleRiders.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"

using FA = FSvcAmbu;

namespace
{
	const FVector AmUp(0, 0, 1), AmY(0, 1, 0), AmX(1, 0, 0);
	const float AmH0 = 12.f, AmSL = 460.f, AmLow = -81.92f;
	const FVector AmHinge(282.f, 0.f, 125.f), AmTailH(-427.96f, 0.f, 125.f), AmTailA(-360.f, 0.f, 66.f), AmFlapP(-571.6f, 0.f, 126.8f);
	const float AmRate[FA::Parts] = {6.f, 11.f, 9.f, 0.35f, 22.f, 18.f, 14.f, 10.f, 0.4f, 45.f, 0.5f, 0.5f};   // (cm, degrees or 0..1 a second)
	const TCHAR* AmKey[] = {TEXT("DoorFront"), TEXT("DoorFrontOut"), TEXT("DoorRearIn"), TEXT("DoorRearOut"), TEXT("EStop"), TEXT("Light"), TEXT("Blue"), TEXT("Heater"), TEXT("Work"),
		TEXT("InTailUp"), TEXT("InTailDown"), TEXT("InTailFold"), TEXT("InFlap"), TEXT("InTailStop"), TEXT("TailUp"), TEXT("TailDown"), TEXT("TailFold"), TEXT("TailStop"), TEXT("StabEStop"),
		TEXT("CabHazard"), TEXT("CabBlue"), TEXT("CabStop"), TEXT("CabBeacon"), TEXT("CabWork"), TEXT("Horn")};
	FTransform AmT(const FVector& V) { return FTransform(V); }
	FTransform AmRot(const FVector& P, const FVector& Ax, float Deg) { return FTransform(-P) * FTransform(FQuat(Ax, FMath::DegreesToRadians(Deg))) * FTransform(P); }   // (Blender: about y the same sign, about x and z the other)
	FTransform AmAim(const FVector& A, const FVector& B0, const FVector& B1) { return FTransform(-A) * FTransform(FQuat::FindBetweenNormals((B0 - A).GetSafeNormal(), (B1 - A).GetSafeNormal())) * FTransform(A); }
	const FVector AmBay[2] = {FVector(-160.f, 74.f, 129.f), FVector(-160.f, -74.f, 129.f)};   // (R1, L1)
}

void AGSE::AmbuDress(FTractor& T)
{
	ABabylonModel* M = T.M.Get(); if (!M) return;
	T.Am = MakeShared<FSvcAmbu>(); FSvcAmbu& C = *T.Am;
	const FTransform I = FTransform::Identity;
	auto Pre = [&](const TCHAR* N, const TCHAR* S = TEXT("")) { return M->BindGroup({FString(N) + S}, true, I); };
	static const TCHAR* W[] = {TEXT("FL"), TEXT("FR"), TEXT("RL"), TEXT("RR")}; static const TCHAR* LR[] = {TEXT("L"), TEXT("R")};
	for (int32 i = 0; i < 4; ++i) C.GLeg[i] = Pre(TEXT("StabLeg_"), W[i]);
	C.GSc[0] = Pre(TEXT("ScissorA_")); C.GSc[1] = Pre(TEXT("ScissorB_")); C.GDeck = Pre(TEXT("Plat_")); C.GBridge = Pre(TEXT("Bridge")); C.GArms = Pre(TEXT("TailArms")); C.GFlap = Pre(TEXT("TailFlap")); C.GTail = Pre(TEXT("TailPlatform"));
	for (int32 r = 0; r < 2; ++r)
	{
		C.GRam[r] = Pre(TEXT("RamBarrel_"), LR[r]); C.GRod[r] = Pre(TEXT("RamRod_"), LR[r]); C.GRail[r] = Pre(TEXT("RailPlat_"), LR[r]); C.GGate[r] = Pre(TEXT("GateSlide_"), LR[r]); C.GTRail[r] = Pre(TEXT("TailRail_"), LR[r]);
		C.GPb[r] = Pre(r ? TEXT("PlatRamR_Barrel") : TEXT("PlatRamL_Barrel")); C.GPr[r] = Pre(r ? TEXT("PlatRamR_Rod") : TEXT("PlatRamL_Rod")); C.GDoorF[r] = Pre(TEXT("DoorFront"), LR[r]); C.GDoorR[r] = Pre(TEXT("DoorRear"), LR[r]);
	}
	static const TCHAR* Lv[] = {TEXT("Lever_Stab"), TEXT("Lever_Lift"), TEXT("Lever_Plat"), TEXT("Lever_Bridge")};
	for (int32 i = 0; i < 4; ++i) C.GLev[i] = Pre(Lv[i]);
	for (const TCHAR* K : AmKey)
	{
		const FString N = FString(TEXT("Button_")) + K;
		C.Keys.Add(TPair<int32, FVector>(M->BindGroupWhere([&](const FString& X, int32, const FVector&) { return X == N || (X.StartsWith(N) && X.Len() > N.Len() && !FChar::IsAlpha(X[N.Len()])); }, I), SvcP(T, *(TEXT("PIV_") + N))));
	}
	// what only rides up with the body (ambulift-parts.json moves_with_body, less what has a group of its own)
	C.GBody = M->BindGroupWhere([](const FString& N, int32, const FVector&)
	{
		for (const TCHAR* X : {TEXT("TailBrake"), TEXT("TailArms"), TEXT("TailFlap"), TEXT("TailPlatform"), TEXT("TailRail"), TEXT("BlueGrille"), TEXT("NamePanel_Rear"), TEXT("Plat_"), TEXT("PlatRam")}) if (N.StartsWith(X)) return false;
		for (const TCHAR* X : {TEXT("Box"), TEXT("Stretcher"), TEXT("Strap"), TEXT("BeltBay"), TEXT("Blue"), TEXT("Beacon"), TEXT("Tail"), TEXT("NamePanel_L"), TEXT("NamePanel_R"), TEXT("Platform")}) if (N.StartsWith(X)) return true;
		return false;
	}, I);
	// its two wheelchairs, in the bays R1 and L1
	FVehicleSpec Wc; if (!Wc.Load(TEXT("wheelchair"))) return;
	for (int32 i = 0; i < 2; ++i)
	{
		ABabylonModel* K = GetWorld()->SpawnActor<ABabylonModel>(T.At, FRotator::ZeroRotator);
		if (!K || !K->Place(TEXT("Wheelchair"))) { ADP_WARN(TEXT("GSE: no Wheelchair model - the ambulift has no wheelchairs")); if (K) K->Destroy(); break; }
		K->SetPartsCollision(false); FA::FChair& H = C.Chair[i]; H.M = K;
		for (int32 r = 0; r < 2; ++r)
		{
			H.GWheel[r] = K->BindGroup({FString(TEXT("Wheel_")) + LR[r]}, true, FTransform(Wc.At(*(FString(TEXT("PIV_Wheel_")) + LR[r]))));
			H.GCast[r] = K->BindGroup({FString(TEXT("Castor_")) + LR[r]}, true, FTransform(Wc.At(*(FString(TEXT("PIV_Castor_")) + LR[r]))));
		}
	}
	for (const TCHAR* K : {TEXT("IK_Seat"), TEXT("IK_Arm_L"), TEXT("IK_Arm_R"), TEXT("IK_Foot_L"), TEXT("IK_Foot_R"), TEXT("IK_Push_L"), TEXT("IK_Push_R"), TEXT("PIV_Wheel_L"), TEXT("PIV_Wheel_R"), TEXT("PIV_Castor_L"), TEXT("PIV_Castor_R")}) T.Pt.Add(FString(TEXT("WC_")) + K, Wc.At(K));
}

bool AGSE::AmbuStowed(const FTractor& T) const
{
	if (T.Am && T.Am->bStop) return false;
	if (T.Am) for (int32 p = 0; p < FA::Parts; ++p) if (FMath::Abs(T.Am->Now[p]) > 0.05f || FMath::Abs(T.Am->Want[p]) > 0.05f) return false;
	return true;
}

bool AGSE::AmbuMay(const FTractor& T, int32 Part, float To) const
{
	const FSvcAmbu& C = *T.Am; const float* N = C.Now; const bool bOut = FMath::Abs(To) > FMath::Abs(N[Part]);
	if (C.bStop) return false;   // (an emergency stop)
	// you on it: no floor folds away under you, no door shuts on you; nothing comes down on you
	if (T.YouOn) { const bool bStop = (Part == FA::Legs && !bOut) || (T.YouOn == 2 && Part == FA::Tail) || (T.YouOn == 3 && Part == FA::Plat); if (bStop && T.Job != EJob::None) T.YouStopAt = GetWorld()->GetTimeSeconds(); if (Part == FA::Legs && !bOut) return false; if (T.YouOn == 2 && Part == FA::Tail) return false; if (T.YouOn == 3 && Part == FA::Plat) return false; }
	if (FMath::Abs(T.YouAt.Y) < 62.f && !bOut && ((Part == FA::DoorF && FMath::Abs(T.YouAt.X - 254.f) < 48.f) || (Part == FA::DoorR && FMath::Abs(T.YouAt.X + 395.f) < 48.f))) return false;
	if (!T.YouOn && FMath::Abs(T.YouAt.Y) < 120.f && T.YouAt.X < -400.f && T.YouAt.X > -600.f && ((Part == FA::Arms && bOut) || Part == FA::Tail)) return false;
	switch (Part)
	{
	case FA::Legs: return T.MeasSpeed < 0.02f && !T.Path.Num() && (bOut || (N[FA::Lift] <= 0.f && N[FA::Tail] <= 0.f));
	case FA::Lift: return N[FA::Legs] > 29.9f && N[FA::Tail] <= 0.f && N[FA::DoorR] <= 0.f && N[FA::Bridge] <= 0.f && (N[FA::Plat] <= 0.f || To >= 102.f);
	case FA::Plat: return N[FA::Lift] >= 102.f && N[FA::Rails] <= 0.f && N[FA::Bridge] <= 0.f && N[FA::DoorF] <= 0.f;
	case FA::Rails: return N[FA::Plat] > 89.9f && N[FA::Gates] <= 0.f && N[FA::Bridge] <= 0.f;
	case FA::Gates: case FA::Bridge: return N[FA::Rails] > 0.99f;
	case FA::DoorF: return !bOut || N[FA::Plat] > 89.9f;
	case FA::Tail: return N[FA::Lift] <= 0.f && N[FA::Legs] > 29.9f && N[FA::DoorR] <= 0.f && N[FA::TRails] <= 0.f && N[FA::Arms] >= -0.05f && N[FA::Flap] <= 0.f;
	case FA::Arms: case FA::TRails: return N[FA::Tail] > 89.9f;
	case FA::Flap: return N[FA::Tail] > 89.9f && (To >= 0.f || N[FA::Arms] < AmLow + 1.f);
	case FA::DoorR: return !bOut || (N[FA::Tail] > 89.9f && N[FA::Arms] > -0.5f);
	}
	return true;
}

void AGSE::AmbuParts(FTractor& T, float Dt)
{
	ABabylonModel* M = T.M.Get(); if (!M || !T.Am) return;
	FSvcAmbu& C = *T.Am; float* N = C.Now;
	AmbuRolls(T, Dt); AmbuChairs(T, Dt);
	if (!C.bLive && AmbuStowed(T) && T.Job != EJob::AtStand) return;
	C.bLive = true;
	auto Put = [M](int32 G, const FTransform& X) { if (G != INDEX_NONE) M->MoveGroup(G, X); };
	// (the flap by itself: a stop while the deck is up, a ramp on the ground, flat when it stands)
	C.Want[FA::Flap] = N[FA::Tail] > 89.9f && C.Want[FA::Tail] > 89.f ? (N[FA::Arms] < AmLow + 0.5f && C.Want[FA::Arms] < -80.f ? -12.f : 90.f) : 0.f;
	for (int32 p = 0; p < FA::Parts; ++p)
	{
		const float Was = N[p]; if (Was != C.Want[p] && AmbuMay(T, p, C.Want[p])) N[p] = FMath::FInterpConstantTo(N[p], C.Want[p], Dt, AmRate[p]);
		C.bMove[p] = N[p] != Was;
	}
	const float q = N[FA::Lift], T0 = 0.02609f, An = FMath::Asin((AmH0 + q) / AmSL), Td = FMath::RadiansToDegrees(An - T0); const FTransform U = AmT(AmUp * q);
	Put(C.GBody, U);
	for (int32 i = 0; i < 4; ++i) Put(C.GLeg[i], AmT(-AmUp * N[FA::Legs]));
	const FVector Mid(235.f - 0.5f * AmSL * FMath::Cos(An), 0.f, 97.f + (AmH0 + q) * 0.5f), M0(235.f - 0.5f * AmSL * FMath::Cos(T0), 0.f, 97.f + AmH0 * 0.5f);
	for (int32 i = 0; i < 2; ++i) Put(C.GSc[i], AmT(-M0) * FTransform(FQuat(AmY, FMath::DegreesToRadians(i ? -Td : Td))) * AmT(Mid));
	const float Ea = 0.00878f + An - T0;
	for (int32 r = 0; r < 2; ++r)
	{
		const FVector B = SvcP(T, r ? TEXT("PIV_Ram_R") : TEXT("PIV_Ram_L")), E0 = SvcP(T, r ? TEXT("PIV_RamEye_R") : TEXT("PIV_RamEye_L")), E1(235.f - 259.92f * FMath::Cos(Ea), E0.Y, 97.f + 259.92f * FMath::Sin(Ea));
		const FTransform R = AmAim(B, E0, E1); Put(C.GRam[r], R); Put(C.GRod[r], R * AmT(E1 - B - (E1 - B).GetSafeNormal() * (E0 - B).Size()));
	}
	// the front platform with its rails, gates and bridge (the model's parts lie folded: back out of both foldings, up by the angle, then with the platform)
	const FTransform O = AmRot(AmHinge, AmY, N[FA::Plat]), Fold = AmRot(AmHinge, AmY, -90.f), TFold = AmRot(AmTailH, AmY, 90.f), Mt = AmRot(AmTailH, AmY, -N[FA::Tail]) * AmT(AmbuTip(C));
	Put(C.GDeck, O * U); Put(C.GBridge, AmT(AmUp * N[FA::Bridge]) * O * U);
	Put(C.GArms, AmRot(AmTailA, AmY, N[FA::Arms]) * U); Put(C.GTail, Mt * U); Put(C.GFlap, TFold.Inverse() * AmRot(AmFlapP, AmY, N[FA::Flap]) * TFold * Mt * U);
	for (int32 r = 0; r < 2; ++r)
	{
		const float Sg = r ? 1.f : -1.f; auto Ang = [r](float E) { return 90.f - 90.f * FMath::Clamp(2.f * E - float(r), 0.f, 1.f); };
		const FVector Pr = SvcP(T, r ? TEXT("PIV_RailPlat_R") : TEXT("PIV_RailPlat_L")), Pt = SvcP(T, r ? TEXT("PIV_TailRail_R") : TEXT("PIV_TailRail_L"));
		const FTransform F = (AmRot(Pr, AmX, 90.f * Sg) * Fold).Inverse(), X = AmRot(Pr, AmX, Ang(N[FA::Rails]) * Sg) * Fold * O * U;
		Put(C.GRail[r], F * X); Put(C.GGate[r], F * AmT(AmX * N[FA::Gates]) * X);
		Put(C.GTRail[r], (AmRot(Pt, AmX, 90.f * Sg) * TFold).Inverse() * AmRot(Pt, AmX, Ang(N[FA::TRails]) * Sg) * TFold * Mt * U);
		const FVector B = SvcP(T, r ? TEXT("PIV_PlatRam_R") : TEXT("PIV_PlatRam_L")), E0 = SvcP(T, r ? TEXT("PIV_PlatRamEye_R") : TEXT("PIV_PlatRamEye_L")), E1 = O.TransformPosition(E0); const FTransform R = AmAim(B, E0, E1);
		Put(C.GPb[r], R * U); Put(C.GPr[r], R * AmT(E1 - B - (E1 - B).GetSafeNormal() * (E0 - B).Size()) * U);
		const float S0 = r ? 56.f : -56.f; Put(C.GDoorF[r], AmT(FVector(0.f, S0 * FMath::SmoothStep(0.f, 1.f, N[FA::DoorF]), q))); Put(C.GDoorR[r], AmT(FVector(0.f, S0 * FMath::SmoothStep(0.f, 1.f, N[FA::DoorR]), q)));
	}
	// levers and buttons (pressed 4 mm into what they sit on)
	static const TCHAR* Lp[] = {TEXT("PIV_Lever_Stab"), TEXT("PIV_Lever_Lift"), TEXT("PIV_Lever_Plat"), TEXT("PIV_Lever_Bridge")}; static const FA::EPart Lq[] = {FA::Legs, FA::Lift, FA::Plat, FA::Bridge};
	for (int32 i = 0; i < 4; ++i)
	{
		if (C.bMove[Lq[i]]) C.LevTo[i] = C.Want[Lq[i]] > 0.5f ? 1.f : -1.f;
		C.Lev[i] = FMath::FInterpConstantTo(C.Lev[i], C.bMove[Lq[i]] ? C.LevTo[i] : 0.f, Dt, 5.f);
		Put(C.GLev[i], AmRot(SvcP(T, Lp[i]), AmX, 20.f * C.Lev[i]) * (i ? U : FTransform::Identity));
	}
	for (int32 k = 0; k < C.Keys.Num(); ++k)
	{
		C.Press[k] = FMath::Max(0.f, C.Press[k] - Dt); const float d = FMath::Min(1.f, C.Press[k] * 6.f) * 0.4f;
		const FVector In = k == 3 ? AmX : k == 1 ? -AmX : k == 18 ? AmY : k >= 19 ? FVector(0.8f, 0.f, -0.6f) : k <= 8 && k != 2 ? -AmY : k <= 13 ? -AmX : -AmY;
		Put(C.Keys[k].Key, AmT(In * d + (k < 18 ? AmUp * q : FVector::ZeroVector)));
	}
	// (the airport's name rides along)
	if (C.bMove[FA::Lift] || C.bMove[FA::Tail] || C.bMove[FA::Arms])
		for (UActorComponent* K : M->GetComponents())
		{
			USceneComponent* S = Cast<USceneComponent>(K); if (!S) continue;
			if (K->ComponentHasTag(TEXT("SvcBody"))) { FVector L = S->GetRelativeLocation(); L.Z = SvcP(T, TEXT("IK_Name_L")).Z + q; S->SetRelativeLocation(L); }
			if (K->ComponentHasTag(TEXT("SvcRear"))) { const FTransform X = FTransform(FRotator(0.f, 180.f, 0.f), SvcP(T, TEXT("IK_Name_Rear")) - AmX * 1.2f) * Mt * U; S->SetRelativeLocationAndRotation(X.GetLocation(), X.GetRotation()); }
		}
	if (T.Job != EJob::AtStand && AmbuStowed(T)) { C.bLive = false; for (int32 k = 0; k < C.Keys.Num(); ++k) if (C.Press[k] > 0.f) C.bLive = true; for (float V : C.Lev) if (V != 0.f) C.bLive = true; }
}

FVector AGSE::AmbuTip(const FSvcAmbu& C) { return AmRot(AmTailA, AmY, C.Now[FA::Arms]).TransformPosition(AmTailH) - AmTailH; }

void AGSE::AmbuPress(int32 Ti, const TCHAR* Key) { if (FSvcAmbu* C = Tractors[Ti].Am.Get()) for (int32 k = 0; k < int32(UE_ARRAY_COUNT(AmKey)); ++k) if (FCString::Strcmp(AmKey[k], Key) == 0) C->Press[k] = 0.9f; }

float AGSE::AmbuBlue(const FTractor& T, int8 Side) const
{
	// the blue lamps: on the way to a job and back, and while the body is up; a double flash, one half of each bar after the other
	const FSvcAmbu* C = T.Am.Get(); if (!C) return 0.f;
	const bool bJob = T.Job == EJob::Out || (T.Job == EJob::Back && T.Path.Num() > 0) || C->Now[FA::Lift] > 0.5f;
	if (!bJob && !C->bBlue) return 0.f;
	const float F = FMath::Frac(Flash * 1.15f + (Side > 0 ? 0.5f : 0.f)); return F < 0.07f || (F > 0.14f && F < 0.21f) ? 1.f : 0.f;
}

void AGSE::AmbuRestock(FTractor& T) { if (T.Am) for (FA::FChair& K : T.Am->Chair) { K.bBay = true; K.bHide = false; K.BayYaw = 0.f; } }

void AGSE::AmbuChairs(FTractor& T, float Dt)
{
	FSvcAmbu& C = *T.Am;
	for (int32 i = 0; i < 2; ++i)
	{
		FA::FChair& K = C.Chair[i]; ABabylonModel* M = K.M.Get(); if (!M) continue;
		if (K.bBay) { K.At = TW(T, AmBay[i] + AmUp * C.Now[FA::Lift]); K.Yaw = T.Yaw + K.BayYaw; K.Pitch = 0.f; }
		if (M->IsHidden() != K.bHide) M->SetActorHiddenInGame(K.bHide);
		if (AActor* A = K.Who.Get()) if (A->IsHidden() != K.bHide) A->SetActorHiddenInGame(K.bHide);
		const FRotator Rt = M->GetActorRotation(); if (FVector::DistSquared(M->GetActorLocation(), K.At) < 0.01 && FMath::Abs(FMath::FindDeltaAngleDegrees(Rt.Yaw, K.Yaw)) < 0.05f && FMath::Abs(Rt.Pitch - K.Pitch) < 0.05f) continue;
		const FVector D = K.At - M->GetActorLocation(); if (!K.bBay) K.Spin += float(FVector::DotProduct(FVector(D.X, D.Y, 0.), FRotator(0.f, K.Yaw, 0.f).Vector()));
		M->SetActorLocationAndRotation(K.At, FRotator(K.Pitch, K.Yaw, 0.f));
		for (int32 r = 0; r < 2 && !K.bBay; ++r)
		{
			if (K.GWheel[r] != INDEX_NONE) M->MoveGroup(K.GWheel[r], FTransform(FQuat(AmY, K.Spin / 30.5f), SvcP(T, r ? TEXT("WC_PIV_Wheel_R") : TEXT("WC_PIV_Wheel_L"))));
			if (K.GCast[r] != INDEX_NONE) M->MoveGroup(K.GCast[r], FTransform(FQuat(AmY, K.Spin / 9.5f), SvcP(T, r ? TEXT("WC_PIV_Castor_R") : TEXT("WC_PIV_Castor_L"))));
		}
	}
}

// ------------------------------------------------------------------ your hands on its controls (to-do 238, VehicleControls.h)

FString AGSE::AmbuWhy(const FTractor& T, int32 Part, float To) const
{
	// (AmbuMay said no: the first thing in its way)
	const FSvcAmbu& C = *T.Am; const float* N = C.Now; const bool bOut = FMath::Abs(To) > FMath::Abs(N[Part]);
	if (C.bStop) return TEXT("an emergency stop is pressed");
	const bool bLegs = N[FA::Legs] > 29.9f;
	switch (Part)
	{
	case FA::Legs: return T.MeasSpeed > 0.02f || T.Path.Num() ? TEXT("it is moving") : N[FA::Lift] > 0.f ? TEXT("the body is up") : N[FA::Tail] > 0.f ? TEXT("the tail lift is open") : TEXT("you are standing on it");
	case FA::Lift: return !bLegs ? TEXT("the legs are not down") : N[FA::Tail] > 0.f ? TEXT("the tail lift is open") : N[FA::DoorR] > 0.f ? TEXT("the rear doors are open") : N[FA::Bridge] > 0.f ? TEXT("the bridge is out") : TEXT("the front platform is out");
	case FA::Plat: return N[FA::Lift] < 102.f ? TEXT("the body is too low, the cab is in the way") : N[FA::Rails] > 0.f ? TEXT("its rails are up") : N[FA::Bridge] > 0.f ? TEXT("the bridge is out") : N[FA::DoorF] > 0.f ? TEXT("the front doors are open") : TEXT("you are standing on it");
	case FA::Rails: return N[FA::Plat] < 89.9f ? TEXT("the platform is not out") : N[FA::Gates] > 0.f ? TEXT("the gates are out") : TEXT("the bridge is out");
	case FA::Gates: case FA::Bridge: return TEXT("the rails are not up");
	case FA::DoorF: return bOut ? TEXT("the platform is not out") : TEXT("you are in the doorway");
	case FA::Tail: return N[FA::Lift] > 0.f ? TEXT("the body is up") : !bLegs ? TEXT("the legs are not down") : N[FA::DoorR] > 0.f ? TEXT("the rear doors are open") : N[FA::TRails] > 0.f ? TEXT("its rails are up") : N[FA::Arms] < -0.05f ? TEXT("the deck is not at floor height") : TEXT("you are standing too near");
	case FA::Arms: return N[FA::Tail] < 89.9f ? TEXT("the deck is folded") : TEXT("you are standing under it");
	case FA::TRails: case FA::Flap: return TEXT("the deck is folded");
	case FA::DoorR: return !bOut ? TEXT("you are in the doorway") : N[FA::Tail] < 89.9f ? TEXT("the tail lift is folded") : TEXT("the deck is not at floor height");
	}
	return TEXT("not now");
}

TSharedPtr<FVehTable> AGSE::AmbuControls()
{
	using K = FVehControl;
	// what a point rides on; points are as the model names them: the platforms' deployed, the tail deck's level
	enum { Chassis, Body, Plat, RailL, RailR, GateL, GateR, Deck, DRailL, DRailR, Folded };
	enum { SwHaz, SwBlue, SwBeacon, SwWork, SwLight, SwHorn, SwStop };
	const FVector L(0, -1, 0), R(0, 1, 0), F(1, 0, 0), B(-1, 0, 0), Dash(-0.8f, 0.f, 0.6f), No(0, 0, 0);
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>(); TArray<K>& C = Tb->Controls;
	auto Mo = [&C](const TCHAR* Part, const TCHAR* Pt, K::EKind Kind, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 M, float A, float Bv, bool bTurns = true) { K X; X.Part = Part; X.Point = Pt; X.Kind = Kind; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Motion = M; X.A = A; X.B = Bv; X.bTurns = bTurns; C.Add(X); };
	auto Sw = [&C](const TCHAR* Part, const TCHAR* Pt, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 S, bool bSeat = false) { K X; X.Part = Part; X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Switch = S; X.bSeat = bSeat; C.Add(X); };
	// the legs, outside on the left
	Mo(TEXT("Lever_Stab"), TEXT("IK_Lever_Stab"), K::Lever, TEXT("Legs down / up"), Chassis, L, FA::Legs, 0.f, 30.f);
	Sw(TEXT("Button_StabEStop"), TEXT("IK_Button_StabEStop"), TEXT("Emergency stop"), Chassis, L, SwStop);
	// the panel inside at the front left
	Mo(TEXT("Lever_Lift"), TEXT("IK_Lever_Lift"), K::Lever, TEXT("Body up / down"), Body, R, FA::Lift, 0.f, 290.f);
	Mo(TEXT("Lever_Plat"), TEXT("IK_Lever_Plat"), K::Lever, TEXT("Front platform out / in"), Body, R, FA::Plat, 0.f, 90.f);
	Mo(TEXT("Lever_Bridge"), TEXT("IK_Lever_Bridge"), K::Lever, TEXT("Bridge out / in"), Body, R, FA::Bridge, 0.f, 50.f);
	Mo(TEXT("Button_DoorFront"), TEXT("IK_Button_DoorFront"), K::Push, TEXT("Front doors"), Body, R, FA::DoorF, 0.f, 1.f);
	Sw(TEXT("Button_EStop"), TEXT("IK_Button_EStop"), TEXT("Emergency stop"), Body, R, SwStop);
	Sw(TEXT("Button_Light"), TEXT("IK_Button_Light"), TEXT("Cabin lights"), Body, R, SwLight);
	Sw(TEXT("Button_Blue"), TEXT("IK_Button_Blue"), TEXT("Blue lights"), Body, R, SwBlue);
	Sw(TEXT("Button_Work"), TEXT("IK_Button_Work"), TEXT("Work lights"), Body, R, SwWork);
	Mo(TEXT("Button_DoorFrontOut"), TEXT("IK_Button_DoorFrontOut"), K::Push, TEXT("Front doors"), Body, F, FA::DoorF, 0.f, 1.f);
	// the front platform's rails and gates, by hand
	Mo(TEXT("RailPlat_L"), TEXT("IK_Rail_Hand_L"), K::Hand, TEXT("Platform rails up / down"), RailL, No, FA::Rails, 0.f, 1.f);
	Mo(TEXT("RailPlat_R"), TEXT("IK_Rail_Hand_R"), K::Hand, TEXT("Platform rails up / down"), RailR, No, FA::Rails, 0.f, 1.f);
	Mo(TEXT("GateSlide_L"), TEXT("IK_Handle_Gate_L"), K::Hand, TEXT("Gates out / in"), GateL, No, FA::Gates, 0.f, 40.f);
	Mo(TEXT("GateSlide_R"), TEXT("IK_Handle_Gate_R"), K::Hand, TEXT("Gates out / in"), GateR, No, FA::Gates, 0.f, 40.f);
	// the tail lift: outside at the rear right, inside at the rear, its rails by hand
	Mo(TEXT("Button_TailUp"), TEXT("IK_Button_TailUp"), K::Hold, TEXT("Tail lift up"), Body, R, FA::Arms, AmLow, 0.f, false);
	Mo(TEXT("Button_TailDown"), TEXT("IK_Button_TailDown"), K::Hold, TEXT("Tail lift down"), Body, R, FA::Arms, 0.f, AmLow, false);
	Mo(TEXT("Button_TailFold"), TEXT("IK_Button_TailFold"), K::Hold, TEXT("Tail lift unfold / fold"), Body, R, FA::Tail, 0.f, 90.f);
	Sw(TEXT("Button_TailStop"), TEXT("IK_Button_TailStop"), TEXT("Emergency stop"), Body, R, SwStop);
	Mo(TEXT("Button_InTailUp"), TEXT("IK_Button_InTailUp"), K::Hold, TEXT("Tail lift up"), Body, F, FA::Arms, AmLow, 0.f, false);
	Mo(TEXT("Button_InTailDown"), TEXT("IK_Button_InTailDown"), K::Hold, TEXT("Tail lift down"), Body, F, FA::Arms, 0.f, AmLow, false);
	Mo(TEXT("Button_InTailFold"), TEXT("IK_Button_InTailFold"), K::Hold, TEXT("Tail lift unfold / fold"), Body, F, FA::Tail, 0.f, 90.f);
	Sw(TEXT("Button_InTailStop"), TEXT("IK_Button_InTailStop"), TEXT("Emergency stop"), Body, F, SwStop);
	Mo(TEXT("TailRail_L"), TEXT("IK_Tail_Rail_L_Hand"), K::Hand, TEXT("Tail lift rails up / down"), DRailL, No, FA::TRails, 0.f, 1.f);
	Mo(TEXT("TailRail_R"), TEXT("IK_Tail_Rail_R_Hand"), K::Hand, TEXT("Tail lift rails up / down"), DRailR, No, FA::TRails, 0.f, 1.f);
	Mo(TEXT("Button_DoorRearIn"), TEXT("IK_Button_DoorRearIn"), K::Push, TEXT("Rear doors"), Body, F, FA::DoorR, 0.f, 1.f);
	Mo(TEXT("Button_DoorRearOut"), TEXT("IK_Button_DoorRearOut"), K::Push, TEXT("Rear doors"), Body, B, FA::DoorR, 0.f, 1.f);
	// the cab
	Sw(TEXT("Button_CabHazard"), TEXT("IK_Button_CabHazard"), TEXT("Hazard lights"), Chassis, Dash, SwHaz, true);
	Sw(TEXT("Button_CabBlue"), TEXT("IK_Button_CabBlue"), TEXT("Blue lights"), Chassis, Dash, SwBlue, true);
	Sw(TEXT("Button_CabStop"), TEXT("IK_Button_CabStop"), TEXT("Emergency stop"), Chassis, Dash, SwStop, true);
	Sw(TEXT("Button_CabBeacon"), TEXT("IK_Button_CabBeacon"), TEXT("Beacon"), Chassis, Dash, SwBeacon, true);
	Sw(TEXT("Button_CabWork"), TEXT("IK_Button_CabWork"), TEXT("Work lights"), Chassis, Dash, SwWork, true);
	Sw(TEXT("Button_Horn"), TEXT("IK_Horn"), TEXT("Horn"), Chassis, Dash, SwHorn, true);

	Tb->Frame = [](const AGSE& G, const FTractor& T, int32 Fr) -> FTransform
	{
		const FSvcAmbu& A = *T.Am; const float* N = A.Now; const FTransform U = AmT(AmUp * N[FA::Lift]);
		if (Fr == Chassis) return FTransform::Identity;
		if (Fr == Body) return U;
		// (as AmbuParts places the parts themselves)
		const FTransform Pl = AmRot(AmHinge, AmY, N[FA::Plat] - 90.f) * U, Dk = AmRot(AmTailH, AmY, 90.f - N[FA::Tail]) * AmT(AmbuTip(A)) * U;
		if (Fr == Plat) return Pl;
		if (Fr == Deck) return Dk;
		if (Fr == Folded) return AmRot(AmTailH, AmY, -N[FA::Tail]) * AmT(AmbuTip(A)) * U;
		const int32 r = Fr == RailR || Fr == GateR || Fr == DRailR ? 1 : 0; const float Sg = r ? 1.f : -1.f; auto Ang = [r](float E) { return 90.f - 90.f * FMath::Clamp(2.f * E - float(r), 0.f, 1.f); };
		if (Fr == DRailL || Fr == DRailR) return AmRot(G.SvcP(T, r ? TEXT("PIV_TailRail_R") : TEXT("PIV_TailRail_L")), AmX, Ang(N[FA::TRails]) * Sg) * Dk;
		return AmT(AmX * (Fr == GateL || Fr == GateR ? N[FA::Gates] : 0.f)) * AmRot(G.SvcP(T, r ? TEXT("PIV_RailPlat_R") : TEXT("PIV_RailPlat_L")), AmX, Ang(N[FA::Rails]) * Sg) * Pl;
	};
	Tb->Now = [](const FTractor& T, int32 M) { return T.Am->Now[M]; };
	Tb->Want = [](FTractor& T, int32 M, float To) { T.Am->Want[M] = To; T.Am->bLive = true; };
	Tb->May = [](const AGSE& G, const FTractor& T, int32 M, float To, FString& Why) { if (G.AmbuMay(T, M, To)) return true; Why = G.AmbuWhy(T, M, To); return false; };
	Tb->End = [](const FTractor& T, int32 M, float E) { return M == FA::Lift && T.Am->Now[FA::Plat] > 0.f ? FMath::Max(E, 102.f) : E; };   // (with the platform out not down onto the cab)
	Tb->Press = [](AGSE& G, int32 Ti, const FVehControl& X) { if (FCString::Strncmp(X.Part, TEXT("Button_"), 7) == 0) { G.AmbuPress(Ti, X.Part + 7); if (G.Tractors[Ti].Am) G.Tractors[Ti].Am->bLive = true; } };
	Tb->Switch = [](AGSE& G, int32 Ti, int32 S)
	{
		FTractor& T = G.Tractors[Ti]; FSvcAmbu& A = *T.Am;
		switch (S)
		{
		case SwHaz: T.bSwHaz = !T.bSwHaz; break;
		case SwBlue: A.bBlue = !A.bBlue; break;
		case SwBeacon: T.bSwNoBeacon = !T.bSwNoBeacon; break;
		case SwWork: T.bSwWork = !T.bSwWork; break;
		case SwLight: T.bSwLight = !T.bSwLight; break;
		case SwHorn: G.Sound(TEXT("BTHorn"), G.TW(T, FVector(480.f, 0.f, 80.f))); break;   // (the electric tractor's)
		case SwStop: A.bStop = !A.bStop; if (A.bStop) for (int32 p = 0; p < FA::Parts; ++p) A.Want[p] = A.Now[p]; break;
		default: return false;
		}
		return true;
	};
	Tb->Aboard = [](const FTractor& T, const FVector& P) -> uint8
	{
		const FSvcAmbu& A = *T.Am; const float Up = 128.5f + A.Now[FA::Lift]; const FVector Tip = AmbuTip(A);
		if (P.X > -428.f && P.X < 258.f && FMath::Abs(P.Y) < 115.f && FMath::Abs(P.Z - Up) < 60.f) return 1;
		if (A.Now[FA::Tail] > 1.f && P.X > -575.f + Tip.X && P.X < -428.f + Tip.X && FMath::Abs(P.Y) < 80.f && P.Z > Up + Tip.Z - 12.f && P.Z < Up + Tip.Z + 60.f) return 2;
		if (P.X >= 258.f && P.X < 497.f && FMath::Abs(P.Y) < 80.f && P.Z > Up - 15.f && P.Z < Up + 60.f) return 3;
		return 0;
	};
	Tb->Stowed = [](const AGSE& G, const FTractor& T) { return G.AmbuStowed(T); };
	Tb->Stow = [](AGSE&, FTractor& T) { FSvcAmbu& A = *T.Am; A.bStop = A.bBlue = false; T.bSwHaz = T.bSwNoBeacon = T.bSwWork = T.bSwLight = false; for (float& V : A.Want) V = 0.f; A.bLive = true; };   // (the order by AmbuMay)
	// (no Worked: what counts for the turn-round is the passengers moved, its crew's job)

	auto Box = [&Tb](const FVector& Mn, const FVector& Mx, int32 Fr, TFunction<bool(const FTractor&)> On = nullptr) { FVehSolid S; S.Min = Mn; S.Max = Mx; S.Frame = Fr; S.On = MoveTemp(On); Tb->Solids.Add(S); };
	// chassis and cab; the body's floor, sides, ends beside the doors and roof edge; the doors shut
	Box(FVector(-400, -117, 4), FVector(500, 117, 95), Chassis); Box(FVector(259, -117, 95), FVector(500, 117, 214), Chassis);
	Box(FVector(-426, -117, 108), FVector(257, 117, 128.5), Body);
	for (const float S : {-1.f, 1.f})
	{
		Box(FVector(-398, S > 0 ? 113 : -120, 128.5), FVector(257, S > 0 ? 120 : -113, 345), Body);
		Box(FVector(250, S > 0 ? 56 : -120, 128.5), FVector(257, S > 0 ? 120 : -56, 345), Body); Box(FVector(-398, S > 0 ? 56 : -120, 128.5), FVector(-391, S > 0 ? 120 : -56, 345), Body);
		Box(FVector(259, S > 0 ? 64 : -72, 128.5), FVector(493, S > 0 ? 72 : -64, 240), Plat, [](const FTractor& T) { return T.Am->Now[FA::Plat] > 89.9f; });   // (the platform's sides: never off them, up there)
		Box(FVector(-566, S > 0 ? 70 : -76, 128.5), FVector(-430, S > 0 ? 76 : -70, 228), Deck, [](const FTractor& T) { return T.Am->Now[FA::TRails] > 0.99f; });
	}
	Box(FVector(250, -56, 128.5), FVector(257, 56, 340), Body, [](const FTractor& T) { return T.Am->Now[FA::DoorF] < 0.9f; });
	Box(FVector(-398, -56, 128.5), FVector(-391, 56, 340), Body, [](const FTractor& T) { return T.Am->Now[FA::DoorR] < 0.9f; });
	// the front platform (its nose shut to you too), folded or out; the tail deck, and the flap while it stands up
	Box(FVector(259, -72, 116), FVector(493, 72, 128.5), Plat); Box(FVector(487, -72, 128.5), FVector(493, 72, 240), Plat, [](const FTractor& T) { return T.Am->Now[FA::Plat] > 89.9f; });
	Box(FVector(-571, -76, 119), FVector(-428, 76, 128.5), Deck); Box(FVector(-575, -76, 128.5), FVector(-571, 76, 168), Deck, [](const FTractor& T) { return T.Am->Now[FA::Flap] > 45.f; });
	return Tb;
}
