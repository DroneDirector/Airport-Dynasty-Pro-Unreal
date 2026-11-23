// The walkers' navigation mesh (PaxNav.h).
#include "PaxNav.h"
#include "ADPLog.h"
#include "TerminalPlan.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Components/BrushComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "RoadNetwork.h"
#include "NavMesh/PImplRecastNavMesh.h"
#include "NavMesh/RecastVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/Compression.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "ADPGameMode.h"
#include "Site.h"

namespace
{
	TWeakObjectPtr<UWorld> GNavWorld;
	double GStart = 0.0;
	bool GReady = false, GSeenBuild = false;
	int32 GVolumes = 0;
	bool GBuildAsked = false, GWarned = false;
	// the ready-made map (owner, 2026-11-23: it took a minute and more to make on every start): baked into the release
	// (Tools/bake_paxnav.sh, -PaxNavBake -> the site's Data folder), or kept from the last run (Saved/PaxNav)
	bool GLoaded = false, GSaved = false;
	FString GKey;
	double GQuietFrom = -1.0, GAllInFrom = -1.0;
	constexpr uint32 NavMagic = 0x564E5041;   // "APNV"

	/** The actors whose things move (or open, or come and go): never part of the navmesh. By class name, so this file
	 *  needs none of their headers. */
	bool Moves(const AActor* A)
	{
		static const TSet<FName> Names = {
			TEXT("ADPPeople"), TEXT("PaxFlow"), TEXT("TerminalCrowd"), TEXT("Pet"), TEXT("ADPWalker"),
			TEXT("DepartureBelts"), TEXT("BaggageBelts"),
			TEXT("Coaches"), TEXT("GSE"), TEXT("Tugs"), TEXT("FireTruck"), TEXT("AirTraffic"), TEXT("SimTraffic"),
			TEXT("JetwayRigs"), TEXT("TerminalJetways"), TEXT("JetwayHmi"),
			TEXT("TerminalDoors"), TEXT("FireStationDoors"), TEXT("RollerDoor"), TEXT("TerminalLift"), TEXT("TerminalLifts"), TEXT("DoorPanelUse"),
			TEXT("Seats"), TEXT("WashHose"), TEXT("Flags"), TEXT("WindSocks"), TEXT("LampFlares"), TEXT("Puddles"),
			TEXT("GroundDecals"), TEXT("GrassField"), TEXT("SiteTrees"), TEXT("InteractOutline") };
		for (const UClass* C = A->GetClass(); C && C != AActor::StaticClass(); C = C->GetSuperClass())
			if (Names.Contains(C->GetFName())) return true;
		return false;
	}

	/** A box volume made at run time: its brush's shape is one box (no editor brush builder in a packaged game). */
	template <class T> T* BoxVolume(UWorld* W, const FBox& B)
	{
		const FTransform At(B.GetCenter());
		T* V = W->SpawnActorDeferred<T>(T::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!V) return nullptr;
		if (UBrushComponent* BC = V->GetBrushComponent())
		{
			UBodySetup* BS = NewObject<UBodySetup>(BC);
			const FVector S = B.GetSize();
			BS->AggGeom.BoxElems.Add(FKBoxElem(S.X, S.Y, S.Z));
			BC->BrushBodySetup = BS;
			BC->SetCanEverAffectNavigation(false);
			// (a native component is registered while the actor is spawned, before this shape is given - its bounds
			//  were empty, and so was the walkable area the navigation took from it: "1 empty bounds" in the 18:37 test)
			BC->UpdateBounds();
		}
		V->FinishSpawning(At);
		return V;
	}

	UNavigationSystemV1* Nav(UWorld* W) { return W ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(W) : nullptr; }
	/** The walkers' navmesh: the default one, else the first in the world (the agent is named "Passenger"). */
	ANavigationData* NavData(UWorld* W, UNavigationSystemV1* NS)
	{
		if (ANavigationData* ND = NS->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) return ND;
		for (TActorIterator<ARecastNavMesh> It(W); It; ++It) return *It;
		return nullptr;
	}
}

namespace
{
	FString BakedFile() { return ADPSite::File(TEXT("paxnav.bin")); }
	FString KeptFile() { return FPaths::ProjectSavedDir() / TEXT("PaxNav") / (ADPSite::Id() + TEXT(".bin")); }

	/** What the map was made from: the engine's navmesh format, the walker, the areas, and the files that place the terminal
	 *  and the roads. A different one anywhere, and the old map isn't used (it is made again, and kept). */
	FString MakeKey(UNavigationSystemV1* NS, const TArray<FBox>& Areas)
	{
		uint32 C = 0;
		auto Add = [&C](const FString& S) { C = FCrc::StrCrc32(*S, C); };
		for (const FNavDataConfig& A : NS->GetSupportedAgents()) Add(FString::Printf(TEXT("%s %.1f %.1f %.1f"), *A.Name.ToString(), A.AgentRadius, A.AgentHeight, A.AgentStepHeight));
		const ARecastNavMesh* D = GetDefault<ARecastNavMesh>();
		Add(FString::Printf(TEXT("%.1f %.2f %.2f"), D->TileSizeUU, D->GetCellSize(ENavigationDataResolution::Default), D->GetCellHeight(ENavigationDataResolution::Default)));
		for (const FBox& B : Areas) Add(FString::Printf(TEXT("%.0f %.0f %.0f %.0f %.0f %.0f"), B.Min.X, B.Min.Y, B.Min.Z, B.Max.X, B.Max.Y, B.Max.Z));
		auto AddFile = [&C](const FString& F) { TArray<uint8> B; if (FFileHelper::LoadFileToArray(B, *F, FILEREAD_Silent)) C = FCrc::MemCrc32(B.GetData(), B.Num(), C); };
		for (const TCHAR* N : {TEXT("terminal-plan.json"), TEXT("terminal-layout.json"), TEXT("babylon-terminal.json"), TEXT("babylon-jetways.json"), TEXT("escalator.json"), TEXT("airfield.json")})
		{
			const FString F = FPaths::ProjectContentDir() / TEXT("Data") / N;
			AddFile(FPaths::FileExists(F) ? F : FPaths::ProjectDir() / TEXT("Data") / N);
		}
		for (const TCHAR* N : {TEXT("roads.json"), TEXT("road-ground.json")}) AddFile(ADPSite::File(N));
		return FString::Printf(TEXT("%s-%d-%08x"), *ADPSite::Id(), int32(NAVMESHVER_LATEST), C);
	}

	/** The navmesh's own data (protected in the engine: reached by the name a subclass may use). */
	struct FNavAccess : public ARecastNavMesh
	{
		static FPImplRecastNavMesh* Of(ARecastNavMesh& N) { FPImplRecastNavMesh* (ARecastNavMesh::*F)() = &FNavAccess::GetRecastNavMeshImpl; return (N.*F)(); }
	};

	/** Never generated again by itself: what is in it is all there is. */
	void MakeStatic(ANavigationData* N)
	{
		if (FEnumProperty* P = FindFProperty<FEnumProperty>(ANavigationData::StaticClass(), TEXT("RuntimeGeneration")))
			*P->ContainerPtrToValuePtr<ERuntimeGenerationType>(N) = ERuntimeGenerationType::Static;
		else ADP_WARN(TEXT("PaxFlow: the navmesh couldn't be made static"));
	}

	bool ReadNavFile(const FString& F, const FString& Key, TArray<uint8>& Raw, FString& Why)
	{
		TArray<uint8> File;
		if (!FFileHelper::LoadFileToArray(File, *F, FILEREAD_Silent)) { Why = TEXT("not there"); return false; }
		FMemoryReader A(File, true);
		uint32 Magic = 0; FString K; int32 Size = 0; TArray<uint8> Packed;
		A << Magic; if (Magic != NavMagic) { Why = TEXT("not a walking map"); return false; }
		A << K; if (K != Key) { Why = FString::Printf(TEXT("made for another terminal (%s, this is %s)"), *K, *Key); return false; }
		A << Size; A << Packed;
		if (A.IsError() || Size <= 0 || Size > 512 * 1024 * 1024) { Why = TEXT("damaged"); return false; }
		Raw.SetNumUninitialized(Size);
		if (!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), Size, Packed.GetData(), Packed.Num())) { Why = TEXT("damaged"); return false; }
		return true;
	}

	bool LoadReady(UWorld* W, UNavigationSystemV1* NS)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("PaxNavBake")) || FParse::Param(FCommandLine::Get(), TEXT("NoPaxNavFile")) || !NS->GetSupportedAgents().Num()) return false;
		const double T0 = FPlatformTime::Seconds();
		TArray<uint8> Raw; FString From, Why;
		for (const FString& F : {BakedFile(), KeptFile()})
		{
			if (ReadNavFile(F, GKey, Raw, Why)) { From = F; break; }
			ADP_LOG(TEXT("PaxFlow: ready-made walking map %s: %s"), *F, *Why);
		}
		if (From.IsEmpty()) return false;
		ARecastNavMesh* RN = Cast<ARecastNavMesh>(NavData(W, NS));
		if (RN) { RN->CancelBuild(); MakeStatic(RN); RN->ConditionalConstructGenerator(); }   // (one already made, empty: no generator for it any more)
		else
		{
			RN = W->SpawnActorDeferred<ARecastNavMesh>(ARecastNavMesh::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!RN) return false;
			MakeStatic(RN);
			RN->SetConfig(NS->GetSupportedAgents()[0]);
			RN->FinishSpawning(FTransform::Identity);
		}
		FPImplRecastNavMesh* Impl = FNavAccess::Of(*RN);
		if (!Impl) return false;
		FMemoryReader Ar(Raw, true);
		FCustomVersionContainer V; V.SetVersion(FFortniteMainBranchObjectVersion::GUID, FFortniteMainBranchObjectVersion::LatestVersion, TEXT("Main"));
		Ar.SetCustomVersions(V);
		Impl->Serialize(Ar, NAVMESHVER_LATEST);
		const int32 Tiles = RN->GetRecastMesh() ? RN->GetNavMeshTilesCount() : 0;
		if (Ar.IsError() || Tiles <= 0)
		{
			ADP_WARN(TEXT("PaxFlow: the ready-made walking map couldn't be used (%d tiles) - making it"), Tiles);
			RN->Destroy();
			return false;
		}
		GLoaded = true; GReady = true;
		ADP_LOG(TEXT("PaxFlow: navmesh ready, ready-made (%s, %d tiles, %.0f ms)"), From == KeptFile() ? TEXT("kept from the last run") : TEXT("with the game"), Tiles, (FPlatformTime::Seconds() - T0) * 1000.0);
		return true;
	}

	/** Made in this run and everything built (roads and town too), quiet for 10 s: kept for the next start - or, with
	 *  -PaxNavBake, written where the release takes it from, and the game closes. */
	void KeepWhenDone(UNavigationSystemV1* NS, ARecastNavMesh* RN)
	{
		if (GLoaded || GSaved || !RN) return;
		const double T = FPlatformTime::Seconds();
		if (ADPLateWorld::Done() && GAllInFrom < 0.0) GAllInFrom = T;
		const bool bBusy = !ADPLateWorld::Done() || NS->IsNavigationBuildInProgress() || NS->HasDirtyAreasQueued() || NS->GetNumRemainingBuildTasks() + NS->GetNumRunningBuildTasks() > 0;
		if (bBusy) { GQuietFrom = -1.0; return; }
		if (GQuietFrom < 0.0) GQuietFrom = T;
		if (T - GQuietFrom < 10.0) return;
		GSaved = true;
		ADP_LOG(TEXT("PaxFlow: walking map check (made here): %s"), *PaxNav::Check(RN->GetWorld()));
		const bool bBake = FParse::Param(FCommandLine::Get(), TEXT("PaxNavBake"));
		FPImplRecastNavMesh* Impl = FNavAccess::Of(*RN); if (!Impl) return;
		TArray<uint8> Raw;
		FMemoryWriter Ar(Raw, true);
		Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
		Impl->Serialize(Ar, NAVMESHVER_LATEST);
		const FString File = bBake ? FPaths::ProjectDir() / TEXT("Data") / (ADPSite::IsDefault() ? FString() : TEXT("Sites") / ADPSite::Id()) / TEXT("paxnav.bin") : KeptFile();
		const int32 Tiles = RN->GetNavMeshTilesCount();
		const double Made = GQuietFrom - GStart;
		// (all of it is in it now: never generated again in this run - the doors and whatever moves cost nothing)
		MakeStatic(RN); RN->ConditionalConstructGenerator();
		Async(EAsyncExecution::ThreadPool, [Raw = MoveTemp(Raw), Key = GKey, File, Tiles, Made, bBake]() mutable
		{
			int32 Size = Raw.Num(), PackedSize = FCompression::CompressMemoryBound(NAME_Zlib, Size);
			TArray<uint8> Packed; Packed.SetNum(PackedSize);
			bool bOk = FCompression::CompressMemory(NAME_Zlib, Packed.GetData(), PackedSize, Raw.GetData(), Size);
			Packed.SetNum(PackedSize);
			TArray<uint8> Out; FMemoryWriter A(Out, true);
			uint32 Magic = NavMagic; A << Magic; A << Key; A << Size; A << Packed;
			const FString Tmp = File + TEXT(".part");
			bOk = bOk && FFileHelper::SaveArrayToFile(Out, *Tmp) && IFileManager::Get().Move(*File, *Tmp, true, true);
			AsyncTask(ENamedThreads::GameThread, [File, bOk, Tiles, MB = Out.Num() / 1048576.0, Key, Made, bBake]()
			{
				ADP_LOG(TEXT("PaxNav bake: %s %s (%d tiles, %.1f MB, key %s, all in %.0f s after it started)"), *File, bOk ? TEXT("written") : TEXT("NOT written"), Tiles, MB, *Key, Made);
				if (bBake) FGenericPlatformMisc::RequestExit(false);
			});
		});
	}
}

void PaxNav::NoNav(AActor* A)
{
	if (!A) return;
	TArray<UPrimitiveComponent*> Ps; A->GetComponents<UPrimitiveComponent>(Ps);
	for (UPrimitiveComponent* P : Ps) if (P->CanEverAffectNavigation()) P->SetCanEverAffectNavigation(false);
}

void PaxNav::Sweep(UWorld* W)
{
	if (!W) return;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (Moves(A)) { NoNav(A); continue; }
		// (a skeletal mesh anywhere is a person, an animal or a moving part)
		TArray<USkeletalMeshComponent*> Sk; A->GetComponents<USkeletalMeshComponent>(Sk);
		for (USkeletalMeshComponent* S : Sk) if (S->CanEverAffectNavigation()) S->SetCanEverAffectNavigation(false);
	}
}

void PaxNav::Start(UWorld* W, const FTerminalPlan& Plan, const FBox& Landside)
{
	if (!W || GNavWorld.Get() == W) return;
	UNavigationSystemV1* NS = Nav(W);
	if (!NS) { ADP_WARN(TEXT("PaxFlow: no navigation system in this world - walkers keep their straight ways")); return; }
	GNavWorld = W; GReady = false; GSeenBuild = false; GBuildAsked = false; GWarned = false; GVolumes = 0; GStart = FPlatformTime::Seconds();
	GLoaded = GSaved = false; GQuietFrom = GAllInFrom = -1.0;
	Sweep(W);   // (before anything is gathered: nothing that moves goes in)
	// the walkable bounds: only where passengers really walk (a little over the walls, so the doorways join) - the
	// Grand Hall and its entrances, security, the airside lobby and corridor, the escalator room, Floor 2's landing,
	// link, gallery, gate walkway and gate rooms, the baggage claim wing and its way out; the landside pavement strip
	// and the car park with its crossings. Nothing else of the building (the 18:59 test: the whole building was 60+ s)
	const float F2 = Plan.F2Y;
	TArray<FBox> Boxes;
	auto Area = [&](int32 Floor, float X0, float Z0, float X1, float Z1, float Pad)
	{
		const float Y0 = Floor >= 2 ? F2 - 0.4f : -2.f, Y1 = Floor >= 2 ? Plan.Roof + 1.f : F2 - 0.4f;
		FBox B(ForceInit);
		B += FTerminalPlan::ToWorld(FMath::Min(X0, X1) - Pad, Y0, FMath::Min(Z0, Z1) - Pad);
		B += FTerminalPlan::ToWorld(FMath::Max(X0, X1) + Pad, Y1, FMath::Max(Z0, Z1) + Pad);
		Boxes.Add(B);
	};
	static const TSet<FString> Ids = {
		TEXT("F1-HALL"), TEXT("F1-ENTR-W"), TEXT("F1-ENTR-C"), TEXT("F1-ENTR-E"), TEXT("F1-SECURITY"), TEXT("F1-AIRSIDE"),
		TEXT("F1-AIRSIDE-CORR"), TEXT("F1-ESC-PASS"), TEXT("F1-MUSEUM"),
		TEXT("F2-LANDING"), TEXT("F2-LINK"), TEXT("F2-FOOD"), TEXT("F2-TAXFREE"), TEXT("F2-WALK") };
	for (const FPlanRoom& R : Plan.Rooms)
		if (R.Kind != TEXT("empty") && (Ids.Contains(R.Id) || R.Kind == TEXT("gate2") || R.Kind == TEXT("eatery")))
			Area(R.Floor, R.X0, R.Z0, R.X1, R.Z1, 0.6f);
	Area(2, 0.f, 8.f, 16.f, 30.f, 0.6f);      // (Floor 2 over the escalators' top: off the landing to the link)
	Area(1, -82.f, -1.f, 0.f, 52.f, 0.6f);    // (the baggage claim wing - not in the plan file yet - with its belts and the arrivals escalators' foot)
	// the landside: the pavement in front of the terminal (the arrivals' doors to the departures' entrances) and the car
	// park with the crossings - its ground heights from the roads' own ground
	auto Land = [&](float X0, float Z0, float X1, float Z1)
	{
		FBox B(ForceInit);
		for (float X = X0; X <= X1 + 0.1f; X += FMath::Max(5.f, (X1 - X0) / 12.f))
			for (float Z = Z0; Z <= Z1 + 0.1f; Z += FMath::Max(5.f, (Z1 - Z0) / 12.f))
			{
				const FVector P = FTerminalPlan::ToWorld(X, 0.f, Z); const float G = float(ARoadNetwork::GroundAt(X, Z) * 100.0);
				B += FVector(P.X, P.Y, G - 300.f); B += FVector(P.X, P.Y, G + 500.f);
			}
		Boxes.Add(B);
	};
	Land(-45.f, -14.f, 133.f, 1.f);
	Land(17.f, -150.f, 133.f, -14.f);
	(void)Landside;   // (PaxFlow's own landside box: the two above cover it, smaller)
	// ready-made for exactly this terminal: loaded in a moment, nothing generated
	GKey = MakeKey(NS, Boxes);
	if (LoadReady(W, NS)) return;
	for (const FBox& B : Boxes) if (ANavMeshBoundsVolume* V = BoxVolume<ANavMeshBoundsVolume>(W, B)) { NS->OnNavigationBoundsUpdated(V); ++GVolumes; }
	// the revolving doors' drums: no navmesh in them (owner 2026-11-21) - nobody is ever routed through the glass; the
	// riders go round with a compartment instead (PaxFlow: the revolving ride, off the navmesh like the escalators)
	int32 Drums = 0;
	for (const FPlanDoor& D : Plan.Doors)
	{
		if (D.Type != TEXT("revolving")) continue;
		const FVector C = FTerminalPlan::ToWorld(D.X, Plan.FloorY(D.Floor), D.Z) + FVector(0, 0, 100.f);
		AActor* K = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(C));
		if (!K) continue;
		USceneComponent* Rc = NewObject<USceneComponent>(K, TEXT("Root"));
		K->SetRootComponent(Rc); Rc->RegisterComponent(); K->SetActorLocation(C);
		UNavModifierComponent* M = NewObject<UNavModifierComponent>(K, TEXT("Drum"));
		const float E = D.Width * 50.f + 20.f;
		M->FailsafeExtent = FVector(E, E, 120.f);
		M->AreaClass = UNavArea_Null::StaticClass();
		M->RegisterComponent();
		++Drums;
	}
	ADP_LOG(TEXT("PaxFlow: navmesh - %d walkable areas (where passengers walk, both floors + the landside), %d revolving door drums kept out, generating in the background"), GVolumes, Drums);
}

void PaxNav::Kick(UWorld* W)
{
	if (!W || GNavWorld.Get() == W) return;
	// (as early as play begins: the terminal's pieces are placed in the very first frame - its walls are what the
	//  navmesh is made from)
	if (!W->HasBegunPlay()) return;
	FTerminalPlan Plan; if (!Plan.Load()) return;
	Start(W, Plan, FBox(ForceInit));
}

bool PaxNav::Failed(UWorld* W) { return W && GNavWorld.Get() == W && !GReady && GWarned && FPlatformTime::Seconds() - GStart > 120.0; }

int32 PaxNav::JobsLeft(UWorld* W)
{
	UNavigationSystemV1* NS = Nav(W);
	return NS ? NS->GetNumRemainingBuildTasks() + NS->GetNumRunningBuildTasks() : 0;
}

bool PaxNav::Making()
{
	if (!GNavWorld.IsValid() || GLoaded || GSaved) return false;
	if (GAllInFrom < 0.0 && ADPLateWorld::Done()) GAllInFrom = FPlatformTime::Seconds();
	return GAllInFrom < 0.0 || FPlatformTime::Seconds() - GAllInFrom < 180.0;
}

bool PaxNav::Started(UWorld* W) { return W && GNavWorld.Get() == W; }

bool PaxNav::Ready(UWorld* W)
{
	if (!W || GNavWorld.Get() != W) return false;
	UNavigationSystemV1* NS = Nav(W); if (!NS) return false;
	if (GReady) { if (!GLoaded && !GSaved) KeepWhenDone(NS, Cast<ARecastNavMesh>(NavData(W, NS))); return true; }
	const double T = FPlatformTime::Seconds() - GStart;
	ANavigationData* ND = NavData(W, NS);
	if (!ND)
	{
		// (the navigation makes its navmesh actor itself when the first walkable area comes in; if it hasn't after 5 s,
		//  ask for the build outright - once, a short wait while it is made)
		if (T > 5.0 && !GBuildAsked)
		{
			GBuildAsked = true;
			ADP_WARN(TEXT("PaxFlow: no navmesh after %.0f s (%d walkable areas registered) - building it now"), T, NS->GetNavigationBounds().Num());
			NS->Build();
		}
		if (T > 60.0 && !GWarned) { GWarned = true; ADP_WARN(TEXT("PaxFlow: the navmesh never came (60 s, %d walkable areas registered, no navmesh actor) - walkers keep their straight ways"), NS->GetNavigationBounds().Num()); }
		return false;
	}
	const bool bBuilding = NS->IsNavigationBuildInProgress() || NS->HasDirtyAreasQueued();
	if (bBuilding)
	{
		GSeenBuild = true;
		if (T > 60.0 && !GWarned) { GWarned = true; ADP_WARN(TEXT("PaxFlow: the navmesh is still building after 60 s (%d tile jobs left, %d tiles done) - walkers keep their straight ways until it is done"), NS->GetNumRemainingBuildTasks() + NS->GetNumRunningBuildTasks(), Cast<ARecastNavMesh>(ND) ? Cast<ARecastNavMesh>(ND)->GetNavMeshTilesCount() : 0); }
		return false;
	}
	if (!GSeenBuild && T < 2.0) return false;   // (the first tiles not even queued yet)
	const ARecastNavMesh* RN = Cast<ARecastNavMesh>(ND);
	const int32 Tiles = RN ? RN->GetNavMeshTilesCount() : 0;
	if (Tiles <= 0)
	{
		if (!GWarned && T > 60.0) { GWarned = true; ADP_WARN(TEXT("PaxFlow: the navmesh came out empty (0 tiles after 60 s) - walkers keep their straight ways")); }
		return false;
	}
	GReady = true;
	ADP_LOG(TEXT("PaxFlow: navmesh ready, made here (%d tiles, %.0f ms)"), Tiles, T * 1000.0);
	return true;
}

FString PaxNav::Check(UWorld* W)
{
	FTerminalPlan Plan; if (!W || !Plan.Load()) return TEXT("no plan");
	int32 N[4] = {}; double Len = 0.0; FString Not;
	auto Mid = [&Plan](const FPlanRoom& R) { return FTerminalPlan::ToWorld((R.X0 + R.X1) * 0.5f, (R.Floor >= 2 ? Plan.F2Y : 0.f) + 0.1f, (R.Z0 + R.Z1) * 0.5f); };
	for (int32 Floor = 1; Floor <= 2; ++Floor)
	{
		const FPlanRoom* From = Plan.Rooms.FindByPredicate([Floor](const FPlanRoom& R) { return R.Id == (Floor == 1 ? TEXT("F1-HALL") : TEXT("F2-LANDING")); });
		if (!From) continue;
		auto Try = [&](const FString& Name, const FVector& To)
		{
			TArray<FVector> P; const EPath R = Path(W, Mid(*From), To, P);
			++N[int32(R)];
			if (R == Full) for (int32 i = 1; i < P.Num(); ++i) Len += FVector::Dist(P[i - 1], P[i]);
			else Not += FString::Printf(TEXT("%s%s %s"), Not.IsEmpty() ? TEXT("") : TEXT(", "), *Name, R == NoWay ? TEXT("no way") : R == Partial ? TEXT("part of the way") : TEXT("off the map"));
		};
		for (const FPlanRoom& R : Plan.Rooms)
			if (R.Floor == Floor && &R != From && R.Kind != TEXT("empty") && (R.Kind == TEXT("gate2") || R.Id.StartsWith(TEXT("F1-ENTR")) || R.Id == TEXT("F1-SECURITY") || R.Id == TEXT("F1-AIRSIDE") || R.Id == TEXT("F2-LINK") || R.Id == TEXT("F2-WALK") || R.Id == TEXT("F2-TAXFREE"))) Try(R.Id, Mid(R));
		if (Floor == 1) { FVector K = FTerminalPlan::ToWorld(75.f, 0.f, -8.f); K.Z = ARoadNetwork::GroundAt(75.0, -8.0) * 100.0 + 10.0; Try(TEXT("FORECOURT"), K); }
	}
	return FString::Printf(TEXT("%d reached (%.1f m), %d part of the way, %d no way, %d off the map%s"), N[Full], Len / 100.0, N[Partial], N[NoWay], N[OffMesh], Not.IsEmpty() ? TEXT("") : *(TEXT(": ") + Not));
}

PaxNav::EPath PaxNav::Path(UWorld* W, const FVector& From, const FVector& To, TArray<FVector>& Out)
{
	Out.Reset();
	UNavigationSystemV1* NS = Nav(W); if (!NS || !GReady) return OffMesh;
	ANavigationData* ND = NavData(W, NS); if (!ND) return OffMesh;
	// (both ends onto the navmesh: within a stride sideways, a knee's height up or down)
	const FVector Ext(60.f, 60.f, 120.f);
	FNavLocation A, B;
	if (!NS->ProjectPointToNavigation(From, A, Ext, ND) || !NS->ProjectPointToNavigation(To, B, Ext, ND)) return OffMesh;
	FPathFindingQuery Q(nullptr, *ND, A.Location, B.Location);
	const FPathFindingResult R = NS->FindPathSync(Q);
	if (!R.IsSuccessful() || !R.Path.IsValid()) return NoWay;
	for (const FNavPathPoint& Pt : R.Path->GetPathPoints()) Out.Add(Pt.Location);
	if (Out.Num() < 2) return NoWay;
	return R.IsPartial() ? Partial : Full;
}
