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
	/** Advances one module up to To in steps of its interval (at most Budget seconds of real work this call). */
	bool Step(FSlot& S, const FDateTime& To, bool bCatchUp, double Budget);
};
