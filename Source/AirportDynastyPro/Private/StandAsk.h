#pragma once
#include "CoreMinimal.h"

class UWorld;

/** An aircraft at a stand without a jetway whose stairs or bus did not come by themselves asks for them (to-do 296, 277 D6). Not in
 *  the career: after a load each flight that still waits asks again. */
namespace ADPStandAsk
{
	/** Every couple of seconds while the service is due. Where: 0 missing, 1 on its way, 2 there. Back: how often it has been asked
	 *  for (0: not yet - three minutes after it should have come; then once more every five, each time more urgent). */
	int32 Check(UWorld* W, const FDateTime& Now, const FString& Key, const FString& What, const FString& Who, int32 Where, bool bCan, const FString& Why);
}
