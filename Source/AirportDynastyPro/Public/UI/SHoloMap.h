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
	/** A hangar on the map (plan metres, segments x0, z0, x1, z1): its walls and door, its crew annex's two floors. */
	void AddHangar(const FString& Name, const TArray<FVector4f>& Outline, const TArray<FVector4f>& Door);
	virtual FReply OnMouseWheel(const FGeometry& Geo, const FPointerEvent& Event) override;
	/** The full map zoomed out to everything on it (the hangars too). */
	void ShowAll() { View = All.ExpandBy(10.f); }
	bool IsFull() const { return bFull; }
	void ClearRoute() { Route.Reset(); Waypoints.Reset(); }
	bool HasRoute() const { return Waypoints.Num() > 0; }
	/** The legend in the full-screen map: what the symbols mean and the controls (for the device in use). */
	void SetLegend(const TArray<FString>& Lines, const FString& Key) { Legend = Lines; LegendKey = Key; }
	/** The legend starts minimized (a small tab); the Buttons key opens and closes it while the map is full screen. */
	void ToggleLegend() { bLegendOpen = !bLegendOpen; }

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
	/** The other buildings, each with its own floors (the jetways: one floor; the car rental centre; the control
	 *  tower: ground floor, lift lobby, cab): drawn whole on the map; the one you are in gives the floor reading. */
	struct FBFloor { FString Label; float Y0 = 0, Y1 = 0; TArray<FSeg> Segs; };
	struct FBuilding { FString Label; FBox2f Box; TArray<FBFloor> Floors; bool bNamed = false; };
	TArray<FBuilding> Buildings;
	void LoadBuildings();
	/** The building you are in (INDEX_NONE: the terminal) and its floor there. */
	int32 BuildingAt(const FVector& P, int32& OutFloor) const;
	FBox2f Bounds;
	/** The full map's view: it opens on the terminal (Bounds) and zooms with the wheel round the pointer, out as far as
	 *  everything on it (the hangars, a kilometre off); View is the plan box shown. */
	FBox2f All = FBox2f(ForceInit), View = FBox2f(ForceInit);
	void ResetView();
	FTerminalPlan Plan;
	FWalkMap Map;

	FVector Player = FVector::ZeroVector;
	float Yaw = 0;
	bool bFull = false;
	TArray<FVector> Route;
	/** Your own marks (plan metres): the route goes through them in order; a click adds one, a right-click on one removes it. */
	TArray<FVector> Waypoints;
	void Rebuild();
	int32 PlaceAt(float X, float Z, int32 Floor) const;
	TArray<FString> Legend;
	FString LegendKey;
	bool bLegendOpen = false;

	int32 FloorOf(float Height) const { return Height > Plan.F2Y * 0.5f ? 2 : 1; }
	/** Plan (x, z) to the widget: the small map turned with the player, the full map north up and fitted. */
	FVector2f ToLocal(const FVector2f& P, const FVector2f& Size) const;
	FVector2f FromLocal(const FVector2f& L, const FVector2f& Size) const;
	float Scale(const FVector2f& Size) const;
	#pragma endregion
};
