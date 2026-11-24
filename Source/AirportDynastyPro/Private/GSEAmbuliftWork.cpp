// The ambulift at work (to-do 212; the vehicle, its driving and its people are the shared ones of GSEService.cpp, its parts are
// GSEAmbulift.cpp): your driver and helper take it to the front right door (R1) for the passengers of a flight who cannot walk -
// about one flight in four has one or two (AGSE::AmbuPax). Legs down, the tail lift out, the driver up inside; the helper pushes the
// vehicle's own wheelchair to the ramp-level door nearest the stand (a ramp room's staff door), the passenger gets in behind it,
// unseen, and is pushed out and up the tail lift; body up, platform out, rails, gates, bridge, to the aircraft's door. An arriving
// flight the other way round. The aircraft is one mesh without a cabin (to-do 209): at its sill they are out of sight, as the
// caterers with their trolleys. The passenger is one of the passengers' people (AADPPeople), sat by the crew's own IK rig.
#include "GSEAmbulift.h"
#include "GSE.h"
#include "ServiceVehicles.h"
#include "StaffPeople.h"
#include "People.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"

using FA = FSvcAmbu;

namespace
{
	const FVector AwUp(0, 0, 1), AwY(0, 1, 0), AwX(1, 0, 0);
	const float AwFloor = 128.5f, AwPush = 78.f, AwCabinSeconds = 14.f, AwLow = -81.92f;
	const FVector AwBay[2] = {FVector(-160.f, 74.f, 1.f), FVector(-160.f, -74.f, 1.f)};
	const FVector AwInside(-345.f, 45.f, 1.f), AwFront(225.f, -35.f, 1.f), AwWaitH(150.f, 35.f, 1.f), AwAisle(-250.f, 0.f, 1.f), AwDeck(-455.f, 38.f, 2.f), AwRail(-504.f, 0.f, 2.f), AwBehind(-690.f, 0.f, 0.f), AwCtl(-395.f, 180.f, 0.f), AwRamp0(-625.f, 0.f, 0.f), AwRamp1(-572.f, 0.f, 2.f), AwChair(-480.f, 0.f, 2.f), AwSpin(-45.f, 0.f, 1.f);
	FVector AwRot(const FVector& P, const FVector& Ax, float Deg, const FVector& V) { return P + FQuat(Ax, FMath::DegreesToRadians(Deg)).RotateVector(V - P); }
	bool AwIs(const FSvcAmbu& C, int32 P) { return FMath::IsNearlyEqual(C.Now[P], C.Want[P], 0.01f); }
	float AwLen(const TArray<FVector>& P) { double L = 0.; for (int32 i = 1; i < P.Num(); ++i) L += FVector::Dist2D(P[i - 1], P[i]); return float(L); }
	void AwSmooth(TArray<FVector>& P)
	{
		for (int32 n = 0; n < 3 && P.Num() > 2; ++n)
		{
			TArray<FVector> Q; Q.Add(P[0]);
			for (int32 i = 0; i + 1 < P.Num(); ++i) { if (i) Q.Add(FMath::Lerp(P[i], P[i + 1], 0.25)); if (i + 2 < P.Num()) Q.Add(FMath::Lerp(P[i], P[i + 1], 0.75)); }
			Q.Add(P.Last()); P = MoveTemp(Q);
		}
	}
}

// ------------------------------------------------------------------ walking and pushing

FVector AGSE::AmbuL(const FSvcAmbu& C, const FVector& P)
{
	// (z is where: 0 the ground, 1 the body's floor, 2 the tail lift's deck; between them in between)
	const FVector G(P.X, P.Y, 0.), F(P.X, P.Y, AwFloor + C.Now[FA::Lift]), D = FVector(P.X, P.Y, AwFloor) + AmbuTip(C);
	if (P.Z >= 1.) return FMath::Lerp(F, D, FMath::Clamp(P.Z - 1., 0., 1.));
	return P.Z <= 0. ? G : FMath::Lerp(G, F, P.Z);
}

FVector AGSE::AmbuOn(int32 Ti, const TArray<FVector>& P, bool bLocal, float S) const
{
	// along the line (beyond its ends straight on, level); on the vehicle each end where it is now - the deck may be low
	const FTractor& T = Tractors[Ti]; auto W = [&](const FVector& X) { return bLocal ? FVector(TW(T, AmbuL(*T.Am, X))) : X; };
	for (int32 i = 1; i < P.Num(); ++i)
	{
		const float D = float(FVector::Dist2D(P[i - 1], P[i])); if (S > D && i < P.Num() - 1) { S -= D; continue; }
		const FVector A = W(P[i - 1]), B = W(P[i]); if (D < 0.01f) return B;
		const double K = S / D; FVector R = FMath::Lerp(A, B, K); if (K > 1.) R.Z = B.Z; if (K < 0.) R.Z = A.Z; return R;
	}
	return P.Num() ? W(P[0]) : FVector::ZeroVector;
}

FTransform AGSE::AmbuChairX(int32 Ti, int32 i) const { const FA::FChair& K = Tractors[Ti].Am->Chair[i]; return FTransform(FRotator(K.Pitch, K.Yaw, 0.f), K.At); }

void AGSE::AmbuRolls(FTractor& T, float Dt)
{
	// the chair ahead, its pusher behind; over the apron they stop for people, for you and for what drives by
	FSvcAmbu& C = *T.Am; const ATugs* G = Tg(); const int32 Ti = int32(&T - Tractors.GetData());
	for (TPair<int32, FA::FRoll>& R : C.Rolls)
	{
		FA::FRoll& X = R.Value; if (!X.bOn || X.P.Num() < 2) continue;
		const FVector Ps = AmbuOn(Ti, X.P, X.bLocal, X.S), D = (AmbuOn(Ti, X.P, X.bLocal, X.S + 40.f) - Ps).GetSafeNormal2D(); bool bStop = false;
		if (!X.bLocal)
		{
			const FVector Nose = Ps + D * (X.Chair >= 0 ? 55.f : 15.f);
			bStop = G && G->PersonAhead(Nose, D, 130.f);
			for (int32 o = 0; o < Tractors.Num() && !bStop; ++o)
			{
				const FTractor& V = Tractors[o]; if (o == Ti || !V.M.IsValid() || V.Speed < 1.f) continue;
				const FVector L = FTransform(FRotator(0.f, V.Yaw, 0.f), V.At).InverseTransformPosition(Nose + D * 150.f); bStop = FMath::Abs(L.X) < V.KHalfL + 250.f && FMath::Abs(L.Y) < V.KHalfW + 150.f;
			}
			// (through the ramp room's door only when it stands open; held open while they are in it)
			if (C.bRoom && C.Room != INDEX_NONE) { const FVector Dw = Plan(C.RoomX + 5.f, 82.f); const float Near = float(FMath::Min(FVector::Dist2D(Ps, Dw), FVector::Dist2D(Ps - D * AwPush, Dw))); if (Near < 330.f) if (ABabylonModel::StaffDoor(Dw, true) < 0.97f) bStop = true; }
		}
		const float Vm = X.Chair >= 0 ? (X.bLocal ? 60.f : 95.f) : (X.bLocal ? 85.f : 120.f), Want = bStop ? 0.f : FMath::Min(Vm, 18.f + (X.L - X.S) * 1.1f);
		X.V = FMath::FInterpConstantTo(X.V, Want, Dt, bStop ? 260.f : 110.f); X.S = FMath::Min(X.L, X.S + X.V * Dt);
		if (X.Chair >= 0)
		{
			FA::FChair& K = C.Chair[X.Chair]; const FVector H = AmbuOn(Ti, X.P, X.bLocal, X.S + 32.f) - AmbuOn(Ti, X.P, X.bLocal, X.S - 32.f);
			K.bBay = false; K.At = AmbuOn(Ti, X.P, X.bLocal, X.S); K.Yaw = float(H.Rotation().Yaw); K.Pitch = float(FMath::RadiansToDegrees(FMath::Atan2(H.Z, H.Size2D())));
		}
		if (X.S >= X.L - 0.1f) { X.bOn = false; X.V = 0.f; }
	}
}

void AGSE::AmbuGo(int32 Ci, int32 Ti, int32 Chair, TFunction<TArray<FVector>()> Line, bool bLocal)
{
	// walk a line (with a wheelchair: both hands on its push handles all the way; the line is the chair's)
	Do(Ci, [this, Ci, Ti, Chair, Line, bLocal]
	{
		FSvcAmbu* C = Tractors[Ti].Am.Get(); ATugs* G = Tg(); if (!C || !G) return;
		FA::FRoll X; X.P = Line(); X.bLocal = bLocal; X.Chair = Chair; X.L = AwLen(X.P); X.bOn = X.P.Num() > 1 && X.L > 1.f; C->Rolls.Add(Ci, X);
		G->Crew[Ci].bUp = true;
		G->Crew[Ci].Hold = [this, Ci, Ti](float, float PH, FDriverPose& P)
		{
			const FTractor& T = Tractors[Ti]; const FA::FRoll* R = T.Am ? T.Am->Rolls.Find(Ci) : nullptr; if (!R || R->P.Num() < 2) return;
			const float Bk = R->Chair >= 0 ? AwPush : 0.f, St = 46.f, S = R->S; const int32 n = FMath::FloorToInt(S / St); const float F = S / St - n, Sm = FMath::SmoothStep(0.1f, 0.9f, F);
			auto On = [&](float s) { return AmbuOn(Ti, R->P, R->bLocal, s); };
			auto Dir = [&](float s) { return (On(s + 32.f) - On(s - 32.f)).GetSafeNormal2D(); };
			// (a pusher walks where the chair's back points, on whatever is under his own feet)
			auto Mine = [&](float s) { if (Bk <= 0.f) return On(s); FVector B = On(s) - Dir(s) * Bk; B.Z = On(s - Bk).Z; return B; };
			const FVector Fd = Bk > 0.f ? Dir(S) : (On(S + 20.f) - On(S - 8.f)).GetSafeNormal2D(), Lt = ADPPose::LeftOf(Fd); P = FDriverPose(); P.Facing = Fd; P.Lean = R->V > 1.f ? 5.f : 2.f; float Lo = 1e9f;
			for (int32 k = 0; k < 2; ++k)
			{
				const bool bStand = ((n & 1) == 0) == (k == 0); const float Fs = (n + 0.5f) * St;
				P.Foot[k] = Mine(FMath::Clamp(bStand ? Fs : FMath::Lerp(Fs - St, Fs + St, Sm), 0.f, R->L)) + Lt * (k ? -10.f : 10.f); Lo = FMath::Min(Lo, float(P.Foot[k].Z)); if (!bStand && R->bOn && S < R->L - 2.f) P.Foot[k].Z += 6.f * FMath::Sin(PI * Sm);
			}
			P.Pelvis = Mine(S); P.Pelvis.Z = Lo + PH - 3.f * FMath::Sin(PI * F);
			if (R->Chair >= 0)
			{
				const FTransform K = AmbuChairX(Ti, R->Chair); P.bHand[0] = P.bHand[1] = true; P.Grip[0] = P.Grip[1] = 0.95f; P.Lean += 5.f;
				P.Hand[0] = K.TransformPosition(SvcP(T, TEXT("WC_IK_Push_L"))); P.Hand[1] = K.TransformPosition(SvcP(T, TEXT("WC_IK_Push_R")));
			}
		};
	});
	WaitFor(Ci, [this, Ci, Ti](float E) { const FSvcAmbu* C = Tractors[Ti].Am.Get(); const FA::FRoll* X = C ? C->Rolls.Find(Ci) : nullptr; return !X || !X->bOn || E > 900.f; });
}

void AGSE::AmbuHold(int32 Ci, int32 Ti, FVector At, FVector Face)
{
	if (ATugs* G = Tg()) { G->Crew[Ci].bUp = true; G->Crew[Ci].Hold = [this, Ti, At, Face](float, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Standing(TW(T, AmbuL(*T.Am, At)), FRotator(0.f, T.Yaw, 0.f).RotateVector(Face), PH); }; }
}

void AGSE::AmbuHand(int32 Ci, int32 Ti, FVector At, TFunction<FVector()> To, TFunction<void()> Fn, TFunction<bool()> Is, bool bBoth, float MaxT)
{
	// a hand (both) from where he stands to a part that may move; what it does begun when it is there and held until it is so
	auto Ps = [this, Ti, At, To, bBoth](float E, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const FVector A = TW(T, AmbuL(*T.Am, At)), H = To(); FVector Fc = (H - A).GetSafeNormal2D(); if (Fc.IsNearlyZero()) Fc = FRotator(0.f, T.Yaw, 0.f).Vector();
		const FVector Lt = ADPPose::LeftOf(Fc) * 15.f; if (bBoth) { P = ADPPose::Both(A, Fc, H + Lt, H - Lt, 0.9f, PH, E); } else { P = ADPPose::Reach(A, Fc, 1, H, 0.8f, PH, E); }
		const float Low = FMath::Clamp((float(A.Z) + PH + 10.f - float(H.Z)) / 90.f, 0.f, 1.f) * E; P.Pelvis -= AwUp * (PH * 0.28f * Low) + Fc * (10.f * Low); P.Lean += 24.f * Low;
	};
	PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(FMath::SmoothStep(0.f, 1.f, K), PH, P); }, 0.8f);
	Do(Ci, [this, Ci, Fn, Ps] { if (Fn) Fn(); if (ATugs* G = Tg()) G->Crew[Ci].Hold = [Ps](float, float PH, FDriverPose& P) { Ps(1.f, PH, P); }; });
	WaitFor(Ci, [Is, MaxT](float E) { return E > 0.4f && (!Is || Is() || E > MaxT); });
	PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(1.f - FMath::SmoothStep(0.f, 1.f, K), PH, P); }, 0.6f);
	Do(Ci, [this, Ci, Ti, At, To] { const FTractor& T = Tractors[Ti]; AmbuHold(Ci, Ti, At, FRotator(0.f, -T.Yaw, 0.f).RotateVector((To() - TW(T, AmbuL(*T.Am, At))).GetSafeNormal2D())); });
}

bool AGSE::AmbuRoute(const FVector& From, const FVector& To, TArray<FVector>& Out) const
{
	// over the apron with the chair: the shortest free way (a grid of 60 cm; what stands there and the vehicles as they are now)
	const float Cell = 60.f; const FVector2D A(From), B(To); const FBox2D Bx(FVector2D::Min(A, B) - FVector2D(1500., 1500.), FVector2D::Max(A, B) + FVector2D(1500., 1500.));
	const int32 W = FMath::CeilToInt(Bx.GetSize().X / Cell), H = FMath::CeilToInt(Bx.GetSize().Y / Cell); if (W * H > 40000) return false;
	TArray<FOutline> Ol; Outlines(Ol); TArray<int8> Free; Free.Init(-1, W * H);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPAmbuRoute), false); if (const ATugs* G = Tg()) for (const ATugs::FCrew& K : G->Crew) if (K.A.IsValid()) Q.AddIgnoredActor(K.A.Get());
	const double North = FMath::Min(-From.Y, -To.Y) - 60.;   // (never nearer the terminal's wall than its door)
	auto At = [&](int32 i) { return FVector(Bx.Min.X + (i % W + 0.5) * Cell, Bx.Min.Y + (i / W + 0.5) * Cell, From.Z); };
	auto Ok = [&](int32 i)
	{
		if (Free[i] >= 0) return Free[i] == 1;
		const FVector P = At(i); bool b = -P.Y > North;
		for (const FOutline& O : Ol) if (b) { const FVector D = P - O.C; b = FMath::Abs(FVector::DotProduct(D, O.X)) > O.Hx + 75.f || FMath::Abs(FVector::DotProduct(D, O.Y)) > O.Hy + 75.f; }
		if (b) b = !GetWorld()->OverlapBlockingTestByChannel(P + AwUp * 100.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(55.f, 75.f), Q);
		Free[i] = b ? 1 : 0; return b;
	};
	auto Ix = [&](const FVector& P) { return FMath::Clamp(FMath::FloorToInt(float((P.X - Bx.Min.X) / Cell)), 0, W - 1) + W * FMath::Clamp(FMath::FloorToInt(float((P.Y - Bx.Min.Y) / Cell)), 0, H - 1); };
	const int32 S = Ix(From), E = Ix(To); Free[S] = Free[E] = 1;
	typedef TPair<float, int32> FN; auto Less = [](const FN& X, const FN& Y) { return X.Key < Y.Key; };
	TArray<float> Gs; Gs.Init(1e9f, W * H); TArray<int32> Pv; Pv.Init(-1, W * H); TArray<FN> Open; const FVector Ep = At(E);
	Gs[S] = 0.f; Open.HeapPush(FN(0.f, S), Less); bool bFound = false;
	while (Open.Num())
	{
		FN Nx; Open.HeapPop(Nx, Less); const int32 i = Nx.Value; if (i == E) { bFound = true; break; }
		for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx)
		{
			const int32 x = i % W + dx, y = i / W + dy; if ((!dx && !dy) || x < 0 || y < 0 || x >= W || y >= H) continue;
			const int32 j = x + y * W; const float Gn = Gs[i] + Cell * (dx && dy ? 1.414f : 1.f); if (Gn >= Gs[j] || !Ok(j) || (dx && dy && (!Ok(i + dx) || !Ok(i + dy * W)))) continue;
			Gs[j] = Gn; Pv[j] = i; Open.HeapPush(FN(Gn + float(FVector::Dist2D(At(j), Ep)), j), Less);
		}
	}
	if (!bFound) return false;
	TArray<FVector> P; for (int32 i = E; i >= 0; i = Pv[i]) P.Insert(At(i), 0);
	P[0] = From; P.Last() = To;
	auto Sees = [&](const FVector& X, const FVector& Y) { const int32 n = FMath::CeilToInt(float(FVector::Dist2D(X, Y)) / 30.f); for (int32 k = 0; k <= n; ++k) if (!Ok(Ix(FMath::Lerp(X, Y, double(k) / FMath::Max(1, n))))) return false; return true; };
	Out.Reset(); Out.Add(P[0]); for (int32 i = 0; i + 1 < P.Num();) { int32 j = P.Num() - 1; while (j > i + 1 && !Sees(P[i], P[j])) --j; Out.Add(P[j]); i = j; }
	return true;
}

// ------------------------------------------------------------------ the passengers

int32 AGSE::AmbuPax(const FString& Flight, const FDateTime& Day, int32 Pax)
{
	// about one flight in four has somebody who cannot walk aboard, one in five of those two of them: always the same for a flight of a day
	if (Flight.IsEmpty() || Pax < 1) return 0;
	const uint32 H = FCrc::StrCrc32(*FString::Printf(TEXT("%s/%d"), *Flight, Day.GetYear() * 400 + Day.GetDayOfYear())), R = (H ^ (H >> 13)) % 100;
	return R < 5 && Pax > 1 ? 2 : R < 25 ? 1 : 0;
}

bool AGSE::AmbuSeat(int32 Ti, int32 i)
{
	// one of the passengers' people, sat in wheelchair i, out of sight (false: not loaded yet)
	FA::FChair& K = Tractors[Ti].Am->Chair[i]; AADPPeople* Pp = AADPPeople::Get(GetWorld()); ATugs* G = Tg(); if (!Pp || !G || K.Who.IsValid()) return true;
	if (K.Face.IsEmpty()) { K.Face = Pp->PickBuilt(TEXT("passenger")); if (K.Face.IsEmpty()) return true; }
	if (!K.bAsked) { K.bAsked = true; K.Load = UAssetManager::GetStreamableManager().RequestAsyncLoad(FSoftObjectPath(FString::Printf(TEXT("/Game/People/Built/%s/BP_%s.BP_%s_C"), *K.Face, *K.Face, *K.Face))); }
	if (K.Load.IsValid() && !K.Load->HasLoadCompleted()) return false;
	AActor* A = Pp->Spawn(K.Face, K.At, K.Yaw); if (!A) return true;
	A->SetActorHiddenInGame(K.bHide); K.Who = A; K.Pax = G->Enlist(A, FString(), TEXT("wheelchair passenger")); if (K.Pax == INDEX_NONE) return true;
	G->Crew[K.Pax].bGse = true; G->Crew[K.Pax].bUp = true;
	G->Crew[K.Pax].Hold = [this, Ti, i](float, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; const FTransform X = AmbuChairX(Ti, i); auto At = [&](const TCHAR* N) { return X.TransformPosition(SvcP(T, N)); };
		P = FDriverPose(); P.Facing = X.GetRotation().GetForwardVector().GetSafeNormal2D(); P.Lean = -4.f; P.Pelvis = At(TEXT("WC_IK_Seat")) + AwUp * 10.f;
		P.Foot[0] = At(TEXT("WC_IK_Foot_L")); P.Foot[1] = At(TEXT("WC_IK_Foot_R")); P.bHand[0] = P.bHand[1] = true; P.Grip[0] = P.Grip[1] = 0.25f; P.Hand[0] = At(TEXT("WC_IK_Arm_L")) + AwUp * 3.f; P.Hand[1] = At(TEXT("WC_IK_Arm_R")) + AwUp * 3.f;
	};
	return true;
}

void AGSE::AmbuUnseat(int32 Ti, int32 i)
{
	FA::FChair& K = Tractors[Ti].Am->Chair[i]; if (ATugs* G = Tg()) if (K.Pax != INDEX_NONE && G->Crew[K.Pax].A == K.Who) G->Discharge(K.Pax);
	if (AActor* A = K.Who.Get()) A->Destroy();
	K.Who = nullptr; K.Pax = INDEX_NONE; K.Face.Reset(); K.Load.Reset(); K.bAsked = false;
}

// ------------------------------------------------------------------ the call

bool AGSE::AmbuIs(const FSvcJob& J) const { return ADPService::All()[J.Svc].bAmbu; }

bool AGSE::AmbuCall(float StandX, const FString& Reg, const FString& Flight, const FDateTime& When, int32 N, bool bArr, bool bGo)
{
	const FString Key = FString::Printf(TEXT("%s/%s/%d"), *Flight, bArr ? TEXT("in") : TEXT("out"), When.GetDayOfYear()); if (N < 1 || AmbuDone.Contains(Key)) return true;
	for (TPair<int32, FSvcJob>& P : SvcJobs) if (AmbuIs(P.Value) && P.Value.Reg == Reg && P.Value.Phase < 9) { if ((P.Value.CtDoor == 1) != bArr) return false; P.Value.bGo |= bGo; return P.Value.Phase > 0; }   // (the arriving ones first)
	if (!SvcFor(TEXT("ambulift"), StandX)) return true;
	{ const UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); float X0 = 0.f; if (!Sp || Sp->RampNear(StandX - 13.f, X0) == INDEX_NONE) return true; }   // (no ramp-level door to come out of: none of them in 3D)
	const int32 Job = SvcNext; SvcAsk(TEXT("ambulift"), StandX, Reg, Flight, When);
	if (FSvcJob* J = SvcJ(Job)) { J->CtDoor = bArr ? 1 : 0; J->Rows = N; J->bGo = bGo; AmbuFlown.Add(Job, Key); ADP_LOG(TEXT("GSE: %s %s %d passenger%s in a wheelchair - the ambulift is called"), *Flight, bArr ? TEXT("brings") : TEXT("takes"), N, N > 1 ? TEXT("s") : TEXT("")); }
	return false;
}

bool AGSE::AmbuBusy(float StandX, bool bArr, FString& Why, bool bMayWait) const
{
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		const FSvcJob& J = P.Value; if (!AmbuIs(J) || FMath::Abs(J.StandX - StandX) > 1.f || (J.CtDoor == 1) != bArr || J.Phase >= 3 || J.CtDone || J.T > 2700.f) continue;
		if (bMayWait && J.Phase < 2 && CaterAtFront(StandX)) continue;   // (the caterers were first: they stay seated until the truck is gone)
		Why = J.Phase == 0 ? FString::Printf(TEXT("the ambulift waits: %s"), J.Why.IsEmpty() ? TEXT("its people are being called") : *J.Why) : J.Phase == 1 ? FString(CaterAtFront(StandX) ? TEXT("the ambulift waits for the catering truck") : TEXT("the ambulift is on its way")) : FString::Printf(TEXT("the passengers in wheelchairs %s by the ambulift (%d of %d)"), bArr ? TEXT("get off") : TEXT("board"), J.RowsDone, J.Rows);
		return true;
	}
	return false;
}

bool AGSE::CaterAtFront(float StandX) const
{
	for (const TPair<int32, FSvcJob>& P : SvcJobs) { const FSvcJob& J = P.Value; if (ADPService::All()[J.Svc].bCater && FMath::Abs(J.StandX - StandX) < 1.f && J.CtDoor == 0 && (J.Phase == 2 || (J.Phase == 1 && Tractors.IsValidIndex(J.Ti) && Tractors[J.Ti].Job == EJob::Out))) return true; }
	return false;
}

bool AGSE::AmbuAtFront(float StandX, bool bSoon) const
{
	// the front right door is the ambulift's (bSoon: or called for an arriving flight and manned)
	for (const TPair<int32, FSvcJob>& P : SvcJobs)
	{
		const FSvcJob& J = P.Value; if (!AmbuIs(J) || FMath::Abs(J.StandX - StandX) > 1.f || !Tractors.IsValidIndex(J.Ti)) continue;
		const EJob Jb = Tractors[J.Ti].Job; if ((J.Phase == 1 && (Jb == EJob::Out || (bSoon && J.CtDoor == 1))) || J.Phase == 2 || (J.Phase == 3 && Jb == EJob::AtStand)) return true;
	}
	return false;
}

void AGSE::AmbuArrive(int32 Job)
{
	FSvcJob* J = SvcJ(Job); if (!J || !Tractors[J->Ti].Am) return;
	FTractor& T = Tractors[J->Ti]; FSvcAmbu& C = *T.Am; FVector2f At, Hd; float Sz = 0.f; CaterDock(*J, 0, At, Hd, Sz);
	C.bLive = true; C.Sill = FMath::Clamp(Sz - float(T.At.Z), 235.f, 418.f); C.bArr = J->CtDoor == 1; C.N = FMath::Clamp(J->Rows, 1, 2); J->Stage = 0; J->RowsDone = 0; T.bUsed = true;
	const float Off = float(FVector::Dist2D(T.At, Plan(At.X, At.Y))); if (Off > 20.f) ADP_WARN(TEXT("GSE: the %s stands %.0f cm off the door of %s"), *T.Name, Off, *J->Reg);
	ADP_LOG(TEXT("GSE: the %s at %s's front right door, sill %.0f cm over its ground"), *T.Name, *J->Reg, C.Sill);
}

// ------------------------------------------------------------------ the work

void AGSE::AmbuStage(int32 Job)
{
	// 0 out of the cab, 1 legs, 2 the tail lift out, 3 the driver up, 4 the helper up and (departures) the trips to the terminal,
	// 5 the tail in, 6 up, 7 the platform out, 8 the trips to the aircraft, 9 the platform in, 10 down, 11 the tail out again,
	// 12 (arrivals) the trips to the terminal, the helper down, 13 the driver down, 14 the tail in, 15 legs, 16 away
	FSvcJob* J = SvcJ(Job); if (!J || J->Phase != 2 || J->Men.Num() < 2 || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Am) return;
	const int32 Ti = J->Ti, St = J->Stage, C0 = J->Men[0].Ci, C1 = J->Men[1].Ci; const bool bArr = Tractors[Ti].Am->bArr; const int32 N = Tractors[Ti].Am->N; Tractors[Ti].Am->bLive = true;
	auto Am = [this, Ti]() -> FSvcAmbu& { return *Tractors[Ti].Am; };
	auto Next = [this, Job](int32 Ci, int32 n) { Do(Ci, [this, Job, n] { if (FSvcJob* X = SvcJ(Job)) X->Stage = n; Later([this, Job] { AmbuStage(Job); }); }); Wait(Ci, 0.4f); };
	// (the other one waits where he is until the next stage)
	auto End = [this, Job, St, C0, C1, Next](int32 Ci, int32 n) { Next(Ci, n); WaitFor(Ci == C0 ? C1 : C0, [this, Job, St](float) { const FSvcJob* X = SvcJ(Job); return !X || X->Phase != 2 || X->Stage != St; }); };
	auto Set = [Am](int32 P, float V) -> TFunction<void()> { return [Am, P, V] { Am().Want[P] = V; }; };
	auto Is = [Am](int32 P) -> TFunction<bool()> { return [Am, P] { return AwIs(Am(), P); }; };
	// (for the other one: until a part has been sent there and is there)
	auto There = [this, Am](int32 Ci, int32 P, float V, float MaxT) { WaitFor(Ci, [Am, P, V, MaxT](float E) { return E > MaxT || (FMath::Abs(Am().Want[P] - V) < 0.05f && FMath::Abs(Am().Now[P] - V) < 0.05f); }); };
	auto At = [Am](int32 Ci, FVector E) -> TFunction<bool()> { return [Am, Ci, E] { const FA::FRoll* R = Am().Rolls.Find(Ci); return R && !R->bOn && R->P.Num() && R->P.Last().Equals(E); }; };
	auto On = [this, Ti](const TCHAR* Nm, bool bBody = true) -> TFunction<FVector()> { const FVector L = SvcP(Tractors[Ti], Nm); return [this, Ti, L, bBody] { const FTractor& T = Tractors[Ti]; return FVector(TW(T, L + AwUp * (bBody ? T.Am->Now[FA::Lift] : 0.f))); }; };
	auto Key = [this, Ti, On](int32 Ci, FVector P, const TCHAR* K, TFunction<void()> Fn, TFunction<bool()> Done) { AmbuHand(Ci, Ti, P, On(*(FString(TEXT("IK_Button_")) + K)), [this, Ti, K, Fn] { AmbuPress(Ti, K); Fn(); }, Done, false, 40.f); };
	auto Lever = [this, Ti, On](int32 Ci, FVector P, const TCHAR* K, TFunction<void()> Fn, TFunction<bool()> Done, bool bBody) { AmbuHand(Ci, Ti, P, On(K, bBody), Fn, Done, false, 60.f); };
	auto Walk = [this, Ti](int32 Ci, TArray<FVector> P) { AmbuGo(Ci, Ti, INDEX_NONE, [P] { return P; }, true); };
	auto Level = [this](int32 Ci) { Do(Ci, [this, Ci] { if (ATugs* G = Tg()) { G->Crew[Ci].bUp = false; G->Crew[Ci].Hold = nullptr; } }); };
	auto Ground = [this, Ti, Level](int32 Ci, FVector P) { Level(Ci); WalkTo(Ci, [this, Ti, P] { return TW(Tractors[Ti], FVector(P.X, P.Y, 0.)); }, 12.f); };
	// the deck's two rails, the left first up, the right first down (both hands on the edge)
	auto TRail = [this, Ti, Am](int32 Ci, bool bUp)
	{
		for (int32 k = 0; k < 2; ++k)
		{
			const int32 r = bUp ? k : 1 - k;
			AmbuHand(Ci, Ti, FVector(AwRail.X, r ? 5. : -5., 2.), [this, Ti, r]
			{
				const FTractor& T = Tractors[Ti]; const FVector Pv = SvcP(T, r ? TEXT("PIV_TailRail_R") : TEXT("PIV_TailRail_L")), Hd = SvcP(T, r ? TEXT("IK_Tail_Rail_R_Hand") : TEXT("IK_Tail_Rail_L_Hand"));
				const float A = 90.f - 90.f * FMath::Clamp(2.f * T.Am->Now[FA::TRails] - float(r), 0.f, 1.f); return FVector(TW(T, AwRot(Pv, AwX, A * (r ? 1.f : -1.f), Hd) + AmbuTip(*T.Am)));
			}, [Am, bUp, k] { Am().Want[FA::TRails] = k ? (bUp ? 1.f : 0.f) : 0.5f; }, [Am] { return AwIs(Am(), FA::TRails); }, true, 8.f);
		}
	};
	const TArray<FVector> OnDeck = {AwBehind, AwRamp0, AwRamp1, AwRail}, OffDeck = {AwRail, AwRamp1, AwRamp0, AwBehind};
	switch (St)
	{
	case 0:
		if (J->Men[1].bSeated) SvcAlight(Job, 1);
		if (J->Men[0].bSeated) ClimbOut(C0, Ti, [this, Job] { if (FSvcMan* X = SvcM(Job, 0)) { X->bSeated = false; X->bOut = true; } });
		WaitFor(C0, [this, Job](float E) { return E > 60.f || SvcAll(Job, [](const FSvcMan& X) { return X.bOut; }); });
		End(C0, 1); break;
	case 1: case 15:
	{
		const FVector Sp(136.f, -172.f, 0.f); Ground(C0, Sp); Lever(C0, Sp, TEXT("IK_Lever_Stab"), Set(FA::Legs, St == 1 ? 30.f : 0.f), Is(FA::Legs), false); Level(C0);
		if (St == 15) Do(C0, [this, Job] { if (FSvcJob* X = SvcJ(Job)) for (FSvcMan& M : X->Men) M.bDown = true; });
		End(C0, St + 1); break;
	}
	case 2:   // the tail lift's deck open at floor height, down to the ground, its rails up
		Ground(C1, AwCtl); Key(C1, AwCtl, TEXT("TailFold"), Set(FA::Tail, 90.f), Is(FA::Tail)); Key(C1, AwCtl, TEXT("TailDown"), Set(FA::Arms, AwLow), Is(FA::Arms));
		Ground(C1, AwBehind); Walk(C1, OnDeck); TRail(C1, true); Walk(C1, OffDeck); Ground(C1, AwCtl); End(C1, 3); break;
	case 3:   // the driver up with it (the helper at the buttons outside), the rear doors, the panel inside
	{
		Ground(C0, AwBehind); Walk(C0, {AwBehind, AwRamp0, AwRamp1, AwDeck});
		const TFunction<bool()> Up = At(C0, AwDeck); WaitFor(C1, [Up](float E) { return E > 120.f || Up(); }); Key(C1, AwCtl, TEXT("TailUp"), Set(FA::Arms, 0.f), Is(FA::Arms)); Level(C1);
		There(C0, FA::Arms, 0.f, 120.f); Key(C0, AwDeck, TEXT("DoorRearOut"), Set(FA::DoorR, 1.f), Is(FA::DoorR)); Walk(C0, {AwDeck, FVector(-430.f, 12.f, 1.f), FVector(-390.f, 5.f, 1.f), AwInside});
		Key(C0, AwInside, TEXT("InTailDown"), Set(FA::Arms, AwLow), Is(FA::Arms)); Next(C0, 4); break;
	}
	case 4:
		Ground(C1, AwBehind); Walk(C1, {AwBehind, AwRamp0, AwRamp1, AwChair}); AmbuTail(C0, C1, Ti, 0.f); Walk(C1, {AwChair, FVector(-430.f, 0.f, 1.f), AwAisle});
		if (!bArr) for (int32 i = 0; i < N; ++i) AmbuTerminal(Job, i, true);
		Next(C1, 5); break;
	case 5:   // the rails down, the rear doors, the deck up against them
	{
		Walk(C1, {AwAisle, FVector(-430.f, 0.f, 1.f), AwRail}); TRail(C1, false); Walk(C1, {AwRail, FVector(-430.f, 0.f, 1.f), AwAisle, FVector(-100.f, 0.f, 1.f), AwWaitH});
		const TFunction<bool()> In = At(C1, AwWaitH); WaitFor(C0, [In](float E) { return E > 150.f || (E > 2.f && In()); });
		Key(C0, AwInside, TEXT("DoorRearIn"), Set(FA::DoorR, 0.f), Is(FA::DoorR)); Key(C0, AwInside, TEXT("InTailFold"), Set(FA::Tail, 0.f), Is(FA::Tail)); Walk(C0, {AwInside, AwAisle, FVector(120.f, 0.f, 1.f), AwFront}); End(C0, 6); break;
	}
	case 6:   // (until the passengers get on or off) up to the sill, both with it
		WaitFor(C0, [this, Job](float) { const FSvcJob* X = SvcJ(Job); return !X || X->bGo; });
		Lever(C0, AwFront, TEXT("IK_Lever_Lift"), [Am] { Am().Want[FA::Lift] = FMath::Clamp(Am().Sill - 128.f, 105.f, 290.f); }, Is(FA::Lift), true); End(C0, 7); break;
	case 7:
		Lever(C0, AwFront, TEXT("IK_Lever_Plat"), Set(FA::Plat, 90.f), Is(FA::Plat), true); Key(C0, AwFront, TEXT("DoorFront"), Set(FA::DoorF, 1.f), Is(FA::DoorF));
		There(C1, FA::DoorF, 1.f, 120.f); AmbuFrontRails(Job, true); There(C0, FA::Gates, 40.f, 120.f); Lever(C0, AwFront, TEXT("IK_Lever_Bridge"), Set(FA::Bridge, 50.f), Is(FA::Bridge), true);
		Do(C0, [this, Job] { if (const FSvcJob* X = SvcJ(Job)) ADP_LOG(TEXT("GSE: the ambulift's platform is at %s's front right door"), *X->Reg); }); End(C0, 8); break;
	case 8: for (int32 i = 0; i < N; ++i) AmbuDoor(Job, i, bArr); End(C1, 9); break;
	case 9:
	{
		Lever(C0, AwFront, TEXT("IK_Lever_Bridge"), Set(FA::Bridge, 0.f), Is(FA::Bridge), true); There(C1, FA::Bridge, 0.f, 30.f); AmbuFrontRails(Job, false);
		const TFunction<bool()> In = At(C1, AwWaitH); WaitFor(C0, [In, Am](float E) { return E > 150.f || (Am().Now[FA::Rails] <= 0.f && In()); });
		Key(C0, AwFront, TEXT("DoorFront"), Set(FA::DoorF, 0.f), Is(FA::DoorF)); Lever(C0, AwFront, TEXT("IK_Lever_Plat"), Set(FA::Plat, 0.f), Is(FA::Plat), true); End(C0, 10); break;
	}
	case 10:
		Lever(C0, AwFront, TEXT("IK_Lever_Lift"), Set(FA::Lift, 0.f), Is(FA::Lift), true);
		if (!bArr) Do(C0, [this, Job] { if (FSvcJob* X = SvcJ(Job)) X->CtDone = 1; });
		End(C0, 11); break;
	case 11:
		Walk(C0, {AwFront, FVector(120.f, 0.f, 1.f), AwAisle, AwInside}); Key(C0, AwInside, TEXT("InTailFold"), Set(FA::Tail, 90.f), Is(FA::Tail)); Key(C0, AwInside, TEXT("DoorRearIn"), Set(FA::DoorR, 1.f), Is(FA::DoorR)); End(C0, 12); break;
	case 12:   // the rails up; (arrivals: to the terminal;) the helper down
		Walk(C1, {AwWaitH, FVector(-100.f, 0.f, 1.f), AwAisle, FVector(-430.f, 0.f, 1.f), AwRail}); TRail(C1, true); Walk(C1, {AwRail, FVector(-430.f, 0.f, 1.f), AwAisle});
		if (bArr) { for (int32 i = 0; i < N; ++i) AmbuTerminal(Job, i, false); Do(C1, [this, Job] { if (FSvcJob* X = SvcJ(Job)) X->CtDone = 1; }); }
		Walk(C1, {AwAisle, FVector(-430.f, 0.f, 1.f), AwChair}); AmbuTail(C0, C1, Ti, AwLow); Walk(C1, {AwChair, AwRamp1, AwRamp0, AwBehind}); Ground(C1, AwCtl); Next(C1, 13); break;
	case 13:   // the driver down: the deck up by the panel, out, the doors shut, down by the helper's button
		Key(C0, AwInside, TEXT("InTailUp"), Set(FA::Arms, 0.f), Is(FA::Arms)); Walk(C0, {AwInside, FVector(-390.f, 5.f, 1.f), FVector(-430.f, 12.f, 1.f), AwDeck}); Key(C0, AwDeck, TEXT("DoorRearOut"), Set(FA::DoorR, 0.f), Is(FA::DoorR));
		There(C1, FA::DoorR, 0.f, 120.f); Wait(C1, 1.5f); Key(C1, AwCtl, TEXT("TailDown"), Set(FA::Arms, AwLow), Is(FA::Arms)); There(C0, FA::Arms, AwLow, 120.f); Walk(C0, {AwDeck, AwRamp1, AwRamp0, AwBehind}); Level(C0); End(C0, 14); break;
	case 14:
		WalkTo(C0, [this, Ti] { return TW(Tractors[Ti], FVector(-600.f, -230.f, 0.f)); }, 25.f); Ground(C1, AwBehind); Walk(C1, OnDeck); TRail(C1, false); Walk(C1, OffDeck); Ground(C1, AwCtl);
		Key(C1, AwCtl, TEXT("TailUp"), Set(FA::Arms, 0.f), Is(FA::Arms)); Key(C1, AwCtl, TEXT("TailFold"), Set(FA::Tail, 0.f), Is(FA::Tail)); Level(C1); End(C1, 15); break;
	default: AmbuEnd(Job); SvcLeave(Job); SvcBoard(Job, 1); break;
	}
}

void AGSE::AmbuEnd(int32 Job)
{
	if (const FString* K = AmbuFlown.Find(Job)) { AmbuDone.Add(*K); AmbuFlown.Remove(Job); }
	FSvcJob* J = SvcJ(Job); if (!J || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Am) return;
	J->CtDone = 1; FSvcAmbu& C = *Tractors[J->Ti].Am; C.Rolls.Reset();
	if (C.bRoom) if (UStaffPeople* Sp = UStaffPeople::Get(GetWorld())) { Sp->RampFree(C.Room); C.bRoom = false; }
}

void AGSE::AmbuTail(int32 C0, int32 C1, int32 Ti, float To)
{
	// the deck up or down by the driver at the panel inside, the helper (and the chair) on it
	auto Am = [this, Ti]() -> FSvcAmbu& { return *Tractors[Ti].Am; }; const bool bUp = To > -1.f;
	Do(C1, [this, C0, Ti, To, bUp, Am]
	{
		const FVector L = SvcP(Tractors[Ti], bUp ? TEXT("IK_Button_InTailUp") : TEXT("IK_Button_InTailDown"));
		AmbuHand(C0, Ti, AwInside, [this, Ti, L] { return FVector(TW(Tractors[Ti], L)); }, [this, Ti, To, bUp, Am] { AmbuPress(Ti, bUp ? TEXT("InTailUp") : TEXT("InTailDown")); Am().Want[FA::Arms] = To; }, [Am] { return AwIs(Am(), FA::Arms); }, false, 30.f);
	});
	WaitFor(C1, [Am, To](float E) { return E > 90.f || (FMath::Abs(Am().Want[FA::Arms] - To) < 0.1f && FMath::Abs(Am().Now[FA::Arms] - To) < 0.1f && AwIs(Am(), FA::Flap)); });
	Wait(C1, 0.5f);
}

void AGSE::AmbuFrontRails(int32 Job, bool bOut)
{
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti, Ci = J->Men[1].Ci; auto Am = [this, Ti]() -> FSvcAmbu& { return *Tractors[Ti].Am; };
	auto Rail = [this, Ti, Ci, Am](int32 r, float To)
	{
		AmbuHand(Ci, Ti, FVector(385.f, r ? 8.f : -8.f, 1.f), [this, Ti, r]
		{
			const FTractor& T = Tractors[Ti]; const FVector Pv = SvcP(T, r ? TEXT("PIV_RailPlat_R") : TEXT("PIV_RailPlat_L")), Hd = SvcP(T, r ? TEXT("IK_Rail_Hand_R") : TEXT("IK_Rail_Hand_L"));
			return FVector(TW(T, AwRot(Pv, AwX, (90.f - 90.f * FMath::Clamp(2.f * T.Am->Now[FA::Rails] - float(r), 0.f, 1.f)) * (r ? 1.f : -1.f), Hd) + AwUp * T.Am->Now[FA::Lift]));
		}, [Am, To] { Am().Want[FA::Rails] = To; }, [Am] { return AwIs(Am(), FA::Rails); }, true, 8.f);
	};
	// (the two gates are one: the right one follows)
	auto Gate = [this, Ti, Ci, Am](float To) { AmbuHand(Ci, Ti, FVector(421.f, -15.f, 1.f), [this, Ti] { const FTractor& T = Tractors[Ti]; return FVector(TW(T, SvcP(T, TEXT("IK_Handle_Gate_L")) + FVector(T.Am->Now[FA::Gates], 0.f, T.Am->Now[FA::Lift]))); }, [Am, To] { Am().Want[FA::Gates] = To; }, [Am] { return AwIs(Am(), FA::Gates); }, false, 6.f); };
	auto Walk = [this, Ti, Ci](TArray<FVector> P) { AmbuGo(Ci, Ti, INDEX_NONE, [P] { return P; }, true); };
	const FVector Pf(385.f, 0.f, 1.f), Pg(421.f, -15.f, 1.f), Dr(220.f, 0.f, 1.f);
	if (bOut) { Walk({AwWaitH, Dr, Pf}); Rail(0, 0.5f); Rail(1, 1.f); Walk({Pf, Pg}); Gate(40.f); Walk({Pg, FVector(330.f, 0.f, 1.f), Dr, AwWaitH}); }
	else { Walk({AwWaitH, Dr, Pg}); Gate(0.f); Walk({Pg, Pf}); Rail(1, 0.5f); Rail(0, 0.f); Walk({Pf, Dr, AwWaitH}); }
}

void AGSE::AmbuDoor(int32 Job, int32 i, bool bFetch)
{
	// wheelchair i from its bay to the aircraft's door and back: full in and empty out, or (bFetch) the other way round
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti, Ci = J->Men[1].Ci; const FVector Bay = AwBay[i], Sill(440.f, 0.f, 1.f);
	auto Vis = [this, Ci, Ti, i](bool b) { if (ATugs* G = Tg()) if (AActor* A = G->Crew[Ci].A.Get()) A->SetActorHiddenInGame(!b); Tractors[Ti].Am->Chair[i].bHide = !b; };
	AmbuGo(Ci, Ti, INDEX_NONE, [Bay] { return TArray<FVector>{AwWaitH, FVector(0.f, 0.f, 1.f), FVector(-190.f, 0.f, 1.f), FVector(-262.f, Bay.Y * 0.5, 1.f), FVector(Bay.X - AwPush, Bay.Y, 1.f)}; }, true);
	AmbuGo(Ci, Ti, i, [Bay, Sill] { TArray<FVector> P = {Bay, FVector(Bay.X + 45.f, Bay.Y, 1.f), AwSpin, FVector(60.f, 0.f, 1.f), FVector(300.f, 0.f, 1.f), Sill}; AwSmooth(P); return P; }, true);
	Do(Ci, [Vis] { Vis(false); }); Wait(Ci, AwCabinSeconds * 0.5f);
	if (bFetch) WaitFor(Ci, [this, Ti, i](float E) { return E > 30.f || AmbuSeat(Ti, i); }); else Do(Ci, [this, Ti, i] { AmbuUnseat(Ti, i); });
	Wait(Ci, AwCabinSeconds * 0.5f);
	// (he comes out turned round, the chair ahead of him into the body; in its bay it faces the rear now: the way it leaves)
	AmbuGo(Ci, Ti, i, [Sill] { return TArray<FVector>{Sill, FVector(Sill.X - 4.f, 0.f, 1.f)}; }, true); Do(Ci, [Vis] { Vis(true); });
	AmbuGo(Ci, Ti, i, [Bay] { TArray<FVector> P = {FVector(436.f, 0.f, 1.f), FVector(60.f, 0.f, 1.f), AwSpin, FVector(Bay.X + 45.f, Bay.Y, 1.f), Bay}; AwSmooth(P); return P; }, true);
	Do(Ci, [this, Job, Ti, i, bFetch]
	{
		FA::FChair& K = Tractors[Ti].Am->Chair[i]; K.BayYaw = 180.f; K.bBay = true; FSvcJob* X = SvcJ(Job); if (!X) return;
		if (!bFetch) ++X->RowsDone; ADP_LOG(TEXT("GSE: wheelchair passenger %d of %d %s %s"), i + 1, X->Rows, bFetch ? TEXT("off") : TEXT("aboard"), *X->Reg);
	});
	AmbuGo(Ci, Ti, INDEX_NONE, [Bay] { return TArray<FVector>{FVector(Bay.X + AwPush, Bay.Y, 1.f), FVector(20.f, 0.f, 1.f), AwWaitH}; }, true);
}

void AGSE::AmbuTerminal(int32 Job, int32 i, bool bFetch)
{
	// wheelchair i from its bay down the tail lift to the ramp room's door, behind it the passenger gets in (bFetch) or out, unseen, and back
	FSvcJob* J = SvcJ(Job); if (!J) return;
	const int32 Ti = J->Ti, C0 = J->Men[0].Ci, Ci = J->Men[1].Ci; const FVector Bay = AwBay[i];
	TArray<FVector> Dn;
	if (Tractors[Ti].Am->Chair[i].BayYaw > 90.f || !bFetch)
	{
		AmbuGo(Ci, Ti, INDEX_NONE, [Bay] { return TArray<FVector>{AwAisle, FVector(-120.f, 0.f, 1.f), FVector(Bay.X + AwPush, Bay.Y, 1.f)}; }, true);
		Dn = {Bay, FVector(Bay.X - 45.f, Bay.Y, 1.f), FVector(-320.f, 0.f, 1.f), FVector(-400.f, 0.f, 1.f), AwChair}; AwSmooth(Dn);
	}
	else
	{
		// (it faces forward: out into the aisle and turned round there first)
		AmbuGo(Ci, Ti, INDEX_NONE, [Bay] { return TArray<FVector>{AwAisle, FVector(-262.f, Bay.Y * 0.5, 1.f), FVector(Bay.X - AwPush, Bay.Y, 1.f)}; }, true);
		AmbuGo(Ci, Ti, i, [Bay] { TArray<FVector> P = {Bay, FVector(Bay.X + 40.f, Bay.Y, 1.f), FVector(AwSpin.X - 25.f, 0.f, 1.f), AwSpin}; AwSmooth(P); return P; }, true);
		AmbuSpin(Ci, Ti, i); Dn = {AwSpin, FVector(-300.f, 0.f, 1.f), FVector(-400.f, 0.f, 1.f), AwChair};
	}
	AmbuGo(Ci, Ti, i, [Dn] { return Dn; }, true); AmbuTail(C0, Ci, Ti, AwLow);
	AmbuGo(Ci, Ti, i, [] { return TArray<FVector>{AwChair, AwRamp1, AwRamp0, FVector(-760.f, 0.f, 0.f)}; }, true);
	Do(Ci, [this, Job, Ti]
	{
		UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); FSvcAmbu& C = *Tractors[Ti].Am; float S = 0.f, Nz = 0.f, Span = 36.f; const FSvcJob* X = SvcJ(Job);
		if (!C.bRoom) { C.Room = INDEX_NONE; if (Sp && X && StandGeo(X->StandX, S, Nz, Span)) C.Room = Sp->RampNear(S - 13.f, C.RoomX); if (C.Room == INDEX_NONE) ADP_WARN(TEXT("GSE: no ramp-level door near this stand for the passenger in the wheelchair")); }
	});
	WaitFor(Ci, [this, Ti](float) { FSvcAmbu& C = *Tractors[Ti].Am; UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); if (!C.bRoom && Sp && C.Room != INDEX_NONE) C.bRoom = Sp->RampTake(C.Room); return C.bRoom || C.Room == INDEX_NONE; });
	auto Way = [this, Ti](bool bTo)
	{
		const FSvcAmbu& C = *Tractors[Ti].Am; const FVector D = Plan(C.RoomX + 5.2f, 87.f), R = TW(Tractors[Ti], FVector(-760.f, 0.f, 0.f)); TArray<FVector> P;
		if (!AmbuRoute(bTo ? R : D, bTo ? D : R, P)) { ADP_WARN(TEXT("GSE: no free way between the ambulift and the ramp room's door - straight")); P = {bTo ? R : D, bTo ? D : R}; }
		return P;
	};
	AmbuGo(Ci, Ti, i, [this, Ti, Way]
	{
		if (Tractors[Ti].Am->Room == INDEX_NONE) return TArray<FVector>();
		TArray<FVector> P = Way(true); const float X = Tractors[Ti].Am->RoomX; P.Append({Plan(X + 5.2f, 82.f), Plan(X + 5.2f, 79.6f), Plan(X + 4.6f, 78.2f), Plan(X + 3.2f, 77.2f)}); AwSmooth(P); return P;
	}, false);
	// the door shut behind them and you not in there: in or out of the chair
	WaitFor(Ci, [this, Ti, i, bFetch](float E)
	{
		FSvcAmbu& C = *Tractors[Ti].Am; UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); if (!Sp || C.Room == INDEX_NONE) return true;
		if (E < 2.f || ABabylonModel::StaffDoor(Plan(C.RoomX + 5.f, 82.f), false) > 0.05f || Sp->RampSeen(C.Room)) return false;
		if (bFetch) return AmbuSeat(Ti, i);
		AmbuUnseat(Ti, i); return true;
	});
	Wait(Ci, 6.f);
	Do(Ci, [this, Job, bFetch] { if (FSvcJob* X = SvcJ(Job)) { if (!bFetch) ++X->RowsDone; ADP_LOG(TEXT("GSE: the wheelchair passenger %s the terminal's ramp door (%s)"), bFetch ? TEXT("comes out of") : TEXT("is in at"), *X->Reg); } });
	// (round in there, out again)
	AmbuGo(Ci, Ti, i, [this, Ti, Way]
	{
		if (Tractors[Ti].Am->Room == INDEX_NONE) return TArray<FVector>();
		const float X = Tractors[Ti].Am->RoomX; TArray<FVector> P = {Plan(X + 3.2f, 77.2f), Plan(X + 2.2f, 76.4f), Plan(X + 2.4f, 75.f), Plan(X + 4.2f, 75.2f), Plan(X + 5.2f, 77.f), Plan(X + 5.2f, 82.f)}; P.Append(Way(false)); P.Add(TW(Tractors[Ti], FVector(-690.f, 0.f, 0.f))); AwSmooth(P); return P;
	}, false);
	Do(Ci, [this, Ti] { FSvcAmbu& C = *Tractors[Ti].Am; if (C.bRoom) if (UStaffPeople* Sp = UStaffPeople::Get(GetWorld())) { Sp->RampFree(C.Room); C.bRoom = false; } });
	AmbuGo(Ci, Ti, i, [] { return TArray<FVector>{AwBehind, AwRamp0, AwRamp1, AwChair}; }, true); AmbuTail(C0, Ci, Ti, 0.f);
	AmbuGo(Ci, Ti, i, [Bay] { TArray<FVector> P = {AwChair, FVector(-400.f, 0.f, 1.f), FVector(-320.f, 0.f, 1.f), FVector(Bay.X - 45.f, Bay.Y, 1.f), Bay}; AwSmooth(P); return P; }, true);
	Do(Ci, [this, Ti, i] { FA::FChair& K = Tractors[Ti].Am->Chair[i]; K.BayYaw = 0.f; K.bBay = true; });
	AmbuGo(Ci, Ti, INDEX_NONE, [Bay] { return TArray<FVector>{FVector(Bay.X - AwPush, Bay.Y, 1.f), FVector(-262.f, Bay.Y * 0.4, 1.f), AwAisle}; }, true);
}

void AGSE::AmbuSpin(int32 Ci, int32 Ti, int32 i)
{
	// turned round on the spot in the aisle (its castors swivel): he walks round with it on the side without seats, both hands on the handles
	TSharedRef<float> Y0 = MakeShared<float>(0.f); Do(Ci, [this, Ti, i, Y0] { *Y0 = float(FMath::FindDeltaAngleDegrees(Tractors[Ti].Yaw, Tractors[Ti].Am->Chair[i].Yaw)); });
	PoseAct(Ci, [this, Ti, i, Y0](float K, float PH, FDriverPose& P)
	{
		FTractor& T = Tractors[Ti]; FA::FChair& W = T.Am->Chair[i]; W.At = TW(T, AmbuL(*T.Am, AwSpin)); W.Yaw = T.Yaw + FMath::Lerp(*Y0, -180.f, FMath::SmoothStep(0.f, 1.f, K)); W.Pitch = 0.f;
		const FVector D = FRotator(0.f, W.Yaw, 0.f).Vector(), Me = W.At - D * AwPush, Lt = ADPPose::LeftOf(D); P = ADPPose::Standing(Me, D, PH); const float Sw = FMath::Sin(K * PI * 10.f);
		P.Foot[0] = Me + Lt * 10.f + AwUp * FMath::Max(0.f, Sw) * 5.f; P.Foot[1] = Me - Lt * 10.f + AwUp * FMath::Max(0.f, -Sw) * 5.f; const FTransform X = AmbuChairX(Ti, i);
		P.bHand[0] = P.bHand[1] = true; P.Grip[0] = P.Grip[1] = 0.95f; P.Lean = 8.f; P.Hand[0] = X.TransformPosition(SvcP(T, TEXT("WC_IK_Push_L"))); P.Hand[1] = X.TransformPosition(SvcP(T, TEXT("WC_IK_Push_R")));
	}, 5.f);
}

void AGSE::AmbuLost(int32 Job, int32 m)
{
	// somebody vanished at work (the ordinary ground relief comes): on the ground the stage is done again; with people up inside all
	// is stowed by the one who is left, the flight no longer waits
	FSvcJob* J = SvcJ(Job); ATugs* G = Tg(); if (!J || !G || !Tractors.IsValidIndex(J->Ti) || !Tractors[J->Ti].Am) return;
	const int32 Ti = J->Ti, Ci = J->Men[m].Ci, Co = J->Men.Num() > 1 ? J->Men[1 - m].Ci : INDEX_NONE; ATugs::FCrew& K = G->Crew[Ci]; K.bUp = false; K.Q.Reset(); K.Hold = nullptr; J->Men[m].bSeated = false;
	if (J->Phase != 2) { if (m == 1) SvcBoard(Job, 1); return; }
	FSvcAmbu& C = *Tractors[Ti].Am; C.Rolls.Remove(Ci);
	if (J->Stage > 2 && J->Stage < 14)
	{
		ADP_WARN(TEXT("GSE: somebody of the ambulift at %s vanished at stage %d - all is stowed, the flight no longer waits for it"), *J->Reg, J->Stage);
		J->CtDone = 1; for (int32 i = 0; i < 2; ++i) { AmbuUnseat(Ti, i); C.Chair[i].bBay = true; C.Chair[i].bHide = false; }
		if (Co != INDEX_NONE) { G->Crew[Co].Q.Reset(); if (G->Crew[Co].bUp && J->Stage != 4) AmbuHold(Co, Ti, AwAisle, AwX); }
		WaitFor(Ci, [this, Ti](float)
		{
			FSvcAmbu& X = *Tractors[Ti].Am; X.Want[FA::Bridge] = X.Want[FA::Gates] = 0.f; if (X.Now[FA::Bridge] <= 0.f && X.Now[FA::Gates] <= 0.f) X.Want[FA::Rails] = 0.f;
			if (X.Now[FA::Rails] <= 0.f) X.Want[FA::DoorF] = X.Want[FA::Plat] = 0.f;
			if (X.Now[FA::Plat] <= 0.f) X.Want[FA::Lift] = 0.f;
			if (X.Now[FA::Lift] > 0.f) return false;
			X.Want[FA::Tail] = 90.f; if (X.Now[FA::Tail] > 89.9f) { X.Want[FA::DoorR] = X.Now[FA::Arms] > -0.5f ? 1.f : X.Want[FA::DoorR]; }
			return X.Now[FA::Tail] > 89.9f;
		});
		// (the one inside walks out over the deck, let down with him)
		if (Co != INDEX_NONE && G->Crew[Co].bUp) { WaitFor(Co, [this, Ti](float E) { return E > 120.f || Tractors[Ti].Am->Now[FA::DoorR] > 0.99f; }); AmbuGo(Co, Ti, INDEX_NONE, [] { return TArray<FVector>{AwAisle, FVector(-430.f, 0.f, 1.f), AwChair}; }, true); Do(Co, [this, Ti] { Tractors[Ti].Am->Want[FA::DoorR] = 0.f; Tractors[Ti].Am->Want[FA::Arms] = AwLow; }); WaitFor(Co, [this, Ti](float E) { return E > 30.f || Tractors[Ti].Am->Now[FA::Arms] < AwLow + 0.1f; }); AmbuGo(Co, Ti, INDEX_NONE, [] { return TArray<FVector>{AwChair, AwRamp1, AwRamp0, AwBehind}; }, true); Do(Co, [this, Co, Ci, Job] { if (ATugs* X = Tg()) { X->Crew[Co].bUp = false; X->Crew[Co].Hold = nullptr; } if (FSvcJob* Y = SvcJ(Job)) Y->Stage = 14; Later([this, Job] { AmbuStage(Job); }); }); Wait(Co, 0.4f); J->Stage = 13; return; }
		J->Stage = 14;
	}
	else if (Co != INDEX_NONE) G->Crew[Co].Q.Reset();
	Do(Ci, [this, Job] { Later([this, Job] { AmbuStage(Job); }); }); Wait(Ci, 0.4f);
}
