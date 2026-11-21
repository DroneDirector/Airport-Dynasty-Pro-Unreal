#include "Tugs.h"
#include "ADPLog.h"
#include "ADPKeep.h"
#include "BabylonModel.h"
#include "Airfield.h"
#include "TerminalPlan.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"

namespace
{
	// the tug's own measures (Tools/make_pushback_tug.py, metres): its middle to the towbar head's pin, the pin's height
	constexpr float HeadAhead = 2.5525f + 0.12f + 4.35f;    // L/2 + hitch + bar
	enum ELensKind : uint8 { Low, Tail, Marker, Reverse, Beacon, Indicator };
	USoundAttenuation* Att()
	{
		static USoundAttenuation* A = nullptr;
		if (!A)
		{
			A = ADPKeep(NewObject<USoundAttenuation>(GetTransientPackage()));
			FSoundAttenuationSettings& S = A->Attenuation;
			S.bAttenuate = true; S.bSpatialize = true; S.AttenuationShapeExtents = FVector(200.f); S.FalloffDistance = 6000.f;
			S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; S.dBAttenuationAtMax = -60.f;
			// heard from where you stand: through walls and the terminal muffled (the sound placement rule)
			S.bEnableOcclusion = true; S.OcclusionTraceChannel = ECC_Visibility; S.OcclusionLowPassFilterFrequency = 900.f; S.OcclusionVolumeAttenuation = 0.35f; S.OcclusionInterpolationTime = 0.15f;
		}
		return A;
	}
	USoundBase* Snd(const TCHAR* Name)
	{
		static TMap<FString, USoundBase*> Cache;
		if (USoundBase** F = Cache.Find(Name)) return *F;
		USoundBase* S = ADPKeep(LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/PushbackTug/Sounds/%s.%s"), Name, Name)));
		Cache.Add(Name, S); return S;
	}
}

ATugs::ATugs()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

ATugs* ATugs::Get(UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<ATugs> It(W); It; ++It) return *It;
	return W->SpawnActor<ATugs>(FVector::ZeroVector, FRotator::ZeroRotator);
}

bool ATugs::Spawn(FJob& J)
{
	ABabylonModel* M = GetWorld()->SpawnActor<ABabylonModel>(J.At, FRotator::ZeroRotator);
	if (!M || !M->Place(TEXT("PushbackTug"))) { if (M) M->Destroy(); ADP_WARN(TEXT("Tugs: the tug model is missing (Tools/import_model.py ... PushbackTug)")); return false; }
	M->SetPartsCollision(false);
	J.M = M;
	// its windows: the fire truck's glass; its lenses: the fire truck's lens material, one glow value per lens
	UMaterialInterface* Glass = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_TruckGlass.M_TruckGlass")));
	UMaterialInterface* LensM = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_TruckLens.M_TruckLens")));
	TArray<UInstancedStaticMeshComponent*> Parts; M->Parts(Parts);
	TMap<UMaterialInterface*, UMaterialInterface*> Swap;
	for (UInstancedStaticMeshComponent* K : Parts)
		for (int32 Si = 0; Si < K->GetNumMaterials(); ++Si)
		{
			UMaterialInterface* Mi = K->GetMaterial(Si); if (!Mi) continue;
			if (UMaterialInterface** F = Swap.Find(Mi)) { if (*F) K->SetMaterial(Si, *F); continue; }
			const UMaterialInstanceDynamic* Md = Cast<UMaterialInstanceDynamic>(Mi);
			const FString N = Md && Md->Parent ? Md->Parent->GetName() : Mi->GetName();
			UMaterialInstanceDynamic* D = nullptr;
			if (N.Contains(TEXT("Tug_Glass")) && Glass)
			{
				D = UMaterialInstanceDynamic::Create(Glass, this); D->SetVectorParameterValue(TEXT("BC"), FLinearColor(0.015f, 0.022f, 0.026f, 0.38f));
			}
			else if (LensM && (N.Contains(TEXT("Tug_HeadLens")) || N.Contains(TEXT("Tug_RedLens")) || N.Contains(TEXT("Tug_BeaconLens"))))
			{
				D = UMaterialInstanceDynamic::Create(LensM, this);
				const bool bRed = N.Contains(TEXT("Red")), bAmber = N.Contains(TEXT("Beacon"));
				D->SetVectorParameterValue(TEXT("BC"), bRed ? FLinearColor(1.f, 0.04f, 0.02f) : bAmber ? FLinearColor(1.f, 0.45f, 0.03f) : FLinearColor(1.f, 0.97f, 0.9f));
				D->SetScalarParameterValue(TEXT("Glow"), bAmber ? 60.f : bRed ? 30.f : 28.f); D->SetScalarParameterValue(TEXT("Dots"), 4.f);
				K->SetNumCustomDataFloats(1);
			}
			Swap.Add(Mi, D); if (D) K->SetMaterial(Si, D);
		}
	// which lens is which lamp (its node's name, Tools/make_pushback_tug.py)
	struct FW { const TCHAR* Name; ELensKind Kind; };
	static const FW Which[] = {{TEXT("LowBeam_"), Low}, {TEXT("TailBrake_"), Tail}, {TEXT("SideMarker_"), Marker}, {TEXT("Reverse_"), Reverse},
		{TEXT("BeaconLens"), Beacon}, {TEXT("FlasherRear_"), Beacon}, {TEXT("IndicatorF_"), Indicator}, {TEXT("IndicatorR_"), Indicator}};
	for (const FW& Wh : Which)
	{
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Found; M->InstancesNamed(Wh.Name, true, Found);
		for (const auto& F : Found)
			if (F.Get<2>().EndsWith(TEXT("_Lens")) || F.Get<2>() == TEXT("BeaconLens")) { FLens L; L.K = F.Get<0>(); L.Ix = F.Get<1>(); L.Kind = Wh.Kind; J.Lenses.Add(L); }
	}
	// the engine: started as it hooks on, idling under the pushback
	J.Engine = UGameplayStatics::SpawnSoundAttached(Snd(TEXT("TugIdle")), M->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, 0.9f, 1.f, 0.f, Att(), nullptr, false);
	Sound(TEXT("TugStart"), J.At);
	ADP_LOG(TEXT("Tugs: a tug at work (%d lenses)"), J.Lenses.Num());
	return true;
}

void ATugs::Sound(const TCHAR* Name, const FVector& At)
{
	if (USoundBase* S = Snd(Name)) UGameplayStatics::PlaySoundAtLocation(this, S, At, 1.f, 1.f, 0.f, Att());
}

void ATugs::PoseTug(FJob& J)
{
	ABabylonModel* M = J.M.Get(); if (!M) return;
	// the model's front (Blender +x) is the root's +X; Place turns the root half round on the actor - so the actor faces
	// the aircraft's nose direction for the tug's front to face the aircraft
	M->SetActorLocationAndRotation(J.At, FRotator(0.f, J.Fwd.Rotation().Yaw, 0.f));
}

void ATugs::Lamps(FJob& J, bool bReverse)
{
	// the beacons flash as the jetway beacons do: two short flashes a second
	const float T = FMath::Fmod(Flash, 1.f);
	const bool bFlash = T < 0.08f || (T > 0.18f && T < 0.26f);
	for (FLens& L : J.Lenses)
	{
		float V = 0.f;
		switch (L.Kind)
		{
		case Low: V = 0.8f; break;
		case Tail: V = 0.3f; break;
		case Marker: V = 0.5f; break;
		case Reverse: V = bReverse ? 1.f : 0.f; break;
		case Beacon: V = bFlash ? 1.f : 0.f; break;
		default: V = 0.f;
		}
		if (V != L.Now && L.K) { L.Now = V; L.K->SetCustomDataValue(L.Ix, 0, V, true); }
	}
}

void ATugs::Push(const FString& Key, const FVector& Fitting, const FVector& Fwd, float Speed)
{
	FJob& J = Jobs.FindOrAdd(Key);
	J.Fwd = Fwd.GetSafeNormal2D(); J.Speed = Speed;
	// the towbar's head pin on the tow fitting; the tug its length ahead, facing the aircraft
	J.At = Fitting + J.Fwd * HeadAhead * 100.f;            // (Fitting is given on the ground: the apron is flat round a stand)
	if (!J.M.IsValid() && !Spawn(J)) { Jobs.Remove(Key); return; }
	if (!J.bHooked) { J.bHooked = true; Sound(TEXT("TugHitchClunk"), Fitting); }
	PoseTug(J);
}

void ATugs::Release(const FString& Key)
{
	FJob* J = Jobs.Find(Key);
	if (!J || J->Leave >= 0.f) return;
	J->Leave = 0.f;
	if (J->M.IsValid()) Sound(TEXT("TugHitchClunk"), J->At - J->Fwd * HeadAhead * 100.f);
	// it backs away from the aircraft: the reversing beeper and lamps
	if (J->M.IsValid()) J->Beep = UGameplayStatics::SpawnSoundAttached(Snd(TEXT("TugReverseBeep")), J->M->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, 0.7f, 1.f, 0.f, Att(), nullptr, false);
}

void ATugs::Tick(float Dt)
{
	Super::Tick(Dt);
	Flash += Dt;
	TArray<FString> Gone;
	for (auto& It : Jobs)
	{
		FJob& J = It.Value;
		if (!J.M.IsValid()) { Gone.Add(It.Key); continue; }
		if (J.Leave >= 0.f)
		{
			// let go: 4 s still (unhooking), then backs away 20 m at walking pace, waits, stops its engine and goes
			J.Leave += Dt;
			if (J.Leave > 4.f && J.Leave < 18.f) { J.At += J.Fwd * 150.f * Dt; PoseTug(J); }
			if (J.Leave >= 18.f && J.Beep) { J.Beep->Stop(); J.Beep = nullptr; }
			if (J.Leave >= 24.f && J.Engine) { J.Engine->Stop(); J.Engine = nullptr; Sound(TEXT("TugStop"), J.At); }
			Lamps(J, J.Leave > 4.f && J.Leave < 18.f);
			if (J.Leave > 60.f) { J.M->Destroy(); Gone.Add(It.Key); }
			continue;
		}
		Lamps(J, false);
	}
	for (const FString& K : Gone) Jobs.Remove(K);
}
