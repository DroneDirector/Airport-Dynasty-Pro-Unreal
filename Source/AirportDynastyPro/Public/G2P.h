// Words to the voice model's sounds, in Unreal itself (owner 2026-11-21: the voices with no web layer - the web pages are
// being switched off). A port of the game's own pronunciation rules (Airline-Manager-Pro-Babylon/src/ui/announcer-voice.ts,
// itself a copy of game/src/core/g2p.ts - keep them the same) with misaki's British dictionary from the voice pack
// (Content/Voice/lexicon-gb.json.gz, Apache 2.0): numbers, codes, registrations and units written out, dictionary words
// looked up (with plurals, past tenses, -ing), names and unknown words sounded out by spelling rules.
#pragma once
#include "CoreMinimal.h"

namespace ADPG2P
{
	/** Loads the dictionary once (a few seconds: call from a worker thread). False if the voice pack isn't there. */
	bool Ready();
	/** A line as Kokoro's sound numbers, a list per piece (sentences, long ones cut at their commas). */
	TArray<TArray<int64>> Ids(const FString& Text);
	/** The phonemes of a line, one string per sentence (for tests). */
	TArray<FString> Phonemize(const FString& Text);
}
