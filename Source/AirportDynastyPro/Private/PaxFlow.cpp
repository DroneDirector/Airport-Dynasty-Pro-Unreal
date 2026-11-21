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
#include "TerminalPlan.h"
#include "Turnaround.h"
#include "Site.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSimData.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPWorldData.h"
#include "UI/ADPHUD.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimSingleNodeInstance.h"
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
		G.Jw = FVector2f((Rm.X0 + Rm.X1) / 2, Rm.Z1);
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
	bTestMade = true;
	// a real turnaround at a jetway gate if one is going on (its jetway docked), else gate 2 for the departure, 3 for the arrival
	int32 DepGate = 2; FString DepReg;
	for (const ADPTurnaround::FTurn& T : ADPTurnaround::All()) { const int32 G = ADPGround::GateOfStand(T.Stand); const int32 Gi = GateRoom(G); if (Gi != INDEX_NONE && Gates[Gi].N == G && Gates[Gi].bF2) { DepGate = G; DepReg = T.Reg; break; } }
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
		R.Gate = DepGate == 3 ? 2 : 3; R.Pax = 14; R.Visible = 14; R.ArrStart = RealT + 40.0;
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
	// ---- departures: every aircraft standing here with its next leg (the boards' own list, so gate and flight agree) ----
	for (const FADPBoardRow& Row : ADPBoards::Rows(this, false, 40))
	{
		if (Row.Reg.IsEmpty() || Row.Gate <= 0 || Row.Status.StartsWith(TEXT("Departed"))) continue;
		const FADPAircraft* A = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Row.Reg; });
		if (!A) continue;
		const double Dep = PxSec(A->ReadyAt > Now ? A->ReadyAt : Now + FTimespan::FromMinutes(30.0));
		const FString Key = TEXT("D:") + Row.Reg + TEXT(":") + Row.Flight;
		if (FRun* Ex = Runs.FindByPredicate([&](const FRun& R) { return R.Key == Key; })) { Ex->DepT = Dep; Ex->Gate = Row.Gate; continue; }
		const double MinTo = (Dep - SimT) / 60.0;
		if (MinTo > 122.0 || MinTo < 25.0) continue;   // (too early yet, or too late to walk the whole way now)
		FRun R; R.Key = Key; R.Label = Row.Flight; R.Reg = Row.Reg; R.AirlineId = Row.AirlineId; R.Gate = Row.Gate; R.DepT = Dep;
		for (const FADPAirline& Al : Sim->World.Airlines) if (Al.Id == Row.AirlineId) R.bPlayer = Al.bPlayer;
		const FADPTypeInfo* T = ADPWorldData::Type(A->TypeId);
		R.Pax = T && T->Seats > 0 ? FMath::RoundToInt(T->Seats * 0.8f) : 120;   // (booked at the gate by the simulation; until then about 80 % full)
		if (R.Pax <= 0) continue;
		R.Visible = FMath::Min(R.Pax, int32(VisibleDep));
		int32 Missed = 0;
		for (int32 i = 0; i < R.Visible; ++i)
		{
			const double At = Dep - Rng.FRandRange(85.f, 120.f) * 60.0;
			if (At < SimT - 8.0 * 60.0) { ++Missed; continue; }   // (they would be inside already - never popped in there)
			R.SpawnAt.Add(At);
		}
		R.SpawnAt.Sort();
		R.Visible = R.SpawnAt.Num();
		ChooseDesks(R); R.Jet = JetwayFor(GateRoom(R.Gate));
		ADP_LOG(TEXT("PaxFlow: departure %s (%s) to %s, gate %d, in %.0f min - %d passengers, %d of them walk (the rest simulated)%s"), *R.Label, *R.Reg, *Row.Place, R.Gate, MinTo, R.Pax, R.Visible, Missed ? *FString::Printf(TEXT(", %d already past the car park when the game came to them"), Missed) : TEXT(""));
		Runs.Add(R);
	}
	// ---- arrivals: an aircraft's turnaround with deplaning started ----
	for (const ADPTurnaround::FTurn& T : ADPTurnaround::All())
	{
		const ADPTurnaround::FTask* Dp = T.Tasks.FindByPredicate([](const ADPTurnaround::FTask& K) { return K.Id == TEXT("deplane"); });
		if (!Dp || !Dp->bStarted) continue;
		const FString Key = TEXT("A:") + T.Reg + TEXT(":") + FString::Printf(TEXT("%lld"), T.OnBlocks.GetTicks() / ETimespan::TicksPerMinute);
		if (Runs.ContainsByPredicate([&](const FRun& R) { return R.Key == Key; })) continue;
		const FADPFlight* In = nullptr;
		for (const FADPFlight& F : Sim->World.Flights) if (F.Reg == T.Reg && (F.To == Here || F.Diverted == Here) && (!In || F.DepAt > In->DepAt)) In = &F;
		FRun R; R.Key = Key; R.bArr = true; R.Reg = T.Reg; R.Gate = ADPGround::GateOfStand(T.Stand);
		R.Pax = In ? In->Pax + In->PaxJ + In->PaxF : T.Pax;
		R.Label = T.Reg;
		for (const FADPBoardRow& Row : ADPBoards::Rows(this, true, 40)) if (Row.Reg == T.Reg) { R.Label = Row.Flight; break; }
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
	R.bCalled = true; R.CallT = Clock(R);
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
		// getting out of their cars, a few seconds apart at the most
		if (R.Spawned < R.SpawnAt.Num() && Now >= R.SpawnAt[R.Spawned] && SpawnGap <= 0.f) { SpawnDep(Ri); SpawnGap = 0.8f; }
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
			if (!R.bAllLogged && R.Spawned >= R.Visible && R.Aboard >= R.Spawned && R.Spawned > 0)
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
		if (!bDocked && !(R.bTest && R.DockWait > 30.f)) return;
		R.bDeplaning = true; R.NextArr = Now;
		// their bags: the flight's claim belt, the shown passengers' bags tagged with their ids
		TArray<FString> Ids; for (int32 k = 0; k < R.Visible; ++k) if (k % 5 != 4) Ids.Add(FString::Printf(TEXT("%s-%02d"), *R.Label, k + 1));
		if (ABaggageBelts::BeltOf(GetWorld(), R.Label) == INDEX_NONE)
		{
			const int32 Gi = GateRoom(R.Gate);
			ABaggageBelts::OnArrived(GetWorld(), Gates.IsValidIndex(Gi) ? Gates[Gi].Jw.X : 30.f, R.Label);
		}
		ABaggageBelts::TagBags(GetWorld(), R.Label, Ids);
		ADP_LOG(TEXT("PaxFlow: %s deplaning at gate %d%s - belt %d, %d bags tagged to their passengers"), *R.Label, R.Gate, bDocked ? TEXT("") : TEXT(" (test: the jetway is not docked - walking out anyway)"), ABaggageBelts::BeltOf(GetWorld(), R.Label) + 1, Ids.Num());
	}
	if (Now >= R.NextArr && SpawnGap <= 0.f) { SpawnArr(Ri); R.NextArr = Now + 3.0; SpawnGap = 0.5f; }
}

#pragma endregion

#pragma region People

int32 APaxFlow::NewPx()
{
	if (FreePx.Num()) { const int32 i = FreePx.Pop(); Pax[i] = FPx(); return i; }
	return Pax.AddDefaulted();
}

AActor* APaxFlow::Body(const FVector& At, float Yaw, FPx& P)
{
	AADPPeople* People = AADPPeople::Get(GetWorld());
	if (!People) return nullptr;
	P.Name = Names[Rng.RandRange(0, Names.Num() - 1)];
	AActor* A = People->Spawn(P.Name, At, Yaw);
	if (!A) return nullptr;
	A->SetActorLocation(At);   // (Spawn finds the floor from above; inside a jetway's cab that is its roof - our own point)
	P.A = A;
	if (AADPPeople::FWho* W = People->Find(A)) P.Body = W->Body;
	// (far away their animation updates only while seen, and less often - no change up close)
	TArray<USkeletalMeshComponent*> Sk; A->GetComponents<USkeletalMeshComponent>(Sk);
	for (USkeletalMeshComponent* S : Sk) { S->bEnableUpdateRateOptimizations = true; S->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered; }
	P.Holder = NewObject<USceneComponent>(this); P.Holder->RegisterComponent(); Holders.Add(P.Holder);
	P.Pace = Rng.FRandRange(118.f, 145.f);
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
	const bool bBag = Dep && (R.bTest || Rng.FRand() < 0.75f) && R.Desks.Num();
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
	P.Stage = TEXT("out of the car, in the car park");
	WayToHall(P, At);
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
	P.Stage = TEXT("off the aircraft, in the jetway's cab");
	auto Add = [&](const FVector& W, uint8 Mode = 0) { FWp Wp; Wp.P = W; Wp.Mode = Mode; P.Way.Add(Wp); };
	if (R.Jet != INDEX_NONE) { Add(Mid); Add(Ro); }
	Add(PW(G.Jw.X, Fy, G.Jw.Y + 1.2f)); Add(PW(G.Jw.X, Fy, G.Jw.Y)); Add(Jit(P, PW(G.Jw.X, Fy, G.Jw.Y - 1.5f)));
	P.Do = EDo::ArrIn;
	if (R.Spawned <= 3 || R.bTest) ADP_LOG(TEXT("PaxFlow: %s (%s) off %s at gate %d%s"), *P.Id, *P.Name, *R.Label, R.Gate, P.bArrBag ? TEXT(", a bag in the hold") : TEXT(", hand luggage only"));
	return i;
}

void APaxFlow::Finish(int32 i, const TCHAR* Why)
{
	FPx& P = Pax[i];
	for (FLine& L : DeskLines) L.Who.Remove(i);
	for (FLine& L : LaneLines) L.Who.Remove(i);
	if (AActor* A = P.A.Get())
	{
		LeanSpots.RemoveAll([&](const FVector& L) { return FVector::Dist2D(L, A->GetActorLocation()) < 60.f; });
		ASeats::Release(A);
		if (P.bHasBag && P.bArr) if (ABaggageBelts* B = ABaggageBelts::Get(GetWorld())) B->DropPaxBag(P.Id);
		if (P.bHasBag && !P.bArr && P.Bag != INDEX_NONE) if (ADepartureBelts* D = ADepartureBelts::Get(GetWorld())) D->RemoveBag(P.Bag);
		A->Destroy();
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
			// (AADPPeople::WalkTo's own mapping onto the idle-walk-run blend space)
			One->SetBlendSpacePosition(FVector(0.f, FMath::Min(P.Speed * 1.5f, 300.f), 0.f));
			One->SetPlayRate(P.Speed > 20.f ? FMath::Clamp(P.Speed / FMath::Max(P.Speed * 1.5f, 100.f), 0.5f, 1.4f) : 1.f);
		}
}

void APaxFlow::Idle(FPx& P, AActor* A, float Dt, const FVector* Face)
{
	P.Speed = FMath::FInterpTo(P.Speed, 0.f, Dt, 5.f);
	if (Face && !Face->IsNearlyZero()) A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, Face->Rotation().Yaw - 90.f, 0.f), Dt, 5.f));
	Anim(P);
}

void APaxFlow::CarryBag(FPx& P, AActor* A)
{
	if (!P.bHasBag || !P.Holder || (!P.bArr && P.bBagDown)) return;
	USkeletalMeshComponent* B = P.Body.Get();
	const float Fy = A->GetActorRotation().Yaw + 90.f;
	const FVector Right = FRotator(0.f, Fy + 90.f, 0.f).Vector();
	const FVector Hand = B ? B->GetBoneLocation(TEXT("hand_r")) : A->GetActorLocation() + Right * 25.f + FVector(0, 0, 80.f);
	// (the handle in the palm, the bag hanging beside the leg, upright: a suitcase's end handle on top)
	const FQuat Q = FRotator(0.f, Fy, 0.f).Quaternion() * (P.Handle.Y < -10.f ? PXUPRIGHT : FQuat::Identity);
	P.Holder->SetWorldLocationAndRotation(Hand + Right * 9.f - FVector(0, 0, 6.f), Q);
}

bool APaxFlow::Follow(FPx& P, AActor* A, float Dt)
{
	if (P.PauseT > 0.f)
	{
		const FVector Face = P.Way.IsValidIndex(P.Wi) ? P.Way[P.Wi].Face : FVector::ZeroVector;
		P.PauseT -= Dt; Idle(P, A, Dt, &Face);
		if (P.PauseT <= 0.f) ++P.Wi;
		return false;
	}
	if (P.Wi >= P.Way.Num()) { Idle(P, A, Dt); return true; }
	const FWp& W = P.Way[P.Wi];
	const FVector Loc = A->GetActorLocation();
	FVector D = W.P - Loc; D.Z = 0.f;
	const float Dist = D.Size();
	const bool bLast = P.Wi == P.Way.Num() - 1 || W.Pause > 0.f;
	const bool bRide = W.Mode == 2;
	float Want = W.Mode == 1 ? 55.f : P.Pace;
	if (bLast && !bRide) Want *= FMath::Clamp(Dist / 90.f, 0.3f, 1.f);
	if (bRide) P.Speed = FMath::FInterpTo(P.Speed, 0.f, Dt, 6.f); else P.Speed = FMath::FInterpTo(P.Speed, Want, Dt, 4.f);
	if (Dist < (bRide ? 12.f : 30.f))
	{
		if (W.Pause > 0.f) P.PauseT = W.Pause; else ++P.Wi;
		Anim(P);
		return P.Wi >= P.Way.Num() && P.PauseT <= 0.f;
	}
	const float Mv = FMath::Min(Dist, (bRide ? 50.f : FMath::Max(P.Speed, 25.f)) * Dt);
	FVector N = Loc + D / Dist * Mv;
	// on the floor: straight down from knee height (stairs, escalator steps, the jetway's slope), never onto another person
	FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(PaxFloor), false, A);
	bool bFloor = false;
	if (GetWorld()->LineTraceSingleByChannel(H, FVector(N.X, N.Y, Loc.Z + 45.f), FVector(N.X, N.Y, Loc.Z - 150.f), ECC_Visibility, Q))
	{
		const AActor* Hit = H.GetActor();
		bool bPerson = false; if (Hit) for (const FPx& O : Pax) if (O.A.Get() == Hit) { bPerson = true; break; }
		if (!bPerson) { N.Z = H.ImpactPoint.Z; bFloor = true; }
	}
	if (!bFloor) N.Z = W.bGround ? FMath::FInterpTo(Loc.Z, W.P.Z, Dt, 4.f) : FMath::FInterpTo(Loc.Z, W.P.Z, Dt, 1.f);
	A->SetActorLocation(N);
	A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, D.Rotation().Yaw - 90.f, 0.f), Dt, bRide ? 3.f : 6.f));
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

void APaxFlow::WayToHall(FPx& P, const FVector& From)
{
	// through the car park to the pavement, over the forecourt at the crossing nearest, the pavement to the main entrance,
	// its two sliding doors, into the Grand Hall
	(void)From;
	P.Way.Reset(); P.Wi = 0;
	Crossing(P, true, PXDOORX + float(P.Jit.X) * 0.6f);
	auto Add = [&](float X, float Z, bool bG) { FWp W; W.P = PW(X, 0, Z); W.bGround = bG; if (bG) W.P.Z = GroundZ(X, Z); P.Way.Add(W); };
	const float J = float(P.Jit.X) / 100.f;
	Add(PXDOORX + J * 2.f, -8.f, true); Add(PXDOORX + J, -4.f, false); Add(PXDOORX + J, -2.f, false); Add(PXDOORX + J, 0.f, false); Add(PXDOORX + J * 3.f, 2.5f, false);
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
	Add(Wt + J * 1.2f, 9.8f); Add(Wt + J * 1.2f, 21.8f); Add(DoorX + J * 2.f, 28.8f); Add(DoorX + J * 2.f, 30.9f);
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
	// up to Floor 2: the stairs from the airside lobby (round their west end), or the new airside corridor west to the
	// escalator room and up the escalators (standing still while they carry you)
	P.Way.Reset(); P.Wi = 0;
	auto Add = [&](float X, float H, float Z, uint8 Mode = 0) { FWp W; W.P = PW(X, H, Z); W.Mode = Mode; P.Way.Add(W); };
	const float J = float(P.Jit.X) / 100.f;
	const bool bTestRun = Runs.IsValidIndex(P.Run) && Runs[P.Run].bTest;
	P.bEsc = Rng.FRand() < (bTestRun ? 0.7f : 0.5f);
	if (!P.bEsc)
	{
		Add(67.5f, 0, 51.f); Add(57.f, 0, 50.8f); Add(57.f, 0, 47.f + J); Add(66.6f, F2Y, 47.f + J); Add(68.5f, F2Y, 47.f + J);
		P.Stage = TEXT("up the stairs from the airside lobby");
		return;
	}
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
	P.Way.Reset(); P.Wi = 0;
	AddPlanWay(P, INDEX_NONE, FVector(Here.X, Fy, Here.Z), G.Place, FVector(G.Jw.X, Fy, G.Jw.Y - 1.2f), true);
	auto Add = [&](const FVector& W) { FWp Wp; Wp.P = W; P.Way.Add(Wp); };
	Add(PW(G.Jw.X, Fy, G.Jw.Y)); Add(PW(G.Jw.X, Fy, G.Jw.Y + 1.2f));
	AJetwayRigs* J = AJetwayRigs::Get(GetWorld());
	FVector Ro, Dr, Mid, Front;
	if (J && R.Jet != INDEX_NONE && J->Frame(R.Jet, Ro, Dr) && J->CabView(R.Jet, Mid, Front))
	{
		Ro.Z = F2Z; Add(Ro);
		Add(Mid); Add(Mid + Front.GetSafeNormal2D() * 70.f);
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
		WayUpstairs(P); P.Do = EDo::Upstairs; P.St = EPx::Walk;
		break;
	case EDo::Upstairs:
		if (!WayToWait(P)) { P.St = EPx::Wait; break; }
		P.St = EPx::Walk;
		break;
	case EDo::AtSeat:
	{
		AADPPeople* People = AADPPeople::Get(GetWorld());
		const int32 Gi = GateRoom(R.Gate);
		const FVector L = A->GetActorLocation();
		TArray<FBox2D> Area; Area.Add(FBox2D(FVector2D(L.X - 400.f, L.Y - 400.f), FVector2D(L.X + 400.f, L.Y + 400.f))); if (Gates.IsValidIndex(Gi)) Area.Add(Gates[Gi].Box);
		if (People && P.Seat != INDEX_NONE && People->SitOn(A, P.Seat, Area, F2Z)) { P.St = EPx::Seated; P.Stage = TEXT("sitting, waiting for the boarding call"); P.Speed = 0.f; }
		else { ASeats::Release(A); P.Seat = INDEX_NONE; P.St = EPx::Wait; P.Stage = TEXT("standing, waiting for the boarding call"); }
		break;
	}
	case EDo::AtWall: P.St = EPx::Lean; P.T = 0.f; P.Fidget = Rng.FRandRange(6.f, 14.f); P.Stage = TEXT("leaning on a wall, waiting for the boarding call"); break;
	case EDo::AtGateSpot: P.St = EPx::Wait; P.Stage = R.bCalled ? TEXT("at the gate, waiting their turn to board") : TEXT("standing at the gate"); break;
	case EDo::Aboard:
		++R.Aboard; ++TotalAboard;
		ADP_LOG(TEXT("PaxFlow: %s aboard %s (%d of %d walkers; about %d of %d passengers aboard)"), *P.Id, *R.Label, R.Aboard, R.Visible, FMath::Min(R.Pax, FMath::RoundToInt(float(R.Aboard) / FMath::Max(1, R.Visible) * R.Pax)), R.Pax);
		Finish(i, TEXT("is aboard (gone in at the cab's end)"));
		break;
	case EDo::ArrIn: WayArrivals(P); P.St = EPx::Walk; break;
	case EDo::AtClaim:
		P.St = EPx::ClaimWait; P.T = 0.f; P.Stage = TEXT("waiting at the claim belt for their bag");
		if (!P.bArrBag) { P.Way.Reset(); P.Wi = 0; WayFromHall(P, A->GetActorLocation()); P.Do = EDo::OutCarPark; P.St = EPx::Walk; P.Stage = TEXT("hand luggage only - out to the car park"); }
		break;
	case EDo::OutCarPark: ++R.Out; ++TotalOut; Finish(i, TEXT("picked up in the car park")); break;
	default: P.St = EPx::Wait; break;
	}
}

void APaxFlow::StepPx(int32 i, float Dt)
{
	FPx& P = Pax[i];
	AActor* A = P.A.Get();
	if (!A) { if (P.St != EPx::Gone) { P.St = EPx::Gone; FreePx.Add(i); } return; }
	FRun& R = Runs[P.Run];
	AADPPeople* People = AADPPeople::Get(GetWorld());
	P.T += Dt;
	// the boarding call heard: their turn comes (a free walker gets up and goes when it is due)
	if (!P.bArr && R.bCalled && P.BoardAt < 0.0) P.BoardAt = Clock(R) + 20.0;   // (came after the call)
	const bool bDue = !P.bArr && R.bCalled && Clock(R) >= P.BoardAt;
	switch (P.St)
	{
	case EPx::Walk:
		CarryBag(P, A);
		if (Follow(P, A, Dt)) Arrive(i);
		break;
	case EPx::DeskQ:
	{
		const TArray<int32>& L = DeskLines[P.Desk - 1].Who;
		const int32 K = L.IndexOfByKey(i);
		const FVector To = DeskSpot(P.Desk, FMath::Max(0, K));
		CarryBag(P, A);
		if (FVector::Dist2D(To, A->GetActorLocation()) > 25.f) { P.Way.Reset(); P.Wi = 0; FWp W; W.P = To; P.Way.Add(W); Follow(P, A, Dt); }
		else
		{
			const FVector North(0, -1, 0); Idle(P, A, Dt, &North);
			if (K == 0) { P.St = EPx::Desk; P.T = 0.f; P.Stage = FString::Printf(TEXT("at desk %d - the bag on the weigh belt"), P.Desk); }
		}
		break;
	}
	case EPx::Desk:
	{
		const FVector North(0, -1, 0); Idle(P, A, Dt, &North);
		CarryBag(P, A);
		ADepartureBelts* D = ADepartureBelts::Get(GetWorld());
		if (!P.bBagDown && P.T > 1.2f && D && P.Bag != INDEX_NONE) { D->SetDownOnDesk(P.Bag, P.Desk); P.bBagDown = true; P.bHasBag = false; if (R.bTest || R.Spawned <= 3) ADP_LOG(TEXT("PaxFlow: %s sets bag #%d on desk %d's weigh belt"), *P.Id, P.Bag, P.Desk); }
		if (P.T > (R.bTest ? 12.f : 30.f) + float(P.Jit.Y) / 10.f)
		{
			DeskLines[P.Desk - 1].Who.Remove(i);
			WayToSecurity(P); P.Do = EDo::JoinLane; P.St = EPx::Walk; P.Stage = TEXT("checked in - to security through the walk-through");
		}
		break;
	}
	case EPx::LaneQ:
	{
		const int32 K = LaneLines[P.Lane].Who.IndexOfByKey(i);
		const FVector To = LaneSpot(P.Lane, FMath::Max(0, K));
		if (FVector::Dist2D(To, A->GetActorLocation()) > 25.f) { P.Way.Reset(); P.Wi = 0; FWp W; W.P = To; P.Way.Add(W); Follow(P, A, Dt); }
		else
		{
			const FVector North(0, -1, 0); Idle(P, A, Dt, &North);
			// (one at a time through the lane: the next steps up when the one ahead is through the arch)
			bool bClear = true; for (const FPx& O : Pax) if (&O != &P && O.St == EPx::Walk && O.Do == EDo::Screened && O.Lane == P.Lane && O.Wi < 3) { bClear = false; break; }
			if (K == 0 && bClear) { LaneLines[P.Lane].Who.Remove(i); WayScreening(P); P.Do = EDo::Screened; P.St = EPx::Walk; P.Stage = FString::Printf(TEXT("security lane %d - X-ray belt, the arch"), P.Lane + 1); }
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
		if (!bDocked) R.DockWait += Dt / FMath::Max(1, R.Spawned);
		if (!bDocked && !(R.bTest && R.DockWait > 45.f)) { Idle(P, A, Dt); break; }
		P.Seat = INDEX_NONE;
		WayToJetway(P); P.St = EPx::Jetway;
		break;
	}
	case EPx::Jetway:
		if (Follow(P, A, Dt)) Arrive(i);
		break;
	case EPx::ClaimWait:
	{
		ABaggageBelts* B = ABaggageBelts::Get(GetWorld());
		FVector At; bool bOn = false;
		const bool bKnown = B && B->PaxBag(P.Id, At, bOn);
		const FVector Toward = bOn ? (At - A->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
		Idle(P, A, Dt, bOn ? &Toward : nullptr);
		if (bOn && FVector::Dist2D(At, A->GetActorLocation()) < 900.f) { P.St = EPx::ToBag; P.Stage = TEXT("their bag is coming - to take it off"); break; }
		if (!bKnown || P.T > (R.bTest ? 300.f : 900.f))
		{
			ADP_WARN(TEXT("PaxFlow: %s - %s, going on without it"), *P.Id, bKnown ? TEXT("no bag on the belt after the wait") : TEXT("their bag isn't in the claim system"));
			P.Way.Reset(); P.Wi = 0; WayFromHall(P, A->GetActorLocation()); P.Do = EDo::OutCarPark; P.St = EPx::Walk; P.Stage = TEXT("out to the car park (no bag)");
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
				P.bHasBag = true; CarryBag(P, A);
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
	if (!bReady)
	{
		if ((ReadyIn -= Dt) > 0.f) return;
		ReadyIn = 2.f;
		bReady = Prepare();
		if (!bReady) return;
	}
	ADP_PERF_SCOPE("pax flow: the passengers' trips (each frame)", 2.f);
	++Frame;
	SpawnGap -= Dt;
	if (bTest && !bTestMade) MakeTestRuns();
	if ((PollIn -= Dt) <= 0.f) { PollIn = 5.f; Poll(); }
	for (int32 r = 0; r < Runs.Num(); ++r) StepRun(r, Dt);
	// the walkers: close ones every frame, far ones every fourth (they keep going at the same speed)
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector Eye = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	ATerminalDoors::Openers.Reset();
	for (int32 i = 0; i < Pax.Num(); ++i)
	{
		FPx& P = Pax[i];
		if (P.St == EPx::Gone) continue;
		P.Acc += Dt;
		const AActor* A = P.A.Get();
		const bool bNear = A && FVector::DistSquared(A->GetActorLocation(), Eye) < FMath::Square(6000.f);
		if (!bNear && (Frame + i) % 4 != 0 && i != Watched) continue;
		const float Step = FMath::Min(P.Acc, 0.3f); P.Acc = 0.f;
		StepPx(i, Step);
		if (Pax.IsValidIndex(i)) if (const AActor* A2 = Pax[i].A.Get()) if (Pax[i].St == EPx::Walk || Pax[i].St == EPx::Jetway) ATerminalDoors::Openers.Add(A2->GetActorLocation());
	}
	// now and then: how many are where
	if ((SayIn -= Dt) <= 0.f)
	{
		SayIn = 30.f;
		int32 N[12] = {}; for (const FPx& P : Pax) ++N[(int32)P.St];
		const int32 Live = Walking();
		if (Live || Runs.Num()) ADP_LOG(TEXT("PaxFlow: %d about - %d walking, %d in desk queues, %d at desks, %d at security, %d waiting standing, %d sitting, %d leaning, %d boarding, %d at the claim; %d aboard, %d out so far; %d flights"),
			Live, N[(int32)EPx::Walk], N[(int32)EPx::DeskQ], N[(int32)EPx::Desk], N[(int32)EPx::LaneQ], N[(int32)EPx::Wait], N[(int32)EPx::Seated], N[(int32)EPx::Lean], N[(int32)EPx::Call] + N[(int32)EPx::Jetway], N[(int32)EPx::ClaimWait] + N[(int32)EPx::ToBag], TotalAboard, TotalOut, Runs.Num());
	}
	// (the runs stay listed - indices are held by the walkers; a finished one costs nothing each frame but its check)
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
