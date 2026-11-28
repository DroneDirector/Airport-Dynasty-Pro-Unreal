// The catering high-loader at work (to-do 212; the vehicle and its crew's part are the shared ones of GSEService.cpp): your
// driver and loader drive to the right side of the aircraft - down between the wingtips, never under a wing - and up to the
// front service door, then the rear one; legs down, up into the body, up with it, the front roll-up door, the platform out
// to the fuselage, its flap and rails; the full trolleys in and the empty ones out, one trip after the other; and all back.
// One stage after the other (FSvcJob::Stage), one of the two working at a time: whoever is left goes on from the stage.
// The doors: R1 and R4 as Tools/make_aircraft_doors.py measured them on Art/A320/A320.glb (Data/aircraft-doors.json): R1
// 0.14 m ahead of the nose wheel, sill 3.46 m, skin 1.94 m out, turned 3 deg forward; R4 24.49 m behind, sill 3.56 m, skin
// 1.70 m out and turned 9 deg toward the tail. The aircraft is one mesh: its doors do not open and there is no
// cabin (to-do 209) - a knock on it, and with their trolley they are out of sight at the sill, as the cleaners at theirs.
#include "GSECater.h"
#include "AircraftDoors.h"
#include "GSE.h"
#include "VehicleControls.h"
#include "GameFramework/PlayerController.h"
#include "ServiceVehicles.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	const float CtRearPatience = 1200.f;   // (20 min at the front door for the rear one to come free)
	const FVector CtUp(0, 0, 1);
	const float CtFloor = 150.f, CtDeck = 136.5f, CtSlide = 295.f, CtReach = 531.3f + 3.f, CtCabinSeconds = 14.f, CtLane = -20.43f;
	// R1 and R4: measured on the model (Data/aircraft-doors.json, Tools/make_aircraft_doors.py); these if the file is missing
	struct FCtDoors
	{
		FVector2D Skin[2] = {FVector2D(196.0, -14.0), FVector2D(178.0, 2449.0)};   // (right of the centre line, behind the nose wheel)
		float Sill[2] = {346.f, 356.f}, Turn[2] = {0.f, 11.f};
		FCtDoors() { const TCHAR* Nm[2] = {TEXT("R1"), TEXT("R4")}; for (int32 d = 0; d < 2; ++d) if (const ADPDoors::FDoor* Dr = ADPDoors::Find(TEXT("A320"), Nm[d])) { Skin[d] = Dr->Skin; Sill[d] = Dr->Sill; Turn[d] = Dr->Turn; } }
	};
	const FCtDoors& CtDoors() { static const FCtDoors Ds; return Ds; }
	const FVector2D CtW[2] = {FVector2D(171.0, -80.0), FVector2D(165.0, 75.0)};        // (where the two wait in the body)
	// the trolleys' places (Art/CateringTruck/cateringtruck-parts.json stock)
	struct FCtPlace { float X, Y; };
	const FCtPlace CtStock[18] = {{-358.7f, -99.5f}, {-269.7f, -99.5f}, {-180.7f, -99.5f}, {-91.7f, -99.5f}, {-2.7f, -99.5f}, {-358.7f, -68.f}, {-269.7f, -68.f}, {-180.7f, -68.f}, {-91.7f, -68.f}, {-2.7f, -68.f},
		{-205.7f, 99.5f}, {-116.7f, 99.5f}, {-27.7f, 99.5f}, {61.3f, 99.5f}, {-205.7f, 68.f}, {-116.7f, 68.f}, {-27.7f, 68.f}, {61.3f, 68.f}};
	// a trip: which full one goes in, which place an empty one comes back to (an outer place while the inner one before it is empty)
	struct FCtTrip { int32 In, Out; };
	const TArray<FCtTrip>& CtTrips(int32 Door)
	{
		static const TArray<FCtTrip> A = {{17, -1}, {13, 13}, {-1, 17}, {9, -1}, {4, 4}, {-1, 9}};
		static const TArray<FCtTrip> B = {{16, -1}, {12, 12}, {-1, 16}, {8, -1}, {3, 3}, {-1, 8}, {15, -1}, {11, 11}, {-1, 15}, {7, 7}};
		return Door ? B : A;
	}
	// what is left of a door's trips when you brought Done of its trolleys (two at a time, the inner one first)
	TArray<FCtTrip> CtTripsLeft(int32 Door, int32 Done)
	{
		if (Done <= 0) return CtTrips(Door);
		static const int32 In[2][7] = {{17, 13, 9, 4, 0, 0, 0}, {16, 12, 8, 3, 15, 11, 7}};
		const int32 n = FMath::Max(0, (Door ? 7 : 4) - Done); TArray<FCtTrip> R;
		for (int32 k = 0; k + 1 < n; k += 2) { R.Add({In[Door][k], -1}); R.Add({In[Door][k + 1], In[Door][k + 1]}); R.Add({-1, In[Door][k]}); }
		if (n % 2) R.Add({In[Door][n - 1], In[Door][n - 1]});
		return R;
	}
	enum : int32 { CtMLegs, CtMLift, CtMPlat, CtMFlap, CtMRail0, CtMRail1, CtMGate, CtMRollF, CtMRollR, CtMFold, CtMTail, CtMDoor };   // (the motions)
	FVector CtTailTip(float Tail) { const float A = FMath::DegreesToRadians(47.17f - 100.83f * Tail); return FVector(-369.7f - 90.f * FMath::Cos(A), 0.f, 80.f + 90.f * FMath::Sin(A)); }
	float CtFloorZ(float X) { return X < 239.f ? CtFloor : FMath::Lerp(CtFloor, CtDeck, FMath::Clamp((X - 239.f) / 46.f, 0.f, 1.f)); }
	float CtLen(const TArray<FVector2D>& P) { double L = 0.0; for (int32 i = 1; i < P.Num(); ++i) L += FVector2D::Distance(P[i - 1], P[i]); return float(L); }
	FVector2D CtAt(const TArray<FVector2D>& P, float S)
	{
		for (int32 i = 1; i < P.Num(); ++i) { const float D = float(FVector2D::Distance(P[i - 1], P[i])); if (S <= D || i == P.Num() - 1) return FMath::Lerp(P[i - 1], P[i], D > 0.01f ? FMath::Clamp(S / D, 0.f, 1.f) : 1.f); S -= D; }
		return P.Num() ? P[0] : FVector2D::ZeroVector;
	}
	FVector CtDirW(const AGSE::FTractor& T, const FVector2D& D) { return FRotator(0.f, T.Yaw, 0.f).RotateVector(FVector(D.X, D.Y, 0.f)).GetSafeNormal2D(); }
	float CtQ(const AGSE::FTractor& T) { return T.Ct ? T.Ct->Lift : 0.f; }
	FVector CtPt(const AGSE::FTractor& T, float X, float Y, float Z) { return T.At + FRotator(0.f, T.Yaw, 0.f).RotateVector(FVector(X, Y, Z)); }
	FVector CtOn(const AGSE::FTractor& T, const FVector2D& P) { return CtPt(T, float(P.X), float(P.Y), CtFloorZ(float(P.X)) + CtQ(T)); }
	// walking on the body's floor or the platform: one foot stands, the other swings past it
	FDriverPose CtWalk(const AGSE::FTractor& T, const TArray<FVector2D>& P, float S, const FVector& Face, float PH)
	{
		const float L = CtLen(P), St = 46.f; const int32 n = FMath::FloorToInt(S / St); const float F = S / St - n, Sm = FMath::SmoothStep(0.1f, 0.9f, F);
		FDriverPose Q; Q.Facing = Face; Q.Lean = 4.f; const FVector Lt = ADPPose::LeftOf(Face); float Lo = 1e9f;
		for (int32 k = 0; k < 2; ++k)
		{
			const bool bStand = ((n & 1) == 0) == (k == 0); const float Fs = (n + 0.5f) * St;
			Q.Foot[k] = CtOn(T, CtAt(P, FMath::Clamp(bStand ? Fs : FMath::Lerp(Fs - St, Fs + St, Sm), 0.f, L))) + Lt * (k == 0 ? 10.f : -10.f);
			Lo = FMath::Min(Lo, float(Q.Foot[k].Z)); if (!bStand && S < L - 2.f) Q.Foot[k].Z += 6.f * FMath::Sin(PI * Sm);
		}
		Q.Pelvis = CtOn(T, CtAt(P, S)); Q.Pelvis.Z = Lo + PH - 3.f * FMath::Sin(PI * F);
		return Q;
	}
	FVector2D CtTangent(const TArray<FVector2D>& P, float S) { const float L = CtLen(P); FVector2D D = CtAt(P, FMath::Min(S + 18.f, L)) - CtAt(P, FMath::Max(0.f, FMath::Min(S, L - 4.f) - 8.f)); D.Normalize(); return D.IsNearlyZero() ? FVector2D(1.0, 0.0) : D; }
	// both hands on a trolley's handle (its end at the back of it as it is pushed; Dir: which way, +1 to the aircraft)
	void CtHands(const AGSE::FTractor& T, FDriverPose& Q, float Dir)
	{
		const FVector C = T.Ct->MovAt; const float X = float(C.X) - Dir * 43.3f, Z = float(C.Z) + 99.f;
		Q.bHand[0] = Q.bHand[1] = true; Q.Grip[0] = Q.Grip[1] = 0.95f;
		Q.Hand[0] = CtPt(T, X, float(C.Y) - Dir * 7.f, Z); Q.Hand[1] = CtPt(T, X, float(C.Y) + Dir * 7.f, Z); Q.Lean = 8.f;
	}
	// its own chocks and cones all in their trays
	bool CtGearIn(const FSvcCater& C) { return C.Chock[0] == FSvcCater::InTray && C.Chock[1] == FSvcCater::InTray && C.Cone[0] == FSvcCater::InTray && C.Cone[1] == FSvcCater::InTray; }
	const TCHAR* CtGearHeld(const FSvcCater& C)
	{
		return C.Chock[0] == FSvcCater::InYours || C.Chock[1] == FSvcCater::InYours ? TEXT("a wheel chock") : C.Cone[0] == FSvcCater::InYours || C.Cone[1] == FSvcCater::InYours ? TEXT("a warning cone") : nullptr;
	}
}

// ------------------------------------------------------------------ the parts

void AGSE::CaterDress(FTractor& T)
{
	ABabylonModel* M = T.M.Get(); if (!M) return;
	T.Ct = MakeShared<FSvcCater>(); FSvcCater& C = *T.Ct;
	auto Pre = [&](const TCHAR* N, const FVector& P) { return M->BindGroup({FString(N)}, true, FTransform(P)); };
	C.GBody = M->BindGroupWhere([](const FString& N, int32, const FVector&)
	{
		for (const TCHAR* X : {TEXT("BoxStep"), TEXT("BoxDoorL"), TEXT("TailBrake"), TEXT("RollF_"), TEXT("RollR_"), TEXT("TailArms"), TEXT("TailPlatform")}) if (N.StartsWith(X)) return false;
		for (const TCHAR* X : {TEXT("Box"), TEXT("Roll"), TEXT("Tail"), TEXT("NamePanel_L"), TEXT("NamePanel_R"), TEXT("WorkLamp")}) if (N.StartsWith(X) && !N.StartsWith(TEXT("NamePanel_Rear"))) return true;
		return false;
	}, FTransform::Identity);
	for (int32 i = 0; i < 12; ++i) C.GRoll.Add(Pre(*FString::Printf(TEXT("RollF_%02d"), i), FVector::ZeroVector));
	for (int32 i = 0; i < 12; ++i) C.GRollR.Add(Pre(*FString::Printf(TEXT("RollR_%02d"), i), FVector::ZeroVector));
	C.GTailArm = Pre(TEXT("TailArms"), SvcP(T, TEXT("PIV_TailArms"))); C.GTailPl = Pre(TEXT("TailPlatform"), SvcP(T, TEXT("PIV_TailPlatform")));
	C.GPlat = Pre(TEXT("Platform"), FVector::ZeroVector); C.GFlap = Pre(TEXT("RampFlap"), SvcP(T, TEXT("PIV_RampFlap")));
	C.GRail[0] = Pre(TEXT("RailPlatform_L"), SvcP(T, TEXT("PIV_RailPlatform_L"))); C.GRail[1] = Pre(TEXT("RailPlatform_R"), SvcP(T, TEXT("PIV_RailPlatform_R")));
	C.GGate = Pre(TEXT("GatePlatform"), SvcP(T, TEXT("PIV_RailPlatform_R")));   // (it lies in the right rail and goes with it; across the end only when you shut it)
	C.GSc[0] = Pre(TEXT("ScissorA"), SvcP(T, TEXT("PIV_ScissorMid"))); C.GSc[1] = Pre(TEXT("ScissorB"), SvcP(T, TEXT("PIV_ScissorMid")));
	for (int32 r = 0; r < 2; ++r)
	{
		const FString S = r ? TEXT("R") : TEXT("L");
		C.GRamB[r] = Pre(*(TEXT("RamBarrel_") + S), SvcP(T, *(TEXT("PIV_Ram_") + S))); C.GRamR[r] = Pre(*(TEXT("RamRod_") + S), SvcP(T, *(TEXT("PIV_RamEye_") + S)));
	}
	static const TCHAR* W[] = {TEXT("FL"), TEXT("FR"), TEXT("RL"), TEXT("RR")};
	for (int32 i = 0; i < 4; ++i) C.GLeg[i] = Pre(*(FString(TEXT("StabLeg_")) + W[i]), FVector::ZeroVector);
	C.GLev[0] = Pre(TEXT("Lever_Stab"), SvcP(T, TEXT("PIV_Lever_Stab"))); C.GLev[1] = Pre(TEXT("Lever_Lift"), SvcP(T, TEXT("PIV_Lever_Lift"))); C.GLev[2] = Pre(TEXT("Lever_Plat"), SvcP(T, TEXT("PIV_Lever_Plat")));
	// its own chocks and cones, each about its base
	C.GChock[0] = Pre(TEXT("Chock_0"), SvcP(T, TEXT("PIV_Chock_0"))); C.GChock[1] = Pre(TEXT("Chock_1"), SvcP(T, TEXT("PIV_Chock_1")));
	C.GCone[0] = Pre(TEXT("Cone_0"), SvcP(T, TEXT("PIV_Cone_0"))); C.GCone[1] = Pre(TEXT("Cone_1"), SvcP(T, TEXT("PIV_Cone_1")));
	if (C.GChock[0] == INDEX_NONE || C.GCone[0] == INDEX_NONE) ADP_WARN(TEXT("GSE: the %s model has no own chocks or cones (Chock_0 %d, Cone_0 %d)"), *T.Name, C.GChock[0], C.GCone[0]);
	// the trolleys and boxes: the kit's own meshes, one instance list each, on the truck
	ABabylonModel* Kit = GetWorld()->SpawnActor<ABabylonModel>(T.At - CtUp * 400.f, FRotator::ZeroRotator);
	if (!Kit || !Kit->Place(TEXT("CateringKit"))) { ADP_WARN(TEXT("GSE: no CateringKit model - the catering truck has no trolleys")); if (Kit) Kit->Destroy(); return; }
	auto List = [&](const TCHAR* Name) -> UInstancedStaticMeshComponent*
	{
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> G; Kit->InstancesNamed(Name, false, G); if (!G.Num()) return nullptr;
		UInstancedStaticMeshComponent* S = G[0].Get<0>(); UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(M);
		K->SetMobility(EComponentMobility::Movable); K->SetupAttachment(M->GetRootComponent()); K->SetStaticMesh(S->GetStaticMesh());
		for (int32 i = 0; i < S->GetNumMaterials(); ++i) K->SetMaterial(i, S->GetMaterial(i));
		K->SetCollisionEnabled(ECollisionEnabled::NoCollision); K->SetCanEverAffectNavigation(false); K->RegisterComponent(); M->AddInstanceComponent(K); C.Kits.Add(K);
		return K;
	};
	UInstancedStaticMeshComponent *Tr = List(TEXT("Trolley")), *Dr = List(TEXT("TrolleyDoor")), *Rd = List(TEXT("TrolleyPedalRed")), *Gn = List(TEXT("TrolleyPedalGreen")), *Cs = List(TEXT("TrolleyCastor")), *Bx = List(TEXT("GalleyBox")), *Bd = List(TEXT("GalleyBoxDoor"));
	const FRotator Half(0.f, 180.f, 0.f);
	for (const FCtPlace& Pl : CtStock)
	{
		FSvcCater::FTrolley& X = C.Trolleys.AddDefaulted_GetRef(); X.Home = X.At = FVector(Pl.X, Pl.Y, CtFloor);
		const FTransform At(FRotator(0.f, 90.f, 0.f), X.Home);
		auto Add = [&](UInstancedStaticMeshComponent* K, const FTransform& Rel, uint8 Pedal = 0) { if (K) { FSvcCater::FBit B; B.K = K; B.Rel = Rel; B.Pedal = Pedal; B.Ix = K->AddInstance(Rel * At, false); X.Bits.Add(B); } };
		Add(Tr, FTransform::Identity);
		Add(Dr, FTransform(FVector(-14.5f, -40.9f, 14.f))); Add(Dr, FTransform(Half, FVector(14.5f, 40.9f, 14.f)));
		Add(Rd, FTransform(FVector(-3.4f, -38.5f, 9.9f)), 1); Add(Gn, FTransform(FVector(3.4f, -38.5f, 9.9f)), 2);
		Add(Rd, FTransform(Half, FVector(3.4f, 38.5f, 9.9f)), 1); Add(Gn, FTransform(Half, FVector(-3.4f, 38.5f, 9.9f)), 2);
		for (const float a : {-11.5f, 11.5f}) for (const float b : {-33.5f, 33.5f}) Add(Cs, FTransform(FVector(a, b, 10.5f)));
	}
	for (const float z : {163.2f, 215.2f, 267.2f}) for (const float x : {-368.7f, -340.2f, -311.7f, -283.2f})
	{ if (Bx) Bx->AddInstance(FTransform(FVector(x, 94.5f, z)), false); if (Bd) Bd->AddInstance(FTransform(FVector(x, 94.5f - 19.85f, z + 27.1f)), false); }
	Kit->Destroy();
}

void AGSE::CaterParts(FTractor& T, float Dt)
{
	ABabylonModel* M = T.M.Get(); if (!M || !T.Ct || !T.Ct->bLive) return;
	FSvcCater& C = *T.Ct; if (T.Job == EJob::None) YouCater(T, Dt);
	auto Put = [M](int32 G, const FTransform& X) { if (G != INDEX_NONE) M->MoveGroup(G, X); };
	auto Run = [Dt](float& V, float Want, float Rate) { const float Was = V; V = FMath::FInterpConstantTo(V, Want, Dt, Rate); return V - Was; };
	// (never the platform out below 180 cm, never down with it out)
	const float Lw = C.Out > 1.f ? FMath::Max(C.LiftWant, 180.f) : C.LiftWant;
	const bool bNoLift = C.Lift != Lw && CaterHeld(T, CtMLift, Lw), bNoOut = C.Out != C.OutWant && CaterHeld(T, CtMPlat, C.OutWant), bNoLegs = C.Legs != C.LegsWant && CaterHeld(T, CtMLegs, C.LegsWant);
	const float Dq = bNoLift ? 0.f : Run(C.Lift, Lw, 11.f), Dp = bNoOut ? 0.f : Run(C.Out, C.Lift >= 180.f || C.OutWant < C.Out ? C.OutWant : C.Out, 20.f), Dl = bNoLegs ? 0.f : Run(C.Legs, C.LegsWant, 6.f);
	if (C.RollRWant > C.RollR || (C.RollR != C.RollRWant && !CaterHeld(T, CtMRollR, C.RollRWant))) Run(C.RollR, C.RollRWant, 80.f);
	if (C.Gate != C.GateWant) Run(C.Gate, C.GateWant, 0.6f);
	if (C.Fold != C.FoldWant && !CaterHeld(T, CtMFold, C.FoldWant)) Run(C.Fold, C.FoldWant, 0.35f);
	if (C.Tail != C.TailWant && !CaterHeld(T, CtMTail, C.TailWant)) Run(C.Tail, C.TailWant, 1.f / 14.f);
	if (C.RollWant > C.Roll || (C.Roll != C.RollWant && !CaterHeld(T, CtMRollF, C.RollWant))) Run(C.Roll, C.RollWant, 80.f); Run(C.Flap, C.FlapWant, 0.5f); Run(C.Rail[0], C.RailWant[0], 0.6f); Run(C.Rail[1], C.RailWant[1], 0.6f);
	const float Dv[3] = {Dl, Dq, Dp};
	for (int32 i = 0; i < 3; ++i)
	{
		Run(C.Lev[i], Dv[i] > 0.001f ? 1.f : Dv[i] < -0.001f ? -1.f : 0.f, 5.f);
		const FVector P = SvcP(T, i == 0 ? TEXT("PIV_Lever_Stab") : i == 1 ? TEXT("PIV_Lever_Lift") : TEXT("PIV_Lever_Plat"));
		Put(C.GLev[i], FTransform(FQuat(i == 0 ? FVector::ForwardVector : FVector::RightVector, FMath::DegreesToRadians(15.f * C.Lev[i])), i == 0 ? P : P + CtUp * C.Lift));
	}
	const FVector Q = CtUp * C.Lift, Sl = Q + FVector(C.Out, 0.f, 0.f);
	Put(C.GBody, FTransform(Q)); Put(C.GPlat, FTransform(Sl));
	if (T.SvcDoors.Num() > 1 && T.SvcDoors[1].G != INDEX_NONE && (C.Lift > 0.f || Dq != 0.f)) Put(T.SvcDoors[1].G, FTransform(T.SvcDoors[1].Piv + Q));
	for (int32 i = 0; i < C.GRoll.Num(); ++i) { const bool bIn = CtFloor + 15.8f * (i + 0.5f) + C.Roll > 347.f; Put(C.GRoll[i], FTransform(FQuat::Identity, bIn ? FVector(226.f, 0.f, 350.f) + Q : Q + CtUp * C.Roll, bIn ? FVector(0.001f) : FVector::OneVector)); }
	Put(C.GFlap, FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(106.f * FMath::SmoothStep(0.f, 1.f, C.Flap))), SvcP(T, TEXT("PIV_RampFlap")) + Q));
	for (int32 r = 0; r < 2; ++r)
	{
		const FTransform X(FQuat(FVector::ForwardVector, FMath::DegreesToRadians((r ? -90.f : 90.f) * FMath::SmoothStep(0.f, 1.f, C.Rail[r]))), SvcP(T, r ? TEXT("PIV_RailPlatform_R") : TEXT("PIV_RailPlatform_L")) + Sl);
		Put(C.GRail[r], X); if (!r) continue;
		const FVector Pr = SvcP(T, TEXT("PIV_RailPlatform_R")), Pg = SvcP(T, TEXT("PIV_GatePlatform"));   // (lying it goes with its rail; standing it turns about its own hinge, given where that stands)
		Put(C.GGate, C.Gate > 0.f ? FTransform(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-90.f)), Pr - Pg) * FTransform(FQuat(CtUp, FMath::DegreesToRadians(90.f * FMath::SmoothStep(0.f, 1.f, C.Gate))), Pg + Sl) : X);
	}
	for (int32 i = 0; i < C.GRollR.Num(); ++i) { const bool bIn = CtFloor + 15.8f * (i + 0.5f) + C.RollR > 347.f; Put(C.GRollR[i], FTransform(FQuat::Identity, bIn ? FVector(-413.7f, 0.f, 350.f) + Q : Q + CtUp * C.RollR, bIn ? FVector(0.001f) : FVector::OneVector)); }
	Put(C.GTailArm, FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(-100.83f * C.Tail)), SvcP(T, TEXT("PIV_TailArms")) + Q));
	Put(C.GTailPl, FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(-90.f * FMath::SmoothStep(0.f, 1.f, C.Fold))), SvcP(T, TEXT("PIV_TailPlatform")) + CtTailTip(C.Tail) - CtTailTip(0.f) + Q));
	const float An = FMath::Asin((12.f + C.Lift) / 440.f), Tn = An - FMath::DegreesToRadians(1.563f); const FVector Mid(136.3f - 220.f * FMath::Cos(An), 0.f, 110.f + (12.f + C.Lift) * 0.5f);
	Put(C.GSc[0], FTransform(FQuat(FVector::RightVector, Tn), Mid)); Put(C.GSc[1], FTransform(FQuat(FVector::RightVector, -Tn), Mid));
	for (int32 r = 0; r < 2; ++r)
	{
		const FVector B = SvcP(T, r ? TEXT("PIV_Ram_R") : TEXT("PIV_Ram_L")), E0 = SvcP(T, r ? TEXT("PIV_RamEye_R") : TEXT("PIV_RamEye_L")), E(136.3f - 260.f * FMath::Cos(An), E0.Y, 110.f + 260.f * FMath::Sin(An) - 1.2f);
		const FQuat R(FVector::RightVector, -float(FMath::Atan2(E.Z - B.Z, E.X - B.X) - FMath::Atan2(E0.Z - B.Z, E0.X - B.X)));
		Put(C.GRamB[r], FTransform(R, B)); Put(C.GRamR[r], FTransform(R, E));
	}
	for (int32 i = 0; i < 4; ++i) Put(C.GLeg[i], FTransform(-CtUp * C.Legs));
	if (Dq != 0.f)
	{
		for (UActorComponent* K : M->GetComponents()) if (K && K->ComponentHasTag(TEXT("SvcBody"))) if (USceneComponent* S = Cast<USceneComponent>(K)) { FVector L = S->GetRelativeLocation(); L.Z = SvcP(T, TEXT("IK_Name_L")).Z + C.Lift; S->SetRelativeLocation(L); }
		for (UInstancedStaticMeshComponent* K : C.Kits) if (K) K->SetRelativeLocation(Q);
	}
	// the trolley on its way
	if (C.Trolleys.IsValidIndex(C.Mov))
	{
		const FTransform At(FQuat(CtUp, FMath::DegreesToRadians(90.f)), C.MovAt - Q, C.bMovShow ? FVector::OneVector : FVector(0.001f));
		for (const FSvcCater::FBit& B : C.Trolleys[C.Mov].Bits)
		{
			FTransform R = B.Rel; if (B.Pedal && B.Pedal == C.Pedal) R = FTransform(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(14.f))) * R;
			B.K->UpdateInstanceTransform(B.Ix, R * At, false, true, true);
		}
	}
	// its own chocks and cones: in their trays, in a crew's hand (by their grip), in yours, or set out on the ground
	{
		const ATugs* GearG = Tg(); const USkeletalMeshComponent* GearB = C.GearCi != INDEX_NONE ? Body(C.GearCi) : nullptr;
		const bool bGearBody = GearB && GearG && GearG->Crew.IsValidIndex(C.GearCi) && GearG->Crew[C.GearCi].A.IsValid() && !GearG->Crew[C.GearCi].A->IsHidden();
		const FTransform GearRoot(FRotator(0.f, T.Yaw, 0.f), T.At);
		const float GearFace = bGearBody ? float(GearG->Crew[C.GearCi].A->GetActorRotation().Yaw) + 90.f - T.Yaw : 0.f;
		auto GearAt = [&](uint8 S, const TCHAR* Ik, const TCHAR* Pv, const TCHAR* Place) -> FTransform
		{
			const FVector Base = SvcP(T, Pv), Grip = SvcP(T, Ik) - Base;   // (the grip stands straight over the base)
			if (bGearBody && (S == FSvcCater::InRight || S == FSvcCater::InLeft))
				return FTransform(FRotator(0.f, GearFace, 0.f), GearRoot.InverseTransformPosition(GearB->GetBoneLocation(S == FSvcCater::InRight ? TEXT("hand_r") : TEXT("hand_l"))) - Grip);
			if (S == FSvcCater::InYours) return FTransform(T.YouHand - Grip);
			return FTransform(S == FSvcCater::AtPlace ? SvcP(T, Place) : Base);
		};
		Put(C.GChock[0], GearAt(C.Chock[0], TEXT("IK_Chock_0"), TEXT("PIV_Chock_0"), TEXT("IK_ChockPlace_0"))); Put(C.GChock[1], GearAt(C.Chock[1], TEXT("IK_Chock_1"), TEXT("PIV_Chock_1"), TEXT("IK_ChockPlace_1")));
		Put(C.GCone[0], GearAt(C.Cone[0], TEXT("IK_Cone_0"), TEXT("PIV_Cone_0"), TEXT("IK_ConePlace_0"))); Put(C.GCone[1], GearAt(C.Cone[1], TEXT("IK_Cone_1"), TEXT("PIV_Cone_1"), TEXT("IK_ConePlace_1")));
	}
	// (all in its place: nothing more to do until the next job)
	const int32 Me = int32(&T - Tractors.GetData()); bool bJob = false; for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Phase > 0 && P.Value.Phase < 9 && P.Value.Ti == Me) bJob = true;
	if (!bJob && C.Lift == 0.f && C.Out == 0.f && C.Legs == 0.f && C.Roll == 0.f && C.Flap == 0.f && C.Rail[0] == 0.f && C.Rail[1] == 0.f && CaterStowed(T) && !T.YouOn && C.PedalT <= 0.f && C.Lev[0] == 0.f && C.Lev[1] == 0.f && C.Lev[2] == 0.f && CtGearIn(C) && T.bPrev) C.bLive = false;
}

void AGSE::CaterRestock(FTractor& T)
{
	if (!T.Ct) return;
	T.Ct->bLive = true; T.Ct->Mov = T.Ct->YouMov = INDEX_NONE;
	for (FSvcCater::FTrolley& X : T.Ct->Trolleys) { X.State = FSvcCater::AtHome; X.At = X.Home; X.bEmpty = X.bOwed = false; }
	for (const FSvcCater::FTrolley& X : T.Ct->Trolleys) for (const FSvcCater::FBit& B : X.Bits) B.K->UpdateInstanceTransform(B.Ix, B.Rel * FTransform(FRotator(0.f, 90.f, 0.f), X.Home), false, true, true);
}

// ------------------------------------------------------------------ the way to a door, and who else is there

void AGSE::CaterDock(const FSvcJob& J, int32 Door, FVector2f& At, FVector2f& Hd, float& SillZ) const
{
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J.StandX, S, N, Sp);
	const FVector Nose = PlanT(S, S, N); const AActor* Pl = nullptr; double Bd = 900.0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		for (const FName& Tg2 : It->Tags)
		{
			const FString Ts = Tg2.ToString(); if (!Ts.StartsWith(TEXT("SIM/")) && !Ts.StartsWith(TEXT("MOVE/"))) continue;
			const double D = FVector::Dist2D(It->GetActorLocation(), Nose); if (D < Bd) { Bd = D; Pl = *It; }
		}
	// (the aircraft's root at its nose wheel: x its right, y aft - where it really stands)
	const float A = FMath::DegreesToRadians(CtDoors().Turn[Door]); const FVector2D O(FMath::Cos(A), FMath::Sin(A)), K = CtDoors().Skin[Door] + O * (Tractors.IsValidIndex(J.Ti) && Tractors[J.Ti].Am ? 499.0 : double(CtReach));   // (the ambulift's platform ends 4.96 m ahead of its middle)
	auto World = [&](const FVector2D& L, float Z) { return Pl ? Pl->GetActorTransform().TransformPosition(FVector(L.X, L.Y, Z)) : FVector(PlanT(S, S - float(L.X) / 100.f, N + float(L.Y) / 100.f) + CtUp * Z); };
	const FVector Wo = World(K, 0.f), Ws = World(CtDoors().Skin[Door], CtDoors().Sill[Door]);
	At = FVector2f(float(Wo.X) / 100.f, float(-Wo.Y) / 100.f); const FVector D = Ws - Wo; Hd = FVector2f(float(D.X), float(-D.Y)).GetSafeNormal(); SillZ = float(Ws.Z);
}

void AGSE::CaterIn(const FSvcJob& J, int32 Door, float FromY, TArray<FVector>& W) const
{
	// down the middle between the two wingtips, one bend in front of the wing (R1) or behind it (R2), straight up to the door
	float S = 0.f, N = 0.f, Sp = 36.f, Sz = 0.f; StandGeo(J.StandX, S, N, Sp); FVector2f At, Hd; CaterDock(J, Door, At, Hd, Sz);
	{ const FAirfieldData::FStandFrame Fr = StandFrame(S); At = Fr.Turn(At); Hd = Fr.Way(Hd.X, Hd.Y); }   // (terminal-way, as FromY)
	const float R = Tractors[J.Ti].KTurnR, X = S + CtLane; const FVector2f Pre = At - Hd * (Door ? 7.f : FMath::Max(1.f, (At.X - X) - R / 100.f - 0.02f));
	const float Ys = Pre.Y + R / 100.f + (Door ? 2.9f : 0.f);
	if (FromY > Ys + 0.5f) LegT(S, W, X, FromY, 0.f, -1.f, X, Ys, 0.f, -1.f, R);
	LegT(S, W, X, FMath::Min(Ys, FromY), 0.f, -1.f, Pre.X, Pre.Y, Hd.X, Hd.Y, R * 0.98f); LegT(S, W, Pre.X, Pre.Y, Hd.X, Hd.Y, At.X, At.Y, Hd.X, Hd.Y, R);
}

bool AGSE::SvcSideBusy(int32 Ti, float StandX) const
{
	// the lane on the aircraft's right has room for one: another service vehicle on its way in or out, a train or the belt loader moving there
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(StandX, S, N, Sp); const FVector Mid = PlanT(S, S - 15.f, N + 25.f);
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		const FTractor& O = Tractors[i]; if (i == Ti || !O.M.IsValid() || FMath::Abs(O.StandX - StandX) > 1.f) continue;
		if (O.Svc != INDEX_NONE)
		{
			if (ADPService::All()[O.Svc].Lat >= 0.f || ADPService::All()[O.Svc].Wet) continue;   // (the water and the toilet truck come from behind)
			if (O.Job == EJob::Out) return true;
			for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Ti == i && P.Value.Phase == 3 && O.Job == EJob::AtStand && O.Path.Num()) return true;
		}
		else if (O.Path.Num() && O.Job == EJob::Back && FVector::Dist2D(O.At, Mid) < 3000.f) return true;
	}
	return false;
}

bool AGSE::CaterMoving(float StandX) const
{
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(StandX, S, N, Sp); const FVector Mid = PlanT(S, S - 15.f, N + 22.f);
	for (const FTractor& O : Tractors) if (O.Svc != INDEX_NONE && O.Ct && FMath::Abs(O.StandX - StandX) < 1.f && O.Path.Num() && FVector::Dist2D(O.At, Mid) < 4200.f) return true;
	return false;
}

bool AGSE::CaterRear(float StandX) const
{
	// at the rear door (or on its way between the two): where a train stands and the belt loader goes
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		const FSvcJob& J = P.Value; if (FMath::Abs(J.StandX - StandX) > 1.f || J.CtDoor != 1 || !Tractors.IsValidIndex(J.Ti) || !Tractors[J.Ti].Ct) continue;
		if (J.Phase == 2 || (J.Phase == 3 && Tractors[J.Ti].Job == EJob::AtStand)) return true;
	}
	return false;
}

bool AGSE::CaterRearFree(float StandX) const
{
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(StandX, S, N, Sp); const FVector Mid = PlanT(S, S - 10.f, N + 30.f);
	auto In = [&](const FVector& W) { const FVector2f Q = StandFrame(S).Turn(FVector2f(float(W.X) / 100.f, float(-W.Y) / 100.f)); const float X = Q.X, Y = Q.Y; return X > S - 22.f && X < S - 2.f && Y > N + 21.f && Y < N + 50.f; };
	for (const FTractor& O : Tractors)
	{
		if (O.Svc != INDEX_NONE || !O.M.IsValid() || O.Job == EJob::None) continue;
		if (In(O.At)) return false;
		for (const int32 c : O.Train) if (Carts.IsValidIndex(c) && In(Carts[c].At)) return false;
		if (FMath::Abs(O.StandX - StandX) < 1.f && O.Path.Num() && (O.Job == EJob::Out || FVector::Dist2D(O.At, Mid) < 6000.f)) return false;   // (on its way out: from however far - it goes first, or it would stand in the truck's way out)
	}
	return true;
}

// ------------------------------------------------------------------ the work

void AGSE::CaterHold(int32 Ci, int32 Ti, FVector2D At, FVector2D Face)
{
	if (ATugs* G = Tg()) G->Crew[Ci].Hold = [this, Ti, At, Face](float, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Standing(CtOn(T, At), CtDirW(T, Face), PH); };
}

void AGSE::CaterWalk(int32 Ci, int32 Ti, TArray<FVector2D> Pts, FVector2D EndFace)
{
	const float L = CtLen(Pts); if (L < 1.f) return;
	const FVector2D End = Pts.Last();
	PoseAct(Ci, [this, Ti, Pts, L, EndFace](float K, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const float S = K * L; const FVector F = FMath::Lerp(CtDirW(T, CtTangent(Pts, S)), CtDirW(T, EndFace), FMath::SmoothStep(0.75f, 1.f, K)).GetSafeNormal2D();
		P = CtWalk(T, Pts, S, F.IsNearlyZero() ? CtDirW(T, EndFace) : F, PH);
	}, FMath::Max(0.6f, L / 95.f), true);
	Do(Ci, [this, Ci, Ti, End, EndFace] { CaterHold(Ci, Ti, End, EndFace); });
}

void AGSE::CaterHand(int32 Ci, int32 Ti, FVector2D At, int32 Side, TFunction<FVector()> To, float Dur, TFunction<void()> Fn, bool bBothHands)
{
	// a hand (or both) to a place in the body - it may move meanwhile - and what it does begun when it is there; Dur: held so long
	auto Ps = [this, Ti, At, Side, To, bBothHands](float E, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const FVector A = CtOn(T, At), H = To(); FVector Fc = (H - A).GetSafeNormal2D(); if (Fc.IsNearlyZero()) Fc = CtDirW(T, FVector2D(1.0, 0.0));
		const FVector Lt = ADPPose::LeftOf(Fc) * 14.f;
		if (bBothHands) { P = ADPPose::Both(A, Fc, H + Lt, H - Lt, 0.9f, PH, E); return; }
		P = ADPPose::Reach(A, Fc, Side, H, 0.9f, PH, E);
		const float Low = FMath::Clamp((float(P.Pelvis.Z) + 10.f - float(H.Z)) / 90.f, 0.f, 1.f) * E; P.Pelvis -= CtUp * (PH * 0.28f * Low) + Fc * (10.f * Low); P.Lean += 24.f * Low;
	};
	PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(FMath::SmoothStep(0.f, 1.f, K), PH, P); }, 0.7f);
	if (Fn) Do(Ci, MoveTemp(Fn));
	PoseAct(Ci, [Ps](float, float PH, FDriverPose& P) { Ps(1.f, PH, P); }, Dur);
	PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(1.f - FMath::SmoothStep(0.f, 1.f, K), PH, P); }, 0.6f);
}

TFunction<void(float, float, FDriverPose&)> AGSE::CaterClimbFn(int32 Ti, bool bUp) const
{
	// the three steps at the side door, both hands on its rails (going down the same way backwards: facing the body)
	return [this, Ti, bUp](float K0, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const float K = bUp ? K0 : 1.f - K0; const FVector Fc = FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(); const float X = 187.f;
		auto St = [&](int32 n, float Dx) { return TW(T, SvcP(T, *FString::Printf(TEXT("IK_Step_BoxDoorL_%d"), n)) + FVector(X - 191.3f + Dx, 0.f, 0.f)); };
		auto Key = [&](int32 i)
		{
			FDriverPose Q; Q.Facing = Fc; Q.Lean = 12.f; Q.bHand[0] = Q.bHand[1] = true; Q.Grip[0] = Q.Grip[1] = 1.f;
			const float Hz = 150.f + 22.f * i; Q.Hand[0] = TW(T, FVector(226.3f, -129.5f, Hz)); Q.Hand[1] = TW(T, FVector(144.8f, -129.5f, Hz));
			switch (i)
			{
			case 0: Q = ADPPose::Standing(TW(T, FVector(X, -166.f, 0.f)), Fc, PH); break;
			case 1: Q.Pelvis = TW(T, FVector(X, -144.f, PH + 4.f)); Q.Foot[0] = St(0, 8.f); Q.Foot[1] = TW(T, FVector(X - 9.f, -156.f, 0.f)); break;
			case 2: Q.Pelvis = TW(T, FVector(X, -134.f, 41.8f + PH - 4.f)); Q.Foot[0] = St(0, 8.f); Q.Foot[1] = St(1, -8.f); break;
			case 3: Q.Pelvis = TW(T, FVector(X, -128.f, 71.8f + PH - 4.f)); Q.Foot[0] = St(2, 8.f); Q.Foot[1] = St(1, -8.f); break;
			case 4: Q.Pelvis = TW(T, FVector(X, -112.f, 101.8f + PH + 4.f)); Q.Lean = 18.f; Q.Foot[0] = St(2, 8.f); Q.Foot[1] = TW(T, FVector(X - 9.f, -94.f, CtFloor)); break;
			default: Q = ADPPose::Standing(TW(T, FVector(X, -66.f, CtFloor)), Fc, PH); break;
			}
			return Q;
		};
		const float U = K * 5.f; const int32 i = FMath::Clamp(FMath::FloorToInt(U), 0, 4);
		P = ADPPose::Lerp(Key(i), Key(i + 1), FMath::SmoothStep(0.f, 1.f, U - i));
	};
}

void AGSE::CaterBoxDoor(int32 Ci, int32 Ti, bool bOpen)
{
	// the side door from inside: pushed open, or drawn shut by the inner handle (as far out as the arm goes with it)
	CaterHand(Ci, Ti, FVector2D(187.0, -72.0), 0, [this, Ti]
	{
		const FTractor& T = Tractors[Ti]; const FTractor::FSvcDoor& D = T.SvcDoors[1]; const FVector H = SvcP(T, TEXT("IK_Handle_BoxDoorL_In"));
		return FVector(TW(T, D.Piv + FRotator(0.f, 100.f * FMath::Min(0.45f, FMath::SmoothStep(0.f, 1.f, D.Now)), 0.f).RotateVector(H - D.Piv)));
	}, 1.5f, [this, Ti, bOpen] { FTractor& T = Tractors[Ti]; T.SvcDoors[1].Want = bOpen ? 1.f : 0.f; if (bOpen) Sound(*T.SndDoorOpen, TW(T, T.SvcDoors[1].Piv)); }, false);
	WaitFor(Ci, [this, Ti](float E) { const FTractor::FSvcDoor& D = Tractors[Ti].SvcDoors[1]; return E > 5.f || D.Now == D.Want; });
}

void AGSE::CaterTrip(int32 Job, int32 Ci, int32 Ti, int32 W, int32 In, int32 Out)
{
	const FVector2D W0 = CtW[W], Fore(1.0, 0.0), Back(-1.0, 0.0), Sill(387.0, 0.0);
	auto Place = [](int32 i) { return FVector2D(CtStock[i].X, CtStock[i].Y); };
	// beside its own handle when it stands in its place: in the aisle (an outer one further over, the place before it is empty)
	auto By = [](int32 i, double Dir) { const double Sg = CtStock[i].Y > 0.f ? 1.0 : -1.0; const bool bOuter = FMath::Abs(CtStock[i].Y) > 80.f; return FVector2D(CtStock[i].X - Dir * (bOuter ? 50.0 : 66.0), Sg * (bOuter ? 33.0 : 12.0)); };
	auto Visible = [this, Ci](bool b) { if (ATugs* G = Tg()) if (AActor* A = G->Crew[Ci].A.Get()) A->SetActorHiddenInGame(!b); };
	// the pedal under the foot: red on, green off
	auto Pedal = [this, Ci, Ti](FVector2D At, double Dir, uint8 Kind)
	{
		PoseAct(Ci, [this, Ti, At, Dir, Kind](float K, float PH, FDriverPose& P)
		{
			FTractor& T = Tractors[Ti]; if (!T.Ct) return; FSvcCater& C = *T.Ct; const float E = FMath::Sin(PI * K); const FVector To = CtPt(T, float(C.MovAt.X - Dir * 47.0), float(C.MovAt.Y), float(C.MovAt.Z) + 12.f);
			const FVector A = CtOn(T, At), Fc = (To - A).GetSafeNormal2D(); P = ADPPose::Standing(A, Fc, PH); CtHands(T, P, float(Dir));
			if (FVector::Dist2D(To, A) < 75.f) P.Foot[1] = FMath::Lerp(P.Foot[1], To, E);
			C.Pedal = E > 0.8f ? Kind : 0;
		}, 1.1f);
		Do(Ci, [this, Ti] { if (FSvcCater* C = Tractors[Ti].Ct.Get()) C->Pedal = 0; });
	};
	// the trolley along a line with the man behind it (both hands on it all the way)
	auto Push = [this, Ci, Ti](FVector2D T0, FVector2D T1, FVector2D B0, FVector2D B1, double Dir, float Secs)
	{
		PoseAct(Ci, [this, Ti, T0, T1, B0, B1, Dir](float K, float PH, FDriverPose& P)
		{
			FTractor& T = Tractors[Ti]; if (!T.Ct) return;
			const float E = FMath::SmoothStep(0.f, 1.f, K) * 0.35f + K * 0.65f; const FVector2D C = FMath::Lerp(T0, T1, E);
			T.Ct->MovAt = FVector(C.X, C.Y, 0.5f * (CtFloorZ(float(C.X) - 30.f) + CtFloorZ(float(C.X) + 30.f)) + T.Ct->Lift);
			const TArray<FVector2D> Pt = {B0, B1}; const FVector2D B = FMath::Lerp(B0, B1, E); FVector2D Fd = C - B; Fd.Normalize();
			P = CtWalk(T, Pt, E * CtLen(Pt), CtDirW(T, Fd), PH); CtHands(T, P, float(Dir));
		}, Secs, true);
	};
	if (In >= 0)
	{
		const FVector2D P0 = Place(In), P1(P0.X, 0.0), B0 = By(In, 1.0), B1(P0.X - 95.5, 0.0), P2(Sill.X + 95.5, 0.0);
		CaterWalk(Ci, Ti, {W0, FVector2D(W0.X - 30.0, 0.0), FVector2D(FMath::Min(P0.X + 40.0, W0.X - 40.0), 0.0), FVector2D(P0.X - 60.0, 0.0), B0}, P0 - FVector2D(43.0, 0.0) - B0);
		Do(Ci, [this, Ti, In, P0] { if (FSvcCater* C = Tractors[Ti].Ct.Get()) { C->bLive = true; C->Mov = In; C->bMovShow = true; C->MovAt = FVector(P0.X, P0.Y, CtFloor + C->Lift); } });
		Pedal(B0, 1.0, 2); Push(P0, P1, B0, B1, 1.0, 2.4f); Push(P1, P2, B1, Sill, 1.0, FMath::Max(2.f, float(P2.X - P1.X) / 80.f));
		Do(Ci, [this, Ti, Ci, Sill, Fore, Visible] { if (FSvcCater* C = Tractors[Ti].Ct.Get()) C->bMovShow = false; Visible(false); CaterHold(Ci, Ti, Sill, Fore); Tractors[Ti].bUsed = true; });
	}
	else
	{
		CaterWalk(Ci, Ti, {W0, FVector2D(W0.X - 10.0, 0.0), FVector2D(490.0, 0.0)}, Fore);
		Do(Ci, [Visible] { Visible(false); });
	}
	Wait(Ci, CtCabinSeconds * 0.5f);
	Do(Ci, [this, Ti] { if (FSvcCater* C = Tractors[Ti].Ct.Get()) if (!C->bMovShow) C->Mov = INDEX_NONE; });   // (it stays in the cabin)
	Wait(Ci, CtCabinSeconds * 0.5f);
	if (Out >= 0)
	{
		const FVector2D P0 = Place(Out), P1(P0.X, 0.0), Bs(495.5, 0.0), Pst(400.0, 0.0), B1(P0.X + 95.5, 0.0), B0 = By(Out, -1.0);
		Do(Ci, [this, Ti, Ci, Out, Pst, Bs, Back, Visible] { FSvcCater* C = Tractors[Ti].Ct.Get(); if (!C) return; C->Mov = Out; C->bMovShow = true; C->MovAt = FVector(Pst.X, Pst.Y, CtDeck + C->Lift); CaterHold(Ci, Ti, Bs, Back); Visible(true); });
		Push(Pst, P1, Bs, B1, -1.0, FMath::Max(2.f, float(Pst.X - P1.X) / 80.f)); Push(P1, P0, B1, B0, -1.0, 2.4f); Pedal(B0, -1.0, 1);
		CaterWalk(Ci, Ti, {B0, FVector2D(FMath::Min(P0.X + 110.0, 140.0), 0.0), FVector2D(140.0, 0.0), W0}, Fore);
	}
	else
	{
		Do(Ci, [this, Ci, Ti, Back, Visible] { CaterHold(Ci, Ti, FVector2D(490.0, 0.0), Back); Visible(true); });
		CaterWalk(Ci, Ti, {FVector2D(490.0, 0.0), FVector2D(W0.X - 10.0, 0.0), W0}, Fore);
	}
	Do(Ci, [this, Job, Ti, In]
	{
		if (FSvcCater* C = Tractors[Ti].Ct.Get()) C->Mov = INDEX_NONE;
		FSvcJob* X = SvcJ(Job); if (!X) return;
		++X->CtTrip; if (In < 0) return;
		++X->RowsDone; ADP_LOG(TEXT("GSE: %s - trolley %d of %d in at the %s right door (the door itself does not move, no cabin yet)"), *X->Reg, X->RowsDone, X->Rows, X->CtDoor ? TEXT("rear") : TEXT("front"));
	});
}

void AGSE::CaterArrive(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J || !Tractors[J->Ti].Ct) return;
	FTractor& T = Tractors[J->Ti]; FVector2f At, Hd; float Sz = 0.f; CaterDock(*J, J->CtDoor, At, Hd, Sz);
	T.Ct->bStop = T.Ct->bStabStop = T.Ct->bStow = false; T.Ct->bLive = true; T.Ct->Sill = FMath::Clamp(Sz - float(T.At.Z), 330.f, 405.f); J->Stage = 0; J->CtTrip = 0;
	const float Off = float(FVector::Dist2D(T.At, Plan(At.X, At.Y))); if (Off > 20.f) ADP_WARN(TEXT("GSE: the %s stands %.0f cm off the door of %s"), *T.Name, Off, *J->Reg);
	ADP_LOG(TEXT("GSE: the %s at %s's %s right door, sill %.0f cm over its ground"), *T.Name, *J->Reg, J->CtDoor ? TEXT("rear") : TEXT("front"), T.Ct->Sill);
}

void AGSE::CaterStage(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J || J->Phase != 2 || !J->Men.Num() || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Ct) return;
	const int32 Ti = J->Ti, St = J->Stage, Door = J->CtDoor; Tractors[Ti].Ct->bLive = true;
	const bool bTwo = J->Men.Num() > 1 && !J->Men[1].bGone; const int32 H = bTwo ? 1 : 0, C0 = J->Men[0].Ci, C1 = J->Men[H].Ci;
	const TArray<FCtTrip> Trips = CtTripsLeft(Door, J->CtYou[Door]);
	if (St == 0 && (J->CtDone & (Door ? 2 : 1)))   // (you did this door: on to the other one, or home)
	{
		ADP_LOG(TEXT("GSE: the %s right door has its catering from you"), Door ? TEXT("rear") : TEXT("front"));
		if (Door) SvcLeave(Job); else { Later([this, Job] { if (SvcJ(Job)) CaterMove(Job); }); }
		return;
	}
	if (!bTwo && (St == 2 || St == 14)) { J->Stage = St + 1; if (St == 14) SvcDoor(C0, Ti, 1, false); CaterStage(Job); return; }
	if (St == 8 && !Trips.IsValidIndex(J->CtTrip)) { J->Stage = 9; CaterStage(Job); return; }
	auto Ct = [this, Ti]() -> FSvcCater& { return *Tractors[Ti].Ct; };
	auto Pt = [this, Ti](const TCHAR* N) { return SvcP(Tractors[Ti], N); };
	auto Level = [this](int32 Ci, bool b) { Do(Ci, [this, Ci, b] { if (ATugs* G = Tg()) { G->Crew[Ci].bUp = b; G->Crew[Ci].Hold = nullptr; } }); };
	// the one whose stage it is; the other one waits where he is
	auto End = [this, Job, St, bTwo, C0, C1](int32 Ci, int32 n)
	{
		Do(Ci, [this, Job, n] { if (FSvcJob* X = SvcJ(Job)) X->Stage = n; Later([this, Job] { CaterStage(Job); }); }); Wait(Ci, 0.4f);
		if (bTwo) WaitFor(Ci == C0 ? C1 : C0, [this, Job, St](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Phase != 2 || X->Stage != St; });
	};
	// a lever by the front door held until it has got there
	auto Lever = [this, Ti](int32 Ci, const TCHAR* Name, TFunction<void()> Fn, TFunction<bool()> Is)
	{
		const FVector2D At = CtW[0]; const FVector L = SvcP(Tractors[Ti], Name);
		TFunction<FVector()> Grip = [this, Ti, L] { return FVector(TW(Tractors[Ti], L + CtUp * CtQ(Tractors[Ti]))); };
		PoseAct(Ci, [this, Ti, At, Grip](float K, float PH, FDriverPose& P) { const FVector A = CtOn(Tractors[Ti], At), Hd = Grip(); P = ADPPose::Reach(A, (Hd - A).GetSafeNormal2D(), 0, Hd, 0.9f, PH, FMath::SmoothStep(0.f, 1.f, K)); }, 0.8f);
		Do(Ci, [this, Ci, Ti, At, Grip, Fn]
		{
			Fn(); if (ATugs* G = Tg()) G->Crew[Ci].Hold = [this, Ti, At, Grip](float, float PH, FDriverPose& P) { const FVector A = CtOn(Tractors[Ti], At), Hd = Grip(); P = ADPPose::Reach(A, (Hd - A).GetSafeNormal2D(), 0, Hd, 0.9f, PH, 1.f); };
		});
		WaitFor(Ci, [Is](float E) { return E > 600.f || Is(); });   // (somebody under it or on it holds it)
		PoseAct(Ci, [this, Ti, At, Grip](float K, float PH, FDriverPose& P) { const FVector A = CtOn(Tractors[Ti], At), Hd = Grip(); P = ADPPose::Reach(A, (Hd - A).GetSafeNormal2D(), 0, Hd, 0.9f, PH, 1.f - FMath::SmoothStep(0.f, 1.f, K)); }, 0.6f);
		Do(Ci, [this, Ci, Ti, At] { CaterHold(Ci, Ti, At, FVector2D(1.0, 0.0)); });
	};
	const FVector2D Fore(1.0, 0.0), Entry(187.0, -66.0), Wh = CtW[H], Mid(185.0, 0.0);
	// a rail from the deck: both hands on its top edge all the way up, or down
	auto Rail = [this, Ti, Ct, C1](int32 r, bool bUp)
	{
		CaterHand(C1, Ti, FVector2D(330.0, r ? 22.0 : -22.0), 0, [this, Ti, r]
		{
			const FTractor& T = Tractors[Ti]; const FSvcCater& C = *T.Ct; const FVector Pv = SvcP(T, r ? TEXT("PIV_RailPlatform_R") : TEXT("PIV_RailPlatform_L")); FVector Hd = SvcP(T, r ? TEXT("IK_Rail_R_Hand") : TEXT("IK_Rail_L_Hand")) - Pv; Hd.X = 35.0 - Pv.X;
			const float A = 90.f * (1.f - FMath::SmoothStep(0.f, 1.f, C.Rail[r])) * (r ? 1.f : -1.f);
			return FVector(TW(T, Pv + FQuat(FVector::ForwardVector, FMath::DegreesToRadians(A)).RotateVector(Hd) + FVector(C.Out, 0.f, C.Lift)));
		}, 2.2f, [Ct, r, bUp] { Ct().RailWant[r] = bUp ? 1.f : 0.f; }, true);
	};
	auto Flap = [this, Ti, Ct, C1](bool bDown)
	{
		CaterHand(C1, Ti, FVector2D(196.0, 0.0), 1, [this, Ti]
		{
			const FTractor& T = Tractors[Ti]; const FVector Pv = SvcP(T, TEXT("PIV_RampFlap")), Hd = SvcP(T, TEXT("IK_Handle_RampFlap")) - Pv;
			return FVector(TW(T, Pv + FQuat(FVector::RightVector, FMath::DegreesToRadians(106.f * FMath::Min(0.75f, FMath::SmoothStep(0.f, 1.f, T.Ct->Flap)))).RotateVector(Hd) + CtUp * T.Ct->Lift));
		}, 2.1f, [Ct, bDown] { Ct().FlapWant = bDown ? 1.f : 0.f; }, false);
	};
	auto RollDoor = [this, Ti, Ct, C1](bool bUp)
	{
		CaterHand(C1, Ti, FVector2D(185.0, 0.0), 1, [this, Ti] { const FTractor& T = Tractors[Ti]; return FVector(TW(T, SvcP(T, TEXT("IK_Handle_RollF_In")) + CtUp * (T.Ct->Lift + FMath::Min(T.Ct->Roll, 178.f)))); }, 2.6f,
			[Ct, bUp] { Ct().RollWant = bUp ? 190.f : 0.f; }, true);
	};
	// its own chocks and cones, carried by hand like the refueller's: out before the legs go down, back once they are up
	// (round the back, the aircraft stands before it; what is done already is left; the same limits as yours)
	auto Gear = [this, Ti, Ct, Pt](int32 Ci, bool bOut)
	{
		auto GHand = [this, Ci, Ti](const FVector& GSt, int32 Side, const FVector& GTo, float Dur, TFunction<void()> Fn, bool bLow)
		{
			auto GPs = [this, Ti, GSt, Side, GTo, bLow](float E, float PH, FDriverPose& P)
			{
				const FTractor& T = Tractors[Ti]; const FVector A = TW(T, GSt), Hd = TW(T, GTo), Fc = (Hd - A).GetSafeNormal2D();
				P = bLow ? ADPPose::Stoop(A, Fc, Side, Hd, 0.9f, PH, E) : ADPPose::Reach(A, Fc, Side, Hd, 0.9f, PH, E);
			};
			PoseAct(Ci, [GPs](float K, float PH, FDriverPose& P) { GPs(FMath::SmoothStep(0.f, 1.f, K), PH, P); }, Dur * 0.5f);
			Do(Ci, MoveTemp(Fn));
			PoseAct(Ci, [GPs](float K, float PH, FDriverPose& P) { GPs(1.f - FMath::SmoothStep(0.f, 1.f, K), PH, P); }, Dur * 0.5f);
		};
		auto GGo = [this, Ci, Ti](const FVector& L) { WalkTo(Ci, [this, Ti, L] { return FVector(TW(Tractors[Ti], L)); }, 12.f); };
		auto GSet = [Ct](bool bCone, int32 i, uint8 S) { return [Ct, bCone, i, S] { FSvcCater& X = Ct(); (bCone ? X.Cone[i] : X.Chock[i]) = S; X.bLive = true; }; };
		const FSvcCater& C = Ct(); const uint8 Is = bOut ? FSvcCater::AtPlace : FSvcCater::InTray;
		const bool bCh = C.Chock[0] != Is || C.Chock[1] != Is, bCo = C.Cone[0] != Is || C.Cone[1] != Is; if (!bCh && !bCo) return;
		const FVector GrCh[2] = {Pt(TEXT("IK_Chock_0")) - Pt(TEXT("PIV_Chock_0")), Pt(TEXT("IK_Chock_1")) - Pt(TEXT("PIV_Chock_1"))}, GrCo[2] = {Pt(TEXT("IK_Cone_0")) - Pt(TEXT("PIV_Cone_0")), Pt(TEXT("IK_Cone_1")) - Pt(TEXT("PIV_Cone_1"))};
		const FVector TrCh[2] = {Pt(TEXT("IK_Chock_0")), Pt(TEXT("IK_Chock_1"))}, TrCo[2] = {Pt(TEXT("IK_Cone_0")), Pt(TEXT("IK_Cone_1"))};
		const FVector PlCh[2] = {Pt(TEXT("IK_ChockPlace_0")) + GrCh[0], Pt(TEXT("IK_ChockPlace_1")) + GrCh[1]}, PlCo[2] = {Pt(TEXT("IK_ConePlace_0")) + GrCo[0], Pt(TEXT("IK_ConePlace_1")) + GrCo[1]};
		const FVector StCh = Pt(TEXT("IK_Stand_Chocks")) + FVector(0.f, -10.f, 0.f), StPl = Pt(TEXT("IK_Stand_ChockPlace")), StCo = Pt(TEXT("IK_Stand_Cones")) + FVector(0.f, 10.f, 0.f);
		const FVector StP0 = Pt(TEXT("IK_Stand_ConePlace_0")), StP1 = Pt(TEXT("IK_Stand_ConePlace_1")), BackL(-520.f, -240.f, 0.f), BackR(-520.f, 240.f, 0.f);
		Do(Ci, [Ct, Ci] { Ct().GearCi = Ci; });
		if (bOut)
		{
			if (bCh)
			{
				GGo(StCh);
				if (C.Chock[0] == FSvcCater::InTray) GHand(StCh, 1, TrCh[0], 1.4f, GSet(false, 0, FSvcCater::InRight), false);
				if (C.Chock[1] == FSvcCater::InTray) GHand(StCh, 0, TrCh[1], 1.4f, GSet(false, 1, FSvcCater::InLeft), false);
				GGo(StPl);
				if (C.Chock[0] != FSvcCater::AtPlace) GHand(StPl, 1, PlCh[0], 1.8f, GSet(false, 0, FSvcCater::AtPlace), true);
				if (C.Chock[1] != FSvcCater::AtPlace) GHand(StPl, 0, PlCh[1], 1.8f, GSet(false, 1, FSvcCater::AtPlace), true);
			}
			if (bCo)
			{
				GGo(BackL); GGo(BackR); GGo(StCo);
				if (C.Cone[1] == FSvcCater::InTray) GHand(StCo, 1, TrCo[1], 1.4f, GSet(true, 1, FSvcCater::InRight), false);
				if (C.Cone[0] == FSvcCater::InTray) GHand(StCo, 0, TrCo[0], 1.4f, GSet(true, 0, FSvcCater::InLeft), false);
				GGo(BackR); GGo(BackL); GGo(StP1);
				if (C.Cone[1] != FSvcCater::AtPlace) GHand(StP1, 1, PlCo[1], 1.6f, GSet(true, 1, FSvcCater::AtPlace), true);
				GGo(StP0);
				if (C.Cone[0] != FSvcCater::AtPlace) GHand(StP0, 0, PlCo[0], 1.6f, GSet(true, 0, FSvcCater::AtPlace), true);
			}
		}
		else
		{
			if (bCo)
			{
				GGo(StP0);
				if (C.Cone[0] == FSvcCater::AtPlace) GHand(StP0, 1, PlCo[0], 1.6f, GSet(true, 0, FSvcCater::InRight), true);
				GGo(StP1);
				if (C.Cone[1] == FSvcCater::AtPlace) GHand(StP1, 0, PlCo[1], 1.6f, GSet(true, 1, FSvcCater::InLeft), true);
				GGo(BackL); GGo(BackR); GGo(StCo);   // (the lower one goes on its post first, the other on top of it)
				if (C.Cone[0] != FSvcCater::InTray) GHand(StCo, 1, TrCo[0], 1.4f, GSet(true, 0, FSvcCater::InTray), false);
				if (C.Cone[1] != FSvcCater::InTray) GHand(StCo, 0, TrCo[1], 1.4f, GSet(true, 1, FSvcCater::InTray), false);
				GGo(BackR); GGo(BackL);
			}
			if (bCh)
			{
				GGo(StPl);
				if (C.Chock[0] == FSvcCater::AtPlace) GHand(StPl, 1, PlCh[0], 1.8f, GSet(false, 0, FSvcCater::InRight), true);
				if (C.Chock[1] == FSvcCater::AtPlace) GHand(StPl, 0, PlCh[1], 1.8f, GSet(false, 1, FSvcCater::InLeft), true);
				GGo(StCh);
				if (C.Chock[0] != FSvcCater::InTray) GHand(StCh, 1, TrCh[0], 1.4f, GSet(false, 0, FSvcCater::InTray), false);
				if (C.Chock[1] != FSvcCater::InTray) GHand(StCh, 0, TrCh[1], 1.4f, GSet(false, 1, FSvcCater::InTray), false);
			}
		}
		Do(Ci, [bOut] { ADP_LOG(TEXT("GSE: the catering truck's chocks and cones are %s"), bOut ? TEXT("set out") : TEXT("back in their trays")); });
	};
	switch (St)
	{
	case 0:   // out of the cab, both
		if (bTwo && J->Men[1].bSeated) SvcAlight(Job, 1);
		if (J->Men[0].bSeated) ClimbOut(C0, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) { X->bSeated = false; X->bOut = true; } });
		WaitFor(C0, [this, Job](float E) { return E > 60.f * (Tg() ? Tg()->Late() : 1.f) || SvcAll(Job, [](const FSvcMan& X) { return X.bOut; }); });
		End(C0, 1); break;
	case 1: case 15:   // the four legs, down or up
	{
		const FVector Sp = Pt(TEXT("IK_Stand_Stab")) + FVector(0.f, 8.f, 0.f), Lv = Pt(TEXT("IK_Lever_Stab")); const bool bDown = St == 1;
		if (bDown) Gear(C0, true);   // (the chocks before the legs go down, the cones out too)
		WalkTo(C0, [this, Ti, Sp] { return TW(Tractors[Ti], Sp); }, 12.f);
		auto Ps = [this, Ti, Sp, Lv](float E, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Reach(TW(T, Sp), FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), 1, TW(T, Lv), 0.9f, PH, E); };
		PoseAct(C0, [Ps](float K, float PH, FDriverPose& P) { Ps(FMath::SmoothStep(0.f, 1.f, K), PH, P); }, 0.8f);
		Do(C0, [Ct, bDown] { Ct().LegsWant = bDown ? 30.f : 0.f; });
		PoseAct(C0, [Ps](float, float PH, FDriverPose& P) { Ps(1.f, PH, P); }, 5.3f);
		PoseAct(C0, [Ps](float K, float PH, FDriverPose& P) { Ps(1.f - FMath::SmoothStep(0.f, 1.f, K), PH, P); }, 0.6f);
		if (!bDown) Gear(C0, false);
		End(C0, St + 1); break;
	}
	case 2: case 3:   // up the three steps, the loader first; the driver shuts the side door
	{
		const int32 Ci = St == 2 ? C1 : C0, Wi = St == 2 ? H : 0;
		WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], FVector(160.f, -215.f, 0.f)); }, 25.f);
		if (Tractors[Ti].SvcDoors.Num() > 1 && Tractors[Ti].SvcDoors[1].Want < 0.5f) SvcDoor(Ci, Ti, 1, true);
		WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], FVector(187.f, -166.f, 0.f)); }, 10.f);
		Level(Ci, true); PoseAct(Ci, CaterClimbFn(Ti, true), 5.2f, true); Do(Ci, [this, Ci, Ti, Entry] { CaterHold(Ci, Ti, Entry, FVector2D(0.0, 1.0)); });
		if (St == 3) CaterBoxDoor(Ci, Ti, false);
		CaterWalk(Ci, Ti, {Entry, FVector2D(150.0, -30.0), CtW[Wi]}, Fore);
		End(Ci, St + 1); break;
	}
	case 4:   // (when the plan has it: the passengers off) up to the sill, the crew with it
		WaitFor(C0, [this, Job, Ti](float) { const FSvcJob* X = SvcJ(Job); FTractor& V = Tractors[Ti]; if (X && X->bGo && V.YouOn) V.YouStopAt = GetWorld()->GetTimeSeconds(); return !X || (X->bGo && !V.YouOn); });
		Lever(C0, TEXT("IK_Lever_Lift"), [Ct] { Ct().LiftWant = FMath::Clamp(Ct().Sill - CtDeck, 180.f, 275.f); }, [Ct] { return Ct().Lift == Ct().LiftWant; });
		End(C0, 5); break;
	case 5: CaterWalk(C1, Ti, {Wh, Mid}, Fore); RollDoor(true); CaterWalk(C1, Ti, {Mid, Wh}, Fore); End(C1, 6); break;
	case 6: Lever(C0, TEXT("IK_Lever_Plat"), [Ct] { Ct().OutWant = CtSlide; }, [Ct] { return Ct().Out == Ct().OutWant; }); End(C0, 7); break;
	case 7:   // the flap, out on the platform, its rails (the right one lies on top), a knock on the door
		CaterWalk(C1, Ti, {Wh, Mid, FVector2D(196.0, 0.0)}, Fore); Flap(true);
		CaterWalk(C1, Ti, {FVector2D(196.0, 0.0), FVector2D(330.0, 0.0), FVector2D(330.0, 22.0)}, FVector2D(0.0, 1.0)); Rail(1, true);
		CaterWalk(C1, Ti, {FVector2D(330.0, 22.0), FVector2D(330.0, -22.0)}, FVector2D(0.0, -1.0)); Rail(0, true);
		CaterWalk(C1, Ti, {FVector2D(330.0, -22.0), FVector2D(480.0, 0.0)}, Fore);
		for (int32 i = 0; i < 2; ++i) CaterHand(C1, Ti, FVector2D(480.0, 0.0), 1, [this, Ti] { const FTractor& T = Tractors[Ti]; return FVector(CtPt(T, 529.f, 10.f, CtDeck + CtQ(T) + 135.f)); }, 0.15f, nullptr, false);
		Do(C1, [this, Job] { if (const FSvcJob* X = SvcJ(Job)) ADP_LOG(TEXT("GSE: the catering platform is at %s's %s right door"), *X->Reg, X->CtDoor ? TEXT("rear") : TEXT("front")); });
		CaterWalk(C1, Ti, {FVector2D(480.0, 0.0), Mid, Wh}, Fore); End(C1, 8); break;
	case 8:
	{
		const FCtTrip X = Trips[J->CtTrip]; const int32 N = J->CtTrip, m = bTwo ? N % 2 : 0, Ci = m ? C1 : C0;
		CaterTrip(Job, Ci, Ti, m, X.In, X.Out);
		// (the same stage again: the other one waits for the trip, not for the stage)
		Do(Ci, [this, Job] { Later([this, Job] { CaterStage(Job); }); }); Wait(Ci, 0.4f);
		if (bTwo) WaitFor(Ci == C0 ? C1 : C0, [this, Job, N](float) { const FSvcJob* Y = SvcJ(Job); return !Y || Y->Phase != 2 || Y->Stage != 8 || Y->CtTrip != N; });
		break;
	}
	case 9:   // and back: the left rail first, the right one over it, the flap
		CaterWalk(C1, Ti, {Wh, Mid, FVector2D(330.0, 0.0), FVector2D(330.0, -22.0)}, FVector2D(0.0, -1.0)); Rail(0, false);
		CaterWalk(C1, Ti, {FVector2D(330.0, -22.0), FVector2D(330.0, 22.0)}, FVector2D(0.0, 1.0)); Rail(1, false);
		CaterWalk(C1, Ti, {FVector2D(330.0, 22.0), FVector2D(330.0, 0.0), FVector2D(196.0, 0.0)}, Fore); Flap(false);
		CaterWalk(C1, Ti, {FVector2D(196.0, 0.0), Mid, Wh}, Fore); End(C1, 10); break;
	case 10: Lever(C0, TEXT("IK_Lever_Plat"), [Ct] { Ct().OutWant = 0.f; }, [Ct] { return Ct().Out == 0.f; }); End(C0, 11); break;
	case 11: CaterWalk(C1, Ti, {Wh, Mid}, Fore); RollDoor(false); CaterWalk(C1, Ti, {Mid, Wh}, Fore); End(C1, 12); break;
	case 12:
		Lever(C0, TEXT("IK_Lever_Lift"), [Ct] { Ct().LiftWant = 0.f; }, [Ct] { return Ct().Lift == 0.f; });
		Do(C0, [this, Job, Door] { if (FSvcJob* X = SvcJ(Job)) { X->CtDone |= Door ? 2 : 1; ADP_LOG(TEXT("GSE: %s's catering at the %s right door is done"), *X->Reg, Door ? TEXT("rear") : TEXT("front")); if (Door) for (FSvcMan& M : X->Men) M.bDown = true; } });
		End(C0, 13); break;
	case 13: case 14:   // out: the driver opens the side door, the last one shuts it
	{
		const int32 Ci = St == 13 ? C0 : C1, Wi = St == 13 ? 0 : H;
		CaterWalk(Ci, Ti, {CtW[Wi], FVector2D(150.0, -30.0), Entry}, FVector2D(0.0, 1.0));
		if (St == 13) CaterBoxDoor(Ci, Ti, true);
		PoseAct(Ci, CaterClimbFn(Ti, false), 5.2f, true); Level(Ci, false);
		WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], FVector(120.f, -235.f, 0.f)); }, 25.f);
		if (St == 14) SvcDoor(Ci, Ti, 1, false);
		End(Ci, St + 1); break;
	}
	default:
		{   // (it drives off now: anything of its own still out goes back, said so)
			FSvcCater& GC = Ct(); bool bOff = false;
			for (int32 i = 0; i < 2; ++i) { if (GC.Chock[i] != FSvcCater::InTray) { GC.Chock[i] = FSvcCater::InTray; bOff = true; } if (GC.Cone[i] != FSvcCater::InTray) { GC.Cone[i] = FSvcCater::InTray; bOff = true; } }
			if (bOff) ADP_WARN(TEXT("GSE: the catering truck left with its chocks or cones still out: put back in their trays"));
		}
		if (Door) { SvcLeave(Job); if (bTwo) SvcBoard(Job, 1); break; }
		// on to the rear door, when no train or belt loader is there
		if (bTwo) SvcBoard(Job, 1);
		ClimbIn(C0, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) X->bSeated = true; });
		WaitFor(C0, [this, Job, Ti](float) { return SvcAll(Job, [](const FSvcMan& X) { return X.bSeated; }) && Tractors[Ti].SvcDoors[0].Now < 0.01f; });
		Do(C0, [this, Job] { Later([this, Job] { CaterMove(Job); }); }); Wait(C0, 0.5f);
		break;
	}
}

void AGSE::CaterMove(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti; J->Why = WetAt(J->StandX, true, Ti) ? TEXT("the toilet truck is at the tail") : TEXT("the baggage train or the belt loader is at the rear door");
	const double Until = GetWorld()->GetTimeSeconds() + CtRearPatience * (Tg() ? Tg()->Late() : 1.f);
	auto Free = [this, Ti, X = J->StandX] { return CaterRearFree(X) && !WetAt(X, true, Ti) && !SvcSideBusy(Ti, X); };
	Tractors[Ti].Until = [this, Free, Until] { return GetWorld()->GetTimeSeconds() > Until || Free(); };
	Tractors[Ti].AfterUntil = [this, Job, Ti, Free]
	{
		FSvcJob* X = SvcJ(Job); if (!X) return;
		if (!Free())
		{
			// (it never came free: nobody waits for good - home without the rear door's trolleys)
			ADP_WARN(TEXT("GSE: the catering truck gives up the rear door of %s after %.0f min: %s"), *X->Reg, CtRearPatience / 60.f, *X->Why);
			X->Why.Reset(); X->CtDone |= 2; for (FSvcMan& M : X->Men) M.bDown = true;
			SvcLeave(Job); return;
		}
		X->Why.Reset(); X->CtDoor = 1; FTractor& T = Tractors[Ti]; T.Job = EJob::Out; Motor(T, true);
		float S = 0.f, N = 0.f, Sp = 36.f, Sz = 0.f; StandGeo(X->StandX, S, N, Sp); FVector2f At, Hd; CaterDock(*X, 1, At, Hd, Sz); At = StandFrame(S).Turn(At);
		// straight back out to the lane and up it past the rear door, then in again
		const float Yb = At.Y + 20.f; TArray<FVector> R; for (int32 i = X->InWay.Num() - 1; i >= 0; --i) if (AftY(S, X->InWay[i]) < Yb - 0.5f) R.Add(X->InWay[i]);
		R.Add(PlanT(S, S + CtLane, Yb));
		ADP_LOG(TEXT("GSE: the %s on to %s's rear right door"), *T.Name, *X->Reg);
		Go(T, MoveTemp(R), true, [this, Job, Ti, Yb]
		{
			FSvcJob* Y = SvcJ(Job); if (!Y) return;
			TArray<FVector> W, Full; CaterIn(*Y, 1, Yb, W);
			// (the way out again: up the lane)
			float S2 = 0.f, N2 = 0.f, Sp2 = 36.f; StandGeo(Y->StandX, S2, N2, Sp2); for (float Ly = N2 + 46.f; Ly > Yb; Ly -= 1.f) Full.Add(PlanT(S2, S2 + CtLane, Ly));
			Full.Append(W); Y->InWay = MoveTemp(Full);
			Go(Tractors[Ti], MoveTemp(W), false, [this, Job] { SvcWork(Job); });
		});
	};
}

bool AGSE::CaterLost(int32 Job, int32 m)
{
	// somebody vanished at work (false: gone, the other one goes on alone and may drive); the last one, or the driver on the
	// way: whoever takes over, the body let down meanwhile
	FSvcJob* J = SvcJ(Job); ATugs* G = Tg(); if (!J || !G || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Ct) return true;
	FTractor& T = Tractors[J->Ti]; FSvcCater& C = *T.Ct; const int32 Lost = J->Men[m].Ci, Ti = J->Ti; G->Crew[Lost].bUp = false;
	for (uint8* S : {&C.Chock[0], &C.Chock[1], &C.Cone[0], &C.Cone[1]}) if (*S == FSvcCater::InRight || *S == FSvcCater::InLeft) *S = FSvcCater::InTray;   // (what was in his hands: back in its tray)
	if (C.GearCi == Lost) C.GearCi = INDEX_NONE;
	if (C.Trolleys.IsValidIndex(C.Mov)) { const int32 i = C.Mov; C.Mov = INDEX_NONE; for (const FSvcCater::FBit& B : C.Trolleys[i].Bits) B.K->UpdateInstanceTransform(B.Ix, B.Rel * FTransform(FRotator(0.f, 90.f, 0.f), C.Trolleys[i].Home), false, true, true); }
	const bool bOther = J->Men.Num() > 1 && !J->Men[1 - m].bGone;
	if (J->Phase != 2 && bOther && m == 1) { J->Men[1].bGone = true; return false; }
	if (!bOther || J->Phase != 2)
	{
		G->Crew[Lost].bRelCater = true;
		if (J->Phase == 2)
		{
			C.RailWant[0] = C.RailWant[1] = C.FlapWant = C.OutWant = C.RollWant = C.LiftWant = 0.f;
			if (J->Stage > 1 && J->Stage < 15) J->Stage = (J->CtDone & (J->CtDoor ? 2 : 1)) ? 15 : 2;
			G->Crew[Lost].Q.Reset(); WaitFor(Lost, [this, Ti](float) { const FSvcCater* X = Tractors[Ti].Ct.Get(); return !X || X->Lift == 0.f; }); Do(Lost, [this, Job] { Later([this, Job] { CaterStage(Job); }); });
		}
		ADP_LOG(TEXT("GSE: the catering truck waits at %s until somebody takes over"), *J->Reg);
		return true;
	}
	if (m == 0) { J->Men.Swap(0, 1); J->Men[0].Seat = 0; J->Men[1].Seat = 1; T.Driver = J->Men[0].Ci; }
	J->Men[1].bGone = true;
	const int32 Ci = J->Men[0].Ci; ATugs::FCrew& K = G->Crew[Ci]; if (K.A.IsValid()) K.A->SetActorHiddenInGame(false);
	if (J->Stage >= 2 && J->Stage <= 14) { K.Q.Reset(); if (J->Stage >= 4 && J->Stage <= 13 && K.bUp) CaterHold(Ci, Ti, CtW[0], FVector2D(1.0, 0.0)); }
	ADP_LOG(TEXT("GSE: the %s is gone from the catering at %s - the other one goes on alone"), m ? TEXT("loader") : TEXT("driver"), *J->Reg);
	Do(Ci, [this, Job] { Later([this, Job] { CaterStage(Job); }); }); Wait(Ci, 0.4f);
	return false;
}

// ------------------------------------------------------------------ yours (to-do 238): every part of it as its crew works it

namespace
{
	enum : int32 { CtFChassis, CtFBody, CtFPlat, CtFRail0, CtFRail1, CtFGate, CtFFlap, CtFRollF, CtFRollR, CtFDoor, CtFTail, CtFFold, CtFStep, CtFCabin, CtFChockAt0, CtFChockAt1, CtFConeAt0, CtFConeAt1, CtFTrolley = 100, CtFGrip = 200 };
	enum : int32 { CtSStop, CtSStab, CtSClimb, CtSCabin, CtSChock0, CtSChock1, CtSChockSet0, CtSChockSet1, CtSCone0, CtSCone1, CtSConeSet0, CtSConeSet1, CtSTake = 100 };
	// (a chock or cone switch: which one, a cone or not, at its place or in its tray)
	int32 CtGearIx(int32 S, bool& bCone, bool& bSet) { const int32 k = S - CtSChock0; bCone = k >= 4; bSet = k % 4 >= 2; return k % 2; }
	bool CtIsGear(int32 S) { return S >= CtSChock0 && S <= CtSConeSet1; }
	const int32 CtNeed[2] = {4, 7};
	int32 CtHeldTr(const FSvcCater& C) { return C.Trolleys.IsValidIndex(C.YouMov) && C.Trolleys[C.YouMov].State == FSvcCater::YourHands ? C.YouMov : INDEX_NONE; }
	bool CtIn(const FVector& P, const FVector& Mn, const FVector& Mx) { return P.X > Mn.X && P.X < Mx.X && P.Y > Mn.Y && P.Y < Mx.Y && P.Z > Mn.Z && P.Z < Mx.Z; }
	bool CtOut(const FSvcCater::FTrolley& X) { return X.State == FSvcCater::YourHands || X.State == FSvcCater::Parked; }
	// a trolley out of its place that stands in X0 .. X1
	bool CtTrolleyIn(const FSvcCater& C, float X0, float X1) { for (const FSvcCater::FTrolley& X : C.Trolleys) if (CtOut(X) && X.At.X + 44.0 > X0 && X.At.X - 44.0 < X1) return true; return false; }
	bool CtFolded(const FSvcCater& C) { return C.Fold == 0.f && C.FoldWant == 0.f && C.Tail == 0.f; }
	bool CtRailsDown(const FSvcCater& C) { return C.Rail[0] == 0.f && C.Rail[1] == 0.f && C.RailWant[0] == 0.f && C.RailWant[1] == 0.f && C.Gate == 0.f && C.Flap == 0.f && C.FlapWant == 0.f; }
	FVector CtHandle(const FSvcCater& C, int32 i) { const FVector& A = C.Trolleys[i].At; return FVector(A.X - C.Dir * 43.3, A.Y, A.Z + 99.0 + C.Lift); }
	float CtNow(const AGSE::FTractor& T, int32 M)
	{
		if (!T.Ct) return 0.f; const FSvcCater& C = *T.Ct;
		switch (M)
		{
		case CtMLegs: return C.Legs; case CtMLift: return C.Lift; case CtMPlat: return C.Out; case CtMFlap: return C.Flap; case CtMRail0: return C.Rail[0]; case CtMRail1: return C.Rail[1];
		case CtMGate: return C.Gate; case CtMRollF: return C.Roll; case CtMRollR: return C.RollR; case CtMFold: return C.Fold; case CtMTail: return C.Tail; default: return T.SvcDoors.Num() > 1 ? T.SvcDoors[1].Now : 0.f;
		}
	}
	void CtShow(const FSvcCater& C, int32 i)
	{
		const FSvcCater::FTrolley& X = C.Trolleys[i];
		const FTransform At(FQuat(CtUp, FMath::DegreesToRadians(90.f)), X.At, X.State == FSvcCater::InCabin ? FVector(0.001f) : FVector::OneVector);
		for (const FSvcCater::FBit& B : X.Bits)
		{
			FTransform R = B.Rel; if (B.Pedal && B.Pedal == C.Pedal && i == C.PedalTr && C.PedalT > 0.f) R = FTransform(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(14.f))) * R;
			B.K->UpdateInstanceTransform(B.Ix, R * At, false, true, true);
		}
	}
	void CtPark(FSvcCater& C, int32 i) { C.Trolleys[i].State = FSvcCater::Parked; C.YouMov = INDEX_NONE; C.Pedal = 1; C.PedalTr = i; C.PedalT = 0.6f; CtShow(C, i); }
}

bool AGSE::CaterStowed(const FTractor& T) const
{
	if (!T.Ct) return true; const FSvcCater& C = *T.Ct;
	for (const FSvcCater::FTrolley& X : C.Trolleys) if (X.State != FSvcCater::AtHome) return false;
	for (const float V : {C.Lift, C.LiftWant, C.Out, C.OutWant, C.Legs, C.LegsWant, C.Roll, C.RollWant, C.RollR, C.RollRWant, C.Flap, C.FlapWant, C.Rail[0], C.RailWant[0], C.Rail[1], C.RailWant[1], C.Gate, C.GateWant, C.Fold, C.FoldWant, C.Tail, C.TailWant}) if (V != 0.f) return false;
	return !C.bStop && !C.bStabStop && CtGearIn(C);
}

bool AGSE::CaterUnder(const FTractor& T, const FVector& Mn, const FVector& Mx) const
{
	if (!T.YouOn && CtIn(T.YouAt, Mn, Mx)) return true;
	const ATugs* Gt = Tg(); if (!Gt) return false; const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
	for (const auto& M : Gt->Crew) if (M.A.IsValid() && !M.A->IsHidden() && CtIn(Root.InverseTransformPosition(M.A->GetActorLocation()), Mn, Mx)) return true;
	return false;
}

bool AGSE::CaterHeld(const FTractor& T, int32 M, float To) const
{
	if (!T.Ct) return false; const FSvcCater& C = *T.Ct; const UWorld* W = GetWorld(); const bool bYours = T.Job == EJob::None;
	auto Person = [&](bool b) { if (b && W) T.YouStopAt = W->GetTimeSeconds(); return b; };
	auto In = [&](const FVector& Mn, const FVector& Mx) { return Person((T.YouOn && CtIn(T.YouAt, Mn, Mx)) || CaterUnder(T, Mn, Mx)); };
	auto Clear = [&](const FVector& A, const FVector& B)
	{
		const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; if (!W) return true;
		FCollisionQueryParams Q; Q.AddIgnoredActor(T.M.Get()); if (PC && PC->GetPawn()) Q.AddIgnoredActor(PC->GetPawn()); FHitResult Hit;
		return !W->LineTraceSingleByChannel(Hit, TW(T, A), TW(T, B), ECC_Pawn, Q) || Hit.bStartPenetrating;
	};
	const float Dk = CtDeck + C.Lift;
	switch (M)
	{
	case CtMLegs:
		if (C.bStabStop) return true;
		if (To < C.Legs) return C.Lift > 0.f || C.Out > 0.f || (bYours && Person(T.YouOn != 0));
		for (const float x : {113.f, -286.f}) for (const float y : {-119.f, 97.f}) if (In(FVector(x - 8.f, y - 4.f, -60.f), FVector(x + 31.f, y + 26.f, 60.f))) return true;
		return false;
	case CtMLift:
		if (C.bStop || C.bStabStop) return true;
		if (To < C.Lift) return In(FVector(-424.f, -112.f, -60.f), FVector(236.f, 112.f, 100.f + C.Lift)) || (C.Out > 0.f && In(FVector(236.f, -84.f, -60.f), FVector(240.f + C.Out, 84.f, Dk - 40.f)));
		if (!bYours) return false;
		for (const float y : {-100.f, 0.f, 100.f}) if (!Clear(FVector(232.f, y, 374.f + C.Lift), FVector(232.f, y, 388.f + C.Lift))) return true;
		if (C.Out > 1.f) for (const float y : {-71.5f, 71.5f}) if (!Clear(FVector(230.f + C.Out, y, Dk), FVector(230.f + C.Out, y, Dk + 12.f + 108.f * C.Rail[0]))) return true;
		return T.YouOn == 2 && !Clear(T.YouAt + CtUp * 150.0, T.YouAt + CtUp * 188.0);
	case CtMPlat:
		if (C.bStop || C.bStabStop) return true;
		if (To < C.Out) return CtTrolleyIn(C, 236.f, 900.f) || (bYours && (Person(T.YouOn == 2) || !CtRailsDown(C)));
		if (!bYours) return false;
		for (const float y : {-78.f, 0.f, 78.f}) if (!Clear(FVector(230.f + C.Out, y, Dk - 3.f), FVector(240.f + C.Out, y, Dk - 3.f))) return true;
		return false;
	case CtMRollF: return CtTrolleyIn(C, 215.f, 232.f) || (bYours && In(FVector(200.f, -80.f, 100.f + C.Lift), FVector(250.f, 80.f, 330.f + C.Lift)));
	case CtMRollR: return CtTrolleyIn(C, -425.f, -408.f) || In(FVector(-440.f, -120.f, 100.f), FVector(-390.f, 120.f, 330.f));
	case CtMFold: return C.bStop || In(FVector(-640.f, -125.f, -60.f), FVector(-422.f, 125.f, 330.f));
	case CtMTail:
	{
		if (C.bStop) return true; const FVector P = CtTailTip(C.Tail);
		return To > C.Tail && In(FVector(P.X - 180.f, -125.f, -60.f), FVector(-372.f, 125.f, P.Z - 25.f));   // (never down on anybody; who stands on it goes along)
	}
	default: return false;
	}
}

bool AGSE::CaterFree(const FTractor& T, int32 Tr, const FVector2D& From, const FVector2D& To) const
{
	const FSvcCater& C = *T.Ct; const double X0 = To.X - 44.0, X1 = To.X + 44.0, Y = FMath::Abs(To.Y) + 16.0, Yw = FMath::Abs(From.Y) + 16.0;
	// (past a limit already: it may go back out of it, never further in)
	auto Wall = [](double Now, double Was, double Max) { return Now > Max && Now > Was; };
	if (Wall(Y, Yw, 118.0) || Wall(-X0, 44.0 - From.X, 405.0)) return false;
	if (X1 > 130.0 && Y > 60.0 && (Y > Yw || To.X > From.X)) return false;
	const bool bOpen = C.Roll >= 185.f, bBridge = C.Out >= 294.9f && C.Flap > 0.98f && C.Rail[0] > 0.98f && C.Rail[1] > 0.98f;
	if (To.X > From.X && ((X1 > 222.0 && !bOpen) || (X1 > 236.0 && !bBridge) || X1 > (C.Gate > 0.f || C.GateWant > 0.f ? 490.0 : 528.0))) return false;
	if (X0 < -250.0 && To.Y > 0.0 && Y > 70.0 && (Y > Yw || To.X < From.X)) return false;   // (the galley boxes)
	for (int32 i = 0; i < C.Trolleys.Num(); ++i)
	{
		const FSvcCater::FTrolley& O = C.Trolleys[i]; if (i == Tr || O.State == FSvcCater::InCabin) continue;
		const double Dx = FMath::Abs(O.At.X - To.X), Dy = FMath::Abs(O.At.Y - To.Y); if (Dx >= 87.8 || Dy >= 31.2) continue;
		const double Wx = FMath::Abs(O.At.X - From.X), Wy = FMath::Abs(O.At.Y - From.Y); if (!(Wx < 87.8 && Wy < 31.2) || Dx < Wx || Dy < Wy) return false;
	}
	auto You = [&T](const FVector2D& A) { const FVector& P = T.YouAt; return FVector2D::Distance(FVector2D(P.X, P.Y), FVector2D(FMath::Clamp(P.X, A.X - 44.0, A.X + 44.0), FMath::Clamp(P.Y, A.Y - 16.0, A.Y + 16.0))); };
	const double D = You(To); return D >= 36.0 || D >= You(From);
}

bool AGSE::CaterTake(const FTractor& T, int32 Tr, float Dir, FString& Why) const
{
	const FSvcCater& C = *T.Ct; if (!C.Trolleys.IsValidIndex(Tr)) return false; const FSvcCater::FTrolley& X = C.Trolleys[Tr];
	if (T.Job != EJob::None || (T.YouOn != 1 && T.YouOn != 2) || CtHeldTr(C) != INDEX_NONE || !T.M.IsValid()) return false;
	if ((X.State != FSvcCater::AtHome && X.State != FSvcCater::Parked) || Dir * (X.At.X - T.YouAt.X) < 30.0 || FMath::Abs(X.At.Y - T.YouAt.Y) > 90.0) return false;
	if (X.State == FSvcCater::AtHome) for (int32 i = 0; i < C.Trolleys.Num(); ++i)
	{
		const FSvcCater::FTrolley& O = C.Trolleys[i];
		if (i != Tr && O.State != FSvcCater::InCabin && FMath::Abs(O.At.X - X.At.X) < 80.0 && O.At.Y * X.At.Y > 0.0 && FMath::Abs(O.At.Y) < FMath::Abs(X.At.Y) - 10.0) return false;   // (behind another one)
	}
	if (C.Lift != C.LiftWant || C.Out != C.OutWant) { Why = TEXT("it is moving"); return false; }
	return true;
}

void AGSE::YouCater(FTractor& T, float Dt)
{
	FSvcCater& C = *T.Ct; const UWorld* W = GetWorld(); if (!W) return;
	// (left open, and nobody near it: one part after the other, as its crew puts it back)
	if (C.bStow)
	{
		if (CaterStowed(T)) C.bStow = false;
		else
		{
			C.bStop = C.bStabStop = false; C.YouMov = INDEX_NONE;
			for (int32 i = 0; i < C.Trolleys.Num(); ++i) if (C.Trolleys[i].State != FSvcCater::AtHome) { C.Trolleys[i].State = FSvcCater::AtHome; C.Trolleys[i].At = C.Trolleys[i].Home; CtShow(C, i); }
			for (uint8* S : {&C.Chock[0], &C.Chock[1], &C.Cone[0], &C.Cone[1]}) *S = FSvcCater::InTray;   // (its chocks and cones back in their trays)
			struct FS { float* V; float* Want; };
			for (const FS& S : {FS{&C.Gate, &C.GateWant}, FS{&C.Rail[0], &C.RailWant[0]}, FS{&C.Rail[1], &C.RailWant[1]}, FS{&C.Flap, &C.FlapWant}, FS{&C.Out, &C.OutWant}, FS{&C.Roll, &C.RollWant}, FS{&C.Lift, &C.LiftWant}, FS{&C.Tail, &C.TailWant}, FS{&C.Fold, &C.FoldWant}, FS{&C.RollR, &C.RollRWant}, FS{&C.Legs, &C.LegsWant}})
				if (*S.V != 0.f || *S.Want != 0.f) { *S.Want = 0.f; break; }
		}
	}
	// which aircraft's door it stands at: one that waits for its catering
	if ((C.SeeT -= Dt) <= 0.f && T.YouOn)
	{
		C.SeeT = 0.5f; C.YouSill = 0.f; const FTransform Root(FRotator(0.f, T.Yaw, 0.f), T.At);
		for (const TPair<int32, FSvcJob>& P : SvcJobs)
		{
			const FSvcJob& J = P.Value; if (J.Phase != 0 || !ADPService::All()[J.Svc].bCater) continue;
			const AActor* Pl = WetPlane(J.StandX); if (!Pl) continue;
			for (int32 d = 0; d < 2; ++d)
			{
				const FVector L = Root.InverseTransformPosition(Pl->GetActorTransform().TransformPosition(FVector(CtDoors().Skin[d].X, CtDoors().Skin[d].Y, CtDoors().Sill[d])));
				if (L.X < 500.0 || L.X > 590.0 || FMath::Abs(L.Y) > 45.0) continue;
				if (FMath::Abs(C.YouStandX - J.StandX) > 1.f || C.YouDoor != d) ADP_LOG(TEXT("GSE: your %s stands at %s's %s right door"), *T.Name, *J.Reg, d ? TEXT("rear") : TEXT("front"));
				C.YouStandX = J.StandX; C.YouDoor = d; C.YouSill = float(L.Z);
			}
		}
	}
	if (C.PedalT > 0.f) { C.PedalT -= Dt; if (C.PedalT <= 0.f && C.Trolleys.IsValidIndex(C.PedalTr)) CtShow(C, C.PedalTr); }
	// the one you push: its handle in your hand, on its own wheels, only where it has room
	if (const int32 i = CtHeldTr(C); i != INDEX_NONE)
	{
		FSvcCater::FTrolley& X = C.Trolleys[i]; const APlayerController* PC = W->GetFirstPlayerController();
		if (T.YouOn != 1 && T.YouOn != 2) CtPark(C, i);
		else if (PC)
		{
			const FVector Lk = FRotator(0.f, T.Yaw, 0.f).UnrotateVector(FRotator(0.f, PC->GetControlRotation().Yaw, 0.f).Vector());
			const FVector2D Aim(T.YouAt.X + C.Dir * 88.0, T.YouAt.Y + (Lk.X * C.Dir > 0.1 ? FMath::Clamp(Lk.Y * 110.0, -70.0, 70.0) : FMath::Clamp(X.At.Y - T.YouAt.Y, -70.0, 70.0)));
			FVector2D P(X.At.X, X.At.Y); double Go = FMath::Min(90.0 * Dt, FVector2D::Distance(P, Aim));
			while (Go > 0.05)
			{
				const double St = FMath::Min(Go, 3.0); Go -= St; const FVector2D D = (Aim - P).GetSafeNormal() * St, Was = P;
				if (CaterFree(T, i, P, P + FVector2D(D.X, 0.0))) P.X += D.X;
				if (CaterFree(T, i, P, P + FVector2D(0.0, D.Y))) P.Y += D.Y;
				if (P.Equals(Was, 0.001)) break;
			}
			X.At = FVector(P.X, P.Y, 0.5 * (CtFloorZ(float(P.X) - 30.f) + CtFloorZ(float(P.X) + 30.f))); CtShow(C, i);
		}
	}
	// the brake on near its place: into it, locked there; in the cabin: what comes out is ready after a while
	for (int32 i = 0; i < C.Trolleys.Num(); ++i)
	{
		FSvcCater::FTrolley& X = C.Trolleys[i];
		if (X.State == FSvcCater::InCabin) X.CabinT = FMath::Max(0.f, X.CabinT - Dt);
		if (X.State != FSvcCater::Parked || FVector::Dist2D(X.At, X.Home) > 24.0) continue;
		const FVector N = FMath::VInterpConstantTo(X.At, X.Home, Dt, 30.f); if (!CaterFree(T, i, FVector2D(X.At.X, X.At.Y), FVector2D(N.X, N.Y))) continue;
		X.At = N; if (X.At.Equals(X.Home, 0.1)) { X.At = X.Home; X.State = FSvcCater::AtHome; }
		CtShow(C, i); if (X.State != FSvcCater::AtHome || !X.bEmpty || !X.bOwed) continue;
		// (an empty one back in its place: one exchange of that door's)
		X.bOwed = false; T.bUsed = true;
		for (TPair<int32, FSvcJob>& P : SvcJobs)
		{
			FSvcJob& J = P.Value; if (J.Phase != 0 || !ADPService::All()[J.Svc].bCater || FMath::Abs(J.StandX - C.YouStandX) > 1.f) continue;
			const int32 d = X.Door; if (J.CtYou[d] < CtNeed[d]) { ++J.CtYou[d]; ++J.RowsDone; }
			ADP_LOG(TEXT("GSE: %s's %s right door: %d of %d trolleys exchanged by you"), *J.Reg, d ? TEXT("rear") : TEXT("front"), J.CtYou[d], CtNeed[d]);
			if (J.CtYou[d] >= CtNeed[d]) J.CtDone |= d ? 2 : 1;
			break;
		}
	}
	if (C.YouStandX > -1e5f && !T.YouOn && CaterStowed(T))
	{
		// (all back in its place: what you did is done; else a crew does the rest)
		for (auto It = SvcJobs.CreateIterator(); It; ++It)
		{
			FSvcJob& J = It.Value(); if (J.Phase != 0 || !ADPService::All()[J.Svc].bCater || FMath::Abs(J.StandX - C.YouStandX) > 1.f) continue;
			if (J.CtDone == 3) { ADP_LOG(TEXT("GSE: %s served by you"), *J.Reg); It.RemoveCurrent(); }
			break;
		}
		C.YouStandX = -1e6f; C.YouSill = 0.f;
	}
}

TSharedPtr<FVehTable> AGSE::CaterControls()
{
	using K = FVehControl;
	const FVector No(0, 0, 0), L(0, -1, 0), R(0, 1, 0); const int32 NTr = int32(UE_ARRAY_COUNT(CtStock));
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>(); TArray<K>& Cs = Tb->Controls;
	auto Mo = [&Cs](const TCHAR* Part, const TCHAR* Pt, K::EKind Kind, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 M, float A, float B, bool bTurns = true) { K X; X.Part = Part; X.Point = Pt; X.Kind = Kind; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Motion = M; X.A = A; X.B = B; X.bTurns = bTurns; Cs.Add(X); };
	auto Sw = [&Cs](const TCHAR* Part, const TCHAR* Pt, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 S) { K X; X.Part = Part; X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Switch = S; Cs.Add(X); };
	auto Box = [&Tb](const FVector& Mn, const FVector& Mx, int32 Fr, TFunction<bool(const FTractor&)> On = nullptr) { FVehSolid S; S.Min = Mn; S.Max = Mx; S.Frame = Fr; S.On = MoveTemp(On); Tb->Solids.Add(S); };

	Mo(TEXT("Lever_Stab"), TEXT("IK_Lever_Stab"), K::Lever, TEXT("Stabilisers down / up"), CtFChassis, L, CtMLegs, 0.f, 30.f);
	Sw(TEXT("IK_Button_StabEStop"), TEXT("IK_Button_StabEStop"), TEXT("Emergency stop, stabilisers"), CtFChassis, L, CtSStab);
	Mo(TEXT("Lever_Lift"), TEXT("IK_Lever_Lift"), K::Lever, TEXT("Body up / down"), CtFBody, No, CtMLift, 0.f, 275.f);
	Mo(TEXT("Lever_Plat"), TEXT("IK_Lever_Plat"), K::Lever, TEXT("Platform out / in"), CtFBody, No, CtMPlat, 0.f, CtSlide);
	Sw(TEXT("IK_Button_EStop"), TEXT("IK_Button_EStop"), TEXT("Emergency stop"), CtFBody, No, CtSStop);
	Mo(TEXT("BoxDoorL"), TEXT("IK_Handle_BoxDoorL_Out"), K::Push, TEXT("Side door"), CtFDoor, No, CtMDoor, 0.f, 1.f, false);
	Mo(TEXT("BoxDoorL"), TEXT("IK_Handle_BoxDoorL_In"), K::Push, TEXT("Side door"), CtFDoor, No, CtMDoor, 0.f, 1.f, false);
	Sw(TEXT("IK_Step_BoxDoorL_2"), TEXT("IK_Step_BoxDoorL_2"), TEXT("Steps"), CtFStep, No, CtSClimb);
	Mo(TEXT("RollF_00"), TEXT("IK_Handle_RollF_In"), K::Hand, TEXT("Front roll-up door"), CtFRollF, No, CtMRollF, 0.f, 190.f);
	Mo(TEXT("RollF_00"), TEXT("IK_Handle_RollF_Out"), K::Hand, TEXT("Front roll-up door"), CtFRollF, No, CtMRollF, 0.f, 190.f);
	Mo(TEXT("RollR_00"), TEXT("IK_Handle_RollR_In"), K::Hand, TEXT("Rear roll-up door"), CtFRollR, No, CtMRollR, 0.f, 190.f);
	Mo(TEXT("RollR_00"), TEXT("IK_Handle_RollR_Out"), K::Hand, TEXT("Rear roll-up door"), CtFRollR, No, CtMRollR, 0.f, 190.f);
	Mo(TEXT("RampFlap"), TEXT("IK_Handle_RampFlap"), K::Hand, TEXT("Bridge flap"), CtFFlap, No, CtMFlap, 0.f, 1.f);
	Mo(TEXT("RailPlatform_L"), TEXT("IK_Rail_L_Hand"), K::Hand, TEXT("Left rail"), CtFRail0, No, CtMRail0, 0.f, 1.f);
	Mo(TEXT("RailPlatform_R"), TEXT("IK_Rail_R_Hand"), K::Hand, TEXT("Right rail"), CtFRail1, No, CtMRail1, 0.f, 1.f);
	Mo(TEXT("GatePlatform"), TEXT("IK_Handle_GatePlatform"), K::Hand, TEXT("Platform gate"), CtFGate, No, CtMGate, 0.f, 1.f);
	Mo(TEXT("TailPlatform"), TEXT("IK_Handle_TailPlatform"), K::Hand, TEXT("Tail lift: fold out / up"), CtFFold, No, CtMFold, 0.f, 1.f);
	Mo(TEXT("IK_Button_TailUp"), TEXT("IK_Button_TailUp"), K::Hold, TEXT("Tail lift up"), CtFBody, R, CtMTail, 1.f, 0.f, false);
	Mo(TEXT("IK_Button_TailDown"), TEXT("IK_Button_TailDown"), K::Hold, TEXT("Tail lift down"), CtFBody, R, CtMTail, 0.f, 1.f, false);
	Sw(TEXT("IK_Box_In"), TEXT("IK_Box_In"), TEXT("Take the empty trolley out of the aircraft"), CtFCabin, No, CtSCabin);
	for (int32 i = 0; i < NTr; ++i) for (int32 e = 0; e < 2; ++e) Sw(TEXT("IK_Box_In"), TEXT("IK_Box_In"), TEXT("Take the full trolley (brake off)"), CtFGrip + i * 2 + e, No, CtSTake + i * 2 + e);

	// what stops you: the chassis, the body with its openings, the platform with its rails, the trolleys
	Box(FVector(251, -123, 40), FVector(453, 123, 292), CtFChassis); Box(FVector(-416, -125, 19), FVector(411, 125, 125), CtFChassis); Box(FVector(165, -122, 35), FVector(218, -60, 105), CtFChassis);
	Box(FVector(-424, -125, 130), FVector(236, 125, 150), CtFBody); Box(FVector(-424, -125, 372), FVector(236, 125, 381), CtFBody);
	Box(FVector(-424, 119, 150), FVector(236, 125, 372), CtFBody); Box(FVector(-424, -125, 150), FVector(152, -119, 372), CtFBody); Box(FVector(222, -125, 150), FVector(236, -119, 372), CtFBody); Box(FVector(152, -125, 343), FVector(222, -119, 372), CtFBody);
	Box(FVector(222, -125, 150), FVector(236, -67, 372), CtFBody); Box(FVector(222, 67, 150), FVector(236, 125, 372), CtFBody); Box(FVector(-424, -125, 150), FVector(-411, -105, 372), CtFBody); Box(FVector(-424, 105, 150), FVector(-411, 125, 372), CtFBody);
	Box(FVector(222, -67, 347), FVector(236, 67, 372), CtFBody); Box(FVector(-424, -105, 347), FVector(-411, 105, 372), CtFBody);
	Box(FVector(222, -67, 150), FVector(230, 67, 340), CtFRollF, [](const FTractor& T) { return T.Ct && T.Ct->Roll < 185.f; }); Box(FVector(-418, -105, 150), FVector(-411, 105, 340), CtFRollR, [](const FTractor& T) { return T.Ct && T.Ct->RollR < 185.f; });
	Box(FVector(152, -126, 150), FVector(222, -120, 343), CtFBody, [](const FTractor& T) { return T.SvcDoors.Num() > 1 && T.SvcDoors[1].Now < 0.6f; });
	Box(FVector(-385, 72, 150), FVector(-250, 120, 320), CtFBody); Box(FVector(236, -62, 150), FVector(242, 62, 193), CtFBody, [](const FTractor& T) { return T.Ct && T.Ct->Flap < 0.9f; });
	Box(FVector(-96, -82, 131), FVector(236, 82, 136.5), CtFPlat);
	for (int32 r = 0; r < 2; ++r) { const float y = r ? 71.5f : -71.5f; Box(FVector(-35, y - 4, 137), FVector(206, y + 4, 245), CtFPlat, [r](const FTractor& T) { return T.Ct && T.Ct->Rail[r] > 0.5f; }); }
	Box(FVector(200, -69, 137), FVector(208, 69, 245), CtFPlat, [](const FTractor& T) { return T.Ct && T.Ct->Gate > 0.5f; });
	Box(FVector(-438, -118, 142), FVector(-428, 118, 306), CtFBody, [](const FTractor& T) { return T.Ct && T.Ct->Fold < 0.5f && T.Ct->Tail == 0.f; }); Box(FVector(-595, -118, 139), FVector(-431, 118, 149), CtFTail, [](const FTractor& T) { return T.Ct && T.Ct->Fold > 0.5f; });
	for (int32 i = 0; i < NTr; ++i) Box(FVector(-43.9, -15.65, 2), FVector(43.9, 15.65, 94.6), CtFTrolley + i, [i](const FTractor& T) { return T.Ct && T.Ct->Trolleys.IsValidIndex(i) && T.Ct->Trolleys[i].State != FSvcCater::InCabin && T.Ct->Trolleys[i].State != FSvcCater::YourHands && T.Ct->Mov != i; });

	// its own chocks and cones: taken from the tray or from where they stand, set out at the drive wheel and fore and aft
	for (int32 i = 0; i < 2; ++i)
	{
		const TCHAR* Ch = i ? TEXT("Chock_1") : TEXT("Chock_0"); const TCHAR* ChIk = i ? TEXT("IK_Chock_1") : TEXT("IK_Chock_0"); const TCHAR* Co = i ? TEXT("Cone_1") : TEXT("Cone_0"); const TCHAR* CoIk = i ? TEXT("IK_Cone_1") : TEXT("IK_Cone_0");
		Sw(Ch, ChIk, TEXT("Wheel chock"), CtFChassis, L, CtSChock0 + i); Sw(Ch, ChIk, TEXT("Wheel chock"), CtFChockAt0 + i, L, CtSChockSet0 + i);
		Sw(Co, CoIk, TEXT("Warning cone"), CtFChassis, R, CtSCone0 + i); Sw(Co, CoIk, TEXT("Warning cone"), CtFConeAt0 + i, L, CtSConeSet0 + i);
	}
	// (the cones set out stand in your way; the chocks are under the truck)
	Box(FVector(513, -177, 0), FVector(557, -133, 75), CtFChassis, [](const FTractor& T) { return T.Ct && T.Ct->Cone[0] == FSvcCater::AtPlace; });
	Box(FVector(-482, -197, 0), FVector(-438, -153, 75), CtFChassis, [](const FTractor& T) { return T.Ct && T.Ct->Cone[1] == FSvcCater::AtPlace; });
	Tb->Frame = [](const AGSE& G, const FTractor& T, int32 Fr) -> FTransform
	{
		if (!T.Ct) return FTransform::Identity; const FSvcCater& C = *T.Ct; const FVector Q = CtUp * C.Lift, Sl = Q + FVector(C.Out, 0, 0);
		auto About = [](const FVector& P, const FQuat& Rt, const FVector& Mv) { return FTransform(-P) * FTransform(Rt) * FTransform(P + Mv); };
		auto Turn = [](float K) { return FMath::SmoothStep(0.f, 1.f, K); };
		if (Fr >= CtFGrip) { const int32 i = (Fr - CtFGrip) / 2; return C.Trolleys.IsValidIndex(i) ? FTransform(C.Trolleys[i].At + FVector((Fr - CtFGrip) % 2 ? 43.3 : -43.3, 0, 99) + Q - G.SvcP(T, TEXT("IK_Box_In"))) : FTransform::Identity; }
		if (Fr >= CtFTrolley) return C.Trolleys.IsValidIndex(Fr - CtFTrolley) ? FTransform(C.Trolleys[Fr - CtFTrolley].At + Q) : FTransform::Identity;
		switch (Fr)
		{
		case CtFBody: return FTransform(Q);
		case CtFPlat: return FTransform(Sl);
		// (the rails' and the gate's points are given with the rails up)
		case CtFRail0: case CtFRail1: { const int32 r = Fr - CtFRail0; return About(G.SvcP(T, r ? TEXT("PIV_RailPlatform_R") : TEXT("PIV_RailPlatform_L")), FQuat(FVector::ForwardVector, FMath::DegreesToRadians((r ? 90.f : -90.f) * (1.f - Turn(C.Rail[r])))), Sl); }
		case CtFGate:
			if (C.Gate > 0.f) return About(G.SvcP(T, TEXT("PIV_GatePlatform")), FQuat(CtUp, FMath::DegreesToRadians(90.f * Turn(C.Gate))), Sl);
			return About(G.SvcP(T, TEXT("PIV_RailPlatform_R")), FQuat(FVector::ForwardVector, FMath::DegreesToRadians(90.f * (1.f - Turn(C.Rail[1])))), Sl);
		case CtFFlap: return About(G.SvcP(T, TEXT("PIV_RampFlap")), FQuat(FVector::RightVector, FMath::DegreesToRadians(106.f * Turn(C.Flap))), Q);
		case CtFRollF: return FTransform(Q + CtUp * C.Roll);
		case CtFRollR: return FTransform(Q + CtUp * C.RollR);
		case CtFDoor: return T.SvcDoors.Num() > 1 ? About(T.SvcDoors[1].Piv, FQuat(CtUp, FMath::DegreesToRadians(100.f * T.SvcDoors[1].Now)), Q) : FTransform(Q);
		case CtFTail: return FTransform(CtTailTip(C.Tail) - CtTailTip(0.f));
		case CtFFold: return About(G.SvcP(T, TEXT("PIV_TailPlatform")), FQuat(FVector::RightVector, FMath::DegreesToRadians(-90.f * Turn(C.Fold))), CtTailTip(C.Tail) - CtTailTip(0.f));
		case CtFStep: return FTransform(CtUp * (T.YouOn == 1 ? 60.f : 0.f));
		case CtFCabin: return FTransform(FVector(500, 0, CtDeck + 99) + Q - G.SvcP(T, TEXT("IK_Box_In")));
		case CtFChockAt0: return FTransform(G.SvcP(T, TEXT("IK_ChockPlace_0")) - G.SvcP(T, TEXT("PIV_Chock_0")));
		case CtFChockAt1: return FTransform(G.SvcP(T, TEXT("IK_ChockPlace_1")) - G.SvcP(T, TEXT("PIV_Chock_1")));
		case CtFConeAt0: return FTransform(G.SvcP(T, TEXT("IK_ConePlace_0")) - G.SvcP(T, TEXT("PIV_Cone_0")));
		case CtFConeAt1: return FTransform(G.SvcP(T, TEXT("IK_ConePlace_1")) - G.SvcP(T, TEXT("PIV_Cone_1")));
		default: return FTransform::Identity;
		}
	};
	Tb->Now = CtNow;
	Tb->Want = [](FTractor& T, int32 M, float To)
	{
		if (!T.Ct) return; FSvcCater& C = *T.Ct; C.bLive = true; C.bStow = false;
		switch (M)
		{
		case CtMLegs: C.LegsWant = To; break; case CtMLift: C.LiftWant = To; break; case CtMPlat: C.OutWant = To; break; case CtMFlap: C.FlapWant = To; break; case CtMRail0: C.RailWant[0] = To; break; case CtMRail1: C.RailWant[1] = To; break;
		case CtMGate: C.GateWant = To; break; case CtMRollF: C.RollWant = To; break; case CtMRollR: C.RollRWant = To; break; case CtMFold: C.FoldWant = To; break; case CtMTail: C.TailWant = To; break; default: T.LoadWant = To > 0.5f ? 1 : 0;
		}
	};
	// (at an aircraft's door the body stops level with its sill)
	Tb->End = [](const FTractor& T, int32 M, float To) { return M == CtMLift && To > 1.f && T.Ct && T.Ct->YouSill > 0.f ? FMath::Clamp(T.Ct->YouSill - CtDeck, 180.f, 275.f) : To; };
	Tb->May = [](const AGSE& G, const FTractor& T, int32 M, float To, FString& Why)
	{
		if (!T.Ct) return false; const FSvcCater& C = *T.Ct; const bool bOpen = To > CtNow(T, M); const FVector& P = T.YouAt; Why.Reset();
		const bool bDoor = T.SvcDoors.Num() > 1 && (T.SvcDoors[1].Now > 0.01f || T.SvcDoors[1].Want > 0.5f);
		if (T.Job != EJob::None) Why = TEXT("its crew works with it");
		else if (T.Player.IsValid() || T.Speed > 1.f || T.Path.Num()) Why = TEXT("it is not parked");
		else if (CtHeldTr(C) != INDEX_NONE) Why = TEXT("you have a trolley in your hand");
		else if ((C.bStop || C.bStabStop) && (M == CtMLift || M == CtMPlat || M == CtMFold || M == CtMTail || (M == CtMLegs && C.bStabStop))) Why = TEXT("its emergency stop is pressed");
		else switch (M)
		{
		case CtMLegs: if (!bOpen && (C.Lift > 0.f || C.Out > 0.f)) Why = TEXT("the body is up"); else if (!bOpen && T.YouOn) Why = TEXT("you stand on it");
			else if (bOpen && (C.Chock[0] != FSvcCater::AtPlace || C.Chock[1] != FSvcCater::AtPlace)) Why = TEXT("the wheel chocks first"); break;
		case CtMLift:
			if (C.Legs < 29.9f) Why = TEXT("the stabilisers are not down"); else if (bDoor) Why = TEXT("the side door is open"); else if (!CtFolded(C)) Why = TEXT("the tail lift is not folded up");
			else if (!bOpen && C.Out > 0.f) Why = TEXT("the platform is out");
			break;
		case CtMPlat:
			if (C.Lift < 180.f) Why = TEXT("the body is too low"); else if (C.Roll < 185.f) Why = TEXT("the front roll-up door is shut"); else if (!CtRailsDown(C)) Why = TEXT("its rails and flap come down first");
			else if (T.YouOn == 2) Why = TEXT("you stand on it"); else if (CtTrolleyIn(C, 236.f, 900.f)) Why = TEXT("a trolley stands on it");
			break;
		case CtMFlap:
			if (C.Out < CtSlide - 0.1f) Why = TEXT("the platform is not out"); else if (C.Roll < 185.f) Why = TEXT("the front roll-up door is shut"); else if (CtTrolleyIn(C, 215.f, 300.f)) Why = TEXT("a trolley stands in its way");
			else if (!bOpen && (C.Rail[0] > 0.f || C.Rail[1] > 0.f)) Why = TEXT("the rails go down first"); else if (CtIn(P, FVector(234, -85, C.Lift), FVector(300, 85, C.Lift + 300))) Why = TEXT("you stand in its way");
			break;
		case CtMRail0: case CtMRail1:
			if (C.Out < CtSlide - 0.1f) Why = TEXT("the platform is not out"); else if (C.Flap < 0.98f) Why = TEXT("the bridge flap comes first"); else if (bOpen && M == CtMRail0 && C.Rail[1] < 0.98f) Why = TEXT("the right one comes up first");
			else if (!bOpen && M == CtMRail1 && C.Rail[0] > 0.f) Why = TEXT("the left one goes down first"); else if (!bOpen && C.Gate > 0.f) Why = TEXT("the gate goes back first"); else if (!bOpen && CtTrolleyIn(C, 239.f, 900.f)) Why = TEXT("a trolley stands on the platform");
			break;
		case CtMGate:
			if (C.Rail[1] < 0.98f || C.Rail[0] < 0.98f) Why = TEXT("the rails are not up"); else if (CtTrolleyIn(C, 360.f, 900.f)) Why = TEXT("a trolley stands in its way");
			else if (CtIn(P, FVector(440, -85, C.Lift), FVector(560, 85, C.Lift + 300))) Why = TEXT("you stand in its way");
			break;
		case CtMRollF: if (!bOpen && C.Out > 0.f) Why = TEXT("the platform is out"); else if (!bOpen && C.Flap > 0.f) Why = TEXT("the bridge flap is down"); else if (!bOpen && G.CaterHeld(T, M, 0.f)) Why = TEXT("something stands in it"); break;
		case CtMRollR: if (!bOpen && G.CaterHeld(T, M, 0.f)) Why = TEXT("something stands in it"); break;
		case CtMFold: if (C.Lift > 0.f || C.LiftWant > 0.f) Why = TEXT("the body is up"); else if (!bOpen && C.Tail > 0.f) Why = TEXT("the tail lift is not up"); else if (G.CaterHeld(T, M, To)) Why = TEXT("somebody stands in its way"); break;
		case CtMTail: if (C.Lift > 0.f) Why = TEXT("the body is up"); else if (C.Fold < 0.999f) Why = TEXT("it is folded up"); break;
		default: if (C.Lift > 0.f || C.LiftWant > 0.f) Why = TEXT("the body is up"); break;
		}
		return Why.IsEmpty();
	};
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 S, FString& Why)
	{
		const FTractor& T = G.Tractors[Ti]; if (!T.Ct) return false; const FSvcCater& C = *T.Ct;
		if (S == CtSStop || S == CtSStab) return true;
		if (T.Player.IsValid() || T.Speed > 1.f || T.Path.Num()) { Why = TEXT("it is not parked"); return false; }
		if (S == CtSClimb)
		{
			if (T.SvcDoors.Num() < 2 || T.SvcDoors[1].Now < 0.98f) Why = TEXT("the side door is shut"); else if (C.Lift > 0.f || C.LiftWant > 0.f) Why = TEXT("the body is up"); else if (CtHeldTr(C) != INDEX_NONE) Why = TEXT("you have a trolley in your hand");
			else if (CtTrolleyIn(C, 100.f, 236.f)) Why = TEXT("a trolley stands at the steps"); else if (T.Job != EJob::None) Why = TEXT("its crew works with it");
			return Why.IsEmpty();
		}
		if (S == CtSCabin) { if (CtTrolleyIn(C, 438.f, 900.f)) { Why = TEXT("a trolley stands in front of the door"); return false; } return true; }
		if (CtIsGear(S))
		{
			bool bCone = false, bSet = false; const int32 i = CtGearIx(S, bCone, bSet); const uint8 Gs = bCone ? C.Cone[i] : C.Chock[i], Place = bSet ? FSvcCater::AtPlace : FSvcCater::InTray;
			const TCHAR* Have = CtGearHeld(C);
			if (T.Job != EJob::None) Why = TEXT("its crew works with it");
			else if (Gs != FSvcCater::InYours && CtHeldTr(C) != INDEX_NONE) Why = TEXT("you have a trolley in your hand");
			else if (Gs != FSvcCater::InYours && Have) Why = FString::Printf(TEXT("you have %s in your hand"), Have);
			else if (Gs != Place && Gs != FSvcCater::InYours) Why = TEXT("it is not there");
			else if (!bCone && (C.Legs > 0.f || C.LegsWant > 0.f) && (Gs == FSvcCater::AtPlace || !bSet)) Why = TEXT("the stabilisers up first");
			else if (bCone && i == 0 && !bSet && Gs == FSvcCater::InTray && C.Cone[1] == FSvcCater::InTray) Why = TEXT("the top cone first");
			else if (bCone && i == 1 && !bSet && Gs == FSvcCater::InYours && C.Cone[0] != FSvcCater::InTray) Why = TEXT("the lower cone back first");
			else if (bCone && i == 0 && !bSet && Gs == FSvcCater::InYours && C.Cone[1] == FSvcCater::InTray) Why = TEXT("the top cone is on its post");
			return Why.IsEmpty();
		}
		return G.CaterTake(T, (S - CtSTake) / 2, (S - CtSTake) % 2 ? -1.f : 1.f, Why);
	};
	Tb->Switch = [](AGSE& G, int32 Ti, int32 S)
	{
		FTractor& T = G.Tractors[Ti]; if (!T.Ct) return false; FSvcCater& C = *T.Ct; C.bLive = true; C.bStow = false;
		if (S == CtSStop) return C.bStop = !C.bStop;
		if (S == CtSStab) return C.bStabStop = !C.bStabStop;
		if (S == CtSClimb) return true;
		if (CtIsGear(S))
		{
			bool bCone = false, bSet = false; const int32 i = CtGearIx(S, bCone, bSet); uint8& Gs = bCone ? C.Cone[i] : C.Chock[i]; const uint8 Place = bSet ? FSvcCater::AtPlace : FSvcCater::InTray;
			Gs = Gs == FSvcCater::InYours ? Place : FSvcCater::InYours; C.GearCi = INDEX_NONE;
			ADP_LOG(TEXT("GSE: catering truck: you %s %s %d"), Gs == FSvcCater::InYours ? TEXT("take") : bSet ? TEXT("set out") : TEXT("put back"), bCone ? TEXT("cone") : TEXT("chock"), i);
			return true;
		}
		int32 Tr = INDEX_NONE; FString Why;
		if (S == CtSCabin)
		{
			for (int32 i = 0; i < C.Trolleys.Num(); ++i) if (C.Trolleys[i].State == FSvcCater::InCabin && C.Trolleys[i].CabinT <= 0.f) Tr = i;
			if (Tr == INDEX_NONE || T.YouAt.X > 404.0) return false;
			C.Trolleys[Tr].At = FVector(484, 0, CtDeck); C.Dir = 1.f;
		}
		else
		{
			if (S < CtSTake || !G.CaterTake(T, (S - CtSTake) / 2, (S - CtSTake) % 2 ? -1.f : 1.f, Why)) return false;
			Tr = (S - CtSTake) / 2; C.Dir = (S - CtSTake) % 2 ? -1.f : 1.f;
		}
		C.Trolleys[Tr].State = FSvcCater::YourHands; C.YouMov = Tr; C.Pedal = 2; C.PedalTr = Tr; C.PedalT = 0.6f; C.bSqueeze = true; return true;
	};
	Tb->Say = [](const AGSE& G, const FTractor& T, const FVehControl& X) -> FString
	{
		if (!T.Ct) return FString(); const FSvcCater& C = *T.Ct;
		if (X.Switch >= CtSTake)
		{
			FString Why; const int32 i = (X.Switch - CtSTake) / 2; if (!G.CaterTake(T, i, (X.Switch - CtSTake) % 2 ? -1.f : 1.f, Why) && Why.IsEmpty()) return FString();
			return C.Trolleys[i].bEmpty ? TEXT("Take the empty trolley (brake off)") : X.Label;
		}
		if (X.Switch == CtSCabin)
		{
			if (T.YouOn != 2 || CtHeldTr(C) != INDEX_NONE || T.YouAt.X > 404.0) return FString();
			for (const FSvcCater::FTrolley& Y : C.Trolleys) if (Y.State == FSvcCater::InCabin && Y.CabinT <= 0.f) return X.Label;
			return FString();
		}
		if (CtIsGear(X.Switch))
		{
			bool bCone = false, bSet = false; const int32 i = CtGearIx(X.Switch, bCone, bSet); const uint8 Gs = bCone ? C.Cone[i] : C.Chock[i];
			if (Gs == FSvcCater::InYours) return bCone ? (bSet ? TEXT("Set the cone here") : TEXT("Put the cone back on its post")) : (bSet ? TEXT("Put the chock at the wheel") : TEXT("Put the chock back in its tray"));
			if (Gs == (bSet ? FSvcCater::AtPlace : FSvcCater::InTray) && !CtGearHeld(C) && (!bCone || bSet || i == 1 || C.Cone[1] != FSvcCater::InTray)) return bCone ? (bSet ? TEXT("Take the cone") : TEXT("Take the warning cone")) : (bSet ? TEXT("Take the chock away") : TEXT("Take the wheel chock"));
			return FString();
		}
		if (X.Switch == CtSStop) return C.bStop ? TEXT("Emergency stop: pull it out") : X.Label;
		if (X.Switch == CtSStab) return C.bStabStop ? TEXT("Emergency stop, stabilisers: pull it out") : X.Label;
		return X.Label;
	};
	Tb->Held = [](const FTractor& T) -> const TCHAR* { return !T.Ct || T.Job != EJob::None ? nullptr : CtHeldTr(*T.Ct) != INDEX_NONE ? TEXT("a trolley") : CtGearHeld(*T.Ct); };
	Tb->Grip = [](const AGSE& G, const FTractor& T, FVector& P) { const int32 i = T.Ct ? CtHeldTr(*T.Ct) : INDEX_NONE; if (i == INDEX_NONE) return false; P = CtHandle(*T.Ct, i); return true; };
	Tb->Leash = [](const AGSE& G, const FTractor& T, FVector& From) { if (T.Ct && CtGearHeld(*T.Ct)) { From = G.SvcP(T, TEXT("IK_Stand_Chocks")); return 3000.f; } const int32 i = T.Ct ? CtHeldTr(*T.Ct) : INDEX_NONE; if (i == INDEX_NONE) return 0.f; From = CtHandle(*T.Ct, i); return 112.f; };
	Tb->Drop = [](FTractor& T)
	{
		const int32 i = T.Ct ? CtHeldTr(*T.Ct) : INDEX_NONE; if (i != INDEX_NONE) CtPark(*T.Ct, i);
		if (T.Ct) for (uint8* S : {&T.Ct->Chock[0], &T.Ct->Chock[1], &T.Ct->Cone[0], &T.Ct->Cone[1]}) if (*S == FSvcCater::InYours) { *S = FSvcCater::InTray; T.Ct->bLive = true; }   // (back in its tray)
	};
	// the full one over the sill into the aircraft, or the brake on where it stands
	static auto MayIn = [](const AGSE& G, const FTractor& T) -> bool
	{
		const FSvcCater& C = *T.Ct; const int32 i = CtHeldTr(C); if (i == INDEX_NONE || C.Trolleys[i].bEmpty || C.Trolleys[i].At.X < 478.0 || C.YouSill <= 0.f || FMath::Abs(CtDeck + C.Lift - C.YouSill) > 15.f) return false;
		int32 In = 0, Owed = 0; for (const FSvcCater::FTrolley& X : C.Trolleys) { In += X.State == FSvcCater::InCabin ? 1 : 0; Owed += X.bOwed && X.Door == C.YouDoor ? 1 : 0; }
		for (const TPair<int32, FSvcJob>& P : G.SvcJobs) if (P.Value.Phase == 0 && ADPService::All()[P.Value.Svc].bCater && FMath::Abs(P.Value.StandX - C.YouStandX) < 1.f) return In < 2 && P.Value.CtYou[C.YouDoor] + Owed < CtNeed[C.YouDoor];
		return false;
	};
	Tb->HandSay = [](const AGSE& G, const FTractor& T, FString& Why) -> FString { if (CtHeldTr(*T.Ct) == INDEX_NONE) return FString(); return MayIn(G, T) ? TEXT("Hand the full trolley into the aircraft") : TEXT("Let go of the trolley (brake on)"); };
	Tb->HandDo = [](AGSE& G, FTractor& T, bool bSqueeze)
	{
		FSvcCater& C = *T.Ct; const bool bNew = bSqueeze && !C.bSqueeze; C.bSqueeze = bSqueeze; const int32 i = CtHeldTr(C); if (!bNew || i == INDEX_NONE) return;
		C.bLive = true; if (!MayIn(G, T)) { CtPark(C, i); return; }
		// (out of sight as its crew's: the aircraft has no cabin)
		FSvcCater::FTrolley& X = C.Trolleys[i]; X.State = FSvcCater::InCabin; X.bEmpty = X.bOwed = true; X.Door = uint8(C.YouDoor); X.CabinT = CtCabinSeconds; C.YouMov = INDEX_NONE; T.bUsed = true; CtShow(C, i);
	};
	Tb->Climb = [](const AGSE& G, int32 Ti, int32 S, float& Secs) -> TFunction<void(float, float, FDriverPose&)> { if (S != CtSClimb) return nullptr; Secs = 5.2f; return G.CaterClimbFn(Ti, !G.Tractors[Ti].YouOn); };
	Tb->Aboard = [](const FTractor& T, const FVector& P) -> uint8
	{
		if (!T.Ct) return 0; const FSvcCater& C = *T.Ct; const FVector Tp = CtTailTip(C.Tail);
		if (P.X > -424.0 && P.X < 239.0 && FMath::Abs(P.Y) < 122.0 && FMath::Abs(P.Z - CtFloor - C.Lift) < 40.0) return 1;
		if (C.Out > 1.f && P.X >= 239.0 && P.X < 244.0 + C.Out && FMath::Abs(P.Y) < 84.0 && FMath::Abs(P.Z - CtFloorZ(float(P.X)) - C.Lift) < 40.0) return 2;
		if (C.Fold > 0.5f && P.X < Tp.X + 5.0 && P.X > Tp.X - 172.0 && FMath::Abs(P.Y) < 120.0 && FMath::Abs(P.Z - Tp.Z - 3.0) < 22.0 && P.Z > 4.0) return 3;
		return C.Lift == 0.f && P.X > 160.0 && P.X < 225.0 && P.Y > -126.0 && P.Y < -60.0 && P.Z > 25.0 && P.Z < 130.0 ? 4 : 0;
	};
	Tb->Stowed = [](const AGSE& G, const FTractor& T) { return G.CaterStowed(T); };
	Tb->Stow = [](AGSE& G, FTractor& T) { if (T.Ct) T.Ct->bStow = T.Ct->bLive = true; };
	return Tb;
}
