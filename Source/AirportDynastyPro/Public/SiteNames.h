// The airport's name on the models' own pictures, written at run time from the active site (SiteNames.cpp).
#pragma once
#include "CoreMinimal.h"

namespace ADPSiteNames
{
	/** The pictures of this model that carry the Icelandic airport's name, drawn again with the active site's (nothing at that site itself). */
	void Apply(class AActor* Model, const FString& ModelName);
}
