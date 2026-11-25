// The game's own log file (ADPLog.h).
#include "ADPLog.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/Event.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "CoreGlobals.h"

namespace ADPLog
{
	namespace
	{
		FCriticalSection Guard;
		/** The writing thread: lines in the order they came, written at once. */
		class FADPLogWriter : public FRunnable
		{
		public:
			FADPLogWriter() : Wake(FPlatformProcess::GetSynchEventFromPool(false)) { FRunnableThread::Create(this, TEXT("ADPLog"), 0, TPri_BelowNormal); }
			void Add(TFunction<void()>&& Fn) { { FScopeLock Ql(&QueueGuard); Queue.Add(MoveTemp(Fn)); } Wake->Trigger(); }
			void Work()
			{
				FScopeLock Wl(&WorkGuard);
				for (;;)
				{
					TArray<TFunction<void()>> Now; { FScopeLock Ql(&QueueGuard); Now = MoveTemp(Queue); Queue.Reset(); }
					if (!Now.Num()) return;
					for (TFunction<void()>& Fn : Now) Fn();
				}
			}
			/** From any thread; never waits for ever (in a crash the writing thread may be stopped in the middle of a line). */
			void DrainNow() { for (int32 i = 0; i < 100; ++i) { if (WorkGuard.TryLock()) { Work(); WorkGuard.Unlock(); return; } FPlatformProcess::Sleep(0.005f); } }
			virtual uint32 Run() override { for (;;) { Wake->Wait(); Work(); } }
		private:
			FEvent* Wake; FCriticalSection QueueGuard, WorkGuard; TArray<TFunction<void()>> Queue;
		};
		FADPLogWriter* Writer = nullptr;   // (never ended: it lives as long as the game)
		FArchive* File = nullptr;
		bool bTried = false;
	}

	void Later(TFunction<void()> Work)
	{
		{
			FScopeLock Gl(&Guard);
			if (!Writer)
			{
				if (!FPlatformProcess::SupportsMultithreading() || IsEngineExitRequested()) { Work(); return; }
				Writer = new FADPLogWriter();
			}
		}
		Writer->Add(MoveTemp(Work));
		if (IsEngineExitRequested()) Writer->DrainNow();
	}

	void Drain() { if (Writer) Writer->DrainNow(); }

	void Write(const FString& Line)
	{
		const FString Out = FDateTime::Now().ToString(TEXT("%H:%M:%S.%s ")) + Line + LINE_TERMINATOR;
		Later([Out]()
		{
		if (!File && !bTried)
		{
			bTried = true;
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Logs"), Now = Dir / TEXT("ADP.log"), Prev = Dir / TEXT("ADP-prev.log");
			IFileManager& FM = IFileManager::Get();
			FM.MakeDirectory(*Dir, true);
			// the run before kept once; quietly, since another run of the game (or a stuck one) may still hold the
			// file: then this run writes beside it, under its own process number (2026-11-12: a stuck editor session
			// held ADP.log, and the cook counted the failed move as an error and stopped)
			if (FM.FileExists(*Now)) FM.Move(*Prev, *Now, true, true, false, true);
			File = FM.CreateFileWriter(*Now, FILEWRITE_AllowRead);
			if (!File) File = FM.CreateFileWriter(*(Dir / FString::Printf(TEXT("ADP-%u.log"), FPlatformProcess::GetCurrentProcessId())), FILEWRITE_AllowRead);
		}
		if (!File) return;
		const FTCHARToUTF8 U(*Out);
		File->Serialize(const_cast<ANSICHAR*>(U.Get()), U.Length());
		File->Flush();
		});
	}
}

// Everything that goes wrong goes into the game's log as well (owner, 2026-11-11): a crash (the error and where in the
// program, as Unreal records it), a glitch (an engine check that failed), and every error or warning any part of the
// game or Unreal reports (where Unreal keeps its log at all), so the fault can be found from ADP.log afterwards.
namespace
{
	class FADPErrorsToLog : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			const ELogVerbosity::Type Kind = ELogVerbosity::Type(Verbosity & ELogVerbosity::VerbosityMask);
			if (Kind > ELogVerbosity::Warning || Category == FName(TEXT("LogTemp"))) return;   // (the game's own lines are written already)
			ADPLog::Write(FString::Printf(TEXT("%s %s: %s"), Kind == ELogVerbosity::Fatal ? TEXT("FATAL") : Kind == ELogVerbosity::Error ? TEXT("Error") : TEXT("Warning"), *Category.ToString(), V));
			if (Kind == ELogVerbosity::Fatal) ADPLog::Drain();
		}
		virtual bool CanBeUsedOnAnyThread() const override { return true; }
	};

	void InstallErrors()
	{
		static FADPErrorsToLog Device;
		if (GLog) GLog->AddOutputDevice(&Device);
		FCoreDelegates::OnHandleSystemError.AddStatic([]() { ADPLog::Write(FString(TEXT("CRASH: ")) + GErrorHist); ADPLog::Drain(); });
		FCoreDelegates::OnEnginePreExit.AddStatic([]() { ADPLog::Drain(); });
		FCoreDelegates::OnHandleSystemEnsure.AddStatic([]() { ADPLog::Write(FString(TEXT("GLITCH (a check failed): ")) + GErrorHist); });
		ADPLog::Write(TEXT("Airport Dynasty Pro started"));
	}

	struct FInstallAtLoad { FInstallAtLoad() { FCoreDelegates::OnPostEngineInit.AddStatic(&InstallErrors); } } InstallAtLoad;
}
