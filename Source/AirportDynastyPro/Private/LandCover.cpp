// The land cover at a point (LandCover.h).
#include "LandCover.h"
#include "TerrainPacks.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace ADPLandCover
{
	namespace
	{
		struct FTile { int32 W = 0, H = 0; TArray<uint8> Bgra; };
		TMap<FString, TSharedPtr<FTile>> Cache;

		// the colours MakeLandCover.py paints, by class
		struct FPaint { EClass C; uint8 R, G, B; };
		const FPaint Paints[] = {
			{Trees, 40, 66, 34}, {Shrub, 104, 100, 62}, {Grass, 118, 132, 72}, {Crops, 152, 146, 96}, {Built, 132, 128, 122},
			{Bare, 176, 164, 142}, {Snow, 236, 240, 245}, {Water, 28, 52, 78}, {Wetland, 84, 104, 74}, {Mangrove, 38, 72, 50}, {Moss, 122, 118, 86}};

		FString Root()
		{
			const FString D = ADPTerrain::Dir() / TEXT("landcover");
			if (FPaths::DirectoryExists(D)) return D;
			if (FPaths::DirectoryExists(TEXT("F:/ADP-Terrain/landcover"))) return TEXT("F:/ADP-Terrain/landcover");
			return FString();
		}

		const FTile* Load(const FString& File)
		{
			if (const TSharedPtr<FTile>* F = Cache.Find(File)) return F->Get();
			TSharedPtr<FTile> T = MakeShared<FTile>();
			TArray<uint8> Raw;
			if (FFileHelper::LoadFileToArray(Raw, *File))
			{
				IImageWrapperModule& M = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
				TSharedPtr<IImageWrapper> W = M.CreateImageWrapper(EImageFormat::JPEG);
				if (W.IsValid() && W->SetCompressed(Raw.GetData(), Raw.Num()) && W->GetRaw(ERGBFormat::BGRA, 8, T->Bgra))
				{ T->W = W->GetWidth(); T->H = W->GetHeight(); }
			}
			if (Cache.Num() > 256) Cache.Reset();   // a few hundred tiles at most (round where you are)
			Cache.Add(File, T);
			return T.Get();
		}

		EClass Nearest(uint8 R, uint8 G, uint8 B)
		{
			EClass Best = Unknown; int32 BestD = MAX_int32;
			for (const FPaint& P : Paints)
			{
				const int32 D = FMath::Square(int32(R) - P.R) + FMath::Square(int32(G) - P.G) + FMath::Square(int32(B) - P.B);
				if (D < BestD) { BestD = D; Best = P.C; }
			}
			return Best;
		}
	}

	EClass ClassAt(double Lon, double Lat)
	{
		const FString R = Root();
		if (R.IsEmpty()) return Unknown;
		// the North Atlantic's level 7 (4 x 2 root tiles over -28.125..-5.625, 56.25..67.5), else the world's level 8
		FString File; double U, V;
		if (Lon >= -28.125 && Lon < -5.625 && Lat >= 56.25 && Lat < 67.5)
		{
			const double Dx = 22.5 / (4 * 128), Dy = 11.25 / (2 * 128);
			const int32 X = FMath::FloorToInt((Lon + 28.125) / Dx), Y = FMath::FloorToInt((Lat - 56.25) / Dy);
			File = FString::Printf(TEXT("%s/region/7/%d/%d.jpg"), *R, X, Y);
			U = (Lon + 28.125) / Dx - X; V = (Lat - 56.25) / Dy - Y;
		}
		else
		{
			const double D = 180.0 / 256;
			const int32 X = FMath::FloorToInt((Lon + 180.0) / D), Y = FMath::FloorToInt((Lat + 90.0) / D);
			File = FString::Printf(TEXT("%s/world/8/%d/%d.jpg"), *R, X, Y);
			U = (Lon + 180.0) / D - X; V = (Lat + 90.0) / D - Y;
		}
		const FTile* T = Load(File);
		if (!T || !T->W) return Unknown;
		// the picture's rows run north to south
		const int32 Px = FMath::Clamp(int32(U * T->W), 0, T->W - 1), Py = FMath::Clamp(int32((1.0 - V) * T->H), 0, T->H - 1);
		const uint8* P = T->Bgra.GetData() + (Py * T->W + Px) * 4;
		return Nearest(P[2], P[1], P[0]);
	}

	bool Grows(EClass C) { return C == Grass || C == Crops || C == Shrub || C == Moss || C == Wetland || C == Trees; }
}
