// Where a person may appear or vanish (to-do 294, owner 2026-11-25): only where no camera of yours can see it. One look for
// everybody - the passengers (PaxFlow), the landside staff (StaffPeople), the taxi bus with its fare (GSEService) - and the
// landside places behind something solid they come from and go to, found on the real geometry in small steps.
#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;

namespace ADPUnseen
{
	enum class ESee : uint8
	{
		Seen,         // on screen with a clear line to some part of the body, or within 25 m with nothing between
		OutOfFrame,   // a clear line, but outside the picture and over 25 m off (a sudden turn finds them walking, not popping up)
		Covered,      // something solid between every camera and the whole body
		Far           // under 2 pixels high
	};
	/** May something Height tall and 2 x Half wide appear or vanish at Feet now? All local cameras (the player's, flown and test
	 *  views: the camera manager's) and the scene captures that draw people. People never hide, nor glass, nor anything
	 *  translucent, masked or hidden. Ignore: the vehicle itself. */
	ESee Look(UWorld* W, const FVector& Feet, float Height = 185.f, float Half = 35.f, AActor* Ignore = nullptr);
	/** One of the looks this frame may have (two a frame for the whole game): false - ask again next frame. */
	bool Budget();
	/** Budget and Look together: may they, now? False also when this frame's looks are used up. */
	bool May(UWorld* W, const FVector& Feet, float Height = 185.f, float Half = 35.f, AActor* Ignore = nullptr);

	// ---- the landside places: the car park and the pavement along the terminal, on the walkers' navmesh, each with a way to
	// the terminal's doors that keeps off the forecourt's road (the zebra crossings are put in by the walkers' own ways)
	struct FPlace { FVector P = FVector::ZeroVector; bool bTerminalSide = false, bCover = false; ESee See = ESee::Seen; double SeenAt = -1.0, UsedAt = -1000.0, HeldTo = -1000.0; };
	/** Finds them, once, then keeps them up (what stands there now, what is hidden now): a few short traces a frame. */
	void Tick(UWorld* W);
	/** A place nobody can see now and nobody is at; INDEX_NONE: none yet (ask again next frame) - bNone: none at all now. */
	int32 Appear(UWorld* W, FPlace& Out, bool& bNone);
	/** A place to walk to and vanish at, kept for that person until Release (Not: the one they just left). */
	int32 Leave(UWorld* W, const FVector& From, bool bTerminalSide, int32 Not, FPlace& Out);
	void Release(UWorld* W, int32 Place);
	/** All looked for, and none there (no navmesh). */
	bool NoPlaces(UWorld* W);
	/** "84 places (31 behind something), 52 hidden now" */
	FString Line(UWorld* W);
}
