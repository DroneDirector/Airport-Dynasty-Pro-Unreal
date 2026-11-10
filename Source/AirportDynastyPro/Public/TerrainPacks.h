// The world's terrain for the PC version: downloaded once as region packs (Tools/PackTerrain.py, published as the
// terrain release) into the player's own folder (%LOCALAPPDATA%\AirportDynastyPro\Terrain: Program Files can't be
// written to), checked against the manifest's checksums, and used by the globe from the next start. An update
// downloads only the packs whose checksum changed. Nothing streams while you play.
#pragma once

#include "CoreMinimal.h"

namespace ADPTerrain
{
	/** Where the terrain lives on this PC. */
	FString Dir();
	/** Every pack installed (the globe uses the terrain). */
	bool Ready();
	/** Start the download (or the update) in the background; safe to call again while it runs. */
	void Start();
	/** What it is doing, for Settings: e.g. "Downloading the world's terrain: pack 12 of 129, 0.8 of 6.6 GB". */
	FString Status();
	/** Running now. */
	bool Busy();
	/** Settings' "Move": the player picks a folder (another drive with room, say) and the terrain moves there; the
	 *  game uses it from then on (its place is kept in %LOCALAPPDATA%\AirportDynastyPro\terrain-location.txt). */
	void ChooseAndMove();
}
