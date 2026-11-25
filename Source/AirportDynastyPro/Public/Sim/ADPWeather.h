// The real world's weather for the simulation (owner, 2026-11-20: the real clock and the real weather, as it is in the
// world right now; docs/AI-Simulation-Plan.md part 3b). Live: the official worldwide airport reports (METARs) from
// aviationweather.gov (US government, public domain), the whole world in one ~250 KB file, fetched every 15 minutes (like Flight Simulator's live weather) -
// live weather can't be built in, so the game fetches it (the owner was told). Offline, or an airport with no report
// near it: our built-in climate - typical wind, cloud, temperature, rain and snow for its latitude, season and hour.
#pragma once

#include "CoreMinimal.h"

struct FADPWx
{
	double WindDir = 0, WindKt = 0, GustKt = 0, VisM = 10000, CeilingFt = 99999, TempC = 15;
	bool bStorm = false, bSnow = false, bFreezing = false, bFog = false, bRain = false;
	bool bLive = false;          // from a real report (else the built-in climate)
	FString Station, Raw;        // the report used and its text
};

namespace ADPWeather
{
	/** Starts the live fetching (every 15 minutes); safe to call again. */
	void Start();
	/** The weather at an airport now: its own report, the nearest within 60 nm, or the built-in climate. */
	FADPWx At(const FString& Icao, double Lat, double Lon, const FDateTime& When);
	/** The menus' weather question (the address they ask: /api/data/metar or taf, ids= or bbox=) answered from the reports
	 *  already here, as the weather service itself would (JSON); false: not known, or nothing here yet. Any thread. */
	bool PageJson(const FString& Url, FString& Out);
	int32 NumReports();
	FDateTime FetchedAt();
	/** The crosswind and tailwind (kt) on a runway heading, gusts included. */
	void Components(const FADPWx& W, double RunwayHdg, double& Cross, double& Tail);
}
