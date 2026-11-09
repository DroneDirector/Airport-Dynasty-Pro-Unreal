// The game's code module: Airport Dynasty Pro, PC version. On start it opens the problem log.
#include "Modules/ModuleManager.h"
#include "ProblemLog.h"

#pragma region Module

class FAirportDynastyProModule : public FDefaultGameModuleImpl
{
	TUniquePtr<FProblemLog> Problems;

	virtual void StartupModule() override
	{
		Problems = MakeUnique<FProblemLog>();
		if (GLog) GLog->AddOutputDevice(Problems.Get());
	}
	virtual void ShutdownModule() override
	{
		if (GLog && Problems) GLog->RemoveOutputDevice(Problems.Get());
		Problems.Reset();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FAirportDynastyProModule, AirportDynastyPro, "AirportDynastyPro");

#pragma endregion
