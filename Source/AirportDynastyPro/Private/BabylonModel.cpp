// Places the Babylon model: walks its node tree from the roots, works out each node's transform the way Unreal's glTF
// importer converts them, and adds an instance of the node's imported mesh there.
#include "BabylonModel.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#pragma region Placing

ABabylonModel::ABabylonModel()
{
	PrimaryActorTick.bCanEverTick = true;
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
	// every node's place in the model, its parent, and its mesh instance (for the doors)
	TArray<FTransform> NodeWorld; NodeWorld.SetNum(Nodes.Num());
	TArray<int32> NodeParent; NodeParent.Init(INDEX_NONE, Nodes.Num());
	TArray<TPair<UInstancedStaticMeshComponent*, int32>> NodeInst; NodeInst.Init(TPair<UInstancedStaticMeshComponent*, int32>(nullptr, INDEX_NONE), Nodes.Num());
	TFunction<void(int32, const FTransform&, bool)> Walk = [&](int32 I, const FTransform& Parent, bool bDoor)
	{
		if (!Nodes.IsValidIndex(I)) return;
		const TSharedPtr<FJsonObject> N = Nodes[I]->AsObject();
		const FTransform World = Local(N) * Parent;
		NodeWorld[I] = World;
		// (all the outdoor trees stay until good-looking Unreal trees take their places, owner 2026-11-09)
		{ FString Nm; N->TryGetStringField(TEXT("name"), Nm); bDoor = bDoor || Nm.StartsWith(TEXT("D-")); }
		int32 Mesh;
		if (N->TryGetNumberField(TEXT("mesh"), Mesh))
		{
			UInstancedStaticMeshComponent* K = nullptr;
			FString Asset = FString::Printf(TEXT("%s_mesh_%d"), *Name.ToLower(), Mesh);
			if (bByNode) N->TryGetStringField(TEXT("name"), Asset);
			// a door's parts in lists of their own (they move; the big shared lists never need rebuilding)
			const FString Key = bDoor ? Asset + TEXT("#door") : Asset;
			if (TObjectPtr<UInstancedStaticMeshComponent>* F = ByMesh.Find(Key)) K = *F;
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
				ByMesh.Add(Key, K);
			}
			// relative to the model, so its turn applies; meshes imported one per object already carry their place (Unreal's
			// importer builds each object's position into its mesh), so they go in as they are
			if (K) { NodeInst[I] = TPair<UInstancedStaticMeshComponent*, int32>(K, K->AddInstance(bByNode ? FTransform::Identity : World, false)); ++Placed; }
		}
		const TArray<TSharedPtr<FJsonValue>>* Children;
		if (N->TryGetArrayField(TEXT("children"), Children))
			for (const TSharedPtr<FJsonValue>& C : *Children) { NodeParent[int32(C->AsNumber())] = I; Walk(int32(C->AsNumber()), World, bDoor); }
	};
	for (const TSharedPtr<FJsonValue>& R : J->GetArrayField(TEXT("roots"))) Walk(int32(R->AsNumber()), FTransform::Identity, false);
	UE_LOG(LogTemp, Log, TEXT("Babylon model %s placed: %d objects, %d meshes"), *Name, Placed, ByMesh.Num());

	// the doors (nodes named D-<id>), read the way doors.ts builds them
	if (!bByNode)
	{
		auto Kids = [&](int32 I) { TArray<int32> L; const TArray<TSharedPtr<FJsonValue>>* C; if (Nodes[I]->AsObject()->TryGetArrayField(TEXT("children"), C)) for (const TSharedPtr<FJsonValue>& V : *C) L.Add(int32(V->AsNumber())); return L; };
		auto NameOf = [&](int32 I) { FString S; Nodes[I]->AsObject()->TryGetStringField(TEXT("name"), S); return S; };
		auto Num3 = [&](int32 I, const TCHAR* F, int32 A) { const TArray<TSharedPtr<FJsonValue>>* V; return Nodes[I]->AsObject()->TryGetArrayField(F, V) && V->Num() == 3 ? (*V)[A]->AsNumber() : (FCString::Strcmp(F, TEXT("scale")) == 0 ? 1.0 : 0.0); };
		// a moving part: the node itself and every mesh instance under it, held relative to it
		auto MakePart = [&](int32 P)
		{
			FPart Part;
			Part.Base = Local(Nodes[P]->AsObject());
			Part.Parent = NodeParent[P] != INDEX_NONE ? NodeWorld[NodeParent[P]] : FTransform::Identity;
			TArray<int32> Stack = {P};
			while (Stack.Num())
			{
				const int32 I = Stack.Pop();
				if (NodeInst[I].Key) Part.Inst.Add({NodeInst[I].Key, NodeInst[I].Value, NodeWorld[I].GetRelativeTransform(NodeWorld[P])});
				Stack.Append(Kids(I));
			}
			return Part;
		};
		for (int32 D = 0; D < Nodes.Num(); ++D)
		{
			if (!NameOf(D).StartsWith(TEXT("D-"))) continue;
			FDoor Door;
			Door.At = NodeWorld[D].GetLocation();
			const TArray<int32> C = Kids(D);
			// the width from the frame's posts (at ±(w/2 + 0.05)); sliding leaves are the thin panels (depth up to 0.06 m)
			float W = 1.8f;
			for (const int32 K : C) if (NameOf(K).StartsWith(TEXT("box")) && Num3(K, TEXT("scale"), 2) > 0.3 && Num3(K, TEXT("scale"), 1) > 1.5) W = FMath::Max(W, 2.f * (FMath::Abs(float(Num3(K, TEXT("translation"), 0))) - 0.05f));
			Door.Reach = FMath::Max(2.6f, W * 0.7f);
			for (const int32 K : C)
			{
				const FString N = NameOf(K);
				const double X = Num3(K, TEXT("translation"), 0);
				if (N.StartsWith(TEXT("box")) && Num3(K, TEXT("scale"), 2) <= 0.061 && FMath::Abs(X) > 0.01 && Num3(K, TEXT("translation"), 1) > 0.5)
				{
					FPart P = MakePart(K);
					P.Slide = FMath::Sign(float(X)) * (W / 2 - 0.08f) * 100.f;   // each leaf away from the middle
					Door.Parts.Add(MoveTemp(P));
				}
				else if (N.StartsWith(TEXT("hinge")))
				{
					FPart P = MakePart(K);
					P.Swing = (X < 0 ? 1.f : -1.f) * 1.5f;   // a quarter turn and a bit, as doors.ts
					Door.Parts.Add(MoveTemp(P));
				}
				else if (N.StartsWith(TEXT("wings")))
				{
					FPart P = MakePart(K);
					P.bSpin = true;
					Door.Parts.Add(MoveTemp(P));
					Door.bSpin = true;
				}
			}
			if (Door.Parts.Num()) Doors.Add(MoveTemp(Door));
		}
		UE_LOG(LogTemp, Log, TEXT("Babylon model %s: %d doors that move"), *Name, Doors.Num());
		LightFittings(Nodes, NodeWorld, NodeInst);
	}
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

#pragma region Light fittings

void ABabylonModel::LightFittings(const TArray<TSharedPtr<FJsonValue>>& Nodes, const TArray<FTransform>& NodeWorld, const TArray<TPair<UInstancedStaticMeshComponent*, int32>>& NodeInst)
{
	auto NameOf = [&](int32 I) { FString S; Nodes[I]->AsObject()->TryGetStringField(TEXT("name"), S); return S; };
	int32 Spots = 0, Soft = 0, Strip = 0, Lamp = 0;
	auto Add = [&](ULocalLightComponent* L, const FVector& At, float Candelas, float Reach, const FLinearColor& Col)
	{
		L->SetupAttachment(Root);
		L->SetRelativeLocation(At);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(Candelas);
		L->SetLightColor(Col);
		L->SetAttenuationRadius(Reach);
		L->SetCastShadows(false);   // hundreds of lamps: no shadows from them (the sun and the sky keep theirs)
		L->RegisterComponent();
		AddInstanceComponent(L);
	};
	const FLinearColor Warm(1.f, 0.93f, 0.82f);
	for (int32 I = 0; I < Nodes.Num(); ++I)
	{
		const FString N = NameOf(I);
		if (N.Contains(TEXT("DOWNLIGHT")))
		{
			// the round downlights under the wave roof: a spot straight down into the hall
			USpotLightComponent* S = NewObject<USpotLightComponent>(this);
			S->SetRelativeRotation(FRotator(-90.f, 0, 0));
			S->SetInnerConeAngle(20.f);
			S->SetOuterConeAngle(42.f);
			Add(S, NodeWorld[I].GetLocation() - FVector(0, 0, 10.f), 900.f, 2600.f, Warm);
			// and its spill: a soft glow just under the roof, so the timber ceiling round each downlight is lit as in life
			UPointLightComponent* Spill = NewObject<UPointLightComponent>(this);
			Spill->SetSourceRadius(12.f);
			// a small warm glow centred right on the fitting (owner, 2026-11-09: no big blobs, 20% darker than before) ...
			Add(Spill, NodeWorld[I].GetLocation() - FVector(0, 0, 25.f), 18.f, 180.f, Warm);
			// ... and a soft fill from straight below it, so the timber between the fittings is lit, not black
			UPointLightComponent* Fill = NewObject<UPointLightComponent>(this);
			Fill->SetSourceRadius(80.f);
			Fill->SetSoftSourceRadius(150.f);
			Add(Fill, NodeWorld[I].GetLocation() - FVector(0, 0, 180.f), 55.f, 900.f, Warm);
			++Spots;
		}
		else if (N.Contains(TEXT("/LAMP-")))
		{
			// the ceiling lamps: every other one gives a soft light into the room (the panels themselves all glow)
			if ((Lamp++ & 1) == 0)
			{
				UPointLightComponent* P = NewObject<UPointLightComponent>(this);
				P->SetSourceRadius(30.f);
				P->SetSoftSourceRadius(60.f);
				Add(P, NodeWorld[I].GetLocation() - FVector(0, 0, 25.f), 140.f, 900.f, Warm);
				++Soft;
			}
		}
		else if (N.EndsWith(TEXT("/LED")) && NodeInst[I].Key)
		{
			// a jetway section's LED strip: lights every 3.5 m along it, just under the roof
			const FBox B = NodeInst[I].Key->GetStaticMesh()->GetBoundingBox().TransformBy(NodeWorld[I]);
			const FVector Ext = B.GetExtent(), C = B.GetCenter();
			const bool bAlongX = Ext.X >= Ext.Y;
			const float Len = 2.f * (bAlongX ? Ext.X : Ext.Y);
			const int32 Count = FMath::Max(1, FMath::RoundToInt(Len / 350.f));
			for (int32 k = 0; k < Count; ++k)
			{
				const float T = (k + 0.5f) / Count - 0.5f;
				const FVector At = C + (bAlongX ? FVector(T * Len, 0, 0) : FVector(0, T * Len, 0)) - FVector(0, 0, Ext.Z + 20.f);
				UPointLightComponent* P = NewObject<UPointLightComponent>(this);
				P->SetSourceRadius(20.f);
				Add(P, At, 180.f, 600.f, FLinearColor(0.95f, 0.97f, 1.f));
				++Strip;
			}
		}
	}
	// the glowing materials (the lamps, downlights, LED strips, signs' lights) brighter, so they bloom as lit fittings do
	TArray<UInstancedStaticMeshComponent*> Parts;
	GetComponents(Parts);
	int32 Glows = 0;
	for (UInstancedStaticMeshComponent* K : Parts)
		for (int32 S = 0; S < K->GetNumMaterials(); ++S)
		{
			UMaterialInterface* M = K->GetMaterial(S);
			if (!M) continue;
			const FString MN = M->GetName();
			if (!MN.StartsWith(TEXT("glow")) && !MN.EndsWith(TEXT("_LED"))) continue;
			UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(M);
			if (!D) { D = UMaterialInstanceDynamic::Create(M, this); K->SetMaterial(S, D); }
			TArray<FMaterialParameterInfo> Infos; TArray<FGuid> Ids;
			D->GetAllScalarParameterInfo(Infos, Ids);
			for (const FMaterialParameterInfo& P : Infos)
				if (P.Name.ToString().Contains(TEXT("EmissiveStrength"))) D->SetScalarParameterValueByInfo(P, 12.f);
			// (the emissive colour itself too, where the material has one: the fittings must outshine what they light)
			TArray<FMaterialParameterInfo> VInfos; TArray<FGuid> VIds;
			D->GetAllVectorParameterInfo(VInfos, VIds);
			for (const FMaterialParameterInfo& P : VInfos)
				if (P.Name.ToString().Contains(TEXT("Emissive")))
				{
					FLinearColor C;
					if (D->GetVectorParameterValue(P, C)) D->SetVectorParameterValueByInfo(P, C * 8.f);
				}
			++Glows;
		}
	UE_LOG(LogTemp, Log, TEXT("Babylon model lights: %d downlights, %d room lights, %d jetway lights, %d glowing materials"), Spots, Soft, Strip, Glows);
}

#pragma endregion

#pragma region Doors

void ABabylonModel::MovePart(FPart& P, float E, float Spin)
{
	FTransform L = P.Base;
	if (P.Slide != 0) L.AddToTranslation(L.GetRotation().RotateVector(FVector(P.Slide * E, 0, 0)));
	if (P.Swing != 0) L.SetRotation(L.GetRotation() * FQuat(FVector::UpVector, P.Swing * E));
	if (P.bSpin) L.SetRotation(L.GetRotation() * FQuat(FVector::UpVector, -Spin));
	const FTransform Now = L * P.Parent;
	TSet<UInstancedStaticMeshComponent*> Touched;
	for (const FInst& I : P.Inst)
	{
		I.K->UpdateInstanceTransform(I.I, I.Rel * Now, false, false, true);
		Touched.Add(I.K);
	}
	for (UInstancedStaticMeshComponent* K : Touched) K->MarkRenderStateDirty();
}

bool ABabylonModel::SlidingDoorAt(FVector& Out) const
{
	for (const FDoor& D : Doors)
		for (const FPart& P : D.Parts)
			if (P.Slide != 0 && D.Reach > 2.9f) { Out = Root->GetComponentTransform().TransformPosition(D.At); return true; }
	return false;
}

void ABabylonModel::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Doors.Num()) return;
	Clock += Dt;
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	// where you are, in the model's own space (feet height)
	FVector Me(1e9);
	if (P) Me = Root->GetComponentTransform().InverseTransformPosition(P->GetActorLocation() - FVector(0, 0, P->GetSimpleCollisionHalfHeight()));
	for (FDoor& D : Doors)
	{
		if (D.bSpin) { for (FPart& Part : D.Parts) if (Part.bSpin) MovePart(Part, 0, Clock * 0.6f); continue; }
		const bool bNear = FVector2D::Distance(FVector2D(Me), FVector2D(D.At)) < D.Reach * 100.f && FMath::Abs(Me.Z - D.At.Z) < 250.f;
		const float Want = bNear ? 1.f : 0.f;
		if (FMath::Abs(D.K - Want) < 0.001f) continue;
		D.K += FMath::Sign(Want - D.K) * FMath::Min(FMath::Abs(Want - D.K), Dt * 1.6f);
		const float E = D.K * D.K * (3 - 2 * D.K);   // eased, as doors.ts
		for (FPart& Part : D.Parts) MovePart(Part, E, 0);
	}
}

#pragma endregion

#pragma endregion
