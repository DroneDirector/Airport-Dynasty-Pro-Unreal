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

void UADPBridge::OpenLounge()
{
	if (AADPHUD* H = Hud.Get()) H->OpenLounge();
}

void UADPBridge::GameState(const FString& Json)
{
	if (AADPHUD* H = Hud.Get()) H->OnGameState(Json);
}

void UADPBridge::SpeechPcm(int32 Id, const FString& B64)
{
	if (AADPHUD* H = Hud.Get()) H->SpeechPcm(Id, B64);
}

void UADPBridge::Log(const FString& Line)
{
	UE_LOG(LogTemp, Log, TEXT("Menus: %s"), *Line.Left(600));
}

void UADPBridge::Board(const FString& Key, const FString& B64)
{
	if (AADPHUD* H = Hud.Get()) H->OnBoard(Key, B64);
}

void UADPBridge::SpeechFail(int32 Id)
{
	if (AADPHUD* H = Hud.Get()) H->SpeechFallback(Id);
}

void UADPBridge::GameFids(const FString& Json)
{
	if (AADPHUD* H = Hud.Get()) H->OnGameFids(Json);
}
