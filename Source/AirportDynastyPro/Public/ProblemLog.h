// The problem log: every warning and error the game meets is written, with the time, to Saved/Logs/Problems.log,
// so a problem can be sent and its cause found in the code (the settings panel opens the file's folder). Kept under
// 2 MB: when it grows past that it starts again.
#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

class FProblemLog : public FOutputDevice
{
public:
	FProblemLog();
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual bool CanBeUsedOnAnyThread() const override { return true; }

private:
	FString Path;
	FCriticalSection Lock;
};
