// World positions <-> places on the Earth through the globe's georeference (2026-11-20). Cesium's conversions work in the
// georeference's own frame, which the globe actor turns so the airport's x runs along its runway (Globe.cpp); the world
// is outside that frame. Every conversion between a world position and longitude / latitude / height goes through these,
// so the terrain, its cuts, the sea and anything placed on the Earth agree with the airport (before this the terrain was
// never turned: the airport lay east-west on the real map).
#pragma once

#include "CoreMinimal.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"   // (before Cesium's headers, as the other files do)
#endif
#include "CesiumGeoreference.h"

namespace ADPGeo
{
	/** Longitude, latitude (degrees), height (m on the ellipsoid) to a world position. */
	inline FVector ToWorld(const ACesiumGeoreference* G, const FVector& LonLatH)
	{ return G->GetActorTransform().TransformPosition(G->TransformLongitudeLatitudeHeightPositionToUnreal(LonLatH)); }
	/** A world position to longitude, latitude (degrees), height (m). */
	inline FVector ToEarth(const ACesiumGeoreference* G, const FVector& WorldPos)
	{ return G->TransformUnrealPositionToLongitudeLatitudeHeight(G->GetActorTransform().InverseTransformPosition(WorldPos)); }
	/** East, south, up at a world position, as world directions (the matrix's axes). */
	inline FMatrix EastSouthUp(const ACesiumGeoreference* G, const FVector& WorldPos)
	{
		const FMatrix Local = G->ComputeEastSouthUpToUnrealTransformation(G->GetActorTransform().InverseTransformPosition(WorldPos));
		return Local * G->GetActorTransform().ToMatrixNoScale().RemoveTranslation();
	}
}
