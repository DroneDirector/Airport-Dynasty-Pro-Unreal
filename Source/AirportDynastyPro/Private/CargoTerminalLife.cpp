#include "CargoTerminalLife.h"
#include "ADPKeep.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "PerfLog.h"
#include "Puddles.h"
#include "Shapes.h"
#include "TerminalBuilder.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

ACargoTerminalLife::ACargoTerminalLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;   // (rain and the printer: twice a second is plenty)
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

USoundAttenuation* ACargoTerminalLife::Heard(float FalloffCm)
{
	static TMap<int32, USoundAttenuation*> Cache;
	const int32 Key = FMath::Max(1, FMath::RoundToInt(FalloffCm / 100.f));
	if (USoundAttenuation** F = Cache.Find(Key)) return *F;
	USoundAttenuation* A = ADPKeep(NewObject<USoundAttenuation>(GetTransientPackage()));
	FSoundAttenuationSettings& S = A->Attenuation;
	S.bAttenuate = true; S.bSpatialize = true; S.AttenuationShapeExtents = FVector(150.f); S.FalloffDistance = Key * 100.f;
	S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; S.dBAttenuationAtMax = -60.f;
	// heard from where you stand: through walls muffled (the sound placement rule)
	S.bEnableOcclusion = true; S.OcclusionTraceChannel = ECC_Visibility; S.OcclusionLowPassFilterFrequency = 900.f; S.OcclusionVolumeAttenuation = 0.35f; S.OcclusionInterpolationTime = 0.15f;
	Cache.Add(Key, A);
	return A;
}

USoundBase* ACargoTerminalLife::Keep(const TCHAR* Path)
{
	static TMap<FString, USoundBase*> Cache;
	if (USoundBase** F = Cache.Find(Path)) return *F;
	USoundBase* S = ADPKeep(LoadObject<USoundBase>(nullptr, Path));
	if (!S) ADP_WARN(TEXT("Cargo terminal: sound %s missing"), Path);
	Cache.Add(Path, S);
	return S;
}

ACargoTerminalLife* ACargoTerminalLife::Make(ABabylonModel* Model)
{
	if (!Model || !Model->GetWorld() || !Model->GetModelRoot()) return nullptr;
	FString P = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/cargo-lights.json"));
	if (!FPaths::FileExists(P)) P = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/cargo-lights.json"));
	FString Text; TSharedPtr<FJsonObject> J;
	if (!FFileHelper::LoadFileToString(Text, *P) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J)
	{
		ADP_WARN(TEXT("Cargo terminal: no lights and sounds (%s)"), *P);
		return nullptr;
	}
	USceneComponent* MR = Model->GetModelRoot();
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACargoTerminalLife* A = Model->GetWorld()->SpawnActor<ACargoTerminalLife>(MR->GetComponentLocation(), MR->GetComponentRotation(), SP);
	if (!A) return nullptr;
	A->AttachToComponent(MR, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	const FTransform T = MR->GetComponentTransform();
	auto Loc = [](double X, double Y, double Z) { return FVector(X, -Y, Z) * 100.0; };   // (model-local Blender metres -> the model's frame, cm)
	auto Num = [](const TSharedPtr<FJsonObject>& O, const TCHAR* F) { return O->GetNumberField(F); };

	// fittings: steel housings, wires and poles, glowing lamp faces (one instanced list each)
	auto List = [&](const TCHAR* Name, UStaticMesh* Mesh, const FName Mat, bool bCollide)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(A, Name);
		C->SetupAttachment(A->GetRootComponent());
		C->SetStaticMesh(Mesh);
		C->SetMaterial(0, ATerminalBuilder::KitMaterial(A, Mat));
		if (bCollide) C->SetCollisionProfileName(TEXT("BlockAll"));
		else C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(bCollide);   // (small fittings high up: no shadows; the masts' poles cast theirs)
		ADPShapes::NoNaniteIfSeeThrough(C);
		C->RegisterComponent();
		A->AddInstanceComponent(C);
		return C;
	};
	UInstancedStaticMeshComponent* Steel = List(TEXT("CargoLampSteel"), ADPShapes::Cube(), TEXT("steel"), false);
	UInstancedStaticMeshComponent* Face = List(TEXT("CargoLampFace"), ADPShapes::Cube(), TEXT("lamp"), false);
	UInstancedStaticMeshComponent* Pole = List(TEXT("CargoMastPole"), ADPShapes::Cylinder(), TEXT("steel"), true);
	TArray<FTransform> SteelT, FaceT, PoleT;
	auto Box = [](TArray<FTransform>& To, const FVector& Mid, const FVector& SizeM) { To.Emplace(FQuat::Identity, Mid, SizeM); };
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	int32 NFit = 0;
	if (J->TryGetArrayField(TEXT("fittings"), Arr))
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O) continue;
			const FString K = O->GetStringField(TEXT("kind"));
			const double X = Num(O, TEXT("x")), Y = Num(O, TEXT("y")), Z = Num(O, TEXT("z"));
			if (K == TEXT("highbay"))
			{
				// a twin-strip LED high-bay: 1.2 x 0.4 m housing, its lit strip set into the underside, two wires up into the roof
				Box(SteelT, Loc(X, Y, Z), FVector(1.2, 0.4, 0.12));
				Box(FaceT, Loc(X, Y, Z - 0.05), FVector(1.1, 0.3, 0.05));
				const double Lo = Z + 0.05, Hi = Num(O, TEXT("top")) + 0.05;
				for (const double Dx : {-0.5, 0.5}) Box(SteelT, Loc(X + Dx, Y, (Lo + Hi) / 2), FVector(0.015, 0.015, Hi - Lo));
			}
			else if (K == TEXT("panel")) Box(FaceT, Loc(X, Y, Z - 0.01), FVector(0.6, 0.6, 0.04));   // (its top set into the ceiling)
			else if (K == TEXT("canopy"))
			{
				Box(SteelT, Loc(X, Y, Z - 0.035), FVector(0.5, 0.5, 0.09));
				Box(FaceT, Loc(X, Y, Z - 0.085), FVector(0.4, 0.4, 0.03));
			}
			else if (K == TEXT("dock"))
			{
				Box(SteelT, Loc(X, Y, Z), FVector(0.3, 0.22, 0.22));
				Box(FaceT, Loc(X, Y - 0.1, Z), FVector(0.22, 0.04, 0.16));
			}
			else if (K == TEXT("mast"))
			{
				// a 16 m galvanised mast, a cross-arm and two floods facing the court
				PoleT.Emplace(FQuat::Identity, Loc(X, Y, Z / 2), FVector(0.3, 0.3, Z));
				Box(SteelT, Loc(X, Y, Z - 0.2), FVector(2.4, 0.15, 0.15));
				for (const double Dx : {-0.9, 0.9})
				{
					Box(SteelT, Loc(X + Dx, Y - 0.3, Z - 0.4), FVector(0.7, 0.35, 0.45));
					Box(FaceT, Loc(X + Dx, Y - 0.47, Z - 0.4), FVector(0.6, 0.04, 0.35));
				}
			}
			else continue;
			++NFit;
		}
	Steel->AddInstances(SteelT, false); Face->AddInstances(FaceT, false); Pole->AddInstances(PoleT, false);

	// lights: the model's own lamps (no shadows, no flares, a reach each, dimmed by day)
	int32 NLight = 0;
	const FLinearColor Led(1.f, 0.97f, 0.92f);
	if (J->TryGetArrayField(TEXT("lights"), Arr))
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O) continue;
			const FVector At = Loc(Num(O, TEXT("x")), Num(O, TEXT("y")), Num(O, TEXT("z")));
			const float Cd = Num(O, TEXT("cd")), Reach = Num(O, TEXT("reach")) * 100.f;
			ULocalLightComponent* L = nullptr;
			if (O->GetStringField(TEXT("kind")) == TEXT("rect"))
			{
				URectLightComponent* R = NewObject<URectLightComponent>(Model);
				R->SetSourceWidth(Num(O, TEXT("w")) * 100.f);
				R->SetSourceHeight(Num(O, TEXT("d")) * 100.f);
				R->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));   // (facing down)
				L = R;
			}
			else
			{
				USpotLightComponent* S = NewObject<USpotLightComponent>(Model);
				const TArray<TSharedPtr<FJsonValue>>& Aim = O->GetArrayField(TEXT("aim"));
				const FVector Dir(Aim[0]->AsNumber(), -Aim[1]->AsNumber(), Aim[2]->AsNumber());
				S->SetRelativeRotation(Dir.GetSafeNormal().Rotation());
				const float Cone = Num(O, TEXT("cone"));
				S->SetOuterConeAngle(Cone);
				S->SetInnerConeAngle(Cone * 0.45f);
				S->SetSourceRadius(20.f);
				L = S;
			}
			Model->AddLamp(L, At, Cd, Reach, Led);
			++NLight;
		}

	// sounds: looping where they happen (the rain ones only while it rains)
	if (J->TryGetArrayField(TEXT("sounds"), Arr))
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O) continue;
			const FString N = O->GetStringField(TEXT("sound"));
			USoundBase* S = Keep(*FString::Printf(TEXT("/Game/CargoTerminal/Sounds/%s.%s"), *N, *N));
			if (!S) continue;
			UAudioComponent* C = NewObject<UAudioComponent>(A);
			C->SetupAttachment(A->GetRootComponent());
			C->SetRelativeLocation(Loc(Num(O, TEXT("x")), Num(O, TEXT("y")), Num(O, TEXT("z"))));
			C->SetSound(S);
			C->bAutoActivate = false;
			C->bAutoDestroy = false;
			C->bOverrideAttenuation = false;
			C->AttenuationSettings = Heard(Num(O, TEXT("falloff")) * 100.f);
			C->RegisterComponent();
			A->Audio.Add(C);
			FLoop Lp; Lp.C = C; Lp.Vol = Num(O, TEXT("vol"));
			const FString R = O->GetStringField(TEXT("rain"));
			Lp.Rain = R == TEXT("in") ? 1 : R == TEXT("out") ? 2 : 0;
			if (!Lp.Rain) { C->SetVolumeMultiplier(Lp.Vol); C->Play(FMath::FRand() * 20.f); }
			A->Loops.Add(Lp);
		}
	if (J->TryGetArrayField(TEXT("printers"), Arr))
		for (const TSharedPtr<FJsonValue>& V : *Arr)
			if (const TSharedPtr<FJsonObject> O = V->AsObject())
				A->Printers.Add(T.TransformPosition(Loc(Num(O, TEXT("x")), Num(O, TEXT("y")), Num(O, TEXT("z")))));
	A->NextPrint = FMath::FRandRange(40.f, 120.f);
	ADP_LOG(TEXT("Cargo terminal: %d light fittings, %d lights (the lamp system), %d sounds"), NFit, NLight, A->Loops.Num());
	return A;
}

void ACargoTerminalLife::Tick(float Dt)
{
	Super::Tick(Dt);
	ADP_PERF_SCOPE("CargoTerminalLife", 0.2f);
	// rain on the metal roof and canopies: as loud as it rains, silent when dry or snowing
	const APuddles* Pd = APuddles::Get();
	const float Rain = Pd ? Pd->GetRain() : 0.f;
	for (FLoop& L : Loops)
	{
		if (!L.Rain || !L.C) continue;
		const float Vol = L.Vol * FMath::Clamp(Rain * 1.2f, 0.f, 1.f);
		if (Vol < 0.02f) { if (L.bOn) L.C->FadeOut(2.f, 0.f); L.bOn = false; continue; }
		if (!L.bOn) { L.C->FadeIn(2.f, Vol, FMath::FRand() * 10.f); L.bOn = true; }
		else L.C->SetVolumeMultiplier(Vol);
	}
	// now and then a printer in the offices (on one of the floors)
	NextPrint -= Dt;
	if (NextPrint <= 0.f && Printers.Num())
	{
		NextPrint = FMath::FRandRange(60.f, 180.f);
		if (USoundBase* S = Keep(TEXT("/Game/CargoTerminal/Sounds/CgPrinter.CgPrinter")))
			UGameplayStatics::SpawnSoundAtLocation(this, S, Printers[FMath::RandRange(0, Printers.Num() - 1)], FRotator::ZeroRotator, 0.5f, 1.f, 0.f, Heard(1500.f));
	}
}
