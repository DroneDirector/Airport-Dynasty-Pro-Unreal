// To-do 238: the belt loader worked by you - its boom, its belt, its stops and its lamp by its own controls, through what
// its crew use (GSELoader.cpp, GSEBags.cpp); at the bulk door it counts like theirs, and a crew go on with it when it is left.
#include "GSE.h"
#include "VehicleControls.h"
#include "CabControls.h"
#include "ServiceVehicles.h"
#include "Airfield.h"
#include "ADPLog.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	const FVector LdPivot(-320.f, 0.f, 76.f);
	const float LdSill = 9.22f, LdUp = 20.5f, LdOut = 5.69f, LdAft = 19.6f;   // (as its crew: GSELoader.cpp)
	enum : int32 { LdFwd = 1, LdStop, LdBack, LdCab, LdEStop, LdLamp };
	enum : int32 { LdFChassis, LdFBoom, LdFJoy };
	FTransform LdRot(const FVector& P, float Deg) { return FTransform(-P) * FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(Deg)), P); }
}

bool AGSE::LoaderHeld(const FTractor& T, float To) const
{
	// never with you on it; down: never onto anybody under it (the boom rises as T.Boom goes below 0)
	if (T.YouOn) return true;
	if (T.Job == EJob::None && T.bLStop) return true;
	return To > T.Boom && CaterUnder(T, FVector(-140.f, -48.f, 0.f), FVector(395.f, 48.f, 78.f));
}

bool AGSE::LoaderFree(const FTractor& T) const
{
	if (!T.bLoader || T.Job != EJob::None || !T.M.IsValid() || T.Player.IsValid() || T.YouOn || T.LYouX > -1e5f) return false;
	if (T.Boom != 0.f || T.BoomWant != 0.f || T.bBelt || T.BeltBags.Num()) return false;
	return T.LLeftT > 120.f || FVector::Dist2D(T.At, T.Home) < 200.f;   // (not from under your hands)
}

void AGSE::LoaderBelt(FTractor& T, int32 Way)
{
	const bool bBack = Way < 0;
	if (Way != 0 && bBack != T.bBeltBack)
	{
		T.bBeltBack = bBack;
		for (FTractor::FBeltBag& B : T.BeltBags)
		{
			if (bBack && B.Stage <= 1) { B.Stage = 3; B.T = 0.f; }
			else if (!bBack && (B.Stage == 3 || B.Stage == 4)) { B.Stage = 0; B.T = 0.f; }
		}
		// (riding up, each stops behind the one before it in the list)
		T.BeltBags.Sort([](const FTractor::FBeltBag& A, const FTractor::FBeltBag& B) { return A.S > B.S; });
	}
	T.bBelt = Way != 0;
}

void AGSE::YouLoader(int32 Li, float Dt)
{
	FTractor& T = Tractors[Li]; const APlayerController* PC = GetWorld()->GetFirstPlayerController(); const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const bool bAway = !T.Player.IsValid() && !T.YouOn && (!Me || FVector::Dist2D(Me->GetActorLocation(), T.At) > 4000.f);
	T.LLeftT = bAway ? T.LLeftT + Dt : 0.f;
	if (T.YouOn && T.bBelt) LoaderBelt(T, 0);   // (the belt stands with you on it)
	if ((T.LDockT += Dt) < 1.f || (!T.Player.IsValid() && FVector::Dist2D(T.At, T.Home) < 200.f && T.LYouX < -1e5f)) return;
	const float Step = T.LDockT; T.LDockT = 0.f;
	// which aircraft's bulk door it stands at, or how far the nearest one is (once a second, parked)
	const float Was = T.LYouX; T.LYouX = -1e6f; T.bLNear = false; double Best = 1500.0;
	if (T.Speed < 5.f)
		for (const FAirfieldData::FStand& St : AAirfield::Shared().Stands)
		{
			float S = 0.f, N = 0.f, Sp = 36.f;
			if (FVector::Dist2D(Plan(St.X, St.Nose), T.At) > 5500.f || !StandGeo(St.X, S, N, Sp) || Sp > 40.f || !WetPlane(S)) continue;
			float Yw = 0.f; const FVector At = PlanPose(S - LdOut, N + LdAft, 1.f, 0.f, Yw), Fw = FRotator(0.f, T.Yaw, 0.f).Vector(), D = At - T.At;
			if (D.Size2D() > Best) continue;
			Best = D.Size2D(); T.bLNear = true; T.LAhead = float(D | Fw); T.LAside = float(D.X * -Fw.Y + D.Y * Fw.X); T.LTurn = FMath::FindDeltaAngleDegrees(T.Yaw, Yw);
			T.LYouX = T.LAhead > -5.f && T.LAhead < 40.f && FMath::Abs(T.LAside) < 35.f && FMath::Abs(T.LTurn) < 5.f ? S : -1e6f;
		}
	const bool bAt = T.LYouX > -1e5f;
	if (bAt != (Was > -1e5f))
	{
		T.StandX = bAt ? T.LYouX : 0.f; T.JobT = 0.f; T.LWaitT = 0.f;
		if (bAt) ADP_LOG(TEXT("GSE: your %s stands at the bulk hold of the aircraft at stand x %.0f"), *T.Name, T.LYouX);
	}
	if (!bAt) return;
	// its baggage train, and what it waits for
	int32 Train = INDEX_NONE;
	for (int32 i = 0; i < Tractors.Num(); ++i) { const FTractor& B = Tractors[i]; if (B.IsTrain() && FMath::Abs(B.StandX - T.LYouX) < 1.f && (B.Job == EJob::Out || B.Job == EJob::AtStand || B.Job == EJob::Loading)) { Train = i; break; } }
	const bool bWork = Train != INDEX_NONE && Tractors[Train].Job == EJob::Loading;
	const bool bSet = FMath::IsNearlyEqual(T.Boom, -LdSill, 0.3f) && T.bBelt && !T.bLStop;
	T.LWaitT = bWork && (!bSet || Tractors[Train].bArrival) ? T.LWaitT + Step : 0.f;
	if (T.Player.IsValid() || T.YouOn || T.Speed > 1.f) return;
	// left alone (2 min, 40 m), or its train waits for it (a belt not running; arriving bags need a man in the hold): a crew go on
	if (T.LLeftT > 120.f || T.LWaitT > (bWork && Tractors[Train].bArrival ? 5.f : 60.f)) LoaderTake(Li, Train != INDEX_NONE ? Tractors[Train].Key : FString(), Train != INDEX_NONE);
}

void AGSE::LoaderTake(int32 Li, const FString& Key, bool bTrain)
{
	const int32 Ci = Borrow(Tractors[Li].At, nullptr, Li); if (Ci == INDEX_NONE) return;   // (nobody free: asked again in a second)
	FTractor& L = Tractors[Li];
	L.StandX = L.LYouX; L.LYouX = -1e6f; L.LLeftT = L.LWaitT = 0.f; L.bLStop = false; LoaderBelt(L, bTrain && L.bBelt ? 1 : 0);
	L.Driver = Ci; L.Job = EJob::Boarding; L.JobT = 0.f; L.Key = Key; bWorked = true;
	ADP_LOG(TEXT("GSE: a crew go on with the %s you left at the stand x %.0f"), *L.Name, L.StandX);
	ClimbIn(Ci, Li, [this, Li, Ci, bTrain]
	{
		if (bTrain) { LoaderIn(Li); return; }
		// nothing to load: the boom down and home
		FTractor& U = Tractors[Li]; U.Job = EJob::AtStand; U.BoomWant = 0.f;
		WaitFor(Ci, [this, Li](float) { return FMath::IsNearlyEqual(Tractors[Li].Boom, 0.f, 0.05f); });
		Do(Ci, [this, Li] { Later([this, Li] { Motor(Tractors[Li], true); LoaderLeave(Li); }); });
	});
}

TSharedPtr<FVehTable> AGSE::LoaderControls()
{
	using K = FVehControl;
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>(); TArray<K>& Cs = Tb->Controls;
	const FVector No = FVector::ZeroVector, L(0, -1, 0);
	auto Sw = [&Cs](const TCHAR* Pt, const TCHAR* Lb, int32 Fr, const FVector& Out, int32 S, bool bSeat) { K X; X.Part = TEXT(""); X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Frame = Fr; X.Out = Out; X.Switch = S; X.bSeat = bSeat; Cs.Add(X); };
	{ K X; X.Part = TEXT("CabBoomJoystick"); X.Point = TEXT("IK_Hand_BoomLever"); X.Kind = K::Lever; X.Label = TEXT("Boom up / down"); X.Frame = LdFJoy; X.Motion = 0; X.A = 0.f; X.B = LdUp; X.bSeat = true; X.bGrip = true; Cs.Add(X); }
	Sw(TEXT("LD_CabBelt"), TEXT("Belt"), LdFChassis, No, LdCab, true); Sw(TEXT("LD_CabStop"), TEXT("Emergency stop"), LdFChassis, No, LdEStop, true);
	Sw(TEXT("LD_RearStop"), TEXT("Emergency stop"), LdFChassis, FVector(-1, 0, 0), LdEStop, false);
	// (the two panels on the boom: the green button, the two black ones under it, the toggle)
	Sw(TEXT("LD_B0"), TEXT("Belt forward (up)"), LdFBoom, L, LdFwd, false); Sw(TEXT("LD_B1"), TEXT("Belt stop"), LdFBoom, L, LdStop, false); Sw(TEXT("LD_B2"), TEXT("Belt back (down)"), LdFBoom, L, LdBack, false);
	Sw(TEXT("LD_BStop"), TEXT("Emergency stop"), LdFBoom, L, LdEStop, false); Sw(TEXT("LD_BLamp"), TEXT("Work lamp"), LdFBoom, L, LdLamp, false);
	Sw(TEXT("LD_T0"), TEXT("Belt forward (up)"), LdFBoom, L, LdFwd, false); Sw(TEXT("LD_T1"), TEXT("Belt stop"), LdFBoom, L, LdStop, false); Sw(TEXT("LD_T2"), TEXT("Belt back (down)"), LdFBoom, L, LdBack, false);
	Sw(TEXT("LD_TStop"), TEXT("Emergency stop"), LdFBoom, L, LdEStop, false); Sw(TEXT("LD_TLamp"), TEXT("Work lamp"), LdFBoom, L, LdLamp, false);

	Tb->Frame = [](const AGSE&, const FTractor& T, int32 Fr) -> FTransform { return Fr == LdFBoom ? LdRot(LdPivot, T.Boom) : Fr == LdFJoy ? LdRot(FVector(-34.f, -53.5f, 88.f), T.JoyNow) : FTransform::Identity; };
	Tb->Now = [](const FTractor& T, int32) { return -T.Boom; };
	Tb->Want = [](FTractor& T, int32, float To) { T.BoomWant = -To; };
	Tb->End = [](const FTractor& T, int32, float E) { return E > 1.f && T.LYouX > -1e5f ? LdSill : E; };   // (at a door: its sill, as its crew)
	Tb->May = [](const AGSE& G, const FTractor& T, int32, float To, FString& Why)
	{
		if (T.Job != EJob::None) Why = TEXT("its crew have it");
		else if (T.Speed > 1.f || T.Path.Num()) Why = TEXT("it is not parked");
		else if (T.bLStop) Why = TEXT("the emergency stop is pressed");
		else if (T.BeltBags.Num()) Why = TEXT("there are bags on the belt");
		else if (T.YouOn) Why = TEXT("somebody is on the belt");
		else if (G.LoaderHeld(T, -To)) Why = TEXT("somebody stands under the boom");
		return Why.IsEmpty();
	};
	Tb->Say = [](const AGSE& G, const FTractor& T, const K& X) -> FString
	{
		if (X.Motion == 0)
		{
			if (T.LYouX > -1e5f) return FString::Printf(TEXT("%s (%.1f degrees, at the bulk door: %.1f)"), X.Label, -T.Boom, LdSill);
			if (T.bLNear) return FString::Printf(TEXT("%s (%.1f degrees; the bulk door: %.1f m %s, %.1f m to the %s, turn %.0f degrees %s)"), X.Label, -T.Boom, FMath::Abs(T.LAhead) / 100.f, T.LAhead < 0.f ? TEXT("back") : TEXT("ahead"), FMath::Abs(T.LAside) / 100.f, T.LAside < 0.f ? TEXT("left") : TEXT("right"), FMath::Abs(T.LTurn), T.LTurn < 0.f ? TEXT("left") : TEXT("right"));
			return FString::Printf(TEXT("%s (%.1f degrees)"), X.Label, -T.Boom);
		}
		if (X.Switch == LdEStop) return T.bLStop ? TEXT("Pull the emergency stop out") : TEXT("Emergency stop");
		if (X.Switch == LdCab) return T.bBelt ? TEXT("Belt: stop") : TEXT("Belt: forward (up)");
		if (X.Switch == LdLamp) return T.bSwWork ? TEXT("Work lamp: off") : TEXT("Work lamp: on");
		return X.Label;
	};
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 S, FString& Why)
	{
		const FTractor& T = G.Tractors[Ti]; if (S < LdFwd || S > LdLamp) return true;
		if (T.Job != EJob::None) Why = TEXT("its crew have it");
		else if (S == LdFwd || S == LdBack || (S == LdCab && !T.bBelt))
		{
			if (T.bLStop) Why = TEXT("the emergency stop is pressed");
			else if (T.Speed > 1.f) Why = TEXT("it is not parked");
			else if (T.YouOn) Why = TEXT("you are on the belt");
		}
		return Why.IsEmpty();
	};
	Tb->Switch = [](AGSE& G, int32 Ti, int32 S)
	{
		if (S < LdFwd || S > LdLamp) return false;
		FTractor& T = G.Tractors[Ti]; if (T.Job != EJob::None) return true;
		switch (S)
		{
		case LdFwd: G.LoaderBelt(T, 1); break;
		case LdBack: G.LoaderBelt(T, -1); break;
		case LdStop: G.LoaderBelt(T, 0); break;
		case LdCab: G.LoaderBelt(T, T.bBelt ? 0 : 1); break;
		case LdLamp: T.bSwWork = !T.bSwWork; break;
		default: T.bLStop = !T.bLStop; if (T.bLStop) { G.LoaderBelt(T, 0); T.BoomWant = T.Boom; } break;
		}
		return true;
	};
	// your feet on the belt (its own line, as the boom stands)
	Tb->Aboard = [](const FTractor& T, const FVector& P) -> uint8
	{
		const FVector Q = LdRot(LdPivot, T.Boom).InverseTransformPosition(P);
		return Q.X > -365.f && Q.X < 385.f && FMath::Abs(Q.Y) < 42.f && Q.Z > 72.f && Q.Z < 150.f ? 1 : 0;
	};
	Tb->Stowed = [](const AGSE&, const FTractor& T) { return T.Boom == 0.f && T.BoomWant == 0.f && !T.bBelt && !T.bLStop; };
	// left alone: at a bulk door its crew go on with it (YouLoader); elsewhere the boom comes down
	Tb->Stow = [](AGSE& G, FTractor& T) { if (T.LYouX > -1e5f) return; T.bLStop = false; G.LoaderBelt(T, 0); if (T.BeltBags.Num() == 0) T.BoomWant = 0.f; };
	return Tb;
}
