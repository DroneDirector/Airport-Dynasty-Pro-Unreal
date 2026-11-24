// The ambulift's moving parts (to-do 212): they follow pose() in Art/Ambulift/ambulift_checks.py. Whoever works a part - its crew
// (GSEAmbuliftWork.cpp) or you - only sets FSvcAmbu::Want; AmbuParts moves it at its real speed and AmbuMay keeps the interlocks,
// so nobody can do what the vehicle does not allow. And its two wheelchairs.
#include "GSEAmbulift.h"
#include "GSE.h"
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
	if (T.Am) for (int32 p = 0; p < FA::Parts; ++p) if (FMath::Abs(T.Am->Now[p]) > 0.05f || FMath::Abs(T.Am->Want[p]) > 0.05f) return false;
	return true;
}

bool AGSE::AmbuMay(const FTractor& T, int32 Part, float To) const
{
	const FSvcAmbu& C = *T.Am; const float* N = C.Now; const bool bOut = FMath::Abs(To) > FMath::Abs(N[Part]);
	if (C.bStop) return false;   // (an emergency stop)
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
	if (T.Job != EJob::AtStand && AmbuStowed(T)) C.bLive = false;
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
