// The grass round the airport (see GrassField.h).
#include "GrassField.h"
#include "Misc/ScopeExit.h"
#include "ADPLog.h"
#include "LandCover.h"
// (Windows' own OPAQUE macro would break Cesium's glTF names when the build puts a Windows file before this one)
#ifdef OPAQUE
#undef OPAQUE
#endif
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "EngineUtils.h"
#include "TerminalPlan.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "LandCorridor.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	/** The globe's own georeference (Globe.cpp spawns it); Cesium's "default" one is another, at 0 N 0 E. */
	ACesiumGeoreference* GrassGlobeGeo(UWorld* W)
	{
		// (kept once found: it was searched for on every look down - 2026-11-15)
		static TWeakObjectPtr<ACesiumGeoreference> Kept;
		if (Kept.IsValid() && Kept->GetWorld() == W) return Kept.Get();
		if (W) for (TActorIterator<ACesiumGeoreference> It(W); It; ++It) if (!It->GetName().Contains(TEXT("Default"))) { Kept = *It; return *It; }
		return nullptr;
	}
}

float AGrassField::NearDistance = 45.f;
float AGrassField::FarReach = 2500.f;
float AGrassField::FarReachGround = 600.f;
static int32 FarTufts = 0;
bool AGrassField::bOn = true;
bool AGrassField::bAway = false;

namespace
{
	constexpr float MidReachGround = 18000.f, MidReachAir = 40000.f;   // the simpler swaying clumps: 180 m on the ground, 400 m in the air
	constexpr float FarStep = 250.f;        // the far tufts: one in every 2.5 m square (wide, so the field reads as grass)
	constexpr int32 FarRowsPerTick = 2;     // 4 m rows of the whole airfield laid per frame

	/** Where the airfield's grass also grows outside it (owner, 2026-11-15): Data/grass-area.png + .json (Tools/
	 *  make_grass_area.py) - a ring 5 km round the airfield and all our own land along the roads and in the town, never
	 *  within 30 m of water. Plan metres, 5 m a pixel. */
	struct FGrassArea
	{
		double X0 = 0, Y0 = 0, Step = 5; int32 NX = 0, NY = 0; TArray<uint8> Px; bool bRead = false;
		/** Inside the map and not grass there (a road, a car park, a building, water, outside the ring): none may grow. */
		bool Bare(double X, double Y) const
		{
			const int32 i = FMath::FloorToInt((X - X0) / Step), j = FMath::FloorToInt((Y - Y0) / Step);
			return i >= 0 && j >= 0 && i < NX && j < NY && Px[j * NX + i] < 200;   // (128: a road over grass ground; 0: none)
		}
		bool At(double X, double Y) const
		{
			const int32 i = FMath::FloorToInt((X - X0) / Step), j = FMath::FloorToInt((Y - Y0) / Step);
			return i >= 0 && j >= 0 && i < NX && j < NY && Px[j * NX + i] > 127;
		}
	};
	const FGrassArea& GrassArea()
	{
		static FGrassArea A;
		if (A.bRead) return A;
		A.bRead = true;
		auto File = [](const TCHAR* N) { const FString C = FPaths::ProjectContentDir() / TEXT("Data") / N; return FPaths::FileExists(C) ? C : FPaths::ProjectDir() / TEXT("Data") / N; };
		FString Text; TSharedPtr<FJsonObject> J; TArray<uint8> Png, Raw;
		if (!FFileHelper::LoadFileToString(Text, *File(TEXT("grass-area.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()) return A;
		if (!FFileHelper::LoadFileToArray(Png, *File(TEXT("grass-area.png")))) return A;
		IImageWrapperModule& M = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> W = M.CreateImageWrapper(EImageFormat::PNG);
		if (!W.IsValid() || !W->SetCompressed(Png.GetData(), Png.Num()) || !W->GetRaw(ERGBFormat::Gray, 8, Raw)) return A;
		A.X0 = J->GetNumberField(TEXT("x0")); A.Y0 = J->GetNumberField(TEXT("y0")); A.Step = J->GetNumberField(TEXT("step"));
		A.NX = W->GetWidth(); A.NY = W->GetHeight(); A.Px = MoveTemp(Raw);
		ADP_LOG(TEXT("Grass: the area outside the airfield %d x %d at %.0f m"), A.NX, A.NY, A.Step);
		return A;
	}

	/** Is what the look down found grass? The airfield's grass slab (its "grass" list) or the model's lawns. */
	bool IsGrass(const FHitResult& H)
	{
		const UPrimitiveComponent* C = H.GetComponent();
		if (!C || H.ImpactNormal.Z < 0.85f) return false;
		if (C->GetFName() == TEXT("grass")) return true;
		if (const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(C))
			if (const UStaticMesh* M = S->GetStaticMesh()) if (M->GetName() == TEXT("terminal_mesh_2611")) return true;
		// our own land along the roads and the town, and the globe in the ring round the airfield: the same grass,
		// wherever the area says (owner, 2026-11-15: black sand too, 30 m clear of water); roads, kerbs, pavements,
		// corners and car parks are met first by the look down and stay bare
		const bool bOurLand = C->GetOwner() && C->GetOwner()->IsA<ALandCorridor>();
		const bool bGlobe = C->GetOwner() && C->GetOwner()->IsA<ACesium3DTileset>();
		if ((bOurLand || bGlobe) && GrassArea().At(H.ImpactPoint.X / 100.0, -H.ImpactPoint.Y / 100.0)) return true;
		if (bOurLand) return false;
		// the real ground (the globe's terrain): grass where the land cover says the land is grassy
		if (bGlobe)
			if (ACesiumGeoreference* G = GrassGlobeGeo(C->GetWorld()))
			{
				const FVector LL = G->TransformUnrealPositionToLongitudeLatitudeHeight(H.ImpactPoint);
				return ADPLandCover::Grows(ADPLandCover::ClassAt(LL.X, LL.Y));
			}
		return false;
	}
}

AGrassField::AGrassField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AGrassField::Start(float X0, float Y0, float X1, float Y1)
{
	Near = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Materials/Grass/SM_GrassNear.SM_GrassNear"));
	Mid = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Materials/Grass/SM_GrassFar.SM_GrassFar"));
	Tuft = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Materials/Grass/SM_GrassTuft.SM_GrassTuft"));
	const FVector A = FTerminalPlan::ToWorld(X0, 0, Y0), B = FTerminalPlan::ToWorld(X1, 0, Y1);
	Field = FBox2D(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)));
	FarY = Field.Min.Y;
	{ bool B0 = true; GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("Grass"), B0, GGameUserSettingsIni); bOn = B0; }
	GConfig->GetFloat(TEXT("ADP.Graphics"), TEXT("GrassNear"), NearDistance, GGameUserSettingsIni);
	ADP_LOG(TEXT("Grass: %s, field %.0f x %.0f m"), Near && Mid && Tuft ? TEXT("ready") : TEXT("meshes missing"), Field.GetSize().X / 100, Field.GetSize().Y / 100);
}

bool AGrassField::GroundAt(float X, float Y, float& Z) const
{
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(GrassLook), true, this);   // complex: the globe's terrain has only its own shape to hit
	// (inside the airfield the terrain is cut away: the grass grows on the airfield's own grass there)
	if (!bAway && Field.IsInside(FVector2D(X, Y)))
		for (TActorIterator<ACesium3DTileset> It(GetWorld()); It; ++It) Q.AddIgnoredActor(*It);
	if (!GetWorld()->LineTraceSingleByChannel(H, FVector(X, Y, 30000.f), FVector(X, Y, -5000.f), ECC_Visibility, Q) || !IsGrass(H)) return false;
	Z = H.ImpactPoint.Z;
	return true;
}

bool AGrassField::PatchStep(const FIntPoint& C, double Deadline)
{
	FPatch& P = Patches.FindOrAdd(C);
	const int32 N = int32(Cell / 100.f);
	if (P.H.Num() != N * N) { P.H.SetNumUninitialized(N * N); P.Rows = 0; }
	while (P.Rows < N && FPlatformTime::Seconds() < Deadline)
	{
		const int32 j = P.Rows++;
		for (int32 i = 0; i < N; ++i)
		{
			float Z;
			P.H[j * N + i] = GroundAt(C.X * Cell + (i + 0.5f) * 100.f, C.Y * Cell + (j + 0.5f) * 100.f, Z) ? Z : TNumericLimits<float>::Lowest();
		}
	}
	return P.Rows >= N;
}

const AGrassField::FPatch& AGrassField::Patch(const FIntPoint& C)
{
	PatchStep(C, 1e30);
	return Patches.FindChecked(C);
}

UHierarchicalInstancedStaticMeshComponent* AGrassField::NewLayer(UStaticMesh* Mesh, float WpoUntil, float CullFrom, float CullTo)
{
	UHierarchicalInstancedStaticMeshComponent* K = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	K->SetupAttachment(Root);
	K->SetStaticMesh(Mesh);
	K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	K->SetCastShadow(false);
	K->bAffectDistanceFieldLighting = false;
	K->SetCanEverAffectNavigation(false);
	K->WorldPositionOffsetDisableDistance = int32(WpoUntil / 100.f);   // the wind only close by (metres)
	K->SetEvaluateWorldPositionOffset(WpoUntil > 0);
	K->SetCullDistances(int32(CullFrom), int32(CullTo));
	K->SetVisibility(bOn);
	K->RegisterComponent();
	return K;
}

void AGrassField::FillCell(UHierarchicalInstancedStaticMeshComponent* K, const FIntPoint& C, float PerM2, float S0, float S1, int32 Seed, const FVector& From, float Reach)
{
	const FPatch& P = Patch(C);
	const int32 N = int32(Cell / 100.f);
	FRandomStream R(Seed ^ (C.X * 73856093) ^ (C.Y * 19349663));
	TArray<FTransform> T;
	for (int32 j = 0; j < N; ++j)
		for (int32 i = 0; i < N; ++i)
		{
			const float Z = P.H[j * N + i];
			if (Z == TNumericLimits<float>::Lowest()) continue;
			// a whole number of clumps per square metre on average, scattered inside it
			int32 Count = int32(PerM2); if (R.FRand() < PerM2 - Count) ++Count;
			for (int32 k = 0; k < Count; ++k)
			{
				float S = R.FRandRange(S0, S1);
				const FVector At(C.X * Cell + (i + R.FRand()) * 100.f, C.Y * Cell + (j + R.FRand()) * 100.f, Z - 1.f);
				// (the outer quarter of the layer's reach: fewer and smaller toward its edge, so it fades into the ground's own
				// grass colour - no hard square of grass round the camera, 2026-11-15)
				if (Reach > 0.f)
				{
					const float D = FVector2D::Distance(FVector2D(At), FVector2D(From)), F = FMath::Clamp((Reach - D) / (Reach * 0.25f), 0.f, 1.f);
					if (F <= 0.f || R.FRand() > F) continue;
					S *= FMath::Lerp(0.55f, 1.f, F);
				}
				T.Add(FTransform(FRotator(0, R.FRandRange(0, 360), 0), At, FVector(S, S, S * R.FRandRange(0.85f, 1.2f))));
			}
		}
	if (T.Num()) K->AddInstances(T, false, true);
}

void AGrassField::ApplyOn()
{
	for (auto& It : NearCells) if (It.Value) It.Value->SetVisibility(bOn);
	for (auto& It : MidCells) if (It.Value) It.Value->SetVisibility(bOn);
	for (auto& It : FarCells) if (It.Value) It.Value->SetVisibility(bOn);
	for (auto& K : FarRows) if (K) K->SetVisibility(bOn);
	bLastOn = bOn;
}

static double GrassTickSum = 0; static int32 GrassTickN = 0;
void AGrassField::Tick(float Dt)
{
	Super::Tick(Dt);
	const double TickStart = FPlatformTime::Seconds();
	ON_SCOPE_EXIT { GrassTickSum += FPlatformTime::Seconds() - TickStart; ++GrassTickN; };
	if (!Near || !Mid || !Tuft || !Field.bIsValid) return;
	if (bOn != bLastOn) ApplyOn();
	// away from the airport: its field's far tufts hidden; the patches found there forgotten (the ground moved)
	if (bAway != bLastAway) { bLastAway = bAway; for (auto& K : FarRows) if (K) K->SetVisibility(!bAway && bOn); Patches.Reset(); for (auto& It : NearCells) if (It.Value) It.Value->DestroyComponent(); NearCells.Reset(); for (auto& It : MidCells) if (It.Value) It.Value->DestroyComponent(); MidCells.Reset(); for (auto& It : FarCells) if (It.Value) It.Value->DestroyComponent(); FarCells.Reset(); }

	// the whole airfield's tufts, a few rows each frame until it's all laid (once)
	if (FarY < Field.Max.Y && !bAway)
	{
		UHierarchicalInstancedStaticMeshComponent* K = NewLayer(Tuft, 0.f, 0.f, 0.f);
		FRandomStream R{int32(FarY)};
		TArray<FTransform> T;
		for (int32 r = 0; r < FarRowsPerTick && FarY < Field.Max.Y; ++r, FarY += FarStep)
			for (float X = Field.Min.X; X < Field.Max.X; X += FarStep)
			{
				const float PX = X + R.FRand() * FarStep, PY = FarY + R.FRand() * FarStep;
				float Z;
				if (GroundAt(PX, PY, Z)) { const float S = R.FRandRange(2.2f, 3.2f); T.Add(FTransform(FRotator(0, R.FRandRange(0, 360), 0), FVector(PX, PY, Z - 2.f), FVector(S, S, S * 0.8f))); }
			}
		if (T.Num()) { K->AddInstances(T, false, true); FarRows.Add(K); } else K->DestroyComponent();
	}

	// round the camera, wherever it is (walking, the drone, a view camera, the picture tests): the blades are where you look
	// from (2026-11-15: they grew round your body only, so a camera elsewhere saw none)
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;
	FVector L;
	if (PC->PlayerCameraManager) L = PC->PlayerCameraManager->GetCameraLocation();
	else if (const APawn* Me = PC->GetPawn()) L = Me->GetActorLocation();
	else return;
	const FIntPoint At(FMath::FloorToInt(L.X / Cell), FMath::FloorToInt(L.Y / Cell));
	const float NearReach = bOn ? FMath::Clamp(NearDistance, 0.f, 150.f) * 100.f : 0.f;

	// the layers' cells round you: dropped when out of reach, the nearest missing ones added
	auto Keep = [&](TMap<FIntPoint, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Cells, float Reach, float Sz)
	{
		for (auto It = Cells.CreateIterator(); It; ++It)
		{
			const FVector2D Mid2((It.Key().X + 0.5f) * Sz, (It.Key().Y + 0.5f) * Sz);
			if (FVector2D::Distance(Mid2, FVector2D(L)) > Reach + Sz) { if (It.Value()) It.Value()->DestroyComponent(); It.RemoveCurrent(); }
		}
	};
	auto Missing = [&](const TMap<FIntPoint, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Cells, float Reach, float Sz, FIntPoint& Out)
	{
		const FIntPoint At2(FMath::FloorToInt(L.X / Sz), FMath::FloorToInt(L.Y / Sz));
		const int32 R = FMath::CeilToInt(Reach / Sz);
		float Best = TNumericLimits<float>::Max();
		for (int32 y = -R; y <= R; ++y)
			for (int32 x = -R; x <= R; ++x)
			{
				const FIntPoint C(At2.X + x, At2.Y + y);
				const FVector2D Mid2((C.X + 0.5f) * Sz, (C.Y + 0.5f) * Sz);
				const float D = FVector2D::Distance(Mid2, FVector2D(L));
				if (D > Reach + Sz * 0.7f || Cells.Contains(C)) continue;
				if (D < Best) { Best = D; Out = C; }
			}
		return Best < TNumericLimits<float>::Max();
	};
	// the far grass's reach: FarReachGround on the ground, rising with the camera's height over it to FarReach by 300 m
	// up (owner, 2026-11-15: walking and driving need less; from a helicopter or a plane the grass goes on far)
	if (FPlatformTime::Seconds() - CamHeightAt > 0.5)
	{
		CamHeightAt = FPlatformTime::Seconds();
		FHitResult HH; FCollisionQueryParams QQ(SCENE_QUERY_STAT(GrassCam), false, this);
		if (GetWorld()->LineTraceSingleByChannel(HH, L, L - FVector(0, 0, 500000.f), ECC_Visibility, QQ)) CamHeight = (L.Z - HH.ImpactPoint.Z) / 100.f;
	}
	const float Lift = FMath::Clamp((CamHeight - 20.f) / 280.f, 0.f, 1.f);
	const float FarR = bOn ? FMath::Lerp(FMath::Min(FarReachGround, FarReach), FMath::Clamp(FarReach, 0.f, 4000.f), Lift) * 100.f : 0.f;
	Keep(NearCells, NearReach, Cell);
	// (the middle grass too: 180 m on the ground - the near grass and the land's own rise hide what is further - to 400 m
	// in the air; owner, 2026-11-15: at ground level the frames matter, from up high the grass must not visibly stop)
	const float MidReach = FMath::Lerp(MidReachGround, MidReachAir, Lift);
	Keep(MidCells, bOn ? MidReach : 0.f, MidCell);
	Keep(FarCells, FarR, FarCell);
	// a far piece whose distance band changed (you flew closer or away) is laid again at its new spacing; 100 m of slack
	// so a piece on a band's edge does not flicker between the two
	for (auto It = FarCells.CreateIterator(); It; ++It)
	{
		const int32* B = FarBand.Find(It.Key()); if (!B) continue;
		const float Dm = FVector2D::Distance(FVector2D((It.Key().X + 0.5f) * FarCell, (It.Key().Y + 0.5f) * FarCell), FVector2D(L)) / 100.f;
		const int32 Lo = FarBandAt(Dm - 100.f), Hi = FarBandAt(Dm + 100.f);
		if (*B < Lo || *B > Hi) { if (It.Value()) It.Value()->DestroyComponent(); FarBand.Remove(It.Key()); It.RemoveCurrent(); }
	}
	for (auto It = FarBand.CreateIterator(); It; ++It) if (!FarCells.Contains(It.Key())) It.RemoveCurrent();
	// one patch laid at a time, its ground measured a little each frame within about 2.5 ms (2026-11-15: whole patches
	// at once stalled the game at 3 to 9 frames a second); the layers take turns - near, middle, far
	const double Deadline = FPlatformTime::Seconds() + 0.0025;
	static int32 Turn = 0;
	FIntPoint C;
	while (FPlatformTime::Seconds() < Deadline)
	{
		if (Job.Layer < 0)
		{
			bool bGot = false;
			for (int32 t = 0; t < 3 && !bGot; ++t)
			{
				const int32 Ly = (Turn + t) % 3;
				if (Ly == 0 && NearReach > 0 && Missing(NearCells, NearReach, Cell, C)) { Job = FJob(); Job.Layer = 0; Job.C = C; Job.Reach = NearReach; bGot = true; }
				else if (Ly == 1 && bOn && Missing(MidCells, MidReach, MidCell, C)) { Job = FJob(); Job.Layer = 1; Job.C = C; Job.Reach = MidReach; Job.Grid.SetNumUninitialized(MidGrid * MidGrid); bGot = true; }
				else if (Ly == 2 && FarR > 0 && Missing(FarCells, FarR, FarCell, C)) { Job = FJob(); Job.Layer = 2; Job.C = C; Job.Reach = FarR; Job.Grid.SetNumUninitialized(FarGrid * FarGrid); bGot = true; }
			}
			Turn = (Turn + 1) % 3;
			if (!bGot) break;
			Job.From = L;
		}
		if (Job.Layer == 0)
		{
			if (!PatchStep(Job.C, Deadline)) break;
			UHierarchicalInstancedStaticMeshComponent* K = NewLayer(Near, 5000.f, Job.Reach * 0.85f, Job.Reach);
			FillCell(K, Job.C, 14.f, 0.85f, 1.2f, 1, Job.From, Job.Reach);
			NearCells.Add(Job.C, K);
			Job.Layer = -1;
		}
		else if (Job.Layer == 1)
		{
			const float GS = MidCell / (MidGrid - 1);
			while (Job.Done < MidGrid * MidGrid && FPlatformTime::Seconds() < Deadline)
			{
				const int32 gi = Job.Done % MidGrid, gj = Job.Done / MidGrid;
				float Z;
				Job.Grid[Job.Done++] = GroundAt(Job.C.X * MidCell + gi * GS, Job.C.Y * MidCell + gj * GS, Z) ? Z : TNumericLimits<float>::Lowest();
			}
			if (Job.Done < MidGrid * MidGrid) break;
			UHierarchicalInstancedStaticMeshComponent* K = NewLayer(Mid, 12000.f, Job.Reach * 0.9f, Job.Reach);
			FillMidCell(K, Job.C, Job.From, Job.Reach, Job.Grid);
			MidCells.Add(Job.C, K);
			Job.Layer = -1;
		}
		else
		{
			const float GS = FarCell / (FarGrid - 1);
			while (Job.Done < FarGrid * FarGrid && FPlatformTime::Seconds() < Deadline)
			{
				const int32 gi = Job.Done % FarGrid, gj = Job.Done / FarGrid;
				float Z;
				Job.Grid[Job.Done++] = GroundAt(Job.C.X * FarCell + gi * GS, Job.C.Y * FarCell + gj * GS, Z) ? Z : TNumericLimits<float>::Lowest();
			}
			if (Job.Done < FarGrid * FarGrid) break;
			UHierarchicalInstancedStaticMeshComponent* K = NewLayer(Tuft, 0.f, FarReach * 100.f * 0.92f, FarReach * 100.f);
			FillFarCell(K, Job.C, Job.From, Job.Reach, Job.Grid);
			FarCells.Add(Job.C, K);
			Job.Layer = -1;
		}
	}
	{ static double LastLog = 0; const double Now = FPlatformTime::Seconds(); if (Now - LastLog > 5.0) { LastLog = Now; ADP_LOG(TEXT("Grass: %d near, %d mid, %d far patches (%d far tufts so far), %d ground patches known; camera %.0f m up, far grass to %.0f m; its own work %.2f ms a frame"), NearCells.Num(), MidCells.Num(), FarCells.Num(), FarTufts, Patches.Num(), CamHeight, FarR / 100.f, GrassTickN ? GrassTickSum / GrassTickN * 1000.0 : 0.0); GrassTickSum = 0; GrassTickN = 0; } }
}

void AGrassField::FillMidCell(UHierarchicalInstancedStaticMeshComponent* K, const FIntPoint& C, const FVector& From, float Reach, const TArray<float>& Grid)
{
	// the middle grass (45-300 m): 3 swaying clumps a square metre, their height from the patch's ground measured every
	// 2.5 m (MidGrid; a look down for every metre was 280 000 of them round you, 2026-11-15), none where a corner round
	// them is not grass (so they keep 2.5 m off a road at most), fewer and smaller over the outer quarter of the reach
	const int32 G = MidGrid, N = int32(MidCell / 100.f);
	const float GS = MidCell / (G - 1), Lo = TNumericLimits<float>::Lowest();
	FRandomStream R(2 ^ (C.X * 73856093) ^ (C.Y * 19349663));
	TArray<FTransform> T;
	for (int32 j = 0; j < N; ++j)
		for (int32 i = 0; i < N; ++i)
		{
			int32 Count = 3;
			for (int32 k = 0; k < Count; ++k)
			{
				const float LX = (i + R.FRand()) * 100.f, LY = (j + R.FRand()) * 100.f;
				float S = R.FRandRange(1.3f, 1.8f); const float Ry = R.FRandRange(0, 360), Rh = R.FRandRange(0.85f, 1.2f), Rs = R.FRand();
				const float X = C.X * MidCell + LX, Y = C.Y * MidCell + LY;
				const float D = FVector2D::Distance(FVector2D(X, Y), FVector2D(From)), F = FMath::Clamp((Reach - D) / (Reach * 0.25f), 0.f, 1.f);
				if (F <= 0.f || Rs > F) continue;
				S *= FMath::Lerp(0.55f, 1.f, F);
				const int32 gi = FMath::Min(int32(LX / GS), G - 2), gj = FMath::Min(int32(LY / GS), G - 2);
				const float fx = LX / GS - gi, fy = LY / GS - gj;
				const float z00 = Grid[gj * G + gi], z10 = Grid[gj * G + gi + 1], z01 = Grid[(gj + 1) * G + gi], z11 = Grid[(gj + 1) * G + gi + 1];
				if (z00 == Lo || z10 == Lo || z01 == Lo || z11 == Lo) continue;
				const float Z = FMath::Lerp(FMath::Lerp(z00, z10, fx), FMath::Lerp(z01, z11, fx), fy);
				T.Add(FTransform(FRotator(0, Ry, 0), FVector(X, Y, Z - 2.f), FVector(S, S, S * Rh)));
			}
		}
	if (T.Num()) K->AddInstances(T, false, true);
}

void AGrassField::FillFarCell(UHierarchicalInstancedStaticMeshComponent* K, const FIntPoint& C, const FVector& From, float Reach, const TArray<float>& Grid)
{
	// one wide still tuft in every 4 m square, its height from the patch's ground measured every 20 m (FarGrid: the far
	// grass needs no finer - 2026-11-15: a look down for every tuft stalled the game), fewer and smaller over the outer
	// quarter of the reach so it fades into the ground's own grass colour; none inside the airfield (its tufts are laid
	// once, AGrassField::Start), none where a corner round it is not grass
	// the spacing band (GrassField.h FarBand): the same ground covered by fewer, bigger tufts further out
	const int32 Band = FarBandAt(FVector2D::Distance(FVector2D((C.X + 0.5f) * FarCell, (C.Y + 0.5f) * FarCell), FVector2D(From)) / 100.f);
	const float Mult = Band == 0 ? 1.f : Band == 1 ? 2.f : 3.f;
	FarBand.Add(C, Band);
	const float Step = 400.f * Mult; const int32 G = FarGrid, N = int32(FarCell / Step);
	const float GS = FarCell / (G - 1);
	FRandomStream R(7 ^ (C.X * 73856093) ^ (C.Y * 19349663));
	TArray<FTransform> T;
	for (int32 j = 0; j < N; ++j)
		for (int32 i = 0; i < N; ++i)
		{
			const float LX = (i + R.FRand()) * Step, LY = (j + R.FRand()) * Step;
			const float X = C.X * FarCell + LX, Y = C.Y * FarCell + LY;
			const float Rs = R.FRand(), Ry = R.FRandRange(0, 360), Rh = R.FRandRange(0.75f, 0.95f);
			float S = R.FRandRange(2.8f, 4.0f) * Mult;
			if (!bAway && Field.IsInside(FVector2D(X, Y))) continue;
			if (!bAway && GrassArea().Bare(X / 100.0, -Y / 100.0)) continue;   // (its 20 m ground grid can't see an 8 m road: the map can)
			const float D = FVector2D::Distance(FVector2D(X, Y), FVector2D(From)), F = FMath::Clamp((Reach - D) / (Reach * 0.25f), 0.f, 1.f);
			if (F <= 0.f || Rs > F) continue;
			S *= FMath::Lerp(0.6f, 1.f, F);
			const int32 gi = FMath::Min(int32(LX / GS), G - 2), gj = FMath::Min(int32(LY / GS), G - 2);
			const float fx = LX / GS - gi, fy = LY / GS - gj;
			const float z00 = Grid[gj * G + gi], z10 = Grid[gj * G + gi + 1], z01 = Grid[(gj + 1) * G + gi], z11 = Grid[(gj + 1) * G + gi + 1];
			const float Lo = TNumericLimits<float>::Lowest();
			if (z00 == Lo || z10 == Lo || z01 == Lo || z11 == Lo) continue;
			const float Z = FMath::Lerp(FMath::Lerp(z00, z10, fx), FMath::Lerp(z01, z11, fx), fy);
			T.Add(FTransform(FRotator(0, Ry, 0), FVector(X, Y, Z - 6.f), FVector(S, S, S * Rh)));
		}
	if (T.Num()) K->AddInstances(T, false, true);
	FarTufts += T.Num();
}
