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
#include "Tugs.h"
#include "People.h"
#include "CrewPose.h"
#include "DepartureBelts.h"
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
	// (the load box, Blender (+-1.07, +-0.605, 0.54..1.78) m: three 71 cm places along, two 60 cm across, three 36 cm layers)
	const int32 Layer = Slot / 6, R = Slot % 6;
	return FTransform(FVector(-71.f + 71.f * (R % 3), (R / 3 == 0 ? 30.f : -30.f), 55.f + 36.f * Layer));
}

FString AGSE::BagFlight(int32 Ti) const
{
	const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D) return FString();
	const FTractor& T = Tractors[Ti];
	if (!T.Key.IsEmpty() && T.Key != TEXT("TEST") && D->BagsOf(T.Key).Num()) return T.Key;
	// (no bags under its departure's name - the test: the flight with the most bags in the system)
	FString Best; int32 N = 0;
	for (const FString& F : BagFlights) { if (MadeUp.Contains(F)) continue; const int32 K = D->BagsOf(F).Num(); if (K > N) { N = K; Best = F; } }
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
	return Plan(FMath::Clamp(Cx, Cp.X - 6.f, Cp.X + 6.f), Cp.Y + 3.4f);
}

int32 AGSE::Pickable(int32 Ti) const
{
	const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); const FString* Fl = BagKey.Find(Ti);
	if (!D || !Fl || Fl->IsEmpty()) return INDEX_NONE;
	const FVector P = PickStand(Ti);
	int32 Best = INDEX_NONE; float Bd = 115.f;
	for (const int32 Id : D->BagsOnCarousel(*Fl)) { const float Dd = FVector::Dist2D(D->BagTransform(Id).GetLocation(), P); if (Dd < Bd) { Bd = Dd; Best = Id; } }
	return Best;
}

void AGSE::MakeUp(int32 Ci, int32 Ti, float Started)
{
	ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
	FTractor& T = Tractors[Ti]; T.Job = EJob::Loading;
	FString& Fl = BagKey.FindOrAdd(Ti);
	if (Fl.IsEmpty()) Fl = BagFlight(Ti);
	const bool bFirst = !MadeUp.Contains(Fl) && !Fl.IsEmpty();
	if (bFirst)
	{
		MadeUp.Add(Fl);
		// (the crew load the carts now - the departure belts' unseen clearing off)
		if (D) D->bAutoClear = false;
		ADP_LOG(TEXT("GSE: %s's driver at make-up carousel %d for %s's bags"), *T.Name, D ? D->CarouselOf(Fl) + 1 : 0, *Fl);
		const FVector Toward = D ? D->CarouselCentre(FMath::Max(0, D->CarouselOf(Fl))) : Plan(124.f, 33.5f);
		Curtains(Ci, Ti, true, nullptr, &Toward);
	}
	// a free slot (cart by cart), what is on the carts
	int32 Cart = INDEX_NONE, Slot = INDEX_NONE, Loaded = 0;
	for (const int32 c : T.Train) for (int32 s = 0; s < Carts[c].Bags.Num(); ++s) { if (Carts[c].Bags[s] != INDEX_NONE) ++Loaded; else if (Cart == INDEX_NONE) { Cart = c; Slot = s; } }
	const float Limit = bTest ? 420.f : 1800.f;
	// waiting at the carousel's edge for the flight's next bag to come round - or made up
	WalkTo(Ci, [this, Ti] { return PickStand(Ti); }, 25.f);
	WaitFor(Ci, [this, Ti, Started, Loaded, Cart, Limit](float E)
	{
		if (Pickable(Ti) != INDEX_NONE && Cart != INDEX_NONE && !(bTest && Loaded >= 16)) return true;
		const ADepartureBelts* D2 = ADepartureBelts::Get(GetWorld()); const FString* F = BagKey.Find(Ti);
		const int32 Left = D2 && F ? D2->BagsOf(*F).Num() - Loaded : 0;
		return Cart == INDEX_NONE || (bTest && Loaded >= 16) || (Loaded > 0 && Left <= 0 && E > 20.f) || GetWorld()->GetTimeSeconds() - Started > Limit || (!D2 && E > 5.f);
	});
	Do(Ci, [this, Ci, Ti, Started, Cart, Slot] { Later([this, Ci, Ti, Started, Cart, Slot]
	{
		ADepartureBelts* D2 = ADepartureBelts::Get(GetWorld());
		const int32 Id = Pickable(Ti);
		FTractor& U = Tractors[Ti];
		int32 Loaded = 0; for (const int32 c : U.Train) for (const int32 b : Carts[c].Bags) Loaded += b != INDEX_NONE;
		if (!D2 || Id == INDEX_NONE || Cart == INDEX_NONE || (bTest && Loaded >= 16) || GetWorld()->GetTimeSeconds() - Started > (bTest ? 420.f : 1800.f))
		{
			// made up: the curtains down, back in, off to the aircraft
			ADP_LOG(TEXT("GSE: %s's train made up - %d bags of %s on %d carts"), *U.Name, Loaded, BagKey.Contains(Ti) ? *BagKey[Ti] : TEXT("-"), U.Train.Num());
			const FVector Toward = D2 ? D2->CarouselCentre(FMath::Max(0, D2->CarouselOf(BagKey.FindRef(Ti)))) : Plan(124.f, 33.5f);
			Curtains(Ci, Ti, false, nullptr, &Toward);
			ClimbIn(Ci, Ti, [this, Ti] { TrainToStand(Ti); });
			return;
		}
		// lifted off by its grips as it comes past
		PoseAct(Ci, [this, Ci, Ti, Id](float K, float PH, FDriverPose& P)
		{
			const ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld()); if (!D3) return;
			const FDepBagGrips G = D3->Grips(Id); const FVector S = PickStand(Ti), Bg = D3->BagTransform(Id).GetLocation();
			const FVector Face = (Bg - S).GetSafeNormal2D().IsNearlyZero() ? FVector(0, 1, 0) : (Bg - S).GetSafeNormal2D();
			const float E = FMath::Min(1.f, K * 1.6f);
			P = G.bTwoHands ? ADPPose::Both(S, Face, G.Left, G.Right, 1.f, PH, E, 18.f) : ADPPose::Stoop(S, Face, 1, G.Handle, 1.f, PH, E);
		}, 0.8f);
		Do(Ci, [this, Ci, Id] { if (ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld())) D3->TakeBagById(Id, Body(Ci), GsbHand); });
		// carried to the cart's open side, set into its slot (the hand brings its handle to where the slot puts it)
		const float Sx = CartSlot(Cart, Slot).GetLocation().X;
		WalkTo(Ci, [this, Cart, Sx] { return CW(Carts[Cart], FVector(Sx, 118.f, 0.f)); }, 15.f);
		PoseAct(Ci, [this, Cart, Slot, Id, Sx](float K, float PH, FDriverPose& P)
		{
			const ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld()); if (!D3) return;
			const FCart& C = Carts[Cart];
			const FTransform SlotW = CartSlot(Cart, Slot) * C.M->GetActorTransform();
			const FTransform Bw = D3->BagTransform(Id); const FDepBagGrips G = D3->Grips(Id);
			const FVector Lh = Bw.InverseTransformPosition(G.Handle);
			const FVector S = CW(C, FVector(Sx, 118.f, 0.f)), Face = -FRotator(0.f, C.Yaw + 90.f, 0.f).Vector();
			FDriverPose Q = ADPPose::Reach(S, Face, 1, SlotW.TransformPosition(Lh), 1.f, PH, FMath::SmoothStep(0.f, 1.f, K));
			if (G.bTwoHands) { Q.bHand[0] = true; Q.Hand[0] = FMath::Lerp(G.Left, SlotW.TransformPosition(Bw.InverseTransformPosition(G.Left)), K); Q.Grip[0] = 1.f; }
			P = Q;
		}, 1.1f);
		Do(Ci, [this, Cart, Slot, Id]
		{
			if (ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld())) if (Carts[Cart].M.IsValid()) D3->PutOnSlot(Id, Carts[Cart].M->GetRootComponent(), NAME_None, CartSlot(Cart, Slot));
			Carts[Cart].Bags[Slot] = Id;
		});
		const float Now = Started;
		Do(Ci, [this, Ci, Ti, Now] { Later([this, Ci, Ti, Now] { MakeUp(Ci, Ti, Now); }); });
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
	for (int32 i = 0; i < L.BeltBags.Num(); ++i)
	{
		FTractor::FBeltBag& B = L.BeltBags[i];
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
	// every bag on the train, cart by cart, top layer first: off its slot by its grips, to the belt's bottom end, laid on it
	ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
	FTractor& T = Tractors[Ti];
	float S = 0.f, N = 0.f, Sp = 36.f; StandGeo(T.StandX, S, N, Sp);
	const FVector Craft = Plan(S, N + 8.f);
	int32 Count = 0;
	for (const int32 c : T.Train)
	{
		const float Ys = FVector::DotProduct(Craft - Carts[c].At, FRotator(0.f, Carts[c].Yaw + 90.f, 0.f).Vector()) > 0.f ? 1.f : -1.f;
		for (int32 s = Carts[c].Bags.Num() - 1; s >= 0; --s)
		{
			const int32 Id = Carts[c].Bags[s]; if (Id == INDEX_NONE || !D) continue;
			++Count;
			const float Sx = CartSlot(c, s).GetLocation().X;
			WalkTo(Ci, [this, c, Sx, Ys] { return CW(Carts[c], FVector(Sx, Ys * 118.f, 0.f)); }, 15.f);
			PoseAct(Ci, [this, c, Id, Sx, Ys](float K, float PH, FDriverPose& P)
			{
				const ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld()); if (!D3) return;
				const FDepBagGrips G = D3->Grips(Id); const FCart& C = Carts[c];
				const FVector St = CW(C, FVector(Sx, Ys * 118.f, 0.f)), Face = FRotator(0.f, C.Yaw + 90.f, 0.f).Vector() * -Ys;
				const float E = FMath::Min(1.f, K * 1.5f);
				P = G.bTwoHands ? ADPPose::Both(St, Face, G.Left, G.Right, 1.f, PH, E, 14.f) : ADPPose::Reach(St, Face, 1, G.Handle, 1.f, PH, E);
			}, 0.9f);
			Do(Ci, [this, Ci, c, s, Id] { if (ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld())) D3->TakeBagById(Id, Body(Ci), GsbHand); Carts[c].Bags[s] = INDEX_NONE; });
			if (Li == INDEX_NONE) { Do(Ci, [this, Id] { if (ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld())) D3->RemoveBag(Id); }); continue; }
			WalkTo(Ci, [this, Li] { return TW(Tractors[Li], GsbStandLoad); }, 20.f);
			WaitFor(Ci, [this, Li](float E) { return (BeltBottomFree(Tractors[Li]) && Tractors[Li].bBelt) || E > 60.f; });
			PoseAct(Ci, [this, Li, Id](float K, float PH, FDriverPose& P)
			{
				const ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld()); if (!D3) return;
				const FTractor& L = Tractors[Li];
				const FTransform SlotW = BeltOffset(L, 0.f) * L.M->GetActorTransform();
				const FTransform Bw = D3->BagTransform(Id); const FDepBagGrips G = D3->Grips(Id);
				const FVector St = TW(L, GsbStandLoad), Face = FRotator(0.f, L.Yaw, 0.f).Vector();
				FDriverPose Q = ADPPose::Reach(St, Face, 1, SlotW.TransformPosition(Bw.InverseTransformPosition(G.Handle)), 1.f, PH, FMath::SmoothStep(0.f, 1.f, K));
				if (G.bTwoHands) { Q.bHand[0] = true; Q.Hand[0] = FMath::Lerp(G.Left, SlotW.TransformPosition(Bw.InverseTransformPosition(G.Left)), K); Q.Grip[0] = 1.f; }
				P = Q;
			}, 1.0f);
			Do(Ci, [this, Li, Id]
			{
				FTractor& L = Tractors[Li]; FTractor::FBeltBag B; B.Id = Id; B.S = 0.f; L.BeltBags.Add(B);
				if (ADepartureBelts* D3 = ADepartureBelts::Get(GetWorld())) D3->PutOnSlot(Id, L.M->GetRootComponent(), NAME_None, BeltOffset(L, 0.f));
			});
		}
	}
	ADP_LOG(TEXT("GSE: %d bags from %s's train to the bulk hold"), Count, *T.Name);
	// the bags all up: the curtains down, back in, home (the belt loader leaves once the train has gone)
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
	const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D) return;
	int32 Id = BagScan;
	for (; Id < BagScan + 400; ++Id) { const FDepBagTag* T = D->Tag(Id); if (!T) break; if (!T->Flight.IsEmpty()) BagFlights.Add(T->Flight); }
	BagScan = Id;
	if (bTest) return;
	for (const FString& F : BagFlights)
	{
		if (MadeUp.Contains(F) || D->BagsOf(F).Num() == 0) continue;
		bool bHas = false; for (const auto& K : BagKey) if (K.Value == F) bHas = true;
		if (bHas) continue;
		// a free tractor and a pair of free carts, the driver to the carousel now (its stand comes with its departure)
		StartFor(F, 0.f);
		break;
	}
}
