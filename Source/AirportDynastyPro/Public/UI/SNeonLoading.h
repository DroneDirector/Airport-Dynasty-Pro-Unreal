// The neon loading screen drawn by the game itself (owner, 2026-11-17: "I want the neon loading screen to be everywhere
// where loading screens are, with the same effect everywhere"): the same look as the title screen's web loading screen
// (Airline-Manager-Pro-Babylon/src/ui/loading-neon.ts, Content/Web/styles.css .ln-*) - the deep purple background with
// its faint scan lines, the cyan kicker line, the big glowing Audiowide title, the long bar filling cyan > violet > pink
// with a moving sheen, under it what is loading now with the percentage at its end, and the game tips at the bottom (a
// new one every 5 s, fading in). Done: "Loading complete" and "Get ready for takeoff", glowing cyan.
// Every loading screen the game draws itself uses this one (UI/Loading.cpp); the web pages use loading-neon.ts.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class AIRPORTDYNASTYPRO_API SNeonLoading : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SNeonLoading) : _Kicker(TEXT("Airport Dynasty Pro")) {}
		/** The small spaced line over the title, e.g. "Airport Dynasty Pro  ·  into the airport". */
		SLATE_ARGUMENT(FString, Kicker)
		/** The big glowing title, e.g. "Loading the airport" (shown as "Loading complete" once done). */
		SLATE_ATTRIBUTE(FString, Title)
		/** What is loading now (under the bar, left). */
		SLATE_ATTRIBUTE(FString, Step)
		/** 0..1. */
		SLATE_ATTRIBUTE(float, Fraction)
		/** Everything is in: the finished look. */
		SLATE_ATTRIBUTE(bool, Done)
		/** A small line at the very bottom (e.g. "Esc: back to the menus"); empty: none. */
		SLATE_ATTRIBUTE(FString, Hint)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(640.f, 360.f); }

	/** A new loading: the bar from its beginning (it was kept at its end from the last one). */
	void Restart();

	/** The game tips (the same as the web loading screen's). */
	static const TArray<FString>& Tips();

private:
	FString Kicker;
	TAttribute<FString> Title, Step, Hint;
	TAttribute<float> Fraction;
	TAttribute<bool> Done;
	double T0 = 0;
	mutable float Shown = 0.f;     // the bar as drawn: eases towards Fraction, never backwards
	mutable double LastPaint = 0;
};
