// The electric apron buses as vehicles (to-do 277, D3). One line of ADPService::All() (GSEService.cpp: model ApronBus, points
// Data/apronbus-riders.json) makes one vehicle for every row of Data/apron-buses.json; they drive, steer, brake and find their
// way as every service vehicle, yours by AServiceDrive, a hired bus driver's by the crew queue. AApronBuses keeps their looks.
// Here: the seven doors of two leaves (pushed out, then along the body; a chime first; never shut on anyone; no driving with
// one open), the body's walls and floor with real openings, the way to the driver's seat (front door, step, cab door) and the
// driving to a place and home. No riders, no schedule, no charging yet.
#include "GSE.h"
#include "ServiceVehicles.h"
#include "ApronBus.h"
#include "VehicleControls.h"
#include "CrewPose.h"
#include "StaffPeople.h"
#include "Tugs.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

struct FSvcBus
{
	int32 G[14]; FVector Piv[14];
	float Ask[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};      // (what each door is asked for: 0 shut, 1 open)
	float Chime[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};    // (seconds of warning still to go before it moves)
	bool bToldIn = false;                                    // (somebody in a doorway: said once)
	float HeldT = 0.f;                                       // (wants to go, doors not shut: how long)
	int32 Ci = INDEX_NONE; uint8 Stage = 0;                  // its hired driver: 0 none, 1 coming, 2 at the wheel, 3 getting out
	float Warn[2] = {-1.f, -1.f}, Run = -1.f, In = -1.f;     // (lamps as last set)
};

namespace
{
	constexpr float BusOut = 10.f, BusAlong = 80.f, BusMoveS = 2.6f, BusChimeS = 1.2f, BusFloor = 33.f, BusR = 11.2f;
	struct FBusDoor { const TCHAR* Key; float X, Sg; const TCHAR* Label; };
	const FBusDoor BusDoors[7] = {
		{TEXT("L+1p95"), 195.f, -1.f, TEXT("Left front door")}, {TEXT("L-1p55"), -155.f, -1.f, TEXT("Left middle door")}, {TEXT("L-5p15"), -515.f, -1.f, TEXT("Left rear door")},
		{TEXT("R+5p55"), 555.f, 1.f, TEXT("Front door")}, {TEXT("R+1p95"), 195.f, 1.f, TEXT("Right second door")}, {TEXT("R-1p55"), -155.f, 1.f, TEXT("Right third door")}, {TEXT("R-5p15"), -515.f, 1.f, TEXT("Right rear door")}};

	// a leaf's way: first out from the body, then along it (the front door's front leaf: no further than the bus's nose)
	FVector BusLeafMove(int32 d, int32 Leaf, float Now)
	{
		const float A = FMath::SmoothStep(0.f, 0.22f, Now), B = FMath::SmoothStep(0.16f, 1.f, Now);
		const float L = d == 3 && Leaf == 1 ? 42.f : BusAlong;
		return FVector((Leaf ? 1.f : -1.f) * L * B, BusDoors[d].Sg * BusOut * A, 0.f);
	}

	FVector BusPt(const AGSE::FTractor& T, const TCHAR* N) { const FVector* P = T.Pt.Find(N); return P ? *P : FVector::ZeroVector; }

	// ---- walking with planted feet along a way (root cm), K 0..1; the step up at the door's edge
	struct FBusWalk
	{
		TArray<FVector2D> P; float Len = 0.f;
		FBusWalk(std::initializer_list<FVector2D> In, bool bBack)
		{
			for (const FVector2D& V : In) if (bBack) P.Insert(V, 0); else P.Add(V);
			for (int32 i = 1; i < P.Num(); ++i) Len += float(FVector2D::Distance(P[i - 1], P[i]));
		}
		FVector2D At(float S) const
		{
			S = FMath::Clamp(S, 0.f, Len);
			for (int32 i = 1; i < P.Num(); ++i) { const float D = float(FVector2D::Distance(P[i - 1], P[i])); if (S <= D || i == P.Num() - 1) return FMath::Lerp(P[i - 1], P[i], FMath::Clamp(S / FMath::Max(D, 0.01f), 0.f, 1.f)); S -= D; }
			return P[0];
		}
		FVector2D Dir(float S) const { const FVector2D D = At(FMath::Min(S + 18.f, Len)) - At(FMath::Max(S - 18.f, 0.f)); return D.GetSafeNormal(); }
		// a foot set down there: never on the floor's very edge
		FVector Foot(float S, int32 Side) const
		{
			const FVector2D A = At(S), D = Dir(S); FVector F(A.X, A.Y, 0.f);
			F += FVector(D.Y, -D.X, 0.f) * (Side == 0 ? 9.f : -9.f);   // (left of the way: y turns right)
			if (F.Y < 153.f) { F.Y = FMath::Min(F.Y, 132.f); F.Z = BusFloor; } else F.Y = FMath::Max(F.Y, 166.f);
			return F;
		}
		void Pose(float K, float PH, FDriverPose& Q) const
		{
			const float Step = Len / FMath::Max(1, FMath::RoundToInt(Len / 52.f)), S = FMath::Clamp(K, 0.f, 1.f) * Len;
			const float U = S / Step + 0.5f; const int32 n = FMath::FloorToInt(U); const float f = U - n;
			Q = FDriverPose(); Q.Facing = FVector(Dir(S), 0.f);
			const FVector Stance = Foot(n * Step, n & 1), From = Foot((n - 1) * Step, (n + 1) & 1), To = Foot((n + 1) * Step, (n + 1) & 1);
			FVector Sw = FMath::Lerp(From, To, FMath::SmoothStep(0.1f, 0.9f, f));
			const float Dz = To.Z - From.Z;
			Sw.Z = From.Z + Dz * (Dz >= 0.f ? FMath::SmoothStep(0.f, 0.45f, f) : FMath::SmoothStep(0.55f, 1.f, f)) + (From.Equals(To, 1.f) ? 0.f : 10.f * FMath::Sin(PI * f));
			Q.Foot[n & 1] = Stance; Q.Foot[(n + 1) & 1] = Sw; Q.FootPitch[(n + 1) & 1] = 0.f;
			const FVector2D C = At(S); const float Zs = FMath::Lerp(BusFloor, 0.f, FMath::SmoothStep(112.f, 172.f, float(C.Y)));
			Q.Pelvis = FVector(C.X, C.Y, 0.f);
			// (the standing leg never longer than it is)
			const float D2 = float(FVector::DistSquared2D(Q.Pelvis, Stance)), Reach = FMath::Sqrt(FMath::Max(PH * PH - D2, PH * PH * 0.4f));
			Q.Pelvis.Z = FMath::Min(Zs + PH - 2.f, Stance.Z + Reach - 1.f);
			Q.Lean = Stance.Z != To.Z ? 8.f : 3.f;
		}
	};
	const FBusWalk& BusWayIn(bool bBack) { static const FBusWalk A({{640., 200.}, {552., 197.}, {550., 112.}, {592., 72.}, {632., 27.}}, false), B({{640., 200.}, {552., 197.}, {550., 112.}, {592., 72.}, {632., 27.}}, true); return bBack ? B : A; }
	const FBusWalk& BusWayCab(bool bBack) { static const FBusWalk A({{632., 27.}, {588., 0.}, {583., -46.}}, false), B({{632., 27.}, {588., 0.}, {583., -46.}}, true); return bBack ? B : A; }

	void BusToWorld(const AGSE::FTractor& T, FDriverPose& P)
	{
		const FRotator R(0.f, T.Yaw, 0.f);
		P.Pelvis = T.At + R.RotateVector(P.Pelvis); P.Facing = R.RotateVector(P.Facing);
		for (int32 s = 0; s < 2; ++s) { P.Foot[s] = T.At + R.RotateVector(P.Foot[s]); if (P.bHand[s]) P.Hand[s] = T.At + R.RotateVector(P.Hand[s]); }
	}
	FVector BusHandleNow(const AGSE::FTractor& T) { return T.KHinge + FRotator(0.f, T.DoorAng, 0.f).RotateVector(BusPt(T, TEXT("IK_CabDoorHandle")) - T.KHinge); }
}

int32 AGSE::BusOf(const FString& Name) const { for (int32 i = 0; i < Tractors.Num(); ++i) if (Tractors[i].Bu && Tractors[i].Name == Name) return i; return INDEX_NONE; }

void AGSE::BusDress(FTractor& T)
{
	T.Bu = MakeShared<FSvcBus>(); ABabylonModel* M = T.M.Get(); int32 Miss = 0;
	T.SvcDoors.Reset(); T.SvcDoors.SetNum(7);
	for (int32 d = 0; d < 7; ++d) for (int32 l = 0; l < 2; ++l)
	{
		TArray<FString> N; for (const TCHAR* K : {TEXT("DoorLeaf_"), TEXT("DoorGlass_"), TEXT("DoorGlassLow_"), TEXT("DoorEdgeRubber_"), TEXT("DoorHandleBar_"), TEXT("DoorPivot_")}) N.Add(FString::Printf(TEXT("%s%s_%d"), K, BusDoors[d].Key, l));
		const int32 i = d * 2 + l; T.Bu->Piv[i] = BusPt(T, *FString::Printf(TEXT("BUS_Door%d_%d"), d, l));
		T.Bu->G[i] = M ? M->BindGroup(N, true, FTransform(T.Bu->Piv[i])) : INDEX_NONE;
		if (T.Bu->G[i] == INDEX_NONE) ++Miss;
	}
	if (Miss) ADP_WARN(TEXT("Apron bus %s: %d door leaves not found in its model"), *T.Name, Miss);
	else ADP_LOG(TEXT("Apron bus %s: seven doors (14 leaves) ready"), *T.Name);
}

bool AGSE::BusDoorsShut(const FTractor& T) const
{
	if (!T.Bu) return true;
	for (int32 d = 0; d < 7; ++d) if (T.SvcDoors[d].Now > 0.001f || T.Bu->Ask[d] > 0.5f) return false;
	return true;
}

bool AGSE::BusDoorIs(int32 Ti, int32 Door, bool bOpen) const
{
	return Tractors.IsValidIndex(Ti) && Tractors[Ti].SvcDoors.IsValidIndex(Door) && (bOpen ? Tractors[Ti].SvcDoors[Door].Now > 0.99f : Tractors[Ti].SvcDoors[Door].Now < 0.001f);
}

bool AGSE::BusSomebodyIn(const FTractor& T, int32 Door) const
{
	const FBusDoor& D = BusDoors[Door]; const FRotator R(0.f, T.Yaw, 0.f);
	auto In = [&](const AActor* A) { const FVector L = R.UnrotateVector(A->GetActorLocation() - T.At); const float Y = float(L.Y) * D.Sg; return L.Z < 260.f && L.Z > -60.f && FMath::Abs(L.X - D.X) < 118.f && Y > 100.f && Y < 190.f; };
	const APlayerController* Pc = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Pc && Pc->GetPawn() && !T.Player.IsValid() && In(Pc->GetPawn())) return true;
	if (ATugs* G = Tg()) for (const ATugs::FCrew& C : G->Crew) if (!C.bSlotFree && C.A.IsValid() && !C.A->IsHidden() && In(C.A.Get())) return true;
	return false;
}

bool AGSE::BusDoor(int32 Ti, int32 Door, bool bOpen, FString& Why)
{
	if (!Tractors.IsValidIndex(Ti) || !Tractors[Ti].Bu || Door < 0 || Door > 6) { Why = TEXT("no such door"); return false; }
	FTractor& T = Tractors[Ti];
	if (bOpen && FMath::Abs(T.Speed) > 1.f) { Why = TEXT("the bus is not standing"); return false; }
	if ((T.Bu->Ask[Door] > 0.5f) == bOpen) return true;
	T.Bu->Ask[Door] = bOpen ? 1.f : 0.f; T.Bu->Chime[Door] = bOpen ? 0.4f : BusChimeS;
	Sound(TEXT("ABDoorChime"), TW(T, FVector(BusDoors[Door].X, BusDoors[Door].Sg * 148.f, 200.f)));
	return true;
}

void AGSE::BusSetAll(int32 Ti, bool bOpen) { FString Why; for (int32 d = 0; d < 7; ++d) BusDoor(Ti, d, bOpen, Why); }

void AGSE::BusParts(FTractor& T, float Dt)
{
	FSvcBus& B = *T.Bu; ABabylonModel* M = T.M.Get(); if (!M) return;
	bool bMove[2] = {false, false}, bOpenAny = false;
	for (int32 d = 0; d < 7; ++d)
	{
		FTractor::FSvcDoor& D = T.SvcDoors[d]; const float Was = D.Now;
		if (B.Chime[d] > 0.f) { B.Chime[d] -= Dt; bMove[BusDoors[d].Sg > 0.f] = true; }
		else if (B.Ask[d] < 0.5f && D.Now > 0.f && BusSomebodyIn(T, d))
		{
			// never shut on anyone: open again, and tried again when the doorway is free
			D.Want = 1.f; B.Chime[d] = BusChimeS; Sound(TEXT("ABDoorChime"), TW(T, FVector(BusDoors[d].X, BusDoors[d].Sg * 148.f, 200.f)));
			if (!B.bToldIn) { B.bToldIn = true; ADP_LOG(TEXT("Apron bus %s: somebody in the doorway of the %s - it waits"), *T.Name, BusDoors[d].Label); }
		}
		else if (D.Want != B.Ask[d] || D.Now != B.Ask[d])
		{
			if (D.Want != B.Ask[d] || Was == 1.f - B.Ask[d]) Sound(B.Ask[d] > 0.5f ? *T.SndDoorOpen : *T.SndDoorClose, TW(T, FVector(BusDoors[d].X, BusDoors[d].Sg * 148.f, 120.f)));
			D.Want = B.Ask[d]; if (D.Want < 0.5f) B.bToldIn = false;
		}
		if (D.Now != D.Want)
		{
			D.Now = FMath::FInterpConstantTo(D.Now, D.Want, Dt, 1.f / BusMoveS); bMove[BusDoors[d].Sg > 0.f] = true;
			for (int32 l = 0; l < 2; ++l) if (B.G[d * 2 + l] != INDEX_NONE) M->MoveGroup(B.G[d * 2 + l], FTransform(B.Piv[d * 2 + l] + BusLeafMove(d, l, D.Now)));
		}
		bOpenAny |= D.Now > 0.f;
	}
	// wants to go, a door still open: why it stands
	if (T.Path.Num() && !BusDoorsShut(T)) { if ((B.HeldT += Dt) > 15.f) { B.HeldT = 0.f; ADP_LOG(TEXT("Apron bus %s: waits - a door is not shut"), *T.Name); } }
	else B.HeldT = 0.f;
	// its own lamps: door warning while its doors move, running light, signs and the inside
	AApronBuses* Bs = AApronBuses::Find(); AApronBuses::FBus* Lk = Bs ? Bs->Of(M) : nullptr; if (!Lk) return;
	for (int32 s = 0; s < 2; ++s)
	{
		const float On = bMove[s] && FMath::Fmod(GetWorld()->GetTimeSeconds(), 0.7f) < 0.35f ? 1.f : 0.f;
		if (On != B.Warn[s]) { B.Warn[s] = On; Bs->SetLamps(*Lk, EApronBusLamp::DoorWarn, On, s ? -1 : 1); }
	}
	const float Run = T.bOn ? 1.f : 0.f, In = T.bOn || bOpenAny ? 1.f : 0.f;
	if (Run != B.Run) { B.Run = Run; Bs->SetLamps(*Lk, EApronBusLamp::Drl, Run); Bs->SetLamps(*Lk, EApronBusLamp::Destination, Run); }
	if (In != B.In) { B.In = In; Bs->SetLamps(*Lk, EApronBusLamp::Inside, In); }
}

TFunction<void(float, float, FDriverPose&)> AGSE::BusClimbFn(int32 Ti, bool bIn) const
{
	// in: through the front door, up onto the floor, to the cab door; it is opened by its handle; in, round and onto the seat.
	// out: the cab door opened from the seat, out, the door pushed to, down at the front door - walking forwards both ways
	return [this, Ti, bIn](float K, float PH, FDriverPose& P)
	{
		const FTractor& T = Tractors[Ti]; FTractor& Td = const_cast<FTractor&>(T);   // (the cab door goes with their hand)
		auto Seat = [&] { FDriverPose Q; SeatFn(Ti)(1.f, PH, Q); return Q; };
		auto Walk = [&](const FBusWalk& W, float U) { FDriverPose Q; W.Pose(U, PH, Q); BusToWorld(T, Q); return Q; };
		const FVector HandleW = TW(T, BusHandleNow(T));
		auto Hand = [&](FDriverPose& Q, float W) { if (T.DoorAng > 40.f) W *= FMath::Clamp((70.f - T.DoorAng) / 30.f, 0.f, 1.f); if (W <= 0.f) return; const FVector Of = Q.Pelvis + ADPPose::LeftOf(Q.Facing) * -22.f - FVector(0, 0, 15.f); Q.bHand[1] = true; Q.Hand[1] = FMath::Lerp(Of, HandleW, W); Q.Grip[1] = W; };
		auto Cab = [&](FDriverPose A, FDriverPose B, float U) { return ADPPose::Lerp(A, B, FMath::SmoothStep(0.f, 1.f, U)); };
		auto Turned = [&](const FBusWalk& W, float U, const FVector& FaceL, float Tu) { FDriverPose Q = Walk(W, U); Q.Facing = FMath::Lerp(Q.Facing, FRotator(0.f, T.Yaw, 0.f).RotateVector(FaceL), Tu).GetSafeNormal2D(); return Q; };
		const FVector AtDoor(-0.45f, -0.9f, 0.f);
		if (bIn)
		{
			if (K < 0.50f) P = Walk(BusWayIn(false), K / 0.50f);
			else if (K < 0.62f) { const float U = (K - 0.50f) / 0.12f; P = Turned(BusWayIn(false), 1.f, AtDoor, FMath::SmoothStep(0.f, 0.5f, U)); Hand(P, FMath::SmoothStep(0.f, 0.45f, U)); if (U > 0.5f) Td.DoorWant = T.KDoorDeg; }
			else if (K < 0.68f) P = Turned(BusWayIn(false), 1.f, AtDoor, 1.f);
			else if (K < 0.84f) P = Turned(BusWayCab(false), (K - 0.68f) / 0.16f, AtDoor, 1.f - FMath::SmoothStep(0.68f, 0.72f, K));
			else P = Cab(Walk(BusWayCab(false), 1.f), Seat(), (K - 0.84f) / 0.16f);
			if (K > 0.97f) Td.DoorWant = 0.f;
			return;
		}
		Td.DoorWant = K < 0.50f ? T.KDoorDeg : 0.f;
		if (K < 0.14f) { P = Seat(); }
		else if (K < 0.28f) P = Cab(Seat(), Walk(BusWayCab(true), 0.f), (K - 0.14f) / 0.14f);
		else if (K < 0.46f) P = Walk(BusWayCab(true), (K - 0.28f) / 0.18f);
		else if (K < 0.54f) { const float U = (K - 0.46f) / 0.08f; P = Turned(BusWayCab(true), 1.f, AtDoor, FMath::Sin(PI * FMath::Min(U, 0.5f))); Hand(P, ADPPose::Ramp(U)); if (U > 0.5f) P.Facing = FMath::Lerp(P.Facing, Walk(BusWayIn(true), 0.f).Facing, (U - 0.5f) * 2.f).GetSafeNormal2D(); }
		else P = Walk(BusWayIn(true), (K - 0.54f) / 0.46f);
	};
}

// ---- its hired driver: to the bus, to a place, home, out

void AGSE::BusWalkRound(int32 Ci, int32 Ti)
{
	ATugs* G = Tg(); if (!G || !G->Crew.IsValidIndex(Ci) || !G->Crew[Ci].A.IsValid()) return;
	const FTractor& T = Tractors[Ti]; const FVector L = FRotator(0.f, T.Yaw, 0.f).UnrotateVector(G->Crew[Ci].A->GetActorLocation() - T.At);
	if (L.Y > 165.f) return;   // (on its right already)
	const float X = L.X > -150.f ? 860.f : -860.f;   // (round the front or the rear)
	if (FMath::Abs(L.X) < 800.f || L.Y < -100.f) { const FVector A = TW(T, FVector(X, FMath::Min(float(L.Y), -220.f), 0.f)); WalkTo(Ci, [A] { return A; }, 40.f); }
	const FVector B = TW(T, FVector(X, 300.f, 0.f)); WalkTo(Ci, [B] { return B; }, 40.f);
}

bool AGSE::BusMan(int32 Ti, TFunction<void()> Then, FString& Why)
{
	FTractor& T = Tractors[Ti]; FSvcBus& B = *T.Bu; ATugs* G = Tg(); UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
	if (T.Player.IsValid()) { Why = TEXT("you are driving it"); return false; }
	if (B.Stage == 2) { Then(); return true; }
	if (B.Stage) { Why = B.Stage == 1 ? TEXT("its driver is on the way to it") : TEXT("its driver is getting out"); return false; }
	if (!G || !Sp || !T.M.IsValid()) { Why = TEXT("no crew yet"); return false; }
	if (T.Job != EJob::None || T.Driver != INDEX_NONE) { Why = TEXT("it is busy"); return false; }
	if (!Sp->HiresRole(TEXT("bus"))) { Why = TEXT("nobody is hired as a bus driver"); return false; }
	TArray<UStaffPeople::FTaken> Team; const FString Key = TEXT("bus:") + T.Name;
	if (!Sp->AskTeam(Key, EStaffTask::ApronBus, FDateTime(2000, 1, 1), T.At, T.Name, Team) || !Team.Num()) { Why = TEXT("no bus driver is free"); return false; }
	const int32 Ci = G->Enlist(Team[0].A.Get(), Team[0].Id, UStaffPeople::DutyName(Team[0].Duty));
	if (Ci == INDEX_NONE) { Why = TEXT("no crew place for its driver"); return false; }
	G->Crew[Ci].bGse = true; T.Driver = Ci; T.Job = EJob::Boarding; T.Key = Key; T.JobT = 0.f; B.Ci = Ci; B.Stage = 1;
	ADP_LOG(TEXT("Apron bus %s: %s goes to drive it"), *T.Name, *G->Crew[Ci].Name);
	BusWalkRound(Ci, Ti);
	ClimbIn(Ci, Ti, [this, Ti, Then = MoveTemp(Then)] { if (Tractors[Ti].Bu->Stage != 1) return; Tractors[Ti].Bu->Stage = 2; Motor(Tractors[Ti], true); Then(); });
	return true;
}

bool AGSE::BusDriveTo(int32 Ti, const FVector2f& To, float Heading, TFunction<void()> Done, FString& Why)
{
	if (!Tractors.IsValidIndex(Ti) || !Tractors[Ti].Bu) { Why = TEXT("no such bus"); return false; }
	if (Tractors[Ti].Path.Num()) { Why = TEXT("it is on its way"); return false; }
	return BusMan(Ti, [this, Ti, To, Heading, Done = MoveTemp(Done)] { BusRoute(Ti, To, Heading, Done); }, Why);
}

void AGSE::BusRoute(int32 Ti, FVector2f To, float Heading, TFunction<void()> Done)
{
	FTractor& T = Tractors[Ti]; if (!T.Bu || T.Bu->Stage != 2) return;
	BusSetAll(Ti, false); Motor(T, true);   // (it rolls when the doors are shut: Drive)
	const FVector Fw = FRotator(0.f, T.Yaw, 0.f).Vector(); const float a = FMath::DegreesToRadians(Heading), c = FMath::Cos(a), s = FMath::Sin(a);
	TArray<FVector> H; FVector From = T.At;
	if (FVector::Dist2D(T.At, T.Home) < 200.f) { H.Add(T.At + Fw * 400.f); H.Add(T.At + Fw * 800.f); From = T.At + Fw * 1200.f; H.Add(From); }   // (straight out of its bay)
	const float Ld = BusR + 8.f, Lx = To.X - c * Ld, Ly = To.Y - s * Ld;
	TArray<FVector> Way;
	if (!Apron(Way, From, Plan(Lx, Ly), &T))
	{
		ADP_LOG(TEXT("Apron bus %s: no way to plan (%.0f, %.0f) now - tried again"), *T.Name, To.X, To.Y);
		RetryIn(Ti, 8.f, [this, Ti, To, Heading, Done] { BusRoute(Ti, To, Heading, Done); });
		return;
	}
	H.Append(Way);
	// (the last of it in a straight line onto the place, with its heading)
	for (int32 i = 1; i <= 8; ++i) H.Add(Plan(FMath::Lerp(Lx, To.X, i / 8.f), FMath::Lerp(Ly, To.Y, i / 8.f)));
	ADP_LOG(TEXT("Apron bus %s: drives to plan (%.0f, %.0f), heading %.0f"), *T.Name, To.X, To.Y, Heading);
	Go(T, MoveTemp(H), false, [this, Ti, Done] { ADP_LOG(TEXT("Apron bus %s: there"), *Tractors[Ti].Name); if (Done) Done(); });
}

bool AGSE::BusHome(int32 Ti, TFunction<void()> Done, FString& Why)
{
	if (!Tractors.IsValidIndex(Ti) || !Tractors[Ti].Bu) { Why = TEXT("no such bus"); return false; }
	if (Tractors[Ti].Path.Num()) { Why = TEXT("it is on its way"); return false; }
	if (FVector::Dist2D(Tractors[Ti].At, Tractors[Ti].Home) < 100.f) { if (Done) Done(); return true; }
	return BusMan(Ti, [this, Ti, Done = MoveTemp(Done)] { BusRouteHome(Ti, Done); }, Why);
}

void AGSE::BusRouteHome(int32 Ti, TFunction<void()> Done)
{
	FTractor& T = Tractors[Ti]; if (!T.Bu || T.Bu->Stage != 2) return;
	BusSetAll(Ti, false); Motor(T, true);
	// across its bay's mouth, turning out of it, then backed in nose out (plan metres)
	const FVector W = FRotator(0.f, T.HomeYaw, 0.f).Vector(); const FVector2f Hd(W.X, -W.Y), Ho(T.Home.X / 100.f, -T.Home.Y / 100.f); const FVector2f At(T.At.X / 100.f, -T.At.Y / 100.f);
	FVector2f Sd(Hd.Y, -Hd.X); if (FVector2f::DotProduct(At - Ho, Sd) < 0.f) Sd = -Sd;
	const FVector2f P0 = Ho + Sd * (BusR + 16.f) + Hd * 14.f, P1 = Ho + Sd * (BusR + 0.4f) + Hd * 14.f, P2 = Ho + Hd * (14.4f + BusR), P3 = P2 + Hd * 4.f;
	TArray<FVector> H;
	if (!Apron(H, T.At, Plan(P0.X, P0.Y), &T))
	{
		ADP_LOG(TEXT("Apron bus %s: no way home now - tried again"), *T.Name);
		RetryIn(Ti, 8.f, [this, Ti, Done] { BusRouteHome(Ti, Done); });
		return;
	}
	Leg(H, P0.X, P0.Y, -Sd.X, -Sd.Y, P1.X, P1.Y, -Sd.X, -Sd.Y, T.KTurnR);
	Leg(H, P1.X, P1.Y, -Sd.X, -Sd.Y, P2.X, P2.Y, Hd.X, Hd.Y, T.KTurnR);
	H.Add(Plan(P3.X, P3.Y));
	ADP_LOG(TEXT("Apron bus %s: drives home"), *T.Name);
	Go(T, MoveTemp(H), false, [this, Ti, Done]
	{
		FTractor& V = Tractors[Ti]; TArray<FVector> Bk; const FVector A = V.At;
		for (int32 i = 1; i <= 30; ++i) Bk.Add(FMath::Lerp(A, V.Home, i / 30.f));
		Go(V, MoveTemp(Bk), true, [this, Ti, Done] { ADP_LOG(TEXT("Apron bus %s: in its bay again"), *Tractors[Ti].Name); Motor(Tractors[Ti], false); if (Done) Done(); });
	});
}

bool AGSE::BusLeave(int32 Ti)
{
	ATugs* G = Tg(); if (!G || !Tractors.IsValidIndex(Ti) || !Tractors[Ti].Bu) return false;
	FTractor& T = Tractors[Ti]; FSvcBus& B = *T.Bu;
	if (B.Stage != 2 || T.Path.Num() || T.Until || !G->Crew.IsValidIndex(B.Ci)) return false;
	const int32 Ci = B.Ci; B.Stage = 3; Motor(T, false); BusSetAll(Ti, false);
	ClimbOut(Ci, Ti, nullptr);
	Do(Ci, [this, Ti] { FTractor& V = Tractors[Ti]; V.Job = EJob::None; V.Key.Reset(); V.Driver = INDEX_NONE; V.Bu->Ci = INDEX_NONE; V.Bu->Stage = 0; });
	G->LeaveAct(G->Crew[Ci]);
	return true;
}

void AGSE::BusTick(float Dt)
{
	if ((BusPoll -= Dt) > 0.f) return;
	BusPoll = 2.f;
	ATugs* G = Tg(); UStaffPeople* Sp = UStaffPeople::Get(GetWorld()); if (!G || !Sp) return;
	for (int32 Ti = 0; Ti < Tractors.Num(); ++Ti)
	{
		FTractor& T = Tractors[Ti]; if (!T.Bu || !T.Bu->Stage) continue;
		const int32 Ci = T.Bu->Ci;
		if (!G->Crew.IsValidIndex(Ci) || !G->Crew[Ci].A.IsValid() || G->Crew[Ci].bSlotFree || T.Driver != Ci)
		{
			ADP_WARN(TEXT("Apron bus %s: its driver is lost (stage %d) - it stands, free again"), *T.Name, int32(T.Bu->Stage));
			T.Job = EJob::None; T.Key.Reset(); T.Driver = INDEX_NONE; T.Path.Reset(); T.OnArrive = nullptr; T.Until = nullptr; T.AfterUntil = nullptr; T.Bu->Ci = INDEX_NONE; T.Bu->Stage = 0;
			continue;
		}
		// shift over: standing in its bay he gets out (elsewhere he takes it home first)
		if (T.Bu->Stage != 2 || T.Path.Num() || T.Until || Sp->OnShift(G->Crew[Ci].StaffId)) continue;
		if (FVector::Dist2D(T.At, T.Home) < 150.f) BusLeave(Ti);
		else { FString Why; BusHome(Ti, nullptr, Why); }
	}
}

// ---- yours: the door buttons, and what is solid

TSharedPtr<FVehTable> AGSE::BusControls()
{
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>(); using K = FVehControl;
	static TArray<FString> Names; if (!Names.Num()) for (int32 d = 0; d < 7; ++d) for (const TCHAR* F : {TEXT("BUS_Sw%d"), TEXT("BUS_BtnOut%d"), TEXT("BUS_BtnIn%d")}) Names.Add(FString(F).Replace(TEXT("%d"), *FString::FromInt(d)));
	auto Add = [&](const TCHAR* Pt, int32 d, const TCHAR* Lb, bool bSeat) { K X; X.Part = TEXT(""); X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Motion = d; X.bSeat = bSeat; Tb->Controls.Add(X); };
	for (int32 d = 0; d < 7; ++d) { Add(*Names[d * 3], d, BusDoors[d].Label, true); Add(*Names[d * 3 + 1], d, BusDoors[d].Label, false); Add(*Names[d * 3 + 2], d, BusDoors[d].Label, false); }
	Add(TEXT("BUS_SwAll"), INDEX_NONE, TEXT("All doors"), true);
	// what is solid: floor, walls between the doors, ends, roof, the driver's cab, the leaves, the cab door
	auto Box = [&](float X0, float X1, float Y0, float Y1, float Z0, float Z1, int32 Fr = 0) { FVehSolid S; S.Min = FVector(X0, Y0, Z0); S.Max = FVector(X1, Y1, Z1); S.Frame = Fr; Tb->Solids.Add(S); };
	Box(-690, 690, -147, 147, 16, 33); Box(-699, 699, -151, 151, 281, 311); Box(690, 699, -151, 151, 16, 281); Box(-699, -690, -151, 151, 16, 281);
	const float WallR[][2] = {{652, 699}, {292, 458}, {-58, 98}, {-418, -252}, {-699, -612}}, WallL[][2] = {{292, 699}, {-58, 98}, {-418, -252}, {-699, -612}};
	for (const auto& w : WallR) Box(w[0], w[1], 146, 151, 33, 281);
	for (const auto& w : WallL) Box(w[0], w[1], -151, -146, 33, 281);
	for (const FBusDoor& D : BusDoors) Box(D.X - 97, D.X + 97, D.Sg > 0 ? 146 : -151, D.Sg > 0 ? 151 : -146, 250, 281);   // (over the doors)
	Box(533, 537, -144, -25, 33, 215); Box(615, 686, -27, -23, 33, 120); Box(626, 690, -144, -27, 33, 108);
	for (int32 d = 0; d < 7; ++d) for (int32 l = 0; l < 2; ++l) Box(BusDoors[d].X + (l ? 1.f : -94.f), BusDoors[d].X + (l ? 94.f : -1.f), BusDoors[d].Sg > 0 ? 148 : -153, BusDoors[d].Sg > 0 ? 153 : -148, 36, 249, 1 + d * 2 + l);
	Box(537, 613, -26, -23, 35, 118, 15);
	Tb->bAlways = true; Tb->Range = 900.f;
	Tb->Frame = [](const AGSE&, const FTractor& T, int32 Fr)
	{
		if (Fr >= 1 && Fr <= 14 && T.SvcDoors.Num() == 7) return FTransform(BusLeafMove((Fr - 1) / 2, (Fr - 1) % 2, T.SvcDoors[(Fr - 1) / 2].Now));
		if (Fr == 15) return FTransform(-T.KHinge) * FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(T.DoorAng)), T.KHinge);
		return FTransform::Identity;
	};
	Tb->Now = [](const FTractor& T, int32 M) { return T.SvcDoors.IsValidIndex(M) ? T.SvcDoors[M].Now : 0.f; };
	Tb->Want = [](FTractor& T, int32 M, float To) { if (T.Bu && T.SvcDoors.IsValidIndex(M) && (T.Bu->Ask[M] > 0.5f) != (To > 0.5f)) { T.Bu->Ask[M] = To > 0.5f ? 1.f : 0.f; T.Bu->Chime[M] = To > 0.5f ? 0.4f : BusChimeS; } };
	Tb->May = [](const AGSE& G, const FTractor& T, int32 M, float To, FString& Why)
	{
		if (FMath::Abs(T.Speed) > 1.f || T.Path.Num()) Why = TEXT("the bus is not standing");
		else if (!T.SvcDoors.IsValidIndex(M)) Why = TEXT("no such door");
		else if (To < 0.5f && G.BusSomebodyIn(T, M)) Why = TEXT("somebody is in the doorway");
		return Why.IsEmpty();
	};
	Tb->Press = [](AGSE& G, int32 Ti, const FVehControl& C)
	{
		FTractor& T = G.Tractors[Ti]; if (!T.Bu) return;
		if (C.Motion != INDEX_NONE) { G.Sound(TEXT("ABDoorChime"), G.TW(T, FVector(BusDoors[C.Motion].X, BusDoors[C.Motion].Sg * 148.f, 200.f))); return; }
		if (FMath::Abs(T.Speed) > 1.f || T.Path.Num()) return;
		G.BusSetAll(Ti, G.BusDoorsShut(T));
	};
	Tb->Say = [](const AGSE& G, const FTractor& T, const FVehControl& X) -> FString
	{
		if (X.Motion == INDEX_NONE) return G.BusDoorsShut(T) ? TEXT("All doors: open") : TEXT("All doors: shut");
		return T.SvcDoors.IsValidIndex(X.Motion) ? FString(X.Label) + (T.SvcDoors[X.Motion].Now > 0.5f ? TEXT(": shut") : TEXT(": open")) : FString();
	};
	Tb->Aboard = [](const FTractor&, const FVector& L) -> uint8 { return FMath::Abs(L.X) < 690.f && FMath::Abs(L.Y) < 147.f && L.Z > 15.f && L.Z < 140.f ? 1 : 0; };
	Tb->Stowed = [](const AGSE& G, const FTractor& T) { return G.BusDoorsShut(T); };
	Tb->Stow = [](AGSE& G, FTractor& T) { const int32 Ti = int32(&T - G.Tractors.GetData()); if (G.Tractors.IsValidIndex(Ti)) G.BusSetAll(Ti, false); };
	ADP_LOG(TEXT("Apron bus: your controls - seven doors, all doors; the cab's lights, horn and hazard as every vehicle's"));
	return Tb;
}
