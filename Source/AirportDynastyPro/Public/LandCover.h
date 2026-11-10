// The land cover at a point on Earth, read from the land cover tiles on this PC (Tools/MakeLandCover.py, downloaded with
// the world's terrain): the North Atlantic's sharp tiles (about 19 m) where they cover, else the world's (about 300 m).
// The tiles hold colours, each the average of the classes under it; the class is the nearest of the natural colours
// MakeLandCover.py paints. Tiles are read once and kept (a few hundred KB each). Used by the grass: it grows on the
// real ground only where the land is grass, farmland, scrub, moss or wetland, never on sand, rock, snow or water.
#pragma once

#include "CoreMinimal.h"

namespace ADPLandCover
{
	enum EClass : uint8 { Unknown = 0, Trees = 10, Shrub = 20, Grass = 30, Crops = 40, Built = 50, Bare = 60, Snow = 70, Water = 80, Wetland = 90, Mangrove = 95, Moss = 100 };

	/** The class at a longitude and latitude (degrees); Unknown when no tile is on this PC. */
	EClass ClassAt(double Lon, double Lat);

	/** Grass grows there (grass, farmland, scrub, moss, wetland; under trees too). */
	bool Grows(EClass C);
}
