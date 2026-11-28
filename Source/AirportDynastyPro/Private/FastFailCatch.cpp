// Catches the ways a Windows process dies with 0xC0000409 that skip Unreal's crash reporter
// (abort(), a bad CRT parameter, an uncaught C++ exception, a pure virtual call) and writes the
// call stack to the log before the process ends. Added 2026-11-28 for the town-hub silent death.
#include "CoreMinimal.h"
#include "ADPLog.h"
#include "HAL/PlatformStackWalk.h"
#include "Misc/OutputDeviceRedirector.h"

#if PLATFORM_WINDOWS
#include <signal.h>
#include <stdlib.h>
#include <exception>

namespace
{
	void DumpStack(const TCHAR* Why)
	{
		static volatile int32 Once = 0;
		if (FPlatformAtomics::InterlockedExchange(&Once, 1)) return;
		ANSICHAR Stack[16384]; Stack[0] = 0;
		FPlatformStackWalk::StackWalkAndDump(Stack, sizeof(Stack), 0);
		ADP_ERROR(TEXT("FastFail: %s on thread %u - call stack:\n%s"), Why, FPlatformTLS::GetCurrentThreadId(), ANSI_TO_TCHAR(Stack));
		if (GLog) GLog->Flush();
	}
	void OnAbort(int) { DumpStack(TEXT("abort()")); }
	void OnBadParam(const wchar_t* Expr, const wchar_t* Func, const wchar_t* File, unsigned int Line, uintptr_t)
	{
		ADP_ERROR(TEXT("FastFail: bad CRT parameter %s in %s (%s:%u)"), Expr ? Expr : L"?", Func ? Func : L"?", File ? File : L"?", Line);
		DumpStack(TEXT("bad CRT parameter"));
	}
	void OnTerminate() { DumpStack(TEXT("std::terminate (uncaught exception)")); abort(); }
	void OnPureCall() { DumpStack(TEXT("pure virtual call")); abort(); }

	struct FFastFailCatch
	{
		FFastFailCatch()
		{
			signal(SIGABRT, OnAbort);
			_set_invalid_parameter_handler(OnBadParam);
			_set_purecall_handler(OnPureCall);
			std::set_terminate(OnTerminate);
		}
	} GFastFailCatch;
}
#endif
