// What the passengers carry: the owner's phone and tablet as hand props, one instanced list per device part for the
// whole crowd (PaxProps.h).
#include "PaxProps.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "PaxAnimInstance.h"
#include "People.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace PaxPropsInternal
{
	TWeakObjectPtr<APaxProps> GProps;
	const TCHAR* KitModel[2] = {TEXT("Smartphone"), TEXT("Tablet")};
	constexpr float TabletShare = 0.10f, BackpackShare = 0.f, EarphoneShare = 0.f;   // (out of 1; backpack / earphones 0 until to-do 204's models exist)
	constexpr float PropLiftS = 0.7f;   // seconds out of the pocket into the hands (and back)
	float PropEase(float K) { return FMath::SmoothStep(0.f, 1.f, K); }
	/** The hands' own axes (hand-local: along the fingers, out of the palm), per body asset. */
	struct FHands { FVector Along[2], Palm[2]; bool bOk = false; };
	TMap<const UObject*, FHands> GHands;
	const FHands& HandsOf(const USkeletalMeshComponent* B)
	{
		const UObject* Key = B->GetSkinnedAsset();
		if (const FHands* H = GHands.Find(Key)) return *H;
		FHands H;
		H.bOk = UPaxAnimInstance::HandAxes(B, true, H.Along[1], H.Palm[1]) && UPaxAnimInstance::HandAxes(B, false, H.Along[0], H.Palm[0]);
		return GHands.Add(Key, H);
	}
}
using namespace PaxPropsInternal;

APaxProps::APaxProps()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // (after the animation: the device sits in this frame's hand)
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

APaxProps* APaxProps::Find(const UWorld* W)
{
	if (GProps.IsValid() && GProps->GetWorld() == W) return GProps.Get();
	if (!W) return nullptr;
	for (TActorIterator<APaxProps> It(W); It; ++It) { GProps = *It; return *It; }
	return nullptr;
}

APaxProps* APaxProps::Get(UWorld* W)
{
	if (APaxProps* F = Find(W)) return F;
	if (!W) return nullptr;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APaxProps* F = W->SpawnActor<APaxProps>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	GProps = F;
	return F;
}

bool APaxProps::MakeKit(int32 Kind)
{
	FKit& K = Kits[Kind];
	if (K.bTried) return K.bOk;
	K.bTried = true;
	// the owner's model, placed once as ABabylonModel places it for the player (every part where the model has it); its
	// parts are read off it (mesh, materials, place in the device's own frame: screen +z, top -y) and it goes again
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABabylonModel* Tpl = GetWorld()->SpawnActor<ABabylonModel>(FVector::ZeroVector, FRotator::ZeroRotator, SP);
	if (!Tpl || !Tpl->Place(KitModel[Kind])) { if (Tpl) Tpl->Destroy(); ADP_WARN(TEXT("PaxProps: no %s model (/Game/Babylon/%s, Data/babylon-%s.json)"), KitModel[Kind], KitModel[Kind], *FString(KitModel[Kind]).ToLower()); return false; }
	Tpl->SetActorHiddenInGame(true);
	TSet<const UInstancedStaticMeshComponent*> ScreenLists, Shadowed;
	{
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> L;
		Tpl->InstancesNamed(TEXT("Screen"), false, L); for (const auto& T : L) ScreenLists.Add(T.Get<0>());
		L.Reset(); Tpl->InstancesNamed(TEXT("Back"), false, L); for (const auto& T : L) Shadowed.Add(T.Get<0>());
	}
	UMaterialInstanceDynamic* Lit = nullptr;
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Tablet/M_DeviceScreen.M_DeviceScreen")))
	{ Lit = UMaterialInstanceDynamic::Create(Base, this); Lit->SetScalarParameterValue(TEXT("Bright"), 0.8f); }
	const FTransform Frame = Tpl->GetActorTransform();
	TArray<UInstancedStaticMeshComponent*> Src; Tpl->Parts(Src);
	for (UInstancedStaticMeshComponent* S : Src)
	{
		if (!S || !S->GetStaticMesh()) continue;
		for (int32 i = 0; i < S->GetInstanceCount(); ++i)
		{
			FTransform T; if (!S->GetInstanceTransform(i, T, true)) continue;
			UInstancedStaticMeshComponent* M = NewObject<UInstancedStaticMeshComponent>(this);
			M->SetMobility(EComponentMobility::Movable);
			M->SetStaticMesh(S->GetStaticMesh());
			const bool bScreen = ScreenLists.Contains(S);
			for (int32 m = 0; m < S->GetNumMaterials(); ++m) M->SetMaterial(m, bScreen && Lit ? Lit : S->GetMaterial(m));
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetGenerateOverlapEvents(false);
			M->SetCastShadow(Shadowed.Contains(S));   // (one part's shadow is plenty for a phone)
			M->bAffectDistanceFieldLighting = false;
			M->SetCullDistances(2500, 3500);           // (a phone in a hand is gone from sight well before 35 m)
			M->SetupAttachment(GetRootComponent());
			M->RegisterComponent();
			Lists.Add(M);
			K.Parts.Add({M, T.GetRelativeTransform(Frame)});
		}
	}
	Tpl->Destroy();
	K.bOk = K.Parts.Num() > 0;
	ADP_LOG(TEXT("PaxProps: the %s for the passengers - %d parts, one instanced list each"), KitModel[Kind], K.Parts.Num());
	return K.bOk;
}

// a slot nobody holds: half a millimetre big, under the floor - never size nothing: the renderer turns every copy's place
// round (distance fields), and a size of nothing can't be (the Matrix.h check in the log at loading, 2026-11-23)
static const FTransform PaxPropsNone(FQuat::Identity, FVector(0.0, 0.0, -300.0), FVector(0.0005));

int32 APaxProps::TakeSlot(int32 Kind)
{
	FKit& K = Kits[Kind];
	if (!MakeKit(Kind)) return INDEX_NONE;
	if (K.Free.Num()) return K.Free.Pop();
	const FTransform None = PaxPropsNone;
	for (FPart& P : K.Parts) P.List->AddInstance(None, false);
	return K.Slots++;
}

void APaxProps::PutSlot(int32 Kind, int32 Slot, const FTransform& At)
{
	for (FPart& P : Kits[Kind].Parts)
	{
		P.List->UpdateInstanceTransform(Slot, P.Rel * At, false, false, true);
		Dirty.Add(P.List.Get());
	}
}

void APaxProps::Dress(AActor* Passenger, USkeletalMeshComponent* Body, int32 Seed)
{
	if (!Passenger || !Body) return;
	APaxProps* M = Get(Passenger->GetWorld()); if (!M) return;
	FRandomStream R(Seed);
	int32 Ix;
	if (const int32* Have = M->ByActor.Find(Passenger)) Ix = *Have;
	else { Ix = M->FreeWho.Num() ? M->FreeWho.Pop() : M->Who.AddDefaulted(); M->Who[Ix] = FWho(); M->ByActor.Add(Passenger, Ix); }
	FWho& W = M->Who[Ix];
	W.A = Passenger; W.Body = Body;
	W.bTablet = R.FRand() < TabletShare;   // (and everybody a phone)
	// (backpacks: BackpackShare, earphones: EarphoneShare - 0 until to-do 204's models are made; draw them here then)
	const bool bBackpack = R.FRand() < BackpackShare, bEars = R.FRand() < EarphoneShare; (void)bBackpack; (void)bEars;
}

void APaxProps::Hold(AActor* Passenger, int32 Kind, bool bOn)
{
	if (!Passenger) return;
	APaxProps* M = Find(Passenger->GetWorld()); if (!M) return;
	const int32* Ix = M->ByActor.Find(Passenger); if (!Ix) return;
	FWho& W = M->Who[*Ix];
	if (Kind == KTablet && !W.bTablet) return;
	W.bWant[Kind] = bOn;
	if (bOn) W.bWant[1 - Kind] = false;   // (one device at a time)
	M->Anim(W);
}

void APaxProps::HoldPhone(AActor* Passenger, bool bOn) { if (APaxProps* M = Passenger ? Find(Passenger->GetWorld()) : nullptr) M->Hold(Passenger, KPhone, bOn); }
void APaxProps::HoldTablet(AActor* Passenger, bool bOn) { if (APaxProps* M = Passenger ? Find(Passenger->GetWorld()) : nullptr) M->Hold(Passenger, KTablet, bOn); }

bool APaxProps::HasTablet(const AActor* Passenger)
{
	const APaxProps* M = Passenger ? Find(Passenger->GetWorld()) : nullptr;
	const int32* Ix = M ? M->ByActor.Find(Passenger) : nullptr;
	return Ix && M->Who[*Ix].bTablet;
}

void APaxProps::GiveTablet(AActor* Passenger)
{
	APaxProps* M = Passenger ? Find(Passenger->GetWorld()) : nullptr;
	if (const int32* Ix = M ? M->ByActor.Find(Passenger) : nullptr) M->Who[*Ix].bTablet = true;
}

bool APaxProps::IsUsing(const AActor* Passenger)
{
	const APaxProps* M = Passenger ? Find(Passenger->GetWorld()) : nullptr;
	const int32* Ix = M ? M->ByActor.Find(Passenger) : nullptr;
	if (!Ix) return false;
	const FWho& W = M->Who[*Ix];
	return W.bWant[0] || W.bWant[1] || W.K[0] > 0.f || W.K[1] > 0.f;
}

bool APaxProps::UsingPoint(const AActor* Passenger, FVector& Out) const
{
	const int32* Ix = ByActor.Find(Passenger); if (!Ix) return false;
	const FWho& W = Who[*Ix];
	if (W.K[0] < 0.99f && W.K[1] < 0.99f) return false;
	Out = W.Center; return true;
}

void APaxProps::Forget(AActor* Passenger)
{
	APaxProps* M = Passenger ? Find(Passenger->GetWorld()) : nullptr; if (!M) return;
	int32 Ix; if (!M->ByActor.RemoveAndCopyValue(Passenger, Ix)) return;
	FWho& W = M->Who[Ix];
	const FTransform None = PaxPropsNone;
	for (int32 k = 0; k < 2; ++k) if (W.Slot[k] != INDEX_NONE) { M->PutSlot(k, W.Slot[k], None); M->Kits[k].Free.Add(W.Slot[k]); }
	W = FWho(); M->FreeWho.Add(Ix);
}

void APaxProps::Anim(FWho& W)
{
	USkeletalMeshComponent* B = W.Body.Get(); if (!B) return;
	UPaxAnimInstance* A = Cast<UPaxAnimInstance>(B->GetAnimInstance());
	if (!A && !B->IsRunningParallelEvaluation() && !B->IsPostEvaluatingAnimation()) A = UPaxAnimInstance::Install(B);
	const int32 Dev = W.bWant[KPhone] ? 1 : W.bWant[KTablet] ? 2 : 0;
	if (A) A->Device = Dev;
	// (sat: the seat's own pose holds it - AADPPeople)
	if (!PeopleW.IsValid()) PeopleW = AADPPeople::Get(GetWorld());
	if (AADPPeople* P = PeopleW.Get()) P->SeatDevice(W.A.Get(), Dev);
}

bool APaxProps::DeviceAt(const FWho& W, int32 Kind, float K, FTransform& Out) const
{
	const USkeletalMeshComponent* B = W.Body.Get(); if (!B) return false;
	const FHands& H = HandsOf(B); if (!H.bOk) return false;
	const FTransform BT = B->GetComponentTransform();
	const FVector Up = FVector::UpVector;
	const FVector Fwd = FVector::VectorPlaneProject(BT.TransformVectorNoScale(FVector(0.f, 1.f, 0.f)), Up).GetSafeNormal();   // (MetaHumans face their +y)
	FVector Rt = FVector::VectorPlaneProject(B->GetSocketLocation(TEXT("upperarm_r")) - B->GetSocketLocation(TEXT("upperarm_l")), Up).GetSafeNormal();   // (the body's right side)
	if (Rt.IsNearlyZero()) Rt = FVector::CrossProduct(Up, Fwd);
	// in the hands: standing, where the passengers' own animation holds it (UPaxAnimInstance's Phone / Tablet pose -
	// both hands at its sides, the head down at it: its frame X the screen's top, Z out of the screen); sat (the seat's
	// pose, AADPPeople::SeatDevice: both hands together in front) between the two palms, the screen up to the face.
	// The model's own frame: its top -y, its screen +z (Art/Smartphone/make_smartphone.py, Art/Tablet/make_tablet.py).
	FTransform Held; float Kw = PropEase(K);
	FTransform G; float Gw = 0.f;
	const UPaxAnimInstance* PA = Cast<UPaxAnimInstance>(B->GetAnimInstance());
	if (PA && PA->GetDeviceGrip(G, Gw))
	{
		Held = FTransform(FRotationMatrix::MakeFromYZ(-G.GetUnitAxis(EAxis::X), G.GetUnitAxis(EAxis::Z)).ToQuat(), G.GetLocation());
		Kw = FMath::Min(Kw, FMath::SmoothStep(0.3f, 1.f, Gw));   // (out of the pocket as the hands come in)
	}
	else
	{
		const FTransform Hr = B->GetSocketTransform(TEXT("hand_r")), Hl = B->GetSocketTransform(TEXT("hand_l"));
		const FVector Nr = Hr.TransformVectorNoScale(H.Palm[1]), Nl = Hl.TransformVectorNoScale(H.Palm[0]);
		const FVector Mid0 = (Hr.GetLocation() + Hr.TransformVectorNoScale(H.Along[1]) * 5.f + Hl.GetLocation() + Hl.TransformVectorNoScale(H.Along[0]) * 5.f) * 0.5f;
		const FVector Eye = B->GetSocketLocation(TEXT("head")) + Fwd * 8.f;
		FVector N = ((Nr + Nl).GetSafeNormal() + (Eye - Mid0).GetSafeNormal()).GetSafeNormal(); if (N.IsNearlyZero()) N = Up;
		FVector Top = FVector::VectorPlaneProject(Fwd, N).GetSafeNormal();
		if (Top.IsNearlyZero()) Top = FVector::VectorPlaneProject(Up, N).GetSafeNormal();
		Held = FTransform(FRotationMatrix::MakeFromYZ(-Top, N).ToQuat(), Mid0 + N * 2.f);
	}
	const FVector Mid = Held.GetLocation();
	W.Center = Mid;
	// put away: the phone upright in the right front trouser pocket, the tablet down at the left side
	FTransform Away;
	if (Kind == KPhone) Away = FTransform(FRotationMatrix::MakeFromYZ(-Up, Rt).ToQuat(), B->GetSocketLocation(TEXT("thigh_r")) + Rt * 9.f + Fwd * 4.f - Up * 12.f);
	else Away = FTransform(FRotationMatrix::MakeFromYZ(-Fwd, -Rt).ToQuat(), B->GetSocketLocation(TEXT("pelvis")) - Rt * 22.f);
	Out.Blend(Away, Held, Kw);
	return true;
}

void APaxProps::Tick(float Dt)
{
	Super::Tick(Dt);
	const FTransform None = PaxPropsNone;
	if (!PeopleW.IsValid()) PeopleW = AADPPeople::Get(GetWorld());
	AADPPeople* People = PeopleW.Get();
	for (int32 i = 0; i < Who.Num(); ++i)
	{
		FWho& W = Who[i];
		if (!W.A.IsValid())
		{
			// (gone without a word: its devices go too)
			if (W.Slot[0] == INDEX_NONE && W.Slot[1] == INDEX_NONE && !W.Body.IsValid()) continue;
			for (int32 k = 0; k < 2; ++k) if (W.Slot[k] != INDEX_NONE) { PutSlot(k, W.Slot[k], None); Kits[k].Free.Add(W.Slot[k]); }
			for (auto It = ByActor.CreateIterator(); It; ++It) if (It.Value() == i) It.RemoveCurrent();
			W = FWho(); FreeWho.Add(i);
			continue;
		}
		// (the idle movements' own phone / tablet check - UPaxAnimInstance's, or the seated one in AADPPeople - shows the
		//  owner's device in the hands too, never empty hands)
		bool bAuto[2] = {false, false};
		if (!W.bWant[0] && !W.bWant[1])
		{
			const USkeletalMeshComponent* B = W.Body.Get();
			const UPaxAnimInstance* PA = B ? Cast<UPaxAnimInstance>(B->GetAnimInstance()) : nullptr;
			int32 Fk = PA ? PA->CurrentFidget() : int32(EPaxFidget::None);
			if (!PA && People) { const int32 Sd = People->SeatDeviceNow(W.A.Get()); Fk = Sd == 1 ? int32(EPaxFidget::Phone) : Sd == 2 ? int32(EPaxFidget::Tablet) : Fk; }
			bAuto[0] = Fk == EPaxFidget::Phone;   // (a tablet movement by someone with none: empty hands, rare)
			bAuto[1] = Fk == EPaxFidget::Tablet && W.bTablet;
		}
		for (int32 k = 0; k < 2; ++k)
		{
			const bool bOut = W.bWant[k] || bAuto[k];
			if (!bOut && W.K[k] <= 0.f) continue;
			W.K[k] = FMath::Clamp(W.K[k] + (bOut ? Dt : -Dt) / PropLiftS, 0.f, 1.f);
			if (W.K[k] <= 0.f)
			{
				// (back in the pocket: not drawn, its instances free for the next one)
				if (W.Slot[k] != INDEX_NONE) { PutSlot(k, W.Slot[k], None); Kits[k].Free.Add(W.Slot[k]); W.Slot[k] = INDEX_NONE; }
				continue;
			}
			if (W.Slot[k] == INDEX_NONE) W.Slot[k] = TakeSlot(k);
			if (W.Slot[k] == INDEX_NONE) { W.bWant[k] = false; W.K[k] = 0.f; continue; }
			FTransform At;
			// (a device only just out of the pocket is still in it: drawn from a fifth of the way)
			if (W.K[k] < 0.2f || !DeviceAt(W, k, W.K[k], At) || At.GetScale3D().IsNearlyZero()) At = None;
			PutSlot(k, W.Slot[k], At);
		}
	}
	for (UInstancedStaticMeshComponent* L : Dirty) if (L) L->MarkRenderStateDirty();
	Dirty.Reset();
}
