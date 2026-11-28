// The simulation's flights at our airport in 3D - see SimTraffic.h.
#include "SimTraffic.h"
#include "PaxFlow.h"
#include "Tugs.h"
#include "HangarTow.h"
#include "Turnaround.h"
#include "A320Parts.h"
#include "A320Sound.h"
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
#include "Puddles.h"
#include "Sim/ADPWeather.h"
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
#include "Components/MaterialBillboardComponent.h"
#include "Effects.h"
#include "ADPKeep.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	constexpr float KtMs = 0.5144f;
	constexpr float Wheelbase = 12.64f;        // A320: nose gear to main gear
	// THE PERFORMANCE TABLE, one row per aircraft type (owner 11-28: real speeds, real braking, the wind counted; every type
	// its own real figures) - only speeds, decelerations, timings and the turning radius; the movement itself is the one
	// below. A type without a row flies on the A320 row, logged once. Speeds are IAS at the reference mass (they go with
	// the square root of the mass); the groundspeed is the IAS less the headwind.
	// A320-200 sources: Airbus A320 FCOM DSC-32-30 (autobrake LO 1.7 m/s2, MED 3.0 m/s2; reverse idle by 70 kt), FCOM
	// PRO-NOR-SOP (taxi about 20 kt straight, about 10 kt in turns; VAPP = VLS + 1/3 of the headwind, at least 5 and at most
	// 15 kt; idle at about 20 ft, a few kt lost in the flare); Airbus "Getting to Grips with Aircraft Performance" (2002:
	// VR about 140-150 kt and V2 about VR + 4 kt at mid weight, the take-off distance about with the mass squared; climb at
	// V2+10, accelerating from 1,500 ft as in the ICAO NADP); ICAO Annex 11 / SERA: 250 kt below FL100; FCOM limitations:
	// VMO 350 kt, MMO 0.82, cruise about M0.78 (about 450 kt TAS); Airbus Flight Operations Briefing Note "Landing on
	// Contaminated Runways" and EASA AMC 25.1591: stopping distance about x1.3-1.5 wet, about x2 on snow / compact snow;
	// Airbus "A320 Aircraft Characteristics - Airport and Maintenance Planning" 4-3: about 22 m turning radius at the wing
	// tip at full nose-wheel steering (75 deg), a 180 deg turn on about 23 m of pavement.
	// Mass: typical mid weight 64 t (MTOW 78 t, MLW 66 t, empty about 42.6 t).
	struct FAcPerf
	{
		const TCHAR* Type;
		float MaxTaxiKt, TurnTaxiKt, StandTaxiKt, PushKt, MinTurnRadiusM;
		float RefMassT, EmptyT, PaxT, DepFuelT, ArrFuelT, MinMassT, MaxMassT;
		float VrKt, V2Kt, InitClimbAddKt, AccelHeightM, ClimbKt, CruiseKt, TopKt;   // (V2 + InitClimbAdd to the acceleration height, then ClimbKt below FL100; CruiseKt TAS, TopKt VMO)
		float VlsKt, VappMinAddKt, VappMaxAddKt, TouchdownKt;   // (VAPP = VLS + the wind addition; TouchdownKt in calm air)
		float RollAcc, ClimbAcc, FlareDec;   // m/s2: the take-off roll at the reference mass, V2+10 to the climb speed, the flare at idle
		float SpoilerDelayS, FreeRollDec;    // ground spoilers + autobrake about 2 s after the mains touch; the free roll till then
		float BrakeLowDry, BrakeMedDry, BrakeWetFactor, BrakeSnowFactor;   // autobrake LOW / MED on dry (m/s2); the stop's length wet, on snow
		float VacateKt;                      // the autobrake off and the exit reached at taxi speed
	};
	const FAcPerf AcPerfTab[] =
	{
		//  type        taxi  turn  stand push turnR  ref   empty  pax   fuelD fuelA min   max    Vr     V2     +10   accel  climb  cruise top    VLS    +min +max  TD     roll   climb flare spoil free  LOW   MED  wet   snow vacate
		{ TEXT("A320"), 20.f, 10.f, 5.f, 3.f, 22.f,   64.f, 42.6f, 0.1f, 8.f, 3.5f, 50.f, 78.f,  143.f, 147.f, 10.f, 457.f, 250.f, 450.f, 350.f, 134.f, 5.f, 15.f, 132.f, 1.75f, 0.5f, 1.4f, 2.f, 0.4f, 1.7f, 3.f, 1.4f, 2.f, 20.f },
	};
	const FAcPerf& PerfFor(const FString& Type)
	{
		for (const FAcPerf& R : AcPerfTab) if (Type.Equals(R.Type, ESearchCase::IgnoreCase) || Type.StartsWith(FString(R.Type) + TEXT("-"), ESearchCase::IgnoreCase)) return R;
		static TSet<FString> Told;
		if (!Told.Contains(Type)) { Told.Add(Type); ADP_WARN(TEXT("Traffic: no performance row for aircraft type '%s' - it uses the A320 figures"), Type.IsEmpty() ? TEXT("(none)") : *Type); }
		return AcPerfTab[0];
	}
	constexpr float Glide = 3.f;
	// the mass (t) by its load: the empty mass, the passengers with their bags, the fuel on board (more leaving than arriving)
	float MassFor(const FAcPerf& Pf, int32 Pax, bool bDep) { return FMath::Clamp(Pf.EmptyT + Pax * Pf.PaxT + (bDep ? Pf.DepFuelT : Pf.ArrFuelT), Pf.MinMassT, Pf.MaxMassT); }
	float MassK(const FAcPerf& Pf, float MassT) { return FMath::Sqrt(FMath::Clamp(MassT, Pf.MinMassT, Pf.MaxMassT) / Pf.RefMassT); }
	float VrMs(const FAcPerf& Pf, float MassT) { return Pf.VrKt * MassK(Pf, MassT) * KtMs; }
	float V2Ms(const FAcPerf& Pf, float MassT) { return Pf.V2Kt * MassK(Pf, MassT) * KtMs; }
	// the headwind along the runway in use (m/s, - a tailwind): groundspeed = IAS - headwind. The surface wind is used aloft too.
	float HeadMs(bool bToX1)
	{
		double Hdg = 336.0; if (TSharedPtr<FJsonObject> A = ADPSite::AirportBlock()) A->TryGetNumberField(TEXT("hdg"), Hdg);
		if (!bToX1) Hdg += 180.0;
		return AWindSocks::WindKt * FMath::Cos(FMath::DegreesToRadians(AWindSocks::WindFrom - float(Hdg))) * KtMs;
	}
	float VappMs(const FAcPerf& Pf, float MassT, float Head) { return (Pf.VlsKt * MassK(Pf, MassT) + FMath::Clamp(Head / KtMs / 3.f, Pf.VappMinAddKt, Pf.VappMaxAddKt)) * KtMs; }
	float TouchdownLossMs(const FAcPerf& Pf) { return FMath::Max(0.f, Pf.VlsKt + Pf.VappMinAddKt - Pf.TouchdownKt) * KtMs; }   // (about 7 kt lost in the flare)
	// the runway's surface: the site's weather (live report, else the built-in climate) and the ground's own wetness; dry by default
	enum class ERwyCond : uint8 { Dry, Wet, Snow };
	ERwyCond RunwayCond()
	{
		static ERwyCond Cond = ERwyCond::Dry; static double Next = 0.0;
		const double Now = FPlatformTime::Seconds();
		if (Now < Next) return Cond;
		Next = Now + 30.0;
		ERwyCond C = ERwyCond::Dry;
		if (const TSharedPtr<FJsonObject> Ap = ADPSite::AirportBlock())
		{
			double La = 0, Lo = 0; FString Icao; Ap->TryGetNumberField(TEXT("lat"), La); Ap->TryGetNumberField(TEXT("lon"), Lo); Ap->TryGetStringField(TEXT("icao"), Icao);
			const FADPWx X = ADPWeather::At(Icao, La, Lo, FDateTime::UtcNow());
			C = X.bSnow || X.bFreezing ? ERwyCond::Snow : X.bRain ? ERwyCond::Wet : ERwyCond::Dry;
		}
		if (C == ERwyCond::Dry) if (const APuddles* P = APuddles::Get()) if (P->GetWetness() > 0.3f) C = ERwyCond::Wet;   // (still wet after the rain)
		if (C != Cond) ADP_LOG(TEXT("Traffic: the runway is %s now"), C == ERwyCond::Snow ? TEXT("snow-covered") : C == ERwyCond::Wet ? TEXT("wet") : TEXT("dry"));
		return Cond = C;
	}
	float CondFactor(const FAcPerf& Pf, ERwyCond C) { return C == ERwyCond::Snow ? Pf.BrakeSnowFactor : C == ERwyCond::Wet ? Pf.BrakeWetFactor : 1.f; }
	float Heading(const FVector2f& From, const FVector2f& To) { return FMath::Atan2(To.Y - From.Y, To.X - From.X); }
	constexpr float TenK = 3048.f, FlareM = 9.f;   // 10,000 ft; the flare from about 30 ft (metres over the runway)
	// test only (-AirplaneTest, never in normal play): a departure 20 s after lift-off is put at 10,000 ft on its climb track
	// and flies on there, a gentle standard-rate turn included, for the camera
	bool AirplaneTest() { static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("AirplaneTest")); return bOn; }
	// the pitch towards Want at up to MaxRate deg/s, its rate eased in and out (3 deg/s2, then 1.5 x the error, at least 0.3
	// deg/s so it arrives): neither the attitude nor its rate ever jumps
	void EasePitchTo(float& Pitch, float& V, float Want, float MaxRate, float Dt)
	{
		const float Err = Want - Pitch;
		if (FMath::Abs(Err) < 1e-4f) { Pitch = Want; V = 0.f; return; }
		const float RateWant = FMath::Sign(Err) * FMath::Min(MaxRate, FMath::Max(0.3f, 1.5f * FMath::Abs(Err)));
		V = FMath::FInterpConstantTo(V, RateWant, Dt, 3.f);
		const float Step = V * Dt;
		if ((Err > 0.f && Step >= Err) || (Err < 0.f && Step <= Err)) { Pitch = Want; V = 0.f; } else Pitch += Step;
	}
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
// (11-28 owner: take off from the end of the runway; the whole aircraft behind the hold-short marking)
// the nose wheel stops 7.5 m short of the holding position's middle: the nose (5.1 m ahead of it) 1.6 m clear of the
// solid lines. Lined up: a curved turn (20 m, the nose wheel's steering) off the end taxiway onto the centre line, the
// nose wheel ending 20 m further on - the tail about 12 m inside the runway's end, the full length ahead
constexpr float HoldStop = 107.5f + 7.5f, LineUpR = 20.f;
static TArray<FVector2f> LineUpPath(float LineUp, float RY, float Dir, float Side)
{
	TArray<FVector2f> P;
	const FVector2f C(LineUp + Dir * LineUpR, RY + Side * LineUpR);
	for (int32 k = 0; k <= 9; ++k)
	{
		const float Th = HALF_PI * k / 9.f;
		P.Add(C + FVector2f(-Dir * LineUpR * FMath::Cos(Th), -Side * LineUpR * FMath::Sin(Th)));
	}
	P.Add(FVector2f(LineUp + Dir * 3000.f, RY));   // (room for the real roll: up to about 2,500 m to lift-off at 78 t with a tailwind)
	return P;
}

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
			FMove M; M.FlightId = Fl.Id; M.Reg = Fl.Reg; M.bDeparture = true; M.Stand = St->Id; M.Phase = EPhase::Push; M.TypeId = A ? A->TypeId : FString(); M.MassT = MassFor(PerfFor(M.TypeId), Fl.Pax + Fl.PaxJ + Fl.PaxF, true);
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
			FMove M; M.FlightId = Fl.Id; M.Reg = Fl.Reg; M.bDeparture = false; M.Phase = EPhase::Approach; M.TypeId = A ? A->TypeId : FString(); M.MassT = MassFor(PerfFor(M.TypeId), Fl.Pax + Fl.PaxJ + Fl.PaxF, false);
			M.Stand = A ? A->Stand : FString();
			// a straight-in 3 degree approach to the touchdown zone (500 m past the threshold), 12 km out (630 m: the gear comes down at 600 m)
			const float Dir = bToX1 ? 1.f : -1.f, Thr = bToX1 ? X0 : X1, TD = Thr + Dir * 500.f, From = 12000.f;
			M.Nose = FVector2f(TD - Dir * From, RY); M.Main = M.Nose - FVector2f(Dir * Wheelbase, 0.f);
			M.Height = From * FMath::Tan(FMath::DegreesToRadians(Glide)); { const float Hd = HeadMs(bToX1); M.Speed = VappMs(PerfFor(M.TypeId), M.MassT, Hd) - Hd; }   // (groundspeed)
			M.Path = {FVector2f(TD, RY)};
			Moves.Add(M); ADPParked::Coming.Add(Fl.Reg);
		}
	}
	// models for new movements
	for (FMove& M : Moves) if (!M.Model.IsValid()) ModelFor(M);
}

void ASimTraffic::ModelFor(FMove& M)
{
	ADP_PERF_SCOPE("traffic: an aircraft model made", 5.f);
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABabylonModel* B = GetWorld()->SpawnActor<ABabylonModel>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (B && B->Place(TEXT("A320"))) { B->Tags.Add(FName(*(TEXT("MOVE/") + M.Reg))); M.Model = B; AddLights(M); Pose(M); ADP_LOG(TEXT("Traffic: %s %s in 3D (%s)"), *M.Reg, M.bDeparture ? TEXT("departing") : TEXT("arriving"), *M.Stand); }
	else { ADP_WARN(TEXT("Traffic: %s - no A320 model could be placed"), *M.Reg); if (B) B->Destroy(); }
}

FString ASimTraffic::FreeStand() const
{
	// (once, when a test asks: the parked aircraft's noses - a walk over the models, never in a frame's work)
	TArray<FVector2f> Noses;
	for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
	{
		if (It->IsHidden()) continue;
		const FVector L = It->GetActorLocation(); Noses.Add(FVector2f(L.X / 100.f, -L.Y / 100.f));   // (world -> plan)
	}
	const FAirfieldData::FStand* Best = nullptr;
	for (const FAirfieldData::FStand& S : F.Stands)
	{
		if (S.bCargo || S.bRemote || Moves.ContainsByPredicate([&](const FMove& O) { return O.Stand == S.Id; })) continue;
		const FVector2f At = S.Frame().P(0.f, 0.f);
		if (Noses.ContainsByPredicate([&](const FVector2f& N) { return FVector2f::Distance(N, At) < 30.f; })) continue;
		// (the one nearest the taxi-out's connector, 60 m or more from it: a straight run on the taxilane first)
		const float D = FMath::Abs(FMath::Abs(S.X - TaxiOutX(RunwayToX1())) - 120.f);
		if (!Best || D < FMath::Abs(FMath::Abs(Best->X - TaxiOutX(RunwayToX1())) - 120.f)) Best = &S;
	}
	return Best ? Best->Id : FString();
}

void ASimTraffic::Pose(FMove& M)
{
	ABabylonModel* B = M.Model.Get();
	if (!B) return;
	// the actor sits on the nose tyre (as on the stands: ParkedAircraft.cpp), the nose pointing from the main gear to it
	const FVector2f D = (M.Nose - M.Main).GetSafeNormal();
	// (11-28 owner: the nose wheel floated while taxiing - the aircraft was held level on the ground under its nose; now it lies
	//  on the ground under both gears: tilted by the slope from the main gear to the nose, the nose tyre placed from the main gear)
	// (owner 11-28: flush on every runway, taxiway and apron - the surface drawn under each gear, not the grade; where one
	//  surface steps onto another the gear follows within about half a second, never a jump)
	const float WantN = AAirfield::SurfaceAt(GetWorld(), M.Nose.X, M.Nose.Y, B) - AAirfield::GroundAt(M.Nose.X, M.Nose.Y);
	const float WantM = AAirfield::SurfaceAt(GetWorld(), M.Main.X, M.Main.Y, B) - AAirfield::GroundAt(M.Main.X, M.Main.Y);
	const float Step = 0.6f * FMath::Max(GetWorld()->GetDeltaSeconds(), 0.f);
	if (!M.bSurfSet) { M.SurfN = WantN; M.SurfM = WantM; M.bSurfSet = true; }
	M.SurfN += FMath::Clamp(WantN - M.SurfN, -Step, Step); M.SurfM += FMath::Clamp(WantM - M.SurfM, -Step, Step);
	const float GNose = AAirfield::GroundAt(M.Nose.X, M.Nose.Y) + M.SurfN, GMain = AAirfield::GroundAt(M.Main.X, M.Main.Y) + M.SurfM;
	const float Slope = FMath::RadiansToDegrees(FMath::Atan2(GNose - GMain, Wheelbase));
	// (the body on its struts: lifted by the nose strut's bounce, pitched by the difference to the main's - Gear())
	const float Up = M.Pitch + Slope + FMath::RadiansToDegrees(FMath::Atan2(M.BncN - M.BncM, Wheelbase));   // (the rotation / flare on top of the ground's own tilt)
	const FVector World = FTerminalPlan::ToWorld(M.Nose.X, GMain + M.Height + M.BncM + Wheelbase * FMath::Sin(FMath::DegreesToRadians(Up)), M.Nose.Y);
	const FVector Dir(D.X, -D.Y, 0.f);   // (plan (x, y) -> world (x, -y))
	// (2026-11-21: the moving aircraft drew nose-backwards - its nose away from the gate and the tug inside its body; the model is
	//  created unturned here and turned after Place, unlike ParkedAircraft's: the half turn the other way)
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.X, -Dir.Y));
	// the attitude: nose up about 8 degrees in the climb, about 3 on the approach (the model's nose is the actor's -Y, so
	// nose-up is a roll about the actor's X axis); level on the ground
	// (11-28 owner: it rotates gently as in real life, about 3 degrees a second, and lifts off about 4 s later; the nose comes up
	//  round the main gear, so the nose tyre - where the actor sits - rises by the wheelbase times the sine of the pitch)
	//  - the main gear stays on its ground while the nose comes up: the nose tyre's height above it is the wheelbase times the sine)
	B->SetActorLocationAndRotation(World, FRotator(-M.Bank, Yaw, Up));   // (the bank: a roll about the fuselage, + right wing down - Surfaces())
}

bool ASimTraffic::TestMove(UWorld* W, bool bTakeoff, float SecOut, FString& Reg)
{
	ASimTraffic* T = nullptr;
	if (W) for (TActorIterator<ASimTraffic> It(W); It; ++It) { T = *It; break; }
	if (!T) { ADP_WARN(TEXT("Traffic: test %s - no traffic here"), bTakeoff ? TEXT("take-off") : TEXT("landing")); return false; }
	if (RunwayBusy(W)) { ADP_WARN(TEXT("Traffic: test %s - the runway is busy"), bTakeoff ? TEXT("take-off") : TEXT("landing")); return false; }
	const bool bToX1 = T->RunwayToX1(); const float Dir = bToX1 ? 1.f : -1.f, RY = T->F.RwyY;
	FMove M; M.TypeId = TEXT("A320"); M.Reg = Reg = bTakeoff ? TEXT("HB-TOFF") : TEXT("HB-LAND"); M.FlightId = TEXT("TEST-") + M.Reg; M.bDeparture = bTakeoff;
	if (bTakeoff)
	{
		const float LineUp = bToX1 ? T->F.RwyX0 + 20.f : T->F.RwyX1 - 20.f;
		M.Phase = EPhase::Roll; M.Nose = FVector2f(LineUp + Dir * LineUpR, RY); M.Main = M.Nose - FVector2f(Dir * Wheelbase, 0.f);   // (lined up at the end: LineUpPath)
		M.Path = {M.Nose, FVector2f(LineUp + Dir * 3000.f, RY)};
	}
	else
	{
		const float Thr = bToX1 ? T->F.RwyX0 : T->F.RwyX1, TD = Thr + Dir * 500.f, Hd = HeadMs(bToX1), Out = (VappMs(PerfFor(TEXT("A320")), M.MassT, Hd) - Hd) * FMath::Max(SecOut, 5.f);
		M.Phase = EPhase::Approach; M.Nose = FVector2f(TD - Dir * Out, RY); M.Main = M.Nose - FVector2f(Dir * Wheelbase, 0.f);
		M.Height = Out * FMath::Tan(FMath::DegreesToRadians(Glide)); M.Speed = VappMs(PerfFor(TEXT("A320")), M.MassT, Hd) - Hd; M.Path = {FVector2f(TD, RY)};
		M.Stand = T->FreeStand();   // (no flight, no stand from the simulation: a free one to taxi in to)
	}
	if (T->Moves.ContainsByPredicate([&](const FMove& O) { return O.Reg == Reg; })) { ADP_WARN(TEXT("Traffic: test %s is still about"), *Reg); return false; }
	T->Seen.Add(M.FlightId);
	T->Moves.Add(M);
	// (2026-11-28: its model was made only in Sync, which waits for a running simulation - a views run has no career, so
	//  HB-TOFF stood in the list unseen and unmoving, kept the runway "busy" and HB-LAND was refused: made here, now)
	T->ModelFor(T->Moves.Last());
	ADP_LOG(TEXT("Traffic: test %s %s on runway %s, nose at %.0f %.0f, %.0f m up%s%s"), *Reg, bTakeoff ? TEXT("takes off") : TEXT("lands"), bToX1 ? TEXT("towards x1") : TEXT("towards x0"), M.Nose.X, M.Nose.Y, M.Height, M.Stand.IsEmpty() ? TEXT("") : TEXT(", stand "), *M.Stand);
	{ const FMove& N = T->Moves.Last(); if (N.Model.IsValid()) ADP_LOG(TEXT("Traffic: test %s spawned - model %s at %s"), *Reg, *N.Model->GetName(), *N.Model->GetActorLocation().ToCompactString());
	  else ADP_WARN(TEXT("Traffic: test %s - its model could not be made"), *Reg); }
	return T->Moves.Last().Model.IsValid();
}

bool ASimTraffic::TestTaxiOut(UWorld* W, FString& Reg)
{
	ASimTraffic* T = nullptr;
	if (W) for (TActorIterator<ASimTraffic> It(W); It; ++It) { T = *It; break; }
	if (!T) { ADP_WARN(TEXT("Traffic: test taxi-out - no traffic here")); return false; }
	Reg = TEXT("HB-TAXI");
	if (T->Moves.ContainsByPredicate([&](const FMove& O) { return O.Reg == Reg; })) { ADP_WARN(TEXT("Traffic: test %s is still about"), *Reg); return false; }
	const FString Id = T->FreeStand();
	const FAirfieldData::FStand* St = T->F.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == Id; });
	if (!St) { ADP_WARN(TEXT("Traffic: test taxi-out - no free passenger stand")); return false; }
	// on the taxilane by the stand, as the push leaves it: the main gear on T at the stand, the nose towards the connector
	const float Dx = TaxiOutDir(*St, T->RunwayToX1());
	FMove M; M.TypeId = TEXT("A320"); M.Reg = Reg; M.FlightId = TEXT("TEST-") + Reg; M.bDeparture = true; M.Stand = St->Id; M.Phase = EPhase::Taxi;
	M.Main = FVector2f(St->X, T->F.TY); M.Nose = M.Main + FVector2f(Dx * Wheelbase, 0.f);
	T->TaxiOutPath(M);
	T->Seen.Add(M.FlightId);
	T->Moves.Add(M);
	T->ModelFor(T->Moves.Last());
	ADP_LOG(TEXT("Traffic: test %s taxis out from stand %s (%d waypoints to the holding point, runway %s)"), *Reg, *St->Id, M.Path.Num(), T->RunwayToX1() ? TEXT("towards x1") : TEXT("towards x0"));
	return T->Moves.Last().Model.IsValid();
}

FString ASimTraffic::TestPhase(const UWorld* W, const FString& Reg)
{
	if (!W) return TEXT("gone");
	for (TActorIterator<ASimTraffic> It(const_cast<UWorld*>(W)); It; ++It)
		for (const FMove& M : It->Moves)
		{
			if (M.Reg != Reg) continue;
			switch (M.Phase)
			{
			case EPhase::Push: return TEXT("push");
			case EPhase::Taxi: return M.Path.Num() == 1 && FVector2f::Distance(M.Path[0], M.Nose) < 0.5f ? TEXT("hold") : TEXT("taxi");
			case EPhase::Roll: return M.Step == 0 || M.Clock < 4.f ? TEXT("lineup") : TEXT("roll");
			case EPhase::Climb: return M.Height < 100.f ? TEXT("climb") : !M.b10k ? TEXT("away") : FMath::Abs(M.Bank) > 5.f ? TEXT("turn") : TEXT("high");   // (high / turn: after the -AirplaneTest jump)
			case EPhase::Approach: return TEXT("final");
			case EPhase::Rollout: return TEXT("rollout");
			case EPhase::TaxiIn: return FMath::Abs(M.Nose.Y - It->F.RwyY) < 140.f && FMath::Abs(M.Nose.X - 81.f) < 30.f ? TEXT("vacate") : TEXT("taxiin");
			default: return TEXT("stand");
			}
		}
	return TEXT("gone");
}

void ASimTraffic::TaxiOutPath(FMove& M)
{
	// the tug has let go; taxi along T to the connector, up to A, along A to the holding point of the runway end in use
	const bool bToX1 = RunwayToX1();
	const float Dir = bToX1 ? 1.f : -1.f, RY = F.RwyY, LineUp = bToX1 ? F.RwyX0 + 20.f : F.RwyX1 - 20.f;
	const FAirfieldData::FStand* MySt = F.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == M.Stand; });
	const bool bCargoSt = MySt && MySt->bCargo;
	M.Step = 0; M.Path.Reset();
	if (bCargoSt)
	{
		// the cargo apron: along K to its end towards the runway end in use, down to B, along B and round its curve to the holding point
		const FAirfieldData::FTwy* Lk = F.Find(bToX1 ? TEXT("Q0") : TEXT("Q2"));
		const float Lx = Lk && Lk->Pts.Num() ? Lk->Pts[0].X : bToX1 ? -530.f : 620.f;
		M.Path = {FVector2f(M.Nose.X - Dir * 40.f, F.KY), FVector2f(Lx, F.KY), FVector2f(Lx, F.BY)};
		if (const FAirfieldData::FTwy* Cv = F.Find(bToX1 ? TEXT("CURVE-B-B1") : TEXT("CURVE-B-B3")))
			for (int32 i = Cv->Pts.Num() - 1; i >= 0; --i) M.Path.Add(FVector2f(Cv->Pts[i].X, Cv->Pts[i].Y));
		else M.Path.Add(FVector2f(LineUp - Dir * 40.f, F.BY));
		M.Path.Add(FVector2f(LineUp, RY + HoldStop));
		return;
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
	M.Path.Add(FVector2f(LineUp, RY - HoldStop));
}

// (11-28 owner: gentle smooth taxi turns) each corner of a taxi path becomes an arc of about 40 m radius - the curved
// centre line a pilot follows - instead of a kink where the nose wheel snapped to the next leg; the ends stay exact
static void RoundCorners(const FVector2f& From, TArray<FVector2f>& Path, float MinR = 0.f)
{
	const float R = FMath::Max(40.f, MinR);   // (about 40 m, never under the type's minimum turning radius)
	if (Path.Num() < 2) return;
	TArray<FVector2f> Out; Out.Reserve(Path.Num() * 4);
	for (int32 i = 0; i < Path.Num(); ++i)
	{
		const FVector2f A = i ? Path[i - 1] : From, B = Path[i];
		if (i == Path.Num() - 1) { Out.Add(B); break; }
		const FVector2f C = Path[i + 1];
		const float L1 = FVector2f::Distance(A, B), L2 = FVector2f::Distance(B, C);
		if (L1 < 0.5f || L2 < 0.5f) { Out.Add(B); continue; }
		const FVector2f U1 = (B - A) / L1, U2 = (C - B) / L2;
		const float Th = FMath::Acos(FMath::Clamp(FVector2f::DotProduct(U1, U2), -1.f, 1.f));
		if (Th < FMath::DegreesToRadians(3.f)) { Out.Add(B); continue; }
		const float D = FMath::Min3(R * FMath::Tan(Th * 0.5f), 0.45f * L1, 0.45f * L2);
		const FVector2f S = B - U1 * D, E = B + U2 * D;
		const int32 N = FMath::Clamp(FMath::CeilToInt(FMath::RadiansToDegrees(Th) / 6.f), 2, 24);
		for (int32 k = 0; k <= N; ++k) { const float T = float(k) / N; Out.Add((1.f - T) * (1.f - T) * S + 2.f * (1.f - T) * T * B + T * T * E); }
	}
	Path = MoveTemp(Out);
}

bool ASimTraffic::Advance(FMove& M, float Dt)
{
	if ((M.Phase == EPhase::Taxi || M.Phase == EPhase::TaxiIn || M.Phase == EPhase::Rollout) && M.Step == 0 && M.Path.Num() > 1 && (M.Path.Num() != M.RoundedNum || M.Path.Last() != M.RoundedEnd))
	{
		RoundCorners(M.Nose, M.Path, PerfFor(M.TypeId).MinTurnRadiusM);
		M.RoundedNum = M.Path.Num(); M.RoundedEnd = M.Path.Last();
	}
	const bool bToX1 = RunwayToX1();
	const float Dir = bToX1 ? 1.f : -1.f, RY = F.RwyY, X0 = F.RwyX0, X1 = F.RwyX1, Hold = 107.5f;
	const float Head = HeadMs(bToX1);   // (m/s along the runway in use; - a tailwind)
	const FAcPerf& Pf = PerfFor(M.TypeId);   // (its type's row of the performance table)
	const float MaxTaxi = Pf.MaxTaxiKt * KtMs, TurnTaxi = Pf.TurnTaxiKt * KtMs, StandTaxi = Pf.StandTaxiKt * KtMs, PushSpeed = Pf.PushKt * KtMs;
	const FAirfieldData::FStand* MySt = M.bDeparture ? F.Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == M.Stand; }) : nullptr;
	const bool bCargoSt = MySt && MySt->bCargo; const float LaneY = bCargoSt ? F.KY : F.TY;   // (the taxilane it is pushed onto)
	// the tower (to-do 378, RunwayRadio.h): in the game and in the tests alike nothing goes onto the runway, lands or taxis in
	// without its clearance read back. The pilot's callsign as in the tug's talk (the registration's last three letters,
	// ICAO), his voice one of the men's by the registration
	if (M.Cs.IsEmpty())
	{
		M.Cs = ADPRunwayRadio::Spell(M.Reg.Mid(M.Reg.Find(TEXT("-")) + 1).Right(3));
		static const TCHAR* Vo[] = {TEXT("bm_george"), TEXT("bm_lewis"), TEXT("bm_daniel"), TEXT("bm_fable"), TEXT("am_michael"), TEXT("am_adam")};
		M.Voice = Vo[GetTypeHash(M.Reg) % UE_ARRAY_COUNT(Vo)];
	}
	auto Talk = [&](uint8 For, const FString& Ask, bool bGive, const FString& Give, const FString& HoldLine, const FString& HoldBack, const FString& Back)
	{
		if (M.RadFor != For) { M.Rad = ADPRunwayRadio::FCall(); M.RadFor = For; }
		ADPRunwayRadio::FWho Who; Who.Radio = M.Model.Get(); Who.Voice = M.Voice; Who.Callsign = M.Cs;
		return ADPRunwayRadio::Exchange(GetWorld(), M.Rad, Who, Ask, bGive, Give, HoldLine, HoldBack, Back);
	};
	// what keeps the runway from this one, as the tower says it (empty: free)
	auto Taken = [&](bool bLanding) -> FString
	{
		for (const FMove& O : Moves)
		{
			if (&O == &M) continue;
			if (O.Phase == EPhase::Roll || (O.Phase == EPhase::Climb && O.Height < 100.f)) return TEXT("traffic departing");
			if (O.Phase == EPhase::Rollout || (O.Phase == EPhase::TaxiIn && O.Step == 0 && FMath::Abs(O.Main.Y - RY) < 45.f)) return TEXT("runway occupied");
			if (!bLanding && O.Phase == EPhase::Approach && (O.Height < 200.f || O.bLandOk)) return TEXT("traffic on final");
			if (bLanding && O.Phase == EPhase::Approach && O.bLandOk) return TEXT("traffic landing ahead");
			if (bLanding && O.Phase == EPhase::Taxi && O.bTakeoffOk) return TEXT("traffic departing");
		}
		if (AHangarTow::OnRunway(GetWorld())) return TEXT("traffic crossing the runway");
		return FString();
	};
	const FString RwS = RunwaySpoken(GetWorld());
	// lined up without a take-off clearance (a test's take-off starts there): asked for here, the roll after it
	if (M.Phase == EPhase::Roll && M.Step >= M.Path.Num() - 1 && !M.bTakeoffOk)
	{
		const FString Why = Taken(false);
		if (Talk(1, FString::Printf(TEXT("%s, %s, lined up runway %s, ready for departure."), *ADPRunwayRadio::Tower(), *M.Cs, *RwS), Why.IsEmpty(),
			FString::Printf(TEXT("%s, runway %s, cleared for take-off."), *M.Cs, *RwS), FString::Printf(TEXT("%s, hold position, %s."), *M.Cs, *Why),
			FString::Printf(TEXT("Holding position, %s."), *M.Cs), FString::Printf(TEXT("Cleared for take-off runway %s, %s."), *RwS, *M.Cs)))
		{ M.bTakeoffOk = true; ADP_LOG(TEXT("Traffic: %s cleared for take-off runway %s (lined up)"), *M.Reg, *RwS); }
		else M.Clock = 0.f;   // (its moment lined up counts from the clearance)
	}
	// on final: cleared to land once the runway is free and it is within about 7 km (400 m up); never cleared by 60 m: warned
	if (M.Phase == EPhase::Approach && !M.bLandOk)
	{
		const FString Why = Taken(true);
		if (Talk(2, FString::Printf(TEXT("%s, %s, established runway %s."), *ADPRunwayRadio::Tower(), *M.Cs, *RwS), Why.IsEmpty() && M.Height < 400.f,
			FString::Printf(TEXT("%s, runway %s, cleared to land."), *M.Cs, *RwS), Why.IsEmpty() ? FString() : FString::Printf(TEXT("%s, continue approach, %s."), *M.Cs, *Why),
			FString::Printf(TEXT("Continue approach, %s."), *M.Cs), FString::Printf(TEXT("Cleared to land runway %s, %s."), *RwS, *M.Cs)))
		{ M.bLandOk = true; ADP_LOG(TEXT("Traffic: %s cleared to land runway %s at %.0f m"), *M.Reg, *RwS, M.Height); }
		else if (M.Height < 60.f && !M.Rad.bIncursion) { M.Rad.bIncursion = true; ADP_WARN(TEXT("Traffic: %s below 60 m on final without a landing clearance (%s) - it lands anyway, no go-around yet"), *M.Reg, Why.IsEmpty() ? TEXT("the exchange not finished") : *Why); }
	}
	// off the runway: "vacated", the tower's taxi clearance to the stand and its readback - short of the taxiway until then
	bool bTaxiInHeld = false;
	if (M.Phase == EPhase::TaxiIn && !M.bTaxiInOk && !M.Stand.IsEmpty() && M.Path.Num() > 1)
	{
		const bool bN = M.Path.Last().Y > RY; const FString Via = bN ? TEXT("Bravo, Kilo") : TEXT("Alpha, Tango");
		const FString StS = TEXT("stand ") + M.Stand.Replace(TEXT("STAND-"), TEXT(""));
		if (M.Step == 0 && FMath::Abs(M.Main.Y - RY) < 45.f) bTaxiInHeld = true;   // (still on the strip: the call once it is off)
		else if (Talk(3, FString::Printf(TEXT("%s, %s, runway %s vacated."), *ADPRunwayRadio::Tower(), *M.Cs, *RwS), true,
			FString::Printf(TEXT("%s, taxi to %s via %s."), *M.Cs, *StS, *Via), FString(), FString(), FString::Printf(TEXT("Taxi to %s via %s, %s."), *StS, *Via, *M.Cs)))
		{ M.bTaxiInOk = true; ADP_LOG(TEXT("Traffic: %s cleared to taxi to %s"), *M.Reg, *StS); }
		else bTaxiInHeld = true;
	}
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
	else if (M.Phase == EPhase::Roll && M.Step < M.Path.Num() - 1)
	{
		// (up to the hold's end and round the line-up turn at turn speed, slowing to a stop lined up on the centre line)
		float Rem = FVector2f::Distance(M.Nose, M.Path[M.Step]);
		for (int32 i = M.Step + 1; i < M.Path.Num() - 1; ++i) Rem += FVector2f::Distance(M.Path[i - 1], M.Path[i]);
		Want = FMath::Min(TurnTaxi, 0.3f + FMath::Sqrt(1.2f * Rem));
	}
	// (on the runway and in the air the table's speeds are IAS: the groundspeed M.Speed is the IAS less the headwind)
	else if (M.Phase == EPhase::Roll) Want = M.Clock < 4.f ? 0.f : VrMs(Pf, M.MassT) + 20.f * KtMs - Head;   // (a moment lined up, then the take-off power: never reached before lift-off)
	else if (M.Phase == EPhase::Climb) Want = M.b10k ? M.Speed : M.Height < Pf.AccelHeightM ? V2Ms(Pf, M.MassT) + Pf.InitClimbAddKt * KtMs - Head : FMath::Min(Pf.ClimbKt, Pf.TopKt) * KtMs - Head;   // (V2+10, then 250 kt below FL100; the -AirplaneTest jump keeps its speed for its fixed turn)
	else if (M.Phase == EPhase::Approach) Want = VappMs(Pf, M.MassT, Head) - (M.Height < FlareM ? TouchdownLossMs(Pf) : 0.f) - Head;   // (VAPP; idle in the flare to touch down about 7 kt slower)
	else if (M.Phase == EPhase::Rollout) Want = FMath::Abs(M.Nose.X - 81.f) < 80.f ? TurnTaxi : Pf.VacateKt * KtMs;   // (about 10 kt for the turn off at the exit)
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
		// gentle braking to the leg's end - the hold-short line, the stand's stop mark (0.5 m/s2, never a dead stop from taxi speed)
		if (M.Phase == EPhase::Taxi || M.Phase == EPhase::TaxiIn)
		{
			float Rem = FVector2f::Distance(M.Nose, M.Path[M.Step]);
			for (int32 i = M.Step + 1; i < M.Path.Num() && Rem < 200.f; ++i) Rem += FVector2f::Distance(M.Path[i - 1], M.Path[i]);
			if (Rem < 200.f) Want = FMath::Min(Want, FMath::Max(0.4f, FMath::Sqrt(2.f * 0.5f * Rem)));
		}
	}
	if (bTaxiInHeld) Want = M.Step >= 1 ? 0.f : FMath::Min(Want, FMath::Max(0.4f, FMath::Sqrt(FVector2f::Distance(M.Nose, M.Path[0]))));   // (waits on the exit for its taxi clearance)
	// the rates (m/s2, the table): the take-off roll by the mass (thrust over mass), the line-up turn as before, the climb's
	// acceleration, the flare's idle; the landing roll: a free roll till the spoilers and autobrake come on, then the
	// autobrake chosen at touchdown on this surface (reversers in to about 70 kt, then the brakes alone - Spoilers()),
	// harder only if that would carry it past the exit (logged), off at taxi speed
	float Rate = 1.f;
	if (M.Phase == EPhase::Roll) Rate = M.Step < M.Path.Num() - 1 ? 2.6f : Pf.RollAcc * Pf.RefMassT / FMath::Clamp(M.MassT, Pf.MinMassT, Pf.MaxMassT);
	else if (M.Phase == EPhase::Climb && M.Height >= Pf.AccelHeightM) Rate = Pf.ClimbAcc;
	else if (M.Phase == EPhase::Approach && M.Height < FlareM) Rate = Pf.FlareDec;
	if (M.Phase == EPhase::Rollout)
	{
		float Dec = M.NoseT < Pf.SpoilerDelayS ? Pf.FreeRollDec : M.AbDec;
		const float VEx = Pf.VacateKt * KtMs, Rem = FMath::Abs(M.Nose.X - 81.f) - 80.f;
		if (M.NoseT >= Pf.SpoilerDelayS && M.Speed > VEx)
		{
			const float Need = (M.Speed * M.Speed - VEx * VEx) / (2.f * FMath::Max(Rem, 1.f));
			if (Need > Dec * 1.02f)
			{
				if (!M.bHardBrake) ADP_LOG(TEXT("Traffic: %s brakes harder (%.1f m/s2, autobrake %.1f) to make the exit at %.0f kt, %.0f m to go"), *M.Reg, Need, M.AbDec, M.Speed / KtMs, Rem);
				M.bHardBrake = true; Dec = Need;
			}
		}
		M.Speed = FMath::Max(Want, M.Speed - Dec * Dt);
	}
	else M.Speed = FMath::FInterpConstantTo(M.Speed, Want, Dt, Rate);
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
		if (M.Step == M.ArcStep && M.ArcR > 1.f)
		{
			// (the arc: the nose on the circle, the main gear one wheelbase back along its tangent - constant yaw rate)
			const float Go = FMath::Min(Left, (M.ArcEnd - M.ArcA) * M.ArcR);
			M.ArcA += Go / M.ArcR; Left -= Go;
			const float Sa = FMath::Sin(M.ArcA), Ca = FMath::Cos(M.ArcA);
			M.Nose = M.ArcC - M.ArcN * (M.ArcR * Ca) + M.ArcD * (M.ArcR * Sa);
			M.Main = M.Nose - (M.ArcD * Ca + M.ArcN * Sa) * Wheelbase;
			if (M.ArcA >= M.ArcEnd - 1e-5f) { M.Nose = M.Path[M.Step]; ++M.Step; M.ArcR = 0.f; M.ArcStep = -1; }
			continue;
		}
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
	if (M.Phase == EPhase::Roll && M.Step >= M.Path.Num() - 1) M.Clock += Dt;   // (from lined up)
	// the attitude, as a real A320 flies it (11-28 owner): rotation at VR about 3 deg/s to about 10-12 deg, lift-off about 4 s
	// later, then up to the climb's 15 deg while the path angle and the climb rate grow gradually; on the approach about 3 deg,
	// the flare from about 30 ft to about 5.5 deg, the mains touch first; about 2 s later the nose is lowered very gently
	// (about 1 deg/s, slower at the end) until the nose wheel touches softly. The pitch's own rate is eased too (EasePitch):
	// never a jump in the attitude or in its rate
	float PitchWant = 0.f, PitchRate = 3.f;
	if (M.Phase == EPhase::Roll && M.Step >= 1 && (M.RotT >= 0.f || M.Speed + Head >= VrMs(Pf, M.MassT)))
	{
		if (M.RotT < 0.f) ADP_LOG(TEXT("Traffic: %s rotates at %.0f kt IAS (%.0f kt groundspeed, %.0f t, %.0f m rolled; about 3 deg/s)"), *M.Reg, (M.Speed + Head) / KtMs, M.Speed / KtMs, M.MassT, FMath::Abs(M.Nose.X - (bToX1 ? X0 : X1)));
		M.RotT = FMath::Max(0.f, M.RotT) + Dt; PitchWant = 11.f;
	}
	else if (M.Phase == EPhase::Climb) { M.RotT += Dt; PitchWant = M.b10k && M.Height >= TenK ? 6.f : 15.f; PitchRate = 1.5f; }
	else if (M.Phase == EPhase::Approach) { PitchWant = M.Height < FlareM ? 5.5f : 3.f; PitchRate = M.Height < FlareM ? 1.2f : 1.f; }
	else if (M.Phase == EPhase::Rollout)
	{
		M.NoseT += Dt;
		if (M.NoseT < 2.f) PitchWant = M.Pitch;            // (the mains on, the nose held up for about 2 s)
		else { PitchWant = 0.f; PitchRate = 1.f; }          // (then lowered at about 1 deg/s, easing off at the end)
	}
	EasePitchTo(M.Pitch, M.PitchV, PitchWant, PitchRate, Dt);
	if (M.Phase == EPhase::Roll && M.RotT >= 4.f && M.Step < M.Path.Num())
	{
		ADP_LOG(TEXT("Traffic: %s lifts off at %.0f kt, %.1f deg nose up, %.1f s after rotating"), *M.Reg, M.Speed / KtMs, M.Pitch, M.RotT);
		M.Step = M.Path.Num();   // (the roll ends here: off the ground, not at the end of a fixed run)
		M.LiftT = 0.f;
	}
	if (M.Phase == EPhase::Rollout && !M.bNoseDown && Wheelbase * FMath::Sin(FMath::DegreesToRadians(M.Pitch)) < 0.02f)
	{
		M.bNoseDown = true;
		ADP_LOG(TEXT("Traffic: %s nose wheel touches down %.1f s after the mains, at %.0f kt"), *M.Reg, M.NoseT, M.Speed / KtMs);
	}
	if (M.Phase == EPhase::Climb || M.Phase == EPhase::Approach)
	{
		// the climb's path angle grows from level to about 8 deg over the first 8 s (a smooth step: the height and its rate
		// both continuous from the runway); the 3 degree approach down to the touchdown zone, its last 30 ft rounded off
		if (M.Phase == EPhase::Climb)
		{
			if (M.LiftT >= 0.f) M.LiftT += Dt;
			if (M.b10k) M.Gamma = FMath::FInterpConstantTo(M.Gamma, M.Height >= TenK ? 0.f : 8.f, Dt, 1.f);   // (levels off at 10,000 ft)
			else M.Gamma = M.LiftT >= 0.f ? 8.f * FMath::SmoothStep(0.f, 8.f, M.LiftT) : FMath::FInterpConstantTo(M.Gamma, 8.f, Dt, 2.f);
			M.Height += M.Speed * Dt * FMath::Tan(FMath::DegreesToRadians(M.Gamma));
			// test only (-AirplaneTest): 20 s after lift-off on to 10,000 ft on its own climb track, flying on there for the camera
			if (AirplaneTest() && !M.b10k && M.LiftT >= 20.f && M.Path.Num())
			{
				const FVector2f D = (M.Nose - M.Main).GetSafeNormal();
				const float Ahead = FMath::Max(0.f, TenK - 30.f - M.Height) / FMath::Tan(FMath::DegreesToRadians(FMath::Max(M.Gamma, 1.f)));
				M.Nose += D * Ahead; M.Main += D * Ahead; M.Height = TenK - 30.f; M.b10k = true;
				// (about 8 s straight, then a 180 deg standard-rate turn to the right - 3 deg/s, some 24 deg of bank at 160 kt,
				//  the wings seen to dip and level again - and on straight)
				const FVector2f N(-D.Y, D.X); const float R = M.Speed / FMath::DegreesToRadians(3.f);
				const FVector2f P0 = M.Nose + D * (M.Speed * 8.f), C = P0 + N * R;
				M.Path.Reset(); M.Path.Add(P0);
				// (flown as one true arc, not the old 1 deg chords: Path[1] is the arc's end, reached round C at a constant 3 deg/s)
				M.Path.Add(C + N * R); M.ArcC = C; M.ArcD = D; M.ArcN = N; M.ArcR = R; M.ArcA = 0.f; M.ArcEnd = PI; M.ArcStep = 1;
				M.Path.Add(M.Path.Last() - D * 80000.f); M.Step = 0;
				ADP_LOG(TEXT("Traffic: %s test jump - %.0f m on along its climb track to %.0f m (10,000 ft is %.0f m), flying on there"), *M.Reg, Ahead, M.Height, TenK);
			}
			if (!M.bBelt && M.Height > TenK) { M.bBelt = true; ADP_LOG(TEXT("Traffic: %s passes 10,000 ft - the seatbelt announcement is due"), *M.Reg); }
		}
		else
		{
			// (the flare: below 30 ft the height follows a cubic of the 3 degree line's - same height and sink rate where it
			//  starts, about 0.15 of the sink rate (some 0.5 m/s) at touchdown: no step in the height or its rate)
			const float H = FMath::Max(0.f, FMath::Abs(M.Nose.X - M.Path.Last().X) * FMath::Tan(FMath::DegreesToRadians(Glide)));
			constexpr float A1 = 0.15f, C3 = (A1 - 1.f) / (FlareM * FlareM), B2 = 2.f * (1.f - A1) / FlareM;
			if (H < FlareM && M.Height >= FlareM && M.NoseT < -0.5f) { M.NoseT = -0.25f; ADP_LOG(TEXT("Traffic: %s flare at 30 ft, %.0f kt, %.1f deg nose up"), *M.Reg, M.Speed / KtMs, M.Pitch); }
			M.Height = H < FlareM ? FMath::Max(0.001f, H * (A1 + H * (B2 + H * C3))) : H;
		}
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
		TaxiOutPath(M);
		return false;
	case EPhase::Taxi:
	{
		// at the holding point: "ready for departure"; the tower clears it to line up and take off once the runway is free
		// (nothing rolling, landing, cleared to land or on short final, no tow crossing), or holds it short and says why
		if (!M.bTakeoffOk)
		{
			const FString Why = Taken(false), Twy = ADPRunwayRadio::TwyAt(M.Nose);
			const bool bOk = Talk(1, FString::Printf(TEXT("%s, %s, holding point %s, ready for departure."), *ADPRunwayRadio::Tower(), *M.Cs, *Twy), Why.IsEmpty(),
				FString::Printf(TEXT("%s, line up runway %s, cleared for take-off."), *M.Cs, *RwS), FString::Printf(TEXT("%s, hold short runway %s, %s."), *M.Cs, *RwS, *Why),
				FString::Printf(TEXT("Holding short runway %s, %s."), *RwS, *M.Cs), FString::Printf(TEXT("Line up runway %s, cleared for take-off, %s."), *RwS, *M.Cs));
			if (!bOk) { M.Speed = 0.f; M.Path = {M.Nose}; M.Step = 0; return false; }
			M.bTakeoffOk = true; ADP_LOG(TEXT("Traffic: %s cleared to line up and take off runway %s"), *M.Reg, *RwS);
		}
		M.Clock = 0.f;
		M.Phase = EPhase::Roll; M.Path = LineUpPath(LineUp, RY, Dir, M.Nose.Y > RY ? 1.f : -1.f); M.Step = 0;
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
		M.NoseT = 0.f; M.bNoseDown = false;
		{
			// the autobrake as the crew set it: LOW if it stops for the exit on this surface, else MED (FCOM); the surface
			// lengthens the stop (the table's factors)
			const ERwyCond Cond = RunwayCond(); const float Fc = CondFactor(Pf, Cond), VEx = Pf.VacateKt * KtMs;
			const float Avail = FMath::Abs(M.Nose.X - 81.f) - 80.f - M.Speed * Pf.SpoilerDelayS;
			const float LowD = (M.Speed * M.Speed - VEx * VEx) / (2.f * Pf.BrakeLowDry / Fc);
			const bool bLow = LowD <= Avail;
			M.AbDec = (bLow ? Pf.BrakeLowDry : Pf.BrakeMedDry) / Fc; M.bHardBrake = false;
			ADP_LOG(TEXT("Traffic: %s autobrake %s on a %s runway (%.1f m/s2), %.0f kt IAS, %.0f kt headwind, %.0f t, %.0f m to the exit"), *M.Reg, bLow ? TEXT("LOW") : TEXT("MED"),
				Cond == ERwyCond::Snow ? TEXT("snow-covered") : Cond == ERwyCond::Wet ? TEXT("wet") : TEXT("dry"), M.AbDec, (M.Speed + HeadMs(bToX1)) / KtMs, HeadMs(bToX1) / KtMs, M.MassT, FMath::Abs(M.Nose.X - 81.f));
		}
		ADP_LOG(TEXT("Traffic: %s main gear touches down at %.0f kt, %.1f deg nose up - the struts compress, the wheels spin up"), *M.Reg, M.Speed / KtMs, M.Pitch);
		TouchSmoke(M);
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
	Smoke(FMath::Min(Dt, 0.1f));
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
		Haze(M, FMath::Min(Dt, 0.1f));
		Surfaces(M, FMath::Min(Dt, 0.1f));
		Spoilers(M, FMath::Min(Dt, 0.1f));
		if (ABabylonModel* Bm = M.Model.Get())
		{
			ADPA320Snd::FIn Si; Si.bEngines = M.bEnginesOn; Si.Power = M.Power; Si.Rev = M.RevPos; Si.GearSt = uint8(M.GearSt); Si.Flap = M.FlapDeg;
			Si.Height = M.Height; Si.Speed = M.Speed; Si.Stage = uint8(M.Phase);
			// (normal play: the visible climb ends near 1,200 m - the 10,000 ft announcement goes with its last 15 s)
			Si.bLeaving = M.Phase == EPhase::Climb && !M.b10k && M.Path.Num() && FVector2f::Distance(M.Nose, M.Path.Last()) < 15.f * FMath::Max(M.Speed, 1.f);
			if (Si.bLeaving && !M.bBelt) { M.bBelt = true; ADP_LOG(TEXT("Traffic: %s leaves the visible climb at %.0f m - the seatbelt announcement is due"), *M.Reg, M.Height); }
			ADPA320Snd::Moving(Bm, M.Reg, Si, FMath::Min(Dt, 0.1f));
			Fans(M, FMath::Min(Dt, 0.1f));   // (after the sound: it moves the engines' N1 on)
		}
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
	// the gear parts' pivots (the model's root, cm: Blender (x, y, z) m -> (x, -y, z) * 100 - the FlightGear A320-200 CFM
	// model as Art/A320/make_a320_fg.py places it, its "FGPOS" lines)
	const FVector NoseAxle(0.f, 0.f, 37.6f);                  // the nose wheels' axle
	const FVector MainAxle[2] = {FVector(-387.1f, 1252.2f, 54.4f), FVector(385.f, 1252.2f, 54.4f)};
	constexpr float NoseR = 37.6f, MainR = 54.4f;             // (tyre radii, cm)
	constexpr float SteerSign = -1.f;                         // (the steer angle is the plan's, + to the left; + about the root's z turns the wheel right)
	// (11-28: the struts looked compressed in the air - they hung out 12/8 cm; make_a320_fg.py built them 30 cm in (STATIC_M):
	//  fully out in the air is 30 cm, straight down, back to FlightGear's own length)
	constexpr float StrutCm = 30.f;
	// the retraction (to-do 380), as the A320 flies it: gear up once climbing, 10 m (about 35 ft) and 2.5 s after lift-off;
	// gear down at 600 m (2,000 ft, about 11 km out), down and locked long before 300 m (1,000 ft)
	constexpr float GearUpM = 10.f, GearUpAfter = 2.5f, GearDownM = 600.f;
	// FlightGear's A320 (Systems/a320-fcs.xml, Models/fuselage.xml), position-norm 1 down - 0 up: the doors open from 1 to
	// 0.8 and close from 0.2 to 0, the legs move from 0.8 to 0.2; nose 10.5 s up / 12 s down, main 9 s up / 16 s down
	constexpr float NoseUpRate = 1.f / 10.5f, NoseDownRate = 1.f / 12.f, MainUpRate = 1.f / 9.f, MainDownRate = 1.f / 16.f;
	// every pivot and axis below: FlightGear's, placed on the built model by Art/A320/a320_surfaces.py (its GEAR lines);
	// the axis is the one FQuat turns FlightGear's +degrees about
	struct FGearHinge { FVector P, Ax; float Deg; };
	const FGearHinge NoseHinge = {FVector(-47.15f, 32.18f, 202.04f), FVector(-1.f, 0.00005f, -0.00013f), 90.f};
	const FGearHinge MainHinge[2] = {{FVector(-384.67f, 1229.16f, 303.03f), FVector(0.01547f, 0.98378f, 0.17871f), -81.11f},
		{FVector(384.56f, 1229.13f, 303.13f), FVector(-0.01542f, 0.98378f, 0.17871f), 81.11f}};
	const FGearHinge BayDoorH[2] = {{FVector(-25.49f, 1268.81f, 169.43f), FVector(0.00005f, 0.99999f, 0.00521f), -90.f},
		{FVector(25.44f, 1268.81f, 169.44f), FVector(0.00005f, 0.99999f, 0.00521f), 90.f}};
	const FGearHinge FairingH[2] = {{FVector(-397.31f, 1171.43f, 276.54f), FVector(-0.04122f, -0.99295f, -0.11115f), -121.5f},
		{FVector(396.98f, 1171.39f, 276.65f), FVector(0.04115f, -0.99295f, -0.11114f), 121.5f}};
	const FGearHinge NoseDoorH[2] = {{FVector(-52.14f, -197.45f, 201.27f), FVector(-0.00031f, -0.99688f, 0.07890f), -55.f},
		{FVector(51.66f, -197.46f, 201.29f), FVector(0.00020f, -0.99688f, 0.07890f), 55.f}};
	const FGearHinge AftDoorH[2] = {{FVector(-52.05f, 24.48f, 185.74f), FVector(0.00005f, 0.99999f, 0.00442f), 55.f},
		{FVector(51.59f, 24.47f, 185.75f), FVector(-0.00005f, -0.99999f, -0.00442f), 55.f}};
	// the steering axis up the nose strut (FlightGear's leans about 5 degrees sideways, not symmetric: kept upright here)
	const FVector SteerP(0.f, 12.51f, 92.05f), SteerAx(0.f, 0.14489f, 0.98945f);
	const FVector FgUp(-0.00013f, -0.00442f, 0.99999f);       // (FlightGear's up, the doors lift along it)
	// the bay doors' actuators: turned about their lock axis to follow the door, the rods sliding out (FlightGear's table, m)
	const FVector DoorActC[2] = {FVector(-29.44f, 1124.07f, 201.72f), FVector(29.36f, 1124.06f, 201.73f)};
	const FVector DoorActT[2] = {FVector(-83.93f, 1124.22f, 168.25f), FVector(83.86f, 1124.21f, 168.27f)};
	const FVector DoorActLock(0.00005f, 0.99999f, 0.00442f);
	const FVector DoorRodDir[2] = {FVector(-0.85206f, 0.00235f, -0.52344f), FVector(0.85119f, 0.00228f, -0.52485f)};
	// the tables, against position-norm (x): a share of the full turn, or metres
	const FVector2f TLeg[] = {{0.2f, 1.f}, {0.8f, 0.f}};
	const FVector2f TDoor[] = {{0.f, 0.f}, {0.2f, 1.f}, {0.8f, 1.f}, {1.f, 0.f}};
	const FVector2f TFairing[] = {{0.2f, 0.f}, {0.8f, 1.f}};
	const FVector2f TAftDoor[] = {{0.f, 0.f}, {0.2f, 1.f}};
	const FVector2f TNoseLift[] = {{0.f, 0.f}, {0.2f, 0.04f}, {0.8f, 0.04f}, {1.f, 0.f}};
	const FVector2f TAftLift[] = {{0.f, 0.f}, {0.8f, 0.04f}};
	const FVector2f TRod[] = {{0.f, 0.f}, {0.1f, 0.25f}, {0.2f, 0.3f}, {0.8f, 0.3f}, {0.9f, 0.25f}, {1.f, 0.f}};
	template <int32 N> float GTab(const FVector2f (&T)[N], float X)
	{
		if (X <= T[0].X) return T[0].Y;
		for (int32 i = 1; i < N; ++i) if (X <= T[i].X) return FMath::Lerp(T[i - 1].Y, T[i].Y, (X - T[i - 1].X) / FMath::Max(T[i].X - T[i - 1].X, 1e-4f));
		return T[N - 1].Y;
	}
	// FlightGear's locked-track pairs: two bars from A (on its parent) to B (on the other part), bending at the knee K about
	// the hinge H - the knee on the circle about A (H its plane's normal), the second bar's length from B. FlightGear's pose
	// (struts out); the ends on a strut's tube (bTube) sit StrutCm higher in the built model
	struct FGearLinkDef { const TCHAR* Bar1; const TCHAR* Bar2; FVector A, K, B, H; bool bTube; };
	const FGearLinkDef LinkDef[] = {
		{TEXT("SideBraceUp_L"), TEXT("SideBraceLo_L"), FVector(-277.56f, 1145.13f, 271.24f), FVector(-322.34f, 1185.02f, 213.88f), FVector(-371.51f, 1231.08f, 148.36f), FVector(0.13705f, 0.85431f, 0.50138f), false},
		{TEXT("SideBraceUp_R"), TEXT("SideBraceLo_R"), FVector(277.46f, 1145.10f, 271.31f), FVector(322.26f, 1184.99f, 213.96f), FVector(371.45f, 1231.04f, 148.45f), FVector(-0.13709f, 0.85432f, 0.50134f), false},
		{TEXT("SideBraceUp_L.LockStay1"), TEXT("SideBraceUp_L.LockStay2"), FVector(-387.69f, 1166.66f, 262.99f), FVector(-348.00f, 1176.46f, 235.44f), FVector(-323.64f, 1179.92f, 222.89f), FVector(0.13705f, 0.85431f, 0.50138f), false},
		{TEXT("SideBraceUp_R.LockStay1"), TEXT("SideBraceUp_R.LockStay2"), FVector(387.60f, 1166.63f, 263.09f), FVector(347.91f, 1176.42f, 235.53f), FVector(324.01f, 1179.85f, 223.20f), FVector(-0.13709f, 0.85432f, 0.50134f), false},
		{TEXT("MainLeg_L.TorqueLink1"), TEXT("MainLeg_L.TorqueLink2"), FVector(-387.18f, 1224.29f, 89.93f), FVector(-385.17f, 1207.00f, 52.90f), FVector(-387.13f, 1237.11f, 26.71f), FVector(1.f, 0.f, 0.f), true},
		{TEXT("MainLeg_R.TorqueLink1"), TEXT("MainLeg_R.TorqueLink2"), FVector(387.14f, 1224.25f, 90.03f), FVector(385.13f, 1206.96f, 53.00f), FVector(387.10f, 1237.08f, 26.81f), FVector(1.f, 0.f, 0.f), true},
		{TEXT("MainLegUpper_L.SlaveLink1"), TEXT("MainLegUpper_L.SlaveLink2"), FVector(-387.18f, 1255.61f, 93.06f), FVector(-387.25f, 1276.61f, 65.12f), FVector(-387.15f, 1262.84f, 32.38f), FVector(-1.f, 0.f, 0.f), true},
		{TEXT("MainLegUpper_R.SlaveLink1"), TEXT("MainLegUpper_R.SlaveLink2"), FVector(387.15f, 1255.58f, 93.16f), FVector(387.22f, 1276.57f, 65.22f), FVector(387.13f, 1262.81f, 32.48f), FVector(-1.f, 0.f, 0.f), true},
		{TEXT("NoseLegUpper.DragStrut1"), TEXT("NoseLegUpper.DragStrut2"), FVector(-0.12f, -102.25f, 285.40f), FVector(-0.10f, -32.78f, 179.18f), FVector(-0.14f, 0.57f, 128.17f), FVector(-1.f, 0.f, 0.f), false},
		{TEXT("NoseLegUpper.LockStay2"), TEXT("NoseLegUpper.LockStay1"), FVector(-0.07f, -25.95f, 179.00f), FVector(-0.10f, -5.45f, 186.44f), FVector(-0.10f, 12.89f, 194.04f), FVector(-1.f, 0.f, 0.f), false},
		{TEXT("NoseLeg.TorqueLink1"), TEXT("NoseLeg.TorqueLink2"), FVector(-0.72f, 18.37f, 68.84f), FVector(-0.63f, 38.47f, 35.89f), FVector(-0.54f, 8.52f, 11.82f), FVector(-1.f, 0.f, 0.f), true},
	};
	enum EGearLink { LSide = 0, LLock = 2, LTorque = 4, LSlave = 6, LDrag = 8, LNoseLock = 9, LNoseTorque = 10, LNum = 11 };
	FVector GearKnee(const FVector& A, const FVector& B, double L1, double L2, FVector H, double Side)
	{
		H = H.GetSafeNormal(); const double Off = FVector::DotProduct(B - A, H); const FVector Bp = B - H * Off;
		const double R2 = FMath::Sqrt(FMath::Max(L2 * L2 - Off * Off, 1e-6));
		const double D = FMath::Clamp(FMath::Max((Bp - A).Size(), 1e-6), FMath::Abs(L1 - R2) + 0.01, L1 + R2 - 0.01);
		const FVector E1 = (Bp - A).GetSafeNormal(), E2 = FVector::CrossProduct(H, E1);
		const double Ax = (L1 * L1 - R2 * R2 + D * D) / (2.0 * D), Hh = FMath::Sqrt(FMath::Max(L1 * L1 - Ax * Ax, 0.0));
		return A + E1 * Ax + E2 * (Hh * Side);
	}
	// a bar carried by its parent's move T, then turned about its own end to point at its new knee
	FTransform BarTo(const FVector& P0, const FVector& K0, const FTransform& T, const FVector& K1)
	{
		const FVector P1 = T.TransformPosition(P0); const FQuat R0 = T.GetRotation();
		const FQuat R = FQuat::FindBetweenVectors(R0.RotateVector(K0 - P0), K1 - P1) * R0;
		return FTransform(R, P1 - R.RotateVector(P0));
	}
	FTransform About(const FVector& H, const FQuat& Q) { return FTransform(Q, H - Q.RotateVector(H)); }   // (a turn about H)
	FQuat QDeg(const FVector& Ax, float Deg) { return FQuat(Ax.GetSafeNormal(), FMath::DegreesToRadians(Deg)); }
}

void ASimTraffic::Haze(FMove& M, float Dt)
{
	// (to-do 379, owner 2026-11-28) the hot exhaust shimmering behind each running engine. The A320's nacelles measured on
	// Art/A320/A320.glb (the FlightGear CFM56 model): centres 5.62 m either side, 1.79 m up, the core nozzle's end 11.1 m behind the nose wheel (the
	// actor's root; its tail is the actor's +Y). Ring 0 just behind the nozzle at any power; rings 1 and 2 further back,
	// bigger, only with power (take-off) - the plume grows longer and stronger as the engines spool up, and fades again
	ABabylonModel* B = M.Model.Get(); if (!B || Dt <= 0.f) return;
	static UMaterialInterface* Mat = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Effects/M_HeatHaze.M_HeatHaze")));
	if (!Mat) return;
	if (!M.HazeB.Num())
	{
		static const float RingY[3] = {1260.f, 1760.f, 2610.f}, RingW[3] = {260.f, 420.f, 640.f}, RingH[3] = {200.f, 260.f, 330.f}, RingZ[3] = {179.f, 194.f, 224.f};
		for (int32 r = 0; r < 3; ++r)
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Mat, B, FName(*FString::Printf(TEXT("HeatHaze_Engine%d"), r)));   // (the name keeps "HeatHaze": Effects.cpp finds it)
			M.HazeM.Add(Mid);
			for (const float Side : {-562.f, 562.f})
			{
				UMaterialBillboardComponent* Hz = NewObject<UMaterialBillboardComponent>(B);
				Hz->SetupAttachment(B->GetRootComponent());
				Hz->SetRelativeLocation(FVector(Side, RingY[r], RingZ[r]));
				Hz->AddElement(Mid, nullptr, false, RingW[r], RingH[r], nullptr);
				Hz->SetCastShadow(false); Hz->SetVisibility(false);
				Hz->SetCullDistance(ADPEffects::HazeReachCm());
				Hz->RegisterComponent();
				M.HazeB.Add(Hz);
			}
		}
	}
	// the engines: started during the pushback (once it moves) or already running on arrival; shut down 8 s on the stand
	if (!M.bDeparture || M.Phase != EPhase::Push || M.Speed > 0.05f) M.bEnginesOn = true;
	if (M.Phase == EPhase::Done && M.Clock >= 8.f) M.bEnginesOn = false;
	float Want = 0.f, PowerWant = 0.f;
	if (M.bEnginesOn)
	{
		const bool bTOGA = (M.Phase == EPhase::Roll && M.bTakeoffOk && M.Step >= M.Path.Num() - 1 && M.Clock >= 4.f) || (M.Phase == EPhase::Roll && M.Step >= M.Path.Num()) || M.Phase == EPhase::Climb;
		Want = bTOGA ? 1.f : M.Phase == EPhase::Approach ? 0.35f : (M.Phase == EPhase::Rollout && M.RevPos >= 1.f) ? 0.45f : 0.15f;   // (idle 0.15; the approach's power; reverse power once the reverser doors are open - Spoilers())
		PowerWant = Want;   // (the sound's power: the climb keeps its thrust when the haze goes)
		if (M.Phase == EPhase::Climb && M.Height > 250.f) Want = 0.f;   // (high up: nothing to see against)
	}
	M.Power = FMath::FInterpConstantTo(M.Power, PowerWant, Dt, PowerWant > M.Power ? 0.18f : 0.25f);
	// spooling: up from idle to take-off power in about 5 s, back down a little quicker
	M.Thrust = FMath::FInterpConstantTo(M.Thrust, Want, Dt, Want > M.Thrust ? 0.18f : 0.25f);
	if (FMath::Abs(M.Thrust - M.HazeShown) < 0.02f && !(M.Thrust == 0.f && M.HazeShown != 0.f)) return;
	M.HazeShown = M.Thrust;
	const bool bOn = ADPEffects::On(ADPEffects::HeatHaze);
	const float T = M.Thrust;
	const float Str[3] = {T > 0.f ? 0.45f + 0.75f * T : 0.f, FMath::Clamp((T - 0.3f) * 1.6f, 0.f, 1.1f), FMath::Clamp((T - 0.55f) * 2.f, 0.f, 0.9f)};
	for (int32 r = 0; r < 3 && r < M.HazeM.Num(); ++r)
	{
		if (UMaterialInstanceDynamic* Mid = M.HazeM[r].Get()) { Mid->SetScalarParameterValue(TEXT("Strength"), Str[r]); Mid->SetScalarParameterValue(TEXT("Speed"), 0.8f + 2.2f * T); }
		for (int32 k = 0; k < 2; ++k) if (UMaterialBillboardComponent* Hz = M.HazeB.IsValidIndex(r * 2 + k) ? M.HazeB[r * 2 + k].Get() : nullptr) Hz->SetVisibility(bOn && Str[r] > 0.01f);
	}
}

namespace
{
	// the control surfaces' hinges (Art/A320/a320_surfaces.py "SURF" lines: FlightGear's own hinge lines of the A320-family
	// model, carried into the game frame; cm from the actor's root, tail +Y, right +X). Each axis is signed so FQuat(axis,
	// +deg) is the positive deflection: trailing edge down (flaps, ailerons, elevators, fairings), leading edge down (slats),
	// the rudder's trailing edge to the left. Trans: the flaps' run aft and down their tracks at full flap (cm)
	enum ESurfKind : uint8 { SkFlap1, SkFlap2, SkSlat, SkAilL, SkAilR, SkElev, SkRud, SkFair1, SkFair2 };
	struct FSurfHinge { const TCHAR* Name; ESurfKind Kind; FVector At, Axis, Trans; };
	const FSurfHinge SurfTab[] = {
		{TEXT("FlapL1"), SkFlap1, FVector(-197.0f, 1323.4f, 261.7f), FVector(-0.98844f, -0.00060f, 0.15162f), FVector(0.0f, 26.9f, -13.3f)},
		{TEXT("FlapL2"), SkFlap2, FVector(-644.2f, 1323.5f, 330.1f), FVector(-0.94021f, 0.32493f, 0.10211f), FVector(4.8f, 23.8f, -11.8f)},
		{TEXT("FlapR1"), SkFlap1, FVector(196.9f, 1323.4f, 261.8f), FVector(-0.98840f, 0.00069f, -0.15187f), FVector(0.0f, 26.9f, -13.3f)},
		{TEXT("FlapR2"), SkFlap2, FVector(644.1f, 1323.5f, 330.3f), FVector(-0.94021f, -0.32484f, -0.10236f), FVector(-4.8f, 23.8f, -11.8f)},
		{TEXT("SlatL1"), SkSlat, FVector(-519.2f, 984.4f, 279.6f), FVector(0.89574f, -0.43990f, -0.06437f), FVector::ZeroVector},
		{TEXT("SlatL2"), SkSlat, FVector(-1634.9f, 1540.6f, 408.9f), FVector(0.88591f, -0.45450f, -0.09275f), FVector::ZeroVector},
		{TEXT("SlatR1"), SkSlat, FVector(518.9f, 984.3f, 279.7f), FVector(0.89576f, 0.43981f, 0.06460f), FVector::ZeroVector},
		{TEXT("SlatR2"), SkSlat, FVector(1634.6f, 1540.4f, 409.3f), FVector(0.88592f, 0.45442f, 0.09298f), FVector::ZeroVector},
		{TEXT("AileronL"), SkAilL, FVector(-1328.4f, 1508.5f, 409.1f), FVector(-0.93649f, 0.33771f, 0.09457f), FVector::ZeroVector},
		{TEXT("AileronR"), SkAilR, FVector(1328.3f, 1508.4f, 409.4f), FVector(-0.93650f, -0.33762f, -0.09481f), FVector::ZeroVector},
		{TEXT("ElevatorL"), SkElev, FVector(-90.9f, 2920.6f, 481.9f), FVector(-0.93317f, 0.32714f, 0.14887f), FVector::ZeroVector},
		{TEXT("ElevatorR"), SkElev, FVector(610.9f, 3102.8f, 565.0f), FVector(-0.93317f, -0.32705f, -0.14912f), FVector::ZeroVector},
		{TEXT("Rudder"), SkRud, FVector(0.1f, 2926.7f, 804.2f), FVector(-0.00110f, 0.39204f, 0.91995f), FVector::ZeroVector},
		{TEXT("FairingL1"), SkFair1, FVector(-504.1f, 1228.9f, 288.9f), FVector(-0.99062f, -0.00056f, 0.13663f), FVector::ZeroVector},
		{TEXT("FairingL2"), SkFair2, FVector(-846.4f, 1288.3f, 329.7f), FVector(-0.99392f, -0.00044f, 0.11012f), FVector::ZeroVector},
		{TEXT("FairingL3"), SkFair2, FVector(-1206.6f, 1424.5f, 374.8f), FVector(-0.99391f, -0.00044f, 0.11017f), FVector::ZeroVector},
		{TEXT("FairingR1"), SkFair1, FVector(504.0f, 1228.8f, 289.0f), FVector(-0.99059f, 0.00065f, -0.13688f), FVector::ZeroVector},
		{TEXT("FairingR2"), SkFair2, FVector(846.3f, 1288.3f, 330.0f), FVector(-0.99389f, 0.00054f, -0.11038f), FVector::ZeroVector},
		{TEXT("FairingR3"), SkFair2, FVector(1206.5f, 1424.4f, 375.1f), FVector(-0.99388f, 0.00054f, -0.11043f), FVector::ZeroVector},
	};
	// the A320's flap lever positions (FCOM): slats / flaps degrees for 0, 1, 1+F, 2, 3, FULL
	const TCHAR* ConfName[6] = {TEXT("0"), TEXT("1"), TEXT("1+F"), TEXT("2"), TEXT("3"), TEXT("FULL")};
	const float ConfSlat[6] = {0.f, 18.f, 18.f, 22.f, 22.f, 27.f}, ConfFlap[6] = {0.f, 0.f, 10.f, 15.f, 20.f, 40.f};
	constexpr float FlapRate = 40.f / 20.f, SlatRate = 27.f / 20.f;   // (about 20 s each for the whole travel, as the real one)
	constexpr float ElevRate = 15.f, AilRate = 20.f, RudRate = 8.f;   // (degrees a second: smooth hands, well inside the jacks' speed)
	// FlightGear's tables against the flap's travel (0..1 = 0..40 deg): the tracks' run (fraction of the full run) and the fairings' droop
	float SurfLerp(const float* X, const float* Y, int32 Cnt, float T)
	{
		if (T <= X[0]) return Y[0];
		for (int32 i = 1; i < Cnt; ++i) if (T <= X[i]) return Y[i - 1] + (Y[i] - Y[i - 1]) * (T - X[i - 1]) / (X[i] - X[i - 1]);
		return Y[Cnt - 1];
	}
	float FlapRun(ESurfKind K, float Travel)
	{
		static const float X[3] = {0.f, 0.5f, 1.f}, Y1[3] = {0.f, 0.20f / 0.30f, 1.f}, Y2[3] = {0.f, 0.16f / 0.27f, 1.f};
		return SurfLerp(X, K == SkFlap1 ? Y1 : Y2, 3, Travel);
	}
	float FairDeg(ESurfKind K, float Travel)
	{
		static const float X1[5] = {0.f, 0.25f, 0.375f, 0.5f, 1.f}, Y1[5] = {0.f, 7.f, 9.5f, 13.5f, 22.5f};
		static const float X2[5] = {0.f, 0.2f, 0.5f, 0.75f, 1.f}, Y2[5] = {0.f, 5.75f, 11.75f, 15.25f, 19.f};
		return K == SkFair1 ? SurfLerp(X1, Y1, 5, Travel) : SurfLerp(X2, Y2, 5, Travel);
	}
}

void ASimTraffic::Surfaces(FMove& M, float Dt)
{
	ABabylonModel* B = M.Model.Get(); if (!B || Dt <= 0.f) return;
	const bool bAir = M.Height > 0.05f;
	// the lever, as a real A320 crew sets it: 1+F after engine start for the taxi out; 1 at about 200 m in the climb, 0 at about
	// 900 m; on the approach 1 at about 1,000 m, 2, 3 and FULL set by 300 m; FULL until clear of the runway, then 0
	int8 Want = 0;
	if (M.bDeparture)
	{
		if (M.Phase == EPhase::Push) Want = M.bEnginesOn ? 2 : 0;
		else if (M.Phase == EPhase::Taxi || M.Phase == EPhase::Roll) Want = 2;
		else if (M.Phase == EPhase::Climb) Want = M.Height >= 900.f ? 0 : M.Height >= 200.f ? 1 : 2;
		if (M.Phase == EPhase::Climb && M.Conf >= 0) Want = FMath::Min<int8>(Want, M.Conf);   // (never out again once going up)
	}
	else if (M.Phase == EPhase::Approach)
	{
		Want = M.Height > 1000.f ? 0 : M.Height > 700.f ? 1 : M.Height > 480.f ? 3 : M.Height > 340.f ? 4 : 5;
		if (M.Conf >= 0) Want = FMath::Max<int8>(Want, M.Conf);   // (never back in on the way down)
	}
	else if (M.Phase == EPhase::Rollout) Want = 5;
	// the crosswind on the runway in use (kt, + from the right)
	double RwHdg = 336.0; if (TSharedPtr<FJsonObject> Ab = ADPSite::AirportBlock()) Ab->TryGetNumberField(TEXT("hdg"), RwHdg);
	if (!RunwayToX1()) RwHdg += 180.0;
	const float Xw = AWindSocks::WindKt * FMath::Sin(FMath::DegreesToRadians(AWindSocks::WindFrom - float(RwHdg)));
	// the track (plan: its angle turns the other way round from the world's heading) and its turn rate
	const FVector2f Dv = M.Nose - M.Main;
	const float Trk = FMath::RadiansToDegrees(FMath::Atan2(Dv.Y, Dv.X));
	if (!M.bSurf)
	{
		M.bSurf = true; M.Track = Trk;
		M.GSurf.Reset();
		int32 Found = 0;
		for (const FSurfHinge& H : SurfTab) { const int32 G = B->BindGroup({H.Name}, false, FTransform(H.At)); M.GSurf.Add(G); Found += G != INDEX_NONE; }
		// (met in the air or on the stand: the surfaces start where the crew has them by now)
		M.Conf = Want; M.SlatDeg = ConfSlat[Want]; M.FlapDeg = ConfFlap[Want];
		ADP_LOG(TEXT("Traffic: %s control surfaces - %d of %d parts bound; flaps %s (slats %.0f, flaps %.0f) at %.0f m"), *M.Reg, Found, int32(UE_ARRAY_COUNT(SurfTab)), ConfName[Want], M.SlatDeg, M.FlapDeg, M.Height);
		for (int32 i = 0; i < M.GSurf.Num(); ++i) if (M.GSurf[i] == INDEX_NONE) ADP_WARN(TEXT("Traffic: %s has no part %s - it stays still"), *M.Reg, SurfTab[i].Name);
	}
	if (Want != M.Conf)
	{
		ADP_LOG(TEXT("Traffic: %s flaps %s at %.0f m (slats %.0f, flaps %.0f; %s)"), *M.Reg, ConfName[Want], M.Height, ConfSlat[Want], ConfFlap[Want],
			M.Phase == EPhase::Push ? TEXT("engines running") : M.Phase == EPhase::Climb ? TEXT("climbing") : M.Phase == EPhase::Approach ? TEXT("on the approach") : M.Phase == EPhase::TaxiIn ? TEXT("clear of the runway") : TEXT("taxiing"));
		M.Conf = Want;
	}
	M.SlatDeg = FMath::FInterpConstantTo(M.SlatDeg, ConfSlat[M.Conf], Dt, SlatRate);
	M.FlapDeg = FMath::FInterpConstantTo(M.FlapDeg, ConfFlap[M.Conf], Dt, FlapRate);
	M.SurfT += Dt;
	// the elevators (- = trailing edge up): back stick at rotation, less once the pitch is set for the climb, a little in the
	// approach, the flare, and held back while the nose comes down after touchdown
	uint8 ESt = 0; float EWant = 0.f;
	if (M.Phase == EPhase::Roll && M.RotT >= 0.f && !bAir) { ESt = 1; EWant = -13.f; }
	else if (M.Phase == EPhase::Roll && M.RotT >= 0.f) { ESt = 2; EWant = -6.f; }
	else if (M.Phase == EPhase::Climb) { const bool bLift = M.Pitch < 14.5f && M.Height < 100.f; ESt = bLift ? 2 : 3; EWant = bLift ? -6.f : -2.f; }   // (pitching up only just after lift-off; higher up the climb holds its pitch)
	else if (M.Phase == EPhase::Approach) { ESt = M.Height < FlareM ? 5 : 4; EWant = M.Height < FlareM ? -8.f : -1.f; }
	else if (M.Phase == EPhase::Rollout && M.Pitch > 0.3f) { ESt = 6; EWant = -4.f; }
	// (a banked turn needs a little back stick to hold the height: about 3 deg more up at 24 deg of bank, load factor 1.09)
	if (bAir && ESt != 1) EWant -= FMath::Clamp((1.f / FMath::Cos(FMath::DegreesToRadians(M.Bank)) - 1.f) * 30.f, 0.f, 3.f);
	if (ESt != M.ElevSt)
	{
		static const TCHAR* EWhy[7] = {TEXT("neutral"), TEXT("rotation"), TEXT("lift-off, pitching up"), TEXT("climb pitch held"), TEXT("approach"), TEXT("the flare"), TEXT("nose lowered onto its wheel")};
		if (M.ElevSt != 255) ADP_LOG(TEXT("Traffic: %s elevators %.0f deg %s at %.0f m (%s, %.0f kt)"), *M.Reg, FMath::Abs(EWant), EWant < 0.f ? TEXT("up") : TEXT("centred"), M.Height, EWhy[ESt], M.Speed / KtMs);
		M.ElevSt = ESt;
	}
	M.Elev = FMath::FInterpConstantTo(M.Elev, EWant, Dt, ElevRate);
	// the bank: a turn in the air flown coordinated (tan bank = speed x turn rate / g); the ailerons roll it in and out, droop
	// 5 deg with the flaps out (the A320's aileron droop) and lean into a crosswind in the flare and on the rollout
	const float dTrk = FMath::FindDeltaAngleDegrees(M.Track, Trk); M.Track = Trk;
	M.TurnRate = FMath::FInterpTo(M.TurnRate, -dTrk / Dt, Dt, 2.f);   // (deg/s, + to the right)
	// (11-28 owner: "it feels like running on a wire" - the bank came from the turn already flown, so it yawed flat first,
	//  rolled in 5 s late and was still banked after the turn; a pilot rolls in first and the turn follows: on a planned arc
	//  the bank leads by the roll time - in about 2.5 s before the arc, out about 2.5 s before its end)
	float RateWant = M.TurnRate;
	if (bAir && M.ArcStep >= 0 && M.ArcR > 1.f)
	{
		const float Lead = M.Speed * 2.5f;
		const FVector2f Hd = (M.Nose - M.Main).GetSafeNormal(), ToC = M.ArcC - M.Nose;
		const float ArcRate = -FMath::Sign(Hd.X * ToC.Y - Hd.Y * ToC.X) * FMath::RadiansToDegrees(M.Speed / M.ArcR);
		if (M.Step == M.ArcStep) RateWant = (M.ArcEnd - M.ArcA) * M.ArcR > Lead ? ArcRate : 0.f;
		else if (M.Step + 1 == M.ArcStep && M.Path.IsValidIndex(M.Step)) RateWant = FVector2f::Distance(M.Nose, M.Path[M.Step]) < Lead ? ArcRate : 0.f;
	}
	const float BankWant = bAir ? FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(M.Speed * FMath::DegreesToRadians(RateWant) / 9.81f)), -25.f, 25.f) : 0.f;
	const float BankWas = M.Bank;
	// (eased: the roll starts and ends softly, at most 6 deg/s - about the airliner's autopilot roll)
	// (owner 11-28, standing for every type: the wings never snap level - a softly sprung roll, a little under-damped, so on
	//  levelling out they tip ~10 % past level to the other side first and then settle; roll rate a gentle 6 deg/s at most)
	{
		constexpr float Wn = 1.0f, Zeta = 0.6f;
		M.BankV += (Wn * Wn * (BankWant - M.Bank) - 2.f * Zeta * Wn * M.BankV) * Dt;
		M.BankV = FMath::Clamp(M.BankV, -6.f, 6.f);
		M.Bank += M.BankV * Dt;
	}
	if (FMath::Abs(M.Bank) > FMath::Abs(M.BankPeak)) M.BankPeak = M.Bank;
	if (FMath::Abs(M.BankPeak) > 3.f && FMath::Abs(M.Bank) < 1.f)
	{
		ADP_LOG(TEXT("Traffic: %s turn flown - bank peak %.0f deg %s at %.0f m, %.0f kt"), *M.Reg, FMath::Abs(M.BankPeak), M.BankPeak > 0.f ? TEXT("right") : TEXT("left"), M.Height, M.Speed / KtMs);
		M.BankPeak = 0.f;
	}
	else if (FMath::Abs(M.BankPeak) <= 3.f && FMath::Abs(M.Bank) < 1.f) M.BankPeak = 0.f;
	// (one line a second while banked or on the arc: heading, yaw rate, bank - the next run shows the turn is smooth)
	if (bAir && (FMath::Abs(M.Bank) > 1.f || (M.ArcStep >= 0 && M.Step == M.ArcStep)) && (M.TurnLogT += Dt) >= 1.f)
	{
		M.TurnLogT = 0.f;
		ADP_LOG(TEXT("Traffic: %s turning - track %.1f deg (plan), yaw rate %.2f deg/s (this frame %.2f), bank %.1f deg (want %.1f), %.0f m, dt %.1f ms"),
			*M.Reg, Trk, M.TurnRate, -dTrk / Dt, M.Bank, BankWant, M.Height, Dt * 1000.f);
	}
	else if (!bAir || (FMath::Abs(M.Bank) <= 1.f && M.Step != M.ArcStep)) M.TurnLogT = 1.f;
	float RollCmd = FMath::Clamp((M.Bank - BankWas) / Dt * 2.f, -15.f, 15.f);   // (+ rolls right: left aileron down, right up)
	uint8 ASt = FMath::Abs(BankWant) > 3.f ? 3 : 0;
	if (M.Phase == EPhase::Approach && M.Height < 15.f) { RollCmd += FMath::Clamp(Xw * 0.3f, -6.f, 6.f); if (FMath::Abs(Xw) > 2.f) ASt = 4; }
	else if (M.Phase == EPhase::Rollout) { RollCmd += FMath::Clamp(Xw * 0.3f, -6.f, 6.f) * FMath::Clamp(M.Speed / (100.f * KtMs), 0.f, 1.f); if (FMath::Abs(Xw) > 2.f) ASt = 5; }
	const float Droop = FMath::Clamp(M.FlapDeg / 10.f, 0.f, 1.f) * 5.f;
	if (ASt == 0 && Droop > 4.9f) ASt = 1;
	if (ASt != M.AilSt)
	{
		static const TCHAR* AWhy[6] = {TEXT("centred"), TEXT("drooped 5 deg with the flaps"), TEXT("-"), TEXT("banking"), TEXT("into the wind in the flare"), TEXT("into the wind on the rollout")};
		if (M.AilSt != 255) ADP_LOG(TEXT("Traffic: %s ailerons %s at %.0f m (bank %.0f deg, crosswind %.0f kt from the %s)"), *M.Reg, AWhy[ASt], M.Height, BankWant, FMath::Abs(Xw), Xw >= 0.f ? TEXT("right") : TEXT("left"));
		M.AilSt = ASt;
	}
	M.AilL = FMath::FInterpConstantTo(M.AilL, FMath::Clamp(Droop + RollCmd, -25.f, 20.f), Dt, AilRate);
	M.AilR = FMath::FInterpConstantTo(M.AilR, FMath::Clamp(Droop - RollCmd, -25.f, 20.f), Dt, AilRate);
	// the rudder (+ = trailing edge left, the nose yaws left): small corrections against the crosswind's gusts on final (crabbed,
	// so centred on average), then the decrab in the flare - the nose yawed from into the wind onto the centre line; centred otherwise
	uint8 RSt = 0; float RWant = 0.f;
	if (M.Phase == EPhase::Approach && M.Height < 15.f) { RSt = 2; RWant = FMath::Clamp(Xw * 0.5f, -10.f, 10.f); }
	else if (M.Phase == EPhase::Approach && M.Height < 300.f && FMath::Abs(Xw) > 2.f)
	{
		RSt = 1; RWant = FMath::Clamp(FMath::Abs(Xw) * 0.15f, 0.5f, 2.5f) * FMath::Sin(M.SurfT * 0.9f) * (0.6f + 0.4f * FMath::Sin(M.SurfT * 0.37f + 1.f));
	}
	if (RSt != M.RudSt)
	{
		static const TCHAR* RWhy[3] = {TEXT("centred"), TEXT("small crosswind corrections on final"), TEXT("decrab in the flare")};
		if (M.RudSt != 255) ADP_LOG(TEXT("Traffic: %s rudder %s at %.0f m (%.0f deg, crosswind %.0f kt from the %s)"), *M.Reg, RWhy[RSt], M.Height, FMath::Abs(RWant), FMath::Abs(Xw), Xw >= 0.f ? TEXT("right") : TEXT("left"));
		M.RudSt = RSt;
	}
	M.Rud = FMath::FInterpConstantTo(M.Rud, RWant, Dt, RudRate);
	// draw only what moved (the gear's group moves: each part about its hinge)
	const float Cur[6] = {M.FlapDeg, M.SlatDeg, M.Elev, M.AilL, M.AilR, M.Rud};
	bool bMoved = false; for (int32 k = 0; k < 6; ++k) if (FMath::Abs(Cur[k] - M.SurfShown[k]) > 0.01f) { bMoved = true; M.SurfShown[k] = Cur[k]; }
	if (!bMoved) return;
	const float Travel = M.FlapDeg / 40.f;
	for (int32 i = 0; i < M.GSurf.Num() && i < int32(UE_ARRAY_COUNT(SurfTab)); ++i)
	{
		if (M.GSurf[i] == INDEX_NONE) continue;
		const FSurfHinge& H = SurfTab[i];
		float Deg = 0.f; FVector Run = FVector::ZeroVector;
		switch (H.Kind)
		{
		case SkFlap1: case SkFlap2: Deg = M.FlapDeg; Run = H.Trans * FlapRun(H.Kind, Travel); break;
		case SkSlat: Deg = M.SlatDeg; break;
		case SkAilL: Deg = M.AilL; break;
		case SkAilR: Deg = M.AilR; break;
		case SkElev: Deg = M.Elev; break;
		case SkRud: Deg = M.Rud; break;
		case SkFair1: case SkFair2: Deg = FairDeg(H.Kind, Travel); break;
		}
		// (FlightGear's order: the turn about the hinge first, then the run along the track)
		B->MoveGroup(M.GSurf[i], FTransform(FQuat(H.Axis.GetSafeNormal(), FMath::DegreesToRadians(Deg)), H.At + Run));
	}
	B->FlushMoves();
}

namespace
{
	// the spoilers (Art/A320/a320_surfaces.py "SPOIL" lines: FlightGear's fuselage.xml hinges carried into the game frame, cm
	// from the root, tail +Y, right +X). FQuat(Axis, +deg) lifts the panel's trailing edge; the actuator turns Fac x deg about
	// its own pivot, and its rod turns with it and slides out along Dir by FlightGear's table (against the panel's deg)
	struct FSpoilHinge { const TCHAR* Name; FVector At, Axis; const TCHAR* Act; FVector ActAt, ActAxis; float Fac; FVector Dir; };
	const FSpoilHinge SpTab[] = {
		{TEXT("SpoilerL1"), FVector(-456.2f, 1243.6f, 323.2f), FVector(0.98857f, 0.00298f, -0.15071f), TEXT("SpoilerActuatorL1"), FVector(-541.7f, 1240.6f, 325.8f), FVector(0.98810f, 0.03729f, -0.14921f), 0.675f, FVector(0.02729f, 0.91229f, 0.40864f)},
		{TEXT("SpoilerL2"), FVector(-696.6f, 1265.1f, 353.0f), FVector(0.93747f, -0.33629f, -0.08982f), TEXT("SpoilerActuatorL2"), FVector(-780.8f, 1295.2f, 355.4f), FVector(0.93709f, -0.33453f, -0.09977f), 0.800f, FVector(0.34872f, 0.87814f, 0.32752f)},
		{TEXT("SpoilerL3"), FVector(-864.4f, 1325.3f, 369.1f), FVector(0.93737f, -0.33658f, -0.08978f), TEXT("SpoilerActuatorL3"), FVector(-943.0f, 1353.4f, 371.0f), FVector(0.93709f, -0.33452f, -0.09974f), 0.800f, FVector(0.34872f, 0.87814f, 0.32752f)},
		{TEXT("SpoilerL4"), FVector(-1020.6f, 1381.4f, 384.0f), FVector(0.93751f, -0.33617f, -0.08979f), TEXT("SpoilerActuatorL4"), FVector(-1096.1f, 1408.3f, 385.8f), FVector(0.93708f, -0.33455f, -0.09975f), 0.800f, FVector(0.34872f, 0.87814f, 0.32752f)},
		{TEXT("SpoilerL5"), FVector(-1169.8f, 1435.0f, 398.3f), FVector(0.93741f, -0.33642f, -0.08986f), TEXT("SpoilerActuatorL5"), FVector(-1242.6f, 1460.6f, 399.8f), FVector(0.93707f, -0.33458f, -0.09975f), 0.800f, FVector(0.34872f, 0.87814f, 0.32752f)},
		{TEXT("SpoilerR1"), FVector(610.1f, 1243.0f, 346.9f), FVector(0.98840f, -0.00330f, 0.15182f), TEXT("SpoilerActuatorR1"), FVector(541.6f, 1240.5f, 325.9f), FVector(0.98806f, -0.03738f, 0.14946f), 0.675f, FVector(-0.02730f, 0.91229f, 0.40863f)},
		{TEXT("SpoilerR2"), FVector(851.3f, 1320.2f, 367.9f), FVector(0.93827f, 0.33400f, 0.08994f), TEXT("SpoilerActuatorR2"), FVector(780.7f, 1295.1f, 355.6f), FVector(0.93710f, 0.33443f, 0.10001f), 0.800f, FVector(-0.34871f, 0.87817f, 0.32743f)},
		{TEXT("SpoilerR3"), FVector(1007.9f, 1376.4f, 383.0f), FVector(0.93742f, 0.33634f, 0.09011f), TEXT("SpoilerActuatorR3"), FVector(942.9f, 1353.3f, 371.3f), FVector(0.93710f, 0.33443f, 0.09998f), 0.800f, FVector(-0.34871f, 0.87817f, 0.32743f)},
		{TEXT("SpoilerR4"), FVector(1158.0f, 1430.3f, 397.4f), FVector(0.93742f, 0.33633f, 0.09010f), TEXT("SpoilerActuatorR4"), FVector(1096.0f, 1408.2f, 386.1f), FVector(0.93709f, 0.33447f, 0.10000f), 0.800f, FVector(-0.34871f, 0.87817f, 0.32743f)},
		{TEXT("SpoilerR5"), FVector(1301.7f, 1481.8f, 411.2f), FVector(0.93742f, 0.33633f, 0.09010f), TEXT("SpoilerActuatorR5"), FVector(1242.5f, 1460.5f, 400.1f), FVector(0.93708f, 0.33449f, 0.09999f), 0.800f, FVector(-0.34871f, 0.87817f, 0.32743f)},
	};
	// the CFM56-5B pivot doors (Fuselage/a320.cfm.xml; the engine model in the fuselage's own frame - only so do the hinges sit
	// on the doors): FQuat(Axis, +deg) swings a door's front edge out of the nacelle and its back edge into the fan stream;
	// FlightGear opens them 70 deg at reverser-pos-norm 1
	struct FRevHinge { const TCHAR* Name; FVector At, Axis; };
	const FRevHinge RevTab[] = {
		{TEXT("ReverserLDoor1"), FVector(-625.2f, 887.1f, 241.4f), FVector(-0.59068f, -0.00857f, -0.80686f)},
		{TEXT("ReverserLDoor2"), FVector(-629.3f, 887.2f, 124.7f), FVector(0.57714f, 0.01083f, -0.81657f)},
		{TEXT("ReverserLDoor3"), FVector(-501.1f, 887.1f, 241.4f), FVector(-0.59081f, 0.00862f, 0.80677f)},
		{TEXT("ReverserLDoor4"), FVector(-497.0f, 887.2f, 124.7f), FVector(0.57665f, -0.01090f, 0.81692f)},
		{TEXT("ReverserRDoor1"), FVector(501.0f, 887.0f, 241.6f), FVector(-0.59067f, -0.00857f, -0.80686f)},
		{TEXT("ReverserRDoor2"), FVector(496.9f, 887.1f, 124.8f), FVector(0.57705f, 0.01085f, -0.81664f)},
		{TEXT("ReverserRDoor3"), FVector(625.1f, 887.0f, 241.6f), FVector(-0.59080f, 0.00863f, 0.80677f)},
		{TEXT("ReverserRDoor4"), FVector(629.2f, 887.1f, 124.9f), FVector(0.57666f, -0.01089f, 0.81691f)},
	};
	// (FlightGear a320-spoilers.xml: 50 deg full, 35 deg a second with hydraulics; the reverser doors' travel about 2 s)
	constexpr float SpoilFull = 50.f, SpoilRate = 35.f, RollSpoilMax = 12.f, RevOpenDeg = 70.f, RevTravel = 2.f;
	float RodRun(float Deg)
	{
		static const float X[6] = {0.f, 10.f, 20.f, 30.f, 40.f, 50.f}, Y[6] = {-2.0f, -0.8f, 0.4f, 1.6f, 2.4f, 3.3f};   // (cm)
		return SurfLerp(X, Y, 6, Deg);
	}
}

void ASimTraffic::Spoilers(FMove& M, float Dt)
{
	ABabylonModel* B = M.Model.Get(); if (!B || Dt <= 0.f) return;
	const bool bGround = M.Height < 0.05f;
	const float Kt = M.Speed / KtMs;
	if (!M.bSpoil)
	{
		M.bSpoil = true; M.GSpoil.Reset();
		int32 Found = 0, Want = 0;
		for (const FSpoilHinge& H : SpTab)
		{
			const int32 G0 = B->BindGroup({H.Name}, false, FTransform(H.At)), G1 = B->BindGroup({H.Act}, false, FTransform(H.ActAt));
			const int32 G2 = B->BindGroup({FString(H.Act).Replace(TEXT("Actuator"), TEXT("ActuatorRod"))}, false, FTransform(H.ActAt));
			M.GSpoil.Append({G0, G1, G2}); Found += (G0 != INDEX_NONE) + (G1 != INDEX_NONE) + (G2 != INDEX_NONE); Want += 3;
			if (G0 == INDEX_NONE) ADP_WARN(TEXT("Traffic: %s has no part %s - it stays down"), *M.Reg, H.Name);
		}
		for (const FRevHinge& H : RevTab)
		{
			const int32 G = B->BindGroup({H.Name}, false, FTransform(H.At)); M.GSpoil.Add(G); Found += G != INDEX_NONE; ++Want;
			if (G == INDEX_NONE) ADP_WARN(TEXT("Traffic: %s has no part %s - it stays stowed"), *M.Reg, H.Name);
		}
		ADP_LOG(TEXT("Traffic: %s spoilers and reversers - %d of %d parts bound"), *M.Reg, Found, Want);
	}
	// the ground spoilers (armed on the approach): all ten panels to 50 deg once the main gear is down on the runway, in again
	// below about 20 kt (the roll-out's end) or once clear of it. The traffic has no rejected take-off yet (nothing to arm).
	// In the air the roll spoilers 2-5 rise on the down-going wing, with the bank (+ right wing down)
	uint8 St = 0;
	if (M.Phase == EPhase::Rollout && bGround && Kt > 20.5f) St = 2;
	else if (!bGround && (FMath::Abs(M.Bank) > 3.f || (M.SpoilSt == 1 && FMath::Abs(M.Bank) > 2.f))) St = 1;   // (a little hysteresis: no flicker of the log)
	else if (M.Phase == EPhase::Approach) St = 3;
	if (St != M.SpoilSt)
	{
		static const TCHAR* SWhy[4] = {TEXT("spoilers retracted"), TEXT("roll spoilers 2-5 up on the down-going wing"), TEXT("ground spoilers out, all ten panels to 50 deg"), TEXT("ground spoilers armed")};
		if (M.SpoilSt != 255 || St != 0) ADP_LOG(TEXT("Traffic: %s %s at %.0f m, %.0f kt (bank %.0f deg, %s)"), *M.Reg, SWhy[St], M.Height, Kt, M.Bank,
			M.Phase == EPhase::Rollout ? TEXT("rollout") : M.Phase == EPhase::TaxiIn ? TEXT("clear of the runway") : M.Phase == EPhase::Approach ? TEXT("approach") : TEXT("in flight"));
		M.SpoilSt = St;
	}
	const float Roll = St == 1 ? FMath::Clamp(FMath::Abs(M.Bank) * 0.5f, 0.f, RollSpoilMax) : 0.f;
	bool bMoved = M.SpoilShown < 0.f;
	for (int32 k = 0; k < 5; ++k)
	{
		const float WL = St == 2 ? SpoilFull : (k > 0 && M.Bank < 0.f ? Roll : 0.f), WR = St == 2 ? SpoilFull : (k > 0 && M.Bank > 0.f ? Roll : 0.f);
		const float L = FMath::FInterpConstantTo(M.SpoilL[k], WL, Dt, SpoilRate), R = FMath::FInterpConstantTo(M.SpoilR[k], WR, Dt, SpoilRate);
		bMoved |= L != M.SpoilL[k] || R != M.SpoilR[k]; M.SpoilL[k] = L; M.SpoilR[k] = R;
	}
	// the reversers: selected right after the main gear touches, the doors out in about 2 s (Haze() gives reverse power only
	// once they are open, as the engines spool up), back to reverse idle and stowed at about 70 kt, fully in before the taxi
	const bool bSelect = M.Phase == EPhase::Rollout && bGround && Kt > 70.f;
	const uint8 RSt = bSelect ? (M.RevPos >= 1.f ? 2 : 1) : (M.RevPos > 0.f ? 3 : 0);
	if (RSt != M.RevSt)
	{
		static const TCHAR* RWhy[4] = {TEXT("stowed"), TEXT("deploying (pivot doors opening)"), TEXT("deployed, reverse thrust"), TEXT("stowing")};
		if (M.RevSt != 255 || RSt != 0) ADP_LOG(TEXT("Traffic: %s thrust reversers %s at %.0f kt (%s)"), *M.Reg, RWhy[RSt], Kt, M.Phase == EPhase::Rollout ? TEXT("rollout") : M.Phase == EPhase::TaxiIn ? TEXT("taxi-in") : TEXT("-"));
		M.RevSt = RSt;
	}
	const float RevNow = FMath::FInterpConstantTo(M.RevPos, bSelect ? 1.f : 0.f, Dt, 1.f / RevTravel);
	bMoved |= RevNow != M.RevPos; M.RevPos = RevNow;
	if (!bMoved) return;
	M.SpoilShown = 1.f;
	for (int32 i = 0; i < int32(UE_ARRAY_COUNT(SpTab)) && i * 3 + 2 < M.GSpoil.Num(); ++i)
	{
		const FSpoilHinge& H = SpTab[i];
		const float Deg = i < 5 ? M.SpoilL[i] : M.SpoilR[i - 5];
		if (M.GSpoil[i * 3] != INDEX_NONE) B->MoveGroup(M.GSpoil[i * 3], FTransform(FQuat(H.Axis.GetSafeNormal(), FMath::DegreesToRadians(Deg)), H.At));
		const FQuat QA(H.ActAxis.GetSafeNormal(), FMath::DegreesToRadians(Deg * H.Fac));
		if (M.GSpoil[i * 3 + 1] != INDEX_NONE) B->MoveGroup(M.GSpoil[i * 3 + 1], FTransform(QA, H.ActAt));
		// (the rod: turned with its cylinder, then out along the turned line - FlightGear's order)
		if (M.GSpoil[i * 3 + 2] != INDEX_NONE) B->MoveGroup(M.GSpoil[i * 3 + 2], FTransform(QA, H.ActAt + QA.RotateVector(H.Dir.GetSafeNormal() * RodRun(Deg))));
	}
	const int32 R0 = int32(UE_ARRAY_COUNT(SpTab)) * 3;
	for (int32 i = 0; i < int32(UE_ARRAY_COUNT(RevTab)) && R0 + i < M.GSpoil.Num(); ++i)
		if (M.GSpoil[R0 + i] != INDEX_NONE) B->MoveGroup(M.GSpoil[R0 + i], FTransform(FQuat(RevTab[i].Axis.GetSafeNormal(), FMath::DegreesToRadians(RevOpenDeg * M.RevPos)), RevTab[i].At));
	B->FlushMoves();
}

namespace
{
	// the gear's groups (FMove::GG): each bound at the model's root (no pivot), so a move is the whole part's transform
	enum EGg { GgNoseW, GgNoseLeg, GgNoseUp, GgNoseLower, GgMainW, GgMainLeg = GgMainW + 2, GgMainUp = GgMainLeg + 2, GgFair = GgMainUp + 2,
		GgBay = GgFair + 2, GgNDoor = GgBay + 2, GgAft = GgNDoor + 2, GgAct = GgAft + 2, GgRod = GgAct + 2, GgLink = GgRod + 2, GgNum = GgLink + 2 * LNum };
	// the linkages as built (gear down, struts in): the knee solved for the built ends
	struct FGearLinkRt { FVector A0, B0, K0, H; double L1, L2, Side; };
	const FGearLinkRt* GearLinks()
	{
		static FGearLinkRt R[LNum];
		static bool bDone = false;
		if (!bDone)
		{
			bDone = true;
			for (int32 i = 0; i < LNum; ++i)
			{
				const FGearLinkDef& D = LinkDef[i]; FGearLinkRt& L = R[i];
				L.H = D.H.GetSafeNormal(); L.L1 = (D.K - D.A).Size(); L.L2 = (D.K - D.B).Size();
				L.Side = FVector::DotProduct(D.K - D.A, FVector::CrossProduct(L.H, D.B - D.A)) >= 0.0 ? 1.0 : -1.0;
				L.A0 = D.A; L.B0 = D.B + (D.bTube ? FVector(0.f, 0.f, StrutCm) : FVector::ZeroVector);
				L.K0 = GearKnee(L.A0, L.B0, L.L1, L.L2, L.H, L.Side);
			}
		}
		return R;
	}
}

void ASimTraffic::Gear(FMove& M, float Dt)
{
	ABabylonModel* B = M.Model.Get(); if (!B || Dt <= 0.f) return;
	// the doors shut, the RAT stowed, the flap mechanisms hidden; the APU's inlet flap open while the APU runs: on the ground
	// with the engines off (at the gate after arrival, and on the push until the engines start)
	{
		ADPA320::FWant PW; PW.K[ADPA320::Apu] = M.Height < 0.05f && !M.bEnginesOn ? 1.f : 0.f;
		ADPA320::Set(B, PW); ADPA320::Step(B, Dt);
	}
	if (!M.bGear)
	{
		M.bGear = true;
		M.GG.Init(INDEX_NONE, GgNum);
		const FTransform I = FTransform::Identity;
		auto Bind = [B, &I](std::initializer_list<const TCHAR*> Names, bool bPrefix = false) -> int32
		{
			TArray<FString> N; for (const TCHAR* X : Names) N.Add(X);
			return B->BindGroup(N, bPrefix, I);
		};
		M.GG[GgNoseW] = Bind({TEXT("NoseWheels")}, true);
		M.GG[GgNoseLeg] = Bind({TEXT("NoseLeg")});            // (exactly, + its torque links: NoseLegUpper is the fitting)
		M.GG[GgNoseUp] = Bind({TEXT("NoseLegUpper")});
		M.GG[GgNoseLower] = Bind({TEXT("NoseLegUpper.Lower"), TEXT("NoseLegUpper.LowerPlacards")});   // (steers with the tube)
		const TCHAR* Sd[2] = {TEXT("_L"), TEXT("_R")};
		for (int32 s = 0; s < 2; ++s)
		{
			const FString X = Sd[s];
			M.GG[GgMainW + s] = B->BindGroup({TEXT("MainWheels") + X}, true, I);
			M.GG[GgMainLeg + s] = B->BindGroup({TEXT("MainLeg") + X}, false, I);
			M.GG[GgMainUp + s] = B->BindGroup({TEXT("MainLegUpper") + X, TEXT("MainDoor") + X}, false, I);   // (the leg door rides the leg)
			M.GG[GgFair + s] = B->BindGroup({TEXT("MainDoor") + X + TEXT(".Fairing"), TEXT("MainDoor") + X + TEXT(".FairingInner")}, false, I);
			M.GG[GgBay + s] = B->BindGroup({TEXT("MainBayDoor") + X}, false, I);
			M.GG[GgNDoor + s] = B->BindGroup({TEXT("NoseDoor") + X}, false, I);
			M.GG[GgAft + s] = B->BindGroup({TEXT("NoseLegUpper.AftDoor") + X, TEXT("NoseLegUpper.AftDoor") + X + TEXT("Inner")}, false, I);
			M.GG[GgAct + s] = B->BindGroup({FString(TEXT("GearLDoorActuator")) + (s == 0 ? TEXT("L") : TEXT("R"))}, false, I);
			M.GG[GgRod + s] = B->BindGroup({FString(TEXT("GearLDoorActuatorRod")) + (s == 0 ? TEXT("L") : TEXT("R"))}, false, I);
		}
		for (int32 i = 0; i < LNum; ++i)
		{
			M.GG[GgLink + 2 * i] = Bind({LinkDef[i].Bar1}); M.GG[GgLink + 2 * i + 1] = Bind({LinkDef[i].Bar2});
		}
		M.PrevNose = M.Nose; M.PrevMain = M.Main;
		M.ExtN = M.ExtM = M.Height > 0.5f ? 1.f : 0.f;
		const bool bUp = (M.Phase == EPhase::Approach && M.Height > GearDownM) || (M.Phase == EPhase::Climb && M.Height > 15.f);
		M.GearSt = bUp ? 2 : 0; M.PosN = M.PosM = bUp ? 0.f : 1.f;
		int32 Missing = 0; FString Which;
		for (int32 i = 0; i < GgNum; ++i) if (M.GG[i] == INDEX_NONE) { ++Missing; Which += FString::Printf(TEXT(" %d"), i); }
		if (Missing) ADP_WARN(TEXT("Traffic: %s gear - %d of %d moving parts not in the model:%s"), *M.Reg, Missing, int32(GgNum), *Which);
		else ADP_LOG(TEXT("Traffic: %s gear - all %d moving parts bound (FlightGear's pivots and linkages); gear %s"), *M.Reg, int32(GgNum), bUp ? TEXT("up") : TEXT("down"));
	}
	if (M.GG.Num() != GgNum || M.GG[GgNoseW] == INDEX_NONE) return;
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
	// (signed by the fuselage's line: backwards in the push; radii from the meshes: nose 0.376 m, mains 0.544 m)
	if (bGround)
	{
		const float SgN = FVector2f::DotProduct(DN, Body) < 0.f ? -1.f : 1.f, SgM = FVector2f::DotProduct(M.Main - M.PrevMain, Body) < 0.f ? -1.f : 1.f;
		M.RollN += SgN * FMath::RadiansToDegrees(DistN * 100.f / NoseR); M.RollM += SgM * FMath::RadiansToDegrees(DistM * 100.f / MainR);
	}
	// weight on wheels: each strut goes out only as far as its own wheels clear the ground (the main gear's height, the
	// nose's that plus the wheelbase times the pitch's sine), with a damped spring under that - so the main legs compress
	// first at touchdown, the nose strut extends first at rotation; never into the ground
	const float ClearM = M.Height, ClearN = M.Height + Wheelbase * FMath::Sin(FMath::DegreesToRadians(M.Pitch));
	// once a gear's wheels are down, the body rides on it as an underdamped spring (about 1 Hz, damping 0.22): kicked down at
	// the wheels' first touch, it sinks, rebounds a little above resting and settles in about 2 s; the strut's tube slides by
	// the same amount (E below 0 = further in than resting), so the wheels stay on the ground (owner 11-28: "the airplane
	// settles a little bit up again after landing")
	auto Strut = [Dt](float& E, float& V, float Clear, float& Bnc, float& BncV, bool& bOn, float Kick, const TCHAR* Which, const FString& Reg)
	{
		if (Clear > 0.005f)
		{
			bOn = false; Bnc = BncV = 0.f;
			const float Lim = FMath::Clamp(Clear * 100.f / StrutCm, 0.f, 1.f);
			V += ((1.f - E) * 90.f - V * 11.f) * Dt; E += V * Dt;
			if (E >= Lim) { E = Lim; V = FMath::Min(V, 0.f); }
			if (E <= 0.f) { E = 0.f; V = FMath::Max(V, 0.f); }
			return;
		}
		if (!bOn) { bOn = true; BncV = Kick; ADP_LOG(TEXT("Traffic: %s %s gear on the ground - the struts take the weight (body dips at %.1f m/s, rebounds, settles)"), *Reg, Which, -Kick); }
		const float W = 2.f * PI * 1.0f, Z = 0.22f, Sub = 1.f / 120.f;
		for (float T = 0.f; T < Dt; T += Sub)
		{
			const float H = FMath::Min(Sub, Dt - T);
			BncV += (-W * W * Bnc - 2.f * Z * W * BncV) * H; Bnc += BncV * H;
		}
		if (FMath::Abs(Bnc) < 0.0005f && FMath::Abs(BncV) < 0.002f) Bnc = BncV = 0.f;
		Bnc = FMath::Clamp(Bnc, -0.12f, 0.10f);   // (within the strut's travel past resting)
		E = Bnc * 100.f / StrutCm; V = 0.f;
	};
	Strut(M.ExtM, M.ExtMV, ClearM, M.BncM, M.BncMV, M.bOnM, (M.Phase == EPhase::Approach || M.Phase == EPhase::Rollout) ? -0.7f : 0.f, TEXT("main"), M.Reg);
	Strut(M.ExtN, M.ExtNV, ClearN, M.BncN, M.BncNV, M.bOnN, M.Phase == EPhase::Rollout ? -0.35f : 0.f, TEXT("nose"), M.Reg);
	M.bGearPrev = bGround;
	M.PrevNose = M.Nose; M.PrevMain = M.Main;
	// the retraction and extension (to-do 380) at FlightGear's rates: the doors, legs and doors again follow the
	// position-norm by FlightGear's own tables
	M.AirT = bGround ? 0.f : M.AirT + Dt;
	const float Ft = M.Height / 0.3048f;
	switch (M.GearSt)
	{
	case 0:   // down and locked: up once climbing away
		if (M.Phase == EPhase::Climb && M.Height >= GearUpM && M.AirT >= GearUpAfter)
		{
			M.GearSt = 1;
			ADP_LOG(TEXT("Traffic: %s positive climb - gear up at %.0f ft, %.1f s after lift-off"), *M.Reg, Ft, M.AirT);
		}
		break;
	case 1:   // retracting: nose 10.5 s, main 9 s
		M.PosN = FMath::Max(0.f, M.PosN - NoseUpRate * Dt); M.PosM = FMath::Max(0.f, M.PosM - MainUpRate * Dt);
		if (M.PosN <= 0.f && M.PosM <= 0.f) { M.GearSt = 2; ADP_LOG(TEXT("Traffic: %s gear up and locked, doors closed at %.0f ft"), *M.Reg, Ft); }
		break;
	case 2:   // up and locked: down at 2,000 ft on the approach
		if (M.Phase == EPhase::Approach && M.Height <= GearDownM)
		{
			M.GearSt = 3;
			ADP_LOG(TEXT("Traffic: %s gear down at %.0f ft"), *M.Reg, Ft);
		}
		break;
	case 3:   // extending: nose 12 s, main 16 s (hurried if ever this low)
	{
		const float Hurry = M.Height < 150.f ? 3.f : 1.f;
		M.PosN = FMath::Min(1.f, M.PosN + NoseDownRate * Hurry * Dt); M.PosM = FMath::Min(1.f, M.PosM + MainDownRate * Hurry * Dt);
		if (M.PosN >= 1.f && M.PosM >= 1.f) { M.GearSt = 0; ADP_LOG(TEXT("Traffic: %s gear down and locked, three greens at %.0f ft"), *M.Reg, Ft); }
		break;
	}
	}
	if (bGround && M.GearSt != 0)
	{
		ADP_WARN(TEXT("Traffic: %s on the ground with the gear %s - locked down"), *M.Reg, M.GearSt == 2 ? TEXT("up") : TEXT("moving"));
		M.GearSt = 0; M.PosN = M.PosM = 1.f;
	}
	// the poses (the model's root frame; A * B: A first)
	const TArray<int32>& G = M.GG;
	auto Put = [B, &G](int32 Gi, const FTransform& T) { if (G[Gi] != INDEX_NONE) B->MoveGroup(G[Gi], T); };
	const FGearLinkRt* Lk = GearLinks();
	// a linkage: bar 1 carried by TA, bar 2 by TB, the knee hinge turned by TH; returns bar 1's move (bar 2's in T2)
	FTransform T2;
	auto Link = [&](int32 i, const FTransform& TA, const FTransform& TB, const FTransform& TH) -> FTransform
	{
		const FGearLinkRt& L = Lk[i];
		const FVector A1 = TA.TransformPosition(L.A0), B1 = TB.TransformPosition(L.B0), H1 = TH.TransformVectorNoScale(L.H);
		const FVector K1 = GearKnee(A1, B1, L.L1, L.L2, H1, L.Side);
		const FTransform T1 = BarTo(L.A0, L.K0, TA, K1);
		T2 = BarTo(L.B0, L.K0, TB, K1);
		Put(GgLink + 2 * i, T1); Put(GgLink + 2 * i + 1, T2);
		return T1;
	};
	const FTransform I = FTransform::Identity;
	// nose: the fitting turns forward about its hinge; the lower fitting, tube, torque links and wheels steer about the
	// strut's axis; the tube slides out of it
	const FTransform RN = About(NoseHinge.P, QDeg(NoseHinge.Ax, NoseHinge.Deg * GTab(TLeg, M.PosN)));
	if (UADPAircraftLights* Lm = M.Lamps.Get()) Lm->SetNoseGear(RN, M.GearSt == 0);   // (taxi, take-off and turn-off lamps sit on the nose strut)
	const FTransform ST = About(SteerP, QDeg(SteerAx, SteerSign * M.Steer)) * RN;
	const FTransform TubeN = FTransform(FVector(0.f, 0.f, -StrutCm * M.ExtN)) * ST;
	Put(GgNoseUp, RN); Put(GgNoseLower, ST); Put(GgNoseLeg, TubeN);
	Put(GgNoseW, About(NoseAxle, FQuat(FVector::ForwardVector, FMath::DegreesToRadians(M.RollN))) * TubeN);
	Link(LDrag, I, RN, I);
	const FTransform Drag2 = T2;                  // (the nose lock stay rides the drag strut's lower bar)
	Link(LNoseLock, Drag2, RN, I);
	Link(LNoseTorque, ST, TubeN, ST);
	for (int32 s = 0; s < 2; ++s)
	{
		// the nose doors: forward doors open and close by the table, lifting 4 cm; the aft doors open with the leg and stay
		Put(GgNDoor + s, About(NoseDoorH[s].P, QDeg(NoseDoorH[s].Ax, NoseDoorH[s].Deg * GTab(TDoor, M.PosN))) * FTransform(FgUp * (100.f * GTab(TNoseLift, M.PosN))));
		Put(GgAft + s, About(AftDoorH[s].P, QDeg(AftDoorH[s].Ax, AftDoorH[s].Deg * GTab(TAftDoor, M.PosN))) * FTransform(FgUp * (100.f * GTab(TAftLift, M.PosN))));
		// main: the leg (and its door) turns inboard about its hinge; the tube slides; the fairing and bay door by their tables
		const FTransform RM = About(MainHinge[s].P, QDeg(MainHinge[s].Ax, MainHinge[s].Deg * GTab(TLeg, M.PosM)));
		const FTransform TubeM = FTransform(FVector(0.f, 0.f, -StrutCm * M.ExtM)) * RM;
		Put(GgMainUp + s, RM);
		Put(GgFair + s, About(FairingH[s].P, QDeg(FairingH[s].Ax, FairingH[s].Deg * GTab(TFairing, M.PosM))));
		Put(GgMainLeg + s, TubeM);
		Put(GgMainW + s, About(MainAxle[s], FQuat(FVector::ForwardVector, FMath::DegreesToRadians(M.RollM))) * TubeM);
		const FTransform BD = About(BayDoorH[s].P, QDeg(BayDoorH[s].Ax, BayDoorH[s].Deg * GTab(TDoor, M.PosM)));
		Put(GgBay + s, BD);
		// the bay door's actuator follows the door's fitting about its lock axis; the rod slides out by FlightGear's table
		FVector V0 = DoorActT[s] - DoorActC[s], V1 = BD.TransformPosition(DoorActT[s]) - DoorActC[s];
		V0 -= DoorActLock * FVector::DotProduct(V0, DoorActLock); V1 -= DoorActLock * FVector::DotProduct(V1, DoorActLock);
		const FTransform RA = About(DoorActC[s], FQuat::FindBetweenVectors(V0, V1));
		Put(GgAct + s, RA); Put(GgRod + s, FTransform(DoorRodDir[s] * (100.f * GTab(TRod, M.PosM))) * RA);
		// the linkages: side stay (wing to leg) and its lock stay, the torque and slave links scissoring with the tube
		const FTransform Up = Link(LSide + s, I, RM, I);
		Link(LLock + s, I, Up, I);
		Link(LTorque + s, RM, TubeM, RM);
		Link(LSlave + s, RM, TubeM, RM);
	}
	B->FlushMoves();
}

// the CFM56-5B fans (owner 11-28: they turn as in the real world, start gently and stop gently): each fan part about its hub
// (the model's own frame, the hub from the part's bounds; the engine's axis is the model's Y), by ADPA320Snd::FanN1 -
// 100 % N1 = 5,000 rpm. 36 blades = 10 deg apart: a picture can only show them turning forwards while they move less than
// half of that between two drawn frames (any faster and the eye, like any camera, sees them stand or run backwards), so
// the turn shown is the real one up to 4.5 deg a frame and held there above it - a fan at idle or full power reads as a
// fast spin; the start and the run-down below that are the real speed. At each light-off a very faint puff from the exhaust.
namespace
{
	const TCHAR* FanPart[2] = {TEXT("FanLCFM56L"), TEXT("FanLCFM56R")};
	const FVector FanHub[2] = {FVector(-563.1f, 718.2f, 179.9f), FVector(563.1f, 718.2f, 179.9f)};
	constexpr float FanRpm100 = 5000.f, FanStepMax = 4.5f;
}
void ASimTraffic::Fans(FMove& M, float Dt)
{
	ABabylonModel* B = M.Model.Get(); if (!B || Dt <= 0.f) return;
	if (!M.bFans)
	{
		M.bFans = true; int32 Found = 0;
		for (int32 e = 0; e < 2; ++e) { M.GFan[e] = B->BindGroup({FanPart[e]}, false, FTransform(FanHub[e])); Found += M.GFan[e] != INDEX_NONE; if (M.GFan[e] == INDEX_NONE) ADP_WARN(TEXT("Traffic: %s has no part %s - that fan stays still"), *M.Reg, FanPart[e]); }
		ADP_LOG(TEXT("Traffic: %s engine fans - %d of 2 bound, N1 now %.0f / %.0f %%"), *M.Reg, Found, ADPA320Snd::FanN1(B, 0), ADPA320Snd::FanN1(B, 1));
	}
	bool bMoved = false;
	for (int32 e = 0; e < 2; ++e)
	{
		bool bLit = false; const float N1 = ADPA320Snd::FanN1(B, e, &bLit);
		if (bLit)
		{
			// the light-off: a breath of grey from the core nozzle, drifting back and up, almost see-through
			const FTransform X = B->GetActorTransform();
			const FVector Back = X.TransformVectorNoScale(FVector(0.f, 1.f, 0.f)).GetSafeNormal();
			const FVector At = X.TransformPosition(FVector(FanHub[e].X, 1230.f, 175.f));
			for (int32 k = 0; k < 5; ++k)
			{
				FPuff P; P.Seed = FMath::FRand(); P.Span = FMath::FRandRange(4.f, 5.5f); P.Life = -0.25f * k;
				P.Alpha = FMath::FRandRange(0.08f, 0.13f); P.Size0 = 0.7f; P.Grow = FMath::FRandRange(2.f, 3.f);
				P.P = At + Back * (60.f * k) + FVector(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-10.f, 10.f));
				P.V = Back * FMath::FRandRange(150.f, 280.f) + FVector(0.f, 0.f, FMath::FRandRange(15.f, 40.f));
				Puffs.Add(P);
			}
			ADP_LOG(TEXT("Traffic: %s engine %d light-off - a faint puff from the exhaust (fan N1 %.1f %%)"), *M.Reg, e + 1, N1);
		}
		const float Step = FMath::Min(N1 / 100.f * FanRpm100 * 6.f * Dt, FanStepMax);   // (rpm x 6 = deg a second)
		const bool bTurn = Step > 1e-4f;
		if (bTurn != M.bFanTurning[e])
		{
			M.bFanTurning[e] = bTurn;
			ADP_LOG(TEXT("Traffic: %s engine %d fan %s (N1 %.2f %%)"), *M.Reg, e + 1, bTurn ? TEXT("starts to turn") : TEXT("has come to rest"), N1);
		}
		if (!bTurn || M.GFan[e] == INDEX_NONE) continue;
		M.FanDeg[e] = FMath::Fmod(M.FanDeg[e] + Step, 360.f);
		B->MoveGroup(M.GFan[e], FTransform(FQuat(FVector(0.f, -1.f, 0.f), FMath::DegreesToRadians(M.FanDeg[e])), FanHub[e]));
		bMoved = true;
	}
	if (bMoved) B->FlushMoves();
}

// tyre smoke at touchdown (to-do 377): the main wheels spin up from standstill and scrub - a short puff of white-grey smoke
// at each main gear (the A320's track 7.6 m), none from the nose; it is dragged a little way along, then hangs, rises,
// spreads and fades in 2-3 s. Every traffic type lands through here, so every one has it.
void ASimTraffic::TouchSmoke(const FMove& M)
{
	const FVector2f D2 = (M.Nose - M.Main).GetSafeNormal();
	const FVector Fwd(D2.X, -D2.Y, 0.f), Side(-Fwd.Y, Fwd.X, 0.f);
	const float G = AAirfield::GroundAt(M.Main.X, M.Main.Y) + M.SurfM;
	const FVector At = FTerminalPlan::ToWorld(M.Main.X, G + 0.5f, M.Main.Y);
	// (owner 11-28: too faint - now a dense white-grey burst at each main gear, laid along the first ~0.4 s of the roll as
	//  the wheels spin up, so a short trail stays on the runway behind the gear; it hangs, spreads to ~7 m and thins in 3-4 s)
	constexpr int32 NPer = 12;
	for (const float S : {-1.f, 1.f})
		for (int32 k = 0; k < NPer; ++k)
		{
			const float Delay = 0.035f * k;
			FPuff P; P.Seed = FMath::FRand(); P.Span = FMath::FRandRange(2.8f, 4.f); P.Life = -Delay;
			P.Alpha = FMath::FRandRange(0.85f, 1.f) * (k < 3 ? 1.f : 1.f - 0.5f * float(k - 3) / float(NPer - 3));   // (thickest at the first contact)
			P.Size0 = FMath::FRandRange(1.4f, 2.f); P.Grow = FMath::FRandRange(4.5f, 6.f);
			P.P = At + Side * (S * 380.f + FMath::FRandRange(-60.f, 60.f)) + Fwd * (M.Speed * 100.f * Delay + FMath::FRandRange(-80.f, 40.f));
			P.V = Fwd * (M.Speed * 100.f * FMath::FRandRange(0.1f, 0.25f)) + Side * (S * FMath::FRandRange(30.f, 120.f)) + FVector(0.f, 0.f, FMath::FRandRange(40.f, 110.f));
			Puffs.Add(P);
		}
	ADP_LOG(TEXT("Traffic: %s tyre smoke at both main gears (%d puffs over 0.4 s), none from the nose"), *M.Reg, 2 * NPer);
}

void ASimTraffic::Smoke(float Dt)
{
	UInstancedStaticMeshComponent* I = SmokeIsm.Get();
	if (!Puffs.Num()) { if (I && I->GetInstanceCount() > 0) I->ClearInstances(); return; }
	ADP_PERF_SCOPE("traffic: touchdown tyre smoke", 1.f);
	if (!I)
	{
		static UStaticMesh* Plane = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
		static UMaterialInterface* Mist = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_WaterMist.M_WaterMist")));
		if (!Plane || !Mist) { ADP_WARN(TEXT("Traffic: tyre smoke - %s missing, no smoke"), !Plane ? TEXT("the plane mesh") : TEXT("M_WaterMist")); Puffs.Reset(); return; }
		I = NewObject<UInstancedStaticMeshComponent>(this, TEXT("TyreSmoke"));
		I->SetupAttachment(GetRootComponent());
		I->SetStaticMesh(Plane); I->SetMaterial(0, Mist);
		I->SetCollisionEnabled(ECollisionEnabled::NoCollision); I->SetCastShadow(false);
		I->SetNumCustomDataFloats(2);
		I->SetUsingAbsoluteLocation(true); I->SetUsingAbsoluteRotation(true); I->SetUsingAbsoluteScale(true);
		I->SetWorldTransform(FTransform::Identity);
		I->RegisterComponent();
		SmokeIsm = I;
	}
	FVector Eye = FVector::ZeroVector;
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr) if (PC->PlayerCameraManager) Eye = PC->PlayerCameraManager->GetCameraLocation();
	TArray<FTransform> Shown; TArray<float> Data;
	for (int32 i = Puffs.Num() - 1; i >= 0; --i)
	{
		FPuff& P = Puffs[i];
		P.Life += Dt;
		if (P.Life >= P.Span) { Puffs.RemoveAtSwap(i); continue; }
		if (P.Life < 0.f) continue;
		P.P += P.V * Dt; P.V *= FMath::Max(0.f, 1.f - 2.5f * Dt); P.V.Z = FMath::Max(P.V.Z, 25.f);   // (dragged along, then hanging, rising slowly)
		const float K = P.Life / P.Span;
		const FVector ToEye = (Eye - P.P).GetSafeNormal();
		const FQuat Q = FQuat(ToEye, P.Seed * 2.f * PI) * FRotationMatrix::MakeFromZ(ToEye).ToQuat();
		Shown.Add(FTransform(Q, P.P, FVector(P.Size0 + P.Grow * FMath::Sqrt(K))));   // (its own size: the tyre smoke 1.5 m to ~7 m, the start-up puff smaller)
		Data.Add(P.Seed); Data.Add(P.Alpha * FMath::Square(1.f - K) * FMath::Min(1.f, P.Life * 10.f));
	}
	I->ClearInstances();
	if (Shown.Num())
	{
		I->AddInstances(Shown, false, true, false);
		for (int32 i = 0; i < Shown.Num(); ++i) I->SetCustomData(i, TArrayView<const float>(&Data[i * 2], 2), false);
		I->MarkRenderStateDirty();
	}
}
