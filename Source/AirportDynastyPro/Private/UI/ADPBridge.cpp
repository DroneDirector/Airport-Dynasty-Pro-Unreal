// The link from the app's menus to Unreal.
#include "UI/ADPBridge.h"
#include "UI/ADPHUD.h"

void UADPBridge::OpenTerminal()
{
	if (AADPHUD* H = Hud.Get()) H->ShowTerminal();
}
