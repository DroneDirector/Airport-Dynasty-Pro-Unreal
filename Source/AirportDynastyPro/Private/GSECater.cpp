// The catering high-loader at work (to-do 212; the vehicle and its crew's part are the shared ones of GSEService.cpp): your
// driver and loader drive to the right side of the aircraft - down between the wingtips, never under a wing - and up to the
// front service door, then the rear one; legs down, up into the body, up with it, the front roll-up door, the platform out
// to the fuselage, its flap and rails; the full trolleys in and the empty ones out, one trip after the other; and all back.
// One stage after the other (FSvcJob::Stage), one of the two working at a time: whoever is left goes on from the stage.
// The doors (measured on Art/A320/A320.glb): R1 0.30 m ahead of the nose wheel, sill 3.87 m, skin 2.00 m out; R2 24.77 m
// behind, sill 3.49 m, skin 1.57 m out and turned 11 deg. The aircraft is one mesh: its doors do not open and there is no
// cabin (to-do 209) - a knock on it, and with their trolley they are out of sight at the sill, as the cleaners at theirs.
#include "GSECater.h"
#include "GSE.h"
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
	const FVector2D CtSkin[2] = {FVector2D(200.0, -30.0), FVector2D(157.0, 2477.0)};   // (right of the centre line, behind the nose wheel)
	const float CtSill[2] = {387.f, 349.f}, CtTurn[2] = {0.f, 11.f};
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
}

// ------------------------------------------------------------------ the parts

void AGSE::CaterDress(FTractor& T)
{
	ABabylonModel* M = T.M.Get(); if (!M) return;
	T.Ct = MakeShared<FSvcCater>(); FSvcCater& C = *T.Ct;
	auto Pre = [&](const TCHAR* N, const FVector& P) { return M->BindGroup({FString(N)}, true, FTransform(P)); };
	C.GBody = M->BindGroupWhere([](const FString& N, int32, const FVector&)
	{
		for (const TCHAR* X : {TEXT("BoxStep"), TEXT("BoxDoorL"), TEXT("TailBrake"), TEXT("RollF_")}) if (N.StartsWith(X)) return false;
		for (const TCHAR* X : {TEXT("Box"), TEXT("Roll"), TEXT("Tail"), TEXT("NamePanel_L"), TEXT("NamePanel_R"), TEXT("WorkLamp")}) if (N.StartsWith(X) && !N.StartsWith(TEXT("NamePanel_Rear"))) return true;
		return false;
	}, FTransform::Identity);
	for (int32 i = 0; i < 12; ++i) C.GRoll.Add(Pre(*FString::Printf(TEXT("RollF_%02d"), i), FVector::ZeroVector));
	C.GPlat = Pre(TEXT("Platform"), FVector::ZeroVector); C.GFlap = Pre(TEXT("RampFlap"), SvcP(T, TEXT("PIV_RampFlap")));
	C.GRail[0] = Pre(TEXT("RailPlatform_L"), SvcP(T, TEXT("PIV_RailPlatform_L"))); C.GRail[1] = Pre(TEXT("RailPlatform_R"), SvcP(T, TEXT("PIV_RailPlatform_R")));
	C.GGate = Pre(TEXT("GatePlatform"), SvcP(T, TEXT("PIV_RailPlatform_R")));   // (it lies in the right rail and goes with it; open all the time - the fuselage closes the end)
	C.GSc[0] = Pre(TEXT("ScissorA"), SvcP(T, TEXT("PIV_ScissorMid"))); C.GSc[1] = Pre(TEXT("ScissorB"), SvcP(T, TEXT("PIV_ScissorMid")));
	for (int32 r = 0; r < 2; ++r)
	{
		const FString S = r ? TEXT("R") : TEXT("L");
		C.GRamB[r] = Pre(*(TEXT("RamBarrel_") + S), SvcP(T, *(TEXT("PIV_Ram_") + S))); C.GRamR[r] = Pre(*(TEXT("RamRod_") + S), SvcP(T, *(TEXT("PIV_RamEye_") + S)));
	}
	static const TCHAR* W[] = {TEXT("FL"), TEXT("FR"), TEXT("RL"), TEXT("RR")};
	for (int32 i = 0; i < 4; ++i) C.GLeg[i] = Pre(*(FString(TEXT("StabLeg_")) + W[i]), FVector::ZeroVector);
	C.GLev[0] = Pre(TEXT("Lever_Stab"), SvcP(T, TEXT("PIV_Lever_Stab"))); C.GLev[1] = Pre(TEXT("Lever_Lift"), SvcP(T, TEXT("PIV_Lever_Lift"))); C.GLev[2] = Pre(TEXT("Lever_Plat"), SvcP(T, TEXT("PIV_Lever_Plat")));
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
		FSvcCater::FTrolley& X = C.Trolleys.AddDefaulted_GetRef(); X.Home = FVector(Pl.X, Pl.Y, CtFloor);
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
	FSvcCater& C = *T.Ct;
	auto Put = [M](int32 G, const FTransform& X) { if (G != INDEX_NONE) M->MoveGroup(G, X); };
	auto Run = [Dt](float& V, float Want, float Rate) { const float Was = V; V = FMath::FInterpConstantTo(V, Want, Dt, Rate); return V - Was; };
	// (never the platform out below 180 cm, never down with it out)
	const float Lw = C.Out > 1.f ? FMath::Max(C.LiftWant, 180.f) : C.LiftWant;
	const float Dq = Run(C.Lift, Lw, 11.f), Dp = Run(C.Out, C.Lift >= 180.f || C.OutWant < C.Out ? C.OutWant : C.Out, 20.f), Dl = Run(C.Legs, C.LegsWant, 6.f);
	Run(C.Roll, C.RollWant, 80.f); Run(C.Flap, C.FlapWant, 0.5f); Run(C.Rail[0], C.RailWant[0], 0.6f); Run(C.Rail[1], C.RailWant[1], 0.6f);
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
		Put(C.GRail[r], X); if (r) Put(C.GGate, X);
	}
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
	// (all in its place: nothing more to do until the next job)
	const int32 Me = int32(&T - Tractors.GetData()); bool bJob = false; for (const TPair<int32, FSvcJob>& P : SvcJobs) if (P.Value.Phase > 0 && P.Value.Phase < 9 && P.Value.Ti == Me) bJob = true;
	if (!bJob && C.Lift == 0.f && C.Out == 0.f && C.Legs == 0.f && C.Roll == 0.f && C.Flap == 0.f && C.Rail[0] == 0.f && C.Rail[1] == 0.f && C.Lev[0] == 0.f && C.Lev[1] == 0.f && C.Lev[2] == 0.f && T.bPrev) C.bLive = false;
}

void AGSE::CaterRestock(FTractor& T)
{
	if (!T.Ct) return;
	T.Ct->bLive = true; T.Ct->Mov = INDEX_NONE;
	for (const FSvcCater::FTrolley& X : T.Ct->Trolleys) for (const FSvcCater::FBit& B : X.Bits) B.K->UpdateInstanceTransform(B.Ix, B.Rel * FTransform(FRotator(0.f, 90.f, 0.f), X.Home), false, true, true);
}

// ------------------------------------------------------------------ the way to a door, and who else is there

void AGSE::CaterDock(const FSvcJob& J, int32 Door, FVector2f& At, FVector2f& Hd, float& SillZ) const
{
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(J.StandX, S, N, Sp);
	const FVector Nose = Plan(S, N); const AActor* Pl = nullptr; double Bd = 900.0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		for (const FName& Tg2 : It->Tags)
		{
			const FString Ts = Tg2.ToString(); if (!Ts.StartsWith(TEXT("SIM/")) && !Ts.StartsWith(TEXT("MOVE/"))) continue;
			const double D = FVector::Dist2D(It->GetActorLocation(), Nose); if (D < Bd) { Bd = D; Pl = *It; }
		}
	// (the aircraft's root at its nose wheel: x its right, y aft - where it really stands)
	const float A = FMath::DegreesToRadians(CtTurn[Door]); const FVector2D O(FMath::Cos(A), FMath::Sin(A)), K = CtSkin[Door] + O * (Tractors.IsValidIndex(J.Ti) && Tractors[J.Ti].Am ? 499.0 : double(CtReach));   // (the ambulift's platform ends 4.96 m ahead of its middle)
	auto World = [&](const FVector2D& L, float Z) { return Pl ? Pl->GetActorTransform().TransformPosition(FVector(L.X, L.Y, Z)) : FVector(Plan(S - float(L.X) / 100.f, N + float(L.Y) / 100.f) + CtUp * Z); };
	const FVector Wo = World(K, 0.f), Ws = World(CtSkin[Door], CtSill[Door]);
	At = FVector2f(float(Wo.X) / 100.f, float(-Wo.Y) / 100.f); const FVector D = Ws - Wo; Hd = FVector2f(float(D.X), float(-D.Y)).GetSafeNormal(); SillZ = float(Ws.Z);
}

void AGSE::CaterIn(const FSvcJob& J, int32 Door, float FromY, TArray<FVector>& W) const
{
	// down the middle between the two wingtips, one bend in front of the wing (R1) or behind it (R2), straight up to the door
	float S = 0.f, N = 0.f, Sp = 36.f, Sz = 0.f; StandGeo(J.StandX, S, N, Sp); FVector2f At, Hd; CaterDock(J, Door, At, Hd, Sz);
	const float R = Tractors[J.Ti].KTurnR, X = S + CtLane; const FVector2f Pre = At - Hd * (Door ? 7.f : FMath::Max(1.f, (At.X - X) - R / 100.f - 0.02f));
	const float Ys = Pre.Y + R / 100.f + (Door ? 2.9f : 0.f);
	if (FromY > Ys + 0.5f) Leg(W, X, FromY, 0.f, -1.f, X, Ys, 0.f, -1.f, R);
	Leg(W, X, FMath::Min(Ys, FromY), 0.f, -1.f, Pre.X, Pre.Y, Hd.X, Hd.Y, R * 0.98f); Leg(W, Pre.X, Pre.Y, Hd.X, Hd.Y, At.X, At.Y, Hd.X, Hd.Y, R);
}

bool AGSE::SvcSideBusy(int32 Ti, float StandX) const
{
	// the lane on the aircraft's right has room for one: another service vehicle on its way in or out, a train or the belt loader moving there
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(StandX, S, N, Sp); const FVector Mid = Plan(S - 15.f, N + 25.f);
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
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(StandX, S, N, Sp); const FVector Mid = Plan(S - 15.f, N + 22.f);
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
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(StandX, S, N, Sp); const FVector Mid = Plan(S - 10.f, N + 30.f);
	auto In = [&](const FVector& W) { const float X = float(W.X) / 100.f, Y = float(-W.Y) / 100.f; return X > S - 22.f && X < S - 2.f && Y > N + 21.f && Y < N + 50.f; };
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
	}, FMath::Max(0.6f, L / 95.f));
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
		}, Secs);
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
	T.Ct->bLive = true; T.Ct->Sill = FMath::Clamp(Sz - float(T.At.Z), 330.f, 405.f); J->Stage = 0; J->CtTrip = 0;
	const float Off = float(FVector::Dist2D(T.At, Plan(At.X, At.Y))); if (Off > 20.f) ADP_WARN(TEXT("GSE: the %s stands %.0f cm off the door of %s"), *T.Name, Off, *J->Reg);
	ADP_LOG(TEXT("GSE: the %s at %s's %s right door, sill %.0f cm over its ground"), *T.Name, *J->Reg, J->CtDoor ? TEXT("rear") : TEXT("front"), T.Ct->Sill);
}

void AGSE::CaterStage(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J || J->Phase != 2 || !J->Men.Num() || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Ct) return;
	const int32 Ti = J->Ti, St = J->Stage, Door = J->CtDoor; Tractors[Ti].Ct->bLive = true;
	const bool bTwo = J->Men.Num() > 1 && !J->Men[1].bGone; const int32 H = bTwo ? 1 : 0, C0 = J->Men[0].Ci, C1 = J->Men[H].Ci;
	if (!bTwo && (St == 2 || St == 14)) { J->Stage = St + 1; if (St == 14) SvcDoor(C0, Ti, 1, false); CaterStage(Job); return; }
	if (St == 8 && !CtTrips(Door).IsValidIndex(J->CtTrip)) { J->Stage = 9; CaterStage(Job); return; }
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
		WaitFor(Ci, [Is](float E) { return E > 60.f || Is(); });
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
	switch (St)
	{
	case 0:   // out of the cab, both
		if (bTwo && J->Men[1].bSeated) SvcAlight(Job, 1);
		if (J->Men[0].bSeated) ClimbOut(C0, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) { X->bSeated = false; X->bOut = true; } });
		WaitFor(C0, [this, Job](float E) { return E > 60.f || SvcAll(Job, [](const FSvcMan& X) { return X.bOut; }); });
		End(C0, 1); break;
	case 1: case 15:   // the four legs, down or up
	{
		const FVector Sp = Pt(TEXT("IK_Stand_Stab")) + FVector(0.f, 8.f, 0.f), Lv = Pt(TEXT("IK_Lever_Stab")); const bool bDown = St == 1;
		WalkTo(C0, [this, Ti, Sp] { return TW(Tractors[Ti], Sp); }, 12.f);
		auto Ps = [this, Ti, Sp, Lv](float E, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Reach(TW(T, Sp), FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), 1, TW(T, Lv), 0.9f, PH, E); };
		PoseAct(C0, [Ps](float K, float PH, FDriverPose& P) { Ps(FMath::SmoothStep(0.f, 1.f, K), PH, P); }, 0.8f);
		Do(C0, [Ct, bDown] { Ct().LegsWant = bDown ? 30.f : 0.f; });
		PoseAct(C0, [Ps](float, float PH, FDriverPose& P) { Ps(1.f, PH, P); }, 5.3f);
		PoseAct(C0, [Ps](float K, float PH, FDriverPose& P) { Ps(1.f - FMath::SmoothStep(0.f, 1.f, K), PH, P); }, 0.6f);
		End(C0, St + 1); break;
	}
	case 2: case 3:   // up the three steps, the loader first; the driver shuts the side door
	{
		const int32 Ci = St == 2 ? C1 : C0, Wi = St == 2 ? H : 0;
		WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], FVector(160.f, -215.f, 0.f)); }, 25.f);
		if (Tractors[Ti].SvcDoors.Num() > 1 && Tractors[Ti].SvcDoors[1].Want < 0.5f) SvcDoor(Ci, Ti, 1, true);
		WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], FVector(187.f, -166.f, 0.f)); }, 10.f);
		Level(Ci, true); PoseAct(Ci, CaterClimbFn(Ti, true), 5.2f); Do(Ci, [this, Ci, Ti, Entry] { CaterHold(Ci, Ti, Entry, FVector2D(0.0, 1.0)); });
		if (St == 3) CaterBoxDoor(Ci, Ti, false);
		CaterWalk(Ci, Ti, {Entry, FVector2D(150.0, -30.0), CtW[Wi]}, Fore);
		End(Ci, St + 1); break;
	}
	case 4:   // (when the plan has it: the passengers off) up to the sill, the crew with it
		WaitFor(C0, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->bGo; });
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
		const FCtTrip X = CtTrips(Door)[J->CtTrip]; const int32 N = J->CtTrip, m = bTwo ? N % 2 : 0, Ci = m ? C1 : C0;
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
		PoseAct(Ci, CaterClimbFn(Ti, false), 5.2f); Level(Ci, false);
		WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], FVector(120.f, -235.f, 0.f)); }, 25.f);
		if (St == 14) SvcDoor(Ci, Ti, 1, false);
		End(Ci, St + 1); break;
	}
	default:
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
	const double Until = GetWorld()->GetTimeSeconds() + CtRearPatience;
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
		float S = 0.f, N = 0.f, Sp = 36.f, Sz = 0.f; StandGeo(X->StandX, S, N, Sp); FVector2f At, Hd; CaterDock(*X, 1, At, Hd, Sz);
		// straight back out to the lane and up it past the rear door, then in again
		const float Yb = At.Y + 20.f; TArray<FVector> R; for (int32 i = X->InWay.Num() - 1; i >= 0; --i) if (-X->InWay[i].Y / 100.f < Yb - 0.5f) R.Add(X->InWay[i]);
		R.Add(Plan(S + CtLane, Yb));
		ADP_LOG(TEXT("GSE: the %s on to %s's rear right door"), *T.Name, *X->Reg);
		Go(T, MoveTemp(R), true, [this, Job, Ti, Yb]
		{
			FSvcJob* Y = SvcJ(Job); if (!Y) return;
			TArray<FVector> W, Full; CaterIn(*Y, 1, Yb, W);
			// (the way out again: up the lane)
			float S2 = 0.f, N2 = 0.f, Sp2 = 36.f; StandGeo(Y->StandX, S2, N2, Sp2); for (float Ly = N2 + 46.f; Ly > Yb; Ly -= 1.f) Full.Add(Plan(S2 + CtLane, Ly));
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
