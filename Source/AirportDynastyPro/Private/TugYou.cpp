// To-do 238: the pushback tug with you at its wheel. You drive it as you drive every other vehicle (ServiceDrive.cpp, the cab
// CabControls.cpp): a seat line in AGSE::Tractors stands for each of ATugs' tugs, never a second vehicle. What its crew do at the
// nose gear (the bar's head, the bypass pin, the interphone) you do with the same movements at the same places.
#include "GSE.h"
#include "Tugs.h"
#include "VehicleControls.h"
#include "ServiceDrive.h"
#include "CabControls.h"
#include "CrewPose.h"
#include "BabylonModel.h"
#include "ADPLog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	enum : int32 { TgHead = 301, TgPin, TgPhone };
	constexpr float TgLeave = 4000.f, TgWait = 120.f;       // (its crew come: you 40 m away for 2 min, as the baggage tractor)
	constexpr float TgHeadAhead = 792.25f;                  // (the bar's head ahead of the tug's middle: HeadAhead, Tugs.cpp)
	const FVector TgWheelC(28.f, -70.75f, 138.3f), TgWheelAxis(-0.84f, 0.f, 0.54f);
	const FVector TgPedal[2] = {FVector(42.f, -58.8f, 65.f), FVector(42.f, -70.8f, 65.f)};   // (accelerator, brake)
	float TgTurn(float A, float B) { return FMath::FindDeltaAngleDegrees(A, B); }
	FVector TgL(const FVector& F) { return FVector(-F.Y, F.X, 0.f); }   // (the aircraft's right: the side its crew work on)
}

// ------------------------------------------------------------------ the tug's side

/** Where its bar meets the aircraft standing at its gate; false: none there, or its chocks are not in. */
bool ATugs::YouHook(FTug& T) const
{
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
		if (T.State == EState::You && FMath::Abs(T.StandX - StandX) < 1.f && (T.bYouIn || T.YouStep == 0))
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

// ------------------------------------------------------------------ the seat line (AGSE's)

void AGSE::TugSeats()
{
	ATugs* G = Tg(); if (bTugSeats || !G || !G->bReady) return;
	bTugSeats = true;
	for (int32 i = 0; i < G->Tugs.Num(); ++i)
	{
		ATugs::FTug& U = G->Tugs[i]; ABabylonModel* M = U.M.Get(); if (!M) continue;
		FTractor T; T.TugIx = i; T.M = U.M; T.Name = TEXT("pushback tug"); T.At = T.Home = U.At; T.Yaw = T.HomeYaw = U.Yaw;
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
		U.bYouCrew = false; U.YouLeft = 0.f; U.YouSig = 0; U.bYouHaz = false; U.bYouNoBeacon = false; U.YouBeam = 0; U.bYouBack = false;
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
	if (U.YouStep > 0 && U.YouStep < 4 && !bIn)
	{
		T.At = FMath::VInterpConstantTo(T.At, U.HookAt, Dt, 12.f); T.Yaw = FMath::FixedTurn(T.Yaw, U.HookYaw, 2.f * Dt);
		if (!T.At.Equals(U.At, 0.01f) || T.Yaw != U.Yaw) Pose(T);
	}
	U.At = T.At; U.Yaw = T.Yaw; U.Speed = T.Speed;
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
	if (U.YouStep == 0 && !U.bYouCrew && FVector::Dist2D(U.At, U.Home) < 60.f && FMath::Abs(TgTurn(U.Yaw, U.HomeYaw)) < 5.f && U.DoorAng < 1.f)
	{ U.State = ES::Parked; G->Engine(U, false); ADP_LOG(TEXT("Tugs: you brought tug %d back to its box"), T.TugIx); return; }
	if (U.bYouCrew) return;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController(); const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const bool bGone = U.YouStep > 0 && !G->YouHook(U);   // (its aircraft's chocks off or the aircraft gone: nothing to stay on for)
	U.YouLeft = (U.YouStep == 0 || bGone) && (!Me || FVector::DistSquared2D(Me->GetActorLocation(), U.At) > FMath::Square(TgLeave)) ? U.YouLeft + Dt : 0.f;
	if (U.YouLeft > TgWait) { U.YouLeft = 0.f; G->YouHome(T.TugIx); }
}

// ------------------------------------------------------------------ at the nose gear, on foot

FVector AGSE::TugSpot(const FTractor& T, int32 S) const
{
	const ATugs* G = Tg(); if (!G || !G->Tugs.IsValidIndex(T.TugIx)) return T.At;
	const ATugs::FTug& U = G->Tugs[T.TugIx]; const FVector F = U.Fwd, L = TgL(F), Gr = U.Fit - F * 60.f;
	return S == TgHead ? U.Fit + F * 8.f + FVector(0, 0, 48.f) : S == TgPin ? Gr + L * 22.f + FVector(0, 0, 62.f) : Gr + L * 25.f + FVector(0, 0, 135.f);
}

FString AGSE::TugSay(const FTractor& T, int32 S) const
{
	ATugs* G = Tg(); if (!G || !G->Tugs.IsValidIndex(T.TugIx) || T.Player.IsValid()) return FString();
	ATugs::FTug& U = G->Tugs[T.TugIx]; if (U.State != ATugs::EState::You || U.bYouCrew) return FString();
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
	const APlayerController* PC = GetWorld()->GetFirstPlayerController(); const APawn* Me = PC ? PC->GetPawn() : nullptr; if (!Me || S < TgHead || S > TgPhone) return nullptr;
	const FVector Hands = TugSpot(Tractors[Ti], S); FVector At = Me->GetActorLocation(); At.Z = Tg()->Plan(At.X / 100.f, -At.Y / 100.f).Z;
	const FVector Face = (Hands - At).GetSafeNormal2D(); Secs = S == TgHead ? 3.f : S == TgPin ? 2.8f : 2.f;
	TWeakObjectPtr<AGSE> Self(const_cast<AGSE*>(this)); TSharedRef<bool> Done = MakeShared<bool>(false);
	return [Self, Ti, S, At, Face, Hands, Done](float K, float PH, FDriverPose& P)
	{
		const float E = FMath::SmoothStep(0.f, 0.25f, K) * (1.f - FMath::SmoothStep(0.75f, 1.f, K));
		P = S == TgPhone ? ADPPose::Reach(At, Face, 0, Hands, 0.6f, PH, E) : ADPPose::Squat(At, Face, Hands, PH, E);
		AGSE* G = Self.Get(); if (!G || *Done || K < 0.75f) return;
		*Done = true; ATugs* Tu = G->Tg(); const int32 Ix = G->Tractors.IsValidIndex(Ti) ? G->Tractors[Ti].TugIx : INDEX_NONE; if (!Tu || !Tu->Tugs.IsValidIndex(Ix)) return;
		if (G->TugSay(G->Tractors[Ti], S).IsEmpty()) return;   // (a crew or the aircraft was quicker)
		ATugs::FTug& U = Tu->Tugs[Ix]; const int32 Was = U.YouStep;
		U.YouStep = S == TgHead ? (Was == 0 ? 1 : 0) : S == TgPin ? (Was == 1 ? 2 : 1) : (Was == 2 ? 3 : 2);
		if (S == TgHead) G->Sound(TEXT("TugHitchClunk"), Hands);
		ADP_LOG(TEXT("Tugs: tug %d - you %s (step %d of 3)"), Ix, S == TgHead ? (Was == 0 ? TEXT("put the bar's head on") : TEXT("took the bar's head off")) : S == TgPin ? (Was == 1 ? TEXT("put the bypass pin in") : TEXT("took the bypass pin out")) : Was == 2 ? TEXT("plugged the headset in") : TEXT("unplugged the headset"), U.YouStep);
		if (U.YouStep == 3) AVehicleControls::Note(G->GetWorld(), TEXT("Coupled - its crew come for the push when the flight is ready"));
	};
}

TSharedPtr<FVehTable> AGSE::TugControls()
{
	using K = FVehControl;
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>();
	auto Add = [&Tb](const TCHAR* Lb, int32 S) { K X; X.Part = TEXT(""); X.Point = TEXT("TR_Hitch"); X.Kind = K::Push; X.Label = Lb; X.Switch = S; Tb->Controls.Add(X); };
	Add(TEXT("Tow bar"), TgHead); Add(TEXT("Bypass pin"), TgPin); Add(TEXT("Headset"), TgPhone);
	Tb->Range = 1000.f;
	Tb->Frame = [](const AGSE&, const FTractor&, int32) { return FTransform::Identity; };
	Tb->Now = [](const FTractor&, int32) { return 0.f; };
	Tb->Want = [](FTractor&, int32, float) {};
	Tb->May = [](const AGSE&, const FTractor&, int32, float, FString&) { return true; };
	Tb->Stowed = [](const AGSE&, const FTractor&) { return true; };
	Tb->Stow = [](AGSE&, FTractor&) {};
	Tb->Aboard = [](const FTractor&, const FVector&) -> uint8 { return 0; };
	Tb->Spot = [](const AGSE& G, const FTractor& T, const K& X, FVector& W) { if (X.bSeat) return false; W = G.TugSpot(T, X.Switch); return true; };
	Tb->Say = [](const AGSE& G, const FTractor& T, const K& X) -> FString { return X.bSeat ? FString(X.Label) : G.TugSay(T, X.Switch); };
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 S, FString& Why) { return ADPCab::Is(S) || G.TugMay(Ti, S, Why); };
	Tb->Switch = [](AGSE&, int32, int32) { return false; };
	Tb->Climb = [](const AGSE& G, int32 Ti, int32 S, float& Secs) { return G.TugAct(Ti, S, Secs); };
	return Tb;
}
