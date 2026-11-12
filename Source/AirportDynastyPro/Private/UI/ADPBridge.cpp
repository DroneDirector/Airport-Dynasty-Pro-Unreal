// The link from the app's menus to Unreal.
#include "UI/ADPBridge.h"
#include "ADPLog.h"
#include "UI/ADPHUD.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

void UADPBridge::OpenViewer()
{
	if (AADPHUD* H = Hud.Get()) H->OpenViewer();
}

void UADPBridge::ViewerFlight(const FString& Json)
{
	if (AADPHUD* H = Hud.Get()) H->OnViewerFlight(Json);
}

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

static FString SaveFile(const FString& Name)
{
	FString Safe;
	for (TCHAR C : Name) if (FChar::IsAlnum(C) || C == '-' || C == '.' || C == '_') Safe.AppendChar(C);
	if (Safe.IsEmpty() || Safe.Contains(TEXT(".."))) return FString();
	return FPaths::ProjectSavedDir() / TEXT("Saves") / Safe;
}

bool UADPBridge::SaveWrite(const FString& Name, const FString& Data)
{
	const FString F = SaveFile(Name);
	if (F.IsEmpty()) return false;
	if (Data.IsEmpty()) { IFileManager::Get().Delete(*F, false, true, true); return true; }
	const bool bOk = FFileHelper::SaveStringToFile(Data, *F, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	ADP_LOG(TEXT("Menus: save %s %s (%d chars)"), *Name, bOk ? TEXT("written") : TEXT("FAILED"), Data.Len());
	return bOk;
}

FString UADPBridge::SaveRead(const FString& Name)
{
	FString Out;
	const FString F = SaveFile(Name);
	if (!F.IsEmpty()) FFileHelper::LoadFileToString(Out, *F);
	return Out;
}

void UADPBridge::Quit()
{
	ADP_LOG(TEXT("Menus: Exit, the game quits"));
	FGenericPlatformMisc::RequestExit(false);
}

void UADPBridge::OpenSettings()
{
	if (AADPHUD* H = Hud.Get()) H->ToggleSettings();
}

void UADPBridge::CheckUpdates()
{
	if (AADPHUD* H = Hud.Get()) { H->UpdateSent.Reset(); H->CheckForUpdates(); }
}

void UADPBridge::InstallUpdates()
{
	if (AADPHUD* H = Hud.Get()) if (!H->UpdateBusy && (H->UpdateChain.Num() || !H->UpdateFullUrl.IsEmpty())) H->InstallUpdate();
}

void UADPBridge::Log(const FString& Line)
{
	ADP_LOG(TEXT("Menus: %s"), *Line.Left(600));
}

void UADPBridge::Board(const FString& Key, const FString& B64)
{
	if (AADPHUD* H = Hud.Get()) H->OnBoard(Key, B64);
}

void UADPBridge::Kokoro(int32 Id, const FString& IdsJson)
{
	if (AADPHUD* H = Hud.Get()) H->KokoroSpeak(Id, IdsJson);
}

void UADPBridge::SpeechFail(int32 Id)
{
	if (AADPHUD* H = Hud.Get()) H->SpeechFallback(Id);
}

void UADPBridge::GameFids(const FString& Json)
{
	if (AADPHUD* H = Hud.Get()) H->OnGameFids(Json);
}

void UADPBridge::DeviceDone(const FString& Kind)
{
	if (AADPHUD* H = Hud.Get()) H->DeviceDone(Kind);
}

void UADPBridge::Wardrobe(const FString& Action, const FString& Arg)
{
	if (AADPHUD* H = Hud.Get()) H->WardrobeFromPage(Action, Arg);
}
