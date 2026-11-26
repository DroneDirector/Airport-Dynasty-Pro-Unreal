// To-do 238: the pushback tug with you at its wheel. You drive it as you drive every other vehicle (ServiceDrive.cpp, the cab
// CabControls.cpp): a seat line in AGSE::Tractors stands for each of ATugs' tugs, never a second vehicle. What its crew do at the
// nose gear (the bar's head, the bypass pin, the interphone) you do with the same movements at the same places.
#include "GSE.h"
#include "Tugs.h"
#include "HangarTow.h"
#include "VehicleControls.h"
#include "ServiceDrive.h"
#include "CabControls.h"
#include "CrewPose.h"
#include "BabylonModel.h"
#include "ADPLog.h"
#include "Turnaround.h"
#include "StaffPeople.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSchedule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	enum : int32 { TgHead = 301, TgPin, TgPhone, TgShow };
	constexpr float TgLeave = 4000.f, TgWait = 120.f;       // (its crew come: you 40 m away for 2 min, as the baggage tractor)
	constexpr float TgHeadAhead = 792.25f;                  // (the bar's head ahead of the tug's middle: HeadAhead, Tugs.cpp)
	constexpr float TgWheelbase = 12.64f;                   // (the A320's, m: SimTraffic.cpp)
	const FVector TgWheelC(28.f, -70.75f, 138.3f), TgWheelAxis(-0.84f, 0.f, 0.54f);
	const FVector TgPedal[2] = {FVector(42.f, -58.8f, 65.f), FVector(42.f, -70.8f, 65.f)};   // (accelerator, brake)
	float TgTurn(float A, float B) { return FMath::FindDeltaAngleDegrees(A, B); }
	FVector TgL(const FVector& F) { return FVector(-F.Y, F.X, 0.f); }   // (the aircraft's right: the side its crew work on)
}

// ------------------------------------------------------------------ the tug's side

/** Where its bar meets the aircraft standing at its gate; false: none there, or its chocks are not in. */
bool ATugs::YouHook(FTug& T) const
{
	if (T.bYouPush) return false;   // (its push has it)
	if (T.bHangar)
	{
		// (the hangar tow, yours: the nose of the aircraft that waits for it - HangarTow's own job, the same fitting)
		FString K = T.YouStep > 0 ? T.YouTowKey : FString(); FVector2f N, M;
		if (!AHangarTow::YouFit(GetWorld(), T.At, K, N, M)) return false;
		const FVector2f D = (N - M).GetSafeNormal(); const FVector F(D.X, -D.Y, 0.f);
		T.YouTowKey = K; T.Fwd = F; T.Fit = Plan(N.X, N.Y) + F * 60.f;
		T.HookAt = T.Fit + F * TgHeadAhead; T.HookAt.Z = Plan(T.HookAt.X / 100.f, -T.HookAt.Y / 100.f).Z; T.HookYaw = (-F).Rotation().Yaw;
		return true;
	}
	if (T.Ck.Stage < 1 || T.Ck.Stage > 4 || !ChocksIn(T.StandX)) return false;
	const FVector A = Plan(T.StandX, T.StandNose), F = (Plan(T.StandX, T.StandNose + T.StandDir) - A).GetSafeNormal2D();
	// (the fitting: 60 cm ahead of the nose tyre, the tyre 5.1 m behind the stop line - Receptacle, SimTraffic)
	T.Fit = Plan(T.StandX, T.StandNose - T.StandDir * 4.5f); T.Fwd = F;
	T.HookAt = T.Fit + F * TgHeadAhead; T.HookAt.Z = Plan(T.HookAt.X / 100.f, -T.HookAt.Y / 100.f).Z; T.HookYaw = (-F).Rotation().Yaw;
	return true;
}

bool ATugs::YouFree(int32 Tug, FString& Why) const
{
	const FTug& T = Tugs[Tug];
	if (T.State == EState::You && T.bYouPush && !T.bYouCrew) { if (T.Speed > 2.f) { Why = TEXT("it is driving"); return false; } return true; }   // (your own push: out and in again as often as you like)
	if (T.bYouCrew || (T.State != EState::Parked && T.State != EState::You) || !T.Key.IsEmpty()) { Why = TEXT("its crew have it"); return false; }
	if (T.bHired && !HiredFree(T)) { Why = TEXT("its crew are still at work"); return false; }
	if (Crew.IsValidIndex(T.Driver) && T.State == EState::Parked)
		if (const FCrew& D = Crew[T.Driver]; D.Hold || (D.Q.Num() && D.A.IsValid() && FVector::Dist2D(D.A->GetActorLocation(), T.At) < 450.f)) { Why = TEXT("its driver is still at it"); return false; }
	if (T.Speed > 2.f || T.Path.Num()) { Why = TEXT("it is driving"); return false; }
	return true;
}

/** The gate's own tug is with you: a flight there waits for it, and why. */
bool ATugs::YouHave(float StandX, FString& Why) const
{
	for (const FTug& T : Tugs)
		if (!T.bHangar && T.State == EState::You && FMath::Abs(T.StandX - StandX) < 1.f && (T.bYouIn ? T.YouStep != 3 : T.YouStep == 0))
		{
			Why = T.bYouCrew ? TEXT("its pushback tug is being fetched by its driver") : T.bYouIn ? TEXT("you are in its pushback tug") : TEXT("you have its pushback tug - bring it back, or leave it and its driver fetches it");
			return true;
		}
	return false;
}

/** (the push, the next job: you at the wheel of the tug coupled to that flight's aircraft) */
bool ATugs::YouAtWheel(const FString& Key) const
{
	for (const FTug& T : Tugs) if (T.Key == Key && T.bYouIn && T.State == EState::Hooked) return true;
	return false;
}

void ATugs::YouHome(int32 Tug)
{
	FTug& T = Tugs[Tug];
	auto Go = [this, Tug]
	{
		FTug& U = Tugs[Tug]; if (U.State != EState::You) return;
		Engine(U, true); U.YouStep = 0; U.bPinShown = true;
		// (off the nose backwards first, as its crew do)
		if (YouHook(U) && FVector::Dist2D(U.At, U.HookAt) < 900.f && FMath::Abs(TgTurn(U.Yaw, U.HookYaw)) < 60.f) { U.State = EState::Unhook; StartBacking(U); }
		else StartHome(U);
		ADP_LOG(TEXT("Tugs: you left tug %d - its driver takes it home"), Tug);
	};
	T.bYouCrew = true;
	if (!Crew.IsValidIndex(T.Driver)) { Go(); return; }   // (a gate without a driver: as its own jobs)
	ADP_LOG(TEXT("Tugs: you left tug %d - its driver walks out to it"), Tug);
	ClimbIn(Crew[T.Driver], Tug, Go);
}

/** It may move this moment: its departure said so in the last half second (YouDrive). */
bool ATugs::YouMay(const FTug& T) const
{
	return T.State == EState::You && T.bYouPush && T.bYouGo && !T.bYouDone && GetWorld()->GetTimeSeconds() - T.YouGoAt < 0.5;
}

bool ATugs::YouPushing(const FString& Key) const
{
	for (const FTug& T : Tugs) if (T.Key == Key && T.State == EState::You && T.bYouPush && !T.bYouDone) return true;
	return false;
}

void ATugs::YouSayWhy(float StandX, const FString& Why)
{
	for (FTug& T : Tugs)
		if (T.State == EState::You && T.bYouPush && FMath::Abs(T.StandX - StandX) < 1.f) { T.YouWhy = Why; T.YouBeat = GetWorld()->GetTimeSeconds(); }
}

/** The aircraft with the tug at At, Yaw: the bar's head on its fitting, its main gear rolls after it. False: the bar past its limit. */
bool ATugs::YouTow(const FTug& T, const FVector& At, float Yaw, FVector2f& Nose, FVector2f& Main) const
{
	Nose = T.YouNose; Main = T.YouMain;
	if (!T.bYouGo) return true;
	const FVector Bar = FRotator(0.f, Yaw, 0.f).Vector(), H = At + Bar * TgHeadAhead;
	const FVector2f Fit(H.X / 100.f, -H.Y / 100.f), D = (Fit - Main).GetSafeNormal();
	Main = Fit - D * (TgWheelbase + 0.6f); Nose = Fit - D * 0.6f;
	const float Was = FMath::Abs(T.YouBar), B = FMath::Abs(TgTurn(FVector(-D.X, D.Y, 0.f).Rotation().Yaw, Yaw));
	return B <= 75.f || B < Was;
}

/** Everything before the push, your tug already on the nose: as with the crew (Push). */
bool ATugs::PushYou(int32 I)
{
	FTug& T = Tugs[I];
	if (T.Seq >= 8 || T.bYouDone) return true;
	const double Now = GetWorld()->GetTimeSeconds();
	if (T.Seq == 0)
	{
		if (T.bHired && Now - T.YouWhyAt > 3.0) { T.YouWhyAt = Now; CrewWalkers(I); }   // (your own wing walkers, whoever is free: they do the chocks too)
		const int32 Si = T.bHangar ? StandTug(T.StandX) : I; FTug& Sg = Tugs.IsValidIndex(Si) ? Tugs[Si] : T;   // (the hangar tug: its stand's ground power and chocks)
		if (Sg.Gpu.bConnected || Sg.Gpu.bYou || Sg.Gpu.bLying)   // (off by your own hand counts: GpuYou.cpp)
		{
			T.YouWhy = YouShort(T, Sg.Gpu.bConnected ? TEXT("the ground power is still plugged in: take it off, or wait for the handler") : TEXT("the ground power cable is still out"));
			// (the handler, else a walker of yours: one of them, once)
			auto Free = [this](int32 c) { return Crew.IsValidIndex(c) && !Crew[c].bSlotFree && !Crew[c].bLeave && !Crew[c].bLost && !Crew[c].Q.Num() && !Crew[c].Hold; };
			const bool bAt = Sg.Gpu.bInHand || Sg.Gpu.bYouOwn || Sg.GpuCi != INDEX_NONE || (Crew.IsValidIndex(T.YouGpuCi) && !Crew[T.YouGpuCi].bSlotFree && (Crew[T.YouGpuCi].Q.Num() || Crew[T.YouGpuCi].Hold) && (T.YouGpuCi == T.Handler || T.Walkers.Contains(T.YouGpuCi)));
			if (!bAt)
			{
				T.YouGpuCi = INDEX_NONE;
				if (Free(T.Handler)) T.YouGpuCi = T.Handler;
				else if (T.bHired) for (const int32 w : T.Walkers) if (Free(w)) { T.YouGpuCi = w; break; }
				if (T.YouGpuCi != INDEX_NONE) { ADP_LOG(TEXT("Tugs: your push of %s - %s takes the ground power off"), *T.Key, *Crew[T.YouGpuCi].Name); GpuDisconnect(Si, Crew[T.YouGpuCi]); }
			}
			return false;
		}
		if (!ChkOut(Sg)) { T.YouWhy = YouShort(T, TEXT("the chocks and cones are still being taken away")); return false; }
		if ((T.bHired || T.Driver != INDEX_NONE) && T.WalkersReady < WalkNeed(T)) { T.YouWhy = TEXT("the wing walkers are not in their places"); return false; }
		T.Seq = 1; T.SeqT = 0.f; T.SeqWait = FMath::FRandRange(2.f, 5.f);
	}
	T.YouWhy = TEXT("waiting for the cockpit - brakes released");
	return false;
}

bool ATugs::YouDrive(const FString& Key, AActor* Plane, FVector2f& Nose, FVector2f& Main, const TArray<FVector2f>& Path, int32& Step, float& Speed, float& Bar, float TaxiY, float TaxiDir)
{
	Step = INDEX_NONE; Speed = 0.f;
	const int32 I = Tugs.IndexOfByPredicate([&](const FTug& X) { return X.Key == Key && X.State == EState::You && X.bYouPush; }); if (I == INDEX_NONE) return false;
	FTug& T = Tugs[I]; const double Now = GetWorld()->GetTimeSeconds();
	if (!T.bYouGo)
	{
		T.YouNose = Nose; T.YouMain = Main; T.YouStart = Main; T.bYouGo = true; YouMarks(&T, Path);
		ADP_LOG(TEXT("Tugs: %s - brakes released, yours to push"), *Key);
		// (its handler beside the nose, on the headset, as at a crew's push)
		if (Crew.IsValidIndex(T.Handler)) Crew[T.Handler].Follow = [this, I] { const FTug& U = Tugs[I]; return U.Fit - U.Fwd * 60.f - TgL(U.Fwd) * 230.f; };
		AVehicleControls::Note(GetWorld(), TEXT("Brakes released - push along the marked line"));
	}
	T.YouPlane = Plane; T.YouGoAt = T.YouBeat = Now; T.YouWhy.Reset();
	Nose = T.YouNose; Main = T.YouMain;
	const FVector2f D = (Nose - Main).GetSafeNormal(); const FVector F(D.X, -D.Y, 0.f);
	const FVector P = Plan(Nose.X, Nose.Y); T.Fit = P + F * 60.f; T.Fwd = F; T.HookAt = T.At; T.HookYaw = T.Yaw;
	T.YouBar = -TgTurn((-F).Rotation().Yaw, T.Yaw); Bar = FMath::Clamp(T.YouBar, -75.f, 75.f);
	Speed = FMath::Abs(T.Speed) / 100.f;
	T.YouSide = FMath::Abs(Main.Y - TaxiY); T.YouTurn = FMath::RadiansToDegrees(FMath::Atan2(-D.Y * TaxiDir, D.X * TaxiDir));   // (+: its nose still to its left)
	// where its crew would go on from here: the nearest place of their path, on to the next point that lies behind it
	Step = YouOn(T, Main, D, Path, 6.f, 0.5f);
	T.bYouRejoin = Step != INDEX_NONE || (Main - T.YouStart).Size() < 0.5f;
	if (Step == INDEX_NONE && T.bYouRejoin) Step = 0;
	T.YouToGo = T.YouSide <= 3.f && FMath::Abs(T.YouTurn) <= 10.f ? 0.f : 1.f;
	return T.YouToGo == 0.f && FMath::Abs(T.Speed) < 2.f && T.bYouPark;
}

/** The point of the path that lies behind the aircraft from the nearest place of it (none: further off than Within, or turned away). */
void ATugs::YouTowFrame(const FString& Key, AActor* Plane, FVector2f& Nose, FVector2f& Main, bool bGo, bool bMay, float Cap, bool bWing, const FString& Why, const FString& Say)
{
	const int32 I = Tugs.IndexOfByPredicate([&](const FTug& X) { return X.Key == Key && X.State == EState::You && X.bYouPush; }); if (I == INDEX_NONE) return;
	FTug& T = Tugs[I]; const double Now = GetWorld()->GetTimeSeconds();
	T.YouBeat = Now; T.YouTowSay = Say;
	if (!Why.IsEmpty()) T.YouWhy = Why;
	if (!bGo) return;
	if (!T.bYouGo)
	{
		T.YouNose = Nose; T.YouMain = Main; T.YouStart = Main; T.bYouGo = true;
		ADP_LOG(TEXT("Tugs: %s - brakes released, yours to tow"), *Key);
		if (Crew.IsValidIndex(T.Handler)) Crew[T.Handler].Follow = [this, I] { const FTug& U = Tugs[I]; return U.Fit - U.Fwd * 60.f - TgL(U.Fwd) * 230.f; };
		AVehicleControls::Note(GetWorld(), TEXT("Brakes released - tow it yourself, 8 km/h, 5 in turns"));
	}
	T.YouPlane = Plane; T.YouCap = Cap;
	// (blocked: the tug's brakes hold - ServiceDrive. A wingtip too close: only backing away from it)
	if (bMay && (!bWing || T.bYouBack)) { T.YouGoAt = Now; if (Why.IsEmpty()) T.YouWhy.Reset(); }
	Nose = T.YouNose; Main = T.YouMain;
	const FVector2f D = (Nose - Main).GetSafeNormal(); const FVector F(D.X, -D.Y, 0.f);
	T.Fit = Plan(Nose.X, Nose.Y) + F * 60.f; T.Fwd = F; T.HookAt = T.At; T.HookYaw = T.Yaw;
	T.YouBar = -TgTurn((-F).Rotation().Yaw, T.Yaw);
}

int32 ATugs::YouOn(const FTug& T, const FVector2f& Main, const FVector2f& D, const TArray<FVector2f>& Path, float Within, float Dot) const
{
	float Best = 1e9f; int32 Ni = INDEX_NONE;
	for (int32 i = 0; i < Path.Num(); ++i)
	{
		const FVector2f A = i ? Path[i - 1] : T.YouStart, B = Path[i], Ab = B - A; const float L2 = Ab.SizeSquared();
		const float K = L2 > 1e-4f ? FMath::Clamp(FVector2f::DotProduct(Main - A, Ab) / L2, 0.f, 1.f) : 0.f, Dd = (A + Ab * K - Main).Size();
		if (Dd < Best) { Best = Dd; Ni = i; }
	}
	for (int32 i = Ni; Best < Within && Path.IsValidIndex(i); ++i)
		if (const FVector2f To = Path[i] - Main; To.Size() > 1.f && FVector2f::DotProduct(To.GetSafeNormal(), -D) > Dot) return i;
	return INDEX_NONE;
}

void ATugs::YouRejoin(const FString& Key, const FVector2f& Nose, const FVector2f& Main, const TArray<FVector2f>& Path, TArray<FVector2f>& Fwd, int32& Step) const
{
	Fwd.Reset(); Step = INDEX_NONE;
	const int32 I = Tugs.IndexOfByPredicate([&](const FTug& X) { return X.Key == Key; });
	if (I == INDEX_NONE || !Path.Num()) return;
	const FTug& T = Tugs[I]; const AGSE* G = AGSE::Find(GetWorld());
	const int32 Ti = G ? G->Tractors.IndexOfByPredicate([&](const AGSE::FTractor& X) { return X.TugIx == I; }) : INDEX_NONE;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPYouRejoin), true); Q.AddIgnoredActor(T.M.Get()); Q.AddIgnoredActor(T.YouPlane.Get());
	const TCHAR* Last = nullptr;
	auto Safe = [&](const FVector2f& N, const FVector2f& M) { Last = Ti != INDEX_NONE ? G->TowHit(G->Tractors[Ti], N, M, Q, true) : nullptr; return Last == nullptr; };
	// the path from the stand on, and the place S m along it (before the start: straight on towards the stand)
	TArray<FVector2f> V; V.Add(T.YouStart); for (const FVector2f& P : Path) if ((P - V.Last()).Size() > 0.05f) V.Add(P);
	if (V.Num() > 1)
	{
		auto Place = [&V](float S) { if (S <= 0.f) return V[0] + (V[1] - V[0]).GetSafeNormal() * S; for (int32 i = 1; i < V.Num(); ++i) { const float L = (V[i] - V[i - 1]).Size(); if (S <= L) return V[i - 1] + (V[i] - V[i - 1]) * (S / L); S -= L; } return V.Last(); };
		auto Along = [&V](const FVector2f& P) { float Best = 1e9f, At = 0.f, S = 0.f; for (int32 i = 1; i < V.Num(); ++i) { const FVector2f Ab = V[i] - V[i - 1]; const float L = Ab.Size(), K = FMath::Clamp(FVector2f::DotProduct(P - V[i - 1], Ab) / (L * L), 0.f, 1.f), D = (V[i - 1] + Ab * K - P).Size(); if (D < Best) { Best = D; At = S + K * L; } S += L; } return At; };
		// pulled forward, the nose steered (60 degrees at most) to the line ahead of it: the first place the push can go on from;
		// never further in than it stood at the gate
		for (const float Look : {8.f, 14.f, 22.f})
		{
			FVector2f N = Nose, M = Main; TArray<FVector2f> Way;
			for (int32 k = 1; k <= 320; ++k)
			{
				const float S = Along(M); if (S < 0.5f && k > 1) break;
				const FVector2f D = (N - M).GetSafeNormal(), To = (Place(S - TgWheelbase - Look) - N).GetSafeNormal();
				const float Tn = FMath::Clamp(FMath::Atan2(D.X * To.Y - D.Y * To.X, FVector2f::DotProduct(D, To)), -1.05f, 1.05f);
				N += FVector2f(D.X * FMath::Cos(Tn) - D.Y * FMath::Sin(Tn), D.X * FMath::Sin(Tn) + D.Y * FMath::Cos(Tn)) * 0.25f;
				M = N - (N - M).GetSafeNormal() * TgWheelbase;
				if (k % 2 == 0) Way.Add(N);
				if (k % 8 == 0 && !Safe(N, M)) break;
				if (k % 4 != 0) continue;
				if (const int32 St = YouOn(T, M, (N - M).GetSafeNormal(), Path, 1.5f, 0.8f); St != INDEX_NONE && Safe(N, M))
				{
					Fwd = MoveTemp(Way); Step = St;
					ADP_LOG(TEXT("Tugs: %s - its crew pull it %.0f m forward onto the push line, then go on to point %d"), *Key, k * 0.25f, Step);
					return;
				}
			}
		}
	}
	// else back, straight to the nearest point of the path that lies behind it and is clear
	const FVector2f D0 = (Nose - Main).GetSafeNormal();
	auto Back = [&](int32 i) { FVector2f M = Main, N = Nose; const FVector2f To = Path[i] - Main, U = To.GetSafeNormal(); for (float S = 0.f; S < To.Size(); S += 2.f) { M = Main + U * S; N = M + (N - M).GetSafeNormal() * TgWheelbase; if (!Safe(N, M)) return false; } return true; };
	float Best = 1e9f, Dt = -2.f; int32 Any = INDEX_NONE;
	for (int32 i = 0; i < Path.Num(); ++i)
	{
		const FVector2f To = Path[i] - Main; const float L = To.Size(), Dot = L > 0.1f ? FVector2f::DotProduct(To / L, -D0) : -1.f;
		if (Dot > Dt) { Dt = Dot; Any = i; }
		if (L > 2.f && L < Best && Dot > 0.5f && Back(i)) { Best = L; Step = i; }
	}
	if (Step != INDEX_NONE) { ADP_LOG(TEXT("Tugs: %s - its crew push it back %.0f m onto point %d of the push line"), *Key, Best, Step); return; }
	Step = Any;
	ADP_WARN(TEXT("Tugs: %s - no clear way back onto the push line from (%.1f, %.1f)%s%s: straightened along it to point %d"), *Key, Main.X, Main.Y, Last ? TEXT(" - ") : TEXT(""), Last ? Last : TEXT(""), Step);
}

/** What holds it, and why nobody is at it (your own hired people asked, none free). */
FString ATugs::YouShort(const FTug& T, const TCHAR* What) const
{
	if (!T.bHired || T.Handler != INDEX_NONE || T.Walkers.Num()) return What;
	const UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); const FString W = Sp ? Sp->WaitsFor(TEXT("you:") + T.Key) : FString();
	return FString(What) + (W.IsEmpty() ? FString(TEXT(" - a handler is being called")) : TEXT(" - ") + W);
}

/** Your push of your own flight: a hired handler for the ground power, the chocks and the headset (asked every 2 s; HiredTick). */
void ATugs::YouCrew(int32 Tug, bool bLive, const FDateTime& When, const FString& Flight)
{
	FTug& T = Tugs[Tug]; UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
	if (!Sp || !T.bYouPush || T.bYouDone || T.bYouCrew || !bLive) return;
	if (!Sp->Hires())
	{
		ADP_LOG(TEXT("Tugs: your push of %s - no hired crew any more, the gate's own help"), *T.Key);
		if (Crew.IsValidIndex(T.Handler)) { Crew[T.Handler].Follow = nullptr; LeaveAct(Crew[T.Handler]); }
		HiredEnd(T); return;
	}
	if (T.Handler != INDEX_NONE || T.GpuCi != INDEX_NONE) return;
	TArray<UStaffPeople::FTaken> Team;
	if (!Sp->AskTeam(TEXT("you:") + T.Key, EStaffTask::PushHelp, When, T.At, FString::Printf(TEXT("%s pushback"), *Flight), Team) || !Team.Num()) return;
	const int32 Ci = Enlist(Team[0].A.Get(), Team[0].Id, UStaffPeople::DutyName(Team[0].Duty)); if (Ci == INDEX_NONE) return;
	T.Handler = Ci; FCrew& H = Crew[Ci]; const float Sx = T.StandX;
	ADP_LOG(TEXT("Tugs: %s at x %.0f - %s is the handler of your push"), *Flight, Sx, *H.Name);
	SafeWay(H, Sx);
	const bool bAt = Crew.IsValidIndex(T.YouGpuCi) && !Crew[T.YouGpuCi].bSlotFree && Crew[T.YouGpuCi].Q.Num() && T.Walkers.Contains(T.YouGpuCi);
	if (T.Gpu.bConnected && !bAt) { T.YouGpuCi = Ci; GpuDisconnect(Tug, H); }
	if (!T.bYouGo) { FCrewAct A; A.Kind = FCrewAct::Walk; A.To = [this, Sx] { return GateT(Sx, Sx - 18.5f, 104.f); }; A.Stop = 60.f; H.Q.Add(A); }
	else H.Follow = [this, Tug] { const FTug& U = Tugs[Tug]; return U.Fit - U.Fwd * 60.f - TgL(U.Fwd) * 230.f; };
}

/** The crew's path as dashes on the ground (none: taken away). */
void ATugs::YouMarks(const FTug* T, const TArray<FVector2f>& Path)
{
	if (YouLine) YouLine->ClearInstances();
	if (!T || !Path.Num()) return;
	if (!YouLine)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Hangar/M_HangarLiner.M_HangarLiner"));
		if (!Cube || !Mat) { ADP_WARN(TEXT("Tugs: the push line's mesh or material is missing")); return; }
		YouLine = NewObject<UInstancedStaticMeshComponent>(this); YouLine->SetStaticMesh(Cube); YouLine->SetMaterial(0, Mat);
		YouLine->SetCollisionEnabled(ECollisionEnabled::NoCollision); YouLine->SetCastShadow(false); YouLine->SetMobility(EComponentMobility::Movable);
		YouLine->SetupAttachment(GetRootComponent()); YouLine->RegisterComponent();
	}
	FVector2f A = T->YouStart; float Left = 1.f;
	for (const FVector2f& B : Path)
	{
		const FVector2f Ab = B - A; const float L = Ab.Size(); if (L < 0.01f) continue;
		const FVector2f D = Ab / L;
		for (; Left < L; Left += 2.f)
		{
			const FVector2f C = A + D * Left;
			YouLine->AddInstance(FTransform(FVector(D.X, -D.Y, 0.f).Rotation(), Plan(C.X, C.Y) + FVector(0, 0, 2.f), FVector(1.f, 0.12f, 0.004f)), true);
		}
		Left -= L; A = B;
	}
}

/** Clear of the aircraft it pushed: to the side of it past its wing, or far off. */
bool ATugs::YouClear(const FTug& T, FString* Why) const
{
	auto Off = [&T](const FVector& P, float Side) { const FVector R = P - T.Fit; const float A = FVector::DotProduct(R, T.Fwd); return FMath::Abs(FVector::DotProduct(R, TgL(T.Fwd))) > Side || A < -4500.f || A > 20000.f; };
	if (!Off(T.At, 2300.f)) { if (Why) *Why = TEXT("drive the tug clear to the side - past the wing tip"); return false; }
	const APlayerController* PC = GetWorld()->GetFirstPlayerController(); const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (Me && !T.bYouIn && !Off(Me->GetActorLocation(), 1900.f)) { if (Why) *Why = TEXT("step clear to the side - past the wing"); return false; }
	return true;
}

/** Its flight called off, skipped or gone: everything free again. */
void ATugs::YouDrop(int32 Tug, const TCHAR* Why)
{
	FTug& T = Tugs[Tug]; ADP_LOG(TEXT("Tugs: your push of %s is over - %s"), *T.Key, Why);
	WalkersDone(Tug); YouMarks(nullptr, {});
	T.bYouPush = T.bYouDone = T.bYouGo = T.bYouCrewIn = false; T.YouUn = 0; T.Key.Reset(); T.Seq = 0; T.Talk.Reset(); T.YouWhy.Reset(); T.YouPlane.Reset(); T.YouBend = 0.f; T.bPinShown = true;
	if (T.Handler != INDEX_NONE && !Crew[T.Handler].Hold) { FCrew& H = Crew[T.Handler]; H.Follow = nullptr; if (T.Gpu.Holder != T.Handler) H.Q.Reset(); FCrewAct A; A.Kind = FCrewAct::Walk; A.To = [this, Sx = T.StandX] { return GateT(Sx, Sx - 18.5f, 104.f); }; A.Stop = 80.f; A.Max = 150.f; H.Q.Add(A); }
	if (T.bHired && T.Handler != INDEX_NONE) LeaveAct(Crew[T.Handler]);   // (the tug its gate's own again once they are off: HiredTick)
	if (!YouHook(T)) T.YouStep = 0;
}

/** Each frame of it: called off; after the push, you clear - only then may the aircraft taxi. */
void ATugs::YouAfter(int32 Tug, float Dt)
{
	FTug& T = Tugs[Tug]; if (T.State != EState::You || T.bYouCrew) return;
	if (GetWorld()->GetTimeSeconds() - T.YouBeat > 30.0)
	{
		const ADPTurnaround::FTurn* Tn = ADPTurnaround::AtStand(T.StandX);
		// (off: neither planned nor made from its plan any more, or the aircraft that flies it is at another stand. Once it is made
		//  the turn's own PlanId is the NEXT leg's or empty: the flight's id counts then, as everywhere - HiredLive)
		const UADPSim* Sim = UADPSim::Get(GetWorld());
		const bool bMade = Sim && Sim->World.Flights.ContainsByPredicate([&](const FADPFlight& X) { return X.Id == T.Key; });
		if (T.bTow || T.bHangar) { if (!T.Key.IsEmpty() && !Tows.Contains(T.Key)) { YouDrop(Tug, TEXT("the hangar tow is off")); return; } }
		else if (T.bYouGo || !HiredLive(T.Key) || (!bMade && (!Tn || Tn->PlanId != T.Key))) { YouDrop(Tug, TEXT("the flight is off or skipped")); return; }
	}
	if (!T.bYouDone || T.YouUn < 3 || !T.bPinShown || !YouClear(T)) return;
	ADP_LOG(TEXT("Tugs: you are clear of %s"), *T.Key);
	if (T.bHangar) { T.bTow = false; T.TowWay.Reset(); T.TowBack.Reset(); T.YouTowKey.Reset(); T.YouTowSay.Reset(); T.YouCap = 0.f; }
	T.bYouPush = T.bYouDone = T.bYouGo = T.bYouCrewIn = false; T.YouUn = 0; T.Key.Reset(); T.YouPlane.Reset(); T.YouWhy.Reset();   // (hired: their own last steps, then HiredTick)
}

/** You left it (40 m, 2 min): its crew finish from where it stands; it waits where they can't. */
void ATugs::YouTake(int32 Tug)
{
	FTug& T = Tugs[Tug];
	auto Stay = [&](const FString& Why) { if (T.YouWhy != Why) ADP_LOG(TEXT("Tugs: %s waits - %s"), *T.Key, *Why); T.YouWhy = Why; T.bYouCrewIn = true; };
	if (T.bYouCrew) return;
	UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
	if (T.bHired && (!Sp || !Sp->Hires()))
	{
		if (Crew.IsValidIndex(T.Handler) && Crew[T.Handler].bHired) { Crew[T.Handler].Follow = nullptr; if (!T.bYouDone) LeaveAct(Crew[T.Handler]); }
		HiredEnd(T);
	}
	const bool bDriver = !T.bHired && Crew.IsValidIndex(T.Driver);
	if (!T.bYouDone)
	{
		ADP_LOG(TEXT("Tugs: you left %s - its crew go on with the push"), *T.Key);
		T.YouBend = T.bYouGo ? TgTurn((-T.Fwd).Rotation().Yaw, T.Yaw) : 0.f; T.HookAt = T.At; T.HookYaw = T.Yaw; T.Speed = 0.f;
		if (T.bYouGo) T.PreSeq = FMath::Max(T.PreSeq, 2);
		YouMarks(nullptr, {}); T.bYouPush = T.bYouGo = false;
		if (T.bHired) T.State = EState::Parked;                       // (HiredTick calls a team; their driver climbs in)
		else if (bDriver) { T.State = EState::Boarding; CrewBoard(Tug); }
		else T.State = EState::Hooked;
		return;
	}
	bool bNewH = false;
	if (T.bHired)
	{
		// your own people: a driver, and a handler where there is none and the nose gear still has something on it
		const bool bNeed = T.Handler == INDEX_NONE && T.YouUn < 3;
		FDateTime When; FString Flight; HiredLive(T.Key, &When, &Flight);
		TArray<UStaffPeople::FTaken> Team; const FString Ask = TEXT("youend:") + T.Key;
		if (!Sp->AskTeam(Ask, bNeed ? EStaffTask::Pushback : EStaffTask::PushDriver, When, T.At, FString::Printf(TEXT("%s tug"), *Flight), Team))
		{ const FString W = Sp->WaitsFor(Ask); Stay(TEXT("the tug waits: ") + (W.IsEmpty() ? FString(TEXT("a driver is being called")) : W)); return; }
		int32 Dr = INDEX_NONE, Hn = INDEX_NONE;
		for (const UStaffPeople::FTaken& K : Team) (K.Duty == EStaffDuty::TugDriver ? Dr : Hn) = Enlist(K.A.Get(), K.Id, UStaffPeople::DutyName(K.Duty));
		if (Dr == INDEX_NONE || (bNeed && Hn == INDEX_NONE)) { for (const int32 c : {Dr, Hn}) if (c != INDEX_NONE) Discharge(c); Stay(TEXT("no room for its crew")); return; }
		T.Driver = Dr; Crew[Dr].bDriver = true; SafeWay(Crew[Dr], T.StandX);
		if (bNeed) { T.Handler = Hn; bNewH = true; SafeWay(Crew[Hn], T.StandX); }
	}
	else if (!bDriver)
	{
		// (a gate without a crew: its tug goes by itself, as after such a gate's own push)
		ADP_LOG(TEXT("Tugs: you left %s - no crew at this gate, the tug goes home"), *T.Key);
		T.bYouPush = T.bYouDone = T.bYouGo = false; T.YouUn = 0; T.YouStep = 0; T.YouWhy.Reset(); T.bPinShown = true;
		if (T.Seq == 10 || T.Seq == 12) { ++T.Seq; T.SeqT = 0.f; }
		T.HookAt = T.At; T.HookYaw = T.Yaw; T.Speed = 0.f; T.State = EState::Unhook; T.Clock = 0.f; Engine(T, true);
		return;
	}
	T.YouWhy.Reset();
	T.bYouCrew = true; ADP_LOG(TEXT("Tugs: you left %s - its driver goes on (uncoupled %d of 3)"), *T.Key, T.YouUn);
	ClimbIn(Crew[T.Driver], Tug, [this, Tug, bNew = bNewH]
	{
		FTug& U = Tugs[Tug]; if (U.State != EState::You || !U.bYouPush) return;
		const bool bOff = U.YouUn >= 3; Engine(U, true); U.bYouPush = U.bYouDone = U.bYouGo = false; U.bYouCrew = false; U.YouStep = 0;
		U.HookAt = U.At; U.HookYaw = U.Yaw; U.State = EState::Unhook; U.Clock = 0.f;
		if (bOff) { U.YouUn = 0; if (U.Handler == INDEX_NONE) { U.bPinShown = true; if (U.Seq == 12) { U.Seq = 13; U.SeqT = 0.f; U.SeqWait = FMath::FRandRange(2.f, 5.f); } } StartBacking(U); return; }
		if (U.Handler != INDEX_NONE) { FCrew& H = Crew[U.Handler]; if (!bNew) H.Q.Reset(); H.Follow = nullptr; }
		CrewUncouple(Tug); U.YouUn = 0;   // (from the step you left)
	});
}

// ------------------------------------------------------------------ the seat line (AGSE's)

void AGSE::TugSeats()
{
	ATugs* G = Tg(); if (bTugSeats || !G || !G->bReady) return;
	bTugSeats = true;
	for (int32 i = 0; i < G->Tugs.Num(); ++i)
	{
		ATugs::FTug& U = G->Tugs[i]; ABabylonModel* M = U.M.Get(); if (!M) continue;
		FTractor T; T.TugIx = i; T.M = U.M; T.Name = U.bHangar ? TEXT("hangar tug") : TEXT("pushback tug"); T.At = T.Home = U.At; T.Yaw = T.HomeYaw = U.Yaw;
		for (FVector& V : T.KWheel) V = FVector::ZeroVector;
		for (FVector& V : T.KKing) V = FVector::ZeroVector;
		T.KHinge = FVector(57.f, -119.25f, 0.f); T.KStand = FVector(8.f, -178.f, 0.f); T.KHandle = FVector(-5.f, -124.f, 118.f);
		T.KStep = T.KGrabOut = T.KGrabIn = T.KHandle; T.KSeat = FVector(-14.f, -70.75f, 99.f); T.KBrake = FVector(33.f, -70.75f, 72.f); T.KAccel = FVector(33.f, -58.8f, 73.f);
		T.KWheelC = TgWheelC; T.KHandL = FVector(31.8f, -88.6f, 143.6f); T.KHandR = FVector(31.8f, -52.9f, 143.6f);
		T.KHalfL = 255.f; T.KHalfW = 126.f; T.KCentreX = 0.f; T.KTurnR = 550.f; T.KDoorDeg = 72.f; T.SndDoorOpen = TEXT("TugDoorOpen");
		// the cab's parts it has: the wheel, two pedals, the park brake's lever, the drive selector, the horn on the wheel's hub
		const FVector PivB(-11.f, 7.3f, 63.f), PivG(-1.f, -42.8f, 119.5f);
		T.Pt.Add(TEXT("TR_Hitch"), FVector(TgHeadAhead, 0.f, 45.f)); T.Pt.Add(TEXT("IK_Foot_Brake"), T.KBrake);
		T.Pt.Add(TEXT("CAB_Horn"), TgWheelC);
		if ((T.GCab[0] = M->BindGroupWhere([](const FString& N, int32, const FVector&) { return N == TEXT("ParkBrake") || N.StartsWith(TEXT("ParkBrake_")); }, FTransform(PivB))) != INDEX_NONE)
		{ T.Pt.Add(TEXT("PIV_ParkBrake"), PivB); T.Pt.Add(TEXT("IK_ParkBrake"), FVector(-11.f, 7.3f, 105.f)); }
		if ((T.GCab[1] = M->BindGroup({TEXT("DriveSelector")}, true, FTransform(PivG))) != INDEX_NONE)
		{ T.Pt.Add(TEXT("PIV_GearSelect"), PivG); T.Pt.Add(TEXT("IK_GearSelect"), FVector(-1.f, -42.75f, 127.f)); }
		U.YouWheelG = M->BindGroup({TEXT("SteeringWheel"), TEXT("SteeringHub"), TEXT("SteeringSpoke")}, true, FTransform(TgWheelC));
		for (int32 p = 0; p < 2; ++p) T.GPedal[p] = M->BindGroup({FString::Printf(TEXT("Pedal_%d"), p), FString::Printf(TEXT("PedalArm_%d"), p)}, true, FTransform(TgPedal[p]));
		if (T.GCab[0] == INDEX_NONE || U.YouWheelG == INDEX_NONE) ADP_WARN(TEXT("Tugs: tug %d's cab: no park brake lever or steering wheel found in the model"), i);
		Tractors.Add(T);
	}
}

void AGSE::YouTug(int32 Ti, float Dt)
{
	ATugs* G = Tg(); FTractor& T = Tractors[Ti]; if (!G || !G->Tugs.IsValidIndex(T.TugIx)) return;
	ATugs::FTug& U = G->Tugs[T.TugIx]; using ES = ATugs::EState;
	T.DoorAng = U.DoorAng; T.DoorWant = U.DoorWant;
	const bool bIn = T.Player.IsValid(); U.bYouIn = bIn;
	if (bIn && U.State == ES::Parked)
	{
		// (yours from the moment you go to its door: no flight and no crew takes it)
		if (U.bHired) G->HiredEnd(U);
		U.State = ES::You; U.YouLeft = 0.f; U.bReverse = false; ADP_LOG(TEXT("Tugs: you take tug %d"), T.TugIx);
	}
	if (U.State == ES::Parked && U.YouStep > 0 && U.Key.IsEmpty()) U.State = ES::You;   // (its flight called off: still on the nose)
	if (U.State != ES::You)
	{
		U.bYouCrew = false; U.YouLeft = 0.f; U.YouSig = 0; U.bYouHaz = false; U.bYouNoBeacon = false; U.YouBeam = 0; U.bYouBack = false; U.bYouPush = U.bYouGo = U.bYouDone = false;
		if (U.State == ES::Hooked || U.State == ES::Home || U.State == ES::Backing) U.YouStep = 0;
		T.At = U.At; T.Yaw = U.Yaw; T.Speed = 0.f; T.Steer = U.Steer; return;
	}
	if (bIn && T.PlayerSeat == 0)
	{
		const int32 Ns = FMath::Clamp(FMath::CeilToInt(Dt / 0.03f), 1, 5);
		for (int32 n = 0; n < Ns; ++n) SvcPlayerDrive(T, FMath::Min(Dt, 0.15f) / Ns);
		T.Steer = U.Steer;
	}
	else { T.Speed = 0.f; T.PedalA = T.PedalB = 0.f; T.SteerWant = 0.f; }
	// on the fitting: the bar's head drawn home the last centimetres
	if (U.YouStep > 0 && U.YouStep < 4 && !bIn && !U.bYouGo && !U.bYouDone)
	{
		T.At = FMath::VInterpConstantTo(T.At, U.HookAt, Dt, 12.f); T.Yaw = FMath::FixedTurn(T.Yaw, U.HookYaw, 2.f * Dt);
		if (!T.At.Equals(U.At, 0.01f) || T.Yaw != U.Yaw) Pose(T);
	}
	U.At = T.At; U.Yaw = T.Yaw; U.Speed = T.Speed; U.bYouPark = T.bPark;
	U.YouSig = T.Sig; U.bYouHaz = T.bSwHaz; U.bYouNoBeacon = T.bSwNoBeacon; U.YouBeam = T.Beam; U.bYouBack = bIn && T.bReverse;
	CabTick(T, Ti, Dt);
	if (ABabylonModel* M = U.M.Get())
	{
		// the wheel and the pedals under your hands and feet
		const float Want = -U.Steer * 4.f;
		if (U.YouWheelG != INDEX_NONE && !FMath::IsNearlyEqual(U.YouWheelNow, Want, 0.05f))
		{ U.YouWheelNow = FMath::FInterpTo(U.YouWheelNow, Want, Dt, 10.f); M->MoveGroup(U.YouWheelG, FTransform(FQuat(TgWheelAxis.GetSafeNormal(), FMath::DegreesToRadians(U.YouWheelNow)), TgWheelC)); }
		for (int32 p = 0; p < 2; ++p)
			if (const float Pr = p == 0 ? T.PedalA : T.PedalB; T.GPedal[p] != INDEX_NONE && !FMath::IsNearlyEqual(U.YouPedal[p], Pr, 0.005f))
			{ U.YouPedal[p] = Pr; M->MoveGroup(T.GPedal[p], FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(-10.f * Pr)), TgPedal[p])); }
		M->FlushMoves();
	}
	U.YouNote = FMath::Max(0.f, U.YouNote - Dt);
	if (bIn)
	{
		U.YouLeft = 0.f;
		if (AServiceDrive* D = T.Player.Get(); D && T.PlayerSeat == 0 && U.YouNote <= 0.f)
		{
			U.YouNote = 1.5f;
			if (U.bYouPush)
			{
				const bool bGas = D->Throttle() > 0.1f || D->Brake() > 0.1f; FString Wy;
				if (U.bYouDone) { if (U.YouUn < 1) { if (bGas) AVehicleControls::Note(GetWorld(), FString(TEXT("Brakes set - get out and take the tow bar off")) + (U.YouWhy.IsEmpty() ? FString() : TEXT(" (") + U.YouWhy + TEXT(")"))); } else if (U.YouUn < 3) AVehicleControls::Note(GetWorld(), TEXT("The bypass pin and the headset are still on the aircraft")); else if (!G->YouClear(U, &Wy)) AVehicleControls::Note(GetWorld(), FString(TEXT("The aircraft waits - ")) + Wy); }
				else if (U.bTow) AVehicleControls::Note(GetWorld(), !G->YouMay(U) ? FString(TEXT("The tow waits - ")) + (U.YouWhy.IsEmpty() ? TEXT("not cleared yet") : *U.YouWhy) : U.YouTowSay);
				else if (!G->YouMay(U)) { if (bGas) AVehicleControls::Note(GetWorld(), FString(TEXT("The push waits - ")) + (U.YouWhy.IsEmpty() ? TEXT("the flight isn't ready yet") : *U.YouWhy)); }
				else if (U.YouToGo == 0.f) AVehicleControls::Note(GetWorld(), TEXT("In place - stop and set the park brake"));
				else AVehicleControls::Note(GetWorld(), FString::Printf(TEXT("Push: main wheels %.0f m from the taxilane line, the nose %.0f deg to its %s, bar %.0f deg"), U.YouSide, FMath::Abs(U.YouTurn), U.YouTurn > 0.f ? TEXT("left") : TEXT("right"), FMath::Abs(U.YouBar)));
				return;
			}
			if (U.YouStep > 0) { if (D->Throttle() > 0.1f || D->Brake() > 0.1f) AVehicleControls::Note(GetWorld(), TEXT("The tow bar is on the aircraft - take it off first")); return; }
			if (!G->YouHook(U)) return;
			// up to the nose gear: how far, how far aside, how much to turn (as the belt loader's)
			const FVector F = FRotator(0.f, U.HookYaw, 0.f).Vector(), To = U.HookAt - T.At;
			const float Ah = FVector::DotProduct(To, F), Sd = FVector::DotProduct(To, FVector(-F.Y, F.X, 0.f)), Tn = TgTurn(T.Yaw, U.HookYaw);
			if (FMath::Abs(Ah) > 1500.f || FMath::Abs(Sd) > 500.f || FMath::Abs(Tn) > 45.f) return;
			if (To.Size2D() < 25.f && FMath::Abs(Tn) < 4.f) AVehicleControls::Note(GetWorld(), T.bPark ? TEXT("The bar is at the nose gear - get out and put its head on") : TEXT("The bar is at the nose gear - the park brake on"));
			else AVehicleControls::Note(GetWorld(), FString::Printf(TEXT("Nose gear: %.1f m %s, %.1f m to the %s, turn %.0f deg %s"), FMath::Abs(Ah) / 100.f, Ah >= 0.f ? TEXT("ahead") : TEXT("too far"), FMath::Abs(Sd) / 100.f, Sd >= 0.f ? TEXT("right") : TEXT("left"), FMath::Abs(Tn), Tn >= 0.f ? TEXT("right") : TEXT("left")));
		}
		return;
	}
	if (U.YouStep == 0 && !U.bYouPush && !U.bYouCrew && FVector::Dist2D(U.At, U.Home) < 60.f && FMath::Abs(TgTurn(U.Yaw, U.HomeYaw)) < 5.f && U.DoorAng < 1.f)
	{ U.State = ES::Parked; G->Engine(U, false); ADP_LOG(TEXT("Tugs: you brought tug %d back to its box"), T.TugIx); return; }
	if (U.bYouCrew) return;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController(); const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (U.bYouPush || U.YouStep == 3)
	{
		U.YouLeft = !Me || FVector::DistSquared2D(Me->GetActorLocation(), U.At) > FMath::Square(TgLeave) ? U.YouLeft + Dt : 0.f;
		if (U.bYouPush) { if (U.YouLeft > TgWait) { G->YouTake(T.TugIx); U.YouLeft = FMath::Min(U.YouLeft, TgWait - 5.f); } return; }
		if (G->YouHook(U)) return;   // (coupled, no flight called yet: it stays on)
	}
	const bool bGone = U.YouStep > 0 && !G->YouHook(U);   // (its aircraft's chocks off or the aircraft gone: nothing to stay on for)
	U.YouLeft = (U.YouStep == 0 || bGone) && (!Me || FVector::DistSquared2D(Me->GetActorLocation(), U.At) > FMath::Square(TgLeave)) ? U.YouLeft + Dt : 0.f;
	if (U.YouLeft > TgWait) { U.YouLeft = 0.f; G->YouHome(T.TugIx); }
}

// ------------------------------------------------------------------ at the nose gear, on foot

FVector AGSE::TugSpot(const FTractor& T, int32 S) const
{
	const ATugs* G = Tg(); if (!G || !G->Tugs.IsValidIndex(T.TugIx)) return T.At;
	const ATugs::FTug& U = G->Tugs[T.TugIx]; const FVector F = U.Fwd, L = TgL(F), Gr = U.Fit - F * 60.f;
	if (S == TgShow) return U.Fit + F * 300.f + L * 450.f + FVector(0, 0, 150.f);
	return S == TgHead ? U.Fit + F * 8.f + FVector(0, 0, 48.f) : S == TgPin ? Gr + L * 22.f + FVector(0, 0, 62.f) : Gr + L * 25.f + FVector(0, 0, 135.f);
}

FString AGSE::TugSay(const FTractor& T, int32 S) const
{
	ATugs* G = Tg(); if (!G || !G->Tugs.IsValidIndex(T.TugIx) || T.Player.IsValid()) return FString();
	ATugs::FTug& U = G->Tugs[T.TugIx]; if (U.State != ATugs::EState::You || U.bYouCrew) return FString();
	if (U.bYouPush)
	{
		// (pushed: the bar, the pin, the headset, the pin shown - in that order. Your hangar tow: E at the bar on the stop mark ends it)
		if (U.bTow && U.bYouGo && !U.bYouDone) return S == TgHead ? TEXT("End the tow here (on the stop mark)") : TEXT("");
		if (!U.bYouDone || U.Seq < 10) return FString();
		if (S == TgHead) return U.YouUn == 0 ? TEXT("Take the tow bar's head off") : TEXT("");
		if (S == TgPin) return U.YouUn == 1 ? TEXT("Take the steering bypass pin out") : TEXT("");
		if (S == TgPhone) return U.YouUn == 2 && U.Seq >= 12 ? TEXT("Unplug the headset") : TEXT("");
		return U.YouUn == 3 && !U.bPinShown && U.Handler == INDEX_NONE ? TEXT("Show the bypass pin to the cockpit") : TEXT("");
	}
	if (S == TgShow) return FString();
	if (!G->YouHook(U) || FVector::Dist2D(U.At, U.HookAt) > 25.f || FMath::Abs(TgTurn(U.Yaw, U.HookYaw)) > 4.f) return FString();
	switch (S)
	{
	case TgHead: return U.YouStep == 0 ? TEXT("Put the tow bar's head on and pin it") : U.YouStep == 1 ? TEXT("Take the tow bar's head off") : TEXT("");
	case TgPin: return U.YouStep == 1 ? TEXT("Put the steering bypass pin in") : U.YouStep == 2 ? TEXT("Take the steering bypass pin out") : TEXT("");
	default: return U.YouStep == 2 ? TEXT("Plug the headset in") : U.YouStep == 3 ? TEXT("Unplug the headset") : TEXT("");
	}
}

bool AGSE::TugMay(int32 Ti, int32 S, FString& Why) const
{
	const FTractor& T = Tractors[Ti];
	if (TugSay(T, S).IsEmpty()) { Why = TEXT("not now"); return false; }   // (the chocks in, the aircraft on its own brakes, the bar at the fitting: YouHook)
	if (!T.bPark) { Why = TEXT("the tug's park brake first"); return false; }
	const APlayerController* PC = GetWorld()->GetFirstPlayerController(); const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const float D = Me ? FVector::Dist2D(Me->GetActorLocation(), TugSpot(T, S)) : 1e6f;
	if (D > 100.f) { Why = TEXT("step closer"); return false; }
	if (D < 35.f) { Why = TEXT("step back a little"); return false; }
	return true;
}

TFunction<void(float, float, FDriverPose&)> AGSE::TugAct(int32 Ti, int32 S, float& Secs) const
{
	// the crew's own movement (CrewCouple), from where you stand
	const APlayerController* PC = GetWorld()->GetFirstPlayerController(); const APawn* Me = PC ? PC->GetPawn() : nullptr; if (!Me || S < TgHead || S > TgShow) return nullptr;
	const FVector Hands = TugSpot(Tractors[Ti], S); FVector At = Me->GetActorLocation(); At.Z = Tg()->Plan(At.X / 100.f, -At.Y / 100.f).Z;
	const ATugs::FTug& Ut = Tg()->Tugs[Tractors[Ti].TugIx];
	const FVector Face = ((S == TgShow ? Ut.Fit - Ut.Fwd * 300.f : Hands) - At).GetSafeNormal2D(); Secs = S == TgHead ? 3.f : S == TgPin ? 2.8f : S == TgShow ? 5.f : 2.f;
	TWeakObjectPtr<AGSE> Self(const_cast<AGSE*>(this)); TSharedRef<bool> Done = MakeShared<bool>(false);
	return [Self, Ti, S, At, Face, Hands, Done](float K, float PH, FDriverPose& P)
	{
		const float E = FMath::SmoothStep(0.f, 0.25f, K) * (1.f - FMath::SmoothStep(0.75f, 1.f, K));
		P = S == TgShow ? ADPPose::Reach(At, Face, 1, At + Face * 35.f + TgL(Face) * 25.f + FVector(0, 0, 195.f), 0.8f, PH, E) : S == TgPhone ? ADPPose::Reach(At, Face, 0, Hands, 0.6f, PH, E) : ADPPose::Squat(At, Face, Hands, PH, E);
		AGSE* G = Self.Get(); if (!G || *Done || K < 0.75f) return;
		*Done = true; ATugs* Tu = G->Tg(); const int32 Ix = G->Tractors.IsValidIndex(Ti) ? G->Tractors[Ti].TugIx : INDEX_NONE; if (!Tu || !Tu->Tugs.IsValidIndex(Ix)) return;
		if (G->TugSay(G->Tractors[Ti], S).IsEmpty()) return;   // (a crew or the aircraft was quicker)
		ATugs::FTug& U = Tu->Tugs[Ix]; const int32 Was = U.YouStep;
		if (U.bYouPush && U.bTow && U.bYouGo && !U.bYouDone)
		{
			FString Hint;
			if (!AHangarTow::YouEnd(G->GetWorld(), U.Key, Hint)) AVehicleControls::Note(G->GetWorld(), Hint);
			else AVehicleControls::Note(G->GetWorld(), TEXT("On the stop mark - brakes set. Take the tow bar off"));
			return;
		}
		if (U.bYouPush)
		{
			U.YouUn = S - TgHead + 1;
			if (S == TgHead) { U.YouStep = 0; G->Sound(TEXT("TugHitchClunk"), Hands); }
			else if (S == TgPin && U.Seq == 10) { U.Seq = 11; U.SeqT = 0.f; U.SeqWait = FMath::FRandRange(2.f, 5.f); }
			else if (S == TgShow) { U.bPinShown = true; if (U.Seq == 12) { U.Seq = 13; U.SeqT = 0.f; U.SeqWait = FMath::FRandRange(2.f, 5.f); } }
			ADP_LOG(TEXT("Tugs: tug %d - you uncouple %s (%d of 4)"), Ix, *U.Key, U.YouUn);
			if (S == TgShow || (S == TgPhone && U.Handler != INDEX_NONE)) AVehicleControls::Note(G->GetWorld(), TEXT("Uncoupled - drive the tug clear to the side"));
			return;
		}
		U.YouStep = S == TgHead ? (Was == 0 ? 1 : 0) : S == TgPin ? (Was == 1 ? 2 : 1) : (Was == 2 ? 3 : 2);
		if (S == TgHead) G->Sound(TEXT("TugHitchClunk"), Hands);
		ADP_LOG(TEXT("Tugs: tug %d - you %s (step %d of 3)"), Ix, S == TgHead ? (Was == 0 ? TEXT("put the bar's head on") : TEXT("took the bar's head off")) : S == TgPin ? (Was == 1 ? TEXT("put the bypass pin in") : TEXT("took the bypass pin out")) : Was == 2 ? TEXT("plugged the headset in") : TEXT("unplugged the headset"), U.YouStep);
		if (U.YouStep == 3 && U.bHangar) AVehicleControls::Note(G->GetWorld(), TEXT("Coupled - the tow begins once its brake rider is aboard and ATC approves it"));
		else if (U.YouStep == 3) AVehicleControls::Note(G->GetWorld(), TEXT("Coupled - stay for the push when the flight is ready, or leave it to its crew"));
	};
}

TSharedPtr<FVehTable> AGSE::TugControls()
{
	using K = FVehControl;
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>();
	auto Add = [&Tb](const TCHAR* Lb, int32 S) { K X; X.Part = TEXT(""); X.Point = TEXT("TR_Hitch"); X.Kind = K::Push; X.Label = Lb; X.Switch = S; Tb->Controls.Add(X); };
	Add(TEXT("Tow bar"), TgHead); Add(TEXT("Bypass pin"), TgPin); Add(TEXT("Headset"), TgPhone); Add(TEXT("Bypass pin shown"), TgShow);
	GpuAdd(*Tb);
	Tb->Frame = [](const AGSE&, const FTractor&, int32) { return FTransform::Identity; };
	Tb->Now = [](const FTractor&, int32) { return 0.f; };
	Tb->Want = [](FTractor&, int32, float) {};
	Tb->May = [](const AGSE&, const FTractor&, int32, float, FString&) { return true; };
	Tb->Stowed = [](const AGSE&, const FTractor&) { return true; };
	Tb->Stow = [](AGSE&, FTractor&) {};
	Tb->Aboard = [](const FTractor&, const FVector&) -> uint8 { return 0; };
	Tb->Spot = [](const AGSE& G, const FTractor& T, const K& X, FVector& W) { if (X.bSeat) return false; W = GpuIs(X.Switch) ? G.GpuSpot(T, X.Switch) : G.TugSpot(T, X.Switch); return true; };
	Tb->Say = [](const AGSE& G, const FTractor& T, const K& X) -> FString { return X.bSeat ? FString(X.Label) : GpuIs(X.Switch) ? G.GpuSay(T, X.Switch) : G.TugSay(T, X.Switch); };
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 S, FString& Why) { return ADPCab::Is(S) || (GpuIs(S) ? G.GpuMay(Ti, S, Why) : G.TugMay(Ti, S, Why)); };
	Tb->Switch = [](AGSE& G, int32 Ti, int32 S) { return GpuIs(S) && G.GpuSwitch(Ti, S); };
	Tb->Climb = [](const AGSE& G, int32 Ti, int32 S, float& Secs) { return GpuIs(S) ? G.GpuAct(Ti, S, Secs) : G.TugAct(Ti, S, Secs); };
	return Tb;
}
