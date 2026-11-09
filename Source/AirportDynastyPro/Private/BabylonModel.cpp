// Places the Babylon model: walks its node tree from the roots, works out each node's transform the way Unreal's glTF
// importer converts them, and adds an instance of the node's imported mesh there.
#include "BabylonModel.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
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
	// Babylon is left-handed and the exported model keeps its coordinates as they are; Unreal's glTF importer swaps y
	// and z, which leaves the model mirrored north to south. Mirroring it back gives exactly Babylon's layout (and the
	// terminal plan's: x, -z, y), lettering the right way round.
	// (a mirror on the root, scale y -1, turned the model black: lighting broke; the north-south flip is still to be
	// solved properly, see HANDOVER.md)

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
			if (TObjectPtr<UInstancedStaticMeshComponent>* F = ByMesh.Find(Mesh)) K = *F;
			else
			{
				const FString MeshPath = FString::Printf(TEXT("/Game/Babylon/%s/%s/StaticMeshes/%s_mesh_%d.%s_mesh_%d"), *Name, *Name.ToLower(), *Name.ToLower(), Mesh, *Name.ToLower(), Mesh);
				if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *MeshPath))
				{
					K = NewObject<UInstancedStaticMeshComponent>(this);
					K->SetupAttachment(Root);
					K->SetStaticMesh(SM); // with the materials and textures it was imported with
					K->SetCollisionProfileName(TEXT("BlockAll"));
					K->RegisterComponent();
					AddInstanceComponent(K);
				}
				ByMesh.Add(Mesh, K);
			}
			if (K) { K->AddInstance(World, true); ++Placed; }
		}
		const TArray<TSharedPtr<FJsonValue>>* Children;
		if (N->TryGetArrayField(TEXT("children"), Children))
			for (const TSharedPtr<FJsonValue>& C : *Children) Walk(int32(C->AsNumber()), World);
	};
	for (const TSharedPtr<FJsonValue>& R : J->GetArrayField(TEXT("roots"))) Walk(int32(R->AsNumber()), FTransform::Identity);
	UE_LOG(LogTemp, Log, TEXT("Babylon model %s placed: %d objects, %d meshes"), *Name, Placed, ByMesh.Num());
	return Placed > 0;
}

#pragma endregion
