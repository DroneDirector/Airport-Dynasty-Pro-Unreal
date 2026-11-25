// The one movement of every cursor drawn on a device's screen in the game (jetway, phone ...; to-do 263): as fast as the
// pointer on the PC itself, whatever the view's zoom, the frame rate or the game's own mouse settings.
#pragma once
#include "CoreMinimal.h"

class APlayerController;

namespace ADPGameCursor
{
	/** This frame's movement in pixels of the game's window (x right, y down): the mouse's real movement with the pointer
	 *  speed and acceleration set in Windows, and the game's own cursor speed. The pad's left stick as well: across the
	 *  window's height in about 0.8 s held right out, slow and precise near its middle. */
	FVector2D Move(const APlayerController* PC, float Dt, bool bStick = true);

	/** How many window pixels that flat screen in the world is wide and high just now (0 if it isn't in view). */
	FVector2D SizeOnWindow(const APlayerController* PC, const FVector& Mid, const FVector& Right, const FVector& Up, const FVector2D& Size);

	/** The game's own cursor speed (1: exactly the PC's), kept in the settings. */
	float Speed();
	void SetSpeed(float S);
}
