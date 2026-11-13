// The office wardrobe (Wardrobe.h).
#include "Wardrobe.h"
#include "ADPLog.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/ConfigCacheIni.h"
#include "ADPWalker.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	// measured on the imported meshes (2026-11-12): the cabinet's front is its -y face (y -30), the doors come in the
	// cabinet's own space (not about their hinges), hung at x +-58.2 on the front face
	const FVector HingeL(-58.2f, -29.1f, 0.f), HingeR(58.2f, -29.1f, 0.f);
	const float Swing = 95.f;
}

AWardrobe::AWardrobe()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

bool AWardrobe::Build()
{
	UStaticMesh* BodyM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Wardrobe/wardrobe/StaticMeshes/WardrobeBody.WardrobeBody"));
	UStaticMesh* LM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Wardrobe/wardrobe/StaticMeshes/WardrobeDoorL.WardrobeDoorL"));
	UStaticMesh* RM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Wardrobe/wardrobe/StaticMeshes/WardrobeDoorR.WardrobeDoorR"));
	if (!BodyM || !LM || !RM) { ADP_ERROR(TEXT("Wardrobe: the model is missing (/Game/Wardrobe, Tools/set_wardrobe.py)")); return false; }
	auto Part = [this](const TCHAR* Name, UStaticMesh* M, const FVector& At)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name);
		C->SetupAttachment(Root);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(M);
		C->SetRelativeLocation(At);
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->RegisterComponent();
		AddInstanceComponent(C);
		return C;
	};
	Body = Part(TEXT("Body"), BodyM, FVector::ZeroVector);
	DoorL = Part(TEXT("DoorL"), LM, FVector::ZeroVector);
	DoorR = Part(TEXT("DoorR"), RM, FVector::ZeroVector);
	return true;
}

void AWardrobe::SetOpen(bool bOpen)
{
	OpenTarget = bOpen ? 1.f : 0.f;
	if (!bOpen) Undress();
}

void AWardrobe::Tick(float Dt)
{
	Super::Tick(Dt);
	if (OpenK == OpenTarget) return;
	OpenK = FMath::FInterpConstantTo(OpenK, OpenTarget, Dt, 1.6f);
	// swung out quickly, easing to a stop; shut with a last swing
	const float A = OpenTarget > 0.f ? FMath::Sin(OpenK * HALF_PI) : OpenK * OpenK * (2.f - OpenK);
	// each door turned about its hinge, outward (the left one's free edge towards -y: a negative yaw; the right one's the other way)
	auto About = [](const FVector& H, float Yaw) { const FQuat Q(FVector::UpVector, FMath::DegreesToRadians(Yaw)); return FTransform(Q, H - Q.RotateVector(H)); };
	if (DoorL) DoorL->SetRelativeTransform(About(HingeL, -Swing * A));
	if (DoorR) DoorR->SetRelativeTransform(About(HingeR, Swing * A));
}

TArray<AWardrobe::FOutfit> AWardrobe::Outfits()
{
	// every built copy of your character: /Game/People/Built/MH_9xx_player[_<outfit>]/BP_<same>
	TArray<FOutfit> Out;
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Found;
	AR.GetAssetsByPath(FName(TEXT("/Game/People/Built")), Found, true);
	for (const FAssetData& A : Found)
	{
		const FString N = A.AssetName.ToString();
		if (!N.StartsWith(TEXT("BP_MH_9")) || !N.Contains(TEXT("_player")) || N.EndsWith(TEXT("_C"))) continue;   // (the _C is the blueprint's class, not another outfit)
		FOutfit O;
		O.Who = N.Mid(3);
		FString Tail;
		if (O.Who.Split(TEXT("_player_"), nullptr, &Tail)) { Tail.ReplaceInline(TEXT("_"), TEXT(" ")); O.Label = Tail; }
		else O.Label = TEXT("As built (the grey tee and shorts)");
		Out.Add(O);
	}
	if (Out.Num() == 0) Out.Add({TEXT("As built (the grey tee and shorts)"), TEXT("MH_900_player")});
	Out.Sort([](const FOutfit& A, const FOutfit& B) { return A.Who < B.Who; });
	const FString Now = Chosen();
	Out.StableSort([&](const FOutfit& A, const FOutfit& B) { return A.Who == Now && B.Who != Now; });
	return Out;
}

FString AWardrobe::Chosen()
{
	FString Who = TEXT("MH_900_player");
	GConfig->GetString(TEXT("ADP.Player"), TEXT("Outfit"), Who, GGameUserSettingsIni);
	FParse::Value(FCommandLine::Get(), TEXT("Player="), Who);   // (tests: play as another built character)
	return Who;
}

void AWardrobe::Dress(const FString& Who, const FVector& LookFrom)
{
	Undress();
	UClass* C = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Game/People/Built/%s/BP_%s.BP_%s_C"), *Who, *Who, *Who));
	if (!C) { ADP_WARN(TEXT("Wardrobe: %s isn't built"), *Who); return; }
	// beside the open doors, a step out in front (the front is the cabinet's -y), turned to face you (a MetaHuman
	// faces its own right: its yaw is the facing's less a quarter turn)
	const FTransform T = GetActorTransform();
	const FVector At = T.TransformPosition(FVector(135.f, -115.f, 0.f));
	// turned a little off the room's axis, the way the open-world changing rooms stand you: three-quarter to the camera
	FVector Face = LookFrom - At; Face.Z = 0.f;
	const float Yaw = (Face.IsNearlyZero() ? T.Rotator().Yaw : Face.Rotation().Yaw) + 20.f;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Preview = GetWorld()->SpawnActor<AActor>(C, At + FVector(0, 0, 2.f), FRotator(0.f, Yaw - 90.f, 0.f), P);   // (a touch up: the soles on the tiles, not in them)
	// standing naturally while you look (the mannequin's idle carried over to the MetaHuman body), the clothes following
	if (Preview)
	{
		TArray<USkeletalMeshComponent*> Skels; Preview->GetComponents<USkeletalMeshComponent>(Skels);
		USkeletalMeshComponent* Skel = nullptr;
		for (USkeletalMeshComponent* S : Skels) if (S->GetName().StartsWith(TEXT("Body"))) Skel = S;
		if (Skel)
		{
			static UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/People/Anims/MM_Idle_MH.MM_Idle_MH"));
			for (USkeletalMeshComponent* S : Skels) if (S != Skel && !S->GetName().StartsWith(TEXT("Face"))) S->SetLeaderPoseComponent(Skel);
			if (Idle) { Skel->SetAnimationMode(EAnimationMode::AnimationSingleNode); Skel->SetAnimation(Idle); Skel->Play(true); }
		}
	}
	if (Cam) PlaceCam(ViewNow);
	GConfig->SetString(TEXT("ADP.Player"), TEXT("Outfit"), *Who, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	if (AADPWalker* Me = Cast<AADPWalker>(UGameplayStatics::GetPlayerPawn(this, 0))) Me->WearMetaHuman(Who);
	ADP_LOG(TEXT("Wardrobe: you wear %s"), *Who);
}

void AWardrobe::Undress()
{
	if (Preview) { Preview->Destroy(); Preview = nullptr; }
}

const TArray<FString>& AWardrobe::Categories()
{
	static const TArray<FString> C = {TEXT("Outfits"), TEXT("Jackets"), TEXT("Shirts"), TEXT("Trousers"), TEXT("Shoes"), TEXT("Hats"), TEXT("Sunglasses"), TEXT("Ties")};
	return C;
}

AWardrobe::EView AWardrobe::ViewFor(const FString& Category)
{
	if (Category == TEXT("Hats") || Category == TEXT("Sunglasses")) return EView::Face;
	if (Category == TEXT("Shoes")) return EView::Feet;
	return EView::Body;
}

void AWardrobe::PlaceCam(EView View)
{
	if (!Cam || !Preview) return;
	// you on the right third of the screen: the camera stands before you and aims a little to your right (its left),
	// so the menu has the left of the screen; a long lens from further back for the whole body (the portrait look),
	// close for the face, looking down for the feet
	const FTransform T = Preview->GetActorTransform();
	const FVector Face = T.GetRotation().RotateVector(FVector(0.f, 1.f, 0.f));   // a MetaHuman faces its own +y
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Face).GetSafeNormal();
	FVector Target, Eye, Aim; float Fov, Fstop;
	switch (View)
	{
	case EView::Face: Target = T.GetLocation() + FVector(0, 0, 163.f); Eye = Target + Face * 230.f + FVector(0, 0, 3.f); Aim = Target + Right * 27.f; Fov = 30.f; Fstop = 4.0f; break;
	case EView::Feet: Target = T.GetLocation() + FVector(0, 0, 16.f); Eye = Target + Face * 250.f + FVector(0, 0, 112.f); Aim = Target + Right * 24.f + FVector(0, 0, -6.f); Fov = 32.f; Fstop = 2.8f; break;
	default: Target = T.GetLocation() + FVector(0, 0, 98.f); Eye = Target + Face * 520.f + FVector(0, 0, 10.f); Aim = Target + Right * 64.f; Fov = 40.f; Fstop = 1.2f; break;
	}
	// wherever the wardrobe stands: a wall or furniture between you and the camera pulls the camera in before it
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WardrobeCam), false);
		Q.AddIgnoredActor(Preview); Q.AddIgnoredActor(this); if (Cam) Q.AddIgnoredActor(Cam);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Target, Eye, ECC_Visibility, Q))
		{
			const FVector Dir = (Eye - Target).GetSafeNormal();
			const float Dist = FMath::Max(60.f, Hit.Distance - 25.f);
			Eye = Target + Dir * Dist;
			Aim = Target + (Aim - Target) * (Dist / FMath::Max(1.f, (Eye - Target).Size()));
		}
	}
	Cam->SetActorLocationAndRotation(Eye, (Aim - Eye).Rotation());
	UCameraComponent* C = Cam->GetCameraComponent();
	C->SetFieldOfView(Fov);
	C->SetConstraintAspectRatio(false);
	// the portrait look: a wide-open lens focused on you, so the room behind goes soft and you stay sharp
	const float Focus = (Target - Eye).Size() - (View == EView::Face ? 8.f : 0.f);   // (the face: on the eyes, not the back of the head)
	C->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true; C->PostProcessSettings.DepthOfFieldFocalDistance = Focus;
	C->PostProcessSettings.bOverride_DepthOfFieldFstop = true; C->PostProcessSettings.DepthOfFieldFstop = Fstop;
	C->PostProcessSettings.bOverride_DepthOfFieldMinFstop = true; C->PostProcessSettings.DepthOfFieldMinFstop = 1.2f;
	C->PostProcessSettings.bOverride_DepthOfFieldSensorWidth = true; C->PostProcessSettings.DepthOfFieldSensorWidth = 36.f;
	// (no blur by distance: it ramps up from the camera and softens you too; the lens alone keeps you sharp)
	C->PostProcessSettings.bOverride_DepthOfFieldDepthBlurRadius = true; C->PostProcessSettings.DepthOfFieldDepthBlurRadius = 0.f;
	C->PostProcessSettings.bOverride_DepthOfFieldBladeCount = true; C->PostProcessSettings.DepthOfFieldBladeCount = 9;
	C->PostProcessBlendWeight = 1.f;
	ViewNow = View;
}

void AWardrobe::Look(EView View, APlayerController* PC)
{
	if (!PC || !Preview) return;
	const bool bFirst = !Cam;
	if (!Cam)
	{
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Cam = GetWorld()->SpawnActor<ACameraActor>(PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : GetActorLocation(), PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation() : FRotator::ZeroRotator, P);
	}
	PlaceCam(View);
	// the glide: from where you looked the first time, a quick move between views after
	PC->SetViewTargetWithBlend(Cam, bFirst ? 0.9f : 0.5f, EViewTargetBlendFunction::VTBlend_EaseInOut, 2.f);
}

void AWardrobe::LookBack(APlayerController* PC)
{
	if (PC && PC->GetPawn()) PC->SetViewTargetWithBlend(PC->GetPawn(), 0.6f, EViewTargetBlendFunction::VTBlend_EaseInOut, 2.f);
	if (Cam) { Cam->Destroy(); Cam = nullptr; }
}
