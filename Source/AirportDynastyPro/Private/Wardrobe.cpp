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
		if (!N.StartsWith(TEXT("BP_MH_9")) || !N.Contains(TEXT("_player"))) continue;
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
	FVector Face = LookFrom - At; Face.Z = 0.f;
	const float Yaw = Face.IsNearlyZero() ? T.Rotator().Yaw : Face.Rotation().Yaw;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Preview = GetWorld()->SpawnActor<AActor>(C, At, FRotator(0.f, Yaw - 90.f, 0.f), P);
	GConfig->SetString(TEXT("ADP.Player"), TEXT("Outfit"), *Who, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	if (AADPWalker* Me = Cast<AADPWalker>(UGameplayStatics::GetPlayerPawn(this, 0))) Me->WearMetaHuman(Who);
	ADP_LOG(TEXT("Wardrobe: you wear %s"), *Who);
}

void AWardrobe::Undress()
{
	if (Preview) { Preview->Destroy(); Preview = nullptr; }
}
