// The town coach hub (to-do 373 step B) - see TownHub.h.
#include "TownHub.h"
#include "Coaches.h"
#include "VehicleRiders.h"
#include "PaxFlow.h"
#include "People.h"
#include "Seats.h"
#include "Site.h"
#include "Unseen.h"
#include "BabylonModel.h"
#include "ADPLog.h"
#include "Sim/ADPSim.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static TAutoConsoleVariable<int32> CVarTownHub(TEXT("adp.TownHub"), 1, TEXT("Departing passengers come from the town hub by coach (1), or appear at the airport as before (0)."));
static TAutoConsoleVariable<float> CVarTownHubBuffer(TEXT("adp.TownHubBufferMin"), 120.f, TEXT("Town coaches reach the terminal this many minutes before their flight (about 2 h for short-haul)."));

FString ATownHub::NoWhy;

namespace TownHubT
{
	constexpr double DriveS = 7.0 * 60.0;      // the loop, real seconds (about 5-7 min at 50 km/h)
	constexpr double MarginS = 5.0 * 60.0;     // a late start and traffic: the timetable's slack
	constexpr double SendS = 16.0 * 60.0;      // a coach leaves its terminal stand this long before its run leaves the hub
	constexpr double ReadyS = 6.0 * 60.0;      // a passenger at the shelter this long before the run leaves at the latest (walk, bag, board)
	constexpr double ShutS = 3.0 * 60.0;       // the bays shut and the driver gets in this long before the run leaves
	constexpr double HardS = 60.0 * 60.0;      // at the terminal no later than this before the flight (the desks close 45 min before)
	constexpr int32 Slots = 6;                 // people at the coach's door at once
	const TCHAR* const Drivers[] = {TEXT("MH_076_ground"), TEXT("MH_077_ground"), TEXT("MH_078_ground"), TEXT("MH_079_ground"), TEXT("MH_080_ground"), TEXT("MH_090_ground")};
}

ATownHub::ATownHub() { PrimaryActorTick.bCanEverTick = true; }

ATownHub* ATownHub::Get(UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<ATownHub> It(W); It; ++It) return *It;
	return W->SpawnActor<ATownHub>();
}

float ATownHub::BufferMin() { return FMath::Clamp(CVarTownHubBuffer.GetValueOnGameThread(), 30.f, 300.f); }

bool ATownHub::Use(UWorld* W)
{
	// (asked for every flight PaxFlow looks at: the answer kept 20 s)
	static double CheckedAt = -1e9; static bool bWas = false; static TWeakObjectPtr<UWorld> WasIn;
	const double Clock = FPlatformTime::Seconds();
	if (WasIn.Get() == W && Clock < CheckedAt + 20.0) return bWas;
	CheckedAt = Clock; WasIn = W;
	FString Why;
	ACoaches* Co = ACoaches::Find();
	ATownHub* H = nullptr;
	if (CVarTownHub.GetValueOnGameThread() == 0) Why = TEXT("adp.TownHub is 0");
	else if (FParse::Param(FCommandLine::Get(), TEXT("CoachIKTest")) || FParse::Param(FCommandLine::Get(), TEXT("PaxFlowTest"))) Why = TEXT("a test run");
	else if (!(H = Get(W)) || !H->Load()) Why = TEXT("this site's roads.json has no TOWN-HUB with bays and a shelter");
	else if (!Co || Co->Num() == 0) Why = TEXT("no coaches parked at the terminal");
	else
	{
		UVehicleRiders::bTownLine = true;
		FString R;
		if (!Co->Riders(0)) Why = TEXT("the coaches' riders can't be made (their points file or driving parts missing)");
		else if (Co->RoutePath(0, true, H->BayOrder.Num() ? H->BayOrder[0] : 0, Co->RootOf(0), R).Num() < 2) Why = FString::Printf(TEXT("no coach loop to the hub (%s)"), *R);
		if (!Why.IsEmpty()) UVehicleRiders::bTownLine = false;
	}
	if (Why != NoWhy)
	{
		NoWhy = Why;
		if (Why.IsEmpty()) ADP_LOG(TEXT("TownHub: departing passengers come from the town hub by coach (buffer %.0f min)"), BufferMin());
		else ADP_WARN(TEXT("TownHub: off - %s; departing passengers appear at the airport as before"), *Why);
	}
	bWas = Why.IsEmpty();
	return bWas;
}

bool ATownHub::Load()
{
	if (bLoaded) return bDataOk;
	bLoaded = true;
	FString Text; TSharedPtr<FJsonObject> J;
	if (!FFileHelper::LoadFileToString(Text, *ADPSite::File(TEXT("roads.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()) return false;
	const TArray<TSharedPtr<FJsonValue>>* Roads = nullptr;
	if (!J->TryGetArrayField(TEXT("roads"), Roads)) return false;
	auto Plan = [](const TArray<TSharedPtr<FJsonValue>>& A) { return A.Num() >= 2 ? FVector(A[0]->AsNumber() * 100.0, -A[1]->AsNumber() * 100.0, 0.0) : FVector::ZeroVector; };
	for (const TSharedPtr<FJsonValue>& V : *Roads)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O || O->GetStringField(TEXT("id")) != TEXT("TOWN-HUB")) continue;
		const TArray<TSharedPtr<FJsonValue>>* B = nullptr; const TSharedPtr<FJsonObject>* Sh = nullptr; const TArray<TSharedPtr<FJsonValue>>* ShAt = nullptr;
		if (!O->TryGetArrayField(TEXT("bays"), B) || !O->TryGetObjectField(TEXT("shelter"), Sh) || !(*Sh)->TryGetArrayField(TEXT("at"), ShAt)) return false;
		for (const TSharedPtr<FJsonValue>& Bv : *B) Bays.Add(Plan(Bv->AsArray()));
		double Len = 30.0; (*Sh)->TryGetNumberField(TEXT("len"), Len);
		const FVector C = Plan(*ShAt);
		// (the shelter runs along the bays' line)
		FVector Dir = Bays.Num() >= 2 ? (Bays.Last() - Bays[0]).GetSafeNormal2D() : FVector(0, 1, 0);
		if (Dir.IsNearlyZero()) Dir = FVector(0, 1, 0);
		ShelterA = C - Dir * (Len * 50.0); ShelterB = C + Dir * (Len * 50.0);
		for (int32 i = 0; i < Bays.Num(); ++i) BayOrder.Add(i);
		BayOrder.Sort([this, C](int32 X, int32 Y) { return FVector::DistSquared2D(Bays[X], C) < FVector::DistSquared2D(Bays[Y], C); });
		bDataOk = Bays.Num() > 0;
		ADP_LOG(TEXT("TownHub: %d coach bays, a %.0f m shelter"), Bays.Num(), Len);
		return bDataOk;
	}
	return false;
}

float ATownHub::GroundAt(const FVector& Xy) const
{
	// (the lowest solid thing under the point: under the shelter's roof, its pavement)
	FVector From(Xy.X, Xy.Y, 200000.0); const FVector To(Xy.X, Xy.Y, -200000.0);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(TownHubGround), false);
	float Z = 0.f; FHitResult Hit;
	for (int32 k = 0; k < 6 && GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q); ++k)
	{
		Z = float(Hit.ImpactPoint.Z); From.Z = Hit.ImpactPoint.Z - 5.0;
		if (Hit.GetActor()) Q.AddIgnoredActor(Hit.GetActor());
	}
	return Z;
}

ACoaches* ATownHub::Co() const { return ACoaches::Find(); }
double ATownHub::Now() const { const APaxFlow* P = APaxFlow::Find(GetWorld()); return P ? P->SimNow() : 0.0; }

int32 ATownHub::SeatsOf(const FHubCoach& C) const
{
	TArray<int32> S; if (const ACoaches* K = Co()) ASeats::SeatsOf(K->ModelOf(C.Ci), S);
	return S.Num() ? S.Num() : 40;
}

int32 ATownHub::SeatCap() const
{
	TArray<int32> S; if (const ACoaches* K = Co()) if (K->Num()) ASeats::SeatsOf(K->ModelOf(0), S);
	return S.Num() ? S.Num() : 40;
}

bool ATownHub::SpawnSpot(FVector& At)
{
	if (!bDataOk) return false;
	const AADPPeople* Pe = AADPPeople::Get(GetWorld());
	const int32 N = FMath::Max(2, int32(FVector::Dist2D(ShelterA, ShelterB) / 90.f));
	for (int32 k = 0; k < N; ++k)
	{
		const int32 i = (NextSpot + k * 7) % N;
		FVector P = FMath::Lerp(ShelterA, ShelterB, (i + 0.5f) / N);
		P.Z = GroundAt(P);
		if (Pe && Pe->AnyoneNear(P, 75.f)) continue;
		if (!ADPUnseen::May(GetWorld(), P)) continue;
		NextSpot = i + 1; At = P;
		return true;
	}
	return false;
}

// ---- the timetable

int32 ATownHub::NewTrip(const FString& Flight, double DepT, double At)
{
	FTrip T; T.Flight = Flight; T.DepT = DepT; T.LeaveT = At; T.RateAt = Rate;
	T.Late = FMath::FRandRange(0.f, 90.f); T.LimitMs = FMath::FRandRange(11.5f, 13.9f);   // (its small real-life delay; the traffic)
	const int32 i = Trips.Add(T);
	ADP_LOG(TEXT("TownHub: coach run %d for %s - leaves the hub %.0f min before the flight (%.1f min late), reaches the terminal about %.0f min before"), i, *Flight, (DepT - At) / 60.0, T.Late / 60.f, (DepT - At - TownHubT::DriveS * Rate) / 60.0);
	return i;
}

int32 ATownHub::Book(const FString& Flight, double DepT, bool bBag, double NowT, double& SpawnT)
{
	using namespace TownHubT;
	const int32 Cap = SeatCap(), BagCap = 12;
	const double Lead = 10.0 * 60.0 * Rate, Drive = (DriveS + MarginS) * Rate;
	int32 Got = INDEX_NONE, Same = 0;
	for (int32 i = 0; i < Trips.Num(); ++i)
	{
		const FTrip& T = Trips[i];
		if (T.Flight != Flight || T.bGone) continue;
		++Same;
		if (T.People >= Cap || (bBag && T.BagsN >= BagCap) || T.LeaveT < NowT + Lead) continue;
		if (T.Coach != INDEX_NONE && Fleet.IsValidIndex(T.Coach) && Fleet[T.Coach].St > EStage::HubBags) continue;
		Got = i; break;
	}
	if (Got == INDEX_NONE)
	{
		// worked back from the flight: at the terminal the buffer before it, the drive and its margin before that; one run after
		// another for the same flight three minutes apart (earlier)
		double Leave = DepT - BufferMin() * 60.0 - Drive - Same * 180.0 * Rate;
		if (Leave < NowT + Lead) Leave = NowT + Lead;
		if (Leave + Drive > DepT - HardS)
		{
			ADP_WARN(TEXT("TownHub: a passenger of %s can't make any coach - the latest would reach the terminal %.0f min before the flight (at least %.0f needed); they come to the airport instead"), *Flight, (DepT - Leave - Drive) / 60.0, HardS / 60.0);
			return INDEX_NONE;
		}
		Got = NewTrip(Flight, DepT, Leave);
	}
	FTrip& T = Trips[Got];
	++T.People; if (bBag) ++T.BagsN;
	SpawnT = FMath::Max(NowT, T.LeaveT - FMath::FRandRange(10.f, 18.f) * 60.0 * Rate);
	return Got;
}

double ATownHub::LatestSpawn(int32 Run) const { return Trips.IsValidIndex(Run) && !Trips[Run].bGone ? Trips[Run].LeaveT - TownHubT::ReadyS * Rate : -1.0; }

double ATownHub::EarliestSpawn(int32 Run) const { return Trips.IsValidIndex(Run) && !Trips[Run].bGone ? Trips[Run].LeaveT - 18.0 * 60.0 * Rate : 0.0; }

int32 ATownHub::BookArrival(const FString& Flight, bool bBag)
{
	if (!bDataOk || NoWhy.Len()) return INDEX_NONE;
	const double T0 = Now(); const int32 Cap = SeatCap();
	int32 Got = INDEX_NONE;
	for (int32 i = 0; i < Trips.Num() && Got == INDEX_NONE; ++i)
	{
		const FTrip& T = Trips[i];
		if (!T.bArr || T.bGone || T.People >= Cap || (bBag && T.BagsN >= 12)) continue;
		if (T.Coach != INDEX_NONE && Fleet.IsValidIndex(T.Coach) && Fleet[T.Coach].St > EStage::HubBags) continue;
		Got = i;
	}
	if (Got == INDEX_NONE)
	{
		FTrip T; T.bArr = true; T.Flight = Flight; T.RateAt = Rate;
		T.LeaveT = T0 + FMath::FRandRange(8.f, 14.f) * 60.0 * Rate;   // (from the terminal: a while after the first one comes out, sooner when full)
		T.Late = FMath::FRandRange(0.f, 90.f); T.LimitMs = FMath::FRandRange(11.5f, 13.9f);
		Got = Trips.Add(T);
		ADP_LOG(TEXT("TownHub: arrival coach run %d (first for %s) - leaves the terminal for the hub in %.0f min"), Got, *Flight, (T.LeaveT - T0) / 60.0 / Rate);
	}
	FTrip& T = Trips[Got];
	++T.People; if (bBag) ++T.BagsN;
	return Got;
}

void ATownHub::TakeArrival(AActor* Who, bool bBag, const FString& Flight, int32 Run)
{
	FWaiter W; W.A = Who; W.bBag = bBag; W.Flight = Flight; W.Trip = Run; W.bArr = true;
	if (const APaxFlow* PF = APaxFlow::Find(GetWorld())) W.Kind = PF->HubKindOf(Who);
	if (Trips.IsValidIndex(Run)) W.Coach = Trips[Run].Coach;
	Waiters.Add(W);
}

void ATownHub::Unbook(int32 Run, bool bBag, const FString& Why)
{
	if (!Trips.IsValidIndex(Run)) return;
	FTrip& T = Trips[Run];
	T.People = FMath::Max(0, T.People - 1); if (bBag) T.BagsN = FMath::Max(0, T.BagsN - 1);
	ADP_WARN(TEXT("TownHub: a passenger of %s booked on coach run %d won't come - %s"), *T.Flight, Run, *Why);
}

void ATownHub::Take(AActor* Who, bool bBag, const FString& Flight, int32 Run, double DepT)
{
	FWaiter W; W.A = Who; W.bBag = bBag; W.Flight = Flight; W.Trip = Run; W.DepT = DepT;
	if (const APaxFlow* PF = APaxFlow::Find(GetWorld())) W.Kind = PF->HubKindOf(Who);
	if (Trips.IsValidIndex(Run)) W.Coach = Trips[Run].Coach;
	Waiters.Add(W);
}

int32 ATownHub::TaxiFor(FWaiter& W) { return INDEX_NONE; }   // (the hook: no taxi drives yet)

bool ATownHub::Rebook(FWaiter& W, const FString& Why)
{
	using namespace TownHubT;
	const double T0 = Now(), Drive = (DriveS + MarginS) * Rate;
	const FString Who = W.A.IsValid() ? W.A->GetName() : FString(TEXT("?"));
	const int32 Old = W.Trip;
	if (Trips.IsValidIndex(Old)) { Trips[Old].People = FMath::Max(0, Trips[Old].People - 1); if (W.bBag) Trips[Old].BagsN = FMath::Max(0, Trips[Old].BagsN - 1); }
	if (TaxiFor(W) != INDEX_NONE) { W.bTaxi = true; return true; }
	int32 Got = INDEX_NONE;
	for (int32 i = 0; i < Trips.Num() && Got == INDEX_NONE; ++i)
	{
		const FTrip& T = Trips[i];
		if (i == Old || T.Flight != W.Flight || T.bGone || T.People >= SeatCap() || T.LeaveT < T0 + 4.0 * 60.0 * Rate) continue;
		if (T.Coach != INDEX_NONE && Fleet.IsValidIndex(T.Coach) && Fleet[T.Coach].St > EStage::HubBags) continue;
		Got = i;
	}
	if (Got == INDEX_NONE && T0 + 6.0 * 60.0 * Rate + Drive <= W.DepT - HardS) Got = NewTrip(W.Flight, W.DepT, T0 + 6.0 * 60.0 * Rate);
	if (Got == INDEX_NONE)
	{
		ADP_WARN(TEXT("TownHub: %s (%s) missed coach run %d - %s; no later run reaches the terminal in time, they go home"), *Who, *W.Flight, Old, *Why);
		W.Trip = INDEX_NONE; W.Phase = 8;
		return false;
	}
	ADP_WARN(TEXT("TownHub: %s (%s) missed coach run %d - %s; now on run %d"), *Who, *W.Flight, Old, *Why, Got);
	W.Trip = Got; W.Coach = Trips[Got].Coach; W.Phase = 0; W.Slot = INDEX_NONE; W.bIn = false; W.Bay = INDEX_NONE;
	++Trips[Got].People; if (W.bBag) ++Trips[Got].BagsN;
	return true;
}

void ATownHub::Missed(FWaiter& W, const FString& Why)
{
	// (out of this coach's hands first: their pose and the bag in hand of this coach's riders - only when they stand still)
	if (Fleet.IsValidIndex(W.Coach)) if (ACoaches* K = Co()) if (UVehicleRiders* R = K->Riders(Fleet[W.Coach].Ci)) R->Leave(W.A.Get());
	if (W.bArr)   // (an arrival: the next run from the terminal - they stand where they are, on the walking map, till it boards)
	{
		if (Trips.IsValidIndex(W.Trip)) { Trips[W.Trip].People = FMath::Max(0, Trips[W.Trip].People - 1); if (W.bBag) Trips[W.Trip].BagsN = FMath::Max(0, Trips[W.Trip].BagsN - 1); }
		ADP_WARN(TEXT("TownHub: an arrival of %s missed coach run %d - %s; on the next one"), *W.Flight, W.Trip, *Why);
		W.Trip = BookArrival(W.Flight, W.bBag); W.Coach = Trips.IsValidIndex(W.Trip) ? Trips[W.Trip].Coach : INDEX_NONE; W.Phase = 0; W.Slot = INDEX_NONE; W.bIn = false;
		if (AActor* A = W.A.Get()) if (APaxFlow* PF = APaxFlow::Find(GetWorld())) PF->HubWalk(A, A->GetActorLocation());
		return;
	}
	Rebook(W, Why);
}

// ---- the coaches

void ATownHub::Fleetup()
{
	ACoaches* K = Co(); if (!K) return;
	for (int32 i = Fleet.Num(); i < K->Num(); ++i) { FHubCoach C; C.Ci = i; Fleet.Add(C); }
}

void ATownHub::Say(FHubCoach& C, const FString& What, bool bWarn)
{
	if (bWarn) ADP_WARN(TEXT("TownHub: coach %d (run %d): %s"), C.Ci, C.Trip, *What);
	else ADP_LOG(TEXT("TownHub: coach %d (run %d): %s"), C.Ci, C.Trip, *What);
}

bool ATownHub::DriverFor(FHubCoach& C)
{
	// the coach operator's own driver (not the airport's hired staff): appears unseen by the front door, then climbs in
	ACoaches* K = Co(); UVehicleRiders* R = K ? K->Riders(C.Ci) : nullptr;
	if (!R) return false;
	if (C.Driver.IsValid()) return true;
	AADPPeople* P = AADPPeople::Get(GetWorld()); if (!P) return false;
	const FTransform Root = K->RootOf(C.Ci);
	FVector At = Root.TransformPosition(FVector(320.f, 460.f, 0.f)); At.Z = GroundAt(At);
	if (P->AnyoneNear(At, 80.f) || !ADPUnseen::May(GetWorld(), At)) return false;
	AActor* D = P->Spawn(TownHubT::Drivers[C.Ci % UE_ARRAY_COUNT(TownHubT::Drivers)], At + FVector(0, 0, 60.f), float(Root.Rotator().Yaw) - 90.f);
	if (!D) return false;
	if (!R->HireDriver(D)) { D->Destroy(); Say(C, TEXT("its driver could not take the wheel"), true); return false; }
	C.Driver = D; Say(C, TEXT("its driver comes (the coach operator's)"));
	return true;
}

void ATownHub::Dispatch()
{
	using namespace TownHubT;
	const double T0 = Now();
	ACoaches* K = Co(); if (!K) return;
	Retime();
	for (int32 t = 0; t < Trips.Num(); ++t)
	{
		FTrip& T = Trips[t];
		if (T.bGone || T.Coach != INDEX_NONE) continue;
		if (T.People <= 0 && T0 > T.LeaveT) { T.bGone = true; continue; }   // (everyone unbooked)
		if (T.bArr && T0 > T.LeaveT + T.Late + 120.0 * Rate)   // (arrivals are at the terminal already: their run waits on for a coach)
		{
			T.LeaveT = T0 + 5.0 * 60.0 * Rate;
			if (!T.bNoCoachSaid) { T.bNoCoachSaid = true; ADP_WARN(TEXT("TownHub: arrival coach run %d for %s waits for a free coach at the terminal"), t, *T.Flight); }
		}
		if (T0 > T.LeaveT + T.Late + 120.0 * Rate)   // (no coach came at all)
		{
			T.bGone = true;
			ADP_WARN(TEXT("TownHub: coach run %d for %s never left - no coach was free"), t, *T.Flight);
			for (FWaiter& W : Waiters) if (W.Trip == t && W.Phase == 0) Missed(W, TEXT("no coach was free for their run"));
			continue;
		}
		if (T0 < T.LeaveT - SendS * Rate || T.People <= 0) continue;
		int32 Pick = INDEX_NONE;
		// (a departure run takes a coach already parked at the hub first; an arrival run needs one at the terminal)
		for (int32 Pass = T.bArr ? 1 : 0; Pass < 2 && Pick == INDEX_NONE; ++Pass)
			for (int32 c = 0; c < Fleet.Num() && Pick == INDEX_NONE; ++c)
				if (Fleet[c].Trip == INDEX_NONE && Fleet[c].St == EStage::Idle && Fleet[c].bAtHub == (Pass == 0) && !K->OnRoute(Fleet[c].Ci) && K->Riders(Fleet[c].Ci)) Pick = c;
		if (Pick == INDEX_NONE) { if (!T.bNoCoachSaid) { T.bNoCoachSaid = true; ADP_WARN(TEXT("TownHub: coach run %d for %s waits for a free coach"), t, *T.Flight); } continue; }
		FHubCoach& C = Fleet[Pick];
		if (!C.bAtHub)   // (parked at the hub: it keeps its bay)
		{
			TSet<int32> Used; for (const FHubCoach& O : Fleet) if (O.Trip != INDEX_NONE || O.bAtHub) Used.Add(O.Bay);
			C.Bay = INDEX_NONE; for (const int32 b : BayOrder) if (!Used.Contains(b)) { C.Bay = b; break; }
			if (C.Bay == INDEX_NONE) continue;
		}
		C.Trip = t; C.T = 0.f; C.Fails = 0; C.bSlow = false; C.BaysOpen.Reset(); C.BagsIn.Reset(); T.Coach = Pick;
		for (FWaiter& W : Waiters) if (W.Trip == t) W.Coach = Pick;
		if (T.bArr) Say(C, FString::Printf(TEXT("takes arrival run %d for %s at the terminal (%d booked, to hub bay %d in %.0f min)"), t, *T.Flight, T.People, C.Bay, (T.LeaveT - T0) / 60.0 / Rate));
		else Say(C, FString::Printf(TEXT("%s hub bay %d for %s (%d booked, leaves the hub in %.0f min)"), C.bAtHub ? TEXT("waits on in") : TEXT("goes to"), C.Bay, *T.Flight, T.People, (T.LeaveT - T0) / 60.0 / Rate));
	}
}

void ATownHub::Step(FHubCoach& C, float Dt)
{
	using namespace TownHubT;
	ACoaches* K = Co(); UVehicleRiders* R = K ? K->Riders(C.Ci) : nullptr;
	if (!R || C.Trip == INDEX_NONE || !Trips.IsValidIndex(C.Trip)) return;
	FTrip& T = Trips[C.Trip];
	const double T0 = Now();
	AActor* D = C.Driver.Get();
	C.T += Dt;
	auto Go = [&C](EStage S) { C.St = S; C.T = 0.f; C.bSlow = false; };
	if (C.T > 600.f && !C.bSlow && C.St != EStage::HubBags && C.St != EStage::Idle) { C.bSlow = true; Say(C, FString::Printf(TEXT("stage %d takes over 10 min"), int32(C.St)), true); }
	switch (C.St)
	{
	case EStage::Idle:
		if (!DriverFor(C)) break;
		D = C.Driver.Get();
		if (T.bArr || C.bAtHub)   // (boarding where it stands: an arrival run at the terminal, or a departure run at the hub)
		{
			if (!D || R->IsBusy(D)) break;
			if (!R->DriverAtWheel()) { R->DriverGetIn(); break; }   // (the driver to the coach the usual way, then out at its door)
			if (R->DriverGetOut()) { Go(EStage::HubOpen); Say(C, T.bArr ? TEXT("at the terminal, the driver gets out for the arrivals") : TEXT("waits at the hub, the driver gets out")); }
			break;
		}
		if (R->DriverAtWheel()) { Go(EStage::DriverComing); break; }
		if (D && !R->IsBusy(D) && R->DriverGetIn()) Go(EStage::DriverComing);
		break;
	case EStage::DriverComing:
		if (!R->DriverAtWheel()) break;
		if (C.T < 1.f) break;
		if (K->SendOnRoute(C.Ci, true, C.Bay, T.LimitMs)) { Go(EStage::ToTown); Say(C, TEXT("drives out to the town hub")); break; }
		C.T = -9.f;   // (again in 10 s)
		if (++C.Fails >= 4) { Say(C, TEXT("cannot leave its stand - given up, its run waits for another coach"), true); T.Coach = INDEX_NONE; for (FWaiter& W : Waiters) if (W.Trip == C.Trip) W.Coach = INDEX_NONE; C.Trip = INDEX_NONE; Go(EStage::Broken); }
		break;
	case EStage::ToTown:
		if (K->OnRoute(C.Ci) || R->IsDriving()) break;
		if (R->DriverGetOut()) { Go(EStage::HubOpen); Say(C, TEXT("at the hub, the driver gets out")); }
		break;
	case EStage::HubOpen:
	{
		if (!D || R->IsBusy(D)) break;
		// the door-side luggage bays (the same side as the door), opened by the driver one after the other
		const TArray<FVector>& Bl = R->GetSpec().List(TEXT("bays"));
		const float DoorY = float(R->GetSpec().At(TEXT("door_stand")).Y);
		C.BaysOpen.Reset(); C.BagsIn.Reset();
		for (int32 b = 0; b < Bl.Num(); ++b) if (FMath::Sign(Bl[b].Y) == FMath::Sign(DoorY)) { C.BaysOpen.Add(b); C.BagsIn.Add(0); }
		if (!C.BaysOpen.Num()) for (int32 b = 0; b < Bl.Num(); ++b) { C.BaysOpen.Add(b); C.BagsIn.Add(0); }
		for (const int32 b : C.BaysOpen) R->UseBay(D, b, true);
		Go(EStage::HubBags);
		break;
	}
	case EStage::HubBags:
	{
		// one after the other to the door: up to six at once at their place, the bag in, on board
		int32 Busy = 0; TSet<int32> Taken;
		for (const FWaiter& W : Waiters) if (W.Trip == C.Trip && ((W.Phase >= 1 && W.Phase <= 2) || W.Phase == 9)) { ++Busy; Taken.Add(W.Slot); }
		int32 Spawned = 0, Up = 0;
		APaxFlow* PF = APaxFlow::Find(GetWorld());
		for (FWaiter& W : Waiters)
		{
			if (W.Trip != C.Trip || W.Phase >= 7) continue;
			AActor* A = W.A.Get(); if (!A) { W.Phase = 7; continue; }
			++Spawned;
			switch (W.Phase)
			{
			case 0:
				if (Busy >= Slots) break;
				{
					int32 S = 0; while (Taken.Contains(S)) ++S;
					// (to their place by the door on the walking map - round things and people, never straight through)
					if (!PF || !PF->HubWalk(A, R->StopPoint(S))) break;
					W.Slot = S; Taken.Add(S); ++Busy; W.Phase = 9;
				}
				break;
			case 9:
				if (!PF || !PF->HubAt(A)) break;
				PF->HubRelease(A);   // (the riders have them now: the same bag model in the same hand)
				if (!R->WaitAtStop(A, W.Slot, W.bBag, W.Kind)) { Missed(W, TEXT("the coach could not take them at its door")); break; }
				W.Phase = 1;
				break;
			case 1:
				if (R->IsBusy(A)) break;
				if (W.bBag && C.BaysOpen.Num())
				{
					int32 Bi = 0; for (int32 k = 1; k < C.BagsIn.Num(); ++k) if (C.BagsIn[k] < C.BagsIn[Bi]) Bi = k;
					if (C.BagsIn[Bi] >= 4 || K->BayKOf(C.Ci, C.BaysOpen[Bi]) < 0.99f) { if (C.BagsIn[Bi] >= 4) Missed(W, TEXT("the luggage bays are full")); break; }
					if (!R->LoadBag(A, A, C.BaysOpen[Bi])) break;
					++C.BagsIn[Bi]; W.Bay = C.BaysOpen[Bi]; W.bIn = true;
				}
				W.Phase = 2;
				break;
			case 2:
				if (R->IsBusy(A)) break;
				if (R->Board(A)) { W.Phase = 3; W.Slot = INDEX_NONE; }
				else Missed(W, TEXT("no seat left on the coach"));
				break;
			case 3: if (R->IsSeated(A) && !R->IsBusy(A)) W.Phase = 4; break;
			default: break;
			}
			if (W.Phase >= 4 && W.Phase < 7) ++Up;
		}
		// the bays shut once all booked are on, or as the run's time comes (the rest miss it)
		const bool bAll = Spawned > 0 && Up == Spawned && Spawned >= T.People && (!T.bArr || T.People >= SeatCap());   // (an arrival run: on its time unless full)
		if (!bAll && T0 < T.LeaveT + T.Late - ShutS * Rate) break;
		if (D && R->IsBusy(D)) break;
		bool bOnSteps = false;
		for (FWaiter& W : Waiters)
		{
			if (W.Trip != C.Trip || W.Phase >= 7) continue;
			if (W.Phase == 3) { bOnSteps = true; continue; }   // (those getting on still: on)
			if (W.Phase == 0 || W.Phase == 9) { Missed(W, W.Phase == 0 ? TEXT("still waiting when the coach's bays shut") : TEXT("still walking to the coach as it closed")); continue; }
			if (W.Phase <= 2) { if (W.A.IsValid() && R->IsBusy(W.A.Get())) { bOnSteps = true; continue; } Missed(W, W.Phase == 1 ? TEXT("still walking to the coach as it closed") : TEXT("their bag was in but they never got on")); }
		}
		if (bOnSteps) break;
		for (const int32 b : C.BaysOpen) if (D) R->UseBay(D, b, false);
		Go(EStage::HubShut);
		break;
	}
	case EStage::HubShut:
		if (!D || R->IsBusy(D)) break;
		if (R->DriverGetIn()) Go(EStage::HubBoard);
		break;
	case EStage::HubBoard:
	{
		if (!R->DriverAtWheel()) break;
		int32 On = 0; for (const FWaiter& W : Waiters) if (W.Trip == C.Trip && W.Phase == 4) ++On;
		if (T0 < T.LeaveT + T.Late && On < (T.bArr ? SeatCap() : T.People)) break;
		if (K->SendOnRoute(C.Ci, T.bArr, T.bArr ? C.Bay : INDEX_NONE, T.LimitMs))   // (arrivals: forward out of the stand, the one-way loop to its hub bay)
		{
			Go(EStage::ToTerminal); C.bAtHub = false;
			Say(C, FString::Printf(TEXT("leaves %s with %d aboard (%.1f min after its time)"), T.bArr ? TEXT("the terminal for the town hub") : TEXT("the hub for the terminal"), On, (T0 - T.LeaveT) / 60.0 / Rate));
		}
		else C.T = -9.f;
		break;
	}
	case EStage::ToTerminal:
		if (K->OnRoute(C.Ci) || R->IsDriving()) break;
		if (T.bArr) Say(C, FString::Printf(TEXT("at the town hub with the arrivals of %s"), *T.Flight));
		else Say(C, FString::Printf(TEXT("at the terminal %.0f min before %s (buffer %.0f)"), (T.DepT - T0) / 60.0, *T.Flight, BufferMin()), (T.DepT - T0) < HardS);
		if (!R->DriverGetOut()) break;
		for (FWaiter& W : Waiters) if (W.Trip == C.Trip && W.Phase == 4 && W.A.IsValid()) { R->Alight(W.A.Get()); W.Phase = 5; }
		Go(EStage::TermOpen);
		break;
	case EStage::TermOpen:
		if (!D || R->IsBusy(D)) break;
		for (int32 k = 0; k < C.BaysOpen.Num(); ++k) if (C.BagsIn.IsValidIndex(k) && C.BagsIn[k] > 0) R->UseBay(D, C.BaysOpen[k], true);
		Go(EStage::TermOff);
		break;
	case EStage::TermOff:
	{
		bool bLeft = false;
		APaxFlow* PF = APaxFlow::Find(GetWorld());
		for (FWaiter& W : Waiters)
		{
			if (W.Trip != C.Trip || W.Phase < 5 || W.Phase >= 7) continue;
			AActor* A = W.A.Get(); if (!A) { W.Phase = 7; continue; }
			bLeft = true;
			if (R->IsAboard(A) || R->IsBusy(A)) continue;
			if (W.Phase == 5 && W.bIn) { if (K->BayKOf(C.Ci, W.Bay) >= 0.99f && R->TakeBag(A)) W.Phase = 6; continue; }
			R->Leave(A); W.Phase = 7;
			if (T.bArr)   // (at the hub: their bag (the same model) in hand, out of the hub on the walking map, gone where nobody sees)
			{
				FVector Out; if (!SpawnSpot(Out)) { Out = ShelterB + (ShelterB - ShelterA).GetSafeNormal2D() * 1500.f; Out.Z = GroundAt(Out); }
				if (PF) PF->HubExit(A, Out, W.bBag && W.bIn, W.Kind); else ADP_WARN(TEXT("TownHub: %s off the coach at the hub, but no PaxFlow"), *A->GetName());
				continue;
			}
			// (handed back: their own bag in hand becomes the check-in bag, on as before)
			if (!PF || !PF->FromCoach(A)) ADP_WARN(TEXT("TownHub: %s off the coach, but PaxFlow does not know them"), *A->GetName());
		}
		if (bLeft || (D && R->IsBusy(D))) break;
		for (int32 k = 0; k < C.BaysOpen.Num(); ++k) if (C.BagsIn.IsValidIndex(k) && C.BagsIn[k] > 0) R->UseBay(D, C.BaysOpen[k], false);
		Go(EStage::TermShut);
		break;
	}
	case EStage::TermShut:
		if (D && R->IsBusy(D)) break;
		if (!R->DriverAtWheel()) { if (C.T > 0.5f && R->DriverGetIn()) C.T = -1e6f; break; }
		Say(C, T.bArr ? TEXT("empty at the hub - waits there for a departure run or drives back") : TEXT("back at its stand, free"));
		C.bAtHub = T.bArr; if (!T.bArr) C.Bay = INDEX_NONE;
		T.bGone = true; C.Trip = INDEX_NONE; Go(EStage::Idle);
		break;
	default: break;
	}
}

void ATownHub::Retime()
{
	using namespace TownHubT;
	const double T0 = Now();
	const APaxFlow* PF = APaxFlow::Find(GetWorld());
	for (int32 t = 0; t < Trips.Num(); ++t)
	{
		FTrip& T = Trips[t];
		if (T.bGone || T.bArr) continue;
		if (T.Coach != INDEX_NONE && Fleet.IsValidIndex(T.Coach) && Fleet[T.Coach].St > EStage::HubBags) continue;   // (on the way: fine as it is)
		double Dep = T.DepT;
		const bool bDep = PF && PF->DepOf(T.Flight, Dep) && FMath::Abs(Dep - T.DepT) > 60.0;
		const bool bRate = FMath::Abs(Rate - T.RateAt) > 0.25;
		if (!bDep) Dep = T.DepT;
		if (!bDep && !bRate) continue;
		const double Old = T.LeaveT;
		if (bDep) T.LeaveT += Dep - T.DepT;                                     // (a delayed flight: its run moves with it)
		if (bRate) T.LeaveT -= (DriveS + MarginS) * (Rate - T.RateAt);         // (the drive is real time: longer in sim time when faster)
		T.LeaveT = FMath::Max(T.LeaveT, T0 + 2.0 * 60.0 * Rate);
		ADP_LOG(TEXT("TownHub: coach run %d for %s re-timed (flight moved %+.0f min, sim speed %.1fx) - leaves the hub %+.0f min from before, in %.0f min"), t, *T.Flight, (Dep - T.DepT) / 60.0, Rate, (T.LeaveT - Old) / 60.0, (T.LeaveT - T0) / 60.0 / Rate);
		T.DepT = Dep; T.RateAt = Rate;
		for (FWaiter& W : Waiters) if (W.Trip == t) W.DepT = Dep;
	}
}

void ATownHub::HubIdle(FHubCoach& C)
{
	ACoaches* K = Co(); UVehicleRiders* R = K ? K->Riders(C.Ci) : nullptr; if (!R) return;
	if (C.St == EStage::ToHome)
	{
		if (K->OnRoute(C.Ci) || R->IsDriving()) return;
		C.bAtHub = false; C.Bay = INDEX_NONE; C.St = EStage::Idle; C.T = 0.f; Say(C, TEXT("back at its stand from the hub, free"));
		return;
	}
	if (C.St != EStage::Idle) return;
	const double T0 = Now();
	for (const FTrip& T : Trips) if (!T.bArr && !T.bGone && T.Coach == INDEX_NONE && T.People > 0 && T.LeaveT < T0 + 25.0 * 60.0 * Rate) return;   // (a departure run due soon: it waits here for it)
	if (!R->DriverAtWheel()) return;
	if (K->SendOnRoute(C.Ci, false, INDEX_NONE, FMath::FRandRange(11.5f, 13.9f))) { C.St = EStage::ToHome; C.T = 0.f; Say(C, TEXT("no departure run due soon - drives back to the terminal empty")); }
}

void ATownHub::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bDataOk || NoWhy.Len()) return;
	// (sim seconds a real second: the drive takes real time. Straight from the sim's speed, not measured over frames - at
	//  5x or 10x no run is late from frame time; the runs not yet left are re-timed to it in Retime)
	if (const UADPSim* Sm = UADPSim::Get(this)) Rate = FMath::Clamp(Sm->Speed(), 1.0, 20.0);
	Think -= Dt;
	if (Think > 0.f) return;
	const float Step4 = 0.25f - Think; Think = 0.25f;
	Fleetup(); Dispatch();
	for (FHubCoach& C : Fleet) { if (C.Trip == INDEX_NONE && C.bAtHub) HubIdle(C); else Step(C, Step4); }
	// those who missed every run: home, gone where nobody sees
	APaxFlow* PF = APaxFlow::Find(GetWorld());
	for (FWaiter& W : Waiters)
	{
		// (still at the shelter when their run's coach has shut: missed it)
		if (W.Phase == 0 && Trips.IsValidIndex(W.Trip) && (Trips[W.Trip].bGone || (Fleet.IsValidIndex(Trips[W.Trip].Coach) && Fleet[Trips[W.Trip].Coach].St > EStage::HubBags)))
			Missed(W, TEXT("came to the shelter after their coach had shut"));
		if (W.Phase != 8) continue;
		AActor* A = W.A.Get(); if (!A) { W.Phase = 7; continue; }
		FVector F = A->GetActorLocation(); F.Z = GroundAt(F);
		if (ADPUnseen::May(GetWorld(), F, 185.f, 35.f, A)) { W.Phase = 7; if (PF) PF->HubGone(A, TEXT("missed every town coach - gone home, unseen")); }
	}
	Waiters.RemoveAll([](const FWaiter& W) { return W.Phase == 7; });
}
