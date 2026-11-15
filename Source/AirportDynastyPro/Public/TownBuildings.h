// The home town's buildings that are made so far (owner, 2026-11-15: "place it into the world there as a town"): the
// VELDRA car dealership (Art/Dealership, town plan marker 3) south of Main Street by the west loop, its car lot down to
// the street, and the player's villa (Art/Villa, marker 8) on its own lot at the end of the villa road, its glass to the
// north. Finished models from the phone chat, imported as they are (Tools/import_model.py - Art/... <Name>). Where each
// stands is Data/town-buildings.json (Tools/make_roads.py: square to the town plan, with the roads it needs): its own
// frame - origin, its Blender x and y as plan directions - and its quick-start spot, 25-30 m out, looking at it.
// Each stands level on our own land (ARoadNetwork::GroundAt), at the highest ground under its footprint.
#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace ADPTown
{
	/** Places the buildings (after the roads: they stand on the land the roads shaped). */
	void Place(UWorld* W);
	/** A building's quick-start (id: "dealership", "villa"): where to stand and what to look at (plan x, height, plan y). */
	bool Spot(const FString& Id, FVector& At, FVector& Look);
}
