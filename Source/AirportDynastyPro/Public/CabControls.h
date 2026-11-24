// The cab controls every apron service vehicle shares (to-do 238): one set of lines added to each vehicle's own table
// (VehicleControls.h). A line is there where the vehicle's model has its point; what a model does not have works by its key only.
#pragma once

#include "CoreMinimal.h"

namespace ADPCab
{
	/** Switch codes of the shared controls (a vehicle's own codes stay below First). */
	enum : int32 { First = 1000, Park, Gear, GearUp, GearDown, Pto, Horn, Ind, IndL, IndR, Beam, Hazard, Wiper, Beacon, Work };
	inline bool Is(int32 Code) { return Code > First; }
}
