// Our own land along the roads (LandCorridor.h).
#include "LandCorridor.h"
#include "ADPLog.h"
#include "ProceduralMeshComponent.h"
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
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

const TArray<TArray<FVector2D>>& ALandCorridor::GlobeCut() { return Load().Cut; }

void ALandCorridor::AddRoadSample(double X, double Y, double Z, double HalfWidth) { RoadSamples.Add(FVector4(X, Y, Z, HalfWidth)); }

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
						const double D = FMath::Sqrt(FMath::Square(S.X - X) + FMath::Square(S.Y - Y)) - S.W;   // (m beyond its edge)
						if (D < Best) { Best = D; RZ = S.Z; HW = S.W; }
					}
				}
				const double Under = RZ - 0.08;
				if (Best <= 0.6) V = float(Under);
				else if (Best < 6.6) { const double t = FMath::SmoothStep(0.6, 6.6, Best); V = float(FMath::Lerp(Under, double(V), t)); }
			}
	}
	// ---- the mesh: a section per block of 128 x 128 cells, two triangles a cell; a skirt 4 m down under every outer edge ----
	Mesh = NewObject<UProceduralMeshComponent>(this, TEXT("LandCorridor"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->RegisterComponent();
	AddInstanceComponent(Mesh);
	UMaterialInstanceDynamic* Mat = nullptr;
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terminal/Masters/M_TSurface_DS.M_TSurface_DS")   /* (two-sided: the skirt shows from either side) */))
	{
		Mat = UMaterialInstanceDynamic::Create(Base, this);
		if (UTexture2D* Tex = LoadPng(DataFile(TEXT("land-corridor.png")))) Mat->SetTextureParameterValue(TEXT("Base"), Tex);
		else ADP_WARN(TEXT("Land corridor: land-corridor.png missing - plain colour"));
		Mat->SetVectorParameterValue(TEXT("BC"), FLinearColor::White);
		Mat->SetVectorParameterValue(TEXT("OS"), FLinearColor(0, 0, 1, 1));
		Mat->SetScalarParameterValue(TEXT("R"), 0.95f); Mat->SetScalarParameterValue(TEXT("M"), 0.f);
	}
	auto Ok = [&](int32 i, int32 j) { return i >= 0 && j >= 0 && i < B.NX && j < B.NY && !FMath::IsNaN(Z[j * B.NX + i]); };
	auto CellOk = [&](int32 i, int32 j) { return Ok(i, j) && Ok(i + 1, j) && Ok(i, j + 1) && Ok(i + 1, j + 1); };
	auto P = [&](int32 i, int32 j) { return FVector((B.X0 + i * B.Step) * 100.0, -(B.Y0 + j * B.Step) * 100.0, Z[j * B.NX + i] * 100.0); };
	auto Nrm = [&](int32 i, int32 j)
	{
		const int32 i0 = Ok(i - 1, j) ? i - 1 : i, i1 = Ok(i + 1, j) ? i + 1 : i, j0 = Ok(i, j - 1) ? j - 1 : j, j1 = Ok(i, j + 1) ? j + 1 : j;
		const FVector Ax = P(i1, j) - P(i0, j), Ay = P(i, j1) - P(i, j0);
		FVector N = FVector::CrossProduct(Ax, Ay).GetSafeNormal(); if (N.Z < 0) N = -N; return N.IsNearlyZero() ? FVector::UpVector : N;
	};
	auto UV = [&](int32 i, int32 j) { return FVector2D((i + 0.5) / B.NX, 1.0 - (j + 0.5) / B.NY); };
	const int32 Blk = 128; int32 Sct = 0, Tris = 0;
	for (int32 bj = 0; bj < B.NY - 1; bj += Blk)
		for (int32 bi = 0; bi < B.NX - 1; bi += Blk)
		{
			TArray<FVector> V, N; TArray<FVector2D> T; TArray<int32> I; TMap<int32, int32> Idx;
			auto Vtx = [&](int32 i, int32 j) { const int32 K = j * B.NX + i; if (const int32* F = Idx.Find(K)) return *F; const int32 n = V.Add(P(i, j)); N.Add(Nrm(i, j)); T.Add(UV(i, j)); Idx.Add(K, n); return n; };
			auto Down = [&](int32 i, int32 j) { const int32 n = V.Add(P(i, j) - FVector(0, 0, 400.0)); N.Add(Nrm(i, j)); T.Add(UV(i, j)); return n; };
			for (int32 j = bj; j < FMath::Min(bj + Blk, B.NY - 1); ++j)
				for (int32 i = bi; i < FMath::Min(bi + Blk, B.NX - 1); ++i)
				{
					if (!CellOk(i, j)) continue;
					const int32 a = Vtx(i, j), b = Vtx(i + 1, j), c = Vtx(i, j + 1), d = Vtx(i + 1, j + 1);
					I.Append({a, b, d, a, d, c});
					// the skirt: under each side of the cell with no cell beyond it
					auto Skirt = [&](int32 i0, int32 j0, int32 i1, int32 j1) { const int32 p = Vtx(i0, j0), q = Vtx(i1, j1), pd = Down(i0, j0), qd = Down(i1, j1); I.Append({p, q, qd, p, qd, pd}); };
					if (!CellOk(i, j - 1)) Skirt(i, j, i + 1, j);
					if (!CellOk(i, j + 1)) Skirt(i + 1, j + 1, i, j + 1);
					if (!CellOk(i - 1, j)) Skirt(i, j + 1, i, j);
					if (!CellOk(i + 1, j)) Skirt(i + 1, j, i + 1, j + 1);
				}
			if (I.Num() == 0) continue;
			// (wound to face up / out: as the road paint - a triangle shows from the side its corners run clockwise on)
			for (int32 t = 0; t + 2 < I.Num(); t += 3)
			{
				const FVector Fn = FVector::CrossProduct(V[I[t + 1]] - V[I[t]], V[I[t + 2]] - V[I[t]]);
				if (FMath::Abs(Fn.Z) > 1e-3 ? Fn.Z > 0 : false) Swap(I[t + 1], I[t + 2]);
			}
			Mesh->CreateMeshSection_LinearColor(Sct, V, I, N, T, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
			if (Mat) Mesh->SetMaterial(Sct, Mat);
			++Sct; Tris += I.Num() / 3;
		}
	ADP_LOG(TEXT("Land corridor: %d triangles in %d sections, shaped under %d road samples, in %.2f s"), Tris, Sct, RoadSamples.Num(), FPlatformTime::Seconds() - T0);
	return Tris;
}
