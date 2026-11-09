// Places the Babylon model: walks its node tree from the roots, works out each node's transform the way Unreal's glTF
// importer converts them, and adds an instance of the node's imported mesh there.
#include "BabylonModel.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#pragma region Placing

ABabylonModel::ABabylonModel()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

bool ABabylonModel::Place(const FString& Name)
{
	const FString File = FString::Printf(TEXT("Data/babylon-%s.json"), *Name.ToLower());
	FString Path = FPaths::Combine(FPaths::ProjectContentDir(), File);
	if (!FPaths::FileExists(Path)) Path = FPaths::Combine(FPaths::ProjectDir(), File);
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) return false;
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J) return false;
	const TArray<TSharedPtr<FJsonValue>>& Nodes = J->GetArrayField(TEXT("nodes"));
	// the imported meshes are named after the glTF mesh's number (the terminal) or after their object (the lounge)
	FString AssetNames;
	J->TryGetStringField(TEXT("assetNames"), AssetNames);
	const bool bByNode = AssetNames == TEXT("node");
	// Babylon's glTF exporter turns the left-handed model right-handed by flipping x, and Unreal's importer swaps y and
	// z: together that puts the model at (-x, z, y), the Babylon layout turned 180° round the vertical. Turning it back
	// gives exactly the terminal plan's (x, -z, y), with no mirroring (the geometry stays exactly as Babylon made it).
	Root->SetRelativeRotation(FRotator(0, 180.f, 0));

	// a node's own transform, converted as Unreal's glTF importer converts it
	auto Local = [&](const TSharedPtr<FJsonObject>& N)
	{
		FVector T(0), S(1);
		FQuat Q = FQuat::Identity;
		const TArray<TSharedPtr<FJsonValue>>* A;
		if (N->TryGetArrayField(TEXT("translation"), A) && A->Num() == 3) T = FVector((*A)[0]->AsNumber(), (*A)[2]->AsNumber(), (*A)[1]->AsNumber()) * 100.0;
		if (N->TryGetArrayField(TEXT("rotation"), A) && A->Num() == 4) Q = FQuat(-(*A)[0]->AsNumber(), -(*A)[2]->AsNumber(), -(*A)[1]->AsNumber(), (*A)[3]->AsNumber());
		if (N->TryGetArrayField(TEXT("scale"), A) && A->Num() == 3) S = FVector((*A)[0]->AsNumber(), (*A)[2]->AsNumber(), (*A)[1]->AsNumber());
		return FTransform(Q, T, S);
	};

	int32 Placed = 0;
	TFunction<void(int32, const FTransform&)> Walk = [&](int32 I, const FTransform& Parent)
	{
		if (!Nodes.IsValidIndex(I)) return;
		const TSharedPtr<FJsonObject> N = Nodes[I]->AsObject();
		const FTransform World = Local(N) * Parent;
		int32 Mesh;
		if (N->TryGetNumberField(TEXT("mesh"), Mesh))
		{
			UInstancedStaticMeshComponent* K = nullptr;
			FString Asset = FString::Printf(TEXT("%s_mesh_%d"), *Name.ToLower(), Mesh);
			if (bByNode) N->TryGetStringField(TEXT("name"), Asset);
			if (TObjectPtr<UInstancedStaticMeshComponent>* F = ByMesh.Find(Asset)) K = *F;
			else
			{
				const FString MeshPath = FString::Printf(TEXT("/Game/Babylon/%s/%s/StaticMeshes/%s.%s"), *Name, *Name.ToLower(), *Asset, *Asset);
				if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *MeshPath))
				{
					K = NewObject<UInstancedStaticMeshComponent>(this);
					K->SetupAttachment(Root);
					K->SetStaticMesh(SM); // with the materials and textures it was imported with
					K->SetCollisionProfileName(TEXT("BlockAll"));
					K->RegisterComponent();
					AddInstanceComponent(K);
				}
				ByMesh.Add(Asset, K);
			}
			// relative to the model, so its turn applies; meshes imported one per object already carry their place (Unreal's
			// importer builds each object's position into its mesh), so they go in as they are
			if (K) { K->AddInstance(bByNode ? FTransform::Identity : World, false); ++Placed; }
		}
		const TArray<TSharedPtr<FJsonValue>>* Children;
		if (N->TryGetArrayField(TEXT("children"), Children))
			for (const TSharedPtr<FJsonValue>& C : *Children) Walk(int32(C->AsNumber()), World);
	};
	for (const TSharedPtr<FJsonValue>& R : J->GetArrayField(TEXT("roots"))) Walk(int32(R->AsNumber()), FTransform::Identity);
	UE_LOG(LogTemp, Log, TEXT("Babylon model %s placed: %d objects, %d meshes"), *Name, Placed, ByMesh.Num());
	return Placed > 0;
}

void ABabylonModel::AddLights(const FString& Name)
{
	const FString File = FString::Printf(TEXT("Data/babylon-%s-lights.json"), *Name.ToLower());
	FString Path = FPaths::Combine(FPaths::ProjectContentDir(), File);
	if (!FPaths::FileExists(Path)) Path = FPaths::Combine(FPaths::ProjectDir(), File);
	FString Text;
	TArray<TSharedPtr<FJsonValue>> List;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), List)) return;
	// Babylon's own coordinates to the placed model's: the exporter's x flip, the importer's y-z swap and the model's
	// half turn together give (x, -z, y), metres to centimetres
	auto ToModel = [](const TArray<TSharedPtr<FJsonValue>>& A) { return FVector(A[0]->AsNumber(), -A[2]->AsNumber(), A[1]->AsNumber()); };
	int32 Made = 0;
	for (const TSharedPtr<FJsonValue>& V : List)
	{
		const TSharedPtr<FJsonObject> L = V->AsObject();
		const TArray<TSharedPtr<FJsonValue>>* Pos = nullptr;
		if (!L.IsValid() || !L->TryGetArrayField(TEXT("pos"), Pos) || Pos->Num() != 3) continue;
		const FString Kind = L->GetStringField(TEXT("kind"));
		FLinearColor Colour = FLinearColor::White;
		const TArray<TSharedPtr<FJsonValue>>* Col = nullptr;
		if (L->TryGetArrayField(TEXT("color"), Col) && Col->Num() == 3) Colour = FLinearColor((*Col)[0]->AsNumber(), (*Col)[1]->AsNumber(), (*Col)[2]->AsNumber());
		double Strength = 1, Range = 10;
		L->TryGetNumberField(TEXT("intensity"), Strength);
		L->TryGetNumberField(TEXT("range"), Range);
		UPointLightComponent* P = nullptr;
		if (Kind == TEXT("SpotLight"))
		{
			USpotLightComponent* S = NewObject<USpotLightComponent>(this);
			const TArray<TSharedPtr<FJsonValue>>* Dir = nullptr;
			if (L->TryGetArrayField(TEXT("dir"), Dir) && Dir->Num() == 3) S->SetRelativeRotation(ToModel(*Dir).GetSafeNormal().Rotation());
			S->SetOuterConeAngle(40.f);
			S->SetInnerConeAngle(20.f);
			P = S;
		}
		else if (Kind == TEXT("PointLight")) P = NewObject<UPointLightComponent>(this);
		if (!P) continue;
		P->SetupAttachment(Root);
		P->SetRelativeLocation(ToModel(*Pos) * 100.0);
		P->SetIntensityUnits(ELightUnits::Candelas);
		P->SetIntensity(Strength * 25.0);   // Babylon's strengths to candelas: a room lamp (9) about 225 cd
		P->SetLightColor(Colour);
		P->SetAttenuationRadius(Range * 100.0);
		P->SetSourceRadius(4.f);
		P->RegisterComponent();
		AddInstanceComponent(P);
		++Made;
	}
	UE_LOG(LogTemp, Log, TEXT("Babylon model %s: %d lights"), *Name, Made);
}

#pragma endregion
