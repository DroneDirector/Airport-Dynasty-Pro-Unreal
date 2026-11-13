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
	/** The choice made: the other walks out of the picture, then the tubes fade and go back into the fog (the camera still). */
	void Confirm(bool bWoman);
	/** The transition over: the chosen one alone, the creator's UI goes on the free side. */
	bool Done();
	/** She chosen (left) -> the UI on the right; he chosen (right) -> the UI on the left. */
	bool UiOnRight();
	/** Back from the first panel: the tubes come back out of the fog, the other walks back in to their mark. */
	void Back();
	/** Back at the choice (after Back's reverse, or before any choice). */
	bool Choosing();
	/** Before any choice: nobody in the key light. */
	void Nobody();
	/** The stage taken down: everything it spawned gone, the world's fog as it was, the camera back on you. */
	void Teardown(UWorld* W, class APlayerController* PC);
	/** Your partner walks in from the free side's edge and stops beside you (or walks back out). */
	void ShowPartner(bool bOn);
	/** Your pet walks in and sits at your feet (Kind dog or cat, its coat colour); anything else: it walks out. */
	void ShowPet(const FString& Kind, const FLinearColor& Coat);
	/** Partner and pet walk out (back to the choice). */
	void ClearCompany();
}
