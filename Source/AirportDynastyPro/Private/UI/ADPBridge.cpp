// The link from the app's menus to Unreal.
#include "UI/ADPBridge.h"
#include "UI/ADPHUD.h"

void UADPBridge::OpenTerminal()
{
	if (AADPHUD* H = Hud.Get()) H->ShowTerminal();
}

void UADPBridge::OpenSite()
{
	if (AADPHUD* H = Hud.Get()) H->OpenSite();
}

void UADPBridge::GameState(const FString& Json)
{
	if (AADPHUD* H = Hud.Get()) H->OnGameState(Json);
}

void UADPBridge::GameFids(const FString& Json)
{
	if (AADPHUD* H = Hud.Get()) H->OnGameFids(Json);
}
