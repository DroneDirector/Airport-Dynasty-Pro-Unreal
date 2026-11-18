// The performance log: very slow frames with their likely cause and the camera's place (PerfLog.cpp).
#pragma once
#include "CoreMinimal.h"

namespace ADPPerfLog
{
	void Start();
	void Stop();
	/** The stall watch: whenever the game thread stops for 60 ms, where it is (on with -StallWatch, and for every stress walk). */
	void StartStallWatch();
	/** A measured stretch (the stress walk): its frames and findings, grouped by the part named in Mark. */
	void BeginSection(const FString& Name);
	/** The part now running ("up the escalator"); a text in brackets is a note about the walk itself. */
	void Mark(const FString& What);
	/** Ends the stretch: its one-line summary; the full report goes to Saved/Logs/StressWalk-Report.txt (and Report()). */
	FString EndSection();
	/** " | on screen N fps (frame generation ...)" for the time since the counts were last taken (and takes them again). */
	FString ScreenLine(double Seconds, uint64& Shown0, uint64& Made0);
	FString Report();
	/** Everything drawn in the world, measured (Nanite or not, triangles, copies, materials, shadows, lights): text. */
	FString SceneAudit(class UWorld* W);
	/** Added to the end of the stretch's report. */
	void SetAudit(const FString& A);
}

// The time guard (owner, 2026-11-16: catch a heavy job early, before it is a stutter): put ADP_PERF_SCOPE("what", ms) at
// the top of any work that walks through many objects (every chair, every lamp, every building ...). Whenever one run of
// it takes longer than its budget, the performance log says so by name - the first time, then every 20th time with the
// count. ASeats::Gather walking every building every 15 s (112 ms) was the game's stutter until it was caught.
namespace ADPPerfLog { void SlowWork(const TCHAR* What, double Ms, float BudgetMs); }
struct FADPPerfScope
{
	const TCHAR* What; float Budget; double T0;
	FADPPerfScope(const TCHAR* InWhat, float InBudget) : What(InWhat), Budget(InBudget), T0(FPlatformTime::Seconds()) {}
	~FADPPerfScope() { const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0; if (Ms > Budget) ADPPerfLog::SlowWork(What, Ms, Budget); }
};
#define ADP_PERF_SCOPE_CAT2(A, B) A##B
#define ADP_PERF_SCOPE_CAT(A, B) ADP_PERF_SCOPE_CAT2(A, B)
#define ADP_PERF_SCOPE(What, BudgetMs) FADPPerfScope ADP_PERF_SCOPE_CAT(ADPPerfScope_, __LINE__)(TEXT(What), BudgetMs)
