// The performance log: very slow frames with their likely cause and the camera's place (PerfLog.cpp).
#pragma once
#include "CoreMinimal.h"

namespace ADPPerfLog
{
	void Start();
	void Stop();
	/** A measured stretch (the stress walk): its frames and findings, grouped by the part named in Mark. */
	void BeginSection(const FString& Name);
	/** The part now running ("up the escalator"); a text in brackets is a note about the walk itself. */
	void Mark(const FString& What);
	/** Ends the stretch: its one-line summary; the full report goes to Saved/Logs/StressWalk-Report.txt (and Report()). */
	FString EndSection();
	FString Report();
	/** Everything drawn in the world, measured (Nanite or not, triangles, copies, materials, shadows, lights): text. */
	FString SceneAudit(class UWorld* W);
	/** Added to the end of the stretch's report. */
	void SetAudit(const FString& A);
}
