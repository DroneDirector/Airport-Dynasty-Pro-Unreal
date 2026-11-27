// Staff (docs/AI-Simulation-Plan.md part 6), our own model: people made with names from the airline's part of the world,
// the going wage for each job and region, the hiring pool that changes every few hours, the crews a flight needs, and
// each person's days - sickness, morale (pay above all), and quitting after too long unhappy. Your airline and the AI
// airlines' staff alike. (Training, unions and strikes, conduct and court come in the next steps of part 6.)
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSim.h"

namespace ADPStaff
{
	/** The going weekly wage (USD) for a job in a country (ISO code). */
	double GoingRate(const FString& Role, const FString& Country);
	/** A new person for a job (not yet hired: AirlineId empty). */
	FADPPerson MakePerson(const FString& Role, const FString& Country, FRandomStream& Rng);
	/** Hires someone from the pool for an airline at a base: the first week's wage as the hiring fee. */
	bool Hire(FADPWorld& W, const FString& PersonId, const FString& AirlineId, const FString& Base, const FDateTime& At);
	/** Lets someone go (to-do 176): two weeks' notice working, then gone; severance of two weeks' pay booked now. */
	bool Dismiss(FADPWorld& W, const FString& PersonId, const FDateTime& At);
	/** A new weekly wage (their mood follows it day by day). */
	bool SetPay(FADPWorld& W, const FString& PersonId, double Weekly);
	/** Moves someone to another base (their airline's airports). */
	bool SetBase(FADPWorld& W, const FString& PersonId, const FString& Base);
	/** A new person's story (to-do 179): flight hours, languages, home town, strengths and weak points, the pay they ask for. */
	void Describe(FADPPerson& P, const FString& Country, FRandomStream& Rng);
	/** Each day (to-do 178 / 180): first officers made captains, cabin crew senior crew and pursers, raises asked for,
	 *  passed-over people losing heart, holidays asked for, approved ones taken. */
	void CareerDay(FADPWorld& W, const FDateTime& To, FRandomStream& Rng);
	/** An interview (to-do 179): their strengths and weak points shown; a small cost in time only. */
	bool Interview(FADPWorld& W, const FString& PersonId);
	/** A paid job advert (to-do 179): three better candidates for the job come into the pool. Cost by job. */
	bool Advert(FADPWorld& W, const FString& AirlineId, const FString& Role, const FDateTime& At, FRandomStream& Rng);
	double AdvertCost(const FString& Role);
	/** Full or part time (to-do 180). */
	bool SetContract(FADPWorld& W, const FString& PersonId, const FString& Contract);
	/** A holiday request answered (to-do 180). */
	bool AnswerLeave(FADPWorld& W, int32 Index, bool bYes, const FDateTime& At);
	/** Fills the hiring pool at once (a new career: people looking for work from the first minute). */
	void FillPool(FADPWorld& W, FRandomStream& Rng);
	/** People looking for this job at a time: 30-40 (owner 2026-11-27). */
	int32 PoolSize(const FString& Role);
	/** Cabin crew a type needs (1 per 50 seats from 20 seats), pilots 2. */
	int32 CabinCrewFor(int32 Seats);
	/** On-duty cabin crew of an airline at a base (not sick, not on leave). */
	int32 CabinCrewAt(const FADPWorld& W, const FString& AirlineId, const FString& Base);
	/** The airline's country (its hub airport's) - never fixed to one place (the airport can move). */
	FString CountryOf(const FADPWorld& W, const FString& AirlineId);
	/** A type-rating course: cost by class, days; passed or failed at the end (a retake costs a third). */
	bool StartTypeRating(FADPWorld& W, const FString& PersonId, const FString& TypeId, const FDateTime& At);
	/** True while this airline's staff in this job are on strike. */
	bool OnStrike(const FADPWorld& W, const FString& AirlineId, const FString& Role);
	/** The player's answer to a union: meet the demands (pay to the going rate +5%), or a compromise (+half way). */
	void MeetUnion(FADPWorld& W, const FString& AirlineId, const FString& Role, bool bFully, const FDateTime& At);
}

class AIRPORTDYNASTYPRO_API FADPStaffModule : public IADPSimModule
{
public:
	virtual FName Name() const override { return TEXT("Staff"); }
	virtual double Interval() const override { return 3600.0; }   // (every hour; the day's events at 00:00 UTC)
	virtual FTimespan MaxCatchUp() const override { return FTimespan::FromDays(56.0); }
	virtual void Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp) override;
	int32 SickDays = 0, Quits = 0, PoolChanges = 0, Ratings = 0, Strikes = 0, Reports = 0, Trials = 0;
	FRandomStream Rng = FRandomStream(2601);
private:
	void Daily(FADPWorld& W, const FDateTime& To);
};
