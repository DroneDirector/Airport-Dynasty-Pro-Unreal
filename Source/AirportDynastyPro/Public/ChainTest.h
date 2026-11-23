// The functional test of the passenger and crew chain in a real career (ChainTest.cpp; Tools/new_game_test.sh --career).
#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace ADPChainTest
{
	/** The stage to start at (-ChainTest=N), 0: not this test. */
	int32 StartStage();
	/** Tests that run by themselves: no key, mouse or pad reaches anything in this game copy (a key in its window once reached
	 *  the editor's own shortcuts and crashed it). Once. */
	void BlockInput();
	/** The camera director of the test's window (ChainCam.cpp; off: -NoChainCam), each frame with the real time passed. */
	void Camera(UWorld* W, float Dt);
	/** From the new game / continue test, once the airport is in. */
	void Begin(UWorld* W);
}
