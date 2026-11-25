// The card of the one you talk to (TalkCard.h).
#include "UI/TalkCard.h"
#include "People.h"
#include "PaxFlow.h"
#include "StaffPeople.h"
#include "TalkCamera.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSimData.h"
#include "Sim/ADPCrew.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float TcWidth = 600.f, TcRound = 18.f;
	FLinearColor TcRgb(uint8 R, uint8 G, uint8 B, float A = 1.f) { FLinearColor C(FColor(R, G, B)); C.A = A; return C; }
	FSlateFontInfo TcFont(int32 Size, const char* Kind = "Bold") { return FCoreStyle::GetDefaultFontStyle(Kind, Size); }

	TSharedRef<STextBlock> TcText(const FString& T, int32 Size, const FLinearColor& C, const char* Kind = "Bold", int32 Spacing = 0)
	{
		FSlateFontInfo F = TcFont(Size, Kind); F.LetterSpacing = Spacing;
		return SNew(STextBlock).Font(F).ColorAndOpacity(FSlateColor(C)).Text(FText::FromString(T));
	}
	FString TcCapital(FString S) { if (S.Len()) S[0] = FChar::ToUpper(S[0]); return S; }
}

FTalkCard ADPTalkCard::About(UWorld* W, AActor* Who)
{
	FTalkCard C;
	AADPPeople* Pp = AADPPeople::Get(W); AADPPeople::FWho* P = Pp && Who ? Pp->Find(Who) : nullptr;
	if (!P) return C;
	C.Name = P->First;
	const bool bPax = P->Role == TEXT("passenger");
	const FString Job = bPax ? FString(TEXT("Passenger")) : TcCapital(ADPCrew::Role(P->Role) ? ADPCrew::RoleName(P->Role) : P->Role);
	C.Who = FString::Printf(TEXT("%s  ·  %d"), *Job, P->Age);
	if (bPax) { APaxFlow::CardOf(W, Who, C); return C; }
	// one of your own people: who they are in your books
	const UStaffPeople* St = UStaffPeople::Get(W); UADPSim* Sim = UADPSim::Get(W);
	const FString Id = St ? St->IdOf(Who) : FString();
	if (Id.IsEmpty() || !Sim) return C;
	const FADPPerson* M = Sim->World.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == Id; });
	if (!M) return C;
	C.Name = M->Name; C.Who = FString::Printf(TEXT("%s  ·  %d"), *TcCapital(ADPCrew::RoleName(M->Role)), int32(M->Age));
	if (const FADPAirline* A = Sim->World.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == M->AirlineId; })) C.Cells.Add({FString(TEXT("WORKS FOR")), A->Name});
	if (const ADPCrew::FShift* S = ADPCrew::Shift(M->Shift)) C.Cells.Add({FString(TEXT("SHIFT")), S->Label});
	FString Doing, Where;
	if (St->Status(Id, Doing, Where))
	{
		if (!Where.IsEmpty()) C.Cells.Add({FString(TEXT("AT")), TcCapital(Where)});
		C.Status = TcCapital(Doing);
	}
	if (!St->OnShift(Id)) C.Dot = TcRgb(255, 178, 62);
	return C;
}

AADPPeople* STalkCard::People() const { return World.IsValid() ? AADPPeople::Get(World.Get()) : nullptr; }

void STalkCard::Construct(const FArguments& Args)
{
	World = Args._World;
	FString Saved;
	if (GConfig && GConfig->GetString(TEXT("ADP.Boxes"), TEXT("TalkCard"), Saved, GGameUserSettingsIni)) Drag.InitFromString(Saved);
	// (the darker of the two glasses, the owner's choice)
	static const FSlateRoundedBoxBrush Glass(TcRgb(26, 44, 58, 0.68f), TcRound, FLinearColor(1.f, 1.f, 1.f, 0.16f), 1.f);
	static const FSlateRoundedBoxBrush Head(TcRgb(6, 16, 26, 0.34f), FVector4(TcRound, TcRound, 0.f, 0.f));
	static const FSlateRoundedBoxBrush Plain(TcRgb(16, 28, 38, 0.94f), TcRound);
	SetRenderTransformPivot(FVector2D(0.5, 0.5));
	SetRenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::CreateSPLambda(this, [this]() { return Place(); }));
	ChildSlot
	[
		SNew(SBox).WidthOverride(TcWidth)
		[
			// (the same frost whatever the screen's size)
			SNew(SBackgroundBlur).Padding(FMargin(0.f)).CornerRadius(FVector4(TcRound, TcRound, TcRound, TcRound)).bApplyAlphaToBlur(false).LowQualityFallbackBrush(&Plain)
			.BlurStrength_Lambda([this]() { return FMath::Clamp(34.f * GetCachedGeometry().Scale, 30.f, 85.f); })
			[
				SNew(SBorder).BorderImage(&Glass).Padding(FMargin(0.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[SNew(SBorder).BorderImage(&Head).Padding(FMargin(24, 18, 24, 16))[SAssignNew(Top, SVerticalBox)]]
					+ SVerticalBox::Slot().AutoHeight().Padding(24, 16, 24, 10)
					[
						SNew(STextBlock).Font(TcFont(12, "Regular")).ColorAndOpacity(FSlateColor(TcRgb(236, 243, 250))).AutoWrapText(true)
						.Text_Lambda([this]() { AADPPeople* Pp = People(); return FText::FromString(Pp ? Pp->MenuText : FString()); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(18, 0, 18, 4)[SAssignNew(Rows, SVerticalBox)]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 14)
					[
						SNew(STextBlock).Font(TcFont(8, "Regular")).ColorAndOpacity(FSlateColor(TcRgb(196, 210, 224, 0.75f)))
						.Text_Lambda([this]() { return FText::FromString(bByPad ? TEXT("A: choose   ·   B: close") : TEXT("Click or 1-8: choose   ·   drag: move   ·   E or Esc: close")); })
					]
				]
			]
		]
	];
}

TOptional<FSlateRenderTransform> STalkCard::Place()
{
	const FGeometry& G = GetCachedGeometry();
	FVector2D Move, D = Drag; float Size = 1.f;
	ADPTalkCam::PanelPlace(G.Scale, FVector2D(G.GetLocalSize()), D, Move, Size);
	if (bDragging) Drag = D;
	return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2f(Size), FVector2f(Move)));
}

void STalkCard::Tick(const FGeometry& Geo, const double Now, const float Dt)
{
	SCompoundWidget::Tick(Geo, Now, Dt);
	AADPPeople* Pp = People(); if (!Pp || !Pp->MenuOpen()) return;
	AActor* Who = Pp->MenuActor();
	if (Who != Shown.Get()) { Shown = Who; NextLook = 0.0; Sel = 0; bMade = false; Options.Reset(); Let(); }
	if (Now >= NextLook)
	{
		NextLook = Now + 0.5;
		const FTalkCard C = ADPTalkCard::About(World.Get(), Who);
		if (!bMade || !(C == Card)) { Card = C; bMade = true; MakeTop(); }
	}
	if (Options != Pp->MenuOptions) { Options = Pp->MenuOptions; Sel = 0; MakeRows(); }
}

void STalkCard::MakeTop()
{
	if (!Top.IsValid()) return;
	Top->ClearChildren();
	const FLinearColor White(1.f, 1.f, 1.f), Soft = TcRgb(190, 206, 222, 0.9f), Label = TcRgb(160, 182, 204, 0.85f), Blue = TcRgb(122, 196, 255);
	Top->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[TcText(Card.Name, 20, White)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(12, 0, 0, 3)[TcText(Card.Who, 10, Soft, "Regular")]
	];
	if (Card.bTrip)
	{
		auto End = [&](const FString& Code, const FString& Town, EHorizontalAlignment At)
		{
			TSharedRef<SVerticalBox> B = SNew(SVerticalBox);
			B->AddSlot().AutoHeight().HAlign(At)[TcText(Code, 26, White)];
			if (!Town.IsEmpty()) B->AddSlot().AutoHeight().HAlign(At)[TcText(Town, 10, Soft, "Regular")];
			return B;
		};
		Top->AddSlot().AutoHeight().Padding(0, 12, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[End(Card.FromCode, Card.FromName, HAlign_Left)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Top).Padding(18, 18, 18, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[SNew(SBox).HeightOverride(2.f)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FSlateColor(TcRgb(122, 196, 255, 0.7f)))]]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)[TcText(TEXT("▶"), 9, Blue)]
			]
			+ SHorizontalBox::Slot().AutoWidth()[End(Card.ToCode, Card.ToName, HAlign_Right)]
		];
	}
	if (Card.Cells.Num())
	{
		TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(0, 4, 10, 4));
		for (int32 i = 0; i < Card.Cells.Num(); ++i)
			Grid->AddSlot(i % 4, i / 4)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[TcText(Card.Cells[i].Key, 7, Label, "Bold", 80)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 1, 0, 0)
				[SNew(STextBlock).Font(TcFont(11)).ColorAndOpacity(FSlateColor(White)).WrapTextAt(126.f).Text(FText::FromString(Card.Cells[i].Value))]
			];
		Top->AddSlot().AutoHeight().Padding(0, 12, 0, 0)[Grid];
	}
	if (!Card.Status.IsEmpty())
	{
		static const FSlateRoundedBoxBrush DotBrush(FLinearColor::White, 4.f);
		Top->AddSlot().AutoHeight().Padding(0, 10, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 9, 0)
			[SNew(SBox).WidthOverride(8.f).HeightOverride(8.f)[SNew(SBorder).BorderImage(&DotBrush).BorderBackgroundColor(FSlateColor(Card.Dot))]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[SNew(STextBlock).Font(TcFont(10, "Regular")).ColorAndOpacity(FSlateColor(TcRgb(226, 236, 246))).AutoWrapText(true).Text(FText::FromString(Card.Status))]
		];
	}
}

void STalkCard::MakeRows()
{
	if (!Rows.IsValid()) return;
	Rows->ClearChildren();
	static const FSlateRoundedBoxBrush Row(FLinearColor(1.f, 1.f, 1.f, 0.07f), 9.f), RowOn(TcRgb(38, 128, 226, 0.92f), 9.f);
	static const FButtonStyle Bare = FButtonStyle().SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource()).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
	for (int32 i = 0; i < Options.Num(); ++i)
		Rows->AddSlot().AutoHeight().Padding(0, 3)
		[
			// (never takes the keyboard from the game: E and Esc work on after a click)
			SNew(SButton).ButtonStyle(&Bare).IsFocusable(false).ContentPadding(FMargin(0)).Cursor(EMouseCursor::Hand)
			.OnHovered_Lambda([this, i]() { Sel = i; bByPad = false; })
			.OnClicked_Lambda([this, i]() { Sel = i; Take(false); return FReply::Handled(); })
			[
				SNew(SBorder).Padding(FMargin(14, 9)).BorderImage_Lambda([this, i]() -> const FSlateBrush* { return Sel == i ? &RowOn : &Row; })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)[TcText(FString::FromInt(i + 1), 9, TcRgb(170, 196, 222, 0.9f))]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[SNew(STextBlock).Font(TcFont(11)).ColorAndOpacity(FSlateColor(FLinearColor::White)).AutoWrapText(true).Text(FText::FromString(Options[i]))]
				]
			]
		];
}

void STalkCard::Step(int32 By, bool bPad)
{
	bByPad = bPad;
	if (Options.Num()) Sel = (Sel + By + Options.Num()) % Options.Num();
}

void STalkCard::Take(bool bPad)
{
	bByPad = bPad;
	// (the list shown is the one there is: a choice just made may have changed it already)
	AADPPeople* Pp = People();
	if (Pp && Pp->MenuOpen() && Options == Pp->MenuOptions && Options.IsValidIndex(Sel)) Pp->Choose(Sel);
}

void STalkCard::TakeNumber(int32 I) { if (Options.IsValidIndex(I)) { Sel = I; Take(false); } }

void STalkCard::Shut(bool bPad) { bByPad = bPad; if (AADPPeople* Pp = People()) if (Pp->MenuOpen()) Pp->Close(); }

FReply STalkCard::OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E)
{
	if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	bDragging = true; Grab = E.GetScreenSpacePosition(); DragFrom = Drag; bByPad = false;
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply STalkCard::OnMouseMove(const FGeometry& Geo, const FPointerEvent& E)
{
	if (!bDragging) return FReply::Unhandled();
	FVector2D Px(1920, 1080);
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Px);
	Drag = DragFrom + (FVector2D(E.GetScreenSpacePosition()) - Grab) / FVector2D(FMath::Max(1.0, Px.X), FMath::Max(1.0, Px.Y));
	return FReply::Handled();
}

void STalkCard::Let()
{
	if (!bDragging) return;
	bDragging = false;
	if (GConfig) { GConfig->SetString(TEXT("ADP.Boxes"), TEXT("TalkCard"), *Drag.ToString(), GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
}

FReply STalkCard::OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& E)
{
	if (!bDragging || E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	Let();
	return FReply::Handled().ReleaseMouseCapture();
}

void STalkCard::OnMouseCaptureLost(const FCaptureLostEvent& E) { SCompoundWidget::OnMouseCaptureLost(E); Let(); }

FCursorReply STalkCard::OnCursorQuery(const FGeometry& Geo, const FPointerEvent& E) const
{
	return FCursorReply::Cursor(bDragging ? EMouseCursor::GrabHandClosed : EMouseCursor::GrabHand);
}
