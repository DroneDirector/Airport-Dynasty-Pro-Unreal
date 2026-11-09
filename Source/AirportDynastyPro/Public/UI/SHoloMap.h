// The airport map while you walk, as in the Android edition (holomap.ts): a square, holographic blue map in the corner,
// always centred on you and turned the way you face. Its walls come from the plan (with the doors' openings); the
// stairs, lifts and columns too. The floor you are on is drawn clearly, the other floor only as a faint ghost. A click
// on it (or M) opens it full screen, north up, with the rooms' names: click where you want to go and a route is drawn
// there (through the doors, up or down the stairs), warm orange; it follows you and clears when you arrive.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "TerminalPlan.h"
#include "WalkMap.h"

class SHoloMap : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SHoloMap) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	/** Gives the map the plan to draw (walls, stairs, lifts, columns, rooms) and its walking map for routes. */
	void SetPlan(const FTerminalPlan& Plan);
	/** Where the player is: plan metres (x, height, z) and the way they face (Unreal yaw, degrees). */
	void SetPlayer(const FVector& PlanPos, float YawDeg);
	void SetFull(bool bOn);
	bool IsFull() const { return bFull; }
	void ClearRoute() { Route.Reset(); }

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(240, 240); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event) override;

	/** Called when the small map is clicked (to open it full screen). */
	TFunction<void()> OnOpen;

private:
	#pragma region Drawing
	struct FSeg { FVector2f A, B; bool bGlass; };
	TArray<FSeg> Walls[3];         // by floor (1, 2)
	TArray<FBox2f> StairBoxes[3], LiftBoxes[3];
	TArray<FVector2f> Columns;
	FBox2f Bounds;
	FTerminalPlan Plan;
	FWalkMap Map;

	FVector Player = FVector::ZeroVector;
	float Yaw = 0;
	bool bFull = false;
	TArray<FVector> Route;

	int32 FloorOf(float Height) const { return Height > Plan.F2Y * 0.5f ? 2 : 1; }
	/** Plan (x, z) to the widget: the small map turned with the player, the full map north up and fitted. */
	FVector2f ToLocal(const FVector2f& P, const FVector2f& Size) const;
	FVector2f FromLocal(const FVector2f& L, const FVector2f& Size) const;
	float Scale(const FVector2f& Size) const;
	#pragma endregion
};
