// "Where is your airport?" - the first step of a new career when the game has more than one airport site (owner,
// 2026-11-20: choose Iceland or Chablais Airport in Switzerland; Site.h). Drawn by the game itself in the title screen's
// neon look (SNeonLoading's colours and font): one glowing card per site, mouse, keys (Left/Right, Enter, Esc) and pad.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class AIRPORTDYNASTYPRO_API SSiteChoice : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSiteChoice) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(640.f, 360.f); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geo, const FKeyEvent& Key) override;
	virtual FReply OnMouseMove(const FGeometry& Geo, const FPointerEvent& E) override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E) override;

	/** Called with the chosen site's id. */
	TFunction<void(const FString&)> OnPick;
	/** Esc / the pad's back button: back to the title screen. */
	TFunction<void()> OnBack;

private:
	struct FCard { FString Id, Name, Place, Line; };
	TArray<FCard> Cards;
	int32 Hover = 0;
	mutable TArray<FSlateRect> Rects;
	double T0 = 0;
	int32 HitAt(const FGeometry& Geo, const FVector2D& Screen) const;
};
