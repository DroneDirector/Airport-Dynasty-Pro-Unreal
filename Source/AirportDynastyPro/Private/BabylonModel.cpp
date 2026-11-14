// Places the Babylon model: walks its node tree from the roots, works out each node's transform the way Unreal's glTF
// importer converts them, and adds an instance of the node's imported mesh there.
#include "BabylonModel.h"
#include "Misc/PackageName.h"
#include "ADPLog.h"
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
	Model = Name;
	// (the terminal: its own layout, Data/terminal-layout.json, no longer the Babylon export's; -OldTerminal compares)
	const bool bOwnLayout = Name == TEXT("Terminal") && !FParse::Param(FCommandLine::Get(), TEXT("OldTerminal"));
	const FString File = bOwnLayout ? FString(TEXT("Data/terminal-layout.json")) : FString::Printf(TEXT("Data/babylon-%s.json"), *Name.ToLower());
	FString Path = FPaths::Combine(FPaths::ProjectContentDir(), File);
	if (!FPaths::FileExists(Path)) Path = FPaths::Combine(FPaths::ProjectDir(), File);
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) return false;
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J) return false;
	const TArray<TSharedPtr<FJsonValue>>& Nodes = J->GetArrayField(TEXT("nodes"));
	// the true-Unreal terminal (to-do 42): its native piece set (/Game/Terminal/Meshes, Tools/terminal_native_meshes.py),
	// every copy of a piece drawn from that one mesh as one instanced list (Data/terminal-pieces.json: each imported
	// mesh's piece and its offset); -OldTerminal shows the Babylon-exported set as before, to compare
	TMap<FString, TPair<FString, FVector>> Pieces;
	if (Name == TEXT("Terminal") && !FParse::Param(FCommandLine::Get(), TEXT("OldTerminal")))
	{
		FString PText, PPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/terminal-pieces.json"));
		if (!FPaths::FileExists(PPath)) PPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/terminal-pieces.json"));
		TSharedPtr<FJsonObject> PJ;
		if (FFileHelper::LoadFileToString(PText, *PPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PText), PJ) && PJ)
			for (const TPair<FString, TSharedPtr<FJsonValue>>& E : PJ->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>& A = E.Value->AsArray();
				if (A.Num() == 4) Pieces.Add(E.Key, TPair<FString, FVector>(A[0]->AsString(), FVector(A[1]->AsNumber(), A[2]->AsNumber(), A[3]->AsNumber())));
			}
		if (Pieces.Num() && !FPackageName::DoesPackageExist(TEXT("/Game/Terminal/Meshes/") + Pieces.CreateConstIterator()->Value.Key)) Pieces.Reset();
		ADP_LOG(TEXT("Terminal: %s"), Pieces.Num() ? *FString::Printf(TEXT("native pieces (%d meshes)"), Pieces.Num()) : TEXT("the Babylon-exported set"));
	}
	// parts lying exactly on another part's face (a stripe on a door, a lens on its band, a cushion on its seat) fight for
	// the same pixels and flicker: each is moved 1.5 mm out along the faces they share, so it always shows on top
	// (Data/<model>-zfight.json, Tools/find_zfight.py; owner, 2026-11-14: the fire truck flickered - the shape untouched)
	TMap<FString, FVector> Nudge;
	{
		FString ZText, ZPath = FPaths::Combine(FPaths::ProjectContentDir(), FString::Printf(TEXT("Data/%s-zfight.json"), *Name.ToLower()));
		if (!FPaths::FileExists(ZPath)) ZPath = FPaths::Combine(FPaths::ProjectDir(), FString::Printf(TEXT("Data/%s-zfight.json"), *Name.ToLower()));
		TSharedPtr<FJsonObject> ZJ; const TSharedPtr<FJsonObject>* NJ = nullptr;
		if (FFileHelper::LoadFileToString(ZText, *ZPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ZText), ZJ) && ZJ && ZJ->TryGetObjectField(TEXT("nudge"), NJ))
			for (const TPair<FString, TSharedPtr<FJsonValue>>& E : (*NJ)->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>& A = E.Value->AsArray();
				// (glTF metres to the model's own space, as the node translations are converted)
				if (A.Num() == 3) Nudge.Add(E.Key, FVector(A[0]->AsNumber(), A[2]->AsNumber(), A[1]->AsNumber()) * 100.0);
			}
		if (Nudge.Num()) ADP_LOG(TEXT("Babylon model %s: %d parts moved by a hair against flicker"), *Name, Nudge.Num());
	}
	// the imported meshes are named after the glTF mesh's number (the terminal) or after their object (the lounge)
	FString AssetNames;
	J->TryGetStringField(TEXT("assetNames"), AssetNames);
	const bool bByNode = AssetNames == TEXT("node");
	// Babylon's glTF exporter turns the left-handed model right-handed by flipping x, and Unreal's importer swaps y and
	// z: together that puts the model at (-x, z, y), the Babylon layout turned 180° round the vertical. Turning it back
	// gives exactly the terminal plan's (x, -z, y), with no mirroring (the geometry stays exactly as Babylon made it).
	// (the root is the actor's own: the half turn goes on top of the turn it was placed with, which it once replaced,
	// leaving every office, the operations room and the Academy unturned)
	Root->SetRelativeRotation(FRotator(0, Root->GetRelativeRotation().Yaw + 180.f, 0));

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
	// the entrance canopies' parts (KERB/CANOPY-*): in lists of their own, so their underside can be given a matte finish
	TArray<bool> Canopy; Canopy.Init(false, Nodes.Num());
	constexpr float StandsOut = 12.f;   // metres the stands moved out (Data/airfield.json standsNote)
	TFunction<void(int32, const FTransform&, bool)> Walk;
	Walk = [&](int32 I, const FTransform& Parent, bool bDoor)
	{
		if (!Nodes.IsValidIndex(I)) return;
		const TSharedPtr<FJsonObject> N = Nodes[I]->AsObject();
		FTransform L = Local(N);
		{ FString Nm; N->TryGetStringField(TEXT("name"), Nm);
		  if (Name == TEXT("Terminal") && Nm.StartsWith(TEXT("STAND-")) && Nm.EndsWith(TEXT("/AIRCRAFT"))) L.AddToTranslation(FVector(0, StandsOut * 100.f, 0)); }
		const FTransform World = L * Parent;
		NodeWorld[I] = World;
		// (all the outdoor trees stay until good-looking Unreal trees take their places, owner 2026-11-09)
		{ FString Nm; N->TryGetStringField(TEXT("name"), Nm); bDoor = bDoor || Nm.StartsWith(TEXT("D-"));
		  // the offices' own furniture steps aside for the old office's things (owner, 2026-11-11: Offices.cpp places them);
		  // the room, its lamps, the meeting table and its chairs and the painting stay
		  // the stands moved 12 m out (owner, 2026-11-13): his parked aircraft go with them; the model's flat stand markings
		  // and taxilane line step aside for the ones the airfield lays from the layout (Airfield.cpp Aprons)
		  if (Name == TEXT("Terminal") && ((Nm.StartsWith(TEXT("STAND-")) && !Nm.Contains(TEXT("/"))) || Nm == TEXT("APRON/TAXI-LANE"))) return;
		  // the model's own lawn round the terminal lay 25 cm over the airfield's grass (a step at its edge, the taxiways
		  // under it): the airfield's grass, the same, runs under the whole terminal instead (owner, 2026-11-13)
		  if (Name == TEXT("Terminal") && Nm == TEXT("SITE/GRASS")) return;
		  // the fire station model's own ground (its 190 x 160 m ground sheet b27, its forecourt b28, the walk b35 and its
		  // kerb b37) lay a few centimetres off the airfield's and cars bumped over its edges: the airfield lays that
		  // paving now (Data/airfield.json LOT-FIRE-*), one ground everywhere (owner, 2026-11-14)
		  if (Name == TEXT("Firestation") && (Nm == TEXT("b27") || Nm == TEXT("b28") || Nm == TEXT("b30") || Nm == TEXT("b35") || Nm == TEXT("b37"))) return;
		  // the yellow "GATE n" signs hanging at each gate's window (F1-GATE-5..8, F2-GATE-1..4 /SIGN-001) are gone (owner,
		  // 2026-11-14: they shouldn't be at the window)
		  if (Name == TEXT("Terminal") && Nm.Contains(TEXT("-GATE-")) && Nm.EndsWith(TEXT("/SIGN-001"))) return;
		  // his jetways come from their own export now (Data/babylon-jetways.json: J-1 to J-4 rebuilt by his generator for
		  // the stands 12 m out - longer tunnel sections, the corridor at 3.3 degrees); the terminal's older ones stay out
		  if (Name == TEXT("Terminal") && Nm.Len() == 3 && Nm.StartsWith(TEXT("J-")) && FPackageName::DoesPackageExist(TEXT("/Game/Babylon/Jetways/jetways/StaticMeshes/jetways_mesh_0"))) return;
		  if (Nm.StartsWith(TEXT("F1-OFFICE-")) && (Nm.Contains(TEXT("/DESK")) || Nm.Contains(TEXT("/OFFICE-CHAIR")) || Nm.Contains(TEXT("/SHELF")) || Nm.Contains(TEXT("/CABINET")) || Nm.Contains(TEXT("/PLANT")) || Nm.EndsWith(TEXT("/CHAIR-001")) || Nm.EndsWith(TEXT("/CHAIR-002")))) return;
		  // and so does the operations room's and the Academy's (their lamps and the clocks stay)
		  if (Nm.StartsWith(TEXT("F1-OPS/")) && (Nm.Contains(TEXT("/DESK")) || Nm.Contains(TEXT("/OFFICE-CHAIR")) || Nm.Contains(TEXT("/MONITOR-WALL")))) return;
		  if (Nm.StartsWith(TEXT("F1-ACADEMY/")) && (Nm.Contains(TEXT("/DESK")) || Nm.Contains(TEXT("CHAIR")) || Nm.Contains(TEXT("/BOARD")) || Nm.Contains(TEXT("/SCREEN")))) return; }
		int32 Mesh;
		if (N->TryGetNumberField(TEXT("mesh"), Mesh))
		{
			UInstancedStaticMeshComponent* K = nullptr;
			FString Asset = FString::Printf(TEXT("%s_mesh_%d"), *Name.ToLower(), Mesh);
			if (bByNode) N->TryGetStringField(TEXT("name"), Asset);
			// the native terminal: this copy's piece, and where the copy sat against it
			FVector PieceOff = FVector::ZeroVector;
			const bool bPiece = Pieces.Num() > 0;
			if (const TPair<FString, FVector>* Pc = bPiece ? Pieces.Find(Asset) : nullptr) { Asset = Pc->Key; PieceOff = Pc->Value; }
			// a door's parts in lists of their own (they move; the big shared lists never need rebuilding)
			const FString Key = bDoor ? Asset + TEXT("#door") : Canopy[I] ? Asset + TEXT("#canopy") : Asset;
			if (TObjectPtr<UInstancedStaticMeshComponent>* F = ByMesh.Find(Key)) K = *F;
			else
			{
				const FString MeshPath = bPiece ? FString::Printf(TEXT("/Game/Terminal/Meshes/%s.%s"), *Asset, *Asset)
					: FString::Printf(TEXT("/Game/Babylon/%s/%s/StaticMeshes/%s.%s"), *Name, *Name.ToLower(), *Asset, *Asset);
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
			if (K)
			{
				FTransform At = bByNode ? FTransform::Identity : FTransform(PieceOff) * World;
				if (Nudge.Num()) { FString Nn; N->TryGetStringField(TEXT("name"), Nn); if (const FVector* Nv = Nudge.Find(Nn)) At.AddToTranslation(*Nv); }
				const int32 Ix = K->AddInstance(At, false);
				NodeInst[I] = TPair<UInstancedStaticMeshComponent*, int32>(K, Ix); ++Placed;
				FString NodeName; N->TryGetStringField(TEXT("name"), NodeName);
				TArray<FString>& Names = InstNames.FindOrAdd(K); if (Names.Num() <= Ix) Names.SetNum(Ix + 1); Names[Ix] = NodeName;
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Children;
		if (N->TryGetArrayField(TEXT("children"), Children))
			for (const TSharedPtr<FJsonValue>& C : *Children)
			{
				const int32 Ci = int32(C->AsNumber());
				FString Nm; N->TryGetStringField(TEXT("name"), Nm);
				if (Nodes.IsValidIndex(Ci)) Canopy[Ci] = Canopy[I] || Nm.StartsWith(TEXT("KERB/CANOPY"));
				NodeParent[Ci] = I; Walk(Ci, World, bDoor);
			}
	};
	for (const TSharedPtr<FJsonValue>& R : J->GetArrayField(TEXT("roots"))) Walk(int32(R->AsNumber()), FTransform::Identity, false);
	ADP_LOG(TEXT("Babylon model %s placed: %d objects, %d meshes"), *Name, Placed, ByMesh.Num());
	// every node kept, for the parts code moves (BindGroup)
	NName.SetNum(Nodes.Num()); NMesh.Init(INDEX_NONE, Nodes.Num()); NRoot = NodeWorld; NKids.SetNum(Nodes.Num()); NInst = NodeInst;
	for (int32 I = 0; I < Nodes.Num(); ++I)
	{
		const TSharedPtr<FJsonObject> N = Nodes[I]->AsObject();
		N->TryGetStringField(TEXT("name"), NName[I]);
		int32 M; if (N->TryGetNumberField(TEXT("mesh"), M)) NMesh[I] = M;
		const TArray<TSharedPtr<FJsonValue>>* C;
		if (N->TryGetArrayField(TEXT("children"), C)) for (const TSharedPtr<FJsonValue>& V : *C) NKids[I].Add(int32(V->AsNumber()));
	}

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
			Door.Id = NameOf(D);
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
			// the entrance doors' leaves wear the no-smoking sticker
			if (Door.Id.Contains(TEXT("-ENTR-"))) for (FPart& P : Door.Parts) if (P.Slide != 0) AddSticker(P);
			if (Door.Parts.Num()) Doors.Add(MoveTemp(Door));
		}
		ADP_LOG(TEXT("Babylon model %s: %d doors that move"), *Name, Doors.Num());
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
		ADP_LOG(TEXT("Babylon model %s: %d gate screens"), *Name, GateScreens.Num());
		LightFittings(Nodes, NodeWorld, NodeInst);
		ReplaceSeats(Nodes, NodeWorld, NodeInst);
	}
	ToneDown();
	MatchSurfaces();
	return Placed > 0;
}

namespace
{
	// the terminal's materials: the true-Unreal ones (/Game/Terminal/Materials/MI_T_<name>, Tools/set_terminal_materials.py:
	// BC, M, R, EB) or the Babylon-exported (BaseColorFactor, MetallicFactor, RoughnessFactor, EmissiveStrength); the
	// adjustments below work on either, by the material's own name
	FString MatName(const UMaterialInterface* M)
	{
		const UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(M);
		FString N = D && D->Parent ? D->Parent->GetName() : M ? M->GetName() : FString();
		if (N.StartsWith(TEXT("MI_T_"))) N.RightChopInline(5);
		return N;
	}
	bool GetBaseColor(const UMaterialInterface* M, FLinearColor& C)
	{
		return M->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("BC")), C) || M->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("BaseColorFactor")), C);
	}
	bool GetGlow(const UMaterialInterface* M, float& S)
	{
		return M->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("EB")), S) || M->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("EmissiveStrength")), S);
	}
	void SetBaseColor(UMaterialInstanceDynamic* D, const FLinearColor& C) { D->SetVectorParameterValue(TEXT("BC"), C); D->SetVectorParameterValue(TEXT("BaseColorFactor"), C); }
	void SetGlow(UMaterialInstanceDynamic* D, float S) { D->SetScalarParameterValue(TEXT("EB"), S); D->SetScalarParameterValue(TEXT("EmissiveStrength"), S); }
	void SetMetalRough(UMaterialInstanceDynamic* D, float Metal, float Rough)
	{
		D->SetScalarParameterValue(TEXT("M"), Metal); D->SetScalarParameterValue(TEXT("MetallicFactor"), Metal);
		D->SetScalarParameterValue(TEXT("R"), Rough); D->SetScalarParameterValue(TEXT("RoughnessFactor"), Rough);
	}
	UMaterialInterface* TerminalMaterial(const TCHAR* N)
	{
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Terminal/Materials/MI_T_%s.MI_T_%s"), N, N))) return M;
		if (!FParse::Param(FCommandLine::Get(), TEXT("OldTerminal"))) return nullptr;
		return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Babylon/Terminal/terminal/Materials/%s.%s"), N, N));
	}
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
			const FString MN = MatName(M);
			const bool bLamp = MN.StartsWith(TEXT("glow")) || MN.EndsWith(TEXT("_LED")) || MN.StartsWith(TEXT("fids-")) || MN.StartsWith(TEXT("pic_"));
			const EBlendMode Blend = M->GetBlendMode();
			if (!bLamp && Blend == BLEND_Opaque)
			{
				FLinearColor Base;
				float Strength = 0;
				const bool bWhite = GetBaseColor(M, Base) && Base.GetLuminance() > 0.8f;
				const bool bGlow = GetGlow(M, Strength) && Strength > 1.f;
				if (bWhite || bGlow)
				{
					UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(M, this);
					if (bWhite) { SetBaseColor(D, Base * (0.78f / Base.GetLuminance())); ++Whites; }
					if (bGlow) { SetGlow(D, Strength * 0.5f); ++Glows; }
					Out = D;
				}
			}
			Done.Add(M, Out);
			if (Out) K->SetMaterial(S, Out);
		}
	ADP_LOG(TEXT("Babylon model: %d whites toned to paint, %d glows halved"), Whites, Glows);
}

void ABabylonModel::MatchSurfaces()
{
	// (owner, 2026-11-11) the entrance canopies' undersides aren't lamps: matte, as the kerb's departures sign (whiteGloss,
	// toned as paint, no glow); the entrance doors' glass the same glass as the big windows on the terminal's back
	UMaterialInterface* White = TerminalMaterial(TEXT("whiteGloss"));
	UMaterialInterface* Facade = TerminalMaterial(TEXT("glassOuter"));
	UMaterialInstanceDynamic* Matte = nullptr;
	if (White)
	{
		Matte = UMaterialInstanceDynamic::Create(White, this);
		FLinearColor Base;
		if (GetBaseColor(Matte, Base) && Base.GetLuminance() > 0.78f) SetBaseColor(Matte, Base * (0.78f / Base.GetLuminance()));
		SetGlow(Matte, 0.f);
	}
	int32 Canopies = 0, Glass = 0;
	for (const auto& It : ByMesh)
	{
		UInstancedStaticMeshComponent* K = It.Value;
		if (!K) continue;
		const bool bCanopy = It.Key.EndsWith(TEXT("#canopy")), bDoor = It.Key.EndsWith(TEXT("#door"));
		if (!bCanopy && !bDoor) continue;
		for (int32 S = 0; S < K->GetNumMaterials(); ++S)
		{
			UMaterialInterface* M = K->GetMaterial(S);
			const FString MN = MatName(M);
			if (bCanopy && Matte && MN.StartsWith(TEXT("glow"))) { K->SetMaterial(S, Matte); ++Canopies; }
			if (bDoor && Facade && MN == TEXT("glass")) { K->SetMaterial(S, Facade); ++Glass; }
		}
	}
	ADP_LOG(TEXT("Babylon model: %d canopy undersides matte, %d door glass panes as the facade's"), Canopies, Glass);
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
	ADP_LOG(TEXT("Babylon model %s: %d lights"), *Name, Made);
}

void ABabylonModel::ReplaceSeats(const TArray<TSharedPtr<FJsonValue>>& Nodes, const TArray<FTransform>& NodeWorld, const TArray<TPair<UInstancedStaticMeshComponent*, int32>>& NodeInst)
{
	const TCHAR* Dir = TEXT("/Game/Babylon/GateSeat/gateseat/StaticMeshes/");
	UStaticMesh* SeatM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%sSeatModule.SeatModule"), Dir));
	UStaticMesh* LegM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%sLegModule.LegModule"), Dir));
	UStaticMesh* ArmM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%sArmModule.ArmModule"), Dir));
	if (!SeatM || !LegM || !ArmM) { ADP_WARN(TEXT("Seats: the new seating isn't imported")); return; }
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
		SetBaseColor(Navy, FLinearColor(FColor(0x1C, 0x2D, 0x52)));
		Blue = Navy;
	}
	UMaterialInstanceDynamic* Chrome = Steel ? UMaterialInstanceDynamic::Create(Steel, this) : nullptr;
	if (Chrome) { SetBaseColor(Chrome, FLinearColor(0.75f, 0.75f, 0.75f)); SetMetalRough(Chrome, 1.f, 0.36f); }
	UMaterialInstanceDynamic* Dark = Steel ? UMaterialInstanceDynamic::Create(Steel, this) : nullptr;
	if (Dark) { SetBaseColor(Dark, FLinearColor(0.015f, 0.015f, 0.015f)); SetMetalRough(Dark, 0.f, 0.5f); }
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
	ADP_LOG(TEXT("Seats: %d rows, %d places in the new seating"), Rows, Places);
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
	ADP_LOG(TEXT("Lounge fire lit at %s"), *FireWorld.ToString());
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
	ADP_LOG(TEXT("Lounge fireplace %.1f m deeper"), Depth);
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
	ADP_LOG(TEXT("Babylon model lights: %d downlights, %d room lights, %d jetway lights, %d glowing materials"), Spots, Soft, Strip, Glows);
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
	// the sticker rides on the leaf (the leaf's frame without the box's own scale)
	if (P.Sticker) { FTransform F = L; F.SetScale3D(FVector(1.f)); P.Sticker->SetRelativeTransform(P.StickerRel * F * P.Parent); }
}

void ABabylonModel::AddSticker(FPart& Leaf)
{
	static UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Signs/M_NoSmoking.M_NoSmoking"));
	static UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!M || !Plane) { static bool bTold = false; if (!bTold) { bTold = true; ADP_WARN(TEXT("No-smoking sticker: /Game/Signs/M_NoSmoking is missing (Tools/set_no_smoking_sign.py)")); } return; }
	UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(this);
	S->SetupAttachment(Root);
	S->SetMobility(EComponentMobility::Movable);
	S->SetStaticMesh(Plane);
	S->SetMaterial(0, M);
	S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	S->SetCastShadow(false);
	S->RegisterComponent();
	AddInstanceComponent(S);
	// on the leaf's middle line, 1.5 m above the door's floor (the leaf's own height is its box's centre), standing in
	// the glass (the plane's face turned from up to the leaf's normal), 24 cm across
	const float Up = 150.f - float(Leaf.Base.GetTranslation().Z);
	Leaf.StickerRel = FTransform(FRotator(0.f, 0.f, 90.f), FVector(0.f, 0.f, Up), FVector(0.24f));
	Leaf.Sticker = S;
	FTransform F = Leaf.Base; F.SetScale3D(FVector(1.f));
	S->SetRelativeTransform(Leaf.StickerRel * F * Leaf.Parent);
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

FString ABabylonModel::NodeAt(const UPrimitiveComponent* C, int32 Item) const
{
	const TArray<FString>* Names = InstNames.Find(Cast<UInstancedStaticMeshComponent>(C));
	return Names && Names->IsValidIndex(Item) ? (*Names)[Item] : FString();
}

#pragma region MovingParts
int32 ABabylonModel::MakeGroup(const TArray<int32>& Picked, const FTransform& Pivot)
{
	FGroup G;
	TSet<int32> Seen;
	TArray<int32> Stack = Picked;
	while (Stack.Num())
	{
		const int32 I = Stack.Pop();
		if (!NRoot.IsValidIndex(I) || Seen.Contains(I)) continue;
		Seen.Add(I);
		if (NInst.IsValidIndex(I) && NInst[I].Key) G.Inst.Add({NInst[I].Key, NInst[I].Value, NRoot[I].GetRelativeTransform(Pivot)});
		Stack.Append(NKids[I]);
	}
	if (!G.Inst.Num()) return INDEX_NONE;
	return Groups.Add(MoveTemp(G));
}

int32 ABabylonModel::BindGroup(const TArray<FString>& Names, bool bPrefix, const FTransform& Pivot)
{
	TArray<int32> Picked;
	for (int32 I = 0; I < NName.Num(); ++I)
		for (const FString& W : Names)
			if (NName[I] == W || NName[I].StartsWith(W + TEXT(".")) || (bPrefix && NName[I].StartsWith(W))) { Picked.Add(I); break; }
	return MakeGroup(Picked, Pivot);
}

int32 ABabylonModel::BindGroupWhere(TFunctionRef<bool(const FString& Name, int32 Mesh, const FVector& At)> Pick, const FTransform& Pivot)
{
	TArray<int32> Picked;
	for (int32 I = 0; I < NName.Num(); ++I) if (Pick(NName[I], NMesh[I], NRoot[I].GetLocation())) Picked.Add(I);
	return MakeGroup(Picked, Pivot);
}

void ABabylonModel::MoveGroup(int32 Group, const FTransform& NewPivot)
{
	if (!Groups.IsValidIndex(Group)) return;
	if (NewPivot.ContainsNaN() || !NewPivot.IsValid())
	{
		static int32 Told = 0;
		if (Told++ < 5) ADP_ERROR(TEXT("Babylon model %s: moving part group %d given a broken transform %s"), *GetName(), Group, *NewPivot.ToString());
		return;
	}
	for (const FInst& In : Groups[Group].Inst)
	{
		In.K->UpdateInstanceTransform(In.I, In.Rel * NewPivot, false, false, true);
		MovedLists.Add(In.K);
	}
}

void ABabylonModel::FlushMoves()
{
	for (UInstancedStaticMeshComponent* K : MovedLists) if (K) K->MarkRenderStateDirty();
	MovedLists.Reset();
}

void ABabylonModel::InstancesNamed(const FString& Name, bool bPrefix, TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>>& Out) const
{
	for (const auto& P : InstNames)
		for (int32 I = 0; I < P.Value.Num(); ++I)
		{
			const FString& N = P.Value[I];
			const bool bHit = bPrefix ? N.StartsWith(Name) : (N == Name || (N.StartsWith(Name + TEXT(".")) && N.Len() == Name.Len() + 4));
			if (bHit) Out.Add(MakeTuple(const_cast<UInstancedStaticMeshComponent*>(P.Key), I, N));
		}
}

bool ABabylonModel::NodeBase(const FString& Name, FTransform& Out) const
{
	const int32 I = NName.IndexOfByKey(Name);
	if (I == INDEX_NONE) return false;
	Out = NRoot[I];
	return true;
}

void ABabylonModel::NodesNamed(const FString& Name, TArray<FTransform>& Out) const
{
	const FString Dot = Name + TEXT(".");
	for (int32 I = 0; I < NName.Num(); ++I)
		if (NName[I] == Name || (NName[I].StartsWith(Dot) && NName[I].Len() == Dot.Len() + 3 && FCString::IsNumeric(*NName[I].RightChop(Dot.Len()))))
			Out.Add(NRoot[I]);
}

void ABabylonModel::SetPartsCollision(bool bOn)
{
	TArray<UInstancedStaticMeshComponent*> Parts;
	GetComponents(Parts);
	for (UInstancedStaticMeshComponent* K : Parts) K->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}
#pragma endregion
