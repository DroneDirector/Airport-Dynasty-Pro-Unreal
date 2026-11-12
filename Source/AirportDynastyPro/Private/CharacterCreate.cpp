#include "CharacterCreate.h"
#include "ADPLog.h"
#include "TerminalPlan.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/LocalFogVolume.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/SpotLight.h"
#include "Engine/RectLight.h"
#include "Engine/PointLight.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

namespace
{
	// the concept's pieces, for the choice moving between them
	TWeakObjectPtr<ASpotLight> KeyLight;
	TWeakObjectPtr<AActor> She, He;
	FVector HerAt, HimAt;

	AStaticMeshActor* Shape(UWorld* W, const TCHAR* Mesh, const FVector& At, const FRotator& Rot, const FVector& Scale, UMaterialInterface* M)
	{
		UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, Mesh);
		if (!SM) { ADP_WARN(TEXT("Character creator: no mesh %s"), Mesh); return nullptr; }
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(At, Rot, P);
		A->SetMobility(EComponentMobility::Movable);
		A->GetStaticMeshComponent()->SetStaticMesh(SM);
		A->SetActorScale3D(Scale);
		if (M) A->GetStaticMeshComponent()->SetMaterial(0, M);
		return A;
	}

	UMaterialInstanceDynamic* Plain(UObject* Outer, const FLinearColor& Col, float Rough)
	{
		static UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/LightMast/M_LM_Plain.M_LM_Plain"));
		if (!Base) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
		M->SetVectorParameterValue(TEXT("BaseColor"), Col);
		M->SetScalarParameterValue(TEXT("Roughness"), Rough);
		M->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		return M;
	}

	/** A built person standing at At, facing FacingYaw (a MetaHuman faces its own +y), in the mannequin's idle. */
	AActor* Person(UWorld* W, const FString& Who, const FVector& At, float FacingYaw)
	{
		UClass* C = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Game/People/Built/%s/BP_%s.BP_%s_C"), *Who, *Who, *Who));
		if (!C) { ADP_WARN(TEXT("Character creator: %s isn't built"), *Who); return nullptr; }
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = W->SpawnActor<AActor>(C, At, FRotator(0.f, FacingYaw - 90.f, 0.f), P);
		if (!A) return nullptr;
		TArray<USkeletalMeshComponent*> Skels; A->GetComponents<USkeletalMeshComponent>(Skels);
		USkeletalMeshComponent* Body = nullptr;
		for (USkeletalMeshComponent* S : Skels) if (S->GetName().StartsWith(TEXT("Body"))) Body = S;
		if (Body)
		{
			static UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/People/Anims/MM_Idle_MH.MM_Idle_MH"));
			for (USkeletalMeshComponent* S : Skels) if (S != Body && !S->GetName().StartsWith(TEXT("Face"))) S->SetLeaderPoseComponent(Body);
			if (Idle) { Body->SetAnimationMode(EAnimationMode::AnimationSingleNode); Body->SetAnimation(Idle); Body->Play(true); }
		}
		return A;
	}
}

void ADPCharacterCreate::BuildConcept(UWorld* W, APlayerController* PC, bool bWoman)
{
	if (!W) return;
	// the room: 40 m under the terminal (dark there), its floor at C; the camera looks along +y (world) at the two
	const FVector C = FTerminalPlan::ToWorld(0.f, -40.f, 0.f);
	const FVector X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UMaterialInstanceDynamic* Floor = Plain(W, FLinearColor(0.012f, 0.014f, 0.022f), 0.28f);   // a little shine: the neon mirrored in it
	UMaterialInstanceDynamic* Wall = Plain(W, FLinearColor(0.008f, 0.01f, 0.018f), 0.75f);
	const TCHAR* PlaneMesh = TEXT("/Engine/BasicShapes/Plane.Plane");
	Shape(W, PlaneMesh, C, FRotator::ZeroRotator, FVector(16, 16, 1), Floor);
	for (const float Flip : {0.f, 180.f})   // (a plane shows one face: each wall both ways round, so nothing of the world shows through)
	{
		Shape(W, PlaneMesh, C + Z * 480.f, FRotator(Flip, 0.f, 0.f), FVector(16, 16, 1), Wall);
		Shape(W, PlaneMesh, C + Y * 900.f + Z * 240.f, FRotator(0.f, 0.f, 90.f + Flip), FVector(16, 6, 1), Wall);    // the back wall, well behind the fog
		Shape(W, PlaneMesh, C - Y * 800.f + Z * 240.f, FRotator(0.f, 0.f, -90.f + Flip), FVector(16, 6, 1), Wall);   // behind the camera
		Shape(W, PlaneMesh, C - X * 800.f + Z * 240.f, FRotator(90.f + Flip, 0.f, 0.f), FVector(6, 18, 1), Wall);
		Shape(W, PlaneMesh, C + X * 800.f + Z * 240.f, FRotator(-90.f + Flip, 0.f, 0.f), FVector(6, 18, 1), Wall);
	}
	// the fog wall behind them (owner): a bank of fog behind the tubes that hides everything beyond and takes the
	// lights' colours (it is lit through the volumetric fog)
	{
		ALocalFogVolume* V = W->SpawnActor<ALocalFogVolume>(C + Y * 520.f + Z * 230.f, FRotator::ZeroRotator, P);
		if (V)
		{
			V->SetActorScale3D(FVector(9.f, 3.2f, 5.f));   // (the volume is a 1 m sphere at scale 1)
			if (ULocalFogVolumeComponent* F = V->FindComponentByClass<ULocalFogVolumeComponent>())
			{
				F->SetRadialFogExtinction(1.6f);
				F->SetHeightFogExtinction(0.f);
				F->SetFogPhaseG(0.3f);
			}
		}
		if (PC) PC->ConsoleCommand(TEXT("r.LocalFogVolume.ApplyVolumetricFog 1"));
	}

	// the two neon tubes, the title screen's cyan, and the light they throw (a tall soft light behind each, into the fog)
	static UMaterialInterface* NeonBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Neon/M_Neon.M_Neon"));
	UMaterialInstanceDynamic* Neon = NeonBase ? UMaterialInstanceDynamic::Create(NeonBase, W) : nullptr;
	if (Neon) { Neon->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.45f, 0.9f, 1.f)); Neon->SetScalarParameterValue(TEXT("Strength"), 16.f); }
	for (const float Side : {-1.f, 1.f})
	{
		const FVector Foot = C + Y * 210.f + X * (Side * 62.f);
		Shape(W, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Foot + Z * 135.f, FRotator::ZeroRotator, FVector(0.16f, 0.16f, 2.7f), Neon);
		ARectLight* L = W->SpawnActor<ARectLight>(Foot + Z * 140.f - Y * 14.f, FRotator(0.f, -90.f, 0.f), P);
		if (URectLightComponent* R = L ? L->RectLightComponent : nullptr)
		{
			R->SetMobility(EComponentMobility::Movable);
			R->SetLightColor(FLinearColor(0.55f, 0.88f, 1.f));
			R->SetIntensity(2000.f);
			R->SetSourceWidth(18.f); R->SetSourceHeight(260.f);
			R->SetAttenuationRadius(1400.f);
			R->SetVolumetricScatteringIntensity(1.6f);
			R->SetCastShadows(true);
		}
	}
	// the title screen's pink, low on the floor behind them: the fog glows with it
	for (const float Side : {-1.f, 1.f})
	{
		APointLight* L = W->SpawnActor<APointLight>(C + Y * 290.f + X * (Side * 230.f) + Z * 10.f, FRotator::ZeroRotator, P);
		if (UPointLightComponent* Pt = L ? L->PointLightComponent : nullptr)
		{
			Pt->SetMobility(EComponentMobility::Movable);
			Pt->SetLightColor(FLinearColor(1.f, 0.25f, 0.72f));
			Pt->SetIntensity(1600.f);
			Pt->SetAttenuationRadius(650.f);
			Pt->SetVolumetricScatteringIntensity(9.f);
			Pt->SetCastShadows(false);
		}
	}

	// the two of them, facing the camera (-y): she at the left of the picture, he at the right
	const FVector Her = C + X * 118.f, Him = C - X * 118.f;   // (the camera looks along +y: its left is +x)
	HerAt = Her; HimAt = Him;
	She = Person(W, TEXT("MH_003_passenger"), Her, -90.f);
	He = Person(W, TEXT("MH_900_player"), Him, -90.f);
	// the chosen one in the light: a soft key light from above and in front; the other stays in the tubes' rim light
	{
		const FVector On = bWoman ? Her : Him;
		const FVector Eye = On + FVector(bWoman ? 90.f : -90.f, -250.f, 320.f);
		ASpotLight* L = W->SpawnActor<ASpotLight>(Eye, (On + Z * 130.f - Eye).Rotation(), P);
		KeyLight = L;
		if (USpotLightComponent* S = L ? L->SpotLightComponent : nullptr)
		{
			S->SetMobility(EComponentMobility::Movable);
			S->SetLightColor(FLinearColor(1.f, 0.93f, 0.86f));
			S->SetIntensity(16000.f);
			S->SetInnerConeAngle(14.f); S->SetOuterConeAngle(26.f);
			S->SetAttenuationRadius(900.f);
			S->SetVolumetricScatteringIntensity(0.6f);
			S->SetCastShadows(true);
		}
	}

	// the fog: thick over the floor (a concert's fog machine), a thin haze above it for the beams, lit by the lights
	for (TActorIterator<AExponentialHeightFog> It(W); It; ++It)
	{
		It->SetActorLocation(C + Z * 10.f);
		if (UExponentialHeightFogComponent* F = It->GetComponent())
		{
			F->SetFogDensity(2.2f);
			F->SetFogHeightFalloff(55.f);
			F->SetFogMaxOpacity(1.f);
			F->SetStartDistance(0.f);
			F->SetFogInscatteringColor(FLinearColor(0.02f, 0.04f, 0.09f));
			FExponentialHeightFogData Haze; Haze.FogDensity = 0.005f; Haze.FogHeightFalloff = 0.3f; Haze.FogHeightOffset = 200.f;
			F->SetSecondFogData(Haze);
			F->SetVolumetricFog(true);
			F->SetVolumetricFogScatteringDistribution(0.35f);
			F->SetVolumetricFogExtinctionScale(1.2f);
			F->SetVolumetricFogDistance(3000.f);
		}
	}

	// the camera: before them, a little above waist height, both in the picture from the knees up with the tubes between
	if (PC)
	{
		const FVector Eye = C - Y * 600.f + Z * 124.f;
		ACameraActor* Cam = W->SpawnActor<ACameraActor>(Eye, (C + Z * 118.f - Eye).Rotation(), P);
		if (Cam) { Cam->GetCameraComponent()->SetFieldOfView(43.f); Cam->GetCameraComponent()->SetConstraintAspectRatio(false); PC->SetViewTarget(Cam); }
	}
	ADP_LOG(TEXT("Character creator: the concept room built, %s in the light"), bWoman ? TEXT("she") : TEXT("he"));
}

void ADPCharacterCreate::Choose(bool bWoman)
{
	// the chosen one steps a half step forward into the key light, the other back into the shadow
	AActor* On = bWoman ? She.Get() : He.Get();
	AActor* Off = bWoman ? He.Get() : She.Get();
	const FVector OnAt = bWoman ? HerAt : HimAt, OffAt = bWoman ? HimAt : HerAt;
	if (On) On->SetActorLocation(OnAt + FVector(0, -55.f, 0));
	if (Off) Off->SetActorLocation(OffAt);
	if (ASpotLight* L = KeyLight.Get())
	{
		const FVector Eye = OnAt + FVector(bWoman ? 90.f : -90.f, -300.f, 320.f);
		L->SetActorLocationAndRotation(Eye, (OnAt + FVector(0, -55.f, 130.f) - Eye).Rotation());
	}
}
