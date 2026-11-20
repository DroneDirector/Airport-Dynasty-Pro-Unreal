// Events, jobs, reputation, ranks, badges, achievements, news, notifications, mail and the morning briefing
// (docs/AI-Simulation-Plan.md part 8), our own model:
// - world events: volcanic ash, ATC strikes (announced two days ahead), big events in a city (a final, a festival),
//   health scares - they close airports or move demand, and the flights module and demand obey them;
// - the jobs board: charters, cargo and medical flights near your hub, flown by one of your aircraft, paid on arrival;
// - reputation for every airline, from punctuality, quality and incidents (it moves demand a little);
// - your career: XP and ranks, 1,000+ badges in families and tiers (every country and large airport too), 22 achievements;
// - notifications with priorities, the news feed, mail and phone messages, gifts, and a morning briefing at 06:00 at your hub.
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"

struct FADPAirportInfo;

namespace ADPEvents
{
	/** A notification (Priority 0 info, 1 important, 2 urgent); the list is kept to the newest 400. */
	void Notify(FADPWorld& W, const FDateTime& At, const FString& Kind, int32 Priority, const FString& Title, const FString& Text);
	/** A line in the news feed (kept to the newest 300). */
	void News(FADPWorld& W, const FDateTime& At, const FString& Topic, const FString& Headline);
	/** How much the world's events move demand on a pair now (1 = not at all). */
	double DemandFactor(const FADPWorld& W, const FADPAirportInfo& A, const FADPAirportInfo& B, const FDateTime& At);
	/** The event closing this airport now, if any. */
	const FADPWorldEvent* ClosedBy(const FADPWorld& W, const FADPAirportInfo& Ap, const FDateTime& At);
	/** Reputation's pull on travellers (0.75 at 0, 1 at 50, 1.25 at 100). */
	double RepFactor(const FADPAirline* A);
	/** Takes a job off the board with one of your parked aircraft (and its pilot): it flies there, then back to its route. */
	bool TakeJob(FADPWorld& W, const FString& JobId, const FString& Reg, const FDateTime& At);
	/** A job's leg: its load instead of the market's (true if the route is a job's). */
	bool JobLoad(const FADPWorld& W, const FString& RouteId, const FString& To, FADPFlight& F);
	/** After every landing: punctuality, reputation, the job paid, your career (XP, badges, achievements). */
	void OnLanded(FADPWorld& W, const FADPFlight& F, const FDateTime& At);
	/** The morning briefing's text for the player. */
	FString Briefing(const FADPWorld& W, const FDateTime& At);
	FString RankName(int32 Rank);
	/** How many badges there are in all. */
	int32 NumBadges();
}

class AIRPORTDYNASTYPRO_API FADPEventsModule : public IADPSimModule
{
public:
	virtual FName Name() const override { return TEXT("Events"); }
	virtual double Interval() const override { return 3600.0; }
	virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(14.0); }
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) override;
	/** Posts one job on the board near the player's hub (returns its id). */
	FString PostJob(FADPWorld& W, const FDateTime& At);
	/** Starts a world event of a kind (volcano, strike, festival, health) - used by the hourly roll and the tests. */
	void StartEvent(FADPWorld& W, const FString& Kind, const FDateTime& At);
	int32 WorldEvents = 0, JobsPosted = 0, JobsExpired = 0, Briefings = 0, Gifts = 0;
	FRandomStream Rng = FRandomStream(8080);
private:
	void Hourly(FADPWorld& W, const FDateTime& At, bool bCatchUp);
	void Daily(FADPWorld& W, const FDateTime& At);
};
