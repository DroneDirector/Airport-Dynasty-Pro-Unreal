// The "who are you" screen of a new career (owner, 2026-11-12), first as a concept to look at: a dark room with two
// tall neon tubes, fog over the floor lit by the neon, a woman at the left and a man at the right; the one under the
// cursor stands in the light, the other in shadow; picking one goes on to the character creator (32a/32b on the
// list), where the fog clears in a circle round you once you come to the shoes. Colours as the title screen's.
#pragma once

#include "CoreMinimal.h"

namespace ADPCharacterCreate
{
	/** Builds the concept room under the airport, the woman (bWoman) or the man in the light, and puts the camera on it. */
	void BuildConcept(UWorld* W, class APlayerController* PC, bool bWoman);
	/** The choice moved: she (true) or he steps into the light, the other back into the shadow. */
	void Choose(bool bWoman);
}
