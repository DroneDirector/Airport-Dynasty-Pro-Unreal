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

namespace
{
	TWeakObjectPtr<UWorld> GNavWorld;
	double GStart = 0.0;
	bool GReady = false, GSeenBuild = false;
	int32 GVolumes = 0;

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
		}
		V->FinishSpawning(At);
		return V;
	}

	UNavigationSystemV1* Nav(UWorld* W) { return W ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(W) : nullptr; }
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
	GNavWorld = W; GReady = false; GSeenBuild = false; GVolumes = 0; GStart = FPlatformTime::Seconds();
	Sweep(W);   // (before anything is gathered: nothing that moves goes in)
	// the walkable bounds: every finished room of the terminal on its own floor (a little over its walls, so the
	// doorways join), the unfinished ones (kind "empty") left out - nothing outside the rooms, nothing in the rooms that
	// aren't built; and the landside: the car park, the forecourt, its crossings
	const float F2 = Plan.F2Y;
	for (const FPlanRoom& R : Plan.Rooms)
	{
		if (R.Kind == TEXT("empty")) continue;
		const float Y0 = R.Floor >= 2 ? F2 - 0.4f : -2.f, Y1 = R.Floor >= 2 ? Plan.Roof + 1.f : F2 - 0.4f;
		FBox B(ForceInit);
		B += FTerminalPlan::ToWorld(FMath::Min(R.X0, R.X1) - 0.6f, Y0, FMath::Min(R.Z0, R.Z1) - 0.6f);
		B += FTerminalPlan::ToWorld(FMath::Max(R.X0, R.X1) + 0.6f, Y1, FMath::Max(R.Z0, R.Z1) + 0.6f);
		if (BoxVolume<ANavMeshBoundsVolume>(W, B)) ++GVolumes;
	}
	if (Landside.IsValid && BoxVolume<ANavMeshBoundsVolume>(W, Landside)) ++GVolumes;
	ADP_LOG(TEXT("PaxFlow: navmesh - %d walkable areas (finished rooms on both floors + the landside), generating in the background"), GVolumes);
}

bool PaxNav::Ready(UWorld* W)
{
	if (!W || GNavWorld.Get() != W) return false;
	if (GReady) return true;
	UNavigationSystemV1* NS = Nav(W); if (!NS) return false;
	ANavigationData* ND = NS->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (!ND) return false;
	const bool bBuilding = NS->IsNavigationBuildInProgress() || NS->HasDirtyAreasQueued();
	if (bBuilding) { GSeenBuild = true; return false; }
	if (!GSeenBuild && FPlatformTime::Seconds() - GStart < 2.0) return false;   // (the first tiles not even queued yet)
	GReady = true;
	const ARecastNavMesh* RN = Cast<ARecastNavMesh>(ND);
	ADP_LOG(TEXT("PaxFlow: navmesh ready (%d tiles, %.0f ms)"), RN ? RN->GetNavMeshTilesCount() : 0, (FPlatformTime::Seconds() - GStart) * 1000.0);
	return true;
}

PaxNav::EPath PaxNav::Path(UWorld* W, const FVector& From, const FVector& To, TArray<FVector>& Out)
{
	Out.Reset();
	UNavigationSystemV1* NS = Nav(W); if (!NS || !GReady) return OffMesh;
	ANavigationData* ND = NS->GetDefaultNavDataInstance(FNavigationSystem::DontCreate); if (!ND) return OffMesh;
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
