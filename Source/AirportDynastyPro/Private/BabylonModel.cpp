// Places the Babylon model: walks its node tree from the roots, works out each node's transform the way Unreal's glTF
// importer converts them, and adds an instance of the node's imported mesh there.
#include "BabylonModel.h"
#include "Site.h"
#include "PerfLog.h"
#include "ParkedAircraft.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
static FString GRoadsProbe;   // (the road network's data is there: the model's own landside steps aside)
#include "Misc/PackageName.h"
#include "ADPLog.h"
#include "PaxNav.h"
#include "TerminalDoors.h"
#include "JetwayRigs.h"
#include "SiteNames.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "People.h"
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
#include "ADPKeep.h"

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

	bool bNodeTransforms = false; J->TryGetBoolField(TEXT("nodeTransforms"), bNodeTransforms);
	int32 Placed = 0;
	// every node's place in the model, its parent, and its mesh instance (for the doors)
	TArray<FTransform> NodeWorld; NodeWorld.SetNum(Nodes.Num());
	TArray<int32> NodeParent; NodeParent.Init(INDEX_NONE, Nodes.Num());
	TArray<TPair<UInstancedStaticMeshComponent*, int32>> NodeInst; NodeInst.Init(TPair<UInstancedStaticMeshComponent*, int32>(nullptr, INDEX_NONE), Nodes.Num());
	// the entrance canopies' parts (KERB/CANOPY-*): in lists of their own, so their underside can be given a matte finish
	TArray<bool> Canopy; Canopy.Init(false, Nodes.Num());
	NodeInTree.Init(false, Nodes.Num());
	constexpr float StandsOut = 12.f;   // metres the stands moved out (Data/airfield.json standsNote)
	TFunction<void(int32, const FTransform&, bool)> Walk;
	TArray<bool> Wing; Wing.SetNumZeroed(Nodes.Num());   // (a revolving door's wings: lists of their own, you are never stopped by them - ATerminalDoors carries you)
	Walk = [&](int32 I, const FTransform& Parent, bool bDoor)
	{
		if (!Nodes.IsValidIndex(I)) return;
		const TSharedPtr<FJsonObject> N = Nodes[I]->AsObject();
		FTransform L = Local(N);
		{ FString Nm; N->TryGetStringField(TEXT("name"), Nm);
		  if (Name == TEXT("Terminal") && Nm.StartsWith(TEXT("STAND-")) && Nm.EndsWith(TEXT("/AIRCRAFT"))) L.AddToTranslation(FVector(0, StandsOut * 100.f, 0)); }
		// the revolving doors' drum: its two curved glass sides stood across the openings (the kerb's and the lobby's), so
		// nobody could go through - a quarter round, its openings through the wall (as ATerminalDoors, 2026-11-21)
		if (bDoor) { FString Nm; N->TryGetStringField(TEXT("name"), Nm); if (Nm.StartsWith(TEXT("drum"))) L.SetRotation(L.GetRotation() * FQuat(FVector::UpVector, HALF_PI)); }
		const FTransform World = L * Parent;
		NodeWorld[I] = World; NodeInTree[I] = true;
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
		  // his own A320 parks on the stands now (owner, 2026-11-15: "replace all of the custom airplanes at the apron":
		  // ParkedAircraft.h): the model's own parked aircraft step aside when it is imported
		  if (Name == TEXT("Terminal") && Nm.StartsWith(TEXT("STAND-")) && Nm.EndsWith(TEXT("/AIRCRAFT")) && ADPParked::HaveModel()) return;
		  // the model's own landside - its road, markings, kerb, car park with its lines, parked cars and trees - is the road
		  // network's now (owner, 2026-11-14: the roads from his blueprint, RoadNetwork.cpp); its canopies, benches and the
		  // departures sign (KERB/) stay, on the road network's 16 m terminal pavement
		  if (Name == TEXT("Terminal") && (Nm == TEXT("SITE/ROAD") || Nm == TEXT("SITE/ROAD-MARKING") || Nm == TEXT("SITE/KERB") || Nm == TEXT("SITE/KERB-EDGE")
		      || Nm == TEXT("SITE/CARPARK") || Nm == TEXT("SITE/PARKING-LINES") || Nm.StartsWith(TEXT("SITE/CAR-")) || Nm.StartsWith(TEXT("SITE/TREE-")))
		      && FFileHelper::LoadFileToString(GRoadsProbe, *ADPSite::File(TEXT("roads.json")))) return;
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
			N->TryGetStringField(TEXT("asset"), Asset);   // (the imported file's own name when it is neither: Tools/glb_nodes.py)
			// the native terminal: this copy's piece, and where the copy sat against it
			FVector PieceOff = FVector::ZeroVector;
			const bool bPiece = Pieces.Num() > 0;
			if (const TPair<FString, FVector>* Pc = bPiece ? Pieces.Find(Asset) : nullptr) { Asset = Pc->Key; PieceOff = Pc->Value; }
			// (owner, 2026-11-14: always the part's own imported name) - the layout names it after its glTF mesh or after its
			// object; when that file isn't there but the other naming is, the other one is the imported part (logged)
			bool bNodeNamed = bByNode;
			if (!bPiece)
			{
				auto Exists = [&](const FString& A) { return FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/Babylon/%s/%s/StaticMeshes/%s"), *Name, *Name.ToLower(), *A)); };
				if (!Exists(Asset))
				{
					FString Other = FString::Printf(TEXT("%s_mesh_%d"), *Name.ToLower(), Mesh);
					if (!bByNode) N->TryGetStringField(TEXT("name"), Other);
					if (Exists(Other))
					{
						static TSet<FString> Told; if (!Told.Contains(Name)) { Told.Add(Name); ADP_WARN(TEXT("Babylon model %s: its layout names parts %s, the imported files are named %s - using the imported names (re-run Tools/glb_nodes.py after importing)"), *Name, bByNode ? TEXT("by object") : TEXT("by mesh"), bByNode ? TEXT("by mesh") : TEXT("by object")); }
						Asset = Other; bNodeNamed = !bByNode;
					}
					else ADP_WARN(TEXT("Babylon model %s: no imported part %s"), *Name, *Asset);
				}
			}
			// a door's parts in lists of their own (they move; the big shared lists never need rebuilding)
			const FString Key = Wing[I] ? Asset + TEXT("#door#wing") : bDoor ? Asset + TEXT("#door") : Canopy[I] ? Asset + TEXT("#canopy") : Asset;
			if (TObjectPtr<UInstancedStaticMeshComponent>* F = ByMesh.Find(Key)) K = *F;
			else
			{
				const FString MeshPath = bPiece ? FString::Printf(TEXT("/Game/Terminal/Meshes/%s.%s"), *Asset, *Asset)
					: FString::Printf(TEXT("/Game/Babylon/%s/%s/StaticMeshes/%s.%s"), *Name, *Name.ToLower(), *Asset, *Asset);
				if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *MeshPath))
				{
					K = NewObject<UInstancedStaticMeshComponent>(this);
					K->SetupAttachment(Root);
					if (!bDoor && !Canopy[I] && FParse::Param(FCommandLine::Get(), TEXT("ListsStatic"))) K->SetMobility(EComponentMobility::Static);   // (test)
					K->SetStaticMesh(SM); // with the materials and textures it was imported with
					K->SetCollisionProfileName(TEXT("BlockAll"));
					if (Wing[I]) { K->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); K->SetCanEverAffectNavigation(false); K->SetForceDisableNanite(true); }
					K->RegisterComponent();
					AddInstanceComponent(K);
				}
				ByMesh.Add(Key, K);
			}
			// relative to the model, so its turn applies; meshes imported one per object already carry their place (Unreal's
			// importer builds each object's position into its mesh), so they go in as they are
			if (K)
			{
				// ("nodeTransforms": the coach - its shared parts are one mesh each, copied about by their nodes: every node by its own place)
				FTransform At = bNodeNamed && !bNodeTransforms ? FTransform::Identity : FTransform(PieceOff) * World;
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
				if (Nodes.IsValidIndex(Ci)) Wing[Ci] = Wing[I] || (bDoor && Nm.StartsWith(TEXT("wings")));
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
			if (!NameOf(D).StartsWith(TEXT("D-")) || !NodeInTree[D]) continue;
			FDoor Door;
			Door.Id = NameOf(D);
			Door.At = NodeWorld[D].GetLocation();
			const TArray<int32> C = Kids(D);
			// the width from the frame's posts (at ±(w/2 + 0.05)); sliding leaves are the thin panels (depth up to 0.06 m)
			float W = 1.8f;
			for (const int32 K : C) if (NameOf(K).StartsWith(TEXT("box")) && Num3(K, TEXT("scale"), 2) > 0.3 && Num3(K, TEXT("scale"), 1) > 1.5) W = FMath::Max(W, 2.f * (FMath::Abs(float(Num3(K, TEXT("translation"), 0))) - 0.05f));
			Door.Reach = FMath::Max(2.6f, W * 0.7f);
			// (a door on hinges has no sliding leaves: a staff door's card reader - a thin little box beside the frame - was
			//  taken for one, so 21 staff and ramp doors opened by themselves, 2026-11-23; a leaf is half a metre or more)
			const bool bHinged = C.ContainsByPredicate([&](const int32 K) { return NameOf(K).StartsWith(TEXT("hinge")); });
			for (const int32 K : C)
			{
				const FString N = NameOf(K);
				const double X = Num3(K, TEXT("translation"), 0);
				if (!bHinged && FMath::Max(Num3(K, TEXT("scale"), 0), Num3(K, TEXT("scale"), 1)) > 0.5 && N.StartsWith(TEXT("box")) && Num3(K, TEXT("scale"), 2) <= 0.061 && FMath::Abs(X) > 0.01 && Num3(K, TEXT("translation"), 1) > 0.5)
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
			for (const FPart& Pt : Door.Parts) if (Pt.Slide != 0) Door.bSlide = true;
			// (a door passengers go through by themselves - sliding leaves only, never the staff's, the apron's or the jetways')
			Door.bPax = Name == TEXT("Terminal") && Door.bSlide && !Door.Parts.ContainsByPredicate([](const FPart& Pt) { return Pt.Swing != 0; }) && !Door.Id.Contains(TEXT("STAFF"))
				&& (Door.Id.Contains(TEXT("-ENTR-")) || Door.Id.Contains(TEXT("-ARRIVALS-")) || Door.Id.Contains(TEXT("-SECURITY-")) || Door.Id.StartsWith(TEXT("D-F1-AIRSIDE-"))
				// gate 13's swing lobby, on everyone's way down to gates 13-16 (2026-11-24): a passenger door, not a staff one
				|| Door.Id.Contains(TEXT("-SWING-")));
			Door.AtW = Root->GetComponentTransform().TransformPosition(Door.At);
			// (a gate's jetway door: for passengers only while its jetway is docked and a flight boards or gets off there)
			if (Door.bSlide && Door.Id.StartsWith(TEXT("D-F2-GATE-")) && Door.Id.EndsWith(TEXT("-JW")))
			{ const int32 Gn = FCString::Atoi(*Door.Id.Mid(10)); if (Gn > 0 && Gn <= 32) { Door.JwGate = Gn; GateDoorK[Gn] = 0.f; } }
			if (Door.Parts.Num()) Doors.Add(MoveTemp(Door));
		}
		ADP_LOG(TEXT("Babylon model %s: %d doors that move"), *Name, Doors.Num());
		if (Doors.Num()) AllModels.AddUnique(this);   // (its hinged doors open with E / X: UseDoor)
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
	// (the airport's name on its pictures, the active site's - a few seconds in: drawn at once, the pictures were still white)
	if (Name == TEXT("Jetways") || Name == TEXT("Firestation") || Name.StartsWith(TEXT("Office")))
	{
		FTimerHandle Th; TWeakObjectPtr<ABabylonModel> Wk(this); const FString Mn = Name;
		GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateLambda([Wk, Mn]() { if (ABabylonModel* B = Wk.Get()) ADPSiteNames::Apply(B, Mn); }), 8.f, false);
	}
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
			if (MN.StartsWith(TEXT("roof"))) { Done.Add(M, nullptr); continue; }   // (the owner's roof photo textures as they are: Tools/set_roof.py)
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
		const bool bCanopy = It.Key.EndsWith(TEXT("#canopy")), bDoor = It.Key.Contains(TEXT("#door"));
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
		// (to-do 149, 2026-11-20: no shadows from a model's own lamps either - the light rule: the sun and the sky keep theirs.
		// Unreal lights cast shadows unless told not to: these were 45 shadow passes every frame, everywhere;
		// -ModelLightShadows: as before, for measuring)
		P->SetCastShadows(FParse::Param(FCommandLine::Get(), TEXT("ModelLightShadows")));
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
		if (!NameOf(R).Contains(TEXT("/SEATS-")) || !NodeInTree.IsValidIndex(R) || !NodeInTree[R]) continue;
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
		if (!NameOf(R).Contains(TEXT("/SEATS-")) || !NodeInTree.IsValidIndex(R) || !NodeInTree[R]) continue;
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
		// (a curved row - the gates' curved windows, 2026-11-23 - stands on a circle of this radius, each seat facing straight out)
		double Arc = 0.0; Nodes[R]->AsObject()->TryGetNumberField(TEXT("arc"), Arc);
		auto Put = [&](UInstancedStaticMeshComponent* K, float BabylonX)
		{
			FTransform At(FVector(-BabylonX * 100.f, 0, 0));
			if (Arc > 1.0) { const double A = BabylonX / Arc; At = FTransform(FRotator(0, FMath::RadiansToDegrees(A), 0), FVector(-Arc * FMath::Sin(A), Arc * (FMath::Cos(A) - 1.0), 0) * 100.0); }
			K->AddInstance(At * Row, false);
		};
		for (int32 i = 0; i < N; ++i) Put(Seats, -L / 2 + 0.62f * (i + 0.5f));
		for (int32 k = 0; k <= N; ++k) Put(k == 0 || k == N || k % 3 == 0 ? Legs : Arms, -L / 2 + 0.62f * k);
		++Rows; Places += N;
	}
	for (UInstancedStaticMeshComponent* K : {Seats, Legs, Arms}) K->MarkRenderStateDirty();
	if (Rows) ADP_LOG(TEXT("Seats: %d rows, %d places in the new seating"), Rows, Places);
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
		if (FParse::Param(FCommandLine::Get(), TEXT("NoLampLights"))) { L->MarkAsGarbage(); return; }   // (-NoLampLights: test)
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
	// merged lamp lights (owner, 2026-11-17: "make the lights copies of each other" - every light costs each frame, the
	// 16 gates brought 1100 more): the lamps' faces still all glow, but the lamps of one ceiling within a 12 m square share
	// one big soft panel of light, as bright as all of theirs together; ceiling heights apart never share
	struct FLampCell { FBox Box = FBox(ForceInit); float Cd = 0.f, Reach = 0.f; FLinearColor Col = FLinearColor::White; int32 N = 0; };
	TMap<FIntVector, FLampCell> Cells;
	auto Pool = [&](const FBox& Face, float Cd, float Reach, const FLinearColor& Col)
	{
		const FVector C = Face.GetCenter();
		FLampCell& Cl = Cells.FindOrAdd(FIntVector(FMath::FloorToInt(C.X / 1200.f), FMath::FloorToInt(C.Y / 1200.f), FMath::RoundToInt(C.Z / 50.f)));
		Cl.Box += Face; Cl.Cd += Cd; Cl.Reach = FMath::Max(Cl.Reach, Reach); Cl.Col = Col; ++Cl.N;
	};
	// the lamps' faces drawn with the owner's lamp texture (M_WalkLamp, glowing): a flat panel just under each fitting
	// (the imported fittings have no texture coordinates), the fitting's own box hidden. One list of panels per kind.
	UMaterialInterface* WalkLampM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/WalkLamp/M_WalkLamp.M_WalkLamp"));
	UStaticMesh* PlaneM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	auto Panels = [&](float Along, float Across, float Lo, float Hi, float Round) -> UInstancedStaticMeshComponent*
	{
		if (!WalkLampM || !PlaneM || FParse::Param(FCommandLine::Get(), TEXT("NoLampPanels"))) return nullptr;   // (-NoLampPanels: test)
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this);
		K->SetupAttachment(Root);
		K->SetStaticMesh(PlaneM);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(WalkLampM, this);
		D->SetScalarParameterValue(TEXT("Along"), Along); D->SetScalarParameterValue(TEXT("Across"), Across);
		D->SetScalarParameterValue(TEXT("Lo"), Lo); D->SetScalarParameterValue(TEXT("Hi"), Hi); D->SetScalarParameterValue(TEXT("Round"), Round);
		K->SetMaterial(0, D);
		K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		K->SetCastShadow(false);
		// (-PanelsLumen: the old way, in Lumen's scene - to compare)
		if (!FParse::Param(FCommandLine::Get(), TEXT("PanelsLumen")))
		{
			K->bAffectDynamicIndirectLighting = false; K->bAffectDistanceFieldLighting = false; K->bVisibleInRayTracing = false;
		}
		// fixed, never movable (2026-11-17, the 16 gates' lag): these lists span the whole building and never move; made
		// at run time they were "movable", and a movable list that big is checked against every light every frame - with
		// the 16 gates' lamps that cost 27 -> 40 fps (stand test)
		if (!FParse::Param(FCommandLine::Get(), TEXT("PanelsMovable"))) K->SetMobility(EComponentMobility::Static);
		K->RegisterComponent();
		AddInstanceComponent(K);
		PanelLists.Add(K);
		return K;
	};
	// a lamp turned by anything but quarter turns, its long side along its own x: its own size (cm)
	auto LampTurned = [&](int32 C, FVector& Own) -> bool
	{
		const float Yaw = FRotator::NormalizeAxis(NodeWorld[C].Rotator().Yaw);
		if (FMath::Abs(Yaw - 90.f * FMath::RoundToFloat(Yaw / 90.f)) < 0.2f) return false;
		Own = NodeInst[C].Key->GetStaticMesh()->GetBoundingBox().GetSize() * NodeWorld[C].GetScale3D().GetAbs();
		return Own.X > Own.Y;
	};
	// a fitting's box (node C) hidden and a panel put just under it, the panel's U along the fitting's long side; the box's
	// bounds back (in the model's space)
	auto PanelUnder = [&](UInstancedStaticMeshComponent* K, int32 C) -> FBox
	{
		UInstancedStaticMeshComponent* Shared = NodeInst[C].Key;
		const FBox B = Shared->GetStaticMesh()->GetBoundingBox().TransformBy(NodeWorld[C]);
		if (!K) return B;
		FTransform T;
		if (!FParse::Param(FCommandLine::Get(), TEXT("KeepLampBoxes")) && Shared->GetInstanceTransform(NodeInst[C].Value, T, false))   // (-KeepLampBoxes: test)
		{
			FTransform Gone = T; Gone.SetScale3D(FVector(0.0001f));
			Shared->UpdateInstanceTransform(NodeInst[C].Value, Gone, false, true);
		}
		const FVector Ext = B.GetExtent();
		const bool bAlongX = Ext.X >= Ext.Y;
		// (a lamp turned with the gates' curved windows: its own length, width and turn)
		if (FVector Own; LampTurned(C, Own))
		{
			FTransform At(FQuat(FRotator(0.f, NodeWorld[C].Rotator().Yaw, 0.f)) * FQuat(FRotator(0.f, 0.f, 180.f)), FVector(B.GetCenter().X, B.GetCenter().Y, B.Min.Z - 0.3f), FVector(Own.X / 100.f, Own.Y / 100.f, 1.f));
			if (K->Mobility == EComponentMobility::Static) At = At * Root->GetRelativeTransform();
			K->AddInstance(At, false);
			return B;
		}
		// the plane faces up: turned over (roll 180) to face down, and a quarter turn when the fitting runs along Y
		const FRotator Rot(0.f, bAlongX ? 0.f : 90.f, 180.f);
		const FVector Scale(2.f * FMath::Max(Ext.X, Ext.Y) / 100.f, 2.f * FMath::Min(Ext.X, Ext.Y) / 100.f, 1.f);
		FTransform At(Rot, FVector(B.GetCenter().X, B.GetCenter().Y, B.Min.Z - 0.3f), Scale);
		// the lists are fixed in place (static, for speed - 2026-11-17), and a fixed list doesn't take the model root's half
		// turn the way everything else under the root does: every room lamp's panel stood half a turn round the terminal,
		// out in the sky in front of it, while its fitting hung in the room (owner, 2026-11-18: "a ton of squared lights in
		// the sky outside"). The root's own turn given to the panel itself puts it right under its fitting again.
		if (K->Mobility == EComponentMobility::Static) At = At * Root->GetRelativeTransform();
		K->AddInstance(At, false);
		return B;
	};
	UInstancedStaticMeshComponent* WalkLamps = nullptr;   // the walkway's strips (made when the first is met)
	TMap<FIntPoint, UInstancedStaticMeshComponent*> LampPanels;   // the rooms' lamps, one list per size
	for (int32 I = 0; I < Nodes.Num(); ++I)
	{
		if (NodeInTree.IsValidIndex(I) && !NodeInTree[I]) continue;   // (taken out of the layout)
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
			FLampRec& Rec = LampRecs.AddDefaulted_GetRef(); { FString Rm = N.Left(N.Find(TEXT("/LAMP-"))); int32 Sl; if (Rm.FindLastChar(TEXT('/'), Sl)) Rm.RightChopInline(Sl + 1); Rec.Room = Rm; }
			if (Nodes[I]->AsObject()->TryGetArrayField(TEXT("children"), Kids))
				for (const TSharedPtr<FJsonValue>& KV : *Kids)
				{
					const int32 C = int32(KV->AsNumber());
					if (!NodeInst.IsValidIndex(C) || !NodeInst[C].Key) continue;
					const FBox B = NodeInst[C].Key->GetStaticMesh()->GetBoundingBox().TransformBy(NodeWorld[C]);
					float Len = 2.f * FMath::Max(B.GetExtent().X, B.GetExtent().Y), Wid = 2.f * FMath::Min(B.GetExtent().X, B.GetExtent().Y);
					if (FVector Own; LampTurned(C, Own)) { Len = Own.X; Wid = Own.Y; }
					// one list of panels per lamp size (the pattern repeats along the lamp every two thirds of its width)
					const FIntPoint Key(FMath::RoundToInt(Len), FMath::RoundToInt(Wid));
					UInstancedStaticMeshComponent*& K = LampPanels.FindOrAdd(Key);
					if (!K) K = Panels(Len / FMath::Max(1.f, Wid * 2.f / 3.f), 1.f, 0.f, 1.f, 0.f);
					Face = PanelUnder(K, C);
					if (K) { LampRecs.Last().Panels.Add(TPair<UInstancedStaticMeshComponent*, int32>(K, K->GetInstanceCount() - 1)); LampRecs.Last().Fits.Add(NodeInst[C]); }
					LampRecs.Last().Face = Face;
				}
			// (each lamp once lit half the room's share: every other one had a light of 220 cd - now pooled)
			// (-LampPerLamp: the old way, one light per two lamps - to compare)
			static const bool bPerLamp = !FParse::Param(FCommandLine::Get(), TEXT("LampPool"));   // (pooled big lights measured slower, 2026-11-17: kept only as a test switch)
			if (bPerLamp && (Lamp++ & 1) == 0)
			{
				URectLightComponent* L = NewObject<URectLightComponent>(this);
				const FVector Ext = Face.IsValid ? Face.GetExtent() : FVector(60, 30, 2);
				const bool bAlongX = Ext.X >= Ext.Y;
				L->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector(0, 0, -1), bAlongX ? FVector(1, 0, 0) : FVector(0, 1, 0)).Rotator());
				L->SetSourceHeight(2.f * FMath::Max(Ext.X, Ext.Y)); L->SetSourceWidth(2.f * FMath::Min(Ext.X, Ext.Y)); L->SetBarnDoorAngle(88.f);
				Add(L, Face.IsValid ? FVector(Face.GetCenter().X, Face.GetCenter().Y, Face.Min.Z - 1.f) : NodeWorld[I].GetLocation() - FVector(0, 0, 5.f), 220.f, 900.f, Warm);
				if (LampLights.Num() && LampLights.Last() == L) LampRecs.Last().Light = L;
				++Soft;
			}
			else if (!bPerLamp) { ++Lamp; if (Face.IsValid) { Pool(Face, 110.f, 900.f, Warm); ++Soft; } }
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
					static const bool bPerStrip = !FParse::Param(FCommandLine::Get(), TEXT("LampPool"));
					if (!bPerStrip) Pool(B, 260.f, 1100.f, FLinearColor(1.f, 0.96f, 0.9f));
					else
					{
						const FVector Ex2 = B.GetExtent(); const bool bX2 = Ex2.X >= Ex2.Y;
						URectLightComponent* L = NewObject<URectLightComponent>(this);
						L->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector(0, 0, -1), bX2 ? FVector(1, 0, 0) : FVector(0, 1, 0)).Rotator());
						L->SetSourceHeight(2.f * FMath::Max(Ex2.X, Ex2.Y)); L->SetSourceWidth(2.f * FMath::Min(Ex2.X, Ex2.Y)); L->SetBarnDoorAngle(88.f);
						Add(L, B.GetCenter() - FVector(0, 0, Ex2.Z + 1.f), 260.f, 1100.f, FLinearColor(1.f, 0.96f, 0.9f));
					}
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
				// (in the gates' curved bays, 2026-11-23: the corridor's first metres stand inside the gate room - their light down
				//  only; it threw a bright patch through the corridor's roof onto the bay's ceiling)
				const double PlanZ = -Root->GetComponentTransform().TransformPosition(At).Y / 100.0;
				UPointLightComponent* P;
				if (PlanZ > 82.0 && PlanZ < 85.5) { USpotLightComponent* Sp = NewObject<USpotLightComponent>(this); Sp->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); Sp->SetOuterConeAngle(80.f); Sp->SetInnerConeAngle(55.f); P = Sp; }
				else P = NewObject<UPointLightComponent>(this);
				P->SetSourceRadius(20.f);
				Add(P, At, 180.f, 600.f, FLinearColor(0.95f, 0.97f, 1.f));
				++Strip;
			}
		}
	}
	if (LampRecs.Num()) LampCheckIn = 2.f;   // (once everything stands and can be hit: CheckLamps)
	{ int32 Ni = 0; for (const TPair<FIntPoint, UInstancedStaticMeshComponent*>& P : LampPanels) if (P.Value) Ni += P.Value->GetInstanceCount();
	  ADP_LOG(TEXT("Lamp panels: %d lists, %d panels (+ walkway strips %d); material blend %d"), LampPanels.Num(), Ni, WalkLamps ? WalkLamps->GetInstanceCount() : 0,
		WalkLampM ? int32(WalkLampM->GetBlendMode()) : -1); }
	// the pooled lamp lights: one soft panel per cell, the cell's whole ceiling area, facing down
	for (const TPair<FIntVector, FLampCell>& It : Cells)
	{
		const FLampCell& Cl = It.Value;
		const FVector Ext = Cl.Box.GetExtent();
		const bool bAlongX = Ext.X >= Ext.Y;
		URectLightComponent* L = NewObject<URectLightComponent>(this);
		L->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector(0, 0, -1), bAlongX ? FVector(1, 0, 0) : FVector(0, 1, 0)).Rotator());
		L->SetSourceHeight(FMath::Max(60.f, 2.f * FMath::Max(Ext.X, Ext.Y)));
		L->SetSourceWidth(FMath::Max(30.f, 2.f * FMath::Min(Ext.X, Ext.Y)));
		L->SetBarnDoorAngle(88.f);
		Add(L, FVector(Cl.Box.GetCenter().X, Cl.Box.GetCenter().Y, Cl.Box.Min.Z - 1.f), Cl.Cd, Cl.Reach + FMath::Max(Ext.X, Ext.Y), Cl.Col);
	}
	ADP_LOG(TEXT("Babylon model %s: lamp lights pooled into %d (one per 12 m of ceiling)"), *GetName(), Cells.Num());
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
	if (P.bSpin && PaxNav::Making()) return;   // (a revolving door stands still while the walkers' map is made)
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
	(void)Touched;   // (no full rebuild: each moved instance already went to the GPU on its own - the full rebuild re-sent every part of these lists every frame, owner 2026-11-17 lag)
	// the sticker rides on the leaf (the leaf's frame without the box's own scale)
	if (P.Sticker) { FTransform F = L; F.SetScale3D(FVector(1.f)); P.Sticker->SetRelativeTransform(P.StickerRel * F * P.Parent); }
}

void ABabylonModel::AddSticker(FPart& Leaf)
{
	static UMaterialInterface* M = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Signs/M_NoSmoking.M_NoSmoking")));
	static UStaticMesh* Plane = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
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
	ADP_PERF_SCOPE("a Babylon model (each frame)", 2.f);
	FlickerFire();
	Super::Tick(Dt);
	if (LampCheckIn > 0.f && (LampCheckIn -= Dt) <= 0.f) CheckLamps();
	LampsByDaylight();
	if (!Doors.Num()) return;
	Clock += Dt;
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	// where you are, in the model's own space (feet height)
	FVector Me(1e9);
	if (P) Me = Root->GetComponentTransform().InverseTransformPosition(P->GetActorLocation() - FVector(0, 0, P->GetSimpleCollisionHalfHeight()));
	for (FDoor& D : Doors)
	{
		if (D.bSpin)
		{
			// (turned by the door's own works - the passengers' compartments: ATerminalDoors; alone, as doors.ts turns them)
			float Turned = Clock * 0.6f;
			if (const ATerminalDoors* Dr = ATerminalDoors::Find(GetWorld())) Dr->RevolvingTurn(D.Id, Turned);
			for (FPart& Part : D.Parts) if (Part.bSpin) MovePart(Part, 0, Turned);
			continue;
		}
		bool bNear = FVector2D::Distance(FVector2D(Me), FVector2D(D.At)) < D.Reach * 100.f && FMath::Abs(Me.Z - D.At.Z) < 250.f;
		if (D.bPax && !bNear)
		{
			// (the passengers' doors open for passengers too, as the old door objects did (TerminalDoors.cpp) - this terminal
			//  has none of those, so its doors opened for you only and the walkers stood at the closed leaves (2026-11-23):
			//  the entrances 5 m ahead and open while passengers are about, security open until all are through - and all of
			//  them open while the walkers' map is made)
			const bool bEntr = D.Id.Contains(TEXT("-ENTR-"));
			bNear = PaxNav::Making() || (ATerminalDoors::bHoldEntrances && bEntr) || (ATerminalDoors::bHoldSecurity && D.Id.Contains(TEXT("-SECURITY-")));
			const double Rr = FMath::Square(double(bEntr ? FMath::Max(D.Reach, 5.f) : D.Reach) * 100.0);
			for (int32 k = 0; k < ATerminalDoors::Openers.Num() && !bNear; ++k)
				bNear = FMath::Abs(ATerminalDoors::Openers[k].Z - D.AtW.Z) < 150.f && FVector::DistSquared2D(ATerminalDoors::Openers[k], D.AtW) < Rr;
		}
		// a gate's jetway corridor door (owner, to-do 234): never opens onto an undocked jetway - not for you, not for anyone;
		// docked and a flight boarding or getting off there (APaxFlow::GateDoors), it stands open from before the first one
		// reaches it until the last one, late ones too, is through; docked with nobody due, it opens as you come, like any
		// sliding door. Once open it only shuts with nobody in the doorway (below). Not for the walking map: the jetways are off it.
		if (D.JwGate)
		{
			const bool bDocked = JetwayDocked(D);
			if (!bDocked) { if (D.K <= 0.02f) bNear = false; }
			else if (GateDoorsOn & (uint32(1) << (D.JwGate - 1))) bNear = true;
			// (open: a passenger still in its doorway keeps it open, whatever the jetway does)
			for (int32 k = 0; k < ATerminalDoors::Openers.Num() && !bNear && D.K > 0.02f; ++k)
				bNear = FMath::Abs(ATerminalDoors::Openers[k].Z - D.AtW.Z) < 150.f && FVector::DistSquared2D(ATerminalDoors::Openers[k], D.AtW) < FMath::Square(double(D.Reach) * 100.0);
		}
		// sliding doors open by themselves as you come; any other door only when you open it (UseDoor), and its closer
		// pulls it shut 4 s after you've gone from it
		if (!D.bSlide && D.bOpen) { D.Away = bNear ? 0.f : D.Away + Dt; if (D.Away > 4.f) D.bOpen = false; }
		// (never shut on anyone in the doorway, passenger or staff)
		if (D.bSlide && !bNear && D.K > 0.02f) {
			static TWeakObjectPtr<AADPPeople> Pp;
			if (!Pp.IsValid() || Pp->GetWorld() != GetWorld()) { Pp.Reset(); for (TActorIterator<AADPPeople> It(GetWorld()); It; ++It) { Pp = *It; break; } }
			if (Pp.IsValid()) bNear = Pp->AnyoneNear(D.AtW, D.Reach * 75.f);
		}
		const float Want = (D.bSlide ? bNear : D.bOpen) ? 1.f : 0.f;
		if (FMath::Abs(D.K - Want) < 0.001f) continue;
		D.K += FMath::Sign(Want - D.K) * FMath::Min(FMath::Abs(Want - D.K), Dt * (D.bSlide && Want < D.K ? 0.8f : 1.6f));   // (sliding doors shut gently)
		if (D.JwGate) GateDoorK[D.JwGate] = D.K;
		const float E = D.K * D.K * (3 - 2 * D.K);   // eased, as doors.ts
		for (FPart& Part : D.Parts) MovePart(Part, E, 0);
	}
}

#pragma endregion

#pragma endregion

TArray<TWeakObjectPtr<ABabylonModel>> ABabylonModel::AllModels;
bool ABabylonModel::JetwayDocked(FDoor& D)
{
	// (found, never spawned, here: the jetways are rigged by whoever needs them first - PaxFlow, the HMI)
	static TWeakObjectPtr<AJetwayRigs> Jr;
	if (!Jr.IsValid() || Jr->GetWorld() != GetWorld()) { Jr.Reset(); for (TActorIterator<AJetwayRigs> It(GetWorld()); It; ++It) { Jr = *It; break; } }
	if (!Jr.IsValid() || Jr->Num() == 0) return false;   // (not rigged yet: shut)
	if (D.JwIx == -2)
	{
		// the jetway whose rotunda is right outside this door (as APaxFlow::JetwayFor finds it)
		float Bd = 2500.f; D.JwIx = INDEX_NONE;
		for (int32 j = 0; j < Jr->Num(); ++j) { FVector Ro, Dr; if (Jr->Frame(j, Ro, Dr) && FVector::Dist2D(Ro, D.AtW) < Bd) { Bd = FVector::Dist2D(Ro, D.AtW); D.JwIx = j; } }
		if (D.JwIx == INDEX_NONE) ADP_WARN(TEXT("Babylon model %s: gate %d's jetway door %s has no jetway within 25 m - it stays shut"), *GetName(), D.JwGate, *D.Id);
		else ADP_LOG(TEXT("Babylon model %s: gate %d's jetway door opens only while %s is docked"), *GetName(), D.JwGate, *Jr->Name(D.JwIx));
	}
	return D.JwIx != INDEX_NONE && Jr->State(D.JwIx) == AJetwayRigs::EState::Docked;
}

float ABabylonModel::GateDoorK[33] = {-1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f};
uint32 ABabylonModel::GateDoorsOn = 0;

int32 ABabylonModel::FindDoor(const FVector& Feet, const FVector& Facing) const
{
	const FTransform T = Root->GetComponentTransform();
	const FVector Me = T.InverseTransformPosition(Feet);
	int32 Best = -1; float BestD = 1e9f;
	for (int32 i = 0; i < Doors.Num(); ++i)
	{
		const FDoor& D = Doors[i];
		if (D.bSlide || D.bSpin) continue;
		const float Dist = FVector2D::Distance(FVector2D(Me), FVector2D(D.At));
		if (Dist > 190.f || FMath::Abs(Me.Z - D.At.Z) > 250.f) continue;
		// you face it (within about 65 degrees), so the right one of two doors side by side opens
		const FVector To = (T.TransformPosition(D.At) - Feet).GetSafeNormal2D();
		if (Dist > 60.f && FVector::DotProduct(To, Facing.GetSafeNormal2D()) < 0.42f) continue;
		if (Dist < BestD) { BestD = Dist; Best = i; }
	}
	return Best;
}

bool ABabylonModel::DoorNear(const FVector& Feet, const FVector& Facing, bool& bIsOpen)
{
	for (const TWeakObjectPtr<ABabylonModel>& W : AllModels)
		if (const ABabylonModel* M = W.Get())
		{
			const int32 i = M->FindDoor(Feet, Facing);
			if (i >= 0) { bIsOpen = M->Doors[i].bOpen; return true; }
		}
	return false;
}

bool ABabylonModel::TestHingedDoor(FVector& At, int32& NSlide, int32& NHinged)
{
	NSlide = NHinged = 0; bool bGot = false, bStaff = false;   // (a staff door first)
	for (const TWeakObjectPtr<ABabylonModel>& W : AllModels)
		if (const ABabylonModel* M = W.Get())
			for (const FDoor& D : M->Doors)
			{
				if (D.bSpin) continue;
				if (D.bSlide) { ++NSlide; continue; }
				++NHinged;
				if ((!bGot || (!bStaff && D.Id.Contains(TEXT("STAFF")))) && D.At.Z < 200.f) { bStaff = D.Id.Contains(TEXT("STAFF")); At = M->Root->GetComponentTransform().TransformPosition(D.At); bGot = true; }
			}
	return bGot;
}

int32 ABabylonModel::OpenPassengerDoors()
{
	int32 N = 0;
	for (const TWeakObjectPtr<ABabylonModel>& W : AllModels)
		if (ABabylonModel* M = W.Get())
			for (FDoor& D : M->Doors)
			{
				if (!D.bPax) continue;
				D.K = 1.f; ++N;
				for (FPart& Part : D.Parts) M->MovePart(Part, 1.f, 0.f);
			}
	return N;
}

float ABabylonModel::StaffDoor(const FVector& World, bool bOpen)
{
	for (const TWeakObjectPtr<ABabylonModel>& W : AllModels)
		if (ABabylonModel* M = W.Get())
			for (FDoor& D : M->Doors)
			{
				if (D.bSlide || D.bSpin || FMath::Abs(D.AtW.Z - World.Z) > 150.f || FVector::DistSquared2D(D.AtW, World) > FMath::Square(200.f)) continue;
				if (bOpen) { D.bOpen = true; D.Away = 0.f; }
				return D.K;
			}
	return -1.f;
}

bool ABabylonModel::UseDoor(const APawn* Me)
{
	if (!Me) return false;
	const FVector Feet = Me->GetActorLocation() - FVector(0, 0, Me->GetSimpleCollisionHalfHeight());
	const FVector Facing = Me->GetControlRotation().Vector();
	for (const TWeakObjectPtr<ABabylonModel>& W : AllModels)
		if (ABabylonModel* M = W.Get())
		{
			const int32 i = M->FindDoor(Feet, Facing);
			if (i < 0) continue;
			FDoor& D = M->Doors[i];
			D.bOpen = !D.bOpen; D.Away = 0.f;
			ADP_LOG(TEXT("Door %s %s (E / X)"), *D.Id, D.bOpen ? TEXT("opened") : TEXT("closed"));
			return true;
		}
	return false;
}

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
	// (no full rebuild: each moved instance already went to the GPU on its own - the full rebuild re-sent every part of these lists every frame, owner 2026-11-17 lag)
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

void ABabylonModel::SeatSpots(TArray<FSeatSpot>& Out) const
{
	ADP_PERF_SCOPE("a building's places to sit", 4.f);
	if (!Root || Model.StartsWith(TEXT("Firetruck"))) return;   // (the trucks' own seats are the truck's)
	const FTransform RootW = Root->GetComponentTransform();
	auto Under = [this](int32 I, TArray<int32>& Nodes) { TArray<int32> St = {I}; while (St.Num()) { const int32 N = St.Pop(); if (!NRoot.IsValidIndex(N)) continue; Nodes.Add(N); St.Append(NKids[N]); } };
	auto LastPart = [](const FString& N) { int32 P; return N.FindLastChar(TEXT('/'), P) ? N.Mid(P + 1) : N; };
	auto SeatLike = [](const FString& N)
	{
		int32 P; const TCHAR* C = *N + (N.FindLastChar(TEXT('/'), P) ? P + 1 : 0);
		return FCString::Strnicmp(C, TEXT("SEATS-"), 6) == 0 || FCString::Stristr(C, TEXT("CHAIR")) || FCString::Stristr(C, TEXT("STOOL")) || FCString::Stristr(C, TEXT("BENCH")) || FCString::Stristr(C, TEXT("SOFA"));
	};
	auto InstBox = [](UInstancedStaticMeshComponent* K, int32 Item, FBox& B) -> bool
	{
		FTransform T; if (!K || !K->GetStaticMesh() || !K->GetInstanceTransform(Item, T, true) || T.GetScale3D().GetMin() < 0.01f) return false;
		B = K->GetStaticMesh()->GetBoundingBox().TransformBy(T); return true;
	};
	UInstancedStaticMeshComponent* GateSeats = nullptr;
	for (const auto& P : ByMesh) if (P.Value && P.Value->GetStaticMesh() && P.Value->GetStaticMesh()->GetName() == TEXT("SeatModule")) GateSeats = P.Value.Get();
	for (UActorComponent* C : GetComponents()) if (UInstancedStaticMeshComponent* K = Cast<UInstancedStaticMeshComponent>(C)) if (K->GetStaticMesh() && K->GetStaticMesh()->GetName() == TEXT("SeatModule")) GateSeats = K;
	// (where each stands, once: asking for every module again at every seat was most of half a second)
	TArray<FTransform> GateAt;
	if (GateSeats) { GateAt.SetNum(GateSeats->GetInstanceCount()); for (int32 k = 0; k < GateAt.Num(); ++k) GateSeats->GetInstanceTransform(k, GateAt[k], true); }
	// the airport coach (owner, 2026-11-19, to-do 144: every passenger seat sittable, never the driver's): each seat's own
	// cushion - the pairs' and the back bench's (Cushion_A..E), the guide's and the wheelchair space's fold-up seats - facing
	// the way the coach faces (the fold-up seats: the aisle); the seat 45 cm over its floor
	if (Model == TEXT("AirportCoach"))
	{
		const FVector Fwd = RootW.GetRotation().GetForwardVector().GetSafeNormal2D();
		for (int32 I = 0; I < NName.Num(); ++I)
		{
			const FString& Nm = NName[I];
			const bool bFold = Nm.StartsWith(TEXT("FoldSeatCushion")), bGuide = Nm.StartsWith(TEXT("GuideSeatCushion"));
			if (!(Nm.StartsWith(TEXT("Cushion_")) || bFold || bGuide) || !NInst.IsValidIndex(I) || !NInst[I].Key) continue;
			FBox B; if (!InstBox(NInst[I].Key, NInst[I].Value, B)) continue;
			FSeatSpot S; const FVector C = B.GetCenter();
			S.SeatH = 45.f; S.At = FVector(C.X, C.Y, B.Max.Z - 0.5f);
			if (bFold) { const FVector L = RootW.InverseTransformPosition(C); S.Facing = RootW.TransformVectorNoScale(FVector(0, -FMath::Sign(L.Y), 0)).GetSafeNormal2D(); }
			else S.Facing = Fwd;
			S.Parts.Add(NInst[I]);
			Out.Add(S);
		}
		return;
	}
	for (int32 I = 0; I < NName.Num(); ++I)
	{
		if (!SeatLike(NName[I])) continue;   // (most parts: told without making a new text for each)
		const FString Last = LastPart(NName[I]).ToUpper();
		// the gate rows (their seats rebuilt as the new seating): each back with the seat in front of it
		if (Last.StartsWith(TEXT("SEATS-")))
		{
			TArray<int32> Backs, Pans;
			for (int32 C : NKids[I]) { const float Y = NRoot[C].GetLocation().Z - NRoot[I].GetLocation().Z; (FMath::IsNearlyEqual(Y, 72.f, 3.f) ? Backs : FMath::IsNearlyEqual(Y, 44.f, 3.f) ? Pans : Backs).Add(C); }
			// (the row's seats stand about its middle (ReplaceSeats); three rows' old boxes lay 13 m along from it - F1-AIRSIDE-CORR,
			// cut from a longer row - and so did their places to sit)
			FVector Mid = FVector::ZeroVector; for (int32 Pn : Pans) Mid += NRoot[I].InverseTransformPosition(NRoot[Pn].GetLocation()) / Pans.Num();
			const FVector Along = RootW.TransformVector(NRoot[I].TransformVector(FVector(FMath::Abs(Mid.X) > 20.0 ? -Mid.X : 0.0, 0.0, 0.0)));
			for (int32 Bk : Backs)
			{
				if (!FMath::IsNearlyEqual(NRoot[Bk].GetLocation().Z - NRoot[I].GetLocation().Z, 72.f, 3.f)) continue;
				const FVector BW = RootW.TransformPosition(NRoot[Bk].GetLocation()) + Along;
				int32 Best = INDEX_NONE; float BD = 1e9f;
				for (int32 Pn : Pans) { const float D = FVector::Dist2D(RootW.TransformPosition(NRoot[Pn].GetLocation()) + Along, BW); if (D < BD) { BD = D; Best = Pn; } }
				if (Best == INDEX_NONE) continue;
				const FVector PW = RootW.TransformPosition(NRoot[Best].GetLocation()) + Along;
				FSeatSpot S; S.Facing = (PW - BW).GetSafeNormal2D(); if (S.Facing.IsNearlyZero()) continue;
				const float Floor = RootW.TransformPosition(NRoot[I].GetLocation()).Z;
				S.SeatH = 46.f; S.At = FVector(PW.X, PW.Y, Floor + S.SeatH);
				// its piece of the new seating: the seat module nearest
				if (GateSeats)
				{
					int32 Near = INDEX_NONE; float ND = 80.f;
					for (int32 k = 0; k < GateAt.Num(); ++k) { const float D = FVector::Dist2D(GateAt[k].GetLocation(), S.At); if (D < ND) { ND = D; Near = k; } }
					if (Near != INDEX_NONE) S.Parts.Add(TPair<UInstancedStaticMeshComponent*, int32>(GateSeats, Near));
					// the way it faces, from the seat as it stands (owner 2026-11-23: passengers sat the wrong way round): its reclined
					// back reaches further out than its front edge, so its middle lies on the backrest's side of its own point
					if (Near != INDEX_NONE)
					{
						const FTransform& T = GateAt[Near];
						const FVector C = GateSeats->GetStaticMesh()->GetBoundingBox().GetCenter();
						const FVector Front = -T.TransformVector(FVector(C.X, C.Y, 0.0));
						if (Front.Size2D() > 2.0)
						{
							const FVector F = Front.GetSafeNormal2D();
							if (FVector::DotProduct(F, S.Facing) < 0.7) { static int32 Said = 0; if (Said++ < 6) ADP_WARN(TEXT("Seats: %s - the layout's facing and the seat's own back disagree: the seat's own taken"), *NName[I]); }
							S.Facing = F;
						}
					}
				}
				Out.Add(S);
			}
			continue;
		}
		const bool bStool = Last.Contains(TEXT("STOOL"));
		if (!(Last.Contains(TEXT("CHAIR")) || bStool || Last.Contains(TEXT("BENCH")) || Last.Contains(TEXT("SOFA")))) continue;
		// a chair's pieces, all of them under its node: their extent, the floor under them, the seat's height
		TArray<int32> Nodes; Under(I, Nodes);
		FSeatSpot S; FBox All(ForceInit);
		for (int32 N : Nodes) if (NInst.IsValidIndex(N) && NInst[N].Key) { FBox B; if (InstBox(NInst[N].Key, NInst[N].Value, B)) { All += B; S.Parts.Add(NInst[N]); } }
		if (!All.IsValid || S.Parts.Num() == 0) continue;
		const float Floor = All.Min.Z, Tall = All.Max.Z - Floor;
		if (Tall < 30.f || Tall > 160.f) continue;
		S.SeatH = bStool ? FMath::Min(76.f, Tall - 2.f) : FMath::Min(46.f, Tall - 2.f);
		// which way it faces: away from its back (the pieces standing well over the seat)
		FVector BackAt = FVector::ZeroVector; int32 NB = 0;
		for (const auto& P : S.Parts) { FBox B; if (InstBox(P.Key, P.Value, B) && B.Max.Z > Floor + S.SeatH + 18.f && B.Min.Z > Floor + S.SeatH - 15.f) { BackAt += B.GetCenter(); ++NB; } }
		const FVector C = All.GetCenter();
		if (NB) S.Facing = (C - BackAt / NB).GetSafeNormal2D();
		if (!NB || S.Facing.IsNearlyZero()) S.Facing = (RootW.GetRotation() * NRoot[I].GetRotation()).GetForwardVector().GetSafeNormal2D();
		// (a bench without a back - the kerb's: over its long side, to the road, never along it)
		if (!NB && !bStool)
		{
			const FVector Ex = All.GetSize(), Y = (RootW.GetRotation() * NRoot[I].GetRotation()).RotateVector(FVector(0, -1, 0));
			if (FMath::Max(Ex.X, Ex.Y) > 1.5 * FMath::Min(Ex.X, Ex.Y)) { const FVector Sh = Ex.X < Ex.Y ? FVector(1, 0, 0) : FVector(0, 1, 0); S.Facing = FVector::DotProduct(Y, Sh) < 0.0 ? -Sh : Sh; }
		}
		if (S.Facing.IsNearlyZero()) S.Facing = FVector::ForwardVector;
		S.At = FVector(C.X, C.Y, Floor + S.SeatH);
		S.bYours = Model.StartsWith(TEXT("Office")) || Model.StartsWith(TEXT("Ops"));   // (your office's and the ops room's chairs)
		Out.Add(S);
	}
}

ABabylonModel* ABabylonModel::PlaceDoorPanel(UWorld* W, const FVector& From, const FVector& Dir, float FloorZ, const TArray<AActor*>& Ignore, float Proud)
{
	if (!W) return nullptr;
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPDoorPanel), true);
	Q.AddIgnoredActors(Ignore);
	const FVector A(From.X, From.Y, FloorZ + 135.f);
	if (!W->LineTraceSingleByChannel(H, A, A + Dir.GetSafeNormal() * 1500.f, ECC_Visibility, Q)) return nullptr;
	// flat on the wall: the model's front (its -Y) along the wall's normal, its back on the wall, its foot on the floor
	// (it faces back the way we looked from - inside - whichever way the wall's own surface says it faces)
	const FVector N = (-Dir).GetSafeNormal2D();
	// a ribbed wall (the hangars' sheeting): mounted on its ribs - the face nearest the room anywhere behind the panel
	{
		const FVector Tan = FVector::CrossProduct(FVector::UpVector, N).GetSafeNormal();
		float Nearest = H.Distance;
		for (int32 a = -2; a <= 2; ++a) for (int32 b = -1; b <= 1; ++b)
		{
			const FVector S0 = A + Tan * (a * 4.f) + FVector(0, 0, b * 7.f);
			FHitResult Hb;
			if (W->LineTraceSingleByChannel(Hb, S0, S0 + Dir.GetSafeNormal() * 1500.f, ECC_Visibility, Q)) Nearest = FMath::Min(Nearest, Hb.Distance);
		}
		if (Nearest < H.Distance - 0.5f) { ADP_LOG(TEXT("Door panel: on the wall's ribs, %.1f cm proud of where the first look met it"), H.Distance - Nearest); H.ImpactPoint = A + Dir.GetSafeNormal() * Nearest; }
	}
	// (the model's face - Blender's +y - is its root's -Y, and Place gives the root a half turn on top of the actor's: so the
	// face looks along the actor's right - turned to the room)
	const FRotator R(0.f, N.Rotation().Yaw - 90.f, 0.f);
	// (Proud: a lining drawn in front of the wall that a trace can't find - it has no collision - the panel goes on its face)
	const FVector At = FVector(H.ImpactPoint.X, H.ImpactPoint.Y, FloorZ) + N * Proud;
	FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABabylonModel* P = W->SpawnActor<ABabylonModel>(At, R, S);   // (its back right on the wall)
	if (P && !P->Place(TEXT("DoorPanel"))) { P->Destroy(); P = nullptr; }
	// which way its face really looks: the green button stands out of the face, so it lies on the face's side of the plate;
	// if that's the wall's side, the panel is turned round (about its back, which stays on the wall)
	if (P)
	{
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Bt, Pl;
		P->InstancesNamed(TEXT("BtnOpen"), false, Bt); P->InstancesNamed(TEXT("DoorPanelPlate"), false, Pl);
		FTransform Tb, Tp;
		if (Bt.Num() && Pl.Num() && Bt[0].Get<0>()->GetInstanceTransform(Bt[0].Get<1>(), Tb, true) && Pl[0].Get<0>()->GetInstanceTransform(Pl[0].Get<1>(), Tp, true))
		{
			const float Out = float(FVector::DotProduct(Tb.GetLocation() - Tp.GetLocation(), N));
			if (Out < 0.f)
			{
				P->SetActorRotation(P->GetActorRotation() + FRotator(0.f, 180.f, 0.f));
				ADP_LOG(TEXT("Door panel: turned round to face the room"));
			}
		}
	}
	// (the parts as imported lie behind the model's origin, toward its back: brought out by the panel's own depth, so its
	// back touches the wall and its face looks into the room)
	if (P)
	{
		FVector O, E; P->GetActorBounds(false, O, E);
		const float Depth = FMath::Abs(E.X * N.X) + FMath::Abs(E.Y * N.Y);
		const float Behind = float(FVector::DotProduct(O - P->GetActorLocation(), N));   // (negative: its middle is behind the origin)
		P->SetActorLocation(P->GetActorLocation() + N * (Depth - Behind + 0.05f));
	}
	// seen as you'd see it: from a metre in front, straight at it - any surface met before the panel (a lining in front of
	// the wall the first look found) is the wall you see, and the panel goes onto that
	if (P)
	{
		FCollisionQueryParams Q2(SCENE_QUERY_STAT(ADPDoorPanelSee), true); Q2.AddIgnoredActors(Ignore); Q2.AddIgnoredActor(P);
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			const FVector Here = P->GetActorLocation(), Eye = FVector(Here.X, Here.Y, FloorZ + 135.f) + N * 100.f;
			FHitResult H2;
			if (!W->LineTraceSingleByChannel(H2, Eye, Eye - N * 140.f, ECC_Visibility, Q2) || H2.Distance > 99.f) break;
			P->SetActorLocation(FVector(H2.ImpactPoint.X, H2.ImpactPoint.Y, FloorZ) + N * 0.2f);
			ADP_LOG(TEXT("Door panel: brought forward onto %s (it was %.0f cm behind it)"), H2.GetComponent() ? *H2.GetComponent()->GetName() : TEXT("-"), 100.f - H2.Distance);
		}
	}
	if (P) LightDoorPanel(P, true);
	if (P) { FVector O, E; P->GetActorBounds(false, O, E); ADP_LOG(TEXT("Door panel: looked from %s, hit %s/%s at %s (%.0f cm on), normal %s; placed at %s, its parts round %s (%s across)"), *A.ToString(), H.GetActor() ? *H.GetActor()->GetName() : TEXT("-"), H.GetComponent() ? *H.GetComponent()->GetName() : TEXT("-"), *H.ImpactPoint.ToString(), H.Distance, *H.ImpactNormal.ToString(), *P->GetActorLocation().ToString(), *O.ToString(), *(E * 2.f).ToString()); }
	return P;
}

void ABabylonModel::LightDoorPanel(ABabylonModel* P, bool bGreen)
{
	if (!P) return;
	// each button's cap glows from inside in its own colour, the other one dark (its own list per panel: a panel is its
	// own model, so each one's light is its own)
	for (int32 k = 0; k < 2; ++k)
	{
		const bool bOpen = k == 0, bLit = bOpen == bGreen;
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> F;
		P->InstancesNamed(bOpen ? TEXT("BtnOpen") : TEXT("BtnClose"), false, F);
		for (const auto& X : F)
		{
			UInstancedStaticMeshComponent* K = X.Get<0>();
			if (!K) continue;
			UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(K->GetMaterial(0));
			if (!D) { D = UMaterialInstanceDynamic::Create(K->GetMaterial(0), P); K->SetMaterial(0, D); }
			const FLinearColor C = bOpen ? FLinearColor(0.1f, 1.f, 0.2f) : FLinearColor(1.f, 0.08f, 0.05f);
			D->SetVectorParameterValue(TEXT("EmissiveFactor"), bLit ? C : FLinearColor::Black);
			D->SetVectorParameterValue(TEXT("EmissiveColor"), bLit ? C : FLinearColor::Black);
			SetGlow(D, bLit ? 1.6f : 0.f);   // (gentle: the white arrow on it must stay clear)
		}
	}
}

ABabylonModel* ABabylonModel::PlaceDoorPanelAt(UWorld* W, const FVector& At, const FVector& N, float FloorZ)
{
	if (!W) return nullptr;
	// a wall we know (a lining with no collision, which no look can find): the panel goes straight onto it, looking along N
	const FVector From = At + N.GetSafeNormal2D() * 60.f;
	TArray<AActor*> None;
	// (a look from just in front, through nothing collidable to a far stop behind the face, then put back onto the face)
	FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Nn = N.GetSafeNormal2D();
	ABabylonModel* P = W->SpawnActor<ABabylonModel>(FVector(At.X, At.Y, FloorZ), FRotator(0.f, Nn.Rotation().Yaw - 90.f, 0.f), S);
	if (P && !P->Place(TEXT("DoorPanel"))) { P->Destroy(); return nullptr; }
	if (!P) return nullptr;
	LightDoorPanel(P, true);
	// its face to the room (the green button stands out of it), its back on the face
	TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Bt, Pl;
	P->InstancesNamed(TEXT("BtnOpen"), false, Bt); P->InstancesNamed(TEXT("DoorPanelPlate"), false, Pl);
	FTransform Tb, Tp;
	if (Bt.Num() && Pl.Num() && Bt[0].Get<0>()->GetInstanceTransform(Bt[0].Get<1>(), Tb, true) && Pl[0].Get<0>()->GetInstanceTransform(Pl[0].Get<1>(), Tp, true)
		&& FVector::DotProduct(Tb.GetLocation() - Tp.GetLocation(), Nn) < 0.f)
		P->SetActorRotation(P->GetActorRotation() + FRotator(0.f, 180.f, 0.f));
	FVector O, E; P->GetActorBounds(false, O, E);
	const float Depth = FMath::Abs(E.X * Nn.X) + FMath::Abs(E.Y * Nn.Y), Behind = float(FVector::DotProduct(O - P->GetActorLocation(), Nn));
	P->SetActorLocation(P->GetActorLocation() + Nn * (Depth - Behind + 0.05f));
	(void)From; (void)None;
	return P;
}

void ABabylonModel::CheckLamps()
{
	// each room lamp must hang from something: a ceiling (or the roof) within 1.2 m over the top of its fitting. One
	// with nothing over it floats in the open (owner, 2026-11-18: "a ton of squared lights in the sky outside") and is
	// taken out for good, with its light. Every one taken out is written to the log: its room, where (plan metres) and how
	// far down the floor is - a floor right under it means its room is there but has no ceiling (fix the room instead).
	UWorld* W = GetWorld(); if (!W) return;
	const FTransform T = Root->GetComponentTransform();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPLampCheck), true);
	TMap<UInstancedStaticMeshComponent*, TArray<int32>> Gone;
	TMap<FString, int32> ByRoom; int32 Out = 0, Kept = 0;
	for (const FLampRec& L : LampRecs)
	{
		if (!L.Face.IsValid || L.Room.Contains(TEXT("CROWN"))) { ++Kept; continue; }   // (the crown lights hang on their cables)
		const FVector Top = T.TransformPosition(FVector(L.Face.GetCenter().X, L.Face.GetCenter().Y, L.Face.Max.Z + 3.f));
		FHitResult H;
		if (W->LineTraceSingleByChannel(H, Top, Top + FVector(0, 0, 120.f), ECC_Visibility, Q)) { ++Kept; continue; }
		// (a ceiling a hair over the lamp is under the look's start, and a roof seen from under is its back, which nothing
		//  hits: the curved gates' 60 bay lamps were all taken out - so also from above, down onto the roof, 2026-11-23)
		if (L.Room.StartsWith(TEXT("F2-GATE-")) && W->LineTraceSingleByChannel(H, Top + FVector(0, 0, 120.f), Top, ECC_Visibility, Q) && H.ImpactNormal.Z > 0.5f && H.ImpactPoint.Z > Top.Z + 1.f) { ++Kept; continue; }
		// nothing over it: out (its panels and its light)
		FHitResult Fl; const bool bFloor = W->LineTraceSingleByChannel(Fl, Top - FVector(0, 0, 10.f), Top - FVector(0, 0, 3000.f), ECC_Visibility, Q);
		const FVector Pl(Top.X / 100.f, Top.Z / 100.f, -Top.Y / 100.f);   // (plan metres: x, height, z - FTerminalPlan::ToWorld turned back)
		ADP_LOG(TEXT("Lamp check: %s lamp at plan x %.1f z %.1f, %.1f m up - nothing over it, taken out (floor %s)"), *L.Room, Pl.X, Pl.Z, Pl.Y,
			bFloor ? *FString::Printf(TEXT("%.1f m under it"), (Top.Z - Fl.ImpactPoint.Z) / 100.f) : TEXT("none under it"));
		for (const TPair<UInstancedStaticMeshComponent*, int32>& P : L.Panels) if (P.Key) Gone.FindOrAdd(P.Key).Add(P.Value);
		if (L.Light) { const int32 Li = LampLights.IndexOfByKey(L.Light); if (Li != INDEX_NONE) { LampLights.RemoveAt(Li); if (LampBase.IsValidIndex(Li)) LampBase.RemoveAt(Li); } L.Light->DestroyComponent(); }
		++ByRoom.FindOrAdd(L.Room); ++Out;
	}
	for (TPair<UInstancedStaticMeshComponent*, TArray<int32>>& G : Gone) { G.Value.Sort([](int32 A, int32 B) { return A > B; }); for (const int32 Ix : G.Value) G.Key->RemoveInstance(Ix); }
	FString Rooms; for (const TPair<FString, int32>& R : ByRoom) Rooms += FString::Printf(TEXT("%s%s %d"), Rooms.IsEmpty() ? TEXT("") : TEXT(", "), *R.Key, R.Value);
	ADP_LOG(TEXT("Lamp check: %d lamps hang from their ceilings, %d floating ones taken out%s%s"), Kept, Out, Out ? TEXT(": ") : TEXT(""), *Rooms);
	LampRecs.Empty();
}
