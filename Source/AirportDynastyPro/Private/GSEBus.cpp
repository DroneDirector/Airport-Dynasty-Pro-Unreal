// The electric apron buses as vehicles (to-do 277, D3). One line of ADPService::All() (GSEService.cpp: model ApronBus, points
// Data/apronbus-riders.json) makes one vehicle for every row of Data/apron-buses.json; they drive, steer, brake and find their
// way as every service vehicle, yours by AServiceDrive, a hired bus driver's by the crew queue. AApronBuses keeps their looks.
// Here: the seven doors of two leaves (pushed out, then along the body; a chime first; never shut on anyone; no driving with
// one open), the body's walls and floor with real openings, the way to the driver's seat (front door, step, cab door) and the
// driving to a place and home; let down at a stop (kneeling), its electric sounds, you riding in it (standing, holding a pole
// or strap, sitting: Seats.cpp). No passengers of its own, no schedule, no charging yet.
#include "GSE.h"
#include "ServiceVehicles.h"
#include "ApronBus.h"
#include "VehicleControls.h"
#include "CrewPose.h"
#include "StaffPeople.h"
#include "Tugs.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Seats.h"
#include "Components/AudioComponent.h"
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
	enum ESnd { Whine, Avas, AvasBack, Regen, Cool, Comp, Ind, NSnd };
	TWeakObjectPtr<UAudioComponent> Snd[NSnd];
	float Air = 1.f, StoodT = 0.f, Lv[NSnd] = {}; bool bComp = false, bIn = false, bRolled = false, bDown[2] = {false, false};   // (its air pressure; the sounds' levels; you inside)
	FTransform WasX; bool bWasX = false; FVector LastAt = FVector::ZeroVector;
	TArray<TWeakObjectPtr<AActor>> Riders;                   // (passengers on their way on or off, or in it)
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
		const FTransform Kn = T.KneelX();
		P.Pelvis = T.At + R.RotateVector(Kn.TransformPosition(P.Pelvis)); P.Facing = R.RotateVector(P.Facing);
		for (int32 s = 0; s < 2; ++s) { if (P.Foot[s].Z > 10.) P.Foot[s] = Kn.TransformPosition(P.Foot[s]); P.Foot[s] = T.At + R.RotateVector(P.Foot[s]); if (P.bHand[s]) P.Hand[s] = T.At + R.RotateVector(Kn.TransformPosition(P.Hand[s])); }   // (on the floor: let down with it)
	}
	// ---- poles and straps to hold (root cm; the parts Pole_n, StrapLoop_Ln / Rn)
	constexpr int32 BusSwKneel = 1, BusSwPole = 100, BusSwStrap = 200;
	const FVector2D BusPoles[28] = {{-615., 120.}, {-615., -120.}, {-610., 62.}, {-610., -62.}, {-430., 62.}, {-430., -62.}, {-415., 120.}, {-415., -120.}, {-270., 62.}, {-270., -62.}, {-255., 120.}, {-255., -120.}, {-55., 120.}, {-55., -120.},
		{-50., 62.}, {-50., -62.}, {90., 62.}, {90., -62.}, {95., 120.}, {95., -120.}, {290., 62.}, {290., -62.}, {295., 120.}, {295., -120.}, {440., 62.}, {440., -62.}, {455., 120.}, {655., 120.}};
	const int32 BusStraps[19] = {1, 2, 3, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
	bool BusHoldAt(int32 Code, FVector& P, bool& bStrap)
	{
		bStrap = Code >= BusSwStrap;
		if (bStrap) { const int32 i = Code - BusSwStrap; if (i >= 38) return false; P = FVector(-600. + 48. * BusStraps[i / 2], i % 2 ? 62. : -62., 221.); return true; }
		if (Code < BusSwPole || Code >= BusSwPole + 28) return false;
		P = FVector(BusPoles[Code - BusSwPole], BusFloor + 138.); return true;
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
	for (const TWeakObjectPtr<AActor>& A : T.Bu->Riders) if (A.IsValid() && !A->IsHidden() && In(A.Get())) return true;
	return false;
}

void AGSE::BusRider(int32 Ti, AActor* Who, bool bOn)
{
	if (!Tractors.IsValidIndex(Ti) || !Tractors[Ti].Bu || !Who) return;
	TArray<TWeakObjectPtr<AActor>>& R = Tractors[Ti].Bu->Riders;
	R.RemoveAllSwap([&](const TWeakObjectPtr<AActor>& A) { return !A.IsValid() || A.Get() == Who; });
	if (bOn) R.Add(Who);
}

bool AGSE::BusHang(const FTractor& T, const FVector& Feet, FVector& At, FVector& Fingers, FVector& Thumb) const
{
	FVector P; bool bStrap = false; if (!T.Bu || !BusHoldAt(T.YouHang, P, bStrap)) return false;
	const FVector W = TW(T, P); FVector D = W - Feet; D.Z = 0.; const double L = D.Size();
	if (L > (bStrap ? 50. : 72.) || L < 1.) return false;
	D /= L;
	Fingers = bStrap ? FVector::UpVector : D; Thumb = bStrap ? FVector::CrossProduct(D, FVector::UpVector) : FVector::UpVector;
	At = W - Fingers * 8.5;
	return true;
}

void AGSE::BusKneel(FTractor& T, float Dt)
{
	// let down 7 cm on the side whose doors are open (its driver: with the doors; you: by your switch); up again before it goes
	FSvcBus& B = *T.Bu; const bool bStands = FMath::Abs(T.Speed) < 1.f, bShut = BusDoorsShut(T);
	if (!T.Player.IsValid() && B.Stage != 2 && bShut) T.KneelAsk[0] = T.KneelAsk[1] = false;
	bool bTurn = false;
	for (int32 s = 0; s < 2; ++s)
	{
		bool bDoor = false; for (int32 d = 0; d < 7; ++d) if ((BusDoors[d].Sg > 0.f) == (s == 1) && (B.Ask[d] > 0.5f || T.SvcDoors[d].Now > 0.001f)) bDoor = true;
		const bool bWant = bStands && (T.Player.IsValid() ? T.KneelAsk[s] : B.Stage == 2 ? bDoor && !T.Path.Num() : T.KneelAsk[s]);
		if (bWant != B.bDown[s])
		{
			B.bDown[s] = bWant; B.Air = FMath::Max(0.f, B.Air - 0.1f);
			SndOn(T, bWant ? TEXT("ABKneelDown") : TEXT("ABKneelUp"), FVector(0.f, s ? 100.f : -100.f, 40.f), B.bIn ? 0.5f : 0.9f, false);
			ADP_LOG(TEXT("Apron bus %s: %s on its %s side"), *T.Name, bWant ? TEXT("kneels") : TEXT("rises"), s ? TEXT("right") : TEXT("left"));
		}
		const float To = B.bDown[s] ? 1.f : 0.f;
		if (T.Knelt[s] != To) { T.Knelt[s] = FMath::FInterpConstantTo(T.Knelt[s], To, Dt, 0.5f); bTurn = true; }
	}
	if (bTurn) Pose(T);
}

void AGSE::BusSounds(FTractor& T, float Dt)
{
	// the electric drive: its hum and whine by the speed, the warning sound under 20 km/h (its own backwards), the braking
	// current, air brakes, compressor and dryer, air conditioning, indicators. What is outside: muffled while you are inside
	FSvcBus& B = *T.Bu; const bool bIn = T.YouOn || T.Player.IsValid();
	if (bIn != B.bIn) { B.bIn = bIn; B.Lv[FSvcBus::Cool] = -1.f; }
	const float V = T.MeasSpeed, Kmh = V * 3.6f;
	const double Along = FVector::DotProduct(T.At - B.LastAt, FRotator(0.f, T.Yaw, 0.f).Vector()); B.LastAt = T.At;
	if (FMath::Abs(Along) > 0.01) B.bRolled = Along < 0.;   // (backwards)
	const bool bOn = T.bOn, bGoes = bOn && V > 0.1f, bBrake = T.BrakeHold > 0.f;
	auto One = [&](const TCHAR* N, const FVector& At, float Vol) { if (UAudioComponent* A = SndOn(T, N, At, Vol * (bIn ? 0.55f : 1.f), false); A && bIn) { A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency(900.f); } };
	if (V > 0.3f) { if (B.StoodT > 1.f) { One(TEXT("ABBrakeRelease"), FVector(-330.f, 0.f, 60.f), 0.7f); B.Air = FMath::Max(0.f, B.Air - 0.04f); } B.StoodT = 0.f; }
	else if (V < 0.05f) { if (B.StoodT == 0.f && bOn) { One(TEXT("ABStopBrakeApply"), FVector(300.f, 0.f, 60.f), 0.7f); B.Air = FMath::Max(0.f, B.Air - 0.06f); } B.StoodT += Dt; }
	// its air: used by brakes, doors and springs, pumped up by the compressor, dried with a puff when full
	if (bOn)
	{
		for (int32 d = 0; d < 7; ++d) if (T.SvcDoors[d].Now != T.SvcDoors[d].Want) B.Air = FMath::Max(0.f, B.Air - 0.008f * Dt);
		B.Air = FMath::Clamp(B.Air + (B.bComp ? 0.02f : -0.0012f) * Dt, 0.f, 1.f);
		if (!B.bComp && B.Air < 0.75f) { B.bComp = true; One(TEXT("ABCompressorStart"), FVector(-600.f, 0.f, 70.f), 0.6f); }
		else if (B.bComp && B.Air >= 1.f) { B.bComp = false; One(TEXT("ABAirDryerPurge"), FVector(-560.f, 60.f, 50.f), 0.8f); }
	}
	else B.bComp = false;
	const bool bInd = T.bSwHaz || (bOn && (CabMine(T) ? T.Sig != 0 : FMath::Abs(T.Steer) > 8.f));
	const float Low = bGoes ? 1.f - FMath::SmoothStep(18.f, 21.f, Kmh) : 0.f;
	const float Want[FSvcBus::NSnd] = {bGoes ? 0.18f + 0.5f * FMath::Clamp(V / 9.f, 0.f, 1.f) : 0.f, B.bRolled ? 0.f : 0.6f * Low, B.bRolled ? 0.6f * Low : 0.f,
		bGoes && bBrake && V > 1.f ? 0.55f : 0.f, bOn ? (bIn ? 0.35f : 0.08f) : 0.f, B.bComp ? 0.5f : 0.f, bInd ? 0.3f : 0.f};
	static const TCHAR* const Names[FSvcBus::NSnd] = {TEXT("ABMotorWhineLoop"), TEXT("ABAVASLoop"), TEXT("ABAVASReverseLoop"), TEXT("ABRegenLoop"), TEXT("ABACLoop"), TEXT("ABCompressorLoop"), TEXT("ABIndicatorLoop")};
	static const FVector At[FSvcBus::NSnd] = {FVector(-330.f, 0.f, 60.f), FVector(660.f, 0.f, 50.f), FVector(-660.f, 0.f, 50.f), FVector(-330.f, 0.f, 60.f), FVector(0.f, 0.f, 270.f), FVector(-600.f, 0.f, 70.f), FVector(645.f, -60.f, 110.f)};
	const float Pitch[FSvcBus::NSnd] = {0.7f + 0.9f * FMath::Clamp(V / 11.f, 0.f, 1.f), 0.9f + 0.3f * FMath::Clamp(Kmh / 20.f, 0.f, 1.f), 1.f, 0.8f + 0.4f * FMath::Clamp(V / 9.f, 0.f, 1.f), 1.f, 1.f, 1.f};
	for (int32 i = 0; i < FSvcBus::NSnd; ++i)
	{
		const bool bInside = i == FSvcBus::Cool || i == FSvcBus::Ind, bMuf = bIn && !bInside;
		B.Lv[i] = FMath::FInterpTo(FMath::Max(0.f, B.Lv[i]), Want[i], Dt, 5.f);
		UAudioComponent* A = B.Snd[i].Get();
		if (B.Lv[i] < 0.015f && Want[i] <= 0.f) { if (A) { A->Stop(); A->DestroyComponent(); B.Snd[i].Reset(); } continue; }
		if (!A) { A = SndOn(T, Names[i], At[i], 0.01f, true); B.Snd[i] = A; if (!A) continue; }
		A->SetVolumeMultiplier(FMath::Max(0.01f, B.Lv[i] * (bMuf ? 0.6f : 1.f))); A->SetPitchMultiplier(Pitch[i]);
		A->SetLowPassFilterEnabled(bMuf); A->SetLowPassFilterFrequency(900.f);
	}
	if (T.Motor)
	{
		T.Motor->SetPitchMultiplier(0.85f + 0.4f * FMath::Clamp(V / 9.f, 0.f, 1.f)); T.Motor->SetVolumeMultiplier((0.12f + 0.6f * FMath::Clamp(V / 9.f, 0.f, 1.f)) * (bIn ? 0.6f : 1.f));
		T.Motor->SetLowPassFilterEnabled(bIn); T.Motor->SetLowPassFilterFrequency(900.f);
	}
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
	BusKneel(T, Dt); BusSounds(T, Dt);
	// its seats, and who sits on them, go where it goes
	const FTransform Nx = M->GetActorTransform();
	if (B.bWasX && !Nx.Equals(B.WasX, 1e-5)) ASeats::Carry(M, B.WasX, Nx, this);
	B.WasX = Nx; B.bWasX = true;
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
	if (T.Path.Num() && (!BusDoorsShut(T) || BusKnelt(T))) { if ((B.HeldT += Dt) > 15.f) { B.HeldT = 0.f; ADP_LOG(TEXT("Apron bus %s: waits - %s"), *T.Name, BusDoorsShut(T) ? TEXT("not up yet") : TEXT("a door is not shut")); } }
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
		if (T.Bu->Riders.ContainsByPredicate([](const TWeakObjectPtr<AActor>& W) { return W.IsValid(); })) continue;   // (never home with passengers in it)
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
	{ K X; X.Part = TEXT(""); X.Point = TEXT("BUS_SwAll"); X.Kind = K::Push; X.Label = TEXT("Kneel"); X.Switch = BusSwKneel; X.bSeat = true; Tb->Controls.Add(X); }
	// a pole or strap in your hand while you ride
	static TArray<FString> Holds; if (!Holds.Num()) { for (int32 i = 0; i < 28; ++i) Holds.Add(FString::Printf(TEXT("Pole_%d"), i)); for (int32 i = 0; i < 38; ++i) Holds.Add(FString::Printf(TEXT("StrapLoop_%s%d"), i % 2 ? TEXT("R") : TEXT("L"), BusStraps[i / 2])); }
	for (int32 i = 0; i < Holds.Num(); ++i) { K X; X.Part = *Holds[i]; X.Point = TEXT(""); X.Kind = K::Push; X.Label = i < 28 ? TEXT("Pole") : TEXT("Strap"); X.Switch = i < 28 ? BusSwPole + i : BusSwStrap + i - 28; X.bGrip = X.bHelp = true; Tb->Controls.Add(X); }
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
	// inside: the wheel houses with their steps and seats (pans, backs, the rail before them), luggage racks, poles, the cab's post,
	// the fire extinguisher, the wheelchair place's backrest
	const float Hs[][3] = {{301, 451, 1}, {301, 451, -1}, {-415, -253, 1}, {-415, -253, -1}};
	for (const auto& h : Hs)
	{
		auto Side = [&](float Y0, float Y1, float Z0, float Z1, float X0, float X1) { Box(X0, X1, h[2] > 0.f ? Y0 : -Y1, h[2] > 0.f ? Y1 : -Y0, Z0, Z1); };
		const bool bFront = h[0] > 0.f, bSeats = !(bFront && h[2] < 0.f);
		Side(bFront ? 88.f : 78.f, 146.f, 33.f, 108.f, h[0], h[1]);
		if (!bSeats) continue;
		Side(bFront ? 76.f : 66.f, bFront ? 88.f : 78.f, 33.f, 57.f, h[0], h[1]); Side(96.f, 136.f, 108.f, 120.f, h[0], h[1]); Side(135.f, 146.f, 120.f, 178.f, h[0], h[1]); Side(93.f, 95.f, 138.f, 141.f, h[0], h[1]);
	}
	for (const float s : {-1.f, 1.f}) Box(-37, 57, s > 0 ? 118 : -146, s > 0 ? 146 : -118, 33, 150);
	for (const FVector2D& P : BusPoles) Box(P.X - 2.5, P.X + 2.5, P.Y - 2.5, P.Y + 2.5, 33, 273);
	Box(532, 538, -28, -22, 33, 216); Box(538, 552, -137, -123, 60, 120); Box(380, 460, 137, 146, 65, 147);
	Tb->Spot = [](const AGSE& G, const FTractor& T, const FVehControl& C, FVector& Out)
	{
		FVector P; bool bStrap = false;
		if (C.Switch == BusSwKneel) { Out = G.TW(T, FVector(648.f, -54.f, 105.5f)); return true; }
		if (C.Switch == INDEX_NONE || !BusHoldAt(C.Switch, P, bStrap)) return false;
		Out = G.TW(T, P); return true;
	};
	Tb->Switch = [](AGSE& G, int32 Ti, int32 Code)
	{
		FTractor& T = G.Tractors[Ti]; if (!T.Bu) return false;
		if (Code == BusSwKneel)
		{
			if (T.KneelAsk[0] || T.KneelAsk[1]) { T.KneelAsk[0] = T.KneelAsk[1] = false; return true; }
			for (int32 d = 0; d < 7; ++d) if (T.Bu->Ask[d] > 0.5f || T.SvcDoors[d].Now > 0.f) T.KneelAsk[BusDoors[d].Sg > 0.f] = true;
			if (!T.KneelAsk[0]) T.KneelAsk[1] = true;   // (no door open: the front door's side)
			return true;
		}
		T.YouHang = T.YouHang == Code ? INDEX_NONE : Code; return true;
	};
	Tb->MaySwitch = [](AGSE& G, int32 Ti, int32 Code, FString& Why)
	{
		const FTractor& T = G.Tractors[Ti];
		if (Code == BusSwKneel) { if (FMath::Abs(T.Speed) > 1.f || T.Path.Num()) Why = TEXT("the bus is not standing"); else if (!T.bOn) Why = TEXT("its drive is off"); }
		return Why.IsEmpty();
	};
	Tb->bAlways = true; Tb->Range = 900.f;
	Tb->Frame = [](const AGSE&, const FTractor& T, int32 Fr)
	{
		if (Fr >= 1 && Fr <= 14 && T.SvcDoors.Num() == 7) return FTransform(BusLeafMove((Fr - 1) / 2, (Fr - 1) % 2, T.SvcDoors[(Fr - 1) / 2].Now));
		if (Fr == 15) return FTransform(-T.KHinge) * FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(T.DoorAng)), T.KHinge);
		return FTransform::Identity;
	};
	Tb->Now = [](const FTractor& T, int32 M) { return T.SvcDoors.IsValidIndex(M) ? T.SvcDoors[M].Now : 0.f; };
	Tb->Want = [](FTractor& T, int32 M, float To) { if (T.Bu && T.SvcDoors.IsValidIndex(M) && !(To > 0.5f && FMath::Abs(T.Speed) > 1.f) && (T.Bu->Ask[M] > 0.5f) != (To > 0.5f)) { T.Bu->Ask[M] = To > 0.5f ? 1.f : 0.f; T.Bu->Chime[M] = To > 0.5f ? 0.4f : BusChimeS; } };
	Tb->May = [](const AGSE& G, const FTractor& T, int32 M, float To, FString& Why)
	{
		if (FMath::Abs(T.Speed) > 1.f || T.Path.Num()) Why = TEXT("the bus is not standing");
		else if (!T.SvcDoors.IsValidIndex(M)) Why = TEXT("no such door");
		else if (To < 0.5f && G.BusSomebodyIn(T, M)) Why = TEXT("somebody is in the doorway");
		return Why.IsEmpty();
	};
	Tb->Press = [](AGSE& G, int32 Ti, const FVehControl& C)
	{
		FTractor& T = G.Tractors[Ti]; if (!T.Bu || C.Switch != INDEX_NONE) return;
		if (C.Motion != INDEX_NONE) { G.Sound(TEXT("ABDoorChime"), G.TW(T, FVector(BusDoors[C.Motion].X, BusDoors[C.Motion].Sg * 148.f, 200.f))); return; }
		if (FMath::Abs(T.Speed) > 1.f || T.Path.Num()) return;
		G.BusSetAll(Ti, G.BusDoorsShut(T));
	};
	Tb->Say = [](const AGSE& G, const FTractor& T, const FVehControl& X) -> FString
	{
		if (X.Switch == BusSwKneel) return T.KneelAsk[0] || T.KneelAsk[1] ? TEXT("Kneel: up") : TEXT("Kneel: down");
		if (X.Switch != INDEX_NONE)
		{
			FVector P; bool bStrap = false;
			if (!T.YouOn || ASeats::IsSitting() || !BusHoldAt(X.Switch, P, bStrap)) return FString();
			if (T.YouHang == X.Switch) return bStrap ? TEXT("Let go of the strap") : TEXT("Let go of the pole");
			return FVector::Dist2D(P, T.YouAt) > (bStrap ? 44. : 64.) ? FString() : FString(bStrap ? TEXT("Hold the strap") : TEXT("Hold the pole"));
		}
		if (X.Motion == INDEX_NONE) return G.BusDoorsShut(T) ? TEXT("All doors: open") : TEXT("All doors: shut");
		return T.SvcDoors.IsValidIndex(X.Motion) ? FString(X.Label) + (T.SvcDoors[X.Motion].Now > 0.5f ? TEXT(": shut") : TEXT(": open")) : FString();
	};
	Tb->Aboard = [](const FTractor&, const FVector& L) -> uint8 { return FMath::Abs(L.X) < 690.f && FMath::Abs(L.Y) < 147.f && L.Z > 15.f && L.Z < 140.f ? 1 : 0; };
	Tb->Stowed = [](const AGSE& G, const FTractor& T) { return G.BusDoorsShut(T); };
	Tb->Stow = [](AGSE& G, FTractor& T) { const int32 Ti = int32(&T - G.Tractors.GetData()); if (G.Tractors.IsValidIndex(Ti)) G.BusSetAll(Ti, false); };
	ADP_LOG(TEXT("Apron bus: your controls - seven doors, all doors; the cab's lights, horn and hazard as every vehicle's"));
	return Tb;
}

// ---- the passengers' side (PaxApron.cpp)

FVector2f AGSE::BusDoorAt(int32 Door) { return Door >= 0 && Door < 7 ? FVector2f(BusDoors[Door].X, BusDoors[Door].Sg * 148.f) : FVector2f::ZeroVector; }

void AGSE::BusFree(TArray<int32>& Out) const
{
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		const FTractor& T = Tractors[i]; if (!T.Bu || !T.M.IsValid() || T.Player.IsValid() || T.Path.Num() || T.Until) continue;
		if ((T.Bu->Stage == 0 && T.Job == EJob::None) || T.Bu->Stage == 2) Out.Add(i);
	}
}

bool AGSE::BusSee(int32 Ti, FBusSee& Out) const
{
	if (!Tractors.IsValidIndex(Ti) || !Tractors[Ti].Bu || !Tractors[Ti].M.IsValid()) return false;
	const FTractor& T = Tractors[Ti]; Out.X = FTransform(FRotator(0.f, T.Yaw, 0.f), T.At); Out.bStands = FMath::Abs(T.Speed) < 1.f && T.Path.Num() == 0;
	Out.bAtWheel = T.Bu->Stage == 2; Out.bYou = T.Player.IsValid(); Out.bShut = BusDoorsShut(T); Out.bKnelt = BusKnelt(T);
	return true;
}

bool AGSE::BusTry(int32 Ti, const FVector2f& To, float Heading) const
{
	if (!Tractors.IsValidIndex(Ti) || !Tractors[Ti].Bu) return false;
	const FTractor& T = Tractors[Ti]; const float a = FMath::DegreesToRadians(Heading), Ld = BusR + 8.f;
	const FVector Lead = Plan(To.X - FMath::Cos(a) * Ld, To.Y - FMath::Sin(a) * Ld), End = Plan(To.X, To.Y);
	FVector From = T.At; if (FVector::Dist2D(T.At, T.Home) < 200.f) From += FRotator(0.f, T.Yaw, 0.f).Vector() * 1200.f;
	TArray<FVector> W; if (!Apron(W, Lead, End, &T)) return false;
	float L = 0.f; FVector Was = Lead; for (const FVector& V : W) { L += float(FVector::Dist2D(Was, V)); Was = V; }   // (straight in, nothing to go round)
	if (L > Ld * 110.f) return false;
	W.Reset(); return Apron(W, From, Lead, &T);
}

FVector AGSE::BusW(int32 Ti, const FVector& Local) const
{
	if (!Tractors.IsValidIndex(Ti)) return FVector::ZeroVector;
	const FTractor& T = Tractors[Ti]; return T.At + FRotator(0.f, T.Yaw, 0.f).RotateVector(Local.Z > 10. ? T.KneelX().TransformPosition(Local) : Local);
}

void AGSE::BusPoseW(int32 Ti, FDriverPose& P) const { if (Tractors.IsValidIndex(Ti)) BusToWorld(Tractors[Ti], P); }

void AGSE::BusSlots(int32 Ti, TArray<FBusSlot>& Out) const
{
	Out.Reset(); if (!Tractors.IsValidIndex(Ti) || !Tractors[Ti].Bu) return;
	const FTractor& T = Tractors[Ti]; const FTransform Kn = T.KneelX(); const FRotator R(0.f, T.Yaw, 0.f);
	TArray<FVector> All;
	for (int32 i = 0; i < ASeats::Count(); ++i)
	{
		FVector At, Fc; float H; if (!ASeats::SeatOf(i, At, Fc, H) || FVector::Dist2D(At, T.At) > 750.f) continue;
		const FVector L = Kn.InverseTransformPosition(R.UnrotateVector(At - T.At));
		if (FMath::Abs(L.X) > 600. || FMath::Abs(L.Y) > 130. || L.Z < BusFloor || L.Z > 160.) continue;   // (not the driver's)
		All.Add(L); if (ASeats::IsTaken(i)) continue;
		FBusSlot S; S.Seat = i; S.At = L; S.Face = R.UnrotateVector(Fc).GetSafeNormal2D(); Out.Add(S);
	}
	Out.Sort([](const FBusSlot& A, const FBusSlot& B) { return FMath::Abs(A.At.Y) > FMath::Abs(B.At.Y); });   // (the window ones first)
	TArray<FVector> Stood;
	for (int32 k = 0; k < 19; ++k) for (float Sg : {1.f, -1.f})
	{
		const FVector At(-600. + 48. * BusStraps[k], 88. * Sg, BusFloor); bool bOk = At.X < 500.;
		for (int32 d = 0; d < 7 && bOk; ++d) bOk = FMath::Abs(BusDoors[d].X - At.X) > 70.;
		for (const FVector& V : All) if (FVector::Dist2D(V, At) < 72.) bOk = false;                         // (no seat, no knees of a sitting one)
		for (const FVector& V : Stood) if (FVector::Dist2D(V, At) < 47.) bOk = false;
		for (const FVector2D& Po : BusPoles) if (FVector2D::Distance(Po, FVector2D(At)) < 26.) bOk = false;
		if (!bOk) continue;
		FBusSlot S; S.At = At; S.Face = FVector(0., -Sg, 0.); S.Hold = FVector(At.X, 62. * Sg, 221.); S.bStrap = true; double Best = 60.;
		for (const FVector2D& Po : BusPoles) if (FMath::Abs(Po.Y - 62. * Sg) < 1.) { const double D = FVector2D::Distance(Po, FVector2D(At)); if (D < Best) { Best = D; S.Hold = FVector(Po.X, Po.Y, BusFloor + 138.); S.bStrap = false; } }
		Stood.Add(At); Out.Add(S);
	}
}

bool AGSE::Moving(const FVector& At) const
{
	for (const FTractor& T : Tractors) if (FMath::Abs(T.Speed) > 10.f && FVector::Dist2D(T.At, At) < 900.f)
	{
		const FVector L = FRotator(0.f, T.Yaw, 0.f).UnrotateVector(At - T.At); const double Hx = T.Bu ? 660. : 400.;
		if (FMath::Abs(L.Y) < 230. && L.X * (T.Speed > 0.f ? 1. : -1.) > -Hx && L.X * (T.Speed > 0.f ? 1. : -1.) < Hx + 250.) return true;
	}
	return false;
}
