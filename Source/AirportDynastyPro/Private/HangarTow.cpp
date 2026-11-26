// A hangar tow (to-do 311 step A; HangarTow.h). The way: pushed back tail first off its terminal stand onto taxiway T,
// pulled nose first T - P - A - A2 (over the runway) - B - Q - K past the hangar, then pushed tail first round onto its
// lead-in line to the tail-first stop (Hangar.h TailIn). Back: pulled nose first out, K - Q - B - A2 - A - P - T and
// nose-in onto the stand's stop. 5-8 km/h, gently, always stopping exactly. Not yet: the brake rider (step B), your own
// tow (step C). In the hall the hangar's own chocks go under the nose and right main gear (Chock); the tow holds short of
// the runway until nothing lands, rolls or takes off there (SimTraffic::RunwayBusy) and waits for the wing walkers.
#include "HangarTow.h"
#include "Tugs.h"
#include "Hangar.h"
#include "Airfield.h"
#include "ParkedAircraft.h"
#include "BabylonModel.h"
#include "AircraftLights.h"
#include "TerminalPlan.h"
#include "Turnaround.h"
#include "ADPLog.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPBoards.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SimTraffic.h"
#include "CrewPose.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
	constexpr float GearL = 12.8f;           // nose tyre to main gear (A320), m
	constexpr float TowFast = 8.f / 3.6f, TowTurn = 5.f / 3.6f, TowAcc = 0.25f;

	TArray<FVector2f> Rounded(const TArray<FVector2f>& In, float R)
	{
		TArray<FVector2f> Q; for (const FVector2f& P : In) if (!Q.Num() || FVector2f::Distance(Q.Last(), P) > 0.5f) Q.Add(P);
		if (Q.Num() < 3) return Q;
		TArray<FVector2f> Out = {Q[0]};
		for (int32 i = 1; i + 1 < Q.Num(); ++i)
		{
			const FVector2f A = (Q[i] - Q[i - 1]).GetSafeNormal(), B = (Q[i + 1] - Q[i]).GetSafeNormal();
			const float Dot = FMath::Clamp(FVector2f::DotProduct(A, B), -1.f, 1.f);
			if (Dot > 0.999f) { Out.Add(Q[i]); continue; }
			const float Tn = FMath::Min3(R * FMath::Tan(FMath::Acos(Dot) * 0.5f), 0.45f * FVector2f::Distance(Q[i], Q[i - 1]), 0.45f * FVector2f::Distance(Q[i], Q[i + 1]));
			const FVector2f P0 = Q[i] - A * Tn, P2 = Q[i] + B * Tn;
			for (int32 k = 0; k <= 8; ++k) { const float U = k / 8.f; Out.Add(P0 * (1 - U) * (1 - U) + Q[i] * 2.f * U * (1 - U) + P2 * U * U); }
		}
		Out.Add(Q.Last());
		return Out;
	}

	AHangarTow::FLeg MakeLeg(const TArray<FVector2f>& Pts, bool bPush, float R)
	{
		AHangarTow::FLeg L; L.bPush = bPush; L.P = Rounded(Pts, R);
		float Acc = 0.f; for (int32 i = 0; i < L.P.Num(); ++i) { if (i) Acc += FVector2f::Distance(L.P[i - 1], L.P[i]); L.S.Add(Acc); }
		return L;
	}

	// a point at arc length At along the leg, carried on straight before its start and after its end
	FVector2f LegAt(const AHangarTow::FLeg& L, float At)
	{
		if (L.P.Num() < 2) return L.P.Num() ? L.P[0] : FVector2f::ZeroVector;
		if (At <= 0.f) return L.P[0] + (L.P[1] - L.P[0]).GetSafeNormal() * At;
		for (int32 i = 1; i < L.P.Num(); ++i)
			if (At <= L.S[i]) { const float Sg = FMath::Max(L.S[i] - L.S[i - 1], 0.001f); return FMath::Lerp(L.P[i - 1], L.P[i], (At - L.S[i - 1]) / Sg); }
		const int32 E = L.P.Num() - 1; return L.P[E] + (L.P[E] - L.P[E - 1]).GetSafeNormal() * (At - L.S[E]);
	}

	float TwyX(const FAirfieldData& D, const TCHAR* Id, float Fallback) { const FAirfieldData::FTwy* T = D.Find(Id); return T && T->Pts.Num() ? T->Pts[0].X : Fallback; }

	FVector WorldAt(const FVector2f& P) { return FTerminalPlan::ToWorld(P.X, AAirfield::GroundAt(P.X, P.Y), P.Y); }

	FADPAircraft* Plane(UWorld* W, const FString& Reg)
	{
		UADPSim* Sim = UADPSim::Get(W); if (!Sim) return nullptr;
		return Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& A) { return A.Reg == Reg; });
	}

	// a terminal stand the tow can reach (nose to the terminal, in front of taxiway T)
	bool TowStand(const FAirfieldData::FStand& St, const FAirfieldData& D) { return St.Dir < 0.f && !St.bCargo && !St.bRemote && St.HasTug() && St.Frame().Y + GearL + 30.f < D.TY; }

	// on the runway strip: within the hold distance of its centreline (SimTraffic's 107.5 m hold)
	bool InStrip(const FAirfieldData& D, const FVector2f& P) { return FMath::Abs(P.Y - D.RwyY) < 107.5f && P.X > D.RwyX0 && P.X < D.RwyX1; }
}

bool AHangarTow::OnRunway(UWorld* W)
{
	if (!W) return false;
	const FAirfieldData& D = AAirfield::Shared();
	for (TActorIterator<AHangarTow> It(W); It; ++It) for (const FJob& J : It->Jobs) if (J.Stage == 1 && (InStrip(D, J.Nose) || InStrip(D, J.Main))) return true;
	return false;
}

bool AHangarTow::Chock(FJob& J, bool bOn)
{
	ATugs* Tg = ATugs::Get(GetWorld()); UStaticMesh* M = Tg ? Tg->ChockMesh() : nullptr;
	const FAirfieldData& D = AAirfield::Shared();
	const FAirfieldData::FHangarSite* Hs = D.Hangars.FindByPredicate([&](const FAirfieldData::FHangarSite& H) { return H.Id == J.Hangar; });
	if (!M || !Hs) return false;
	if (!Ck)
	{
		Ck = NewObject<UInstancedStaticMeshComponent>(this, TEXT("HangarChocks"));
		Ck->SetMobility(EComponentMobility::Movable); Ck->SetStaticMesh(M); Ck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (!GetRootComponent()) SetRootComponent(Ck);
		Ck->SetUsingAbsoluteLocation(true); Ck->SetUsingAbsoluteRotation(true); Ck->SetUsingAbsoluteScale(true);
		Ck->RegisterComponent();
	}
	// the four chocks: fore and aft of the nose tyre (its right side) and of the right main gear's outer wheel
	const FVector2f Dn = (J.Nose - J.Main).GetSafeNormal();
	const FVector Fwd(Dn.X, -Dn.Y, 0.f), Rt = -ADPPose::LeftOf(Fwd), NoseW = WorldAt(J.Nose);
	const FVector MainW = NoseW - Fwd * (GearL * 100.f) + Rt * 445.f;
	FVector Spots[4] = {NoseW + Rt * 35.f - Fwd * 37.8f, NoseW + Rt * 35.f + Fwd * 37.8f, MainW - Fwd * 45.4f, MainW + Fwd * 45.4f};
	TArray<FTransform> Tf;
	for (int32 i = 0; i < 4; ++i) Tf.Add(FTransform(FRotator(0.f, ((i % 2) ? Fwd : -Fwd).Rotation().Yaw - Tg->ChockYaw(), 0.f), Spots[i]));
	if (J.CkI == INDEX_NONE)   // (a free block of four, else four more)
	{
		for (int32 b = 0; b + 3 < Ck->GetInstanceCount() && J.CkI == INDEX_NONE; b += 4)
			if (!Jobs.ContainsByPredicate([&](const FJob& O) { return O.CkI == b; })) J.CkI = b;
		if (J.CkI == INDEX_NONE) { J.CkI = Ck->GetInstanceCount(); for (int32 i = 0; i < 4; ++i) Ck->AddInstance(FTransform(FQuat::Identity, Spots[i], FVector(0.0001f)), true); }
	}
	const FHangarSpec& Sp = FHangarSpec::Get(Hs->Size);
	const FVector Store = WorldAt(FVector2f(Hs->X - Sp.W / 2 + 5.2f, Hs->Y + Sp.Lip() + Sp.D * 0.52f));
	TWeakObjectPtr<AHangarTow> Me(this); const FString Reg = J.Reg; const int32 Base = J.CkI;
	const bool bTaken = Tg->TowChocks(J.Key, bOn, Store, Spots, Fwd, [Me, Reg, Base, Tf](int32 p, bool bSet)
	{
		AHangarTow* H = Me.Get(); if (!H || !H->Ck) return;
		if (p >= 0) for (int32 s = 0; s < 2; ++s)
		{
			const FTransform& T = Tf[p * 2 + s];
			H->Ck->UpdateInstanceTransform(Base + p * 2 + s, bSet ? T : FTransform(T.GetRotation(), T.GetLocation(), FVector(0.0001f)), true, true);
		}
		FJob* O = H->Jobs.FindByPredicate([&](const FJob& X) { return X.Reg == Reg; }); if (!O) return;
		if (bSet && p == 1) { O->Chocks = 2; ADP_LOG(TEXT("HangarTow: %s chocked in the hangar"), *Reg); }
		if (!bSet && p < 0) { O->Chocks = 0; ADP_LOG(TEXT("HangarTow: %s - the hangar chocks are away"), *Reg); }
	});
	if (bTaken) J.Chocks = bOn ? 1 : 3;
	return bTaken;
}

AHangarTow::AHangarTow() { PrimaryActorTick.bCanEverTick = true; }

AHangarTow* AHangarTow::Get(UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<AHangarTow> It(W); It; ++It) return *It;
	return W->SpawnActor<AHangarTow>(FVector::ZeroVector, FRotator::ZeroRotator);
}

bool AHangarTow::Busy(UWorld* W, const FString& Reg)
{
	if (!W) return false;
	for (TActorIterator<AHangarTow> It(W); It; ++It) for (const FJob& J : It->Jobs) if (J.Reg == Reg) return true;
	return false;
}

void AHangarTow::Tick(float Dt)
{
	Super::Tick(Dt);
	if ((Scan -= Dt) <= 0.f) { Scan = 2.f; Look(); }
	for (int32 i = Jobs.Num() - 1; i >= 0; --i)
	{
		Run(Jobs[i], Dt);
		if (Jobs[i].Stage == 9) Jobs.RemoveAt(i);
	}
}

void AHangarTow::Look()
{
	UWorld* W = GetWorld(); UADPSim* Sim = UADPSim::Get(W); if (!Sim) return;
	const FString Here = ADPBoards::HereIcao();
	// the check over: the tow back (one tow on the move at a time)
	const bool bMoving = Jobs.ContainsByPredicate([](const FJob& J) { return J.Stage < 3; });
	for (FJob& J : Jobs)
		if (J.Stage == 3)
		{
			const FADPAircraft* A = Plane(W, J.Reg);
			if (A && A->Status == TEXT("maintenance") && !A->CheckKind.IsEmpty()) continue;
			if (!bMoving && Start(J, true)) return;
		}
	if (bMoving) return;
	const FAirfieldData& D = AAirfield::Shared();
	for (const FADPAircraft& A : Sim->World.Aircraft)
	{
		if (A.Location != Here || A.Status != TEXT("maintenance") || (A.CheckKind != TEXT("C") && A.CheckKind != TEXT("D")) || Busy(W, A.Reg)) continue;
		FString StandId; ABabylonModel* Shown = ADPParked::Shown(A.Reg, &StandId); if (!Shown) continue;
		const FAirfieldData::FStand* St = D.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == StandId; });
		static TSet<FString> Told;
		if (!St || !TowStand(*St, D)) { if (!Told.Contains(A.Reg)) { Told.Add(A.Reg); ADP_LOG(TEXT("HangarTow: %s on stand %s - no tow route from there (only the terminal stands so far)"), *A.Reg, *StandId); } continue; }
		FJob J; J.Reg = A.Reg; J.Key = TEXT("TOW/") + A.Reg; J.Stand = StandId; J.StandX = St->X;
		const FAirfieldData::FStandFrame Fr = St->Frame(); J.Nose = FVector2f(Fr.X, Fr.Y); J.Main = J.Nose + FVector2f(0.f, GearL);
		if (!Start(J, false)) continue;
		// its own model from here (the parked one gives way in the same frame)
		ABabylonModel* B = W->SpawnActor<ABabylonModel>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (!B || !B->Place(TEXT("A320"))) { if (B) B->Destroy(); continue; }
		ADPParked::Towed.Add(A.Reg); ADPParked::Release(A.Reg);
		J.Model = B; J.Lamps = UADPAircraftLights::Fit(B, TEXT("A320")); if (J.Lamps.IsValid()) J.Lamps->SetParked(true);
		Jobs.Add(J); Pose(Jobs.Last());
		ADP_LOG(TEXT("HangarTow: %s (%s check) from stand %s to hangar %s"), *A.Reg, *A.CheckKind, *StandId, *J.Hangar);
		return;
	}
}

bool AHangarTow::Start(FJob& J, bool bBack)
{
	UWorld* W = GetWorld(); const FAirfieldData& D = AAirfield::Shared();
	FADPAircraft* A = Plane(W, J.Reg); if (!A) return false;
	if (!bBack)   // a free hangar: none another tow holds
	{
		const FAirfieldData::FHangarSite* Best = nullptr;
		for (const FAirfieldData::FHangarSite& H : D.Hangars)
			if (!Jobs.ContainsByPredicate([&](const FJob& O) { return O.Hangar == H.Id; }) && (!Best || FMath::Abs(H.X - J.StandX) < FMath::Abs(Best->X - J.StandX))) Best = &H;
		if (!Best) { ADP_LOG(TEXT("HangarTow: %s waits - no hangar bay free"), *J.Reg); return false; }
		J.Hangar = Best->Id;
	}
	else   // a free stand: its own if nobody else has it, else the first free one
	{
		const FString Here = ADPBoards::HereIcao(); const UADPSim* Sim = UADPSim::Get(W);
		auto Free = [&](const FAirfieldData::FStand& S)
		{
			if (!TowStand(S, D) || ADPTurnaround::AtStand(S.X)) return false;
			for (const FADPAircraft& O : Sim->World.Aircraft) if (O.Reg != J.Reg && O.Location == Here && O.Stand == S.Id && O.Status != TEXT("flying")) return false;
			return true;
		};
		const FAirfieldData::FStand* Pick = D.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == A->Stand && Free(S); });
		if (!Pick) Pick = D.Stands.FindByPredicate(Free);
		if (!Pick) { if ((J.WhyT -= 2.f) <= 0.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s stays in hangar %s - no stand free to tow it to"), *J.Reg, *J.Hangar); } return false; }
		J.Stand = Pick->Id; J.StandX = Pick->X; A->Stand = Pick->Id;
	}
	const FAirfieldData::FHangarSite* Hs = D.Hangars.FindByPredicate([&](const FAirfieldData::FHangarSite& H) { return H.Id == J.Hangar; });
	const FAirfieldData::FStand* St = D.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == J.Stand; });
	if (!Hs || !St) return false;
	const FHangarSpec& Sp = FHangarSpec::Get(Hs->Size);
	const float Hx = Hs->X, Yn = Hs->Y + Sp.Lip() + Sp.TailIn(), Sx = St->X, Sy = St->Frame().Y, R = 25.f;
	const float A2x = TwyX(D, TEXT("A2"), 81.f);
	const float P1 = TwyX(D, TEXT("P1"), -130.f), P2 = TwyX(D, TEXT("P2"), 150.f);
	const float Jx = FMath::Abs(P2 - Sx) >= 60.f && (FMath::Abs(P2 - Sx) <= FMath::Abs(P1 - Sx) || FMath::Abs(P1 - Sx) < 60.f) ? P2 : P1;
	float Qx = TwyX(D, TEXT("Q1"), 200.f);
	for (const TCHAR* Q : {TEXT("Q0"), TEXT("Q1"), TEXT("Q2")}) { const float X = TwyX(D, Q, 1e6f); if (FMath::Abs(A2x - X) + FMath::Abs(X - Hx) < FMath::Abs(A2x - Qx) + FMath::Abs(Qx - Hx)) Qx = X; }
	const float E = Hx >= Qx ? 1.f : -1.f, Sg = Jx > Sx ? 1.f : -1.f;
	const TArray<FVector2f> Field = {FVector2f(Jx, D.TY), FVector2f(Jx, D.AY), FVector2f(A2x, D.AY), FVector2f(A2x, D.BY), FVector2f(Qx, D.BY), FVector2f(Qx, D.KY)};   // (T to K)
	J.Legs.Reset(); J.Way.Reset(); J.Home.Reset();
	if (!bBack)
	{
		J.Nose = FVector2f(Sx, Sy); J.Main = J.Nose + FVector2f(0.f, GearL);
		J.Legs.Add(MakeLeg({J.Main, FVector2f(Sx, D.TY), FVector2f(Sx - Sg * 35.f, D.TY)}, true, 20.f));   // pushed back onto T, the nose towards the P way
		TArray<FVector2f> Pull = {FVector2f(Sx - Sg * (35.f - GearL), D.TY)}; Pull.Append(Field); Pull.Add(FVector2f(Hx + E * (R + GearL + 5.f), D.KY));
		J.Legs.Add(MakeLeg(Pull, false, R));                                                // pulled along the taxiways past the hangar
		J.Legs.Add(MakeLeg({FVector2f(Hx + E * (R + 5.f), D.KY), FVector2f(Hx, D.KY), FVector2f(Hx, Yn + GearL)}, true, R));   // pushed tail first into the hall
		J.Home = {FVector2f(Hx, D.KY)}; for (int32 i = Field.Num() - 1; i >= 0; --i) J.Home.Add(Field[i]); J.Home.Add(FVector2f(Sx, D.TY));
	}
	else
	{
		TArray<FVector2f> Pull = {FVector2f(Hx, Yn), FVector2f(Hx, D.KY)}; for (int32 i = Field.Num() - 1; i >= 0; --i) Pull.Add(Field[i]);
		Pull.Add(FVector2f(Sx, D.TY)); Pull.Add(FVector2f(Sx, Sy));
		J.Legs.Add(MakeLeg(Pull, false, R));                                                // pulled nose first out and nose-in onto the stand
		J.Way = {FVector2f(Sx, D.TY)}; J.Way.Append(Field); J.Way.Add(FVector2f(Hx, D.KY));
	}
	for (FLeg& Lg : J.Legs)   // (where its leading wheel would enter the runway strip: it holds short there)
	{
		Lg.HoldS = -1.f; const float Len = Lg.S.Num() ? Lg.S.Last() : 0.f;
		if (InStrip(D, LegAt(Lg, 0.f))) continue;
		for (float S = 0.f; S <= Len; S += 0.5f) if (InStrip(D, LegAt(Lg, S))) { Lg.HoldS = FMath::Max(0.f, S - 0.5f); break; }
	}
	J.bCleared = false; J.HoldT = 0.f;
	J.Hx = Hx; J.bBack = bBack; J.bPlanned = false; J.Stage = 0; J.Leg = 0; J.S = 0.f; J.V = 0.f; J.Clock = 0.f; J.WhyT = 0.f;
	for (TActorIterator<AHangar> It(W); It; ++It) if (FMath::Abs(It->GetActorLocation().X - Hx * 100.f) < 200.f) J.Hall = *It;
	if (AHangar* Hg = J.Hall.Get()) Hg->Command(true);   // (the door first: 10 minutes to open)
	if (bBack) ADP_LOG(TEXT("HangarTow: %s check done - towed back from hangar %s to stand %s"), *J.Reg, *J.Hangar, *J.Stand);
	return true;
}

void AHangarTow::Pose(FJob& J)
{
	ABabylonModel* B = J.Model.Get(); if (!B) return;
	const FVector2f Dn = (J.Nose - J.Main).GetSafeNormal();
	const FVector Dir(Dn.X, -Dn.Y, 0.f);   // (plan (x, y) -> world (x, -y); turned as SimTraffic::Pose)
	B->SetActorLocationAndRotation(WorldAt(J.Nose), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.X, -Dir.Y)), 0.f));
}

void AHangarTow::Run(FJob& J, float Dt)
{
	UWorld* W = GetWorld(); ATugs* Tg = ATugs::Get(W); ABabylonModel* B = J.Model.Get();
	if (!Tg || !B || J.Stage == 3 || J.Stage == 9) return;
	FString Why;
	if (J.Stage == 0)   // the tug and its crew
	{
		if ((J.Clock -= Dt) > 0.f) return;
		J.Clock = 1.f;
		const FString Ask = J.bBack ? FString::Printf(TEXT("hangar %s, request tow to stand %s."), *J.Hangar, *J.Stand) : FString::Printf(TEXT("stand %s, request tow to hangar %s."), *J.Stand, *J.Hangar);
		const FString Ok = TEXT("tow approved via Tango, Alpha, cross runway, Bravo, Kilo.");
		if (Tg->TowPlan(J.Key, J.StandX, J.Way, J.Home, Ask, Ok, Why) && Why.IsEmpty()) { J.Stage = 1; J.Clock = 0.f; ADP_LOG(TEXT("HangarTow: %s - the tow crew is on its way"), *J.Reg); }
		else if ((J.WhyT -= 1.f) <= 0.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s waits for its tow - %s"), *J.Reg, *Why); }
		return;
	}
	const FVector2f Dn = (J.Nose - J.Main).GetSafeNormal();
	const FVector Fwd(Dn.X, -Dn.Y, 0.f), NoseW = WorldAt(J.Nose);
	if (J.Stage == 2)   // unhooked: the tug gone, the door shut; cold and dark in the hall, handed back at a stand
	{
		if (!J.bBack && J.Chocks == 0 && !Chock(J, true) && (J.WhyT -= Dt) <= 0.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s waits in hangar %s - nobody of the tow crew free to set its chocks"), *J.Reg, *J.Hangar); }
		if (!Tg->IsClear(J.Key) || (J.Clock += Dt) < 30.f || (!J.bBack && J.Chocks != 2)) return;
		if (AHangar* Hd = J.Hall.Get()) Hd->Command(false);
		if (J.bBack)
		{
			ADPParked::Adopt(J.Stand, J.Reg, B); J.Stage = 9; ADP_LOG(TEXT("HangarTow: %s back on stand %s"), *J.Reg, *J.Stand);
			if (UADPSim* Sim = UADPSim::Get(W)) Tg->ChocksOn(J.StandX, J.Reg, Sim->Now() + FTimespan::FromHours(2.0), true, true, false);   // (the stand's chocks in again)
		}
		else { J.Stage = 3; ADP_LOG(TEXT("HangarTow: %s in hangar %s, door closing - its check goes on"), *J.Reg, *J.Hangar); }
		return;
	}
	// stage 1: hooked on and moving; the door open before the hall is entered or left
	const AHangar* Hg = J.Hall.Get();
	const bool bHall = J.bBack ? J.Leg == 0 && J.S < 60.f : J.Leg == 2;
	const bool bDoor = !bHall || !Hg || Hg->DoorOpen() > 0.999f;
	const bool bGo = Tg->Push(J.Key, J.StandX, NoseW + Fwd * 60.f, Fwd);
	if (bGo && !J.bPlanned)
	{
		J.bPlanned = true; B->Tags.AddUnique(FName(*(TEXT("MOVE/") + J.Reg)));
		ADP_LOG(TEXT("HangarTow: %s hooked on, brakes off - the tow begins"), *J.Reg);
	}
	if (UADPAircraftLights* Lm = J.Lamps.Get())
	{
		if (!J.bPlanned) Lm->SetParked(true);
		else { FADPLightState Ls; Ls.bPowered = true; Ls.bBeacon = true; Ls.bNav = ADPAircraftLights::Night(W); Lm->Set(Ls); }   // (towed: the beacon, nav lights at night; no strobes, landing or taxi lights)
	}
	// the tow back: the hangar's chocks away first (by the tow crew, once the tug is on the nose)
	if (J.bBack && J.Chocks == 2 && Tg->TowHooked(J.Key) && !Chock(J, false) && (J.WhyT -= Dt) <= 0.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s waits - nobody of the tow crew free to take its chocks away"), *J.Reg); }
	const bool bChocked = J.bBack && J.Chocks != 0;
	const float Lag = bGo ? Tg->TowWalkerLag(J.Key) : 0.f;   // (the wing walkers keep pace: walking, never far from their wingtips)
	if (!bGo || !bDoor || !J.bPlanned || bChocked || Lag > 10.f)
	{
		J.V = 0.f;
		if ((J.WhyT -= Dt) <= 0.f)
		{
			if (!bDoor) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s waits for the door of hangar %s (%.0f %% open)"), *J.Reg, *J.Hangar, Hg ? Hg->DoorOpen() * 100.f : 0.f); }
			else if (bGo && J.bPlanned && bChocked) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s waits - its hangar chocks are still under the wheels"), *J.Reg); }
			else if (bGo && J.bPlanned && Lag > 10.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s stops - a wing walker is %.0f m from his wingtip"), *J.Reg, Lag); }
		}
		return;
	}
	const FLeg& Lg = J.Legs[J.Leg];
	const float Len = Lg.S.Num() ? Lg.S.Last() : 0.f;
	float Rem = Len - J.S;
	if (!J.bCleared && Lg.HoldS >= 0.f && J.S <= Lg.HoldS + 0.01f)   // holding short of the runway
	{
		Rem = FMath::Min(Rem, Lg.HoldS - J.S);
		if (J.S >= Lg.HoldS - 0.05f)
		{
			if (ASimTraffic::RunwayBusy(W))
			{
				J.HoldT = 0.f;
				if ((J.WhyT -= Dt) <= 0.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s holds short of the runway - traffic"), *J.Reg); }
			}
			else if ((J.HoldT += Dt) >= 10.f) { J.bCleared = true; ADP_LOG(TEXT("HangarTow: %s cleared to cross the runway"), *J.Reg); }
			J.V = 0.f; return;
		}
	}
	const FVector2f H0 = (LegAt(Lg, J.S + 1.f) - LegAt(Lg, J.S)).GetSafeNormal(), H1 = (LegAt(Lg, J.S + 9.f) - LegAt(Lg, J.S + 8.f)).GetSafeNormal();
	const float Cap = Lag > 4.f ? TowTurn * 0.5f : (FVector2f::DotProduct(H0, H1) < 0.985f ? TowTurn : TowFast);
	const float Want = FMath::Min(Cap, FMath::Sqrt(2.f * TowAcc * FMath::Max(Rem, 0.f)));
	J.V = J.V < Want ? FMath::Min(Want, J.V + TowAcc * Dt) : FMath::Max(Want, J.V - 2.f * TowAcc * Dt);
	J.S = FMath::Min(Len, J.S + FMath::Max(J.V, 0.05f) * Dt);
	if (!J.bCleared && Lg.HoldS >= 0.f && J.S - FMath::Max(J.V, 0.05f) * Dt <= Lg.HoldS) J.S = FMath::Min(J.S, Lg.HoldS);   // (never past the hold line uncleared)
	const FVector2f Lead = LegAt(Lg, J.S), Trail = LegAt(Lg, J.S - GearL);
	J.Nose = Lg.bPush ? Trail : Lead; J.Main = Lg.bPush ? Lead : Trail;
	Pose(J);
	if (J.S < Len - 0.005f) return;
	J.V = 0.f; J.S = 0.f; J.bCleared = false; J.HoldT = 0.f;
	if (++J.Leg < J.Legs.Num()) { ADP_LOG(TEXT("HangarTow: %s - leg %d of %d"), *J.Reg, J.Leg + 1, J.Legs.Num()); return; }
	// on its mark: the brakes set, unhooked
	Tg->Release(J.Key); J.Stage = 2; J.Clock = 0.f; B->Tags.Remove(FName(*(TEXT("MOVE/") + J.Reg)));
	if (UADPAircraftLights* Lm = J.Lamps.Get()) Lm->SetParked(J.bBack);   // (in the hall: cold and dark)
	ADP_LOG(TEXT("HangarTow: %s on its mark (%.1f, %.1f) - unhooking"), *J.Reg, J.Nose.X, J.Nose.Y);
}
