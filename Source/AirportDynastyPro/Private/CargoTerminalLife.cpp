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
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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
		if (!Mat.IsNone()) for (int32 m = 0; m < Mesh->GetStaticMaterials().Num(); ++m) C->SetMaterial(m, ATerminalBuilder::KitMaterial(A, Mat));
		if (bCollide) C->SetCollisionProfileName(TEXT("BlockAll"));
		else C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(bCollide);   // (small fittings high up: no shadows; the masts' poles cast theirs)
		ADPShapes::NoNaniteIfSeeThrough(C);
		C->RegisterComponent();
		A->AddInstanceComponent(C);
		return C;
	};
	// the fittings themselves (Art/CargoTerminal/make_cargo_lamps.py, imported as CargoLamps): each a housing with its
	// reflector (its own materials) and its lens, which gets the lamp system's glowing face; poles, wires and the masts'
	// cross-arms are plain steel
	auto Mesh = [](const TCHAR* N) { return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Babylon/CargoLamps/CargoLamps/StaticMeshes/%s.%s"), N, N)); };
	struct FKind { UStaticMesh* Body; UStaticMesh* Lens; UInstancedStaticMeshComponent* B = nullptr; UInstancedStaticMeshComponent* L = nullptr; TArray<FTransform> BT, LT; };
	FKind Kinds[4] = {{Mesh(TEXT("SM_CargoLamp_HighBay")), Mesh(TEXT("SM_CargoLamp_HighBayLens"))}, {Mesh(TEXT("SM_CargoLamp_Panel")), Mesh(TEXT("SM_CargoLamp_PanelLens"))},
	                  {Mesh(TEXT("SM_CargoLamp_Down")), Mesh(TEXT("SM_CargoLamp_DownLens"))}, {Mesh(TEXT("SM_CargoLamp_Flood")), Mesh(TEXT("SM_CargoLamp_FloodLens"))}};
	enum { HighBay, Panel, Down, Flood };
	for (int32 k = 0; k < 4; ++k)
	{
		if (!Kinds[k].Body || !Kinds[k].Lens) { ADP_WARN(TEXT("Cargo terminal: lamp fitting meshes missing (CargoLamps, kind %d) - none of that kind drawn"), k); continue; }
		Kinds[k].B = List(*FString::Printf(TEXT("CargoLampBody%d"), k), Kinds[k].Body, NAME_None, false);
		Kinds[k].L = List(*FString::Printf(TEXT("CargoLampLens%d"), k), Kinds[k].Lens, TEXT("lamp"), false);
	}
	// the floodlight head faces Blender -y; which way the import laid it is read from where its lens sits
	FQuat FloodFace = FQuat::Identity;
	if (Kinds[Flood].Lens)
	{
		FVector C = Kinds[Flood].Lens->GetBoundingBox().GetCenter(); C.Z = 0.0;
		if (!C.IsNearlyZero()) FloodFace = FQuat::FindBetweenNormals(C.GetSafeNormal(), FVector(0, 1, 0));
	}
	// a head facing (Blender) direction Dir in the plan, tipped down by TiltDeg, at Scale
	auto Head = [&](const FVector& At, const FVector& DirBlender, float TiltDeg, float Scale)
	{
		const FVector D = FVector(DirBlender.X, -DirBlender.Y, 0).GetSafeNormal();
		const FQuat Yaw = FQuat::FindBetweenNormals(FVector(0, 1, 0), D);
		const FQuat Q = FQuat(FVector::CrossProduct(D, FVector::UpVector).GetSafeNormal(), FMath::DegreesToRadians(-TiltDeg)) * Yaw * FloodFace;
		Kinds[Flood].BT.Emplace(Q, At, FVector(Scale)); Kinds[Flood].LT.Emplace(Q, At, FVector(Scale));
	};
	auto Put = [&](int32 K, const FVector& At) { Kinds[K].BT.Emplace(FQuat::Identity, At); Kinds[K].LT.Emplace(FQuat::Identity, At); };
	UInstancedStaticMeshComponent* Steel = List(TEXT("CargoLampSteel"), ADPShapes::Cube(), TEXT("steel"), false);
	UInstancedStaticMeshComponent* Pole = List(TEXT("CargoMastPole"), ADPShapes::Cylinder(), TEXT("steel"), true);
	TArray<FTransform> SteelT, PoleT;
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
				// an LED high-bay bell, its rim 6 cm under the fitting's height, on one wire up into the roof
				Put(HighBay, Loc(X, Y, Z - 0.06));
				const double Lo = Z - 0.06 + 0.458, Hi = Num(O, TEXT("top")) + 0.05;
				Box(SteelT, Loc(X, Y, (Lo + Hi) / 2), FVector(0.012, 0.012, Hi - Lo));
			}
			else if (K == TEXT("panel")) Put(Panel, Loc(X, Y, Z));   // (the ceiling's plane: the flange under it, the body above)
			else if (K == TEXT("canopy")) Put(Down, Loc(X, Y, Z));   // (the canopy's underside)
			else if (K == TEXT("dock"))
			{
				// a small floodlight on a bracket off the landside wall, aimed out and down over the dock
				Head(Loc(X, Y + 0.25, Z), FVector(-0.35, 1.0, 0), 14.f, 0.5f);
				Box(SteelT, Loc(X, Y + 0.13, Z + 0.16), FVector(0.05, 0.32, 0.03));
			}
			else if (K == TEXT("mast"))
			{
				// a 16 m galvanised mast, a cross-arm and two floodlight heads aimed back at the court
				PoleT.Emplace(FQuat::Identity, Loc(X, Y, Z / 2), FVector(0.3, 0.3, Z));
				Box(SteelT, Loc(X, Y, Z - 0.2), FVector(2.4, 0.15, 0.15));
				for (const double Dx : {-0.9, 0.9}) Head(Loc(X + Dx, Y + 0.13, Z - 0.57), FVector(0, -1, 0), 24.f, 1.f);   // (the yoke tipped under the arm)
			}
			else continue;
			++NFit;
		}
	Steel->AddInstances(SteelT, false); Pole->AddInstances(PoleT, false);
	for (FKind& Kd : Kinds) if (Kd.B) { Kd.B->AddInstances(Kd.BT, false); Kd.L->AddInstances(Kd.LT, false); }

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
	// the gate barrier: the boom turns about its hinge on the cabinet (Blender x 161.5, y fence - 0.85, 0.95 m up; the
	// boom lies along +x, so up is a turn about the model's y)
	A->Model = Model;
	A->BoomPivot = FTransform(Loc(161.5, 210.0 - 0.85, 0.95));
	A->BoomGroup = Model->BindGroup({TEXT("GateBoom")}, false, A->BoomPivot);
	A->GateAt = T.TransformPosition(Loc(163.0, 210.0, 0.0));
	if (A->BoomGroup == INDEX_NONE) ADP_WARN(TEXT("Cargo terminal: no GateBoom part - the gate barrier stays down"));
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
	TickBoom(Dt);
}

bool ACargoTerminalLife::SomethingAtGate() const
{
	// anyone or any vehicle within 12 m of the gate, either side of the fence (the player found by distance as well, in
	// case the pawn has no collision while walking or flying the camera)
	if (const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
		if (FVector::Dist2D(P->GetActorLocation(), GateAt) < 1200.f) return true;
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_Pawn); Obj.AddObjectTypesToQuery(ECC_PhysicsBody); Obj.AddObjectTypesToQuery(ECC_Vehicle);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CargoGate), false, this);
	return GetWorld()->OverlapAnyTestByObjectType(GateAt + FVector(0.f, 0.f, 200.f), FQuat::Identity, Obj, FCollisionShape::MakeBox(FVector(1200.f, 1200.f, 200.f)), Q);
}

void ACargoTerminalLife::TickBoom(float Dt)
{
	ABabylonModel* M = Model.Get();
	if (!M || BoomGroup == INDEX_NONE) return;
	// up while something is near; down 8 s after the gate is clear (never down on anything: it only comes down when clear)
	const bool bNear = SomethingAtGate();
	GateClear = bNear ? 0.f : GateClear + Dt;
	if (bNear) BoomWant = 1.f;
	else if (GateClear > 8.f) BoomWant = 0.f;
	if (bNear && BoomWant < BoomK) BoomWant = 1.f;
	if (FMath::IsNearlyEqual(BoomK, BoomWant, 0.0005f))
	{
		if (BoomDir && IsValid(BoomHum)) BoomHum->FadeOut(0.2f, 0.f);
		BoomDir = 0;
		SetActorTickInterval(0.5f);
		return;
	}
	SetActorTickInterval(0.f);   // (smooth while it swings; back to twice a second at rest)
	const int8 NowDir = BoomWant > BoomK ? 1 : -1;
	if (NowDir != BoomDir)
	{
		// the barrier's motor (the jetway's drive motor, quieter and a little higher: a small gear motor)
		static USoundBase* S = Keep(TEXT("/Game/JetwayBeacon/Sounds/JwMotor.JwMotor"));
		if (IsValid(BoomHum)) BoomHum->FadeOut(0.1f, 0.f);
		BoomHum = S ? UGameplayStatics::SpawnSoundAtLocation(this, S, GateAt + FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, 0.45f, 1.35f, 0.f, Heard(2500.f), nullptr, true) : nullptr;
		BoomDir = NowDir;
	}
	// 3 s up or down, eased at both ends, to 85 degrees
	BoomK = FMath::FInterpConstantTo(BoomK, BoomWant, Dt, 1.f / 3.f);
	const float Deg = 85.f * FMath::InterpEaseInOut(0.f, 1.f, BoomK, 2.f);
	M->MoveGroup(BoomGroup, FTransform(FRotator(Deg, 0.f, 0.f).Quaternion(), BoomPivot.GetLocation()));
	M->FlushMoves();
}
