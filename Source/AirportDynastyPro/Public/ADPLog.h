// The game's own log (owner, 2026-11-11): every line the game writes to Unreal's log also goes to
// Saved/Logs/ADP.log, in the packaged game too (where Unreal's own log is switched off): on a player's PC
// %LOCALAPPDATA%\AirportDynastyPro\Saved\Logs\ADP.log, the run before kept as ADP-prev.log. So a problem seen
// only in the released game can be read there (the menus' warnings and errors are in it as well). Crashes (the error and
// where in the program), failed engine checks and every error or warning Unreal reports go in it too (ADPLog.cpp).
#pragma once

#include "CoreMinimal.h"

namespace ADPLog
{
	/** One line, with the time, to Saved/Logs/ADP.log. */
	AIRPORTDYNASTYPRO_API void Write(const FString& Line);
}

/** As UE_LOG(LogTemp, Log, ...), and the line into Saved/Logs/ADP.log in every build. */
#define ADP_LOG(Format, ...) do { const FString AdpLogLine_ = FString::Printf(Format, ##__VA_ARGS__); UE_LOG(LogTemp, Log, TEXT("%s"), *AdpLogLine_); ADPLog::Write(AdpLogLine_); } while (0)
#define ADP_WARN(Format, ...) do { const FString AdpLogLine_ = FString::Printf(Format, ##__VA_ARGS__); UE_LOG(LogTemp, Warning, TEXT("%s"), *AdpLogLine_); ADPLog::Write(TEXT("Warning: ") + AdpLogLine_); } while (0)
#define ADP_ERROR(Format, ...) do { const FString AdpLogLine_ = FString::Printf(Format, ##__VA_ARGS__); UE_LOG(LogTemp, Error, TEXT("%s"), *AdpLogLine_); ADPLog::Write(TEXT("Error: ") + AdpLogLine_); } while (0)
