// The unstuck panel (owner, 2026-11-09): when you are stuck inside something (keys held but you don't move, or your
// body inside a wall), a small panel in the middle of the screen; one click puts you on free floor nearby, the panel
// goes and the view is yours again at once.
#include "UI/ADPHUD.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"

TSharedRef<SWidget> AADPHUD::MakeUnstuckPanel()
{
	const FLinearColor InkC(0.85f, 0.95f, 1.f);
	return SNew(SBorder).BorderBackgroundColor(FLinearColor(0.024f, 0.094f, 0.17f, 0.92f)).Padding(FMargin(18, 14))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
			[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12)).ColorAndOpacity(InkC).Text(FText::FromString(TEXT("You seem to be stuck.")))]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.1f, 0.45f, 0.75f, 1.f)).ContentPadding(FMargin(14, 7))
				.OnClicked_Lambda([this]() { Unstick(); return FReply::Handled(); })
				[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)).ColorAndOpacity(InkC).Text(FText::FromString(TEXT("PUT ME ON FREE FLOOR")))]
			]
		];
}

bool AADPHUD::IsStuck(float Dt)
{
	ACharacter* C = Cast<ACharacter>(GetOwningPawn());
	if (!C || !C->GetCharacterMovement() || bGame) { StuckFor = 0; return false; }
	// inside something solid, or keys held for a while without moving
	const bool bPushing = !C->GetLastMovementInputVector().IsNearlyZero() && C->GetVelocity().Size2D() < 5.f;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPStuck), false, C);
	const bool bOverlap = GetWorld()->OverlapBlockingTestByChannel(C->GetActorLocation(), FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(C->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.8f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.8f), Q);
	StuckFor = (bPushing || bOverlap) ? StuckFor + Dt : 0.f;
	return StuckFor > 2.0f;
}

void AADPHUD::Unstick()
{
	ACharacter* C = Cast<ACharacter>(GetOwningPawn());
	if (!C) return;
	const float R = C->GetCapsuleComponent()->GetScaledCapsuleRadius(), H = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPUnstick), false, C);
	const FVector From = C->GetActorLocation();
	// rings round you, nearest first: floor under the spot, and room for the body standing on it
	for (float Dist = 50.f; Dist <= 1200.f; Dist += 50.f)
		for (int32 k = 0; k < 16; ++k)
		{
			const float A = k * PI / 8.f;
			const FVector Spot = From + FVector(FMath::Cos(A) * Dist, FMath::Sin(A) * Dist, 0.f);
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0, 0, 200.f), Spot - FVector(0, 0, 400.f), ECC_Visibility, Q) || Hit.ImpactNormal.Z < 0.7f) continue;
			const FVector Stand = Hit.ImpactPoint + FVector(0, 0, H + 5.f);
			if (GetWorld()->OverlapBlockingTestByChannel(Stand, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(R, H), Q)) continue;
			C->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
			C->GetCharacterMovement()->StopMovementImmediately();
			C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			StuckFor = 0;
			if (UnstuckPanel.IsValid()) UnstuckPanel->SetVisibility(EVisibility::Collapsed);
			// the view yours again at once: no extra click
			if (APlayerController* PC = GetOwningPlayerController()) { PC->SetInputMode(FInputModeGameOnly()); PC->SetShowMouseCursor(false); }
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			UE_LOG(LogTemp, Log, TEXT("Unstuck: moved %.1f m to free floor"), Dist / 100.f);
			return;
		}
	UE_LOG(LogTemp, Warning, TEXT("Unstuck: no free floor within 12 m"));
}
