// Vehicles owned (to-do 318): one list of every airport vehicle you have, read from the live vehicles - see VehiclesOwned.h.
#include "VehiclesOwned.h"
#include "GSE.h"
#include "Tugs.h"
#include "FireTruck.h"
#include "FireStation.h"
#include "Airfield.h"
#include "ServiceVehicles.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSimData.h"
#include "Sim/ADPVehicleArea.h"
#include "BabylonModel.h"
#include "PerfLog.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	// (the vehicle area's kind of a service vehicle line: the same riders' points and roof - as GSESpare.cpp delivers them)
	const ADPVehicleArea::FKind* OwnAreaKind(int32 Svc)
	{
		if (!ADPService::All().IsValidIndex(Svc)) return nullptr;
		const FServiceSpec& S = ADPService::All()[Svc];
		return ADPVehicleArea::Kinds().FindByPredicate([&](const ADPVehicleArea::FKind& K) { return S.Points && K.Points == S.Points && K.bRoof == S.bRoof; });
	}
	// (the kinds in the order of the list: what serves the aircraft first, then the tugs, the fire trucks)
	int32 OwnOrder(const FString& Kind)
	{
		static const TCHAR* Order[] = {TEXT("pushback tug"), TEXT("hangar tug"), TEXT("baggage tractor"), TEXT("belt loader"), TEXT("refueller"), TEXT("catering truck"), TEXT("cleaning van"),
			TEXT("water truck"), TEXT("toilet truck"), TEXT("passenger stairs"), TEXT("roofed passenger stairs"), TEXT("apron bus"), TEXT("ambulift"), TEXT("de-icing truck"), TEXT("maintenance van"), TEXT("fire truck")};
		for (int32 i = 0; i < UE_ARRAY_COUNT(Order); ++i) if (Kind == Order[i]) return i;
		return 100;
	}
	FVector2f OwnPlanOf(const FVector& W) { return FVector2f(float(W.X / 100.0), float(-W.Y / 100.0)); }
}

FString ADPOwned::PlaceOf(UWorld* W, const FVector& At, const FVector& Home)
{
	if (FVector::Dist2D(At, Home) < 300.f) return TEXT("on its own place");
	if (ADPVehicleArea::Built() && At.X > -30000. && At.X < -24900. && At.Y < -24000. && At.Y > -31000.) return TEXT("at the vehicle area");
	const FVector2f P = OwnPlanOf(At); const FAirfieldData& D = AAirfield::Shared();
	if (FVector2f::Distance(P, FVector2f(ADPFireStation::X, ADPFireStation::Y)) < 45.f) return TEXT("at the fire station");
	const FAirfieldData::FStand* Best = nullptr; float Bd = 45.f;
	for (const FAirfieldData::FStand& S : D.Stands) { const float Dd = FVector2f::Distance(P, FVector2f(S.X, S.Nose)); if (Dd < Bd) { Bd = Dd; Best = &S; } }
	if (Best) return FString::Printf(TEXT("at stand %s"), *Best->Id);
	for (const FAirfieldData::FHangarSite& H : D.Hangars) if (FVector2f::Distance(P, FVector2f(H.X, H.Y)) < 70.f) return FString::Printf(TEXT("at hangar %s"), *H.Id);
	if (FMath::Abs(P.Y - D.RwyY) < 107.5f && P.X > D.RwyX0 && P.X < D.RwyX1) return TEXT("on the runway strip");
	return TEXT("on the apron");
}

void AGSE::OwnedList(TArray<FOwnedVehicle>& Out) const
{
	const FString Me = UADPSim::Me();
	for (const FTractor& T : Tractors)
	{
		if (T.TugIx != INDEX_NONE || !T.M.IsValid()) continue;   // (a pushback tug's seat line: the tug is ATugs')
		if (T.bSpare && T.Owner != Me) continue;                 // (another airline's, at the vehicle area)
		if (T.bAway && T.Brk != 4) continue;                     // (a bought one not shown: SpareTick)
		FOwnedVehicle V; V.Name = T.Name; V.At = T.M->GetActorLocation(); V.Home = T.Home; V.bBought = T.bSpare;
		if (T.Svc != INDEX_NONE)
		{
			const ADPVehicleArea::FKind* K = OwnAreaKind(T.Svc);
			V.Kind = K ? K->Label : FString(ADPService::All()[T.Svc].Label ? ADPService::All()[T.Svc].Label : TEXT("service vehicle"));
			if (K) V.BuyKey = K->Key;
		}
		else V.Kind = T.bLoader ? TEXT("belt loader") : TEXT("baggage tractor");
		V.Condition = BreakCondition(T.BrkHours, T.BrkKm);
		const bool bWork = T.Job != EJob::None || T.Path.Num() || T.Driver != INDEX_NONE;
		switch (T.Brk)
		{
		case 1: V.State = FString::Printf(TEXT("breaking down: %s"), *T.BrkFault); V.bBroken = true; break;
		case 2: V.State = T.bBrkFix ? FString::Printf(TEXT("broken down (%s), being mended"), *T.BrkFault) : FString::Printf(TEXT("broken down (%s)"), *T.BrkFault); V.bBroken = true; break;
		case 3: V.State = FString::Printf(TEXT("written off (%s)"), *T.BrkFault); V.bWrittenOff = true; V.Condition = 0; break;
		case 4: V.State = TEXT("written off, taken away"); V.bWrittenOff = true; V.Condition = 0; break;
		case 5: V.State = TEXT("mended, its driver getting in"); V.bBroken = true; break;
		default:
			V.State = T.Player.IsValid() ? TEXT("you are driving it") : bWork ? (T.Key.IsEmpty() ? FString(TEXT("at work")) : FString::Printf(TEXT("at work (%s)"), *T.Key)) : FString(TEXT("parked"));
			V.bWork = bWork || T.Player.IsValid();
		}
		V.Place = T.Brk == 4 ? FString(TEXT("gone")) : DepotAt(V.At) ? FString(TEXT("at the vehicle area")) : ADPOwned::PlaceOf(GetWorld(), V.At, V.Home);
		Out.Add(MoveTemp(V));
	}
	// (bought, not yet on its place: delivered when nobody sees the place - GSESpare.cpp)
	if (const UADPSim* Sim = UADPSim::Get(this))
		if (const FADPAirline* A = Sim->World.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Me; }))
			for (const FADPSpare& S : A->Spares)
			{
				const FString Id = S.Kind + TEXT("@") + S.Place;
				if (Tractors.ContainsByPredicate([&](const FTractor& T) { return T.bSpare && T.Owner == Me && T.SpareId == Id && (!T.bAway || T.Brk == 4); })) continue;
				const ADPVehicleArea::FKind* K = ADPVehicleArea::Kind(S.Kind);
				FOwnedVehicle V; V.Kind = K ? K->Label : S.Kind; V.BuyKey = K ? K->Key : FString(); V.Name = FString::Printf(TEXT("new %s"), *V.Kind); V.Condition = 100; V.bBought = true;
				V.State = TEXT("being delivered"); V.Place = FString::Printf(TEXT("to place %s at the vehicle area"), *S.Place);
				Out.Add(MoveTemp(V));
			}
}

void ATugs::OwnedList(TArray<FOwnedVehicle>& Out) const
{
	int32 Np = 0, Nh = 0;
	for (const FTug& T : Tugs)
	{
		if (!T.M.IsValid()) continue;
		FOwnedVehicle V; V.Kind = T.bHangar ? TEXT("hangar tug") : TEXT("pushback tug");
		V.Name = FString::Printf(TEXT("%s %d"), *V.Kind, T.bHangar ? ++Nh : ++Np);
		V.At = T.M->GetActorLocation(); V.Home = T.Home;
		V.State = T.State == EState::Parked ? FString(TEXT("parked")) : T.State == EState::You ? FString(TEXT("you are driving it"))
			: T.bTow ? FString(TEXT("at work (a hangar tow)")) : T.Key.IsEmpty() ? FString(TEXT("at work")) : FString::Printf(TEXT("at work (%s)"), *T.Key);
		V.bWork = T.State != EState::Parked;
		V.Place = ADPOwned::PlaceOf(GetWorld(), V.At, V.Home);
		Out.Add(MoveTemp(V));
	}
}

void ADPOwned::List(UWorld* W, TArray<FOwnedVehicle>& Out)
{
	Out.Reset(); if (!W) return;
	ADP_PERF_SCOPE("Vehicles owned: list", 2.f);
	if (const AGSE* G = AGSE::Find(W)) G->OwnedList(Out);
	if (const ATugs* Tu = ATugs::Get(W)) Tu->OwnedList(Out);
	int32 Nf = 0;
	for (TActorIterator<AFireTruck> It(W); It; ++It)
	{
		FOwnedVehicle V; V.Kind = TEXT("fire truck"); V.Name = FString::Printf(TEXT("fire truck %d"), ++Nf); V.At = It->GetActorLocation(); V.Home = V.At + FVector(1e6);
		V.bWork = It->IsPlayerControlled(); V.State = V.bWork ? TEXT("you are driving it") : TEXT("ready");
		V.Place = PlaceOf(W, V.At, V.Home);
		Out.Add(MoveTemp(V));
	}
	Out.StableSort([](const FOwnedVehicle& A, const FOwnedVehicle& B) { const int32 a = OwnOrder(A.Kind), b = OwnOrder(B.Kind); return a != b ? a < b : A.Kind < B.Kind; });
}
