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
