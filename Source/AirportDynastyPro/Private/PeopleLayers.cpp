#include "PeopleLayers.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "GroomComponent.h"
#include "GroomAsset.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/LODSyncComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "SceneInterface.h"

namespace
{
	constexpr float FullTo = 2000.f;      // (cm: whole people up to here, the light mesh from here on)
	constexpr float MakeAt = 3200.f;      // (their parts are made from here on, one a frame)
	constexpr float DropAt = 5500.f;      // (and let go from here on)
	const FName LightTag(TEXT("PersonLight"));

	struct FOne
	{
		TWeakObjectPtr<AActor> A; TWeakObjectPtr<USkeletalMeshComponent> Body, Light; TWeakObjectPtr<ULODSyncComponent> Sync;
		TArray<TWeakObjectPtr<UPrimitiveComponent>> Parts;
		float Full = 0.f;                 // (the distance set on all of it; 1: only the light one is drawn)
		bool bFade = false, bFar = false;
	};
	TArray<FOne> Ones;

	FString LightPath(const FString& Name) { return FString::Printf(TEXT("/Game/People/Layers/%s/SKM_%s_Layer"), *Name, *Name); }
	FString LightObject(const FString& Name) { return FString::Printf(TEXT("%s.SKM_%s_Layer"), *LightPath(Name), *Name); }
	bool HasLight(const FString& Name)
	{
		static TMap<FString, bool> Known;
		if (const bool* K = Known.Find(Name)) return *K;
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("NoPeopleLayers"));   // (to compare)
		return Known.Add(Name, !bOff && FPackageName::DoesPackageExist(LightPath(Name)));
	}
	USkeletalMesh* LightOf(const FString& Name) { return HasLight(Name) ? FindObject<USkeletalMesh>(nullptr, *LightObject(Name)) : nullptr; }
	bool IsPartOf(const UActorComponent* C) { return C && (C->IsA<USkeletalMeshComponent>() || C->IsA<UGroomComponent>()) && !C->GetName().StartsWith(TEXT("Body")) && !C->ComponentHasTag(LightTag); }
	void Reach(UPrimitiveComponent* P, float Min, float Max)
	{
		if (!P) return;
		P->MinDrawDistance = Min;
		P->SetCachedMaxDrawDistance(Max);
		if (P->IsRegistered() && P->GetScene()) P->GetScene()->UpdatePrimitiveDrawDistance(P, Min, Max, P->GetVirtualTextureMainPassMaxDrawDistance());
	}
	bool Missing(const UPrimitiveComponent* P)
	{
		if (!P || P->IsRegistered()) return false;
		if (const UGroomComponent* G = Cast<UGroomComponent>(P)) return G->GroomAsset != nullptr;
		const USkeletalMeshComponent* S = Cast<USkeletalMeshComponent>(P);
		return S && S->GetSkeletalMeshAsset();
	}
	void MakePart(FOne& O, UPrimitiveComponent* P)
	{
		// (its parents first: the hair sits on the face)
		if (UPrimitiveComponent* Up = Cast<UPrimitiveComponent>(P->GetAttachParent()); Up && !Up->IsRegistered()) { MakePart(O, Up); return; }
		ADP_PERF_SCOPE("people: one part of a person made", 4.f);
		if (O.Light.IsValid()) { P->bUseAttachParentBound = true; P->SetCachedMaxDrawDistance(O.Full); }
		if (USkeletalMeshComponent* S = Cast<USkeletalMeshComponent>(P)) S->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		P->RegisterComponent();
	}
	void SetFull(FOne& O, float Full)
	{
		if (FMath::IsNearlyEqual(O.Full, Full, 1.f)) return;
		O.Full = Full;
		Reach(O.Light.Get(), Full > 1.f ? Full : 0.f, 0.f);
		Reach(O.Body.Get(), 0.f, Full);
		for (const TWeakObjectPtr<UPrimitiveComponent>& P : O.Parts) Reach(P.Get(), 0.f, Full);
	}
}

TArray<FSoftObjectPath> ADPLayers::Paths(const FString& Name)
{
	TArray<FSoftObjectPath> Out;
	Out.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/People/Built/%s/BP_%s.BP_%s_C"), *Name, *Name, *Name)));
	if (HasLight(Name)) Out.Add(FSoftObjectPath(LightObject(Name)));
	return Out;
}

ADPLayers::FBodyOnly::FBodyOnly(UClass* C, const FString& Name)
{
	if (!LightOf(Name)) return;
	for (UBlueprintGeneratedClass* B = Cast<UBlueprintGeneratedClass>(C); B; B = Cast<UBlueprintGeneratedClass>(B->GetSuperClass()))
		if (B->SimpleConstructionScript)
			for (USCS_Node* N : B->SimpleConstructionScript->GetAllNodes())
				if (N && IsPartOf(N->ComponentTemplate) && N->ComponentTemplate->bAutoRegister) { N->ComponentTemplate->bAutoRegister = false; Off.Add(N->ComponentTemplate.Get()); }
}

ADPLayers::FBodyOnly::~FBodyOnly() { for (const TWeakObjectPtr<UActorComponent>& T : Off) if (T.IsValid()) T->bAutoRegister = true; }

bool ADPLayers::IsLight(const UActorComponent* C) { return C && C->ComponentHasTag(LightTag); }

void ADPLayers::Add(AActor* A, USkeletalMeshComponent* Body, const FString& Name)
{
	if (!A || !Body || Ones.ContainsByPredicate([A](const FOne& O) { return O.A == A; })) return;
	FOne O; O.A = A; O.Body = Body; O.Sync = A->FindComponentByClass<ULODSyncComponent>();
	TInlineComponentArray<UPrimitiveComponent*> All(A);
	for (int32 Round = 0; Round < 3; ++Round)   // (the face, the clothes, the hair)
		for (UPrimitiveComponent* P : All)
			if (IsPartOf(P) && (P->IsA<UGroomComponent>() ? 2 : P->GetName().StartsWith(TEXT("Face")) ? 0 : 1) == Round) O.Parts.Add(P);
	USkeletalMesh* M = LightOf(Name);
	if (!M && HasLight(Name))
	{
		ADP_LOG(TEXT("People: %s came before their light mesh was in memory"), *Name);
		M = LoadObject<USkeletalMesh>(nullptr, *LightObject(Name));
	}
	if (M)
	{
		USkeletalMeshComponent* S = NewObject<USkeletalMeshComponent>(A, TEXT("PersonLight"));
		S->ComponentTags.Add(LightTag);
		S->SetupAttachment(Body);
		S->SetSkeletalMeshAsset(M);
		S->SetLeaderPoseComponent(Body);
		S->bUseAttachParentBound = true;
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision); S->SetCanEverAffectNavigation(false); S->SetGenerateOverlapEvents(false);
		S->bEnableUpdateRateOptimizations = true; S->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		S->SetReceivesDecals(false);
		S->RegisterComponent();
		O.Light = S;
		for (const TWeakObjectPtr<UPrimitiveComponent>& P : O.Parts) if (P.IsValid() && P->IsRegistered()) { P->bUseAttachParentBound = true; P->UpdateBounds(); }
		// (nothing but the light mesh until the parts are all there: Tick)
		SetFull(O, 1.f);
	}
	Ones.Add(MoveTemp(O));
}

void ADPLayers::Whole(AActor* A)
{
	for (FOne& O : Ones) if (O.A == A)
		for (const TWeakObjectPtr<UPrimitiveComponent>& P : O.Parts) for (int32 k = 0; k < 4 && Missing(P.Get()); ++k) MakePart(O, P.Get());
}

void ADPLayers::Tick(UWorld* W, float Dt)
{
	ADP_PERF_SCOPE("people: their layers (each frame)", 1.f);
	const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager) return;
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	// (zoomed in they are as near as they look; and the settings' view distance would stretch every distance)
	static const IConsoleVariable* Vd = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewDistanceScale"));
	const float Fov = FMath::Clamp(PC->PlayerCameraManager->GetFOVAngle(), 5.f, 120.f);
	const float Zoom = FMath::RoundToFloat(4.f * FMath::Clamp(FMath::Tan(FMath::DegreesToRadians(35.f)) / FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)), 1.f, 12.f)) * 0.25f;
	const float Full = FullTo * Zoom / FMath::Max(Vd ? Vd->GetFloat() : 1.f, 0.1f);
	FOne* NextO = nullptr; UPrimitiveComponent* NextP = nullptr; float NextD = MAX_flt;
	FOne* DropO = nullptr; float DropD = 0.f;
	Ones.RemoveAllSwap([](const FOne& O) { return !O.A.IsValid() || !O.Body.IsValid(); });
	for (FOne& O : Ones)
	{
		AActor* A = O.A.Get();
		USkeletalMeshComponent* Body = O.Body.Get(); USkeletalMeshComponent* Light = O.Light.Get();
		if (O.bFade || A->IsHidden()) continue;
		// (boarding: the light rising over them takes over all their parts)
		if (Body->GetOverlayMaterial() || (Light && Light->GetOverlayMaterial())) { O.bFade = true; continue; }
		const float D = float(FVector::Dist(Cam, A->GetActorLocation())) / Zoom;
		UPrimitiveComponent* Gap = nullptr; bool bAny = false;
		for (const TWeakObjectPtr<UPrimitiveComponent>& P : O.Parts)
			if (P.IsValid()) { if (P->IsRegistered()) bAny = true; else if (!Gap && Missing(P.Get())) Gap = P.Get(); }
		if (Gap && (D < MakeAt || !Light) && D < NextD) { NextO = &O; NextP = Gap; NextD = D; }
		if (!Light) continue;
		if (bAny && D > DropAt && D > DropD) { DropO = &O; DropD = D; }
		SetFull(O, Gap ? 1.f : Full);
		// far away: the body does not keep the close-up's bones going, and its pose is wanted as long as the light one is seen
		const bool bFar = Gap || D > FullTo * 1.1f;
		if (bFar != O.bFar)
		{
			O.bFar = bFar;
			if (O.Sync.IsValid()) O.Sync->SetComponentTickEnabled(!bFar);
			if (bFar) Body->SetForcedLOD(FMath::Min(3, Body->GetNumLODs()));
			else if (!O.Sync.IsValid()) Body->SetForcedLOD(0);
		}
		if (const float T = Light->GetLastRenderTimeOnScreen(); T > Body->GetLastRenderTimeOnScreen()) Body->SetLastRenderTime(T);
	}
	if (NextP) MakePart(*NextO, NextP);
	else if (DropO)
		for (int32 i = DropO->Parts.Num() - 1; i >= 0; --i)
			if (UPrimitiveComponent* P = DropO->Parts[i].Get(); P && P->IsRegistered()) { P->UnregisterComponent(); break; }
}

FString ADPLayers::Counts()
{
	int32 Both = 0, Lit = 0, None = 0, Parts = 0;
	for (const FOne& O : Ones)
	{
		if (!O.Light.IsValid()) ++None; else if (O.Full > 1.f) ++Both; else ++Lit;
		for (const TWeakObjectPtr<UPrimitiveComponent>& P : O.Parts) if (P.IsValid() && P->IsRegistered()) ++Parts;
	}
	return FString::Printf(TEXT("%d people whole when near, %d light only, %d without a light mesh, %d parts there"), Both, Lit, None, Parts);
}
