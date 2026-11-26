// Vehicles owned (to-do 318): a native page over the tablet's, the computers' and the phone's screen (the Vehicles app on
// their desktop: ue.adp.vehicles). Every airport vehicle you have, grouped by kind with how many, each with its condition
// (its wear, GSEBreak.cpp), what it does and where it is (ADPOwned::List, VehiclesOwned.h); a Buy button per kind with its
// price - it buys one more, delivered to a free place of its kind at the vehicle area (ADPGround::BuyVehicle, GSESpare.cpp),
// or says why not. Updated once a second while it is open; Esc or Close: back to the device's desktop.
#include "UI/ADPHUD.h"
#include "VehiclesOwned.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSimData.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPVehicleArea.h"
#include "ADPLog.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor VpPanel(0.02f, 0.05f, 0.09f, 0.96f), VpRow(0.05f, 0.11f, 0.18f, 1.f), VpHead(0.07f, 0.16f, 0.26f, 1.f);
	const FLinearColor VpInk(0.85f, 0.94f, 1.f), VpDim(0.55f, 0.68f, 0.78f), VpGold(0.96f, 0.71f, 0.27f), VpBad(1.f, 0.42f, 0.36f), VpGood(0.45f, 0.9f, 0.55f);
	FSlateFontInfo VpFont(int32 Size, const char* Kind = "Regular") { return FCoreStyle::GetDefaultFontStyle(Kind, Size); }
	// (the menus' money: $ and thousands with commas)
	FString VpMoney(double V)
	{
		FString D = FString::Printf(TEXT("%lld"), (long long)FMath::RoundToDouble(FMath::Abs(V))), R;
		for (int32 i = 0; i < D.Len(); ++i) { if (i && (D.Len() - i) % 3 == 0) R += TEXT(","); R += D[i]; }
		return (V < 0 ? TEXT("-$") : TEXT("$")) + R;
	}
	TSharedRef<SWidget> VpText(const FString& S, const FLinearColor& C, int32 Size = 12, const char* Kind = "Regular")
	{
		return SNew(STextBlock).Text(FText::FromString(S)).ColorAndOpacity(C).Font(VpFont(Size, Kind));
	}
}

class SVehiclesPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVehiclesPage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, AADPHUD* InHud)
	{
		Hud = InHud;
		ChildSlot
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(VpPanel).Padding(FMargin(22.f, 16.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[VpText(TEXT("Vehicles owned"), VpGold, 20, "Bold")]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 18, 0)
					[SNew(STextBlock).Font(VpFont(13, "Bold")).ColorAndOpacity(VpInk).Text_Lambda([this]() { return FText::FromString(MoneyLine); })]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SButton).OnClicked_Lambda([this]() { if (AADPHUD* H = Hud.Get()) H->CloseVehicles(); return FReply::Handled(); })
						[VpText(TEXT("Close  (Esc)"), VpInk, 12, "Bold")]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[SNew(STextBlock).Font(VpFont(12)).AutoWrapText(true).ColorAndOpacity_Lambda([this]() { return FSlateColor(bSayBad ? VpBad : VpGood); }).Text_Lambda([this]() { return FText::FromString(Say); })]
				+ SVerticalBox::Slot().FillHeight(1.f)[SAssignNew(Scroll, SScrollBox)]
			]
		];
		Rebuild();
		RegisterActiveTimer(1.f, FWidgetActiveTimerDelegate::CreateSP(this, &SVehiclesPage::Tock));
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& G, const FKeyEvent& K) override
	{
		if (K.GetKey() == EKeys::Escape || K.GetKey() == EKeys::Gamepad_FaceButton_Right) { if (AADPHUD* H = Hud.Get()) H->CloseVehicles(); return FReply::Handled(); }
		return SCompoundWidget::OnKeyDown(G, K);
	}

private:
	TWeakObjectPtr<AADPHUD> Hud; TSharedPtr<SScrollBox> Scroll; FString MoneyLine, Say, LastSig; bool bSayBad = false; float SayLeft = 0.f;
	TMap<FString, FString> ExNot; double ExNotAt = -100.0;

	EActiveTimerReturnType Tock(double, float)
	{
		AADPHUD* H = Hud.Get(); if (!H) return EActiveTimerReturnType::Stop;
		if (H->bVehiclesPage) Rebuild();
		if (SayLeft > 0.f && (SayLeft -= 1.f) <= 0.f) Say.Reset();
		return EActiveTimerReturnType::Continue;
	}

	void Buy(const FString& Key, const FString& Label)
	{
		AADPHUD* H = Hud.Get(); UADPSim* S = H ? UADPSim::Get(H) : nullptr;
		if (!S || !S->IsRunning()) { Say = TEXT("No career is running."); bSayBad = true; SayLeft = 8.f; return; }
		FString Why; const bool bOk = ADPGround::BuyVehicle(S->World, UADPSim::Me(), Key, S->Now(), Why, H->GetWorld());
		bSayBad = !bOk; SayLeft = 8.f;
		const bool bEx = ADPOwned::Extra(Key) != nullptr; ExNotAt = -100.0;
		Say = bOk ? FString::Printf(TEXT("A new %s is bought: it is brought to its %s."), *Label, bEx ? TEXT("spot beside the others when nobody is looking") : TEXT("place at the vehicle area")) : FString::Printf(TEXT("No %s bought: %s."), *Label, *Why);
		ADP_LOG(TEXT("Vehicles owned: buy a %s - %s"), *Label, bOk ? TEXT("bought") : *Why);
		LastSig.Reset(); Rebuild();
	}

	// (why no Buy for this kind, or empty: it can be bought - the same checks BuyVehicle makes)
	FString BuyNot(const FADPWorld& W, const FADPAirline* A, const ADPVehicleArea::FKind* K) const
	{
		if (!K) return TEXT("no parking place for one in the vehicle area");
		if (!ADPVehicleArea::Built() && K->Key != TEXT("fuel") && K->Key != TEXT("catering")) return TEXT("no vehicle area to keep it at yet");
		if (ADPVehicleArea::Built() && !ADPVehicleArea::FirstFree(W, *K)) return TEXT("the vehicle area has no free place for one");
		if (!A || A->Balance < K->Price) return TEXT("not enough money");
		return FString();
	}

	// (the same for a kind parked beside its own, the spot looked for as BuyVehicle does: every 5 s, not every redraw)
	FString BuyNotExtra(const FADPWorld& W, const FADPAirline* A, const ADPOwned::FExtraKind& E)
	{
		if (!A || A->Balance < E.Price) return TEXT("not enough money");
		const double Now = FPlatformTime::Seconds();
		if (Now - ExNotAt > 5.0) { ExNot.Reset(); ExNotAt = Now; }
		if (const FString* C = ExNot.Find(E.Key)) return *C;
		FADPSpare Sp; FString Why; AADPHUD* H = Hud.Get();
		const FString R = ADPOwned::ExtraSpot(H ? H->GetWorld() : nullptr, W, E.Key, Sp, Why) ? FString() : Why;
		ExNot.Add(E.Key, R); return R;
	}

	void Rebuild()
	{
		AADPHUD* H = Hud.Get(); if (!H || !Scroll.IsValid()) return;
		UADPSim* S = UADPSim::Get(H); const bool bRun = S && S->IsRunning();
		const FADPAirline* A = bRun ? S->World.Airlines.FindByPredicate([](const FADPAirline& X) { return X.Id == UADPSim::Me(); }) : nullptr;
		MoneyLine = A ? FString::Printf(TEXT("Money: %s"), *VpMoney(A->Balance)) : FString();
		TArray<FOwnedVehicle> L; ADPOwned::List(H->GetWorld(), L);
		// (built again only when something shown changed: a button under the pointer stays the same button)
		FString Sig = MoneyLine + (bRun ? TEXT("r") : TEXT("-")); for (const FOwnedVehicle& V : L) Sig += V.Name + V.State + V.Place + FString::FromInt(V.Condition);
		if (bRun) for (const ADPVehicleArea::FKind& K : ADPVehicleArea::Kinds()) Sig += BuyNot(S->World, A, &K);
		if (bRun) for (const ADPOwned::FExtraKind& E : ADPOwned::Extras()) Sig += BuyNotExtra(S->World, A, E);
		if (Sig == LastSig) return;
		LastSig = Sig;
		// (every kind you have, then the ones the vehicle area keeps that you have none of yet)
		TArray<FString> Kinds; for (const FOwnedVehicle& V : L) Kinds.AddUnique(V.Kind);
		for (const ADPVehicleArea::FKind& K : ADPVehicleArea::Kinds()) Kinds.AddUnique(K.Label);
		for (const ADPOwned::FExtraKind& E : ADPOwned::Extras()) Kinds.AddUnique(E.Label);
		Scroll->ClearChildren();
		if (!bRun) Scroll->AddSlot()[VpText(TEXT("No career is running."), VpDim, 13)];
		Scroll->AddSlot().Padding(0, 0, 0, 6)[VpText(FString::Printf(TEXT("%d vehicles at the airport"), L.Num()), VpDim, 12)];
		for (const FString& Kind : Kinds)
		{
			const ADPVehicleArea::FKind* K = ADPVehicleArea::Kinds().FindByPredicate([&](const ADPVehicleArea::FKind& X) { return X.Label == Kind; });
			int32 N = 0, NWork = 0, NBroken = 0, NOff = 0;
			for (const FOwnedVehicle& V : L) if (V.Kind == Kind) { ++N; NWork += V.bWork; NBroken += V.bBroken; NOff += V.bWrittenOff; }
			const ADPOwned::FExtraKind* E = K ? nullptr : ADPOwned::Extra(Kind);   // (no place at the vehicle area: one more beside its own, GSEExtra.cpp)
			const FString Not = !bRun ? FString(TEXT("no career is running")) : E ? BuyNotExtra(S->World, A, *E) : BuyNot(S->World, A, K);
			const FString Key = K ? K->Key : E ? FString(E->Key) : FString(), Label = Kind;
			FString Count = FString::Printf(TEXT("%d"), N);
			if (NWork) Count += FString::Printf(TEXT(" - %d at work"), NWork);
			if (NBroken) Count += FString::Printf(TEXT(" - %d broken down"), NBroken);
			if (NOff) Count += FString::Printf(TEXT(" - %d written off"), NOff);
			const FString BuyLine = K || E ? FString::Printf(TEXT("Buy one more - %s"), *VpMoney(K ? K->Price : E->Price)) : FString(TEXT("Buy"));
			Scroll->AddSlot().Padding(0, 8, 0, 2)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(VpHead).Padding(FMargin(12, 6))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)[VpText(Kind.Left(1).ToUpper() + Kind.Mid(1), VpGold, 15, "Bold")]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[VpText(Count, VpInk, 12)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							SNew(SButton).IsEnabled(Not.IsEmpty()).ToolTipText(FText::FromString(Not))
							.OnClicked_Lambda([this, Key, Label]() { Buy(Key, Label); return FReply::Handled(); })
							[VpText(BuyLine, Not.IsEmpty() ? VpInk : VpDim, 12, "Bold")]
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[VpText(Not, VpDim, 10)]
					]
				]
			];
			for (const FOwnedVehicle& V : L) if (V.Kind == Kind)
			{
				const FString Cond = V.Condition < 0 ? FString(TEXT("condition not kept")) : FString::Printf(TEXT("condition %d %%"), V.Condition);
				const FLinearColor CondC = V.Condition < 0 ? VpDim : V.Condition < 25 ? VpBad : V.Condition < 60 ? VpGold : VpGood;
				const FLinearColor StC = V.bWrittenOff || V.bBroken ? VpBad : V.bWork ? VpGold : VpInk;
				Scroll->AddSlot().Padding(14, 1, 0, 1)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(VpRow).Padding(FMargin(10, 4))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(0.26f)[VpText(V.Name + (V.bBought ? TEXT("  (bought)") : TEXT("")), VpInk, 12)]
						+ SHorizontalBox::Slot().FillWidth(0.18f)[VpText(Cond, CondC, 12)]
						+ SHorizontalBox::Slot().FillWidth(0.30f)[SNew(STextBlock).Text(FText::FromString(V.State)).ColorAndOpacity(StC).Font(VpFont(12)).AutoWrapText(true)]
						+ SHorizontalBox::Slot().FillWidth(0.26f)[SNew(STextBlock).Text(FText::FromString(V.Place)).ColorAndOpacity(VpDim).Font(VpFont(12)).AutoWrapText(true)]
					]
				];
			}
			if (!N) Scroll->AddSlot().Padding(14, 1, 0, 1)[VpText(TEXT("none yet"), VpDim, 11)];
		}
	}
};

void AADPHUD::OpenVehicles(const FString& Kind)
{
	if (!bDevice || !VehiclesBox.IsValid()) { ADP_WARN(TEXT("Vehicles owned: not opened - no device is up (%s)"), *Kind); return; }
	TSharedRef<SVehiclesPage> P = SNew(SVehiclesPage, this);
	VehiclesBox->SetContent(P);
	bVehiclesPage = true;
	FSlateApplication::Get().SetKeyboardFocus(P, EFocusCause::SetDirectly);
	ADP_LOG(TEXT("Vehicles owned: opened from the %s"), Kind.IsEmpty() ? *DeviceKind : *Kind);
}

void AADPHUD::CloseVehicles()
{
	if (!bVehiclesPage) return;
	bVehiclesPage = false;
	if (VehiclesBox.IsValid()) VehiclesBox->SetContent(SNullWidget::NullWidget);
	ADP_LOG(TEXT("Vehicles owned: closed"));
}
