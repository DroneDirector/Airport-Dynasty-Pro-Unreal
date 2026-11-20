// The simulation core - see Sim/ADPSim.h.
#include "Sim/ADPSim.h"
#include "ADPLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonObjectConverter.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPFlightsModule.h"
#include "Sim/ADPWeather.h"
#include "Containers/Ticker.h"

// -SimSelfTest: a test module (every 60 s, 2 days of catch-up), the career "closed" 3 days ago - checks the catch-up
// stops at the module's limit, runs in steps without stalling, then goes live; logs the counts and closes the game
namespace
{
	struct FSelfTest : public IADPSimModule
	{
		int64 Steps = 0, Live = 0; FDateTime First, Last; double WorstMs = 0.0;
		virtual FName Name() const override { return TEXT("SelfTest"); }
		virtual double Interval() const override { return 60.0; }
		virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(2.0); }
		virtual void Advance(UADPSim&, const FDateTime& From, const FDateTime& To, bool bCatchUp) override
		{ if (!Steps) First = From; ++Steps; if (!bCatchUp) ++Live; Last = To; }
	};
	TSharedPtr<FSelfTest> GSelfTest; double GSelfLiveAt = 0.0, GSelfWorst = 0.0;
}

UADPSim* UADPSim::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UADPSim>() : nullptr;
}

void UADPSim::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ClockBaseSim = ClockBaseReal = FDateTime::UtcNow();
	ADP_LOG(TEXT("Sim: the simulation core is up (real clock, speed %.1f)"), SpeedFactor);
	ADPWorldData::Load();   // (the airports and aircraft types, on a background thread)
	ADPWeather::Start();    // (the real world's weather, every 15 minutes)
	if (FParse::Param(FCommandLine::Get(), TEXT("SimFlyTest")))
	{
		// a small airline flying two routes from Vagar, the career "closed" 2 days ago: the catch-up flies those 2 days
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float) -> bool
		{
			static TSharedPtr<FADPFlightsModule> M; static double Live = 0.0;
			if (!ADPWorldData::IsReady()) return true;
			if (!M)
			{
				FADPAirline Me; Me.Id = TEXT("me"); Me.Name = TEXT("Test Air"); Me.bPlayer = true; World.Airlines.Add(Me);
				const TCHAR* Regs[] = {TEXT("OY-RCA"), TEXT("OY-RCB")};
				const TArray<TArray<FString>> Stops = {{TEXT("EKVG"), TEXT("EKCH")}, {TEXT("EKVG"), TEXT("BIKF"), TEXT("EGLL"), TEXT("BIKF")}};
				for (int32 i = 0; i < 2; ++i)
				{
					FADPPerson P; P.Id = FString::Printf(TEXT("pilot%d"), i); P.Name = FString::Printf(TEXT("Captain %d"), i); P.Role = TEXT("captain"); P.AirlineId = TEXT("me"); P.Types.Add(TEXT("A320")); World.People.Add(P);
					FADPRoute R; R.Id = FString::Printf(TEXT("r%d"), i); R.AirlineId = TEXT("me"); R.Stops = Stops[i]; R.TypeId = TEXT("A320"); R.PilotId = P.Id; World.Routes.Add(R);
					FADPAircraft A; A.Reg = Regs[i]; A.TypeId = TEXT("A320"); A.AirlineId = TEXT("me"); A.Base = A.Location = TEXT("EKVG"); A.RouteId = R.Id; World.Aircraft.Add(A);
				}
				M = MakeShared<FADPFlightsModule>(); Register(M.ToSharedRef());
				Start(Now() - FTimespan::FromDays(2.0));
				return true;
			}
			if (bCatchingUp) return true;
			if (Live == 0.0) { Live = FPlatformTime::Seconds(); return true; }
			if (FPlatformTime::Seconds() - Live < 20.0) return true;   // (a little live flying too)
			ADP_LOG(TEXT("SimFlyTest: 2 days flown - %lld departures, %lld landings, %lld delayed 15+ min, %lld pilot rests, %d in the air now, %d notices"),
				M->Departures, M->Landings, M->Delayed, M->Rests, World.Flights.Num(), World.Notices.Num());
			for (const FADPAircraft& A : World.Aircraft) ADP_LOG(TEXT("SimFlyTest: %s at %s, %s, %.1f h, %d cycles"), *A.Reg, *A.Location, *A.Status, A.HoursMin / 60.0, A.Cycles);
			for (const FADPFlight& F : World.Flights) ADP_LOG(TEXT("SimFlyTest: in the air %s %s-%s: %s, %.0f ft, %.0f kt, at %.2f %.2f, heading %.0f"), *F.Reg, *F.From, *F.To, *F.Phase, F.AltFt, F.GsKt, F.Lat, F.Lon, F.HeadingDeg);
			for (const TCHAR* Ap : {TEXT("EKVG"), TEXT("BIKF"), TEXT("EGLL"), TEXT("EKCH")}) if (const FADPAirportInfo* I = ADPWorldData::Airport(Ap))
			{ const FADPWx X = ADPWeather::At(Ap, I->Lat, I->Lon, Now()); ADP_LOG(TEXT("SimFlyTest: weather %s %s: wind %03.0f/%.0f kt gust %.0f, vis %.0f m, ceiling %.0f ft, %.0f C%s%s - %s"), Ap, X.bLive ? TEXT("LIVE") : TEXT("climate"), X.WindDir, X.WindKt, X.GustKt, X.VisM, X.CeilingFt, X.TempC, X.bSnow ? TEXT(" snow") : TEXT(""), X.bStorm ? TEXT(" storm") : TEXT(""), *X.Raw); }
			ADP_LOG(TEXT("SimFlyTest: %d live reports; weather waits %lld, de-icings %lld"), ADPWeather::NumReports(), M->WxWaits, M->DeIcings);
			ADP_LOG(TEXT("SimFlyTest: %s"), M->Landings >= 8 && M->Rests >= 1 ? TEXT("PASS") : TEXT("FAIL"));
			FPlatformMisc::RequestExit(false);
			return false;
		}), 0.2f);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("SimProfileTest")))
	{
		// flight plans for known routes, compared by eye with real block times (logged), then the game closes
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) -> bool
		{
			if (!ADPWorldData::IsReady()) return true;
			struct FCase { const TCHAR* T; const TCHAR* A; const TCHAR* B; const TCHAR* Real; };
			const FCase Cases[] = {{TEXT("A320"), TEXT("EKVG"), TEXT("EKCH"), TEXT("about 2 h 25")}, {TEXT("A320"), TEXT("BIKF"), TEXT("EGLL"), TEXT("about 3 h 00")},
				{TEXT("B789"), TEXT("BIKF"), TEXT("KJFK"), TEXT("about 5 h 50 (westbound)")}, {TEXT("AT76"), TEXT("EKVG"), TEXT("BIRK"), TEXT("about 1 h 40")},
				{TEXT("A388"), TEXT("EGLL"), TEXT("OMDB"), TEXT("about 6 h 55")}, {TEXT("DHC6"), TEXT("BIRK"), TEXT("BIVM"), TEXT("about 30 min")}};
			for (const FCase& C : Cases)
			{
				FADPProfile P;
				if (!ADPWorldData::MakeProfile(C.T, C.A, C.B, P)) { ADP_LOG(TEXT("SimProfileTest: %s %s-%s unknown"), C.T, C.A, C.B); continue; }
				const FADPProfile::FState Mid = P.At(P.BlockMin() / 2);
				ADP_LOG(TEXT("SimProfileTest: %s %s-%s %.0f nm, FL%03.0f, block %d h %02d (real %s); half way: %s, %.0f ft, %.0f kt, %.2f %.2f, heading %.0f"),
					C.T, C.A, C.B, P.DistNm, P.CruiseFt / 100.0, int32(P.BlockMin()) / 60, int32(P.BlockMin()) % 60, C.Real, FADPProfile::PhaseName(Mid.Phase), Mid.AltFt, Mid.GsKt, Mid.Lat, Mid.Lon, Mid.HeadingDeg);
			}
			const FADPAirportInfo* V = ADPWorldData::Airport(TEXT("EKVG"));
			ADP_LOG(TEXT("SimProfileTest: %d airports; Vagar %s, %d runway(s), longest %d ft"), ADPWorldData::NumAirports(), V ? *V->Name : TEXT("missing"), V ? V->Runways.Num() : 0, V ? V->LongestFt() : 0);
			FPlatformMisc::RequestExit(false);
			return false;
		}), 0.5f);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("SimSaveTest")))
	{
		// a small world written, cleared, read back and compared, then removed
		FADPAirline A; A.Id = TEXT("me"); A.Name = TEXT("Test Air"); A.bPlayer = true; A.Balance = 1234567.5; World.Airlines.Add(A);
		for (int32 i = 0; i < 50; ++i) { FADPPerson P; P.Id = FString::Printf(TEXT("p%d"), i); P.Name = FString::Printf(TEXT("Person %d"), i); P.Role = i % 2 ? TEXT("fa") : TEXT("captain"); P.Types.Add(TEXT("A320")); World.People.Add(P); }
		FADPAircraft C; C.Reg = TEXT("TF-ABC"); C.TypeId = TEXT("A320"); C.Condition = 87.25; World.Aircraft.Add(C);
		FADPTxn T; T.At = FDateTime(2026, 11, 20, 8, 0, 0); T.Kind = TEXT("revenue"); T.Amount = 9999.99; World.Ledger.Add(T);
		const bool bSaved = SaveCareer(TEXT("selftest"));
		World = FADPWorld();
		const bool bLoaded = LoadCareer(TEXT("selftest"));
		const bool bSame = bSaved && bLoaded && World.Airlines.Num() == 1 && World.Airlines[0].Balance == 1234567.5 && World.People.Num() == 50 && World.People[49].Types.Num() == 1
			&& World.Aircraft.Num() == 1 && World.Aircraft[0].Condition == 87.25 && World.Ledger.Num() == 1 && World.Ledger[0].At == FDateTime(2026, 11, 20, 8, 0, 0) && CareerSlots().Contains(TEXT("selftest"));
		ADP_LOG(TEXT("SimSaveTest: saved %d, loaded %d, the same %d (%lld bytes) - %s"), bSaved, bLoaded, bSame, IFileManager::Get().FileSize(*CareerFile(TEXT("selftest"))), bSame ? TEXT("PASS") : TEXT("FAIL"));
		IFileManager::Get().Delete(*CareerFile(TEXT("selftest")));
		World = FADPWorld();
		FPlatformMisc::RequestExit(false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("SimSelfTest")))
	{
		GSelfTest = MakeShared<FSelfTest>(); Register(GSelfTest.ToSharedRef());
		Start(Now() - FTimespan::FromDays(3.0));
	}
}

FString UADPSim::CareerFile(const FString& Slot) { return FPaths::ProjectSavedDir() / TEXT("Careers") / (Slot + TEXT(".json")); }

TArray<FString> UADPSim::CareerSlots()
{
	TArray<FString> Files; IFileManager::Get().FindFiles(Files, *(FPaths::ProjectSavedDir() / TEXT("Careers") / TEXT("*.json")), true, false);
	for (FString& F : Files) F = FPaths::GetBaseFilename(F);
	return Files;
}

bool UADPSim::SaveCareer(const FString& Slot)
{
	World.Slot = Slot; World.SavedAt = FDateTime::UtcNow(); World.SimAt = Now();
	World.ModuleTimes.Reset(); for (const FSlot& S : Slots) World.ModuleTimes.Add(S.M->Name().ToString(), S.At);
	FString Text;
	if (!FJsonObjectConverter::UStructToJsonObjectString(World, Text)) { ADP_WARN(TEXT("Sim: the career couldn't be written")); return false; }
	// (written beside, then moved over the old one: a crash mid-write never leaves a broken save)
	const FString F = CareerFile(Slot), Tmp = F + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(Text, *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) || !IFileManager::Get().Move(*F, *Tmp, true, true))
	{ ADP_WARN(TEXT("Sim: saving %s failed"), *F); return false; }
	return true;
}

bool UADPSim::LoadCareer(const FString& Slot)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *CareerFile(Slot))) return false;
	FADPWorld W;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Text, &W, 0, 0)) { ADP_WARN(TEXT("Sim: the career %s couldn't be read"), *Slot); return false; }
	World = MoveTemp(W);
	ADP_LOG(TEXT("Sim: career %s loaded (version %d, saved %s, %d airlines, %d aircraft, %d people)"), *Slot, World.Version, *World.SavedAt.ToString(), World.Airlines.Num(), World.Aircraft.Num(), World.People.Num());
	return true;
}

void UADPSim::Deinitialize()
{
	if (bRunning && !World.Slot.IsEmpty()) SaveCareer(World.Slot);   // (closing the game saves the career)
	bRunning = false;
	Slots.Reset();
	Super::Deinitialize();
}

FDateTime UADPSim::Now() const
{
	const FTimespan Real = FDateTime::UtcNow() - ClockBaseReal;
	return ClockBaseSim + FTimespan::FromSeconds(Real.GetTotalSeconds() * SpeedFactor);
}

void UADPSim::SetSpeed(double S)
{
	// (re-based so the clock never jumps when the speed changes)
	ClockBaseSim = Now(); ClockBaseReal = FDateTime::UtcNow();
	SpeedFactor = FMath::Clamp(S, 0.0, 1000.0);
	ADP_LOG(TEXT("Sim: speed %.1f"), SpeedFactor);
}

void UADPSim::Register(TSharedRef<IADPSimModule> Module)
{
	for (const FSlot& S : Slots) if (S.M->Name() == Module->Name()) { ADP_WARN(TEXT("Sim: module %s registered twice"), *Module->Name().ToString()); return; }
	FSlot& S = Slots.Add_GetRef(FSlot(Module));
	S.At = Now();
	ADP_LOG(TEXT("Sim: module %s (every %.0f s, catches up %.0f days)"), *Module->Name().ToString(), Module->Interval(), Module->MaxCatchUp().GetTotalDays());
}

FDateTime UADPSim::ModuleTime(FName Module) const
{
	for (const FSlot& S : Slots) if (S.M->Name() == Module) return S.At;
	return FDateTime();
}

void UADPSim::Start(const FDateTime& LastRun)
{
	const FDateTime NowT = Now();
	for (FSlot& S : Slots)
	{
		// (a module catches up no further back than its own limit, as the web game did)
		const FDateTime Oldest = NowT - S.M->MaxCatchUp();
		S.At = LastRun > Oldest ? LastRun : Oldest;
		if (S.At > NowT) S.At = NowT;
	}
	bRunning = true; bCatchingUp = true;
	ADP_LOG(TEXT("Sim: started - catching up %.1f hours since the career was last run"), (NowT - LastRun).GetTotalHours());
}

bool UADPSim::Step(FSlot& S, const FDateTime& To, bool bCatchUp, double Budget)
{
	const double T0 = FPlatformTime::Seconds();
	const FTimespan Iv = FTimespan::FromSeconds(FMath::Max(0.05, S.M->Interval()));
	while (S.At + Iv <= To)
	{
		const FDateTime From = S.At;
		S.At = From + Iv;
		S.M->Advance(*this, From, S.At, bCatchUp);
		if (FPlatformTime::Seconds() - T0 > Budget) return false;   // (more next frame)
	}
	return true;
}

void UADPSim::Tick(float Dt)
{
	if (!bRunning || !bRunsWorld) return;
	if (!World.Slot.IsEmpty() && (AutosaveClock += Dt) >= 30.0) { AutosaveClock = 0.0; SaveCareer(World.Slot); }
	const double TickT0 = FPlatformTime::Seconds();
	const FDateTime NowT = Now();
	// catching up: a few milliseconds of work a frame until every module is at now; then live
	const double Budget = bCatchingUp ? 0.004 : 0.002;
	bool bAllDone = true;
	for (FSlot& S : Slots) bAllDone &= Step(S, NowT, bCatchingUp, Budget / FMath::Max(1, Slots.Num()));
	if (bCatchingUp && bAllDone) { bCatchingUp = false; ADP_LOG(TEXT("Sim: caught up - running live")); if (GSelfTest) GSelfLiveAt = FPlatformTime::Seconds(); }
	if (GSelfTest)
	{
		GSelfWorst = FMath::Max(GSelfWorst, (FPlatformTime::Seconds() - TickT0) * 1000.0);
		if (GSelfLiveAt > 0.0 && FPlatformTime::Seconds() - GSelfLiveAt > 130.0)
		{
			ADP_LOG(TEXT("SimSelfTest: %lld steps (%lld live), from %s to %s (%.2f days back, limit 2), worst frame of sim work %.2f ms - %s"),
				GSelfTest->Steps, GSelfTest->Live, *GSelfTest->First.ToString(), *GSelfTest->Last.ToString(), (NowT - GSelfTest->First).GetTotalDays(), GSelfWorst,
				GSelfTest->Steps >= 2880 && GSelfTest->Live >= 2 && (NowT - GSelfTest->First).GetTotalDays() < 2.01 && GSelfWorst < 8.0 ? TEXT("PASS") : TEXT("FAIL"));
			GSelfTest.Reset(); FPlatformMisc::RequestExit(false);
		}
	}
}
