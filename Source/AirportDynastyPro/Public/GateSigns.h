// The new gates' signs (16 gates, owner 2026-11-17): today's gate signs are baked into the terminal model; the copied
// gate rooms 9-16 get theirs here - a dark plate over each walkway opening with "Gate n" / "View bay n", as today's.
#pragma once
#include "CoreMinimal.h"
struct FTerminalPlan;
class UWorld;
namespace ADPGateSigns { void Place(UWorld* W, const FTerminalPlan& Plan); }
