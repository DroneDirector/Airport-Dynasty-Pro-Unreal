// A small box the player can drag anywhere on the screen (with the pointer free: the middle mouse button). Its place is
// kept in the settings file under its name; it is held inside the window, and the parts of the screen that belong to
// other panels (the walking bar, the map, the Buttons panel) push it away, so nothing ever sits on top of anything.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SDragBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDragBox) {}
		SLATE_ARGUMENT(FString, Name)
		SLATE_ARGUMENT(FVector2D, Start)   // where it starts, as a share of the window (0..1)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	/** Where it is, as a share of the window (0..1). */
	FVector2D At() const { return Pos; }

	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& E) override;
	virtual FReply OnMouseMove(const FGeometry& Geo, const FPointerEvent& E) override;
	virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override;

private:
	FString Name;
	FVector2D Pos = FVector2D(0.8, 0.3);
	bool bDragging = false;
	FVector2D Grab = FVector2D::ZeroVector;
	FVector2D Window = FVector2D(1920, 1080);
	TSharedPtr<class SBox> Holder;
	void Place();
};
