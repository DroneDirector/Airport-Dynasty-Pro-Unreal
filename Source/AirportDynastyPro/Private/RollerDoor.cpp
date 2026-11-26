// A roller door: the moving curtain of slats in its guides (RollerDoor.h).
#include "RollerDoor.h"
#include "ADPLog.h"
#include "TerminalPlan.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "CargoTerminalLife.h"
#include "Components/AudioComponent.h"

TArray<TWeakObjectPtr<ARollerDoor>> ARollerDoor::All;

ARollerDoor::ARollerDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

ARollerDoor* ARollerDoor::Place(UWorld* W, const FString& Id, float X0, float X1, float Z, float Height)
{
	return PlaceAt(W, Id, FTerminalPlan::ToWorld((X0 + X1) * 0.5f, 0.01f, Z), 0.f, X1 - X0, Height);
}

ARollerDoor* ARollerDoor::PlaceAt(UWorld* W, const FString& Id, const FVector& Foot, float YawDeg, float Width, float Height, const FString& SlatPath, const FString& BarPath, float PanelPitch, const FString& WinPath)
{
	if (!W) return nullptr;
	const FString SP = SlatPath.IsEmpty() ? FString(TEXT("/Game/Terminal/Meshes/roller_slat.roller_slat")) : SlatPath;
	const FString BP = BarPath.IsEmpty() ? FString(TEXT("/Game/Terminal/Meshes/roller_bar.roller_bar")) : BarPath;
	UStaticMesh* SlatM = LoadObject<UStaticMesh>(nullptr, *SP);
	UStaticMesh* BarM = LoadObject<UStaticMesh>(nullptr, *BP);
	if (!SlatM || !BarM) { ADP_WARN(TEXT("Roller door %s: its slat or bar is missing (%s, %s - Tools/import_annex.py)"), *Id, *SP, *BP); return nullptr; }
	FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARollerDoor* D = W->SpawnActor<ARollerDoor>(Foot, FRotator(0.f, YawDeg, 0.f), S);
	if (!D) return nullptr;
	D->Id = Id; D->Width = Width; D->Height = Height;
	auto List = [D](UStaticMesh* M)
	{
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(D);
		K->SetupAttachment(D->Root);
		K->SetMobility(EComponentMobility::Movable);
		K->SetStaticMesh(M);
		K->SetCollisionProfileName(TEXT("BlockAll"));
		K->RegisterComponent();
		D->AddInstanceComponent(K);
		return K;
	};
	D->Slats = List(SlatM); D->Bar = List(BarM);
	int32 N = FMath::CeilToInt(Height / Pitch) + 1;
	if (PanelPitch > 0.05f)
	{
		// a sectional door: panels made to its width (scale 1), the mesh's width found from its bounds (the import may
		// lay a Blender model's x along Unreal's y: then the panel is turned a quarter); the window panel at eye height
		const FBox B = SlatM->GetBoundingBox();
		D->Panel = PanelPitch; D->PanelLo = B.Min.Z / 100.f;
		if (B.GetSize().Y > B.GetSize().X) D->LeafTurn = FQuat(FVector::UpVector, PI * 0.5f);
		N = FMath::CeilToInt((Height - D->PanelLo) / PanelPitch - 0.01f) + 1;
		if (UStaticMesh* WinM = WinPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *WinPath))
		{
			D->Win = List(WinM); D->Win->AddInstance(FTransform::Identity, false);
			D->WinIdx = FMath::Clamp(FMath::FloorToInt(1.6f / PanelPitch), 1, N - 2);
			// its panes: their own mesh beside it (see-through glass is drawn without Nanite: never inside a Nanite mesh)
			if (UStaticMesh* GlassM = LoadObject<UStaticMesh>(nullptr, *WinPath.Replace(TEXT("_Window_"), TEXT("_Glass_"))))
			{
				D->Glass = List(GlassM); D->Glass->SetCollisionEnabled(ECollisionEnabled::NoCollision); D->Glass->AddInstance(FTransform::Identity, false);
			}
		}
	}
	// every slat of the curtain (one more than the opening's height needs: the top one runs into the hood)
	for (int32 i = 0; i < N; ++i) D->Slats->AddInstance(FTransform::Identity, false);
	D->Bar->AddInstance(FTransform::Identity, false);
	D->Layout();
	All.Add(D);
	ADP_LOG(TEXT("Roller door %s: %.1f m wide, %.1f m high, %d slats"), *Id, D->Width, Height, N);
	return D;
}

void ARollerDoor::Layout()
{
	// the curtain's foot at K x the height; the slats stacked on it; any slat above the opening's head is wound into the
	// hood (shrunk away: its list is shared by the door's own slats only)
	const float Foot = K * Height;
	const int32 N = Slats->GetInstanceCount();
	if (Panel > 0.f)
	{
		// sectional: each panel slides up into the head (the lintel / header hides it) and is put away once its bottom
		// is past the opening's head; the bottom rail with its seal rides under the lowest panel
		const FTransform Gone(FQuat::Identity, FVector(0.f, 0.f, Height * 100.f + 30.f), FVector(0.0001f));
		TArray<FTransform> T; T.SetNum(N);
		FTransform WinT = Gone;
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = Foot + i * Panel;
			const bool bIn = Y + PanelLo < Height - 0.005f;
			const FTransform At(LeafTurn, FVector(0.f, 0.f, Y * 100.f));
			if (i == WinIdx && Win) { WinT = bIn ? At : Gone; T[i] = Gone; }
			else T[i] = bIn ? At : Gone;
		}
		Slats->BatchUpdateInstancesTransforms(0, T, false, true, true);
		if (Win) Win->UpdateInstanceTransform(0, WinT, false, true, true);
		if (Glass) Glass->UpdateInstanceTransform(0, WinT, false, true, true);
		Bar->UpdateInstanceTransform(0, FTransform(LeafTurn, FVector(0.f, 0.f, Foot * 100.f)), false, true, true);
		return;
	}
	TArray<FTransform> T; T.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		const float Y = Foot + i * Pitch;
		const bool bIn = Y + Pitch <= Height + 0.02f;
		T[i] = bIn ? FTransform(FQuat::Identity, FVector(0.f, 0.f, Y * 100.f), FVector(Width, 1.f, 1.f))
		           : FTransform(FQuat::Identity, FVector(0.f, 0.f, Height * 100.f + 30.f), FVector(0.0001f));
	}
	Slats->BatchUpdateInstancesTransforms(0, T, false, true, true);
	Bar->UpdateInstanceTransform(0, FTransform(FQuat::Identity, FVector(0.f, 0.f, FMath::Max(0.f, Foot - 0.06f) * 100.f), FVector(Width, 1.f, 1.f)), false, true, true);
}

bool ARollerDoor::SomethingInDoorway() const
{
	// the doorway's box, 1.5 m deep each side of the curtain: anyone or any vehicle in it stops the door coming down
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_Pawn); Obj.AddObjectTypesToQuery(ECC_PhysicsBody); Obj.AddObjectTypesToQuery(ECC_Vehicle); Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(RollerDoor), false, this);
	const FVector C = GetActorLocation() + FVector(0.f, 0.f, Height * 50.f + 10.f);
	return GetWorld()->OverlapAnyTestByObjectType(C, GetActorQuat(), Obj, FCollisionShape::MakeBox(FVector(Width * 50.f, 150.f, Height * 50.f - 15.f)), Q);
}

void ARollerDoor::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Slats) return;
	// someone near it (the player): it stays as it is; nobody for 40 s and it closes by itself
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bNear = P && FVector::Dist2D(P->GetActorLocation(), GetActorLocation()) < FMath::Max(800.f, Width * 60.f);
	Quiet = bNear ? 0.f : Quiet + Dt;
	if (HoldOpen > 0.f) { HoldOpen -= Dt; Want = 1.f; Quiet = 0.f; }
	if (Want > 0.5f && K >= 1.f && Quiet > 40.f && HoldOpen <= 0.f) { Want = 0.f; ADP_LOG(TEXT("Roller door %s: closing by itself (nobody near)"), *Id); }
	// never on anyone: coming down with something in the doorway, it goes back up
	if (Want < K && SomethingInDoorway()) { Want = 1.f; Quiet = 0.f; ADP_LOG(TEXT("Roller door %s: something in the doorway - back up"), *Id); }
	if (FMath::IsNearlyEqual(K, Want, 0.0005f))
	{
		// arrived: going up, the motor's whine fades as it stops at the top; coming down, the recording ends on its thud
		if (Dir > 0 && IsValid(Hum)) Hum->FadeOut(0.3f, 0.f);
		Dir = 0;
		return;
	}
	const int8 NowDir = Want > K ? 1 : -1;
	if (NowDir != Dir)
	{
		// a start or a turn: the motor and the roll (the recording at the door's own pace: down, so its landing clunks come
		// as the bottom bar touches the floor - the recording lands 14.6 s in; up, from the switch and the motor's start)
		static USoundBase* UpS = ACargoTerminalLife::Keep(TEXT("/Game/FireStation/Sounds/FsDoorUp.FsDoorUp"));
		static USoundBase* DownS = ACargoTerminalLife::Keep(TEXT("/Game/FireStation/Sounds/FsDoorDown.FsDoorDown"));
		if (IsValid(Hum)) Hum->FadeOut(0.15f, 0.f);
		Hum = nullptr;
		const float Travel = (NowDir > 0 ? 1.f - K : K) * 2.f * FMath::Max(1.f, Height);   // (seconds left of the stroke)
		if (USoundBase* S = NowDir > 0 ? UpS : DownS)
			Hum = UGameplayStatics::SpawnSoundAtLocation(this, S, GetActorLocation() + FVector(0.f, 0.f, Height * 100.f), FRotator::ZeroRotator,
				FMath::Clamp(Width / 6.f, 0.6f, 1.f), 1.f, NowDir > 0 ? 0.f : FMath::Max(0.f, 14.6f - Travel), ACargoTerminalLife::Heard(4500.f), nullptr, true);
		Dir = NowDir;
	}
	// half a metre a second, as a real door's motor (the 8 m door: 8 s up)
	K = FMath::FInterpConstantTo(K, Want, Dt, 0.5f / FMath::Max(1.f, Height));
	Layout();
}

static ARollerDoor* FindFaced(UWorld* W, const FVector& Feet, const FVector& Facing)
{
	ARollerDoor* Best = nullptr; float BestD = 1e9f;
	for (TActorIterator<ARollerDoor> It(W); It; ++It)
	{
		ARollerDoor* D = *It;
		// the nearest point of the door's line (it runs along the door's own x, however the door is turned)
		const FVector C = D->GetActorLocation();
		const float HalfW = D->DoorWidth() * 50.f;
		const FVector L = D->GetActorTransform().InverseTransformPositionNoScale(Feet);
		const FVector At = D->GetActorTransform().TransformPositionNoScale(FVector(FMath::Clamp(L.X, -HalfW + 50.f, HalfW - 50.f), 0.f, 0.f));
		const float Dist = FVector::Dist2D(Feet, At);
		if (Dist > 300.f || FMath::Abs(Feet.Z - C.Z) > 250.f) continue;
		const FVector To = (At - Feet).GetSafeNormal2D();
		if (Dist > 80.f && FVector::DotProduct(To, Facing.GetSafeNormal2D()) < 0.3f) continue;
		if (Dist < BestD) { BestD = Dist; Best = D; }
	}
	return Best;
}

bool ARollerDoor::DoorNear(UWorld* W, const FVector& Feet, const FVector& Facing, bool& bIsOpen)
{
	if (!W) return false;
	if (ARollerDoor* D = FindFaced(W, Feet, Facing)) { bIsOpen = D->Want > 0.5f; return true; }
	return false;
}

bool ARollerDoor::UseDoor(const APawn* Me)
{
	if (!Me || !Me->GetWorld()) return false;
	const FVector Feet = Me->GetActorLocation() - FVector(0, 0, Me->GetSimpleCollisionHalfHeight());
	const FVector Facing = Me->GetControlRotation().Vector();
	ARollerDoor* D = FindFaced(Me->GetWorld(), Feet, Facing);
	if (!D) return false;
	D->Want = D->Want > 0.5f ? 0.f : 1.f; D->Quiet = 0.f;
	if (D->Want < 0.5f) D->HoldOpen = 0.f;
	ADP_LOG(TEXT("Roller door %s %s (E / X)"), *D->Id, D->Want > 0.5f ? TEXT("going up") : TEXT("coming down"));
	return true;
}

void ARollerDoor::OpenFor(UWorld* W, const FVector& At, float Radius, float Seconds)
{
	if (!W) return;
	for (TActorIterator<ARollerDoor> It(W); It; ++It)
		if (FVector::Dist2D(It->GetActorLocation(), At) < Radius) { It->HoldOpen = FMath::Max(It->HoldOpen, Seconds); It->Want = 1.f; }
}
