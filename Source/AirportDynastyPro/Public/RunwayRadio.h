// Crossing the runway on the tower frequency (hangar step 6, docs/Hired-Crew-Plan.md "Hangar tow"): the hangar tow's tug
// and the maintenance vans call the tower at the holding point, the tower clears them across or holds them short, they
// read it back and report the runway vacated (ICAO phraseology). The clearance is what lets them cross; it is never given
// while the runway is busy (ASimTraffic::RunwayBusy, a tow or a van on the strip) and only after it has been free for 10 s.
// Heard in the voices (CrewVoice.h): the driver in his own voice where he sits, the tower (one controller, bf_emma) from
// the vehicle's radio; every line logged ("Radio: [...]"). One frequency: one line at a time, tows and vans alike.
#pragma once
#include "CoreMinimal.h"

class AActor;
class UWorld;

namespace ADPRunwayRadio
{
	/** One crossing's exchange: kept by the tow / the van from the holding point to the far side. */
	struct FCall
	{
		int32 Step = 0;            // 0 not called, 1 asked, 2 told to hold (readback due), 3 holding short, 4 cleared (readback due), 5 cleared and read back, 6 vacated reported
		double At = 0.0;           // (world s) the next line not before this
		double FreeSince = -1.0;   // (world s) the runway free since; -1 busy
		bool bIncursion = false;   // (on the strip without a clearance: warned once)
	};
	/** Who talks: Radio the vehicle (the tower is heard from it), Driver the one at its wheel (null: the radio), Voice his
	 *  (ADPCrewVoice::VoiceFor), Callsign ("tug 1", "maintenance van 2"), Towing ("towing Hotel Bravo ..." or empty), Twy the
	 *  holding point's taxiway spoken ("Alpha 2"), To where to ("to the hangar"); bYou: the player talks (shown, not voiced). */
	struct FWho
	{
		AActor* Radio = nullptr; AActor* Driver = nullptr;
		FString Voice, Callsign, Towing, Twy, To;
		bool bYou = false;
	};
	/** One frame at the holding point. bCall: the request may be said now (crew: always; you: after E). bBusy: the runway may
	 *  not be crossed; Traffic: why, as the tower says it ("traffic landing"). True: cleared and read back - it may cross. */
	bool Hold(UWorld* W, FCall& C, const FWho& Who, bool bCall, bool bBusy, const FString& Traffic);
	/** Off the strip on the far side after a clearance: "runway vacated" (once) and the tower's "roger". */
	void Vacated(UWorld* W, FCall& C, const FWho& Who);
	/** On the strip without a clearance (only you can): the tower's call, a warning, logged - once per crossing. */
	void Incursion(UWorld* W, FCall& C, const FWho& Who);
	/** "<airport> Tower": the site's name (Data/airfield.json via ADPSite, never baked). */
	FString Tower();
	/** The taxiway nearest a plan point (the holding point), spoken ("Alpha 2"). */
	FString TwyAt(const FVector2f& P);
	/** A registration spelled out ("HB-JXA" -> "Hotel Bravo Juliett X-ray Alpha"). */
	FString Spell(const FString& Reg);
	/** An airliner's clearance on the same frequency (to-do 378): Ask the pilot's request (empty: the tower calls first),
	 *  bGive the tower may give it now, Give the clearance, HoldLine the tower's "hold / continue" while it may not (empty:
	 *  it just waits), HoldBack its readback, Back the clearance's readback. True: given and read back. */
	bool Exchange(UWorld* W, FCall& C, const FWho& Who, const FString& Ask, bool bGive, const FString& Give, const FString& HoldLine, const FString& HoldBack, const FString& Back);
	/** The frequency is free (no line being said by anyone). */
	bool FreqFree(UWorld* W);
}
