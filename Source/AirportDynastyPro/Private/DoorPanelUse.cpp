// Using a garage-door button panel (DoorPanelUse.h).
#include "DoorPanelUse.h"
#include "ADPLog.h"
#include "ADPWalker.h"
#include "BabylonModel.h"
#include "FireTruck.h"
#include "Hangar.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

TWeakObjectPtr<ADoorPanelUse> ADoorPanelUse::One;

namespace
{
	bool PartAt(ABabylonModel* P, const TCHAR* Name, UInstancedStaticMeshComponent*& K, int32& I, FTransform& T)
	{
		if (!P) return false;
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> F;
		P->InstancesNamed(Name, false, F);
		if (!F.Num() || !F[0].Get<0>()) return false;
		K = F[0].Get<0>(); I = F[0].Get<1>();
		return K->GetInstanceTransform(I, T, true);
	}
}

ADoorPanelUse::ADoorPanelUse()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

bool ADoorPanelUse::IsActive() { return One.IsValid() && One->State != EState::None; }

ABabylonModel* ADoorPanelUse::PanelFor(APawn* Who, AActor* Aimed)
{
	ABabylonModel* Best = nullptr;
	if (ABabylonModel* M = Cast<ABabylonModel>(Aimed)) if (M->Model == TEXT("DoorPanel") && FVector::Dist2D(M->GetActorLocation(), Who->GetActorLocation()) < Reach + 60.f) return M;
	// the one right by you that you face
	float BestD = Reach;
	const FVector Look = Who->GetControlRotation().Vector().GetSafeNormal2D();
	for (TActorIterator<ABabylonModel> It(Who->GetWorld()); It; ++It)
	{
		if (It->Model != TEXT("DoorPanel")) continue;
		const FVector To = It->GetActorLocation() - Who->GetActorLocation();
		const float D = To.Size2D();
		if (D < BestD && FVector::DotProduct(To.GetSafeNormal2D(), Look) > 0.5f && FMath::Abs(To.Z) < 200.f) { BestD = D; Best = *It; }
	}
	return Best;
}

bool ADoorPanelUse::NearPanel(const FVector& At)
{
	if (!One.IsValid()) return false;
	for (TActorIterator<ABabylonModel> It(One->GetWorld()); It; ++It) if (It->Model == TEXT("DoorPanel") && FVector::Dist2D(It->GetActorLocation(), At) < Reach) return true;
	return false;
}

bool ADoorPanelUse::Interact(APawn* Who, AActor* Aimed)
{
	if (!One.IsValid() || !Who) return false;
	ADoorPanelUse* S = One.Get();
	if (S->State == EState::Use) { S->Press(); return true; }
	if (S->State != EState::None) return true;   // (on its way in or out, or a press under way)
	AADPWalker* Me = Cast<AADPWalker>(Who);
	ABabylonModel* P = Me ? PanelFor(Who, Aimed) : nullptr;
	if (!P) return false;
	S->Start(Me, P);
	return true;
}

bool ADoorPanelUse::Leave()
{
	if (!One.IsValid()) return false;
	ADoorPanelUse* S = One.Get();
	if (S->State == EState::None || S->State == EState::Out) return S->State == EState::Out;
	if (S->State == EState::Pressing) return true;   // (a press finishes first; Back again after)
	S->State = EState::Out; S->T = 0.f;
	if (AADPWalker* Me = S->User.Get()) if (APlayerController* PC = Cast<APlayerController>(Me->GetController())) PC->SetViewTargetWithBlend(Me, 0.45f, VTBlend_EaseInOut, 2.f);
	return true;
}

FVector ADoorPanelUse::Face() const
{
	// straight out of its face: the side its buttons stand out on
	ABabylonModel* P = Panel.Get();
	if (!P) return FVector::ForwardVector;
	UInstancedStaticMeshComponent* K; int32 I; FTransform Tb, Tp;
	if (PartAt(P, TEXT("BtnOpen"), K, I, Tb) && PartAt(P, TEXT("DoorPanelPlate"), K, I, Tp))
		return P->GetActorRightVector() * (FVector::DotProduct(Tb.GetLocation() - Tp.GetLocation(), P->GetActorRightVector()) >= 0.f ? 1.f : -1.f);
	return P->GetActorRightVector();
}

FVector ADoorPanelUse::ButtonAt(bool bG) const
{
	UInstancedStaticMeshComponent* K; int32 I; FTransform Tr;
	if (!PartAt(Panel.Get(), bG ? TEXT("BtnOpenArrow") : TEXT("BtnCloseArrow"), K, I, Tr) || !K->GetStaticMesh()) return Panel.IsValid() ? Panel->GetActorLocation() : FVector::ZeroVector;
	return Tr.TransformPosition(K->GetStaticMesh()->GetBoundingBox().GetCenter());
}

void ADoorPanelUse::PushButton(bool bG, float Depth)
{
	// the cap and its arrow pushed in along the face (they come back out as the finger lifts)
	static TMap<TPair<UInstancedStaticMeshComponent*, int32>, FTransform> Home;
	for (const TCHAR* N : {bG ? TEXT("BtnOpen") : TEXT("BtnClose"), bG ? TEXT("BtnOpenArrow") : TEXT("BtnCloseArrow")})
	{
		UInstancedStaticMeshComponent* K; int32 I; FTransform Tr;
		if (!PartAt(Panel.Get(), N, K, I, Tr)) continue;
		const TPair<UInstancedStaticMeshComponent*, int32> Key(K, I);
		if (!Home.Contains(Key)) Home.Add(Key, Tr);
		FTransform At = Home[Key]; At.AddToTranslation(-Face() * Depth);
		K->UpdateInstanceTransform(I, At, true, true, true);
	}
}

void ADoorPanelUse::Start(AADPWalker* Me, ABabylonModel* P)
{
	User = Me; Panel = P; State = EState::In; T = 0.f; bGreen = true; Aim = 1.f; bCommanded = false; MouseY = 0.f;
	Me->SetLed(true);
	// (owner, 2026-11-14: pressing is done through your own eyes - from behind you're switched in, and back out after)
	bWasThird = Me->IsThirdPerson();
	if (bWasThird) Me->SetThirdPerson(false);
	// you step up to it: half a metre out from its face, facing it
	const FVector N = Face();
	StandFrom = Me->GetActorLocation();
	StandAt = FVector(P->GetActorLocation().X, P->GetActorLocation().Y, StandFrom.Z) + N * 52.f;
	LookYaw = (-N).Rotation().Yaw;
	WristAt = ButtonAt(true) + N * 12.f;
	if (!Cam) { FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; Cam = GetWorld()->SpawnActor<ACameraActor>(Sp); if (Cam) { Cam->GetCameraComponent()->SetFieldOfView(62.f); Cam->GetCameraComponent()->bConstrainAspectRatio = false; } }
	PlaceCam(0.f, true);
	if (APlayerController* PC = Cast<APlayerController>(Me->GetController())) if (Cam) PC->SetViewTargetWithBlend(Cam, 0.45f, VTBlend_EaseInOut, 2.f);
	ADP_LOG(TEXT("Door panel: in use (%s)"), *P->GetName());
}

void ADoorPanelUse::Press()
{
	if (State != EState::Use) return;
	State = EState::Pressing; PressT = 0.f; bPressGreen = bGreen; bCommanded = false;
}

void ADoorPanelUse::Command(bool bOpen)
{
	ABabylonModel* P = Panel.Get();
	if (!P) return;
	ABabylonModel::LightDoorPanel(P, bOpen);
	// only the door beside this panel: a hangar's own door, or the fire station's bay door nearest it
	if (AHangar* H = Cast<AHangar>(P->GetOwner())) H->Command(bOpen);
	else AFireStationDoors::Command(P->GetActorLocation(), bOpen);
	ADP_LOG(TEXT("Door panel: %s pressed"), bOpen ? TEXT("green (open)") : TEXT("red (close)"));
}

void ADoorPanelUse::ChosenParts(TArray<TPair<UInstancedStaticMeshComponent*, int32>>& Out)
{
	ADoorPanelUse* S = One.Get();
	if (!S || S->State == EState::None || S->State == EState::Out) return;
	const bool bG = S->State == EState::Pressing ? S->bPressGreen : S->bGreen;
	for (const TCHAR* N : {bG ? TEXT("BtnOpen") : TEXT("BtnClose"), bG ? TEXT("BtnOpenBezel") : TEXT("BtnCloseBezel")})
	{
		UInstancedStaticMeshComponent* K; int32 I; FTransform Tr;
		if (PartAt(S->Panel.Get(), N, K, I, Tr)) Out.Add(TPair<UInstancedStaticMeshComponent*, int32>(K, I));
	}
}

void ADoorPanelUse::PlaceCam(float Dt, bool bSnap)
{
	if (!Cam || !Panel.IsValid()) return;
	// half a metre out from the panel, off to its left (your right hand comes in from the right) and a little up: the
	// buttons, your fingertip on one, your hand and some of your forearm across the picture
	const FVector N = Face();
	const FVector Ahead = -N, Right = FRotator(0.f, Ahead.Rotation().Yaw + 90.f, 0.f).Vector();
	const FVector Mid = (ButtonAt(true) + ButtonAt(false)) * 0.5f;
	// (owner, 2026-11-14: to the right of your body - clear of it - a little up and over your arm, tilted down along it to
	// the panel: you stand half a metre out, your right shoulder some 20 cm right of the panel's middle)
	const FVector At = Mid + N * 42.f + Right * 50.f + FVector(0, 0, 46.f);   // (high enough to look down over the arm, whichever button)
	const FVector LookAt = Mid + N * 3.f + Right * 2.f - FVector(0, 0, 4.f);
	const FRotator R = (LookAt - At).Rotation();
	if (bSnap) Cam->SetActorLocationAndRotation(At, R);
	else Cam->SetActorLocationAndRotation(FMath::VInterpTo(Cam->GetActorLocation(), At, Dt, 8.f), FMath::RInterpTo(Cam->GetActorRotation(), R, Dt, 8.f));
}

void ADoorPanelUse::TestUse(APawn* Who, ABabylonModel* P) { if (One.IsValid() && Cast<AADPWalker>(Who) && P) One->Start(Cast<AADPWalker>(Who), P); }
void ADoorPanelUse::TestPick(bool bG) { if (One.IsValid() && One->State == EState::Use) One->bGreen = bG; }
void ADoorPanelUse::TestPress() { if (One.IsValid()) One->Press(); }

void ADoorPanelUse::Tick(float Dt)
{
	Super::Tick(Dt);
	One = this;
	if (State == EState::None) return;
	AADPWalker* Me = User.Get();
	ABabylonModel* P = Panel.Get();
	if (!Me || !P) { State = EState::None; if (Me) if (APlayerController* PCx = Cast<APlayerController>(Me->GetController())) PCx->SetViewTarget(Me); if (Me) { Me->SetArm(0.f, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector); Me->SetPoint(false); Me->SetLed(false); } return; }
	T += Dt;
	APlayerController* PC = Cast<APlayerController>(Me->GetController());
	const FVector N = Face();

	// choosing a button: mouse up / down, the wheel, W / S, the arrows, the D-pad, the left stick
	if (PC && State == EState::Use)
	{
		float Mx = 0.f, My = 0.f; PC->GetInputMouseDelta(Mx, My);
		MouseY = FMath::Clamp(MouseY + My, -6.f, 6.f);
		bool bUp = PC->WasInputKeyJustPressed(EKeys::MouseScrollUp) || PC->WasInputKeyJustPressed(EKeys::W) || PC->WasInputKeyJustPressed(EKeys::Up) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up) || MouseY > 4.f;
		bool bDown = PC->WasInputKeyJustPressed(EKeys::MouseScrollDown) || PC->WasInputKeyJustPressed(EKeys::S) || PC->WasInputKeyJustPressed(EKeys::Down) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down) || MouseY < -4.f;
		const float Stick = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
		if (Stick > 0.6f && StickWas <= 0.6f) bUp = true;
		if (Stick < -0.6f && StickWas >= -0.6f) bDown = true;
		StickWas = Stick;
		if (bUp) { bGreen = true; MouseY = 0.f; }
		if (bDown) { bGreen = false; MouseY = 0.f; }
		// (pressing: E or the left mouse button come through Interact - owner, 2026-11-14)
		if (PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right) || PC->WasInputKeyJustPressed(EKeys::Gamepad_Special_Left)) Leave();
	}

	// stepping up to it, your view settling on it
	const float KIn = State == EState::In ? FMath::SmoothStep(0.f, 0.45f, T) : 1.f;
	if (State == EState::In || State == EState::Use || State == EState::Pressing)
	{
		Me->SetActorLocation(FMath::Lerp(StandFrom, StandAt, KIn), false, nullptr, ETeleportType::TeleportPhysics);
		if (PC)
		{
			const FVector Eye = Me->GetPawnViewLocation();
			const FRotator Want = (ButtonAt(bGreen) - Eye).Rotation();
			PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Want, Dt, 6.f));
		}
	}

	// the arm: reaching out (0.6 s), then the fingertip on the chosen button, a little off it; pressing pushes it in
	float ArmW = 1.f, Off = 0.25f;   // (owner, 2026-11-14: the fingertip really on the button)
	Aim = FMath::FInterpTo(Aim, bGreen ? 1.f : 0.f, Dt, 10.f);
	if (State == EState::In) { ArmW = FMath::SmoothStep(0.f, 0.6f, T); if (T >= 0.65f) { State = EState::Use; T = 0.f; } }
	else if (State == EState::Pressing)
	{
		PressT += Dt;
		// in (0.18 s), held a moment, back out (0.18 s); the door told as the button bottoms out
		const float In = PressT < 0.18f ? PressT / 0.18f : PressT < 0.3f ? 1.f : FMath::Max(0.f, 1.f - (PressT - 0.3f) / 0.18f);
		Off = FMath::Lerp(0.25f, -0.45f, FMath::SmoothStep(0.f, 1.f, In));
		PushButton(bPressGreen, FMath::Max(0.f, -Off + 0.05f) + (In > 0.9f ? 0.35f : 0.f));
		if (!bCommanded && PressT >= 0.18f) { bCommanded = true; Command(bPressGreen); }
		if (PressT >= 0.5f) { PushButton(bPressGreen, 0.f); State = EState::Use; }
	}
	else if (State == EState::Out)
	{
		ArmW = 1.f - FMath::SmoothStep(0.f, 0.55f, T);
		if (T >= 0.6f)
		{
			Me->SetArm(0.f, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector); Me->SetPoint(false); Me->SetLed(false);
			if (bWasThird) { Me->SetThirdPerson(true); bWasThird = false; }
			State = EState::None;
			ADP_LOG(TEXT("Door panel: done, arm down"));
			return;
		}
	}
	// where the fingertip should be, and the wrist moved until it's there (the fingertip measured on your own hand)
	const FVector Btn = FMath::Lerp(ButtonAt(false), ButtonAt(true), Aim);
	const FVector TipWant = Btn + N * Off;
	FVector Tip;
	if (Me->FingerTipNow(Tip)) WristAt += (TipWant - Tip) * FMath::Min(1.f, Dt * 20.f);
	else WristAt = TipWant + N * 14.f;
	PlaceCam(Dt, false);
	// the hand in line with the forearm, from your shoulder to the button (the arm rises, lowers and reaches as it needs -
	// never a bent-back wrist), turned a little more towards the wall; the thumb up
	const FVector Sh = Me->ShoulderNow();
	const FVector Line = (TipWant - Sh).GetSafeNormal();
	const FVector Fingers = (Line * 0.7f - N * 0.3f).GetSafeNormal();
	Me->SetPoint(true);
	Me->SetArm(ArmW, WristAt, Fingers, FVector::UpVector);
}
