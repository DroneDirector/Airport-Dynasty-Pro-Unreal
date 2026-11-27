// Maintenance (to-do 311): a native page over the tablet's, the computers' and the phone's screen (the Maintenance app on
// their desktop, beside Vehicles: ue.adp.maintenance). One row per aircraft of ours: registration, type, condition, where
// it is, its state (flying, at stand, waiting for the tow, being towed in, in hangar X, being towed out - HangarTow::TowOf),
// the check or fault it is in with the game time left and a progress bar. Each one at our airport that is not flying has
// Send to hangar: the check (A, C, D, or putting right its FaultDue), the hall (a free one or the first free), the price and
// length under the current Settings > Realism > Maintenance time (ADPMaint::Quote); it starts through ADPMaint::StartCheck
// and HangarTow tows it to the chosen hall (FADPAircraft::HangarWant) - or the page says why not.
// Built again once a second only when something shown changed; the time left and the bars follow every frame.
#include "UI/ADPHUD.h"
#include "HangarTow.h"
#include "Airfield.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSimData.h"
#include "Sim/ADPMaint.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPWorldData.h"
#include "ADPLog.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor MpPanel(0.02f, 0.05f, 0.09f, 0.96f), MpRow(0.05f, 0.11f, 0.18f, 1.f), MpHead(0.07f, 0.16f, 0.26f, 1.f);
	const FLinearColor MpInk(0.85f, 0.94f, 1.f), MpDim(0.55f, 0.68f, 0.78f), MpGold(0.96f, 0.71f, 0.27f), MpBad(1.f, 0.42f, 0.36f), MpGood(0.45f, 0.9f, 0.55f);
	FSlateFontInfo MpFont(int32 Size, const char* Kind = "Regular") { return FCoreStyle::GetDefaultFontStyle(Kind, Size); }
	FString MpMoney(double V)
	{
		FString D = FString::Printf(TEXT("%lld"), (long long)FMath::RoundToDouble(FMath::Abs(V))), R;
		for (int32 i = 0; i < D.Len(); ++i) { if (i && (D.Len() - i) % 3 == 0) R += TEXT(","); R += D[i]; }
		return (V < 0 ? TEXT("-$") : TEXT("$")) + R;
	}
	// (game hours as said: 40 min, 2 h 15 min, 3 d 4 h)
	FString MpHours(double H)
	{
		H = FMath::Max(0.0, H);
		const int32 Min = FMath::RoundToInt(H * 60.0);
		if (Min < 60) return FString::Printf(TEXT("%d min"), Min);
		if (Min < 48 * 60) return Min % 60 ? FString::Printf(TEXT("%d h %d min"), Min / 60, Min % 60) : FString::Printf(TEXT("%d h"), Min / 60);
		const int32 Hr = Min / 60; return Hr % 24 ? FString::Printf(TEXT("%d d %d h"), Hr / 24, Hr % 24) : FString::Printf(TEXT("%d d"), Hr / 24);
	}
	TSharedRef<SWidget> MpText(const FString& S, const FLinearColor& C, int32 Size = 12, const char* Kind = "Regular")
	{
		return SNew(STextBlock).Text(FText::FromString(S)).ColorAndOpacity(C).Font(MpFont(Size, Kind)).AutoWrapText(true);
	}
	const TCHAR* MpCheckName(const FString& K)
	{
		return K == TEXT("A") ? TEXT("A-check") : K == TEXT("C") ? TEXT("C-check") : K == TEXT("D") ? TEXT("D-check") : K == TEXT("F") ? TEXT("Fault") : TEXT("?");
	}
	FADPAircraft* MpPlane(UADPSim* S, const FString& Reg)
	{
		return S ? S->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg && X.AirlineId == UADPSim::Me(); }) : nullptr;
	}
}

class SMaintPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaintPage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, AADPHUD* InHud)
	{
		Hud = InHud;
		ChildSlot
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(MpPanel).Padding(FMargin(22.f, 16.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[MpText(TEXT("Maintenance"), MpGold, 20, "Bold")]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 18, 0)
					[SNew(STextBlock).Font(MpFont(13, "Bold")).ColorAndOpacity(MpInk).Text_Lambda([this]() { return FText::FromString(MoneyLine); })]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SButton).OnClicked_Lambda([this]() { if (AADPHUD* H = Hud.Get()) H->CloseMaint(); return FReply::Handled(); })
						[MpText(TEXT("Close  (Esc)"), MpInk, 12, "Bold")]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[SNew(STextBlock).Font(MpFont(12)).AutoWrapText(true).ColorAndOpacity_Lambda([this]() { return FSlateColor(bSayBad ? MpBad : MpGood); }).Text_Lambda([this]() { return FText::FromString(Say); })]
				+ SVerticalBox::Slot().FillHeight(1.f)[SAssignNew(Scroll, SScrollBox)]
			]
		];
		Rebuild();
		RegisterActiveTimer(1.f, FWidgetActiveTimerDelegate::CreateSP(this, &SMaintPage::Tock));
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& G, const FKeyEvent& K) override
	{
		if (K.GetKey() == EKeys::Escape || K.GetKey() == EKeys::Gamepad_FaceButton_Right) { if (AADPHUD* H = Hud.Get()) H->CloseMaint(); return FReply::Handled(); }
		return SCompoundWidget::OnKeyDown(G, K);
	}

private:
	TWeakObjectPtr<AADPHUD> Hud; TSharedPtr<SScrollBox> Scroll; FString MoneyLine, Say, LastSig; bool bSayBad = false; float SayLeft = 0.f;
	TMap<FString, FString> PickKind, PickHall;   // (per registration: the check chosen, the hall chosen - empty: the first free)

	EActiveTimerReturnType Tock(double, float)
	{
		AADPHUD* H = Hud.Get(); if (!H) return EActiveTimerReturnType::Stop;
		if (H->bMaintPage) Rebuild();
		if (SayLeft > 0.f && (SayLeft -= 1.f) <= 0.f) Say.Reset();
		return EActiveTimerReturnType::Continue;
	}

	// (the halls no tow holds now)
	void FreeHalls(TArray<FString>& Out) const
	{
		AADPHUD* H = Hud.Get(); UWorld* W = H ? H->GetWorld() : nullptr;
		for (const FAirfieldData::FHangarSite& Hs : AAirfield::Shared().Hangars) if (!AHangarTow::HallTaken(W, Hs.Id)) Out.Add(Hs.Id);
	}

	// (where it is, what it is doing; the hall it is in or going to, if any)
	FString StateOf(const FADPAircraft& A, const FString& Here, FString& Where) const
	{
		AADPHUD* H = Hud.Get(); UWorld* W = H ? H->GetWorld() : nullptr;
		const bool bHere = A.Location == Here;
		Where = A.Status == TEXT("flying") ? FString(TEXT("in the air")) : A.Location.IsEmpty() ? FString(TEXT("?")) : A.Location;
		FString Hall; int32 Stage = 0; bool bBack = false;
		if (A.Status == TEXT("flying")) return TEXT("flying");
		if (bHere && AHangarTow::TowOf(W, A.Reg, Hall, Stage, bBack))
		{
			Where = FString::Printf(TEXT("%s, hangar %s"), *A.Location, *Hall);
			if (bBack) return Stage <= 1 ? FString::Printf(TEXT("being towed out of hangar %s"), *Hall) : FString(TEXT("back at its stand"));
			if (Stage == 0) return FString::Printf(TEXT("waiting for the tow to hangar %s"), *Hall);
			if (Stage == 1) return FString::Printf(TEXT("being towed in to hangar %s"), *Hall);
			return FString::Printf(TEXT("in hangar %s"), *Hall);
		}
		if (bHere && !A.HangarIn.IsEmpty()) { Where = FString::Printf(TEXT("%s, hangar %s"), *A.Location, *A.HangarIn); return FString::Printf(TEXT("in hangar %s"), *A.HangarIn); }
		if (bHere && !A.Stand.IsEmpty()) Where = FString::Printf(TEXT("%s, stand %s"), *A.Location, *A.Stand);
		if (A.Status == TEXT("maintenance")) return bHere ? FString(TEXT("waiting for the tow")) : FString(TEXT("in its check there"));
		if (A.Status == TEXT("grounded")) return A.MaintWait.IsEmpty() ? FString(TEXT("grounded")) : FString::Printf(TEXT("grounded - its check %s"), *A.MaintWait);
		if (!A.MaintWait.IsEmpty()) return FString::Printf(TEXT("its check %s"), *A.MaintWait);   // (to-do 311: not enough free hired mechanics)
		if (A.Status == TEXT("leased")) return TEXT("leased out");
		return bHere ? (A.Stand.IsEmpty() ? FString(TEXT("parked")) : FString(TEXT("at stand"))) : FString(TEXT("on the ground"));
	}

	void Send(const FString& Reg)
	{
		AADPHUD* H = Hud.Get(); UADPSim* S = H ? UADPSim::Get(H) : nullptr;
		SayLeft = 8.f; bSayBad = true;
		if (!S || !S->IsRunning()) { Say = TEXT("No career is running."); return; }
		FADPAircraft* A = MpPlane(S, Reg); if (!A) { Say = FString::Printf(TEXT("%s is not ours any more."), *Reg); return; }
		const FString Kind = PickKind.FindRef(Reg), Hall = PickHall.FindRef(Reg);
		FString Why; double Usd = 0.0, Hours = 0.0;
		TArray<FString> Free; FreeHalls(Free);
		if (A->Location != ADPBoards::HereIcao()) Why = TEXT("not at our airport");
		else if (A->Status == TEXT("flying")) Why = TEXT("flying");
		else if (A->Status == TEXT("maintenance")) Why = TEXT("already in a check");
		else if (!Free.Num()) Why = TEXT("no free hall");
		else if (!Hall.IsEmpty() && !Free.Contains(Hall)) Why = FString::Printf(TEXT("hangar %s is not free any more"), *Hall);
		else if (ADPMaint::Quote(S->World, *A, Kind, Usd, Hours, Why) && ADPMaint::StartCheck(S->World, *A, Kind, S->Now(), Why)) Why.Reset();
		else if (Why.IsEmpty()) Why = TEXT("refused");
		if (Why.IsEmpty())
		{
			A->HangarWant = Hall;
			bSayBad = false;
			Say = FString::Printf(TEXT("%s: %s started (%s, %s) - the tow takes it to %s."), *Reg, MpCheckName(Kind), *MpMoney(Usd), *MpHours(A->CheckLenH), Hall.IsEmpty() ? TEXT("the first free hangar") : *(TEXT("hangar ") + Hall));
		}
		else Say = FString::Printf(TEXT("%s not sent to the hangar: %s."), *Reg, *Why);
		ADP_LOG(TEXT("Maintenance page: send %s for %s to %s - %s"), *Reg, MpCheckName(Kind), Hall.IsEmpty() ? TEXT("the first free hall") : *Hall, Why.IsEmpty() ? TEXT("started") : *Why);
		LastSig.Reset(); Rebuild();
	}

	TSharedRef<SWidget> Choice(const FString& Label, bool bOn, TFunction<void()> Do)
	{
		return SNew(SBox).Padding(FMargin(0, 0, 4, 2))
		[
			SNew(SButton).ButtonColorAndOpacity(bOn ? MpGold : FLinearColor(0.35f, 0.45f, 0.55f, 1.f))
			.OnClicked_Lambda([this, Do]() { Do(); LastSig.Reset(); Rebuild(); return FReply::Handled(); })
			[MpText(Label, bOn ? FLinearColor::Black : MpInk, 11, bOn ? "Bold" : "Regular")]
		];
	}

	void Rebuild()
	{
		AADPHUD* H = Hud.Get(); if (!H || !Scroll.IsValid()) return;
		UADPSim* S = UADPSim::Get(H); const bool bRun = S && S->IsRunning();
		const FADPAirline* Al = bRun ? S->World.Airlines.FindByPredicate([](const FADPAirline& X) { return X.Id == UADPSim::Me(); }) : nullptr;
		MoneyLine = Al ? FString::Printf(TEXT("Money: %s"), *MpMoney(Al->Balance)) : FString();
		const FString Here = ADPBoards::HereIcao();
		TArray<FString> Free; FreeHalls(Free);
		const int32 Mode = ADPMaint::MaintTime();
		// what each row shows (the time left and bars are live lambdas, not in here)
		struct FRow { FString Reg, Type, Where, State, Check, Quote, Not, Kind, Hall; int32 Cond = 0; bool bSend = false, bFault = false, bIn = false; };
		TArray<FRow> Rows;
		if (bRun) for (const FADPAircraft& A : S->World.Aircraft)
		{
			if (A.AirlineId != UADPSim::Me()) continue;
			FRow R; R.Reg = A.Reg; R.Cond = FMath::RoundToInt(A.Condition);
			const FADPTypeInfo* Ti = ADPWorldData::Type(A.TypeId); R.Type = Ti && !Ti->Name.IsEmpty() ? Ti->Name : A.TypeId;
			R.State = StateOf(A, Here, R.Where);
			R.bIn = A.Status == TEXT("maintenance");
			R.bFault = !A.FaultDue.IsEmpty();
			if (R.bIn) { const ADPMaint::FFault* F = ADPMaint::Fault(A.CheckFault); R.Check = FString::Printf(TEXT("%s (%s-check)"), F ? F->Name : *A.CheckFault, *A.CheckKind); }
			else if (R.bFault) { const ADPMaint::FFault* F = ADPMaint::Fault(A.FaultDue); R.Check = FString::Printf(TEXT("to put right: %s"), F ? F->Name : *A.FaultDue); }
			else { double Fr = 0.0; const FString Nd = ADPMaint::NextDue(S->World, A, S->Now(), Fr); R.Check = Nd.IsEmpty() ? FString(TEXT("no check due")) : FString::Printf(TEXT("next: %s, %d %% of its interval"), MpCheckName(Nd), FMath::RoundToInt(Fr * 100.0)); }
			R.bSend = A.Location == Here && A.Status != TEXT("flying");
			if (R.bSend)
			{
				FString& K = PickKind.FindOrAdd(A.Reg);
				if (K.IsEmpty() || (K == TEXT("F") && !R.bFault)) { double Fr = 0.0; const FString Nd = ADPMaint::NextDue(S->World, A, S->Now(), Fr); K = R.bFault ? FString(TEXT("F")) : Nd.IsEmpty() ? FString(TEXT("A")) : Nd; }
				R.Kind = K;
				FString& Hl = PickHall.FindOrAdd(A.Reg);
				if (!Hl.IsEmpty() && !Free.Contains(Hl)) Hl.Reset();
				R.Hall = Hl;
				double Usd = 0.0, Hr = 0.0; FString Why;
				if (A.Status == TEXT("maintenance")) R.Not = TEXT("already in a check");
				else if (!Free.Num()) R.Not = TEXT("no free hall");
				else if (!ADPMaint::Quote(S->World, A, K, Usd, Hr, Why)) R.Not = Why;
				if (Usd > 0.0 || Hr > 0.0) R.Quote = FString::Printf(TEXT("%s - %s (%s)"), *MpMoney(Usd), *MpHours(Hr), Mode == 0 ? TEXT("Quick") : Mode == 2 ? TEXT("Real") : TEXT("Fast"));
			}
			Rows.Add(R);
		}
		FString Sig = MoneyLine + (bRun ? TEXT("r") : TEXT("-")) + FString::FromInt(Mode); for (const FString& F : Free) Sig += F;
		for (const FRow& R : Rows) Sig += R.Reg + R.Where + R.State + R.Check + R.Quote + R.Not + R.Kind + R.Hall + FString::FromInt(R.Cond) + (R.bSend ? TEXT("s") : TEXT("-"));
		if (Sig == LastSig) return;
		LastSig = Sig;
		Scroll->ClearChildren();
		if (!bRun) { Scroll->AddSlot()[MpText(TEXT("No career is running."), MpDim, 13)]; return; }
		Scroll->AddSlot().Padding(0, 0, 0, 6)[MpText(FString::Printf(TEXT("%d aircraft - %d free hangar%s here"), Rows.Num(), Free.Num(), Free.Num() == 1 ? TEXT("") : TEXT("s")), MpDim, 12)];
		Scroll->AddSlot().Padding(0, 0, 0, 2)
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(MpHead).Padding(FMargin(10, 4))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.20f)[MpText(TEXT("Aircraft"), MpGold, 12, "Bold")]
				+ SHorizontalBox::Slot().FillWidth(0.10f)[MpText(TEXT("Condition"), MpGold, 12, "Bold")]
				+ SHorizontalBox::Slot().FillWidth(0.18f)[MpText(TEXT("Where"), MpGold, 12, "Bold")]
				+ SHorizontalBox::Slot().FillWidth(0.20f)[MpText(TEXT("State"), MpGold, 12, "Bold")]
				+ SHorizontalBox::Slot().FillWidth(0.32f)[MpText(TEXT("Check or fault"), MpGold, 12, "Bold")]
			]
		];
		if (!Rows.Num()) Scroll->AddSlot()[MpText(TEXT("No aircraft of ours yet."), MpDim, 12)];
		TWeakObjectPtr<AADPHUD> WH = Hud;
		for (const FRow& R : Rows)
		{
			const FString Reg = R.Reg;
			const FLinearColor CondC = R.Cond < 25 ? MpBad : R.Cond < 60 ? MpGold : MpGood;
			auto Live = [WH, Reg](double& Left, float& Done) -> bool
			{
				AADPHUD* Hh = WH.Get(); UADPSim* Ss = Hh ? UADPSim::Get(Hh) : nullptr; const FADPAircraft* A = MpPlane(Ss, Reg);
				if (!A || A->Status != TEXT("maintenance") || A->CheckLenH <= 0.0) return false;
				const FDateTime Now = Ss->Now();
				Left = (A->CheckUntil - Now).GetTotalHours(); Done = FMath::Clamp(float((Now - A->CheckStart).GetTotalHours() / A->CheckLenH), 0.f, 1.f);
				return true;
			};
			TSharedRef<SVerticalBox> CheckCol = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[MpText(R.Check, R.bIn || R.bFault ? MpGold : MpDim, 12)];
			if (R.bIn)
			{
				CheckCol->AddSlot().AutoHeight().Padding(0, 2, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[SNew(SBox).HeightOverride(10.f)[SNew(SProgressBar).FillColorAndOpacity(MpGood).Percent_Lambda([Live]() { double L; float D; return Live(L, D) ? TOptional<float>(D) : TOptional<float>(0.f); })]]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[SNew(STextBlock).Font(MpFont(11)).ColorAndOpacity(MpInk).Text_Lambda([Live]() { double L; float D; return FText::FromString(Live(L, D) ? (L > 0.0 ? MpHours(L) + TEXT(" left") : FString(TEXT("done"))) : FString()); })]
				];
			}
			TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.20f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[MpText(R.Reg, MpInk, 13, "Bold")]
						+ SVerticalBox::Slot().AutoHeight()[MpText(R.Type, MpDim, 11)]
					]
					+ SHorizontalBox::Slot().FillWidth(0.10f)[MpText(FString::Printf(TEXT("%d %%"), R.Cond), CondC, 12)]
					+ SHorizontalBox::Slot().FillWidth(0.18f)[MpText(R.Where, MpDim, 12)]
					+ SHorizontalBox::Slot().FillWidth(0.20f)[MpText(R.State, R.State == TEXT("flying") ? MpInk : R.bIn ? MpGold : MpInk, 12)]
					+ SHorizontalBox::Slot().FillWidth(0.32f)[CheckCol]
				];
			if (R.bSend)
			{
				// Send to hangar: the check, the hall, the price and length, the button (or why not)
				TSharedRef<SWrapBox> Kinds = SNew(SWrapBox).UseAllottedSize(true);
				Kinds->AddSlot()[MpText(TEXT("Check: "), MpDim, 11)];
				TArray<FString> Ks = {TEXT("A"), TEXT("C"), TEXT("D")}; if (R.bFault) Ks.Add(TEXT("F"));
				for (const FString& K : Ks)
				{
					FString Lb = MpCheckName(K); if (K == TEXT("F")) { FString Fd; for (const FADPAircraft& A : S->World.Aircraft) if (A.Reg == Reg) Fd = A.FaultDue; const ADPMaint::FFault* F = ADPMaint::Fault(Fd); Lb = FString::Printf(TEXT("Put right %s"), F ? F->Name : *Fd); }
					Kinds->AddSlot()[Choice(Lb, R.Kind == K, [this, Reg, K]() { PickKind.Add(Reg, K); })];
				}
				Kinds->AddSlot()[MpText(TEXT("   Hangar: "), MpDim, 11)];
				Kinds->AddSlot()[Choice(TEXT("First free"), R.Hall.IsEmpty(), [this, Reg]() { PickHall.Add(Reg, FString()); })];
				for (const FString& Hl : Free) Kinds->AddSlot()[Choice(Hl, R.Hall == Hl, [this, Reg, Hl]() { PickHall.Add(Reg, Hl); })];
				Box->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Kinds]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 8, 0)[MpText(R.Quote, MpInk, 11)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							SNew(SButton).IsEnabled(R.Not.IsEmpty()).ToolTipText(FText::FromString(R.Not))
							.OnClicked_Lambda([this, Reg]() { Send(Reg); return FReply::Handled(); })
							[MpText(TEXT("Send to hangar"), R.Not.IsEmpty() ? MpInk : MpDim, 12, "Bold")]
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[MpText(R.Not, MpBad, 10)]
					]
				];
			}
			Scroll->AddSlot().Padding(0, 1, 0, 1)
			[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(MpRow).Padding(FMargin(10, 5))[Box]];
		}
	}
};

void AADPHUD::OpenMaint(const FString& Kind)
{
	if (!bDevice || !MaintBox.IsValid()) { ADP_WARN(TEXT("Maintenance page: not opened - no device is up (%s)"), *Kind); return; }
	if (bVehiclesPage) CloseVehicles();   // (one page over the device at a time)
	TSharedRef<SMaintPage> P = SNew(SMaintPage, this);
	MaintBox->SetContent(P);
	bMaintPage = true;
	FSlateApplication::Get().SetKeyboardFocus(P, EFocusCause::SetDirectly);
	ADP_LOG(TEXT("Maintenance page: opened from the %s"), Kind.IsEmpty() ? *DeviceKind : *Kind);
}

void AADPHUD::CloseMaint()
{
	if (!bMaintPage) return;
	bMaintPage = false;
	if (MaintBox.IsValid()) MaintBox->SetContent(SNullWidget::NullWidget);
	ADP_LOG(TEXT("Maintenance page: closed"));
}
