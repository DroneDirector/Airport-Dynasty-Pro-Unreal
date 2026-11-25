// The card beside the face of whoever you talk to (owner, 2026-11-25): what is known about them on top - a passenger's
// flight, from where to where, gate, boarding, bag, security; a crew member's job, shift and what they do now - and under
// it what they say and what you can ask. Very frosted dark glass: the picture behind is blurred until nothing of it can be
// told. Taken with the mouse it is dragged about (the place is kept); a choice is a click, the gamepad's d-pad or left
// stick with A (B closes), the keyboard's arrows with Enter, or its number.
#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"

class AActor;
class UWorld;
class SVerticalBox;

/** What the card's top shows; anything not known is left empty and not shown. */
struct FTalkCard
{
	FString Name, Who;
	bool bTrip = false;
	FString FromCode, FromName, ToCode, ToName;
	TArray<TPair<FString, FString>> Cells;
	FString Status; FLinearColor Dot = FLinearColor(0.15f, 0.75f, 0.35f);
	bool operator==(const FTalkCard& O) const
	{
		return Name == O.Name && Who == O.Who && bTrip == O.bTrip && FromCode == O.FromCode && FromName == O.FromName && ToCode == O.ToCode
			&& ToName == O.ToName && Cells == O.Cells && Status == O.Status && Dot == O.Dot;
	}
};

namespace ADPTalkCard
{
	/** Everything known now about the one you talk to. */
	FTalkCard About(UWorld* W, AActor* Who);
}

class STalkCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STalkCard) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E) override;
	virtual FReply OnMouseMove(const FGeometry& Geo, const FPointerEvent& E) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& E) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& E) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& Geo, const FPointerEvent& E) const override;
	/** The highlight one up or down; the highlighted choice taken; the choice of a number taken. bPad: from the gamepad. */
	void Step(int32 By, bool bPad);
	void Take(bool bPad);
	void TakeNumber(int32 I);
	void Shut(bool bPad);
private:
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AActor> Shown;
	TSharedPtr<SVerticalBox> Top, Rows;
	FTalkCard Card; bool bMade = false;
	TArray<FString> Options; int32 Sel = 0; bool bByPad = false;
	double NextLook = 0.0;
	FVector2D Drag = FVector2D::ZeroVector, Grab = FVector2D::ZeroVector, DragFrom = FVector2D::ZeroVector; bool bDragging = false;   // (as a share of the window)
	class AADPPeople* People() const;
	void MakeTop();
	void MakeRows();
	void Let();
	TOptional<FSlateRenderTransform> Place();
};
