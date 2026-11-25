// The passengers' trips through the airport, both ways (PaxFlow.h). Plan metres: x east, z north (airside), height up -
// FTerminalPlan::ToWorld(x, height, z); the roads' plan (x, y) is the same frame (y = plan z).
#include "PaxFlow.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "People.h"
#include "Seats.h"
#include "DepartureBelts.h"
#include "BaggageBelts.h"
#include "JetwayRigs.h"
#include "Escalators.h"
#include "RoadNetwork.h"
#include "TerminalDoors.h"
#include "GSE.h"
#include "TerminalPlan.h"
#include "Turnaround.h"
#include "BabylonModel.h"
#include "Site.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSimData.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSchedule.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPWorldData.h"
#include "UI/ADPHUD.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GroomComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Misc/PackageName.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "PaxAnimInstance.h"
#include "PaxNav.h"
#include "PaxProps.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace PaxFlowInternal
{
	// the security lanes' belts (TerminalFittings: the X-ray belt along z at x, the arch 1.675 m east of it at z 35.6)
	const float PXLANES[] = {53.f, 59.2f, 65.3f, 71.5f, 77.7f, 83.8f};
	// the arrivals' claim belts' middles (BaggageBelts.cpp BELTX; all at plan z 31, plates 1.8 m round the ends)
	const float PXCLAIM[] = {-60.8f, -48.15f, -35.5f, -22.85f, -10.2f};
	// the zebra crossings over the forecourt (roads.json AIRPORT-FORECOURT: plan x), when the roads' own list is empty
	const float PXCROSS[] = {48.0f, 74.1f, 100.4f};
	constexpr float PXDOORX = 75.f;          // the main entrance (5 m sliding doors at z -4 and z 0)
	constexpr float PXARRX = -36.5f;         // the arrivals hall's east doors (kerb front z -4, x -39..-34)
	constexpr float PXARRESC = -77.18f;      // the arrivals escalators' down unit (the pair's origin x -78.8, +1.62)
	constexpr float PXARRFOOT = 37.46f;      // ... their foot (plan z), rising north to Floor 2
	const FQuat PXUPRIGHT(FVector::XAxisVector, -HALF_PI);   // a bag lying on its back stood up, its end handle on top
	TWeakObjectPtr<APaxFlow> GPax;
	FDateTime PxEpoch() { return FDateTime(2000, 1, 1); }
	double PxSec(const FDateTime& T) { return (T - PxEpoch()).GetTotalSeconds(); }
}
using namespace PaxFlowInternal;

APaxFlow::APaxFlow()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Rng.Initialize(20261121);
}

bool APaxFlow::BoardingDone(const UWorld* W, const FString& Reg)
{
	APaxFlow* P = Find(W); if (!P || Reg.IsEmpty()) return true;
	bool bAll = true;
	for (FRun& R : P->Runs)
	{
		if (R.bArr || R.Reg != Reg) continue;
		if (R.bOff || R.bGone) continue;
		const int32 Ri = int32(&R - P->Runs.GetData());
		if (R.bNoShowGo)
		{
			// (gone without the missing ones, but never with someone in the jetway: those past the gate's door board first)
			int32 In = 0; for (const FPx& X : P->Pax) if (X.St == EPx::Jetway && X.Run == Ri && !X.bArr && P->PastDoor(X)) ++In;
			if (In)
			{
				bAll = false;
				if (P->RealT - R.SaidAt > 120.0)   // (held for them, by name)
				{
					R.SaidAt = P->RealT; FString L;
					for (const FPx& X : P->Pax) if (X.St == EPx::Jetway && X.Run == Ri && !X.bArr && P->PastDoor(X)) if (const AActor* Xa = X.A.Get()) { const FVector At = P->PlanOf(Xa->GetActorLocation()); L += FString::Printf(TEXT(" %s (%.1f, %.1f) %s%s%s;"), *X.Id, At.X, At.Z, *X.Stage, X.HeldBy.IsEmpty() ? TEXT("") : TEXT(", held by "), *X.HeldBy); }
					ADP_WARN(TEXT("PaxFlow: PROBLEM - %s (%s) held, %d still in the jetway:%s"), *R.Label, *R.Reg, In, *L);
				}
			}
			continue;
		}
		const bool bDone = R.bCalled && R.Spawned >= R.Visible && R.Aboard + R.Home >= R.Spawned;
		if (bDone) continue;
		// (not called yet: no clock - nobody could board. It holds its own aircraft only while that is being got ready for it: a later
		//  flight of the same aircraft must never hold, or be written off by, the one before - 2026-11-23)
		const double Clk = P->Clock(R);
		if (!R.bCalled || Clk < R.CallT + 45.0 + R.BoardSpanS)
		{
			if (!R.bCalled && !R.bTest)
			{
				const ADPTurnaround::FTurn* T = ADPTurnaround::All().FindByPredicate([&](const ADPTurnaround::FTurn& X) { return X.Reg == R.Reg; });
				const UADPSim* Sim = UADPSim::Get(W);
				bool bMine = T ? T->PlanId == R.PlanId : false;
				if (!T && Sim && !Sim->World.Flights.ContainsByPredicate([&](const FADPFlight& X) { return X.Reg == R.Reg; }))
				{ const FADPPlanned* Nx = ADPSchedule::Next(Sim->World, R.Reg, ADPBoards::HereIcao()); bMine = Nx && Nx->Id == R.PlanId; }
				if (!bMine) continue;
				if (Clk > R.DepT && P->RealT - R.SaidAt > 120.0) { R.SaidAt = P->RealT; ADP_LOG(TEXT("PaxFlow: %s (%s) at gate %d is held past its time - boarding not called yet"), *R.Label, *R.Reg, R.Gate); }
			}
			bAll = false; continue;
		}
		// (not yet: the first time the aircraft is ready to go, the clock starts - 15 min for the missing ones)
		if (R.HoldSince < 0.0) { R.HoldSince = P->RealT; ADP_LOG(TEXT("PaxFlow: %s at gate %d is ready to close but %d of %d passengers are not aboard yet - holding the doors (up to 15 min)"), *R.Label, R.Gate, FMath::Max(0, R.Visible - R.Aboard - R.Home), R.Visible); }
		if (P->RealT - R.SaidAt > 120.0)
		{
			// (by name, never silent)
			R.SaidAt = P->RealT; FString L; int32 n = 0;
			for (const FPx& X : P->Pax) if (X.St != EPx::Gone && X.A.IsValid() && X.Run == Ri && !X.bArr && !X.bHome && ++n <= 8) { const FVector At = P->PlanOf(X.A->GetActorLocation()); L += FString::Printf(TEXT("%s%s at (%.1f, %.1f) %s%s"), L.IsEmpty() ? TEXT("") : TEXT("; "), *X.Id, At.X, At.Z, *X.Stage, X.bDoorWait ? TEXT(", at the shut gate door") : TEXT("")); }
			ADP_LOG(TEXT("PaxFlow: %s (%s) held by its boarding for %.0f s - %d of %d aboard, %d missing: %s%s"), *R.Label, *R.Reg, P->RealT - R.HoldSince, R.Aboard, R.Visible, n, *L, n > 8 ? TEXT("; ...") : TEXT(""));
		}
		if (P->RealT - R.HoldSince >= 15.0 * 60.0)
		{
			// (those in the jetway already: ten minutes more, then those not past its halfway point are no-shows like the rest and walk
			//  back out - nobody is let through from where they are stuck (owner 2026-11-23); the flight goes once the jetway is empty)
			int32 In = 0; for (FPx& X : P->Pax) if (X.St == EPx::Jetway && X.Run == Ri && !X.bArr && P->PastDoor(X)) ++In;
			if (In) bAll = false;
			if (In && P->RealT - R.HoldSince < 25.0 * 60.0)
			{
				if (!R.bLateTun) { R.bLateTun = true; ADP_WARN(TEXT("PaxFlow: PROBLEM - %s: 15 min over and %d passengers still in the jetway: it waits for them"), *R.Label, In); }
				continue;
			}
			R.bNoShowGo = true;
			ADP_LOG(TEXT("PaxFlow: %s - %d passenger(s) did not show up after 15 min: doors closed without them (their bags come off the aircraft)"), *R.Label, FMath::Max(0, R.Visible - R.Aboard - R.Home));
			for (const FPx& X : P->Pax) if (X.St != EPx::Gone && X.A.IsValid() && P->Runs.IsValidIndex(X.Run) && &P->Runs[X.Run] == &R && !X.bArr && !X.bHome) { const FVector L = P->PlanOf(X.A->GetActorLocation()); ADP_LOG(TEXT("PaxFlow: left behind - %s at plan (%.1f, %.1f, floor %d): %s"), *X.Id, L.X, L.Z, L.Y > 3.f ? 2 : 1, *X.Stage); }
			if (UADPSim* Sim = UADPSim::Get(W)) if (FADPPlanned* Pl = ADPSchedule::ById(Sim->World, R.PlanId)) Pl->NoShow = FMath::RoundToInt(float(FMath::Max(0, R.Visible - R.Aboard)) / FMath::Max(1, R.Visible) * R.Pax);
			continue;
		}
		bAll = false;
	}
	return bAll;
}

bool APaxFlow::PastDoor(const FPx& P) const
{
	if (!Runs.IsValidIndex(P.Run)) return false;
	const int32 Gi = GateRoom(Runs[P.Run].Gate); const AActor* A = P.A.Get();
	return P.FadeT >= 0.f || (A && Gates.IsValidIndex(Gi) && PlanOf(A->GetActorLocation()).Z > Gates[Gi].Jw.Y);
}

int32 APaxFlow::Inside(int32 Ri) const
{
	const FRun& R = Runs[Ri]; if (!R.bArr) return 0;
	int32 N = FMath::Max(0, R.Visible - R.Spawned);
	for (const FPx& X : Pax) if (X.St != EPx::Gone && X.bArr && X.Run == Ri && X.Do == EDo::ArrIn && X.A.IsValid()) ++N;
	return N;
}

bool APaxFlow::DeplaneDone(const UWorld* W, const FString& Reg, const FDateTime& OnBlocks, int32 ArrPax, FString& Why)
{
	APaxFlow* P = Find(W); if (!P || !P->bReady || ArrPax <= 0) return true;
	const FString Key = TEXT("A:") + Reg + TEXT(":") + FString::Printf(TEXT("%lld"), OnBlocks.GetTicks() / ETimespan::TicksPerMinute);   // (as Poll makes it)
	FRun* R = P->Runs.FindByPredicate([&](const FRun& X) { return X.Key == Key; });
	if (!R)
	{
		// (Poll looks every 5 s: not a run yet; after 12 s nobody walks for it)
		static TMap<FString, double> First; const double F = First.FindOrAdd(Key, P->RealT);
		if (P->RealT - F < 12.0 && P->RealT >= F) { Why = TEXT("its passengers are getting up"); return false; }
		return true;
	}
	const int32 Ri = int32(R - P->Runs.GetData()), N = P->Inside(Ri);
	if (!N || R->bNoShowGo) return true;
	Why = FString::Printf(TEXT("%d of %d passengers who walk off %s are still in the aircraft or the jetway"), N, R->Visible, *R->Label);
	if (R->HoldSince < 0.0) R->HoldSince = P->RealT;
	if (P->RealT - R->SaidAt > 120.0) { R->SaidAt = P->RealT; ADP_LOG(TEXT("PaxFlow: %s (%s) at gate %d - getting off takes longer than planned: %s%s"), *R->Label, *R->Reg, R->Gate, *Why, R->AtDoor ? *FString::Printf(TEXT(", %d at the shut gate door"), R->AtDoor) : TEXT("")); }
	if (P->RealT - R->HoldSince < 15.0 * 60.0) return false;
	R->bNoShowGo = true;
	FString L; int32 n = 0;
	for (const FPx& X : P->Pax) if (X.St != EPx::Gone && X.bArr && X.Run == Ri && X.Do == EDo::ArrIn && X.A.IsValid() && ++n <= 10) { const FVector At = P->PlanOf(X.A->GetActorLocation()); L += FString::Printf(TEXT(" %s (%.1f, %.1f) held by %s;"), *X.Id, At.X, At.Z, X.bDoorWait ? TEXT("the shut gate door") : X.HeldBy.IsEmpty() ? TEXT("-") : *X.HeldBy); }
	ADP_WARN(TEXT("PaxFlow: PROBLEM - %s at gate %d: 15 min after its time %s - the turn-round goes on:%s"), *R->Label, R->Gate, *Why, *L);
	return true;
}

bool APaxFlow::PlaneAtJetway(const AJetwayRigs* J, int32 Jet, const FString& Reg) const
{
	// (the flight's own aircraft - tagged SIM/<reg> when parked, MOVE/<reg> while it moves - within 40 m of this jetway's
	//  docked door point: a jetway docked at another aircraft never opens the door for a later flight's passengers)
	FVector Ro, Dr; if (!J || Reg.IsEmpty() || !J->Frame(Jet, Ro, Dr)) return false;
	ADP_PERF_SCOPE("pax flow: gate door - is the flight's aircraft at its jetway", 1.f);
	const FName A(*(TEXT("SIM/") + Reg)), B(*(TEXT("MOVE/") + Reg));
	for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
		if ((It->Tags.Contains(A) || It->Tags.Contains(B)) && FVector::Dist2D(It->GetActorLocation(), Dr) < 4000.f) return true;
	return false;
}

void APaxFlow::GateDoors(float Dt)
{
	if ((GateDoorsIn -= Dt) > 0.f) return;
	const float Step = 0.25f - GateDoorsIn; GateDoorsIn = 0.25f;
	for (FRun& R : Runs) R.InTunnel = R.AtDoor = 0;
	for (const FPx& X : Pax)
	{
		if (X.St == EPx::Gone || !X.A.IsValid() || !Runs.IsValidIndex(X.Run)) continue;
		if (X.bArr ? X.Do != EDo::ArrIn : X.St != EPx::Jetway || X.FadeT >= 0.f) continue;
		++Runs[X.Run].InTunnel; if (X.bDoorWait) ++Runs[X.Run].AtDoor;
	}
	uint32 On = 0; AJetwayRigs* J = AJetwayRigs::Get(GetWorld());
	for (FRun& R : Runs)
	{
		R.DoorT = R.AtDoor ? R.DoorT + Step : 0.f; if (!R.AtDoor) R.DoorSaid = 30.f;
		const bool bDocked = J && R.Jet != INDEX_NONE && J->State(R.Jet) == AJetwayRigs::EState::Docked;
		const int32 Gi = bDocked ? GateRoom(R.Gate) : INDEX_NONE;
		if (Gates.IsValidIndex(Gi) && Gates[Gi].bF2 && Gates[Gi].N > 0 && Gates[Gi].N <= 32)
		{
			// (owner, to-do 234: the jetway corridor door opens EARLY - departures as soon as the jetway is docked at their
			//  aircraft, before the call and before the first one reaches it; arrivals as soon as it is docked, before the first
			//  one off comes down the tunnel - and STAYS open until every walker who has to pass, late ones too, has passed.
			//  ABabylonModel holds it open on this bit, only while that jetway is docked, and never shuts it on anyone)
			const bool bDepDue = !R.bOff && !R.bGone && !R.bNoShowGo && (R.Spawned < R.Visible || R.Aboard + R.Home < R.Spawned);
			// (no gate agent of yours on shift: nobody opens the corridor door, for departures and arrivals alike)
			const bool bUse = StaffGate >= 1 && (R.bArr ? (R.Spawned < R.Visible || R.InTunnel > 0)
				: R.InTunnel > 0 || (bDepDue && (R.bCalled || PlaneAtJetway(J, R.Jet, R.Reg))));
			if (bUse) On |= uint32(1) << (Gates[Gi].N - 1);
		}
		// (someone at a shut door for long: said, with why)
		if (R.DoorT > R.DoorSaid)
		{
			R.DoorSaid += 60.f;
			const bool bNoAgent = StaffGate < 1;
			ADP_WARN(TEXT("PaxFlow: %s - %d passengers wait at the shut door of gate %d for %.0f s (%s%s)"), *R.Label, R.AtDoor, R.Gate, R.DoorT,
				bDocked ? TEXT("the jetway docked") : TEXT("the jetway not docked"), bNoAgent ? TEXT(", no gate agent on shift") : TEXT(""));
			if (bNoAgent)
				if (UADPSim* Sim = UADPSim::Get(this))
					ADPSchedule::Tell(Sim->World, Sim->Now(), TEXT("gateagent:") + R.Label, TEXT("No gate agent"),
						FString::Printf(TEXT("%s's jetway door won't open: no gate agent on shift. Hire one under Crew."), *R.Label));
		}
	}
	ABabylonModel::GateDoorsOn = On;
}

bool APaxFlow::CallsBoarding(const UWorld* W, const FString& Flight)
{
	const APaxFlow* P = Find(W); if (!P || Flight.IsEmpty()) return false;
	for (const FRun& R : P->Runs) if (!R.bArr && R.Label == Flight) return true;
	return false;
}

APaxFlow* APaxFlow::Find(const UWorld* W)
{
	if (GPax.IsValid() && GPax->GetWorld() == W) return GPax.Get();
	if (!W) return nullptr;
	for (TActorIterator<APaxFlow> It(W); It; ++It) { GPax = *It; return *It; }
	return nullptr;
}

APaxFlow* APaxFlow::Get(UWorld* W)
{
	if (APaxFlow* F = Find(W)) return F;
	if (!W) return nullptr;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APaxFlow* F = W->SpawnActor<APaxFlow>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	GPax = F;
	return F;
}

FVector APaxFlow::PW(float X, float H, float Z) { return FTerminalPlan::ToWorld(X, H, Z); }

float APaxFlow::GroundZ(float PlanX, float PlanZ) const { return float(ARoadNetwork::GroundAt(PlanX, PlanZ) * 100.0) + 3.f; }

#pragma region Setting up

namespace PaxFlowInternal
{
	TArray<FVector2f> GCarSpaces;   // the landside car park's spaces (plan x, z), near the terminal
}

bool APaxFlow::Prepare()
{
	UWorld* W = GetWorld();
	if (!ADepartureBelts::Get(W) || !AADPPeople::Get(W)) return false;
	// (the roads' crossings list fills as the forecourt is built: wait for it a while, then go on without)
	if (ARoadNetwork::ZebraCrossings.Num() == 0 && RealT < 90.0) return false;
	FTerminalPlan Plan; if (!Plan.Load()) { ADP_WARN(TEXT("PaxFlow: no terminal plan - no passengers")); return false; }
	F2Y = Plan.F2Y; F2Z = PW(0, F2Y, 0).Z;
	Map.Build(Plan);
	// the gates: the Floor 2 gates (their jetway doors), the ground gates below (their walk-out doors)
	for (const FPlanRoom& Rm : Plan.Rooms)
	{
		const bool bF2 = Rm.Id.StartsWith(TEXT("F2-GATE-")), bF1 = Rm.Id.StartsWith(TEXT("F1-GATE-"));
		if (!bF2 && !bF1) continue;
		FGate G; G.N = FCString::Atoi(*Rm.Id.Mid(8)); G.Room = Rm.Id; G.bF2 = bF2; G.Place = Map.Find(Rm.Id);
		const FVector A = PW(Rm.X0, 0, Rm.Z0), B = PW(Rm.X1, 0, Rm.Z1);
		G.Box = FBox2D(ForceInit); G.Box += FVector2D(A.X, A.Y); G.Box += FVector2D(B.X, B.Y);
		G.Jw = FVector2f((Rm.X0 + Rm.X1) / 2, FMath::Min(Rm.Z1, 82.f));   // (the room reaches to z 85 in the curved window's bay; the old wall's line)
		for (const FPlanDoor& D : Plan.Doors) if (D.A == Rm.Id && (D.B.StartsWith(TEXT("J-")) || D.B == TEXT("APRON"))) { G.Jw = FVector2f(D.X, D.Z); break; }
		Gates.Add(G);
	}
	// where nobody leans: doorways, stair heads, the escalators' ends, the lifts (plan points, keep-out radius m)
	for (const FPlanDoor& D : Plan.Doors) if (D.Floor >= 2) NoLean.Add({FVector2f(D.X, D.Z), D.Width / 2 + 1.f});
	for (const FPlanStair& S : Plan.Stairs) NoLean.Add({FVector2f((S.X0 + S.X1) / 2, (S.Z0 + S.Z1) / 2), FMath::Max(S.X1 - S.X0, S.Z1 - S.Z0) / 2 + 1.5f});
	for (const FPlanPoint& L : Plan.Lifts) NoLean.Add({FVector2f(L.X, L.Z), 3.f});
	NoLean.Add({FVector2f(15.f, 15.f), 8.f});                                            // (the escalators' top, the landing's gap)
	NoLean.Add({FVector2f(PXARRESC, PXARRFOOT + AEscalatorPair::LengthAlong()), 6.f});  // (the arrivals escalators' top)
	// the passengers' faces: the built MetaHumans of role passenger
	{
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/people-roster.json"))))
		{
			TArray<TSharedPtr<FJsonValue>> Rows;
			if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Rows))
				for (const TSharedPtr<FJsonValue>& V : Rows) if (const TSharedPtr<FJsonObject> O = V->AsObject()) if (O->GetStringField(TEXT("role")) == TEXT("passenger")) Names.Add(O->GetStringField(TEXT("name")));
		}
		// (only those this game has: a face left out of a package must not make its passenger vanish)
		const int32 All = Names.Num();
		Names.RemoveAll([](const FString& N) { return !FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/People/Built/%s/BP_%s"), *N, *N)); });
		if (Names.Num() < All) ADP_WARN(TEXT("PaxFlow: %d of the %d passengers' bodies are not in this game"), All - Names.Num(), All);
		if (!Names.Num()) { ADP_WARN(TEXT("PaxFlow: no passengers in Data/people-roster.json - nobody walks")); return false; }
	}
	// the landside car park's spaces near the terminal (roads.json "lots": the big lot's rows)
	{
		FString Text; TSharedPtr<FJsonObject> Root;
		if (FFileHelper::LoadFileToString(Text, *ADPSite::File(TEXT("roads.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Lots = nullptr;
			if (Root->TryGetArrayField(TEXT("lots"), Lots) && Lots->Num())
			{
				const TSharedPtr<FJsonObject> L = (*Lots)[0]->AsObject();
				const TArray<TSharedPtr<FJsonValue>>* RowsJ = nullptr;
				if (L && !L->HasField(TEXT("frame")) && L->TryGetArrayField(TEXT("rows"), RowsJ))
					for (const TSharedPtr<FJsonValue>& RV : *RowsJ)
					{
						const TSharedPtr<FJsonObject> R = RV->AsObject(); if (!R) continue;
						const double X0 = R->GetNumberField(TEXT("x0")), X1 = R->GetNumberField(TEXT("x1")), Y = R->GetNumberField(TEXT("y")), D = R->GetNumberField(TEXT("d")), Wd = R->GetNumberField(TEXT("w"));
						const double Dir = R->GetNumberField(TEXT("dir"));
						if (Y < -140.0) continue;
						for (double X = X0 + Wd / 2; X < X1; X += Wd) if (X > 25.0 && X < 125.0) GCarSpaces.Add(FVector2f(float(X), float(Y + Dir * D / 2)));
					}
			}
		}
		if (!GCarSpaces.Num()) for (int32 k = 0; k < 60; ++k) GCarSpaces.Add(FVector2f(30.f + k * 1.5f, -65.f - (k % 4) * 8.f));
	}
	// the walls and floors as they really are: the navmesh over the finished rooms and the landside (PaxNav.h), made
	// in the background now; until it is ready the walkers keep their straight ways
	{
		FBox Land(ForceInit);
		auto AddPlan = [&](float X, float Z) { const FVector P = PW(X, 0, Z); const float G = GroundZ(X, Z); Land += FVector(P.X, P.Y, G - 300.f); Land += FVector(P.X, P.Y, G + 500.f); };
		for (const FVector2f& S : GCarSpaces) AddPlan(S.X, S.Y);
		for (const TPair<FVector, FVector>& Zc : ARoadNetwork::ZebraCrossings) for (const FVector& Wz : {Zc.Key, Zc.Value}) if (FVector::Dist2D(Wz, PW(79.f, 0, -60.f)) < 25000.f) { Land += Wz - FVector(0, 0, 300.f); Land += Wz + FVector(0, 0, 500.f); }
		for (float X = 40.f; X <= 116.f; X += 4.f) AddPlan(X, 1.f);   // (the front of the terminal: its entrances)
		if (Land.IsValid) Land = Land.ExpandBy(FVector(800.f, 800.f, 0.f));
		PaxNav::Start(W, Plan, Land);
		for (TActorIterator<ATerminalDoors> It(W); It; ++It) { DoorsActor = *It; break; }
		for (TActorIterator<ASeats> It(W); It; ++It) { SeatsActor = *It; break; }
	}
	bTest = FParse::Param(FCommandLine::Get(), TEXT("PaxFlowTest"));
	ADP_LOG(TEXT("PaxFlow: ready - %d gates, %d car park spaces, %d passenger faces, %d zebra crossing parts%s"), Gates.Num(), GCarSpaces.Num(), Names.Num(), ARoadNetwork::ZebraCrossings.Num(), bTest ? TEXT("; -PaxFlowTest: a departure and an arrival now") : TEXT(""));
	return true;
}

int32 APaxFlow::GateRoom(int32 Gate) const
{
	int32 Best = INDEX_NONE;
	for (int32 i = 0; i < Gates.Num(); ++i) if (Gates[i].N == Gate && (Best == INDEX_NONE || Gates[i].bF2)) Best = i;
	if (Best == INDEX_NONE) for (int32 i = 0; i < Gates.Num(); ++i) if (Gates[i].N == 2 && Gates[i].bF2) Best = i;
	return Best == INDEX_NONE && Gates.Num() ? 0 : Best;
}

int32 APaxFlow::JetwayFor(int32 Gi) const
{
	// the jetway whose rotunda is right outside this gate's jetway door
	AJetwayRigs* R = AJetwayRigs::Get(GetWorld()); if (!R || !Gates.IsValidIndex(Gi) || !Gates[Gi].bF2) return INDEX_NONE;
	const FVector Door = PW(Gates[Gi].Jw.X, F2Y, Gates[Gi].Jw.Y);
	int32 Best = INDEX_NONE; float Bd = 2500.f;
	for (int32 j = 0; j < R->Num(); ++j) { FVector Ro, Dr; if (R->Frame(j, Ro, Dr) && FVector::Dist2D(Ro, Door) < Bd) { Bd = FVector::Dist2D(Ro, Door); Best = j; } }
	return Best;
}

void APaxFlow::ChooseDesks(FRun& R)
{
	// your airline at desks 1-4, the others each a block of three (the desks' screens show who checks in where later)
	R.Desks.Reset();
	if (R.bPlayer) { for (int32 d = 1; d <= 4; ++d) R.Desks.Add(d); return; }
	const int32 Start = 5 + int32(GetTypeHash(R.AirlineId.IsEmpty() ? R.Label : R.AirlineId) % 5u) * 3;
	for (int32 d = Start; d < Start + 3; ++d) R.Desks.Add(d);
}

#pragma endregion

#pragma region The flights

void APaxFlow::MakeTestRuns()
{
	// (owner 2026-11-21: the test passengers board THE aircraft at the gate - made before the simulation had registered its
	//  turnaround they fell back to gate 2 with no aircraft, and it pushed back without them. Now: wait for a real
	//  turnaround at a Floor 2 jetway gate - the one leaving last, so boarding has time - and link to it; never guess)
	int32 DepGate = 2; FString DepReg; FDateTime Latest(0);
	for (const ADPTurnaround::FTurn& T : ADPTurnaround::All())
	{
		const int32 G = ADPGround::GateOfStand(T.Stand); const int32 Gi = GateRoom(G);
		if (Gi != INDEX_NONE && Gates[Gi].N == G && Gates[Gi].bF2 && T.OffBlocks > Latest) { Latest = T.OffBlocks; DepGate = G; DepReg = T.Reg; }
	}
	if (DepReg.IsEmpty())
	{
		static double FirstTry = -1.0, LastSaid = -100.0;
		if (FirstTry < 0.0) FirstTry = RealT;
		if (RealT - LastSaid >= 10.0) { LastSaid = RealT; ADP_LOG(TEXT("PaxFlow: test passengers wait for an aircraft's turnaround at a jetway gate (%.0f s)"), RealT - FirstTry); }
		if (RealT - FirstTry < 300.0) return;   // (called again next tick)
		ADP_WARN(TEXT("PaxFlow: no aircraft turnaround at a jetway gate after 5 min - the test passengers go to gate 2 with no aircraft"));
	}
	bTestMade = true;
	bool bPlayerAir = false; FString PlayerId;
	if (const UADPSim* Sim = UADPSim::Get(this)) for (const FADPAirline& A : Sim->World.Airlines) if (A.bPlayer) { bPlayerAir = true; PlayerId = A.Id; }
	{
		FRun R; R.Key = TEXT("T:DEP"); R.Label = TEXT("TEST PX1"); R.bTest = true; R.bPlayer = true; R.AirlineId = PlayerId; R.Reg = DepReg;
		R.Gate = DepGate; R.Pax = 20; R.Visible = 20; R.DepT = RealT + 15.0 * 60.0;
		for (int32 i = 0; i < R.Visible; ++i) R.SpawnAt.Add(RealT + 2.0 + i * 3.5);
		ChooseDesks(R); R.Jet = JetwayFor(GateRoom(R.Gate));
		Runs.Add(R);
		ADP_LOG(TEXT("PaxFlow: test departure %s at gate %d (jetway %d%s) - %d passengers from the car park over 70 s, boarding called 7.5 min in"), *R.Label, R.Gate, R.Jet, DepReg.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", the turnaround of %s"), *DepReg), R.Visible);
	}
	{
		FRun R; R.Key = TEXT("T:ARR"); R.Label = TEXT("TEST AR1"); R.bTest = true; R.bArr = true;
		R.Gate = DepReg.IsEmpty() ? (DepGate == 3 ? 2 : 3) : DepGate; R.Reg = DepReg; R.Pax = 14;   // (the real aircraft: off it first, then boarding)
		 R.Visible = 14; R.ArrStart = RealT + 40.0;
		R.Jet = JetwayFor(GateRoom(R.Gate));
		Runs.Add(R);
		ADP_LOG(TEXT("PaxFlow: test arrival %s at gate %d (jetway %d) - %d passengers off the aircraft from 40 s, one every 3 s"), *R.Label, R.Gate, R.Jet, R.Visible);
	}
	(void)bPlayerAir;
}

void APaxFlow::Poll()
{
	UADPSim* Sim = UADPSim::Get(this);
	if (!Sim || !Sim->IsRunning()) return;
	const FString Here = ADPBoards::HereIcao();
	const FDateTime Now = Sim->Now();
	StaffDesk = ADPSchedule::OnShift(Sim->World, TEXT("checkin"), Now); StaffLane = ADPSchedule::OnShift(Sim->World, TEXT("security"), Now);
	StaffGate = ADPSchedule::OnShift(Sim->World, TEXT("gate"), Now);   // (the gate agent opens the jetway corridor door - GateDoors)
	// ---- departures: the planned legs from here (your timetable's and the other airlines' - Sim/ADPSchedule.h); who comes is
	//      who booked. No planned leg, nobody comes
	for (const FADPPlanned& P : Sim->World.Planned)
	{
		if (P.From != Here) continue;
		const FString Key = TEXT("D:") + P.Id;
		FRun* Ex = Runs.FindByPredicate([&](const FRun& R) { return R.Key == Key; });
		if (Ex)
		{
			Ex->DepT = PxSec(P.Est);
			if (P.State == TEXT("cancelled") && !Ex->bOff) { Ex->bOff = true; ADP_LOG(TEXT("PaxFlow: %s is cancelled - its passengers go home"), *Ex->Label); AADPHUD::Announce(FString::Printf(TEXT("Passengers of flight %s: your flight has been cancelled. Please leave by the arrivals hall; your money will be paid back."), *Ex->Label), 1); }
			if (P.State == TEXT("gone")) Ex->bGone = true;
			const int32 G = ADPGround::GateOfStand(P.Stand);
			if (P.State == TEXT("planned") && G && G != Ex->Gate) { ADP_LOG(TEXT("PaxFlow: %s leaves from gate %d (was %d)"), *Ex->Label, G, Ex->Gate); Ex->Gate = G; Ex->Jet = JetwayFor(GateRoom(G)); }
			continue;
		}
		if (P.State != TEXT("planned") || P.Pax() <= 0) continue;
		const double Dep = PxSec(P.Sched), MinTo = (PxSec(P.Est) - SimT) / 60.0;
		if ((Dep - SimT) / 60.0 > 122.0 || MinTo < 25.0) continue;   // (too early yet, or too late to walk the whole way now)
		FRun R; R.Key = Key; R.PlanId = P.Id; R.Label = P.FlightNo; R.Reg = P.Reg; R.AirlineId = P.AirlineId; R.Gate = ADPGround::GateOfStand(P.Stand); R.DepT = PxSec(P.Est);
		R.bPlayer = P.AirlineId == UADPSim::Me();
		if (R.Gate == 0) R.Gate = ADPBoards::GateFor(P.Reg);   // (a remote stand: a bus gate)
		R.Pax = P.Pax();
		R.Visible = FMath::Min(R.Pax, int32(VisibleDep));
		int32 Missed = 0;
		for (int32 i = 0; i < R.Visible; ++i)
		{
			const double At = Dep - Rng.FRandRange(75.f, 120.f) * 60.0;   // (each at their own time before the flight)
			if (At < SimT - 8.0 * 60.0) { ++Missed; continue; }   // (they would be inside already - never popped in there)
			R.SpawnAt.Add(At);
		}
		R.SpawnAt.Sort();
		R.Visible = R.SpawnAt.Num();
		if (!R.Visible) continue;
		ChooseDesks(R); R.Jet = JetwayFor(GateRoom(R.Gate));
		ADP_LOG(TEXT("PaxFlow: departure %s (%s) to %s, gate %d, in %.0f min - %d passengers booked, %d of them walk (the rest simulated)%s"), *R.Label, *R.Reg, *P.To, R.Gate, MinTo, R.Pax, R.Visible, Missed ? *FString::Printf(TEXT(", %d already past the car park when the game came to them"), Missed) : TEXT(""));
		Runs.Add(R);
	}
	// ---- arrivals: an aircraft just in, its deplaning started (the jetway docked, the door open)
	for (const ADPTurnaround::FTurn& T : ADPTurnaround::All())
	{
		if (!T.bArrival || T.ArrPax <= 0) continue;
		const ADPTurnaround::FTask* Dp = T.Tasks.FindByPredicate([](const ADPTurnaround::FTask& K) { return K.Id == TEXT("deplane"); });
		if (!Dp || !Dp->bStarted) continue;
		const FString Key = TEXT("A:") + T.Reg + TEXT(":") + FString::Printf(TEXT("%lld"), T.OnBlocks.GetTicks() / ETimespan::TicksPerMinute);
		if (Runs.ContainsByPredicate([&](const FRun& R) { return R.Key == Key; })) continue;
		FRun R; R.Key = Key; R.bArr = true; R.Reg = T.Reg; R.Gate = ADPGround::GateOfStand(T.Stand); R.bPlayer = T.bMine;
		R.Pax = T.ArrPax; R.Label = T.ArrFlight.IsEmpty() ? T.Reg : T.ArrFlight;
		R.Visible = FMath::Min(R.Pax, int32(VisibleArr));
		R.ArrStart = SimT; R.Jet = JetwayFor(GateRoom(R.Gate));
		if (R.Visible <= 0 || GateRoom(R.Gate) == INDEX_NONE) continue;
		ADP_LOG(TEXT("PaxFlow: arrival %s (%s) deplaning at gate %d (jetway %d) - %d passengers, %d of them walk (the rest simulated)"), *R.Label, *R.Reg, R.Gate, R.Jet, R.Pax, R.Visible);
		Runs.Add(R);
	}
}

void APaxFlow::CallBoarding(int32 Ri)
{
	FRun& R = Runs[Ri];
	R.bCalled = true; R.CallT = Clock(R); R.CallBefore = float((R.DepT - R.CallT) / 60.0);
	// (Airbus AC 5-2: about 12 a minute at one door, plus 2 min for the last ones to sit - the plan's Board = Pax/12 + 2;
	// the visible ones spread over that whole time: priority first, then by group)
	const double BoardMin = FMath::Max(2.0, R.Pax / 12.0);
	R.BoardSpanS = BoardMin * 60.0;
	TArray<int32> Mine; for (int32 i = 0; i < Pax.Num(); ++i) if (Pax[i].St != EPx::Gone && Pax[i].Run == Ri && !Pax[i].bArr) Mine.Add(i);
	for (int32 i = Mine.Num() - 1; i > 0; --i) Mine.Swap(i, Rng.RandRange(0, i));
	Mine.StableSort([&](const int32 A, const int32 B) { return Pax[A].bPriority && !Pax[B].bPriority; });
	const int32 N = FMath::Max(1, R.Visible);
	for (int32 k = 0; k < Mine.Num(); ++k) Pax[Mine[k]].BoardAt = R.CallT + 45.0 + (k + (R.Visible - Mine.Num())) * R.BoardSpanS / N;
	FString Who = TEXT("Airport Dynasty"), Place;
	if (const UADPSim* Sim = UADPSim::Get(this)) for (const FADPAirline& A : Sim->World.Airlines) if (A.Id == R.AirlineId || (R.AirlineId.IsEmpty() && A.bPlayer)) { Who = A.Name; break; }
	for (const FADPBoardRow& Row : ADPBoards::Rows(this, false, 40)) if (Row.Reg == R.Reg && !R.Reg.IsEmpty()) { Place = Row.Place; break; }
	AADPHUD::Announce(FString::Printf(TEXT("Ladies and gentlemen, %s flight %s%s is now boarding at gate %d. We invite passengers needing assistance, families with young children and our priority passengers to board first, followed by all other passengers by group. Please have your boarding pass and passport ready."),
		*Who, *R.Label, Place.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" to %s"), *Place), R.Gate), 1);
	ADP_LOG(TEXT("PaxFlow: BOARDING CALLED - %s at gate %d (%d passengers, %.1f min at the real rate; %d of the walkers here, the jetway %s)"), *R.Label, R.Gate, R.Pax, BoardMin + 2.0, Mine.Num(),
		R.Jet == INDEX_NONE ? TEXT("- none: walk-out") : AJetwayRigs::Get(GetWorld()) && AJetwayRigs::Get(GetWorld())->State(R.Jet) == AJetwayRigs::EState::Docked ? TEXT("docked") : TEXT("not docked yet"));
}

void APaxFlow::StepRun(int32 Ri, float Dt)
{
	FRun& R = Runs[Ri];
	const double Now = Clock(R);
	if (!R.bArr)
	{
		if (R.bOff || R.bGone) return;
		// getting out of their cars, a few seconds apart at the most
		if (R.Spawned < R.SpawnAt.Num() && Now >= R.SpawnAt[R.Spawned] && SpawnGap <= 0.f && FaceReady()) { SpawnDep(Ri); SpawnGap = 0.8f; }
		// the boarding call: the turnaround's "board" task starting (the test: 7.5 min in)
		if (!R.bCalled)
		{
			bool bGo = false;
			if (R.bTest) bGo = Now >= R.DepT - 7.5 * 60.0;
			else
			{
				const ADPTurnaround::FTurn* T = ADPTurnaround::All().FindByPredicate([&](const ADPTurnaround::FTurn& X) { return X.Reg == R.Reg; });
				const ADPTurnaround::FTask* B = T ? T->Tasks.FindByPredicate([](const ADPTurnaround::FTask& K) { return K.Id == TEXT("board"); }) : nullptr;
				bGo = B ? B->bStarted : (R.DepT - Now) / 60.0 <= 30.0;   // (no turnaround for it - a remote stand: 30 min before)
			}
			// (never into the ones getting off: the same aircraft's arrivals all through the gate's door first - as the turnaround)
			if (bGo) for (int32 k = 0; k < Runs.Num(); ++k) if (Runs[k].bArr && Runs[k].Reg == R.Reg && !Runs[k].bNoShowGo && Runs[k].Gate == R.Gate && Inside(k) > 0) { bGo = false; break; }
			// (your flight: an agent at the gate desk and the crew aboard, else the call waits - and the game says why)
			if (bGo && R.bPlayer && !R.bTest)
				if (UADPSim* Sim = UADPSim::Get(this))
				{
					const FADPAircraft* Ac = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == R.Reg; });
					if (StaffDesk < 1) { bGo = false; ADPSchedule::Tell(Sim->World, Sim->Now(), TEXT("gate:") + R.Label, TEXT("Nobody at the gate"), FString::Printf(TEXT("%s can't board: no passenger service agent on shift. Hire them under Crew."), *R.Label)); }
					else if (Ac && !Ac->Crew.Num()) { bGo = false; ADPSchedule::Tell(Sim->World, Sim->Now(), TEXT("crew:") + R.Label, TEXT("No crew aboard"), FString::Printf(TEXT("%s can't board: its crew isn't aboard yet."), *R.Label)); }
				}
			if (bGo) CallBoarding(Ri);
		}
		else
		{
			if (!R.bFinal && Now >= R.CallT + 45.0 + R.BoardSpanS * 0.8)
			{
				R.bFinal = true;
				AADPHUD::Announce(FString::Printf(TEXT("This is the final call for flight %s. Would all remaining passengers please proceed immediately to gate %d, where the flight is now closing."), *R.Label, R.Gate), 1);
				ADP_LOG(TEXT("PaxFlow: final call - %s at gate %d (%d of %d walkers aboard)"), *R.Label, R.Gate, R.Aboard, R.Spawned);
			}
			if (!R.bAllLogged && R.Spawned >= R.Visible && R.Aboard + R.Home >= R.Spawned && R.Spawned > 0)
			{
				R.bAllLogged = true;
				ADP_LOG(TEXT("PaxFlow: LAST PASSENGER ABOARD - %s at gate %d, %.1f min after the call (%d walked, %d in all counted aboard)"), *R.Label, R.Gate, (Now - R.CallT) / 60.0, R.Aboard, R.Pax);
			}
		}
		return;
	}
	// ---- an arrival: off the aircraft one by one once the jetway is docked (about 20 a minute) ----
	if (R.ArrStart < 0.0 || Now < R.ArrStart || R.Spawned >= R.Visible) return;
	if (!R.bDeplaning)
	{
		AJetwayRigs* J = AJetwayRigs::Get(GetWorld());
		const bool bDocked = R.Jet == INDEX_NONE || (J && J->State(R.Jet) == AJetwayRigs::EState::Docked);
		R.DockWait += Dt;
		// (owner 2026-11-21: the jetway fully out at the aircraft first - nobody walks before it is docked, the test too)
		if (!bDocked) { if (R.DockWait > 60.f && !R.bDockNoted) { R.bDockNoted = true; ADP_LOG(TEXT("PaxFlow: %s waits at gate %d for the jetway to dock"), *R.Label, R.Gate); } return; }
		R.bDeplaning = true; R.NextArr = Now;
		// their bags: the flight's claim belt, the shown passengers' bags tagged with their ids
		TArray<FString> Ids; for (int32 k = 0; k < R.Visible; ++k) if (k % 5 != 4) Ids.Add(FString::Printf(TEXT("%s-%02d"), *R.Label, k + 1));
		if (ABaggageBelts::BeltOf(GetWorld(), R.Label) == INDEX_NONE)
		{
			const int32 Gi = GateRoom(R.Gate);
			bool bCrew = false; if (!R.bTest) for (const ADPTurnaround::FTurn& Tn : ADPTurnaround::All()) if (Tn.Reg == R.Reg) bCrew = true;   // (the test arrival keeps its own bags)
			ABaggageBelts::OnArrived(GetWorld(), Gates.IsValidIndex(Gi) ? Gates[Gi].Jw.X : 30.f, R.Label, bCrew ? 0 : -1);   // (a real aircraft: the ground crew's train brings its bags)
		}
		ABaggageBelts::TagBags(GetWorld(), R.Label, Ids);
		ADP_LOG(TEXT("PaxFlow: %s deplaning at gate %d%s - belt %d, %d bags tagged to their passengers"), *R.Label, R.Gate, bDocked ? TEXT("") : TEXT(" (the jetway docked)"), ABaggageBelts::BeltOf(GetWorld(), R.Label) + 1, Ids.Num());
	}
	if (Now >= R.NextArr && SpawnGap <= 0.f && FaceReady())
	{
		// (the door clear: nobody within a metre of it - they queue in the aircraft)
		FVector Mid, Front; AJetwayRigs* Jr = AJetwayRigs::Get(GetWorld());
		if (Jr && R.Jet != INDEX_NONE && Jr->CabView(R.Jet, Mid, Front)) { const FVector At = Mid + Front.GetSafeNormal2D() * 55.f; for (const FPx& O : Pax) if (const AActor* Oa = O.A.Get()) if (O.St != EPx::Gone && FVector::Dist2D(Oa->GetActorLocation(), At) < 95.f) return; }
		SpawnArr(Ri); R.NextArr = Now + 3.0; SpawnGap = 0.5f; }
}

#pragma endregion

#pragma region People

int32 APaxFlow::NewPx()
{
	if (FreePx.Num()) { const int32 i = FreePx.Pop(); Pax[i] = FPx(); return i; }
	return Pax.AddDefaulted();
}

bool APaxFlow::FaceReady()
{
	// the next one's body: drawn by lot, loaded in the background; nobody is made while it isn't in yet (it would hold the game)
	if (!Names.Num()) return true;
	auto Path = [](const FString& N) { return FString::Printf(TEXT("/Game/People/Built/%s/BP_%s.BP_%s_C"), *N, *N, *N); };
	if (NextFace.IsEmpty()) { NextFace = Names[Rng.RandRange(0, Names.Num() - 1)]; NextFaceAt = RealT; }
	TSharedPtr<FStreamableHandle>& H = FaceLoads.FindOrAdd(NextFace);
	if (!H.IsValid()) H = UAssetManager::GetStreamableManager().RequestAsyncLoad(FSoftObjectPath(Path(NextFace)));
	if (!H.IsValid() || H->HasLoadCompleted()) return true;
	if (RealT - NextFaceAt > 20.0) { ADP_WARN(TEXT("PaxFlow: %s still loading after 20 s - another face"), *NextFace); NextFace.Reset(); }
	return false;
}

AActor* APaxFlow::Body(const FVector& At, float Yaw, FPx& P)
{
	AADPPeople* People = AADPPeople::Get(GetWorld());
	if (!People) return nullptr;
	P.Name = NextFace.IsEmpty() ? Names[Rng.RandRange(0, Names.Num() - 1)] : NextFace;
	NextFace.Reset(); FaceReady();   // (the one after them begins to load)
	AActor* A = People->Spawn(P.Name, At, Yaw);
	if (!A) return nullptr;
	A->SetActorLocation(At);   // (Spawn finds the floor from above; inside a jetway's cab that is its roof - our own point)
	P.A = A;
	if (AADPPeople::FWho* W = People->Find(A)) P.Body = W->Body;
	// (their own animation instance: the same walk blend space, plus the right hand closed round a carried bag's handle
	//  and the arm a little out - PaxAnimInstance.h)
	UPaxAnimInstance::Install(P.Body.Get());
	// (what they carry: everyone a phone, about one in ten a tablet - PaxProps.h; the test: every fourth one a tablet)
	APaxProps::Dress(A, P.Body.Get(), Rng.RandRange(0, MAX_int32 - 1));
	if (bTest && Runs.IsValidIndex(P.Run) && Runs[P.Run].bTest && (Runs[P.Run].Spawned % 4) == 1) APaxProps::GiveTablet(A);
	// (far away their animation updates only while seen, and less often - no change up close)
	TArray<USkeletalMeshComponent*> Sk; A->GetComponents<USkeletalMeshComponent>(Sk);
	for (USkeletalMeshComponent* S : Sk) { S->bEnableUpdateRateOptimizations = true; S->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered; }
	PaxNav::NoNav(A);
	P.Holder = NewObject<USceneComponent>(this); P.Holder->RegisterComponent(); Holders.Add(P.Holder);
	P.Pace = Rng.FRand() < 0.15f ? Rng.FRandRange(150.f, 170.f) : Rng.FRandRange(105.f, 148.f);   // (everyone their own pace: some slow, most normal, a few brisk - owner 2026-11-21)
	P.Jit = FVector2D(Rng.FRandRange(-45.f, 45.f), Rng.FRandRange(-45.f, 45.f));
	P.bPriority = Rng.FRand() < 0.1f;
	P.Order = Frame;
	return A;
}

FVector APaxFlow::CarParkSpot()
{
	const FVector2f S = GCarSpaces[Rng.RandRange(0, GCarSpaces.Num() - 1)];
	const FVector2f J(Rng.FRandRange(-0.9f, 0.9f), Rng.FRandRange(-1.5f, 1.5f));   // (beside where the car stands)
	FVector P = PW(S.X + J.X, 0, S.Y + J.Y); P.Z = GroundZ(S.X + J.X, S.Y + J.Y);
	return P;
}

int32 APaxFlow::SpawnDep(int32 Ri)
{
	FRun& R = Runs[Ri];
	++R.Spawned;
	const int32 i = NewPx(); FPx& P = Pax[i];
	P.Run = Ri; P.Id = FString::Printf(TEXT("%s-%02d"), *R.Label, R.Spawned);
	const FVector At = CarParkSpot();
	AActor* A = Body(At, -90.f, P);
	if (!A) { P.St = EPx::Gone; FreePx.Add(i); return INDEX_NONE; }
	// their bag (three in four check one; in the test all): carried by its handle until it is set on the desk's belt
	ADepartureBelts* Dep = ADepartureBelts::Get(GetWorld());
	// (-PaxFlowTest: walkers 2-4 have hand luggage only and go in by a revolving door - 2 and 4 the west one, 3 the east)
	const bool bTestRev = R.bTest && R.Spawned >= 2 && R.Spawned <= 4;
	const bool bBag = Dep && !bTestRev && (R.bTest || Rng.FRand() < 0.75f) && R.Desks.Num();
	if (bBag)
	{
		int32 Best = R.Desks[0], Bn = MAX_int32;
		for (const int32 D : R.Desks) { int32 N = 0; for (const FPx& O : Pax) if (O.St != EPx::Gone && O.Desk == D) ++N; if (N < Bn) { Bn = N; Best = D; } }
		P.Desk = Best;
		P.Bag = Dep->PassengerDrop(A, P.Desk, P.Id, R.Label, P.Holder, NAME_None, false);
		if (P.Bag != INDEX_NONE)
		{
			P.bHasBag = true;
			const FDepBagGrips G = Dep->Grips(P.Bag);
			P.Handle = Dep->BagTransform(P.Bag).InverseTransformPosition(G.Handle);
		}
	}
	if (P.bHasBag) ++R.NBags;
	P.Stage = TEXT("out of the car, in the car park");
	// (owner 2026-11-21: the entrance. A checked bag (a suitcase carried - and later carts, strollers, wheelchairs) always the
	//  wide sliding doors. Most people take the sliding doors anyway: hand luggage only, about one in six takes the nearest
	//  revolving door, and only when it is clearly (8 m+) nearer on their way than the main entrance - owner 2026-11-21)
	int32 Rev = INDEX_NONE;
	if (!P.bHasBag) if (const ATerminalDoors* Dr = Doors()) if (Dr->NumRevolving())
	{
		if (bTestRev) Rev = Dr->NearestRevolving(PW(R.Spawned == 3 ? 101.f : 50.f, 0, -4.f));
		else
		{
			const int32 Near = Dr->NearestRevolving(At);
			FVector C, Ax; float Rc;
			if (Near != INDEX_NONE && Dr->RevolvingFrame(Near, C, Ax, Rc) && FMath::Abs(PlanOf(C).X - PlanOf(At).X) + 8.f < FMath::Abs(PXDOORX - PlanOf(At).X) && Rng.FRand() < 0.18f) Rev = Near;
		}
	}
	WayToHall(P, At, Rev);
	P.Do = P.bHasBag ? EDo::JoinDesk : EDo::JoinLane;
	if (!P.bHasBag) WayToSecurity(P);   // (no bag to check in: checked in on the phone, straight to security)
	if (R.Spawned <= 3 || R.bTest) ADP_LOG(TEXT("PaxFlow: %s (%s) out of a car in the car park for %s%s"), *P.Id, *P.Name, *R.Label, P.bHasBag ? *FString::Printf(TEXT(", bag #%d for desk %d"), P.Bag, P.Desk) : TEXT(", no bag"));
	return i;
}

int32 APaxFlow::SpawnArr(int32 Ri)
{
	FRun& R = Runs[Ri];
	++R.Spawned;
	const int32 i = NewPx(); FPx& P = Pax[i];
	P.Run = Ri; P.bArr = true; P.Id = FString::Printf(TEXT("%s-%02d"), *R.Label, R.Spawned);
	P.bArrBag = (R.Spawned - 1) % 5 != 4;
	const int32 Gi = GateRoom(R.Gate);
	if (!Gates.IsValidIndex(Gi)) { P.St = EPx::Gone; FreePx.Add(i); return INDEX_NONE; }
	const FGate& G = Gates[Gi];
	// at the cab's end, where the aircraft's door is (the cab's floor found from inside it); a ground gate: in from the
	// apron through its walk-out door
	const float Fy = G.bF2 ? F2Y : 0.f;
	FVector At = PW(G.Jw.X, Fy, G.Jw.Y + 1.5f), Ro = At, Mid = At, Front = FVector(0, -1, 0);
	AJetwayRigs* J = AJetwayRigs::Get(GetWorld());
	if (G.bF2 && (R.Jet == INDEX_NONE || !J))
	{
		ADP_WARN(TEXT("PaxFlow: %s at gate %d has no jetway to come off by - its passengers stay data"), *R.Label, R.Gate);
		R.Spawned = R.Visible; P.St = EPx::Gone; FreePx.Add(i); return INDEX_NONE;
	}
	if (J && R.Jet != INDEX_NONE && J->CabView(R.Jet, Mid, Front))
	{
		FVector Dr; J->Frame(R.Jet, Ro, Dr);
		FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(PaxCab), false);
		if (GetWorld()->LineTraceSingleByChannel(H, Mid, Mid - FVector(0, 0, 400.f), ECC_Visibility, Q)) Mid.Z = H.ImpactPoint.Z;
		At = Mid + Front.GetSafeNormal2D() * 60.f;
		Ro.Z = F2Z;
	}
	AActor* A = Body(At, (Ro - At).GetSafeNormal2D().Rotation().Yaw, P);
	if (!A) { P.St = EPx::Gone; FreePx.Add(i); return INDEX_NONE; }
	P.Stage = TEXT("off the aircraft, along the jetway");
	auto Add = [&](const FVector& W, uint8 Mode = 0) { FWp Wp; Wp.P = W; Wp.Mode = Mode; P.Way.Add(Wp); };
	// (along the jetway's inside as it is now: its slices' middles, each to the right of it as far as there is room - the walls known)
	P.TunA = 0;
	if (R.Jet != INDEX_NONE && Measure(R))
		for (int32 k = 0; k < R.Walk.Num(); k += 3) { const FSlice& Sl = R.Walk[k]; Add(Sl.C + Sl.Rt * FMath::Clamp(float(P.Jit.X) * 0.4f + 8.f, -Sl.Lim, Sl.Lim)); }
	else if (R.Jet != INDEX_NONE) { Add(Mid); Add(Ro); Add(PW(G.Jw.X, Fy, G.Jw.Y + 1.2f)); }
	P.TunB = P.Way.Num();
	for (FWp& Wt : P.Way) Wt.bNav = true;
	Add(PW(G.Jw.X + float(P.Jit.X) / 250.f, Fy, G.Jw.Y - 1.5f));
	P.Do = EDo::ArrIn;
	if (R.Spawned <= 3 || R.bTest) ADP_LOG(TEXT("PaxFlow: %s (%s) off %s at gate %d%s"), *P.Id, *P.Name, *R.Label, R.Gate, P.bArrBag ? TEXT(", a bag in the hold") : TEXT(", hand luggage only"));
	return i;
}

void APaxFlow::Finish(int32 i, const TCHAR* Why, bool bKeepBody)
{
	FPx& P = Pax[i];
	for (FLine& L : DeskLines) L.Who.Remove(i);
	for (FLine& L : LaneLines) L.Who.Remove(i);
	for (FLine& L : RevLines) L.Who.Remove(i);
	if (P.Rev != INDEX_NONE && P.RevComp != INDEX_NONE) if (ATerminalDoors* Dr = Doors()) Dr->RevolvingLeave(P.Rev, P.RevComp);
	P.Rev = INDEX_NONE; P.RevComp = INDEX_NONE;
	if (AActor* A = P.A.Get())
	{
		LeanSpots.RemoveAll([&](const FVector& L) { return FVector::Dist2D(L, A->GetActorLocation()) < 60.f; });
		ASeats::Release(A);
		if (P.bHasBag && P.bArr) if (ABaggageBelts* B = ABaggageBelts::Get(GetWorld())) B->DropPaxBag(P.Id);
		if (P.bHasBag && !P.bArr && P.Bag != INDEX_NONE) if (ADepartureBelts* D = ADepartureBelts::Get(GetWorld())) D->RemoveBag(P.Bag);
		APaxProps::Forget(A);
		if (!bKeepBody) A->Destroy();
	}
	if (P.Holder) { Holders.Remove(P.Holder); P.Holder->DestroyComponent(); P.Holder = nullptr; }
	if (Runs.IsValidIndex(P.Run) && (Runs[P.Run].bTest || Runs[P.Run].Aboard + Runs[P.Run].Out < 3)) ADP_LOG(TEXT("PaxFlow: %s %s"), *P.Id, Why);
	P.St = EPx::Gone; P.A = nullptr;
	if (Watched == i) Watched = INDEX_NONE;
	FreePx.Add(i);
}

void APaxFlow::Anim(FPx& P)
{
	if (USkeletalMeshComponent* B = P.Body.Get())
		if (UAnimSingleNodeInstance* One = B->GetSingleNodeInstance())
		{
			// (the hand closed round the handle while the bag is carried - not once it is set down; it opens and closes
			//  over a quarter of a second. A body that sat down came back on the plain instance: ours again)
			const bool bCarry = P.bHasBag && !(!P.bArr && P.bBagDown) && P.Holder && P.Holder->GetAttachParent() == B;
			UPaxAnimInstance* Mine = Cast<UPaxAnimInstance>(One);
			if (!Mine && bCarry && !B->IsRunningParallelEvaluation() && !B->IsPostEvaluatingAnimation()) { Mine = UPaxAnimInstance::Install(B); One = B->GetSingleNodeInstance(); if (!One) return; }
			if (Mine) { Mine->bGripRight = bCarry; Mine->Twist = P.TurnYaw; }
			// (owner 2026-11-21: no moon-gliding - the legs follow the REAL motion. The blend space has Direction (x, degrees
			//  round the body: 0 ahead, +-90 sideways, 180 back) and Speed (y); measured from how the body actually moved since
			//  the last update - a step aside to let someone by walks sideways, a turn while walking blends the diagonals)
			if (const AActor* A = P.A.Get())
			{
				const double Now = A->GetWorld()->GetTimeSeconds();
				const FVector L = A->GetActorLocation();
				if (P.PrevTime >= 0.0 && Now - P.PrevTime > 1e-3)
				{
					const float Dt = float(Now - P.PrevTime);
					FVector V = (L - P.PrevLoc) / Dt; V.Z = 0.f;
					const float Spd = FMath::Min(V.Size(), 400.f);
					P.MoveSpd = FMath::FInterpTo(P.MoveSpd, Spd, Dt, 8.f);
					if (Spd > 8.f)
					{
						const float Face = A->GetActorRotation().Yaw + 90.f;    // (the body faces the actor's +y)
						const float Rel = FRotator::NormalizeAxis(V.Rotation().Yaw - Face);
						P.MoveDir = FRotator::NormalizeAxis(P.MoveDir + FRotator::NormalizeAxis(Rel - P.MoveDir) * FMath::Min(1.f, Dt * 8.f));
					}
				}
				P.PrevLoc = L; P.PrevTime = Now;
			}
			const float Sp = P.bRideNow ? 0.f : P.MoveSpd;
			One->SetBlendSpacePosition(FVector(P.MoveDir, FMath::Min(Sp * 1.5f, 300.f), 0.f));
			One->SetPlayRate(Sp > 20.f ? FMath::Clamp(Sp / FMath::Max(Sp * 1.5f, 100.f), 0.5f, 1.4f) : 1.f);
		}
}

void APaxFlow::Idle(FPx& P, AActor* A, float Dt, const FVector* Face)
{
	P.Speed = FMath::FInterpTo(P.Speed, 0.f, Dt, 5.f);
	// (someone needs to get past: a small step to the side, held a moment, then back - owner 2026-11-21, like the people in
	//  a real queue; under 40 cm, so a queue place stays the same)
	{
		P.YieldT = FMath::Max(0.f, P.YieldT - Dt);
		const FVector Goal = P.YieldT > 0.f ? P.YieldWant : FVector::ZeroVector;
		const FVector NewOff = FMath::VInterpTo(P.YieldOff, Goal, Dt, 3.f);
		const FVector Step = NewOff - P.YieldOff;
		if (!Step.IsNearlyZero(0.01f)) { A->AddActorWorldOffset(FVector(Step.X, Step.Y, 0.f)); P.YieldOff = NewOff; }
	}
	if (Face && !Face->IsNearlyZero()) A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, Face->Rotation().Yaw - 90.f, 0.f), Dt, 2.2f));   // (turning on the spot: gently)
	Anim(P);
}

void APaxFlow::CarryBag(FPx& P, AActor* A)
{
	if (!P.bHasBag || !P.Holder || (!P.bArr && P.bBagDown)) return;
	USkeletalMeshComponent* B = P.Body.Get();
	const float Fy = A->GetActorRotation().Yaw + 90.f;
	// (the bag hanging upright below the hand, its handle bar along the walk: a suitcase's end handle on top)
	const FQuat Q = FRotator(0.f, Fy, 0.f).Quaternion() * (P.Handle.Y < -10.f ? PXUPRIGHT : FQuat::Identity);
	// (owner 2026-11-21: the hand exactly on the handle, fixed there all the way. The holder - the bag's handle point -
	//  rides on the hand bone itself at the grip (between the palm and the closed fingers, PaxAnimInstance::GripInHand),
	//  so it moves with this frame's pose, not last frame's: no lag, no sliding in the hand. Only its turn is its own:
	//  upright, facing the walk. The bags read it after the animation (the belts tick post-update). Not while setting it
	//  down or picking it up: CarryBag is only called while carried; set down, it is let go of)
	static const FName HandR(TEXT("hand_r"));
	if (B && B->GetBoneIndex(HandR) != INDEX_NONE)
	{
		if (P.Holder->GetAttachParent() != B || P.Holder->GetAttachSocketName() != HandR)
		{
			P.Holder->AttachToComponent(B, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandR);
			P.Holder->SetUsingAbsoluteRotation(true); P.Holder->SetUsingAbsoluteScale(true);
			P.Holder->SetRelativeLocation(UPaxAnimInstance::GripInHand(B));
		}
		P.Holder->SetWorldRotation(Q);
		return;
	}
	const FVector Right = FRotator(0.f, Fy + 90.f, 0.f).Vector();
	P.Holder->SetWorldLocationAndRotation(A->GetActorLocation() + Right * 25.f + FVector(0, 0, 80.f), Q);
}

void APaxFlow::StartFade(FPx& P, AActor* A)
{
	P.FadeT = 0.f; P.bFadeSwap = false;
	static UMaterialInterface* Fade = [] { UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/PaxFade/M_PaxFade.M_PaxFade")); if (M) M->AddToRoot(); return M; }();
	if (!Fade) { ADP_WARN(TEXT("PaxFlow: no M_PaxFade (Tools/make_pax_fade_material.py) - boarding passengers vanish without fading")); return; }
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Fade, A);
	D->SetScalarParameterValue(TEXT("Up"), 0.f); D->SetScalarParameterValue(TEXT("Out"), 0.f);
	P.FadeMid = D;
	// (the light over everything they are and carry: body, face, clothes, anything held - their own meshes and
	//  whatever rides on them)
	TArray<AActor*> All; All.Add(A); A->GetAttachedActors(All, false, true);
	for (AActor* X : All) { TArray<UMeshComponent*> Ms; X->GetComponents<UMeshComponent>(Ms); for (UMeshComponent* M : Ms) if (M->IsVisible() && !M->IsA<UGroomComponent>()) M->SetOverlayMaterial(D); }
}

bool APaxFlow::StepFade(FPx& P, AActor* A, float Dt)
{
	P.FadeT += Dt;
	UMaterialInstanceDynamic* D = P.FadeMid.Get();
	TArray<AActor*> All; All.Add(A); A->GetAttachedActors(All, false, true);
	if (!D) { for (AActor* X : All) X->SetActorHiddenInGame(true); return true; }
	// 0 - 0.5 s: the light rises over them from the feet (an overlay on their own materials)
	const float Rise = 0.5f, Thin = 0.6f;
	D->SetScalarParameterValue(TEXT("Up"), FMath::SmoothStep(0.f, 1.f, FMath::Min(P.FadeT / Rise, 1.f)));
	if (P.FadeT >= Rise && !P.bFadeSwap)
	{
		// fully covered: the light itself takes their materials' place (it looks the same), so it can thin away
		P.bFadeSwap = true;
		for (AActor* X : All)
		{
			TArray<UPrimitiveComponent*> Ps; X->GetComponents<UPrimitiveComponent>(Ps);
			for (UPrimitiveComponent* C : Ps)
			{
				if (UMeshComponent* M = Cast<UMeshComponent>(C))
				{
					if (M->GetOverlayMaterial() != D) { if (!M->IsVisible()) continue; M->SetVisibility(false); continue; }   // (hair strands and the like: no overlay - gone under the light)
					for (int32 k = 0; k < M->GetNumMaterials(); ++k) M->SetMaterial(k, D);
					M->SetOverlayMaterial(nullptr); M->SetCastShadow(false);
				}
			}
		}
	}
	// 0.5 - 1.1 s: thinning away to nothing
	D->SetScalarParameterValue(TEXT("Out"), FMath::SmoothStep(0.f, 1.f, FMath::Clamp((P.FadeT - Rise) / Thin, 0.f, 1.f)));
	if (P.FadeT < Rise + Thin) return false;
	for (AActor* X : All) X->SetActorHiddenInGame(true);
	return true;
}

bool APaxFlow::Follow(FPx& P, AActor* A, float Dt)
{
	if (P.LooseT >= 0.f)
	{
		// (held by geometry: eased back along the floor to the nearest free point - half a second)
		P.LooseT += Dt; const float K = FMath::SmoothStep(0.f, 1.f, FMath::Min(P.LooseT / 0.6f, 1.f));
		FVector N = FMath::Lerp(P.LooseFrom, P.LooseTo, K); N.Z = A->GetActorLocation().Z; A->SetActorLocation(N);
		if (P.LooseT >= 0.6f) P.LooseT = -1.f;
		Anim(P); return false;
	}
	if (P.PauseT > 0.f)
	{
		const FVector Face = P.Way.IsValidIndex(P.Wi) ? P.Way[P.Wi].Face : FVector::ZeroVector;
		P.PauseT -= Dt; Idle(P, A, Dt, &Face);
		if (P.PauseT <= 0.f) ++P.Wi;
		return false;
	}
	if (P.Wi >= P.Way.Num()) { Idle(P, A, Dt); return true; }
	// (owner 2026-11-21: they know where the walls and floors are. The leg to the next waypoint walked out once on the
	//  navmesh - round the walls, through the real doorways - its corners put in before it. Not the escalator rides and
	//  not the jetways (they move; off the navmesh: straight as before), not the short hand-placed steps in the security
	//  lane, not the queues (their spot moves). No way at all: logged, and straight to the waypoint as before)
	if (!P.Way[P.Wi].bNav && P.Way[P.Wi].Mode != 2 && P.Way[P.Wi].Mode != 3 && (P.St == EPx::Walk || (P.St == EPx::Jetway && P.Wi < P.TunA)) && !(P.Do == EDo::Screened && P.Wi < 5) && NavBudget > 0 && PaxNav::Ready(GetWorld()))
	{
		--NavBudget;
		FWp& To = P.Way[P.Wi]; To.bNav = true;
		const FVector From = A->GetActorLocation();
		if (FVector::Dist2D(From, To.P) > (P.ReplanT > 0.f ? 80.f : 400.f))
		{
			TArray<FVector> Pts;
			const PaxNav::EPath R = PaxNav::Path(GetWorld(), From, To.P, Pts);
			// (a part way that stops short of the waypoint: only when it gets close, else it isn't a way)
			const bool bUse = R == PaxNav::Full || (R == PaxNav::Partial && Pts.Num() >= 2 && FVector::Dist2D(Pts.Last(), To.P) < 150.f);
			if (bUse && Pts.Num() > 2)
			{
				TArray<FWp> Corners;
				for (int32 k = 1; k < Pts.Num() - 1; ++k) { FWp C; C.P = Pts[k]; C.Mode = To.Mode; C.bGround = To.bGround; C.bNav = true; Corners.Add(C); }
				P.Way.Insert(Corners, P.Wi);
				if (P.TunB > P.TunA && P.Wi <= P.TunA) { P.TunA += Corners.Num(); P.TunB += Corners.Num(); if (P.FadeFrom != INDEX_NONE) P.FadeFrom += Corners.Num(); }
			}
			else if (!bUse && R != PaxNav::OffMesh)
			{
				const FVector Fa = PlanOf(From), Ta = PlanOf(To.P);
				const uint64 Key = (uint64(uint16(int32(Fa.X) + 2000)) << 48) | (uint64(uint16(int32(Fa.Z) + 2000)) << 32) | (uint64(uint16(int32(Ta.X) + 2000)) << 16) | uint64(uint16(int32(Ta.Z) + 2000));
				bool bSeen = false; NoWayLogged.Add(Key, &bSeen);
				if (!bSeen) ADP_LOG(TEXT("PaxFlow: no walkable way from A to B - %s from plan (%.1f, %.1f) to (%.1f, %.1f) (%s); straight to the waypoint"), *P.Id, Fa.X, Fa.Z, Ta.X, Ta.Z, *P.Stage);
			}
		}
	}
	const FWp& W = P.Way[P.Wi];
	const bool bTunnel = P.TunB > P.TunA && P.Wi >= P.TunA && P.Wi < P.TunB;
	P.bQueued = false; P.bDoorWait = false;
	const FVector Loc = A->GetActorLocation();
	FVector D = W.P - Loc; D.Z = 0.f;
	const float Dist = D.Size();
	const bool bLast = P.Wi == P.Way.Num() - 1 || W.Pause > 0.f || W.Mode == 3;
	const bool bRide = W.Mode == 2;
	P.bRideNow = bRide;   // (on an escalator: standing still while it carries them - no walking legs)
	float Want = W.Mode == 1 ? 55.f : P.Pace;
	// (late: after their flight's boarding call a fast walk, after the final call - or with the doors about to close - a
	//  run. Only while walking between places: in the desk and security queues, at the desk and in the lanes they always
	//  wait their turn (those steps never come through here at a hurry: queue spots are short legs, the lane steps slow)
	if (W.Mode == 0 && !P.bArr && Runs.IsValidIndex(P.Run) && P.Do != EDo::JoinDesk && P.Do != EDo::JoinLane && P.Do != EDo::Screened)
	{
		const FRun& Rn = Runs[P.Run];
		const bool bNotThere = P.Do != EDo::Aboard && P.St == EPx::Walk;
		if (bNotThere && Rn.bFinal) Want = FMath::Max(Want, 330.f + float(FMath::Fmod(FMath::Abs(P.Jit.Y) * 97.0, 40.0)));        // (a run)
		else if (bNotThere && Rn.bCalled) Want = FMath::Max(Want, 175.f + float(FMath::Fmod(FMath::Abs(P.Jit.Y) * 97.0, 25.0)));  // (a brisk, fast walk)
	}
	if (bLast && !bRide) Want *= FMath::Clamp(Dist / 90.f, 0.3f, 1.f);
	if (bRide) P.Speed = FMath::FInterpTo(P.Speed, 0.f, Dt, 6.f); else P.Speed = FMath::FInterpTo(P.Speed, Want, Dt, 4.f);
	// (a revolving door: there at the end of its queue - each one waiting stands 90 cm further out)
	const float Reach = W.Mode == 3 ? 30.f + 90.f * RevLine(W.Rev, W.RevSide).Who.Num() : bRide ? 12.f : 30.f;
	if (Dist < Reach)
	{
		if (W.Mode == 3) { StartRevolve(int32(&P - Pax.GetData())); return false; }
		if (W.Pause > 0.f) P.PauseT = W.Pause; else ++P.Wi;
		Anim(P);
		return P.Wi >= P.Way.Num() && P.PauseT <= 0.f;
	}
	FVector Dn = D / Dist;
	float TurnWant = 0.f; bool bHold = false, bLine = false;
	if (!bRide)
	{
		// (aware of the people round them - owner 2026-11-21: walk past the queues and away from the desk between people,
		//  the shoulders turned to slip through, and people standing step aside to let them by)
		FVector Lat = FVector::ZeroVector; float Slow = 1.f;
		const float Look = 160.f + P.Speed * 0.6f;   // (the faster they go, the further ahead they look - a runner never runs into anyone)
		const FVector Side(-Dn.Y, Dn.X, 0.f);
		for (FPx& O : Pax)
		{
			if (&O == &P) continue;
			const AActor* OA = O.A.Get(); if (!OA || O.St == EPx::Gone || O.St == EPx::Seated) continue;
			FVector Rel = OA->GetActorLocation() - Loc; if (FMath::Abs(Rel.Z) > 120.f) continue; Rel.Z = 0.f;
			const float R = Rel.Size(); if (R > Look || R < 1.f) continue;
			const float Ahead = FVector::DotProduct(Rel, Dn), Off = FVector::DotProduct(Rel, Side);
			if (Ahead < -30.f) continue;                                             // (behind: not my problem)
			const float Near = FMath::Clamp((Look - R) / Look, 0.f, 1.f);
			// (coming towards each other: both a little to their right; else away from them, sideways)
			const float Along = O.Speed > 15.f && O.St != EPx::Seated ? FVector::DotProduct(FRotator(0.f, OA->GetActorRotation().Yaw + 90.f, 0.f).Vector(), Dn) : 0.f;
			if (Along < -0.35f && Ahead > 0.f && FMath::Abs(Off) < 90.f) Lat += Side * Near * 1.2f;
			else Lat -= Side * FMath::Sign(Off == 0.f ? 1.f : Off) * Near * (Ahead > 0.f ? 1.f : 0.5f);
			// (a tight gap: the shoulders turned - the near one forward - by the spine, the feet going on)
			if (R < 85.f && Ahead > -10.f && FMath::Abs(Off) > 18.f && FMath::Abs(Off) < 62.f) TurnWant = FMath::Sign(Off) * -38.f;
			// (someone right in front going the same way, or in the narrow jetway: behind them, never pushed on)
			if (Ahead > 0.f && FMath::Abs(Off) < 26.f && R < 62.f && (Along > 0.3f || bTunnel)) { P.bQueued = true; bHold = true; }
			// (someone waiting at the gate's shut door: behind them, in the same line)
			if (O.bDoorWait && O.Run == P.Run && P.bArr == O.bArr && (P.St == EPx::Jetway || P.Do == EDo::ArrIn) && Ahead > 0.f && FMath::Abs(Off) < 45.f && R < 85.f) { P.bQueued = true; bHold = bLine = true; }
			if (Ahead > 0.f && FMath::Abs(Off) < 45.f && R < 60.f + P.Speed * 0.35f) Slow = FMath::Min(Slow, R < 70.f ? 0.3f : 0.6f);   // (someone right in front: slow down behind them until there is room to pass)
			// (standing in my way: please step aside - the side away from my line)
			const bool bStanding = O.St != EPx::Walk || O.Speed < 15.f;
			if (!bTunnel && bStanding && !O.bDoorWait && Ahead > 0.f && R < 110.f && FMath::Abs(Off) < 55.f && O.YieldT <= 0.f)
			{ O.YieldWant = Side * FMath::Sign(Off == 0.f ? 1.f : Off) * 34.f; O.YieldT = 2.2f; }
		}
		if (!Lat.IsNearlyZero()) Dn = (Dn + Lat * 0.8f).GetSafeNormal2D();
		P.Speed *= FMath::Lerp(1.f, Slow, FMath::Min(1.f, Dt * 6.f));
		if (Slow < 1.f) P.bQueued = true;
	}
	P.TurnYaw = FMath::FInterpTo(P.TurnYaw, TurnWant, Dt, 3.f);
	float Mv = bHold ? 0.f : FMath::Min(Dist, (bRide ? 50.f : FMath::Max(P.Speed, 25.f)) * Dt);
	FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(PaxFloor), false, A);
	// (never onto a bag or a belt either: the bags go their own way through the desks, hatches and belts - owner 2026-11-21)
	if (AActor* Db = ADepartureBelts::Get(GetWorld())) Q.AddIgnoredActor(Db);
	if (AActor* Ab = ABaggageBelts::Get(GetWorld())) Q.AddIgnoredActor(Ab);
	// the safety net: a knee-height look along the step against the building (never past the waypoint itself, so a
	// spot by a wall is still reached). A wall ahead: slide along it, never through; a corner: stand. Not on the rides,
	// the slow hand-placed steps (the arch, the escalator's foot) or in a jetway; the doors open for walkers, the chairs
	// are sat on - neither counts
	if (!bRide && W.Mode == 0 && !bTunnel && Mv > 0.f)
	{
		FCollisionQueryParams Qw(SCENE_QUERY_STAT(PaxWall), false, A);
		if (AActor* Db = ADepartureBelts::Get(GetWorld())) Qw.AddIgnoredActor(Db);
		if (AActor* Ab = ABaggageBelts::Get(GetWorld())) Qw.AddIgnoredActor(Ab);
		if (AActor* Dr = DoorsActor.Get()) Qw.AddIgnoredActor(Dr);
		if (AActor* St = SeatsActor.Get()) Qw.AddIgnoredActor(St);
		const FCollisionObjectQueryParams Ob(ECC_WorldStatic);
		const FVector Knee = Loc + FVector(0, 0, 45.f);
		const float Look = FMath::Min(Dist, Mv + 30.f);
		FHitResult Wh; P.ReplanT -= Dt;
		if (GetWorld()->LineTraceSingleByObjectType(Wh, Knee, Knee + Dn * Look, Ob, Qw) && FMath::Abs(Wh.ImpactNormal.Z) < 0.5f)
		{
			const FVector Wn = FVector(Wh.ImpactNormal.X, Wh.ImpactNormal.Y, 0.f).GetSafeNormal();
			// (something in the way that wasn't on their line - stepped aside for someone: the way to the next point looked up again from
			//  here, round it; meanwhile along its side)
			if (P.ReplanT <= 0.f && P.Way.IsValidIndex(P.Wi)) { P.ReplanT = 1.5f; P.Way[P.Wi].bNav = false; }
			P.HeldBy = (Wh.GetActor() ? Wh.GetActor()->GetName() : FString()) + TEXT("/") + (Wh.GetComponent() ? Wh.GetComponent()->GetName() : FString());
			FVector Sl = Dn - Wn * FVector::DotProduct(Dn, Wn); Sl.Z = 0.f;
			if (Sl.Size() < 0.2f) { const FVector Al(-Wn.Y, Wn.X, 0.f); Sl = Al * (FVector::DotProduct(Al, D) >= 0.f ? 1.f : -1.f); }   // (head on: along the wall, the waypoint's side)
			Sl = Sl.GetSafeNormal2D();
			FVector On; const bool bOnMap = PaxNav::Along(GetWorld(), Loc, Loc + D.GetClampedToMaxSize(Mv + 60.f), On) && FVector::Dist2D(On, Loc) > 4.f;   // (they know the room: along the map's edge, round the corner)
			if (bOnMap) { Dn = (On - Loc).GetSafeNormal2D(); Mv = FMath::Min(Mv, FVector::Dist2D(On, Loc)); }
			else if (!Sl.IsNearlyZero() && !GetWorld()->LineTraceTestByObjectType(Knee, Knee + Sl * (Mv + 30.f), Ob, Qw)) Dn = Sl;
			else { Mv = 0.f; P.HeldBy = (Wh.GetActor() ? Wh.GetActor()->GetName() : FString()) + TEXT("/") + (Wh.GetComponent() ? Wh.GetComponent()->GetName() : FString()); }
		}
	}
	FVector N = Loc + Dn * Mv;
	// (in the jetway: within its inside, a body off its walls, whatever the others make them do)
	float FloorZ = 0.f; bool bSl = false;
	if (bTunnel && Runs.IsValidIndex(P.Run) && Runs[P.Run].Walk.Num())
	{
		const FRun& Rw = Runs[P.Run]; const int32 k = SliceAt(Rw, N);
		const FSlice& Sl = Rw.Walk[k]; const float Off = FVector::DotProduct(FVector(N.X - Sl.C.X, N.Y - Sl.C.Y, 0.f), Sl.Rt);
		N -= Sl.Rt * (Off - FMath::Clamp(Off, -Sl.Lim, Sl.Lim)); FloorZ = Sl.C.Z; bSl = true;
	}
	// (the gate's jetway door, both ways: its leaves not open - they wait 55 cm before it, in their lane, until they are)
	if (!bRide && (P.St == EPx::Jetway || P.Do == EDo::ArrIn) && Runs.IsValidIndex(P.Run) && Runs[P.Run].Jet != INDEX_NONE)
	{
		const int32 Gi = GateRoom(Runs[P.Run].Gate);
		if (Gates.IsValidIndex(Gi) && Gates[Gi].bF2 && Gates[Gi].N > 0 && Gates[Gi].N <= 32 && ABabylonModel::GateDoorK[Gates[Gi].N] >= 0.f && ABabylonModel::GateDoorK[Gates[Gi].N] < 0.85f)
		{
			const FVector A0 = PlanOf(Loc), A1 = PlanOf(N); const double D0 = A0.Z - Gates[Gi].Jw.Y, D1 = A1.Z - Gates[Gi].Jw.Y;
			if (FMath::Abs(A0.X - Gates[Gi].Jw.X) < 3.0 && FMath::Abs(A0.Y - F2Y) < 1.5 && (D0 * D1 <= 0.0 || (FMath::Abs(D1) < 0.55 && FMath::Abs(D1) < FMath::Abs(D0))))
			{ N = Loc; P.Speed = FMath::FInterpTo(P.Speed, 0.f, Dt, 8.f); P.bQueued = true; bLine = true; P.HeldBy = TEXT("the gate's door, shut"); }
		}
	}
	P.bDoorWait = bLine;
	// on the floor: straight down from knee height (stairs, escalator steps, the jetway's slope), never onto another person
	bool bFloor = false;
	if (GetWorld()->LineTraceSingleByChannel(H, FVector(N.X, N.Y, Loc.Z + 45.f), FVector(N.X, N.Y, Loc.Z - 150.f), ECC_Visibility, Q))
	{
		const AActor* Hit = H.GetActor();
		bool bPerson = false; if (Hit) for (const FPx& O : Pax) if (O.A.Get() == Hit) { bPerson = true; break; }
		if (!bPerson) { N.Z = H.ImpactPoint.Z; bFloor = true; }
	}
	if (bSl && (!bFloor || FMath::Abs(N.Z - FloorZ) > 35.f)) { N.Z = FMath::FInterpTo(Loc.Z, FloorZ, Dt, 6.f); bFloor = true; }   // (the jetway's own floor)
	if (!bFloor) N.Z = W.bGround ? FMath::FInterpTo(Loc.Z, W.P.Z, Dt, 4.f) : FMath::FInterpTo(Loc.Z, W.P.Z, Dt, 1.f);
	A->SetActorLocation(N);
	A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, Dn.Rotation().Yaw - 90.f, 0.f), Dt, bRide ? 2.f : 2.6f));   // (a gentle turn - owner 2026-11-21: not abrupt)
	Anim(P);
	return false;
}

#pragma endregion

#pragma region Ways

namespace PaxFlowInternal
{
	/** The walking map's place (a room on that floor) a plan point is in, the smallest that holds it. */
	int32 PxPlaceAt(const FWalkMap& M, const FVector& Plan)
	{
		int32 Best = INDEX_NONE; float Ba = MAX_flt;
		for (int32 i = 0; i < M.Places.Num(); ++i)
		{
			const FWalkMap::FPlace& Pl = M.Places[i];
			if (Pl.bOutside || FMath::Abs(Pl.Y - Plan.Y) > 1.f) continue;
			if (Plan.X < Pl.X0 || Plan.X > Pl.X1 || Plan.Z < Pl.Z0 || Plan.Z > Pl.Z1) continue;
			const float Ar = (Pl.X1 - Pl.X0) * (Pl.Z1 - Pl.Z0);
			if (Ar < Ba) { Ba = Ar; Best = i; }
		}
		return Best;
	}
}

void APaxFlow::AddPlanWay(FPx& P, int32 FromPlace, const FVector& FromPlan, int32 ToPlace, const FVector& ToPlan, bool bJit)
{
	if (FromPlace == INDEX_NONE) FromPlace = PxPlaceAt(Map, FromPlan);
	if (ToPlace == INDEX_NONE) ToPlace = PxPlaceAt(Map, ToPlan);
	TArray<FVector> Pts;
	if (FromPlace == INDEX_NONE || ToPlace == INDEX_NONE || !Map.Way(FromPlace, FromPlan, ToPlace, ToPlan, Pts)) { Pts.Reset(); Pts.Add(ToPlan); }
	for (int32 k = 0; k < Pts.Num(); ++k)
	{
		FWp W; W.P = PW(Pts[k].X, Pts[k].Y, Pts[k].Z);
		if (bJit && k < Pts.Num() - 1) W.P = Jit(P, W.P);   // (side by side, not in single file - the doors are wide enough)
		P.Way.Add(W);
	}
}

void APaxFlow::Crossing(FPx& P, bool bToTerminal, float NearX)
{
	// the zebra crossing nearest NearX: its parts' ends (the roads' own list), from the car park's side to the terminal's
	TArray<FVector> Ends;
	float BestX = PXCROSS[0];
	{
		TArray<float> Xs;
		for (const TPair<FVector, FVector>& Z : ARoadNetwork::ZebraCrossings) if (FMath::Abs(PlanOf(Z.Key).Z + 46.75f) < 40.f) Xs.AddUnique(FMath::RoundToFloat((Z.Key.X + Z.Value.X) / 200.f));
		if (!Xs.Num()) for (const float X : PXCROSS) Xs.Add(X);
		BestX = Xs[0];
		for (const float X : Xs) if (FMath::Abs(X - NearX) < FMath::Abs(BestX - NearX)) BestX = X;
		for (const TPair<FVector, FVector>& Z : ARoadNetwork::ZebraCrossings)
			if (FMath::Abs((Z.Key.X + Z.Value.X) / 200.f - BestX) < 4.f && FMath::Abs(PlanOf(Z.Key).Z + 46.75f) < 40.f) { Ends.Add(Z.Key); Ends.Add(Z.Value); }
	}
	if (!Ends.Num()) for (const float Z : {-51.25f, -42.25f, -38.75f, -21.5f}) Ends.Add(FVector(BestX * 100.f, -Z * 100.f, GroundZ(BestX, Z)));
	// (world Y is minus plan z: the car park's side has the larger Y)
	Ends.Sort([bToTerminal](const FVector& A, const FVector& B) { return bToTerminal ? A.Y > B.Y : A.Y < B.Y; });
	const float Along = FMath::Clamp(float(P.Jit.X) * 0.02f, -0.9f, 0.9f) * 100.f;   // (within the crossing's 3 m)
	// (onto the pavement first, square to the crossing)
	const FVector First = Ends[0] + FVector(Along, bToTerminal ? 150.f : -150.f, 0.f);
	FWp W0; W0.P = First; W0.bGround = true; W0.P.Z = GroundZ(First.X / 100.f, -First.Y / 100.f); P.Way.Add(W0);
	for (const FVector& E : Ends) { FWp W; W.P = E + FVector(Along, 0, 0); W.bGround = true; P.Way.Add(W); }
	const FVector Last = Ends.Last() + FVector(Along, bToTerminal ? -150.f : 150.f, 0.f);
	FWp W1; W1.P = Last; W1.bGround = true; W1.P.Z = GroundZ(Last.X / 100.f, -Last.Y / 100.f); P.Way.Add(W1);
}

void APaxFlow::WayToHall(FPx& P, const FVector& From, int32 RevDoor)
{
	// through the car park to the pavement, over the forecourt at the crossing nearest, the pavement to the main entrance,
	// its two sliding doors, into the Grand Hall
	(void)From;
	P.Way.Reset(); P.Wi = 0;
	auto Add = [&](float X, float Z, bool bG) { FWp W; W.P = PW(X, 0, Z); W.bGround = bG; if (bG) W.P.Z = GroundZ(X, Z); P.Way.Add(W); };
	const float J = float(P.Jit.X) / 100.f;
	FVector C, Ax; float Rc = 0.f;
	const ATerminalDoors* Dr = RevDoor != INDEX_NONE ? Doors() : nullptr;
	if (Dr && Dr->RevolvingFrame(RevDoor, C, Ax, Rc))
	{
		// a side entrance's revolving door: the crossing nearest it, up to its kerb opening (the queue's head spot), the
		// ride round (StepRevolve - off the navmesh, like the escalators), out into the lobby, its sliding door, the hall
		const FVector Cp = PlanOf(C);
		Crossing(P, true, Cp.X + float(P.Jit.X) * 0.6f);
		const int32 Side = FVector::DotProduct(PW(Cp.X, 0, Cp.Z - 4.f) - C, Ax) >= 0.f ? 1 : -1;   // (the kerb's side)
		Add(Cp.X + J * 2.f, Cp.Z - 4.f, true);
		FWp Q; Q.P = C + Ax * Side * (Rc + 80.f); Q.bGround = true; Q.P.Z = GroundZ(PlanOf(Q.P).X, PlanOf(Q.P).Z); Q.Mode = 3; Q.bNav = true; Q.Rev = int8(RevDoor); Q.RevSide = int8(Side);
		P.Way.Add(Q);
		FWp Out; Out.P = C - Ax * Side * (Rc + 70.f); Out.bNav = true; P.Way.Add(Out);   // (just out of the drum)
		Add(Cp.X + J * 0.5f, -1.f, false); Add(Cp.X + J * 0.5f, 0.f, false); Add(Cp.X + J * 3.f, 2.5f, false);
		return;
	}
	Crossing(P, true, PXDOORX + float(P.Jit.X) * 0.6f);
	Add(PXDOORX + J * 2.f, -8.f, true); Add(PXDOORX + J, -4.f, false); Add(PXDOORX + J, -2.f, false); Add(PXDOORX + J, 0.f, false); Add(PXDOORX + J * 3.f, 2.5f, false);
}

bool APaxFlow::OutToTaxi(FPx& P)
{
	// (to-do 239b piece 4) an arrival with nothing to carry takes the taxi bus at the kerb now and then, as a car-park pick-up is
	// the rest: when it is free (its driver at the wheel, no fare, nobody else on the way to it - AGSE::TaxiClaim), one in seven.
	// Out of the hall's east doors as WayFromHall, the pavement south-west, clear of the landside staff's places behind the bus
	// (x -42.5 and east, z -19.8), to its sliding door's stand point; there AGSE::TaxiBoard takes them over (EDo::ToTaxi)
	if (P.bHasBag || P.bHome || Rng.FRand() >= 0.15f) return false;
	AGSE* G = AGSE::Find(GetWorld()); FVector Stand;
	if (!G || !G->TaxiClaim(P.Id, Stand)) return false;
	P.Way.Reset(); P.Wi = 0;
	const float J = float(P.Jit.X) / 100.f;
	auto Add = [&](float X, float Z, bool bG) { FWp W; W.P = PW(X, 0, Z); W.bGround = bG; if (bG) W.P.Z = GroundZ(X, Z); P.Way.Add(W); };
	Add(PXARRX + J * 2.f, 8.f, false); Add(PXARRX + J, -1.5f, false); Add(PXARRX + J, -4.f, false); Add(PXARRX + J, -7.f, true);
	Add(-46.5f + J * 0.3f, -17.5f, true);
	const FVector Pl = PlanOf(Stand);
	FWp W; W.P = Stand; W.bGround = true; W.P.Z = GroundZ(Pl.X, Pl.Z); P.Way.Add(W);
	P.Do = EDo::ToTaxi; P.St = EPx::Walk;
	return true;
}

void APaxFlow::WayFromHall(FPx& P, const FVector& From)
{
	// the arrivals' way out: the arrivals hall's east doors (kerb front z -4), the pavement, the crossing, a car
	(void)From;
	const float J = float(P.Jit.X) / 100.f;
	auto Add = [&](float X, float Z, bool bG) { FWp W; W.P = PW(X, 0, Z); W.bGround = bG; if (bG) W.P.Z = GroundZ(X, Z); P.Way.Add(W); };
	Add(PXARRX + J * 2.f, 8.f, false); Add(PXARRX + J, -1.5f, false); Add(PXARRX + J, -4.f, false); Add(PXARRX + J, -7.f, true);
	Crossing(P, false, PXCROSS[0]);
	FWp W; W.P = CarParkSpot(); W.bGround = true; P.Way.Add(W);
}

FVector APaxFlow::DeskSpot(int32 Desk, int32 K) const
{
	// the head of the queue where the passenger stands to put the bag on the weigh belt; the queue behind, south
	const ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
	const FVector Head = D ? D->DeskStandPoint(Desk) : PW(50.f + Desk * 2.08f, 0, 15.35f);
	return Head + FVector(0, 90.f * K, 0);
}

FVector APaxFlow::LaneSpot(int32 Lane, int32 K) const
{
	// the security lane's queue: in front of its arch along the glass, west away from it (clear of the X-ray belt)
	return PW(PXLANES[Lane] + 1.675f - 0.75f * K, 0, 31.0f);
}

void APaxFlow::WayToDesk(FPx& P, int32 Spot)
{
	P.Way.Reset(); P.Wi = 0;
	FWp W; W.P = DeskSpot(P.Desk, Spot); P.Way.Add(W);
}

void APaxFlow::WayToSecurity(FPx& P)
{
	// from the desk: a step to the side of the queue, back south, along to the walk-through between the desks, through it
	// and on to the nearer security door; no desk (checked in on the phone): straight from the entrance to the walk-through
	auto Add = [&](float X, float Z) { FWp W; W.P = PW(X, 0, Z); P.Way.Add(W); };
	float Dx = PXDOORX + float(P.Jit.X) / 20.f;
	if (P.Desk > 0) Dx = DeskSpot(P.Desk, 0).X / 100.f;
	const float Wt = Dx < 73.f ? 62.35f : 78.95f, DoorX = Wt < 70.f ? 62.f : 78.f, J = float(P.Jit.X) / 100.f;
	if (P.Desk > 0) { const float S = Wt > Dx ? 1.f : -1.f; P.Way.Reset(); P.Wi = 0; Add(Dx + S * 1.04f, 14.4f); Add(Dx + S * 1.04f, 9.8f); }
	Add(Wt + J * 1.2f, 9.8f); Add(Wt + J * 1.2f, 21.8f);
	// (the door's boarding-pass e-gates, sheet 2b: five cabinets at DoorX -2.8 .. +2.8, z 26.35..27.45, four 1.0 m lanes between
	//  them at DoorX -2.1 / -0.7 / +0.7 / +2.1 - Tools/terminal_hallfit_edit.py; into one lane, the pass on its reader facing
	//  the door, a moment, through)
	const float Lx = DoorX + (float(FMath::Clamp(int32((J + 0.45f) / 0.9f * 4.f), 0, 3)) - 1.5f) * 1.4f;
	Add(Lx, 25.6f); { FWp W; W.P = PW(Lx, 0, 26.45f); W.Pause = 1.f; W.Face = FVector(0, -1, 0); P.Way.Add(W); } Add(Lx, 28.3f);
	Add(DoorX + J * 2.f, 28.8f); Add(DoorX + J * 2.f, 30.9f);
}

void APaxFlow::WayScreening(FPx& P)
{
	// the lane: things into a tray on the X-ray belt, through the arch (a stop in it), picked up at the belt's far end,
	// then the one-way door into the airside lobby
	P.Way.Reset(); P.Wi = 0;
	const float X = PXLANES[FMath::Clamp(P.Lane, 0, 5)];
	const FVector West(-1, 0, 0), North(0, -1, 0);
	auto Add = [&](float Px, float Pz, float Pause, const FVector& Face, uint8 Mode = 0) { FWp W; W.P = PW(Px, 0, Pz); W.Pause = Pause; W.Face = Face; W.Mode = Mode; P.Way.Add(W); };
	Add(X + 0.75f, 32.7f, Rng.FRandRange(5.f, 9.f), West);
	Add(X + 1.675f, 34.6f, 0.8f, North);
	Add(X + 1.675f, 35.7f, 2.f, North, 1);
	Add(X + 1.675f, 38.0f, 0.f, North);
	Add(X + 0.75f, 39.4f, Rng.FRandRange(4.f, 7.f), West);
	const float J = float(P.Jit.X) / 100.f;
	Add(70.f + J * 3.f, 41.3f, 0.f, North); Add(70.f + J * 3.f, 42.8f, 0.f, North); Add(70.f + J * 4.f, 44.5f, 0.f, North);
}

void APaxFlow::WayUpstairs(FPx& P)
{
	// up to Floor 2: along the new airside corridor west to the escalator room and up the escalators (standing still
	// while they carry you) - the only way up (owner 2026-11-21: the airside lobby's stairs are gone)
	P.Way.Reset(); P.Wi = 0;
	auto Add = [&](float X, float H, float Z, uint8 Mode = 0) { FWp W; W.P = PW(X, H, Z); W.Mode = Mode; P.Way.Add(W); };
	const float J = float(P.Jit.X) / 100.f;
	P.bEsc = true;
	const float Foot = 16.f + AEscalatorPair::LengthAlong(), Zu = Rng.FRand() < 0.5f ? 12.33f : 16.05f;
	Add(52.f, 0, 48.f + J); Add(50.f, 0, 48.f + J); Add(14.f, 0, 48.f + J); Add(10.f + J, 0, 46.f); Add(10.f + J, 0, 44.f); Add(10.f + J, 0, 32.f); Add(10.f + J, 0, 30.f);
	Add(10.f, 0, 27.5f); Add(31.5f, 0, 23.f); Add(Foot + 2.5f, 0, Zu); Add(Foot + 0.3f, 0, Zu, 1);
	Add(16.2f, F2Y, Zu, 2);   // (riding up)
	Add(14.4f, F2Y, Zu); Add(10.f + J, F2Y, 27.5f); Add(10.f + J, F2Y, 31.5f);
	P.Stage = TEXT("along the airside corridor and up the escalators");
}

bool APaxFlow::WallSpot(const FPx& P, const FBox2D& Region, FVector& Out, float& Yaw)
{
	// a wall to lean on: found by looking round from a point in the region, a metre or more from doorways, stairs, the
	// escalators and the lifts, never glass, nobody else leaning there, and room for a body in front of it
	UWorld* W = GetWorld();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(PaxLean), false);
	for (const FPx& O : Pax) if (AActor* A = O.A.Get()) Q.AddIgnoredActor(A);
	for (int32 Try = 0; Try < 12; ++Try)
	{
		const FVector C(Rng.FRandRange(Region.Min.X, Region.Max.X), Rng.FRandRange(Region.Min.Y, Region.Max.Y), F2Z + 110.f);
		const float Start = Rng.FRandRange(0.f, 360.f);
		for (int32 k = 0; k < 8; ++k)
		{
			const FVector Dir = FRotator(0.f, Start + k * 45.f, 0.f).Vector();
			FHitResult H;
			if (!W->LineTraceSingleByChannel(H, C, C + Dir * 700.f, ECC_Visibility, Q) || FMath::Abs(H.ImpactNormal.Z) > 0.2f || H.Distance < 60.f) continue;
			if (const UPrimitiveComponent* Pc = H.GetComponent())
			{
				const UMaterialInterface* M = Pc->GetMaterial(FMath::Max(0, int32(H.ElementIndex)));
				if ((M && M->GetName().Contains(TEXT("lass"))) || Pc->GetName().Contains(TEXT("lass")) || Pc->GetName().Contains(TEXT("Door"))) continue;
			}
			const FVector N = FVector(H.ImpactNormal.X, H.ImpactNormal.Y, 0.f).GetSafeNormal();
			const FVector S = FVector(H.ImpactPoint.X, H.ImpactPoint.Y, F2Z) + N * 24.f;
			if (!Region.IsInside(FVector2D(S.X, S.Y))) continue;
			const FVector Sp = PlanOf(S);
			bool bNo = false;
			for (const TPair<FVector2f, float>& K : NoLean) if (FVector2f::Distance(K.Key, FVector2f(Sp.X, Sp.Z)) < K.Value) { bNo = true; break; }
			for (const FVector& L : LeanSpots) if (FVector::Dist2D(L, S) < 130.f) { bNo = true; break; }
			if (bNo) continue;
			FHitResult Fl;
			if (!W->LineTraceSingleByChannel(Fl, S + FVector(0, 0, 100.f), S - FVector(0, 0, 100.f), ECC_Visibility, Q) || FMath::Abs(Fl.ImpactPoint.Z - F2Z) > 30.f) continue;
			if (W->OverlapAnyTestByChannel(S + FVector(0, 0, 100.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(26.f, 60.f), Q)) continue;
			Out = FVector(S.X, S.Y, Fl.ImpactPoint.Z); Yaw = N.Rotation().Yaw;
			return true;
		}
	}
	(void)P;
	return false;
}

bool APaxFlow::WayToWait(FPx& P)
{
	// the wait for the boarding call, a mix (owner 2026-11-21): a chair at their own gate, a free chair out on the gate
	// walkway (never another gate's), or leaning on a wall; standing at their gate when none of those is free
	FRun& R = Runs[P.Run];
	AActor* A = P.A.Get(); if (!A) return false;
	const int32 Gi = GateRoom(R.Gate); if (!Gates.IsValidIndex(Gi)) return false;
	const FGate& G = Gates[Gi];
	const FVector Here = PlanOf(A->GetActorLocation());
	const FVector HerePlan(Here.X, F2Y, Here.Z);
	const int32 WalkP = Map.Find(TEXT("F2-WALK"));
	FBox2D WalkBox(ForceInit);
	if (WalkP != INDEX_NONE)
	{
		const FWalkMap::FPlace& Wp = Map.Places[WalkP];
		const FVector A0 = PW(FMath::Max(Wp.X0, G.Jw.X - 30.f), 0, Wp.Z0 + 0.8f), B0 = PW(FMath::Min(Wp.X1, G.Jw.X + 30.f), 0, Wp.Z1 - 0.8f);
		WalkBox += FVector2D(A0.X, A0.Y); WalkBox += FVector2D(B0.X, B0.Y);
	}
	const FBox2D GateIn(G.Box.Min + FVector2D(100.f, 100.f), G.Box.Max - FVector2D(100.f, 100.f));
	P.Way.Reset(); P.Wi = 0;
	if (R.bCalled) { AddPlanWay(P, INDEX_NONE, HerePlan, G.Place, FVector(G.Jw.X + float(P.Jit.X) / 50.f, F2Y, G.Jw.Y - 3.f)); P.Do = EDo::AtGateSpot; P.Stage = TEXT("to the gate - boarding has been called"); return true; }
	const float Roll = Rng.FRand();
	auto TrySeat = [&](bool bOwnGate) -> bool
	{
		TArray<int32> Free;
		for (int32 s = 0; s < ASeats::Count(); ++s)
		{
			FVector SAt, SF; float SH;
			if (!ASeats::SeatOf(s, SAt, SF, SH) || ASeats::IsTaken(s) || SH > 55.f || FMath::Abs(SAt.Z - SH - F2Z) > 60.f) continue;
			const FVector2D S2(SAt.X, SAt.Y);
			if (bOwnGate ? !G.Box.IsInside(S2) : !WalkBox.IsInside(S2)) continue;
			bool bOther = false; for (const FGate& O : Gates) if (&O != &G && O.Box.IsInside(S2)) { bOther = true; break; }
			if (!bOther) Free.Add(s);
		}
		for (int32 Try = 0; Try < 6 && Free.Num(); ++Try)
		{
			const int32 I = Free[Rng.RandRange(0, Free.Num() - 1)];
			FVector SAt, SF; float SH; ASeats::SeatOf(I, SAt, SF, SH);
			if (ASeats::Claim(A, SAt) != I) { ASeats::Release(A); Free.Remove(I); continue; }
			P.Seat = I;
			const FVector Front = PlanOf(FVector(SAt.X, SAt.Y, F2Z) + SF * 60.f);
			AddPlanWay(P, INDEX_NONE, HerePlan, INDEX_NONE, FVector(Front.X, F2Y, Front.Z));
			P.Do = EDo::AtSeat; P.Stage = bOwnGate ? TEXT("to a chair at their gate") : TEXT("to a free chair on the gate walkway");
			return true;
		}
		return false;
	};
	if (Roll < 0.5f && TrySeat(true)) return true;
	if (Roll < 0.75f && TrySeat(false)) return true;
	{
		FVector Spot; float Yaw;
		const FBox2D& Region = (Rng.FRand() < 0.5f || !WalkBox.bIsValid) ? GateIn : WalkBox;
		if (WallSpot(P, Region, Spot, Yaw))
		{
			LeanSpots.Add(Spot); P.LeanYaw = Yaw;
			const FVector Sp = PlanOf(Spot);
			AddPlanWay(P, INDEX_NONE, HerePlan, INDEX_NONE, FVector(Sp.X, F2Y, Sp.Z));
			P.Do = EDo::AtWall; P.Stage = TEXT("to a wall to lean on");
			return true;
		}
	}
	if (TrySeat(true) || TrySeat(false)) return true;
	AddPlanWay(P, INDEX_NONE, HerePlan, G.Place, FVector(Rng.FRandRange(G.Jw.X - 10.f, G.Jw.X + 10.f), F2Y, Rng.FRandRange(72.f, 78.f)));
	P.Do = EDo::AtGateSpot; P.Stage = TEXT("to stand at their gate");
	return true;
}

void APaxFlow::WayToJetway(FPx& P)
{
	// boarding: to the gate door, through it into the jetway's rotunda, along the tunnel to the very end of the cab
	FRun& R = Runs[P.Run];
	AActor* A = P.A.Get(); if (!A) return;
	const int32 Gi = GateRoom(R.Gate); if (!Gates.IsValidIndex(Gi)) return;
	const FGate& G = Gates[Gi];
	const FVector Here = PlanOf(A->GetActorLocation());
	const float Fy = G.bF2 ? F2Y : 0.f;
	P.Way.Reset(); P.Wi = 0; P.TunA = P.TunB = 0;
	AddPlanWay(P, INDEX_NONE, FVector(Here.X, Fy, Here.Z), G.Place, FVector(G.Jw.X, Fy, G.Jw.Y - 1.2f), true);
	auto Add = [&](const FVector& W) { FWp Wp; Wp.P = W; P.Way.Add(Wp); };
	Add(PW(G.Jw.X, Fy, G.Jw.Y)); Add(PW(G.Jw.X, Fy, G.Jw.Y + 1.2f));
	AJetwayRigs* J = AJetwayRigs::Get(GetWorld());
	FVector Ro, Dr, Mid, Front;
	P.FadeFrom = INDEX_NONE; P.FadeT = -1.f; P.bFadeSwap = false;
	if (J && R.Jet != INDEX_NONE && J->Frame(R.Jet, Ro, Dr) && J->CabView(R.Jet, Mid, Front))
	{
		Ro.Z = F2Z;
		P.TunA = P.Way.Num();
		// (owner 2026-11-21: no cabin yet - halfway along the tunnel they fade away, one by one, walking on as they go; its inside
		//  as it is now, as off the aircraft, the other way)
		if (Measure(R))
			for (int32 k = R.Walk.Num() - 1; k >= 0; k -= 3)
			{
				const FSlice& Sl = R.Walk[k]; Add(Sl.C - Sl.Rt * FMath::Clamp(float(P.Jit.X) * 0.4f + 8.f, -Sl.Lim, Sl.Lim));
				if (P.FadeFrom == INDEX_NONE && FVector::Dist2D(Sl.C, Mid) < FVector::Dist2D(Ro, Mid) * 0.5f) P.FadeFrom = P.Way.Num() - 1;
			}
		else { Add(Ro); Add(FMath::Lerp(Ro, Mid, 0.5f)); P.FadeFrom = P.Way.Num() - 1; Add(Mid); Add(Mid + Front.GetSafeNormal2D() * 70.f); }
		P.TunB = P.Way.Num();
		for (int32 k = P.TunA; k < P.TunB; ++k) P.Way[k].bNav = true;
	}
	P.Do = EDo::Aboard; P.Stage = TEXT("boarding - through the jetway to the aircraft");
}

void APaxFlow::WayArrivals(FPx& P)
{
	// off the aircraft: out of the gate onto the gate walkway, west to the arrivals escalators, down into the claim hall,
	// to their flight's belt
	FRun& R = Runs[P.Run];
	AActor* A = P.A.Get(); if (!A) return;
	const FVector Here = PlanOf(A->GetActorLocation());
	P.Way.Reset(); P.Wi = 0;
	const float L = AEscalatorPair::LengthAlong(), J = float(P.Jit.X) / 100.f;
	AddPlanWay(P, INDEX_NONE, FVector(Here.X, F2Y, Here.Z), Map.Find(TEXT("F2-WALK")), FVector(PXARRESC + J, F2Y, 54.f));
	auto Add = [&](float X, float H, float Z, uint8 Mode = 0) { FWp W; W.P = PW(X, H, Z); W.Mode = Mode; P.Way.Add(W); };
	Add(PXARRESC, F2Y, PXARRFOOT + L + 1.2f); Add(PXARRESC, F2Y, PXARRFOOT + L - 0.2f, 1);
	Add(PXARRESC, 0.f, PXARRFOOT + 0.25f, 2);   // (riding down)
	Add(PXARRESC, 0.f, PXARRFOOT - 1.6f);
	int32 B = ABaggageBelts::BeltOf(GetWorld(), R.Label); if (B == INDEX_NONE) B = 2;
	Add(PXCLAIM[B] + Rng.FRandRange(-6.5f, 6.5f), 0.f, 31.f - 1.8f - Rng.FRandRange(0.9f, 1.6f));
	P.Do = EDo::AtClaim; P.Stage = FString::Printf(TEXT("down the arrivals escalators to belt %d"), B + 1);
}

void APaxFlow::GoHome(int32 i, const TCHAR* Why)
{
	// (too late, or their flight is off) back out the way there is: from Floor 2 down the arrivals escalators and out of the
	// arrivals hall; from the hall or outside, the main entrance and the crossing - to a car in the car park
	FPx& P = Pax[i];
	AActor* A = P.A.Get(); if (!A) return;
	for (FLine& L : DeskLines) L.Who.Remove(i);
	for (FLine& L : LaneLines) L.Who.Remove(i);
	ASeats::Release(A); P.Seat = INDEX_NONE;
	LeanSpots.RemoveAll([&](const FVector& L) { return FVector::Dist2D(L, A->GetActorLocation()) < 60.f; });
	if (P.bBagDown && P.Bag != INDEX_NONE) { if (ADepartureBelts* D = ADepartureBelts::Get(GetWorld())) D->RemoveBag(P.Bag); P.Bag = INDEX_NONE; P.bBagDown = false; P.bHasBag = false; }   // (its bag doesn't fly without them)
	P.bHome = true; P.Way.Reset(); P.Wi = 0; P.PauseT = 0.f; P.TunA = P.TunB = 0;
	const FVector Here = PlanOf(A->GetActorLocation());
	auto Add = [&](float X, float H, float Z, uint8 Mode = 0, bool bG = false) { FWp W; W.P = PW(X, H, Z); W.Mode = Mode; W.bGround = bG; if (bG) W.P.Z = GroundZ(X, Z); P.Way.Add(W); };
	if (Here.Y > 3.f)
	{
		const float L = AEscalatorPair::LengthAlong();
		AddPlanWay(P, INDEX_NONE, FVector(Here.X, F2Y, Here.Z), Map.Find(TEXT("F2-WALK")), FVector(PXARRESC + float(P.Jit.X) / 100.f, F2Y, 54.f));
		Add(PXARRESC, F2Y, PXARRFOOT + L + 1.2f); Add(PXARRESC, F2Y, PXARRFOOT + L - 0.2f, 1);
		Add(PXARRESC, 0.f, PXARRFOOT + 0.25f, 2); Add(PXARRESC, 0.f, PXARRFOOT - 1.6f);
		WayFromHall(P, A->GetActorLocation());
	}
	else
	{
		const float J = float(P.Jit.X) / 100.f;
		if (Here.Z > -3.f) { Add(PXDOORX + J * 3.f, 0.f, 2.5f); Add(PXDOORX + J, 0.f, 0.f); Add(PXDOORX + J, 0.f, -4.f); Add(PXDOORX + J * 2.f, 0.f, -8.f, 0, true); }
		if (Here.Z > -45.f) Crossing(P, false, PXDOORX);
		FWp W; W.P = CarParkSpot(); W.bGround = true; P.Way.Add(W);
	}
	P.Do = EDo::OutCarPark; P.St = EPx::Walk; P.Stage = FString::Printf(TEXT("going home (%s)"), Why);
	ADP_LOG(TEXT("PaxFlow: %s goes home - %s"), *P.Id, Why);
}

#pragma endregion

#pragma region Each step

void APaxFlow::Arrive(int32 i)
{
	FPx& P = Pax[i];
	AActor* A = P.A.Get(); if (!A) return;
	FRun& R = Runs[P.Run];
	switch (P.Do)
	{
	case EDo::JoinDesk:
		if (!R.bTest && Clock(R) > R.DepT - 45.0 * 60.0) { ++R.Late; GoHome(i, TEXT("too late - check-in closed 45 min before")); break; }
		DeskLines[FMath::Clamp(P.Desk, 1, 20) - 1].Who.Add(i);
		P.St = EPx::DeskQ; P.Stage = FString::Printf(TEXT("in the queue at desk %d"), P.Desk);
		break;
	case EDo::JoinLane:
	{
		// the lane with the shortest queue near the door they came through
		const float X = PlanOf(A->GetActorLocation()).X;
		int32 Best = 0; float Bs = MAX_flt;
		for (int32 l = 0; l < 6; ++l) { const float S = FMath::Abs(PXLANES[l] + 1.675f - X) / 6.f + LaneLines[l].Who.Num() * 1.5f; if (S < Bs) { Bs = S; Best = l; } }
		P.Lane = Best; LaneLines[Best].Who.Add(i);
		P.St = EPx::LaneQ; P.Stage = FString::Printf(TEXT("in the queue for security lane %d"), Best + 1);
		break;
	}
	case EDo::Screened:
		++R.NLane; WayUpstairs(P); P.Do = EDo::Upstairs; P.St = EPx::Walk;
		break;
	case EDo::Upstairs:
		++R.NF2;
		if (R.bOff || R.bGone) { GoHome(i, R.bOff ? TEXT("the flight is off") : TEXT("missed the flight")); break; }
		if (!WayToWait(P)) { P.St = EPx::Wait; break; }
		P.St = EPx::Walk;
		break;
	case EDo::AtSeat:
	{
		AADPPeople* People = AADPPeople::Get(GetWorld());
		const int32 Gi = GateRoom(R.Gate);
		const FVector L = A->GetActorLocation();
		TArray<FBox2D> Area; Area.Add(FBox2D(FVector2D(L.X - 400.f, L.Y - 400.f), FVector2D(L.X + 400.f, L.Y + 400.f))); if (Gates.IsValidIndex(Gi)) Area.Add(Gates[Gi].Box);
		// (still the seat walked to: the list of seats is gathered again as models come and go)
		{ FVector Sa, Sf; float Sh; if (P.Seat != INDEX_NONE && (!ASeats::SeatOf(P.Seat, Sa, Sf, Sh) || FVector::Dist2D(Sa, L) > 160.f)) { ADP_LOG(TEXT("PaxFlow: %s - seat %d is another one now, they stay standing"), *P.Id, P.Seat); P.Seat = INDEX_NONE; } }
		if (People && P.Seat != INDEX_NONE && People->SitOn(A, P.Seat, Area, F2Z)) { P.St = EPx::Seated; P.Stage = TEXT("sitting, waiting for the boarding call"); P.Speed = 0.f; }
		else { ASeats::Release(A); P.Seat = INDEX_NONE; P.St = EPx::Wait; P.Stage = TEXT("standing, waiting for the boarding call"); }
		break;
	}
	case EDo::AtWall: P.St = EPx::Lean; P.T = 0.f; P.Fidget = Rng.FRandRange(6.f, 14.f); P.Stage = TEXT("leaning on a wall, waiting for the boarding call"); break;
	case EDo::AtGateSpot: P.St = EPx::Wait; P.Stage = R.bCalled ? TEXT("at the gate, waiting their turn to board") : TEXT("standing at the gate"); break;
	case EDo::Aboard:
		++R.Aboard; ++TotalAboard;
		ADP_LOG(TEXT("PaxFlow: %s aboard %s (%d of %d walkers; about %d of %d passengers aboard)"), *P.Id, *R.Label, R.Aboard, R.Visible, FMath::Min(R.Pax, FMath::RoundToInt(float(R.Aboard) / FMath::Max(1, R.Visible) * R.Pax)), R.Pax);
		Finish(i, TEXT("is aboard (faded away halfway along the jetway)"));
		break;
	case EDo::ArrIn: P.TunA = P.TunB = 0; WayArrivals(P); P.St = EPx::Walk; break;
	case EDo::AtClaim:
		P.St = EPx::ClaimWait; P.T = 0.f; P.Stage = TEXT("waiting at the claim belt for their bag");
		if (!P.bArrBag && OutToTaxi(P)) P.Stage = TEXT("hand luggage only - to the taxi bus at the kerb");
		else if (!P.bArrBag) { P.Way.Reset(); P.Wi = 0; WayFromHall(P, A->GetActorLocation()); P.Do = EDo::OutCarPark; P.St = EPx::Walk; P.Stage = TEXT("hand luggage only - out to the car park"); }
		break;
	case EDo::ToTaxi:
	{
		// (at the taxi bus's sliding door: the crew's now - in, a rear seat, the fare; or it is gone: on to the car park)
		AGSE* G = AGSE::Find(GetWorld());
		if (G && G->TaxiBoard(A, P.Id, P.Name)) { ++R.Out; ++TotalOut; Finish(i, TEXT("handed to the taxi bus at the kerb"), true); break; }
		ADP_WARN(TEXT("PaxFlow: %s - the taxi bus is gone or taken, out to the car park instead"), *P.Id);
		P.Way.Reset(); P.Wi = 0; Crossing(P, false, PXCROSS[0]);
		{ FWp W; W.P = CarParkSpot(); W.bGround = true; P.Way.Add(W); }
		P.Do = EDo::OutCarPark; P.St = EPx::Walk; P.Stage = TEXT("the taxi bus is gone - out to the car park");
		break;
	}
	case EDo::OutCarPark: if (P.bHome) { ++R.Home; Finish(i, TEXT("gone home from the car park")); break; } ++R.Out; ++TotalOut; Finish(i, TEXT("picked up in the car park")); break;
	default: P.St = EPx::Wait; break;
	}
}

void APaxFlow::Held(int32 i, float Dt)
{
	// the last resort (owner 2026-11-23: never through people, walls or things, never teleported, never outside): truly held by
	// geometry - not queueing, not resting, the way ahead of them free of people, and not a step in 12 s - they are eased back, at
	// most 60 cm along the same floor, to the nearest free point of the same walkable space (the jetway's inside, or the walking
	// map). Each one logged with its place and cause: a fault to be found, never the normal way on
	FPx& P = Pax[i]; AActor* A = P.A.Get(); if (!A) return;
	const FVector L = A->GetActorLocation();
	if (P.PauseT > 0.f || P.bRideNow || P.bQueued || P.LooseT >= 0.f || FVector::Dist(P.StuckAt, L) > 30.f) { P.StuckAt = L; P.StuckT = 0.f; return; }
	if ((P.StuckT += Dt) < 12.f) return;
	P.StuckT = 0.f;
	FRun& R = Runs[P.Run];
	const bool bTunnel = P.TunB > P.TunA && P.Wi >= P.TunA && P.Wi < P.TunB && R.Walk.Num();
	FVector Free = L; bool bOk = false;
	if (bTunnel) { const FSlice& Sl = R.Walk[SliceAt(R, L)]; const float Off = FVector::DotProduct(FVector(L.X - Sl.C.X, L.Y - Sl.C.Y, 0.f), Sl.Rt); Free = Sl.C + Sl.Rt * FMath::Clamp(Off, -Sl.Lim * 0.5f, Sl.Lim * 0.5f); bOk = FVector::Dist2D(Free, L) < 60.f; }
	else bOk = PaxNav::Nearest(GetWorld(), L, 60.f, Free);
	++(bTunnel ? StuckJet : StuckElse);
	const FVector Pl = PlanOf(L), Wt = P.Way.IsValidIndex(P.Wi) ? PlanOf(P.Way[P.Wi].P) : Pl;
	ADP_WARN(TEXT("PaxFlow: STUCK %s at plan (%.1f, %.1f, height %.1f)%s, next point (%.1f, %.1f, height %.1f), held by %s: %s - %s"), *P.Id, Pl.X, Pl.Z, Pl.Y, bTunnel ? TEXT(" in the jetway") : TEXT(""), Wt.X, Wt.Z, Wt.Y,
		P.HeldBy.IsEmpty() ? TEXT("-") : *P.HeldBy, *P.Stage, bOk ? *FString::Printf(TEXT("eased %.0f cm back to the free floor"), FVector::Dist2D(Free, L)) : TEXT("no free point near"));
	if (bOk) { P.LooseFrom = L; P.LooseTo = Free; P.LooseT = 0.f; }
}

int32 APaxFlow::SliceAt(const FRun& R, const FVector& At) const
{
	int32 B = 0; float Bd = MAX_flt;
	for (int32 k = 0; k < R.Walk.Num(); ++k) { const float D = FVector::DistSquared2D(R.Walk[k].C, At); if (D < Bd) { Bd = D; B = k; } }
	return B;
}

bool APaxFlow::Measure(FRun& R)
{
	if (R.bMeasured) return R.Walk.Num() > 4;
	AJetwayRigs* J = AJetwayRigs::Get(GetWorld()); const int32 Gi = GateRoom(R.Gate);
	FVector Ro, Dr, Mid, Front;
	if (!J || R.Jet == INDEX_NONE || !Gates.IsValidIndex(Gi) || J->State(R.Jet) != AJetwayRigs::EState::Docked || !J->Frame(R.Jet, Ro, Dr) || !J->CabView(R.Jet, Mid, Front)) return false;
	R.bMeasured = true;
	UWorld* W = GetWorld(); const FGate& G = Gates[Gi];
	FCollisionQueryParams Q(SCENE_QUERY_STAT(PaxJetway), true), Qs(SCENE_QUERY_STAT(PaxJetway), false);
	for (const FPx& O : Pax) if (AActor* A = O.A.Get()) { Q.AddIgnoredActor(A); Qs.AddIgnoredActor(A); }
	if (AActor* D = DoorsActor.Get()) { Q.AddIgnoredActor(D); Qs.AddIgnoredActor(D); }
	if (APlayerController* PC = W->GetFirstPlayerController()) if (APawn* Pw = PC->GetPawn()) { Q.AddIgnoredActor(Pw); Qs.AddIgnoredActor(Pw); }
	// its middle line: the cab's front, the cab, the rotunda, the gate's door - a point every 40 cm
	Ro.Z = F2Z;
	TArray<FVector> Line = {Mid + Front.GetSafeNormal2D() * 55.f, Mid, Ro, PW(G.Jw.X, F2Y, G.Jw.Y + 1.2f), PW(G.Jw.X, F2Y, G.Jw.Y)};
	for (int32 s = 0; s + 1 < Line.Num(); ++s)
	{
		const FVector A = Line[s], B = Line[s + 1]; const int32 N = FMath::Max(1, FMath::RoundToInt(FVector::Dist2D(A, B) / 40.f));
		for (int32 k = 0; k < N; ++k) { FSlice Sl; Sl.C = FMath::Lerp(A, B, float(k) / N); Sl.Rt = FVector::CrossProduct(FVector::UpVector, (B - A).GetSafeNormal2D()); R.Walk.Add(Sl); }
	}
	// (a corner is turned over a metre and a half, not at a point)
	{ TArray<FSlice> Was = R.Walk; for (int32 k = 0; k < Was.Num(); ++k) { FVector Rt = FVector::ZeroVector; for (int32 q = FMath::Max(0, k - 2); q <= FMath::Min(Was.Num() - 1, k + 2); ++q) Rt += Was[q].Rt; R.Walk[k].Rt = Rt.GetSafeNormal2D(); } }
	float Min = MAX_flt, Max = 0.f, Len = 0.f; FVector MinAt = Mid; int32 Tight = 0; FString What;
	float Z = Mid.Z;
	for (int32 k = 0; k < R.Walk.Num(); ++k)
	{
		FSlice& Sl = R.Walk[k]; FHitResult H;
		// the floor here (from chest height: never a roof), then how far it is free to each side at the knee, the hip and the chest
		const FVector P0(Sl.C.X, Sl.C.Y, k == 0 ? Sl.C.Z + 10.f : Z + 90.f);   // (the cab's middle is at chest height)
		if (W->LineTraceSingleByChannel(H, P0, P0 - FVector(0, 0, 260.f), ECC_Visibility, Q)) Z = H.ImpactPoint.Z; else Z = FMath::FInterpTo(Z, Sl.C.Z, 1.f, 0.5f);
		Sl.C.Z = Z;
		auto Free = [&](const FVector& From, float Sg)
		{
			float D = 260.f;
			for (const float Hh : {45.f, 95.f, 140.f})
			{
				FHitResult X; const FVector F0 = From + FVector(0, 0, Hh), F1 = F0 + Sl.Rt * Sg * 260.f;
				if (W->LineTraceSingleByChannel(X, F0, F1, ECC_Visibility, Q)) { if (X.Distance < D) D = X.Distance; }
				if (W->LineTraceSingleByObjectType(X, F0, F1, FCollisionObjectQueryParams(ECC_WorldStatic), Qs)) { if (X.Distance < D) { D = X.Distance; } if (X.Distance < 30.f && What.Len() < 200) { const FString Nm = X.GetComponent() ? X.GetComponent()->GetName() : FString(); if (!What.Contains(Nm)) What += TEXT(" ") + Nm; } }
			}
			return D;
		};
		// (the middle of the free width: a point that is itself in something looks for one up to 80 cm aside)
		float Lw = 0.f, Rw = 0.f; FVector Best = Sl.C; float Bw = -1.f;
		for (const float Try : {0.f, 30.f, -30.f, 55.f, -55.f, 80.f, -80.f})
		{
			const FVector At = Sl.C + Sl.Rt * Try; const float L0 = Free(At, -1.f), R0 = Free(At, 1.f);
			if (FMath::Min(L0, R0) > 3.f && L0 + R0 > Bw + 25.f) { Bw = L0 + R0; Lw = L0; Rw = R0; Best = At; }
			if (Bw > 110.f) break;
		}
		if (Bw < 0.f) { Sl.Lim = 0.f; ++Tight; continue; }
		const float Wd = FMath::Min(Lw + Rw, 320.f);
		Sl.C = Best + Sl.Rt * (FMath::Min(Rw, 160.f) - FMath::Min(Lw, 160.f)) * 0.5f;
		Sl.Lim = FMath::Max(0.f, Wd * 0.5f - 34.f);   // (a body's half width and a hand's breadth)
		if (Wd < Min) { Min = Wd; MinAt = Sl.C; }
		Max = FMath::Max(Max, Wd); if (Wd < 70.f) ++Tight;
	}
	for (int32 k = 1; k + 1 < R.Walk.Num(); ++k) { const FVector C = (R.Walk[k - 1].C + R.Walk[k].C * 2.f + R.Walk[k + 1].C) * 0.25f; R.Walk[k].C = FVector(C.X, C.Y, R.Walk[k].C.Z); }
	for (int32 k = 1; k < R.Walk.Num(); ++k) Len += FVector::Dist(R.Walk[k - 1].C, R.Walk[k].C);
	const FVector Mp = PlanOf(MinAt);
	ADP_LOG(TEXT("PaxFlow: %s's jetway measured inside - %.1f m, %.2f to %.2f m free, narrowest at plan (%.1f, %.1f), %d of %d places under 0.7 m%s%s"), *R.Label, Len / 100.f, Min / 100.f, Max / 100.f, Mp.X, Mp.Z, Tight, R.Walk.Num(), What.IsEmpty() ? TEXT("") : TEXT("; close on it:"), *What);
	return R.Walk.Num() > 4;
}

void APaxFlow::StepPx(int32 i, float Dt)
{
	FPx& P = Pax[i];
	AActor* A = P.A.Get();
	if (!A) { if (P.St != EPx::Gone) { P.St = EPx::Gone; FreePx.Add(i); } return; }
	FRun& R = Runs[P.Run];
	AADPPeople* People = AADPPeople::Get(GetWorld());
	P.T += Dt;
	if ((R.bOff || R.bGone) && !P.bArr && P.St == EPx::Seated && People) { People->StandFrom(A); P.St = EPx::Wait; return; }
	if ((R.bOff || R.bGone) && !P.bHome && !P.bArr && P.St != EPx::Revolve && P.St != EPx::Jetway && !(People && People->IsHeld(A)))
	{
		// (in the lanes, the corridor and on the escalators they go on to Floor 2 first: the way out is down the arrivals escalators)
		const FVector L = PlanOf(A->GetActorLocation());
		const bool bLanes = L.Y < 3.f ? L.Z > 27.f : P.bRideNow || P.Do == EDo::Upstairs;
		if (!bLanes || P.St == EPx::LaneQ) { GoHome(i, R.bOff ? TEXT("the flight is off") : TEXT("missed the flight")); return; }
	}
	// the boarding call heard: their turn comes (a free walker gets up and goes when it is due)
	if (!P.bArr && R.bCalled && P.BoardAt < 0.0) P.BoardAt = Clock(R) + 20.0;   // (came after the call)
	const bool bDue = !P.bArr && R.bCalled && Clock(R) >= P.BoardAt;
	switch (P.St)
	{
	case EPx::Walk:
		CarryBag(P, A);
		if (Follow(P, A, Dt)) { Arrive(i); break; }
		Held(i, Dt);
		break;
	case EPx::Revolve: StepRevolve(i, Dt); break;
	case EPx::DeskQ:
	{
		const TArray<int32>& L = DeskLines[P.Desk - 1].Who;
		const int32 K = L.IndexOfByKey(i);
		const FVector To = DeskSpot(P.Desk, FMath::Max(0, K));
		CarryBag(P, A);
		if (FVector::Dist2D(To, A->GetActorLocation()) > 40.f) { P.Way.Reset(); P.Wi = 0; FWp W; W.P = To; P.Way.Add(W); Follow(P, A, Dt); }   // (40 cm: Follow calls it arrived at 30 - with 25 here the queue head stood 25-30 cm off for ever and nobody reached a desk, owner 2026-11-21)
		else
		{
			const FVector North(0, -1, 0); Idle(P, A, Dt, &North);
			if (K == 0 && R.bPlayer && !R.bTest && StaffDesk < 1)
			{
				P.Stage = FString::Printf(TEXT("at the head of the queue at desk %d - nobody at the desk"), P.Desk);
				if (UADPSim* Sim = UADPSim::Get(this)) ADPSchedule::Tell(Sim->World, Sim->Now(), TEXT("desk:") + R.Label, TEXT("Check-in is closed"), FString::Printf(TEXT("%s's passengers stand at the desks: no passenger service agent on shift. Hire them under Crew."), *R.Label));
			}
			else if (K == 0) { P.St = EPx::Desk; P.T = 0.f; P.Stage = FString::Printf(TEXT("at desk %d - the bag on the weigh belt"), P.Desk); }
		}
		break;
	}
	case EPx::Desk:
	{
		const FVector North(0, -1, 0); Idle(P, A, Dt, &North);
		CarryBag(P, A);
		ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
		if (!P.bBagDown && P.T > 1.2f && D && P.Bag != INDEX_NONE) { D->SetDownOnDesk(P.Bag, P.Desk); P.bBagDown = true; P.bHasBag = false; if (P.Holder) P.Holder->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform); if (R.bTest || R.Spawned <= 3) ADP_LOG(TEXT("PaxFlow: %s sets bag #%d on desk %d's weigh belt"), *P.Id, P.Bag, P.Desk); }
		if (P.T > (R.bTest ? 12.f : 30.f) + float(P.Jit.Y) / 10.f)
		{
			DeskLines[P.Desk - 1].Who.Remove(i); ++R.NDesk;
			WayToSecurity(P); P.Do = EDo::JoinLane; P.St = EPx::Walk; P.Stage = TEXT("checked in - to security through the walk-through");
		}
		break;
	}
	case EPx::LaneQ:
	{
		const int32 K = LaneLines[P.Lane].Who.IndexOfByKey(i);
		const FVector To = LaneSpot(P.Lane, FMath::Max(0, K));
		if (FVector::Dist2D(To, A->GetActorLocation()) > 40.f) { P.Way.Reset(); P.Wi = 0; FWp W; W.P = To; P.Way.Add(W); Follow(P, A, Dt); }   // (40 cm: Follow calls it arrived at 30 - with 25 here the queue head stood 25-30 cm off for ever and nobody reached a desk, owner 2026-11-21)
		else
		{
			const FVector North(0, -1, 0); Idle(P, A, Dt, &North);
			// (one at a time through the lane: the next steps up when the one ahead is through the arch)
			bool bClear = true; for (const FPx& O : Pax) if (&O != &P && O.St == EPx::Walk && O.Do == EDo::Screened && O.Lane == P.Lane && O.Wi < 3) { bClear = false; break; }
			if (K == 0 && R.bPlayer && !R.bTest && StaffLane < 1)
			{
				if (UADPSim* Sim = UADPSim::Get(this)) ADPSchedule::Tell(Sim->World, Sim->Now(), TEXT("lane"), TEXT("Security is closed"), FString::Printf(TEXT("%s's passengers wait at security: no security officer on shift. Hire them under Crew."), *R.Label));
			}
			else if (K == 0 && bClear) { LaneLines[P.Lane].Who.Remove(i); WayScreening(P); P.Do = EDo::Screened; P.St = EPx::Walk; P.Stage = FString::Printf(TEXT("security lane %d - X-ray belt, the arch"), P.Lane + 1); }
		}
		break;
	}
	case EPx::Wait:
		Idle(P, A, Dt);
		if (bDue) P.St = EPx::Call;
		break;
	case EPx::Lean:
	{
		// leaning back on the wall; now and then a little shift of weight (arm poses: with the pose library later)
		P.Speed = FMath::FInterpTo(P.Speed, 0.f, Dt, 5.f); Anim(P);
		if ((P.Fidget -= Dt) <= 0.f) { P.Fidget = Rng.FRandRange(6.f, 14.f); P.LeanYaw += Rng.FRandRange(-6.f, 6.f); }
		A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, P.LeanYaw - 90.f, -5.f), Dt, 2.f));
		if (bDue) { A->SetActorRotation(FRotator(0.f, P.LeanYaw - 90.f, 0.f)); LeanSpots.RemoveAll([&](const FVector& L) { return FVector::Dist2D(L, A->GetActorLocation()) < 60.f; }); P.St = EPx::Call; }
		break;
	}
	case EPx::Seated:
		if (bDue && People) { People->StandFrom(A); P.St = EPx::Call; P.Stage = TEXT("getting up - their turn to board"); }
		break;
	case EPx::Call:
	{
		if (People && People->IsHeld(A)) break;   // (still getting up)
		// the jetway must be at the aircraft (the test: after 45 s they go in anyway)
		AJetwayRigs* J = AJetwayRigs::Get(GetWorld());
		const bool bDocked = R.Jet == INDEX_NONE || (J && J->State(R.Jet) == AJetwayRigs::EState::Docked);
		if (R.bNoShowGo) { GoHome(i, TEXT("missed the flight")); break; }   // (the doors are being closed)
		if (!bDocked) R.DockWait += Dt / FMath::Max(1, R.Spawned);
		if (!bDocked) { if (R.DockWait > 60.f && !R.bDockNoted) { R.bDockNoted = true; ADP_LOG(TEXT("PaxFlow: %s's boarding waits at gate %d for the jetway to dock"), *R.Label, R.Gate); } Idle(P, A, Dt); break; }
		P.Seat = INDEX_NONE;
		WayToJetway(P); P.St = EPx::Jetway;
		break;
	}
	case EPx::Jetway:
	{
		// along the jetway; past its halfway point they fade away over a second (never a pop), and only then are they
		// counted aboard - the aircraft (SimTraffic, BoardingDone) waits for that
		// (too late at the gate's door: home - never into a jetway that is going)
		if ((R.bNoShowGo || R.bOff || R.bGone) && P.FadeT < 0.f && !PastDoor(P)) { GoHome(i, R.bOff ? TEXT("the flight is off") : TEXT("missed the flight")); break; }
		// (closed without them while they were in it, before its halfway point: back along the way they came, out of the gate's door)
		if (R.bNoShowGo && P.FadeT < 0.f && !P.bHome && !(P.FadeFrom != INDEX_NONE && P.Wi > P.FadeFrom))
		{
			const int32 Door = FMath::Max(0, (P.TunB > P.TunA ? P.TunA : P.Way.Num()) - 3);
			TArray<FWp> Out; int32 nT = 0;
			for (int32 k = FMath::Min(P.Wi, P.Way.Num()) - 1; k >= Door; --k) { Out.Add(P.Way[k]); if (P.TunB > P.TunA && k >= P.TunA) ++nT; }
			P.Way = MoveTemp(Out); P.Wi = 0; P.TunA = 0; P.TunB = nT; P.PauseT = 0.f; P.FadeFrom = INDEX_NONE; P.bHome = true;
			P.Stage = TEXT("missed the flight - walking back out of the jetway");
			ADP_LOG(TEXT("PaxFlow: %s turns back in %s's jetway - the doors are closed without them"), *P.Id, *R.Label);
		}
		const bool bEnd = Follow(P, A, Dt);
		Held(i, Dt);
		if (P.bHome && P.FadeT < 0.f) { if (bEnd) GoHome(i, TEXT("missed the flight")); break; }
		if (P.FadeT < 0.f && (bEnd || (P.FadeFrom != INDEX_NONE && P.Wi > P.FadeFrom))) StartFade(P, A);
		if (P.FadeT >= 0.f && StepFade(P, A, Dt)) Arrive(i);
		break;
	}
	case EPx::ClaimWait:
	{
		ABaggageBelts* B = ABaggageBelts::Get(GetWorld());
		FVector At; bool bOn = false;
		const bool bKnown = B && B->PaxBag(P.Id, At, bOn);
		const FVector Toward = bOn ? (At - A->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
		Idle(P, A, Dt, bOn ? &Toward : nullptr);
		if (bOn && FVector::Dist2D(At, A->GetActorLocation()) < 900.f) { P.St = EPx::ToBag; P.Stage = TEXT("their bag is coming - to take it off"); break; }
		if (!bKnown || P.T > (R.bTest ? 300.f : 2700.f))   // (real: the last bag 30-40 min after on-blocks)
		{
			ADP_WARN(TEXT("PaxFlow: %s - %s, going on without it"), *P.Id, bKnown ? TEXT("no bag on the belt after the wait") : TEXT("their bag isn't in the claim system"));
			if (OutToTaxi(P)) P.Stage = TEXT("no bag - to the taxi bus at the kerb");
			else { P.Way.Reset(); P.Wi = 0; WayFromHall(P, A->GetActorLocation()); P.Do = EDo::OutCarPark; P.St = EPx::Walk; P.Stage = TEXT("out to the car park (no bag)"); }
		}
		break;
	}
	case EPx::ToBag:
	{
		ABaggageBelts* B = ABaggageBelts::Get(GetWorld());
		FVector At; bool bOn = false;
		if (!B || !B->PaxBag(P.Id, At, bOn) || !bOn || FVector::Dist2D(At, A->GetActorLocation()) > 1300.f) { P.St = EPx::ClaimWait; break; }
		// (to the belt's edge by the bag: out from the belt's middle line)
		const FVector Pl = PlanOf(At);
		int32 Bi = 0; for (int32 b = 1; b < 5; ++b) if (FMath::Abs(PXCLAIM[b] - Pl.X) < FMath::Abs(PXCLAIM[Bi] - Pl.X)) Bi = b;
		const FVector C = PW(FMath::Clamp(Pl.X, PXCLAIM[Bi] - 7.5f, PXCLAIM[Bi] + 7.5f), 0, 31.f);
		const FVector Out = FVector(At.X - C.X, At.Y - C.Y, 0.f).GetSafeNormal();
		const FVector To = FVector(At.X, At.Y, A->GetActorLocation().Z) + Out * 95.f;
		if (FVector::Dist2D(At, A->GetActorLocation()) < 125.f)
		{
			if (B->TakePaxBag(P.Id, P.Holder, &P.Handle))
			{
				P.bHasBag = true; ++R.NBag; CarryBag(P, A);
				if (R.bTest || R.Out < 3) ADP_LOG(TEXT("PaxFlow: %s takes their own bag off the belt"), *P.Id);
				P.Way.Reset(); P.Wi = 0; WayFromHall(P, A->GetActorLocation()); P.Do = EDo::OutCarPark; P.St = EPx::Walk; P.Stage = TEXT("bag in hand - out to the car park");
			}
			break;
		}
		P.Way.Reset(); P.Wi = 0; FWp W; W.P = To; P.Way.Add(W); Follow(P, A, Dt);
		break;
	}
	default: break;
	}
}

#pragma endregion

#pragma region Running

void APaxFlow::Tick(float Dt)
{
	Super::Tick(Dt);
	Dt = FMath::Min(Dt, 0.1f);
	RealT += Dt;
	{ const UADPSim* Sim = UADPSim::Get(this); SimT = Sim && Sim->IsRunning() ? PxSec(Sim->Now()) : RealT; }
	PaxNav::Kick(GetWorld());   // (the navmesh started as early as the airport is in - PaxNav.h)
	if (!bReady)
	{
		if ((ReadyIn -= Dt) > 0.f) return;
		ReadyIn = 2.f;
		bReady = Prepare();
		if (!bReady) return;
	}
	ADP_PERF_SCOPE("pax flow: the passengers' trips (each frame)", 2.f);
	++Frame;
	NavBudget = 4;   // (new navmesh legs this frame - the rest walk straight a frame longer)
	PaxNav::Ready(GetWorld());   // (each frame: "navmesh ready" logged the moment it is, or a warning if it never comes)
	// (-PaxFlowTest: the first passengers wait for the navmesh rather than walk straight through walls)
	if (bTest && !bTestMade && !PaxNav::Ready(GetWorld()) && !PaxNav::Failed(GetWorld()))
	{
		static double NextSay = 0.0;
		if (RealT >= NextSay) { NextSay = RealT + 10.0; ADP_LOG(TEXT("PaxFlow: test passengers wait for the navmesh (%d tile jobs left)"), PaxNav::JobsLeft(GetWorld())); }
		return;
	}
	if ((NavSweepIn -= Dt) <= 0.f) { NavSweepIn = 5.f; PaxNav::Sweep(GetWorld()); if (!DoorsActor.IsValid()) for (TActorIterator<ATerminalDoors> It(GetWorld()); It; ++It) { DoorsActor = *It; break; } if (!SeatsActor.IsValid()) for (TActorIterator<ASeats> It(GetWorld()); It; ++It) { SeatsActor = *It; break; } }   // (things spawned since: off the navmesh)
	SpawnGap -= Dt;
	if (bTest && !bTestMade) MakeTestRuns();
	// (tests: after the revolving door ride, a few seconds more on the rider, then back to who was followed before)
	if (RevReturnIn > 0.f && (RevReturnIn -= Dt) <= 0.f)
	{
		if (Pax.IsValidIndex(RevReturn) && Pax[RevReturn].St != EPx::Gone && Pax[RevReturn].A.IsValid()) { Watched = RevReturn; ADP_LOG(TEXT("PaxFlow: the camera goes back to %s - %s"), *Pax[Watched].Id, *Pax[Watched].Stage); }
		RevReturn = INDEX_NONE;
	}
	// (tests: after a while on someone using a phone at the gate, back to who was followed before)
	if (PhoneShowIn > 0.f && (PhoneShowIn -= Dt) <= 0.f)
	{
		PhoneWho = INDEX_NONE;
		if (Pax.IsValidIndex(PhoneReturn) && Pax[PhoneReturn].St != EPx::Gone && Pax[PhoneReturn].A.IsValid()) { Watched = PhoneReturn; ADP_LOG(TEXT("PaxFlow: the camera goes back to %s - %s"), *Pax[Watched].Id, *Pax[Watched].Stage); }
		PhoneReturn = INDEX_NONE;
	}
	if ((PollIn -= Dt) <= 0.f) { PollIn = 5.f; Poll(); }
	for (int32 r = 0; r < Runs.Num(); ++r) StepRun(r, Dt);
	GateDoors(Dt);
	// the walkers: close ones every frame, far ones every fourth (they keep going at the same speed)
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector Eye = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	ATerminalDoors::Openers.Reset();
	{
		// (owner 2026-11-23: the doors from the Grand Hall to security stand open from the first passenger on the hall's ground
		//  floor to the last - at check-in, in the queues, on their way, in the doorway or coming to it from the lanes - and
		//  shut by themselves 4 s after the last has gone; counted four times a second, passengers only)
		if ((HallIn -= Dt) <= 0.f)
		{
			HallIn = 0.25f; int32 N = 0;
			for (const FPx& Q : Pax)
			{
				if (Q.St == EPx::Gone) continue;
				const AActor* QA = Q.A.Get(); if (!QA) continue;
				const FVector L = QA->GetActorLocation(); const float X = L.X / 100.f, Z = -L.Y / 100.f;
				if (L.Z > 350.f || L.Z < -150.f) continue;
				if ((X > 42.f && X < 114.f && Z > -0.5f && Z < 30.f) || (X > 57.f && X < 83.f && Z >= 30.f && Z < 33.f)) ++N;
			}
			const bool bWas = ATerminalDoors::bHoldSecurity;
			if (N) HallQuiet = 0.f; else HallQuiet += 0.25f;
			ATerminalDoors::bHoldSecurity = N > 0 || (bWas && HallQuiet < 4.f);
			if (bWas != ATerminalDoors::bHoldSecurity) ADP_LOG(TEXT("PaxFlow: the security doors %s (%d passengers on the Grand Hall's floor)"), bWas ? TEXT("shut - the hall is empty") : TEXT("open and stay open"), N);
		}
		bool bAny = false; for (const FPx& Q : Pax) if (Q.St != EPx::Gone) { bAny = true; break; }
		ATerminalDoors::bHoldEntrances = bAny;
	}
	for (int32 i = 0; i < Pax.Num(); ++i)
	{
		FPx& P = Pax[i];
		if (P.St == EPx::Gone) continue;
		P.Acc += Dt;
		const AActor* A = P.A.Get();
		const bool bNear = A && FVector::DistSquared(A->GetActorLocation(), Eye) < FMath::Square(6000.f);
		if (!bNear && (Frame + i) % 4 != 0 && i != Watched && P.St != EPx::Revolve) continue;   // (at a revolving door: every frame - its windows are short)
		const float Step = FMath::Min(P.Acc, 0.3f); P.Acc = 0.f;
		StepPx(i, Step);
		// the idle movements (owner 2026-11-21, PaxAnimInstance.h): only near the camera (within 60 m: the close ones above, every frame), standing
		// still - a desk or security queue, at the desk once the bag is on the belt, waiting at the gate or for the jetway, at
		// the claim belt; never walking, riding an escalator, in a revolving door or while setting the bag down. A bag
		// in the right hand: only movements that leave the right arm alone. After 30 s still, now and then one
		if (Pax.IsValidIndex(i))
		{
			FPx& Q = Pax[i];
			if (UPaxAnimInstance* Pa = Q.Body.IsValid() ? Cast<UPaxAnimInstance>(Q.Body->GetAnimInstance()) : nullptr)
			{
				const bool bStand = Q.St == EPx::DeskQ || Q.St == EPx::LaneQ || Q.St == EPx::Wait || Q.St == EPx::Call || Q.St == EPx::ClaimWait || (Q.St == EPx::Desk && Q.bBagDown && Q.T > 3.f);
				const bool bStill = bNear && bStand && !Q.bRideNow && Q.MoveSpd < 15.f && Q.YieldT <= 0.f;
				Pa->IdleFidgets(Step, bStill, false, Q.bHasBag && !(!Q.bArr && Q.bBagDown));
			}
		}
		// waiting at the gate (sat, standing or leaning; owner 2026-11-21): after 20-60 s the phone comes out and stays out a
		// long while (45 s - 3 min; a tablet owner now and then the tablet instead), away again, a while later out again;
		// put away for the boarding call (APaxProps: both hands, the head down at it)
		if (Pax.IsValidIndex(i))
		{
			FPx& Q = Pax[i];
			AActor* QA = Q.A.Get();
			const bool bGate = QA && !Q.bArr && (Q.St == EPx::Seated || Q.St == EPx::Lean || (Q.St == EPx::Wait && Q.MoveSpd < 15.f));
			const bool bTestRun = Runs.IsValidIndex(Q.Run) && Runs[Q.Run].bTest;
			if (bGate)
			{
				if (Q.DevIn < 0.f && !Q.Dev) Q.DevIn = bTestRun ? Rng.FRandRange(3.f, 8.f) : Rng.FRandRange(20.f, 60.f);
				if (!Q.Dev && (Q.DevIn -= Step) <= 0.f)
				{
					Q.Dev = APaxProps::HasTablet(QA) && Rng.FRand() < 0.4f ? 2 : 1;
					Q.DevFor = Rng.FRandRange(45.f, 180.f);
					if (Q.Dev == 2) APaxProps::HoldTablet(QA, true); else APaxProps::HoldPhone(QA, true);
					if (bTestRun) ADP_LOG(TEXT("PaxFlow: %s takes out their %s (%s)"), *Q.Id, Q.Dev == 2 ? TEXT("tablet") : TEXT("phone"), *Q.Stage);
				}
				else if (Q.Dev && (Q.DevFor -= Step) <= 0.f)
				{
					APaxProps::HoldPhone(QA, false); APaxProps::HoldTablet(QA, false);
					Q.Dev = 0; Q.DevIn = Rng.FRandRange(10.f, 35.f);
				}
			}
			else if (Q.Dev || Q.DevIn >= 0.f)
			{
				if (Q.Dev && QA) { APaxProps::HoldPhone(QA, false); APaxProps::HoldTablet(QA, false); }
				Q.Dev = 0; Q.DevIn = -1.f;
			}
		}
		if (Pax.IsValidIndex(i)) if (const AActor* A2 = Pax[i].A.Get()) if (Pax[i].St == EPx::Walk || Pax[i].St == EPx::Jetway || Pax[i].St == EPx::Revolve) ATerminalDoors::Openers.Add(A2->GetActorLocation());   // (also the revolving doors' presence sensor)
	}
	// now and then: how many are where
	if ((SayIn -= Dt) <= 0.f)
	{
		SayIn = 30.f;
		int32 N[13] = {}; for (const FPx& P : Pax) ++N[(int32)P.St];
		const int32 Live = Walking();
		if (Live || Runs.Num()) ADP_LOG(TEXT("PaxFlow: %d about - %d walking, %d in desk queues, %d at desks, %d at security, %d waiting standing, %d sitting, %d leaning, %d boarding, %d at the claim; %d aboard, %d out so far; %d flights"),
			Live, N[(int32)EPx::Walk], N[(int32)EPx::DeskQ], N[(int32)EPx::Desk], N[(int32)EPx::LaneQ], N[(int32)EPx::Wait], N[(int32)EPx::Seated], N[(int32)EPx::Lean], N[(int32)EPx::Call] + N[(int32)EPx::Jetway], N[(int32)EPx::ClaimWait] + N[(int32)EPx::ToBag], TotalAboard, TotalOut, Runs.Num());
	}
	// (the runs stay listed - indices are held by the walkers; a finished one costs nothing each frame but its check)
}

void APaxFlow::Counts(TArray<FCount>& Out) const
{
	for (int32 r = 0; r < Runs.Num(); ++r)
	{
		const FRun& R = Runs[r]; FCount C;
		C.Label = R.Label; C.bArr = R.bArr; C.Pax = R.Pax; C.Walkers = R.Visible; C.Spawned = R.Spawned; C.Bags = R.NBags; C.Desk = R.NDesk; C.Lane = R.NLane; C.F2 = R.NF2; C.Aboard = R.Aboard; C.Bag = R.NBag; C.Out = R.Out; C.Late = R.Late; C.Home = R.Home; C.bCalled = R.bCalled; C.bOff = R.bOff; C.CallBefore = R.CallBefore;
		TMap<FString, int32> St;
		for (const FPx& P : Pax) if (P.St != EPx::Gone && P.Run == r) { ++C.About; ++St.FindOrAdd(P.Stage); if (P.TunB > P.TunA && P.Wi >= P.TunA && P.Wi < P.TunB) ++C.Jetway; }
		C.Jet = R.Jet;
		for (const auto& K : St) C.Stages += FString::Printf(TEXT("%s%d %s"), C.Stages.IsEmpty() ? TEXT("") : TEXT("; "), K.Value, *K.Key);
		Out.Add(C);
	}
}

void APaxFlow::JetwayLine(int32 Jet, TArray<FVector>& Out) const
{
	Out.Reset();
	for (int32 r = Runs.Num() - 1; r >= 0; --r) if (Runs[r].Jet == Jet && Runs[r].Walk.Num() > 4 && !Runs[r].bGone) { for (const FSlice& Sl : Runs[r].Walk) Out.Add(Sl.C); return; }
}

void APaxFlow::Scenes(TArray<FScene>& Out) const
{
	for (int32 i = 0; i < Pax.Num(); ++i)
	{
		const FPx& P = Pax[i]; const AActor* A = P.A.Get();
		if (P.St == EPx::Gone || !A || !Runs.IsValidIndex(P.Run)) continue;
		FScene S; S.Id = P.Id; S.At = A->GetActorLocation(); S.Face = FRotator(0.f, A->GetActorRotation().Yaw + 90.f, 0.f).Vector(); S.Who = A; S.News = -P.T;
		const FVector L = PlanOf(S.At); const FRun& R = Runs[P.Run];
		if (P.bArr)
		{
			if (P.Do == EDo::ArrIn) { S.Kind = 6; if (P.TunB > 2 && P.Wi < P.TunB - 1) S.Jet = R.Jet; }
			else if (P.St == EPx::ClaimWait || P.St == EPx::ToBag) S.Kind = 8;
			else if (P.Do == EDo::OutCarPark) S.Kind = 9;
			else { S.Kind = 7; if (L.Z > 58.f) continue; }
		}
		else if (P.St == EPx::Jetway) { S.Kind = 5; if (P.FadeFrom != INDEX_NONE && P.Wi >= P.FadeFrom - 1) S.Jet = R.Jet; }
		else if (P.St == EPx::Seated || P.St == EPx::Lean || P.St == EPx::Wait || P.St == EPx::Call) S.Kind = 4;
		else if (P.St == EPx::Desk || P.St == EPx::DeskQ) S.Kind = 1;
		else if (P.St == EPx::LaneQ || P.Do == EDo::Screened) S.Kind = 2;
		else if (P.Do == EDo::Upstairs) { S.Kind = 3; if (!P.bRideNow && L.Z > 25.f) continue; }
		else if (P.St == EPx::Walk && L.Y < 3.f) S.Kind = 0;
		else continue;
		S.Line = FString::Printf(TEXT("%s    %s"), *P.Id, *P.Stage);
		Out.Add(S);
	}
}

int32 APaxFlow::Walking() const { int32 N = 0; for (const FPx& P : Pax) if (P.St != EPx::Gone) ++N; return N; }

bool APaxFlow::TestDone() const
{
	const FRun* D = Runs.FindByPredicate([](const FRun& R) { return R.Key == TEXT("T:DEP"); });
	const FRun* A = Runs.FindByPredicate([](const FRun& R) { return R.Key == TEXT("T:ARR"); });
	return D && A && D->Spawned >= D->Visible && D->Aboard >= D->Spawned && A->Spawned >= A->Visible && A->Out >= A->Spawned;
}

bool APaxFlow::WatchPoints(TArray<FVector>& Pts, FVector& Side) const
{
	// one passenger followed from the start of their trip to its end, then the newest one still on the move
	APaxFlow* Me = const_cast<APaxFlow*>(this);
	auto Moving = [](const FPx& P) { return P.St != EPx::Gone && P.St != EPx::Seated && P.St != EPx::Lean && P.St != EPx::Wait && P.A.IsValid(); };
	// (tests: the first walker to come to a revolving door is followed through it - the waiting, the ride, the step out)
	if (bTest && !bRevShown)
		for (int32 i = 0; i < Pax.Num(); ++i)
			if (Pax[i].St == EPx::Revolve && Pax[i].A.IsValid())
			{
				Me->bRevShown = true; Me->RevReturn = Watched; Me->Watched = i;
				ADP_LOG(TEXT("PaxFlow: the camera follows %s (%s) through revolving door %s"), *Pax[i].Id, *Pax[i].Name, Doors() ? *Doors()->RevolvingId(Pax[i].Rev) : TEXT("?"));
				break;
			}
	// (tests: the first one at the gate with a phone or tablet out in their hands is shown for 12 s - the device, the
	//  hands, the head bent down to it - seen from their front right)
	if (bTest && !bPhoneShown && RevReturn == INDEX_NONE)
		if (const APaxProps* Props = APaxProps::Find(GetWorld()))
			for (int32 i = 0; i < Pax.Num(); ++i)
			{
				FVector Dv;
				if (Pax[i].Dev && Pax[i].A.IsValid() && Props->UsingPoint(Pax[i].A.Get(), Dv))
				{
					Me->bPhoneShown = true; Me->PhoneWho = i; Me->PhoneReturn = Watched; Me->PhoneShowIn = 12.f;
					ADP_LOG(TEXT("PaxFlow: the camera shows %s (%s) using their %s - %s"), *Pax[i].Id, *Pax[i].Name, Pax[i].Dev == 2 ? TEXT("tablet") : TEXT("phone"), *Pax[i].Stage);
					break;
				}
			}
	if (Pax.IsValidIndex(PhoneWho) && PhoneShowIn > 0.f && Pax[PhoneWho].A.IsValid())
	{
		const AActor* PA = Pax[PhoneWho].A.Get();
		const FVector L = PA->GetActorLocation(); Pts.Reset();
		FVector Dv; if (const APaxProps* Props = APaxProps::Find(GetWorld())) if (Props->UsingPoint(PA, Dv)) Pts.Add(Dv);
		Pts.Add(L + FVector(0, 0, 60.f)); Pts.Add(L + FVector(0, 0, 175.f));
		const FRotator Y(0.f, PA->GetActorRotation().Yaw, 0.f);
		Side = (FRotationMatrix(Y).GetUnitAxis(EAxis::Y) * 0.8f - FRotationMatrix(Y).GetUnitAxis(EAxis::X) * 0.6f).GetSafeNormal();   // (a MetaHuman faces its actor's +y; its right is -x)
		return true;
	}
	if (!Pax.IsValidIndex(Watched) || Pax[Watched].St == EPx::Gone || !Pax[Watched].A.IsValid() || (!Moving(Pax[Watched]) && Pax[Watched].T > 15.f))
	{
		int32 Best = INDEX_NONE;
		for (int32 i = 0; i < Pax.Num(); ++i) if (Moving(Pax[i]) && (Best == INDEX_NONE || Pax[i].Order > Pax[Best].Order)) Best = i;
		if (Best == INDEX_NONE) for (int32 i = 0; i < Pax.Num(); ++i) if (Pax[i].St != EPx::Gone && Pax[i].A.IsValid()) { Best = i; break; }
		if (Best != INDEX_NONE && Best != Watched) ADP_LOG(TEXT("PaxFlow: the camera follows %s (%s) - %s"), *Pax[Best].Id, *Pax[Best].Name, *Pax[Best].Stage);
		Me->Watched = Best;
	}
	if (!Pax.IsValidIndex(Watched)) return false;
	const FPx& P = Pax[Watched];
	const AActor* A = P.A.Get(); if (!A) return false;
	Pts.Reset();
	const FVector L = A->GetActorLocation();
	Pts.Add(L); Pts.Add(L + FVector(0, 0, 180.f));
	if (P.bHasBag && P.Holder) Pts.Add(P.Holder->GetComponentLocation() - FVector(0, 0, 40.f));
	if (P.St == EPx::Revolve) if (const ATerminalDoors* Dr = Doors()) { FVector C, Ax; float Rc; if (Dr->RevolvingFrame(P.Rev, C, Ax, Rc)) { const FVector Sd(-Ax.Y, Ax.X, 0.f); Pts.Add(C + Sd * Rc); Pts.Add(C - Sd * Rc); Pts.Add(C + FVector(0, 0, 250.f)); } }   // (the whole drum in frame)
	if (P.St == EPx::Desk || P.St == EPx::DeskQ) if (const ADepartureBelts* D = ADepartureBelts::Get(GetWorld())) Pts.Add(D->DeskSlot(P.Desk).GetLocation());
	if (P.St == EPx::ToBag || P.St == EPx::ClaimWait) if (const ABaggageBelts* B = ABaggageBelts::Get(GetWorld())) { FVector At; bool bOn = false; if (B->PaxBag(P.Id, At, bOn) && bOn) Pts.Add(At); }
	Side = FRotator(0.f, A->GetActorRotation().Yaw + 180.f, 0.f).Vector();   // (the actor's -X: the person's right - a MetaHuman faces its actor's +Y)
	return true;
}

FString APaxFlow::WatchWhat() const
{
	if (!Pax.IsValidIndex(Watched) || Pax[Watched].St == EPx::Gone) return FString();
	const FPx& P = Pax[Watched];
	return FString::Printf(TEXT("%s (%s, %s): %s"), *P.Id, P.bArr ? TEXT("arriving") : TEXT("departing"), Runs.IsValidIndex(P.Run) ? *Runs[P.Run].Label : TEXT("?"), *P.Stage);
}

#pragma endregion

#pragma region Revolving doors

// (owner 2026-11-21: the revolving doors work for passengers - nobody ever stuck, everyone out). At the door they wait
// at a short queue outside its opening until an EMPTY compartment lines up with them (ATerminalDoors::RevolvingBoard:
// one walker a compartment - PaxFlow has no parties yet; never strangers together), step in, and are then carried
// round kinematically on the compartment's arc at the door's own speed - never steered by collision, so they can't
// snag on the glass; the legs walk with it (Anim measures the real motion). Lined up with the far opening, they step
// out and the compartment is free. Missed it (the way out blocked): the door's safety stop turns them back to it.

ATerminalDoors* APaxFlow::Doors() const { return ATerminalDoors::Find(GetWorld()); }

APaxFlow::FLine& APaxFlow::RevLine(int32 Rev, int32 Side)
{
	const int32 K = FMath::Max(0, Rev) * 2 + (Side > 0 ? 1 : 0);
	if (RevLines.Num() <= K) RevLines.SetNum(K + 1);
	return RevLines[K];
}

void APaxFlow::StartRevolve(int32 i)
{
	FPx& P = Pax[i];
	const FWp W = P.Way[P.Wi];
	P.RevAfter.Reset();
	for (int32 k = P.Wi + 1; k < P.Way.Num(); ++k) P.RevAfter.Add(P.Way[k]);
	P.Rev = W.Rev; P.RevSide = W.RevSide; P.RevComp = INDEX_NONE; P.RevPhase = 0; P.RevT = 0.f;
	RevLine(P.Rev, P.RevSide).Who.AddUnique(i);
	P.St = EPx::Revolve;
	const ATerminalDoors* Dr = Doors();
	const FString Id = Dr ? Dr->RevolvingId(P.Rev) : FString(TEXT("?"));
	P.Stage = FString::Printf(TEXT("at revolving door %s, waiting for an empty compartment"), *Id);
	if (Runs.IsValidIndex(P.Run) && (Runs[P.Run].bTest || Runs[P.Run].Spawned <= 3)) ADP_LOG(TEXT("PaxFlow: %s at revolving door %s (%d waiting there)"), *P.Id, *Id, RevLine(P.Rev, P.RevSide).Who.Num());
}

void APaxFlow::EndRevolve(FPx& P, bool bThrough)
{
	const int32 i = int32(&P - Pax.GetData());
	RevLine(P.Rev, P.RevSide).Who.Remove(i);
	if (P.RevComp != INDEX_NONE) if (ATerminalDoors* Dr = Doors()) Dr->RevolvingLeave(P.Rev, P.RevComp);
	P.Way.Reset(); P.Wi = 0;
	if (!bThrough)
	{
		// (never stuck: no way in by this door - along the front to the main entrance's sliding doors instead)
		const float J = float(P.Jit.X) / 100.f;
		auto Add = [&](float X, float Z, bool bG) { FWp W; W.P = PW(X, 0, Z); W.bGround = bG; if (bG) W.P.Z = GroundZ(X, Z); P.Way.Add(W); };
		Add(PXDOORX + J * 2.f, -8.f, true); Add(PXDOORX + J, -4.f, false); Add(PXDOORX + J, 0.f, false); Add(PXDOORX + J * 3.f, 2.5f, false);
		for (int32 k = 4; k < P.RevAfter.Num(); ++k) P.Way.Add(P.RevAfter[k]);   // (past the door's own lobby points)
	}
	else P.Way = P.RevAfter;
	P.RevAfter.Reset();
	P.Rev = INDEX_NONE; P.RevComp = INDEX_NONE; P.RevPhase = 0;
	P.St = EPx::Walk;
	P.Stage = bThrough ? TEXT("through the revolving door, into the hall") : TEXT("round to the main entrance");
	if (i == Watched && RevReturn != INDEX_NONE) RevReturnIn = 4.f;
}

void APaxFlow::StepRevolve(int32 i, float Dt)
{
	FPx& P = Pax[i];
	AActor* A = P.A.Get(); if (!A) return;
	ATerminalDoors* Dr = Doors();
	FVector C, Ax; float Rc = 0.f;
	if (!Dr || !Dr->RevolvingFrame(P.Rev, C, Ax, Rc)) { EndRevolve(P, false); return; }
	const FVector In = Ax * float(P.RevSide);   // (from the middle out to their own side)
	const FVector Exit = C - In * (Rc + 70.f);
	P.RevT += Dt;
	P.bRideNow = false;   // (they walk with the wing: the legs follow the real motion)
	// on the floor: straight down from knee height, never onto a person (as Follow)
	auto Place = [&](FVector N, const FVector& Face, float Turn)
	{
		FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(PaxRevFloor), false, A);
		const float Z0 = A->GetActorLocation().Z;
		N.Z = Z0;
		if (GetWorld()->LineTraceSingleByChannel(H, FVector(N.X, N.Y, Z0 + 45.f), FVector(N.X, N.Y, Z0 - 150.f), ECC_Visibility, Q))
		{
			bool bPerson = false; if (const AActor* Hit = H.GetActor()) for (const FPx& O : Pax) if (O.A.Get() == Hit) { bPerson = true; break; }
			if (!bPerson) N.Z = H.ImpactPoint.Z;
		}
		A->SetActorLocation(N);
		if (!Face.IsNearlyZero()) A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, Face.Rotation().Yaw - 90.f, 0.f), Dt, Turn));
		Anim(P);
	};
	switch (P.RevPhase)
	{
	case 0:   // waiting outside, in the queue
	{
		const TArray<int32>& L = RevLine(P.Rev, P.RevSide).Who;
		const int32 K = FMath::Max(0, L.IndexOfByKey(i));
		FVector Spot = C + In * (Rc + 80.f + 90.f * K);
		Spot.Z = GroundZ(PlanOf(Spot).X, PlanOf(Spot).Z);
		if (FVector::Dist2D(Spot, A->GetActorLocation()) > 35.f)
		{
			P.Way.Reset(); P.Wi = 0; FWp W; W.P = Spot; W.bGround = true; W.bNav = true; P.Way.Add(W);
			Follow(P, A, Dt);
			break;
		}
		const FVector Face = -In; Idle(P, A, Dt, &Face);
		if (K != 0) break;
		const int32 Comp = Dr->RevolvingBoard(P.Rev, P.RevSide, i, P.Id);
		if (Comp != INDEX_NONE)
		{
			RevLine(P.Rev, P.RevSide).Who.Remove(i);
			P.RevComp = Comp; P.RevPhase = 1; P.RevT = 0.f; P.RevFrom = A->GetActorLocation();
			P.Stage = FString::Printf(TEXT("stepping into compartment %d of revolving door %s"), Comp + 1, *Dr->RevolvingId(P.Rev));
			if (Runs.IsValidIndex(P.Run) && Runs[P.Run].bTest) ADP_LOG(TEXT("PaxFlow: %s steps into compartment %d of revolving door %s"), *P.Id, Comp + 1, *Dr->RevolvingId(P.Rev));
		}
		else if (P.RevT > 60.f)
		{
			ADP_WARN(TEXT("PaxFlow: %s waited 60 s at revolving door %s with no empty compartment coming round - round to the main entrance"), *P.Id, *Dr->RevolvingId(P.Rev));
			EndRevolve(P, false);
		}
		break;
	}
	case 1:   // stepping in: onto the compartment's middle as it comes round
	{
		FVector Dir; float Sp = 0.f;
		const FVector Tgt = Dr->RevolvingSpot(P.Rev, P.RevComp, 0.6f, &Dir, &Sp);
		const float K = FMath::SmoothStep(0.f, 1.f, FMath::Min(P.RevT / 1.1f, 1.f));
		const FVector N = FMath::Lerp(P.RevFrom, Tgt, K);
		P.Speed = 100.f;
		Place(N, FMath::Lerp(-In, Dir, K), 4.f);
		if (P.RevT >= 1.1f)
		{
			Dr->RevolvingRiding(P.Rev, P.RevComp);
			P.RevPhase = 2; P.RevT = 0.f;
			P.Stage = FString::Printf(TEXT("riding round in compartment %d of revolving door %s"), P.RevComp + 1, *Dr->RevolvingId(P.Rev));
		}
		break;
	}
	case 2:   // carried round with it, walking along
	{
		FVector Dir; float Sp = 0.f;
		const FVector N = Dr->RevolvingSpot(P.Rev, P.RevComp, 0.6f, &Dir, &Sp);
		P.Speed = Sp;
		Place(N, Sp > 3.f ? Dir : FVector::ZeroVector, 4.f);
		bool bHeld = false;
		if (Dr->RevolvingAtExit(P.Rev, P.RevComp, &bHeld))
		{
			// (out when the way out is clear - someone standing right there: wait; the door stopped for them: out now)
			bool bClear = true;
			for (const FPx& O : Pax)
			{
				if (&O == &P || O.St == EPx::Gone) continue;
				const AActor* OA = O.A.Get(); if (!OA) continue;
				if (FMath::Abs(OA->GetActorLocation().Z - Exit.Z) < 120.f && FVector::DistSquared2D(OA->GetActorLocation(), Exit) < FMath::Square(60.f)) { bClear = false; break; }
			}
			if (bClear || bHeld) { P.RevPhase = 3; P.RevT = 0.f; P.RevFrom = A->GetActorLocation(); P.Stage = TEXT("stepping out of the revolving door"); }
		}
		break;
	}
	default:   // stepping out, square through the opening
	{
		const float K = FMath::SmoothStep(0.f, 1.f, FMath::Min(P.RevT / 1.0f, 1.f));
		P.Speed = 100.f;
		Place(FMath::Lerp(P.RevFrom, Exit, K), -In, 4.f);
		if (P.RevT >= 1.0f)
		{
			if (Runs.IsValidIndex(P.Run) && (Runs[P.Run].bTest || Runs[P.Run].Spawned <= 3)) ADP_LOG(TEXT("PaxFlow: %s out of revolving door %s (compartment %d free again)"), *P.Id, *Dr->RevolvingId(P.Rev), P.RevComp + 1);
			EndRevolve(P, true);
		}
		break;
	}
	}
}

#pragma endregion
