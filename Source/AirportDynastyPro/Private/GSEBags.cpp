// The bags from the make-up carousel to the aircraft's hold (owner 2026-11-21: "every animation must be connected for the
// bags" - DepartureBelts.h holds every bag, pinned to a belt, a hand or a slot, never flying):
//  - the train's driver is at the flight's make-up carousel ahead of its bags (the train made up beside it, the curtains
//    on that side rolled up); each of the flight's bags that comes round within reach is lifted off by its grips (one hand
//    by the carry handle, two by its sides for a big one), carried in the hand and set into a cart's load slot (18 a cart:
//    3 along, 2 across, 3 high in the load box IK_Load_Min..Max), cart by cart
//  - made up (the flight's bags all on the carts, or the carts full): the curtains down, off to the aircraft
//  - at the aircraft: each bag lifted off its slot, carried to the belt loader's bottom end and laid on the belt; pinned on
//    it, it rides up at the belt's real speed (0.45 m/s) to the hold loader at the top, who takes it off into the hold
// Tests: -GseTest with -DepBagTest (the bags fed in at check-in).
#include "GSE.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSchedule.h"
#include "Tugs.h"
#include "People.h"
#include "CrewPose.h"
#include "DepartureBelts.h"
#include "Turnaround.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	const FName GsbHand(TEXT("hand_r"));
	const FVector GsbPivot(-320.f, 0.f, 76.f), GsbBeltBottom(-355.f, 0.f, 97.6f), GsbStandLoad(-425.f, -25.f, 0.f);
	constexpr float GsbBeltLen = 675.f, GsbBeltSpeed = 45.f;   // (cm: bottom to where the hold loader takes them; cm/s, the real belt)
}

USkeletalMeshComponent* AGSE::Body(int32 Ci) const
{
	const ATugs* G = Tg(); if (!G || !G->Crew.IsValidIndex(Ci) || !G->Crew[Ci].A.IsValid()) return nullptr;
	AADPPeople* P = AADPPeople::Get(GetWorld()); AADPPeople::FWho* W = P ? P->Find(G->Crew[Ci].A.Get()) : nullptr;
	return W ? W->Body : nullptr;
}

FTransform AGSE::CartSlot(int32 Cart, int32 Slot) const
{
	// (the load box, Blender (+-1.07, +-0.605, 0.54..1.78) m: three 71 cm places along, two 60 cm across, four 31 cm layers -
	//  24 bags, about what a real covered cart takes)
	const int32 Layer = Slot / 6, R = Slot % 6;
	return FTransform(FVector(-71.f + 71.f * (R % 3), (R / 3 == 0 ? 30.f : -30.f), 55.f + 31.f * Layer));
}

FString AGSE::BagFlight(int32 Ti) const
{
	const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D) return FString();
	const FTractor& T = Tractors[Ti];
	if (!T.Key.IsEmpty() && T.Key != TEXT("TEST") && D->BagsOf(T.Key).Num()) return T.Key;
	// (the passengers tag their bags with the boards' flight number - the aircraft at this train's stand)
	if (const ADPTurnaround::FTurn* Tn = T.StandX != 0.f ? ADPTurnaround::AtStand(T.StandX) : nullptr)
		for (const FADPBoardRow& Row : ADPBoards::Rows(this, false, 40)) if (Row.Reg == Tn->Reg && D->BagsOf(Row.Flight).Num()) return Row.Flight;
	// (no bags under its departure's name - the test: the flight with the most bags in the system)
	FString Best; int32 N = 0;
	for (const FString& F : BagFlights) { if (MadeUp.Contains(F) || F.StartsWith(TEXT("ARR "))) continue; const int32 K = D->BagsOf(F).Num(); if (K > N) { N = K; Best = F; } }
	return Best;
}

FVector AGSE::PickStand(int32 Ti) const
{
	const ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
	const FString* Fl = BagKey.Find(Ti);
	int32 K = D && Fl ? D->CarouselOf(*Fl) : INDEX_NONE; if (K == INDEX_NONE) K = 0;
	const FVector C = D ? D->CarouselCentre(K) : Plan(124.f, 33.5f);
	const FVector2f Cp(C.X / 100.f, -C.Y / 100.f);
	// at the carousel's edge towards the carts (its oval 16 x 5 m), level with the train's first cart
	const FTractor& T = Tractors[Ti];
	const float Cx = T.Train.Num() && Carts.IsValidIndex(T.Train[0]) ? Carts[T.Train[0]].At.X / 100.f : Cp.X;
	// (right at its edge: the bags ride 1.8 m out from its middle line, its edge is 2.5 m out - the old place, 3.4 m out, was too
	//  far to reach a single bag)
	return Plan(FMath::Clamp(Cx, Cp.X - 4.5f, Cp.X + 4.5f), Cp.Y + 2.78f);
}

int32 AGSE::Pickable(int32 Ti, int32 H) const
{
	const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); const FString* Fl = BagKey.Find(Ti);
	if (!D || !Fl || Fl->IsEmpty()) return INDEX_NONE;
	const FVector P = PickStand(Ti) + FVector(H * 280.f, 0.f, 0.f);
	int32 Best = INDEX_NONE; float Bd = 135.f;
	for (const int32 Id : D->BagsOnCarousel(*Fl)) { const float Dd = FVector::Dist2D(D->BagTransform(Id).GetLocation(), P); if (Dd < Bd) { Bd = Dd; Best = Id; } }
	return Best;
}

void AGSE::MakeUp(int32 Ci, int32 Ti, float Started) { MakeUpBy(Ci, Ti, Started, 0); }

void AGSE::MakeUpBy(int32 Ci, int32 Ti, float Started, int32 H)
{
	// (a real ramp team: the driver and a second handler lift the flight's bags off the carousel side by side, each at his
	//  own place at its edge, each into the next free slot - no two on one place, no slot taken twice)
	ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
	FTractor& T = Tractors[Ti]; T.Job = EJob::Loading;
	FString& Fl = BagKey.FindOrAdd(Ti);
	if (Fl.IsEmpty()) Fl = BagFlight(Ti);
	if (H == 0 && !MadeUp.Contains(Fl) && !Fl.IsEmpty())
	{
		MadeUp.Add(Fl);
		if (D) D->NoAutoClear.Add(Fl);   // (the crew load this flight's carts: the departure belts leave its bags to them)
		ADP_LOG(TEXT("GSE: %s's driver at make-up carousel %d for %s's bags"), *T.Name, D ? D->CarouselOf(Fl) + 1 : 0, *Fl);
		const FVector Toward = D ? D->CarouselCentre(FMath::Max(0, D->CarouselOf(Fl))) : Plan(124.f, 33.5f);
		Curtains(Ci, Ti, true, nullptr, &Toward);
		const int32 Hp = Borrow(T.At);
		if (Hp != INDEX_NONE)
		{
			Helper.Add(Ti, Hp); HelperDone.Remove(Ti);
			ADP_LOG(TEXT("GSE: %s helps at the carousel"), Tg() ? *Tg()->Crew[Hp].Name : TEXT("-"));
			Later([this, Hp, Ti, Started] { MakeUpBy(Hp, Ti, Started, 1); });
		}
	}
	const float Limit = bTest ? 420.f : 1800.f;
	auto Done = [this, Ti, Started, Limit](float E)
	{
		int32 Loaded = 0, Free = 0;
		for (const int32 c : Tractors[Ti].Train) for (int32 s = 0; s < Carts[c].Bags.Num(); ++s) { if (Carts[c].Bags[s] != INDEX_NONE) ++Loaded; else if (!SlotTaken.Contains(c * 100 + s)) ++Free; }
		const ADepartureBelts* D2 = ADepartureBelts::Get(GetWorld()); const FString F = BagKey.FindRef(Ti);
		if (!bTest && ADPTurnaround::ForFlight(F)) return Free == 0 || ADPTurnaround::BagCutOff(F, Tractors[Ti].StandX);   // (the game: until the cut-off, or the carts full - more trips after)
		const int32 Left = D2 ? D2->BagsOf(F).Num() - Loaded : 0;
		return Free == 0 || (bTest && Loaded >= 24) || (Loaded > 0 && Left <= 0 && E > 20.f) || GetWorld()->GetTimeSeconds() - Started > Limit || (!D2 && E > 5.f);
	};
	WalkTo(Ci, [this, Ti, H] { return PickStand(Ti) + FRotator(0.f, 0.f, 0.f).Vector() * (H * 280.f); }, 25.f);
	WaitFor(Ci, [this, Ti, H, Done](float E) { return Pickable(Ti, H) != INDEX_NONE || Done(E); });
	Do(Ci, [this, Ci, Ti, Started, H, Done] { Later([this, Ci, Ti, Started, H, Done]
	{
		ADepartureBelts* D2 = ADepartureBelts::Get(GetWorld());
		FTractor& U = Tractors[Ti];
		int32 Cart = INDEX_NONE, Slot = INDEX_NONE;
		for (const int32 c : U.Train) for (int32 s = 0; s < Carts[c].Bags.Num() && Cart == INDEX_NONE; ++s) if (Carts[c].Bags[s] == INDEX_NONE && !SlotTaken.Contains(c * 100 + s)) { Cart = c; Slot = s; }
		const int32 Id = Pickable(Ti, H);
		if (!D2 || Id == INDEX_NONE || Cart == INDEX_NONE || Done(0.f))
		{
			if (H != 0) { HelperDone.Add(Ti); GiveBack(Ci); return; }
			// made up: (the helper finished too) the curtains down, back in, off to the aircraft
			int32 Loaded = 0; for (const int32 c : U.Train) for (const int32 b : Carts[c].Bags) Loaded += b != INDEX_NONE;
			ADP_LOG(TEXT("GSE: %s's train made up - %d bags of %s on %d carts"), *U.Name, Loaded, *BagKey.FindRef(Ti), U.Train.Num());
			WaitFor(Ci, [this, Ti](float E) { return !Helper.Contains(Ti) || HelperDone.Contains(Ti) || E > 60.f; });
			Do(Ci, [this, Ti] { Helper.Remove(Ti); });
			const FVector Toward = D2 ? D2->CarouselCentre(FMath::Max(0, D2->CarouselOf(BagKey.FindRef(Ti)))) : Plan(124.f, 33.5f);
			Curtains(Ci, Ti, false, nullptr, &Toward);
			ClimbIn(Ci, Ti, [this, Ti] { TrainToStand(Ti); });
			return;
		}
		SlotTaken.Add(Cart * 100 + Slot);
		D2->TakeBagById(Id, nullptr, NAME_None);   // (claimed off the carousel: it waits in place for his hands - re-held below)
		const FTransform Was = D2->BagTransform(Id);
		D2->PutOnSlot(Id, GetRootComponent(), NAME_None, Was);
		const FVector Stand = PickStand(Ti) + FVector(H * 280.f, 0.f, 0.f);
		PoseAct(Ci, [this, Id, Stand](float K, float PH, FDriverPose& P)
		{
			const ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld()); if (!D3) return;
			const FDepBagGrips G = D3->Grips(Id); const FVector Bg = D3->BagTransform(Id).GetLocation();
			const FVector Face = (Bg - Stand).GetSafeNormal2D().IsNearlyZero() ? FVector(0, 1, 0) : (Bg - Stand).GetSafeNormal2D();
			const float E = FMath::Min(1.f, K * 1.6f);
			P = G.bTwoHands ? ADPPose::Both(Stand, Face, G.Left, G.Right, 1.f, PH, E, 18.f) : ADPPose::Stoop(Stand, Face, 1, G.Handle, 1.f, PH, E);
		}, 0.8f);
		Do(Ci, [this, Ci, Id] { if (ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld())) D3->TakeBagById(Id, Body(Ci), GsbHand); });
		const float Sx = CartSlot(Cart, Slot).GetLocation().X;
		WalkTo(Ci, [this, Cart, Sx] { return CW(Carts[Cart], FVector(Sx, 118.f, 0.f)); }, 15.f);
		PoseAct(Ci, [this, Cart, Slot, Id, Sx](float K, float PH, FDriverPose& P)
		{
			const ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld()); if (!D3) return;
			const FCart& C = Carts[Cart];
			const FTransform SlotW = CartSlot(Cart, Slot) * C.M->GetActorTransform(), Bw = D3->BagTransform(Id); const FDepBagGrips G = D3->Grips(Id);
			const FVector S = CW(C, FVector(Sx, 118.f, 0.f)), Face = -FRotator(0.f, C.Yaw + 90.f, 0.f).Vector();
			FDriverPose Q = ADPPose::Reach(S, Face, 1, SlotW.TransformPosition(Bw.InverseTransformPosition(G.Handle)), 1.f, PH, FMath::SmoothStep(0.f, 1.f, K));
			if (G.bTwoHands) { Q.bHand[0] = true; Q.Hand[0] = FMath::Lerp(G.Left, SlotW.TransformPosition(Bw.InverseTransformPosition(G.Left)), K); Q.Grip[0] = 1.f; }
			P = Q;
		}, 1.1f);
		Do(Ci, [this, Cart, Slot, Id]
		{
			if (ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld())) if (Carts[Cart].M.IsValid()) D3->PutOnSlot(Id, Carts[Cart].M->GetRootComponent(), NAME_None, CartSlot(Cart, Slot));
			Carts[Cart].Bags[Slot] = Id; SlotTaken.Remove(Cart * 100 + Slot);
		});
		Do(Ci, [this, Ci, Ti, Started, H] { Later([this, Ci, Ti, Started, H] { MakeUpBy(Ci, Ti, Started, H); }); });
	}); });
}

FTransform AGSE::BeltOffset(const FTractor& L, float S) const
{
	// on the belt, S along it from its bottom end, lying along it (boom-local -> the loader's root, the boom's pitch)
	const FQuat Qb(FVector::RightVector, FMath::DegreesToRadians(L.Boom));
	return FTransform(Qb, GsbPivot + Qb.RotateVector(GsbBeltBottom + FVector(S, 0.f, 1.f) - GsbPivot));
}

bool AGSE::BeltBottomFree(const FTractor& L) const
{
	for (const FTractor::FBeltBag& B : L.BeltBags) if (B.S < 90.f) return false;
	return true;
}

void AGSE::BeltTick(FTractor& L, float Dt)
{
	ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D || !L.M.IsValid()) return;
	// arriving bags: the hold loader lifts the next one out of the hold and onto the belt's top (one every few seconds,
	// once the arrivals train stands below and the top is clear)
	if (L.ArrivalsLeft > 0 && L.bBelt && L.TopCrew != INDEX_NONE && Body(L.TopCrew))
	{
		bool bTopFree = true; for (const FTractor::FBeltBag& B : L.BeltBags) if (B.Stage == 5 || (B.Stage == 3 && B.S > GsbBeltLen - 90.f)) bTopFree = false;
		bool bTrain = false; for (const FTractor& T : Tractors) if (T.bArrival && FMath::Abs(T.StandX - L.StandX) < 1.f && T.Job == EJob::Loading) bTrain = true;
		if (bTopFree && bTrain && (L.ArrivalIn -= Dt) <= 0.f)
		{
			L.ArrivalIn = 3.5f;
			const ATugs* G = Tg();
			const FString Pax = FString::Printf(TEXT("ARR %s %d"), *L.ArrKey, L.ArrivalsLeft);
			const int32 Id = D->PassengerDrop(G && G->Crew.IsValidIndex(L.TopCrew) ? G->Crew[L.TopCrew].A.Get() : nullptr, 1, Pax, TEXT("ARR ") + L.ArrKey, Body(L.TopCrew), GsbHand, false);
			if (Id != INDEX_NONE) { FTractor::FBeltBag B; B.Id = Id; B.S = GsbBeltLen; B.Stage = 5; L.BeltBags.Add(B); --L.ArrivalsLeft; }
		}
	}
	for (int32 i = 0; i < L.BeltBags.Num(); ++i)
	{
		FTractor::FBeltBag& B = L.BeltBags[i];
		if (B.Stage == 5) { if ((B.T += Dt) > 1.4f) { B.Stage = 3; B.T = 0.f; D->PutOnSlot(B.Id, L.M->GetRootComponent(), NAME_None, BeltOffset(L, B.S)); } continue; }
		if (B.Stage == 3)
		{
			// riding down (a bag stops behind the one waiting at the bottom)
			float Lim = 0.f; for (const FTractor::FBeltBag& O : L.BeltBags) if (&O != &B && (O.Stage == 3 || O.Stage == 4) && O.S < B.S) Lim = FMath::Max(Lim, O.S + 80.f);
			const float Was = B.S; if (L.bBelt) B.S = FMath::Max(B.S - GsbBeltSpeed * Dt, Lim);
			if (B.S != Was) D->PutOnSlot(B.Id, L.M->GetRootComponent(), NAME_None, BeltOffset(L, B.S));
			if (B.S <= 0.5f) B.Stage = 4;
			continue;
		}
		if (B.Stage == 4) continue;   // (at the bottom end: the train's driver takes it - GSEArrivals.cpp)
		if (B.Stage == 0)
		{
			// riding up, pinned (a bag stops behind the one waiting at the top)
			const float Was = B.S, Lim = i == 0 ? GsbBeltLen : L.BeltBags[i - 1].S - 80.f;
			if (L.bBelt) B.S = FMath::Min(FMath::Min(B.S + GsbBeltSpeed * Dt, Lim), GsbBeltLen);
			if (B.S != Was || B.T == 0.f) { D->PutOnSlot(B.Id, L.M->GetRootComponent(), NAME_None, BeltOffset(L, B.S)); B.T = 0.001f; }
			if (B.S >= GsbBeltLen - 0.5f) { B.Stage = 1; B.T = 0.f; }
		}
		else if (B.Stage == 1)
		{
			// at the top: the hold loader takes it in both hands / by its handle
			if ((B.T += Dt) > 0.9f)
			{
				if (L.TopCrew != INDEX_NONE && Body(L.TopCrew)) { D->TakeBagById(B.Id, Body(L.TopCrew), GsbHand); B.Stage = 2; B.T = 0.f; }
				else if (B.T > 2.f) { D->RemoveBag(B.Id); L.BeltBags.RemoveAt(i--); }
			}
		}
		else if ((B.T += Dt) > 1.6f)
		{
			// stowed in the hold
			D->RemoveBag(B.Id); L.BeltBags.RemoveAt(i--);
		}
	}
}

void AGSE::Unload(int32 Ci, int32 Ti, int32 Li)
{
	// every bag on the train, cart by cart, top layer first, by two handlers side by side (the driver and a second one): off
	// the slot by its grips, to his own place at the belt's bottom end, laid on it - the belt never waiting long
	ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
	FTractor& T = Tractors[Ti];
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(T.StandX, S, N, Sp);
	const FVector Craft = Plan(S, N + 8.f);
	const int32 Hp = Li != INDEX_NONE ? Borrow(T.At) : INDEX_NONE;
	if (Hp != INDEX_NONE) { Helper.Add(Ti, Hp); HelperDone.Remove(Ti); ADP_LOG(TEXT("GSE: %s helps load the hold"), Tg() ? *Tg()->Crew[Hp].Name : TEXT("-")); }
	const int32 Who[2] = {Ci, Hp};
	int32 Count = 0;
	for (const int32 c : T.Train)
	{
		const float Ys = FVector::DotProduct(Craft - Carts[c].At, FRotator(0.f, Carts[c].Yaw + 90.f, 0.f).Vector()) > 0.f ? 1.f : -1.f;
		for (int32 s = Carts[c].Bags.Num() - 1; s >= 0; --s)
		{
			const int32 Id = Carts[c].Bags[s]; if (Id == INDEX_NONE || !D) continue;
			const int32 h = (Hp != INDEX_NONE && (Count % 2) == 1) ? 1 : 0; const int32 P = Who[h];
			++Count;
			const float Sx = CartSlot(c, s).GetLocation().X;
			const FVector BeltStand = h == 0 ? FVector(-425.f, -25.f, 0.f) : FVector(-425.f, 55.f, 0.f);
			WalkTo(P, [this, c, Sx, Ys] { return CW(Carts[c], FVector(Sx, Ys * 118.f, 0.f)); }, 15.f);
			PoseAct(P, [this, c, Id, Sx, Ys](float K, float PH, FDriverPose& Pz)
			{
				const ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld()); if (!D3) return;
				const FDepBagGrips G = D3->Grips(Id); const FCart& C = Carts[c];
				const FVector St = CW(C, FVector(Sx, Ys * 118.f, 0.f)), Face = FRotator(0.f, C.Yaw + 90.f, 0.f).Vector() * -Ys;
				const float E = FMath::Min(1.f, K * 1.5f);
				Pz = G.bTwoHands ? ADPPose::Both(St, Face, G.Left, G.Right, 1.f, PH, E, 14.f) : ADPPose::Reach(St, Face, 1, G.Handle, 1.f, PH, E);
			}, 0.9f);
			Do(P, [this, P, c, s, Id] { if (ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld())) D3->TakeBagById(Id, Body(P), GsbHand); Carts[c].Bags[s] = INDEX_NONE; });
			if (Li == INDEX_NONE) { Do(P, [this, Id] { if (ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld())) D3->RemoveBag(Id); }); continue; }
			WalkTo(P, [this, Li, BeltStand] { return TW(Tractors[Li], BeltStand); }, 20.f);
			WaitFor(P, [this, Li](float E) { FTractor& L = Tractors[Li]; if (!L.bBottomHeld && BeltBottomFree(L) && L.bBelt) { L.bBottomHeld = true; return true; } return E > 60.f; });
			PoseAct(P, [this, Li, Id, BeltStand](float K, float PH, FDriverPose& Pz)
			{
				const ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld()); if (!D3) return;
				const FTractor& L = Tractors[Li];
				const FTransform SlotW = BeltOffset(L, 0.f) * L.M->GetActorTransform(), Bw = D3->BagTransform(Id); const FDepBagGrips G = D3->Grips(Id);
				const FVector St = TW(L, BeltStand), Face = (SlotW.GetLocation() - St).GetSafeNormal2D();
				FDriverPose Q = ADPPose::Reach(St, Face, 1, SlotW.TransformPosition(Bw.InverseTransformPosition(G.Handle)), 1.f, PH, FMath::SmoothStep(0.f, 1.f, K));
				if (G.bTwoHands) { Q.bHand[0] = true; Q.Hand[0] = FMath::Lerp(G.Left, SlotW.TransformPosition(Bw.InverseTransformPosition(G.Left)), K); Q.Grip[0] = 1.f; }
				Pz = Q;
			}, 1.0f);
			Do(P, [this, Li, Id]
			{
				FTractor& L = Tractors[Li];
				L.bBottomHeld = false;
				FTractor::FBeltBag B; B.Id = Id; B.S = 0.f; L.BeltBags.Add(B);
				if (ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld())) D3->PutOnSlot(Id, L.M->GetRootComponent(), NAME_None, BeltOffset(L, 0.f));
			});
		}
	}
	ADP_LOG(TEXT("GSE: %d bags from %s's train to the bulk hold"), Count, *T.Name);
	if (Hp != INDEX_NONE) { Do(Hp, [this, Ti] { HelperDone.Add(Ti); }); GiveBack(Hp); }
	// the bags all up: the curtains down, back in, home (the belt loader leaves once the train has gone)
	WaitFor(Ci, [this, Ti](float E) { return !Helper.Contains(Ti) || HelperDone.Contains(Ti) || E > 120.f; });
	Do(Ci, [this, Ti] { Helper.Remove(Ti); });
	if (Li != INDEX_NONE) WaitFor(Ci, [this, Li](float E) { return Tractors[Li].BeltBags.Num() == 0 || E > 120.f; });
	Curtains(Ci, Ti, false, nullptr);
	ClimbIn(Ci, Ti, [this, Ti] { Motor(Tractors[Ti], true); TrainHome(Ti); });
}

void AGSE::PollBags(float Dt)
{
	// the flights whose bags are in the system (their tags read as new bags come); in the game a train for each flight
	// whose bags are coming, its driver at the carousel ahead of them
	if ((BagPoll += Dt) < 5.f) return;
	BagPoll = 0.f;
	ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D) return;
	int32 Id = BagScan;
	for (; Id < BagScan + 400; ++Id) { const FDepBagTag* T = D->Tag(Id); if (!T) break; if (!T->Flight.IsEmpty()) BagFlights.Add(T->Flight); }
	BagScan = Id;
	if (bTest)
	{
		// (the test train's flight: the one with most bags - kept for the crew as soon as it shows)
		for (int32 t = 0; t < Tractors.Num(); ++t) if (!Tractors[t].bLoader && !Tractors[t].bArrival && Tractors[t].Job != EJob::None && BagKey.FindRef(t).IsEmpty()) { const FString F = BagFlight(t); if (!F.IsEmpty()) { BagKey.Add(t, F); if (ADepartureBelts* Dm = ADepartureBelts::Get(GetWorld())) Dm->NoAutoClear.Add(F); } }
		return;
	}
	for (const FString& F : BagFlights)
	{
		if (F.StartsWith(TEXT("ARR ")) || D->BagsOf(F).Num() == 0) continue;   // (several trips: a new train when none is on it)
		bool bHas = false; for (const auto& K : BagKey) if (K.Value == F && Tractors.IsValidIndex(K.Key) && Tractors[K.Key].Job != EJob::None) bHas = true;
		{ int32 OnCarts = 0; for (const FCart& C : Carts) for (const int32 b : C.Bags) if (b != INDEX_NONE) { const FDepBagTag* T = D->Tag(b); if (T && T->Flight == F) ++OnCarts; } if (D->BagsOf(F).Num() <= OnCarts) bHas = true; }
		if (bHas) continue;
		// (too late for another trip: what comes now would miss the hold - left for the next flight, never holding this one up)
		if (const ADPTurnaround::FTurn* Tn = ADPTurnaround::ForFlight(F)) if (ADPTurnaround::LastNow() > Tn->OffBlocks - FTimespan::FromMinutes(28.0)) continue;
		if (UADPSim* Sim = UADPSim::Get(this)) if (Sim->IsRunning())
			if (const FADPPlanned* Pl = Sim->World.Planned.FindByPredicate([&](const FADPPlanned& X) { return X.FlightNo == F && X.State == TEXT("planned"); }))
				if (Pl->AirlineId == UADPSim::Me() && ADPSchedule::OnShift(Sim->World, TEXT("ground"), Sim->Now()) < 2)
				{ ADPSchedule::Tell(Sim->World, Sim->Now(), TEXT("bags:") + F, TEXT("No ground crew"), FString::Printf(TEXT("%s's bags wait in the make-up: too few of your ground crew on shift."), *F)); continue; }
		// a free tractor and a pair of free carts, the driver to the carousel now (its stand comes with its departure)
		StartFor(F, 0.f);
		break;
	}
}
