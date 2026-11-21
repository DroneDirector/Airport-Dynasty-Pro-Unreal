// The baggage tractors' crew and their jobs (owner 2026-11-21: "every ground vehicle ... animated doors and animated
// MetaHumans that interact with the vehicles and sit in them, operate them"; one crew member drives a vehicle; the ramp
// team is the real size and its people switch jobs - ATugs' ground crew, lent while they are free).
//  - the driver walks to the tractor (from the apron through the Ramp services 13 passage and the baggage hall), takes the
//    door handle, swings the door open, climbs in by the step and the grab handles, sits at the wheel (feet on the pedals,
//    hands on the wheel), pulls the door shut
//  - coupling by hand: out again, the tractor's (or the cart in front's) pin lifted out, the cart's drawbar lowered from
//    standing up until its eye drops into the jaw (the clank), the pin pushed down through it (the clunk); uncoupling the
//    reverse, the drawbar left standing up (its parking brake)
//  - at the aircraft: the curtains on its side unstrapped and rolled up by their hem, the loading, rolled down and strapped
//  - a GPU: the tractor's pin out, the GPU's drawbar eye lifted into the jaw, the pin in
// Bodies posed by the fire truck's IK rig at the models' IK points (CrewPose.h).
#include "GSE.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "ADPLog.h"
#include "Airfield.h"
#include "BabylonModel.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"


namespace
{
	// the tractor (root cm): the door's outside stand, its handle, the step, the grab handles, the seat, the pedals, the wheel
	// (the vehicle's own door, step, grab handles, seat, pedals and wheel: FTractor::K*, set when it is placed - GSE.cpp)
	const FVector GscTPinTop(-162.f, 0.f, 62.5f), GscTPinStand(-205.f, -40.f, 0.f), GscTPinHitch(-162.f, 0.f, 40.f);
	// the cart
	const FVector GscCPinTop(-127.f, 0.f, 59.55f), GscCPinStand(-175.f, -45.f, 0.f), GscCBarStand(178.f, -100.f, 0.f), GscCJawAt(-127.f, 0.f, 40.f);
	const FVector GscCAxleP(80.f, 0.f, 38.9f), GscCBarP(130.f, 0.f, 30.f), GscCHandle(180.f, 0.f, 45.54f), GscCEyeAt(201.f, 0.f, 40.f);
	// the GPU (Art/GPU/gpu-parts.json): its drawbar handle and a stand beside it
	const FVector GscGHandle(170.f, 0.f, 48.f), GscGStand(172.f, -62.f, 0.f), GscGEyeAt(205.f, 0.f, 40.f);
	float GscPause() { return FMath::FRandRange(1.5f, 4.f); }
	ATugs::FCrewAct GscAct(ATugs::FCrewAct::EKind K) { ATugs::FCrewAct A; A.Kind = K; return A; }
	// where people are: 0 the apron, 1 the baggage hall / the Ramp services 13 passage, 2 the departure cart lane
	int32 GscZoneOf(const FVector& W) { const float Y = -W.Y / 100.f; return Y >= 82.6f ? 0 : Y >= 52.f ? 1 : 2; }
}

// ------------------------------------------------------------------ small acts

void AGSE::Do(int32 Ci, TFunction<void()> Fn) { if (ATugs* G = Tg()) { ATugs::FCrewAct A = GscAct(ATugs::FCrewAct::Do); A.Fn = MoveTemp(Fn); G->Crew[Ci].Q.Add(A); } }
void AGSE::Wait(int32 Ci, float S) { if (ATugs* G = Tg()) { ATugs::FCrewAct A = GscAct(ATugs::FCrewAct::Wait); A.Dur = S; G->Crew[Ci].Q.Add(A); } }
void AGSE::WaitFor(int32 Ci, TFunction<bool(float)> Until) { if (ATugs* G = Tg()) { ATugs::FCrewAct A = GscAct(ATugs::FCrewAct::WaitFor); A.Until = MoveTemp(Until); G->Crew[Ci].Q.Add(A); } }
void AGSE::PoseAct(int32 Ci, TFunction<void(float, float, FDriverPose&)> Fn, float Dur) { if (ATugs* G = Tg()) { ATugs::FCrewAct A = GscAct(ATugs::FCrewAct::Pose); A.PoseFn = MoveTemp(Fn); A.Dur = Dur; G->Crew[Ci].Q.Add(A); } }
void AGSE::Hold(int32 Ci, TFunction<void(float, float, FDriverPose&)> Fn) { Do(Ci, [this, Ci, Fn] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = Fn; }); }

void AGSE::RetryIn(int32 Ti, float Seconds, TFunction<void()> Fn)
{
	FTractor& T = Tractors[Ti];
	const float At = GetWorld()->GetTimeSeconds() + Seconds;
	T.Until = [this, At] { return GetWorld()->GetTimeSeconds() >= At; };
	T.AfterUntil = MoveTemp(Fn);
}

void AGSE::WalkTo(int32 Ci, TFunction<FVector()> To, float Stop)
{
	ATugs* G = Tg(); if (!G || !G->Crew.IsValidIndex(Ci)) return;
	ATugs::FCrew& C = G->Crew[Ci];
	// from where the walk starts (where the last walk queued ends, or where they stand): through the building's openings -
	// the Ramp services 13 passage (its east side, x 272.5: the trains use its middle), the hall's south side (z 59.5,
	// beside the trains' line), the cart lane's roller door (its west half, x 130)
	FVector From = C.A.IsValid() ? C.A->GetActorLocation() : FVector::ZeroVector;
	for (int32 i = C.Q.Num() - 1; i >= 0; --i) if (C.Q[i].Kind == ATugs::FCrewAct::Walk && C.Q[i].To) { From = C.Q[i].To(); break; }
	const FVector End = To();
	const int32 Za = GscZoneOf(From), Zb = GscZoneOf(End);
	TArray<FVector> Via;
	if (Za != Zb)
	{
		if (Za == 0) { Via.Add(Plan(272.5f, 88.f)); Via.Add(Plan(272.5f, 70.f)); Via.Add(Plan(272.5f, 59.5f)); }
		if (Za == 2) { Via.Add(Plan(130.f, 50.f)); Via.Add(Plan(130.f, 54.f)); Via.Add(Plan(138.f, 59.5f)); }
		if (Zb == 2) { Via.Add(Plan(138.f, 59.5f)); Via.Add(Plan(130.f, 54.f)); Via.Add(Plan(130.f, 50.f)); }
		if (Zb == 0) { Via.Add(Plan(272.5f, 59.5f)); Via.Add(Plan(272.5f, 70.f)); Via.Add(Plan(272.5f, 88.f)); }
	}
	for (const FVector& V : Via) { ATugs::FCrewAct A = GscAct(ATugs::FCrewAct::Walk); A.To = [V] { return V; }; A.Stop = 70.f; A.Max = 150.f; C.Q.Add(A); }
	ATugs::FCrewAct A = GscAct(ATugs::FCrewAct::Walk); A.To = MoveTemp(To); A.Stop = Stop; A.Max = 150.f; C.Q.Add(A);
}

int32 AGSE::Borrow(const FVector& Near)
{
	ATugs* G = Tg(); if (!G) return INDEX_NONE;
	// (owner 2026-11-21: a real ramp team, the same people switching jobs) - a wing / tail walker no push needs (three are
	// kept free for a push), else a tug driver or handler whose gate is quiet
	auto Idle = [](const ATugs::FCrew& C) { return C.A.IsValid() && !C.bGse && !C.Q.Num() && !C.Hold && !C.Follow; };
	int32 FreeW = 0; for (const ATugs::FCrew& C : G->Crew) if (C.bWalker && C.Tug == INDEX_NONE && Idle(C)) ++FreeW;
	int32 Best = INDEX_NONE; float BestD = 1e12f;
	for (int32 pass = 0; pass < 3 && Best == INDEX_NONE; ++pass)
		for (int32 i = 0; i < G->Crew.Num(); ++i)
		{
			const ATugs::FCrew& C = G->Crew[i]; if (!Idle(C)) continue;
			bool bOk = false;
			if (pass == 0) bOk = C.bWalker && C.Tug == INDEX_NONE && FreeW > 3;
			if (pass == 1) bOk = !C.bWalker && G->Tugs.IsValidIndex(C.Tug) && G->Tugs[C.Tug].State == ATugs::EState::Parked && G->Tugs[C.Tug].Key.IsEmpty();
			if (pass == 2) bOk = bTest && C.bWalker && C.Tug == INDEX_NONE;   // (the test: a walker even if that leaves a push short - logged)
			if (!bOk) continue;
			const float D = FVector::DistSquared2D(C.A->GetActorLocation(), Near);
			if (D < BestD) { BestD = D; Best = i; }
		}
	if (Best == INDEX_NONE) { ADP_LOG(TEXT("GSE: no ground crew free for a tractor job just now")); return INDEX_NONE; }
	G->Crew[Best].bGse = true;
	ADP_LOG(TEXT("GSE: %s (%s) takes a tractor job - %.0f m away"), *G->Crew[Best].Name, G->Crew[Best].bWalker ? TEXT("wing walker") : TEXT("tug crew"), FMath::Sqrt(BestD) / 100.f);
	return Best;
}

void AGSE::GiveBack(int32 Ci)
{
	ATugs* G = Tg(); if (!G || !G->Crew.IsValidIndex(Ci)) return;
	const FVector Home = G->Crew[Ci].Spot;
	WalkTo(Ci, [Home] { return Home; }, 30.f);
	Do(Ci, [this, Ci] { if (ATugs* G2 = Tg()) { G2->Crew[Ci].bGse = false; G2->Crew[Ci].AnchorYaw = G2->Crew[Ci].SpotYaw; ADP_LOG(TEXT("GSE: %s back at the gate, free again"), *G2->Crew[Ci].Name); } });
}

// ------------------------------------------------------------------ in and out of the tractor

TFunction<void(float, float, FDriverPose&)> AGSE::SeatFn(int32 Ti) const
{
	// sat at the wheel: the hips on the seat, the left foot on the brake, the right on the accelerator, both hands on the wheel
	return [this, Ti](float, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti];
		P = FDriverPose(); P.Facing = FRotator(0.f, T.Yaw, 0.f).Vector();
		P.Pelvis = TW(T, T.KSeat + FVector(0, 0, 10.f)); P.Lean = -4.f;
		P.Foot[0] = TW(T, T.KBrake); P.Foot[1] = TW(T, T.KAccel); P.FootPitch[0] = P.FootPitch[1] = 20.f;
		P.bHand[0] = P.bHand[1] = true; P.Hand[0] = TW(T, T.KHandL); P.Hand[1] = TW(T, T.KHandR); P.Grip[0] = P.Grip[1] = 1.f;
	};
}

TFunction<void(float, float, FDriverPose&)> AGSE::ClimbFn(int32 Ti, bool bIn) const
{
	// up the step through the door and round onto the seat (IK_Stand_Door, IK_Step, IK_Grab_Outside / _Inside, the cab floor)
	return [this, Ti, bIn](float K0, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti];
		const float K = bIn ? K0 : 1.f - K0;
		const FVector In = FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), Fwd = FRotator(0.f, T.Yaw, 0.f).Vector();
		auto Key = [&](int32 i)
		{
			FDriverPose Q; Q.Facing = In;
			switch (i)
			{
			case 0: Q = ADPPose::Standing(TW(T, T.KStand), In, PH); Q.bHand[1] = true; Q.Hand[1] = TW(T, T.KGrabOut); Q.Grip[1] = 1.f; break;          // at the door, a hand on the grab handle
			case 1: Q.Pelvis = TW(T, FVector((T.KStand.X + T.KStep.X) * 0.5f - 1.f, (T.KStand.Y + T.KStep.Y) * 0.5f - 8.f, PH + 16.f)); Q.Lean = 12.f;
				Q.Foot[0] = TW(T, T.KStep + FVector(6.f, 0.f, 0.f)); Q.Foot[1] = TW(T, FVector(T.KStand.X - 7.f, T.KStand.Y + 8.f, 0.f));
				Q.bHand[0] = Q.bHand[1] = true; Q.Hand[0] = TW(T, T.KGrabIn); Q.Hand[1] = TW(T, T.KGrabOut); Q.Grip[0] = Q.Grip[1] = 1.f; break;   // the left foot on the step
			case 2: Q.Pelvis = TW(T, FVector(T.KStep.X - 3.f, T.KStep.Y - 17.f, T.KStep.Z + PH - 6.f)); Q.Lean = 14.f;
				Q.Foot[0] = TW(T, T.KStep + FVector(6.f, 0.f, 0.f)); Q.Foot[1] = TW(T, T.KStep + FVector(-12.f, -1.f, 0.f));
				Q.bHand[0] = Q.bHand[1] = true; Q.Hand[0] = TW(T, T.KGrabIn); Q.Hand[1] = TW(T, T.KGrabOut); Q.Grip[0] = Q.Grip[1] = 1.f; break;   // up on the step
			case 3: Q.Pelvis = TW(T, FVector(T.KSeat.X + 2.f, FMath::Lerp(T.KStep.Y, T.KSeat.Y, 0.2f), T.KFloor + PH - 26.f)); Q.Lean = 26.f;
				Q.Foot[0] = TW(T, FVector(T.KSeat.X + 19.f, FMath::Lerp(T.KStep.Y, T.KSeat.Y, 0.55f), T.KFloor)); Q.Foot[1] = TW(T, T.KStep + FVector(-10.f, 0.f, 0.f));
				Q.bHand[0] = true; Q.Hand[0] = TW(T, T.KGrabIn); Q.Grip[0] = 1.f; break;                                                     // in under the roof, a foot on the floor
			case 4: Q.Facing = (In + Fwd).GetSafeNormal2D(); Q.Pelvis = TW(T, FVector(T.KSeat.X + 1.f, T.KSeat.Y - 14.f, T.KFloor + PH - 38.f)); Q.Lean = 24.f;
				Q.Foot[0] = TW(T, FVector(T.KSeat.X + 35.f, T.KSeat.Y - 16.f, T.KFloor)); Q.Foot[1] = TW(T, FVector(T.KSeat.X + 29.f, T.KSeat.Y + 10.f, T.KFloor)); break;                        // turning to the seat
			default: { const auto S = SeatFn(Ti); S(1.f, PH, Q); } break;
			}
			return Q;
		};
		static const float At[] = {0.f, 0.22f, 0.45f, 0.68f, 0.85f, 1.f};
		int32 i = 0; while (i < 4 && K > At[i + 1]) ++i;
		P = ADPPose::Lerp(Key(i), Key(i + 1), FMath::SmoothStep(0.f, 1.f, (K - At[i]) / (At[i + 1] - At[i])));
	};
}

void AGSE::ClimbIn(int32 Ci, int32 Ti, TFunction<void()> Then)
{
	WalkTo(Ci, [this, Ti] { return TW(Tractors[Ti], Tractors[Ti].KStand); }, 15.f);
	// the hand to the handle, the door swung open (its sound), up and in, sat, the door pulled shut
	PoseAct(Ci, [this, Ti](float K, float PH, FDriverPose& P) { const FTractor& T = Tractors[Ti]; P = ADPPose::Reach(TW(T, T.KStand), FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), 1, TW(T, T.KHandle), 0.7f, PH, FMath::Sin(PI * K)); }, 1.0f);
	Do(Ci, [this, Ti] { FTractor& T = Tractors[Ti]; T.DoorWant = 80.f; Sound(*T.SndDoorOpen, TW(T, T.KHandle + FVector(0.f, 0.f, 20.f))); });
	Wait(Ci, 0.6f);
	PoseAct(Ci, ClimbFn(Ti, true), 3.2f);
	Do(Ci, [this, Ci, Ti] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = SeatFn(Ti); Tractors[Ti].DoorWant = 0.f; });
	Wait(Ci, 1.0f);
	if (Then) Do(Ci, MoveTemp(Then));
}

void AGSE::ClimbOut(int32 Ci, int32 Ti, TFunction<void()> Then)
{
	Do(Ci, [this, Ti] { FTractor& T = Tractors[Ti]; T.DoorWant = 80.f; Sound(*T.SndDoorOpen, TW(T, T.KHandle + FVector(0.f, 0.f, 20.f))); });
	Wait(Ci, 0.7f);
	Do(Ci, [this, Ci] { if (ATugs* G = Tg()) G->Crew[Ci].Hold = nullptr; });
	PoseAct(Ci, ClimbFn(Ti, false), 2.8f);
	// down: the door pushed shut behind
	PoseAct(Ci, [this, Ti](float K, float PH, FDriverPose& P) { FTractor& T = Tractors[Ti]; P = ADPPose::Reach(TW(T, T.KStand), FRotator(0.f, T.Yaw + 90.f, 0.f).Vector(), 1, TW(T, T.KHandle + FVector(-5.f, -4.5f, 18.f)), 0.3f, PH, FMath::Sin(PI * K)); if (K > 0.4f) T.DoorWant = 0.f; }, 0.9f);
	if (Then) Do(Ci, MoveTemp(Then));
}

// ------------------------------------------------------------------ coupling by hand

void AGSE::CoupleCart(int32 Ci, int32 Ti, int32 Cart, int32 Front, TFunction<void()> Then)
{
	// the hitch it goes on: the tractor's rear pin hitch, or the rear coupler jaw of the cart in front
	auto PinTop = [this, Ti, Front] { return Front == INDEX_NONE ? TW(Tractors[Ti], GscTPinTop) : CW(Carts[Front], GscCPinTop); };
	auto PinStand = [this, Ti, Front] { return Front == INDEX_NONE ? TW(Tractors[Ti], GscTPinStand) : CW(Carts[Front], GscCPinStand); };
	auto SetPin = [this, Ti, Front](float V) { if (Front == INDEX_NONE) Tractors[Ti].PinWant = V; else Carts[Front].PinWant = V; };
	auto PinOf = [this, Ti, Front] { return Front == INDEX_NONE ? Tractors[Ti].Pin : Carts[Front].Pin; };
	// the cart's drawbar handle where it is now (on the turntable axle, raised by Lift)
	auto Handle = [this, Cart]
	{
		const FCart& C = Carts[Cart];
		const FQuat Qa(FVector::UpVector, FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(C.Yaw, C.AxleYaw))), Ql(FVector::RightVector, FMath::DegreesToRadians(C.Lift));
		return CW(C, GscCAxleP + Qa.RotateVector(GscCBarP - GscCAxleP + Ql.RotateVector(GscCHandle - GscCBarP)));
	};
	const bool bTractor = Front == INDEX_NONE;
	// 1. the pin up
	WalkTo(Ci, PinStand, 15.f);
	Do(Ci, [this, bTractor, PinTop, SetPin] { SetPin(1.f); Sound(bTractor ? TEXT("BTPinOut") : TEXT("BCPinOut"), PinTop()); });
	PoseAct(Ci, [PinTop, PinStand, PinOf, bTractor](float K, float PH, FDriverPose& P) { const FVector S = PinStand(), T = PinTop(); P = ADPPose::Stoop(S, (T - S).GetSafeNormal2D(), 1, T + FVector(0, 0, (bTractor ? 40.f : 10.f) * PinOf() + 6.f), 1.f, PH, ADPPose::Ramp(K)); }, 1.5f);
	// 2. the drawbar lowered: both hands on its handle, the eye down into the jaw
	WalkTo(Ci, [this, Cart] { return CW(Carts[Cart], GscCBarStand); }, 15.f);
	Do(Ci, [this, Cart] { Carts[Cart].AxleYaw = Carts[Cart].Yaw; Carts[Cart].LiftWant = 0.f; });
	PoseAct(Ci, [this, Cart, Handle](float K, float PH, FDriverPose& P)
	{
		const FCart& C = Carts[Cart]; const FVector S = CW(C, GscCBarStand), H = Handle(), Rt = FRotator(0.f, C.Yaw + 90.f, 0.f).Vector(), Fw = FRotator(0.f, C.Yaw, 0.f).Vector();
		P = ADPPose::Both(S, Rt, H + Fw * 9.f, H - Fw * 9.f, 1.f, PH, ADPPose::Ramp(K), 10.f);
	}, 3.0f);
	Do(Ci, [this, Cart, PinTop] { Carts[Cart].Lift = Carts[Cart].LiftWant = 0.f; Carts[Cart].bMoved = true; Sound(TEXT("BCDrawbarClank"), PinTop() - FVector(0, 0, 20.f)); });
	// 3. the pin down through the eye
	WalkTo(Ci, PinStand, 15.f);
	Do(Ci, [SetPin] { SetPin(0.f); });
	PoseAct(Ci, [PinTop, PinStand, PinOf, bTractor](float K, float PH, FDriverPose& P) { const FVector S = PinStand(), T = PinTop(); P = ADPPose::Stoop(S, (T - S).GetSafeNormal2D(), 1, T + FVector(0, 0, (bTractor ? 40.f : 10.f) * PinOf() + 6.f), 0.9f, PH, ADPPose::Ramp(K)); }, 1.3f);
	Do(Ci, [this, Ti, Cart, bTractor, PinTop]
	{
		Sound(bTractor ? TEXT("BTHitchPin") : TEXT("BCPinIn"), PinTop());
		FTractor& T = Tractors[Ti]; Carts[Cart].Tractor = Ti; Carts[Cart].Flight = T.Key; T.Train.AddUnique(Cart);
		ADP_LOG(TEXT("GSE: cart %d coupled to %s (train of %d)"), Cart + 1, bTractor ? *T.Name : TEXT("the cart in front"), T.Train.Num());
	});
	if (Then) Do(Ci, MoveTemp(Then));
}

void AGSE::UncoupleCart(int32 Ci, int32 Ti, int32 Cart, int32 Front, TFunction<void()> Then)
{
	auto PinTop = [this, Ti, Front] { return Front == INDEX_NONE ? TW(Tractors[Ti], GscTPinTop) : CW(Carts[Front], GscCPinTop); };
	auto PinStand = [this, Ti, Front] { return Front == INDEX_NONE ? TW(Tractors[Ti], GscTPinStand) : CW(Carts[Front], GscCPinStand); };
	auto SetPin = [this, Ti, Front](float V) { if (Front == INDEX_NONE) Tractors[Ti].PinWant = V; else Carts[Front].PinWant = V; };
	auto PinOf = [this, Ti, Front] { return Front == INDEX_NONE ? Tractors[Ti].Pin : Carts[Front].Pin; };
	auto Handle = [this, Cart]
	{
		const FCart& C = Carts[Cart];
		const FQuat Qa(FVector::UpVector, FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(C.Yaw, C.AxleYaw))), Ql(FVector::RightVector, FMath::DegreesToRadians(C.Lift));
		return CW(C, GscCAxleP + Qa.RotateVector(GscCBarP - GscCAxleP + Ql.RotateVector(GscCHandle - GscCBarP)));
	};
	const bool bTractor = Front == INDEX_NONE;
	WalkTo(Ci, PinStand, 15.f);
	Do(Ci, [this, bTractor, PinTop, SetPin] { SetPin(1.f); Sound(bTractor ? TEXT("BTPinOut") : TEXT("BCPinOut"), PinTop()); });
	PoseAct(Ci, [PinTop, PinStand, PinOf, bTractor](float K, float PH, FDriverPose& P) { const FVector S = PinStand(), T = PinTop(); P = ADPPose::Stoop(S, (T - S).GetSafeNormal2D(), 1, T + FVector(0, 0, (bTractor ? 40.f : 10.f) * PinOf() + 6.f), 1.f, PH, ADPPose::Ramp(K)); }, 1.5f);
	// the drawbar up out of the jaw and left standing (its brake on): the cart is off the train
	WalkTo(Ci, [this, Cart] { return CW(Carts[Cart], GscCBarStand); }, 15.f);
	Do(Ci, [this, Ti, Cart] { FTractor& T = Tractors[Ti]; T.Train.Remove(Cart); Carts[Cart].Tractor = INDEX_NONE; Carts[Cart].LiftWant = -80.f; Sound(TEXT("BCDrawbarClank"), CW(Carts[Cart], GscCEyeAt)); });
	PoseAct(Ci, [this, Cart, Handle](float K, float PH, FDriverPose& P)
	{
		const FCart& C = Carts[Cart]; const FVector S = CW(C, GscCBarStand), H = Handle(), Rt = FRotator(0.f, C.Yaw + 90.f, 0.f).Vector(), Fw = FRotator(0.f, C.Yaw, 0.f).Vector();
		P = ADPPose::Both(S, Rt, H + Fw * 9.f, H - Fw * 9.f, 1.f, PH, ADPPose::Ramp(K), 10.f);
	}, 3.0f);
	WalkTo(Ci, PinStand, 15.f);
	Do(Ci, [SetPin] { SetPin(0.f); });
	PoseAct(Ci, [PinTop, PinStand, PinOf, bTractor](float K, float PH, FDriverPose& P) { const FVector S = PinStand(), T = PinTop(); P = ADPPose::Stoop(S, (T - S).GetSafeNormal2D(), 1, T + FVector(0, 0, (bTractor ? 40.f : 10.f) * PinOf() + 6.f), 0.9f, PH, ADPPose::Ramp(K)); }, 1.3f);
	Do(Ci, [this, Ti, Cart, bTractor, PinTop] { Sound(bTractor ? TEXT("BTHitchPin") : TEXT("BCPinIn"), PinTop()); Carts[Cart].Flight.Reset(); ADP_LOG(TEXT("GSE: cart %d uncoupled, its drawbar up (brake on)"), Cart + 1); });
	if (Then) Do(Ci, MoveTemp(Then));
}

void AGSE::CoupleGpu(int32 Ci, int32 Ti, int32 Gate, bool bOn, TFunction<void()> Then)
{
	auto GpuAt = [this, Gate](const FVector& L) { const ATugs* G = Tg(); return G && G->Tugs.IsValidIndex(Gate) && G->Tugs[Gate].Gpu.M.IsValid() ? G->Tugs[Gate].Gpu.M->GetActorTransform().TransformPosition(L) : FVector::ZeroVector; };
	auto PinTop = [this, Ti] { return TW(Tractors[Ti], GscTPinTop); };
	auto PinStand = [this, Ti] { return TW(Tractors[Ti], GscTPinStand); };
	auto PinPose = [this, Ti, PinTop, PinStand](float Grip) { return [this, Ti, PinTop, PinStand, Grip](float K, float PH, FDriverPose& P) { const FVector S = PinStand(), T = PinTop(); P = ADPPose::Stoop(S, (T - S).GetSafeNormal2D(), 1, T + FVector(0, 0, 40.f * Tractors[Ti].Pin + 6.f), Grip, PH, ADPPose::Ramp(K)); }; };
	WalkTo(Ci, PinStand, 15.f);
	Do(Ci, [this, Ti, PinTop] { Tractors[Ti].PinWant = 1.f; Sound(TEXT("BTPinOut"), PinTop()); });
	PoseAct(Ci, PinPose(1.f), 1.5f);
	// the GPU's drawbar: its eye lifted into the jaw (on), lifted out and set down on its park leg (off)
	WalkTo(Ci, [GpuAt] { return GpuAt(GscGStand); }, 15.f);
	PoseAct(Ci, [GpuAt](float K, float PH, FDriverPose& P) { const FVector S = GpuAt(GscGStand), H = GpuAt(GscGHandle) + FVector(0, 0, 6.f * FMath::Sin(PI * K)); P = ADPPose::Squat(S, (H - S).GetSafeNormal2D(), H, PH, ADPPose::Ramp(K)); }, 2.4f);
	Do(Ci, [this, Ti, Gate, bOn, GpuAt] { Tractors[Ti].TowGpu = bOn ? Gate : INDEX_NONE; Sound(TEXT("BCDrawbarClank"), GpuAt(GscGEyeAt)); });
	WalkTo(Ci, PinStand, 15.f);
	Do(Ci, [this, Ti] { Tractors[Ti].PinWant = 0.f; });
	PoseAct(Ci, PinPose(0.9f), 1.3f);
	Do(Ci, [this, Ti, bOn, PinTop] { Sound(TEXT("BTHitchPin"), PinTop()); ADP_LOG(TEXT("GSE: %s %s the GPU"), *Tractors[Ti].Name, bOn ? TEXT("coupled to") : TEXT("uncoupled from")); });
	if (Then) Do(Ci, MoveTemp(Then));
}

void AGSE::Curtains(int32 Ci, int32 Ti, bool bOpen, TFunction<void()> Then, const FVector* Toward)
{
	// each cart of the train, the side facing the aircraft: the four straps unhooked (stooping to each), the hem taken in
	// both hands and rolled up over the head - or down and strapped again
	const FTractor& T = Tractors[Ti];
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(T.StandX, S, N, Sp);
	const FVector Craft = Toward ? *Toward : Plan(S, N + 8.f);   // (the side towards the aircraft - or the make-up carousel)
	for (const int32 c : T.Train)
	{
		if (!Carts.IsValidIndex(c)) continue;
		const FCart& C = Carts[c];
		const int32 s = FVector::DotProduct(Craft - C.At, FRotator(0.f, C.Yaw + 90.f, 0.f).Vector()) > 0.f ? 1 : 0;   // (1: the cart's right, the root's +y)
		const float Ys = s == 0 ? -1.f : 1.f;
		auto Stand = [this, c, Ys](float X) { return [this, c, Ys, X] { return CW(Carts[c], FVector(X, Ys * 115.f, 0.f)); }; };
		auto Face = [this, c, Ys] { return FRotator(0.f, Carts[c].Yaw + 90.f, 0.f).Vector() * -Ys; };
		auto Strap = [this, c, Ci, Ys, Face, Stand](float X, float Sx)
		{
			PoseAct(Ci, [this, c, Ys, X, Sx, Face, Stand](float K, float PH, FDriverPose& P) { P = ADPPose::Stoop(Stand(Sx)(), Face(), 1, CW(Carts[c], FVector(X, Ys * 82.f, 58.5f)), 0.9f, PH, ADPPose::Ramp(K)); }, 1.0f);
			Do(Ci, [this, c, X, Ys] { Sound(TEXT("BCCurtainStrap"), CW(Carts[c], FVector(X, Ys * 82.f, 58.5f))); });
		};
		auto Roll = [this, c, s, Ci, Ys, Face, Stand]
		{
			PoseAct(Ci, [this, c, s, Ys, Face, Stand](float K, float PH, FDriverPose& P)
			{
				const FCart& C2 = Carts[c]; const float Z = FMath::Lerp(55.5f, 180.1f, C2.Curtain[s]);
				// (IK_Curtain_<side>_HandL / _HandR: riding with the hem)
				const FVector Hl = CW(C2, FVector(s == 0 ? 12.5f : -12.5f, Ys * 79.1f, Z)), Hr = CW(C2, FVector(s == 0 ? -12.5f : 12.5f, Ys * 79.1f, Z));
				P = ADPPose::Both(Stand(0.f)(), Face(), Hl, Hr, 1.f, PH, ADPPose::Ramp(K), 4.f);
			}, 2.8f);
		};
		if (bOpen)
		{
			WalkTo(Ci, Stand(-58.f), 15.f); Strap(-85.f, -58.f); Strap(-30.f, -58.f);
			WalkTo(Ci, Stand(58.f), 15.f); Strap(30.f, 58.f); Strap(85.f, 58.f);
			Do(Ci, [this, c, s] { Carts[c].bStraps[s] = false; });
			WalkTo(Ci, Stand(0.f), 15.f);
			Do(Ci, [this, c, s] { Carts[c].CurtainWant[s] = 1.f; Sound(TEXT("BCCurtainUp"), CW(Carts[c], FVector(0.f, s == 0 ? -80.f : 80.f, 120.f))); });
			Roll();
		}
		else
		{
			WalkTo(Ci, Stand(0.f), 15.f);
			Do(Ci, [this, c, s] { Carts[c].CurtainWant[s] = 0.f; Sound(TEXT("BCCurtainDown"), CW(Carts[c], FVector(0.f, s == 0 ? -80.f : 80.f, 120.f))); });
			Roll();
			WalkTo(Ci, Stand(-58.f), 15.f); Strap(-85.f, -58.f); Strap(-30.f, -58.f);
			WalkTo(Ci, Stand(58.f), 15.f); Strap(30.f, 58.f); Strap(85.f, 58.f);
			Do(Ci, [this, c, s] { Carts[c].bStraps[s] = true; });
		}
		Do(Ci, [c, bOpen, s] { ADP_LOG(TEXT("GSE: cart %d's %s curtain %s"), c + 1, s == 0 ? TEXT("left") : TEXT("right"), bOpen ? TEXT("rolled up for loading") : TEXT("down and strapped")); });
	}
	if (Then) Do(Ci, MoveTemp(Then));
}

// ------------------------------------------------------------------ the baggage train job

void AGSE::StartTrain(int32 Ti, const TArray<int32>& Pair)
{
	if (!Tractors.IsValidIndex(Ti) || Pair.Num() < 1) return;
	FTractor& T = Tractors[Ti];
	const int32 Ci = Borrow(T.At);
	if (Ci == INDEX_NONE) return;
	bWorked = true;
	T.Driver = Ci; T.Job = EJob::Boarding; T.JobT = 0.f;
	ADP_LOG(TEXT("GSE: %s - a train of %d carts for %s"), *T.Name, Pair.Num(), T.Key.IsEmpty() ? TEXT("the next departure") : *T.Key);
	Wait(Ci, GscPause());
	ClimbIn(Ci, Ti, [this, Ti, Pair, Ci]
	{
		FTractor& U = Tractors[Ti]; Motor(U, true); U.Job = EJob::ToCarts;
		// backed out of its box, then round and straight back to the first cart's drawbar: the pin hitch where the cart's
		// eye comes down
		const FCart& C0 = Carts[Pair[0]];
		const FVector Eye = CW(C0, GscCEyeAt), Fw = FRotator(0.f, C0.Yaw, 0.f).Vector();
		FVector Pose = Eye + Fw * 162.f; Pose = Plan(Pose.X / 100.f, -Pose.Y / 100.f);
		const FVector Out = U.At - FRotator(0.f, U.Yaw, 0.f).Vector() * 400.f;
		TArray<FVector> W;
		Dubins(W, U.At, U.Yaw + 180.f, Out, U.Yaw + 180.f, 500.f);
		Dubins(W, Out, U.Yaw + 180.f, Pose, C0.Yaw + 180.f, 500.f);
		Go(U, MoveTemp(W), true, [this, Ti, Pair, Ci]
		{
			FTractor& V = Tractors[Ti]; V.Job = EJob::Coupling;
			ADP_LOG(TEXT("GSE: %s at the carts - coupling by hand"), *V.Name);
			Wait(Ci, GscPause() * 0.5f);
			ClimbOut(Ci, Ti, nullptr);
			for (int32 k = 0; k < Pair.Num(); ++k) CoupleCart(Ci, Ti, Pair[k], k == 0 ? INDEX_NONE : Pair[k - 1], nullptr);
			// then the bags: at the flight's make-up carousel beside the carts (GSEBags.cpp) - the train goes when it is made up
			if (Tractors[Ti].bArrival) ClimbIn(Ci, Ti, [this, Ti] { TrainToStand(Ti); });   // (an empty train for the arriving bags: straight out)
			else Do(Ci, [this, Ci, Ti] { const float Now = GetWorld()->GetTimeSeconds(); Later([this, Ci, Ti, Now] { MakeUp(Ci, Ti, Now); }); });
		});
	});
}

void AGSE::TrainToStand(int32 Ti)
{
	FTractor& T = Tractors[Ti];
	// (made up before its departure was called: waits in the lane until its stand is known)
	if (!bTest && !T.Key.IsEmpty() && T.StandX == 0.f)
	{
		if (const ATugs* G = Tg()) for (const ATugs::FTug& U : G->Tugs) if (U.Key == T.Key) T.StandX = U.StandX;
		if (T.StandX == 0.f) { RetryIn(Ti, 15.f, [this, Ti] { TrainToStand(Ti); }); return; }
	}
	T.Job = EJob::Out;
	RouteOut(T, [this, Ti] { TrainApron(Ti); });
}

void AGSE::TrainApron(int32 Ti)
{
	FTractor& T = Tractors[Ti];
	// its flight's stand (the test: the next departure; with none yet, the stand nearest the apron exit with an aircraft)
	if (T.Key.IsEmpty() && bTestStand) { T.Key = TestKey; T.StandX = TestStand; }
	// (a train made up for its flight's bags before its departure was called: the stand from its tug, else wait for it)
	if (!bTest && !T.Key.IsEmpty() && T.StandX == 0.f)
	{
		if (const ATugs* G = Tg()) for (const ATugs::FTug& U : G->Tugs) if (U.Key == T.Key) T.StandX = U.StandX;
		if (T.StandX == 0.f) { RetryIn(Ti, 15.f, [this, Ti] { TrainApron(Ti); }); return; }
	}
	float S = 0.f, N = 0.f, Sp = 36.f;
	if (T.Key.IsEmpty() || !StandGeo(T.StandX, S, N, Sp))
	{
		float Best = 1e9f; float BestX = 0.f; bool bAny = false;
		if (const ATugs* G = Tg())
			for (const ATugs::FTug& U : G->Tugs)
			{
				const float D = FMath::Abs(U.StandX - 269.f) - (AircraftAt(U.StandX) ? 1000.f : 0.f);
				if (D < Best) { Best = D; BestX = U.StandX; bAny = true; }
			}
		if (!bAny) { ADP_WARN(TEXT("GSE: no stand for %s's train - it waits"), *T.Name); RetryIn(Ti, 10.f, [this, Ti] { TrainApron(Ti); }); return; }
		T.StandX = BestX; if (T.Key.IsEmpty()) T.Key = TEXT("TEST");
		if (bTest && !bTestStand) { bTestStand = true; TestStand = BestX; TestKey = T.Key; }   // (both test trains to the same stand)
		StandGeo(T.StandX, S, N, Sp);
	}
	for (const int32 c : T.Train) if (Carts.IsValidIndex(c)) Carts[c].Flight = T.Key;
	// its belt loader to the bulk hold too (GSELoader.cpp), if none is there yet
	EnsureLoader(T.StandX, T.Key);
	// across the apron behind the stands' tails (the stand lines crossed only straight across) to the lane end of the
	// aircraft's right side (away from its jetway), 11 m out from its centre line: from there straight up beside it to
	// the belt loader's bottom end, behind the wing, clear of the tailplane
	TArray<FVector> W;
	if (!Apron(W, T.At, Plan(S - 11.f, N + 48.f), &T)) { RetryIn(Ti, 8.f, [this, Ti] { TrainApron(Ti); }); return; }
	ADP_LOG(TEXT("GSE: %s's train to %s at the stand x %.0f"), *T.Name, *T.Key, S);
	Go(T, MoveTemp(W), false, [this, Ti] { TrainHoldIn(Ti); });
}

void AGSE::TrainHoldIn(int32 Ti)
{
	// held beside the stand, ahead of the wingtip, until no aircraft moves at the stand (an arrival taxiing in, a push) -
	// then in ahead of the wing to its place beside the forward hold, facing the terminal (its way out ahead of it)
	FTractor& T = Tractors[Ti];
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(T.StandX, S, N, Sp);
	const FVector Nose = Plan(S, N);
	// (and the belt loader in its place first - it turns in across this side)
	T.Until = [this, Nose, S, Ti]
	{
		const int32 Li = LoaderAt(S); bool bOther = false;
		for (int32 o = 0; o < Tractors.Num(); ++o) if (o != Ti && !Tractors[o].bLoader && Tractors[o].bArrival && FMath::Abs(Tractors[o].StandX - S) < 1.f && (Tractors[o].Job == EJob::Out || Tractors[o].Job == EJob::AtStand || Tractors[o].Job == EJob::Loading)) bOther = true;
		// (arrivals first: a departure train waits for the arrivals train to leave the stand)
		return !PlaneNear(Nose, 6000.f) && (Li == INDEX_NONE || Tractors[Li].Job == EJob::AtStand) && (Tractors[Ti].bArrival || !bOther);
	};
	T.AfterUntil = [this, Ti, S, N]
	{
		FTractor& U = Tractors[Ti];
		TArray<FVector> W;
		Leg(W, S - 11.f, N + 48.f, 0.f, -1.f, S - 11.f, N + 25.74f, 0.f, -1.f, 400.f);   // (the tractor's front 24 m behind the nose wheel)
		Go(U, MoveTemp(W), false, [this, Ti] { TrainLoad(Ti); });
	};
}

void AGSE::TrainLoad(int32 Ti)
{
	if (Tractors[Ti].bArrival) { ArrivalLoad(Ti); return; }
	FTractor& T = Tractors[Ti]; T.Job = EJob::AtStand;
	ADP_LOG(TEXT("GSE: %s's train parked beside the aircraft at the stand x %.0f for %s"), *T.Name, T.StandX, *T.Key);
	const int32 Ci = T.Driver;
	Motor(T, false);
	Wait(Ci, GscPause());
	ClimbOut(Ci, Ti, [this, Ti] { Tractors[Ti].Job = EJob::Loading; });
	Curtains(Ci, Ti, true, nullptr);
	// the bags up the belt loader into the bulk hold (its belt running - waited for, 2.5 min at most), bag by bag
	const float StandX = T.StandX;
	WaitFor(Ci, [this, StandX](float E) { const int32 Li = LoaderAt(StandX); return Li == INDEX_NONE || Tractors[Li].bBelt || E > 150.f; });
	Do(Ci, [this, Ti] { ADP_LOG(TEXT("GSE: loading %s from %s's train into the bulk hold"), *Tractors[Ti].Key, *Tractors[Ti].Name); });
	Do(Ci, [this, Ci, Ti, StandX] { Later([this, Ci, Ti, StandX] { Unload(Ci, Ti, LoaderAt(StandX)); }); });
	// (the curtains down and the way home: queued by Unload once the bags are off - GSEBags.cpp)
}

void AGSE::TrainHome(int32 Ti)
{
	FTractor& T = Tractors[Ti]; T.Job = EJob::Back;
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(T.StandX, S, N, Sp);
	const float Xc = S - (Sp * 0.5f + 2.4f);
	// round to the right, away from the aircraft and the belt loader, behind the wing, out between the wingtips and off the
	// stand
	TArray<FVector> W;
	float Yw = 0.f; const FVector A = PlanPose(Xc, N + 30.f, 0.f, 1.f, Yw);
	Dubins(W, T.At, T.Yaw, A, Yw, 400.f);
	Leg(W, Xc, N + 30.f, 0.f, 1.f, Xc, N + 46.f, 0.f, 1.f, 400.f);
	ADP_LOG(TEXT("GSE: %s's train leaves the stand x %.0f"), *T.Name, S);
	Go(T, MoveTemp(W), false, [this, Ti]
	{
		FTractor& U = Tractors[Ti];
		RouteIn(U, U.At, U.Yaw, [this, Ti] { TrainDrop(Ti); });
	});
}

void AGSE::TrainDrop(int32 Ti)
{
	// in the lane: the carts left in a row beside the tractor boxes (ready for the next flight), uncoupled by hand
	FTractor& T = Tractors[Ti];
	const FVector2f H(T.Home.X / 100.f, -T.Home.Y / 100.f);
	TArray<FVector> W;
	Leg(W, 133.f, 50.f, 0.f, -1.f, H.X - 5.f, H.Y - 1.5f, 1.f, 0.f, 300.f);
	Go(T, MoveTemp(W), false, [this, Ti]
	{
		FTractor& U = Tractors[Ti]; U.Job = EJob::Uncoupling; const int32 Ci = U.Driver;
		ADP_LOG(TEXT("GSE: %s back in the lane - uncoupling"), *U.Name);
		Wait(Ci, GscPause() * 0.5f);
		ClimbOut(Ci, Ti, nullptr);
		const TArray<int32> Tr = U.Train;
		for (int32 k = Tr.Num() - 1; k >= 0; --k) UncoupleCart(Ci, Ti, Tr[k], k == 0 ? INDEX_NONE : Tr[k - 1], nullptr);
		ClimbIn(Ci, Ti, [this, Ti] { ToBox(Ti); });
	});
}

void AGSE::ToBox(int32 Ti)
{
	// forward into its box (nose in), the motor off, the driver out and back to his gate
	FTractor& T = Tractors[Ti]; T.Job = EJob::Home;
	const FVector2f H(T.Home.X / 100.f, -T.Home.Y / 100.f), P(T.At.X / 100.f, -T.At.Y / 100.f);
	TArray<FVector> W;
	const float Hx = FMath::Cos(FMath::DegreesToRadians(T.Yaw)), Hy = -FMath::Sin(FMath::DegreesToRadians(T.Yaw));
	if (FVector2f::Distance(P, H) > 0.3f) Leg(W, P.X, P.Y, Hx, Hy, H.X, H.Y, 1.f, 0.f, 300.f);
	Go(T, MoveTemp(W), false, [this, Ti]
	{
		FTractor& U = Tractors[Ti]; U.At = U.Home; U.Yaw = U.HomeYaw; Pose(U); Motor(U, false);
		const int32 Ci = U.Driver;
		ADP_LOG(TEXT("GSE: %s parked in its box, switched off"), *U.Name);
		Wait(Ci, GscPause());
		ClimbOut(Ci, Ti, [this, Ti] { FTractor& V = Tractors[Ti]; V.Job = EJob::None; V.Key.Reset(); V.StandX = 0.f; V.Driver = INDEX_NONE; V.bArrival = false; BagKey.Remove(Ti); });
		GiveBack(Ci);
	});
}

// ------------------------------------------------------------------ the GPU job (the same tractor, the middle slot)

void AGSE::StartGpu(int32 Ti, int32 Gate)
{
	if (!Tractors.IsValidIndex(Ti)) return;
	FTractor& T = Tractors[Ti];
	const int32 Ci = Borrow(T.At);
	if (Ci == INDEX_NONE) return;
	bWorked = true;
	T.Driver = Ci; T.Job = EJob::Boarding; T.JobT = 0.f;
	if (const ATugs* G = Tg()) if (G->Tugs.IsValidIndex(Gate)) T.StandX = G->Tugs[Gate].StandX;
	ADP_LOG(TEXT("GSE: %s - moving the GPU at the gate x %.0f a short way and back"), *T.Name, T.StandX);
	Wait(Ci, GscPause() + 4.f);
	ClimbIn(Ci, Ti, [this, Ti, Gate]
	{
		FTractor& U = Tractors[Ti]; Motor(U, true); U.Job = EJob::ToGpu;
		// backed out of its box and round to face the lane's door
		const FVector2f H(U.Home.X / 100.f, -U.Home.Y / 100.f);
		float Yw = 0.f; const FVector B = PlanPose(H.X - 12.5f, H.Y - 3.3f, 0.f, 1.f, Yw);
		const FVector Out = U.At - FRotator(0.f, U.Yaw, 0.f).Vector() * 400.f;
		TArray<FVector> W;
		Dubins(W, U.At, U.Yaw + 180.f, Out, U.Yaw + 180.f, 500.f);
		Dubins(W, Out, U.Yaw + 180.f, B, Yw + 180.f, 500.f);
		Go(U, MoveTemp(W), true, [this, Ti, Gate] { RouteOut(Tractors[Ti], [this, Ti, Gate] { GpuApron(Ti, Gate); }); });
	});
}

void AGSE::GpuApron(int32 Ti, int32 Gate)
{
	FTractor& T = Tractors[Ti];
	const ATugs* G = Tg(); if (!G || !G->Tugs.IsValidIndex(Gate) || !G->Tugs[Gate].Gpu.M.IsValid()) return;
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(G->Tugs[Gate].StandX, S, N, Sp);
	// (the stand must be empty and its GPU unplugged - else it waits)
	if (AircraftAt(S) || G->Tugs[Gate].Gpu.bConnected || G->Tugs[Gate].Gpu.bInHand) { ADP_LOG(TEXT("GSE: the GPU at x %.0f is in use - %s waits"), S, *T.Name); RetryIn(Ti, 10.f, [this, Ti, Gate] { GpuApron(Ti, Gate); }); return; }
	// down the empty stand on its side away from the jetway, round in front of the GPU and back onto its towing eye
	const ABabylonModel* Gm = G->Tugs[Gate].Gpu.M.Get();
	const float Gy = Gm->GetActorRotation().Yaw; const FVector Gf = FRotator(0.f, Gy, 0.f).Vector();
	const FVector Eye = Gm->GetActorTransform().TransformPosition(FVector(205.f, 0.f, 0.f));
	FVector Hook = Eye + Gf * 162.f; Hook = Plan(Hook.X / 100.f, -Hook.Y / 100.f);
	FVector Pre = Hook + Gf * 263.f; Pre = Plan(Pre.X / 100.f, -Pre.Y / 100.f);
	TArray<FVector> W;
	if (!Apron(W, T.At, Plan(S - 5.f, N + 16.f), &T)) { RetryIn(Ti, 8.f, [this, Ti, Gate] { GpuApron(Ti, Gate); }); return; }
	float Yd = 0.f; const FVector D0 = PlanPose(S - 5.f, N + 16.f, 0.f, -1.f, Yd);
	Dubins(W, D0, Yd, Pre, Gy, 350.f);
	Go(T, MoveTemp(W), false, [this, Ti, Gate, Hook, Gy]
	{
		FTractor& U = Tractors[Ti];
		TArray<FVector> B; Dubins(B, U.At, U.Yaw + 180.f, Hook, Gy + 180.f, 400.f);
		Go(U, MoveTemp(B), true, [this, Ti, Gate, Hook, Gy]
		{
			FTractor& V = Tractors[Ti]; V.Job = EJob::GpuCouple; const int32 Ci = V.Driver;
			Wait(Ci, GscPause() * 0.5f);
			ClimbOut(Ci, Ti, nullptr);
			CoupleGpu(Ci, Ti, Gate, true, nullptr);
			ClimbIn(Ci, Ti, [this, Ti, Gate, Hook, Gy]
			{
				// 4 m forward with it, stopped, and pushed straight back into its box
				FTractor& X = Tractors[Ti]; X.Job = EJob::GpuMove;
				const FVector Fw = FRotator(0.f, Gy, 0.f).Vector(); FVector Ahead = Hook + Fw * 400.f; Ahead = Plan(Ahead.X / 100.f, -Ahead.Y / 100.f);
				TArray<FVector> F; Dubins(F, X.At, X.Yaw, Ahead, Gy, 400.f);
				Go(X, MoveTemp(F), false, [this, Ti, Gate, Hook, Gy]
				{
					FTractor& Y = Tractors[Ti];
					RetryIn(Ti, 3.f, [this, Ti, Gate, Hook, Gy]
					{
						FTractor& Z = Tractors[Ti];
						TArray<FVector> R; Dubins(R, Z.At, Z.Yaw + 180.f, Hook, Gy + 180.f, 400.f);
						Go(Z, MoveTemp(R), true, [this, Ti, Gate]
						{
							FTractor& Q = Tractors[Ti]; Q.Job = EJob::GpuUncouple; const int32 Ci2 = Q.Driver;
							// (back exactly where it stood)
							if (FGpuRig* Rg = GpuRigs.Find(Gate)) if (ATugs* G2 = Tg()) if (G2->Tugs[Gate].Gpu.M.IsValid()) G2->Tugs[Gate].Gpu.M->SetActorLocationAndRotation(Rg->Home, FRotator(0.f, Rg->HomeYaw, 0.f));
							ClimbOut(Ci2, Ti, nullptr);
							CoupleGpu(Ci2, Ti, Gate, false, nullptr);
							ClimbIn(Ci2, Ti, [this, Ti, Gate] { GpuLeave(Ti, Gate); });
						});
					});
					(void)Y;
				});
			});
		});
	});
}

void AGSE::GpuLeave(int32 Ti, int32 Gate)
{
	FTractor& T = Tractors[Ti]; T.Job = EJob::Back;
	float S = 0.f, N = 0.f, Sp = 36.f;
	if (const ATugs* G = Tg()) if (G->Tugs.IsValidIndex(Gate)) StandGeo(G->Tugs[Gate].StandX, S, N, Sp);
	// round to the stand and down it, back across the apron and into the building, nose-in into its box
	TArray<FVector> W;
	float Yw = 0.f; const FVector A = PlanPose(S - 1.f, N + 2.f, 0.f, 1.f, Yw);
	Dubins(W, T.At, T.Yaw, A, Yw, 350.f);
	Leg(W, S - 1.f, N + 2.f, 0.f, 1.f, S - 1.f, N + 16.f, 0.f, 1.f, 350.f);
	Go(T, MoveTemp(W), false, [this, Ti]
	{
		FTractor& U = Tractors[Ti];
		RouteIn(U, U.At, U.Yaw, [this, Ti]
		{
			FTractor& V = Tractors[Ti]; const FVector2f H(V.Home.X / 100.f, -V.Home.Y / 100.f);
			TArray<FVector> B; Leg(B, 133.f, 50.f, 0.f, -1.f, H.X - 7.f, H.Y, 1.f, 0.f, 300.f);
			Go(V, MoveTemp(B), false, [this, Ti] { ToBox(Ti); });
		});
	});
}
