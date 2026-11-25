// The control tower's shells as finished Nanite meshes (to-do 302): Tools/bake_tower.sh builds the tower once in the editor,
// saves its solid shells, its trim and the desk's flap to /Game/Tower/Baked with a fingerprint of the buffers they were made
// from (Data/tower-baked.json), and the game places those when the fingerprint is the same; else the procedural shells.
#include "TowerADC.h"
#include "ADPLog.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "StaticMeshResources.h"
#if WITH_EDITOR
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Rendering/NaniteResources.h"
#endif

namespace
{
	/** A shell's fingerprint: the triangles and material names exactly, the places, normals and UVs as sums over blocks of
	 *  64 corners. Sums with a tolerance, not a hash of the numbers: the sines behind them may differ in the last digit on
	 *  another processor, and a hash would then throw the saved meshes away there. A corner moved 0.2 mm shows. */
	struct FShellPrint { uint32 Hash = 0; TArray<double> Sums; };
	constexpr int32 PrintBlock = 64, PerBlock = 8;
	const double PrintTolerance[PerBlock] = {0.02, 0.02, 0.02, 0.001, 0.001, 0.001, 0.002, 0.002};   // cm, cm, cm, normal, UV
	struct FShellSec { FName Key; const TArray<FVector>* V; const TArray<int32>* T; const TArray<FVector>* N; const TArray<FVector2D>* UV; };
	struct FSavedShell { FString Mesh; FShellPrint P; };
	const TMap<FString, FSavedShell>& SavedShells()
	{
		static TMap<FString, FSavedShell> M; static bool bRead = false;
		if (bRead) return M;
		bRead = true;
		FString T; TSharedPtr<FJsonObject> J;
		if ((!FFileHelper::LoadFileToString(T, *(FPaths::ProjectContentDir() / TEXT("Data/tower-baked.json"))) && !FFileHelper::LoadFileToString(T, *(FPaths::ProjectDir() / TEXT("Data/tower-baked.json")))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T), J) || !J.IsValid())
		{
			ADP_WARN(TEXT("Tower: Data/tower-baked.json missing or unreadable"));
			return M;
		}
		const TSharedPtr<FJsonObject>* S = nullptr;
		if (!J->TryGetObjectField(TEXT("shells"), S)) return M;
		for (const auto& E : (*S)->Values)
		{
			const TSharedPtr<FJsonObject> O = E.Value->AsObject();
			FSavedShell& B = M.Add(FString(*E.Key));
			B.Mesh = O->GetStringField(TEXT("mesh"));
			B.P.Hash = uint32(O->GetNumberField(TEXT("hash")));
			for (const TSharedPtr<FJsonValue>& V : O->GetArrayField(TEXT("sums"))) B.P.Sums.Add(V->AsNumber());
		}
		return M;
	}
	FShellPrint PrintOf(const TArray<FShellSec>& Secs)
	{
		FShellPrint R;
		uint32 H = 0x70e30001u;   // (the last digit: bumped when the way the meshes are saved changes)
		for (const FShellSec& S : Secs)
		{
			H = FCrc::StrCrc32(*S.Key.ToString().ToLower(), H);
			const int32 Counts[4] = {S.V->Num(), S.T->Num(), S.N->Num(), S.UV->Num()};
			H = FCrc::MemCrc32(Counts, sizeof(Counts), H);
			H = FCrc::MemCrc32(S.T->GetData(), S.T->Num() * sizeof(int32), H);
			for (int32 i = 0; i < S.V->Num(); i += PrintBlock)
			{
				double Sum[PerBlock] = {};
				for (int32 k = i; k < FMath::Min(i + PrintBlock, S.V->Num()); ++k)
				{
					const FVector& V = (*S.V)[k]; const FVector& N = (*S.N)[k]; const FVector2D& UV = (*S.UV)[k];
					Sum[0] += V.X; Sum[1] += V.Y; Sum[2] += V.Z;
					Sum[3] += N.X; Sum[4] += N.Y; Sum[5] += N.Z;
					Sum[6] += UV.X; Sum[7] += UV.Y;
				}
				R.Sums.Append(Sum, PerBlock);
			}
		}
		R.Hash = H;
		return R;
	}
	bool SamePrint(const FShellPrint& A, const FShellPrint& B, FString& Why)
	{
		if (A.Hash != B.Hash) { Why = TEXT("its triangles or materials changed"); return false; }
		if (A.Sums.Num() != B.Sums.Num()) { Why = TEXT("its number of corners changed"); return false; }
		for (int32 i = 0; i < A.Sums.Num(); ++i)
			if (FMath::Abs(A.Sums[i] - B.Sums[i]) > PrintTolerance[i % PerBlock])
			{
				Why = FString::Printf(TEXT("block %d, sum %d: %.5f, saved %.5f"), i / PerBlock, i % PerBlock, A.Sums[i], B.Sums[i]);
				return false;
			}
		return true;
	}
#if WITH_EDITOR
	enum class ETowerMake : uint8 { Game, Bake, Check };
	ETowerMake GTowerMake = ETowerMake::Game;
	TSharedPtr<FJsonObject> GTowerOut;
	int32 GTowerBad = 0;
#endif
}

bool ATowerADC::IsShellsOnly()
{
#if WITH_EDITOR
	return GTowerMake != ETowerMake::Game;
#else
	return false;
#endif
}

void ATowerADC::SolidShells(USceneComponent* Parent, TMap<FName, FBuf>& Set, bool bCollide, bool bShadow, const FName Name)
{
	TArray<FShellSec> Secs;
	for (const TPair<FName, FBuf>& E : Set)
		if (E.Value.T.Num()) Secs.Add({E.Key, &E.Value.V, &E.Value.T, &E.Value.N, &E.Value.UV});
	const FShellPrint P = PrintOf(Secs);
#if WITH_EDITOR
	if (GTowerMake == ETowerMake::Bake)
	{
		if (!SaveShell(Set, bCollide, Name)) ++GTowerBad;
		else
		{
			TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("mesh"), FString::Printf(TEXT("/Game/Tower/Baked/SM_%s.SM_%s"), *Name.ToString(), *Name.ToString()));
			O->SetNumberField(TEXT("hash"), double(P.Hash));
			TArray<TSharedPtr<FJsonValue>> A;
			for (const double D : P.Sums) A.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(D * 1e5) / 1e5));
			O->SetArrayField(TEXT("sums"), A);
			GTowerOut->SetObjectField(Name.ToString(), O);
		}
		Set.Empty();
		return;
	}
#endif
	FString Why = TEXT("not saved yet");
	UStaticMesh* SM = nullptr;
	if (const FSavedShell* S = SavedShells().Find(Name.ToString()))
		if (SamePrint(P, S->P, Why))
		{
			SM = LoadObject<UStaticMesh>(nullptr, *S->Mesh);
			if (!SM) Why = S->Mesh + TEXT(" could not be loaded");
			else if (SM->GetStaticMaterials().Num() != Secs.Num()) { Why = FString::Printf(TEXT("%d materials saved, %d made"), SM->GetStaticMaterials().Num(), Secs.Num()); SM = nullptr; }
		}
	if (!SM)
	{
		ADP_WARN(TEXT("Tower: %s built at run time (%s): run Tools/bake_tower.sh"), *Name.ToString(), *Why);
		Shells(Parent, Set, bCollide, bShadow, Name);
		return;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name);
	C->SetupAttachment(Parent);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(SM);
	for (int32 i = 0; i < Secs.Num(); ++i) C->SetMaterial(i, Mat(Secs[i].Key));
	C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	C->SetCastShadow(bShadow);
	// (as the procedural shells were: not in Lumen's distance fields - the invisible pieces in Build tell it the tower)
	C->SetAffectDistanceFieldLighting(false);
	C->RegisterComponent();
	AddInstanceComponent(C);
	Set.Empty();
	++BakedShells;
}

int32 ATowerADC::BakeShells(UObject* WorldContext, bool bCheckOnly)
{
#if WITH_EDITOR
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) { ADP_ERROR(TEXT("Tower bake: no world")); return 0; }
	GTowerMake = bCheckOnly ? ETowerMake::Check : ETowerMake::Bake;
	GTowerOut = MakeShared<FJsonObject>();
	GTowerBad = 0;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATowerADC* T = W->SpawnActor<ATowerADC>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (T) T->Build();
	GTowerMake = ETowerMake::Game;
	int32 N = 0;
	if (T && bCheckOnly) N = T->BakedShells;
	else if (T && GTowerBad == 0 && GTowerOut->Values.Num())
	{
		N = GTowerOut->Values.Num();
		TSharedRef<FJsonObject> Man = MakeShared<FJsonObject>();
		Man->SetStringField(TEXT("about"), TEXT("ATowerADC::BakeShells (Tools/bake_tower.sh): the control tower's saved shells and their fingerprints"));
		Man->SetObjectField(TEXT("shells"), GTowerOut);
		FString Out; FJsonSerializer::Serialize(Man, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out));
		if (!FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectDir() / TEXT("Data/tower-baked.json")))) { ADP_ERROR(TEXT("Tower bake: Data/tower-baked.json could not be written")); N = 0; }
	}
	ADP_LOG(TEXT("Tower bake: %s, %d shells, %d failed"), bCheckOnly ? TEXT("checked") : TEXT("saved"), N, GTowerBad);
	GTowerOut.Reset();
	if (T) T->Destroy();
	return N;
#else
	return 0;
#endif
}

bool ATowerADC::SaveShell(const TMap<FName, FBuf>& Set, bool bCollide, const FName ShellName)
{
#if WITH_EDITOR
	// ---- the buffers as they are: no welding, no new normals, the procedural mesh's own plain tangent ----
	FMeshDescription MD; FStaticMeshAttributes Attr(MD); Attr.Register();
	TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Nor = Attr.GetVertexInstanceNormals(), Tan = Attr.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> Sign = Attr.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> Uv = Attr.GetVertexInstanceUVs();
	TVertexInstanceAttributesRef<FVector4f> Col = Attr.GetVertexInstanceColors();
	const FString Name = TEXT("SM_") + ShellName.ToString(), PkgName = TEXT("/Game/Tower/Baked/") + Name;
	UPackage* Pkg = CreatePackage(*PkgName);
	Pkg->FullyLoad();
	if (UStaticMesh* Old = FindObject<UStaticMesh>(Pkg, *Name)) Old->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	UStaticMesh* SM = NewObject<UStaticMesh>(Pkg, *Name, RF_Public | RF_Standalone);
	struct FWant { FName Key; int32 Tris; FBox Box; };
	TArray<FWant> Want;
	int32 Tris = 0;
	for (const TPair<FName, FBuf>& E : Set)
	{
		const FBuf& B = E.Value;
		if (B.T.Num() == 0) continue;
		UMaterialInterface* M = Mat(E.Key);
		while (UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(M)) M = D->Parent;
		const UMaterial* Base = M ? M->GetMaterial() : nullptr;
		if (!Base || !Base->GetUsageByFlag(MATUSAGE_Nanite) || IsTranslucentBlendMode(M->GetBlendMode()))
		{
			ADP_ERROR(TEXT("Tower bake: %s: material %s (%s) is see-through or not made for Nanite"), *Name, *E.Key.ToString(), M ? *M->GetPathName() : TEXT("none"));
			return false;
		}
		ADP_LOG(TEXT("Tower bake: %s slot %d %s = %s, base %s, two-sided %d"), *Name, Want.Num(), *E.Key.ToString(), *M->GetName(), *Base->GetName(), M->IsTwoSided() ? 1 : 0);
		FStaticMaterial& Slot = SM->GetStaticMaterials().Emplace_GetRef(M, E.Key, E.Key);
		Slot.UVChannelData = FMeshUVChannelInfo(1.f);
		const FPolygonGroupID G = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[G] = E.Key;
		TArray<FVertexInstanceID> Corner; Corner.Reserve(B.V.Num());
		FBox Box(ForceInit);
		for (int32 i = 0; i < B.V.Num(); ++i)
		{
			const FVertexID V = MD.CreateVertex();
			Pos[V] = FVector3f(B.V[i]);
			const FVertexInstanceID I = MD.CreateVertexInstance(V);
			Nor[I] = FVector3f(B.N[i]); Tan[I] = FVector3f(1, 0, 0); Sign[I] = 1.f;
			Uv.Set(I, 0, FVector2f(B.UV[i])); Col[I] = FVector4f(1, 1, 1, 1);
			Corner.Add(I); Box += FVector(FVector3f(B.V[i]));
		}
		for (int32 t = 0; t + 2 < B.T.Num(); t += 3) MD.CreateTriangle(G, {Corner[B.T[t]], Corner[B.T[t + 1]], Corner[B.T[t + 2]]});
		Want.Add({E.Key, B.T.Num() / 3, Box}); Tris += B.T.Num() / 3;
	}
	if (!Want.Num()) return false;

	// ---- the mesh: Nanite, its backup the very same triangles (what you walk against), no distance field ----
	FStaticMeshSourceModel& Src = SM->AddSourceModel();
	FMeshBuildSettings& BS = Src.BuildSettings;
	BS.bRecomputeNormals = false; BS.bRecomputeTangents = false; BS.bRemoveDegenerates = false;
	BS.bUseFullPrecisionUVs = true; BS.bUseHighPrecisionTangentBasis = true; BS.bGenerateLightmapUVs = false;
	BS.DistanceFieldResolutionScale = 0.f; BS.MaxLumenMeshCards = 0;
	SM->CreateMeshDescription(0, MoveTemp(MD));
	SM->CommitMeshDescription(0);
	for (int32 i = 0; i < Want.Num(); ++i)
	{
		FMeshSectionInfo Info(i);
		Info.bEnableCollision = bCollide; Info.bCastShadow = true;
		SM->GetSectionInfoMap().Set(0, i, Info);
		SM->GetOriginalSectionInfoMap().Set(0, i, Info);
	}
	FMeshNaniteSettings NS = SM->GetNaniteSettings();
	NS.bEnabled = true; NS.bExplicitTangents = true; NS.PositionPrecision = 6; NS.NormalPrecision = 12; NS.TangentPrecision = 12;
	NS.GenerateFallback = ENaniteGenerateFallback::Enabled;
	NS.FallbackTarget = ENaniteFallbackTarget::PercentTriangles; NS.FallbackPercentTriangles = 1.f; NS.FallbackRelativeError = 0.f;
	SM->SetNaniteSettings(NS);
	SM->SetLightingGuid();
	SM->CreateBodySetup();
	SM->GetBodySetup()->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
	SM->SetCustomizedCollision(true);
	SM->Build(true);

	// ---- what came out, against the buffers ----
	if (!SM->GetRenderData() || SM->GetRenderData()->LODResources.Num() != 1) { ADP_ERROR(TEXT("Tower bake: %s: no mesh came out"), *Name); return false; }
	bool bOk = true;
	const FStaticMeshLODResources& L = SM->GetRenderData()->LODResources[0];
	const FIndexArrayView Ix = L.IndexBuffer.GetArrayView();
	if (L.Sections.Num() != Want.Num()) { ADP_ERROR(TEXT("Tower bake: %s: %d sections, %d made"), *Name, L.Sections.Num(), Want.Num()); bOk = false; }
	for (int32 s = 0; s < FMath::Min(L.Sections.Num(), Want.Num()); ++s)
	{
		const FStaticMeshSection& S = L.Sections[s];
		FBox Box(ForceInit);
		for (uint32 k = 0; k < S.NumTriangles * 3; ++k) Box += FVector(L.VertexBuffers.PositionVertexBuffer.VertexPosition(Ix[S.FirstIndex + k]));
		const bool bSame = S.MaterialIndex == s && SM->GetStaticMaterials()[s].MaterialSlotName == Want[s].Key && int32(S.NumTriangles) == Want[s].Tris
			&& Box.Min.Equals(Want[s].Box.Min, 0.001) && Box.Max.Equals(Want[s].Box.Max, 0.001);
		ADP_LOG(TEXT("Tower bake: %s section %d %s: %d triangles of %d, bounds %s .. %s %s"), *Name, s, *Want[s].Key.ToString(), S.NumTriangles, Want[s].Tris,
			*Box.Min.ToCompactString(), *Box.Max.ToCompactString(), bSame ? TEXT("same") : TEXT("DIFFERENT"));
		bOk &= bSame;
	}
	const Nanite::FResources* NR = SM->GetRenderData()->NaniteResourcesPtr.Get();
	const int32 NTris = NR ? int32(NR->NumInputTriangles) : 0;
	int32 Hard = 0;
	SM->GetBodySetup()->InvalidatePhysicsData();
	SM->GetBodySetup()->CreatePhysicsMeshes();
	FTriMeshCollisionData Tm;
	if (SM->GetBodySetup()->TriMeshGeometries.Num() == 1 && SM->GetPhysicsTriMeshData(&Tm, true)) Hard = Tm.Indices.Num();
	ADP_LOG(TEXT("Tower bake: %s Nanite %d (valid %d, %d triangles of %d), collision %d triangles (wanted %d)"), *Name, SM->IsNaniteEnabled() ? 1 : 0, SM->HasValidNaniteData() ? 1 : 0,
		NTris, Tris, Hard, bCollide ? Tris : 0);
	if (!SM->IsNaniteEnabled() || !SM->HasValidNaniteData() || NTris != Tris || Hard != (bCollide ? Tris : 0)) { ADP_ERROR(TEXT("Tower bake: %s is not the same as its buffers"), *Name); bOk = false; }
	if (!bOk) return false;
	FAssetRegistryModule::AssetCreated(SM);
	Pkg->MarkPackageDirty();
	FSavePackageArgs SA; SA.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Pkg, SM, *FPackageName::LongPackageNameToFilename(PkgName, FPackageName::GetAssetPackageExtension()), SA)) { ADP_ERROR(TEXT("Tower bake: %s could not be saved"), *Name); return false; }
	return true;
#else
	return false;
#endif
}
