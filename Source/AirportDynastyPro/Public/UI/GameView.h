// The airline game itself (the Android edition's game code: the Overview, fleet, crew, money, timetables, every menu
// and screen, and its cloud sync) running inside the PC version in Unreal's web browser view. The same code as on the
// phone, so the rules never drift apart, and through its cloud sync the PC joins the same world: one device runs it,
// the other shows it live. Its files are served from a local secure address (https://game.adp.local/), as the sync's
// encryption needs: in a packaged game from Content/Web (the build puts the Android edition's built www/ there), in the
// editor from Airline-Manager-Pro-Babylon/www once it has been built.
#pragma once

#include "CoreMinimal.h"

class SWidget;

namespace ADPGameView
{
	/** The game's page in a web view, or a note saying how to get it when it hasn't been built here. */
	TSharedRef<SWidget> Make();
	/** Where the game's built files are (empty when they aren't there). */
	FString Folder();
}
