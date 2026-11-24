// The cabin cleaning van's kit in your hands (to-do 238; GSECleanYou.cpp). Its crew's own part: GSEService.cpp.
#pragma once

#include "CoreMinimal.h"

struct FSvcClean
{
	enum EState : uint8 { Van, Hand, Laid, Cabin };   // in its place or with the crew, in your hand, stood on the apron by you, handed in at the aircraft's door
	struct FThing { uint8 State = Van; FVector At = FVector::ZeroVector; float Yaw = 0.f; };   // (At: world, where its grip is)
	TArray<FThing> Things;                // (as the van's items)
	float IdleT = 1e6f, AskT = 0.f;       // since you last did something with it; till a cleaner is asked again
	double InAt = -10.;                   // when you were last in the load space
	int32 FetchCi = INDEX_NONE, FetchIt = INDEX_NONE; bool bSqueeze = false;
	int32 Held() const { for (int32 i = 0; i < Things.Num(); ++i) if (Things[i].State == Hand) return i; return INDEX_NONE; }
	bool Out() const { for (const FThing& X : Things) if (X.State == Hand || X.State == Laid) return true; return false; }
};
