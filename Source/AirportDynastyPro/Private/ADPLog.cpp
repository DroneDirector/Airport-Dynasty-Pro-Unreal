// The game's own log file (ADPLog.h).
#include "ADPLog.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

namespace ADPLog
{
	namespace
	{
		FCriticalSection Guard;
		FArchive* File = nullptr;
		bool bTried = false;
	}

	void Write(const FString& Line)
	{
		FScopeLock L(&Guard);
		if (!File && !bTried)
		{
			bTried = true;
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Logs"), Now = Dir / TEXT("ADP.log"), Prev = Dir / TEXT("ADP-prev.log");
			IFileManager& FM = IFileManager::Get();
			FM.MakeDirectory(*Dir, true);
			// the run before kept once
			if (FM.FileExists(*Now)) FM.Move(*Prev, *Now, true, true);
			File = FM.CreateFileWriter(*Now, FILEWRITE_AllowRead);
		}
		if (!File) return;
		const FString Out = FDateTime::Now().ToString(TEXT("%H:%M:%S.%s ")) + Line + LINE_TERMINATOR;
		const FTCHARToUTF8 U(*Out);
		File->Serialize(const_cast<ANSICHAR*>(U.Get()), U.Length());
		File->Flush();
	}
}
