// The people talk with each other (owner 2026-11-27): walk past two passengers waiting and you hear them chatting - a
// few turns of everyday small talk, each line in that person's own voice where they stand (CrewVoice.h, through
// AADPPeople::SayAloud, so the subtitle is the one the game already shows), one of them at a time, and the two of them
// turned to each other with their heads and eyes on the other one's head (TalkLook.h pairs).
// Who: two people on their feet or seated within about 2.5 m, roughly facing, on the same floor, neither walking, at a
// job, in your talk menu or part of the conduct scene. The lines come from a small table by who is talking
// (passenger - passenger, passenger - staff, staff - staff); nothing about you, nothing about the airport's business.
// Only what you could hear is started and kept (AADPPeople::CanHear); out of earshot it ends at once, so no voice is
// ever synthesised for a conversation you cannot hear, and at most three run together.
#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

namespace ADPNpcChat
{
	/** Each frame, from AADPPeople::Tick: steps the conversations and now and then looks for a new pair. */
	void Tick(UWorld* W, float Dt);
	/** Is this person in a conversation just now (they stay where they are until it ends)? */
	bool Busy(const AActor* Who);
	/** All of them off at once (the world goes). */
	void Drop();
	/** How many are running (the logs and the tests). */
	int32 Running();
}
