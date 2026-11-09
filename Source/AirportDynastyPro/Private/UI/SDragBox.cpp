// The draggable box: a press grabs it, moving drags it, letting go puts it down (and keeps the place).
#include "UI/SDragBox.h"
#include "Widgets/Layout/SBox.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

void SDragBox::Construct(const FArguments& Args)
{
	Name = Args._Name;
	Pos = Args._Start;
	FString Saved;
	if (GConfig && GConfig->GetString(TEXT("ADP.Boxes"), *Name, Saved, GGameUserSettingsIni)) Pos.InitFromString(Saved);
	ChildSlot.HAlign(HAlign_Left).VAlign(VAlign_Top)[SAssignNew(Holder, SBox)[Args._Content.Widget]];
}

void SDragBox::Place()
{
	// held inside the window, clear of the walking bar (top left), the map (bottom left) and the Buttons panel (bottom right)
	const FVector2D Size = Holder.IsValid() ? Holder->GetDesiredSize() : FVector2D(120, 40);
	FVector2D Px = Pos * Window;
	Px.X = FMath::Clamp(Px.X, 8.0, FMath::Max(8.0, Window.X - Size.X - 8));
	Px.Y = FMath::Clamp(Px.Y, 8.0, FMath::Max(8.0, Window.Y - Size.Y - 8));
	if (Px.X < 420 && Px.Y < 90) Px.Y = 90;                                    // the walking bar
	if (Px.X < 280 && Px.Y + Size.Y > Window.Y - 290) Px.X = 280;             // the map
	if (Px.X + Size.X > Window.X - 220 && Px.Y + Size.Y > Window.Y - 80) Px.Y = Window.Y - 80 - Size.Y; // the Buttons tab
	ChildSlot.Padding(FMargin(Px.X, Px.Y, 0, 0));
}

void SDragBox::Tick(const FGeometry& Geo, const double Now, const float Dt)
{
	SCompoundWidget::Tick(Geo, Now, Dt);
	Window = Geo.GetLocalSize();
	Place();
}

FReply SDragBox::OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E)
{
	if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	const FVector2D Local = Geo.AbsoluteToLocal(E.GetScreenSpacePosition());
	const FVector2D Size = Holder.IsValid() ? Holder->GetDesiredSize() : FVector2D(120, 40);
	const FVector2D Px = Pos * Window;
	if (Local.X < Px.X || Local.Y < Px.Y || Local.X > Px.X + Size.X || Local.Y > Px.Y + Size.Y) return FReply::Unhandled();
	bDragging = true;
	Grab = Local - Px;
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SDragBox::OnMouseMove(const FGeometry& Geo, const FPointerEvent& E)
{
	if (!bDragging) return FReply::Unhandled();
	const FVector2D Local = Geo.AbsoluteToLocal(E.GetScreenSpacePosition());
	Pos = (Local - Grab) / FVector2D(FMath::Max(1.0, Window.X), FMath::Max(1.0, Window.Y));
	return FReply::Handled();
}

FReply SDragBox::OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& E)
{
	if (!bDragging) return FReply::Unhandled();
	bDragging = false;
	if (GConfig) { GConfig->SetString(TEXT("ADP.Boxes"), *Name, *Pos.ToString(), GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
	return FReply::Handled().ReleaseMouseCapture();
}
