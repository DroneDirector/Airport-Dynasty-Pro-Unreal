// The interaction outline (InteractOutline.h).
#include "InteractOutline.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "Hangar.h"
#include "Wardrobe.h"
#include "WashHose.h"
#include "Seats.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

TArray<AInteractOutline::FThing> AInteractOutline::Listed;
bool AInteractOutline::bOn = true;

AInteractOutline::AInteractOutline()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AInteractOutline::Add(UPrimitiveComponent* C, int32 Item, uint64 Key)
{
	if (C) Listed.Add(FThing{C, Item, Key ? Key : uint64(UPTRINT(C)) * 4099u + uint64(Item + 1)});
}

void AInteractOutline::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Interact/PP_InteractOutline.PP_InteractOutline")))
	{
		Volume = GetWorld()->SpawnActor<APostProcessVolume>();
		if (Volume)
		{
			Volume->bUnbound = true; Volume->Priority = -10.f;
			Volume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, M));
		}
	}
	else ADP_WARN(TEXT("Outline: the material is missing (Tools/set_outline_material.py)"));
}

void AInteractOutline::Gather()
{
	Found = Listed;
	Found.RemoveAll([](const FThing& T) { return !T.C.IsValid(); });
	auto AllOf = [this](ABabylonModel* M) { TArray<UInstancedStaticMeshComponent*> P; M->Parts(P); for (UInstancedStaticMeshComponent* K : P) Found.Add(FThing{K, INDEX_NONE, uint64(UPTRINT(M))}); };
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (ABabylonModel* M = Cast<ABabylonModel>(A))
		{
			// your phone and the operations monitors, whole; the office's laptop and planner, piece by piece
			if (M->Model == TEXT("Smartphone") || M->Model.StartsWith(TEXT("Monitor")) || M->Model == TEXT("DoorPanel")) AllOf(M);
			else if (M->Model.StartsWith(TEXT("Office")))
			{
				TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> F;
				M->InstancesNamed(TEXT("laptop~"), true, F); M->InstancesNamed(TEXT("planner~"), true, F);
				for (const auto& X : F) Found.Add(FThing{X.Get<0>(), X.Get<1>(), uint64(UPTRINT(M)) + (X.Get<2>().StartsWith(TEXT("laptop")) ? 1u : 2u)});
			}
		}
		// the office wardrobe (its body, not its doors)
		else if (AWardrobe* Wd = Cast<AWardrobe>(A)) { if (UPrimitiveComponent* B = Wd->BodyPart()) Found.Add(FThing{B, INDEX_NONE, uint64(UPTRINT(B))}); }
		// the hangar doors' buttons (the buttons, not the doors)
		else if (AHangar* Hg = Cast<AHangar>(A)) { for (UStaticMeshComponent* B : Hg->ButtonList()) Found.Add(FThing{B, INDEX_NONE, uint64(UPTRINT(B))}); }
	}
}

void AInteractOutline::Tick(float Dt)
{
	Super::Tick(Dt);
	if ((GatherIn -= Dt) <= 0.f) { GatherIn = 5.f; Gather(); }
	if ((PickIn -= Dt) > 0.f) return;
	PickIn = 0.15f;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	// what's within reach now (the wash hose's gun only while it hangs on its hook; nothing from inside a vehicle)
	struct FNear { UPrimitiveComponent* C; int32 Item; float D; uint64 Key; float Aim; };
	TArray<FNear> Near;
	// (owner, 2026-11-14: only the one thing in the middle of your view - not every chair round you - and none while you
	// sit or hold something)
	const bool bWalking = Me && Me->IsA<ACharacter>() && bOn && !ASeats::IsSitting() && !AWashHose::Held();
	FVector Cam = Me ? Me->GetPawnViewLocation() : FVector::ZeroVector, Look = FVector::ForwardVector;
	if (PC && PC->PlayerCameraManager) { Cam = PC->PlayerCameraManager->GetCameraLocation(); Look = PC->PlayerCameraManager->GetCameraRotation().Vector(); }
	if (bWalking)
	{
		const FVector At = Me->GetActorLocation();
		auto Consider = [&](UPrimitiveComponent* C, int32 Item, uint64 Key)
		{
			if (!C || !C->IsVisible()) return;
			FVector P;
			if (Item != INDEX_NONE) { UInstancedStaticMeshComponent* K = Cast<UInstancedStaticMeshComponent>(C); FTransform T; if (!K || !K->GetInstanceTransform(Item, T, true) || T.GetScale3D().GetMin() < 0.01f) return; P = T.GetLocation(); }
			else P = C->Bounds.Origin;
			const float D = FVector::Dist(P, At);
			if (D < Reach) Near.Add(FNear{C, Item, D, Key, float(FVector::DotProduct(Look, (P - Cam).GetSafeNormal()))});
		};
		for (const FThing& T : Found) if (!(T.Key >= (uint64(1) << 62) && T.Key < (uint64(1) << 62) + 1000000 && ASeats::IsTaken(int32(T.Key - (uint64(1) << 62))))) Consider(T.C.Get(), T.Item, T.Key);
		if (AWashHose* H = AWashHose::First()) if (!AWashHose::Held()) { UInstancedStaticMeshComponent* K = nullptr; int32 I = INDEX_NONE; if (H->GunOnHook(K, I)) { Consider(K, I, uint64(UPTRINT(H))); for (const auto& L : H->CoilPieces()) Consider(L.Key, L.Value, uint64(UPTRINT(H))); } }
		// the one you look at: its piece nearest the middle of the view (within about 20 degrees), then all its pieces
		float BestAim = FMath::Cos(FMath::DegreesToRadians(20.f)); uint64 BestKey = 0; bool bAny = false;
		for (const FNear& N : Near) if (N.Aim > BestAim) { BestAim = N.Aim; BestKey = N.Key; bAny = true; }
		{ static uint64 LastKey = 1; if (BestKey != LastKey) { LastKey = BestKey; float MaxAim = -2.f; for (const FNear& N : Near) MaxAim = FMath::Max(MaxAim, N.Aim); ADP_LOG(TEXT("Outline: %d within reach, best aim %.3f, %s"), Near.Num(), MaxAim, bAny ? TEXT("one picked") : TEXT("none in view")); } }
		Near.RemoveAll([&](const FNear& N) { return !bAny || N.Key != BestKey; });
		if (Near.Num() > 24) Near.SetNum(24);
	}
	// whole components: drawn into the stencil while near
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Now;
	int32 Used = 0;
	for (const FNear& N : Near)
	{
		if (N.Item == INDEX_NONE && !N.C->IsA<UInstancedStaticMeshComponent>())
		{
			N.C->SetRenderCustomDepth(true); N.C->SetCustomDepthStencilValue(1); Now.Add(N.C);
			continue;
		}
		// an instanced piece (or every instance of a small model's parts): a stand-in copy, drawn only into the stencil
		UInstancedStaticMeshComponent* K = Cast<UInstancedStaticMeshComponent>(N.C);
		if (!K || !K->GetStaticMesh()) continue;
		const int32 From = N.Item == INDEX_NONE ? 0 : N.Item, To = N.Item == INDEX_NONE ? FMath::Min(K->GetInstanceCount(), 8) : N.Item + 1;
		for (int32 i = From; i < To; ++i)
		{
			FTransform T; if (!K->GetInstanceTransform(i, T, true)) continue;
			if (Used >= StandIns.Num())
			{
				UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(this);
				S->SetupAttachment(RootComponent);
				S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				S->SetCastShadow(false);
				S->SetRenderInMainPass(false); S->SetRenderInDepthPass(false);
				S->SetRenderCustomDepth(true); S->SetCustomDepthStencilValue(1);
				S->SetUsingAbsoluteLocation(true); S->SetUsingAbsoluteRotation(true); S->SetUsingAbsoluteScale(true);
				S->RegisterComponent();
				StandIns.Add(S);
			}
			UStaticMeshComponent* S = StandIns[Used++];
			if (S->GetStaticMesh() != K->GetStaticMesh()) S->SetStaticMesh(K->GetStaticMesh());
			S->SetWorldTransform(T);
			S->SetVisibility(true);
		}
	}
	for (int32 i = Used; i < StandIns.Num(); ++i) if (StandIns[i]->IsVisible()) StandIns[i]->SetVisibility(false);
	for (const TWeakObjectPtr<UPrimitiveComponent>& W : Marked) if (W.IsValid() && !Now.Contains(W)) W->SetRenderCustomDepth(false);
	Marked = Now;
}
