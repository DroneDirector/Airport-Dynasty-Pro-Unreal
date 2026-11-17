// The neon loading screen (SNeonLoading.h): drawn element by element so it looks like the web one (styles.css .ln-*).
#include "UI/SNeonLoading.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"

namespace
{
	// the title screen's colours (styles.css: --neo1 pink #FF3FB8, --neo2 cyan #35F0FF, the violet #7C5CFF, paper #EDE8FF)
	const FLinearColor Pink = FLinearColor::FromSRGBColor(FColor(0xFF, 0x3F, 0xB8));
	const FLinearColor Cyan = FLinearColor::FromSRGBColor(FColor(0x35, 0xF0, 0xFF));
	const FLinearColor Violet = FLinearColor::FromSRGBColor(FColor(0x7C, 0x5C, 0xFF));
	const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(0xED, 0xE8, 0xFF));
	FLinearColor A(const FLinearColor& C, float Alpha) { return FLinearColor(C.R, C.G, C.B, Alpha); }
	FLinearColor Hex(uint8 R, uint8 G, uint8 B, float Alpha = 1.f) { FLinearColor C = FLinearColor::FromSRGBColor(FColor(R, G, B)); C.A = Alpha; return C; }

	FSlateFontInfo Face(int32 Size, int32 Spacing)
	{
		static const FString Path = FPaths::ProjectContentDir() / TEXT("Fonts/Audiowide-Regular.ttf");
		static const bool bHave = FPaths::FileExists(Path);
		FSlateFontInfo F = bHave ? FSlateFontInfo(Path, Size) : FCoreStyle::GetDefaultFontStyle("Bold", Size);
		F.LetterSpacing = Spacing;
		return F;
	}
	FVector2D Measure(const FString& S, const FSlateFontInfo& F) { return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, F); }
}

const TArray<FString>& SNeonLoading::Tips()
{
	// (the same tips as loading-neon.ts)
	static const TArray<FString> T = {
		TEXT("Hire the crew your first aircraft needs before you look at a second one."),
		TEXT("A used aircraft flies as well as a new one, with more hours on the clock and a lower price."),
		TEXT("Fuel bought at your bases is cheaper than fuel bought on the road."),
		TEXT("Your mechanics keep the fleet in condition; grounded aircraft earn nothing."),
		TEXT("Passengers remember delays. Keep a spare crew for the days somebody calls in sick."),
		TEXT("Loans are cheap on Easy and dear on Hard: borrow with the difficulty in mind."),
		TEXT("Your office wardrobe holds every outfit made for you: change there before a big day."),
		TEXT("Autosave keeps a copy every ten minutes; Save game keeps the one you choose."),
		TEXT("A route pays when the aircraft fits it: not too big, not too small."),
		TEXT("Check the weather before the first departure: the Faroes have their own ideas."),
	};
	return T;
}

void SNeonLoading::Construct(const FArguments& Args)
{
	Kicker = Args._Kicker; Hint = Args._Hint;
	Title = Args._Title; Step = Args._Step; Fraction = Args._Fraction; Done = Args._Done;
	T0 = FPlatformTime::Seconds();
	ForceVolatile(true);   // (it moves every frame: the sheen, the tips, the bar)
}

int32 SNeonLoading::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	const FVector2D S = Geo.GetLocalSize();
	const double Now = FPlatformTime::Seconds();
	const float T = float(Now - T0);
	const float Dt = LastPaint > 0 ? FMath::Min(float(Now - LastPaint), 0.1f) : 0.f;
	LastPaint = Now;
	const bool bDone = Done.Get(false);
	auto Box = [&](int32 L, const FVector2D& At, const FVector2D& Sz, const FLinearColor& C)
	{
		FSlateDrawElement::MakeBox(Out, L, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(At)), White, ESlateDrawEffect::None, C);
	};
	auto Grad = [&](int32 L, const FVector2D& At, const FVector2D& Sz, const TArray<FSlateGradientStop>& Stops, EOrientation O)
	{
		FSlateDrawElement::MakeGradient(Out, L, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(At)), Stops, O);
	};
	// text, optionally with a neon glow: the same words drawn round themselves, faint, in the glow's colours
	auto Text = [&](int32 L, const FString& W, const FSlateFontInfo& F, const FVector2D& At, const FLinearColor& C, const FLinearColor& G1, const FLinearColor& G2, float Glow)
	{
		const FVector2D Sz = Measure(W, F);
		if (Glow > 0.f)
			for (int32 Ring = 3; Ring >= 1; --Ring)
				for (int32 K = 0; K < 8; ++K)
				{
					const float R = Glow * Ring, Ang = K * PI / 4.f;
					const FLinearColor GC = Ring == 1 ? A(FLinearColor::White, 0.22f) : A(Ring == 2 ? G1 : G2, Ring == 2 ? 0.13f : 0.07f);
					FSlateDrawElement::MakeText(Out, L, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(At + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * R)), W, F, ESlateDrawEffect::None, GC);
				}
		FSlateDrawElement::MakeText(Out, L + 1, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(At)), W, F, ESlateDrawEffect::None, C);
		return Sz;
	};

	// the background: radial-gradient(#1a0c3a at the upper left, #06081c, #03020a) - a top-to-bottom fade with a violet
	// glow from the left
	Grad(Layer, FVector2D::ZeroVector, S, {FSlateGradientStop(FVector2D(0, 0), Hex(0x1a, 0x0c, 0x3a)), FSlateGradientStop(FVector2D(0, S.Y * 0.55f), Hex(0x06, 0x08, 0x1c)), FSlateGradientStop(FVector2D(0, S.Y), Hex(0x03, 0x02, 0x0a))}, Orient_Horizontal);
	Grad(Layer + 1, FVector2D::ZeroVector, S, {FSlateGradientStop(FVector2D(0, 0), Hex(0x1a, 0x0c, 0x3a, 0.55f)), FSlateGradientStop(FVector2D(S.X * 0.6f, 0), Hex(0x06, 0x08, 0x1c, 0.f))}, Orient_Vertical);
	// the scan lines (6 px apart, a faint cyan)
	for (float Y = 3.f; Y < S.Y; Y += 6.f) Box(Layer + 2, FVector2D(0, Y), FVector2D(S.X, 1.f), A(Cyan, 0.018f));

	// the box: min(1100 px, 76 % of the width), the content centred on the screen
	const float Scale = FMath::Clamp(S.Y / 1080.f, 0.6f, 2.f);
	const float W = FMath::Min(1100.f * Scale, S.X * 0.76f), X0 = (S.X - W) * 0.5f;
	const FSlateFontInfo KickF = Face(FMath::RoundToInt(12 * Scale), 500), TitleF = Face(FMath::RoundToInt(FMath::Clamp(S.Y * 0.046f, 26.f, 42.f * Scale)), 80),
		LineF = Face(FMath::RoundToInt(12.5f * Scale), 180), TipKF = Face(FMath::RoundToInt(11 * Scale), 500), TipF = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(FMath::Clamp(S.Y * 0.022f, 14.f, 19.f * Scale)));
	const FString TitleS = (bDone ? FString(TEXT("Loading complete")) : Title.Get(TEXT("Loading"))).ToUpper();
	const float KickH = Measure(TEXT("A"), KickF).Y, TitleH = Measure(TEXT("A"), TitleF).Y, BarH = 14.f * Scale, LineH = Measure(TEXT("A"), LineF).Y;
	const float Total = KickH + 4 * Scale + TitleH + 26 * Scale + BarH + 12 * Scale + LineH;
	float Y = (S.Y - Total) * 0.5f;
	// (fades in over 0.7 s, as the web one does)
	const float In = FMath::Clamp(T / 0.7f, 0.f, 1.f);

	// the kicker: cyan, widely spaced
	Text(Layer + 3, Kicker.ToUpper(), KickF, FVector2D(X0, Y), A(Cyan, In), A(Cyan, In), A(Cyan, In), 1.2f * Scale);
	Y += KickH + 4 * Scale;
	// the title: white with a pink-cyan glow; done: the cyan glow
	Text(Layer + 3, TitleS, TitleF, FVector2D(X0, Y), A(FLinearColor::White, In), bDone ? Cyan : Pink, bDone ? Cyan : Pink, 3.f * Scale);
	Y += TitleH + 26 * Scale;

	// the bar: the dark track with a faint blue frame and a cyan halo, the fill cyan > violet > pink with its own glow,
	// and the sheen running over it
	const float Want = FMath::Clamp(Fraction.Get(0.f), 0.f, 1.f);
	Shown = FMath::Max(Shown, Dt > 0 ? FMath::Min(Want, Shown + (Want - Shown) * FMath::Min(1.f, Dt * 6.f) + Dt * 0.02f) : Want);
	if (bDone) Shown = 1.f;
	for (int32 K = 3; K >= 1; --K) Box(Layer + 4, FVector2D(X0 - K * 3 * Scale, Y - K * 3 * Scale), FVector2D(W + K * 6 * Scale, BarH + K * 6 * Scale), A(Cyan, 0.025f));
	Box(Layer + 5, FVector2D(X0 - 1, Y - 1), FVector2D(W + 2, BarH + 2), Hex(0x8c, 0xbe, 0xff, 0.3f));
	Box(Layer + 6, FVector2D(X0, Y), FVector2D(W, BarH), Hex(0x0e, 0x0a, 0x28, 0.95f));
	const float Fill = W * Shown;
	if (Fill > 1.f)
	{
		for (int32 K = 3; K >= 1; --K) Box(Layer + 7, FVector2D(X0, Y - K * 2 * Scale), FVector2D(Fill, BarH + K * 4 * Scale), A(K == 1 ? Cyan : Pink, 0.06f));
		// the gradient over the whole bar's width (as the CSS fill), shown only as far as the fill: clipped
		const FSlateRect Abs = Geo.GetLayoutBoundingRect();
		const float GS = Geo.GetAccumulatedLayoutTransform().GetScale();
		Out.PushClip(FSlateClippingZone(FSlateRect(Abs.Left + X0 * GS, Abs.Top + Y * GS, Abs.Left + (X0 + Fill) * GS, Abs.Top + (Y + BarH) * GS)));
		Grad(Layer + 8, FVector2D(X0, Y), FVector2D(W, BarH), {FSlateGradientStop(FVector2D(0, 0), Cyan), FSlateGradientStop(FVector2D(W * 0.55f, 0), Violet), FSlateGradientStop(FVector2D(W, 0), Pink)}, Orient_Vertical);
		// the sheen: 28 % of the bar, crossing it every 1.6 s (inside the same clip)
		const float SW = W * 0.28f, SX = FMath::Fmod(T / 1.6f, 1.f) * (W + SW) - SW;
		Grad(Layer + 9, FVector2D(X0 + SX, Y), FVector2D(SW, BarH), {FSlateGradientStop(FVector2D(0, 0), A(FLinearColor::White, 0.f)), FSlateGradientStop(FVector2D(SW * 0.5f, 0), A(FLinearColor::White, 0.35f)), FSlateGradientStop(FVector2D(SW, 0), A(FLinearColor::White, 0.f))}, Orient_Vertical);
		Out.PopClip();
	}
	Y += BarH + 12 * Scale;

	// the line under it (86 % wide): what is loading, and the percentage at its end
	const float LW = W * 0.86f, LX = X0 + (W - LW) * 0.5f;
	const FString StepS = (bDone ? FString(TEXT("Get ready for takeoff")) : Step.Get(FString())).ToUpper();
	Text(Layer + 3, StepS, LineF, FVector2D(LX, Y), bDone ? FLinearColor::White : Cyan, Cyan, Cyan, bDone ? 1.6f * Scale : 1.f * Scale);
	const FString Pct = FString::Printf(TEXT("%d%%"), FMath::FloorToInt(Shown * 100.f));
	const FVector2D PS = Measure(Pct, LineF);
	Text(Layer + 3, Pct, LineF, FVector2D(LX + LW - PS.X, Y), FLinearColor::White, Pink, Pink, 1.4f * Scale);

	// the tips at the bottom: "TIP" in pink, the tip under it, a new one every 5 s fading in (8 px up)
	const TArray<FString>& Tp = Tips();
	const int32 Idx = int32(T / 5.f) % Tp.Num();
	const float Age = FMath::Fmod(T, 5.f), Fade = FMath::Clamp(Age / 0.6f, 0.f, 1.f), Ease = 1.f - FMath::Pow(1.f - Fade, 3.f);
	const float Bottom = S.Y - FMath::Clamp(S.Y * 0.06f, 28.f, 64.f * Scale);
	const FVector2D TipS = Measure(Tp[Idx], TipF), TK = Measure(TEXT("TIP"), TipKF);
	const float TipY = Bottom - TipS.Y;
	Text(Layer + 3, TEXT("TIP"), TipKF, FVector2D((S.X - TK.X) * 0.5f, TipY - TK.Y - 8 * Scale), Pink, Pink, Pink, 1.2f * Scale);
	Text(Layer + 3, Tp[Idx], TipF, FVector2D((S.X - TipS.X) * 0.5f, TipY + (1.f - Ease) * 8.f * Scale), A(Paper, 0.9f * Ease), Paper, Paper, 0.f);
	// the hint, small, under everything
	if (!Hint.IsEmpty())
	{
		const FSlateFontInfo HF = Face(FMath::RoundToInt(9 * Scale), 200);
		const FVector2D HS = Measure(Hint.ToUpper(), HF);
		Text(Layer + 3, Hint.ToUpper(), HF, FVector2D((S.X - HS.X) * 0.5f, S.Y - HS.Y - 10 * Scale), A(Paper, 0.45f), Paper, Paper, 0.f);
	}
	return Layer + 10;
}
