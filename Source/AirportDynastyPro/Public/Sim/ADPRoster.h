// Crew rostering on the real EASA flight time limitations (to-do 177; owner, 2026-11-20 night: "yes, the real rules").
// From EU regulation 965/2012 ORO.FTL (simplified where noted):
//  - the maximum flight duty period (FDP) for an acclimatised crew: 13 h when it starts 06:00-13:29 local, less for
//    starts later in the day and early in the morning (down to 11 h), 30 min less for each sector after the second,
//    never under 9 h (ORO.FTL.205 table 2);
//  - rest before a duty at least as long as the duty before it: at home base at least 12 h, away at least 10 h
//    (ORO.FTL.235);
//  - flight time at most 100 h in any 28 days and 900 h in a calendar year (here: the last 52 weeks), (ORO.FTL.210);
//  - days off: at least 36 h including two local nights every 168 h - here two days off after six days with duty;
//  - two pilots on every airliner (captain and first officer), cabin crew one per 50 seats from 20 seats (ORO.CC.100);
//  - a flight leaves only with its whole crew rostered; a missing crew member delays it.
// Your airline's crews are rostered this way; the AI airlines keep their simpler single-crew rule.
#pragma once

#include "CoreMinimal.h"
#include "Sim/ADPSimData.h"

namespace ADPRoster
{
	/** The maximum flight duty period (minutes) for a duty starting at this local hour:minute with this many sectors. */
	double MaxFdpMin(int32 LocalHour, int32 LocalMinute, int32 Sectors);
	/** The local time at an airport (its longitude; the site's own clock at our airport). */
	FDateTime LocalAt(const FString& Icao, const FDateTime& Utc);
	/** Flight minutes in the last 28 days / 52 weeks. */
	double Fly28(const FADPPerson& P);
	double FlyYear(const FADPPerson& P);
	/** Can this person fly a leg of LegMin minutes from Where now (rest, days off, limits, type, status)? Why says why not. */
	bool CanFly(const FADPWorld& W, const FADPPerson& P, const FString& TypeId, const FString& Where, double LegMin, const FDateTime& Now, FString& Why);
	/** Makes sure the aircraft has a full legal crew for its next leg: keeps the one on board if it may go on, else
	 *  releases it to rest and rosters a fresh crew from the people at that airport. False: none - Why says who is missing. */
	bool Crew(FADPWorld& W, FADPAircraft& A, double LegMin, int32 CabinNeeded, const FString& PreferCaptain, const FDateTime& Now, FString& Why);
	/** After a leg: flight time on each crew member's record, the duty goes on. */
	void Landed(FADPWorld& W, const FADPFlight& F, const FDateTime& At);
	/** Ends the duty of the aircraft's crew (rest by the rules, days off when due). */
	void Release(FADPWorld& W, FADPAircraft& A, const FDateTime& At);
	/** Days off asked for in the Overview (two days from the next end of duty, or now if off duty). */
	bool GiveDaysOff(FADPWorld& W, const FString& PersonId, int32 Days, const FDateTime& At);
	/** Day number since 2000-01-01 (UTC). */
	int32 DayOf(const FDateTime& T);
}
