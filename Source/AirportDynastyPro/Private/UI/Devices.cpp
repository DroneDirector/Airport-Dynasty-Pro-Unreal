// Your phone and the computers in the rooms (ADPHUD.h, owner 2026-11-11). In the 3D terminal you use them as in the
// game's own rooms: look at the operations room's monitor, your office's monitor or laptop and press E (the
// controller's X, or click it), and the game's computer opens over the room: its desktop with the taskbar, every app,
// the operations screens as apps of their own (the game's screens/ue-device.ts draws it, the same as on the Android
// edition's desk). Your smartphone lies on the desk with its screen off; take it and it comes up off the desk to you,
// its screen comes on and its apps open in front (the phone's own frame). Esc (or a tap beside the phone, or the
// legend's button at the bottom: Esc and a phone with an arrow curving down) puts it away: the phone goes back down to
// where it lay. The menus' page draws it, see-through over the room, which keeps being drawn behind.
#include "DoorPanelUse.h"
#include "UI/ADPHUD.h"
#include "ADPWalker.h"
#include "ADPKeep.h"
#include "Kismet/GameplayStatics.h"
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
#include "JetwayRigs.h"
#include "GameCursor.h"
#include "JetwayHmi.h"
#include "ADPLog.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "CollisionQueryParams.h"
#include "FireTruck.h"
#include "ServiceDrive.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

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
				.Text_Lambda([this]() { return FText::FromString(DeviceKind == TEXT("phone") || PhoneDir != 0 ? TEXT("Put the phone down") : DeviceKind == TEXT("tablet") ? TEXT("Back to holding your tablet") : DeviceKind.StartsWith(TEXT("hmi")) ? TEXT("Step back from the screen") : TEXT("Back to the room")); })]
		];
}

TSharedRef<SWidget> AADPHUD::MakePanelHint()
{
	// (owner, 2026-11-14) low in the middle while you're at a door panel: how to choose, press and step back
	const FLinearColor HintInk(0.9f, 0.96f, 1.f), Cap(0.12f, 0.18f, 0.26f, 0.95f);
	auto Key = [&](const TCHAR* K) { return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Cap).Padding(FMargin(8, 3))
		[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)).ColorAndOpacity(HintInk).Text(FText::FromString(K))]; };
	auto Say = [&](const TCHAR* T) { return SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11)).ColorAndOpacity(HintInk).Text(FText::FromString(T)); };
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.02f, 0.05f, 0.09f, 0.78f)).Padding(FMargin(14, 8))
		.Visibility_Lambda([]() { return ADoorPanelUse::IsActive() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[Key(TEXT("Mouse up / down"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[Key(TEXT("D-pad / stick"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 22, 0)[Say(TEXT("Choose green (open) or red (close)"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[Key(TEXT("E / Left click"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[Key(TEXT("X"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 22, 0)[Say(TEXT("Press"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[Key(TEXT("Esc"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[Key(TEXT("B / Back"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Say(TEXT("Step back"))]
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
	bDevice = true; DeviceKind = Kind; DevicePending = Kind == TEXT("phone") && bPhoneHeld ? FString(TEXT("phone-held")) : Kind;
	Breadcrumb(Kind == TEXT("phone") ? TEXT("Using your phone") : Kind == TEXT("tablet") ? TEXT("Using your tablet") : Kind.StartsWith(TEXT("hmi")) ? TEXT("At the jetway's control screen") : TEXT("At a computer"));
	GameBox->SetVisibility(EVisibility::Visible);
	if (TopBar.IsValid()) TopBar->SetVisibility(EVisibility::Collapsed);
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
	if (DeviceKind.StartsWith(TEXT("hmi"))) EndHmi();   // (the jetway's HMI: the camera back to you)
	if (DeviceKind == TEXT("tablet")) TabletScreenClosed();   // (your tablet: back in your hands)
	bDevice = false; DevicePending.Empty();
	if (!bGame && GameBox.IsValid()) GameBox->SetVisibility(EVisibility::Collapsed);
	if (TopBar.IsValid()) TopBar->SetVisibility(EVisibility::Visible);
	// the phone back down onto the desk
	if (ABabylonModel* P = Phone.Get()) { P->SetActorHiddenInGame(false); PhoneDir = -1; bPhoneHeld = false; bPhoneLook = false; }
	if (PhoneDir == 0 && DeviceLegend.IsValid()) DeviceLegend->SetVisibility(EVisibility::Collapsed);
	DeviceKind = PhoneDir != 0 ? TEXT("phone") : FString();
	UpdateMouse();
}

void AADPHUD::TickDevice(float Dt)
{
	TickHmi(Dt);
	// the page asked for the phone or the computer once it's there (made the first time: it loads)
	if (!DevicePending.IsEmpty() && Browser.IsValid() && Browser->IsLoaded())
	{
		Browser->ExecuteJavascript(FString::Printf(TEXT("window.__ueDevice && window.__ueDevice('%s')"), *DevicePending));
		DevicePending.Empty();
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocus(Browser, EFocusCause::SetDirectly);
	}
	// the phone (owner, 2026-11-19, his photo; docs/Device-Hold-Plan.md): your right arm reaches for it on the desk, takes it
	// and brings it up into your hand - held as in his photo, fixed in your view (the world moves, not the phone); its
	// screen live on the 3D phone (the page lays the phone's screen exactly on it); back down the same way
	ABabylonModel* P = Phone.Get();
	if (!P || (PhoneDir == 0 && !bPhoneHeld)) return;
	APlayerController* PC = GetOwningPlayerController(); if (!PC) return;
	if (PhoneDir != 0) PhoneK = FMath::Clamp(PhoneK + PhoneDir * Dt / PhoneLift, 0.f, 1.f);
	FVector Eye; FRotator Look; PC->GetPlayerViewPoint(Eye, Look);
	const FRotationMatrix CamM(Look);
	const FVector F = CamM.GetUnitAxis(EAxis::X), Rt = CamM.GetUnitAxis(EAxis::Y), U = CamM.GetUnitAxis(EAxis::Z);
	// held: a little right of the middle and low, its screen turned to your eyes, its top up (the model's top is its -y,
	// its screen its +z - Art/Smartphone/make_smartphone.py)
	const FVector At = Eye + F * 30.f + Rt * 6.5f - U * 5.f;
	const FVector N = (Eye - At).GetSafeNormal();
	const FVector Top = (U - N * FVector::DotProduct(U, N)).GetSafeNormal();
	const FTransform Held(FRotationMatrix::MakeFromYZ(-Top, N).ToQuat(), At);
	// two phases: the hand reaches it (the first third), then it comes up with the hand (an arc, as a hand lifts it)
	// (the phone held in your hand with its screen live on it is still being tuned - the hand's pose: only with -PhoneHand
	// for now; without it the phone comes up and opens over the room as before - owner, 2026-11-19: release first)
	static const bool bHandOn = FParse::Param(FCommandLine::Get(), TEXT("PhoneHand"));
	const float K = PhoneK, HandReach = FMath::SmoothStep(0.f, 1.f, FMath::Min(1.f, K / 0.35f)), Lift = FMath::SmoothStep(0.f, 1.f, FMath::Max(0.f, (K - 0.35f) / 0.65f));
	FTransform Now; Now.Blend(PhoneHome, Held, Lift);
	Now.AddToTranslation(U * (6.f * FMath::Sin(PI * Lift)));
	P->SetActorTransform(Now);
	if (bHandOn) PhoneHand(P, HandReach);
	if (!bHandOn && PhoneDir > 0 && PhoneK >= 1.f) { PhoneDir = 0; P->SetActorHiddenInGame(true); OpenDevice(TEXT("phone")); return; }
	if (PhoneDir > 0 && PhoneK >= 1.f) { PhoneDir = 0; bPhoneHeld = true; bPhoneLook = false; PhU = 0.5f; PhV = 0.5f; PhPtrSent.Empty(); FMemory::Memzero(PhQuad); OpenDevice(TEXT("phone")); if (FParse::Param(FCommandLine::Get(), TEXT("UIDevice"))) DevicePending.Empty(); }   // (the test run: no career, so no page - the hand and the 3D phone alone)
	else if (PhoneDir < 0 && PhoneK <= 0.f)
	{
		PhoneDir = 0; P->SetActorTransform(PhoneHome); Phone = nullptr; DeviceKind.Empty();
		PhoneHand(nullptr, 0.f);
		if (DeviceLegend.IsValid()) DeviceLegend->SetVisibility(EVisibility::Collapsed);
		UpdateMouse();
		return;
	}
	if (!bPhoneHeld || !Browser.IsValid()) return;
	// the "+" (the default): the mouse moves it on the phone's screen - its speed from how fast you move the mouse (slow
	// and precise, or a flick across); the pad's left stick too
	if (!bPhoneLook)
	{
		// as fast as the PC's own pointer on the window (to-do 263), by the phone's screen as large as it is there just now
		int32 Vw = 1, Vh = 1; PC->GetViewportSize(Vw, Vh);
		const FVector2D Wide((PhQuad[2] - PhQuad[0]) * Vw, (PhQuad[3] - PhQuad[1]) * Vh), High((PhQuad[6] - PhQuad[0]) * Vw, (PhQuad[7] - PhQuad[1]) * Vh);
		const FVector2D D = ADPGameCursor::Move(PC, Dt);
		PhU = FMath::Clamp(PhU + D.X / FMath::Max(120.0, Wide.Size()), 0.0, 1.0); PhV = FMath::Clamp(PhV + D.Y / FMath::Max(260.0, High.Size()), 0.0, 1.0);
	}
	const FString Ptr = FString::Printf(TEXT("window.__uePhonePtr && window.__uePhonePtr(%.4f,%.4f,%s)"), PhU, PhV, bPhoneLook ? TEXT("false") : TEXT("true"));
	if (Ptr != PhPtrSent) { PhPtrSent = Ptr; Browser->ExecuteJavascript(Ptr); }
	// the screen's four corners on the window (top-left, top-right, bottom-right, bottom-left), sent when they move
	const int32 Sc = P->NodeIndex(TEXT("Screen"));
	const TPair<UInstancedStaticMeshComponent*, int32> In = Sc != INDEX_NONE ? P->NodeInstance(Sc) : TPair<UInstancedStaticMeshComponent*, int32>(nullptr, INDEX_NONE);
	FTransform ST;
	if (In.Key && In.Key->GetStaticMesh() && In.Key->GetInstanceTransform(In.Value, ST, true))
	{
		const FBox Bx = In.Key->GetStaticMesh()->GetBoundingBox();
		const FVector Cs[4] = { FVector(Bx.Min.X, Bx.Min.Y, Bx.Max.Z), FVector(Bx.Max.X, Bx.Min.Y, Bx.Max.Z), FVector(Bx.Max.X, Bx.Max.Y, Bx.Max.Z), FVector(Bx.Min.X, Bx.Max.Y, Bx.Max.Z) };
		int32 VW = 1, VH = 1; PC->GetViewportSize(VW, VH);
		float Q[8]; bool bOk = true, bMoved = false;
		for (int32 i = 0; i < 4; ++i)
		{
			FVector2D S2; bOk &= PC->ProjectWorldLocationToScreen(ST.TransformPosition(Cs[i]), S2, false);
			Q[i * 2] = S2.X / FMath::Max(1, VW); Q[i * 2 + 1] = S2.Y / FMath::Max(1, VH);
			bMoved |= FMath::Abs(Q[i * 2] - PhQuad[i * 2]) > 0.0004f || FMath::Abs(Q[i * 2 + 1] - PhQuad[i * 2 + 1]) > 0.0004f;
		}
		{ static double SaidAt = 0.0; const double Tn = FPlatformTime::Seconds(); if (Tn - SaidAt > 1.0) { SaidAt = Tn; ADP_LOG(TEXT("Phone: screen corners %.3f,%.3f %.3f,%.3f %.3f,%.3f %.3f,%.3f (ok %d, viewport %dx%d, box %s)"), Q[0], Q[1], Q[2], Q[3], Q[4], Q[5], Q[6], Q[7], bOk ? 1 : 0, VW, VH, *Bx.ToString()); } }
		if (bOk && bMoved)
		{
			FMemory::Memcpy(PhQuad, Q, sizeof(Q));
			Browser->ExecuteJavascript(FString::Printf(TEXT("window.__uePhoneQuad && window.__uePhoneQuad([%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f])"), Q[0], Q[1], Q[2], Q[3], Q[4], Q[5], Q[6], Q[7]));
		}
	}
	// the thumb's press on the side button eases back
	PhThumb = FMath::Max(0.f, PhThumb - Dt / 0.18f);
}

void AADPHUD::PhoneHand(ABabylonModel* P, float HandReach)
{
	// your right arm and hand on the phone (his photo): the wrist under its lower right corner and behind it, the fingers
	// across its back towards its upper left, the thumb's tip on the side button (pressed: a few mm in)
	AADPWalker* Wk = Cast<AADPWalker>(GetOwningPawn());
	if (!Wk) return;
	if (!P || HandReach <= 0.001f) { Wk->SetArm(0.f, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector); Wk->SetGrip(false); return; }
	const FTransform PT = P->GetActorTransform();
	// (on the desk the hand comes from above it; in your hand from behind it)
	// (the actor's +z is the phone's back - checked on the test picture: -z put the hand in front of the screen)
	const float Up = FMath::Lerp(-3.f, 3.f, FMath::Clamp((PhoneK - 0.35f) / 0.3f, 0.f, 1.f));
	// (his photo: the palm behind the phone's right half, the fingers running left across its back and wrapping only its
	// left edge - not over the screen; the thumb up the right edge)
	const FVector Wrist = PT.TransformPosition(FVector(6.0f, 6.5f, Up));
	const FVector Fingers = PT.TransformVectorNoScale(FVector(-0.92f, -0.38f, 0.f)).GetSafeNormal();
	const FVector Thumb = PT.TransformVectorNoScale(FVector(-0.15f, 1.f, 0.25f)).GetSafeNormal();   // (the rig reads the thumb mirrored: this turns the palm to face you from behind the phone)
	// the side button: its key on the right edge (Key1 - the middle one), the thumb's tip on its outside
	FVector Key = PT.TransformPosition(FVector(3.75f, -1.5f, 0.f));
	const int32 Ki = P->NodeIndex(TEXT("Key1"));
	if (Ki != INDEX_NONE) { const TPair<UInstancedStaticMeshComponent*, int32> In = P->NodeInstance(Ki); FTransform KT; if (In.Key && In.Key->GetInstanceTransform(In.Value, KT, true)) Key = KT.GetLocation(); }
	const FVector Out = PT.TransformVectorNoScale(FVector(1.f, 0.f, 0.f)).GetSafeNormal();
	Wk->SetArm(HandReach, Wrist, Fingers, Thumb);
	{ static double SaidAt = 0.0; const double Tn = FPlatformTime::Seconds(); if (Tn - SaidAt > 0.5) { SaidAt = Tn; ADP_LOG(TEXT("Phone: hand reach %.2f, wrist %.0f cm from your eyes"), HandReach, FVector::Dist(Wrist, Wk->GetPawnViewLocation())); } }
	Wk->SetGrip(PhoneK > 0.3f, Key + Out * (0.5f - 0.35f * PhThumb));
}

void AADPHUD::PhoneSide(int32 Dir)
{
	if (!bPhoneHeld || !Browser.IsValid()) return;
	PhThumb = 1.f;
	Browser->ExecuteJavascript(FString::Printf(TEXT("window.__uePhoneStep && window.__uePhoneStep(%d)"), Dir));
	// a small click from the phone's side (the side button only - not the "+", not the wheel)
	static USoundBase* Click = ADPKeep(LoadObject<USoundBase>(nullptr, TEXT("/Game/Player/Sounds/PhSideClick.PhSideClick")));
	if (Click && Phone.IsValid()) UGameplayStatics::PlaySoundAtLocation(this, Click, Phone->GetActorLocation(), 0.6f);
}

void AADPHUD::PhoneWheelUp() { if (bPhoneHeld && !bPhoneLook && Browser.IsValid()) Browser->ExecuteJavascript(TEXT("window.__uePhoneScroll && window.__uePhoneScroll(-140)")); }
void AADPHUD::PhoneWheelDown() { if (bPhoneHeld && !bPhoneLook && Browser.IsValid()) Browser->ExecuteJavascript(TEXT("window.__uePhoneScroll && window.__uePhoneScroll(140)")); }

void AADPHUD::PickPhoneNear(const FVector& At)
{
	ABabylonModel* Best = nullptr; double Bd = 1e12;
	for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
		if (It->Model == TEXT("Smartphone")) { const double D = FVector::Dist(It->GetActorLocation(), At); if (D < Bd) { Bd = D; Best = *It; } }
	if (!Best || bDevice || PhoneDir != 0) return;
	Phone = Best; PhoneHome = Best->GetActorTransform(); PhoneK = 0.f; PhoneDir = 1;
	UpdateMouse();
}

// ---- the jetway's HMI (to-do 115, owner 2026-11-18; docs/Jetway-HMI-Plan.md) ---------------------------------------
namespace
{
	const float HmiGlide = 0.7f;   // seconds, up to the screen (and back)
	const float HmiFov = 50.f;     // the close camera's view (degrees across)
	FVector2D HmiViewport() { return GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport ? FVector2D(GEngine->GameViewport->Viewport->GetSizeXY()) : FVector2D(1920, 1080); }
}

void AADPHUD::OpenHmi(int32 J)
{
	APlayerController* PC = GetOwningPlayerController(); APawn* Me = PC ? PC->GetPawn() : nullptr;
	AJetwayRigs* JR = AJetwayRigs::Get(GetWorld());
	FVector Mid, N, Up; FVector2D Sz;
	if (!PC || !Me || !JR || bDevice || HmiOpenIn >= 0.f || !JR->ScreenView(J, Mid, N, Up, Sz)) return;
	// how far in front of it: the screen filling about four fifths of the picture
	const FVector2D Vp = HmiViewport();
	const float Aspect = Vp.X / FMath::Max(1.f, (float)Vp.Y), TanH = FMath::Tan(FMath::DegreesToRadians(HmiFov * 0.5f));
	const float FitH = (Sz.Y * 0.5f) / (TanH / Aspect) / 0.97f, FitW = (Sz.X * 0.5f) / TanH / 0.97f;   // (as far in as it goes: the screen fills the view)
	float D = FMath::Clamp(FMath::Max(FitH, FitW), 12.f, 90.f);
	// never inside anything: a small ball swept out from the screen along its face; it stops short of what it meets
	FCollisionQueryParams Q(SCENE_QUERY_STAT(HmiCam), true); Q.AddIgnoredActor(Me);
	FHitResult H;
	if (GetWorld()->SweepSingleByChannel(H, Mid + N * 3.f, Mid + N * D, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(4.f), Q)) D = FMath::Max(H.Distance - 2.f, 12.f);
	const FVector Eye = Mid + N * D;
	const FRotator Rot = FRotationMatrix::MakeFromXZ(-N, Up).Rotator();
	ACameraActor* Cam = HmiCam.Get();
	if (!Cam) { Cam = GetWorld()->SpawnActor<ACameraActor>(Eye, Rot); HmiCam = Cam; }
	if (!Cam) return;
	Cam->SetActorLocationAndRotation(Eye, Rot);
	Cam->GetCameraComponent()->SetConstraintAspectRatio(false); Cam->GetCameraComponent()->SetFieldOfView(HmiFov);
	// (a screen read up close: the eye's brightness held where it was in the cab, so the bright screen filling the view
	//  doesn't wash out)
	{ FPostProcessSettings& P = Cam->GetCameraComponent()->PostProcessSettings; Cam->GetCameraComponent()->PostProcessBlendWeight = 1.f;
	  P.bOverride_AutoExposureBias = true; P.AutoExposureBias = -1.2f; P.bOverride_AutoExposureSpeedUp = true; P.AutoExposureSpeedUp = 20.f; P.bOverride_AutoExposureSpeedDown = true; P.AutoExposureSpeedDown = 20.f; }
	// the way there: straight from your eyes if nothing is between (a ball swept along it), else a short fade instead of
	// gliding through anything
	FVector From; FRotator FromR; PC->GetPlayerViewPoint(From, FromR);
	const bool bClear = !GetWorld()->SweepSingleByChannel(H, From, Eye, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(4.f), Q);
	HmiJ = J;
	HmiBackIn = -1.f;   // (opened again while still gliding back: that glide no longer gives the controls back)
	Me->DisableInput(PC);
	JR->HoldAt(J, Cam, Cast<ACharacter>(Me));   // (the camera and you ride with the screen while it drives - to-do 265)
	if (AJetwayHmi* L = AJetwayHmi::Get(GetWorld())) L->Hold(J);   // (the live HMI stays on this monitor)
	if (bClear) PC->SetViewTargetWithBlend(Cam, HmiGlide, VTBlend_EaseInOut, 2.f);
	else { if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.35f, FLinearColor::Black); PC->SetViewTarget(Cam); }
	HmiOpenIn = bClear ? HmiGlide : 0.35f;
	ADP_LOG(TEXT("Jetway HMI: %s, the camera %.0f cm in front of its screen (%.0f x %.0f cm)%s"), *JR->Name(J), D, Sz.X, Sz.Y, bClear ? TEXT("") : TEXT(", faded (something between)"));
}

void AADPHUD::TickHmi(float Dt)
{
	APlayerController* PC = GetOwningPlayerController();
	// the live HMI on the jetways' monitors (JetwayHmi.h): there in airport mode, once the jetways are
	{ static float Check = 0.f; if (!bGame && (Check -= Dt) <= 0.f) { Check = 2.f; if (AJetwayRigs* JRm = AJetwayRigs::Get(GetWorld())) if (JRm->Num() > 0) AJetwayHmi::Get(GetWorld()); } }
	// arrived: the live HMI on the monitor takes the mouse - you use it on the screen itself
	if (HmiOpenIn >= 0.f && (HmiOpenIn -= Dt) < 0.f)
	{
		HmiOpenIn = -1.f;
		if (AJetwayHmi* L = AJetwayHmi::Get(GetWorld())) { L->Hold(HmiJ); L->SetPointer(true); }
		bHmiPointer = true; UpdateMouse();
	}
	// up close: the mouse and the pad's left stick move the "+" on the screen (clicks: Interact)
	if (bHmiPointer && PC)
		if (AJetwayHmi* L = AJetwayHmi::Get(GetWorld()))
		{
			// as fast as the PC's own pointer on the window (to-do 263), by the screen as large as it is there just now
			L->MoveCursorWindow(ADPGameCursor::Move(PC, Dt), PC);
		}
	// the live values for the page, four times a second
	if (bDevice && DeviceKind.StartsWith(TEXT("hmi")) && Browser.IsValid() && (HmiSend -= Dt) <= 0.f)
	{
		HmiSend = 0.25f;
		if (AJetwayRigs* JR = AJetwayRigs::Get(GetWorld())) Browser->ExecuteJavascript(FString::Printf(TEXT("window.__ueHmi && window.__ueHmi(%s)"), *JR->HmiState(HmiJ)));
	}
	// back with you: your controls again once the camera is home
	if (HmiBackIn >= 0.f && (HmiBackIn -= Dt) < 0.f)
	{
		HmiBackIn = -1.f;
		if (AJetwayRigs* JR = AJetwayRigs::Get(GetWorld())) JR->LetGo();   // (walking again, where you stand in it now)
		if (APawn* Me = PC ? PC->GetPawn() : nullptr) Me->EnableInput(PC);
	}
}

void AADPHUD::EndHmi()
{
	APlayerController* PC = GetOwningPlayerController();
	HmiOpenIn = -1.f;
	if (AJetwayHmi* L = AJetwayHmi::Get(GetWorld())) { L->SetPointer(false); L->Hold(INDEX_NONE); }
	const bool bWasPointer = bHmiPointer; bHmiPointer = false;
	if (PC && PC->GetPawn())
	{
		FVector Eye; FRotator R; PC->GetPawn()->GetActorEyesViewPoint(Eye, R);
		ACameraActor* Cam = HmiCam.Get();
		FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(HmiBack), true); Q.AddIgnoredActor(PC->GetPawn());
		const bool bClear = Cam && !GetWorld()->SweepSingleByChannel(H, Cam->GetActorLocation(), Eye, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(4.f), Q);
		if (bClear) PC->SetViewTargetWithBlend(PC->GetPawn(), HmiGlide * 0.8f, VTBlend_EaseInOut, 2.f);
		else { if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.35f, FLinearColor::Black); PC->SetViewTarget(PC->GetPawn()); }
		HmiBackIn = bClear ? HmiGlide * 0.8f : 0.05f;
	}
	ADP_LOG(TEXT("Jetway HMI: closed"));
	HmiJ = -1;
	if (bWasPointer) UpdateMouse();
}

void AADPHUD::HmiCommand(const FString& Action, const FString& Arg)
{
	AJetwayRigs* JR = AJetwayRigs::Get(GetWorld());
	if (!JR || HmiJ == INDEX_NONE) return;
	ADP_LOG(TEXT("Jetway HMI: %s %s on %s"), *Action, *Arg, *JR->Name(HmiJ));
	AJetwayRigs::LastNote.Reset();
	if (Action == TEXT("DOCK")) JR->Dock(HmiJ);
	else if (Action == TEXT("UNDOCK")) JR->Undock(HmiJ);
	// the page shows the answer when a move was refused (why)
	if (Browser.IsValid() && !AJetwayRigs::LastNote.IsEmpty())
	{
		const FString Note = AJetwayRigs::LastNote.Replace(TEXT("\\"), TEXT("/")).Replace(TEXT("'"), TEXT("`"));
		Browser->ExecuteJavascript(FString::Printf(TEXT("window.__ueHmiNote && window.__ueHmiNote('%s')"), *Note));
	}
}


// ==== your tablet (was Tablet.cpp) ====
// Your tablet, always with you (owner, 2026-11-18, to-do 117; Art/Tablet/make_tablet.py). Tab (the pad's D-pad Down)
// takes it up: it comes up from below into your view and you hold it, its screen showing your wallpaper; E brings it up
// close and its screen opens (the game's own tablet screen over the room, as the phone's does), the room behind softly
// blurred as in a portrait photo; Esc back to holding it; Tab again puts it away. Holding the phone blurs the room too.
// The Overview opens only from the tablet and the computers (ADPHUD::OverviewFrom), and its logo brings you back here.

namespace ADPTabletK
{
	const float TabLift = 0.55f;     // seconds, up into your hands (and down again)
	const float TabClose = 0.45f;    // seconds, from your hands up close (and back)
	float Ease(float K) { return K * K * (3.f - 2.f * K); }
}

void AADPHUD::ToggleTablet()
{
	if (bGame || bHidden || bSettings || bWardrobe || bWhoAreYou || HmiJ != INDEX_NONE || PhoneDir != 0) return;
	if (bDevice && DeviceKind != TEXT("tablet")) return;   // (the phone or a computer is up)
	if (Cast<AFireTruck>(GetOwningPawn()) || Cast<AServiceDrive>(GetOwningPawn())) return;        // (not while driving)
	if (TabWant < 0.5f)
	{
		if (!Tablet.IsValid())
		{
			FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABabylonModel* T = GetWorld()->SpawnActor<ABabylonModel>(FVector(0, 0, -100000.f), FRotator::ZeroRotator, Sp);
			if (!T || !T->Place(TEXT("Tablet"))) { if (T) T->Destroy(); ADP_WARN(TEXT("Tablet: the model isn't there (/Game/Babylon/Tablet, Data/babylon-tablet.json)")); return; }
			T->SetActorEnableCollision(false);
			Tablet = T; TabletScreen(TEXT(""));
		}
		Tablet->SetActorHiddenInGame(false);
		TabWant = 1.f; TabCloseWant = 0.f;
		TabHomeLeft = 0.f;   // (its home screen drawn for its 3D screen right away - TickTablet)
		Breadcrumb(TEXT("Holding your tablet"));
		ADP_LOG(TEXT("Tablet: taken up"));
	}
	else
	{
		if (bDevice && DeviceKind == TEXT("tablet")) { bTabAway = true; PutDeviceAway(); return; }   // (up close: first back to your hands, then away)
		TabWant = 0.f; TabCloseWant = 0.f;
		ADP_LOG(TEXT("Tablet: put away"));
	}
	UpdateMouse();
}

bool AADPHUD::TabletUse()
{
	// E while you hold it: up close, its screen opens
	if (!Tablet.IsValid() || TabWant < 0.5f || TabK < 0.99f || bDevice || TabCloseWant > 0.5f) return false;
	TabCloseWant = 1.f;
	ADP_LOG(TEXT("Tablet: up close"));
	return true;
}

void AADPHUD::TabletScreen(const FString& DataUrl)
{
	// the picture on the tablet's own screen while you hold it: your wallpaper (the default his drone photo; the tablet's
	// screen sends the chosen one, as a JPEG, when it changes)
	ABabylonModel* T = Tablet.Get(); if (!T) return;
	if (!TabScreenMat)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Tablet/M_DeviceScreen.M_DeviceScreen"));
		if (!Base) { ADP_WARN(TEXT("Tablet: no /Game/Tablet/M_DeviceScreen (Tools/make_device_screen.py)")); return; }
		TabScreenMat = UMaterialInstanceDynamic::Create(Base, this);
		TabScreenMat->SetScalarParameterValue(TEXT("Bright"), 0.85f);
		const int32 Sc = T->NodeIndex(TEXT("Screen"));
		const TPair<UInstancedStaticMeshComponent*, int32> In = Sc != INDEX_NONE ? T->NodeInstance(Sc) : TPair<UInstancedStaticMeshComponent*, int32>(nullptr, INDEX_NONE);
		if (In.Key) In.Key->SetMaterial(0, TabScreenMat);
	}
	if (DataUrl.IsEmpty()) return;
	FString B64 = DataUrl; int32 Comma; if (B64.FindChar(TEXT(','), Comma)) B64 = B64.RightChop(Comma + 1);
	TArray<uint8> Bytes; if (!FBase64::Decode(B64, Bytes) || Bytes.Num() < 100) return;
	if (UTexture2D* Pic = FImageUtils::ImportBufferAsTexture2D(Bytes)) { TabWallTex = Pic; TabScreenMat->SetTextureParameterValue(TEXT("Pic"), Pic); ADP_LOG(TEXT("Tablet: its wallpaper (%dx%d)"), Pic->GetSizeX(), Pic->GetSizeY()); }
}

void AADPHUD::TickTablet(float Dt)
{
	APlayerController* PC = GetOwningPlayerController(); APawn* Me = PC ? PC->GetPawn() : nullptr;
	// the portrait blur: the room behind softly out of focus while you hold the tablet or the phone, or use either
	{
		const bool bBlur = (Tablet.IsValid() && TabK > 0.05f) || PhoneDir != 0 || (bDevice && (DeviceKind == TEXT("phone") || DeviceKind == TEXT("tablet")));
		BlurK = FMath::FInterpConstantTo(BlurK, bBlur ? 1.f : 0.f, Dt, 2.5f);
		if (UCameraComponent* Cam = Me ? Me->FindComponentByClass<UCameraComponent>() : nullptr)
		{
			FPostProcessSettings& P = Cam->PostProcessSettings;
			const bool bOn = BlurK > 0.001f;
			if (bOn || P.bOverride_DepthOfFieldFstop)
			{
				P.bOverride_DepthOfFieldFocalDistance = bOn; P.bOverride_DepthOfFieldFstop = bOn; P.bOverride_DepthOfFieldMinFstop = bOn;
				P.DepthOfFieldFocalDistance = (PhoneDir != 0 || DeviceKind == TEXT("phone")) ? 26.f : FMath::Lerp(48.f, 30.f, ADPTabletK::Ease(TabCloseK));   // (the phone held closer)   // (focused on the device in your hands)
				P.DepthOfFieldFstop = FMath::Lerp(22.f, 1.2f, ADPTabletK::Ease(BlurK)); P.DepthOfFieldMinFstop = 1.f;
				if (bOn) Cam->PostProcessBlendWeight = 1.f;
			}
		}
	}
	ABabylonModel* T = Tablet.Get();
	if (!T || !PC) return;
	// its 3D screen shows its whole home screen (or its lock screen), drawn by the game's own tablet screen: when you take
	// it up and every 30 s while you hold it (the clock, the mail) - owner, 2026-11-18
	if (TabWant > 0.5f && !bDevice && (TabHomeLeft -= Dt) <= 0.f)
	{
		TabHomeLeft = 30.f;
		if (Browser.IsValid()) Browser->ExecuteJavascript(TEXT("window.__ueTabletHome ? window.__ueTabletHome() : (window.ue && ue.adp && ue.adp.log && ue.adp.log('Tablet screen: the page has no __ueTabletHome'))"));
	}
	TabK = FMath::Clamp(TabK + (TabWant > 0.5f ? 1.f : -1.f) * Dt / ADPTabletK::TabLift, 0.f, 1.f);
	if (!bDevice) TabCloseK = FMath::Clamp(TabCloseK + (TabCloseWant > 0.5f ? 1.f : -1.f) * Dt / ADPTabletK::TabClose, 0.f, 1.f);
	if (TabK <= 0.f && TabWant < 0.5f) { T->SetActorHiddenInGame(true); return; }
	FVector Eye; FRotator Look; PC->GetPlayerViewPoint(Eye, Look);
	const FRotationMatrix CamM(Look);
	const FVector F = CamM.GetUnitAxis(EAxis::X), U = CamM.GetUnitAxis(EAxis::Z);
	// where it is: below your view (away), held in front and a little low, tilted to your eyes (held), or up close
	const FVector Away = Eye + F * 30.f - U * 62.f, Held = Eye + F * 46.f - U * 16.f, Close = Eye + F * 29.f - U * 0.5f;
	FVector At = FMath::Lerp(Away, Held, ADPTabletK::Ease(TabK)); At = FMath::Lerp(At, Close, ADPTabletK::Ease(TabCloseK));
	const FVector N = (Eye - At).GetSafeNormal();                                  // (its screen to your eyes)
	const FVector Top = (U - N * FVector::DotProduct(U, N)).GetSafeNormal();        // (its top edge up)
	// the model: its top is its -y, its screen its +z (Art/Tablet/make_tablet.py, as placed - as the phone's)
	T->SetActorTransform(FTransform(FRotationMatrix::MakeFromYZ(-Top, N).ToQuat(), At));
	// up close: its screen opens (the model steps aside behind it)
	if (TabCloseWant > 0.5f && TabCloseK >= 1.f && !bDevice) { T->SetActorHiddenInGame(true); OpenDevice(TEXT("tablet")); }
}

void AADPHUD::TabletScreenClosed()
{
	// its screen put away (Esc, its own button): back in your hands - or away, if Tab asked for that
	if (Tablet.IsValid()) Tablet->SetActorHiddenInGame(false);
	TabCloseWant = 0.f; TabHomeLeft = 0.f;   // (back in your hands: its screen drawn again as you left it)
	if (bTabAway) { bTabAway = false; TabWant = 0.f; }
}

// ---- the Overview: only from your tablet and the computers (owner, 2026-11-18) ----------------------------------------
void AADPHUD::OverviewFrom(const FString& Kind)
{
	if (Kind == TEXT("phone")) return;   // (never from the phone)
	OverviewReturn = Kind;
	ADP_LOG(TEXT("Overview: opened from the %s"), *Kind);
	if (bDevice) DeviceDone(DeviceKind);
	if (Kind == TEXT("tablet")) { TabCloseWant = 0.f; TabCloseK = 0.f; TabWant = 0.f; TabK = 0.f; if (Tablet.IsValid()) Tablet->SetActorHiddenInGame(true); }
	if (!bGame) ToggleGame();
	if (Browser.IsValid()) Browser->ExecuteJavascript(FString::Printf(TEXT("window.__ueOverview && window.__ueOverview('%s')"), *Kind));
}

void AADPHUD::OverviewBack()
{
	// the Overview's logo: back to the tablet or the computer you came from (not to the title screen)
	const FString Kind = OverviewReturn; OverviewReturn.Reset();
	ADP_LOG(TEXT("Overview: back to the %s"), Kind.IsEmpty() ? TEXT("3D world") : *Kind);
	if (bGame) ToggleGame();
	if (Kind == TEXT("tablet"))
	{
		if (!Tablet.IsValid()) { ToggleTablet(); }
		else { Tablet->SetActorHiddenInGame(false); TabWant = 1.f; }
		TabK = 1.f; TabCloseWant = 1.f;   // (straight up close again, its screen opening)
	}
	else if (!Kind.IsEmpty()) OpenDevice(Kind);
}

void AADPHUD::MenusKey()
{
	// G / the pad's Y: the Overview no longer opens from the 3D world (owner, 2026-11-18) - only from your tablet and the
	// computers; from the menus the key still brings you back into the 3D world
	if (bGame) { OverviewReturn.Reset(); ToggleGame(); return; }
	// following a flight in 3D: back out to the Overview it came from (owner, 2026-11-18)
	if (bViewer) { ToggleGame(); return; }
	Hint(TEXT("The Overview opens from your tablet (Tab) or a computer"));
}

void AADPHUD::Hint(const FString& Text) { HintText = Text; HintLeft = 3.f; }

void AADPHUD::Back3D() { if (bGame) ToggleGame(); }

// ==== your place (was Pose.cpp) ====
// Where you are (owner, 2026-11-18, to-do 117/119): a New Game starts you 35 m outside the main hall, looking straight at
// its centre entrance door; Continue / Load put you back exactly where you were, facing the same way. Your place is
// written every few seconds while you walk the 3D world and when the game closes (Saved/ADP/pose.txt). Sitting, beds,
// cars, public transport and the world moving on while you were away come later (to-do 119).

namespace ADPPoseK
{
	FString PoseFile() { return FPaths::ProjectSavedDir() / TEXT("ADP") / TEXT("pose.txt"); }
}

void AADPHUD::GoNewGameStart()
{
	// 35 m out from the centre entrance's outer doors (plan x 75, z -4), on its axis, looking straight at them
	GoTo(FVector(75.f, 0.f, -39.f), FVector(75.f, 1.7f, -4.f));
}

void AADPHUD::SavePose()
{
	APlayerController* PC = GetOwningPlayerController(); APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P || bGame || bInLounge || Cast<AFireTruck>(P) || Cast<AServiceDrive>(P) || !bPoseLive) return;
	const FVector L = P->GetActorLocation(); const FRotator R = PC->GetControlRotation();
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("%.1f %.1f %.1f %.2f %.2f"), L.X, L.Y, L.Z, R.Pitch, R.Yaw), *ADPPoseK::PoseFile());
}

bool AADPHUD::RestorePose()
{
	FString S; if (!FFileHelper::LoadFileToString(S, *ADPPoseK::PoseFile())) return false;
	TArray<FString> V; S.TrimStartAndEnd().ParseIntoArrayWS(V); if (V.Num() < 5) return false;
	APlayerController* PC = GetOwningPlayerController(); APawn* P = PC ? PC->GetPawn() : nullptr; if (!P) return false;
	if (AServiceDrive* Sd = Cast<AServiceDrive>(P)) { Sd->ExitNow(); P = PC->GetPawn(); if (!P) return false; }
	if (AFireTruck* T = Cast<AFireTruck>(P)) { T->ExitNow(); P = PC->GetPawn(); if (!P) return false; }
	if (bGame) ToggleGame();
	bInLounge = false;
	const FVector L(FCString::Atof(*V[0]), FCString::Atof(*V[1]), FCString::Atof(*V[2]));
	const FRotator R(FCString::Atof(*V[3]), FCString::Atof(*V[4]), 0.f);
	P->TeleportTo(L + FVector(0, 0, 5.f), FRotator(0, R.Yaw, 0));
	PC->SetControlRotation(R);
	ADP_LOG(TEXT("Pose: back where you were (%s)"), *L.ToString());
	return true;
}

void AADPHUD::OpenSiteStart()
{
	// New Game: the start spot (and the old place forgotten); Continue / Load: where you were, if it is known
	const bool bNew = bNewCareer; bNewCareer = false;
	if (bNew) { IFileManager::Get().Delete(*ADPPoseK::PoseFile(), false, true, true); GoNewGameStart(); ADP_LOG(TEXT("Pose: a new game - outside the main hall")); }
	else if (!RestorePose()) GoNewGameStart();
	bPoseLive = true; PoseLeft = 5.f;
}

void AADPHUD::DevicesDown()
{
	// owner, 2026-11-19: Esc (and the pad's Circle) always puts the device down again - from up close straight away, not
	// back to holding it first
	if (bDevice && DeviceKind == TEXT("tablet")) { bTabAway = true; PutDeviceAway(); }
	else if (bDevice && DeviceKind == TEXT("phone")) PutDeviceAway();
	else if (PhoneDir > 0) PhoneDir = -1;   // (on its way up: back down)
	else if (Tablet.IsValid() && TabWant > 0.5f) { TabCloseWant = 0.f; ToggleTablet(); }
	ADP_LOG(TEXT("Devices: put down (Esc / Circle)"));
}

bool AADPHUD::HoldingDevice() const
{
	return (Tablet.IsValid() && (TabWant > 0.5f || TabK > 0.01f)) || PhoneDir != 0 || (bDevice && (DeviceKind == TEXT("phone") || DeviceKind == TEXT("tablet")));
}
