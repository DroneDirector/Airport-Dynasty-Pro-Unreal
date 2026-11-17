// Small "alive" animations the player may switch off (owner, 2026-11-17, to-do 98): Settings > Graphics > Animations.
// The main animations (jetways driving, doors, belts running, people) are never switchable - only these extras.
// Saved in ADP.Graphics (AnimHoseSway, AnimBeltFlaps) in GameUserSettings; both on by default.
#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"

namespace ADPAnim
{
	/** The jetways' yellow air hoses swaying a little under the cab-end section. */
	inline bool HoseSway = true;
	/** The baggage belts' rubber strip curtains swinging aside as bags push through. */
	inline bool BeltFlaps = true;

	inline void Load()
	{
		GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("AnimHoseSway"), HoseSway, GGameUserSettingsIni);
		GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("AnimBeltFlaps"), BeltFlaps, GGameUserSettingsIni);
	}
	inline void Save()
	{
		GConfig->SetBool(TEXT("ADP.Graphics"), TEXT("AnimHoseSway"), HoseSway, GGameUserSettingsIni);
		GConfig->SetBool(TEXT("ADP.Graphics"), TEXT("AnimBeltFlaps"), BeltFlaps, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
