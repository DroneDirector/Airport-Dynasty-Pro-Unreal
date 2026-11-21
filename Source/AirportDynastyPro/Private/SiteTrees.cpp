// The real trees round the airport - see SiteTrees.h.
#include "SiteTrees.h"
#include "ADPLog.h"
#include "Site.h"
#include "SunClock.h"
#include "Puddles.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

float ASiteTrees::TreeRadiusKm = 4.f;

namespace
{
	const TCHAR* TreeNames[] = {TEXT("Spruce_A"), TEXT("Spruce_B"), TEXT("Fir_A"), TEXT("Beech_A"), TEXT("BeechBare_A")};
	const float BlockM = 1000.f;
	/** A mesh imported from Art/Trees (Content/Babylon/Trees...), found by its name. */
	UStaticMesh* FindTree(const TCHAR* Name)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> In; AR.GetAssetsByPath(FName(TEXT("/Game/Babylon/Trees")), In, true);
		for (const FAssetData& A : In)
			if (A.AssetClassPath.GetAssetName() == TEXT("StaticMesh") && A.AssetName.ToString().Contains(Name) && !(FString(Name) == TEXT("Beech_A") && A.AssetName.ToString().Contains(TEXT("Bare"))))
				return Cast<UStaticMesh>(A.GetAsset());
		return nullptr;
	}
	/** A material's colour parameter, whatever the glTF importer called it. */
	void Tint(UMaterialInstanceDynamic* M, const FLinearColor& C)
	{
		if (!M) return;
		for (const TCHAR* P : {TEXT("Base Color Factor"), TEXT("BaseColorFactor"), TEXT("BaseColor"), TEXT("Base Color"), TEXT("Color"), TEXT("Tint")}) M->SetVectorParameterValue(P, C);
	}
}

ASiteTrees::ASiteTrees()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool ASiteTrees::Load()
{
	if (ADPSite::IsDefault() || FParse::Param(FCommandLine::Get(), TEXT("NoTrees"))) return false;
	TArray<uint8> Bin;
	if (!FFileHelper::LoadFileToArray(Bin, *ADPSite::File(TEXT("trees.bin"))) || Bin.Num() < 24) { ADP_LOG(TEXT("Trees: none for this site")); return false; }
	Count = Bin.Num() / (6 * sizeof(float));
	Trees.SetNumUninitialized(Count);
	FMemory::Memcpy(Trees.GetData(), Bin.GetData(), Count * sizeof(FTree));
	// the 256 m cells (sorted by x then y in the file) grouped into 1 km blocks
	FString J; TSharedPtr<FJsonObject> O;
	if (!FFileHelper::LoadFileToString(J, *ADPSite::File(TEXT("trees.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(J), O) || !O) return false;
	TMap<FIntPoint, TArray<FIntPoint>> Ranges;   // block -> (first, count) of its cells
	for (const TSharedPtr<FJsonValue>& V : O->GetArrayField(TEXT("cells")))
	{
		const TArray<TSharedPtr<FJsonValue>>& C = V->AsArray();
		const int32 Cx = int32(C[0]->AsNumber()), Cy = int32(C[1]->AsNumber());
		const FIntPoint Key(FMath::FloorToInt(Cx * 256.f / BlockM), FMath::FloorToInt(Cy * 256.f / BlockM));
		Ranges.FindOrAdd(Key).Add(FIntPoint(int32(C[2]->AsNumber()), int32(C[3]->AsNumber())));
	}
	// each block keeps its trees together: re-ordered so a block is one run of the array
	TArray<FTree> Sorted; Sorted.Reserve(Count);
	for (auto& R : Ranges)
	{
		FBlock B; B.First = Sorted.Num(); B.Centre = FVector2D((R.Key.X + 0.5f) * BlockM, (R.Key.Y + 0.5f) * BlockM);
		for (const FIntPoint& Run : R.Value) for (int32 i = 0; i < Run.Y; ++i) if (Trees.IsValidIndex(Run.X + i)) Sorted.Add(Trees[Run.X + i]);
		B.Num = Sorted.Num() - B.First;
		Blocks.Add(R.Key, B);
	}
	Trees = MoveTemp(Sorted);
	for (const TCHAR* N : TreeNames) Meshes.Add(FindTree(N));
	if (!Meshes[0] || !Meshes[3]) { ADP_WARN(TEXT("Trees: the tree models are missing (Art/Trees, Tools/import_model.py)")); return false; }
	// seasonal colours on our own instances of the trees' materials (needles, bark, leaves)
	for (UStaticMesh* M : Meshes) if (M) for (int32 s = 0; s < M->GetStaticMaterials().Num(); ++s)
	{
		UMaterialInterface* Base = M->GetMaterial(s);
		const FString N = Base ? Base->GetName() : FString();
		bool bHave = false; for (UMaterialInstanceDynamic* D : Mats) if (D && D->Parent == Base) bHave = true;
		if (Base && !bHave) { UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, this, FName(*(TEXT("Season_") + N))); Mats.Add(D); }
	}
	ADP_LOG(TEXT("Trees: %d real trees in %d blocks (out to %.1f km round you)"), Count, Blocks.Num(), TreeRadiusKm);
	return true;
}

void ASiteTrees::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	if (!Load()) { SetActorTickEnabled(false); return; }
	Season();
	Stream(true);
}

void ASiteTrees::Season()
{
	// the airport's own date (or the one chosen in Settings): beech bare Dec-Mar, fresh in spring, autumn colours Sep-Nov
	const FDateTime L = ADPSite::LocalNow();
	const int32 Day = ASunClock::DayOverride >= 0 ? ASunClock::DayOverride : L.GetDayOfYear();
	if (Day == SeasonDay) return;
	SeasonDay = Day;
	FLinearColor Leaf(0.11f, 0.22f, 0.05f);
	if (Day < 110) Leaf = FLinearColor(0.11f, 0.22f, 0.05f);              // (bare anyway)
	else if (Day < 150) Leaf = FLinearColor(0.22f, 0.40f, 0.07f);         // fresh spring green (late April - May)
	else if (Day < 245) Leaf = FLinearColor(0.10f, 0.21f, 0.05f);         // summer
	else if (Day < 275) Leaf = FLinearColor(0.30f, 0.30f, 0.06f);         // turning (September)
	else if (Day < 305) Leaf = FLinearColor(0.55f, 0.27f, 0.05f);         // beech gold and copper (October)
	else Leaf = FLinearColor(0.32f, 0.16f, 0.06f);                        // brown, the last leaves (November)
	const bool bWinter = Day >= 335 || Day < 100;
	const FLinearColor Needles = bWinter ? FLinearColor(0.035f, 0.075f, 0.04f) : FLinearColor(0.045f, 0.10f, 0.045f);
	for (UMaterialInstanceDynamic* M : Mats)
	{
		const FString N = M && M->Parent ? M->Parent->GetName() : FString();
		if (N.Contains(TEXT("Leaves"))) Tint(M, Leaf);
		else if (N.Contains(TEXT("Needles"))) Tint(M, Needles);
		else if (N.Contains(TEXT("BarkBeech"))) Tint(M, FLinearColor(0.32f, 0.31f, 0.29f));
		else if (N.Contains(TEXT("Bark"))) Tint(M, FLinearColor(0.18f, 0.11f, 0.07f));
	}
	// leaves on or off: the blocks in view are drawn again with the season's beech
	for (auto& B : Blocks) if (B.Value.bIn) { Hide(B.Value); Show(B.Key, B.Value); }
}

void ASiteTrees::Show(const FIntPoint& Key, FBlock& B)
{
	const bool bBare = SeasonDay >= 335 || SeasonDay < 100;
	TArray<TArray<FTransform>> Xf; Xf.SetNum(UE_ARRAY_COUNT(TreeNames));
	for (int32 i = B.First; i < B.First + B.Num; ++i)
	{
		const FTree& T = Trees[i];
		// which model: conifers one of the three by the tree's place (always the same tree there), broadleaf the beech
		const uint32 Hh = uint32(T.X * 7.3f) * 2654435761u ^ uint32(T.Y * 3.1f);
		const int32 K = T.Kind < 0.5f ? int32(Hh % 3) : (bBare ? 4 : 3);
		const float S = FMath::Max(0.15f, T.H / 20.f);   // (the models are 20 m tall)
		const float Wide = K >= 3 ? FMath::Clamp(T.R / 6.5f, 0.6f, 1.5f) / FMath::Max(0.4f, S) : 1.f;   // (a broadleaf's crown as wide as found)
		Xf[K].Add(FTransform(FRotator(0.f, float(Hh % 360), 0.f), FVector(T.X * 100.f, -T.Y * 100.f, T.Z * 100.f), FVector(S * FMath::Clamp(Wide, 0.8f, 1.3f), S * FMath::Clamp(Wide, 0.8f, 1.3f), S)));
	}
	for (int32 k = 0; k < Xf.Num(); ++k)
	{
		if (!Xf[k].Num() || !Meshes[k]) continue;
		UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		C->SetStaticMesh(Meshes[k]);
		for (int32 s = 0; s < Meshes[k]->GetStaticMaterials().Num(); ++s)
			for (UMaterialInstanceDynamic* M : Mats) if (M && M->Parent == Meshes[k]->GetMaterial(s)) C->SetMaterial(s, M);
		C->SetMobility(EComponentMobility::Static);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // (out on the slopes: nobody walks there; near trees come with the walk-anywhere mode)
		// shadows from the trees near you only (their far shadows are in the 10 cm photo already)
		C->SetCastShadow(true); C->bCastFarShadow = false;
		C->SetCullDistances(0, int32(TreeRadiusKm * 100000.f));
		C->SetupAttachment(GetRootComponent());
		C->RegisterComponent();
		C->AddInstances(Xf[k], false, false);
		B.Parts.Add(C);
	}
	B.bIn = true;
}

void ASiteTrees::Hide(FBlock& B)
{
	for (UHierarchicalInstancedStaticMeshComponent* C : B.Parts) if (C) C->DestroyComponent();
	B.Parts.Reset(); B.bIn = false;
}

void ASiteTrees::Stream(bool bAll)
{
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector P = Cam ? Cam->GetCameraLocation() : FVector::ZeroVector;
	const FVector2D Me(P.X / 100.f, -P.Y / 100.f);   // (plan metres)
	const float R = TreeRadiusKm * 1000.f + BlockM * 0.75f;
	int32 Added = 0;
	for (auto& B : Blocks)
	{
		const bool bWant = FVector2D::Distance(B.Value.Centre, Me) < R;
		if (bWant && !B.Value.bIn) { if (!bAll && Added >= 2) continue; Show(B.Key, B.Value); ++Added; }   // (two blocks a step: no hitch)
		else if (!bWant && B.Value.bIn && FVector2D::Distance(B.Value.Centre, Me) > R + BlockM) Hide(B.Value);
	}
}

void ASiteTrees::Tick(float Dt)
{
	Super::Tick(Dt);
	if ((Clock += Dt) < 0.5f) return;
	Clock = 0.f;
	Stream(false);
	static float SeasonClock = 0.f;
	if ((SeasonClock += 0.5f) >= 60.f) { SeasonClock = 0.f; Season(); }
}
