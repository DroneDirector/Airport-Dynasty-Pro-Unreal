// The airport's site on the Earth (owner, 2026-11-20: the player chooses where to be - the Iceland airport or Chablais
// Airport in Switzerland; the airport's layout is the same everywhere). A site is a folder Content/Data/Sites/<id>/ with:
//   site.json   - the airport block (icao, iata, name, lat, lon, elevFt, hdg, magvar, country, word), the time zone, the
//                 runway names, the credit line, the flattened ground box
//   and its own ground data: terrain.json, roads.json, road-ground.json, roads-baked.json, town-buildings.json,
//   ocean.bin, grass-area.*, grass-tiles/, land-corridor*  (any one missing: that part is left out there)
// The default site (iceland) keeps today's files in Content/Data/ (and the project's Data/).
// The site is fixed when the map loads: -Site=<id> on the command line, ?Site=<id> in the map's options, else the one
// chosen last (GameUserSettings [ADP.Site] Site). Changing it reloads the map.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace ADPSite
{
	/** The default site, whose data lives directly in Content/Data. */
	inline const TCHAR* DefaultId() { return TEXT("iceland"); }
	/** Fixes this run's site from the map's options (GameMode::InitGame calls it first). */
	void Begin(const FString& MapOptions);
	/** This run's site. */
	const FString& Id();
	bool IsDefault();
	/** Every site the game has (the default and each Content/Data/Sites/<id>/site.json). */
	TArray<FString> All();
	/** A site's own name and place ("Chablais Airport", "Switzerland"). */
	FString Name(const FString& Id);
	FString Place(const FString& Id);
	/** Chooses the site for the next start, saved. */
	void Choose(const FString& Id);
	/** A data file belonging to the site (its ground, roads, town...): the default site's in Content/Data (or the project's
	 *  Data/), another site's only in its own folder (so nothing of another place is ever drawn there). */
	FString File(const FString& Name);
	/** A site's site.json (null for the default). */
	TSharedPtr<FJsonObject> Info(const FString& Id);
	/** The airport block of airfield.json with this site's own values over it (name, lat/lon, heading...). */
	TSharedPtr<FJsonObject> AirportBlock();
	/** The local time now at the site (its UTC offset; EU summer time where the site says so). */
	FDateTime LocalNow();
	/** The sample destinations on the boards and in the calls (the site's own, else the Iceland set): names and IATA codes. */
	void Destinations(TArray<FString>& Names, TArray<FString>& Codes);
	double UtcOffsetHours(const FDateTime& Utc);
}
