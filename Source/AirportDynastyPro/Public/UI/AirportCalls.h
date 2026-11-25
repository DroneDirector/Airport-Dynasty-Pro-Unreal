// Every announcement of the airport apart from the boarding calls, in one table (to-do 289; owner, 2026-11-25: all the calls
// a real airport makes, the ones for what the game has first, and what is built later brings its own). A feature that has
// announcements in real life does this and no more:
//   - something happens:   ADPCalls::Say(this, TEXT("lift-out"), {{TEXT("where"), TEXT("gate 5")}});
//   - it exists:           ADPCalls::Feature(TEXT("train"));   (the notices about it are then played now and then)
// and, when its words are not in the table (Private/UI/AirportCalls.cpp) yet, adds its line there. Rooms count by themselves:
// a notice about a room kind is played as soon as the terminal's plan has one.
#pragma once

#include "CoreMinimal.h"

struct FTerminalPlan;

namespace ADPCalls
{
	/** Makes the call of that name, its {words} filled in; the same one (Event and Key) not again for the minutes its line
	 *  says. False: unknown, said lately, or too small a matter while the career runs fast. */
	bool Say(const UObject* Ctx, const FString& Event, const TMap<FString, FString>& Words = TMap<FString, FString>(), const FString& Key = FString());
	/** Something the airport has from now on: its notices are played too. */
	void Feature(const FString& Name);
	/** What the timetable, the arrivals, the weather and the world bring: called with the announcer's own step. */
	void Step(const UObject* Ctx);
	/** The next general notice (or a passenger or car's driver asked for, or something found), in turn; Passenger and Place:
	 *  whom to ask for, and where they travel to ("" when somebody who arrived). */
	void Notice(const UObject* Ctx, const FTerminalPlan& Plan, const FString& Passenger, const FString& Place);
	/** " because of ..." for a planned leg's delay, as passengers are told it ("" when there is no reason to give). */
	FString Reason(const UObject* Ctx, const FString& PlanId);
	/** The words for a gate from which a bus goes to the aircraft ("" when it stands at the gate). */
	FString Bus(const UObject* Ctx, const FString& PlanId);
}
