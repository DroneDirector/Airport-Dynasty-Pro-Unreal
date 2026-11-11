// The progress bar with its percentage in the middle (SProgressText.h).
#include "UI/SProgressText.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void SProgressText::Construct(const FArguments& Args)
{
	Fraction = Args._Fraction;
	Extra = Args._Extra;
	Height = Args._Height;
}

int32 SProgressText::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	const FVector2D Size = Geo.GetLocalSize();
	const float F = Fraction.Get();
	const float Fill = FMath::Clamp(F, 0.f, 1.f) * Size.X;
	const FLinearColor Empty(0.03f, 0.12f, 0.21f, 0.95f), Full(0.24f, 0.72f, 1.f, 1.f), Edge(0.35f, 0.62f, 0.85f, 1.f);
	const FLinearColor LightInk(0.85f, 0.95f, 1.f), DarkInk(0.01f, 0.05f, 0.1f);
	// the bar: the empty track, the filled part, a thin frame
	FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(), White, ESlateDrawEffect::None, Empty);
	if (Fill > 0.5f) FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2D(Fill, Size.Y), FSlateLayoutTransform()), White, ESlateDrawEffect::None, Full);
	for (const FVector4f& R : {FVector4f(0, 0, Size.X, 1), FVector4f(0, Size.Y - 1, Size.X, 1), FVector4f(0, 0, 1, Size.Y), FVector4f(Size.X - 1, 0, 1, Size.Y)})
		FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(R.Z, R.W), FSlateLayoutTransform(FVector2D(R.X, R.Y))), White, ESlateDrawEffect::None, Edge);
	if (F < 0.f) return Layer + 2;
	// the number, centred: drawn light over the whole bar, then dark again clipped to the filled part
	const FString Ex = Extra.Get();
	const FString Text = FString::Printf(TEXT("%d%%"), FMath::FloorToInt(FMath::Clamp(F, 0.f, 1.f) * 100.f)) + (Ex.IsEmpty() ? FString() : TEXT("   ") + Ex);
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, int32(Size.Y * 0.5f)));
	const FVector2D TS = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
	const FVector2D At((Size.X - TS.X) * 0.5f, (Size.Y - TS.Y) * 0.5f);
	const FPaintGeometry TG = Geo.ToPaintGeometry(TS, FSlateLayoutTransform(At));
	FSlateDrawElement::MakeText(Out, Layer + 3, TG, Text, Font, ESlateDrawEffect::None, LightInk);
	if (Fill > At.X)
	{
		const FSlateRect Abs = Geo.GetLayoutBoundingRect();
		const float Scale = Geo.GetAccumulatedLayoutTransform().GetScale();
		Out.PushClip(FSlateClippingZone(FSlateRect(Abs.Left, Abs.Top, Abs.Left + Fill * Scale, Abs.Bottom)));
		FSlateDrawElement::MakeText(Out, Layer + 4, TG, Text, Font, ESlateDrawEffect::None, DarkInk);
		Out.PopClip();
	}
	return Layer + 4;
}
