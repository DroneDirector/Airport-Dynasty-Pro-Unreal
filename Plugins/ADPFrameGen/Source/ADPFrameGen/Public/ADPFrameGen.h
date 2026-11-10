// Airport Dynasty Pro's own frame generation, for the game's Settings and frame-rate box.
#pragma once

#include "CoreMinimal.h"

namespace ADPFrameGen
{
	/** Running now (switched on in Settings at the last start, and working on this PC). */
	ADPFRAMEGEN_API bool Enabled();
	/** Pictures shown for each frame drawn right now (2 while making in-between frames, else 1). */
	ADPFRAMEGEN_API int32 Multiplier();
}
