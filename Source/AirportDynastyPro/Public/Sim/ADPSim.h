// The game's simulation core (owner, 2026-11-20, to-do 154; docs/AI-Simulation-Plan.md part 1): everything the game
// simulates by itself - flights, rival airlines, passengers and demand, the economy, staff, events - runs here, in C++,
// built from scratch (the Babylon web game is only the reference for what exists, never ported). Nothing here draws:
// the Overview screens and the 3D world read it and send it the player's decisions.
//
// The clock: the real clock, as the web game had (a career lives in real time), with a speed factor kept for later.
// Each module advances at its own rate and, when a career is opened again, catches up on the time it was closed - up to
// its own limit (flights 14 days, pay 8 weeks, the bank 12 months), in steps so a long absence never stalls a frame.
// Only the device that runs the world advances it (bRunsWorld); a mirror (the other device, for the sync) only shows it.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Sim/ADPSimData.h"
#include "ADPSim.generated.h"

class UADPSim;

/** One part of the simulation: how often it advances, how far back it catches up, and the step itself. */
class AIRPORTDYNASTYPRO_API IADPSimModule
{
public:
	virtual ~IADPSimModule() = default;
	virtual FName Name() const = 0;
	/** Seconds of simulation time between its updates. */
	virtual double Interval() const = 0;
	/** The longest stretch it catches up on after the career was closed (older time is skipped, as the web game did). */
	virtual FTimespan MaxCatchUp() const = 0;
	/** Advance from From to To (simulation time, UTC). bCatchUp: replaying time the game was closed (no pop-ups, voices). */
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) = 0;
};

UCLASS()
class AIRPORTDYNASTYPRO_API UADPSim : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	static UADPSim* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- the clock
	/** Simulation time now (UTC). */
	FDateTime Now() const;
	double Speed() const { return SpeedFactor; }
	void SetSpeed(double S);
	/** Tests: the clock set to a time (the modules go on from it). */
	void SetClock(const FDateTime& To);
	/** True on the device that runs the world (the other only shows it). */
	bool RunsWorld() const { return bRunsWorld; }
	void SetRunsWorld(bool bOn) { bRunsWorld = bOn; }

	// ---- the modules
	void Register(TSharedRef<IADPSimModule> Module);
	/** Starts the clock for a career: catches every module up from LastRun (simulation time) to now, then runs live. */
	void Start(const FDateTime& LastRun);
	void Stop() { bRunning = false; }
	bool IsRunning() const { return bRunning; }
	/** Where each module has got to (simulation time) - saved with the career. */
	FDateTime ModuleTime(FName Module) const;

	// ---- your career (to-do 173): New game makes it at the airport you chose, Continue / Load picks it up
	/** A new career: your airline based at the site's airport, money by difficulty (no aircraft, no crew: you buy them),
	 * the AI airlines founded, the hiring pool and the markets filled; then it runs on the real clock and autosaves. */
	void NewCareer(const FString& Difficulty, const FString& AirlineName = FString());
	/** Continue / Load: this site's career read back and caught up on the time it was closed (false: none yet - a new one is made). */
	bool ResumeCareer();
	/** Save now (to-do 192): the whole career into a named slot of its own (Saved/Careers/<site>~<name>.json) with a
	 *  little card beside it (.meta.json: its name, the date, the airport, your money) - the autosave goes on as before. */
	bool SaveNamed(const FString& Name, FString& OutSlot);
	/** The named saves of every airport, newest first: (slot, name, saved, site, airport, money) as JSON lines. */
	static TArray<TSharedPtr<class FJsonObject>> NamedSaves();
	/** Loads a named save into this airport's career (the same site only; false: another site or unreadable). */
	bool LoadNamed(const FString& Slot);
	/** A named save chosen at the title screen: loaded when the game starts (instead of the airport's own career). */
	FString PendingLoad;
	/** One career per airport: Saved/Careers/career-<site>.json. */
	static FString SiteSlot();
	/** Money a new career starts with (USD) - easy a lot, normal a little more freedom, hard exactly one used A320-class
	 * aircraft, its crew for a few weeks and some spare (owner's new-game economy rules, 2026-11-12). */
	static double StartingMoney(const FString& Difficulty);
	/** The player's airline id. */
	static const TCHAR* Me() { return TEXT("me"); }

	// ---- the world and its save (Saved/Careers/<slot>.json, versioned; autosaved every 30 s while running)
	FADPWorld World;
	bool SaveCareer(const FString& Slot);
	bool LoadCareer(const FString& Slot);
	static TArray<FString> CareerSlots();
	static FString CareerFile(const FString& Slot);

	// FTickableGameObject
	virtual void Tick(float Dt) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UADPSim, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bRunning; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FSlot { TSharedRef<IADPSimModule> M; FDateTime At; double Behind = 0.0; FSlot(TSharedRef<IADPSimModule> In) : M(In) {} };
	TArray<FSlot> Slots;
	bool bRunning = false, bRunsWorld = true;
	double SpeedFactor = 1.0;
	FDateTime ClockBaseSim, ClockBaseReal;   // Now() = ClockBaseSim + (real now - ClockBaseReal) x speed
	bool bCatchingUp = false;
	double AutosaveClock = 0.0;
	/** Advances one module up to To in steps of its interval (at most Budget seconds of real work this call). */
	bool Step(FSlot& S, const FDateTime& To, bool bCatchUp, double Budget);
	/** Every game module registered once (flights, economy, staff, rivals, events). */
	void AddGameModules();
	/** -SimCareerTest (ADPCareer.cpp). */
	void CareerSelfTest();
};
