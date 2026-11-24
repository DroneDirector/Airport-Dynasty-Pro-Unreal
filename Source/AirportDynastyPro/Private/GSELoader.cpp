// The belt loader at work (owner 2026-11-21: built after the tractor and cart - Art/BeltLoader, imported as BeltLoader):
//  - parked on the apron beside the Ramp services 13 opening (outside every red line), engine off
//  - a departure's bulk hold: a crew member walks over, climbs into its cab, drives it round behind the aircraft's tail
//    and up its right side (the A320's bulk cargo door is on the right, aft of the wing: ~24.6 m from the nose, its sill
//    2.10 m up - Airbus AC 2-3), turning in square to the fuselage behind the wing and clear of the engine and the tug's
//    way; the boom raised from the cab (the boom lever) until the bumper sits under the sill (-9.22 deg)
//  - out, the belt started at its bottom controls; a second crew member walks up the belt into the hold to take the bags
//    off at the top; the baggage train's driver lifts the bags off the carts onto its bottom end (the bags themselves come
//    with the bag models - until then the movements alone)
//  - the train gone, the hold loader comes down, the belt stopped, the boom lowered, backed straight away from the door,
//    out past the wingtip and the tail and home - away before the push (the push waits: AGSE::BusyAt)
// Its driving, wheels, door, lamps, dirt and climbing are the tractor's (GSE.cpp / GSECrew.cpp) with its own measures.
#include "GSE.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "DepartureBelts.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	// (Art/BeltLoader/beltloader-parts.json, Blender (x, y, z) m -> root (x, -y, z) * 100)
	const FVector GslPivot(-320.f, 0.f, 76.f), GslBumper(386.f, 0.f, 75.5f);
	const FVector GslRamBase[2] = {FVector(-40.f, 0.f, 42.f), FVector(-270.f, 0.f, 42.f)}, GslRamTop[2] = {FVector(170.f, 0.f, 64.f), FVector(-130.f, 0.f, 64.f)};
	const FVector GslBeltBottom(-355.f, 0.f, 97.6f), GslBeltTop(360.f, 0.f, 97.6f), GslStandTop(430.f, 0.f, 94.6f), GslControlsBottom(-350.f, -47.5f, 86.5f);
	const FVector GslStandControls(-350.f, -80.f, 0.f), GslStandLoad(-425.f, -25.f, 0.f), GslLever(-34.f, -53.5f, 98.f);
	constexpr float GslSillDeg = -9.22f;     // (the A320's bulk hold sill, 2.10 m)
	USoundBase* GslSnd(const TCHAR* Name)
	{
		static TMap<FString, USoundBase*> Cache;
		if (USoundBase** F = Cache.Find(Name)) return *F;
		USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/BeltLoader/Sounds/%s.%s"), Name, Name)); if (S) S->AddToRoot();
		Cache.Add(Name, S); return S;
	}
	float GslPause() { return FMath::FRandRange(1.5f, 4.f); }
}

void AGSE::SetKind(FTractor& T, bool bLoader) const
{
	T.bLoader = bLoader;
	if (!bLoader)
	{
		// the baggage tractor (Art/BaggageTractor/baggagetractor-parts.json)
		T.KWheel[0] = FVector(72.f, -56.f, 26.f); T.KWheel[1] = FVector(72.f, 56.f, 26.f); T.KWheel[2] = FVector(-86.f, -56.f, 31.f); T.KWheel[3] = FVector(-86.f, 56.f, 31.f);
		T.KKing[0] = FVector(72.f, -42.f, 26.f); T.KKing[1] = FVector(72.f, 42.f, 26.f);
		T.KHinge = FVector(34.7f, -71.2f, 125.f); T.KWheelC = FVector(46.f, 0.f, 125.f);
		T.KStand = FVector(-5.f, -120.f, 0.f); T.KHandle = FVector(-20.f, -73.5f, 100.f); T.KStep = FVector(-1.f, -63.f, 31.5f);
		T.KGrabOut = FVector(-43.f, -75.f, 141.f); T.KGrabIn = FVector(41.5f, -60.f, 150.f); T.KSeat = FVector(-7.f, 0.f, 92.f);
		T.KBrake = FVector(70.f, -21.f, 77.f); T.KAccel = FVector(70.f, 21.f, 77.f); T.KHandL = FVector(52.4f, -15.6f, 131.4f); T.KHandR = FVector(52.4f, 15.6f, 131.4f);
		T.KRadF = 26.f; T.KRadR = 31.f; T.KWB = 158.f; T.KTrack = 112.f; T.KFloor = 62.f; T.KHalfL = 171.f; T.KHalfW = 72.f; T.KCentreX = 3.f; T.KTurnR = 300.f; T.KMaxSteer = 32.f;
		T.SndStart = TEXT("BTStart"); T.SndLoop = TEXT("BTMotorLoop"); T.SndStop = TEXT("BTStop"); T.SndDoorOpen = TEXT("BTDoorOpen"); T.SndDoorClose = TEXT("BTDoorClose"); T.SndBeep = TEXT("BTReverseBeep");
		return;
	}
	// the belt loader (Art/BeltLoader/beltloader-parts.json): its cab on the left, its door hinged on its front edge
	T.KWheel[0] = FVector(165.f, -80.f, 34.f); T.KWheel[1] = FVector(165.f, 80.f, 34.f); T.KWheel[2] = FVector(-165.f, -80.f, 34.f); T.KWheel[3] = FVector(-165.f, 80.f, 34.f);
	T.KKing[0] = FVector(165.f, -72.f, 34.f); T.KKing[1] = FVector(165.f, 72.f, 34.f);
	T.KHinge = FVector(41.2f, -109.2f, 118.5f); T.KWheelC = FVector(2.f, -80.f, 110.f);
	T.KStand = FVector(-15.f, -155.f, 0.f); T.KHandle = FVector(-50.f, -111.5f, 100.f); T.KStep = FVector(-15.f, -101.f, 26.8f);
	T.KGrabOut = FVector(-88.f, -113.5f, 141.f); T.KGrabIn = FVector(50.f, -98.5f, 150.f); T.KSeat = FVector(-48.f, -80.f, 75.f);
	T.KBrake = FVector(24.f, -97.f, 60.f); T.KAccel = FVector(24.f, -63.f, 60.f); T.KHandL = FVector(7.66f, -93.86f, 115.66f); T.KHandR = FVector(7.66f, -66.14f, 115.66f);
	T.KRadF = 34.f; T.KRadR = 34.f; T.KWB = 330.f; T.KTrack = 160.f; T.KFloor = 45.f; T.KHalfL = 392.f; T.KHalfW = 108.f; T.KCentreX = 9.f; T.KTurnR = 550.f; T.KMaxSteer = 30.f;
	T.SndStart = TEXT(""); T.SndLoop = TEXT("BLDriveLoop"); T.SndStop = TEXT(""); T.SndDoorOpen = TEXT("BLDoorOpen"); T.SndDoorClose = TEXT("BLDoorClose"); T.SndBeep = TEXT("BLReverseBeep");
}

FVector AGSE::BoomW(const FTractor& T, const FVector& BoomLocal) const
{
	const FQuat Qb(FVector::RightVector, FMath::DegreesToRadians(T.Boom));
	return TW(T, GslPivot + Qb.RotateVector(BoomLocal - GslPivot));
}

void AGSE::LoaderParts(FTractor& T, float Dt)
{
	ABabylonModel* M = T.M.Get(); if (!M) return;
	// the boom: raised or lowered by its two rams (~2.2 deg/s: the sill in about 4 s), its sounds; the bumper pad hanging
	// upright on its pin; each ram's barrel turned about its base, its rod's eye kept on the boom
	const bool bMove = !FMath::IsNearlyEqual(T.Boom, T.BoomWant, 0.01f), bHeld = bMove && LoaderHeld(T, T.BoomWant);
	if (bHeld) { if (T.Job == EJob::None) T.BoomWant = T.Boom; if (T.BoomSnd) { T.BoomSnd->Stop(); T.BoomSnd = nullptr; } }
	// (its joystick goes with it)
	const float Joy = !bMove || bHeld ? 0.f : T.BoomWant < T.Boom ? 18.f : -18.f;
	if (T.GJoy != INDEX_NONE && T.JoyNow != Joy) { T.JoyNow = FMath::FInterpConstantTo(T.JoyNow, Joy, Dt, 120.f); M->MoveGroup(T.GJoy, FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(T.JoyNow)), FVector(-34.f, -53.5f, 88.f))); }
	if (bMove && !bHeld)
	{
		if (!T.BoomSnd)
		{
			UGameplayStatics::PlaySoundAtLocation(this, GslSnd(TEXT("BLBoomStart")), TW(T, FVector(-150.f, 0.f, 60.f)));
			T.BoomSnd = UGameplayStatics::SpawnSoundAttached(GslSnd(T.BoomWant < T.Boom ? TEXT("BLBoomUpLoop") : TEXT("BLBoomDownLoop")), M->GetRootComponent(), NAME_None, FVector(-150.f, 0.f, 60.f), EAttachLocation::KeepRelativeOffset, false, 0.8f);
		}
		T.Boom = FMath::FInterpConstantTo(T.Boom, T.BoomWant, Dt, 2.2f);
		const FQuat Qb(FVector::RightVector, FMath::DegreesToRadians(T.Boom));
		if (T.GBoom != INDEX_NONE) M->MoveGroup(T.GBoom, FTransform(Qb, GslPivot));
		if (T.GBumper != INDEX_NONE) M->MoveGroup(T.GBumper, FTransform(GslPivot + Qb.RotateVector(GslBumper - GslPivot)));
		for (int32 r = 0; r < 2; ++r)
		{
			const FVector Top = GslPivot + Qb.RotateVector(GslRamTop[r] - GslPivot), B = GslRamBase[r];
			const float Phi0 = FMath::Atan2(GslRamTop[r].Z - B.Z, GslRamTop[r].X - B.X), Phi = FMath::Atan2(Top.Z - B.Z, Top.X - B.X);
			const FQuat Q(FVector::RightVector, -(Phi - Phi0));   // (a turn about +y lowers the x axis: the barrel's rise is a negative turn)
			if (T.GRam[r * 2] != INDEX_NONE) M->MoveGroup(T.GRam[r * 2], FTransform(Q, B));
			if (T.GRam[r * 2 + 1] != INDEX_NONE) M->MoveGroup(T.GRam[r * 2 + 1], FTransform(Q, Top));
		}
		if (FMath::IsNearlyEqual(T.Boom, T.BoomWant, 0.01f))
		{
			T.Boom = T.BoomWant;
			if (T.BoomSnd) { T.BoomSnd->Stop(); T.BoomSnd = nullptr; }
			UGameplayStatics::PlaySoundAtLocation(this, GslSnd(TEXT("BLBoomStop")), TW(T, FVector(-150.f, 0.f, 60.f)));
		}
	}
	// the belt running (its start and its loop, heard from the boom's middle)
	if (T.bBelt && !T.BeltSnd)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GslSnd(TEXT("BLBeltStart")), BoomW(T, FVector(0.f, 0.f, 100.f)));
		T.BeltSnd = UGameplayStatics::SpawnSoundAttached(GslSnd(TEXT("BLBeltLoop")), M->GetRootComponent(), NAME_None, FVector(0.f, 0.f, 120.f), EAttachLocation::KeepRelativeOffset, false, 0.7f);
	}
	if (!T.bBelt && T.BeltSnd) { T.BeltSnd->Stop(); T.BeltSnd = nullptr; }
}

int32 AGSE::LoaderAt(float StandX) const
{
	for (int32 i = 0; i < Tractors.Num(); ++i) if (Tractors[i].bLoader && (Tractors[i].Job != EJob::None || Tractors[i].LYouX > -1e5f) && FMath::Abs(Tractors[i].StandX - StandX) < 1.f) return i;
	return INDEX_NONE;
}

void AGSE::EnsureLoader(float StandX, const FString& Key)
{
	if (LoaderAt(StandX) != INDEX_NONE) return;
	if (PreLoader == INDEX_NONE && HiredFlight(Key, StandX)) { AddWant(EWant::Loader, Key, StandX); return; }   // (to-do 206: your people - asked for in the queue, GSEStaff.cpp)
	float S = 0.f, N = 0.f, Sp = 36.f; if (!StandGeo(StandX, S, N, Sp) || Sp > 40.f) return;   // (the A320 stands: its bulk door)
	int32 Li = INDEX_NONE; for (int32 i = 0; i < Tractors.Num() && Li == INDEX_NONE; ++i) if (LoaderFree(Tractors[i])) Li = i;
	if (Li == INDEX_NONE) { ADP_LOG(TEXT("GSE: no belt loader free for the stand x %.0f"), S); return; }
	FTractor& L = Tractors[Li];
	const int32 Ci = Borrow(L.At, &PreLoader);
	if (Ci == INDEX_NONE) return;
	bWorked = true;
	L.Driver = Ci; L.Job = EJob::Boarding; L.JobT = 0.f; L.StandX = S; L.Key = Key;
	ADP_LOG(TEXT("GSE: %s to the bulk hold of %s at the stand x %.0f"), *L.Name, *Key, S);
	Wait(Ci, GslPause());
	ClimbIn(Ci, Li, [this, Li] { Motor(Tractors[Li], true); LoaderApron(Li); });
}

void AGSE::LoaderApron(int32 Li)
{
	FTractor& L = Tractors[Li]; L.Job = EJob::Out;
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(L.StandX, S, N, Sp);
	// out of its place and round, across the apron behind the stands' tails, up the aircraft's right side well clear of
	// the tailplane; held there while an aircraft moves at the stand
	TArray<FVector> W;
	const FVector2f P(L.At.X / 100.f, -L.At.Y / 100.f);
	const float Hx = FMath::Cos(FMath::DegreesToRadians(L.Yaw)), Hy = -FMath::Sin(FMath::DegreesToRadians(L.Yaw));
	const bool bHome = FVector2f::Distance(P, FVector2f(256.f, 88.f)) < 2.f;
	if (bHome) Leg(W, P.X, P.Y, Hx, Hy, 261.5f, 93.5f, 0.f, 1.f, L.KTurnR);
	const FVector From = W.Num() ? W.Last() : L.At;
	if (CaterRear(L.StandX) || !Apron(W, From, Plan(S - 11.4f, N + 40.f), &L)) { W.Reset(); RetryIn(Li, 8.f, [this, Li] { LoaderApron(Li); }); return; }
	Go(L, MoveTemp(W), false, [this, Li, S, N]
	{
		FTractor& U = Tractors[Li];
		const FVector Nose = Plan(S, N);
		U.Until = [this, Nose, S] { return !PlaneNear(Nose, 6000.f) && !CaterRear(S); };
		U.AfterUntil = [this, Li, S, N]
		{
			// up beside the fuselage and square in to the bulk door: the bumper's face 1.75 m from the centre line (the skin
			// at sill height), the door's middle 19.6 m behind the nose wheel
			FTractor& V = Tractors[Li];
			TArray<FVector> W2;
			Leg(W2, S - 11.4f, N + 40.f, 0.f, -1.f, S - 11.4f, N + 25.1f, 0.f, -1.f, V.KTurnR);
			Leg(W2, S - 11.4f, N + 25.1f, 0.f, -1.f, S - 5.69f, N + 19.6f, 1.f, 0.f, V.KTurnR);
			Go(V, MoveTemp(W2), false, [this, Li] { LoaderIn(Li); });
		};
	});
}

void AGSE::LoaderIn(int32 Li)
{
	FTractor& L = Tractors[Li]; L.Job = EJob::AtStand;
	const int32 Ci = L.Driver; ATugs* G = Tg(); if (!G) return;
	ADP_LOG(TEXT("GSE: %s at the bulk hold door - raising the boom to the sill"), *L.Name);
	// the boom up from the cab: his right hand on the boom lever
	Do(Ci, [this, Ci, Li]
	{
		if (ATugs* G2 = Tg()) G2->Crew[Ci].Hold = [this, Li](float K, float PH, FDriverPose& P) { SeatFn(Li)(K, PH, P); P.Hand[1] = TW(Tractors[Li], GslLever); P.Grip[1] = 0.9f; };
		Tractors[Li].BoomWant = GslSillDeg;
	});
	WaitFor(Ci, [this, Li](float) { return FMath::IsNearlyEqual(Tractors[Li].Boom, GslSillDeg, 0.05f); });
	Do(Ci, [this, Ci, Li] { if (ATugs* G2 = Tg()) G2->Crew[Ci].Hold = SeatFn(Li); });
	Wait(Ci, GslPause() * 0.5f);
	ClimbOut(Ci, Li, nullptr);
	// the belt on at its bottom controls
	WalkTo(Ci, [this, Li] { return TW(Tractors[Li], GslStandControls); }, 15.f);
	PoseAct(Ci, [this, Li](float K, float PH, FDriverPose& P) { const FTractor& T = Tractors[Li]; P = ADPPose::Reach(TW(T, GslStandControls), FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), 1, BoomW(T, GslControlsBottom + FVector(0, 0, 4.f)), 0.3f, PH, FMath::Sin(PI * K)); }, 1.2f);
	Do(Ci, [this, Li] { Tractors[Li].bBelt = true; ADP_LOG(TEXT("GSE: %s's belt running"), *Tractors[Li].Name); });
	// the hold loader: up the belt into the hold (a second crew member)
	const int32 Top = Borrow(L.At, nullptr, Li);
	L.TopCrew = Top;
	if (Top != INDEX_NONE)
	{
		WalkTo(Top, [this, Li] { return TW(Tractors[Li], GslStandLoad); }, 15.f);
		WaitFor(Top, [this, Li](float) { return Tractors[Li].bBelt; });
		PoseAct(Top, [this, Li](float K, float PH, FDriverPose& P)
		{
			// (walked up the moving belt, a hand on the rail, step by step - the belt line from its bottom to its top)
			const FTractor& T = Tractors[Li]; const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector(), L2 = ADPPose::LeftOf(Fw);
			const FVector A = BoomW(T, GslBeltBottom), B = BoomW(T, GslStandTop), At = FMath::Lerp(A, B, FMath::SmoothStep(0.f, 1.f, K));
			const float Step = FMath::Sin(K * PI * 16.f) * 12.f;
			P = ADPPose::Standing(At, Fw, PH); P.Lean = 8.f;
			P.Foot[0] = At + L2 * 12.f + Fw * Step; P.Foot[1] = At - L2 * 12.f - Fw * Step;
			P.bHand[0] = true; P.Hand[0] = At + L2 * 36.f + FVector(0, 0, 95.f); P.Grip[0] = 1.f;
		}, 9.f);
		// at the top, in the door: taking each bag off the belt and stowing it (the bag models come next)
		Do(Top, [this, Top, Li]
		{
			if (ATugs* G2 = Tg()) G2->Crew[Top].Hold = [this, Li](float, float PH, FDriverPose& P)
			{
				// facing down the belt: a bag at the top taken by its grips, lifted round into the hold behind him
				const FTractor& T = Tractors[Li]; const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector(), Rt = FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), At = BoomW(T, GslStandTop);
				const FVector Belt = BoomW(T, GslBeltTop - FVector(25.f, 0.f, -12.f));
				const ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
				const FTractor::FBeltBag* B = nullptr; for (const FTractor::FBeltBag& X : T.BeltBags) if (X.Stage >= 1) { B = &X; break; }
				for (const FTractor::FBeltBag& X : T.BeltBags) if (X.Stage == 5) { B = &X; break; }
				if (D && B && B->Stage == 5)
				{
					// (an arriving bag lifted out of the hold behind him and laid on the belt's top)
					const float K = FMath::Min(1.f, B->T / 1.4f);
					const FVector H = FMath::Lerp(At + Fw * 70.f + FVector(0, 0, 40.f), Belt + FVector(0, 0, 25.f), FMath::SmoothStep(0.f, 1.f, K));
					P = ADPPose::Both(At, -Fw, H + Rt * 14.f, H - Rt * 14.f, 1.f, PH, 1.f, 16.f);
				}
				else if (D && B && B->Stage == 1)
				{
					const FDepBagGrips G = D->Grips(B->Id); const float E = FMath::Min(1.f, B->T / 0.8f);
					P = G.bTwoHands ? ADPPose::Both(At, -Fw, G.Left, G.Right, 1.f, PH, E, 22.f) : ADPPose::Stoop(At, -Fw, 1, G.Handle, 1.f, PH, E);
				}
				else if (D && B && B->Stage == 2)
				{
					// (turning with it into the hold, setting it down: the hands from the belt to the hold floor behind him)
					const float K = FMath::Min(1.f, B->T / 1.6f);
					const FVector H = FMath::Lerp(Belt + FVector(0, 0, 25.f), At + Fw * 70.f + FVector(0, 0, 40.f), FMath::SmoothStep(0.f, 1.f, K));
					P = ADPPose::Both(At, FMath::Lerp(-Fw, Fw, K).GetSafeNormal2D().IsNearlyZero() ? Rt : FMath::Lerp(-Fw, Fw, K).GetSafeNormal2D(), H + Rt * 14.f, H - Rt * 14.f, 1.f, PH, 1.f, 16.f);
				}
				else P = ADPPose::Standing(At, -Fw, PH);
			};
			ADP_LOG(TEXT("GSE: the hold loader is up in the bulk hold"));
		});
	}
	// wait for the train's loading to be over (or no train after a long wait), then everything back
	TSharedPtr<bool> bSeen = MakeShared<bool>(false), bOver = MakeShared<bool>(false);
	const float Wait2 = bTest ? 420.f : 1500.f;
	WaitFor(Ci, [this, Li, bSeen, Wait2](float Elapsed)
	{
		bool bTrain = false;
		for (const FTractor& T : Tractors) if (T.IsTrain() && !T.bArrival && FMath::Abs(T.StandX - Tractors[Li].StandX) < 1.f && (T.Job == EJob::Out || T.Job == EJob::AtStand || T.Job == EJob::Loading)) bTrain = true;
		if (bTrain) *bSeen = true;
		if (IsHired(Tractors[Li].Driver))
		{
			// (to-do 206: your operator stays while one of your trains is on its way to this stand or at it - never left without)
			bool bOwn = WaitingAt(Tractors[Li].StandX);
			for (const FTractor& T : Tractors) if (T.IsTrain() && IsHired(T.Driver) && T.JobT < 1500.f && FMath::Abs(T.StandX - Tractors[Li].StandX) < 1.f && (T.Job == EJob::Boarding || T.Job == EJob::ToCarts || T.Job == EJob::Coupling || T.Job == EJob::Loading || T.Job == EJob::Out || T.Job == EJob::AtStand)) { bOwn = true; if (!T.bArrival) *bSeen = true; }
			if (bOwn) return false;
		}
		return (*bSeen && !bTrain) || Elapsed > Wait2;
	});
	Do(Ci, [bOver] { *bOver = true; });
	if (Top != INDEX_NONE)
	{
		WaitFor(Top, [bOver](float) { return *bOver; });
		Do(Top, [this, Top] { if (ATugs* G2 = Tg()) G2->Crew[Top].Hold = nullptr; });
		PoseAct(Top, [this, Li](float K, float PH, FDriverPose& P)
		{
			const FTractor& T = Tractors[Li]; const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector(), L2 = ADPPose::LeftOf(-Fw);
			const FVector A = BoomW(T, GslStandTop), B = BoomW(T, GslBeltBottom), At = FMath::Lerp(A, B, FMath::SmoothStep(0.f, 1.f, K));
			const float Step = FMath::Sin(K * PI * 16.f) * 12.f;
			P = ADPPose::Standing(At, -Fw, PH); P.Lean = -4.f;
			P.Foot[0] = At + L2 * 12.f - Fw * Step; P.Foot[1] = At - L2 * 12.f + Fw * Step;
			P.bHand[0] = true; P.Hand[0] = At + L2 * 36.f + FVector(0, 0, 95.f); P.Grip[0] = 1.f;
		}, 8.f);
		WalkTo(Top, [this, Li] { return TW(Tractors[Li], GslStandLoad + FVector(-90.f, -60.f, 0.f)); }, 20.f);
		Do(Top, [this, Li] { Tractors[Li].TopCrew = INDEX_NONE; ADP_LOG(TEXT("GSE: the hold loader is down from the hold")); });
		GiveBack(Top, Tractors[Li].StandX);
	}
	// the belt off, back into the cab, the boom down
	WaitFor(Ci, [this, Li](float E) { return Tractors[Li].TopCrew == INDEX_NONE || E > 60.f; });
	PoseAct(Ci, [this, Li](float K, float PH, FDriverPose& P) { const FTractor& T = Tractors[Li]; P = ADPPose::Reach(TW(T, GslStandControls), FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), 1, BoomW(T, GslControlsBottom + FVector(0, 0, 4.f)), 0.3f, PH, FMath::Sin(PI * K)); }, 1.2f);
	Do(Ci, [this, Li] { Tractors[Li].bBelt = false; });
	ClimbIn(Ci, Li, [this, Ci, Li]
	{
		if (ATugs* G2 = Tg()) G2->Crew[Ci].Hold = [this, Li](float K, float PH, FDriverPose& P) { SeatFn(Li)(K, PH, P); P.Hand[1] = TW(Tractors[Li], GslLever); P.Grip[1] = 0.9f; };
		Tractors[Li].BoomWant = 0.f;
	});
	WaitFor(Ci, [this, Li](float) { return FMath::IsNearlyEqual(Tractors[Li].Boom, 0.f, 0.05f); });
	Do(Ci, [this, Ci, Li] { if (ATugs* G2 = Tg()) G2->Crew[Ci].Hold = SeatFn(Li); LoaderLeave(Li); });
}

void AGSE::LoaderLeave(int32 Li)
{
	if (CaterRear(Tractors[Li].StandX) || CaterMoving(Tractors[Li].StandX)) { RetryIn(Li, 8.f, [this, Li] { LoaderLeave(Li); }); return; }   // (the catering truck beside it)
	FTractor& L = Tractors[Li]; L.Job = EJob::Back;
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(L.StandX, S, N, Sp);
	ADP_LOG(TEXT("GSE: %s leaves the bulk hold door"), *L.Name);
	// straight back from the door (the beeper), then round towards the tail and down the side, clear of the tailplane
	TArray<FVector> R;
	float Yw = 0.f; const FVector Back = PlanPose(S - 16.f, N + 19.6f, 1.f, 0.f, Yw);
	Dubins(R, L.At, L.Yaw + 180.f, Back, Yw + 180.f, 600.f);
	Go(L, MoveTemp(R), true, [this, Li, S, N]
	{
		FTractor& U = Tractors[Li];
		TArray<FVector> W;
		Leg(W, S - 16.f, N + 19.6f, 1.f, 0.f, S - 10.5f, N + 25.1f, 0.f, 1.f, U.KTurnR);
		Leg(W, S - 10.5f, N + 25.1f, 0.f, 1.f, S - 10.5f, N + 40.f, 0.f, 1.f, U.KTurnR);
		Go(U, MoveTemp(W), false, [this, Li]
		{
			FTractor& V = Tractors[Li];
			TArray<FVector> H;
			if (!Apron(H, V.At, Plan(250.5f, 100.f), &V)) H.Add(Plan(250.5f, 100.f));
			Leg(H, 250.5f, 100.f, 0.f, -1.f, 250.5f, 93.5f, 0.f, -1.f, V.KTurnR);
			Leg(H, 250.5f, 93.5f, 0.f, -1.f, 256.f, 88.f, 1.f, 0.f, V.KTurnR);
			Go(V, MoveTemp(H), false, [this, Li]
			{
				FTractor& X = Tractors[Li]; X.Job = EJob::Home; X.At = X.Home; X.Yaw = X.HomeYaw; Pose(X); Motor(X, false);
				const int32 Ci = X.Driver;
				ADP_LOG(TEXT("GSE: %s parked, switched off"), *X.Name);
				Wait(Ci, GslPause());
				ClimbOut(Ci, Li, [this, Li] { FTractor& Y = Tractors[Li]; Y.Job = EJob::None; Y.Key.Reset(); Y.StandX = 0.f; Y.Driver = INDEX_NONE; });
				GiveBack(Ci);
			});
		});
	});
}

void AGSE::LoadingCycles(int32 Ci, int32 Ti, int32 Li, float Seconds)
{
	// the train's driver: from the open side of a cart to the belt loader's bottom end and back - lifting each bag off the
	// cart and laying it on the belt (the bag models come next: the movements now)
	const int32 N = FMath::Max(1, FMath::RoundToInt(Seconds / 16.f));
	const TArray<int32> Tr = Tractors[Ti].Train;
	float S = 0.f, Nn = 0.f, Sp = 36.f; StandGeo(Tractors[Ti].StandX, S, Nn, Sp);
	const FVector Craft = Plan(S, Nn + 8.f);
	for (int32 k = 0; k < N && Tr.Num(); ++k)
	{
		const int32 c = Tr[k % Tr.Num()];
		const int32 s = FVector::DotProduct(Craft - Carts[c].At, FRotator(0.f, Carts[c].Yaw + 90.f, 0.f).Vector()) > 0.f ? 1 : 0; const float Ys = s == 0 ? -1.f : 1.f;
		WalkTo(Ci, [this, c, Ys] { return CW(Carts[c], FVector(FMath::FRandRange(-60.f, 60.f), Ys * 118.f, 0.f)); }, 15.f);
		PoseAct(Ci, [this, c, Ys](float K, float PH, FDriverPose& P)
		{
			const FCart& C = Carts[c]; const FVector Rt = FRotator(0.f, C.Yaw + 90.f, 0.f).Vector() * Ys, Fw = FRotator(0.f, C.Yaw, 0.f).Vector();
			const FVector St = C.At + Rt * 118.f, H = C.At + Rt * 62.f + FVector(0, 0, 70.f + 40.f * K);
			P = ADPPose::Both(St, -Rt, H + Fw * 18.f, H - Fw * 18.f, 1.f, PH, ADPPose::Ramp(K), 14.f);
		}, 1.8f);
		if (Li == INDEX_NONE) { Wait(Ci, 6.f); continue; }
		WalkTo(Ci, [this, Li] { return TW(Tractors[Li], GslStandLoad); }, 20.f);
		PoseAct(Ci, [this, Li](float K, float PH, FDriverPose& P)
		{
			const FTractor& T = Tractors[Li]; const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector(), Rt = FRotator(0.f, T.Yaw + 90.f, 0.f).Vector();
			const FVector St = TW(T, GslStandLoad), B = BoomW(T, GslBeltBottom + FVector(35.f, 0.f, 16.f));
			P = ADPPose::Both(St, Fw, B + Rt * 16.f, B - Rt * 16.f, 1.f, PH, ADPPose::Ramp(K), 16.f);
		}, 1.6f);
	}
}
