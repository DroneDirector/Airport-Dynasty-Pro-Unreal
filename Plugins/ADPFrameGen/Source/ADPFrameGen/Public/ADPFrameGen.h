// Airport Dynasty Pro's own frame generation, for the game's Settings and frame-rate box.
#pragma once

#include "CoreMinimal.h"

namespace ADPFrameGen
{
	/** Making in-between pictures now (in place and switched on). */
	ADPFRAMEGEN_API bool Enabled();
	/** In place on the display chain (to-do 111, owner 2026-11-18: switched on and off live, no restart): put in at
	 *  every start unless AMD's frame generation was chosen then; while it's switched off every frame goes straight
	 *  through, as drawn. */
	ADPFRAMEGEN_API bool Installed();
	/** Switch it on or off now (works when Installed()). */
	ADPFRAMEGEN_API void SetActive(bool bOn);
	/** Pictures shown for each frame drawn right now (2 while making in-between frames, else 1). */
	ADPFRAMEGEN_API int32 Multiplier();
	/** A menu, Settings, the map or a device screen is up (owner, 2026-11-17: scrolling in the menus flickered): every
	 *  frame shown as it is drawn, no in-between pictures, while it's held. */
	ADPFRAMEGEN_API void Hold(bool bHold);
}
