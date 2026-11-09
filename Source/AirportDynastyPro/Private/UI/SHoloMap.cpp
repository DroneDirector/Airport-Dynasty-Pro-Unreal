// The holographic airport map: drawn line by line in the Android edition's holographic blues (holomap.ts), with the
// route in warm orange.
#include "UI/SHoloMap.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#pragma region Colours
namespace
{
	const FLinearColor Bg(0.012f, 0.05f, 0.1f, 0.84f);
	const FLinearColor Grid(0.35f, 0.7f, 1.f, 0.07f);
	const FLinearColor Wall(0.55f, 0.84f, 1.f, 0.95f);
	const FLinearColor WallGlow(0.2f, 0.63f, 1.f, 0.3f);
	const FLinearColor GlassC(0.55f, 0.84f, 1.f, 0.45f);
	const FLinearColor Ghost(0.35f, 0.59f, 0.9f, 0.16f);
	const FLinearColor Mark(0.63f, 0.92f, 1.f, 0.85f);
	const FLinearColor RouteC(1.f, 0.67f, 0.24f, 0.95f);
	const FLinearColor RouteGlow(1.f, 0.6f, 0.2f, 0.3f);
	const FLinearColor Label(0.81f, 0.94f, 1.f, 1.f);
	constexpr float MiniMetres = 60.f; // the small map shows this much across
}
#pragma endregion

#pragma region Setup

void SHoloMap::Construct(const FArguments& Args) {}

void SHoloMap::SetPlan(const FTerminalPlan& InPlan)
{
	Plan = InPlan;
	Map.Build(Plan);
	for (int32 f = 0; f < 3; ++f) { Walls[f].Reset(); StairBoxes[f].Reset(); LiftBoxes[f].Reset(); }
	Bounds = FBox2f(ForceInit);
	// each room's sides as lines, with the doors' openings left out
	for (const FPlanRoom& R : Plan.Rooms)
	{
		Bounds += FVector2f(R.X0, R.Z0);
		Bounds += FVector2f(R.X1, R.Z1);
		const int32 F = FMath::Clamp(R.Floor, 1, 2);
		auto Side = [&](bool bAlongX, float Fixed, float From, float To, bool bGlass)
		{
			TArray<FVector2f> Gaps;
			for (const FPlanDoor& D : Plan.Doors)
			{
				if (D.Floor != R.Floor || D.bAlongX != bAlongX) continue;
				const float Line = bAlongX ? D.Z : D.X, At = bAlongX ? D.X : D.Z;
				if (FMath::Abs(Line - Fixed) > 0.35f || At < From || At > To) continue;
				Gaps.Add(FVector2f(At - D.Width / 2, At + D.Width / 2));
			}
			Gaps.Sort([](const FVector2f& A, const FVector2f& B) { return A.X < B.X; });
			float C = From;
			auto Add = [&](float A, float B)
			{
				if (B - A < 0.05f) return;
				Walls[F].Add(bAlongX ? FSeg{FVector2f(A, Fixed), FVector2f(B, Fixed), bGlass} : FSeg{FVector2f(Fixed, A), FVector2f(Fixed, B), bGlass});
			};
			for (const FVector2f& G : Gaps) { Add(C, G.X); C = FMath::Max(C, G.Y); }
			Add(C, To);
		};
		Side(true, R.Z0, R.X0, R.X1, R.Glass.Contains(TEXT("s")));
		Side(true, R.Z1, R.X0, R.X1, R.Glass.Contains(TEXT("n")));
		Side(false, R.X0, R.Z0, R.Z1, R.Glass.Contains(TEXT("w")));
		Side(false, R.X1, R.Z0, R.Z1, R.Glass.Contains(TEXT("e")));
	}
	for (const FPlanStair& S : Plan.Stairs)
		for (int32 f = 1; f <= 2; ++f) StairBoxes[f].Add(FBox2f(FVector2f(S.X0, S.Z0), FVector2f(S.X1, S.Z1)));
	for (const FPlanPoint& L : Plan.Lifts)
		for (int32 f = 1; f <= 2; ++f) LiftBoxes[f].Add(FBox2f(FVector2f(L.X - 1.06f, L.Z - 1.06f), FVector2f(L.X + 1.06f, L.Z + 1.06f)));
	Columns.Reset();
	for (const FPlanPoint& C : Plan.Columns) Columns.Add(FVector2f(C.X, C.Z));
	Bounds = Bounds.ExpandBy(6.f);
}

void SHoloMap::SetPlayer(const FVector& PlanPos, float YawDeg)
{
	Player = PlanPos;
	Yaw = YawDeg;
	// the route follows you and clears when you arrive
	if (Route.Num())
	{
		while (Route.Num() > 1 && FVector::Dist2D(FVector(Route[0].X, Route[0].Z, 0), FVector(Player.X, Player.Z, 0)) < 1.5f) Route.RemoveAt(0);
		if (FVector::Dist2D(FVector(Route.Last().X, Route.Last().Z, 0), FVector(Player.X, Player.Z, 0)) < 2.f) Route.Reset();
	}
}

void SHoloMap::SetFull(bool bOn) { bFull = bOn; }

#pragma endregion

#pragma region Placing

float SHoloMap::Scale(const FVector2f& Size) const
{
	if (!bFull) return Size.X / MiniMetres;
	const FVector2f E = Bounds.GetSize();
	return FMath::Min((Size.X - 40) / E.X, (Size.Y - 90) / E.Y);
}

FVector2f SHoloMap::ToLocal(const FVector2f& P, const FVector2f& Size) const
{
	const float S = Scale(Size);
	if (bFull)
	{
		const FVector2f E = Bounds.GetSize() * S;
		const FVector2f O((Size.X - E.X) / 2, 60 + (Size.Y - 70 - E.Y) / 2);
		return FVector2f(O.X + (P.X - Bounds.Min.X) * S, O.Y + (Bounds.Max.Y - P.Y) * S);
	}
	// turned so the way you face is up: Unreal yaw θ faces the plan's (cos θ, -sin θ)
	const float T = FMath::DegreesToRadians(Yaw);
	const FVector2f F(FMath::Cos(T), -FMath::Sin(T)), Rt(-FMath::Sin(T), -FMath::Cos(T));
	const FVector2f D(P.X - Player.X, P.Y - Player.Z);
	return Size * 0.5f + FVector2f(FVector2f::DotProduct(D, Rt), -FVector2f::DotProduct(D, F)) * S;
}

FVector2f SHoloMap::FromLocal(const FVector2f& L, const FVector2f& Size) const
{
	const float S = Scale(Size);
	const FVector2f E = Bounds.GetSize() * S;
	const FVector2f O((Size.X - E.X) / 2, 60 + (Size.Y - 70 - E.Y) / 2);
	return FVector2f(Bounds.Min.X + (L.X - O.X) / S, Bounds.Max.Y - (L.Y - O.Y) / S);
}

#pragma endregion

#pragma region Painting

int32 SHoloMap::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f Size(Geo.GetLocalSize());
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FPaintGeometry PG = Geo.ToPaintGeometry();
	FSlateDrawElement::MakeBox(Out, Layer, PG, White, ESlateDrawEffect::None, Bg);

	// the faint grid
	const float GridStep = bFull ? 40.f : 24.f;
	for (float X = 0; X < Size.X; X += GridStep) FSlateDrawElement::MakeLines(Out, Layer + 1, PG, TArray<FVector2f>{FVector2f(X, 0), FVector2f(X, Size.Y)}, ESlateDrawEffect::None, Grid, false, 1);
	for (float Y = 0; Y < Size.Y; Y += GridStep) FSlateDrawElement::MakeLines(Out, Layer + 1, PG, TArray<FVector2f>{FVector2f(0, Y), FVector2f(Size.X, Y)}, ESlateDrawEffect::None, Grid, false, 1);

	const int32 F = FloorOf(Player.Y), Other = F == 1 ? 2 : 1;
	auto OnScreen = [&](const FVector2f& L) { return L.X > -40 && L.Y > -40 && L.X < Size.X + 40 && L.Y < Size.Y + 40; };
	auto Line = [&](const FVector2f& A, const FVector2f& B, const FLinearColor& C, float T, const FVector2f& Off = FVector2f::ZeroVector)
	{
		const FVector2f LA = ToLocal(A, Size) + Off, LB = ToLocal(B, Size) + Off;
		if (!OnScreen(LA) && !OnScreen(LB)) return;
		FSlateDrawElement::MakeLines(Out, Layer + 2, PG, TArray<FVector2f>{LA, LB}, ESlateDrawEffect::None, C, true, T);
	};
	auto Rect = [&](const FBox2f& B, const FLinearColor& C, float T, const FVector2f& Off = FVector2f::ZeroVector)
	{
		Line(B.Min, FVector2f(B.Max.X, B.Min.Y), C, T, Off); Line(FVector2f(B.Max.X, B.Min.Y), B.Max, C, T, Off);
		Line(B.Max, FVector2f(B.Min.X, B.Max.Y), C, T, Off); Line(FVector2f(B.Min.X, B.Max.Y), B.Min, C, T, Off);
	};

	// the other floor, a faint ghost a little below
	for (const FSeg& S : Walls[Other]) Line(S.A, S.B, Ghost, 1, FVector2f(0, (F - Other) * 7.f));
	// this floor: a soft glow under bright walls, glass lighter
	for (const FSeg& S : Walls[F]) Line(S.A, S.B, WallGlow, 4.f);
	for (const FSeg& S : Walls[F]) Line(S.A, S.B, S.bGlass ? GlassC : Wall, 1.6f);
	for (const FBox2f& B : StairBoxes[F]) Rect(B, Mark, 1);
	for (const FBox2f& B : LiftBoxes[F]) Rect(B, Mark, 1);
	if (F == 1) for (const FVector2f& C : Columns) Rect(FBox2f(C - FVector2f(0.4f), C + FVector2f(0.4f)), Mark, 1);

	// the rooms' names, full screen only
	if (bFull)
	{
		const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 8);
		for (const FPlanRoom& R : Plan.Rooms)
		{
			if (R.Floor != F || (R.X1 - R.X0) * (R.Z1 - R.Z0) < 120) continue;
			FString Name = R.Name;
			int32 Paren;
			if (Name.FindChar(TEXT('('), Paren)) Name = Name.Left(Paren).TrimEnd();
			const FVector2f C = ToLocal(FVector2f((R.X0 + R.X1) / 2, (R.Z0 + R.Z1) / 2), Size);
			FSlateDrawElement::MakeText(Out, Layer + 3, Geo.ToPaintGeometry(FVector2f(160, 14), FSlateLayoutTransform(FVector2f(C.X - Name.Len() * 2.6f, C.Y - 6))), Name.ToUpper(), Small, ESlateDrawEffect::None, FLinearColor(0.63f, 0.84f, 1.f, 0.7f));
		}
	}

	// the route: bright on this floor, faint on the other
	if (Route.Num())
	{
		FVector Prev = Player;
		for (const FVector& P : Route)
		{
			const bool bMine = FloorOf(P.Y) == F && FloorOf(Prev.Y) == F;
			Line(FVector2f(Prev.X, Prev.Z), FVector2f(P.X, P.Z), bMine ? RouteGlow : FLinearColor(1, 0.75f, 0.4f, 0.12f), bMine ? 6.f : 3.f);
			if (bMine) Line(FVector2f(Prev.X, Prev.Z), FVector2f(P.X, P.Z), RouteC, 2.f);
			Prev = P;
		}
	}

	// you: an arrow with your view cone
	{
		const FVector2f C = ToLocal(FVector2f(Player.X, Player.Z), Size);
		const float T = FMath::DegreesToRadians(Yaw);
		// in the small map you always face up; full screen the arrow turns
		const float A = bFull ? FMath::Atan2(FMath::Sin(T), FMath::Cos(T)) : -HALF_PI;
		auto Dir = [&](float Ang, float Len) { return C + FVector2f(FMath::Cos(Ang), FMath::Sin(Ang)) * Len; };
		for (const float S : {-0.6f, 0.6f})
			FSlateDrawElement::MakeLines(Out, Layer + 4, PG, TArray<FVector2f>{C, Dir(A + S, 34)}, ESlateDrawEffect::None, FLinearColor(0.6f, 0.9f, 1.f, 0.35f), true, 1);
		FSlateDrawElement::MakeLines(Out, Layer + 4, PG, TArray<FVector2f>{Dir(A, 9), Dir(A + 2.5f, 6), Dir(A + PI, 2), Dir(A - 2.5f, 6), Dir(A, 9)}, ESlateDrawEffect::None, FLinearColor::White, true, 2.f);
	}

	// the floor shown, and the corner brackets
	const FString FloorName = F == 2 ? TEXT("FLOOR 2") : TEXT("FLOOR 1");
	FSlateDrawElement::MakeText(Out, Layer + 5, Geo.ToPaintGeometry(FVector2f(200, 18), FSlateLayoutTransform(FVector2f(10, 8))), bFull ? FloorName + TEXT("  ·  CLICK WHERE YOU WANT TO GO  ·  M CLOSES") : FloorName, FCoreStyle::GetDefaultFontStyle("Bold", bFull ? 12 : 9), ESlateDrawEffect::None, Label);
	const float B = bFull ? 26.f : 16.f;
	for (const FVector2f& Corner : {FVector2f(0, 0), FVector2f(Size.X, 0), FVector2f(0, Size.Y), FVector2f(Size.X, Size.Y)})
	{
		const float SX = Corner.X > 0 ? -1.f : 1.f, SY = Corner.Y > 0 ? -1.f : 1.f;
		const FVector2f P = Corner + FVector2f(SX * 3, SY * 3);
		FSlateDrawElement::MakeLines(Out, Layer + 5, PG, TArray<FVector2f>{P + FVector2f(SX * B, 0), P, P + FVector2f(0, SY * B)}, ESlateDrawEffect::None, Wall, true, 2);
	}
	return Layer + 5;
}

#pragma endregion

#pragma region Clicking

FReply SHoloMap::OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event)
{
	if (!bFull)
	{
		if (OnOpen) OnOpen();
		return FReply::Handled();
	}
	// full screen: a route from you to where you clicked, on the floor you are looking at
	const FVector2f Size(Geo.GetLocalSize());
	const FVector2f L(Geo.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	const FVector2f P = FromLocal(L, Size);
	const int32 F = FloorOf(Player.Y);
	auto PlaceAt = [&](float X, float Z, int32 Floor)
	{
		for (int32 i = 0; i < Map.Places.Num(); ++i)
		{
			const FWalkMap::FPlace& Q = Map.Places[i];
			if (!Q.bOutside && FloorOf(Q.Y) == Floor && X >= Q.X0 && X <= Q.X1 && Z >= Q.Z0 && Z <= Q.Z1) return i;
		}
		return int32(INDEX_NONE);
	};
	const int32 From = PlaceAt(Player.X, Player.Z, F), To = PlaceAt(P.X, P.Y, F);
	if (From != INDEX_NONE && To != INDEX_NONE)
		Map.Way(From, Player, To, FVector(P.X, Map.Places[To].Y, P.Y), Route);
	return FReply::Handled();
}

#pragma endregion
