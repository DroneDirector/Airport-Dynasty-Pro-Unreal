// The arriving bags (owner 2026-11-21: the passenger chain fluent both ways): when an aircraft is in (the turnaround's bulk
// hold task - out first, then in), the belt loader's hold loader lifts each arriving bag onto the top of the belt; pinned on
// it, it rides down; the arrivals train's driver lifts it off the bottom end onto a cart's load slot (hand IK on its grips,
// pinned on the slot). Then the train is towed in through Ramp services 13 to the arrivals conveyor's loading point south of
// Ramp services 4 (plan x 183, z 63 - BaggageBelts.h) and the bags go onto the conveyor there, riding to the claim belts.
// Only then does the departure train come in beside the aircraft with the outgoing bags.
// (The bag models are the departure belts' - DepartureBelts.h; each carries its tag, "ARR <flight> <n>" as its passenger;
//  the arrivals conveyor has no bags with ids yet - the bag is handed over with ABaggageBelts::Unload(belt, 1, the Ramp
//  services 4 loading point).)
#include "GSE.h"
#include "Sim/ADPBoards.h"
#include "Turnaround.h"
#include "Tugs.h"
#include "CrewPose.h"
#include "DepartureBelts.h"
#include "BaggageBelts.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	const FName GsaHand(TEXT("hand_r"));
	const FVector GsaStandLoad(-425.f, -25.f, 0.f);
	constexpr float GsaStationX = 183.05f, GsaTrunkZ = 63.f;   // (BaggageBelts.cpp: STATIONS[3], TRUNKZ)
}

bool AGSE::ArrivalFor(float StandX, const FString& Key)
{
	for (const FTractor& T : Tractors) if (T.bArrival && T.Job != EJob::None && FMath::Abs(T.StandX - StandX) < 1.f) return true;
	int32 Ti = INDEX_NONE; for (int32 t = 0; t < Tractors.Num() && Ti == INDEX_NONE; ++t) if (TractorFree(Tractors[t])) Ti = t;
	if (Ti == INDEX_NONE) { ADP_LOG(TEXT("GSE: no tractor free for %s's arriving bags"), *Key); return false; }
	for (int32 a = 0; a < Carts.Num(); ++a)
	{
		if (!CartFree(a)) continue;
		bool bFronted = false; for (int32 o = 0; o < Carts.Num(); ++o) if (o != a && FVector::Dist2D(CW(Carts[a], FVector(201.f, 0.f, 40.f)), CW(Carts[o], FVector(-127.f, 0.f, 40.f))) < 30.f) bFronted = true;
		if (bFronted) continue;
		for (int32 b = 0; b < Carts.Num(); ++b)
			if (a != b && CartFree(b) && FVector::Dist2D(CW(Carts[b], FVector(201.f, 0.f, 40.f)), CW(Carts[a], FVector(-127.f, 0.f, 40.f))) < 30.f)
			{ StartArrival(Ti, {a, b}, StandX, Key); return Tractors[Ti].Job != EJob::None; }
	}
	return false;
}

void AGSE::StartArrival(int32 Ti, const TArray<int32>& Pair, float StandX, const FString& Key)
{
	FTractor& T = Tractors[Ti];
	T.bArrival = true; T.Key = Key; T.StandX = StandX; T.ArrKey = Key;
	ADP_LOG(TEXT("GSE: %s - an empty train for %s's arriving bags"), *T.Name, Key.IsEmpty() ? TEXT("the arriving aircraft") : *Key);
	StartTrain(Ti, Pair);
	if (T.Job == EJob::None) { T.bArrival = false; T.Key.Reset(); T.StandX = 0.f; }
}

void AGSE::ArrivalLoad(int32 Ti)
{
	FTractor& T = Tractors[Ti]; T.Job = EJob::AtStand;
	const int32 Ci = T.Driver; const float StandX = T.StandX;
	ADP_LOG(TEXT("GSE: %s's empty train beside the aircraft at the stand x %.0f - the arriving bags first"), *T.Name, StandX);
	Motor(T, false);
	Wait(Ci, FMath::FRandRange(1.5f, 4.f));
	ClimbOut(Ci, Ti, [this, Ti] { Tractors[Ti].Job = EJob::Loading; });
	Curtains(Ci, Ti, true, nullptr);
	WaitFor(Ci, [this, StandX](float E) { const int32 Li = LoaderAt(StandX); return Li == INDEX_NONE || Tractors[Li].bBelt || E > 150.f; });
	// (the bulk hold of an A320: 15-30 bags; the test a handful)
	const int32 N = FMath::Min(bTest ? 12 : FMath::Max(FMath::RandRange(16, 28), ABaggageBelts::TagsOf(GetWorld(), T.ArrKey) + FMath::RandRange(2, 6)), 24 * T.Train.Num());
	Do(Ci, [this, Ti, StandX, N]
	{
		const int32 Li = LoaderAt(StandX);
		if (Li != INDEX_NONE) { Tractors[Li].ArrivalsLeft = N; Tractors[Li].ArrKey = Tractors[Ti].Key; Tractors[Li].ArrivalIn = 0.f; }
		ADP_LOG(TEXT("GSE: unloading %d arriving bags from the bulk hold onto %s's train"), Li != INDEX_NONE ? N : 0, *Tractors[Ti].Name);
	});
	const int32 Hp = Borrow(T.At, nullptr, Ti);
	if (Hp != INDEX_NONE) { Helper.Add(Ti, Hp); HelperDone.Remove(Ti); }
	ArrBy(Ci, Ti, 0);
	if (Hp != INDEX_NONE) ArrBy(Hp, Ti, 1);
}

void AGSE::ArrBy(int32 Pc, int32 Ti, int32 h)
{
	// the next bag at the belt's foot that nobody has, into the first free place on the carts, until the hold and the belt are
	// empty (you may have taken some of them)
	const float StandX = Tractors[Ti].StandX;
	const FVector Stand = h == 0 ? GsaStandLoad : FVector(-425.f, 55.f, 0.f);
	TSharedPtr<int32> Cur = MakeShared<int32>(INDEX_NONE); TSharedPtr<FIntPoint> Slot = MakeShared<FIntPoint>(INDEX_NONE, INDEX_NONE);
	if (LoaderAt(StandX) != INDEX_NONE) WalkTo(Pc, [this, StandX, Stand] { const int32 Li = LoaderAt(StandX); return Li != INDEX_NONE ? TW(Tractors[Li], Stand) : Plan(0.f, 0.f); }, 20.f);
	WaitFor(Pc, [this, StandX, Ti, Cur, Slot](float E)
	{
		const int32 Li = LoaderAt(StandX); if (Li == INDEX_NONE) return true;
		FTractor& L = Tractors[Li];
		for (FTractor::FBeltBag& B : L.BeltBags) if (B.Stage == 4) if (LoaderCartFree(L, Slot->X, Slot->Y)) { B.Stage = 6; *Cur = B.Id; SlotTaken.Add(Slot->X * 100 + Slot->Y); return true; }
		// (the carts full: home with what they hold)
		if (E > 45.f) if (L.BeltBags.ContainsByPredicate([](const FTractor::FBeltBag& B) { return B.Stage == 4; }) && !LoaderCartFree(L, Slot->X, Slot->Y)) return true;
		return L.ArrivalsLeft <= 0 && L.BeltBags.Num() == 0 && (L.LYouBag == INDEX_NONE || E > 300.f);
	});
	Do(Pc, [this, Pc, Ti, h, StandX, Stand, Cur, Slot] { Later([this, Pc, Ti, h, StandX, Stand, Cur, Slot]
	{
		if (*Cur == INDEX_NONE)
		{
			if (h == 1) { Do(Pc, [this, Ti] { HelperDone.Add(Ti); }); GiveBack(Pc, StandX); return; }
			WaitFor(Pc, [this, Ti](float E) { return !Helper.Contains(Ti) || HelperDone.Contains(Ti) || E > 120.f; });
			Do(Pc, [this, Ti] { Helper.Remove(Ti); for (const int32 c : Tractors[Ti].Train) for (int32 s = 0; s < Carts[c].Bags.Num(); ++s) SlotTaken.Remove(c * 100 + s); });
			Curtains(Pc, Ti, false, nullptr);
			ClimbIn(Pc, Ti, [this, Ti] { Motor(Tractors[Ti], true); ArrivalToStation(Ti); });
			return;
		}
		// off the belt's bottom end by its grips
		PoseAct(Pc, [this, StandX, Cur, Stand](float K, float PH, FDriverPose& P)
		{
			const int32 Li = LoaderAt(StandX); const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (Li == INDEX_NONE || !D) return;
			const FTractor& L = Tractors[Li]; const int32 Id = *Cur;
			if (Id == INDEX_NONE) { P = ADPPose::Standing(TW(L, Stand), FRotator(0.f, L.Yaw, 0.f).Vector(), PH); return; }
			const FDepBagGrips G = D->Grips(Id); const FVector St = TW(L, Stand), Face = FRotator(0.f, L.Yaw, 0.f).Vector();
			const float E = FMath::Min(1.f, K * 1.5f);
			P = G.bTwoHands ? ADPPose::Both(St, Face, G.Left, G.Right, 1.f, PH, E, 16.f) : ADPPose::Reach(St, Face, 1, G.Handle, 1.f, PH, E);
		}, 0.9f);
		Do(Pc, [this, Pc, StandX, Cur, Slot, Ti]
		{
			const int32 Li = LoaderAt(StandX); ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (Li == INDEX_NONE || !D) return;
			FTractor& L = Tractors[Li];
			const int32 Ix = L.BeltBags.IndexOfByPredicate([&](const FTractor::FBeltBag& B) { return B.Id == *Cur; });
			if (Ix == INDEX_NONE) { *Cur = INDEX_NONE; SlotTaken.Remove(Slot->X * 100 + Slot->Y); return; }
			L.BeltBags.RemoveAt(Ix);
			D->TakeBagById(*Cur, Body(Pc), GsaHand);
		});
		// to the cart's open side, into its slot
		
		auto SideY = [this, Slot, StandX]
		{
			const int32 c = Slot->X;
			float S2 = 0.f, N2 = 0.f, Sp = 36.f; StandGeo(StandX, S2, N2, Sp);
			return FVector::DotProduct(Plan(S2, N2 + 8.f) - Carts[c].At, FRotator(0.f, Carts[c].Yaw + 90.f, 0.f).Vector()) > 0.f ? 1.f : -1.f;
		};
		WalkTo(Pc, [this, Slot, SideY] { return CW(Carts[Slot->X], FVector(CartSlot(Slot->X, Slot->Y).GetLocation().X, SideY() * 118.f, 0.f)); }, 15.f);
		PoseAct(Pc, [this, Slot, Cur, SideY](float K, float PH, FDriverPose& P)
		{
			const int32 c = Slot->X, s = Slot->Y; const float Sx = CartSlot(c, s).GetLocation().X;
			const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D || *Cur == INDEX_NONE) return;
			const FCart& C = Carts[c]; const float Ys = SideY();
			const FTransform SlotW = CartSlot(c, s) * C.M->GetActorTransform(), Bw = D->BagTransform(*Cur); const FDepBagGrips G = D->Grips(*Cur);
			const FVector St = CW(C, FVector(Sx, Ys * 118.f, 0.f)), Face = FRotator(0.f, C.Yaw + 90.f, 0.f).Vector() * -Ys;
			FDriverPose Q = ADPPose::Reach(St, Face, 1, SlotW.TransformPosition(Bw.InverseTransformPosition(G.Handle)), 1.f, PH, FMath::SmoothStep(0.f, 1.f, K));
			if (G.bTwoHands) { Q.bHand[0] = true; Q.Hand[0] = FMath::Lerp(G.Left, SlotW.TransformPosition(Bw.InverseTransformPosition(G.Left)), K); Q.Grip[0] = 1.f; }
			P = Q;
		}, 1.1f);
		Do(Pc, [this, Slot, Cur]
		{
			const int32 c = Slot->X, s = Slot->Y; SlotTaken.Remove(c * 100 + s);
			if (*Cur == INDEX_NONE) return;
			if (ADepartureBelts* D = ADepartureBelts::Get(GetWorld())) if (Carts[c].M.IsValid()) D->PutOnSlot(*Cur, Carts[c].M->GetRootComponent(), NAME_None, CartSlot(c, s));
			Carts[c].Bags[s] = *Cur;
		});
		Do(Pc, [this, Pc, Ti, h] { Later([this, Pc, Ti, h] { ArrBy(Pc, Ti, h); }); });
	}); });
}

void AGSE::ArrivalToStation(int32 Ti)
{
	if (CaterMoving(Tractors[Ti].StandX)) { RetryIn(Ti, 6.f, [this, Ti] { ArrivalToStation(Ti); }); return; }
	FTractor& T = Tractors[Ti]; T.Job = EJob::Back;
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(T.StandX, S, N, Sp);
	const float Xc = S - (Sp * 0.5f + 2.4f);
	// off the stand as a departure train leaves it, then in through Ramp services 13 and west along the hall to the
	// arrivals conveyor's loading point: parked beside it (z 60, facing west), the carts' side towards it
	TArray<FVector> W;
	float Yw = 0.f; const FVector A = PlanPose(Xc, N + 30.f, 0.f, 1.f, Yw);
	Dubins(W, T.At, T.Yaw, A, Yw, 400.f);
	Leg(W, Xc, N + 30.f, 0.f, 1.f, Xc, N + 46.f, 0.f, 1.f, 400.f);
	ADP_LOG(TEXT("GSE: %s's train with the arriving bags leaves the stand x %.0f"), *T.Name, S);
	Go(T, MoveTemp(W), false, [this, Ti]
	{
		FTractor& U = Tractors[Ti];
		TArray<FVector> R;
		if (!Apron(R, U.At, Plan(275.f, 114.f), &U)) R.Add(Plan(275.f, 114.f));
		Leg(R, 275.f, 114.f, 0.f, -1.f, 275.f, 108.f, 0.f, -1.f, 400.f);
		Leg(R, 275.f, 108.f, 0.f, -1.f, 269.f, 93.f, 0.f, -1.f, 400.f);
		Leg(R, 269.f, 93.f, 0.f, -1.f, 269.f, 61.f, 0.f, -1.f, 400.f);
		Leg(R, 269.f, 61.f, 0.f, -1.f, 265.f, 57.f, -1.f, 0.f, 400.f);
		Leg(R, 265.f, 57.f, -1.f, 0.f, 192.f, 57.f, -1.f, 0.f, 400.f);
		Leg(R, 192.f, 57.f, -1.f, 0.f, 178.f, 60.f, -1.f, 0.f, 400.f);
		Go(U, MoveTemp(R), false, [this, Ti] { ArrivalUnload(Ti); });
	});
}

void AGSE::ArrivalUnload(int32 Ti)
{
	FTractor& T = Tractors[Ti]; T.Job = EJob::Loading;
	const int32 Ci = T.Driver;
	ArrAway.Add(T.ArrKey);
	ADP_LOG(TEXT("GSE: %s at the arrivals conveyor (Ramp services 4) - the bags onto it"), *T.Name);
	Motor(T, false);
	ClimbOut(Ci, Ti, nullptr);
	const FVector Trunk = Plan(GsaStationX, GsaTrunkZ);
	Curtains(Ci, Ti, true, nullptr, &Trunk);
	// (the claim belt the arriving passengers wait at: the boards' flight number for the aircraft at this stand - PaxFlow
	//  names the belt with it; none yet: name one now, with no random bags - this train brings them)
	FString Label = T.Key;
	if (const ADPTurnaround::FTurn* Tn = ADPTurnaround::AtStand(T.StandX)) for (const FADPBoardRow& Row : ADPBoards::Rows(this, true, 40)) if (Row.Reg == Tn->Reg) { Label = Row.Flight; break; }
	int32 Belt = ABaggageBelts::BeltOf(GetWorld(), Label);
	if (Belt == INDEX_NONE) { ABaggageBelts::OnArrived(GetWorld(), T.StandX, Label, 0); Belt = ABaggageBelts::BeltOf(GetWorld(), Label); }
	if (Belt == INDEX_NONE) Belt = FMath::Abs(int32(GetTypeHash(T.Key))) % 5;
	int32 Count = 0;
	for (const int32 c : T.Train)
	{
		const float Ys = FVector::DotProduct(Trunk - Carts[c].At, FRotator(0.f, Carts[c].Yaw + 90.f, 0.f).Vector()) > 0.f ? 1.f : -1.f;
		for (int32 s = Carts[c].Bags.Num() - 1; s >= 0; --s)
		{
			const int32 Id = Carts[c].Bags[s]; if (Id == INDEX_NONE) continue;
			const float Sx = CartSlot(c, s).GetLocation().X; const float At = GsaStationX + 0.6f * (Count % 3 - 1); ++Count;
			WalkTo(Ci, [this, c, Sx, Ys] { return CW(Carts[c], FVector(Sx, Ys * 118.f, 0.f)); }, 15.f);
			PoseAct(Ci, [this, c, Id, Sx, Ys](float K, float PH, FDriverPose& P)
			{
				const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D) return;
				const FDepBagGrips G = D->Grips(Id); const FCart& C = Carts[c];
				const FVector St = CW(C, FVector(Sx, Ys * 118.f, 0.f)), Face = FRotator(0.f, C.Yaw + 90.f, 0.f).Vector() * -Ys;
				const float E = FMath::Min(1.f, K * 1.5f);
				P = G.bTwoHands ? ADPPose::Both(St, Face, G.Left, G.Right, 1.f, PH, E, 14.f) : ADPPose::Reach(St, Face, 1, G.Handle, 1.f, PH, E);
			}, 0.9f);
			Do(Ci, [this, Ci, c, s, Id] { if (ADepartureBelts* D = ADepartureBelts::Get(GetWorld())) D->TakeBagById(Id, Body(Ci), GsaHand); Carts[c].Bags[s] = INDEX_NONE; });
			// onto the conveyor at its loading point (lying across it, as its own bags ride)
			WalkTo(Ci, [this, At] { return Plan(At, GsaTrunkZ - 1.15f); }, 15.f);
			auto Lay = [this, At] { return FTransform(FRotator(0.f, -90.f, 0.f), Plan(At, GsaTrunkZ) + FVector(0, 0, 75.f)); };
			PoseAct(Ci, [this, Id, At, Lay](float K, float PH, FDriverPose& P)
			{
				const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D) return;
				const FTransform Bw = D->BagTransform(Id), Tw = Lay(); const FDepBagGrips G = D->Grips(Id);
				const FVector St = Plan(At, GsaTrunkZ - 1.15f), Face = (Tw.GetLocation() - St).GetSafeNormal2D();
				FDriverPose Q = ADPPose::Reach(St, Face, 1, Tw.TransformPosition(Bw.InverseTransformPosition(G.Handle)), 1.f, PH, FMath::SmoothStep(0.f, 1.f, K));
				if (G.bTwoHands) { Q.bHand[0] = true; Q.Hand[0] = FMath::Lerp(G.Left, Tw.TransformPosition(Bw.InverseTransformPosition(G.Left)), K); Q.Grip[0] = 1.f; }
				P = Q;
			}, 1.0f);
			Do(Ci, [this, Id, Belt, Lay]
			{
				// handed to the conveyor: its own bag takes over as it starts to ride (an id-carrying hand-over comes with
				// the claim side's bags with ids)
				if (ADepartureBelts* D = ADepartureBelts::Get(GetWorld())) D->PutOnSlot(Id, GetRootComponent(), NAME_None, Lay());
				if (ABaggageBelts* B = ABaggageBelts::Get(GetWorld())) B->Unload(Belt, 1, 3);
				PendingRemove.Add(TPair<double, int32>(GetWorld()->GetTimeSeconds() + 2.6, Id));
			});
		}
	}
	Do(Ci, [this, Ti, Count] { ADP_LOG(TEXT("GSE: %d arriving bags onto the arrivals conveyor from %s's train - to the claim"), Count, *Tractors[Ti].Name); });
	Curtains(Ci, Ti, false, nullptr, &Trunk);
	// back to the lane: the carts uncoupled there, the tractor in its box
	ClimbIn(Ci, Ti, [this, Ti]
	{
		FTractor& U = Tractors[Ti]; Motor(U, true);
		TArray<FVector> W;
		Leg(W, 178.f, 60.f, -1.f, 0.f, 137.f, 57.f, -1.f, 0.f, 400.f);
		Leg(W, 137.f, 57.f, -1.f, 0.f, 133.f, 53.f, 0.f, -1.f, 400.f);
		Leg(W, 133.f, 53.f, 0.f, -1.f, 133.f, 50.f, 0.f, -1.f, 300.f);
		Go(U, MoveTemp(W), false, [this, Ti] { TrainDrop(Ti); });
	});
}
