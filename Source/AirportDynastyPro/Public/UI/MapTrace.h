// The holographic map traced from the real terminal (SHoloMap.cpp: ADPMapTrace::Run) - Data/holomap-walls.json.
#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace ADPMapTrace
{
	/** Cuts every wall of the terminal at walking height on Floor 1 and Floor 2 and writes the lines (editor's game only). */
	bool Run(UWorld* W, float F2Y);
}
