// The voices of the ground crew, pilots and controllers (owner 2026-11-21: the pushback talk "very high quality - we have
// our own voice engine"): each line said with Kokoro (UI/Kokoro.cpp) in the speaker's own voice, played where he stands -
// you hear a crew member in his normal voice when you are near him (the radio itself, with its radio sound, only in the
// ATC tower, controlling a crew member or with the Radio app: later).
#pragma once
#include "CoreMinimal.h"

class AActor;

namespace ADPCrewVoice
{
	/** Says a line: Voice a Kokoro voice name (bm_george ...); heard from the speaker (an actor), within about 30 m. */
	void Say(AActor* Speaker, const FString& Words, const FString& Voice);
	/** The voice for a person: a man's or a woman's, picked from the name (always the same for them). */
	FString VoiceFor(const FString& Name, bool bFemale);
	/** The line's sound numbers back from the menus (UADPBridge::KokoroCrew). */
	void OnIds(int32 Id, const FString& IdsJson);
}
