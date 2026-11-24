// The taxi bus in your hands (to-do 239, part B; to-do 238's controls). The vehicle itself is a line of ADPService::All()
// (GSEService.cpp: model TaxiBus, points Data/taxibus-riders.json): the shared service vehicles' driving, wheels, pedals,
// wipers, lamps and seats - E at the driver's door drives it, at the front passenger door rides beside the driver, at the
// sliding door rides in the back (the sitting IK on the coach-built seats). The cab's switches (lights, hazard, beacon,
// horn) are every vehicle's (CabAdd). Here: its own doors by their handles - the sliding door to the passengers' seats and
// the two rear doors to the luggage space - with the cleaning van's limits and speed (the same body and pivots).
// No chocks, no cones, no turn-round task: it waits at the landside kerb.
// The taximeter (to-do 239b piece 3): its middle button, pressed from the driver's seat, starts and ends a fare
// (AGSE::TaxiFare, GSEService.cpp); what it shows - the fare so far - is its line under your finger.
#include "GSE.h"
#include "ServiceVehicles.h"
#include "VehicleControls.h"
#include "ADPLog.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FServiceSpec& TxSpec(const AGSE::FTractor& T) { return ADPService::All()[T.Svc]; }

	// a door's leaf where it is now (the same maths as SvcParts)
	FTransform TxDoorX(const AGSE::FTractor& T, int32 d)
	{
		const AGSE::FTractor::FSvcDoor& D = T.SvcDoors[d]; const FServiceDoor& K = TxSpec(T).Doors[d]; FTransform X(D.Piv);
		if (K.bSlide) { const float Sg = D.Piv.Y >= 0.f ? 1.f : -1.f, A = FMath::Clamp(D.Now / 0.15f, 0.f, 1.f), B = FMath::Clamp((D.Now - 0.15f) / 0.85f, 0.f, 1.f); X.SetLocation(D.Piv + FVector(-4.f * A - 126.f * B, 12.5f * A * Sg, 0.f)); }
		else X.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(K.Deg * FMath::SmoothStep(0.f, 1.f, D.Now))));
		return FTransform(-D.Piv) * X;
	}

	// you (on foot) in a door's opening: it never shuts on you
	bool TxYouInDoor(const AGSE& G, const AGSE::FTractor& T, int32 d)
	{
		const APlayerController* Pc = G.GetWorld() ? G.GetWorld()->GetFirstPlayerController() : nullptr; const APawn* Me = Pc ? Pc->GetPawn() : nullptr;
		if (!Me || !T.SvcDoors.IsValidIndex(d)) return false;
		const FVector L = FRotator(0.f, T.Yaw, 0.f).UnrotateVector(Me->GetActorLocation() - T.At);
		const FVector P = T.SvcDoors[d].Piv;
		if (d == 0) return L.X > P.X - 135.f && L.X < P.X + 10.f && L.Y > 40.f && L.Y < P.Y + 70.f;   // (the sliding door's opening, right side)
		return L.X < P.X + 40.f && L.X > P.X - 110.f && FMath::Abs(L.Y) < 100.f;                       // (behind the rear doors)
	}
}

TSharedPtr<FVehTable> AGSE::TaxiControls()
{
	TSharedPtr<FVehTable> Tb = MakeShared<FVehTable>(); using K = FVehControl;
	auto Door = [&](int32 d, const TCHAR* Pt, const TCHAR* Lb) { K X; X.Part = TEXT(""); X.Point = Pt; X.Kind = K::Push; X.Label = Lb; X.Frame = 1 + d; X.Motion = d; X.bGrip = true; Tb->Controls.Add(X); };
	Door(0, TEXT("IK_Handle_SlideDoor_Out"), TEXT("Sliding door")); Door(1, TEXT("IK_Handle_RearDoorR_Out"), TEXT("Right rear door")); Door(2, TEXT("IK_Handle_RearDoorL_Edge"), TEXT("Left rear door"));
	{ K X; X.Part = TEXT(""); X.Point = TEXT("IK_Taximeter_Button"); X.Kind = K::Push; X.Label = TEXT("Taximeter"); X.bSeat = true; Tb->Controls.Add(X); }   // (Motion none: the meter)
	Tb->Frame = [](const AGSE&, const FTractor& T, int32 Fr) { return Fr < 1 || !T.SvcDoors.IsValidIndex(Fr - 1) || Fr - 1 >= TxSpec(T).Doors.Num() ? FTransform::Identity : TxDoorX(T, Fr - 1); };
	Tb->Now = [](const FTractor& T, int32 M) { return T.SvcDoors.IsValidIndex(M) ? T.SvcDoors[M].Now : 0.f; };
	Tb->Want = [](FTractor& T, int32 M, float To) { if (T.SvcDoors.IsValidIndex(M)) { T.LoadWant = INDEX_NONE; T.SvcDoors[M].Want = To; } };
	Tb->May = [](const AGSE& G, const FTractor& T, int32 M, float To, FString& Why)
	{
		// (the van's order for the rear leaves: the right one first, the left one back first; standing still; never shut on you)
		if (T.Speed > 1.f || T.Path.Num()) Why = TEXT("the taxi bus is not standing");
		else if (!T.SvcDoors.IsValidIndex(M)) Why = TEXT("no such door");
		else if (M == 2 && To > 0.5f && T.SvcDoors[1].Now < 0.99f) Why = TEXT("the right one opens first");
		else if (M == 1 && To < 0.5f && T.SvcDoors[2].Now > 0.f) Why = TEXT("the left one shuts first");
		else if (To < 0.5f && TxYouInDoor(G, T, M)) Why = TEXT("you are in the doorway");
		return Why.IsEmpty();
	};
	Tb->Press = [](AGSE& G, int32 Ti, const FVehControl& C)
	{
		FTractor& T = G.Tractors[Ti]; FString Why;
		if (C.Motion == INDEX_NONE) { G.TaxiFare(!G.TaxiFareOn(), TEXT("the meter's button, you")); return; }
		if (!T.SvcDoors.IsValidIndex(C.Motion) || C.Frame < 1) return;
		const bool bOpen = T.SvcDoors[C.Motion].Now < 0.5f;
		if (CtlTable(T.CtlKey())->May(G, T, C.Motion, bOpen ? 1.f : 0.f, Why)) G.Sound(bOpen ? *T.SndDoorOpen : *T.SndDoorClose, G.TW(T, T.SvcDoors[C.Motion].Piv));
	};
	Tb->Say = [](const AGSE& G, const FTractor& T, const FVehControl& X) -> FString
	{
		if (X.Motion == INDEX_NONE) return G.TaxiMeterSay();
		return T.SvcDoors.IsValidIndex(X.Motion) ? FString(X.Label) + (T.SvcDoors[X.Motion].Now > 0.5f ? TEXT(": shut") : TEXT(": open")) : FString();
	};
	ADP_LOG(TEXT("Taxi bus: your controls - sliding door, rear doors, the taximeter; the cab's lights, horn and hazard as every vehicle's"));
	return Tb;
}
