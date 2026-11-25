// To-do 238: the baggage tractor's bags by your own hands - at the make-up carousel and at the arrivals conveyor (the belt loader's
// carrying, GSELoaderBags.cpp) - and the train you drive as a flight's own: at its stand, left behind, left indoors, loose carts.
#include "GSE.h"
#include "VehicleControls.h"
#include "DepartureBelts.h"
#include "BaggageBelts.h"
#include "BabylonModel.h"
#include "Turnaround.h"
#include "CrewPose.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "CollisionQueryParams.h"
#include "Tugs.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ADPWalker.h"
#include "Algo/Reverse.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	enum { TbTakeCar = 60, TbPutCart, TbPutCar, TbTakeCart, TbPutConv };   // (the controls: GSETractorYou.cpp)
	const float TbStX = 183.05f, TbStZ = 63.f;   // the arrivals conveyor's loading point (GSEArrivals.cpp)
	FVector TbYaw(float Y) { return FRotator(0.f, Y, 0.f).Vector(); }
	float TbTurn(float A, float B) { return FMath::Abs(FMath::FindDeltaAngleDegrees(A, B)); }
	const APawn* TbMe(const UWorld* W) { const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; return PC ? PC->GetPawn() : nullptr; }
	bool TbEye(const UWorld* W, FVector& E, FVector& F) { const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; if (!PC || !PC->GetPawn() || !PC->GetPawn()->IsA<AADPWalker>()) return false; FRotator R; PC->GetPlayerViewPoint(E, R); F = R.Vector(); return true; }
	bool TbArr(const FString& F) { return F.StartsWith(TEXT("ARR ")); }
	FString TbNo(const FString& F) { return TbArr(F) ? FString(TEXT("an arriving flight")) : F; }
	bool TbCross(const FVector& C, const FVector& F, const FVector& R, float Hl, float Hw, const FVector& O, const FVector& X, const FVector& Y, float Hx, float Hy)
	{
		const FVector D = O - C; const FVector Ax[4] = {F, R, X, Y};
		for (const FVector& A : Ax) if (FMath::Abs(D.X * A.X + D.Y * A.Y) > Hl * FMath::Abs(F | A) + Hw * FMath::Abs(R | A) + Hx * FMath::Abs(X | A) + Hy * FMath::Abs(Y | A)) return false;
		return true;
	}
	void TbNoWalkers(UWorld* W, FCollisionQueryParams& Q) { for (TActorIterator<AADPWalker> It(W); It; ++It) { Q.AddIgnoredActor(*It); if (It->HumanActor()) Q.AddIgnoredActor(It->HumanActor()); } }
}

// ------------------------------------------------------------------ whose train, whose bag

FString AGSE::BagOf(int32 Id) const
{
	const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); const FDepBagTag* G = D && Id != INDEX_NONE ? D->Tag(Id) : nullptr;
	return G ? G->Flight : FString();
}

/** The flight whose bags a train takes: a crew's own, else of the bags that lie in it (empty: any). */
FString AGSE::TrainTakes(int32 Ti) const
{
	const FTractor& T = Tractors[Ti]; if (T.Job != EJob::None && !T.bArrival) return BagKey.FindRef(Ti);
	return TrainFlight(T);
}

/** A train whose bags you may handle here: yours, parked; or a crew's that is loading or unloading indoors. */
bool AGSE::TrainHands(int32 Ti) const
{
	const FTractor& T = Tractors[Ti]; if (!T.IsTrain() || T.Train.Num() == 0 || !T.M.IsValid() || FMath::Abs(T.Speed) > 1.f || -T.At.Y / 100.f > 100.f) return false;
	return T.Job == EJob::None ? !T.Player.IsValid() : T.Job == EJob::Loading && (T.bArrival || !BagKey.FindRef(Ti).IsEmpty());
}

int32 AGSE::TrainYouBag() const
{
	for (int32 i = 0; i < Tractors.Num(); ++i) if (Tractors[i].IsTrain() && Tractors[i].LYouBag != INDEX_NONE) return i;
	return INDEX_NONE;
}

/** The train that offers its bags to you: the one whose bag you hold, else the nearest. */
int32 AGSE::TrainYours() const
{
	if (const int32 h = TrainYouBag(); h != INDEX_NONE) return h;
	const APawn* Me = TbMe(GetWorld()); if (!Me) return INDEX_NONE;
	for (const FTractor& T : Tractors) if (T.LYouBag != INDEX_NONE) return INDEX_NONE;   // (one of the belt loader's is in your hands)
	int32 R = INDEX_NONE; double Best = 1200.0;
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		if (!TrainHands(i)) continue;
		double D = FVector::Dist2D(Tractors[i].At, Me->GetActorLocation()); for (const int32 c : Tractors[i].Train) if (Carts.IsValidIndex(c)) D = FMath::Min(D, FVector::Dist2D(Carts[c].At, Me->GetActorLocation()));
		if (D < Best) { Best = D; R = i; }
	}
	return R;
}

bool AGSE::TrainPlace(bool bBag, int32& Ti, int32& Cart, int32& Slot) const
{
	// the bag, or the free place, you look at on the cart before you
	Ti = Cart = Slot = INDEX_NONE; FVector E, F; if (!TbEye(GetWorld(), E, F)) return false;
	double Best = 0.75;
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		if (!TrainHands(i)) continue;
		for (const int32 c : Tractors[i].Train)
		{
			if (!Carts.IsValidIndex(c) || FVector::DistSquared2D(Carts[c].At, E) > FMath::Square(420.f)) continue;
			const int32 Sd = FVector::DotProduct(E - Carts[c].At, TbYaw(Carts[c].Yaw + 90.f)) > 0.f ? 1 : 0;   // (the side you stand at)
			if (Carts[c].Curtain[Sd] < 0.9f) continue;
			for (int32 s = 0; s < Carts[c].Bags.Num(); ++s)
			{
				if ((Carts[c].Bags[s] != INDEX_NONE) != bBag || SlotTaken.Contains(c * 100 + s)) continue;
				const FVector D = CW(Carts[c], CartSlot(c, s).GetLocation() + FVector(0.f, 0.f, 15.f)) - E; const double Dot = D.GetSafeNormal() | F;
				if (D.Size() < 230.0 && Dot > Best) { Best = Dot; Ti = i; Cart = c; Slot = s; }
			}
		}
	}
	return Slot != INDEX_NONE;
}

int32 AGSE::CarouselBag() const
{
	FVector E, F; const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D || !TbEye(GetWorld(), E, F)) return INDEX_NONE;
	const float Now = GetWorld()->GetTimeSeconds(); if (Now < YCarT) return YCarId;
	YCarT = Now + 0.1f; YCarId = INDEX_NONE; double Best = 0.85;
	for (const FString& Fl : BagFlights)
	{
		const int32 K = D->CarouselOf(Fl); if (TbArr(Fl) || K < 0 || FVector::DistSquared2D(D->CarouselCentre(K), E) > FMath::Square(2500.f)) continue;
		for (const int32 Id : D->BagsOnCarousel(Fl))
		{
			const FVector V = D->BagTransform(Id).GetLocation() + FVector(0.f, 0.f, 12.f) - E; const double Dot = V.GetSafeNormal() | F;
			if (V.Size() < 230.0 && Dot > Best) { Best = Dot; YCarId = Id; }
		}
	}
	return YCarId;
}

bool AGSE::AtConveyor() const
{
	FVector E, F; return TbEye(GetWorld(), E, F) && FVector::Dist2D(E, Plan(TbStX, TbStZ)) < 260.f && ((Plan(TbStX, TbStZ) - E).GetSafeNormal2D() | F.GetSafeNormal2D()) > 0.6;
}

// ------------------------------------------------------------------ by your hands

FString AGSE::TrainBagSay(int32 Ti, int32 S) const
{
	if (TrainYours() != Ti) return FString();
	const FTractor& T = Tractors[Ti]; const bool bHand = T.LYouBag != INDEX_NONE; int32 t, c, s;
	if (bHand && (T.LYouTo || T.LYouMov < 1.f)) return FString();
	const bool bArr = bHand && TbArr(BagOf(T.LYouBag));
	switch (S)
	{
	case TbTakeCar: if (!bHand && CarouselBag() != INDEX_NONE) return TEXT("Take the bag off the carousel"); break;
	case TbTakeCart: if (!bHand && TrainPlace(true, t, c, s)) return TEXT("Take the bag off the cart"); break;
	case TbPutCart: if (bHand && TrainPlace(false, t, c, s)) return TEXT("Put the bag on the cart"); break;
	case TbPutConv: if (bArr && AtConveyor()) return TEXT("Put the bag on the conveyor"); break;
	case TbPutCar:
	{
		FVector E, F; const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!bHand || bArr || !D || !TbEye(GetWorld(), E, F)) break;
		const int32 K = D->CarouselOf(BagOf(T.LYouBag)); if (K < 0) break;
		const FVector V = D->CarouselCentre(K) - E;   // (its belt before you)
		if (V.Size2D() < 600.f && (V.GetSafeNormal2D() | F.GetSafeNormal2D()) > 0.3 && !TrainPlace(false, t, c, s)) return TEXT("Put the bag back on the carousel");
		break;
	}
	}
	return FString();
}

bool AGSE::TrainBagMay(int32 Ti, int32 S, FString& Why) const
{
	int32 t, c, s; if (TrainBagSay(Ti, S).IsEmpty()) return false;
	if (S != TbPutCart || !TrainPlace(false, t, c, s)) return true;
	const FString Is = BagOf(Tractors[Ti].LYouBag), To = TrainTakes(t);
	if (To.IsEmpty() || To == Is) return true;
	Why = FString::Printf(TEXT("wrong flight: this bag goes with %s, the train carries %s's"), *TbNo(Is), *TbNo(To));
	return false;
}

FVector AGSE::TrainBagSpot(int32 Ti, int32 S) const
{
	int32 t, c, s; const ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
	if (S == TbTakeCar && D) if (const int32 Id = CarouselBag(); Id != INDEX_NONE) return D->BagTransform(Id).GetLocation() + FVector(0.f, 0.f, 15.f);
	if ((S == TbTakeCart || S == TbPutCart) && TrainPlace(S == TbTakeCart, t, c, s)) return CW(Carts[c], CartSlot(c, s).GetLocation() + FVector(0.f, 0.f, 15.f));
	if (S == TbPutConv) return Plan(TbStX, TbStZ) + FVector(0.f, 0.f, 75.f);
	FVector E, F; if (S == TbPutCar && TbEye(GetWorld(), E, F)) return E + FVector(F.X, F.Y, 0.f).GetSafeNormal() * 110.f - FVector(0.f, 0.f, 80.f);
	return TrainHitch(Tractors[Ti]);
}

bool AGSE::TrainBagDo(int32 Ti, int32 S)
{
	FString Why; if (!TrainBagMay(Ti, S, Why)) return false;
	FTractor& T = Tractors[Ti]; ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D) return false;
	int32 t, c, s;
	switch (S)
	{
	case TbTakeCar:
	{
		const int32 Id = CarouselBag(); const FString F = BagOf(Id); YCarT = 0.f;
		const FTransform Was = D->BagTransform(Id);
		if (Id == INDEX_NONE || !D->TakeBagById(Id, nullptr, NAME_None)) { ADP_WARN(TEXT("GSE: the bag you reached for has left the carousel")); return false; }
		D->PutOnSlot(Id, GetRootComponent(), NAME_None, Was);   // (as the crew's, GSEBags.cpp)
		if (!F.IsEmpty()) D->NoAutoClear.Add(F);   // (its bags wait on the carousel, as for the crew)
		T.LYouCart = T.LYouSlot = INDEX_NONE; LoaderHand(T, Id, 5); return true;
	}
	case TbTakeCart:
	{
		if (!TrainPlace(true, t, c, s)) return false;
		const int32 Id = Carts[c].Bags[s]; Carts[c].Bags[s] = INDEX_NONE; T.LYouCart = c; T.LYouSlot = s; LoaderHand(T, Id, 1); return true;
	}
	case TbPutCart: if (!TrainPlace(false, t, c, s)) return false; T.LYouCart = c; T.LYouSlot = s; LoaderLay(T, 2); return true;
	case TbPutCar: LoaderLay(T, 5); return true;
	case TbPutConv: LoaderLay(T, 6); return true;
	}
	return false;
}

/** A bag of a train out of your hands: where you lay it, or back where it came from. Never lost (no place: you keep it). */
void AGSE::TrainBagDrop(FTractor& T)
{
	ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D || T.LYouBag == INDEX_NONE) return;
	const int32 Id = T.LYouBag, Ti = int32(&T - Tractors.GetData()); const uint8 To = T.LYouTo; const FString F = BagOf(Id); const bool bArr = TbArr(F);
	T.LYouTo = 0;
	if (To == 2) SlotTaken.Remove(T.LYouCart * 100 + T.LYouSlot);
	auto Cart = [&](int32 c, int32 s)
	{
		if (!Carts.IsValidIndex(c) || !Carts[c].Bags.IsValidIndex(s) || Carts[c].Bags[s] != INDEX_NONE || SlotTaken.Contains(c * 100 + s) || !Carts[c].M.IsValid()) return false;
		D->PutOnSlot(Id, Carts[c].M->GetRootComponent(), NAME_None, CartSlot(c, s)); Carts[c].Bags[s] = Id; if (!bArr && Carts[c].Flight.IsEmpty()) Carts[c].Flight = F;
		T.LYouBag = INDEX_NONE; return true;
	};
	if (To == 6 && bArr)
	{
		D->PutOnSlot(Id, GetRootComponent(), NAME_None, FTransform(FRotator(0.f, -90.f, 0.f), Plan(TbStX, TbStZ) + FVector(0, 0, 75.f)));
		if (ABaggageBelts* B = ABaggageBelts::Get(GetWorld())) B->Unload(ArrBelt(T.bArrival ? T.Key : F.RightChop(4), T.StandX), 1, 3); else ADP_WARN(TEXT("GSE: no claim belts for the bag you laid on the arrivals conveyor"));
		PendingRemove.Add(TPair<double, int32>(GetWorld()->GetTimeSeconds() + 2.6, Id)); T.LYouBag = INDEX_NONE; return;
	}
	if ((To == 2 || (To == 0 && T.LYouFrom == 1)) && Cart(T.LYouCart, T.LYouSlot)) return;
	if (!bArr) if (const int32 K = D->CarouselOf(F); K >= 0 && D->PutOnCarousel(Id, K)) { T.LYouBag = INDEX_NONE; return; }
	const FString Tk = TrainTakes(Ti);
	if (Tk.IsEmpty() || Tk == F) for (const int32 c : T.Train) if (Carts.IsValidIndex(c)) for (int32 s = 0; s < Carts[c].Bags.Num(); ++s) if (Cart(c, s)) return;
	if (const float Now = GetWorld()->GetTimeSeconds(); Now > T.YNoteT) { T.YNoteT = Now + 8.f; AVehicleControls::Note(GetWorld(), TEXT("No free place for the bag")); ADP_WARN(TEXT("GSE: no free place for the bag of %s in your hands - you keep it"), *F); }
}

void AGSE::TrainBags(int32 Ti, float Dt)
{
	FTractor& T = Tractors[Ti]; ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D || T.LYouBag == INDEX_NONE || !T.M.IsValid()) return;
	T.LYouMov = FMath::Min(1.f, T.LYouMov + Dt / 0.8f);
	if (T.LYouTo && T.LYouMov >= 1.f) TrainBagDrop(T);
	else D->PutOnSlot(T.LYouBag, T.M->GetRootComponent(), NAME_None, LoaderBagAt(T));
}

// ------------------------------------------------------------------ your train as a flight's own

int32 AGSE::YouTrainFor(const FString& F) const
{
	if (!F.IsEmpty()) for (int32 i = 0; i < Tractors.Num(); ++i) if (Tractors[i].IsTrain() && Tractors[i].Job == EJob::None && Tractors[i].Train.Num() && TrainFlight(Tractors[i]) == F) return i;
	return INDEX_NONE;
}

/** Your train at the carousel of this flight, with room: no second one is sent there. */
bool AGSE::YouAtCarousel(const FString& F) const
{
	const int32 i = YouTrainFor(F); const ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (i == INDEX_NONE || !D) return false;
	const int32 K = D->CarouselOf(F); if (K < 0 || FVector::Dist2D(Tractors[i].At, D->CarouselCentre(K)) > 2500.f) return false;
	for (const int32 c : Tractors[i].Train) if (Carts.IsValidIndex(c) && Carts[c].Bags.Contains(INDEX_NONE)) return true;
	return false;
}

/** Parked in a train's place beside the aircraft (yours, left there). */
bool AGSE::TrainInPlace(const FTractor& T, float StandX) const
{
	float S = 0.f, N = 0.f, Sp = 36.f; if (T.Player.IsValid() || FMath::Abs(T.Speed) > 0.5f || !StandGeo(StandX, S, N, Sp)) return false;
	const FVector D = T.At - PlanT(S, S - 11.f, N + 25.74f); float Yw = 0.f; PoseT(S, S, N, 0.f, -1.f, Yw);
	return FMath::Abs(D.X) < 300.f && FMath::Abs(D.Y) < 700.f && TbTurn(T.Yaw, Yw) < 25.f;
}

bool AGSE::TrainByConveyor(const FTractor& T) const
{
	const float X = T.At.X / 100.f, Y = -T.At.Y / 100.f; return X > 172.f && X < 186.f && Y > 57.5f && Y < 61.5f && TbTurn(T.Yaw, 180.f) < 25.f;
}

void AGSE::YouTrain(int32 Ti, float Dt)
{
	FTractor& T = Tractors[Ti];
	if (T.Job != EJob::None) { if (T.Job != EJob::Boarding) { T.YStandX = -1e6f; T.YJobT = 0.f; } return; }
	if (T.YStandX > -1e5f) T.YJobT += Dt; else T.YJobT = 0.f;
	if ((T.YAskT += Dt) < 2.f) return;
	const float Step = T.YAskT; T.YAskT = 0.f;
	const FString F = T.Train.Num() ? TrainFlight(T) : FString(); const bool bArr = TbArr(F);
	const ADPTurnaround::FTurn* Tn = F.IsEmpty() || bArr ? nullptr : ADPTurnaround::ForFlight(F);
	T.YStandX = Tn ? Tn->StandX : -1e6f;
	const bool bThere = T.YBar == INDEX_NONE && T.LYouBag == INDEX_NONE && (Tn ? TrainInPlace(T, Tn->StandX) : bArr && !T.Player.IsValid() && FMath::Abs(T.Speed) < 0.5f && TrainByConveyor(T));
	if (!bThere) { T.YAtT = 0.f; return; }
	if ((T.YAtT += Step) < 5.f) return;
	T.YAtT = 0.f;
	if (bArr) { TrainJoin(Ti, F, 0.f, [this, Ti] { ArrivalUnload(Ti); }); return; }   // (an arrival train you brought to the conveyor)
	EnsureLoader(Tn->StandX, F);
	TrainJoin(Ti, F, Tn->StandX, [this, Ti] { Tractors[Ti].Job = EJob::Out; Tractors[Ti].bPark = false; TrainLoad(Ti); });
}

/** A crew take over your train where it stands, as the flight's own. */
bool AGSE::TrainJoin(int32 Ti, const FString& F, float StandX, TFunction<void()> Then)
{
	const bool bArr = TbArr(F);
	{ FTractor& T = Tractors[Ti]; T.Key = bArr ? F.RightChop(4) : F; T.StandX = StandX; T.bArrival = bArr; T.ArrKey = bArr ? T.Key : FString(); }
	const int32 Ci = Borrow(Tractors[Ti].At, nullptr, Ti);
	FTractor& U = Tractors[Ti];
	if (Ci == INDEX_NONE) { U.Key.Reset(); U.StandX = 0.f; U.bArrival = false; U.ArrKey.Reset(); ADP_LOG(TEXT("GSE: nobody free yet for the train you left with the bags of %s"), *F); return false; }
	U.Driver = Ci; U.Job = EJob::Boarding; U.JobT = 0.f; U.YLeftT = 0.f; U.YTryT = 0.f; bWorked = true;
	if (!bArr && !F.IsEmpty()) { BagKey.Add(Ti, F); for (const int32 c : U.Train) if (Carts.IsValidIndex(c)) Carts[c].Flight = F; }
	ADP_LOG(TEXT("GSE: a crew take over the train you left for %s (%s, %d carts)"), *F, *U.Name, U.Train.Num());
	TrainTidy(Ci, Ti);
	ClimbIn(Ci, Ti, MoveTemp(Then));
	return true;
}

bool AGSE::YouArrTrain(float StandX, const FString& Key)
{
	// your empty train in the place beside an arrived aircraft: its bags go onto that one
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		FTractor& T = Tractors[i]; if (!T.IsTrain() || T.Job != EJob::None || T.Train.Num() == 0 || !TrainFlight(T).IsEmpty() || T.YBar != INDEX_NONE || !TrainInPlace(T, StandX)) continue;
		return TrainJoin(i, TEXT("ARR ") + Key, StandX, [this, i] { Tractors[i].Job = EJob::Out; Tractors[i].bPark = false; ArrivalLoad(i); });
	}
	return false;
}

// ------------------------------------------------------------------ the way from anywhere indoors

bool AGSE::WayClear(const FTractor& T, const FVector& At, float Yaw, const TArray<FOutline>& Ol, const FCollisionQueryParams& Q) const
{
	const FVector F = TbYaw(Yaw), R = TbYaw(Yaw + 90.f), C = At + F * T.KCentreX, Mine = T.At + TbYaw(T.Yaw) * T.KCentreX; const float Hl = T.KHalfL + 10.f, Hw = T.KHalfW + 12.f;
	for (const FOutline& O : Ol) if (FVector::Dist2D(O.C, Mine) > 5.f && !TrainOwn(T, O.C) && TbCross(C, F, R, Hl, Hw, O.C, O.X, O.Y, O.Hx, O.Hy)) return false;
	return !GetWorld()->OverlapBlockingTestByChannel(FVector(C.X, C.Y, At.Z + 145.f), FQuat(FRotator(0.f, Yaw, 0.f)), ECC_Pawn, FCollisionShape::MakeBox(FVector(Hl, Hw, 85.f)), Q);
}

/** A way for T, as it stands, onto the plan pose (x, y, hx, hy): straight on, else round what stands in the way. */
bool AGSE::WayTo(const FTractor& T, float x, float y, float hx, float hy, TArray<FVector>& Out) const
{
	ADP_PERF_SCOPE("a left train's way home (once a minute)", 250.f);
	TArray<FOutline> Ol; Outlines(Ol); float Yw = 0.f; const FVector G = PlanPose(x, y, hx, hy, Yw);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPTrainWay), true); Q.AddIgnoredActor(T.M.Get()); for (const FCart& Ct : Carts) if (Ct.M.IsValid()) Q.AddIgnoredActor(Ct.M.Get());   // (the carts: by their outlines)
	TbNoWalkers(GetWorld(), Q);
	auto Test = [&]() -> bool
	{
		// driven along, every 80 cm in the direction of travel
		FVector P = T.At; double Since = 80.0, Len = 0.0;
		for (const FVector& N : Out)
		{
			const FVector D = N - P; const double L = D.Size2D(); if (L < 1.0) continue;
			if ((Len += L) > 20000.0) return false;
			for (double s = 0.0; s < L; s += 20.0) if ((Since += FMath::Min(20.0, L)) >= 80.0) { Since = 0.0; if (!WayClear(T, P + D * (s / L), FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), Ol, Q)) return false; }
			P = N;
		}
		return true;
	};
	Out.Reset(); Dubins(Out, T.At, T.Yaw, G, Yw, 400.f); if (Test()) return true;
	// a grid of metres, each cell tried with room for the tractor any way round
	constexpr int32 X0 = 100, Y0 = 20, Nx = 185, Ny = 86; TArray<uint8> Ok; Ok.Init(0, Nx * Ny); TArray<float> Cost; Cost.Init(1e9f, Nx * Ny); TArray<int32> From; From.Init(INDEX_NONE, Nx * Ny);
	auto Cell = [&](const FVector& P) -> int32 { const int32 i = FMath::FloorToInt32(P.X / 100.f) - X0, j = FMath::FloorToInt32(-P.Y / 100.f) - Y0; return i >= 0 && j >= 0 && i < Nx && j < Ny ? j * Nx + i : INDEX_NONE; };
	auto At = [&](int32 k) { return Plan(X0 + k % Nx + 0.5f, Y0 + k / Nx + 0.5f); };
	const FVector Mine = T.At + TbYaw(T.Yaw) * T.KCentreX; const int32 S = Cell(T.At), Gk = Cell(G); if (S == INDEX_NONE || Gk == INDEX_NONE) return false;
	auto Free = [&](int32 k) -> bool
	{
		if (Ok[k]) return Ok[k] == 1;
		const FVector P = At(k); bool b = k == S || k == Gk || !GetWorld()->OverlapBlockingTestByChannel(P + FVector(0.f, 0.f, 145.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeBox(FVector(120.f, 120.f, 85.f)), Q);
		for (const FOutline& O : Ol) if (b && k != S && FVector::Dist2D(O.C, Mine) > 5.f && !TrainOwn(T, O.C)) { const FVector D = P - O.C; b = FMath::Abs(D | O.X) > O.Hx + 115.f || FMath::Abs(D | O.Y) > O.Hy + 115.f; }
		Ok[k] = b ? 1 : 2; return b;
	};
	using FNode = TPair<float, int32>; TArray<FNode> Heap; auto Less = [](const FNode& A, const FNode& B) { return A.Key < B.Key; };
	Cost[S] = 0.f; Heap.HeapPush(FNode(0.f, S), Less); const FVector2D Ge(Gk % Nx, Gk / Nx); int32 Guard = 0;
	while (Heap.Num() && ++Guard < 60000)
	{
		FNode N; Heap.HeapPop(N, Less); const int32 k = N.Value; if (k == Gk) break;
		for (int32 dj = -1; dj <= 1; ++dj) for (int32 di = -1; di <= 1; ++di)
		{
			const int32 i = k % Nx + di, j = k / Nx + dj; if ((!di && !dj) || i < 0 || j < 0 || i >= Nx || j >= Ny) continue;
			const int32 m = j * Nx + i; const float C = Cost[k] + (di && dj ? 1.414f : 1.f); if (C >= Cost[m] || !Free(m)) continue;
			Cost[m] = C; From[m] = k; Heap.HeapPush(FNode(C + float(FVector2D::Distance(Ge, FVector2D(i, j))), m), Less);
		}
	}
	if (From[Gk] == INDEX_NONE) return false;
	TArray<FVector> Pt; for (int32 k = Gk; k != INDEX_NONE; k = From[k]) Pt.Add(k == Gk ? G : k == S ? T.At : At(k)); Algo::Reverse(Pt);
	// the straight pieces, joined by the tractor's own turns
	auto See = [&](const FVector& A, const FVector& B) -> bool { const double L = FVector::Dist2D(A, B); for (double s = 0.0; s < L; s += 50.0) { const int32 k = Cell(A + (B - A) * (s / L)); if (k == INDEX_NONE || !Free(k)) return false; } return true; };
	TArray<FVector> Co; Co.Add(Pt[0]); for (int32 a = 0; a < Pt.Num() - 1;) { int32 b = Pt.Num() - 1; while (b > a + 1 && !See(Pt[a], Pt[b])) --b; Co.Add(Pt[b]); a = b; }
	Out.Reset(); FVector P = T.At; float Y = T.Yaw;
	for (int32 k = 1; k + 1 < Co.Num(); ++k)
	{
		const FVector D = Co[k + 1] - Co[k]; if (D.Size2D() < 600.f) continue;
		const FVector M = Co[k] + D * 0.5f; const float Ym = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)); Dubins(Out, P, Y, M, Ym, 400.f); P = M; Y = Ym;
	}
	Dubins(Out, P, Y, G, Yw, 400.f);
	return Test();
}

/** The way from indoors: 0 home (the tractors' boxes), 1 out to the apron, 2 to the arrivals conveyor. */
bool AGSE::WayIn(const FTractor& T, int32 Kind, TArray<FVector>& W) const
{
	const float X = T.At.X / 100.f, Y = -T.At.Y / 100.f; if (Y >= 100.f) return false;
	const bool bPass = X > 258.f && Y >= 61.f, bHall = !bPass && X >= 137.f && Y >= 50.5f;
	auto L = [this, &W](float a, float b, float c, float d, float e, float f, float g, float h) { Leg(W, a, b, c, d, e, f, g, h, 400.f); };
	if (Kind == 1)
	{
		float Yj = 58.4f;
		if (bPass) { Yj = FMath::Clamp(Y + 10.f, 70.f, 94.f); if (!WayTo(T, 269.f, Yj, 0.f, 1.f, W)) return false; }
		else
		{
			float Xj = 152.f;
			if (bHall) { Xj = FMath::Clamp(X + 12.f, 152.f, 258.f); if (!WayTo(T, Xj, 54.4f, 1.f, 0.f, W)) { if (Xj == 258.f) return false; Xj = 258.f; if (!WayTo(T, Xj, 54.4f, 1.f, 0.f, W)) return false; } }
			else { if (!WayTo(T, 134.43f, 47.f, 0.f, 1.f, W)) return false; L(134.43f, 47.f, 0.f, 1.f, 134.43f, 53.5f, 0.f, 1.f); L(134.43f, 53.5f, 0.f, 1.f, 137.93f, 57.f, 1.f, 0.f); L(137.93f, 57.f, 1.f, 0.f, 152.f, 54.4f, 1.f, 0.f); }
			L(Xj, 54.4f, 1.f, 0.f, 265.f, 54.4f, 1.f, 0.f); L(265.f, 54.4f, 1.f, 0.f, 269.f, 58.4f, 0.f, 1.f);
		}
		if (Yj < 88.f) L(269.f, Yj, 0.f, 1.f, 269.f, 88.f, 0.f, 1.f);
		return true;
	}
	float Xj = 265.f;
	if (bPass) { const float Yj = FMath::Clamp(Y - 10.f, 61.f, 93.f); if (!WayTo(T, 269.f, Yj, 0.f, -1.f, W)) return false; if (Yj > 61.5f) L(269.f, Yj, 0.f, -1.f, 269.f, 61.f, 0.f, -1.f); L(269.f, 61.f, 0.f, -1.f, 265.f, 57.f, -1.f, 0.f); }
	else if (bHall)
	{
		const float Lo = Kind == 2 ? 196.f : 139.f; if (Kind == 2 && X < 200.f) return false;
		Xj = FMath::Clamp(X - 12.f, Lo, 258.f); if (!WayTo(T, Xj, 57.f, -1.f, 0.f, W)) { if (Xj == Lo) return false; Xj = Lo; if (!WayTo(T, Xj, 57.f, -1.f, 0.f, W)) return false; }
	}
	else return Kind == 0 && WayTo(T, 133.f, 50.f, 0.f, -1.f, W);
	if (Kind == 2) { L(Xj, 57.f, -1.f, 0.f, 192.f, 57.f, -1.f, 0.f); L(192.f, 57.f, -1.f, 0.f, 178.f, 60.f, -1.f, 0.f); return true; }
	if (Xj > 139.5f) L(Xj, 57.f, -1.f, 0.f, 137.f, 57.f, -1.f, 0.f);
	L(137.f, 57.f, -1.f, 0.f, 133.f, 53.f, 0.f, -1.f); L(133.f, 53.f, 0.f, -1.f, 133.f, 50.f, 0.f, -1.f);
	return true;
}

/** A crew sit in the train you left: off to where it is wanted (as WayIn). No way: tried again every minute, for good. */
void AGSE::TrainAway(int32 Ti, int32 Kind)
{
	FTractor& T = Tractors[Ti]; TArray<FVector> W; T.bReverse = false; T.bPark = false; T.Job = Kind == 1 ? EJob::Out : EJob::Back;
	Motor(T, true);
	if (Kind == 2 && TrainByConveyor(T)) { ArrivalUnload(Ti); return; }
	if (Kind == 0 && T.TowGpu == INDEX_NONE && TractorWhere(T) == 3) { TractorHome(Ti); return; }
	if (-T.At.Y / 100.f >= 100.f) { if (Kind == 1) TrainApron(Ti); else if (Kind == 2) ArrivalIn(Ti); else TractorHome(Ti); return; }
	if ((Kind != 0 || T.TowGpu == INDEX_NONE) && WayIn(T, Kind, W)) { Go(T, MoveTemp(W), false, [this, Ti, Kind] { if (Kind == 0) TractorIn(Ti); else if (Kind == 1) TrainApron(Ti); else ArrivalUnload(Ti); }); return; }
	// (the wrong way round: out to the apron and in again)
	W.Reset();
	if (Kind != 1 && WayIn(T, 1, W)) { Go(T, MoveTemp(W), false, [this, Ti, Kind] { FTractor& U = Tractors[Ti]; if (Kind == 2) ArrivalIn(Ti); else if (U.TowGpu != INDEX_NONE) TractorGpu(Ti); else RouteIn(U, U.At, U.Yaw, [this, Ti] { TractorIn(Ti); }); }); return; }
	// (nose to a wall: a few metres back first, with nothing behind it)
	if (T.Train.Num() == 0 && T.TowGpu == INDEX_NONE && T.YTryT < 3.f)
	{
		TArray<FOutline> Ol; Outlines(Ol); FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPTrainBack), true); Q.AddIgnoredActor(T.M.Get()); for (const FCart& Ct : Carts) if (Ct.M.IsValid()) Q.AddIgnoredActor(Ct.M.Get());
		TArray<FVector> B; for (int32 m = 1; m <= 8; ++m) { const FVector P = T.At - TbYaw(T.Yaw) * (m * 100.f); if (!WayClear(T, P, T.Yaw, Ol, Q)) break; B.Add(P); }
		if (B.Num() >= 3) { T.YTryT += 1.f; Go(T, MoveTemp(B), true, [this, Ti, Kind] { TrainAway(Ti, Kind); }); return; }
	}
	T.YTryT = 0.f; Motor(T, false);
	ADP_WARN(TEXT("GSE: %s cannot be driven off from where you left it (%.0f, %.0f): no way round what stands there - tried again in a minute"), *T.Name, T.At.X / 100.f, -T.At.Y / 100.f);
	AVehicleControls::Note(GetWorld(), FString::Printf(TEXT("%s is boxed in where you left it: its crew wait for a way out"), *T.Name));
	RetryIn(Ti, 60.f, [this, Ti, Kind] { TrainAway(Ti, Kind); });
}

// ------------------------------------------------------------------ towed and drawn things against what stands there

bool AGSE::TowHit(const FTractor* T, const FVector& Cc, float Yaw, float Hl, float Hw, int32 OwnGpu, const FCollisionQueryParams& Q) const
{
	const FVector F = TbYaw(Yaw), R = TbYaw(Yaw + 90.f);
	TArray<FOutline> Ol; Outlines(Ol);
	for (const FOutline& O : Ol) if (FVector::Dist2D(O.C, Cc) > 30.f && !(T && (TrainOwn(*T, O.C) || FVector::Dist2D(O.C, T->At + TbYaw(T->Yaw) * T->KCentreX) < 60.f)) && TbCross(Cc, F, R, Hl, Hw, O.C, O.X, O.Y, O.Hx, O.Hy)) return true;
	const ATugs* G = Tg();
	if (G) for (int32 g = 0; g < G->Tugs.Num(); ++g)
	{
		const ATugs::FTug& U = G->Tugs[g];
		if (U.M.IsValid() && TbCross(Cc, F, R, Hl, Hw, U.At, TbYaw(U.Yaw), TbYaw(U.Yaw + 90.f), 255.f, 126.f)) return true;
		if (g == OwnGpu || !U.Gpu.M.IsValid()) continue;
		const FVector X = U.Gpu.M->GetActorForwardVector(); if (TbCross(Cc, F, R, Hl, Hw, U.Gpu.M->GetActorTransform().TransformPosition(FVector(40.f, 0.f, 0.f)), X, FVector(-X.Y, X.X, 0.f), 170.f, 90.f)) return true;
	}
	// people along its edge
	if (G) for (float a = -1.f; a <= 1.01f; a += 0.5f) for (const float b : {-1.f, 1.f})
		if (G->PersonAhead(FVector(Cc.X, Cc.Y, Cc.Z + 90.f) + F * (Hl * a) + R * (Hw * b), FVector::ZeroVector, 0.f)) return true;
	return GetWorld()->OverlapBlockingTestByChannel(FVector(Cc.X, Cc.Y, Cc.Z + 130.f), FQuat(FRotator(0.f, Yaw, 0.f)), ECC_Pawn, FCollisionShape::MakeBox(FVector(Hl - 4.f, Hw - 2.f, 70.f)), Q);
}

bool AGSE::CartHit(const FCart& C, const FTractor* T) const
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPCartHand), true); if (C.M.IsValid()) Q.AddIgnoredActor(C.M.Get());
	if (T) { Q.AddIgnoredActor(T->M.Get()); for (const int32 c : T->Train) if (Carts.IsValidIndex(c) && Carts[c].M.IsValid()) Q.AddIgnoredActor(Carts[c].M.Get()); }
	TbNoWalkers(GetWorld(), Q);   // (people: by their places)
	return TowHit(T, C.At - TbYaw(C.Yaw) * 2.f, C.Yaw, 140.f, 80.f, INDEX_NONE, Q);
}

// ------------------------------------------------------------------ a loose cart back to its place

void AGSE::CartFetches(float Dt)
{
	if (YFetch != INDEX_NONE)
	{
		if ((YFetchT += Dt) > 240.f) { ADP_WARN(TEXT("GSE: nobody finished putting the loose cart back - asked again")); YFetch = INDEX_NONE; bYFetchOn = false; YFetchT = 0.f; return; }
		if (!bYFetchOn || !Carts.IsValidIndex(YFetch) || Carts[YFetch].Tractor != INDEX_NONE) return;
		FCart& C = Carts[YFetch]; const FVector WasAt = C.At; const float WasY = C.Yaw, WasA = C.AxleYaw; const bool bWas = CartHit(C, nullptr);
		C.At = FMath::VInterpConstantTo(C.At, FVector(C.Place.X, C.Place.Y, C.At.Z), Dt, 30.f);
		const float M = FMath::Clamp(FMath::FindDeltaAngleDegrees(C.Yaw, C.PlaceYaw), -4.f * Dt, 4.f * Dt); C.Yaw += M; C.AxleYaw += M; C.bMoved = true;
		if (!bWas && CartHit(C, nullptr)) { C.At = WasAt; C.Yaw = WasY; C.AxleYaw = WasA; }
		PoseCart(C);
		return;
	}
	if ((YFetchT += Dt) < 2.f) return;
	const float Step = YFetchT; YFetchT = 0.f; const APawn* Me = TbMe(GetWorld());
	for (int32 c = 0; c < Carts.Num(); ++c)
	{
		FCart& C = Carts[c];
		if (!C.bOut || !C.bLoose || C.Tractor != INDEX_NONE || !C.M.IsValid()) continue;
		if (Me && FVector::Dist2D(Me->GetActorLocation(), C.At) < 4000.f) { C.YLeftT = 0.f; continue; }
		if ((C.YLeftT += Step) < 120.f) continue;
		C.YLeftT = 60.f;   // (nobody free, or something in its way: again in a minute)
		const int32 Ci = Borrow(C.At); if (Ci == INDEX_NONE) { ADP_LOG(TEXT("GSE: nobody free to put a loose cart back")); continue; }
		YFetch = c; bYFetchOn = false; YFetchT = 0.f; bWorked = true;
		ADP_LOG(TEXT("GSE: a loose cart is put back by hand (%.1f m off its place)"), FVector::Dist2D(C.At, C.Place) / 100.f);
		WalkTo(Ci, [this, c] { return CW(Carts[c], FVector(225.f, 62.f, 0.f)); }, 15.f);
		const TFunction<void(float, float, FDriverPose&)> Bar = CartBarPose(c);
		PoseAct(Ci, [Bar](float K, float PH, FDriverPose& P) { Bar(K * 0.5f, PH, P); }, 1.4f);
		Do(Ci, [this] { bYFetchOn = true; });
		PoseAct(Ci, [Bar](float, float PH, FDriverPose& P) { Bar(0.5f, PH, P); }, 5.f + FMath::Max(FVector::Dist2D(C.At, C.Place) / 25.f, TbTurn(C.Yaw, C.PlaceYaw) / 3.5f));
		Do(Ci, [this, Ci, c]
		{
			FCart& K = Carts[c]; if (YFetch == c) { bYFetchOn = false; YFetch = INDEX_NONE; YFetchT = 0.f; }
			if (K.Tractor == INDEX_NONE && FVector::Dist2D(K.At, K.Place) < 1.f && TbTurn(K.Yaw, K.PlaceYaw) < 0.5f) { K.bOut = K.bLoose = false; K.YLeftT = 0.f; ADP_LOG(TEXT("GSE: the loose cart stands in its place again")); }
			else ADP_WARN(TEXT("GSE: the loose cart could not be put back: something in its way - tried again in a minute"));
			GiveBack(Ci);
		});
		return;
	}
}
