// The terminal's timetable screens (the model's own boards: the big walls, the totems, the wide screens and the Operations
// wall): drawn here, natively, from the simulation's real flights (ADPBoards), page after page, into one picture per kind,
// shown on every screen of that kind (their imported materials, fids-<kind><shape>, get the picture; lit, as screens are).
// Drawn again only when what they show changes: a row, the minute, the page. No flights: empty rows (to-do 286).
// The gates' own screens and the operations monitors still come as pictures from the menus (OnBoard).
#include "UI/ADPHUD.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SLeafWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "PerfLog.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSim.h"
#include "Site.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/Base64.h"
#include "Async/Async.h"
#include "Tasks/Pipe.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#if WITH_EDITOR
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#endif

// A timetable screen sunk inside its own housing (the true-Unreal terminal's copy came out mirrored: the screen 5 cm in front
// of its 12 cm box in the Babylon edition ended up inside it - the departures wall at Chablais showed as a dark panel,
// 2026-11-21): a screen with something right in front of it on BOTH sides is moved to 1.5 cm in front of the nearer one
// (the housing's face), with a copy turned round, so whichever way the flat screen faces, one of them faces the hall.
// A screen that is seen from one side already (in front of a wall) is left as it is.
// bTotem: the hall's standing screens have a face on either side, a centimetre in front of their housing - nothing to bring out,
// but they are seen from the housing's side only (the housing stayed black): each gets the turned copy too, a little further out
static void BdBringOut(UWorld* W, UInstancedStaticMeshComponent* K, bool bTotem)
{
	if (!W || !K || !K->GetStaticMesh() || K->ComponentTags.Contains(TEXT("BoardOut"))) return;
	K->ComponentTags.Add(TEXT("BoardOut"));
	const FBox Lb = K->GetStaticMesh()->GetBoundingBox();
	const int32 N0 = K->GetInstanceCount();
	int32 Moved = 0;
	for (int32 i = 0; i < N0; ++i)
	{
		FTransform X; if (!K->GetInstanceTransform(i, X, true)) continue;
		const FVector E = Lb.GetExtent() * X.GetScale3D().GetAbs();
		const FVector Ax = E.X <= E.Y && E.X <= E.Z ? FVector::XAxisVector : (E.Y <= E.Z ? FVector::YAxisVector : FVector::ZAxisVector);
		const FVector Nrm = X.TransformVectorNoScale(Ax).GetSafeNormal(), C = X.TransformPosition(Lb.GetCenter());
		FCollisionQueryParams Q(TEXT("BoardOut"), false); Q.AddIgnoredComponent(K);
		float Near[2] = {-1.f, -1.f};
		for (int32 sd = 0; sd < 2; ++sd)
		{
			const FVector Dir = sd ? -Nrm : Nrm; FHitResult H;
			if (W->LineTraceSingleByChannel(H, C + Dir * 40.f, C, ECC_Visibility, Q)) Near[sd] = (H.ImpactPoint - C).Size();
		}
		const bool bIn = Near[0] >= 0.f && Near[1] >= 0.f;
		if (!bIn && !bTotem) continue;   // (seen from one side: fine)
		const int32 sd = Near[0] <= Near[1] ? 0 : 1; const FVector Dir = sd ? -Nrm : Nrm;   // (nothing before it, -1, counts as nearest)
		// (a totem's screen in front of its housing: half a centimetre more; with neither side known, where it is)
		const FVector Shift = bIn ? Dir * (Near[sd] + 1.5f) : (Near[0] < 0.f) != (Near[1] < 0.f) ? Dir * 0.5f : FVector::ZeroVector;
		X.AddToTranslation(Shift);
		// (the mirrored copy also turned the picture upside down: the screen's height axis flipped back, round its middle)
		{
			int32 Up = 0; double Best = -1.0;
			for (int32 a = 0; a < 3; ++a) { const double Zc = FMath::Abs(X.TransformVectorNoScale(a == 0 ? FVector::XAxisVector : a == 1 ? FVector::YAxisVector : FVector::ZAxisVector).Z); if (Zc > Best) { Best = Zc; Up = a; } }
			FVector Sc = X.GetScale3D(); Sc[Up] = -Sc[Up]; X.SetScale3D(Sc);
			X.SetTranslation(X.GetTranslation() + (C + Shift) - X.TransformPosition(Lb.GetCenter()));
		}
		K->UpdateInstanceTransform(i, X, true, false, true);
		FTransform Y = X; Y.SetRotation(FQuat(Nrm.GetSafeNormal().Cross(FVector::UpVector).IsNearlyZero() ? FVector::ForwardVector : FVector::UpVector, PI) * X.GetRotation());
		// (the turned copy turns round the screen's own middle)
		Y.SetTranslation(X.GetTranslation() + (C + Shift) - Y.TransformPosition(Lb.GetCenter()));
		K->AddInstance(Y, true); ++Moved;
	}
	K->MarkRenderStateDirty();
	if (Moved) ADP_LOG(TEXT("Boards: %d screen(s) of %s brought out of their housing"), Moved, *K->GetStaticMesh()->GetName());
}

#pragma region The timetables, drawn here
namespace
{
	struct FBdKind { const TCHAR* Key; int32 W, H, Rows; bool bArr, bFull; };
	const FBdKind BdKinds[] = {
		{TEXT("depwall"), 2048, 745, 10, false, true}, {TEXT("arrwall"), 2048, 745, 10, true, true},
		{TEXT("deptotem"), 1024, 760, 8, false, false}, {TEXT("arrtotem"), 1024, 760, 8, true, false},
		{TEXT("depwide"), 1024, 576, 8, false, false}, {TEXT("arrwide"), 1024, 576, 8, true, false},
		{TEXT("opswall"), 4096, 722, 11, false, false}};   // (six monitors side by side, departures and arrivals in turn)
	constexpr int32 BdOps = 6, BdOpsPanels = 6;

	struct FBdRow { FString Cell[6]; FColor Col; };
	struct FBdPanel { bool bArr = false; float X = 0.f, W = 0.f; TArray<FBdRow> Rows; FString Note; };
	struct FBdPicture { int32 W = 0, H = 0, MaxRows = 0; bool bFull = false, bLive = false; FString Clock; TArray<FBdPanel> Panels; };

	/** One kind's picture: the same look as the model's own boards always had (title, clock, column heads, rows, statuses). */
	class SBdBoard : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBdBoard) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&, const FBdPicture& In) { P = In; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(P.W, P.H); }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			Brushes.Reset();
			const TSharedRef<FSlateFontMeasure> Meas = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
			const auto Geo = [&G](float X, float Y, float W, float H) { return G.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))); };
			const auto Hex = [](uint32 C) { return FLinearColor(FColor((C >> 16) & 255, (C >> 8) & 255, C & 255)); };
			const auto Box = [&](float X, float Y, float W, float H, const FLinearColor& C) { FSlateDrawElement::MakeBox(Out, ++Layer, Geo(X, Y, W, H), White, ESlateDrawEffect::None, C); };
			const auto Round = [&](float X, float Y, float W, float H, const FLinearColor& C, float Rad, const FLinearColor& Edge, float Thick)
			{
				Brushes.Add(MakeUnique<FSlateRoundedBoxBrush>(C, Rad, Edge, Thick));
				FSlateDrawElement::MakeBox(Out, ++Layer, Geo(X, Y, W, H), Brushes.Last().Get(), ESlateDrawEffect::None, FLinearColor::White);
			};
			const auto Grad = [&](float X, float Y, float W, float H, std::initializer_list<TPair<float, FLinearColor>> Stops, float Rad = 0.f)
			{
				TArray<FSlateGradientStop> S; for (const TPair<float, FLinearColor>& K : Stops) S.Add(FSlateGradientStop(FVector2f(0.f, K.Key * H), K.Value));
				FSlateDrawElement::MakeGradient(Out, ++Layer, Geo(X, Y, W, H), S, Orient_Horizontal, ESlateDrawEffect::None, FVector4f(Rad, Rad, Rad, Rad));
			};
			const auto Font = [](float Px, bool bBold) { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::Max(6, FMath::RoundToInt(Px * 0.75f))); };
			// words at the middle of a height; cut with a full-stop sign when wider than they may be (bRight: ending at X)
			const auto Words = [&](const FString& S, float X, float Mid, float Px, bool bBold, const FLinearColor& C, float MaxW, bool bRight = false)
			{
				if (S.IsEmpty()) return 0.f;
				const FSlateFontInfo F = Font(Px, bBold);
				FString T = S; FVector2f Sz = FVector2f(Meas->Measure(T, F));
				for (int32 n = S.Len() - 1; n > 0 && Sz.X > MaxW; --n) { T = S.Left(n).TrimEnd() + TEXT("."); Sz = FVector2f(Meas->Measure(T, F)); }
				FSlateDrawElement::MakeText(Out, ++Layer, Geo(FMath::RoundToFloat(bRight ? X - Sz.X : X), FMath::RoundToFloat(Mid - Sz.Y / 2), Sz.X, Sz.Y), T, F, ESlateDrawEffect::None, C);
				return float(Sz.X);
			};

			Box(0, 0, P.W, P.H, Hex(0x050608));
			for (const FBdPanel& M : P.Panels)
			{
				const float Bz = FMath::Max(6.f, FMath::RoundToFloat(FMath::Min(M.W, float(P.H)) * 0.022f));
				const float SX = M.X + Bz, SY = Bz, SW = M.W - 2 * Bz, SH = P.H - 2 * Bz;
				Grad(M.X, 0, M.W, P.H, {{0.f, Hex(0x2A2F36)}, {0.5f, Hex(0x14171C)}, {1.f, Hex(0x0A0C0F)}}, Bz * 1.2f);
				Box(M.X + Bz, 1, M.W - 2 * Bz, FMath::Max(1.f, Bz * 0.12f), FLinearColor(1, 1, 1, 0.18f));
				Grad(SX, SY, SW, SH, {{0.f, Hex(0x0E1E3A)}, {0.55f, Hex(0x09152B)}, {1.f, Hex(0x060D1B)}});
				const float Head = FMath::RoundToFloat(SH * (P.bFull ? 0.15f : 0.145f)), Sub = FMath::RoundToFloat(SH * 0.072f), Foot = FMath::RoundToFloat(SH * 0.07f), Pad = SW * 0.03f;
				const float RowH = FMath::FloorToFloat((SH - Head - Sub - Foot) / P.MaxRows);

				// the header: the plane, the title, the place and the time
				if (M.bArr) Grad(SX, SY, SW, Head, {{0.f, Hex(0x5FD6FF)}, {1.f, Hex(0x1E9BD8)}}); else Grad(SX, SY, SW, Head, {{0.f, Hex(0xFFD24A)}, {1.f, Hex(0xE8A417)}});
				Box(SX, SY, SW, FMath::Max(1.f, Head * 0.06f), FLinearColor(1, 1, 1, 0.22f));
				Box(SX, SY + Head - FMath::Max(1.f, Head * 0.05f), SW, FMath::Max(1.f, Head * 0.05f), FLinearColor(0, 0, 0, 0.35f));
				const FLinearColor Ink = Hex(0x0A1424);
				const float Ic = Head * 0.62f, MidH = SY + Head / 2;
				{
					const float Cx = SX + Pad + Ic / 2, An = M.bArr ? 0.42f : -0.42f, Cs = FMath::Cos(An), Sn = FMath::Sin(An), U = Ic / 2;
					const auto Line = [&](float X1, float Y1, float X2, float Y2, float Th)
					{
						const auto At = [&](float X, float Y) { return FVector2f(Cx + (X * Cs - Y * Sn) * U, MidH + (X * Sn + Y * Cs) * U); };
						TArray<FVector2f> Pts; Pts.Add(At(X1, Y1)); Pts.Add(At(X2, Y2));
						FSlateDrawElement::MakeLines(Out, ++Layer, G.ToPaintGeometry(), MoveTemp(Pts), ESlateDrawEffect::None, Ink, true, Th * U);
					};
					Line(-0.9f, 0, 0.95f, 0, 0.2f);                                              // the body
					Line(0.2f, 0, -0.35f, 0.75f, 0.2f); Line(0.2f, 0, -0.35f, -0.75f, 0.2f);     // the wings
					Line(-0.7f, 0, -0.95f, 0.32f, 0.14f); Line(-0.7f, 0, -0.95f, -0.32f, 0.14f); // the tail
				}
				FString Title = M.bArr ? TEXT("Arrivals") : TEXT("Departures");
				if (P.bFull && ADPSite::SignLanguage() == TEXT("fr")) Title += M.bArr ? TEXT("  ·  Arrivées") : TEXT("  ·  Départs");
				const FString Right = P.bFull ? ADPBoards::HereIcao() + TEXT("   ") + P.Clock : P.Clock;
				const float RightW = Words(Right, SX + SW - Pad, MidH, Head * 0.46f, true, Ink, SW * 0.4f, true);
				Words(Title, SX + Pad * 1.6f + Ic, MidH, Head * 0.52f, true, Ink, SW - Pad * 3.6f - Ic - RightW);

				// the column heads
				const float Cf[] = {0.09f, 0.11f, 0.13f, 0.32f, 0.13f, 0.22f}, C4[] = {0.16f, 0.21f, 0.33f, 0.30f};
				const TCHAR* Nf[] = {TEXT("TIME"), TEXT("EXPECTED"), TEXT("FLIGHT"), M.bArr ? TEXT("FROM") : TEXT("TO"), TEXT("AIRCRAFT"), TEXT("STATUS")};
				const TCHAR* N4[] = {TEXT("TIME"), TEXT("FLIGHT"), M.bArr ? TEXT("FROM") : TEXT("TO"), TEXT("STATUS")};
				const int32 NCol = P.bFull ? 6 : 4;
				const float In = SW - 2 * Pad;
				Box(SX, SY + Head, SW, Sub, FLinearColor(1, 1, 1, 0.04f));
				for (int32 i = 0; i < NCol; ++i)
				{
					float X = SX + Pad; for (int32 k = 0; k < i; ++k) X += In * (P.bFull ? Cf[k] : C4[k]);
					Words(P.bFull ? Nf[i] : N4[i], X, SY + Head + Sub / 2, Sub * 0.5f, true, Hex(0x8FA6BD), In * (P.bFull ? Cf[i] : C4[i]) * 0.96f);
				}

				// the rows
				const float Fs = RowH * 0.5f;
				for (int32 r = 0; r < P.MaxRows; ++r)
				{
					const float Y = SY + Head + Sub + r * RowH, Mid = Y + RowH / 2;
					if (r % 2 == 0) Box(SX, Y, SW, RowH, FLinearColor(0.19f, 0.40f, 1.f, 0.045f));
					Box(SX + Pad / 2, Y + RowH - 1, SW - Pad, 1, FLinearColor(1, 1, 1, 0.05f));
					if (!M.Rows.IsValidIndex(r)) continue;
					const FBdRow& R = M.Rows[r];
					float X = SX + Pad;
					for (int32 i = 0; i < NCol; ++i)
					{
						const float Cw = In * (P.bFull ? Cf[i] : C4[i]);
						const int32 What = P.bFull ? i : (i == 0 ? 0 : i == 1 ? 2 : i == 2 ? 3 : 5);
						if (What == 5)
						{
							if (!R.Cell[5].IsEmpty())
							{
								const float Tw = FMath::Min(float(Meas->Measure(R.Cell[5], Font(Fs * 0.92f, true)).X), Cw - Fs * 0.9f);
								const FLinearColor Edge(R.Col); FLinearColor In2 = Edge; In2.A = 0.2f;
								Round(X, Y + RowH * 0.17f, Tw + Fs * 0.9f, RowH * 0.66f, In2, RowH * 0.2f, Edge, FMath::Max(1.f, RowH * 0.035f));
								Words(R.Cell[5], X + Fs * 0.45f, Mid, Fs * 0.92f, true, Edge, Cw - Fs * 0.85f);
							}
						}
						else Words(R.Cell[What], X, Mid, Fs, What != 4, What == 1 ? Hex(0xF2B544) : What == 2 ? Hex(0xFFE9A8) : FLinearColor::White, Cw * 0.96f);
						X += Cw;
					}
				}

				// the foot: live since, and the page
				const float Fy = SY + SH - Foot;
				Box(SX, Fy, SW, Foot, FLinearColor(0, 0, 0, 0.3f));
				if (P.bLive)
				{
					const float Rd = Foot * 0.16f;
					Round(SX + Pad, Fy + Foot / 2 - Rd, Rd * 2, Rd * 2, Hex(0x3FD0A0), Rd, Hex(0x3FD0A0), 0.f);
					Words(TEXT("Live · updated ") + P.Clock, SX + Pad + Foot * 0.5f, Fy + Foot / 2, Foot * 0.5f, true, Hex(0x8FA6BD), SW * 0.6f);
				}
				Words(M.Note, SX + SW - Pad, Fy + Foot / 2, Foot * 0.5f, true, Hex(0x8FA6BD), SW * 0.3f, true);
			}
			return Layer;
		}
	private:
		FBdPicture P;
		mutable TArray<TUniquePtr<FSlateRoundedBoxBrush>> Brushes;
	};

	bool BdIsNative(const FString& Key)
	{
		for (const FBdKind& K : BdKinds) if (Key == K.Key) return true;
		return false;
	}
}

void AADPHUD::TickBoards(float Dt)
{
	// once a second every kind is looked at, one a frame; drawn only when what it shows has changed
	if ((BoardClock += Dt) >= 1.f) { BoardClock = 0.f; if (BoardNext < 0) BoardNext = 0; }
	if (BoardNext < 0) return;
	if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized()) { BoardNext = -1; return; }
	if (BoardNext == 0)
	{
		// the screens, looked for until the model is there (then, for a kind it does not have, every 5 minutes)
		TArray<FString> Missing; for (const FBdKind& K : BdKinds) if (!BoardMats.Contains(K.Key) || !BoardMats[K.Key].Num()) Missing.Add(K.Key);
		const double T = FPlatformTime::Seconds();
		if (Missing.Num() && T - BoardSeekAt > (BoardTries < 10 ? 20.0 : 300.0))
		{
			BoardSeekAt = T; ++BoardTries;
			SeekBoards(Missing);
			BoardFound = 0; for (const FBdKind& K : BdKinds) BoardFound += BoardMats[K.Key].Num() > 0 ? 1 : 0;
		}
		const bool bLive = ADPBoards::Live(this);
		const FString Say = FString::Printf(TEXT("%d %d"), BoardFound, bLive ? 1 : 0);
		if (BoardFound > 0 && Say != BoardSaid)
		{
			BoardSaid = Say;
			const int32 D = ADPBoards::Shown(this, false).Num(), A = ADPBoards::Shown(this, true).Num();
			int32 Pd = 1, Pa = 1; ADPBoards::Page(D, 10, Pd); ADPBoards::Page(A, 10, Pa);
			ADP_LOG(TEXT("Boards: %d kinds native, rows %d/%d, pages %d/%d (the walls)%s"), BoardFound, D, A, Pd, Pa, bLive ? TEXT("") : TEXT(", no career"));
		}
	}
	DrawBoard(BoardNext);
	if (++BoardNext >= int32(UE_ARRAY_COUNT(BdKinds))) BoardNext = -1;
}

bool AADPHUD::DrawBoard(int32 Kind)
{
	const FBdKind& K = BdKinds[Kind]; const FString Key = K.Key;
	const TArray<TObjectPtr<UMaterialInstanceDynamic>>* Screens = BoardMats.Find(Key);
	if (!Screens || !Screens->Num()) return false;

	const UADPSim* Sim = UADPSim::Get(this);
	FBdPicture P; P.W = K.W; P.H = K.H; P.MaxRows = K.Rows; P.bFull = K.bFull; P.bLive = Sim && ADPBoards::Live(this);
	const auto Hm = [](const FDateTime& T) { return T.ToString(TEXT("%H:%M")); };
	P.Clock = Hm(ADPBoards::Local(P.bLive ? Sim->Now() : FDateTime::UtcNow()));
	const auto Fill = [&](FBdPanel& M, int32 Panel, int32 NPanels)
	{
		const TArray<FADPBoardRow>& Rows = ADPBoards::Shown(this, M.bArr);
		int32 Pages = 1, Pg = ADPBoards::Page(Rows.Num(), K.Rows, Pages);
		// (the Operations wall: three monitors of each side by side - up to three pages stand still, more turn together)
		if (NPanels > 1) Pg = Pages <= NPanels ? Panel : (ADPBoards::Turn() * NPanels + Panel) % Pages;
		if (Pages > 1 && Pg < Pages) M.Note = FString::Printf(TEXT("Page %d/%d"), Pg + 1, Pages);
		for (int32 i = Pg * K.Rows; i < FMath::Min((Pg + 1) * K.Rows, Rows.Num()); ++i)
		{
			const FADPBoardRow& R = Rows[i]; FBdRow& Q = M.Rows.AddDefaulted_GetRef();
			const FString Sch = Hm(R.Sched), Exp = Hm(R.Expected);
			const bool bGone = R.Status.StartsWith(TEXT("Departed")) || R.Status.StartsWith(TEXT("Landed"));
			Q.Cell[0] = !K.bFull && !bGone && Exp != Sch ? Exp : Sch;
			Q.Cell[1] = Exp; Q.Cell[2] = R.Flight;
			Q.Cell[3] = R.PlaceCode + TEXT(" ") + (K.bFull ? R.Place : R.Place.Left(13));
			if (K.bFull && Sim && !R.Reg.IsEmpty()) if (const auto* Ac = Sim->World.Aircraft.FindByPredicate([&R](const auto& X) { return X.Reg == R.Reg; })) Q.Cell[4] = Ac->TypeId;
			Q.Cell[5] = !M.bArr && R.Gate > 0 ? FString::Printf(TEXT("%s · G%d"), *R.Status, R.Gate) : R.Status;
			Q.Col = R.Col;
		}
	};
	if (Kind == BdOps)
	{
		const float Gap = FMath::RoundToFloat(K.W * 0.006f), Pw = (K.W - Gap * (BdOpsPanels - 1)) / BdOpsPanels;
		for (int32 i = 0; i < BdOpsPanels; ++i) { FBdPanel& M = P.Panels.AddDefaulted_GetRef(); M.bArr = i % 2 == 1; M.X = i * (Pw + Gap); M.W = Pw; Fill(M, i / 2, BdOpsPanels / 2); }
	}
	else { FBdPanel& M = P.Panels.AddDefaulted_GetRef(); M.bArr = K.bArr; M.W = K.W; Fill(M, 0, 1); }

	// the same as it shows already: nothing to do (and, when time runs fast, every two seconds at most)
	uint32 Sig = HashCombine(GetTypeHash(P.Clock), P.bLive ? 1u : 0u);
	for (const FBdPanel& M : P.Panels)
	{
		Sig = HashCombine(Sig, GetTypeHash(M.Note)); Sig = HashCombine(Sig, uint32(M.Rows.Num()));
		for (const FBdRow& R : M.Rows) { Sig = HashCombine(Sig, GetTypeHash(R.Col)); for (const FString& C : R.Cell) Sig = HashCombine(Sig, GetTypeHash(C)); }
	}
	const double T = FPlatformTime::Seconds();
	const bool bFirst = !BoardSig.Contains(Key);
	if (!bFirst && (BoardSig[Key] == Sig || T - BoardDrawn.FindRef(Key) < 1.9)) return false;
	BoardSig.Add(Key, Sig); BoardDrawn.Add(Key, T);

	ADP_PERF_SCOPE("a timetable drawn", 4.f);
	UTextureRenderTarget2D* Rt = BoardRT.FindRef(Key);
	if (!Rt)
	{
		Rt = NewObject<UTextureRenderTarget2D>(this, FName(*(TEXT("BoardRT_") + Key)));
		Rt->ClearColor = FLinearColor::Black;
		Rt->InitCustomFormat(K.W, K.H, PF_B8G8R8A8, false);
		Rt->UpdateResourceImmediate(true);
		BoardRT.Add(Key, Rt);
	}
	FWidgetRenderer* Draw = new FWidgetRenderer(false);
	Draw->DrawWidget(Rt, SNew(SBdBoard, P), FVector2D(K.W, K.H), 0.f);
	BeginCleanup(Draw);
	if (bFirst) PutBoard(Key, Rt);
	// (-BoardDump: its first picture also written to Saved/Boards/<kind>.png)
	if (bFirst && FParse::Param(FCommandLine::Get(), TEXT("BoardDump"))) UKismetRenderingLibrary::ExportRenderTarget(this, Rt, FPaths::ProjectSavedDir() / TEXT("Boards"), Key + TEXT(".png"));
	return true;
}
#pragma endregion

void AADPHUD::OnBoard(const FString& Key, const FString& B64)
{
	if (BdIsNative(Key)) return;   // (drawn here: TickBoards)
	// (the picture decoded in the background: up to 70 ms each, in the middle of the game every minute - to-do 269; one
	//  after the other, so an older one never lands on a newer one)
	FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	static UE::Tasks::FPipe Pipe(TEXT("Boards"));
	Pipe.Launch(TEXT("Board"), [Me = TWeakObjectPtr<AADPHUD>(this), Key, B64]()
	{
	TArray<uint8> Png;
	if (!FBase64::Decode(B64, Png)) return;
	// (-BoardDump: every board's picture also written to Saved/Boards/<kind>.png, to check what the menus drew)
	if (FParse::Param(FCommandLine::Get(), TEXT("BoardDump"))) FFileHelper::SaveArrayToFile(Png, *(FPaths::ProjectSavedDir() / TEXT("Boards") / (Key + TEXT(".png"))));
	IImageWrapperModule& M = FModuleManager::GetModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> W = M.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> Bgra;
	if (!W.IsValid() || !W->SetCompressed(Png.GetData(), Png.Num()) || !W->GetRaw(ERGBFormat::BGRA, 8, Bgra)) return;
	AsyncTask(ENamedThreads::GameThread, [Me, Key, Wd = (int32)W->GetWidth(), Ht = (int32)W->GetHeight(), Bgra = MoveTemp(Bgra)]() mutable { if (AADPHUD* Hud = Me.Get()) if (Hud->GetWorld() && !Hud->IsActorBeingDestroyed()) Hud->ShowBoard(Key, Wd, Ht, MoveTemp(Bgra)); });
	});
}

void AADPHUD::ShowBoard(const FString& Key, int32 Wd, int32 Ht, TArray<uint8> Bgra)
{
	// the same board again at the same size: its new pixels written into the picture it already has (a new picture every
	// refresh made the game wait for the old one's cleanup - a stall of up to 0.4 s every few seconds, the title film
	// stuttering; owner 2026-11-15)
	if (TObjectPtr<UTexture2D>* Old = BoardTex.Find(Key))
		if (UTexture2D* OT = Old->Get())
			if (OT->GetSizeX() == Wd && OT->GetSizeY() == Ht && OT->GetResource())
			{
				uint8* Copy = (uint8*)FMemory::Malloc(Bgra.Num()); FMemory::Memcpy(Copy, Bgra.GetData(), Bgra.Num());
				FUpdateTextureRegion2D* R = new FUpdateTextureRegion2D(0, 0, 0, 0, Wd, Ht);
				OT->UpdateTextureRegions(0, 1, R, Wd * 4, 4, Copy, [](uint8* Data, const FUpdateTextureRegion2D* Rg) { FMemory::Free(Data); delete Rg; });
				return;
			}
	UTexture2D* T = UTexture2D::CreateTransient(Wd, Ht, PF_B8G8R8A8, FName(*(TEXT("Board_") + Key)));
	if (!T) return;
	T->SRGB = true;
	void* Dst = T->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Dst, Bgra.GetData(), Bgra.Num());
	T->GetPlatformData()->Mips[0].BulkData.Unlock();
	T->UpdateResource();
	BoardTex.Add(Key, T);

	// the screens of this kind: their material slots swapped once for a copy that shows the board
	// (the operations room's monitors: the game's own operations screen, drawn for each monitor's proportions: opsmon169,
	// opsmon219, opsmon329 on the screens MonitorScreen169... of the three kinds, OpsMonitors.h)
	if (!BoardMats.Contains(Key)) BoardMats.Add(Key);
	if (!BoardMats[Key].Num() && FPlatformTime::Seconds() - BoardSought.FindRef(Key) > 20.0)   // (again until the model is there, not at every picture)
	{
		BoardSought.Add(Key, FPlatformTime::Seconds());
		TArray<TObjectPtr<UMaterialInstanceDynamic>>& L = BoardMats[Key];
		// a gate's own screen (gate1…gate8): its face's material, wherever the model put it
		if (Key.StartsWith(TEXT("gate")))
		{
			const int32 Gate = FCString::Atoi(*Key.Mid(4));
			for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
				if (TObjectPtr<UInstancedStaticMeshComponent>* F = It->GateScreens.Find(Gate))
					if (UInstancedStaticMeshComponent* K = *F)
						for (int32 S = 0; S < K->GetNumMaterials(); ++S)
						{
							UMaterialInterface* Mat = K->GetMaterial(S);
							if (!Mat) continue;
							UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(Mat);
							if (!D) { D = UMaterialInstanceDynamic::Create(Mat, this); K->SetMaterial(S, D); }
							L.Add(D);
						}
		}
		else SeekBoards({Key});
		ADP_LOG(TEXT("Board %s: %dx%d on %d screens"), *Key, Wd, Ht, BoardMats[Key].Num());
	}
	PutBoard(Key, T);
}

/** The screens of these kinds in the model (one look through it for them all): their materials into BoardMats. */
void AADPHUD::SeekBoards(const TArray<FString>& Keys)
{
	ADP_PERF_SCOPE("the timetable screens looked for", 5.f);
	TArray<FString> Wants;
	for (const FString& Key : Keys)
	{
		BoardMats.FindOrAdd(Key);
		Wants.Add(Key.StartsWith(TEXT("opsmon")) ? TEXT("MonitorScreen") + Key.Mid(6) : TEXT("fids-") + Key);
	}
	{
		for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
		{
			TArray<UInstancedStaticMeshComponent*> Parts;
			It->GetComponents(Parts);
			for (UInstancedStaticMeshComponent* K : Parts)
				for (int32 S = 0; S < K->GetNumMaterials(); ++S)
				{
					UMaterialInterface* Mat = K->GetMaterial(S);
					if (!Mat) continue;
					// (a material already copied to be toned down, ToneDown, is known by the one it was copied from)
					const UMaterialInstanceDynamic* Copy = Cast<UMaterialInstanceDynamic>(Mat);
					const UMaterialInterface* Named = Copy && Copy->Parent ? Copy->Parent.Get() : Mat;
					// (the true-Unreal terminal's materials are MI_T_<the same name>)
					const auto Plain = [](const FString& N) { return N.StartsWith(TEXT("MI_T_")) ? N.RightChop(5) : N; };
					const FString N1 = Plain(Named->GetName()), N2 = Plain(Mat->GetName());
					const int32 Which = Wants.IndexOfByPredicate([&](const FString& Want) { return N1.StartsWith(Want) || N2.StartsWith(Want); });
					if (Which == INDEX_NONE) continue;
					const FString& Key = Keys[Which]; TArray<TObjectPtr<UMaterialInstanceDynamic>>& L = BoardMats[Key];
					UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(Mat);
					if (!D) { D = UMaterialInstanceDynamic::Create(Mat, this); K->SetMaterial(S, D); }
					L.Add(D);
					BdBringOut(GetWorld(), K, Key.EndsWith(TEXT("totem")));
#if WITH_EDITOR
					// (-BoardDump: the screen part's texture coordinates - all at one point shows one pixel of the picture)
					if (FParse::Param(FCommandLine::Get(), TEXT("BoardDump")) && K->GetStaticMesh())
						if (const FMeshDescription* MD = K->GetStaticMesh()->GetMeshDescription(0))
						{
							FStaticMeshConstAttributes A(*MD); const auto UV = A.GetVertexInstanceUVs();
							FBox2f B(ForceInit); for (const FVertexInstanceID V : MD->VertexInstances().GetElementIDs()) if (UV.GetNumChannels() > 0) B += UV.Get(V, 0);
							FTransform X0; K->GetInstanceTransform(0, X0, true);
							ADP_LOG(TEXT("Board %s part %s (%s, %d instances, first at %s): uv %s, slot %d of %d"), *Key, *K->GetName(), *K->GetStaticMesh()->GetName(), K->GetInstanceCount(), *X0.GetLocation().ToString(), *B.ToString(), S, K->GetNumMaterials());
							const FBox Lb = K->GetStaticMesh()->GetBoundingBox();
							ADP_LOG(TEXT("Board %s part size %s, rot %s"), *Key, *(Lb.GetSize() * X0.GetScale3D()).ToString(), *X0.Rotator().ToString());
							const FVector C0 = X0.TransformPosition(Lb.GetCenter());
							for (const FVector Dir : {FVector::XAxisVector, -FVector::XAxisVector, FVector::YAxisVector, -FVector::YAxisVector})
							{
								TArray<FHitResult> Hits; GetWorld()->LineTraceMultiByChannel(Hits, C0 + Dir * 900.f, C0 - Dir * 20.f, ECC_Visibility);
								FHitResult H1; const bool b1 = GetWorld()->LineTraceSingleByChannel(H1, C0 + Dir * 900.f, C0 - Dir * 20.f, ECC_Visibility);
								ADP_LOG(TEXT("Board %s seen from %s: first hit %s at %.0f cm from the screen"), *Key, *Dir.ToString(), b1 && H1.GetComponent() ? *(H1.GetComponent()->GetName() + TEXT("/") + (Cast<UStaticMeshComponent>(H1.GetComponent()) && Cast<UStaticMeshComponent>(H1.GetComponent())->GetStaticMesh() ? Cast<UStaticMeshComponent>(H1.GetComponent())->GetStaticMesh()->GetName() : FString())) : TEXT("nothing"), b1 ? (H1.ImpactPoint - C0).Size() : -1.f);
							}
						}
#endif
				}
		}
		// (-BoardDump: each screen's material and its picture slots, to see why a screen stays dark)
		if (FParse::Param(FCommandLine::Get(), TEXT("BoardDump")))
			for (const FString& Key : Keys) for (UMaterialInstanceDynamic* D : BoardMats[Key]) if (D)
			{
				TArray<FMaterialParameterInfo> Infos; TArray<FGuid> Ids; D->GetAllTextureParameterInfo(Infos, Ids);
				FString Ns; for (const FMaterialParameterInfo& I : Infos) Ns += I.Name.ToString() + TEXT(" ");
				ADP_LOG(TEXT("Board %s screen: %s (parent %s, base %s) textures: %s"), *Key, *D->GetName(), D->Parent ? *D->Parent->GetName() : TEXT("-"), D->GetBaseMaterial() ? *D->GetBaseMaterial()->GetName() : TEXT("-"), *Ns);
			}
	}
}

/** A picture on a kind's screens. */
void AADPHUD::PutBoard(const FString& Key, UTexture* T)
{
	for (UMaterialInstanceDynamic* D : BoardMats.FindOrAdd(Key))
	{
		if (!D) continue;
		// (the true-Unreal terminal's lit screens, M_TUnlit: picture x BC + glow x EC - BC was the import's dark "screen off"
		// colour, so the picture showed nearly black (Chablais, 2026-11-21): the picture at its own colour, no second glow)
		if (const UMaterial* Bm = D->GetBaseMaterial()) if (Bm->GetName().StartsWith(TEXT("M_TUnlit")))
		{
			D->SetVectorParameterValue(TEXT("BC"), FLinearColor::White); D->SetVectorParameterValue(TEXT("EC"), FLinearColor::Black);
			float Eb = 1.f; D->GetScalarParameterValue(FMaterialParameterInfo(TEXT("EB")), Eb); if (Eb < 1.f) D->SetScalarParameterValue(TEXT("EB"), 1.f);
		}
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		D->GetAllTextureParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& I : Infos)
		{
			// (the operations monitors: only the colour and the light; the picture as their bumps, metal and shade turned
			// the screen dark in the packaged game)
			const FString P = I.Name.ToString();
			if (Key.StartsWith(TEXT("opsmon")) && !P.Contains(TEXT("baseColor"), ESearchCase::IgnoreCase) && !P.Contains(TEXT("emissive"), ESearchCase::IgnoreCase)) continue;
			if (P == TEXT("Nrm") || P == TEXT("Det")) continue;   // (the true-Unreal materials: the picture is colour and light only)
			D->SetTextureParameterValueByInfo(I, T);   // the colour and the light: the same picture
		}
	}
}
