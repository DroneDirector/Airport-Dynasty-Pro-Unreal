// Places the Babylon model: walks its node tree from the roots, works out each node's transform the way Unreal's glTF
// importer converts them, and adds an instance of the node's imported mesh there.
#include "BabylonModel.h"
#include "TerminalBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/DirectionalLight.h"
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
		// the gate screens (nodes M-F<floor>-G<gate>): their face ("scr"), each its own mesh, for the live gate pictures
		for (int32 G = 0; G < Nodes.Num(); ++G)
		{
			const FString N = NameOf(G);
			int32 At = INDEX_NONE;
			if (!N.StartsWith(TEXT("M-F")) || !N.FindLastChar(TCHAR('G'), At) || At < 4 || N[At - 1] != TCHAR('-')) continue;
			const int32 Gate = FCString::Atoi(*N.Mid(At + 1));
			if (Gate <= 0) continue;
			for (const int32 K : Kids(G)) if (NameOf(K).StartsWith(TEXT("scr")) && NodeInst[K].Key) GateScreens.Add(Gate, NodeInst[K].Key);
		}
		UE_LOG(LogTemp, Log, TEXT("Babylon model %s: %d gate screens"), *Name, GateScreens.Num());
		LightFittings(Nodes, NodeWorld, NodeInst);
		ReplaceSeats(Nodes, NodeWorld, NodeInst);
	}
	ToneDown();
	return Placed > 0;
}

void ABabylonModel::ToneDown()
{
	// whites as real paint is (about 78% at most: the Babylon colours go to 98%, which glows under Unreal's light), and
	// glowing pictures that aren't lamps (the lounge's fire, panels) at half their strength; glass and lamps untouched
	TMap<UMaterialInterface*, UMaterialInterface*> Done;
	int32 Whites = 0, Glows = 0;
	TArray<UInstancedStaticMeshComponent*> Parts;
	GetComponents(Parts);
	for (UInstancedStaticMeshComponent* K : Parts)
		for (int32 S = 0; S < K->GetNumMaterials(); ++S)
		{
			UMaterialInterface* M = K->GetMaterial(S);
			if (!M || Cast<UMaterialInstanceDynamic>(M)) continue;
			if (UMaterialInterface** F = Done.Find(M)) { if (*F) K->SetMaterial(S, *F); continue; }
			UMaterialInterface* Out = nullptr;
			const FString MN = M->GetName();
			const bool bLamp = MN.StartsWith(TEXT("glow")) || MN.EndsWith(TEXT("_LED")) || MN.StartsWith(TEXT("fids-")) || MN.StartsWith(TEXT("pic_"));
			const EBlendMode Blend = M->GetBlendMode();
			if (!bLamp && Blend == BLEND_Opaque)
			{
				FLinearColor Base;
				float Strength = 0;
				const bool bWhite = M->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("BaseColorFactor")), Base) && Base.GetLuminance() > 0.8f;
				const bool bGlow = M->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("EmissiveStrength")), Strength) && Strength > 1.f;
				if (bWhite || bGlow)
				{
					UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(M, this);
					if (bWhite) { D->SetVectorParameterValue(TEXT("BaseColorFactor"), Base * (0.78f / Base.GetLuminance())); ++Whites; }
					if (bGlow) { D->SetScalarParameterValue(TEXT("EmissiveStrength"), Strength * 0.5f); ++Glows; }
					Out = D;
				}
			}
			Done.Add(M, Out);
			if (Out) K->SetMaterial(S, Out);
		}
	UE_LOG(LogTemp, Log, TEXT("Babylon model: %d whites toned to paint, %d glows halved"), Whites, Glows);
}

void ABabylonModel::AddLights(const FString& Name)
{
	const FString File = FString::Printf(TEXT("Data/babylon-%s-lights.json"), *Name.ToLower());
	FString Path = FPaths::Combine(FPaths::ProjectContentDir(), File);
	if (!FPaths::FileExists(Path)) Path = FPaths::Combine(FPaths::ProjectDir(), File);
	FString Text;
	TArray<TSharedPtr<FJsonValue>> List;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), List)) return;
	// Babylon's own coordinates to the placed model's: the exporter's x flip and the importer's y-z swap give (-x, z, y)
	// under the model's root, whose half turn then puts it at (x, -z, y) in the world, as the meshes are (these were
	// once given as (x, -z, y) under the root: every lounge light stood mirrored, on the far side of the room)
	auto ToModel = [](const TArray<TSharedPtr<FJsonValue>>& A) { return FVector(-A[0]->AsNumber(), A[2]->AsNumber(), A[1]->AsNumber()); };
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
		FString LName; L->TryGetStringField(TEXT("name"), LName);
		const bool bFireLight = LName.StartsWith(TEXT("fire"));
		P->SetIntensity(Strength * (bFireLight ? 7.0 : 25.0));   // Babylon's strengths to candelas: a room lamp (9) about 225 cd; the fire's own glow far gentler (it sits a hand's width from the logs)
		P->SetLightColor(Colour);
		P->SetAttenuationRadius(Range * 100.0);
		P->SetSourceRadius(4.f);
		P->RegisterComponent();
		AddInstanceComponent(P);
		++Made;
		// the fireplace's lights flicker with the flames
		FString LN; L->TryGetStringField(TEXT("name"), LN);
		if (LN.StartsWith(TEXT("fire"))) { FireLights.Add(P); FireBase.Add(Strength * 7.0); if (LN == TEXT("fireL")) { AddFire(ToModel(*Pos) * 100.0); DeepenFireplace(0.6f); } }
	}
	UE_LOG(LogTemp, Log, TEXT("Babylon model %s: %d lights"), *Name, Made);
}

void ABabylonModel::ReplaceSeats(const TArray<TSharedPtr<FJsonValue>>& Nodes, const TArray<FTransform>& NodeWorld, const TArray<TPair<UInstancedStaticMeshComponent*, int32>>& NodeInst)
{
	const TCHAR* Dir = TEXT("/Game/Babylon/GateSeat/gateseat/StaticMeshes/");
	UStaticMesh* SeatM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%sSeatModule.SeatModule"), Dir));
	UStaticMesh* LegM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%sLegModule.LegModule"), Dir));
	UStaticMesh* ArmM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%sArmModule.ArmModule"), Dir));
	if (!SeatM || !LegM || !ArmM) { UE_LOG(LogTemp, Warning, TEXT("Seats: the new seating isn't imported")); return; }
	auto Kit = [this](UStaticMesh* M)
	{
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this);
		K->SetupAttachment(Root);
		K->SetStaticMesh(M);
		K->SetCollisionProfileName(TEXT("BlockAll"));
		K->RegisterComponent();
		AddInstanceComponent(K);
		return K;
	};
	UInstancedStaticMeshComponent* Seats = Kit(SeatM);
	UInstancedStaticMeshComponent* Legs = Kit(LegM);
	UInstancedStaticMeshComponent* Arms = Kit(ArmM);
	auto NameOf = [&](int32 I) { FString S; Nodes[I]->AsObject()->TryGetStringField(TEXT("name"), S); return S; };
	int32 Rows = 0, Places = 0;
	// their materials from the terminal's own (already made for instanced Nanite lists): the old seats' dark blue for
	// the shells (owner, 2026-11-10), the terminal's steel as 75% matte chrome for legs, arms and beam, black for the hubs
	UMaterialInterface* Blue = nullptr; UMaterialInterface* Steel = nullptr;
	for (int32 R = 0; R < Nodes.Num() && (!Blue || !Steel); ++R)
	{
		if (!NameOf(R).Contains(TEXT("/SEATS-"))) continue;
		const TArray<TSharedPtr<FJsonValue>>* Kids = nullptr;
		if (!Nodes[R]->AsObject()->TryGetArrayField(TEXT("children"), Kids)) continue;
		for (const TSharedPtr<FJsonValue>& V : *Kids)
		{
			const int32 C = int32(V->AsNumber());
			UInstancedStaticMeshComponent* K = NodeInst[C].Key;
			if (!K) continue;
			const TArray<TSharedPtr<FJsonValue>>* T = nullptr;
			const bool bBeam = Nodes[C]->AsObject()->TryGetArrayField(TEXT("translation"), T) && T->Num() == 3 && FMath::IsNearlyEqual((*T)[1]->AsNumber(), 0.32, 0.01);
			if (bBeam && !Steel) Steel = K->GetMaterial(0);
			if (!bBeam && !Blue && FMath::IsNearlyEqual((*T)[1]->AsNumber(), 0.44, 0.01)) Blue = K->GetMaterial(0);
		}
	}
	// the blue as Babylon shows it: its colour is a screen colour (#1C2D52), read here as light it came out pale
	if (Blue)
	{
		UMaterialInstanceDynamic* Navy = UMaterialInstanceDynamic::Create(Blue, this);
		Navy->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor(FColor(0x1C, 0x2D, 0x52)));
		Blue = Navy;
	}
	UMaterialInstanceDynamic* Chrome = Steel ? UMaterialInstanceDynamic::Create(Steel, this) : nullptr;
	if (Chrome) { Chrome->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor(0.75f, 0.75f, 0.75f)); Chrome->SetScalarParameterValue(TEXT("MetallicFactor"), 1.f); Chrome->SetScalarParameterValue(TEXT("RoughnessFactor"), 0.36f); }
	UMaterialInstanceDynamic* Dark = Steel ? UMaterialInstanceDynamic::Create(Steel, this) : nullptr;
	if (Dark) { Dark->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor(0.015f, 0.015f, 0.015f)); Dark->SetScalarParameterValue(TEXT("MetallicFactor"), 0.f); Dark->SetScalarParameterValue(TEXT("RoughnessFactor"), 0.5f); }
	// the pieces' material slots: Shell, Metal (and Black on the legs), as made in make_gate_seat.py
	auto Dress = [&](UInstancedStaticMeshComponent* K)
	{
		for (int32 S = 0; S < K->GetNumMaterials(); ++S)
		{
			const FName Slot = K->GetMaterialSlotNames().IsValidIndex(S) ? K->GetMaterialSlotNames()[S] : NAME_None;
			if (Slot == TEXT("Shell") && Blue) K->SetMaterial(S, Blue);
			else if (Slot == TEXT("Metal") && Chrome) K->SetMaterial(S, Chrome);
			else if (Slot == TEXT("Black") && Dark) K->SetMaterial(S, Dark);
		}
	};
	Dress(Seats); Dress(Legs); Dress(Arms);
	for (int32 R = 0; R < Nodes.Num(); ++R)
	{
		if (!NameOf(R).Contains(TEXT("/SEATS-"))) continue;
		const TArray<TSharedPtr<FJsonValue>>* Kids = nullptr;
		if (!Nodes[R]->AsObject()->TryGetArrayField(TEXT("children"), Kids)) continue;
		// the row's seats: its backs (the boxes 0.72 m up); the old boxes hidden (shrunk away: their lists are shared)
		int32 N = 0;
		for (const TSharedPtr<FJsonValue>& V : *Kids)
		{
			const int32 C = int32(V->AsNumber());
			const TArray<TSharedPtr<FJsonValue>>* T = nullptr;
			if (Nodes[C]->AsObject()->TryGetArrayField(TEXT("translation"), T) && T->Num() == 3 && FMath::IsNearlyEqual((*T)[1]->AsNumber(), 0.72, 0.01)) ++N;
			if (UInstancedStaticMeshComponent* K = NodeInst[C].Key)
			{
				FTransform X; K->GetInstanceTransform(NodeInst[C].Value, X);
				X.SetScale3D(FVector(0.0001f));
				K->UpdateInstanceTransform(NodeInst[C].Value, X, false, false, true);
			}
		}
		if (N <= 0) continue;
		// along the row (Babylon x, the glTF's -x): seats at the 0.62 m pitch, legs at both ends and every third seat
		const float L = N * 0.62f;
		const FTransform& Row = NodeWorld[R];
		auto Put = [&](UInstancedStaticMeshComponent* K, float BabylonX) { K->AddInstance(FTransform(FVector(-BabylonX * 100.f, 0, 0)) * Row, false); };
		for (int32 i = 0; i < N; ++i) Put(Seats, -L / 2 + 0.62f * (i + 0.5f));
		for (int32 k = 0; k <= N; ++k) Put(k == 0 || k == N || k % 3 == 0 ? Legs : Arms, -L / 2 + 0.62f * k);
		++Rows; Places += N;
	}
	for (UInstancedStaticMeshComponent* K : {Seats, Legs, Arms}) K->MarkRenderStateDirty();
	UE_LOG(LogTemp, Log, TEXT("Seats: %d rows, %d places in the new seating"), Rows, Places);
}

void ABabylonModel::AddFire(const FVector& LightAt)
{
	// the fire sits on the logs, half a metre back from its light (the Babylon fire: 0.48 m behind it, 12 cm up; under
	// the root Babylon's x runs the other way)
	const FVector Base(LightAt.X + 48.f, LightAt.Y, 12.f);
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Fire = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Fire/M_Fire.M_Fire"));
	if (Plane && Fire)
		// three crossed panels of flame (each its own pattern), so it looks full from every side
		for (int32 i = 0; i < 6; ++i)
		{
			UStaticMeshComponent* F = NewObject<UStaticMeshComponent>(this);
			F->SetupAttachment(Root);
			F->SetStaticMesh(Plane);
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Fire, this);
			D->SetScalarParameterValue(TEXT("Seed"), 3.7f * i);
			D->SetScalarParameterValue(TEXT("Glow"), i < 2 ? 9.f : 6.f);
			F->SetMaterial(0, D);
			// layers through the fire's depth and turned about, so it is full from every side (a body of flame, not cards)
			const float Depth = (i - 2.5f) * 4.f, Turn = 90.f + 30.f * i + (i % 2 ? 12.f : -12.f);
			const float Wide = i < 2 ? 0.92f : 0.7f - 0.04f * i, Tall = i < 2 ? 0.88f : 0.72f;
			F->SetRelativeTransform(FTransform(FRotator(0, Turn, 90.f), Base + FVector(Depth, (i % 3 - 1) * 6.f, 12.f + Tall * 50.f), FVector(Wide, Tall, 1.f)));
			F->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			F->SetCastShadow(false);
			F->RegisterComponent();
			AddInstanceComponent(F);
		}
	// the wood: three charred logs across the grate (two side by side, one laid over them), on a bed of embers
	if (UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		UMaterialInterface* Bark = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		UMaterialInstanceDynamic* Char = Bark ? UMaterialInstanceDynamic::Create(Bark, this) : nullptr;
		if (Char) { Char->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.035f, 0.022f, 0.015f)); Char->SetScalarParameterValue(TEXT("Roughness"), 0.95f); }
		struct FLog { FVector At; float Yaw, Roll, Len, R; };
		const FLog Logs[] = { {FVector(-6.f, -9.f, 7.f), 0.f, 90.f, 0.62f, 0.13f}, {FVector(6.f, 8.f, 7.f), 8.f, 90.f, 0.58f, 0.12f}, {FVector(0.f, 0.f, 17.f), -14.f, 90.f, 0.55f, 0.11f} };
		for (const FLog& L : Logs)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetupAttachment(Root);
			C->SetStaticMesh(Cyl);
			if (Char) C->SetMaterial(0, Char);
			// lying along the fireplace's width (the cylinder stands up along Z: rolled onto its side, along Y)
			C->SetRelativeTransform(FTransform(FRotator(0, L.Yaw, L.Roll), Base + L.At, FVector(L.R, L.R, L.Len)));
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->RegisterComponent();
			AddInstanceComponent(C);
		}
		// the fireplace's back, black with soot behind the fire
		if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			UStaticMeshComponent* B = NewObject<UStaticMeshComponent>(this);
			B->SetupAttachment(Root);
			B->SetStaticMesh(Cube);
			UMaterialInstanceDynamic* Soot = Bark ? UMaterialInstanceDynamic::Create(Bark, this) : nullptr;
			if (Soot) { Soot->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.012f, 0.011f, 0.01f)); Soot->SetScalarParameterValue(TEXT("Roughness"), 1.f); B->SetMaterial(0, Soot); }
			B->SetRelativeTransform(FTransform(FRotator::ZeroRotator, Base + FVector(17.f, 0, 47.f), FVector(0.02f, 1.12f, 0.94f)));
			B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			B->RegisterComponent();
			AddInstanceComponent(B);
		}
		// the embers under them: a low glowing bed (the flame pattern laid flat, dim)
		if (Plane && Fire)
		{
			UStaticMeshComponent* E = NewObject<UStaticMeshComponent>(this);
			E->SetupAttachment(Root);
			E->SetStaticMesh(Plane);
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Fire, this);
			D->SetScalarParameterValue(TEXT("Seed"), 9.1f);
			D->SetScalarParameterValue(TEXT("Glow"), 3.f);
			E->SetMaterial(0, D);
			E->SetRelativeTransform(FTransform(FRotator::ZeroRotator, Base + FVector(0, 0, 2.f), FVector(0.45f, 0.7f, 1.f)));
			E->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			E->SetCastShadow(false);
			E->RegisterComponent();
			AddInstanceComponent(E);
		}
	}
	// the heat over it: the air above the flames shimmering (the picture behind bent by rising waves)
	if (UMaterialInterface* Heat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Fire/M_FireHeat.M_FireHeat")))
		if (Plane)
		{
			UStaticMeshComponent* F = NewObject<UStaticMeshComponent>(this);
			F->SetupAttachment(Root);
			F->SetStaticMesh(Plane);
			F->SetMaterial(0, Heat);
			F->SetRelativeTransform(FTransform(FRotator(0, 90.f, 90.f), Base + FVector(-12.f, 0, 85.f), FVector(0.9f, 0.75f, 1.f)));
			F->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			F->SetCastShadow(false);
			F->RegisterComponent();
			AddInstanceComponent(F);
		}
	// its sound: a wood fire's roar and crackle, heard by the fireplace and fading over the room (looping)
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/Materials/Fire/S_FireCrackle.S_FireCrackle")))
	{
		UAudioComponent* A = NewObject<UAudioComponent>(this);
		A->SetupAttachment(Root);
		A->SetRelativeLocation(Base + FVector(0, 0, 30.f));
		A->SetSound(S);
		A->bOverrideAttenuation = true;
		A->AttenuationOverrides.bAttenuate = true;
		A->AttenuationOverrides.bSpatialize = true;
		A->AttenuationOverrides.FalloffDistance = 900.f;
		A->AttenuationOverrides.AttenuationShapeExtents = FVector(120.f);
		A->SetVolumeMultiplier(0.55f);
		A->bAutoActivate = true;
		A->RegisterComponent();
		AddInstanceComponent(A);
		A->Play();
	}
	FireWorld = Root->GetComponentTransform().TransformPosition(Base);
	UE_LOG(LogTemp, Log, TEXT("Lounge fire lit at %s"), *FireWorld.ToString());
}

void ABabylonModel::DeepenFireplace(float Depth)
{
	// the pieces as the Babylon lounge names them: b-579 the dark breast (3 m wide, 4.8 m tall, its face at Babylon
	// x -10.88), b-580…b-594 the five golden frames, b-597 the stone hearth, b-600…b-608 the golden rays above;
	// b-596 (the back wall in the opening) and b-599 (the logs' bed) stay
	auto Part = [this](int32 N) -> UInstancedStaticMeshComponent* { const TObjectPtr<UInstancedStaticMeshComponent>* F = ByMesh.Find(FString::Printf(TEXT("b-%d"), N)); return F ? F->Get() : nullptr; };
	UInstancedStaticMeshComponent* Breast = Part(579);
	if (!Breast) return;
	// under the model's root Babylon's x (into the room) runs along -X
	const FVector Out(-Depth * 100.f, 0, 0);
	TArray<int32> Move;
	for (int32 N = 580; N <= 594; ++N) Move.Add(N);
	Move.Add(597);
	for (int32 N = 600; N <= 608; ++N) Move.Add(N);
	for (const int32 N : Move) if (UInstancedStaticMeshComponent* K = Part(N)) K->SetRelativeLocation(K->GetRelativeLocation() + Out);
	// the breast again, deeper and open where the fire is: three blocks round the opening (1.2 m wide, 1 m high,
	// as the inner frame), from the wall out to the new face; and a dark floor in the firebox
	UMaterialInterface* Dark = Breast->GetMaterial(0);
	UMaterialInterface* Stone = Part(597) ? Part(597)->GetMaterial(0) : Dark;
	Breast->SetVisibility(false);
	Breast->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) return;
	const float X0 = -11.0f, X1 = -10.88f + Depth, Z0 = -4.58f, Z1 = -1.58f, OZ0 = -3.68f, OZ1 = -2.48f, OY = 1.0f, H = 4.8f;
	auto Block = [&](float BX0, float BX1, float BY0, float BY1, float BZ0, float BZ1, UMaterialInterface* M)
	{
		// Babylon (x, y, z) to under the root: (-x, z, y), metres to centimetres
		const FVector Mid(-(BX0 + BX1) * 50.f, (BZ0 + BZ1) * 50.f, (BY0 + BY1) * 50.f);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetupAttachment(Root);
		C->SetStaticMesh(Cube);
		if (M) C->SetMaterial(0, M);
		C->SetRelativeTransform(FTransform(FRotator::ZeroRotator, Mid, FVector(BX1 - BX0, BZ1 - BZ0, BY1 - BY0)));
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->RegisterComponent();
		AddInstanceComponent(C);
	};
	Block(X0, X1, 0.f, H, Z0, OZ0, Dark);          // left of the opening
	Block(X0, X1, 0.f, H, OZ1, Z1, Dark);          // right of it
	Block(X0, X1, OY, H, OZ0, OZ1, Dark);          // above it
	Block(-10.84f, X1, 0.f, 0.03f, OZ0, OZ1, Stone);   // the firebox's floor
	// the five golden frames as stepped walls (owner, 2026-11-10): each gap between two frames filled solid, out to
	// the outer frame's face, so the surround steps forward towards the opening; the golden edges stay as they are
	// (Babylon's frames: inner width 1.2 + 0.28 s, height 1.0 + 0.14 s, depth 0.14 + 0.02 (4 - s), 12 cm off the wall)
	const float FZ = -3.08f;
	for (int32 S = 0; S < 4; ++S)
	{
		const int32 O = S + 1;
		const float WI = (1.2f + 0.28f * S) / 2, WO = (1.2f + 0.28f * O) / 2, HI = 1.0f + 0.14f * S, HO = 1.0f + 0.14f * O;
		const float Face = X0 + 0.12f + (0.14f + (4 - O) * 0.02f) + Depth;
		Block(X1, Face, 0.f, HO, FZ - WO, FZ - WI, Dark);   // left
		Block(X1, Face, 0.f, HO, FZ + WI, FZ + WO, Dark);   // right
		Block(X1, Face, HI, HO, FZ - WI, FZ + WI, Dark);    // over the opening
	}
	UE_LOG(LogTemp, Log, TEXT("Lounge fireplace %.1f m deeper"), Depth);
}

void ABabylonModel::FlickerFire()
{
	if (!FireLights.Num()) return;
	const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	// as the Babylon lounge's fire: two slow waves and a quick one, never steady
	const float K = 0.85f + 0.3f * FMath::Sin(T * 9.f) * FMath::Sin(T * 4.7f) + 0.1f * FMath::Sin(T * 23.f);
	for (int32 i = 0; i < FireLights.Num(); ++i) if (FireLights[i]) FireLights[i]->SetIntensity(FireBase[i] * K);
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
		LampLights.Add(L); LampBase.Add(Candelas);
		AddInstanceComponent(L);
	};
	const FLinearColor Warm(1.f, 0.93f, 0.82f);
	// the lamps' faces drawn with the owner's lamp texture (M_WalkLamp, glowing): a flat panel just under each fitting
	// (the imported fittings have no texture coordinates), the fitting's own box hidden. One list of panels per kind.
	UMaterialInterface* WalkLampM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/WalkLamp/M_WalkLamp.M_WalkLamp"));
	UStaticMesh* PlaneM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	auto Panels = [&](float Along, float Across, float Lo, float Hi, float Round) -> UInstancedStaticMeshComponent*
	{
		if (!WalkLampM || !PlaneM) return nullptr;
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this);
		K->SetupAttachment(Root);
		K->SetStaticMesh(PlaneM);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(WalkLampM, this);
		D->SetScalarParameterValue(TEXT("Along"), Along); D->SetScalarParameterValue(TEXT("Across"), Across);
		D->SetScalarParameterValue(TEXT("Lo"), Lo); D->SetScalarParameterValue(TEXT("Hi"), Hi); D->SetScalarParameterValue(TEXT("Round"), Round);
		K->SetMaterial(0, D);
		K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		K->SetCastShadow(false);
		K->RegisterComponent();
		AddInstanceComponent(K);
		return K;
	};
	// a fitting's box (node C) hidden and a panel put just under it, the panel's U along the fitting's long side; the box's
	// bounds back (in the model's space)
	auto PanelUnder = [&](UInstancedStaticMeshComponent* K, int32 C) -> FBox
	{
		UInstancedStaticMeshComponent* Shared = NodeInst[C].Key;
		const FBox B = Shared->GetStaticMesh()->GetBoundingBox().TransformBy(NodeWorld[C]);
		if (!K) return B;
		FTransform T;
		if (Shared->GetInstanceTransform(NodeInst[C].Value, T, false))
		{
			FTransform Gone = T; Gone.SetScale3D(FVector(0.0001f));
			Shared->UpdateInstanceTransform(NodeInst[C].Value, Gone, false, true);
		}
		const FVector Ext = B.GetExtent();
		const bool bAlongX = Ext.X >= Ext.Y;
		// the plane faces up: turned over (roll 180) to face down, and a quarter turn when the fitting runs along Y
		const FRotator Rot(0.f, bAlongX ? 0.f : 90.f, 180.f);
		const FVector Scale(2.f * FMath::Max(Ext.X, Ext.Y) / 100.f, 2.f * FMath::Min(Ext.X, Ext.Y) / 100.f, 1.f);
		K->AddInstance(FTransform(Rot, FVector(B.GetCenter().X, B.GetCenter().Y, B.Min.Z - 0.3f), Scale), false);
		return B;
	};
	UInstancedStaticMeshComponent* WalkLamps = nullptr;   // the walkway's strips (made when the first is met)
	TMap<FIntPoint, UInstancedStaticMeshComponent*> LampPanels;   // the rooms' lamps, one list per size
	for (int32 I = 0; I < Nodes.Num(); ++I)
	{
		const FString N = NameOf(I);
		if (N.Contains(TEXT("DOWNLIGHT")))
		{
			// the round downlights under the wave roof, inside and out: as the car rental canopy's (owner, 2026-11-10)
			// the fitting itself glows (its lamp material, with the bloom round it) and that is all: no spot, no pool of
			// light on the roof or the floor, no glow blob
			++Spots;
		}
		else if (N.Contains(TEXT("/LAMP-")))
		{
			// the rooms' ceiling lamps (the gate rooms, the crew corridors, offices, shops and the rest): their face the
			// walkway lamps' diffuser (owner, 2026-11-09), its frame along the long sides, the pattern at its own scale
			// whatever the lamp's size; every other one also lights the room as a soft panel does
			const TArray<TSharedPtr<FJsonValue>>* Kids = nullptr;
			FBox Face(ForceInit);
			if (Nodes[I]->AsObject()->TryGetArrayField(TEXT("children"), Kids))
				for (const TSharedPtr<FJsonValue>& KV : *Kids)
				{
					const int32 C = int32(KV->AsNumber());
					if (!NodeInst.IsValidIndex(C) || !NodeInst[C].Key) continue;
					const FBox B = NodeInst[C].Key->GetStaticMesh()->GetBoundingBox().TransformBy(NodeWorld[C]);
					const float Len = 2.f * FMath::Max(B.GetExtent().X, B.GetExtent().Y), Wid = 2.f * FMath::Min(B.GetExtent().X, B.GetExtent().Y);
					// one list of panels per lamp size (the pattern repeats along the lamp every two thirds of its width)
					const FIntPoint Key(FMath::RoundToInt(Len), FMath::RoundToInt(Wid));
					UInstancedStaticMeshComponent*& K = LampPanels.FindOrAdd(Key);
					if (!K) K = Panels(Len / FMath::Max(1.f, Wid * 2.f / 3.f), 1.f, 0.f, 1.f, 0.f);
					Face = PanelUnder(K, C);
				}
			if ((Lamp++ & 1) == 0)
			{
				URectLightComponent* L = NewObject<URectLightComponent>(this);
				const FVector Ext = Face.IsValid ? Face.GetExtent() : FVector(60, 30, 2);
				const bool bAlongX = Ext.X >= Ext.Y;
				L->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector(0, 0, -1), bAlongX ? FVector(1, 0, 0) : FVector(0, 1, 0)).Rotator());
				L->SetSourceHeight(2.f * FMath::Max(Ext.X, Ext.Y));
				L->SetSourceWidth(2.f * FMath::Min(Ext.X, Ext.Y));
				L->SetBarnDoorAngle(88.f);
				Add(L, Face.IsValid ? FVector(Face.GetCenter().X, Face.GetCenter().Y, Face.Min.Z - 1.f) : NodeWorld[I].GetLocation() - FVector(0, 0, 5.f), 220.f, 900.f, Warm);
				++Soft;
			}
		}
		else if (N.Contains(TEXT("CEILING-LIGHT-")))
		{
			// the long walkway's linear lamps beside the glass vault: each strip is itself the light (its whole face
			// glows), so a soft panel of light down from the full strip, not a spot (owner, 2026-11-09)
			const TArray<TSharedPtr<FJsonValue>>* Kids = nullptr;
			if (Nodes[I]->AsObject()->TryGetArrayField(TEXT("children"), Kids))
				for (const TSharedPtr<FJsonValue>& KV : *Kids)
				{
					const int32 C = int32(KV->AsNumber());
					if (!NodeInst.IsValidIndex(C) || !NodeInst[C].Key) continue;
					if (!WalkLamps) WalkLamps = Panels(12.f, 1.f, 0.f, 1.f, 0.f);
					PanelUnder(WalkLamps, C);
					const FBox B = NodeInst[C].Key->GetStaticMesh()->GetBoundingBox().TransformBy(NodeWorld[C]);
					const FVector Ext = B.GetExtent();
					const bool bAlongX = Ext.X >= Ext.Y;
					URectLightComponent* L = NewObject<URectLightComponent>(this);
					// facing straight down, the strip's length along its local Z, its width along its local Y
					L->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector(0, 0, -1), bAlongX ? FVector(1, 0, 0) : FVector(0, 1, 0)).Rotator());
					L->SetSourceHeight(2.f * FMath::Max(Ext.X, Ext.Y));
					L->SetSourceWidth(2.f * FMath::Min(Ext.X, Ext.Y));
					L->SetBarnDoorAngle(88.f);
					Add(L, B.GetCenter() - FVector(0, 0, Ext.Z + 1.f), 260.f, 1100.f, FLinearColor(1.f, 0.96f, 0.9f));
					++Strip;
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
			// a lamp drawn as an unlit black face (its light was a glow effect in Babylon): the face itself is the light now
			FLinearColor Base;
			if (D->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("BaseColorFactor")), Base) && Base.GetLuminance() < 0.02f)
				D->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor(1.f, 0.96f, 0.9f) * 5.f);
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

void ABabylonModel::LampsByDaylight()
{
	// the sun's height (the sun light's own angle, set by the sun clock): dark below 6 degrees under the horizon,
	// full day from 15 degrees up; between, as the light outside changes
	static TWeakObjectPtr<ADirectionalLight> Sun;
	if (!Sun.IsValid()) Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	if (!Sun.IsValid() || !LampLights.Num()) return;
	const float El = -Sun->GetActorRotation().Pitch;
	const float Day = FMath::SmoothStep(-6.f, 15.f, El);
	const float Scale = FMath::Lerp(1.f, 0.2f, Day);
	if (FMath::Abs(Scale - LampScale) < 0.005f) return;
	LampScale = Scale;
	for (int32 i = 0; i < LampLights.Num(); ++i)
		if (LampLights[i]) LampLights[i]->SetIntensity(LampBase[i] * Scale);
}

void ABabylonModel::Tick(float Dt)
{
	FlickerFire();
	Super::Tick(Dt);
	LampsByDaylight();
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
