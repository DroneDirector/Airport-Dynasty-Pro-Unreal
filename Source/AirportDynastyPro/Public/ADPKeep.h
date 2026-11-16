// An object kept in a function's static pointer must never be freed by Unreal's garbage collector, or the pointer is
// left dangling and the next use crashes (0.17.16 crashed on the "Who are you?" stage's second visit: its neon
// material, held in a static, had been collected). ADPKeep pins it for the game's life.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"

template <class T> T* ADPKeep(T* O)
{
	if (O) O->AddToRoot();
	return O;
}
