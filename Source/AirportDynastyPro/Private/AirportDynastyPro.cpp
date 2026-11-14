// The game's code module: Airport Dynasty Pro, PC version. On start it opens the problem log.
#include "Modules/ModuleManager.h"
#include "ProblemLog.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"

#pragma region Module

class FAirportDynastyProModule : public FDefaultGameModuleImpl
{
	TUniquePtr<FProblemLog> Problems;

	virtual void StartupModule() override
	{
		Problems = MakeUnique<FProblemLog>();
		if (GLog) GLog->AddOutputDevice(Problems.Get());
#if !WITH_EDITOR
		// where the game is installed, remembered for the installer (owner, 2026-11-14: a new installer downloaded outside
		// the game must always find this copy and install over it): HKCU\Software\DroneDirector\AirportDynastyPro\InstallDir
		FPlatformMisc::SetStoredValue(TEXT("DroneDirector"), TEXT("AirportDynastyPro"), TEXT("InstallDir"), FPaths::ConvertRelativePathToFull(FPaths::RootDir()));
#endif
	}
	virtual void ShutdownModule() override
	{
		if (GLog && Problems) GLog->RemoveOutputDevice(Problems.Get());
		Problems.Reset();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FAirportDynastyProModule, AirportDynastyPro, "AirportDynastyPro");

#pragma endregion
