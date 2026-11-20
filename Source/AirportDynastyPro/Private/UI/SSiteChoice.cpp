// "Where is your airport?" (SSiteChoice.h).
#include "UI/SSiteChoice.h"
#include "Site.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"

namespace ADPSiteChoiceLook
{
	// the title screen's colours (as SNeonLoading)
	const FLinearColor Pink = FLinearColor::FromSRGBColor(FColor(0xFF, 0x3F, 0xB8));
	const FLinearColor Cyan = FLinearColor::FromSRGBColor(FColor(0x35, 0xF0, 0xFF));
	const FLinearColor Violet = FLinearColor::FromSRGBColor(FColor(0x7C, 0x5C, 0xFF));
	const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(0xED, 0xE8, 0xFF));
	FLinearColor Al(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, A); }
	FSlateFontInfo Font(int32 Size, int32 Spacing = 0)
	{
		static const FString Path = FPaths::ProjectContentDir() / TEXT("Fonts/Audiowide-Regular.ttf");
		static const bool bHave = FPaths::FileExists(Path);
		FSlateFontInfo F = bHave ? FSlateFontInfo(Path, Size) : FCoreStyle::GetDefaultFontStyle("Bold", Size);
		F.LetterSpacing = Spacing;
		return F;
	}
	FSlateFontInfo Body(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }
	FVector2D Measure(const FString& S, const FSlateFontInfo& F) { return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, F); }
}

void SSiteChoice::Construct(const FArguments& Args)
{
	for (const FString& Id : ADPSite::All())
	{
		FCard C; C.Id = Id; C.Name = ADPSite::Name(Id); C.Place = ADPSite::Place(Id);
		C.Line = Id == ADPSite::DefaultId() ? TEXT("On the black sands of south Iceland, between the glaciers and the North Atlantic")
			: Id == TEXT("chablais") ? TEXT("On the Rhône plain at the end of Lake Geneva, between the Alps of Vaud, Valais and Savoy")
			: FString();
		Cards.Add(C);
	}
	Hover = FMath::Max(0, Cards.IndexOfByPredicate([](const FCard& C) { return C.Id == ADPSite::Id(); }));
	T0 = FPlatformTime::Seconds();
	ForceVolatile(true);   // (the glow breathes)
}

int32 SSiteChoice::HitAt(const FGeometry& Geo, const FVector2D& Screen) const
{
	const FVector2D L = Geo.AbsoluteToLocal(Screen);
	for (int32 i = 0; i < Rects.Num(); ++i) if (Rects[i].ContainsPoint(L)) return i;
	return INDEX_NONE;
}

FReply SSiteChoice::OnMouseMove(const FGeometry& Geo, const FPointerEvent& E)
{
	const int32 H = HitAt(Geo, E.GetScreenSpacePosition());
	if (H != INDEX_NONE) Hover = H;
	return FReply::Handled();
}

FReply SSiteChoice::OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E)
{
	const int32 H = HitAt(Geo, E.GetScreenSpacePosition());
	if (H != INDEX_NONE && OnPick) OnPick(Cards[H].Id);
	return FReply::Handled();
}

FReply SSiteChoice::OnKeyDown(const FGeometry& Geo, const FKeyEvent& K)
{
	const FKey Key = K.GetKey();
	if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left) Hover = FMath::Max(0, Hover - 1);
	else if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right) Hover = FMath::Min(Cards.Num() - 1, Hover + 1);
	else if ((Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom) && Cards.IsValidIndex(Hover)) { if (OnPick) OnPick(Cards[Hover].Id); }
	else if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right) { if (OnBack) OnBack(); }
	else return FReply::Unhandled();
	return FReply::Handled();
}

int32 SSiteChoice::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	using namespace ADPSiteChoiceLook;
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	const FVector2D S = Geo.GetLocalSize();
	const float K = FMath::Clamp(S.Y / 1080.f, 0.5f, 2.5f);   // (sized to the screen's height: ultrawide keeps it centred)
	const float Breath = 0.5f + 0.5f * FMath::Sin(float(FPlatformTime::Seconds() - T0) * 2.2f);
	auto Box = [&](FVector2D P, FVector2D Sz, const FLinearColor& C, int32 L)
	{ FSlateDrawElement::MakeBox(Out, L, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(P)), White, ESlateDrawEffect::None, C); };
	auto Text = [&](const FString& T, const FSlateFontInfo& F, FVector2D P, const FLinearColor& C, int32 L)
	{ FSlateDrawElement::MakeText(Out, L, Geo.ToPaintGeometry(Measure(T, F), FSlateLayoutTransform(P)), T, F, ESlateDrawEffect::None, C); };
	auto Centred = [&](const FString& T, const FSlateFontInfo& F, float CX, float Y, const FLinearColor& C, int32 L, float Glow = 0.f)
	{
		const FVector2D M = Measure(T, F); const FVector2D P(CX - M.X / 2, Y);
		if (Glow > 0) for (int32 i = 3; i >= 1; --i) for (const FVector2D& O : {FVector2D(-i, 0), FVector2D(i, 0), FVector2D(0, -i), FVector2D(0, i)})
			Text(T, F, P + O * K, Al(C, Glow * 0.12f), L);
		Text(T, F, P, C, L + 1);
	};
	// the stage behind, darkened (the 3D world stays faintly there)
	Box(FVector2D::ZeroVector, S, FLinearColor(0.02f, 0.0f, 0.06f, 0.82f), Layer);
	for (float Y = 0; Y < S.Y; Y += 4 * K) Box(FVector2D(0, Y), FVector2D(S.X, 1), FLinearColor(1, 1, 1, 0.018f), Layer + 1);
	Centred(TEXT("A I R P O R T   D Y N A S T Y   P R O   ·   N E W   C A R E E R"), Font(int32(13 * K), 0), S.X / 2, S.Y * 0.16f, Cyan, Layer + 2);
	Centred(TEXT("Where is your airport?"), Font(int32(46 * K), 0), S.X / 2, S.Y * 0.21f, Paper, Layer + 2, 1.f);
	// the cards
	const int32 N = FMath::Max(1, Cards.Num());
	const float CW = 520 * K, CH = 300 * K, Gap = 60 * K;
	const float X0 = S.X / 2 - (N * CW + (N - 1) * Gap) / 2, Y0 = S.Y * 0.38f;
	Rects.SetNum(Cards.Num());
	for (int32 i = 0; i < Cards.Num(); ++i)
	{
		const FCard& C = Cards[i]; const bool bOn = i == Hover;
		const FLinearColor Edge = bOn ? Pink : Violet;
		const FVector2D P(X0 + i * (CW + Gap), Y0 - (bOn ? 8 * K : 0)), Sz(CW, CH);
		Rects[i] = FSlateRect(P.X, P.Y, P.X + CW, P.Y + CH);
		// glow round the edge, then the card
		for (int32 g = 5; g >= 1; --g)
		{
			const float A = (bOn ? 0.10f + 0.06f * Breath : 0.04f) * (6 - g) / 5.f; const float o = g * 3 * K;
			Box(P - FVector2D(o, o), Sz + FVector2D(2 * o, 2 * o), Al(Edge, A), Layer + 3);
		}
		Box(P, Sz, FLinearColor(0.05f, 0.02f, 0.12f, 0.96f), Layer + 4);
		const float B = 2 * K;
		Box(P, FVector2D(CW, B), Edge, Layer + 5); Box(P + FVector2D(0, CH - B), FVector2D(CW, B), Edge, Layer + 5);
		Box(P, FVector2D(B, CH), Edge, Layer + 5); Box(P + FVector2D(CW - B, 0), FVector2D(B, CH), Edge, Layer + 5);
		Centred(C.Place.ToUpper(), Font(int32(14 * K), 0), P.X + CW / 2, P.Y + 34 * K, bOn ? Cyan : Al(Cyan, 0.7f), Layer + 6);
		Centred(C.Name, Font(int32(30 * K), 0), P.X + CW / 2, P.Y + 70 * K, Paper, Layer + 6, bOn ? 0.8f : 0.f);
		// the line, wrapped by hand to the card
		{
			const FSlateFontInfo F = Body(int32(13 * K)); TArray<FString> Words; C.Line.ParseIntoArray(Words, TEXT(" "));
			FString Cur; float Y = P.Y + 140 * K;
			for (int32 w = 0; w <= Words.Num(); ++w)
			{
				const FString Try = w < Words.Num() ? (Cur.IsEmpty() ? Words[w] : Cur + TEXT(" ") + Words[w]) : FString();
				if (w == Words.Num() || Measure(Try, F).X > CW - 60 * K) { Centred(Cur, F, P.X + CW / 2, Y, Al(Paper, 0.82f), Layer + 6); Y += 24 * K; Cur = w < Words.Num() ? Words[w] : FString(); }
				else Cur = Try;
			}
		}
		if (C.Id == ADPSite::Id()) Centred(TEXT("loaded now"), Body(int32(11 * K)), P.X + CW / 2, P.Y + CH - 40 * K, Al(Cyan, 0.75f), Layer + 6);
		else Centred(TEXT("the world loads again"), Body(int32(11 * K)), P.X + CW / 2, P.Y + CH - 40 * K, Al(Paper, 0.5f), Layer + 6);
	}
	Centred(TEXT("← →  choose     Enter  go     Esc  back"), Body(int32(13 * K)), S.X / 2, S.Y * 0.88f, Al(Paper, 0.6f), Layer + 2);
	return Layer + 8;
}
