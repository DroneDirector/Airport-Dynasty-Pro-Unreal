// A progress bar with its percentage in the middle (owner, 2026-11-11): the number always readable, light over the
// empty part and dark where the filled part has reached it (the colour changes exactly at the fill's edge, so a number
// half covered is two-coloured). Used by the updater, the world's terrain download and the loading screen.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class AIRPORTDYNASTYPRO_API SProgressText : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SProgressText) : _Height(22.f) {}
		/** 0..1, or below 0 for "not known yet" (an empty bar, no number). */
		SLATE_ATTRIBUTE(float, Fraction)
		/** What follows the percentage, e.g. "12.4 MB/s" (empty: the percentage alone). */
		SLATE_ATTRIBUTE(FString, Extra)
		SLATE_ARGUMENT(float, Height)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(240.f, Height); }

private:
	TAttribute<float> Fraction;
	TAttribute<FString> Extra;
	float Height = 22.f;
};
