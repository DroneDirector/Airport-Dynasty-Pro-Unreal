#include "JetwayHmi.h"
#include "JetwayRigs.h"
#include "ADPLog.h"
#include "UI/ADPHUD.h"
#include "Components/WidgetComponent.h"
#include "Widgets/SLeafWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	// (owner, 2026-11-18) seen from the cab the screen is a real screen, slightly backlit at about 10 % (Settings can
	// change it); up close (E) no brightness change at all - a short fade between
	constexpr float DimFade = 0.3f;
	const FLinearColor Bg(0.051f, 0.078f, 0.098f), Bar(0.094f, 0.137f, 0.169f), Pane(0.067f, 0.106f, 0.129f), Tile(0.086f, 0.133f, 0.165f),
		Line(0.141f, 0.196f, 0.235f), Ink(0.84f, 0.89f, 0.92f), Dim(0.49f, 0.59f, 0.65f), White(1, 1, 1), Ok(0.5f, 0.94f, 0.69f),
		Warn(1.f, 0.82f, 0.4f), Bad(1.f, 0.5f, 0.5f), Blue(0.122f, 0.435f, 0.639f), BtnC(0.114f, 0.176f, 0.216f), Go(0.09f, 0.376f, 0.227f), Hov(0.5f, 0.83f, 1.f);
	const TCHAR* StateName(int32 S) { static const TCHAR* N[] = { TEXT("PARKED"), TEXT("DOCKING"), TEXT("DOCKED"), TEXT("UNDOCKING"), TEXT("MANUAL") }; return S >= 0 && S < 5 ? N[S] : TEXT("-"); }
	const TCHAR* Prep[] = { TEXT("Arriving aircraft and assigned stand selected"), TEXT("Aircraft type and passenger door selected"), TEXT("Stored positioning profile retrieved"),
		TEXT("Mechanical and electrical status checked"), TEXT("CCTV checked - apron round the bogie clear"), TEXT("Safety systems confirmed working"), TEXT("Jetway in STANDBY / pre-positioned") };
	constexpr float CheckStep = 0.9f;   // (the system check: seconds per part)
}

/** The HMI screen: draws itself every time the monitor is drawn, and keeps where each of its buttons is. */
class SJetwayHmiPanel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SJetwayHmiPanel) {}
	SLATE_END_ARGS()
	TWeakObjectPtr<AJetwayHmi> Owner;
	struct FHit { FBox2D R; FString Id; };
	mutable TArray<FHit> Hits;
	void Construct(const FArguments&) {}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(ADPHmi::ScrW, ADPHmi::ScrH); }
	/** The button under a point of the screen (pixels), empty if none. */
	FString At(const FVector2D& P) const { for (int32 i = Hits.Num() - 1; i >= 0; --i) if (Hits[i].R.IsInside(P)) return Hits[i].Id; return FString(); }
	bool Where(const FString& Id, FVector2D& Mid) const { for (const FHit& Ht : Hits) if (Ht.Id == Id) { Mid = Ht.R.GetCenter(); return true; } return false; }

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
	{
		AJetwayHmi* O = Owner.Get(); AJetwayRigs* JR = O ? AJetwayRigs::Get(O->GetWorld()) : nullptr;
		FJetwayHmiInfo In; const bool bIn = JR && O->Jetway() != INDEX_NONE && JR->HmiInfo(O->Jetway(), In);
		Hits.Reset();
		int32 Lr = Layer;   // (every shape on a layer of its own, in the order drawn: same-layer shapes may be drawn in any order)
		const float Sx = G.GetLocalSize().X / ADPHmi::ScrW, Sy = G.GetLocalSize().Y / ADPHmi::ScrH;
		const FString Hover = O && O->bShowCursor ? At(O->Cur) : FString();
		static const FSlateRoundedBoxBrush R4(FLinearColor::White, 6.f);
		static const FSlateBrush* Sq = FCoreStyle::Get().GetBrush("WhiteBrush");
		auto Geo = [&](float X, float Y, float Wd, float Ht) { return G.ToPaintGeometry(FVector2D(Wd * Sx, Ht * Sy), FSlateLayoutTransform(FVector2D(X * Sx, Y * Sy))); };
		auto Box = [&](float X, float Y, float Wd, float Ht, const FLinearColor& C, bool bRound = true) { FSlateDrawElement::MakeBox(Out, ++Lr, Geo(X, Y, Wd, Ht), bRound ? (const FSlateBrush*)&R4 : Sq, ESlateDrawEffect::None, C); };
		auto Font = [&](bool bBold, float Px) { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::Max(4, FMath::RoundToInt(Px * 0.75f * Sy))); };
		TSharedRef<FSlateFontMeasure> Fm = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		auto Text = [&](const FString& T, float X, float Y, float Px, const FLinearColor& C, bool bBold = false, int32 Align = 0)   // (Align: 0 left of X, 1 middle, 2 right)
		{
			const FSlateFontInfo F = Font(bBold, Px); const FVector2D Sz = Fm->Measure(T, F) / FVector2D(Sx, Sy);
			const float X0 = Align == 1 ? X - Sz.X * 0.5f : Align == 2 ? X - Sz.X : X;
			FSlateDrawElement::MakeText(Out, ++Lr, Geo(X0, Y - Sz.Y * 0.5f, Sz.X + 4, Sz.Y), T, F, ESlateDrawEffect::None, C);
		};
		auto Button = [&](const FString& Id, float X, float Y, float Wd, float Ht, const FString& Label, const FLinearColor& C, bool bEnabled = true, float Px = 20.f, const FString& Glyph = FString())
		{
			Box(X, Y, Wd, Ht, bEnabled ? C : C * 0.55f);
			if (Hover == Id && bEnabled) { Box(X - 2, Y - 2, Wd + 4, 3, Hov, false); Box(X - 2, Y + Ht - 1, Wd + 4, 3, Hov, false); Box(X - 2, Y, 3, Ht, Hov, false); Box(X + Wd - 1, Y, 3, Ht, Hov, false); }
			const FLinearColor TC = bEnabled ? White : Dim * 0.8f;
			if (Glyph.IsEmpty()) Text(Label, X + Wd * 0.5f, Y + Ht * 0.5f, Px, TC, true, 1);
			else { Text(Glyph, X + Wd * 0.5f, Y + Ht * 0.36f, Px * 1.25f, TC, true, 1); Text(Label, X + Wd * 0.5f, Y + Ht * 0.74f, Px * 0.8f, TC, true, 1); }
			if (bEnabled) Hits.Add({ FBox2D(FVector2D(X, Y), FVector2D(X + Wd, Y + Ht)), Id });
		};
		const FString Tab = O ? O->Tab : TEXT("home");
		const bool bMan = bIn && In.State == 4;
		const float SW = ADPHmi::ScrW;
		// ---- the frame: background, top bar ----
		Box(0, 0, SW, ADPHmi::ScrH, Bg, false);
		Box(0, 0, SW, 72, Bar, false); Box(0, 72, SW, 2, Line, false);
		const FString Id = bIn ? In.Id : TEXT("J-?"); FString Num = Id.RightChop(2); if (Num.Len() > 1 && FChar::IsAlpha(Num[Num.Len() - 1])) Num.LeftChopInline(1);
		Text(FString::Printf(TEXT("GATE %s  ·  JETWAY %s"), *Num, *Id), 26, 36, 30, White, true);
		const bool bMov = bIn && In.bMoving;
		const FString Mode = !bIn ? TEXT("-") : bMov ? TEXT("MOVING") : bMan ? TEXT("MANUAL") : TEXT("AUTO");
		Box(470, 18, 150, 38, bMov || bMan ? FLinearColor(0.35f, 0.27f, 0.06f) : FLinearColor(0.08f, 0.27f, 0.18f)); Text(Mode, 545, 37, 22, bMov || bMan ? Warn : Ok, true, 1);
		Box(632, 18, 160, 38, Tile); Text(TEXT("OPERATOR"), 712, 37, 22, Dim, true, 1);
		Text(FDateTime::Now().ToString(TEXT("%H:%M:%S")), 1350, 37, 26, Ink, false, 2);
		const bool bAlarm = O && O->Events.Num() && O->Events.Last().Level >= 2 && (FDateTime::Now() - O->Events.Last().At).GetTotalSeconds() < 30.0;
		Box(1374, 25, 24, 24, bAlarm ? FLinearColor(1, 0.2f, 0.2f) : bMov ? FLinearColor(1, 0.69f, 0.13f) : FLinearColor(0.23f, 0.27f, 0.31f));
		Button(TEXT("exit"), 1430, 14, 150, 46, TEXT("EXIT"), BtnC, true, 22);
		// ---- the status strip (on every screen) ----
		{
			const float Tw = (SW - 32 - 5 * 8) / 6.f; float X = 16;
			const int32 Pct = bIn ? FMath::RoundToInt(In.Progress * 100.f) : 0;
			const FString Dock = !bIn ? FString(TEXT("-")) : In.State == 2 ? FString(TEXT("DOCKED")) : In.State == 1 ? FString::Printf(TEXT("IN PROGRESS %d%%"), Pct) : In.State == 3 ? FString::Printf(TEXT("UNDOCKING %d%%"), 100 - Pct) : In.State == 4 ? FString(TEXT("MANUAL")) : FString(TEXT("PARKED"));
			struct FT { const TCHAR* L; FString V; FLinearColor C; } Ts[6] = {
				{ TEXT("MODE"), bMan ? TEXT("MANUAL") : TEXT("AUTO"), White }, { TEXT("DOCKING"), Dock, White },
				{ TEXT("AIRCRAFT"), bIn && In.bPlane ? TEXT("DETECTED") : TEXT("NONE"), bIn && In.bPlane ? Ok : Warn },
				{ TEXT("SAFETY SYSTEM"), TEXT("NORMAL"), Ok }, { TEXT("CCTV"), TEXT("AVAILABLE"), Ok }, { TEXT("EMERGENCY STOP"), TEXT("RELEASED"), Ok } };
			for (int32 i = 0; i < 6; ++i)
			{
				Box(X, 84, Tw, 70, Tile); Text(Ts[i].L, X + 12, 104, 15, Dim, true); Text(Ts[i].V, X + 12, 133, 21, Ts[i].C, true);
				if (i == 1 && bIn && (In.State == 1 || In.State == 3)) { Box(X + 12, 147, Tw - 24, 4, Line, false); Box(X + 12, 147, (Tw - 24) * FMath::Clamp(In.State == 1 ? In.Progress : 1.f - In.Progress, 0.f, 1.f), 4, FLinearColor(0.18f, 0.64f, 0.42f), false); }
				X += Tw + 8;
			}
		}
		// ---- the menu along the bottom ----
		{
			Box(0, 912, SW, 88, Bar, false); Box(0, 912, SW, 2, Line, false);
			Button(TEXT("back"), 10, 922, 150, 68, TEXT("< BACK"), BtnC, O && O->BackStack.Num() > 0, 20);
			static const TCHAR* Tabs[][2] = { {TEXT("home"), TEXT("HOME")}, {TEXT("pos"), TEXT("DOCKING")}, {TEXT("cam"), TEXT("CCTV")}, {TEXT("sys"), TEXT("STATUS")}, {TEXT("alm"), TEXT("ALARMS")}, {TEXT("mnt"), TEXT("MAINTENANCE")}, {TEXT("set"), TEXT("SETTINGS")} };
			const float Tw = (SW - 180 - 6 * 6) / 7.f; float X = 170;
			for (auto& T : Tabs) { Button(FString(TEXT("tab:")) + T[0], X, 922, Tw, 68, T[1], Tab == T[0] ? Blue : FLinearColor(0.078f, 0.118f, 0.145f), true, 19); X += Tw + 6; }
		}
		// ---- the screen itself ----
		auto PaneBox = [&](float X, float Y, float Wd, float Ht, const TCHAR* Title) { Box(X, Y, Wd, Ht, Pane); Text(Title, X + 18, Y + 26, 17, Dim, true); };
		auto Row = [&](float X, float Y, float Wd, const FString& L, const FString& V) { Text(L, X, Y, 21, Dim); Text(V, X + Wd, Y, 22, White, true, 2); };
		if (Tab == TEXT("home"))
		{
			PaneBox(16, 166, 980, 734, TEXT("T-10  ·  PREPARATION BEFORE ARRIVAL"));
			const int32 Done = bIn && In.bPlane ? 7 : 0;
			for (int32 i = 0; i < 7; ++i)
			{
				const float Y = 210 + i * 66; Box(34, Y, 944, 56, Tile);
				Box(50, Y + 16, 24, 24, i < Done ? FLinearColor(0.18f, 0.64f, 0.42f) : Line);
				if (i < Done) { TArray<FVector2D> Ck = { FVector2D(55, Y + 28) * FVector2D(Sx, Sy), FVector2D(60, Y + 34) * FVector2D(Sx, Sy), FVector2D(69, Y + 21) * FVector2D(Sx, Sy) }; FSlateDrawElement::MakeLines(Out, ++Lr, G.ToPaintGeometry(), Ck, ESlateDrawEffect::None, Bg, true, 3.f * Sy); }
				Text(Prep[i], 92, Y + 28, 21, Ink);
			}
			Text(TEXT("The jetway never docks on its own: the gate agent (or you) prepares it no later than"), 34, 822, 18, Dim);
			Text(TEXT("10 minutes before arrival; docking starts only when the aircraft is parked, engines and beacon off."), 34, 850, 18, Dim);
			PaneBox(1008, 166, 576, 734, TEXT("NEXT ARRIVAL"));
			Text(bIn && In.bPlane ? In.Type : FString(TEXT("No aircraft at the stand")), 1028, 236, bIn && In.bPlane ? 54 : 30, White, true);
			if (bIn && In.bPlane) Text(TEXT("Door ") + In.Door, 1028, 290, 22, Dim);
			Row(1028, 350, 536, TEXT("Stand"), Id); Row(1028, 390, 536, TEXT("Jetway"), bIn ? StateName(In.State) : TEXT("-"));
			Row(1028, 430, 536, TEXT("Profile"), bIn && In.bPlane ? In.Type + TEXT(" ") + In.Door + TEXT(" stored") : FString(TEXT("-")));
			Button(TEXT("tab:pos"), 1028, 816, 536, 68, TEXT("DOCKING  >"), BtnC, true, 22);
		}
		else if (Tab == TEXT("pos"))
		{
			PaneBox(16, 166, 980, 734, TEXT("POSITION  ·  PLAN VIEW"));
			// the plan: the aircraft's side, its door, the rotunda, the tunnel out to the cab, the cab turned
			const float PX = 34, PY = 200, PW = 944, PH = 430; Box(PX, PY, PW, PH, FLinearColor(0.047f, 0.075f, 0.09f), false);
			for (int32 i = 1; i < 5; ++i) Box(PX, PY + PH * i / 5.f, PW, 1, Line, false);
			const float K = (PW - 120) / FMath::Max(25.f, (bIn ? In.PathLen : 12.f) + 18.f);
			const FVector2D Ro(PX + 60, PY + PH - 50);
			const float Ext = bIn ? In.Ext : 14.f, Prog = bIn ? In.Progress : 0.f;
			const FVector2D Cab(Ro.X + Ext * K - (bIn ? In.Lat : 0.f) * K * 0.4f, Ro.Y - (PH - 170) * Prog - (bIn ? In.Lat : 0.f) * K);
			if (bIn && In.bPlane)
			{
				Box(PX + 200, PY + 30, PW - 260, 70, FLinearColor(0.11f, 0.17f, 0.21f)); Text(In.Type, PX + 200 + (PW - 260) / 2, PY + 65, 24, Dim, false, 1);
				Box(FMath::Min(PX + PW - 90, Ro.X + (In.PathLen + 12.f) * K * 0.92f) - 12, PY + 98, 24, 7, FLinearColor(1, 0.82f, 0.4f), false);
			}
			TArray<FVector2D> Tun = { Ro * FVector2D(Sx, Sy), Cab * FVector2D(Sx, Sy) };
			FSlateDrawElement::MakeLines(Out, ++Lr, G.ToPaintGeometry(), Tun, ESlateDrawEffect::None, FLinearColor(0.5f, 0.65f, 0.73f), true, 22.f * Sy);
			Box(Ro.X - 18, Ro.Y - 18, 36, 36, FLinearColor(0.36f, 0.49f, 0.56f));
			{   // the cab, turned
				const float A = FMath::DegreesToRadians(bIn ? -In.TurnDeg : 0.f), Hw = 26, Hh = 20;
				TArray<FVector2D> C; for (int32 k = 0; k < 5; ++k) { const int32 kk = k % 4; const FVector2D L(kk == 1 || kk == 2 ? Hw : -Hw, kk >= 2 ? Hh : -Hh); C.Add((Cab + FVector2D(L.X * FMath::Cos(A) - L.Y * FMath::Sin(A), L.X * FMath::Sin(A) + L.Y * FMath::Cos(A))) * FVector2D(Sx, Sy)); }
				FSlateDrawElement::MakeLines(Out, ++Lr, G.ToPaintGeometry(), C, ESlateDrawEffect::None, FLinearColor(0.75f, 0.89f, 0.97f), true, 4.f * Sy);
			}
			Text(FString::Printf(TEXT("TUNNEL %.1f m"), Ext), PX + 14, PY + PH - 18, 18, Dim);
			// the manual pad
			Text(TEXT("MANUAL CONTROL  ·  one step each press"), 34, 660, 17, Dim, true);
			const bool bJog = bIn && In.State != 1 && In.State != 2 && In.State != 3;
			static const TCHAR* Pad[3][3][3] = { { {TEXT("jog:CABL"), TEXT("CAB"), TEXT("<<")}, {TEXT("jog:FWD"), TEXT("FORWARD"), TEXT("^")}, {TEXT("jog:CABR"), TEXT("CAB"), TEXT(">>")} },
				{ {TEXT("jog:LEFT"), TEXT("LEFT"), TEXT("<")}, {TEXT("jog:UP"), TEXT("UP"), TEXT("+")}, {TEXT("jog:RIGHT"), TEXT("RIGHT"), TEXT(">")} },
				{ {TEXT(""), TEXT(""), TEXT("")}, {TEXT("jog:BACK"), TEXT("BACK"), TEXT("v")}, {TEXT("jog:DOWN"), TEXT("DOWN"), TEXT("-")} } };
			for (int32 r = 0; r < 3; ++r) for (int32 c = 0; c < 3; ++c)
				if (Pad[r][c][0][0]) Button(Pad[r][c][0], 34 + c * 318, 684 + r * 72, 306, 64, Pad[r][c][1], BtnC, bJog, 18, Pad[r][c][2]);
			PaneBox(1008, 166, 576, 734, TEXT("JETWAY"));
			float Y = 230; auto Rv = [&](const TCHAR* L, const FString& V) { Row(1028, Y, 536, L, V); Y += 44; };
			Rv(TEXT("State"), bIn ? StateName(In.State) : TEXT("-")); Rv(TEXT("Aircraft"), bIn && In.bPlane ? In.Type : FString(TEXT("none at the stand")));
			Rv(TEXT("Door"), bIn ? In.Door : FString(TEXT("-"))); Rv(TEXT("Tunnel length"), FString::Printf(TEXT("%.2f m"), Ext));
			Rv(TEXT("Cab rotation"), FString::Printf(TEXT("%.1f deg"), bIn ? In.TurnDeg : 0.f)); Rv(TEXT("Cab height"), FString::Printf(TEXT("%.2f m"), bIn ? In.Height : 0.f));
			Rv(TEXT("Drive speed"), FString::Printf(TEXT("%.2f m/s"), bIn ? FMath::Abs(In.Speed) : 0.f)); Rv(TEXT("Travel"), FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Prog * 100.f)));
			Rv(TEXT("Steering offset"), FString::Printf(TEXT("%+.2f m"), bIn ? In.Lat : 0.f)); Rv(TEXT("Lift offset"), FString::Printf(TEXT("%+.2f m"), bIn ? In.Lift : 0.f));
			if (O && !O->Note.IsEmpty()) Text(O->Note, 1028, 712, 18, Warn);
			Button(TEXT("cmd:DOCK"), 1028, 760, 262, 72, TEXT("AUTO DOCK"), Go, bIn && In.bPlane && (In.State == 0 || In.State == 4), 21);
			Button(TEXT("cmd:UNDOCK"), 1302, 760, 262, 72, TEXT("UNDOCK"), BtnC, bIn && (In.State == 2 || In.State == 4), 21);
		}
		else if (Tab == TEXT("sys"))
		{
			PaneBox(16, 166, 1568, 734, TEXT("SYSTEM STATUS"));
			const bool bDocked = bIn && In.State == 2;
			struct FS { const TCHAR* N; FString V; FLinearColor C; } S[10] = {
				{ TEXT("Hydraulic system"), TEXT("PRESSURE OK  ·  165 bar"), Ok }, { TEXT("Electrical supply"), TEXT("400 V  ·  OK"), Ok },
				{ TEXT("Drive motors"), bMov ? TEXT("RUNNING") : TEXT("IDLE  ·  OK"), bMov ? Warn : Ok }, { TEXT("Lift columns"), FString::Printf(TEXT("%.2f m  ·  OK"), bIn ? In.Height : 0.f), Ok },
				{ TEXT("Control system"), TEXT("PLC RUNNING  ·  OK"), Ok }, { TEXT("Emergency stop circuit"), TEXT("RELEASED  ·  OK"), Ok },
				{ TEXT("Cab door"), bIn && In.bDoorsOpen ? TEXT("OPEN") : TEXT("SHUT"), Ok }, { TEXT("Canopy"), bDocked ? TEXT("EXTENDED  ·  SEALED") : TEXT("FOLDED"), Ok },
				{ TEXT("Pre-conditioned air"), bDocked ? TEXT("SUPPLYING") : TEXT("STANDBY"), Ok }, { TEXT("400 Hz ground power"), bDocked ? TEXT("SUPPLYING") : TEXT("STANDBY"), Ok } };
			for (int32 i = 0; i < 10; ++i) { const float X = 34 + (i % 2) * 776, Y = 206 + (i / 2) * 136; Box(X, Y, 760, 120, Tile); Box(X, Y, 8, 120, S[i].C, false); Text(S[i].N, X + 30, Y + 38, 22, Dim, true); Text(S[i].V, X + 30, Y + 82, 28, White, true); }
		}
		else if (Tab == TEXT("alm"))
		{
			PaneBox(16, 166, 1568, 734, TEXT("ALARMS  ·  ACTIVE AND HISTORY"));
			Box(34, 206, 1532, 70, bAlarm ? FLinearColor(0.37f, 0.09f, 0.09f) : FLinearColor(0.08f, 0.27f, 0.18f));
			Text(bAlarm ? TEXT("ACTIVE:  ") + O->Events.Last().Text : FString(TEXT("NO ACTIVE ALARMS")), 56, 241, 24, White, true);
			Text(TEXT("HISTORY"), 34, 304, 17, Dim, true);
			int32 Count = 0;
			for (int32 i = O ? O->Events.Num() - 1 : -1; i >= 0 && Count < 12; --i, ++Count)
			{
				const AJetwayHmi::FEvt& E = O->Events[i]; const float Y = 340 + Count * 46;
				Box(34, Y - 20, 1532, 40, Count % 2 ? Pane : Tile, false);
				Text(E.At.ToString(TEXT("%H:%M:%S")), 52, Y, 20, Dim); Box(190, Y - 9, 18, 18, E.Level >= 2 ? Bad : E.Level == 1 ? Warn : Ok); Text(E.Text, 230, Y, 20, Ink);
			}
			if (!Count) Text(TEXT("Nothing yet."), 52, 340, 20, Dim);
		}
		else if (Tab == TEXT("mnt"))
		{
			PaneBox(16, 166, 980, 734, TEXT("MAINTENANCE  ·  DIAGNOSTICS"));
			float Y = 230; auto Rm = [&](const TCHAR* L, const FString& V) { Row(36, Y, 940, L, V); Y += 48; };
			Rm(TEXT("Drive hours run"), FString::Printf(TEXT("%.2f h"), O ? O->RunHours : 0.f)); Rm(TEXT("Dockings since service"), FString::Printf(TEXT("%d"), O ? O->Docks : 0));
			Rm(TEXT("Manual steps"), FString::Printf(TEXT("%d"), O ? O->Steps : 0)); Rm(TEXT("Hydraulic oil"), TEXT("OK  ·  level 92 %"));
			Rm(TEXT("Brake test"), TEXT("PASSED")); Rm(TEXT("Last system check"), O && O->LastCheck.GetTicks() ? O->LastCheck.ToString(TEXT("%H:%M:%S")) : FString(TEXT("not run yet")));
			Rm(TEXT("Service due"), TEXT("in 412 dockings"));
			PaneBox(1008, 166, 576, 734, TEXT("SYSTEM CHECK"));
			static const TCHAR* Chk[] = { TEXT("Control system"), TEXT("Hydraulics"), TEXT("Drive motors"), TEXT("Lift columns"), TEXT("Sensors and bumpers"), TEXT("Emergency stop"), TEXT("Cameras") };
			const float Ct = O ? O->CheckT : -1.f; const bool bRun = Ct >= 0.f && Ct < 7 * CheckStep;
			for (int32 i = 0; i < 7; ++i)
			{
				const float Yc = 230 + i * 60; const bool bDone = Ct >= 0.f && Ct > (i + 1) * CheckStep, bNow = Ct >= 0.f && !bDone && Ct > i * CheckStep;
				Box(1028, Yc - 22, 536, 46, Tile); Text(Chk[i], 1048, Yc, 20, Ink);
				Text(bDone ? TEXT("OK") : bNow ? TEXT("TESTING...") : TEXT("-"), 1544, Yc, 20, bDone ? Ok : bNow ? Warn : Dim, true, 2);
			}
			Button(TEXT("mnt:check"), 1028, 760, 536, 72, bRun ? TEXT("CHECKING...") : TEXT("RUN SYSTEM CHECK"), BtnC, !bRun, 21);
		}
		else if (Tab == TEXT("set"))
		{
			PaneBox(16, 166, 1568, 734, TEXT("SETTINGS  ·  AUTHORISED"));
			Text(TEXT("Screen brightness in the cab"), 36, 240, 22, Ink, true);
			static const float Lv[] = { 0.05f, 0.10f, 0.20f, 0.30f };
			for (int32 i = 0; i < 4; ++i) Button(FString::Printf(TEXT("set:dim:%.2f"), Lv[i]), 36 + i * 180, 270, 166, 64, FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Lv[i] * 100)), O && FMath::IsNearlyEqual(O->DimLevel, Lv[i]) ? Blue : BtnC, true, 21);
			Text(TEXT("Automatic levelling"), 36, 400, 22, Ink, true);
			Button(TEXT("set:level"), 36, 430, 346, 64, O && O->bAutoLevel ? TEXT("ON") : TEXT("OFF"), O && O->bAutoLevel ? Go : BtnC, true, 21);
			Text(TEXT("Drive speed limit"), 36, 560, 22, Ink, true); Text(TEXT("0.30 m/s  (creep 0.05 m/s in the last 1.5 m)"), 36, 600, 21, Dim);
			Text(TEXT("Up close the screen shows at its own brightness; in the cab it is dimmed like a real backlit screen."), 36, 680, 19, Dim);
		}
		else   // CCTV: the cameras come with the 3D cameras on the jetway (a later part)
		{
			PaneBox(16, 166, 1568, 734, TEXT("CCTV"));
			static const TCHAR* Cams[] = { TEXT("CAM 1  ·  BOGIE"), TEXT("CAM 2  ·  AIRCRAFT DOOR"), TEXT("CAM 3  ·  APRON"), TEXT("CAM 4  ·  BRIDGE EXTERIOR") };
			for (int32 i = 0; i < 4; ++i) { const float X = 34 + (i % 2) * 776, Y = 206 + (i / 2) * 340; Box(X, Y, 760, 320, FLinearColor(0.03f, 0.04f, 0.05f)); Text(Cams[i], X + 18, Y + 26, 18, Ink, true); Text(TEXT("NO SIGNAL  ·  camera being fitted"), X + 380, Y + 170, 22, Dim, true, 1); }
		}
		// ---- the "+" (about 1.5 cm across on the 36 cm screen) ----
		if (O && O->bShowCursor)
		{
			const float L = 33.f, T2 = 3.f; const FVector2D C = O->Cur;
			FSlateDrawElement::MakeBox(Out, ++Lr, Geo(C.X - L - 1, C.Y - T2 - 1, 2 * L + 2, 2 * T2 + 2), Sq, ESlateDrawEffect::None, FLinearColor(0, 0, 0, 0.6f));
			FSlateDrawElement::MakeBox(Out, ++Lr, Geo(C.X - T2 - 1, C.Y - L - 1, 2 * T2 + 2, 2 * L + 2), Sq, ESlateDrawEffect::None, FLinearColor(0, 0, 0, 0.6f));
			FSlateDrawElement::MakeBox(Out, ++Lr, Geo(C.X - L, C.Y - T2, 2 * L, 2 * T2), Sq, ESlateDrawEffect::None, White);
			FSlateDrawElement::MakeBox(Out, ++Lr, Geo(C.X - T2, C.Y - L, 2 * T2, 2 * L), Sq, ESlateDrawEffect::None, White);
		}
		return Lr + 1;
	}
};

AJetwayHmi::AJetwayHmi()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

AJetwayHmi* AJetwayHmi::Get(UWorld* World)
{
	if (!World) return nullptr;
	for (TActorIterator<AJetwayHmi> It(World); It; ++It) return *It;
	FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AJetwayHmi>(FVector::ZeroVector, FRotator::ZeroRotator, Sp);
}

void AJetwayHmi::Make()
{
	if (Screen) return;
	Panel = SNew(SJetwayHmiPanel); Panel->Owner = this;
	Screen = NewObject<UWidgetComponent>(this, TEXT("HmiScreen"));
	Screen->SetupAttachment(RootComponent);
	Screen->SetWidgetSpace(EWidgetSpace::World);
	Screen->SetDrawSize(FVector2D(ADPHmi::ScrW, ADPHmi::ScrH));
	Screen->SetPivot(FVector2D(0.5f, 0.5f));
	Screen->SetBlendMode(EWidgetBlendMode::Opaque);
	Screen->SetTwoSided(false);
	Screen->SetCastShadow(false);
	Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Screen->SetRedrawTime(0.1f);   // (ten times a second in the cab; every frame up close)
	Screen->RegisterComponent();
	Screen->SetSlateWidget(Panel);
	Screen->SetVisibility(false);
	Event(TEXT("HMI started"));
	ADP_LOG(TEXT("Jetway HMI: the live monitor made (Unreal's own screen)"));
}

void AJetwayHmi::Event(const FString& Text, int32 Level)
{
	Events.Add({ FDateTime::Now(), Text, Level });
	if (Events.Num() > 200) Events.RemoveAt(0, Events.Num() - 200);
}

void AJetwayHmi::Watch(float Dt)
{
	// the jetway's own doings into the history and the counters
	AJetwayRigs* JR = AJetwayRigs::Get(GetWorld()); FJetwayHmiInfo In;
	if (!JR || J == INDEX_NONE || !JR->HmiInfo(J, In)) return;
	if (In.bMoving) RunHours += Dt / 3600.f;
	if (In.State != LastState && LastState != -1)
	{
		static const TCHAR* N[] = { TEXT("Jetway at rest"), TEXT("Automatic docking started"), TEXT("Docked at the aircraft - canopy out"), TEXT("Undocking"), TEXT("Manual control") };
		if (In.State >= 0 && In.State < 5) Event(N[In.State]);
		if (In.State == 2) ++Docks;
	}
	LastState = In.State;
	if (CheckT >= 0.f) { const float Was = CheckT; CheckT += Dt; if (Was < 7 * CheckStep && CheckT >= 7 * CheckStep) { LastCheck = FDateTime::Now(); Event(TEXT("System check passed - all systems OK")); } }
}

void AJetwayHmi::Tick(float Dt)
{
	Super::Tick(Dt);
	AJetwayRigs* JR = AJetwayRigs::Get(GetWorld());
	APlayerController* PC = GetWorld()->GetFirstPlayerController(); APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!JR || !Me) return;
	// which jetway's monitor (now and then): the console nearest you within 9 m - or the one you're using
	if ((Look -= Dt) <= 0.f || Held != INDEX_NONE)
	{
		Look = 0.5f;
		int32 Best = Held;
		if (Best == INDEX_NONE)
		{
			float BestD = 900.f;
			for (int32 K = 0; K < JR->Num(); ++K) { const float D = FVector::Dist(JR->Console(K), Me->GetActorLocation()); if (D < BestD) { BestD = D; Best = K; } }
		}
		if (Best != J)
		{
			J = Best; LastState = -1;
			if (J != INDEX_NONE) { Make(); ADP_LOG(TEXT("Jetway HMI: live on %s's monitor"), *JR->Name(J)); }
			if (Screen) Screen->SetVisibility(J != INDEX_NONE);
		}
	}
	if (J == INDEX_NONE || !Screen) return;
	Watch(Dt);
	// on the screen's face, wherever the cab has moved to (its front to the operator: the widget faces its +X)
	FVector Mid, N, Up; FVector2D Sz;
	if (!JR->ScreenView(J, Mid, N, Up, Sz)) return;
	Screen->SetWorldLocationAndRotation(Mid + N * 0.3f, FRotationMatrix::MakeFromXZ(N, Up).Rotator());
	Screen->SetWorldScale3D(FVector(1.f, Sz.X / ADPHmi::ScrW, Sz.Y / ADPHmi::ScrH));
	// in the cab: the "+" where your view meets the screen (you aim with it), hidden when you look away or are over 3 m off
	if (!bPointer)
	{
		FVector O; FRotator VR; PC->GetPlayerViewPoint(O, VR);
		const FVector D = VR.Vector(); const float Den = FVector::DotProduct(D, N);
		bAim = false;
		if (Den < -0.05f && FVector::Dist(O, Mid) < 300.f)
		{
			const float T = FVector::DotProduct(Mid - O, N) / Den; const FVector Rel = O + D * T - Mid;
			const FVector Rt = FVector::CrossProduct(Up, N).GetSafeNormal();
			const float U = 0.5f + FVector::DotProduct(Rel, Rt) / Sz.X, V = 0.5f - FVector::DotProduct(Rel, Up) / Sz.Y;
			if (T > 0.f && U >= 0.f && U <= 1.f && V >= 0.f && V <= 1.f) { bAim = true; Cur = FVector2D(U * ADPHmi::ScrW, V * ADPHmi::ScrH); }
		}
		bShowCursor = bAim;
	}
	// gliding onto a button (eased)
	if (GlideT >= 0.f) { GlideT = FMath::Min(1.f, GlideT + Dt / GlideLen); const float E = GlideT * GlideT * (3.f - 2.f * GlideT); Cur = FMath::Lerp(GlideFrom, GlideTo, E); if (GlideT >= 1.f) GlideT = -1.f; }
	// brightness: dim (backlit) in the cab, the screen as it is up close; drawn every frame up close
	{
		const float Want = bPointer ? 1.f : DimLevel;
		Bright = FMath::FInterpConstantTo(Bright, Want, Dt, 1.f / DimFade);
		if (!FMath::IsNearlyEqual(Bright, Shown, 0.002f)) { Shown = Bright; Screen->SetTintColorAndOpacity(FLinearColor(Bright, Bright, Bright, 1.f)); }
		Screen->SetRedrawTime(bPointer || GlideT >= 0.f ? 0.f : 0.1f);
	}
}

void AJetwayHmi::SetPointer(bool bOn)
{
	bPointer = bOn;
	if (bOn && !bAim) Cur = FVector2D(ADPHmi::ScrW * 0.5f, ADPHmi::ScrH * 0.5f);
	bShowCursor = bOn || bAim;
	if (!bOn) GlideT = -1.f;
}

void AJetwayHmi::MoveCursor(const FVector2D& DeltaPx)
{
	if (!bPointer || DeltaPx.IsNearlyZero()) return;
	Cur.X = FMath::Clamp(Cur.X + DeltaPx.X, 0.f, ADPHmi::ScrW); Cur.Y = FMath::Clamp(Cur.Y + DeltaPx.Y, 0.f, ADPHmi::ScrH);
}

void AJetwayHmi::PutCursor(const FVector2D& At)
{
	Cur = FVector2D(FMath::Clamp(At.X, 0.f, 1.f) * ADPHmi::ScrW, FMath::Clamp(At.Y, 0.f, 1.f) * ADPHmi::ScrH);
}

void AJetwayHmi::AimAt(const FString& Id, float GlideTime)
{
	FVector2D To; if (!Panel.IsValid() || !Panel->Where(Id, To)) { ADP_LOG(TEXT("Jetway HMI: no button %s on this screen"), *Id); return; }
	GlideFrom = Cur; GlideTo = To; GlideLen = FMath::Max(0.05f, GlideTime); GlideT = 0.f;
}

void AJetwayHmi::ClickCursor()
{
	if (!bPointer || !Panel.IsValid()) return;
	const FString Id = Panel->At(Cur);
	if (!Id.IsEmpty()) Command(Id);
}

void AJetwayHmi::Command(const FString& Id)
{
	AJetwayRigs* JR = AJetwayRigs::Get(GetWorld());
	if (!JR || J == INDEX_NONE) return;
	ADP_LOG(TEXT("Jetway HMI: %s on %s"), *Id, *JR->Name(J));
	Note.Reset();
	if (Id == TEXT("exit"))
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (AADPHUD* Hd = Cast<AADPHUD>(PC->GetHUD())) Hd->EndHmi();
		return;
	}
	if (Id == TEXT("back")) { if (BackStack.Num()) Tab = BackStack.Pop(); return; }
	if (Id.StartsWith(TEXT("tab:"))) { const FString T = Id.RightChop(4); if (T != Tab) { BackStack.Add(Tab); if (BackStack.Num() > 20) BackStack.RemoveAt(0); Tab = T; } return; }
	if (Id.StartsWith(TEXT("set:dim:"))) { DimLevel = FCString::Atof(*Id.RightChop(8)); Event(FString::Printf(TEXT("Screen brightness in the cab set to %d %%"), FMath::RoundToInt(DimLevel * 100))); return; }
	if (Id == TEXT("set:level")) { bAutoLevel = !bAutoLevel; Event(bAutoLevel ? TEXT("Automatic levelling ON") : TEXT("Automatic levelling OFF"), 1); return; }
	if (Id == TEXT("mnt:check")) { CheckT = 0.f; Event(TEXT("System check started")); return; }
	AJetwayRigs::LastNote.Reset();
	bool bOk = true;
	if (Id == TEXT("cmd:DOCK")) bOk = JR->Dock(J);
	else if (Id == TEXT("cmd:UNDOCK")) bOk = JR->Undock(J);
	else if (Id.StartsWith(TEXT("jog:"))) { bOk = JR->Jog(J, Id.RightChop(4)); if (bOk) { ++Steps; Event(TEXT("Manual step: ") + Id.RightChop(4)); } }
	if (!bOk) { Note = AJetwayRigs::LastNote.IsEmpty() ? FString(TEXT("Not possible now")) : AJetwayRigs::LastNote; Event(TEXT("Refused: ") + Note, 1); }
}
