#include "CharacterCreate.h"
#include "ADPLog.h"
#include "TerminalPlan.h"
#include "Pets.h"
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
#include "Containers/Ticker.h"
#include "Components/MaterialBillboardComponent.h"
#include "ADPKeep.h"

namespace
{
	constexpr float FillCd = 4500.f;   // the soft front light on both (the key light on the chosen one: 16000)
	// the concept's pieces, for the choice moving between them
	TWeakObjectPtr<ASpotLight> KeyLight;
	TWeakObjectPtr<AActor> She, He;
	FVector HerAt, HimAt, Centre;
	TArray<TWeakObjectPtr<AActor>> Tubes;
	TArray<TWeakObjectPtr<ARectLight>> TubeLights;
	TWeakObjectPtr<UMaterialInstanceDynamic> NeonMat;
	// the choice made: the other walks out of the picture, then the tubes go dark and back into the fog
	int32 Phase = 0;           // 0 choosing, 1 the other walking out, 2 the tubes going, 3 done; 4 the tubes coming back, 5 the other walking back in
	float TubesGone = 0.f;     // how far back the tubes went (cm), to bring them the same way back
	float PhaseT = 0.f;
	bool bChoseWoman = true;
	FTSTicker::FDelegateHandle Ticker;
	// everything the stage spawned, and the world's fog as it was, for the teardown
	TArray<TWeakObjectPtr<AActor>> Spawned;
	TWeakObjectPtr<AExponentialHeightFog> WorldFog;
	FVector FogWasAt; float FogWasDensity = 0.02f, FogWasFalloff = 0.2f, FogWasMaxOpacity = 1.f, FogWasStart = 0.f, FogWasScatter = 0.f, FogWasExtinction = 1.f, FogWasDistance = 6000.f;
	FLinearColor FogWasColour; FExponentialHeightFogData FogWasSecond; bool bFogWasVolumetric = false;
	// the steps after the choice (owner, 2026-11-13): your partner walks in from the edge and stops beside you; your pet
	// walks in and sits at your feet. Anyone walking on the stage is a walker: towards a mark, then turned to the camera
	// (or gone, past the edge)
	TWeakObjectPtr<UWorld> StageWorld;
	TWeakObjectPtr<AActor> Partner, PetActor;
	FString PetNow, PetIdNow;
	struct FWalker { TWeakObjectPtr<AActor> A; FVector To; float Speed; bool bGone; };
	TArray<FWalker> Walkers;
	FTSTicker::FDelegateHandle WalkTicker;

	void SetWalking(AActor* A, bool bWalk)
	{
		if (!A) return;
		TArray<USkeletalMeshComponent*> Skels; A->GetComponents<USkeletalMeshComponent>(Skels);
		for (USkeletalMeshComponent* S : Skels) if (S->GetName().StartsWith(TEXT("Body")))
		{
			static UAnimSequence* Walk = ADPKeep(LoadObject<UAnimSequence>(nullptr, TEXT("/Game/People/Anims/MF_Unarmed_Walk_Fwd_MH.MF_Unarmed_Walk_Fwd_MH")));
			static UAnimSequence* Idle = ADPKeep(LoadObject<UAnimSequence>(nullptr, TEXT("/Game/People/Anims/MM_Idle_MH.MM_Idle_MH")));
			if (UAnimSequence* Anim = bWalk ? Walk : Idle) { S->SetAnimationMode(EAnimationMode::AnimationSingleNode); S->SetAnimation(Anim); S->Play(true); }
		}
	}

	bool Step(float Dt)
	{
		if (Phase == 1)
		{
			// the other one walks to the near edge of the picture and out: she to the left (+x), he to the right (-x)
			AActor* Off = bChoseWoman ? He.Get() : She.Get();
			const float Dir = bChoseWoman ? -1.f : 1.f;
			if (Off)
			{
				Off->AddActorWorldOffset(FVector(Dir * 150.f * Dt, 0.f, 0.f));
				if (FMath::Abs(Off->GetActorLocation().X - Centre.X) > 430.f) { Off->SetActorHiddenInGame(true); Phase = 2; PhaseT = 0.f; }
			}
			else { Phase = 2; PhaseT = 0.f; }
		}
		else if (Phase == 2)
		{
			// the tubes switch off, fade, and are drawn away into the fog until they light nothing; the camera stays
			PhaseT += Dt;
			const float K = FMath::Clamp(PhaseT / 3.6f, 0.f, 1.f);
			TubesGone += 240.f * Dt;
			for (const TWeakObjectPtr<AActor>& T : Tubes) if (AActor* A = T.Get()) A->AddActorWorldOffset(FVector(0.f, 240.f * Dt, 0.f));
			for (const TWeakObjectPtr<ARectLight>& L : TubeLights) if (ARectLight* A = L.Get()) { A->AddActorWorldOffset(FVector(0.f, 240.f * Dt, 0.f)); A->RectLightComponent->SetIntensity(2000.f * FMath::Square(1.f - K)); }
			if (UMaterialInstanceDynamic* M = NeonMat.Get()) M->SetScalarParameterValue(TEXT("Strength"), 16.f * FMath::Pow(1.f - K, 2.2f));
			if (K >= 1.f)
			{
				for (const TWeakObjectPtr<AActor>& T : Tubes) if (AActor* A = T.Get()) A->SetActorHiddenInGame(true);
				for (const TWeakObjectPtr<ARectLight>& L : TubeLights) if (ARectLight* A = L.Get()) A->RectLightComponent->SetVisibility(false);
				Phase = 3;
			}
		}
		else if (Phase == 4)
		{
			// back: the tubes come forward out of the fog and light up again
			PhaseT += Dt;
			const float K = FMath::Clamp(PhaseT / 3.f, 0.f, 1.f);
			const float Move = FMath::Min(TubesGone, (TubesGone / 3.f) * Dt);
			TubesGone -= Move;
			for (const TWeakObjectPtr<AActor>& T : Tubes) if (AActor* A = T.Get()) A->AddActorWorldOffset(FVector(0.f, -Move, 0.f));
			for (const TWeakObjectPtr<ARectLight>& L : TubeLights) if (ARectLight* A = L.Get()) { A->AddActorWorldOffset(FVector(0.f, -Move, 0.f)); A->RectLightComponent->SetIntensity(2000.f * FMath::Square(K)); }
			if (UMaterialInstanceDynamic* M = NeonMat.Get()) M->SetScalarParameterValue(TEXT("Strength"), 16.f * FMath::Pow(K, 2.2f));
			if (K >= 1.f)
			{
				// then the other one walks back in from the edge, facing the way they walk
				AActor* Off = bChoseWoman ? He.Get() : She.Get();
				const FVector Mark = bChoseWoman ? HimAt : HerAt;
				const float Dir = bChoseWoman ? 1.f : -1.f;   // back towards the middle
				if (Off)
				{
					Off->SetActorLocation(Mark + FVector(-Dir * 470.f, 0.f, 0.f));
					Off->SetActorRotation(FRotator(0.f, (Dir > 0 ? 0.f : 180.f) - 90.f, 0.f));
					Off->SetActorHiddenInGame(false);
					SetWalking(Off, true);
				}
				Phase = 5;
			}
		}
		else if (Phase == 5)
		{
			AActor* Off = bChoseWoman ? He.Get() : She.Get();
			const FVector Mark = bChoseWoman ? HimAt : HerAt;
			const float Dir = bChoseWoman ? 1.f : -1.f;
			if (Off)
			{
				Off->AddActorWorldOffset(FVector(Dir * 150.f * Dt, 0.f, 0.f));
				if ((Off->GetActorLocation().X - Mark.X) * Dir >= 0.f)
				{
					// on the mark: turned to the camera again, standing
					Off->SetActorLocation(Mark);
					Off->SetActorRotation(FRotator(0.f, -90.f - 90.f, 0.f));
					SetWalking(Off, false);
					Phase = 0;
				}
			}
			else Phase = 0;
		}
		return Phase != 0 && Phase != 3;
	}

	bool WalkStep(float Dt)
	{
		for (int32 i = Walkers.Num() - 1; i >= 0; --i)
		{
			FWalker& Wk = Walkers[i];
			AActor* A = Wk.A.Get(); if (!A) { Walkers.RemoveAt(i); continue; }
			FVector D = Wk.To - A->GetActorLocation(); D.Z = 0.f;
			const float Step = Wk.Speed * Dt;
			if (D.Size() <= Step)
			{
				A->SetActorLocation(FVector(Wk.To.X, Wk.To.Y, A->GetActorLocation().Z));
				if (Wk.bGone) A->SetActorHiddenInGame(true);
				else { A->SetActorRotation(FRotator(0.f, -90.f - 90.f, 0.f)); SetWalking(A, false); }   // on the mark, turned to the camera
				Walkers.RemoveAt(i);
				continue;
			}
			A->AddActorWorldOffset(D.GetSafeNormal() * Step);
			A->SetActorRotation(FRotator(0.f, D.Rotation().Yaw - 90.f, 0.f));   // a MetaHuman faces its own +y
		}
		if (!Walkers.Num()) { WalkTicker.Reset(); return false; }
		return true;
	}
	void Walk(AActor* A, const FVector& To, bool bGone)
	{
		if (!A) return;
		Walkers.RemoveAll([A](const FWalker& W) { return W.A.Get() == A; });
		A->SetActorHiddenInGame(false);
		SetWalking(A, true);
		Walkers.Add({A, To, 150.f, bGone});
		if (!WalkTicker.IsValid()) WalkTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt) { return WalkStep(Dt); }), 0.f);
	}
	/** Where you stand once chosen (half a step forward into the key light), and towards the room's middle from there. */
	FVector YouAt() { return (bChoseWoman ? HerAt : HimAt) + FVector(0.f, -55.f, 0.f); }
	float ToMiddle() { return bChoseWoman ? -1.f : 1.f; }   // she stands at +x: the middle is -x from her
	/** The free side's edge of the picture (where the UI is): the partner and the pet come in from there. */
	FVector EdgeAt(const FVector& Mark) { return FVector(Centre.X + (bChoseWoman ? -470.f : 470.f), Mark.Y, Mark.Z); }

	AStaticMeshActor* Shape(UWorld* W, const TCHAR* Mesh, const FVector& At, const FRotator& Rot, const FVector& Scale, UMaterialInterface* M)
	{
		UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, Mesh);
		if (!SM) { ADP_WARN(TEXT("Character creator: no mesh %s"), Mesh); return nullptr; }
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(At, Rot, P);
		Spawned.Add(A);
		A->SetMobility(EComponentMobility::Movable);
		A->GetStaticMeshComponent()->SetStaticMesh(SM);
		A->SetActorScale3D(Scale);
		if (M) A->GetStaticMeshComponent()->SetMaterial(0, M);
		return A;
	}

	UMaterialInstanceDynamic* Plain(UObject* Outer, const FLinearColor& Col, float Rough)
	{
		static UMaterialInterface* Base = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/LightMast/M_LM_Plain.M_LM_Plain")));
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
		Spawned.Add(A);
		TArray<USkeletalMeshComponent*> Skels; A->GetComponents<USkeletalMeshComponent>(Skels);
		USkeletalMeshComponent* Body = nullptr;
		for (USkeletalMeshComponent* S : Skels) if (S->GetName().StartsWith(TEXT("Body"))) Body = S;
		if (Body)
		{
			static UAnimSequence* Idle = ADPKeep(LoadObject<UAnimSequence>(nullptr, TEXT("/Game/People/Anims/MM_Idle_MH.MM_Idle_MH")));
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
	Centre = C; Phase = 0; Tubes.Reset(); TubeLights.Reset(); Spawned.Reset(); StageWorld = W; Partner = nullptr; PetActor = nullptr; PetNow.Empty(); Walkers.Reset();
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UMaterialInstanceDynamic* Floor = Plain(W, FLinearColor(0.012f, 0.014f, 0.022f), 0.28f);   // a little shine: the neon mirrored in it
	UMaterialInstanceDynamic* Wall = Plain(W, FLinearColor(0.008f, 0.01f, 0.018f), 0.75f);
	const TCHAR* PlaneMesh = TEXT("/Engine/BasicShapes/Plane.Plane");
	// (owner, 2026-11-23, 32:9: the mountains and the steel showed at both sides and along the top - the side walls and the
	//  ceiling ended before the back wall, and the back wall before a wide picture's edge. Closed all round now, from behind
	//  the camera (-8 m) to the back wall (15 m), and wide enough for a 48:9 picture at the back wall.)
	constexpr float RoomY0 = -800.f, RoomY1 = 1500.f, RoomHalf = 2600.f, RoomMid = (RoomY0 + RoomY1) * 0.5f;
	const float LenY = (RoomY1 - RoomY0) / 100.f + 1.f, LenX = RoomHalf * 2.f / 100.f + 1.f;   // (a metre over: the edges overlap)
	Shape(W, PlaneMesh, C + Y * RoomMid, FRotator::ZeroRotator, FVector(LenX, LenY, 1), Floor);
	for (const float Flip : {0.f, 180.f})   // (a plane shows one face: each wall both ways round, so nothing of the world shows through)
	{
		Shape(W, PlaneMesh, C + Y * RoomMid + Z * 480.f, FRotator(Flip, 0.f, 0.f), FVector(LenX, LenY, 1), Wall);
		Shape(W, PlaneMesh, C + Y * RoomY1 + Z * 240.f, FRotator(0.f, 0.f, 90.f + Flip), FVector(LenX, 6, 1), Wall);   // the back wall, well behind the fog (the tubes go back into it)
		Shape(W, PlaneMesh, C + Y * RoomY0 + Z * 240.f, FRotator(0.f, 0.f, -90.f + Flip), FVector(LenX, 6, 1), Wall);   // behind the camera
		Shape(W, PlaneMesh, C - X * RoomHalf + Y * RoomMid + Z * 240.f, FRotator(90.f + Flip, 0.f, 0.f), FVector(6, LenY, 1), Wall);
		Shape(W, PlaneMesh, C + X * RoomHalf + Y * RoomMid + Z * 240.f, FRotator(-90.f + Flip, 0.f, 0.f), FVector(6, LenY, 1), Wall);
	}
	// the fog wall behind them (owner): a bank of fog behind the tubes that hides everything beyond and takes the
	// lights' colours (it is lit through the volumetric fog)
	{
		ALocalFogVolume* V = W->SpawnActor<ALocalFogVolume>(C + Y * 520.f + Z * 230.f, FRotator::ZeroRotator, P);
		if (V)
		{
			Spawned.Add(V);
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
	static UMaterialInterface* NeonBase = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Neon/M_Neon.M_Neon")));
	UMaterialInstanceDynamic* Neon = NeonBase ? UMaterialInstanceDynamic::Create(NeonBase, W) : nullptr;
	if (Neon) { Neon->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.45f, 0.9f, 1.f)); Neon->SetScalarParameterValue(TEXT("Strength"), 16.f); }
	NeonMat = Neon;
	for (const float Side : {-1.f, 1.f})
	{
		const FVector Foot = C + Y * 210.f + X * (Side * 62.f);
		Tubes.Add(Shape(W, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Foot + Z * 135.f, FRotator::ZeroRotator, FVector(0.16f, 0.16f, 2.7f), Neon));
		ARectLight* L = W->SpawnActor<ARectLight>(Foot + Z * 140.f - Y * 14.f, FRotator(0.f, -90.f, 0.f), P);
		TubeLights.Add(L); Spawned.Add(L);
		if (URectLightComponent* R = L ? L->RectLightComponent : nullptr)
		{
			R->SetMobility(EComponentMobility::Movable);
			R->SetLightColor(FLinearColor(0.55f, 0.88f, 1.f));
			R->SetIntensity(2000.f);
			R->SetSourceWidth(18.f); R->SetSourceHeight(260.f);
			R->SetAttenuationRadius(1400.f);
			R->SetVolumetricScatteringIntensity(4.f);
			R->SetCastShadows(true);
		}
	}
	// the title screen's pink, low on the floor behind them: the fog glows with it
	for (const float Side : {-1.f, 1.f})
	{
		APointLight* L = W->SpawnActor<APointLight>(C + Y * 290.f + X * (Side * 230.f) + Z * 10.f, FRotator::ZeroRotator, P);
		Spawned.Add(L);
		if (UPointLightComponent* Pt = L ? L->PointLightComponent : nullptr)
		{
			Pt->SetMobility(EComponentMobility::Movable);
			Pt->SetLightColor(FLinearColor(1.f, 0.25f, 0.72f));
			Pt->SetIntensity(1600.f);
			Pt->SetAttenuationRadius(650.f);
			Pt->SetVolumetricScatteringIntensity(9.f);
			Pt->SetSpecularScale(0.f);   // (its glare on the shiny floor was a white bar beside her feet, through the "She" label)
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
		KeyLight = L; Spawned.Add(L);
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

	// a soft light from the front on each of them, always on: whoever isn't in the light stands in its shadow but can still
	// be seen (owner, 2026-11-23: she was black beside him)
	for (const FVector& At : {Her, Him})
	{
		const FVector Eye = At + FVector(At.X > C.X ? -60.f : 60.f, -420.f, 210.f);
		ASpotLight* L = W->SpawnActor<ASpotLight>(Eye, (At + Z * 105.f - Eye).Rotation(), P);
		Spawned.Add(L);
		if (USpotLightComponent* S = L ? L->SpotLightComponent : nullptr)
		{
			S->SetMobility(EComponentMobility::Movable);
			L->SetActorRotation((At + Z * 105.f - Eye).Rotation());   // (a spot light is spawned turned 90 degrees down from what is asked: the class's own turn comes on top)
			S->SetLightColor(FLinearColor(0.80f, 0.86f, 1.f));
			S->SetIntensity(FillCd);
			S->SetInnerConeAngle(12.f); S->SetOuterConeAngle(24.f);
			S->SetAttenuationRadius(1400.f);
			S->SetVolumetricScatteringIntensity(0.f);
			S->SetSpecularScale(0.4f);
			S->SetCastShadows(false);
		}
	}

	// the fog: thick over the floor (a concert's fog machine), a thin haze above it for the beams, lit by the lights
	for (TActorIterator<AExponentialHeightFog> It(W); It; ++It)
	{
		if (UExponentialHeightFogComponent* F = It->GetComponent())
		{
			// as it was, for after
			WorldFog = *It; FogWasAt = It->GetActorLocation();
			FogWasDensity = F->FogDensity; FogWasFalloff = F->FogHeightFalloff; FogWasMaxOpacity = F->FogMaxOpacity; FogWasStart = F->StartDistance;
			FogWasColour = F->FogInscatteringLuminance; FogWasSecond = F->SecondFogData; bFogWasVolumetric = F->bEnableVolumetricFog;
			FogWasScatter = F->VolumetricFogScatteringDistribution; FogWasExtinction = F->VolumetricFogExtinctionScale; FogWasDistance = F->VolumetricFogDistance;
		}
		It->SetActorLocation(C + Z * 10.f);
		if (UExponentialHeightFogComponent* F = It->GetComponent())
		{
			F->SetFogDensity(2.2f);
			F->SetFogHeightFalloff(55.f);
			F->SetFogMaxOpacity(1.f);
			F->SetStartDistance(0.f);
			F->SetFogInscatteringColor(FLinearColor(0.02f, 0.04f, 0.09f));
			FExponentialHeightFogData Haze; Haze.FogDensity = 0.014f; Haze.FogHeightFalloff = 0.3f; Haze.FogHeightOffset = 200.f;   // (a faint haze in the air: the tubes' light seen round them)
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
		Spawned.Add(Cam);
		if (Cam)
		{
			Cam->GetCameraComponent()->SetFieldOfView(43.f); Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
			// (the view's height kept: a super-ultrawide screen sees more to the sides, never less above and below - owner
			//  2026-11-16, 32:9: the figure and the pet were cut off at the bottom; 16:9 looks as before)
			Cam->GetCameraComponent()->bOverrideAspectRatioAxisConstraint = true;
			Cam->GetCameraComponent()->AspectRatioAxisConstraint = EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV;
			PC->SetViewTarget(Cam);
		}
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
		L->SpotLightComponent->SetIntensity(16000.f);
		const FVector Eye = OnAt + FVector(bWoman ? 90.f : -90.f, -300.f, 320.f);
		L->SetActorLocationAndRotation(Eye, (OnAt + FVector(0, -55.f, 130.f) - Eye).Rotation());
	}
}

void ADPCharacterCreate::Back()
{
	if (Phase != 3) return;
	for (const TWeakObjectPtr<AActor>& T : Tubes) if (AActor* A = T.Get()) A->SetActorHiddenInGame(false);
	for (const TWeakObjectPtr<ARectLight>& L : TubeLights) if (ARectLight* A = L.Get()) { A->RectLightComponent->SetVisibility(true); A->RectLightComponent->SetIntensity(0.f); }
	Phase = 4; PhaseT = 0.f;
	if (Ticker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt) { return Step(Dt); }), 0.f);
}

bool ADPCharacterCreate::Choosing() { return Phase == 0; }

void ADPCharacterCreate::Confirm(bool bWoman)
{
	if (Phase != 0) return;
	Choose(bWoman);
	bChoseWoman = bWoman;
	GConfig->SetBool(TEXT("ADP.Player"), TEXT("Woman"), bWoman, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni);   // (your own breathing and voice follow it)
	// the other one turns and walks out of the picture (the walk carried over to the MetaHuman body)
	AActor* Off = bWoman ? He.Get() : She.Get();
	if (Off)
	{
		const float Facing = bWoman ? 180.f : 0.f;   // he to -x (the picture's right), she to +x (its left)
		Off->SetActorRotation(FRotator(0.f, Facing - 90.f, 0.f));
		SetWalking(Off, true);
	}
	Phase = 1; PhaseT = 0.f; TubesGone = 0.f;
	if (Ticker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt) { return Step(Dt); }), 0.f);
}

bool ADPCharacterCreate::Done() { return Phase >= 3; }
bool ADPCharacterCreate::UiOnRight() { return bChoseWoman; }

void ADPCharacterCreate::Nobody()
{
	// before any choice: both in the tubes' light only
	if (ASpotLight* L = KeyLight.Get()) L->SpotLightComponent->SetIntensity(0.f);
	if (AActor* A = She.Get()) A->SetActorLocation(HerAt);
	if (AActor* A = He.Get()) A->SetActorLocation(HimAt);
}

void ADPCharacterCreate::Teardown(UWorld* W, APlayerController* PC)
{
	if (Ticker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(Ticker); Ticker.Reset(); }
	if (WalkTicker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(WalkTicker); WalkTicker.Reset(); }
	Walkers.Reset(); Partner = nullptr; PetActor = nullptr; PetNow.Empty();
	for (const TWeakObjectPtr<AActor>& A : Spawned) if (AActor* X = A.Get()) X->Destroy();
	Spawned.Reset(); Tubes.Reset(); TubeLights.Reset(); KeyLight = nullptr; She = nullptr; He = nullptr; NeonMat = nullptr;
	Phase = 0;
	if (AExponentialHeightFog* Fog = WorldFog.Get())
	{
		Fog->SetActorLocation(FogWasAt);
		if (UExponentialHeightFogComponent* F = Fog->GetComponent())
		{
			F->SetFogDensity(FogWasDensity); F->SetFogHeightFalloff(FogWasFalloff); F->SetFogMaxOpacity(FogWasMaxOpacity); F->SetStartDistance(FogWasStart);
			F->SetFogInscatteringColor(FogWasColour); F->SetSecondFogData(FogWasSecond); F->SetVolumetricFog(bFogWasVolumetric);
			F->SetVolumetricFogScatteringDistribution(FogWasScatter); F->SetVolumetricFogExtinctionScale(FogWasExtinction); F->SetVolumetricFogDistance(FogWasDistance);
		}
	}
	WorldFog = nullptr;
	if (PC && PC->GetPawn()) PC->SetViewTarget(PC->GetPawn());
}

void ADPCharacterCreate::ShowPartner(bool bOn)
{
	UWorld* W = StageWorld.Get(); if (!W) return;
	// beside you, towards the room's middle and a little behind; in from (and out to) the free side's edge
	const FVector Mark = YouAt() + FVector(ToMiddle() * 92.f, 22.f, 0.f);
	AActor* P = Partner.Get();
	if (bOn)
	{
		if (!P)
		{
			// a MetaHuman of the other sex (a stand-in until the creator makes yours): he for her, she for him
			P = Person(W, bChoseWoman ? TEXT("MH_007_passenger") : TEXT("MH_003_passenger"), EdgeAt(Mark), 0.f);
			Partner = P;
		}
		if (P) { if (P->IsHidden()) P->SetActorLocation(EdgeAt(Mark)); Walk(P, Mark, false); }
	}
	else if (P && !P->IsHidden()) Walk(P, EdgeAt(Mark), true);
}

FString ADPCharacterCreate::PetChoice()
{
	if (APet* P = Cast<APet>(PetActor.Get())) return P->Model().Folder + TEXT("/") + P->Model().Id;
	return FString();
}

void ADPCharacterCreate::ShowPet(const FString& Kind, const FString& Id, const FLinearColor& Coat)
{
	// a real pet model of this kind (every one in /Game/Pets, Pets.h): the first in the list, or the one chosen, walks
	// in from the free side's edge and sits down beside you; moving the choice on, it stands up and walks back out the
	// way it came while the next one walks in (owner, 2026-11-15)
	if (const ADPPets::FModel* Model = (Kind == TEXT("dog") || Kind == TEXT("cat")) ? ADPPets::Find(Kind, Id) : nullptr)
	{
		UWorld* W = StageWorld.Get(); if (!W) return;
		if (Kind == PetNow && Model->Id == PetIdNow && PetActor.IsValid()) return;
		const FVector Mark = YouAt() + FVector(ToMiddle() * 72.f, -18.f, 0.f);   // beside you, towards the middle, a little in front
		if (APet* Old = Cast<APet>(PetActor.Get())) Old->WalkTo(EdgeAt(Mark), false, 0.f, true);
		else if (AActor* OldA = PetActor.Get()) Walk(OldA, EdgeAt(Mark), true);
		PetNow = Kind; PetIdNow = Model->Id; PetActor = nullptr;
		APet* Pet = APet::Spawn(W, *Model, EdgeAt(Mark), ToMiddle() > 0 ? -90.f : 90.f);
		if (!Pet) return;
		Spawned.Add(Pet); PetActor = Pet;
		Pet->WalkTo(Mark, true, -180.f + ToMiddle() * 12.f);      // sits facing the camera, turned a touch towards you
		ADP_LOG(TEXT("Character creator: the %s walks in and sits beside you"), *Model->Name);
		return;
	}
	if (APet* Old = Cast<APet>(PetActor.Get()))
	{
		const FVector Mark = YouAt() + FVector(ToMiddle() * 72.f, -18.f, 0.f);
		Old->WalkTo(EdgeAt(Mark), false, 0.f, true); PetActor = nullptr; PetNow.Empty(); PetIdNow.Empty();
	}
	// the pet's place: at your feet, towards the middle and in front; in from the free side's edge. Until the real 3D
	// cats and dogs are made (owner, 2026-11-13) a stand-in walks in: the animal's silhouette in its coat colour with a
	// glowing question mark above it (Tools/make_pet_placeholders.py), so it is plain it is to be replaced
	UWorld* W = StageWorld.Get(); if (!W) return;
	const FVector Mark = YouAt() + FVector(ToMiddle() * 50.f, -15.f, 0.f);   // between you and your partner, a little in front
	const bool bPet = Kind == TEXT("dog") || Kind == TEXT("cat");
	AActor* Old = PetActor.Get();
	// the same animal, another coat: only its colour changes
	if (Old && bPet && Kind == PetNow)
	{
		if (UMaterialBillboardComponent* B = Old->FindComponentByClass<UMaterialBillboardComponent>())
			if (B->Elements.Num()) if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(B->Elements[0].Material)) M->SetVectorParameterValue(TEXT("Coat"), Coat);
		return;
	}
	PetNow = Kind;
	if (Old) { Walk(Old, EdgeAt(Mark), true); PetActor = nullptr; }
	if (!bPet) return;
	static UMaterialInterface* Base = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/StagePet/M_PetPlaceholder.M_PetPlaceholder")));
	UTexture* Tex = LoadObject<UTexture>(nullptr, Kind == TEXT("cat") ? TEXT("/Game/StagePet/T_CatPlaceholder.T_CatPlaceholder") : TEXT("/Game/StagePet/T_DogPlaceholder.T_DogPlaceholder"));
	if (!Base || !Tex) { ADP_WARN(TEXT("Character creator: the pet stand-in is missing (Tools/set_pet_placeholders.py)")); return; }
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const float Half = 27.f;   // the picture is 54 cm square; the animal stands in its lower part, its feet at the bottom
	AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(EdgeAt(Mark) + FVector(0, 0, Half)), P);
	if (!A) return;
	UMaterialBillboardComponent* B = NewObject<UMaterialBillboardComponent>(A);
	A->SetRootComponent(B); B->SetWorldLocation(EdgeAt(Mark) + FVector(0, 0, Half));
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, A);
	M->SetTextureParameterValue(TEXT("Tex"), Tex);
	M->SetVectorParameterValue(TEXT("Coat"), Coat);
	B->AddElement(M, nullptr, false, Half, Half, nullptr);
	B->SetCastShadow(false);
	B->RegisterComponent();
	Spawned.Add(A); PetActor = A;
	Walk(A, Mark + FVector(0, 0, Half), false);
	ADP_LOG(TEXT("Character creator: a %s (stand-in) walks in, coat %s"), *Kind, *Coat.ToFColor(true).ToHex());
}

void ADPCharacterCreate::ClearCompany()
{
	ShowPartner(false);
	ShowPet(FString(), FString());
}
