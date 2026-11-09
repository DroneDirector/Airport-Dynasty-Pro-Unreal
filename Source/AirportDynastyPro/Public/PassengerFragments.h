// A passenger's data as a Mass Entity fragment: where they are, which stage of the trip they are at, where they are
// walking to and how. Plain data only (their walking way is kept by the crowd, by the passenger's number).
#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "PassengerFragments.generated.h"

/** The stages of a departing passenger's trip through the terminal. */
UENUM()
enum class EPaxStage : uint8
{
	Arrive,    // from the kerb in through the entrance
	CheckIn,   // at a check-in desk
	Security,  // through a security lane
	Airside,   // a stop on the way: the Food Walk, Tax Free, the bar or a seat upstairs
	Gate,      // waiting at the gate
	Board,     // onto the aircraft: the jetway or the walk-out door
};

USTRUCT()
struct AIRPORTDYNASTYPRO_API FPaxFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector Pos = FVector::ZeroVector; // plan metres: x, height, z
	int32 Place = 0;                   // the place of the walking map they are in (or heading through)
	int32 Goal = 0;                    // the place they are walking to
	int32 Num = 0;                     // the passenger's number: their walking way and their figure
	int32 Step = 0;                    // the next point of their way
	int32 Gate = 1;                    // their flight's gate (1–8)
	float Speed = 1.3f;                // walking speed, m/s
	float Wait = 0;                    // seconds left standing where they are
	float Height = 1.75f;
	EPaxStage Stage = EPaxStage::Arrive;
	bool bWalking = false;
};
