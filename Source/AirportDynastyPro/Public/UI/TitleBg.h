// The title screen's film drawn by Unreal (owner, 2026-11-13: the title's background stuttered in Unreal's web view):
// the same film the page drew on its canvas (Airline-Manager-Pro-Babylon src/ui/splash.ts, every number copied), here
// one UI material (M_TitleBg, Tools/set_title_bg.py) on the graphics card, behind the page. The page keeps the neon
// logo, the menus, the captions, the scanlines and the music, and sends its music clock (window.ue.adp.titlebg) so the
// cuts stay on the beat. Without the material the page draws the film itself, as before.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateBrush.h"

class UMaterialInstanceDynamic;
class UTexture;

namespace ADPTitleBg
{
	/** The page: the title is up (or gone), and where its music is (seconds). */
	void Clock(bool bOn, float Seconds);
	bool IsOn();
	/** The material and pictures are there (the page asks before leaving the film to Unreal). */
	bool Ready();
}

class STitleBg : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STitleBg) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual ~STitleBg();
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(16, 9); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	TStrongObjectPtr<UMaterialInstanceDynamic> Mid;
	TMap<FString, TStrongObjectPtr<UTexture>> Tex;
	FSlateBrush Brush;
	TArray<uint8> Env, Kick;                 // the music's loudness and kick, 25 a second (the page's env.json)
	mutable double LastPaint = 0;
	mutable int32 LastShot = -1;
	mutable float Flash = 0.f, Glitch = 0.f;
	void Frame(float W, float H, float Dt) const;
	float Level(const TArray<uint8>& A, float T) const;
};
