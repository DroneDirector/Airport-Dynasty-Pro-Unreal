// To-do 238: the belt loader's bags by your own hands - off the train's cart onto the belt's foot, off its top into the hold, and
// the other way for an arrival: the crew's own bags, places and counts (GSEBags.cpp, GSEArrivals.cpp); they go on with the rest.
#include "GSE.h"
#include "VehicleControls.h"
#include "DepartureBelts.h"
#include "BabylonModel.h"
#include "ADPLog.h"
#include "Tugs.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	const float LbLen = 675.f;
	const APawn* LbMe(const UWorld* W) { const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; return PC ? PC->GetPawn() : nullptr; }
	bool LbEye(const UWorld* W, FVector& E, FVector& F) { const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; if (!PC || !PC->GetPawn()) return false; FRotator R; PC->GetPlayerViewPoint(E, R); F = R.Vector(); return true; }
	FTransform LbRoot(const AGSE::FTractor& L) { return FTransform(FRotator(0.f, L.Yaw, 0.f), L.At); }
}

int32 AGSE::LoaderTrain(const FTractor& L, bool* bArr) const
{
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		const FTractor& B = Tractors[i];
		if (!B.IsTrain() || B.Job != EJob::Loading || FMath::Abs(B.StandX - L.StandX) > 1.f || LoaderAt(B.StandX) != int32(&L - Tractors.GetData())) continue;
		if (bArr) *bArr = B.bArrival;
		return i;
	}
	return INDEX_NONE;
}

bool AGSE::LoaderCartBag(const FTractor& L, int32& Cart, int32& Slot) const
{
	// the bag you look at on its train, within reach and nobody else's
	Cart = Slot = INDEX_NONE; bool bArr = false; const int32 Ti = LoaderTrain(L, &bArr); FVector E, F;
	if (Ti == INDEX_NONE || bArr || !LbEye(GetWorld(), E, F)) return false;
	double Best = 0.8;
	for (const int32 c : Tractors[Ti].Train)
	{
		if (FVector::DistSquared2D(Carts[c].At, E) > FMath::Square(420.f) || FMath::Max(Carts[c].Curtain[0], Carts[c].Curtain[1]) < 0.9f) continue;
		for (int32 s = 0; s < Carts[c].Bags.Num(); ++s)
		{
			if (Carts[c].Bags[s] == INDEX_NONE || SlotTaken.Contains(c * 100 + s)) continue;
			const FVector D = CW(Carts[c], CartSlot(c, s).GetLocation() + FVector(0.f, 0.f, 15.f)) - E; const double Dot = D.GetSafeNormal() | F;
			if (D.Size() < 210.0 && Dot > Best) { Best = Dot; Cart = c; Slot = s; }
		}
	}
	return Slot != INDEX_NONE;
}

bool AGSE::LoaderCartFree(const FTractor& L, int32& Cart, int32& Slot, bool bMine) const
{
	// the first free place on its arrival train (yours: on the cart in front of you)
	Cart = Slot = INDEX_NONE; bool bArr = false; const int32 Ti = LoaderTrain(L, &bArr); FVector E, F;
	if (Ti == INDEX_NONE || !bArr || (bMine && !LbEye(GetWorld(), E, F))) return false;
	for (const int32 c : Tractors[Ti].Train)
	{
		if (bMine && (FVector::DistSquared2D(Carts[c].At + FVector(0.f, 0.f, 0.f), E) > FMath::Square(260.f) || ((Carts[c].At - E).GetSafeNormal2D() | F.GetSafeNormal2D()) < 0.6)) continue;
		for (int32 s = 0; s < Carts[c].Bags.Num(); ++s) if (Carts[c].Bags[s] == INDEX_NONE && !SlotTaken.Contains(c * 100 + s)) { Cart = c; Slot = s; return true; }
	}
	return false;
}

FTransform AGSE::LoaderBagAt(const FTractor& L) const
{
	// where the bag in your hands is now (root cm)
	const APawn* Me = LbMe(GetWorld()); const FTransform Root = LbRoot(L);
	const FVector Fw = Me ? Me->GetActorForwardVector() : Root.GetUnitAxis(EAxis::X);
	const FVector Feet = Me ? Me->GetActorLocation() - FVector(0.f, 0.f, Me->GetSimpleCollisionHalfHeight()) : L.At;
	// (across in front of you, its ends at your hands)
	FTransform To = FTransform(Fw.Rotation() + FRotator(0.f, 90.f, 0.f), Feet + Fw * 48.f + FVector(0.f, 0.f, 88.f)).GetRelativeTransform(Root);
	if (L.LYouTo == 1) To = BeltOffset(L, 0.f);
	else if (L.LYouTo == 2 && Carts.IsValidIndex(L.LYouCart)) To = (CartSlot(L.LYouCart, L.LYouSlot) * FTransform(FRotator(0.f, Carts[L.LYouCart].Yaw, 0.f), Carts[L.LYouCart].At)).GetRelativeTransform(Root);
	else if (L.LYouTo == 3) To = FTransform(FVector(95.f, 0.f, 0.f)) * BeltOffset(L, LbLen);
	else if (L.LYouTo == 4) To = BeltOffset(L, LbLen);
	else if (L.LYouTo == 5) To = FTransform(Fw.Rotation() + FRotator(0.f, 90.f, 0.f), Feet + Fw * 108.f + FVector(0.f, 0.f, 45.f)).GetRelativeTransform(Root);   // (a train's: towards the carousel, onto the conveyor)
	else if (L.LYouTo == 6) To = FTransform(FRotator(0.f, -90.f, 0.f), Plan(183.05f, 63.f) + FVector(0.f, 0.f, 75.f)).GetRelativeTransform(Root);
	const float K = FMath::SmoothStep(0.f, 1.f, L.LYouMov); FTransform R; R.Blend(L.LYouA, To, K); R.SetScale3D(FVector::OneVector);
	R.AddToTranslation(FVector(0.f, 0.f, 14.f * FMath::Sin(PI * K)));
	return R;
}

void AGSE::LoaderHand(FTractor& L, int32 Id, uint8 From)
{
	ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D || Id == INDEX_NONE) return;
	L.LYouA = D->BagTransform(Id).GetRelativeTransform(LbRoot(L)); L.LYouA.SetScale3D(FVector::OneVector);
	if (From == 4) L.LYouA = FTransform(FVector(95.f, 0.f, 0.f)) * BeltOffset(L, LbLen);
	L.LYouBag = Id; L.LYouFrom = From; L.LYouTo = 0; L.LYouMov = 0.f; L.bUsed = true;
}

void AGSE::LoaderLay(FTractor& L, uint8 To)
{
	L.LYouA = LoaderBagAt(L); L.LYouTo = To; L.LYouMov = 0.f;
	if (To == 1) L.bBottomHeld = true;
	if (To == 2) SlotTaken.Add(L.LYouCart * 100 + L.LYouSlot);
}

void AGSE::LoaderDrop(FTractor& L)
{
	if (L.IsTrain()) { TrainBagDrop(L); return; }
	// down where it is to go, or back where it came from (its counts as they were)
	ADepartureBelts* D = ADepartureBelts::Get(GetWorld()); if (!D || L.LYouBag == INDEX_NONE) return;
	const int32 Id = L.LYouBag; const uint8 To = L.LYouTo, From = L.LYouFrom; L.LYouBag = INDEX_NONE; L.LYouTo = 0;
	USceneComponent* Root = L.M.IsValid() ? L.M->GetRootComponent() : nullptr;
	const bool bCart = Carts.IsValidIndex(L.LYouCart) && Carts[L.LYouCart].Bags.IsValidIndex(L.LYouSlot) && Carts[L.LYouCart].Bags[L.LYouSlot] == INDEX_NONE && Carts[L.LYouCart].M.IsValid();
	if (To == 1) L.bBottomHeld = false;
	if (To == 2) SlotTaken.Remove(L.LYouCart * 100 + L.LYouSlot);
	FTractor::FBeltBag B; B.Id = Id;
	if (To == 3) { D->RemoveBag(Id); return; }   // (into the hold, as its hold person's)
	if ((To == 2 || (To == 0 && From == 1)) && bCart)
	{
		D->PutOnSlot(Id, Carts[L.LYouCart].M->GetRootComponent(), NAME_None, CartSlot(L.LYouCart, L.LYouSlot));
		Carts[L.LYouCart].Bags[L.LYouSlot] = Id; return;
	}
	if (Root && (To == 1 || (From == 1 && To == 0 && BeltBottomFree(L)))) { B.S = 0.f; B.Stage = 0; L.BeltBags.Add(B); D->PutOnSlot(Id, Root, NAME_None, BeltOffset(L, 0.f)); return; }
	if (Root && (From == 2 || To == 2)) { B.S = 0.f; B.Stage = 4; L.BeltBags.Insert(B, 0); D->PutOnSlot(Id, Root, NAME_None, BeltOffset(L, 0.f)); return; }
	if (Root && From == 3) { B.S = LbLen; B.Stage = 1; L.BeltBags.Insert(B, 0); D->PutOnSlot(Id, Root, NAME_None, BeltOffset(L, LbLen)); return; }
	if (Root && From == 4) { B.S = LbLen; B.Stage = 3; L.BeltBags.Add(B); D->PutOnSlot(Id, Root, NAME_None, BeltOffset(L, LbLen)); return; }
	D->RemoveBag(Id); ADP_WARN(TEXT("GSE: a bag you carried has no place to go back to (%s) - it stays behind"), *L.Name);
}

void AGSE::LoaderBags(int32 Li, float Dt)
{
	FTractor& L = Tractors[Li]; ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
	if (D && L.LYouBag != INDEX_NONE && L.M.IsValid())
	{
		L.LYouMov = FMath::Min(1.f, L.LYouMov + Dt / 0.8f);
		if (L.LYouTo && L.LYouMov >= 1.f) LoaderDrop(L);
		else if (!L.LYouTo && L.LYouFrom >= 3 && !L.YouOn && (L.LAskT += Dt) > 10.f) { L.LAskT = 0.f; LoaderDrop(L); }   // (off the belt with a bag of its top: it lies there again)
		else D->PutOnSlot(L.LYouBag, L.M->GetRootComponent(), NAME_None, LoaderBagAt(L));
	}
	// you in its seat or on its belt, and a flight's bags wait for it: you are asked
	if (!L.Player.IsValid() && !L.YouOn) { if (L.LYouBag == INDEX_NONE) L.LAskT = 0.f; return; }
	if ((L.LAskT += Dt) < 20.f) return;
	L.LAskT = 0.f; FString Why;
	if (LoaderTrain(L) != INDEX_NONE && LoaderWaits(L.StandX, Why)) AVehicleControls::Note(GetWorld(), Why + (L.Player.IsValid() ? TEXT(" - leave it, or bring it up and let its belt run") : TEXT(" - take the bags here, or come down")));
}

bool AGSE::LoaderWaits(float StandX, FString& Why) const
{
	for (const FTractor& T : Tractors)
		if (T.IsTrain() && (T.Job == EJob::None || T.Job == EJob::Boarding) && FMath::Abs(T.YStandX - StandX) < 1.f)
		{
			Why = T.Job == EJob::Boarding ? TEXT("its bags wait: a crew take over the train you left") : T.Player.IsValid() ? TEXT("its bags are on the train you drive")
				: TEXT("its bags are on the train you left: bring it to its place beside the belt loader");
			return true;
		}
	const int32 Li = LoaderAt(StandX); if (Li == INDEX_NONE) return false;
	const FTractor& L = Tractors[Li]; bool bArr = false; if (LoaderTrain(L, &bArr) == INDEX_NONE && (L.bLOver || !(L.LYouBag != INDEX_NONE || L.BeltBags.ContainsByPredicate([](const FTractor::FBeltBag& B) { return B.Stage <= 2; })))) return false;
	const TCHAR* W = bArr ? TEXT("the arriving bags wait") : TEXT("its bags wait");
	const bool bTop = L.ArrivalsLeft > 0 || L.BeltBags.ContainsByPredicate([](const FTractor::FBeltBag& B) { return B.Stage == 1; });
	const bool bBoom = FMath::IsNearlyEqual(L.Boom, L.BoomWant, 0.3f);
	if (L.YouOn && (L.bBelt || bTop)) Why = FString::Printf(TEXT("%s: you are on the belt loader's belt"), W);
	else if (L.Player.IsValid() && (!L.bBelt || bTop || L.bLStop)) Why = FString::Printf(TEXT("%s: you sit in the belt loader"), W);
	else if (!bBoom && LoaderHeld(L, L.BoomWant)) Why = FString::Printf(TEXT("%s: somebody is under the belt loader's boom"), W);
	else if (bTop && !L.bTopUp) Why = FString::Printf(TEXT("%s: nobody is up at the bulk hold yet"), W);
	else if (L.LYouBag != INDEX_NONE) Why = FString::Printf(TEXT("%s: one of them is in your hands"), W);
	return !Why.IsEmpty();
}
