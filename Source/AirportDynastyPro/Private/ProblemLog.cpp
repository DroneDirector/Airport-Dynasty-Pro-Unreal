// The problem log: warnings and errors appended to Saved/Logs/Problems.log.
#include "ProblemLog.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

FProblemLog::FProblemLog()
{
	Path = FPaths::ProjectLogDir() / TEXT("Problems.log");
	if (IFileManager::Get().FileSize(*Path) > 2 * 1024 * 1024) IFileManager::Get().Delete(*Path);
}

void FProblemLog::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	const ELogVerbosity::Type Level = ELogVerbosity::Type(Verbosity & ELogVerbosity::VerbosityMask);
	if (Level > ELogVerbosity::Warning || Level == ELogVerbosity::NoLogging) return;
	const FString Line = FString::Printf(TEXT("%s  %s  %s: %s\n"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")),
		Level <= ELogVerbosity::Error ? TEXT("ERROR  ") : TEXT("warning"), *Category.ToString(), V);
	FScopeLock L(&Lock);
	FFileHelper::SaveStringToFile(Line, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}
