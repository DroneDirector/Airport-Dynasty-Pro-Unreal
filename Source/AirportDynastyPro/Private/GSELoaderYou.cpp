// To-do 238: the belt loader worked by you - its boom, its belt, its stops and its lamp by its own controls, through what
// its crew use (GSELoader.cpp, GSEBags.cpp); at the bulk door it counts like theirs, and a crew go on with it when it is left.
#include "GSE.h"
#include "VehicleControls.h"
#include "CabControls.h"
#include "ServiceVehicles.h"
#include "Airfield.h"
#include "ADPLog.h"
#include "Tugs.h"
#include "DepartureBelts.h"
#include "BabylonModel.h"
#include "CrewPose.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	const FVector LdPivot(-320.f, 0.f, 76.f);
	const float LdSill = 9.22f, LdUp = 20.5f, LdOut = 5.69f, LdAft = 19.6f;   // (as its crew: GSELoader.cpp)
	enum : int32 { LdFwd = 1, LdStop, LdBack, LdCab, LdEStop, LdLamp, LbTakeCart, LbTakeFoot, LbTakeTop, LbTakeHold, LbPutFoot, LbPutCart, LbPutHold, LbPutTop, LbClimb };
	enum : int32 { LdFChassis, LdFBoom, LdFJoy };
	FTransform LdRot(const FVector& P, float Deg) { return FTransform(-P) * FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(Deg)), P); }
}

bool AGSE::LoaderHeld(const FTractor& T, float To) const
{
	// never with you on it; down: never onto anybody under it (the boom rises as T.Boom goes below 0)
	if (T.YouOn) { T.YouStopAt = GetWorld()->GetTimeSeconds(); return true; }
	if (T.Job == EJob::None && T.bLStop) return true;
	const bool bUnder = To > T.Boom && CaterUnder(T, FVector(-140.f, -48.f, 0.f), FVector(395.f, 48.f, 78.f));
	if (bUnder) T.YouStopAt = GetWorld()->GetTimeSeconds();
	return bUnder;
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
			float Yw = 0.f; const FVector At = PlanPose(St.Frame(), -LdOut, LdAft, 1.f, 0.f, Yw), Fw = FRotator(0.f, T.Yaw, 0.f).Vector(), D = At - T.At;
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
	const bool bTop = T.ArrivalsLeft > 0 || T.BeltBags.ContainsByPredicate([](const FTractor::FBeltBag& B) { return B.Stage == 1; });
	T.LWaitT = bWork && T.LYouBag == INDEX_NONE && (!bSet || bTop || Tractors[Train].bArrival) ? T.LWaitT + Step : 0.f;
	if (T.Player.IsValid() || T.YouOn || T.LYouBag != INDEX_NONE || T.Speed > 1.f) return;
	// left alone (2 min, 40 m), or its train waits for it (a belt not running; arriving bags need a man in the hold): a crew go on
	if (T.LLeftT > 120.f || T.LWaitT > (bWork && Tractors[Train].bArrival ? 45.f : 60.f)) LoaderTake(Li, Train != INDEX_NONE ? Tractors[Train].Key : FString(), Train != INDEX_NONE);
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

	// your hands on its bags, and the way up its belt (they lie where the bags are: Spot; also while its crew work)
	auto Hb = [&Cs](const TCHAR* Lb, int32 S) { K X; X.Part = TEXT(""); X.Point = TEXT("LD_RearStop"); X.Kind = K::Push; X.Label = Lb; X.Frame = LdFChassis; X.Out = FVector::ZeroVector; X.Switch = S; X.bSeat = false; X.bHelp = true; Cs.Add(X); };
	Hb(TEXT("Take the bag off the cart"), LbTakeCart); Hb(TEXT("Take the bag off the belt"), LbTakeFoot); Hb(TEXT("Take the bag off the belt's top"), LbTakeTop); Hb(TEXT("Take a bag out of the hold"), LbTakeHold);
	Hb(TEXT("Put the bag on the belt"), LbPutFoot); Hb(TEXT("Put the bag on the cart"), LbPutCart); Hb(TEXT("Put the bag into the hold"), LbPutHold); Hb(TEXT("Put the bag on the belt's top"), LbPutTop); Hb(TEXT("Walk up the belt"), LbClimb);
	Tb->Range = 2600.f; Tb->bAlways = true;
	Tb->Solids.Add({FVector(-383.f, -108.f, 12.f), FVector(401.f, 108.f, 66.f), LdFChassis, nullptr});
	Tb->Solids.Add({FVector(-95.f, -112.f, 66.f), FVector(60.f, -45.f, 200.f), LdFChassis, nullptr});
	Tb->Solids.Add({FVector(-368.f, -31.f, 70.f), FVector(345.f, 31.f, 97.f), LdFBoom, nullptr});
	Tb->Spot = [](const AGSE& G, const FTractor& T, const K& X, FVector& W)
	{
		if (X.Switch < LbTakeCart) return false;
		int32 c = INDEX_NONE, s = INDEX_NONE;
		switch (X.Switch)
		{
		case LbTakeCart: if (!G.LoaderCartBag(T, c, s)) return false; W = G.CW(G.Carts[c], G.CartSlot(c, s).GetLocation() + FVector(0.f, 0.f, 15.f)); return true;
		case LbPutCart: if (T.LYouBag != INDEX_NONE && T.LYouFrom == 1) { c = T.LYouCart; s = T.LYouSlot; if (!G.Carts.IsValidIndex(c)) return false; } else if (!G.LoaderCartFree(T, c, s, true)) return false; W = G.CW(G.Carts[c], G.CartSlot(c, s).GetLocation() + FVector(0.f, 0.f, 15.f)); return true;
		case LbTakeTop: case LbPutTop: W = G.TW(T, G.BeltOffset(T, 675.f).GetLocation() + FVector(0.f, 0.f, 15.f)); return true;
		case LbTakeHold: case LbPutHold: W = G.TW(T, (FTransform(FVector(70.f, 0.f, 20.f)) * G.BeltOffset(T, 675.f)).GetLocation()); return true;
		case LbClimb: W = G.TW(T, G.BeltOffset(T, T.YouOn ? 20.f : 60.f).GetLocation()); return true;
		default: W = G.TW(T, G.BeltOffset(T, 0.f).GetLocation() + FVector(0.f, 0.f, 12.f)); return true;
		}
	};
	// (there or not: your hands, the bags, where you stand)
	static auto BagMay = [](const AGSE& G, const FTractor& T, int32 S) -> bool
	{
		const bool bHands = T.LYouBag != INDEX_NONE, bTopAt = T.YouOn && T.YouAt.X > 150.0, bSill = FMath::IsNearlyEqual(T.Boom, -LdSill, 0.4f); bool bArr = false; const int32 Tr = G.LoaderTrain(T, &bArr);
		auto Has = [&T](uint8 St) { return T.BeltBags.ContainsByPredicate([St](const FTractor::FBeltBag& B) { return B.Stage == St; }); };
		int32 c, s;
		if (S == LbClimb) return !bHands && !T.Player.IsValid() && (T.Boom == 0.f || bSill) && (T.YouOn ? T.YouAt.X < -170.0 : T.YouAt.X < -330.0);
		if (T.bLOver || (bHands && (T.LYouTo || T.LYouMov < 1.f))) return false;
		switch (S)
		{
		case LbTakeCart: return !bHands && !T.YouOn && G.LoaderCartBag(T, c, s);
		case LbTakeFoot: return !bHands && !T.YouOn && bArr && Has(4);
		case LbTakeTop: return !bHands && bTopAt && bSill && !T.bTopUp && Has(1);
		case LbTakeHold: return !bHands && bTopAt && bSill && !T.bTopUp && Tr != INDEX_NONE && bArr && T.ArrivalsLeft > 0 && !T.BeltBags.ContainsByPredicate([](const FTractor::FBeltBag& B) { return B.S > 675.f - 90.f; });
		case LbPutFoot: return bHands && !T.YouOn && (T.LYouFrom == 1);
		case LbPutCart:
			// (a departing bag: back to the place you took it from)
			if (bHands && T.LYouFrom == 1) return !T.YouOn && G.Carts.IsValidIndex(T.LYouCart) && G.Carts[T.LYouCart].Bags.IsValidIndex(T.LYouSlot) && G.Carts[T.LYouCart].Bags[T.LYouSlot] == INDEX_NONE && !G.SlotTaken.Contains(T.LYouCart * 100 + T.LYouSlot) && FVector::DistSquared2D(G.Carts[T.LYouCart].At, G.TW(T, T.YouAt)) < FMath::Square(260.f);
			return bHands && T.LYouFrom == 2 && G.LoaderCartFree(T, c, s, true);
		case LbPutHold: return bHands && bTopAt && T.LYouFrom == 3;
		case LbPutTop: return bHands && bTopAt && (T.LYouFrom == 3 || T.LYouFrom == 4);
		}
		return false;
	};
	Tb->Held = [](const FTractor& T) -> const TCHAR* { return T.LYouBag != INDEX_NONE ? TEXT("a bag") : nullptr; };
	static auto Hand = [](const AGSE& G, const FTractor& T, FVector& P, float Side) -> bool
	{
		const ADepartureBelts* D = T.LYouBag != INDEX_NONE ? ADepartureBelts::Get(G.GetWorld()) : nullptr; if (!D) return false;
		// (on its two ends: the one on your right for the right hand)
		const FTransform B = G.LoaderBagAt(T); const APlayerController* PC = G.GetWorld()->GetFirstPlayerController(); const APawn* Me = PC ? PC->GetPawn() : nullptr; if (!Me) return false;
		const FVector R = FTransform(FRotator(0.f, T.Yaw, 0.f), T.At).InverseTransformVectorNoScale(Me->GetActorRightVector());
		const FVector X = B.GetUnitAxis(EAxis::X) * 30.f, Y = B.GetUnitAxis(EAxis::Y) * 20.f, V = FMath::Abs(X | R) > FMath::Abs(Y | R) ? X * FMath::Sign(X | R) : Y * FMath::Sign(Y | R);
		P = B.GetLocation() + V * Side + FVector(0.f, 0.f, 12.f); return true;
	};
	Tb->Grip = [](const AGSE& G, const FTractor& T, FVector& P) { return Hand(G, T, P, 1.f); };
	Tb->GripL = [](const AGSE& G, const FTractor& T, FVector& P) { return Hand(G, T, P, -1.f); };
	Tb->Leash = [](const AGSE&, const FTractor& T, FVector& From) { From = FVector::ZeroVector; return 2500.f; };
	Tb->Drop = [](FTractor& T) { if (AGSE* G = T.M.IsValid() ? AGSE::Get(T.M->GetWorld()) : nullptr) G->LoaderDrop(T); };
	Tb->Climb = [](const AGSE& G, int32 Ti, int32 S, float& Secs) -> TFunction<void(float, float, FDriverPose&)> { if (S != LbClimb) return nullptr; Secs = 3.2f; return G.BeltClimb(Ti, !G.Tractors[Ti].YouOn); };

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
		if (X.Switch >= LbTakeCart) return !BagMay(G, T, X.Switch) ? FString() : X.Switch == LbClimb && T.YouOn ? FString(TEXT("Step down off the belt")) : FString(X.Label);
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
		const FTractor& T = G.Tractors[Ti];
		if (S >= LbTakeCart)
		{
			if (S == LbClimb)
			{
				if (T.YouOn) return true;
				if (T.TopCrew != INDEX_NONE) Why = TEXT("the hold person has the belt");
				else if (T.bBelt && T.Job == EJob::None) Why = TEXT("stop the belt first");
				else if (T.Speed > 1.f || !FMath::IsNearlyEqual(T.Boom, T.BoomWant, 0.1f) || (T.Job != EJob::None && T.Job != EJob::AtStand) || T.bLOver) Why = TEXT("it is moving or about to");
			}
			else if (!BagMay(G, T, S)) Why = TEXT("that is no longer there");
			else if (S == LbPutFoot)
			{
				if (T.bLOver || !FMath::IsNearlyEqual(T.Boom, -LdSill, 0.4f)) Why = TEXT("the boom is not at the bulk door");
				else if (T.bBottomHeld || !G.BeltBottomFree(T)) Why = TEXT("the belt's foot is not free yet");
			}
			return Why.IsEmpty();
		}
		if (S < LdFwd || S > LdLamp) return true;
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
		if (S >= LbTakeCart)
		{
			FTractor& T = G.Tractors[Ti]; if (S == LbClimb || !BagMay(G, T, S)) return false;
			auto Off = [&G, &T](uint8 St, uint8 From) { const int32 i = T.BeltBags.IndexOfByPredicate([St](const FTractor::FBeltBag& B) { return B.Stage == St; }); if (i == INDEX_NONE) return; const int32 Id = T.BeltBags[i].Id; T.BeltBags.RemoveAt(i); G.LoaderHand(T, Id, From); };
			switch (S)
			{
			case LbTakeCart: { int32 c, s; if (!G.LoaderCartBag(T, c, s)) return false; const int32 Id = G.Carts[c].Bags[s]; G.Carts[c].Bags[s] = INDEX_NONE; T.LYouCart = c; T.LYouSlot = s; G.LoaderHand(T, Id, 1); break; }
			case LbTakeFoot: Off(4, 2); break;
			case LbTakeTop: Off(1, 3); break;
			case LbTakeHold:
			{
				ADepartureBelts* D = ADepartureBelts::Get(G.GetWorld()); APlayerController* PC = G.GetWorld()->GetFirstPlayerController(); if (!D || !T.M.IsValid()) return false;
				const int32 Id = D->PassengerDrop(PC ? PC->GetPawn() : nullptr, 1, FString::Printf(TEXT("ARR %s %d"), *T.ArrKey, T.ArrivalsLeft), TEXT("ARR ") + T.ArrKey, T.M->GetRootComponent(), NAME_None, false);
				if (Id == INDEX_NONE) return false;
				--T.ArrivalsLeft; G.LoaderHand(T, Id, 4); break;
			}
			case LbPutFoot: G.LoaderLay(T, 1); break;
			case LbPutCart: { if (T.LYouFrom == 1) { G.LoaderLay(T, 2); break; } int32 c, s; if (!G.LoaderCartFree(T, c, s, true)) return false; T.LYouCart = c; T.LYouSlot = s; G.LoaderLay(T, 2); break; }
			case LbPutHold: G.LoaderLay(T, 3); break;
			default: G.LoaderLay(T, T.LYouFrom == 3 ? 0 : 4); if (T.LYouFrom == 3) G.LoaderDrop(T); break;
			}
			return true;
		}
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
