// A hangar tow (to-do 311 step A; HangarTow.h). The way: pushed back tail first off its terminal stand onto taxiway T,
// pulled nose first T - P - A - A2 (over the runway) - B - Q - K past the hangar, then pushed tail first round onto its
// lead-in line to the tail-first stop (Hangar.h TailIn). Back: pulled nose first out, K - Q - B - A2 - A - P - T and
// nose-in onto the stand's stop. 5-8 km/h, gently, always stopping exactly. The brake rider (step B, GSEMaint.cpp) is in
// the cockpit before the brakes come off; he switches the lights on. Not yet: your own tow (step C). In the hall the hangar's own chocks go under the nose and right main gear (Chock); the tow holds short of
// the runway until nothing lands, rolls or takes off there (SimTraffic::RunwayBusy) and waits for the wing walkers.
#include "HangarTow.h"
#include "Tugs.h"
#include "PerfLog.h"
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
#include "GSE.h"
#include "VehicleControls.h"
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


	FVector WorldAt(const FVector2f& P) { return FTerminalPlan::ToWorld(P.X, AAirfield::GroundAt(P.X, P.Y), P.Y); }

	FADPAircraft* Plane(UWorld* W, const FString& Reg)
	{
		UADPSim* Sim = UADPSim::Get(W); if (!Sim) return nullptr;
		return Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& A) { return A.Reg == Reg; });
	}

	// the taxilane a stand is pushed back onto, tail first (SimTraffic's pushback): K for a cargo stand, T for every other
	const FAirfieldData::FTwy* TowLaneOf(const FAirfieldData::FStand& St, const FAirfieldData& D) { return D.Find(St.bCargo ? TEXT("K") : TEXT("T")); }
	float TowLaneY(const FAirfieldData::FStand& St, const FAirfieldData& D) { return St.bCargo ? D.KY : D.TY; }

	// a stand the tow can reach (to-do 311): terminal, remote or cargo, with room for a tug in front of the nose and its taxilane behind the tail
	bool TowStand(const FAirfieldData::FStand& St, const FAirfieldData& D)
	{
		const FAirfieldData::FTwy* L = TowLaneOf(St, D); if (!St.HasTug() || !L || L->Pts.Num() < 2) return false;
		const float Lo = FMath::Min(L->Pts[0].X, L->Pts.Last().X), Hi = FMath::Max(L->Pts[0].X, L->Pts.Last().X);
		return St.X > Lo + 5.f && St.X < Hi - 5.f && (TowLaneY(St, D) - St.Frame().Y) * (St.Dir < 0.f ? 1.f : -1.f) > GearL + 30.f;   // (the lane behind its tail)
	}

	// The taxiway network (to-do 311): every centre line of Data/airfield.json cut where another meets or crosses it, the
	// pieces the edges; extra points (a stand's join on its taxilane, a hangar's on K, the depot road's on T) cut in as nodes.
	// Runway 08/26 is no taxiway: the only ways across are the links that meet on its centre line (A2-B2, A1-B1, A3-B3).
	struct FHtTwyNet
	{
		TArray<FVector2f> N; TArray<TArray<TPair<int32, float>>> E; TArray<TArray<FString>> EId;
		int32 Node(const FVector2f& P)
		{
			for (int32 i = 0; i < N.Num(); ++i) if (FVector2f::Distance(N[i], P) < 1.5f) return i;
			E.AddDefaulted(); EId.AddDefaulted(); return N.Add(P);
		}
		void Link(int32 A, int32 B, const FString& Id)
		{
			if (A == B) return; const float L = FVector2f::Distance(N[A], N[B]);
			E[A].Add(TPair<int32, float>(B, L)); EId[A].Add(Id); E[B].Add(TPair<int32, float>(A, L)); EId[B].Add(Id);
		}
		int32 Near(const FVector2f& P) const { for (int32 i = 0; i < N.Num(); ++i) if (FVector2f::Distance(N[i], P) < 2.f) return i; return INDEX_NONE; }
	};

	void TowBuildNet(const FAirfieldData& D, const TArray<FVector2f>& Extra, FHtTwyNet& Gn)
	{
		struct FSeg { FVector2f A, B; FString Id; };
		TArray<FSeg> Sg;
		for (const FAirfieldData::FTwy& T : D.Taxiways) for (int32 i = 1; i < T.Pts.Num(); ++i) if (FVector2f::Distance(T.Pts[i - 1], T.Pts[i]) > 0.1f) Sg.Add({T.Pts[i - 1], T.Pts[i], T.Id});
		auto Along = [](const FSeg& S, const FVector2f& P, float& U) { const FVector2f Ab = S.B - S.A; U = FMath::Clamp(FVector2f::DotProduct(P - S.A, Ab) / Ab.SizeSquared(), 0.f, 1.f); return FVector2f::Distance(S.A + Ab * U, P); };
		for (int32 i = 0; i < Sg.Num(); ++i)
		{
			const FSeg& S = Sg[i]; TArray<float> Cut = {0.f, 1.f}; float U = 0.f;
			for (int32 j = 0; j < Sg.Num(); ++j)
			{
				if (j == i) continue; const FSeg& O = Sg[j];
				if (Along(S, O.A, U) < 1.5f) Cut.Add(U);   // (an end of another line on this one: a T-junction)
				if (Along(S, O.B, U) < 1.5f) Cut.Add(U);
				const FVector2f R = S.B - S.A, Q = O.B - O.A; const float Den = R.X * Q.Y - R.Y * Q.X;
				if (FMath::Abs(Den) < 1e-4f) continue;
				const FVector2f W = O.A - S.A; const float T0 = (W.X * Q.Y - W.Y * Q.X) / Den, T1 = (W.X * R.Y - W.Y * R.X) / Den;
				if (T0 > 0.f && T0 < 1.f && T1 > 0.f && T1 < 1.f) Cut.Add(T0);   // (a crossing)
			}
			for (const FVector2f& P : Extra) if (Along(S, P, U) < 2.f) Cut.Add(U);
			Cut.Sort();
			int32 Prev = INDEX_NONE;
			for (const float C : Cut) { const int32 Nd = Gn.Node(FMath::Lerp(S.A, S.B, C)); if (Prev != INDEX_NONE) Gn.Link(Prev, Nd, S.Id); Prev = Nd; }
		}
	}

	// the shortest way between two nodes (Dijkstra; a few hundred nodes at most), its points and the taxiways it follows
	bool TowNetRoute(const FHtTwyNet& Gn, int32 From, int32 To, TArray<FVector2f>& Out, TArray<FString>* Ids = nullptr)
	{
		if (From == INDEX_NONE || To == INDEX_NONE) return false;
		TArray<float> Dist; Dist.Init(1e30f, Gn.N.Num()); TArray<int32> Prev; Prev.Init(INDEX_NONE, Gn.N.Num()); TArray<FString> By; By.SetNum(Gn.N.Num()); TArray<bool> Done; Done.Init(false, Gn.N.Num());
		Dist[From] = 0.f;
		for (;;)
		{
			int32 U = INDEX_NONE; for (int32 i = 0; i < Gn.N.Num(); ++i) if (!Done[i] && Dist[i] < 1e29f && (U == INDEX_NONE || Dist[i] < Dist[U])) U = i;
			if (U == INDEX_NONE || U == To) break;
			Done[U] = true;
			for (int32 k = 0; k < Gn.E[U].Num(); ++k) { const int32 V = Gn.E[U][k].Key; const float Dv = Dist[U] + Gn.E[U][k].Value; if (Dv < Dist[V]) { Dist[V] = Dv; Prev[V] = U; By[V] = Gn.EId[U][k]; } }
		}
		if (Dist[To] > 1e29f) return false;
		TArray<int32> Chain; for (int32 v = To; v != INDEX_NONE; v = Prev[v]) Chain.Insert(v, 0);
		Out.Reset(); for (const int32 v : Chain) Out.Add(Gn.N[v]);
		if (Ids) { Ids->Reset(); for (int32 c = 1; c < Chain.Num(); ++c) if (!Ids->Num() || Ids->Last() != By[Chain[c]]) Ids->Add(By[Chain[c]]); }
		return true;
	}

	// the route as said on the radio: "Tango, Papa 2, Alpha, Alpha 2, cross runway, Bravo 2, ..." (the turn pads left out)
	FString TowSpoken(const TArray<FString>& Ids)
	{
		TArray<FString> W;
		for (int32 i = 0; i < Ids.Num(); ++i)
		{
			const FString& Id = Ids[i];
			if (Id.IsEmpty() || Id.StartsWith(TEXT("CURVE"))) continue;
			const TCHAR C = Id[0];
			const TCHAR* Ph = C == 'A' ? TEXT("Alpha") : C == 'B' ? TEXT("Bravo") : C == 'K' ? TEXT("Kilo") : C == 'P' ? TEXT("Papa") : C == 'Q' ? TEXT("Quebec") : C == 'T' ? TEXT("Tango") : nullptr;
			const FString S = (Ph ? FString(Ph) : Id.Left(1)) + (Id.Len() > 1 ? TEXT(" ") + Id.Mid(1) : FString());
			if (!W.Num() || W.Last() != S) W.Add(S);
			// (a link onto the runway's centre line followed by the one off the other side: the crossing)
			const bool bLink = Id.Len() > 1 && FChar::IsDigit(Id[1]) && (C == 'A' || C == 'B');
			if (bLink && Ids.IsValidIndex(i + 1) && Ids[i + 1].Len() > 1 && FChar::IsDigit(Ids[i + 1][1]) && Ids[i + 1][0] == (C == 'A' ? 'B' : 'A')) W.Add(TEXT("cross runway"));
		}
		return FString::Join(W, TEXT(", "));
	}

	// an aircraft as a few lines (nose tyre at N, nose towards Fa; an A320): fuselage, wing, tailplane
	void TowShape(const FVector2f& N, const FVector2f& Fa, FVector2f (&S)[6])
	{
		const FVector2f Sd(-Fa.Y, Fa.X);
		S[0] = N + Fa * 5.f; S[1] = N - Fa * 32.6f; S[2] = N - Fa * 14.f + Sd * 17.9f; S[3] = N - Fa * 14.f - Sd * 17.9f; S[4] = N - Fa * 30.f + Sd * 6.3f; S[5] = N - Fa * 30.f - Sd * 6.3f;
	}
	float TowSegSeg(const FVector2f& A, const FVector2f& B, const FVector2f& C, const FVector2f& Dd)
	{
		auto PS = [](const FVector2f& P, const FVector2f& X, const FVector2f& Y) { const FVector2f Ab = Y - X; const float U = FMath::Clamp(FVector2f::DotProduct(P - X, Ab) / FMath::Max(Ab.SizeSquared(), 1e-4f), 0.f, 1.f); return FVector2f::Distance(X + Ab * U, P); };
		const FVector2f R = B - A, Q = Dd - C; const float Den = R.X * Q.Y - R.Y * Q.X;
		if (FMath::Abs(Den) > 1e-5f) { const FVector2f W = C - A; const float T0 = (W.X * Q.Y - W.Y * Q.X) / Den, T1 = (W.X * R.Y - W.Y * R.X) / Den; if (T0 >= 0.f && T0 <= 1.f && T1 >= 0.f && T1 <= 1.f) return 0.f; }
		return FMath::Min(FMath::Min(PS(A, C, Dd), PS(B, C, Dd)), FMath::Min(PS(C, A, B), PS(Dd, A, B)));
	}
	// the least gap between two aircraft's shapes (m; a fuselage counted 4 m wide)
	float TowGap(const FVector2f (&X)[6], const FVector2f (&Y)[6])
	{
		float M = 1e9f;
		for (int32 a = 0; a < 3; ++a) for (int32 b = 0; b < 3; ++b) M = FMath::Min(M, TowSegSeg(X[a * 2], X[a * 2 + 1], Y[b * 2], Y[b * 2 + 1]) - ((a == 0 ? 2.f : 0.f) + (b == 0 ? 2.f : 0.f)));
		return M;
	}

	// the depot road meets taxilane T here (TugTow.cpp HtRoad): the hangar tug's way onto the taxiways
	constexpr float TowRoadX = -245.f;
	// beside a cargo stand's nose, clear of its wingtip and its neighbour's (the hangar tug's way in and out there)
	constexpr float TowCargoSide = 30.f, TowCargoFront = 1222.5f;

	// on the runway strip: within the hold distance of its centreline (SimTraffic's 107.5 m hold)
	bool InStrip(const FAirfieldData& D, const FVector2f& P) { return FMath::Abs(P.Y - D.RwyY) < 107.5f && P.X > D.RwyX0 && P.X < D.RwyX1; }
}

namespace
{
	bool bTowCrossed = false; FVector2f TowCrossS = FVector2f::ZeroVector, TowCrossN = FVector2f::ZeroVector;   // (the last tow's crossing)
}

bool AHangarTow::Crossing(UWorld* W, FVector2f& South, FVector2f& North)
{
	const FAirfieldData& D = AAirfield::Shared();
	if (W) for (TActorIterator<AHangarTow> It(W); It; ++It) for (const FJob& J : It->Jobs) for (const FLeg& Lg : J.Legs)
	{
		if (Lg.HoldS < 0.f || !Lg.S.Num()) continue;
		const FVector2f In = LegAt(Lg, Lg.HoldS); FVector2f Out = In;
		for (float S = Lg.HoldS + 1.f; S <= Lg.S.Last(); S += 0.5f) { Out = LegAt(Lg, S); if (!InStrip(D, Out)) break; }
		if (InStrip(D, Out)) continue;   // (the leg ends on the strip: not a crossing)
		bTowCrossed = true; TowCrossS = In.Y < Out.Y ? In : Out; TowCrossN = In.Y < Out.Y ? Out : In;
		break;
	}
	// (to-do 318: no tow planned yet - the way a tow from the first hangar to a stand across the runway would take, worked out
	// once, so the maintenance vans may cross from the start; a real tow's crossing replaces it as soon as one is planned)
	static bool bTowGuessed = false;
	if (!bTowCrossed && !bTowGuessed && D.Hangars.Num() && D.Stands.Num())
	{
		bTowGuessed = true;
		const FAirfieldData::FHangarSite& Hs = D.Hangars[0];
		for (int32 Pass = 0; Pass < 2 && !bTowCrossed; ++Pass) for (const FAirfieldData::FStand& St : D.Stands)
		{
			if (St.bCargo != (Pass == 1) || !TowStand(St, D)) continue;
			const FVector2f StJoin(St.X, TowLaneY(St, D)), HJoin(Hs.X, D.KY);
			FHtTwyNet Net; TowBuildNet(D, {StJoin, HJoin}, Net);
			TArray<FVector2f> Route; if (!TowNetRoute(Net, Net.Near(StJoin), Net.Near(HJoin), Route) || Route.Num() < 2) continue;
			const FLeg Lg = MakeLeg(Route, false, 25.f); if (!Lg.S.Num()) continue;
			float Hold = -1.f; if (InStrip(D, LegAt(Lg, 0.f))) continue;
			for (float S = 0.f; S <= Lg.S.Last(); S += 0.5f) if (InStrip(D, LegAt(Lg, S))) { Hold = FMath::Max(0.f, S - 0.5f); break; }
			if (Hold < 0.f) continue;
			const FVector2f In = LegAt(Lg, Hold); FVector2f Out = In;
			for (float S = Hold + 1.f; S <= Lg.S.Last(); S += 0.5f) { Out = LegAt(Lg, S); if (!InStrip(D, Out)) break; }
			if (InStrip(D, Out)) continue;
			bTowCrossed = true; TowCrossS = In.Y < Out.Y ? In : Out; TowCrossN = In.Y < Out.Y ? Out : In;
			ADP_LOG(TEXT("HangarTow: the runway crossing from the start (hangar %s - stand %s): %.0f,%.0f - %.0f,%.0f"), *Hs.Id, *St.Id, TowCrossS.X, TowCrossS.Y, TowCrossN.X, TowCrossN.Y);
			break;
		}
		if (!bTowCrossed) ADP_WARN(TEXT("HangarTow: no runway crossing found from hangar %s to any stand - the vans wait for the first tow"), *Hs.Id);
	}
	South = TowCrossS; North = TowCrossN;
	return bTowCrossed;
}

bool AHangarTow::OnRunway(UWorld* W, bool bVans)
{
	if (!W) return false;
	if (bVans) if (const AGSE* G = AGSE::Find(W)) if (G->VanOnStrip()) return true;   // (a maintenance van crossing)
	const FAirfieldData& D = AAirfield::Shared();
	for (TActorIterator<AHangarTow> It(W); It; ++It) for (const FJob& J : It->Jobs) if (J.Stage == 1 && (InStrip(D, J.Nose) || InStrip(D, J.Main))) return true;
	return false;
}

bool AHangarTow::Chock(FJob& J, bool bOn, bool bNow)
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
	if (bNow)   // (as saved: in place already, nobody carries them - Restore)
	{
		for (int32 i = 0; i < 4; ++i) Ck->UpdateInstanceTransform(J.CkI + i, Tf[i], true, true);
		J.Chocks = 2; return true;
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

bool AHangarTow::TowOf(UWorld* W, const FString& Reg, FString& Hall, int32& Stage, bool& bBack)
{
	if (!W) return false;
	for (TActorIterator<AHangarTow> It(W); It; ++It) for (const FJob& J : It->Jobs) if (J.Reg == Reg && J.Stage != 9) { Hall = J.Hangar; Stage = J.Stage; bBack = J.bBack; return true; }
	return false;
}

bool AHangarTow::HallTaken(UWorld* W, const FString& HallId)
{
	if (!W) return false;
	for (TActorIterator<AHangarTow> It(W); It; ++It) for (const FJob& J : It->Jobs) if (J.Hangar == HallId && J.Stage != 9) return true;
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

void AHangarTow::Restore()
{
	UWorld* W = GetWorld(); UADPSim* Sim = UADPSim::Get(W); if (!Sim) return;
	bRestored = true;
	// (a tow of an earlier game in this session is over: its aircraft are shown on their stands again unless restored here)
	for (auto It = ADPParked::Towed.CreateIterator(); It; ++It) if (!Busy(W, *It)) It.RemoveCurrent();
	const FString Here = ADPBoards::HereIcao(); const FAirfieldData& D = AAirfield::Shared();
	for (FADPAircraft& A : Sim->World.Aircraft)
	{
		if (A.HangarIn.IsEmpty() || Busy(W, A.Reg)) continue;
		const FAirfieldData::FHangarSite* Hs = D.Hangars.FindByPredicate([&](const FAirfieldData::FHangarSite& H) { return H.Id == A.HangarIn; });
		if (A.Location != Here || !Hs || A.Status == TEXT("flying"))
		{ ADP_WARN(TEXT("HangarTow: %s was saved in hangar %s - %s; left out of it"), *A.Reg, *A.HangarIn, !Hs ? TEXT("no such hangar here") : TEXT("not at our airport")); A.HangarIn.Empty(); continue; }
		const FHangarSpec& Sp = FHangarSpec::Get(Hs->Size);
		const float Yn = Hs->Y + Sp.Lip() + Sp.TailIn();
		FJob J; J.Reg = A.Reg; J.Key = TEXT("TOW/") + A.Reg; J.Hangar = Hs->Id; J.Hx = Hs->X; J.Stand = A.Stand;
		if (const FAirfieldData::FStand* St = D.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == A.Stand; })) J.StandX = St->X;
		J.Nose = FVector2f(Hs->X, Yn); J.Main = FVector2f(Hs->X, Yn + GearL);   // (on its stop mark, tail in: where the tow in ends)
		ABabylonModel* B = W->SpawnActor<ABabylonModel>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (!B || !B->Place(TEXT("A320"))) { if (B) B->Destroy(); ADP_WARN(TEXT("HangarTow: %s - its model could not be placed in hangar %s"), *A.Reg, *J.Hangar); continue; }
		ADPParked::Towed.Add(A.Reg); ADPParked::Release(A.Reg);
		J.Model = B; J.Lamps = UADPAircraftLights::Fit(B, TEXT("A320")); if (J.Lamps.IsValid()) J.Lamps->SetParked(false);   // (cold and dark)
		for (TActorIterator<AHangar> It(W); It; ++It) if (FMath::Abs(It->GetActorLocation().X - J.Hx * 100.f) < 200.f) J.Hall = *It;
		J.Stage = 3; Jobs.Add(J); FJob& Jn = Jobs.Last(); Pose(Jn); Chock(Jn, true, true);
		const FTimespan Left = A.Status == TEXT("maintenance") ? A.CheckUntil - Sim->Now() : FTimespan::Zero();
		ADP_LOG(TEXT("HangarTow: %s in hangar %s again after loading - %s"), *A.Reg, *J.Hangar, A.Status == TEXT("maintenance") ? *FString::Printf(TEXT("its %s check has %.2f h to go"), *A.CheckFault, FMath::Max(0.0, Left.GetTotalHours())) : TEXT("its check is over, the tow back comes next"));
	}
}

void AHangarTow::Look()
{
	UWorld* W = GetWorld(); UADPSim* Sim = UADPSim::Get(W); if (!Sim) return;
	if (!bRestored && Sim->World.Aircraft.Num()) Restore();   // (once the career is loaded)
	const FString Here = ADPBoards::HereIcao();
	// the check over: the tow back (one tow on the move at a time)
	const bool bMoving = Jobs.ContainsByPredicate([](const FJob& J) { return J.Stage < 3; });
	for (FJob& J : Jobs)
		if (J.Stage == 3)
		{
			// (the brake rider down by the hall's own stair; the door shut once the stair is parked and its crew are out)
			if (AGSE* Gs = AGSE::Get(W))
			{
				Gs->MaintHall(J.Reg, J.Hangar, J.Model.Get(), true);
				if (AHangar* Hd = J.Hall.Get()) if (Hd->DoorOpen() > 0.999f && !Gs->HallBusy(J.Hangar)) { Hd->Command(false); ADP_LOG(TEXT("HangarTow: hangar %s - its stair parked, the door closes"), *J.Hangar); }
			}
			const FADPAircraft* A = Plane(W, J.Reg);
			if (A && A->Status == TEXT("maintenance") && !A->CheckKind.IsEmpty()) continue;
			if (!bMoving && Start(J, true)) return;
		}
	if (bMoving)
	{
		// (the other aircraft for the wingtip check, Clear: parked SIM/ and moving MOVE/ models; once per 2 s while a tow moves)
		ADP_PERF_SCOPE("HangarTow: the aircraft round a tow (every 2 s)", 2.f);
		Planes.Reset();
		for (TActorIterator<ABabylonModel> It(W); It; ++It)
			for (const FName& Tn : It->Tags) { const FString S = Tn.ToString(); if (S.StartsWith(TEXT("SIM/")) || S.StartsWith(TEXT("MOVE/"))) { Planes.Add(*It); break; } }
		return;
	}
	const FAirfieldData& D = AAirfield::Shared();
	for (const FADPAircraft& A : Sim->World.Aircraft)
	{
		if (A.Location != Here || A.Status != TEXT("maintenance") || A.CheckKind.IsEmpty() || Busy(W, A.Reg)) continue;   // (every check and fault: to-do 311)
		FString StandId; ABabylonModel* Shown = ADPParked::Shown(A.Reg, &StandId); if (!Shown) continue;
		const FAirfieldData::FStand* St = D.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == StandId; });
		static TSet<FString> Told;
		if (!St || !TowStand(*St, D)) { if (!Told.Contains(A.Reg)) { Told.Add(A.Reg); ADP_LOG(TEXT("HangarTow: %s on stand %s - no tow from there (no room for a tug or no taxilane behind it)"), *A.Reg, *StandId); } continue; }
		FJob J; J.Reg = A.Reg; J.Key = TEXT("TOW/") + A.Reg; J.Stand = StandId; J.StandX = St->X;
		const FAirfieldData::FStandFrame Fr = St->Frame(); J.Nose = FVector2f(Fr.X, Fr.Y); J.Main = Fr.P(0.f, GearL);
		if (!Start(J, false)) continue;
		// its own model from here (the parked one gives way in the same frame)
		ABabylonModel* B = W->SpawnActor<ABabylonModel>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (!B || !B->Place(TEXT("A320"))) { if (B) B->Destroy(); continue; }
		ADPParked::Towed.Add(A.Reg); ADPParked::Release(A.Reg);
		J.Model = B; J.Lamps = UADPAircraftLights::Fit(B, TEXT("A320")); if (J.Lamps.IsValid()) J.Lamps->SetParked(true);
		B->Tags.AddUnique(FName(*(StandId + TEXT("/A320"))));   // (on its stand until the tow begins: its jetway docks for the brake rider)
		Jobs.Add(J); Pose(Jobs.Last());
		ADP_LOG(TEXT("HangarTow: %s (%s check, %s, %.2f h) from stand %s to hangar %s"), *A.Reg, *A.CheckKind, *A.CheckFault, A.CheckLenH, *StandId, *J.Hangar);
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
		// (the hall chosen on the Maintenance page, if no other tow holds it; else the nearest free one)
		if (!A->HangarWant.IsEmpty())
		{
			Best = D.Hangars.FindByPredicate([&](const FAirfieldData::FHangarSite& H) { return H.Id == A->HangarWant && !Jobs.ContainsByPredicate([&](const FJob& O) { return O.Hangar == H.Id; }); });
			if (!Best) ADP_LOG(TEXT("HangarTow: %s - hangar %s chosen but not free: the nearest free one instead"), *J.Reg, *A->HangarWant);
		}
		if (!Best) for (const FAirfieldData::FHangarSite& H : D.Hangars)
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
		// (else the first free one of the same kind - terminal, remote or cargo - then any the tow reaches)
		const FAirfieldData::FStand* Was = D.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == A->Stand; });
		if (!Pick && Was) Pick = D.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.bCargo == Was->bCargo && S.bRemote == Was->bRemote && Free(S); });
		if (!Pick) Pick = D.Stands.FindByPredicate(Free);
		if (!Pick) { if ((J.WhyT -= 2.f) <= 0.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s stays in hangar %s - no stand free to tow it to"), *J.Reg, *J.Hangar); } return false; }
		J.Stand = Pick->Id; J.StandX = Pick->X; A->Stand = Pick->Id;
	}
	const FAirfieldData::FHangarSite* Hs = D.Hangars.FindByPredicate([&](const FAirfieldData::FHangarSite& H) { return H.Id == J.Hangar; });
	const FAirfieldData::FStand* St = D.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == J.Stand; });
	if (!Hs || !St) return false;
	const FHangarSpec& Sp = FHangarSpec::Get(Hs->Size);
	const FAirfieldData::FStandFrame Fr = St->Frame();
	const float Hx = Hs->X, Yn = Hs->Y + Sp.Lip() + Sp.TailIn(), Sx = St->X, Sy = Fr.Y, Ly = TowLaneY(*St, D), R = 25.f;
	// the way along the taxiway network (to-do 311): from the stand's join on its taxilane to the hangar's on K (and back),
	// the depot road's join on T for the hangar tug; a cargo stand's tug comes in beside the nose, clear of the wingtips
	const FVector2f StJoin(Sx, Ly), HJoin(Hx, D.KY), RoadJ(TowRoadX, D.TY), CgJoin(Sx + TowCargoSide, D.KY);
	FHtTwyNet Net; TowBuildNet(D, {StJoin, HJoin, RoadJ, CgJoin}, Net);
	TArray<FVector2f> Route; TArray<FString> Ids;
	if (!TowNetRoute(Net, Net.Near(StJoin), Net.Near(HJoin), Route, &Ids) || Route.Num() < 2)
	{
		if ((J.WhyT -= 2.f) <= 0.f) { J.WhyT = 30.f; ADP_WARN(TEXT("HangarTow: %s - no way along the taxiways from stand %s (%.0f, %.0f) to hangar %s (%.0f, %.0f)"), *J.Reg, *J.Stand, Sx, Ly, *J.Hangar, Hx, D.KY); }
		return false;
	}
	J.Via = TowSpoken(Ids);
	// the hangar tug's own ways: out from the depot road to the hangar (tow back), home from the hangar to it (tow out)
	TArray<FVector2f> TugIn, TugHome, TugCargo;
	if (TowNetRoute(Net, Net.Near(RoadJ), Net.Near(HJoin), TugIn)) TugIn.Insert(RoadJ, 0);
	if (TowNetRoute(Net, Net.Near(HJoin), Net.Near(RoadJ), TugHome)) TugHome.Add(RoadJ);   // (its last point is the stand's line, left out: TugTow.cpp HangarHome)
	if (St->bCargo && TowNetRoute(Net, Net.Near(RoadJ), Net.Near(CgJoin), TugCargo)) { TugCargo.Insert(RoadJ, 0); TugCargo.Add(FVector2f(Sx + TowCargoSide, TowCargoFront)); }
	// along K the hangar is passed, then the aircraft is pushed back round into it: which way it comes along K
	const FVector2f Pre = Route[Route.Num() - 2];
	const float E = FMath::Abs(Hx - Pre.X) > 1.f ? (Hx > Pre.X ? 1.f : -1.f) : 1.f;
	J.Legs.Reset(); J.Way.Reset(); J.Home.Reset();
	if (!bBack)
	{
		// pushed back tail first off the stand (every stand of ours is entered nose first: the push is straight back along
		// its centre line, -Dir in plan y, SimTraffic's pushback) and round onto the lane, the nose towards the way it goes
		const float Dx = FMath::Abs(Route[1].X - Sx) > 0.5f ? (Route[1].X > Sx ? 1.f : -1.f) : 1.f;
		const FAirfieldData::FTwy* L = TowLaneOf(*St, D);
		const float End = Dx > 0.f ? FMath::Min(L->Pts[0].X, L->Pts.Last().X) : FMath::Max(L->Pts[0].X, L->Pts.Last().X);
		const float Pd = FMath::Clamp((Sx - End) * Dx - 3.f, 16.f, 35.f);   // (never past the lane's end behind it)
		J.Nose = FVector2f(Sx, Sy); J.Main = Fr.P(0.f, GearL);
		J.Legs.Add(MakeLeg({J.Main, StJoin, FVector2f(Sx - Dx * Pd, Ly)}, true, 20.f));
		TArray<FVector2f> Pull = {FVector2f(Sx - Dx * (Pd - GearL), Ly)}; Pull.Append(Route); Pull.Add(FVector2f(Hx + E * (R + GearL + 5.f), D.KY));
		J.Legs.Add(MakeLeg(Pull, false, R));                                                // pulled along the taxiways past the hangar
		J.Legs.Add(MakeLeg({FVector2f(Hx + E * (R + 5.f), D.KY), HJoin, FVector2f(Hx, Yn + GearL)}, true, R));   // pushed tail first into the hall
		J.Home = TugHome;
	}
	else
	{
		// pulled nose first out of the hall, along the taxiways and nose-in onto the stand, as every stand of ours is entered
		TArray<FVector2f> Pull = {FVector2f(Hx, Yn)}; for (int32 i = Route.Num() - 1; i >= 0; --i) Pull.Add(Route[i]);
		Pull.Add(FVector2f(Sx, Sy));
		J.Legs.Add(MakeLeg(Pull, false, R));
		J.Way = TugIn;
		if (St->bCargo && TugCargo.Num())   // (no apron way there: home the way it came, beside the nose and along K)
		{
			for (int32 i = TugCargo.Num() - 1; i >= 1; --i) J.Home.Add(TugCargo[i]);
			J.Home.Add(RoadJ);
		}
	}
	if (!bBack && St->bCargo) J.Way = TugCargo;   // (the hangar tug to a cargo stand: along the taxiways to beside its nose, TugTow.cpp HangarOut)
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
	J.bRider = false; J.Rider = 0;   // (the tow back: he climbs aboard by the hall's stair first, Run)
	if (bBack) ADP_LOG(TEXT("HangarTow: %s check done - towed back from hangar %s to stand %s"), *J.Reg, *J.Hangar, *J.Stand);
	return true;
}

bool AHangarTow::Clear(const FJob& J, const FVector2f& Nose, const FVector2f& Main, FString& Who) const
{
	FVector2f Me[6]; TowShape(Nose, (Nose - Main).GetSafeNormal(), Me);
	for (const TWeakObjectPtr<AActor>& Pw : Planes)
	{
		const AActor* P = Pw.Get(); if (!P || P == J.Model.Get()) continue;
		const FVector L = P->GetActorLocation(), Nw = -P->GetActorRightVector();
		const FVector2f N(float(L.X / 100.0), float(-L.Y / 100.0)); FVector2f Fa(float(Nw.X), float(-Nw.Y)); Fa = Fa.GetSafeNormal();
		if (Fa.IsNearlyZero() || FVector2f::Distance(N, Nose) > 90.f) continue;
		FVector2f It[6]; TowShape(N, Fa, It);
		if (const float Net = TowGap(Me, It); Net < 3.f) { Who = FString::Printf(TEXT("%s at (%.0f, %.0f), %.1f m off"), P->Tags.Num() ? *P->Tags[0].ToString() : *P->GetName(), N.X, N.Y, FMath::Max(Net, 0.f)); return false; }
	}
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
	if (!J.bBack && !J.bPlanned && J.Stage <= 1)   // (the brake rider: called with the tug, in the cockpit before the brakes come off)
	{
		FString Rw; AGSE* Gs = AGSE::Get(W);
		const int32 Was = J.Rider; J.Rider = Gs ? Gs->MaintRider(J.StandX, J.Reg, B, Rw) : 0;
		if (J.Rider >= 2 && !J.bRider) J.bRider = true;
		if (J.Rider < 3 && J.Stage == 1 && (J.WhyT -= Dt) <= 0.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s waits for its brake rider - %s"), *J.Reg, *Rw); }
		else if (J.Rider != Was && J.Rider < 3) ADP_LOG(TEXT("HangarTow: %s - %s"), *J.Reg, *Rw);
	}
	if (J.bBack && !J.bPlanned && J.Stage <= 1)   // (the tow back: the brake rider up the hall's stair, the stair parked again, before the tug comes in)
	{
		FString Rw; AGSE* Gs = AGSE::Get(W);
		const int32 Was = J.Rider; J.Rider = Gs ? Gs->MaintBack(J.Reg, J.Hangar, B, Rw) : 3;
		if (J.Rider >= 3 && Was < 3) { J.bRider = Gs && Gs->MaintAboard(J.Reg); if (!J.bRider) ADP_WARN(TEXT("HangarTow: %s - %s: towed back without one"), *J.Reg, *Rw); }
		if (J.Rider < 3 && (J.WhyT -= Dt) <= 0.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s waits in hangar %s - %s"), *J.Reg, *J.Hangar, *Rw); }
		if (J.Rider < 3 && J.Stage == 0) return;
	}
	if (!J.bBack && J.Stage == 1 && J.Rider >= 3)   // (the hall's own stair called while the door is open: its crew walk in before it shuts)
		if (AGSE* Gs = AGSE::Get(W)) Gs->MaintHall(J.Reg, J.Hangar, B, false);
	if (J.Stage == 0)   // the tug and its crew
	{
		if ((J.Clock -= Dt) > 0.f) return;
		J.Clock = 1.f;
		const FString Ask = J.bBack ? FString::Printf(TEXT("hangar %s, request tow to stand %s."), *J.Hangar, *J.Stand) : FString::Printf(TEXT("stand %s, request tow to hangar %s."), *J.Stand, *J.Hangar);
		const FString Ok = FString::Printf(TEXT("tow approved via %s."), *J.Via);
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
		if (AHangar* Hd = J.Hall.Get()) if (J.bBack) Hd->Command(false);   // (in: shut once the hall's stair is parked again, Look)
		if (J.bBack)
		{
			ADPParked::Adopt(J.Stand, J.Reg, B); J.Stage = 9; ADP_LOG(TEXT("HangarTow: %s back on stand %s"), *J.Reg, *J.Stand);
			if (FADPAircraft* Ab = Plane(W, J.Reg)) Ab->HangarIn.Empty();   // (out of the hall: saved so)
			B->Tags.AddUnique(FName(*(J.Stand + TEXT("/A320"))));   // (its jetway docks for the brake rider to get off)
			if (AGSE* Gs = AGSE::Get(W)) Gs->MaintOff(J.StandX, J.Reg);
			if (UADPSim* Sim = UADPSim::Get(W)) Tg->ChocksOn(J.StandX, J.Reg, Sim->Now() + FTimespan::FromHours(2.0), true, true, false);   // (the stand's chocks in again)
		}
		else
		{
			J.Stage = 3; if (FADPAircraft* Ai = Plane(GetWorld(), J.Reg)) Ai->HangarWant.Empty(); ADP_LOG(TEXT("HangarTow: %s in hangar %s, door closing - its check goes on"), *J.Reg, *J.Hangar);
			if (FADPAircraft* Ai = Plane(W, J.Reg)) Ai->HangarIn = J.Hangar;   // (in the hall: saved so, there again after loading - Restore)
		}
		return;
	}
	// stage 1: hooked on and moving; the door open before the hall is entered or left
	const AHangar* Hg = J.Hall.Get();
	const bool bHall = J.bBack ? J.Leg == 0 && J.S < 60.f : J.Leg == 2;
	const bool bDoor = !bHall || !Hg || Hg->DoorOpen() > 0.999f;
	const bool bGo = Tg->Push(J.Key, J.StandX, NoseW + Fwd * 60.f, Fwd) && J.Rider >= 3;
	if (bGo && !J.bPlanned)
	{
		J.bPlanned = true; B->Tags.AddUnique(FName(*(TEXT("MOVE/") + J.Reg))); B->Tags.Remove(FName(*(J.Stand + TEXT("/A320"))));
		ADP_LOG(TEXT("HangarTow: %s hooked on, brakes off - the tow begins"), *J.Reg);
	}
	if (UADPAircraftLights* Lm = J.Lamps.Get())
	{
		if (!J.bRider) Lm->SetParked(true);
		else { FADPLightState Ls; Ls.bPowered = true; Ls.bBeacon = true; Ls.bNav = ADPAircraftLights::Night(W); Lm->Set(Ls); }   // (towed: the beacon, nav lights at night; no strobes, landing or taxi lights)
	}
	// the tow back: the hangar's chocks away first (by the tow crew, once the tug is on the nose)
	if (J.bBack && J.Chocks == 2 && Tg->TowHooked(J.Key) && !Chock(J, false) && (J.WhyT -= Dt) <= 0.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s waits - nobody of the tow crew free to take its chocks away"), *J.Reg); }
	const bool bChocked = J.bBack && J.Chocks != 0;
	const float Lag = bGo ? Tg->TowWalkerLag(J.Key) : 0.f;   // (the wing walkers keep pace: walking, never far from their wingtips)
	// your own tow (to-do 311 step C): you steer it at the hangar tug's wheel, the same limits; left to the crew, they go on from where it stands
	if (Tg->YouPushing(J.Key)) { J.bYou = true; YouRun(J, Dt, bGo, bChocked, Lag); return; }
	if (J.bYou && bGo && !Rejoin(J))
	{
		J.V = 0.f;
		if ((J.WhyT -= Dt) <= 0.f) { J.WhyT = 30.f; ADP_WARN(TEXT("HangarTow: %s - its crew can't take your tow over from (%.0f, %.0f): too far off its route"), *J.Reg, J.Nose.X, J.Nose.Y); }
		return;
	}
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
			if (ASimTraffic::RunwayBusy(W) || (AGSE::Find(W) && AGSE::Find(W)->VanOnStrip()))
			{
				J.HoldT = 0.f;
				if ((J.WhyT -= Dt) <= 0.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s holds short of the runway - traffic"), *J.Reg); }
			}
			else if ((J.HoldT += Dt) >= 10.f) { J.bCleared = true; ADP_LOG(TEXT("HangarTow: %s cleared to cross the runway"), *J.Reg); }
			J.V = 0.f; return;
		}
	}
	{
		// wingtip clearance (to-do 311): 3 m from every other aircraft, parked or moving, a few metres ahead; else it stops and says why
		const FVector2f Ld = LegAt(Lg, J.S + 3.f), Tr = LegAt(Lg, J.S + 3.f - GearL);
		FString Who;
		if (!Clear(J, Lg.bPush ? Tr : Ld, Lg.bPush ? Ld : Tr, Who))
		{
			J.V = 0.f;
			if ((J.BlockT -= Dt) <= 0.f) { J.BlockT = 30.f; ADP_WARN(TEXT("HangarTow: %s stops - not clear of another aircraft: %s"), *J.Reg, *Who); }
			return;
		}
		J.BlockT = 0.f;
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
	Finish(J);
}

void AHangarTow::Finish(FJob& J)
{
	// on its mark: the brakes set, unhooked
	ATugs* Tg = ATugs::Get(GetWorld()); ABabylonModel* B = J.Model.Get(); if (!Tg || !B) return;
	Tg->Release(J.Key); J.Stage = 2; J.Clock = 0.f; J.V = 0.f; B->Tags.Remove(FName(*(TEXT("MOVE/") + J.Reg)));
	if (UADPAircraftLights* Lm = J.Lamps.Get()) Lm->SetParked(J.bBack);   // (in the hall: cold and dark)
	ADP_LOG(TEXT("HangarTow: %s on its mark (%.1f, %.1f)%s - unhooking"), *J.Reg, J.Nose.X, J.Nose.Y, J.bYou ? TEXT(" by you") : TEXT(""));
}

void AHangarTow::Mark(const FJob& J, FVector2f& Nose, FVector2f& Dir) const
{
	// (the stop mark: where the crew's tow ends - the last leg's end, the nose tyre on it)
	Nose = J.Nose; Dir = (J.Nose - J.Main).GetSafeNormal(); if (!J.Legs.Num()) return;
	const FLeg& L = J.Legs.Last(); const float Len = L.S.Num() ? L.S.Last() : 0.f;
	const FVector2f Lead = LegAt(L, Len), Trail = LegAt(L, Len - GearL);
	Nose = L.bPush ? Trail : Lead; Dir = (L.bPush ? Trail - Lead : Lead - Trail).GetSafeNormal();
}

bool AHangarTow::YouFit(UWorld* W, const FVector& At, FString& Key, FVector2f& Nose, FVector2f& Main)
{
	if (!W) return false;
	const FJob* Best = nullptr; float Bd = FMath::Square(4000.f);
	for (TActorIterator<AHangarTow> It(W); It; ++It)
		for (const FJob& J : It->Jobs)
		{
			if (!J.Model.IsValid() || J.Stage > 1) continue;
			if (!Key.IsEmpty()) { if (J.Key == Key) Best = &J; continue; }
			if (J.Stage != 0) continue;   // (the crew's own tow already under way)
			if (const float D2 = FVector::DistSquared2D(WorldAt(J.Nose), At); D2 < Bd) { Bd = D2; Best = &J; }
		}
	if (!Best) return false;
	Key = Best->Key; Nose = Best->Nose; Main = Best->Main;
	return true;
}

bool AHangarTow::YouEnd(UWorld* W, const FString& Key, FString& Hint)
{
	if (!W) return false;
	for (TActorIterator<AHangarTow> It(W); It; ++It)
		for (FJob& J : It->Jobs)
		{
			if (J.Key != Key || J.Stage != 1) continue;
			FVector2f Mn, Md; It->Mark(J, Mn, Md);
			const FVector2f Off = J.Nose - Mn, Dn = (J.Nose - J.Main).GetSafeNormal();
			const float Along = FVector2f::DotProduct(Off, Md), Aside = FMath::Abs(Md.X * Off.Y - Md.Y * Off.X);
			const float Turn = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2f::DotProduct(Dn, Md), -1.f, 1.f)));
			if (Off.Size() <= 0.5f && Turn <= 3.f) { ADP_LOG(TEXT("HangarTow: %s - you stop on the mark (%.2f m, %.1f deg off)"), *J.Reg, Off.Size(), Turn); It->Finish(J); return true; }
			Hint = Off.Size() > 30.f ? FString::Printf(TEXT("Not on the stop mark - it is %.0f m away"), Off.Size())
				: FString::Printf(TEXT("Not on the stop mark: the nose wheel %.1f m %s, %.1f m aside, %.0f deg turned (0.5 m, 3 deg)"), FMath::Abs(Along), Along < 0.f ? TEXT("short") : TEXT("past"), Aside, Turn);
			return false;
		}
	Hint = TEXT("No hangar tow of yours here");
	return false;
}

bool AHangarTow::Rejoin(FJob& J)
{
	// (your tow taken over by the crew: from where it stands onto its route - the nearest point of a leg ahead of the leading
	//  gear, the way it faces; that leg goes on from there, the legs before it dropped)
	int32 Bl = INDEX_NONE; float Bs = 0.f, Bd = 40.f;
	for (int32 l = J.Leg; l < J.Legs.Num(); ++l)
	{
		const FLeg& L = J.Legs[l]; const float Len = L.S.Num() ? L.S.Last() : 0.f;
		const FVector2f Lead = L.bPush ? J.Main : J.Nose, Dl = (L.bPush ? J.Main - J.Nose : J.Nose - J.Main).GetSafeNormal();
		for (float S = 0.f; S <= Len; S += 1.f)
		{
			const FVector2f P = LegAt(L, S), Dp = (LegAt(L, S + 1.f) - P).GetSafeNormal();
			if (const float Dd = FVector2f::Distance(P, Lead); Dd < Bd && FVector2f::DotProduct(Dp, Dl) > 0.7f) { Bd = Dd; Bl = l; Bs = S; }
		}
	}
	if (Bl == INDEX_NONE) return false;
	const FLeg Was = J.Legs[Bl];
	const float Len = Was.S.Last(), To = FMath::Min(Bs + 15.f, Len);
	FLeg Nw; Nw.bPush = Was.bPush;
	Nw.P = {Was.bPush ? J.Nose : J.Main, Was.bPush ? J.Main : J.Nose};
	if (FVector2f::Distance(LegAt(Was, To), Nw.P.Last()) > 0.5f) Nw.P.Add(LegAt(Was, To));
	for (int32 i = 0; i < Was.P.Num(); ++i) if (Was.S[i] > To + 0.5f) Nw.P.Add(Was.P[i]);
	Nw.S.Add(0.f); for (int32 i = 1; i < Nw.P.Num(); ++i) Nw.S.Add(Nw.S.Last() + FVector2f::Distance(Nw.P[i - 1], Nw.P[i]));
	const FAirfieldData& D = AAirfield::Shared(); const float Nl = Nw.S.Last();
	Nw.HoldS = -1.f;
	if (!InStrip(D, LegAt(Nw, Nw.S[1]))) for (float S = Nw.S[1]; S <= Nl; S += 0.5f) if (InStrip(D, LegAt(Nw, S))) { Nw.HoldS = FMath::Max(0.f, S - 0.5f); break; }
	J.Legs[Bl] = Nw; J.Leg = Bl; J.S = Nw.S[1]; J.V = 0.f; J.HoldT = 0.f;
	J.bCleared = InStrip(D, J.Nose) || InStrip(D, J.Main);   // (on the runway already: it goes on across)
	J.bYou = false;
	ADP_LOG(TEXT("HangarTow: %s - its crew take your tow over at (%.0f, %.0f), %.0f m from its route, leg %d of %d"), *J.Reg, J.Nose.X, J.Nose.Y, Bd, Bl + 1, J.Legs.Num());
	return true;
}

void AHangarTow::YouRun(FJob& J, float Dt, bool bGo, bool bChocked, float Lag)
{
	UWorld* W = GetWorld(); ATugs* Tg = ATugs::Get(W); ABabylonModel* B = J.Model.Get(); if (!Tg || !B) return;
	const FAirfieldData& D = AAirfield::Shared();
	FString Why, Say; bool bMay = bGo; float Cap = 0.f;
	if (!bGo) Why = J.Rider < 3 ? TEXT("the brake rider is not in the cockpit yet") : TEXT("waiting for ATC's tow approval");
	const FVector2f Dn = (J.Nose - J.Main).GetSafeNormal();
	// the hangar door: opened as you reach its apron; nothing of the aircraft into or out of the hall before it is fully open
	AHangar* Hg = J.Hall.Get();
	if (const FAirfieldData::FHangarSite* Hs = D.Hangars.FindByPredicate([&](const FAirfieldData::FHangarSite& H) { return H.Id == J.Hangar; }); Hg && Hs)
	{
		const FHangarSpec& Sp = FHangarSpec::Get(Hs->Size);
		const float Door = Hs->Y + Sp.Lip(), Half = Sp.W * 0.5f;
		FVector2f Sh[6]; TowShape(J.Nose, Dn, Sh);
		bool bNear = false, bInside = false;
		for (const FVector2f& P : {Sh[0], Sh[1], Sh[2], Sh[3], Sh[4], Sh[5], J.Nose, J.Main})
		{
			if (FMath::Abs(P.X - Hs->X) < Half + 40.f && P.Y > Door - 60.f) bNear = true;
			if (FMath::Abs(P.X - Hs->X) < Half + 2.f && P.Y > Door - 3.f) bInside = true;
		}
		if (bNear && Hg->DoorOpen() < 0.999f && (J.DoorT -= Dt) <= 0.f) { J.DoorT = 15.f; Hg->Command(true); }
		if (bInside && Hg->DoorOpen() < 0.999f) { bMay = false; Why = FString::Printf(TEXT("the hangar door is still opening (%.0f %%)"), Hg->DoorOpen() * 100.f); }
	}
	if (bChocked) { bMay = false; Why = TEXT("its hangar chocks are still under the wheels"); }
	if (Lag > 10.f) { bMay = false; Why = FString::Printf(TEXT("a wing walker is %.0f m from his wingtip"), Lag); }
	else if (Lag > 4.f) Cap = 2.5f / 3.6f * 100.f;   // (the walkers keep pace: 2.5 km/h)
	// the runway: held short 107.5 m from its centre line until nothing rolls, lands or takes off, then 10 s more (as the crew's)
	auto ByStrip = [&D](const FVector2f& P) { return FMath::Abs(P.Y - D.RwyY) < 113.5f && P.X > D.RwyX0 && P.X < D.RwyX1; };
	const bool bOn = InStrip(D, J.Nose) || InStrip(D, J.Main), bBy = ByStrip(J.Nose) || ByStrip(J.Main);
	if (!bBy) { J.bCleared = false; J.HoldT = 0.f; }
	else if (bOn) J.bCleared = true;
	else if (!J.bCleared && bGo)
	{
		bMay = false;
		if (ASimTraffic::RunwayBusy(W) || (AGSE::Find(W) && AGSE::Find(W)->VanOnStrip()))
		{
			J.HoldT = 0.f; Why = TEXT("hold short of the runway - traffic on it or close in");
			if ((J.WhyT -= Dt) <= 0.f) { J.WhyT = 30.f; ADP_LOG(TEXT("HangarTow: %s (yours) holds short of the runway - traffic"), *J.Reg); }
		}
		else if ((J.HoldT += J.V < 0.1f ? Dt : 0.f) >= 10.f) { J.bCleared = true; bMay = true; ADP_LOG(TEXT("HangarTow: %s (yours) cleared to cross the runway"), *J.Reg); AVehicleControls::Note(W, TEXT("Cleared to cross the runway")); }
		else Why = J.V < 0.1f ? FString::Printf(TEXT("holding short of the runway - cleared to cross in %.0f s"), 10.f - J.HoldT) : FString(TEXT("hold short of the runway - stop for the crossing clearance"));
	}
	// wingtips: 3 m from every other aircraft
	FString Who; const bool bWing = bGo && !Clear(J, J.Nose, J.Main, Who);
	if (bWing) { if (Why.IsEmpty()) Why = TEXT("a wingtip within 3 m of another aircraft - back off"); if ((J.BlockT -= Dt) <= 0.f) { J.BlockT = 30.f; ADP_WARN(TEXT("HangarTow: %s (yours) - not clear of another aircraft: %s"), *J.Reg, *Who); } }
	// the stop mark, at the wheel
	FVector2f Mn, Md; Mark(J, Mn, Md);
	const FVector2f Off = J.Nose - Mn; const float Along = FVector2f::DotProduct(Off, Md);
	if (Off.Size() < 30.f) Say = FString::Printf(TEXT("Stop mark: nose wheel %.1f m %s, %.1f m aside - stop on it, then E at the tow bar"), FMath::Abs(Along), Along < 0.f ? TEXT("to go") : TEXT("past"), FMath::Abs(Md.X * Off.Y - Md.Y * Off.X));
	else Say = FString::Printf(TEXT("Tow to %s %s via %s: %.0f m to the stop mark (8 km/h, 5 in turns)"), J.bBack ? TEXT("stand") : TEXT("hangar"), J.bBack ? *J.Stand : *J.Hangar, *J.Via, Off.Size());
	const FVector2f Was = J.Nose;
	Tg->YouTowFrame(J.Key, B, J.Nose, J.Main, bGo, bMay, Cap, bWing, Why, Say);
	J.V = Dt > 0.f ? FVector2f::Distance(Was, J.Nose) / Dt : 0.f;
	if (!J.Nose.Equals(Was, 0.0005f)) Pose(J);
}
