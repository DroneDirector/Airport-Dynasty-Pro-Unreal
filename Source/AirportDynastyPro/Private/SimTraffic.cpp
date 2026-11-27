// The simulation's flights at our airport in 3D - see SimTraffic.h.
#include "SimTraffic.h"
#include "PaxFlow.h"
#include "Tugs.h"
#include "HangarTow.h"
#include "Turnaround.h"
#include "GSE.h"
#include "ParkedAircraft.h"
#include "JetwayRigs.h"
#include "ADPLog.h"
#include "ADPGameMode.h"
#include "PerfLog.h"
#include "BabylonModel.h"
#include "AircraftLights.h"
#include "TerminalPlan.h"
#include "WindSocks.h"
#include "Site.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSchedule.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPWorldData.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr float KtMs = 0.5144f;
	constexpr float Wheelbase = 12.64f;        // A320: nose gear to main gear
	constexpr float MaxTaxi = 25.f * KtMs, TurnTaxi = 10.f * KtMs, StandTaxi = 5.f * KtMs, PushSpeed = 3.f * KtMs;
	constexpr float Vr = 140.f * KtMs, Vapp = 135.f * KtMs, Glide = 3.f;
	float Heading(const FVector2f& From, const FVector2f& To) { return FMath::Atan2(To.Y - From.Y, To.X - From.X); }
}

ASimTraffic::ASimTraffic()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ASimTraffic::Start(UWorld* W, const FAirfieldData& Field)
{
	if (!W || !FPackageName::DoesPackageExist(TEXT("/Game/Babylon/A320/a320/StaticMeshes/A320"))) return;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ASimTraffic* T = W->SpawnActor<ASimTraffic>(FVector::ZeroVector, FRotator::ZeroRotator, P))
	{
		T->F = Field; ADPParked::Coming.Reset();
		TWeakObjectPtr<ASimTraffic> Tw(T);
		ADPSchedule::Ready3D = [Tw](const FADPPlanned& Pl, const FDateTime& Now, FString& Why) { return !Tw.IsValid() || Tw->Ready(Pl, Now, Why); };
	}
}

void ASimTraffic::EndPlay(const EEndPlayReason::Type Why)
{
	ADPSchedule::Ready3D = nullptr; ADPParked::Coming.Reset();
	Super::EndPlay(Why);
}

namespace
{
	// the jetway serving a stand (owner 2026-11-21: "the jetway has to ... leave the airplane and get into its resting
	// position before pushback can commence ... in the right order"): the one whose docked point is nearest the stand's nose
	int32 JetwayAt(UWorld* W, const FAirfieldData::FStand& S)
	{
		if (!S.bJetway) return INDEX_NONE;
		AJetwayRigs* R = AJetwayRigs::Get(W); if (!R) return INDEX_NONE;
		const FVector Nose = FTerminalPlan::ToWorld(S.X, AAirfield::GroundAt(S.X, S.Nose), S.Nose);
		int32 Best = INDEX_NONE; float Bd = 3000.f;
		for (int32 j = 0; j < R->Num(); ++j) { FVector Ro, Dr; if (R->Frame(j, Ro, Dr) && FVector::Dist2D(Dr, Nose) < Bd) { Bd = FVector::Dist2D(Dr, Nose); Best = j; } }
		return Best;
	}
}

namespace
{
	int32 JetwayOf(UWorld* W, const FString& Stand)
	{
		for (const FAirfieldData::FStand& S : AAirfield::Shared().Stands) if (S.Id == Stand) return JetwayAt(W, S);
		return INDEX_NONE;
	}
}

void ASimTraffic::DockAt(UWorld* W, const FString& Stand)
{
	const int32 J = JetwayOf(W, Stand); AJetwayRigs* R = AJetwayRigs::Get(W);
	if (J != INDEX_NONE && R && R->State(J) == AJetwayRigs::EState::Parked && R->AircraftAt(J) && R->Dock(J)) ADP_LOG(TEXT("Traffic: jetway %s docking at %s"), *R->Name(J), *Stand);
}

void ASimTraffic::UndockAt(UWorld* W, const FString& Stand)
{
	const int32 J = JetwayOf(W, Stand); AJetwayRigs* R = AJetwayRigs::Get(W);
	if (J != INDEX_NONE && R && (R->State(J) == AJetwayRigs::EState::Docked || R->State(J) == AJetwayRigs::EState::Manual)) { R->Undock(J); ADP_LOG(TEXT("Traffic: doors closed at %s - jetway %s pulling back"), *Stand, *R->Name(J)); }
}

bool ASimTraffic::JetwayReady(UWorld* W, const FString& Stand, bool bDocked)
{
	const int32 J = JetwayOf(W, Stand); AJetwayRigs* R = AJetwayRigs::Get(W);
	if (J == INDEX_NONE || !R) return true;
	const AJetwayRigs::EState S = R->State(J);
	if (bDocked) { if (S == AJetwayRigs::EState::Parked) DockAt(W, Stand); return S == AJetwayRigs::EState::Docked; }
	if (S == AJetwayRigs::EState::Docked || S == AJetwayRigs::EState::Manual) UndockAt(W, Stand);
	return S == AJetwayRigs::EState::Parked;
}

void ASimTraffic::Scenes(UWorld* W, TArray<APaxFlow::FScene>& Out)
{
	for (TActorIterator<ASimTraffic> It(W); It; ++It)
		for (const FMove& M : It->Moves)
		{
			const ABabylonModel* B = M.Model.Get(); if (!B || M.Phase == EPhase::Done || M.Height > 400.f) continue;
			static const TCHAR* Words[] = {TEXT("pushback"), TEXT("taxiing"), TEXT("take-off"), TEXT("climbing"), TEXT("landing"), TEXT("landed"), TEXT("taxiing in"), TEXT("on blocks")};
			APaxFlow::FScene S; S.Id = TEXT("MOVE ") + M.Reg; S.Kind = 20 + int32(M.Phase); S.At = B->GetActorLocation(); const FVector2f D = (M.Nose - M.Main).GetSafeNormal(); S.Face = FVector(D.X, -D.Y, 0.f); S.Who = B;
			S.At -= S.Face * 1200.f;   // (its middle: the actor is at the nose wheel)
			S.Line = FString::Printf(TEXT("%s    %s"), *M.Reg, Words[FMath::Min(int32(M.Phase), 7)]);
			Out.Add(S);
		}
}

bool ASimTraffic::Airborne(UWorld* W, const FString& Reg)
{
	const UADPSim* Sim = UADPSim::Get(W);
	if (!Sim || !Sim->World.Flights.ContainsByPredicate([&](const FADPFlight& X) { return X.Reg == Reg; })) return false;
	for (TActorIterator<ASimTraffic> It(W); It; ++It) for (const FMove& M : It->Moves) if (M.Reg == Reg && M.bDeparture && M.Phase != EPhase::Climb) return false;
	return true;
}

bool ASimTraffic::Moving(const UWorld* W, const FString& Reg) { return ADPParked::Coming.Contains(Reg); }

bool ASimTraffic::RunwayBusy(const UWorld* W)
{
	if (!W) return false;
	for (TActorIterator<ASimTraffic> It(const_cast<UWorld*>(W)); It; ++It)
		for (const FMove& M : It->Moves)
		{
			if (M.Phase == EPhase::Roll || M.Phase == EPhase::Rollout || (M.Phase == EPhase::Approach && M.Height < 400.f) || (M.Phase == EPhase::Climb && M.Height < 100.f)) return true;
			if ((M.Phase == EPhase::Taxi || M.Phase == EPhase::TaxiIn) && (FMath::Abs(M.Nose.Y - It->F.RwyY) < 100.f || FMath::Abs(M.Main.Y - It->F.RwyY) < 100.f)) return true;   // (lined up, or crossing / vacating)
		}
	return false;
}

FString ASimTraffic::RunwayTraffic(const UWorld* W)
{
	if (!W) return FString();
	for (TActorIterator<ASimTraffic> It(const_cast<UWorld*>(W)); It; ++It)
		for (const FMove& M : It->Moves)
		{
			if (M.Phase == EPhase::Rollout || (M.Phase == EPhase::Approach && M.Height < 400.f)) return TEXT("traffic landing");
			if (M.Phase == EPhase::Roll || (M.Phase == EPhase::Climb && M.Height < 100.f)) return TEXT("traffic departing");
			if ((M.Phase == EPhase::Taxi || M.Phase == EPhase::TaxiIn) && (FMath::Abs(M.Nose.Y - It->F.RwyY) < 100.f || FMath::Abs(M.Main.Y - It->F.RwyY) < 100.f)) return TEXT("traffic on the runway");
		}
	return FString();
}

FString ASimTraffic::RunwaySpoken(const UWorld* W)
{
	// the end in use from the real wind (RunwayToX1), its number from the site's runway heading (as the pushback clearance)
	bool bToX1 = true;
	if (W) for (TActorIterator<ASimTraffic> It(const_cast<UWorld*>(W)); It; ++It) { bToX1 = It->RunwayToX1(); break; }
	double Hdg = 336.0; if (TSharedPtr<FJsonObject> Ab = ADPSite::AirportBlock()) Ab->TryGetNumberField(TEXT("hdg"), Hdg);
	const double H = FMath::Fmod(bToX1 ? Hdg : Hdg + 180.0, 360.0);
	const int32 Rw = FMath::RoundToInt(H / 10.0) == 0 ? 36 : FMath::RoundToInt(H / 10.0);
	static const TCHAR* Dg[] = {TEXT("zero"), TEXT("one"), TEXT("two"), TEXT("three"), TEXT("four"), TEXT("five"), TEXT("six"), TEXT("seven"), TEXT("eight"), TEXT("niner")};
	return FString::Printf(TEXT("%s %s"), Dg[Rw / 10], Dg[Rw % 10]);
}

bool ASimTraffic::Ready(const FADPPlanned& P, const FDateTime& Now, FString& Why) const
{
	UWorld* W = GetWorld();
	if (AHangarTow::Busy(W, P.Reg)) { Why = TEXT("it is being towed to or from the hangar"); return false; }
	if (!APaxFlow::BoardingDone(W, P.Reg)) { Why = TEXT("not everyone is aboard yet"); return false; }
	const ADPTurnaround::FTurn* T = ADPTurnaround::All().FindByPredicate([&](const ADPTurnaround::FTurn& X) { return X.Reg == P.Reg; });
	if (!T) return true;   // (a remote stand: its work is data)
	ON_SCOPE_EXIT { if (!Why.IsEmpty()) if (ATugs* Tw = ATugs::Get(W)) Tw->YouSayWhy(T->StandX, Why); };   // (shown to you at the wheel of the stand's tug)
	if (T->PlanId != P.Id) { Why = TEXT("being got ready"); return false; }
	if (!ADPTurnaround::Done(*T, TEXT("deplane"))) { Why = TEXT("the last flight's passengers are still getting off"); return false; }
	if (!ADPTurnaround::Done(*T, TEXT("board"))) { Why = TEXT("boarding"); return false; }
	if (P.AirlineId == UADPSim::Me()) if (const AGSE* G = AGSE::Find(W)) if (G->HiredWaits(P.FlightNo, T->StandX, Why)) return false;   // (to-do 206: your people at the bags)
	if (const AGSE* G = AGSE::Find(W)) if (G->BusyAt(T->StandX)) { if (!G->LoaderWaits(T->StandX, Why)) Why = TEXT("the bags are still being loaded"); return false; }
	if (!ADPTurnaround::Done(*T, TEXT("doors"))) { Why = TEXT("the doors are being closed, the jetway pulled back"); return false; }
	if (const ATugs* Tg = ATugs::Get(W)) if (Tg->YouHave(T->StandX, Why)) return false;   // (to-do 238: you have the stand's tug)
	// (to-do 206: your own pushback crew, the people you hired - nobody free: the flight waits)
	if (P.AirlineId == UADPSim::Me()) if (ATugs* Tg = ATugs::Get(W)) if (!Tg->CrewReady(P.Id, T->StandX, Why)) return false;
	(void)Now;
	return true;
}

// The way an aircraft taxis off its stand's taxilane (+1: the plan's +x): where its push ends facing, and its first leg.
// A cargo stand: along K to the link at the runway end's side; every other: along T to the connector it takes up to A.
static float TaxiOutX(bool bToX1) { return bToX1 ? -130.f : 362.f; }   // (P1, P3)
static float TaxiOutDir(const FAirfieldData::FStand& St, bool bToX1) { return St.bCargo ? (bToX1 ? -1.f : 1.f) : TaxiOutX(bToX1) >= St.X ? 1.f : -1.f; }

// The push's turn: 40 m for the main gear, less where that would end past its taxilane's end (stand 9 by T's end: the gear
// stops 3 m before it). 22 m still is only 30 degrees of nose wheel, far within what a tug may turn.
static float PushRadius(const FAirfieldData& F, const FAirfieldData::FStand& St, float Dx)
{
	const FAirfieldData::FTwy* L = F.Find(St.bCargo ? TEXT("K") : TEXT("T"));
	if (!L || L->Pts.Num() < 2) return 40.f;
	const float End = Dx > 0.f ? FMath::Min(L->Pts[0].X, L->Pts.Last().X) : FMath::Max(L->Pts[0].X, L->Pts.Last().X);
	return FMath::Clamp((St.X - End) * Dx - 3.f, 22.f, 40.f);
}

bool ASimTraffic::RunwayToX1() const
{
	// the runway's x0 -> x1 direction has the plan's x axis, true heading = the site's runway heading; into the wind
	double Hdg = 336.0; if (TSharedPtr<FJsonObject> A = ADPSite::AirportBlock()) A->TryGetNumberField(TEXT("hdg"), Hdg);
	const float Head = FMath::Cos(FMath::DegreesToRadians(AWindSocks::WindFrom - float(Hdg)));
	return Head >= -0.05f;   // (calm or a headwind on x0 -> x1: that way)
}

void ASimTraffic::Sync()
{
	UADPSim* Sim = UADPSim::Get(this);
	if (!Sim || !Sim->IsRunning()) return;
	// the whole ground fleet is placed in one frame (seconds, a minute with a cold editor cache): not while the roads and the land
	// are still being built under the loading screen - that long frame timed out the web game page that was still connecting
	// (no career saved, Continue had nothing to load; 2026-11-24) - only 5 s after the late world is in
	if (!ADPLateWorld::Started() || ADPLateWorld::Done())
	{
		const double TNow = FPlatformTime::Seconds();
		if (FleetAt <= 0.0) FleetAt = TNow + (ADPLateWorld::Started() ? 5.0 : 0.0);
		if (TNow >= FleetAt && !AGSE::Find(GetWorld()))
		{
			ADP_LOG(TEXT("Traffic: the ground fleet goes to its places now (the late world is in)"));
		}
		if (TNow >= FleetAt) AHangarTow::Get(GetWorld());   // (the hangar tows, to-do 311)
		if (TNow >= FleetAt) ATugs::Get(GetWorld());   // (the ground crew, the tugs, the tractors and the ground power at their places from the start, not only once a flight calls for them)
	}
	const FString Here = ADPBoards::HereIcao();
	const FDateTime Now = Sim->Now();
	const bool bToX1 = RunwayToX1();
	const float RY = F.RwyY, X0 = F.RwyX0, X1 = F.RwyX1;
	const float Hold = 107.5f;   // (the holding point's distance from the runway's centre line)
	for (const FADPFlight& Fl : Sim->World.Flights)
	{
		const FDateTime OnBlocks3 = Fl.DepAt + FTimespan::FromMinutes(Fl.DurMin + Fl.HoldMin);
		// 3 minutes before on-blocks: the gate's tug driver sits in his tug, ready (owner 2026-11-21)
		if (Fl.To == Here && Fl.Diverted.IsEmpty() && !TugCalled.Contains(Fl.Id) && Sim->World.Aircraft.ContainsByPredicate([&](const FADPAircraft& X) { return X.Reg == Fl.Reg && !X.Stand.IsEmpty(); }) && (OnBlocks3 - Now).GetTotalMinutes() <= 3.0 && (OnBlocks3 - Now).GetTotalMinutes() > -1.0 && (Fl.AirlineId != UADPSim::Me() || ADPSchedule::OnShift(Sim->World, TEXT("ground"), Now) >= 2))
		{
			TugCalled.Add(Fl.Id);
			const FADPAircraft* A3 = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Fl.Reg; });
			for (const FAirfieldData::FStand& S : F.Stands) if (A3 && S.Id == A3->Stand) { if (ATugs* T = ATugs::Get(GetWorld())) T->Arriving(S.X, Fl.AirlineId == UADPSim::Me()); break; }
		}
		if (Seen.Contains(Fl.Id)) continue;
		const FADPAircraft* A = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Fl.Reg; });
		// (the tug, the jetway and the bags of a departure: its turnaround - Turnaround.cpp - before it is off blocks)
		// departures from here: from off-blocks, while it can still be on the ground or climbing (12 minutes)
		if (Fl.From == Here && (Now - Fl.DepAt).GetTotalMinutes() >= 0.0 && (Now - Fl.DepAt).GetTotalMinutes() < 12.0)
		{
			Seen.Add(Fl.Id);
			const FAirfieldData::FStand* St = nullptr;
			// (it was parked at a stand: the one ParkedAircraft showed it at - the simulation's own record)
			{
				bool bKnown = false;
				for (const FAirfieldData::FStand& S : F.Stands) if (S.Id == Fl.FromStand || (Fl.FromStand.IsEmpty() && A && S.Id == A->Stand)) { St = &S; bKnown = true; }
				// (no stand on record: the free one it would be given - never one another aircraft is on; two on stand 9 in the test of 2026-11-21)
				if (!bKnown && A)
				{
					TSet<FString> Busy; for (const FMove& O : Moves) if (!O.Stand.IsEmpty() && (O.Phase == EPhase::Push || O.Phase == EPhase::Done)) Busy.Add(O.Stand);
					const FString Id = ADPGround::PickStand(Sim->World, *A, Now, true, nullptr, &Busy);
					St = F.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == Id; });
				}
			}
			if (!St) { ADP_WARN(TEXT("Traffic: %s leaves unseen - no stand on record and none free"), *Fl.Reg); continue; }
			FMove M; M.FlightId = Fl.Id; M.Reg = Fl.Reg; M.bDeparture = true; M.Stand = St->Id; M.Phase = EPhase::Push;
			// at the stand: nose tyre on the stop bar, the body behind it, pushed straight back onto the taxilane
			M.Nose = St->Frame().P(0.f, 0.f); M.Main = St->Frame().P(0.f, Wheelbase);
			// pushed back and turned (as a real pushback): straight back from the stand, then the tail swung round a 40 m arc (PushRadius) so the
			// aircraft ends on the taxilane facing the way it will taxi (towards the runway end in use) - the main gear's way
			{
				const float Dx = TaxiOutDir(*St, bToX1), Rr = PushRadius(F, *St, Dx), YEnd = St->bCargo ? F.KY : F.TY, Y0 = M.Main.Y;   // (a cargo stand: onto K, facing the way it taxis along it)
				const float Ybend = YEnd - (-St->Dir) * Rr;           // where the straight part ends and the arc begins
				M.Path.Reset();
				if ((Ybend - Y0) * (-St->Dir) > 1.f) M.Path.Add(FVector2f(St->X, Ybend));
				const FVector2f C(St->X - Dx * Rr, Ybend);              // the arc's middle: beside the bend, on the side the tail swings to
				for (int32 k = 1; k <= 8; ++k)
				{
					const float Ang = HALF_PI * k / 8.f;                // 0: going straight back, 90 deg: along the taxilane
					M.Path.Add(C + FVector2f(Dx * Rr * FMath::Cos(Ang), (-St->Dir) * Rr * FMath::Sin(Ang)));
				}
			}
			Moves.Add(M);
			ADPParked::Release(Fl.Reg);   // (the parked model gives way to the moving one, in the same frame)
		}
		// arrivals here: from about 9 minutes before on-blocks (the approach, the landing, the taxi in)
		const FDateTime OnBlocks = Fl.DepAt + FTimespan::FromMinutes(Fl.DurMin + Fl.HoldMin);
		if (Fl.To == Here && Fl.Diverted.IsEmpty() && (OnBlocks - Now).GetTotalMinutes() <= 9.0 && (OnBlocks - Now).GetTotalMinutes() > 0.0)
		{
			Seen.Add(Fl.Id);
			FMove M; M.FlightId = Fl.Id; M.Reg = Fl.Reg; M.bDeparture = false; M.Phase = EPhase::Approach;
			M.Stand = A ? A->Stand : FString();
			// a straight-in 3 degree approach to the touchdown zone (500 m past the threshold), 7 km out
			const float Dir = bToX1 ? 1.f : -1.f, Thr = bToX1 ? X0 : X1, TD = Thr + Dir * 500.f, From = 7000.f;
			M.Nose = FVector2f(TD - Dir * From, RY); M.Main = M.Nose - FVector2f(Dir * Wheelbase, 0.f);
			M.Height = From * FMath::Tan(FMath::DegreesToRadians(Glide)); M.Speed = Vapp;
			M.Path = {FVector2f(TD, RY)};
			Moves.Add(M); ADPParked::Coming.Add(Fl.Reg);
		}
	}
	// models for new movements
	for (FMove& M : Moves)
		if (!M.Model.IsValid())
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABabylonModel* B = GetWorld()->SpawnActor<ABabylonModel>(FVector::ZeroVector, FRotator::ZeroRotator, P);
			if (B && B->Place(TEXT("A320"))) { B->Tags.Add(FName(*(TEXT("MOVE/") + M.Reg))); M.Model = B; AddLights(M); Pose(M); ADP_LOG(TEXT("Traffic: %s %s in 3D (%s)"), *M.Reg, M.bDeparture ? TEXT("departing") : TEXT("arriving"), *M.Stand); }
			else if (B) B->Destroy();
		}
}

void ASimTraffic::Pose(FMove& M)
{
	ABabylonModel* B = M.Model.Get();
	if (!B) return;
	// the actor sits on the nose tyre (as on the stands: ParkedAircraft.cpp), the nose pointing from the main gear to it
	const FVector2f D = (M.Nose - M.Main).GetSafeNormal();
	const FVector World = FTerminalPlan::ToWorld(M.Nose.X, AAirfield::GroundAt(M.Nose.X, M.Nose.Y) + M.Height, M.Nose.Y);
	const FVector Dir(D.X, -D.Y, 0.f);   // (plan (x, y) -> world (x, -y))
	// (2026-11-21: the moving aircraft drew nose-backwards - its nose away from the gate and the tug inside its body; the model is
	//  created unturned here and turned after Place, unlike ParkedAircraft's: the half turn the other way)
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.X, -Dir.Y));
	// the attitude: nose up about 8 degrees in the climb, about 3 on the approach (the model's nose is the actor's -Y, so
	// nose-up is a roll about the actor's X axis); level on the ground
	const float Up = M.Phase == EPhase::Climb ? 8.f : M.Phase == EPhase::Approach ? 3.f : (M.Phase == EPhase::Roll && M.Speed > Vr * 0.97f) ? 6.f : 0.f;
	B->SetActorLocationAndRotation(World, FRotator(0.f, Yaw, Up));
}

bool ASimTraffic::Advance(FMove& M, float Dt)
{
	const bool bToX1 = RunwayToX1();
	const float Dir = bToX1 ? 1.f : -1.f, RY = F.RwyY, X0 = F.RwyX0, X1 = F.RwyX1, Hold = 107.5f;
	const FAirfieldData::FStand* MySt = M.bDeparture ? F.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == M.Stand; }) : nullptr;
	const bool bCargoSt = MySt && MySt->bCargo; const float LaneY = bCargoSt ? F.KY : F.TY;   // (the taxilane it is pushed onto)
	// the tug (Tugs.h): pushed back only once a tug has driven in and hooked its towbar on the nose gear's tow fitting
	// (60 cm ahead of the nose wheel) - it is posed on the aircraft from then on; taxiing only once it has backed clear
	if (M.Phase == EPhase::Push)
	{
		const FVector2f D2 = (M.Nose - M.Main).GetSafeNormal();
		const FVector Fwd(D2.X, -D2.Y, 0.f);
		const FVector Nose = FTerminalPlan::ToWorld(M.Nose.X, AAirfield::GroundAt(M.Nose.X, M.Nose.Y), M.Nose.Y);
		float StandX = M.Nose.X;
		for (const FAirfieldData::FStand& S : F.Stands) if (S.Id == M.Stand) StandX = S.X;
		// (owner 2026-11-21: everyone aboard first - whatever the jetway's state; a no-show holds it 15 min at most. Nothing
		//  else starts before: no jetway pull-back, no pushback request, no tug)
		// (the cinematic test: the jetway docked at the aircraft from the start, so nothing holds the test up - owner 2026-11-21;
		//  in the game it comes out when the crew board, to-do 202)
		if (FParse::Param(FCommandLine::Get(), TEXT("PaxFlowTest")))
		{
			static TSet<FString> TestDocked;
			if (!TestDocked.Contains(M.FlightId)) for (const FAirfieldData::FStand& S : F.Stands) if (S.Id == M.Stand)
			{
				const int32 J = JetwayAt(GetWorld(), S); AJetwayRigs* JR = AJetwayRigs::Get(GetWorld());
				if (J != INDEX_NONE && JR && JR->AircraftAt(J))
				{
					TestDocked.Add(M.FlightId);
					if (JR->State(J) == AJetwayRigs::EState::Parked) { JR->Dock(J); ADP_LOG(TEXT("Traffic: test - jetway %s docked to %s from the start"), *JR->Name(J), *M.FlightId); }
				}
				break;
			}
		}
		if (!APaxFlow::BoardingDone(GetWorld(), M.Reg)) { if (M.Clock != 4.f) ADP_LOG(TEXT("Traffic: %s waits - boarding not complete"), *M.FlightId); M.Clock = 4.f; M.Speed = 0.f; if (ATugs* Ty = ATugs::Get(GetWorld())) Ty->YouSayWhy(StandX, TEXT("not everyone is aboard yet")); Pose(M); return false; }
		// the jetway first: never a pushback with it at the door - it pulls back and must be at rest before anything moves
		for (const FAirfieldData::FStand& S : F.Stands)
			if (S.Id == M.Stand)
			{
				const int32 J = JetwayAt(GetWorld(), S);
				if (J != INDEX_NONE)
				{
					AJetwayRigs* JR = AJetwayRigs::Get(GetWorld());
					const AJetwayRigs::EState Js = JR->State(J);
					if (Js != AJetwayRigs::EState::Parked)
					{
						// (boarding still going on: the jetway stays at the door and the aircraft waits)
						if ((Js == AJetwayRigs::EState::Docked || Js == AJetwayRigs::EState::Manual) && !APaxFlow::BoardingDone(GetWorld(), M.Reg))
						{ if (M.Clock != 4.f) ADP_LOG(TEXT("Traffic: %s waits - boarding not complete"), *M.FlightId); M.Clock = 4.f; M.Speed = 0.f; Pose(M); return false; }
						if (Js == AJetwayRigs::EState::Docked || Js == AJetwayRigs::EState::Manual) JR->Undock(J);
						if (M.Clock != 2.f) ADP_LOG(TEXT("Traffic: %s waits for jetway %s to reach its rest position"), *M.FlightId, *JR->Name(J));
						if (ATugs* Ty = ATugs::Get(GetWorld())) Ty->YouSayWhy(StandX, TEXT("the jetway is still moving back"));
						M.Clock = 2.f; M.Speed = 0.f; Pose(M); return false;
					}
				}
				break;
			}
		if (M.Clock == 2.f || M.Clock == 4.f) M.Clock = 0.f;
		ATugs* T = ATugs::Get(GetWorld());
		// the words for the talk: its callsign (the registration's last letters, spelt the ICAO way), the way it will face
		// after the push (the runway in use), the runway's number, the route
		if (T && M.Clock != 3.f)
		{
			static const TCHAR* Ph[] = {TEXT("Alpha"), TEXT("Bravo"), TEXT("Charlie"), TEXT("Delta"), TEXT("Echo"), TEXT("Foxtrot"), TEXT("Golf"), TEXT("Hotel"), TEXT("India"), TEXT("Juliett"), TEXT("Kilo"), TEXT("Lima"), TEXT("Mike"), TEXT("November"), TEXT("Oscar"), TEXT("Papa"), TEXT("Quebec"), TEXT("Romeo"), TEXT("Sierra"), TEXT("Tango"), TEXT("Uniform"), TEXT("Victor"), TEXT("Whiskey"), TEXT("X-ray"), TEXT("Yankee"), TEXT("Zulu")};
			static const TCHAR* Dg[] = {TEXT("zero"), TEXT("one"), TEXT("two"), TEXT("three"), TEXT("four"), TEXT("five"), TEXT("six"), TEXT("seven"), TEXT("eight"), TEXT("niner")};
			FString Cs; const FString Tail = M.Reg.Mid(M.Reg.Find(TEXT("-")) + 1).Right(3).ToUpper();
			for (const TCHAR C : Tail) if (C >= 'A' && C <= 'Z') { if (!Cs.IsEmpty()) Cs += TEXT(" "); Cs += Ph[C - 'A']; }
			double Hdg = 336.0; if (TSharedPtr<FJsonObject> Ab = ADPSite::AirportBlock()) Ab->TryGetNumberField(TEXT("hdg"), Hdg);
			const double H = FMath::Fmod(bToX1 ? Hdg : Hdg + 180.0, 360.0);
			const double Face = FMath::Fmod((MySt ? TaxiOutDir(*MySt, bToX1) : Dir) > 0.f ? Hdg : Hdg + 180.0, 360.0);   // (the way it is pushed to face)
			static const TCHAR* Comp[] = {TEXT("north"), TEXT("east"), TEXT("south"), TEXT("west")};
			const int32 Rw = FMath::RoundToInt(H / 10.0) == 0 ? 36 : FMath::RoundToInt(H / 10.0);
			const FString RwW = FString::Printf(TEXT("%s %s"), Dg[Rw / 10], Dg[Rw % 10]);
			T->Clearance(M.FlightId, Cs, Comp[FMath::RoundToInt(Face / 90.0) % 4], TEXT("Tango, Alpha"), RwW);
		}
		if (T && !T->Push(M.FlightId, StandX, Nose + Fwd * 60.f, Fwd)) { if (M.Clock == 0.f) ADP_LOG(TEXT("Traffic: %s at %s waits for its tug"), *M.FlightId, *M.Stand); M.Clock = 1.f; M.Speed = 0.f; Pose(M); return false; }
		M.Clock = 0.f;
		// ground control: no pushback while another aircraft taxis by on the taxilane within 120 m (it waits, hooked on)
		for (const FMove& O : Moves)
			if (&O != &M && O.Phase == EPhase::Taxi && FMath::Abs(O.Nose.X - M.Nose.X) < 120.f && FMath::Abs(O.Nose.Y - LaneY) < 40.f) { M.Speed = 0.f; if (T) T->YouSayWhy(StandX, TEXT("another aircraft taxis by on the taxilane")); Pose(M); return false; }
		// pushed by you (TugYou.cpp): the aircraft follows your bar; on the line and braked it goes on as after a crew's push
		if (T && T->YouPushing(M.FlightId))
		{
			int32 Re = INDEX_NONE; const bool bDone = T->YouDrive(M.FlightId, M.Model.Get(), M.Nose, M.Main, M.Path, Re, M.Speed, M.YouBar, LaneY, MySt ? TaxiOutDir(*MySt, bToX1) : Dir);
			if (AGSE* G = AGSE::Find(GetWorld()); G && !M.bYou) AddTickPrerequisiteActor(G);   // (after the tug: never a frame behind its bar)
			M.bYou = true; M.YouRe = Re; if (M.Speed > 0.f) M.bYouOff = true;
			Pose(M);
			if (!bDone) return false;
			M.bYou = false; M.Step = M.Path.Num(); M.Speed = 0.f;
		}
		else if (M.bYou)
		{
			// (its crew take over: the rest of their path, from where it stands)
			M.bYou = false; M.Speed = 0.f;
			if (M.Path.IsValidIndex(M.YouRe)) M.Step = M.YouRe;
			else if (T)
			{
				int32 To = INDEX_NONE; T->YouRejoin(M.FlightId, M.Nose, M.Main, M.Path, M.YouFwd, To);
				if (M.Path.IsValidIndex(To)) M.Step = To; else M.YouFwd.Reset();
			}
		}
	}
	// ground control: taxiing, an aircraft holds short of a pushback going on within 80 m ahead of it
	if (M.Phase == EPhase::Taxi && M.Path.IsValidIndex(M.Step))
	{
		const FVector2f Ahead = (M.Path[M.Step] - M.Nose).GetSafeNormal();
		for (const FMove& O : Moves)
			if (&O != &M && O.Phase == EPhase::Push)
			{
				const FVector2f R = O.Main - M.Nose;
				if (R.Size() < 80.f && FVector2f::DotProduct(R, Ahead) > 0.f && O.Speed > 0.05f) { M.Speed = FMath::Max(0.f, M.Speed - 2.f * Dt); Pose(M); return false; }   // (only a pushback already moving: one already taxiing goes first, then the push - never both waiting for each other)
			}
	}
	if (M.Phase == EPhase::Taxi && M.Step == 0)
		if (ATugs* T = ATugs::Get(GetWorld())) if (!T->IsClear(M.FlightId)) { M.Speed = 0.f; Pose(M); return false; }
	const float LineUp = bToX1 ? X0 + 20.f : X1 - 20.f;
	// the target speed: by the phase, the turn coming (the heading change over the next 60 m) and the stand
	float Want = MaxTaxi;
	if (M.Phase == EPhase::Push) Want = PushSpeed;
	else if (M.Phase == EPhase::Roll) Want = Vr * 1.1f;
	else if (M.Phase == EPhase::Climb) Want = 160.f * KtMs;
	else if (M.Phase == EPhase::Approach) Want = Vapp;
	else if (M.Phase == EPhase::Rollout) Want = 20.f * KtMs;
	else if (M.Step < M.Path.Num())
	{
		const float H0 = Heading(M.Main, M.Nose);
		float Look = 0.f; FVector2f P = M.Nose; float Turn = 0.f;
		for (int32 i = M.Step; i < M.Path.Num() && Look < 60.f; ++i)
		{
			Look += FVector2f::Distance(P, M.Path[i]);
			Turn = FMath::Max(Turn, FMath::Abs(FMath::FindDeltaAngleRadians(H0, Heading(P, M.Path[i]))));
			P = M.Path[i];
		}
		if (Turn > FMath::DegreesToRadians(20.f)) Want = TurnTaxi;
		if (M.Phase == EPhase::TaxiIn && M.Step == M.Path.Num() - 1 && FVector2f::Distance(M.Nose, M.Path.Last()) < 60.f) Want = StandTaxi;
	}
	const float Acc = M.Phase == EPhase::Roll ? 2.2f : M.Phase == EPhase::Rollout ? -2.f : 0.6f;
	if (M.Phase == EPhase::Rollout) M.Speed = FMath::Max(Want, M.Speed + Acc * Dt);
	else M.Speed = FMath::FInterpConstantTo(M.Speed, Want, Dt, FMath::Abs(Acc) + 0.4f);
	// the nose wheel along the waypoints; the main gear trails it at the wheelbase (backwards when pushed)
	float Left = M.Speed * Dt;
	if (M.Phase == EPhase::Push && M.YouFwd.Num())
	{
		// (taken over from you off the path: pulled forward onto it first)
		while (Left > 0.f && M.YouFwd.Num())
		{
			const FVector2f To = M.YouFwd[0] - M.Nose; const float D = To.Size(), Go = FMath::Min(D, Left);
			if (D > 1e-3f) { M.Nose += To / D * Go; M.Main = M.Nose - (M.Nose - M.Main).GetSafeNormal() * Wheelbase; }
			Left -= Go; if (D <= Go + 1e-3f) M.YouFwd.RemoveAt(0);
		}
		if (!M.YouFwd.Num()) { M.Speed = 0.f; ADP_LOG(TEXT("Traffic: %s is back on its push line at (%.1f, %.1f)"), *M.FlightId, M.Main.X, M.Main.Y); }
		Left = 0.f;
	}
	while (Left > 0.f && M.Step < M.Path.Num())
	{
		const FVector2f To = M.Path[M.Step] - (M.Phase == EPhase::Push ? M.Main : M.Nose);
		const float D = To.Size();
		const FVector2f U = D > 1e-3f ? To / D : FVector2f::ZeroVector;
		const float Go = FMath::Min(D, Left);
		if (M.Phase == EPhase::Push)
		{
			// pushed: the main gear leads backwards along the line, the nose follows it at the wheelbase
			M.Main += U * Go; M.Nose = M.Main + (M.Nose - M.Main).GetSafeNormal() * Wheelbase;
		}
		else { M.Nose += U * Go; M.Main = M.Nose - (M.Nose - M.Main).GetSafeNormal() * Wheelbase; }
		Left -= Go;
		if (D <= Go + 1e-3f) ++M.Step;
	}
	if (M.Phase == EPhase::Climb || M.Phase == EPhase::Approach)
	{
		// the climb (about 8 degrees) and the 3 degree approach down to the touchdown zone
		if (M.Phase == EPhase::Climb) M.Height += M.Speed * Dt * FMath::Tan(FMath::DegreesToRadians(8.f));
		else M.Height = FMath::Max(0.f, FMath::Abs(M.Nose.X - M.Path.Last().X) * FMath::Tan(FMath::DegreesToRadians(Glide)));
	}
	Pose(M);
	if (M.Phase == EPhase::Push)
	{
		const FVector2f D2 = (M.Nose - M.Main).GetSafeNormal(); const FVector Fwd(D2.X, -D2.Y, 0.f);
		float StandX = M.Nose.X; for (const FAirfieldData::FStand& S : F.Stands) if (S.Id == M.Stand) StandX = S.X;
		if (ATugs* T = ATugs::Get(GetWorld())) T->Push(M.FlightId, StandX, FTerminalPlan::ToWorld(M.Nose.X, AAirfield::GroundAt(M.Nose.X, M.Nose.Y), M.Nose.Y) + Fwd * 60.f, Fwd);
	}
	if (M.Step < M.Path.Num()) return false;
	// the next phase: its waypoints
	M.Step = 0; M.Path.Reset();
	switch (M.Phase)
	{
	case EPhase::Push:
		if (ATugs* T = ATugs::Get(GetWorld())) T->Release(M.FlightId);
		// the tug lets go; taxi along T to the connector, up to A, along A to the holding point of the runway end in use
		M.Phase = EPhase::Taxi;
		if (bCargoSt)
		{
			// the cargo apron: along K to its end towards the runway end in use, down to B, along B and round its curve to the holding point
			const FAirfieldData::FTwy* Lk = F.Find(bToX1 ? TEXT("Q0") : TEXT("Q2"));
			const float Lx = Lk && Lk->Pts.Num() ? Lk->Pts[0].X : bToX1 ? -530.f : 620.f;
			M.Path = {FVector2f(M.Nose.X - Dir * 40.f, F.KY), FVector2f(Lx, F.KY), FVector2f(Lx, F.BY)};
			if (const FAirfieldData::FTwy* Cv = F.Find(bToX1 ? TEXT("CURVE-B-B1") : TEXT("CURVE-B-B3")))
				for (int32 i = Cv->Pts.Num() - 1; i >= 0; --i) M.Path.Add(FVector2f(Cv->Pts[i].X, Cv->Pts[i].Y));
			else M.Path.Add(FVector2f(LineUp - Dir * 40.f, F.BY));
			M.Path.Add(FVector2f(LineUp, RY + Hold));
			return false;
		}
		{
			// (straight on first, onto the line, before the turn up the connector; never past it and back)
			const float Px = TaxiOutX(bToX1), Ahead = Px - M.Nose.X, Face = M.Nose.X >= M.Main.X ? 1.f : -1.f;
			if (Ahead * Face < 0.f) ADP_WARN(TEXT("Traffic: %s faces away from its connector after the push"), *M.Reg);
			M.Path = {FVector2f(M.Nose.X + FMath::Sign(Ahead) * FMath::Min(40.f, FMath::Abs(Ahead) * 0.5f), F.TY), FVector2f(Px, F.TY), FVector2f(Px, F.AY)};
		}
		// along A to its end, round the link curve onto A1 (or A3) and down to the holding point - every centre line
		if (const FAirfieldData::FTwy* Cv = F.Find(bToX1 ? TEXT("CURVE-A-A1") : TEXT("CURVE-A-A3")))
			for (int32 i = Cv->Pts.Num() - 1; i >= 0; --i) M.Path.Add(FVector2f(Cv->Pts[i].X, Cv->Pts[i].Y));
		else M.Path.Add(FVector2f(LineUp - Dir * 40.f, F.AY));
		M.Path.Add(FVector2f(LineUp, RY - Hold));
		return false;
	case EPhase::Taxi:
	{
		// holding short until the runway is clear (nothing rolling, landing or on short final) and the line-up clearance
		// a few seconds later - as the tower gives it
		bool bBusy = false;
		for (const FMove& O : Moves) if (&O != &M && (O.Phase == EPhase::Roll || O.Phase == EPhase::Rollout || (O.Phase == EPhase::Approach && O.Height < 200.f))) bBusy = true;
		if (AHangarTow::OnRunway(GetWorld())) bBusy = true;   // (a hangar tow crossing, to-do 311)
		if (bBusy || (M.Clock += Dt) < 20.f) { M.Speed = 0.f; M.Path = {M.Nose}; M.Step = 0; return false; }
		M.Clock = 0.f;
		M.Phase = EPhase::Roll; M.Path = {FVector2f(LineUp, RY), FVector2f(LineUp + Dir * 1700.f, RY)};
		return false;
	}
	case EPhase::Roll:
		M.Phase = EPhase::Climb; M.Path = {FVector2f(LineUp + Dir * 9000.f, RY)};
		return false;
	case EPhase::Climb:
		return true;
	case EPhase::Approach:
		// touched down: the roll-out to the exit
		M.Phase = EPhase::Rollout; M.Height = 0.f; M.Path = {FVector2f(81.f, RY)};
		return false;
	case EPhase::Rollout:
	case EPhase::TaxiIn:
	{
		// off at the exit to A, along A, down to T, along T and nose in to the stand (its stop bar)
		const bool bWaited = M.Phase == EPhase::TaxiIn;
		if (bWaited && (M.Nose.Y < F.AY || M.Nose.Y > F.KY)) return true;   // (on its stand: this side of A, or beyond K)
		M.Phase = EPhase::TaxiIn;
		if (const UADPSim* Sim = UADPSim::Get(GetWorld()); Sim)   // (the one it has now: given or changed since it came into view)
			if (const FADPAircraft* A = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == M.Reg; })) M.Stand = A->Stand;
		const FAirfieldData::FStand* St = F.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == M.Stand; });
		// a freighter leaves the runway the other way, to the cargo apron; one already on an exit never crosses the runway again
		const FADPTypeInfo* Ty = nullptr;
		if (const UADPSim* Sm = UADPSim::Get(GetWorld())) if (const FADPAircraft* Ac = Sm->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == M.Reg; })) Ty = ADPWorldData::Type(Ac->TypeId);
		const bool bNorth = bWaited ? M.Nose.Y > RY : St ? St->bCargo : Ty && Ty->Seats == 0;
		if (St && St->bCargo != bNorth) St = nullptr;
		// those on the exit before it: waiting there, or only just on their way from it
		int32 Before = 0;
		for (const FMove& O : Moves)
			if (&O != &M && O.Phase == EPhase::TaxiIn && ((O.Path.Num() ? O.Path.Last().Y : O.Nose.Y) > RY) == bNorth && (bWaited ? (O.Nose.Y - M.Nose.Y) * (bNorth ? 1.f : -1.f) > 0.f && FVector2f::Distance(O.Nose, M.Nose) < 150.f : O.Path.Num() <= 1)) ++Before;
		if (!St || Before)
		{
			// no stand (to-do 277), or one without a stand before it: clear of the runway on the exit, short of A - never onto another's
			if (!St) M.Stand.Reset();
			if (bWaited) { M.Speed = 0.f; return false; }
			M.Path = {FVector2f(81.f, bNorth ? F.BY - 45.f - 50.f * Before : F.AY + 45.f + 50.f * Before)};
			ADP_LOG(TEXT("Traffic: %s waits on the exit - %s"), *M.FlightId, St ? TEXT("another waits before it") : TEXT("no free stand"));
			return false;
		}
		if (bNorth)
		{
			// along B to the link the departures don't use, up to K, along K and nose in to the stand
			const FAirfieldData::FTwy* Lk = F.Find(bToX1 ? TEXT("Q2") : TEXT("Q1"));
			const float Lx = Lk && Lk->Pts.Num() ? Lk->Pts[0].X : bToX1 ? 620.f : 200.f;
			M.Path = {FVector2f(81.f, F.BY), FVector2f(Lx, F.BY), FVector2f(Lx, F.KY), FVector2f(St->X, F.KY), St->Frame().P(0.f, 0.f)};
			return false;
		}
		M.Path = {FVector2f(81.f, F.AY), FVector2f(362.f, F.AY), FVector2f(362.f, F.TY), FVector2f(St->X, F.TY), St->Frame().P(0.f, 0.f)};   // (off T into the stand, whichever side of T it is on)
		return false;
	}
	default: return true;
	}
}

void ASimTraffic::AddLights(FMove& M)
{
	// its type's lamps (to-do 243: Data/aircraft-lights.json; the same function for every aircraft - AircraftLights.h)
	M.Lamps = UADPAircraftLights::Fit(M.Model.Get(), TEXT("A320"));
}

void ASimTraffic::Blink(FMove& M)
{
	UADPAircraftLights* Lm = M.Lamps.Get();
	if (!Lm) return;
	// the lights by the real procedure (Airbus SOP; owner 2026-11-21: "as they do in the real world ... the real timing"):
	//  - navigation lights: on whenever it is powered; logo lights and the wing / engine scan lights at night
	//  - the red beacons: on from just before the pushback (doors closed, the jetway away, ready to push) until the engines
	//    are shut down on the stand after arriving (a few seconds after it stops)
	//  - taxi and runway turn-off lights: on while it taxis; the taxi light off while it stands still (holding, waiting - not
	//    to dazzle others) and in the final turn onto its stand (below)
	//  - lining up and the take-off roll: landing lights, strobes and the take-off (nose) light on; once airborne the nose
	//    and turn-off lights go off (with the gear); the landing lights stay on below 10 000 ft
	//  - the approach: landing lights and strobes; the nose light on short final; on the runway after landing they stay on
	//    until it turns off; taxiing in: landing lights and strobes off, taxi and turn-off lights on
	//  - on the stand with the engines off: the parked lights (ADPAircraftLights::Parked - beacon off, nav and logo at night
	//    while powered: the APU runs until the parked aircraft takes over)
	const bool bAir = M.Phase == EPhase::Climb || M.Phase == EPhase::Approach;
	const bool bRunway = M.Phase == EPhase::Roll || M.Phase == EPhase::Rollout;
	const bool bStrobe = bAir || bRunway;
	const bool bLanding = bAir ? M.Height < 3048.f : bRunway;
	const bool bTaxiing = (M.Phase == EPhase::Taxi || M.Phase == EPhase::TaxiIn) && M.Speed > 0.3f;
	const bool bNearStand = M.Phase == EPhase::TaxiIn && M.Step >= M.Path.Num() - 1;
	// the final turn onto the stand (owner 2026-11-24): the nose's taxi light goes off once it is lined up on the stand's own
	// heading (airfield.json: its nose towards the stand's Dir), on the lead-in line and closing in on the stop - before it
	// stops; from where it is and where it heads, not from a timer. Before that the light stays on as it taxis
	bool bFinalTurn = false;
	if (M.Phase == EPhase::TaxiIn)
	{
		if (const FAirfieldData::FStand* St = F.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == M.Stand; }))
		{
			const FVector2f Head = (M.Nose - M.Main).GetSafeNormal();
			const FVector2f Stop = St->Frame().P(0.f, 0.f);   // (the nose tyre's stop, as the lead-in path ends: Advance)
			bFinalTurn = FVector2f::DotProduct(Head, St->Frame().Face()) > FMath::Cos(FMath::DegreesToRadians(25.f)) && FMath::Abs(M.Nose.X - St->X) < 4.f && FVector2f::Distance(M.Nose, Stop) < 120.f;
		}
		else bFinalTurn = bNearStand;   // (no stand known: the last leg of its path is the lead-in)
	}
	const bool bTakeoffL = bRunway || (M.Phase == EPhase::Approach && M.Height < 300.f);
	const bool bTurnoff = ((M.Phase == EPhase::Taxi || M.Phase == EPhase::TaxiIn) && !bNearStand) || bRunway;
	const bool bWaitAtGate = M.Phase == EPhase::Push && (M.Clock == 4.f || M.Clock == 2.f);   // (boarding / the jetway: doors still open)
	const bool bEngines = M.Phase != EPhase::Done || M.Clock < 8.f;
	if (!bEngines) { Lm->SetParked(true); return; }
	const bool bNight = ADPAircraftLights::Night(GetWorld());
	FADPLightState S;
	S.bPowered = true;
	S.bBeacon = !bWaitAtGate;
	S.bStrobe = bStrobe;
	S.bNav = true;
	S.bLogo = bNight;
	S.bLanding = bLanding;
	S.bTakeoff = bTakeoffL;
	S.bTaxi = (bTaxiing && !bFinalTurn) || bTakeoffL;   // (the nose switch's T.O position lights the taxi lamp too)
	S.bTurnoff = bTurnoff;
	S.bWingScan = S.bEngineScan = bNight && !bWaitAtGate && (!bAir || M.Height < 3048.f);
	Lm->Set(S);
}

void ASimTraffic::Tick(float Dt)
{
	Super::Tick(Dt);
	ADP_PERF_SCOPE("the simulation's aircraft in 3D (each frame)", 2.f);
	if ((Poll += Dt) >= 2.f) { Poll = 0.f; bTurnDue = true; ADP_PERF_SCOPE("traffic: the flights followed (each 2 s)", 2.f); Sync(); }
	else if (bTurnDue && Poll >= 1.f) { bTurnDue = false; ADP_PERF_SCOPE("traffic: the turnarounds (each 2 s)", 2.f); if (UADPSim* Sm = UADPSim::Get(this)) if (Sm->IsRunning()) ADPTurnaround::Tick(GetWorld(), Sm->Now()); }   // (each aircraft's turnaround: Turnaround.h)
	// (the cinematic test: every jetway at a stand with an aircraft docked from the start - owner 2026-11-21: nothing may
	//  hold the test up. Each stand once; in the game the jetway comes out when the crew board, to-do 202)
	if (Poll == 0.f && FParse::Param(FCommandLine::Get(), TEXT("PaxFlowTest")))
		if (AJetwayRigs* JR = AJetwayRigs::Get(GetWorld()))
		{
			static TSet<int32> TestDocked;
			for (const FAirfieldData::FStand& S : AAirfield::Shared().Stands)
			{
				const int32 J = JetwayAt(GetWorld(), S);
				if (J == INDEX_NONE || TestDocked.Contains(J) || !JR->AircraftAt(J)) continue;
				TestDocked.Add(J);
				if (JR->State(J) == AJetwayRigs::EState::Parked) { JR->Dock(J); ADP_LOG(TEXT("Traffic: test - jetway %s docked at %s from the start"), *JR->Name(J), *S.Id); }
			}
		}
	// the parked aircraft's tags once a second (not a walk over every model each frame - the release's scan check)
	if ((ParkPoll += Dt) >= 1.f)
	{
		ParkPoll = 0.f; ParkedRegs.Reset();
		bool bWaiting = false; for (const FMove& W2 : Moves) bWaiting |= !W2.bDeparture;
		if (bWaiting) for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It) for (const FName& Tg : It->Tags) { const FString S = Tg.ToString(); if (S.StartsWith(TEXT("SIM/"))) ParkedRegs.Add(S.Mid(4)); }
	}
	SlipPoll = SlipPoll <= 0.f ? 5.f : SlipPoll - Dt;
	for (int32 i = Moves.Num() - 1; i >= 0; --i)
	{
		FMove& M = Moves[i];
		if (!M.Model.IsValid()) continue;
		Blink(M);
		Gear(M, FMath::Min(Dt, 0.1f));
		if (M.bDeparture && M.Phase == EPhase::Push && M.Step == 0 && M.Speed <= 0.f && !M.bYouOff && SlipPoll <= 0.f)
			if (UADPSim* Sl = UADPSim::Get(this)) if (FADPFlight* Fs = Sl->World.Flights.FindByPredicate([&](const FADPFlight& X) { return X.Id == M.FlightId; }))
				if (Sl->Now() > Fs->DepAt) { Fs->DelayMin += (Sl->Now() - Fs->DepAt).GetTotalMinutes(); Fs->DepAt = Sl->Now(); }   // (waiting for its tug: off blocks when it is pushed)
		if (Advance(M, FMath::Min(Dt, 0.1f)))
		{
			// an arrival waits at its stand until the parked aircraft (the simulation's, ParkedAircraft.cpp) takes over
			if (!M.bDeparture && M.Clock < 600.f)
			{
				const bool bParked = ParkedRegs.Contains(M.Reg);
				ADPParked::Coming.Remove(M.Reg);   // (on blocks: the parked one takes over as soon as the simulation has it here)
				if (!bParked) { M.Clock += Dt; M.Phase = EPhase::Done; continue; }
			}
			// (an arrival on its stand: the jetway, the ground power and the bags come with its turnaround - Turnaround.cpp)
			ADPParked::Coming.Remove(M.Reg);
			if (ABabylonModel* B = M.Model.Get()) B->Destroy();
			Moves.RemoveAt(i);
		}
	}
}

namespace
{
	// the gear parts' pivots (the model's root, cm: Blender (x, y, z) m -> (x, -y, z) * 100 - Art/A320/make_a320_parts.py)
	const FVector NoseAxle(0.f, 0.f, 45.4f);                  // the nose wheels' axle; the steering axis runs up through it
	const FVector NoseLegAt(0.f, 0.f, 110.6f);
	const FVector MainAxle[2] = {FVector(-387.8f, 1280.3f, 64.6f), FVector(385.9f, 1280.3f, 64.8f)};
	const FVector MainLegAt[2] = {FVector(-405.4f, 1256.5f, 173.1f), FVector(410.8f, 1251.8f, 139.4f)};
	constexpr float NoseR = 45.4f, MainR = 64.6f;             // (tyre radii, cm)
	constexpr float SteerSign = 1.f;                          // (the root's turn sense: checked in the pictures)
	constexpr float ExtMain = 12.f, ExtNose = 8.f;            // (cm the struts hang out in the air - inside their own cylinders)
}

void ASimTraffic::Gear(FMove& M, float Dt)
{
	ABabylonModel* B = M.Model.Get(); if (!B || Dt <= 0.f) return;
	if (!M.bGear)
	{
		M.bGear = true;
		M.GNoseW = B->BindGroup({TEXT("NoseWheels")}, true, FTransform(NoseAxle));
		M.GNoseLeg = B->BindGroup({TEXT("NoseLeg")}, true, FTransform(NoseLegAt));
		M.GMainW[0] = B->BindGroup({TEXT("MainWheels_L")}, true, FTransform(MainAxle[0])); M.GMainW[1] = B->BindGroup({TEXT("MainWheels_R")}, true, FTransform(MainAxle[1]));
		M.GMainLeg[0] = B->BindGroup({TEXT("MainLeg_L")}, true, FTransform(MainLegAt[0])); M.GMainLeg[1] = B->BindGroup({TEXT("MainLeg_R")}, true, FTransform(MainLegAt[1]));
		M.PrevNose = M.Nose; M.PrevMain = M.Main; M.Ext = M.Height > 0.5f ? 1.f : 0.f;
		ADP_LOG(TEXT("Traffic: %s gear parts - nose wheels %d, nose leg %d, main wheels %d/%d, main legs %d/%d"), *M.Reg, M.GNoseW, M.GNoseLeg, M.GMainW[0], M.GMainW[1], M.GMainLeg[0], M.GMainLeg[1]);
	}
	if (M.GNoseW == INDEX_NONE) return;
	const bool bGround = M.Height < 0.05f;
	// the nose wheel's steer: the way the nose wheel actually travels against the fuselage's line (pushed: backwards)
	const FVector2f DN = M.Nose - M.PrevNose, Body = (M.Nose - M.Main).GetSafeNormal();
	const float DistN = DN.Size(), DistM = (M.Main - M.PrevMain).Size();
	float Want = 0.f;
	if (bGround && M.bYou && M.Phase == EPhase::Push) Want = M.YouBar;   // (your push: as the bar stands)
	else if (bGround && DistN > 0.002f)
	{
		FVector2f Trav = DN / DistN; if (FVector2f::DotProduct(Trav, Body) < 0.f) Trav = -Trav;   // (backwards in the push)
		const float A = FMath::RadiansToDegrees(FMath::Atan2(Body.X * Trav.Y - Body.Y * Trav.X, FVector2f::DotProduct(Body, Trav)));
		Want = FMath::Clamp(A, -75.f, 75.f);   // (the A320's tiller: 75 degrees each way)
	}
	else if (bGround) Want = M.Steer;              // (stopped: the wheel stays where it was)
	M.Steer = FMath::FInterpTo(M.Steer, Want, Dt, 3.f);
	// rolling by the distance (in the air they stop: spun up again at touchdown)
	if (bGround) { M.RollN += FMath::RadiansToDegrees(DistN * 100.f / NoseR); M.RollM += FMath::RadiansToDegrees(DistM * 100.f / MainR); }
	// the struts: hanging out in the air, compressing on touchdown with a little settle (a damped spring)
	const float Target = bGround ? 0.f : 1.f;
	M.ExtV += ((Target - M.Ext) * 90.f - M.ExtV * 11.f) * Dt; M.Ext = FMath::Clamp(M.Ext + M.ExtV * Dt, -0.25f, 1.f);
	if (bGround && !M.bGearPrev && M.Height < 0.05f && M.Phase == EPhase::Rollout) ADP_LOG(TEXT("Traffic: %s touchdown - the main gear compresses, the wheels spin up"), *M.Reg);
	M.bGearPrev = bGround;
	M.PrevNose = M.Nose; M.PrevMain = M.Main;
	const float S = SteerSign * M.Steer;
	const FQuat QS(FVector::UpVector, FMath::DegreesToRadians(S));
	B->MoveGroup(M.GNoseW, FTransform(QS * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-M.RollN)), NoseAxle - FVector(0, 0, ExtNose * M.Ext)));
	if (M.GNoseLeg != INDEX_NONE) B->MoveGroup(M.GNoseLeg, FTransform(QS, NoseLegAt - FVector(0, 0, ExtNose * M.Ext)));
	for (int32 s = 0; s < 2; ++s)
	{
		if (M.GMainW[s] != INDEX_NONE) B->MoveGroup(M.GMainW[s], FTransform(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-M.RollM)), MainAxle[s] - FVector(0, 0, ExtMain * M.Ext)));
		if (M.GMainLeg[s] != INDEX_NONE) B->MoveGroup(M.GMainLeg[s], FTransform(MainLegAt[s] - FVector(0, 0, ExtMain * M.Ext)));
	}
	B->FlushMoves();
}
