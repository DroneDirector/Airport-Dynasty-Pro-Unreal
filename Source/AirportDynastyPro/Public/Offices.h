// The old offices' things in the terminal's offices (owner, 2026-11-11): never the old room itself, its walls, pictures or
// shelves, only what was in it (the walnut desk with the laptop, monitor, banker's lamp, phone, notepad, coffee, reports,
// nameplate and the model aircraft, the chair, the sofa and table, the globe, the rug), exported from the Babylon edition
// exactly (Tools/BabylonExport ?m=office) and set in each office with the desk's back to the outer wall, facing the door.
// Office 1 is yours: your pet asleep in its bed with its bowls (dog or cat, as chosen; none for none). The same for the
// operations room (?m=ops: the desk, the ultra-wide monitor, keyboard, mouse, folders, chair) and the Academy
// (?m=academy: the whiteboard, the instructor's desk and chair, the class's desks and chairs and what is on them, plants).
#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace ADPOffices
{
	/** Place the four offices', the operations room's and the Academy's things (and in Office 1 the pet: "dog", "cat" or "none"). */
	void Place(UWorld* W, const FString& Pet);
}
