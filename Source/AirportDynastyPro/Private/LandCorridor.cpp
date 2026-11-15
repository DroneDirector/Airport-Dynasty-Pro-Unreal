// Our own land along the roads (LandCorridor.h).
#include "LandCorridor.h"
#include "TerminalBuilder.h"
// (Windows' own UpdateResource macro turns UTexture2D::UpdateResource into UpdateResourceW when a Windows header comes
// in before this file in the packaged build's unity files - 2026-11-15)
#ifdef UpdateResource
#undef UpdateResource
#endif
#include "ADPLog.h"
#include "Airfield.h"
#include "Globe.h"
#include "GrassField.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
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
	struct FBand
	{
		bool bTried = false, bOk = false;
		double X0 = 0, Y0 = 0, Step = 2.5; int32 NX = 0, NY = 0;
		TArray<float> Z;                              // natural ground, NaN off the band (row j = y0 + j * step)
		TArray<TArray<FVector2D>> Cut;
		FString Dir;
		float At(int32 i, int32 j) const { return Z[j * NX + i]; }
	};
	FBand Band;
	TArray<FVector4> RoadSamples;                     // x, y, z (plan m), half width

	FString DataFile(const TCHAR* Name)
	{
		const FString A = FPaths::ProjectContentDir() / TEXT("Data") / Name;
		return FPaths::FileExists(A) ? A : FPaths::ProjectDir() / TEXT("Data") / Name;
	}

	const FBand& Load()
	{
		if (Band.bTried) return Band;
		Band.bTried = true;
		FString Text; TSharedPtr<FJsonObject> R;
		if (!FFileHelper::LoadFileToString(Text, *DataFile(TEXT("land-corridor.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), R) || !R.IsValid()) return Band;
		Band.X0 = R->GetNumberField(TEXT("x0")); Band.Y0 = R->GetNumberField(TEXT("y0")); Band.Step = R->GetNumberField(TEXT("step"));
		Band.NX = int32(R->GetNumberField(TEXT("nx"))); Band.NY = int32(R->GetNumberField(TEXT("ny")));
		for (const TSharedPtr<FJsonValue>& P : R->GetArrayField(TEXT("cut")))
		{
			TArray<FVector2D>& Ring = Band.Cut.AddDefaulted_GetRef();
			for (const TSharedPtr<FJsonValue>& C : P->AsArray()) Ring.Add(FVector2D(C->AsArray()[0]->AsNumber(), C->AsArray()[1]->AsNumber()));
		}
		TArray<uint8> Bin;
		if (!FFileHelper::LoadFileToArray(Bin, *DataFile(TEXT("land-corridor.bin"))) || Bin.Num() != Band.NX * Band.NY * 4)
		{
			ADP_ERROR(TEXT("Land corridor: land-corridor.bin missing or the wrong size (run Tools/make_land_corridor.py)")); return Band;
		}
		Band.Z.SetNumUninitialized(Band.NX * Band.NY);
		FMemory::Memcpy(Band.Z.GetData(), Bin.GetData(), Bin.Num());
		Band.bOk = true;
		return Band;
	}

	UTexture2D* LoadPng(const FString& File)
	{
		TArray<uint8> Png;
		if (!FFileHelper::LoadFileToArray(Png, *File)) return nullptr;
		IImageWrapperModule& M = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> W = M.CreateImageWrapper(EImageFormat::PNG);
		TArray<uint8> Bgra;
		if (!W.IsValid() || !W->SetCompressed(Png.GetData(), Png.Num()) || !W->GetRaw(ERGBFormat::BGRA, 8, Bgra)) return nullptr;
		UTexture2D* T = UTexture2D::CreateTransient(W->GetWidth(), W->GetHeight(), PF_B8G8R8A8, TEXT("LandCorridorColours"));
		if (!T) return nullptr;
		T->SRGB = true; T->Filter = TF_Trilinear; T->AddressX = T->AddressY = TA_Clamp;
		void* Dst = T->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Dst, Bgra.GetData(), Bgra.Num());
		T->GetPlatformData()->Mips[0].BulkData.Unlock();
		T->UpdateResource();
		return T;
	}
}

ALandCorridor::ALandCorridor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

const TArray<TArray<FVector2D>>& ALandCorridor::GlobeCut() { return Load().Cut; }

void ALandCorridor::ShowColours()
{
	// the same picture as the globe round it (Settings > Imagery: AGlobe::ShownSource): land-corridor-<source>.png
	if (!Mat) return;
	const int32 S = FMath::Clamp(AGlobe::ShownSource, 0, 3);
	if (S == Shown) return;
	TObjectPtr<UTexture2D>& Tex = Colours.FindOrAdd(S);
	if (!Tex) Tex = LoadPng(DataFile(*FString::Printf(TEXT("land-corridor-%d.png"), S)));
	if (!Tex) { ADP_WARN(TEXT("Land corridor: land-corridor-%d.png missing"), S); return; }
	Mat->SetTextureParameterValue(TEXT("Base"), Tex);
	Shown = S;
}

void ALandCorridor::Tick(float Dt)
{
	Super::Tick(Dt);
	ShowColours();
	if (Plants) Plants->SetVisibility(AGrassField::bOn);
}

void ALandCorridor::PlantRim(const void*)
{
	// along the band's rim: clumps of grass and tufts (the airfield's own grass clumps) in a strip from 2 m outside the
	// edge to 14 m inside it, thickest at the edge - where an edge could show, near the camera, it is broken up
	UStaticMesh* Tuft = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Materials/Grass/SM_GrassFar.SM_GrassFar"));
	if (!Tuft || Rim.Num() == 0) return;
	Plants = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, TEXT("RimPlants"));
	Plants->SetupAttachment(RootComponent); Plants->SetStaticMesh(Tuft);
	Plants->SetCollisionEnabled(ECollisionEnabled::NoCollision); Plants->SetCastShadow(false);
	Plants->SetCullDistances(0, 60000);
	Plants->RegisterComponent(); AddInstanceComponent(Plants);
	FRandomStream R(20261114);
	TArray<FTransform> Xf;
	for (const TPair<FVector, FVector>& E : Rim)
	{
		FVector In = (E.Value - E.Key); In.Z = 0; In = In.GetSafeNormal();
		for (int32 k = 0; k < 6; ++k)
		{
			const double D = FMath::Lerp(-2.0, 14.0, FMath::Pow(R.FRand(), 1.8)) * 100.0;   // (most at the edge)
			const FVector Side = FVector(-In.Y, In.X, 0) * R.FRandRange(-125.0, 125.0);
			FVector At = E.Key + In * D + Side;
			double Zg; const double PX = At.X / 100.0, PY = -At.Y / 100.0;
			if (!HeightAt(PX, PY, Zg)) continue;
			At.Z = Zg * 100.0 - 3.0;
			const float Sc = R.FRandRange(0.7f, 1.6f);
			Xf.Add(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), At, FVector(Sc, Sc, Sc * R.FRandRange(0.8f, 1.3f))));
		}
	}
	Plants->AddInstances(Xf, false, true);
	ADP_LOG(TEXT("Land corridor: %d grass clumps along its rim"), Xf.Num());
}

void ALandCorridor::AddRoadSample(double X, double Y, double Z, double HalfWidth, bool bKerbEdge) { RoadSamples.Add(FVector4(X, Y, Z, bKerbEdge ? -HalfWidth : HalfWidth)); }

bool ALandCorridor::HeightAt(double X, double Y, double& Z)
{
	const FBand& B = Load();
	if (!B.bOk) return false;
	const double fx = (X - B.X0) / B.Step, fy = (Y - B.Y0) / B.Step;
	const int32 i = FMath::FloorToInt(fx), j = FMath::FloorToInt(fy);
	if (i < 0 || j < 0 || i >= B.NX - 1 || j >= B.NY - 1) return false;
	const float a = B.At(i, j), b = B.At(i + 1, j), c = B.At(i, j + 1), d = B.At(i + 1, j + 1);
	if (FMath::IsNaN(a) || FMath::IsNaN(b) || FMath::IsNaN(c) || FMath::IsNaN(d)) return false;
	const double tx = fx - i, ty = fy - j;
	Z = FMath::Lerp(FMath::Lerp(a, b, tx), FMath::Lerp(c, d, tx), ty);
	return true;
}

int32 ALandCorridor::Build()
{
	const FBand& B = Load();
	if (!B.bOk) return 0;
	const double T0 = FPlatformTime::Seconds();
	// ---- the land shaped under the roads: a few cm below their surface across their width, easing back to the land
	// over 6 m beside them (a gentle cut or fill, as a road is built) - so no ground ever shows through a road ----
	TArray<float> Z = B.Z;
	{
		const double Cell = 20.0;
		TMultiMap<FIntPoint, int32> Grid;
		for (int32 k = 0; k < RoadSamples.Num(); ++k) Grid.Add(FIntPoint(FMath::FloorToInt(RoadSamples[k].X / Cell), FMath::FloorToInt(RoadSamples[k].Y / Cell)), k);
		TArray<int32> Near;
		for (int32 j = 0; j < B.NY; ++j)
			for (int32 i = 0; i < B.NX; ++i)
			{
				float& V = Z[j * B.NX + i];
				if (FMath::IsNaN(V)) continue;
				const double X = B.X0 + i * B.Step, Y = B.Y0 + j * B.Step;
				const FIntPoint C(FMath::FloorToInt(X / Cell), FMath::FloorToInt(Y / Cell));
				double Best = 1e9, RZ = 0, HW = 0;
				for (int32 dx = -1; dx <= 1; ++dx) for (int32 dy = -1; dy <= 1; ++dy)
				{
					Near.Reset(); Grid.MultiFind(C + FIntPoint(dx, dy), Near);
					for (const int32 k : Near)
					{
						const FVector4& S = RoadSamples[k];
						const double D = FMath::Sqrt(FMath::Square(S.X - X) + FMath::Square(S.Y - Y)) - FMath::Abs(S.W);   // (m beyond its edge)
						if (D < Best) { Best = D; RZ = S.Z; HW = S.W; }
					}
				}
				// (owner, 2026-11-15: "the roads float above the custom ground - as far down as possible without glitching"):
				// under the road 12 cm below its surface (its curve sags a few cm between its height points on a rise: the
				// land never shows through); right beside it the land comes up to its edge - behind a raised pavement 8 cm
				// over the road (the pavement's top is 15 cm up: its back edge meets the grass, no slab side shows), beside
				// a flush edge 3 cm under it - then eases back to the land over 6 m. The land's points are 2.5 m apart: a
				// triangle reaching from under the road to its side stays below the edge it crosses.
				const bool bKerb = HW < 0;
				const double Under = RZ - 0.12, Side = bKerb ? RZ + 0.08 : RZ - 0.03;
				if (Best <= 0.0) V = float(Under);
				else if (Best <= 1.5) V = float(Side);
				else if (Best < 7.5) { const double t = FMath::SmoothStep(1.5, 7.5, Best); V = float(FMath::Lerp(Side, double(V), t)); }
			}
	}
	// ---- where it runs into the airfield: its land eased under the airfield's own ground (10 cm below it, over 40 m
	// inside the boundary), so the two never fight and the airfield's ground shows ----
	{
		const FAirfieldData::FRect& FB = AAirfield::Shared().Boundary;
		for (int32 j = 0; j < B.NY; ++j)
			for (int32 i = 0; i < B.NX; ++i)
			{
				float& V = Z[j * B.NX + i];
				if (FMath::IsNaN(V)) continue;
				const double X = B.X0 + i * B.Step, Y = B.Y0 + j * B.Step;
				const double In = FMath::Min(FMath::Min(X - FB.X0, FB.X1 - X), FMath::Min(Y - FB.Y0, FB.Y1 - Y));
				if (In > 0.0) V = float(FMath::Lerp(double(V), AAirfield::GroundAt(X, Y) - 0.10, FMath::SmoothStep(0.0, 40.0, In)));
			}
	}
	// ---- the mesh: two triangles a cell, one on a cell at the edge with three of its corners on the band (the outline
	// as fine as the grid's diagonals, no staircase); a skirt 4 m down under every edge that is the band's outer rim ----
	Mesh = NewObject<UProceduralMeshComponent>(this, TEXT("LandCorridor"));   // (unused when the Nanite land is there)
	Mesh->SetupAttachment(RootComponent);
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->RegisterComponent();
	AddInstanceComponent(Mesh);
	UMaterialInterface* Ground = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Land/M_LandCorridor.M_LandCorridor"));   // (its own: no sky sheen - Tools/set_land_corridor.py)
	if (!Ground) Ground = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terminal/Masters/M_TSurface_DS.M_TSurface_DS"));
	if (UMaterialInterface* Base = Ground)   // (two-sided: the skirt shows from either side)
	{
		Mat = UMaterialInstanceDynamic::Create(Base, this);
		// (toned to the globe's own look: Cesium draws the same picture a little darker and bluer - measured on both sides of
		// the rim in the game's pictures, 2026-11-14)
		Mat->SetVectorParameterValue(TEXT("BC"), FLinearColor::White);   // (its own material: no sky sheen, the picture as it is)
		Mat->SetVectorParameterValue(TEXT("OS"), FLinearColor(0, 0, 1, 1));
		Mat->SetScalarParameterValue(TEXT("R"), 0.95f); Mat->SetScalarParameterValue(TEXT("M"), 0.f);
		ShowColours();
	}
	auto Ok = [&](int32 i, int32 j) { return i >= 0 && j >= 0 && i < B.NX && j < B.NY && !FMath::IsNaN(Z[j * B.NX + i]); };
	auto P = [&](int32 i, int32 j) { return FVector((B.X0 + i * B.Step) * 100.0, -(B.Y0 + j * B.Step) * 100.0, Z[j * B.NX + i] * 100.0); };
	auto Nrm = [&](int32 i, int32 j)
	{
		const int32 i0 = Ok(i - 1, j) ? i - 1 : i, i1 = Ok(i + 1, j) ? i + 1 : i, j0 = Ok(i, j - 1) ? j - 1 : j, j1 = Ok(i, j + 1) ? j + 1 : j;
		const FVector Ax = P(i1, j) - P(i0, j), Ay = P(i, j1) - P(i, j0);
		FVector N = FVector::CrossProduct(Ax, Ay).GetSafeNormal(); if (N.Z < 0) N = -N; return N.IsNearlyZero() ? FVector::UpVector : N;
	};
	auto UV = [&](int32 i, int32 j) { return FVector2D((i + 0.5) / B.NX, 1.0 - (j + 0.5) / B.NY); };
	// every triangle by grid corners (K = j * NX + i), and how many triangles share each side
	struct FTri { int32 K[3]; int32 Blk; };
	TArray<FTri> Tris3;
	TMap<uint64, int32> Uses;
	const int32 Blk = 128, NBX = (B.NX + Blk - 1) / Blk;
	auto Side = [](int32 a, int32 b) { return a < b ? (uint64(a) << 32) | uint32(b) : (uint64(b) << 32) | uint32(a); };
	auto AddTri = [&](int32 a, int32 b, int32 c, int32 Bk) { Tris3.Add({{a, b, c}, Bk}); Uses.FindOrAdd(Side(a, b))++; Uses.FindOrAdd(Side(b, c))++; Uses.FindOrAdd(Side(c, a))++; };
	for (int32 j = 0; j < B.NY - 1; ++j)
		for (int32 i = 0; i < B.NX - 1; ++i)
		{
			const bool A = Ok(i, j), Bq = Ok(i + 1, j), C = Ok(i, j + 1), D = Ok(i + 1, j + 1);
			const int32 a = j * B.NX + i, b = a + 1, c = a + B.NX, d = c + 1, Bk = (j / Blk) * NBX + i / Blk;
			const int32 n = int32(A) + int32(Bq) + int32(C) + int32(D);
			if (n == 4) { AddTri(a, b, d, Bk); AddTri(a, d, c, Bk); }
			else if (n == 3) { if (!A) AddTri(b, d, c, Bk); else if (!Bq) AddTri(a, d, c, Bk); else if (!C) AddTri(a, b, d, Bk); else AddTri(a, b, c, Bk); }
		}
	// the rim (sides used once), for the skirt and the plants along it - not where the band runs into the airfield
	const FAirfieldData::FRect& FB = AAirfield::Shared().Boundary;
	auto OnField = [&](int32 K) { const double X = B.X0 + (K % B.NX) * B.Step, Y = B.Y0 + (K / B.NX) * B.Step; return FMath::Min(FMath::Min(X - FB.X0, FB.X1 - X), FMath::Min(Y - FB.Y0, FB.Y1 - Y)) > -5.0; };
	TMap<int32, TArray<int32>> ByBlk;
	for (int32 t = 0; t < Tris3.Num(); ++t) ByBlk.FindOrAdd(Tris3[t].Blk).Add(t);
	int32 Sct = 0, Tris = 0;
	Rim.Reset();
	// the band as Nanite meshes, made ahead in the editor (Tools/make_land_nanite.sh: this run with -DumpLand writes the
	// shaped land, Tools/land_nanite.py and set_land_corridor.py make /Game/Land/Corridor/LC_<block> from it) - when they
	// are there, they are drawn instead of the mesh built here
	const bool bDump = FParse::Param(FCommandLine::Get(), TEXT("DumpLand"));
	const bool bNanite = !bDump && FPackageName::DoesPackageExist(TEXT("/Game/Land/Corridor/LC_Ready"));
	TArray<uint8> DumpBuf;
	auto Put = [&](const void* D, int32 N) { DumpBuf.Append(static_cast<const uint8*>(D), N); };
	for (TPair<int32, TArray<int32>>& It : ByBlk)
	{
		TArray<FVector> V, N; TArray<FVector2D> T; TArray<int32> I; TMap<int32, int32> Idx;
		auto Vtx = [&](int32 K) { if (const int32* F = Idx.Find(K)) return *F; const int32 i = K % B.NX, j = K / B.NX; const int32 n = V.Add(P(i, j)); N.Add(Nrm(i, j)); T.Add(UV(i, j)); Idx.Add(K, n); return n; };
		auto Down = [&](int32 K) { const int32 i = K % B.NX, j = K / B.NX; const int32 n = V.Add(P(i, j) - FVector(0, 0, 400.0)); N.Add(Nrm(i, j)); T.Add(UV(i, j)); return n; };
		for (const int32 t : It.Value)
		{
			const FTri& Tr = Tris3[t];
			I.Append({Vtx(Tr.K[0]), Vtx(Tr.K[1]), Vtx(Tr.K[2])});
			for (int32 e = 0; e < 3; ++e)
			{
				const int32 p = Tr.K[e], q = Tr.K[(e + 1) % 3];
				if (Uses.FindRef(Side(p, q)) != 1 || (OnField(p) && OnField(q))) continue;
				// (the rim's skirt flares out 8 m as it goes 4 m down: where the globe lies lower it meets it as a gentle bank in
				// the same colours, not a step)
				const int32 o3 = Tr.K[(e + 2) % 3];
				FVector Out = (P(p % B.NX, p / B.NX) + P(q % B.NX, q / B.NX)) * 0.5 - P(o3 % B.NX, o3 / B.NX); Out.Z = 0; Out = Out.GetSafeNormal() * 800.0;
				auto Flare = [&](int32 K) { const int32 i = K % B.NX, j = K / B.NX; const int32 n = V.Add(P(i, j) + Out - FVector(0, 0, 400.0)); N.Add(Nrm(i, j)); T.Add(UV(i, j)); return n; };
				const int32 vp = Vtx(p), vq = Vtx(q), dp = Flare(p), dq = Flare(q);
				I.Append({vp, vq, dq, vp, dq, dp});
				const int32 o = Tr.K[(e + 2) % 3];   // (the triangle's third corner: inward from this side)
				Rim.Add(TPair<FVector, FVector>((P(p % B.NX, p / B.NX) + P(q % B.NX, q / B.NX)) * 0.5, P(o % B.NX, o / B.NX)));
			}
		}
		for (int32 t = 0; t + 2 < I.Num(); t += 3)
		{
			const FVector Fn = FVector::CrossProduct(V[I[t + 1]] - V[I[t]], V[I[t + 2]] - V[I[t]]);
			if (FMath::Abs(Fn.Z) > 1e-3 && Fn.Z > 0) Swap(I[t + 1], I[t + 2]);   // (wound to face up, as the road paint)
		}
		if (bDump)
		{
			const int32 H[3] = {It.Key, V.Num(), I.Num()}; Put(H, sizeof(H));
			for (const FVector& X : V) { const float F[3] = {float(X.X), float(X.Y), float(X.Z)}; Put(F, sizeof(F)); }
			for (const FVector& X : N) { const float F[3] = {float(X.X), float(X.Y), float(X.Z)}; Put(F, sizeof(F)); }
			for (const FVector2D& X : T) { const float F[2] = {float(X.X), float(X.Y)}; Put(F, sizeof(F)); }
			Put(I.GetData(), I.Num() * sizeof(int32));
		}
		if (bNanite)
		{
			const FString Name = FString::Printf(TEXT("LC_%03d"), It.Key);
			if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Land/Corridor/%s.%s"), *Name, *Name)))
			{
				UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, *Name);
				C->SetupAttachment(RootComponent); C->SetStaticMesh(SM); C->SetCollisionProfileName(TEXT("BlockAll"));
				// the airfield's own grass ground (owner, 2026-11-15: our land covered by the same grass; the blades on it:
				// GrassField.cpp)
				if (UMaterialInterface* Gm = ATerminalBuilder::KitMaterial(this, TEXT("ggrass"))) C->SetMaterial(0, Gm); else if (Mat) C->SetMaterial(0, Mat);
				C->RegisterComponent(); AddInstanceComponent(C);
			}
			else ADP_WARN(TEXT("Land corridor: %s missing - run Tools/make_land_nanite.sh"), *Name);
		}
		else
		{
			Mesh->CreateMeshSection_LinearColor(Sct, V, I, N, T, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
			if (UMaterialInterface* Gm = ATerminalBuilder::KitMaterial(this, TEXT("ggrass"))) Mesh->SetMaterial(Sct, Gm); else if (Mat) Mesh->SetMaterial(Sct, Mat);
		}
		++Sct; Tris += I.Num() / 3;
	}
	if (bDump)
	{
		const FString F = FPaths::ProjectSavedDir() / TEXT("land-sections.bin");
		FFileHelper::SaveArrayToFile(DumpBuf, *F);
		ADP_LOG(TEXT("Land corridor: shaped land written to %s (%d bytes)"), *F, DumpBuf.Num());
		FGenericPlatformMisc::RequestExit(false);
	}
	PlantRim(nullptr);
	ADP_LOG(TEXT("Land corridor: %d triangles in %d sections, shaped under %d road samples, in %.2f s"), Tris, Sct, RoadSamples.Num(), FPlatformTime::Seconds() - T0);
	return Tris;
}
