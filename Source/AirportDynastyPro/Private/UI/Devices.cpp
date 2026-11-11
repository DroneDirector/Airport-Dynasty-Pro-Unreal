// Your phone and the computers in the rooms (ADPHUD.h, owner 2026-11-11). In the 3D terminal you use them as in the
// game's own rooms: look at the operations room's monitor, your office's monitor or laptop and press E (the
// controller's X, or click it), and the game's computer opens over the room: its desktop with the taskbar, every app,
// the operations screens as apps of their own (the game's screens/ue-device.ts draws it, the same as on the Android
// edition's desk). Your smartphone lies on the desk with its screen off; take it and it comes up off the desk to you,
// its screen comes on and its apps open in front (the phone's own frame). Esc (or a tap beside the phone, or the
// legend's button at the bottom: Esc and a phone with an arrow curving down) puts it away: the phone goes back down to
// where it lay. The menus' page draws it, see-through over the room, which keeps being drawn behind.
#include "UI/ADPHUD.h"
#include "UI/ADPBridge.h"
#include "UI/GameView.h"
#include "BabylonModel.h"
#include "SWebBrowser.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** A phone with an arrow curving down beside it: "put the phone down". */
	class SPhoneDownIcon : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPhoneDownIcon) {}
			SLATE_ARGUMENT(FLinearColor, Color)
		SLATE_END_ARGS()
		void Construct(const FArguments& A) { Color = A._Color; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(30, 28); }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			auto Line = [&](const TArray<FVector2D>& P, float W) { FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), P, ESlateDrawEffect::None, Color, true, W); };
			// the phone: its body and the bottom of its screen
			Line({ FVector2D(3, 4), FVector2D(13, 4), FVector2D(13, 24), FVector2D(3, 24), FVector2D(3, 4) }, 1.8f);
			Line({ FVector2D(6, 21), FVector2D(10, 21) }, 1.4f);
			// the arrow: from above the phone's side, curving round and down, its head pointing down
			TArray<FVector2D> Arc; const FVector2D C(18, 13); const float R = 8.f;
			for (int32 i = 0; i <= 12; ++i) { const float A = FMath::DegreesToRadians(-100.f + 100.f * i / 12.f); Arc.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R); }
			Line(Arc, 1.8f);
			const FVector2D End = C + FVector2D(R, 0), Tip = End + FVector2D(0, 5);
			Line({ End, Tip }, 1.8f);
			Line({ Tip + FVector2D(-3.5f, -3.5f), Tip, Tip + FVector2D(3.5f, -3.5f) }, 1.8f);
			return Layer + 1;
		}
	private:
		FLinearColor Color = FLinearColor::White;
	};

	const float PhoneLift = 0.55f;   // seconds, off the desk up to you (and back down)
}

TSharedRef<SWidget> AADPHUD::MakeDeviceLegend()
{
	const FLinearColor LegendInk(0.9f, 0.96f, 1.f), KeyCap(0.12f, 0.18f, 0.26f, 0.95f);
	return SNew(SButton)
		.ButtonColorAndOpacity(FLinearColor(0.02f, 0.05f, 0.09f, 0.78f))
		.ContentPadding(FMargin(12, 8))
		.ToolTipText(FText::FromString(TEXT("Put it away (Esc)")))
		.OnClicked_Lambda([this]() { PutDeviceAway(); return FReply::Handled(); })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(KeyCap).Padding(FMargin(8, 3))
				[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)).ColorAndOpacity(LegendInk).Text(FText::FromString(TEXT("Esc")))]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[SNew(SPhoneDownIcon).Color(LegendInk)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11)).ColorAndOpacity(LegendInk)
				.Text_Lambda([this]() { return FText::FromString(DeviceKind == TEXT("phone") || PhoneDir != 0 ? TEXT("Put the phone down") : TEXT("Back to the room")); })]
		];
}

void AADPHUD::MakeGameView()
{
	if (bGameMade) return;
	Bridge = NewObject<UADPBridge>(this);
	Bridge->Hud = this;
	StaticCastSharedPtr<SBox>(GameBox)->SetContent(ADPGameView::Make(Bridge, &Browser));
	bGameMade = true;
}

bool AADPHUD::TryDevice(const FHitResult& Hit)
{
	ABabylonModel* M = Cast<ABabylonModel>(Hit.GetActor());
	if (!M || bDevice || PhoneDir != 0) return false;
	// the operations room's monitor (whichever kind is on the desk)
	if (M->Model.StartsWith(TEXT("Monitor"))) { OpenDevice(TEXT("ops")); return true; }
	// your phone: off the desk and up to you first
	if (M->Model == TEXT("Smartphone"))
	{
		Phone = M; PhoneHome = M->GetActorTransform(); PhoneK = 0.f; PhoneDir = 1;
		UpdateMouse();
		return true;
	}
	// your office's monitor or laptop
	const FString Node = M->NodeAt(Hit.GetComponent(), Hit.Item);
	if (M->Model.StartsWith(TEXT("Office")) && (Node.StartsWith(TEXT("laptop~")) || Node.StartsWith(TEXT("planner~")))) { OpenDevice(TEXT("office")); return true; }
	return false;
}

void AADPHUD::OpenDevice(const FString& Kind)
{
	if (bDevice || bGame || !GameBox.IsValid()) return;
	MakeGameView();
	bDevice = true; DeviceKind = Kind; DevicePending = Kind;
	Breadcrumb(Kind == TEXT("phone") ? TEXT("Using your phone") : TEXT("At a computer"));
	GameBox->SetVisibility(EVisibility::Visible);
	if (TopBar.IsValid()) TopBar->SetVisibility(EVisibility::Collapsed);
	if (Credit.IsValid()) Credit->SetVisibility(EVisibility::Collapsed);
	if (MiniBox.IsValid()) MiniBox->SetVisibility(EVisibility::Collapsed);
	if (DeviceLegend.IsValid()) DeviceLegend->SetVisibility(EVisibility::Visible);
	UpdateMouse();
}

void AADPHUD::PutDeviceAway()
{
	if (bDevice && Browser.IsValid()) { Browser->ExecuteJavascript(TEXT("window.__ueDeviceClose ? window.__ueDeviceClose() : (window.ue && ue.adp && ue.adp.devicedone(''))")); return; }
	if (bDevice) DeviceDone(DeviceKind);
}

void AADPHUD::DeviceDone(const FString& Kind)
{
	if (!bDevice) return;
	bDevice = false; DevicePending.Empty();
	if (!bGame && GameBox.IsValid()) GameBox->SetVisibility(EVisibility::Collapsed);
	if (TopBar.IsValid()) TopBar->SetVisibility(EVisibility::Visible);
	if (Credit.IsValid()) Credit->SetVisibility(EVisibility::HitTestInvisible);
	if (MiniBox.IsValid()) MiniBox->SetVisibility(EVisibility::Visible);
	// the phone back down onto the desk
	if (ABabylonModel* P = Phone.Get()) { P->SetActorHiddenInGame(false); PhoneDir = -1; }
	if (PhoneDir == 0 && DeviceLegend.IsValid()) DeviceLegend->SetVisibility(EVisibility::Collapsed);
	DeviceKind = PhoneDir != 0 ? TEXT("phone") : FString();
	UpdateMouse();
}

void AADPHUD::TickDevice(float Dt)
{
	// the page asked for the phone or the computer once it's there (made the first time: it loads)
	if (!DevicePending.IsEmpty() && Browser.IsValid() && Browser->IsLoaded())
	{
		Browser->ExecuteJavascript(FString::Printf(TEXT("window.__ueDevice && window.__ueDevice('%s')"), *DevicePending));
		DevicePending.Empty();
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocus(Browser, EFocusCause::SetDirectly);
	}
	// the phone: off the desk and up in front of your eyes (its screen to you, upright), or back down where it lay
	ABabylonModel* P = Phone.Get();
	if (!P || PhoneDir == 0) return;
	APlayerController* PC = GetOwningPlayerController(); if (!PC) return;
	PhoneK = FMath::Clamp(PhoneK + PhoneDir * Dt / PhoneLift, 0.f, 1.f);
	FVector Eye; FRotator Look; PC->GetPlayerViewPoint(Eye, Look);
	const FRotationMatrix CamM(Look);
	const FVector F = CamM.GetUnitAxis(EAxis::X), U = CamM.GetUnitAxis(EAxis::Z);
	// the model's top is its -y, its screen its +z (Art/Smartphone/make_smartphone.py, as placed)
	const FTransform Held(FRotationMatrix::MakeFromYZ(-U, -F).ToQuat(), Eye + F * 24.f - U * 1.5f);
	const float E = FMath::SmoothStep(0.f, 1.f, PhoneK);
	FTransform Now; Now.Blend(PhoneHome, Held, E);
	// a little arc on the way (up first, then towards you), as a hand lifts it
	Now.AddToTranslation(FVector(0, 0, 8.f * FMath::Sin(PI * E)));
	P->SetActorTransform(Now);
	if (PhoneDir > 0 && PhoneK >= 1.f) { PhoneDir = 0; P->SetActorHiddenInGame(true); OpenDevice(TEXT("phone")); }
	else if (PhoneDir < 0 && PhoneK <= 0.f)
	{
		PhoneDir = 0; P->SetActorTransform(PhoneHome); Phone = nullptr; DeviceKind.Empty();
		if (DeviceLegend.IsValid()) DeviceLegend->SetVisibility(EVisibility::Collapsed);
		UpdateMouse();
	}
}

void AADPHUD::PickPhoneNear(const FVector& At)
{
	ABabylonModel* Best = nullptr; double Bd = 1e12;
	for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
		if (It->Model == TEXT("Smartphone")) { const double D = FVector::Dist(It->GetActorLocation(), At); if (D < Bd) { Bd = D; Best = *It; } }
	if (!Best || bDevice || PhoneDir != 0) return;
	Phone = Best; PhoneHome = Best->GetActorTransform(); PhoneK = 0.f; PhoneDir = 1;
	UpdateMouse();
}
