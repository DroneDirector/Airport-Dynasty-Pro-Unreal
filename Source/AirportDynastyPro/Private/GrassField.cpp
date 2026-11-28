// The grass round the airport (see GrassField.h).
#include "GrassField.h"
#if PLATFORM_WINDOWS
#include "GeoFrame.h"
#include "Site.h"
#include "Windows/WindowsHWrapper.h"   // (Windows' headers the Unreal way, before Cesium's: its own include of windows.h then leaves no min/max/GetObject macros behind for the next files of the build chunk)
#endif
#include "Engine/GameViewportClient.h"
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
#include "RoadNetwork.h"
#include "PerfLog.h"
#include "Components/SkeletalMeshComponent.h"
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
		auto File = [](const TCHAR* N) { return ADPSite::File(N); };   // (the site's own: Site.h)
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

	/** The site's car parks and roads (roads.json): at a site without a grass map the globe's ground lies within a couple of
	 *  centimetres of the tarmac and is often met first by the look down - grass stood in the car park's lanes and on the road
	 *  behind it (2026-11-23). Plan metres; the roads as their points, as wide as their lanes. */
	struct FTarmac
	{
		struct FSeg { FVector2D A, B; double L, R; };
		TArray<FBox2D> Lots; TArray<FSeg> Segs; FBox2D All = FBox2D(ForceInit); bool bRead = false;
		bool On(double X, double Y) const
		{
			const FVector2D P(X, Y);
			for (const FBox2D& B : Lots) if (B.IsInside(P)) return true;
			if (!All.IsInside(P)) return false;
			for (const FSeg& S : Segs)
			{
				const FVector2D D = S.B - S.A; const double Len = D.Size(); if (Len < 0.01) continue;
				const FVector2D U = D / Len, Q = P - S.A; const double T = FVector2D::DotProduct(Q, U);
				if (T < -2.0 || T > Len + 2.0) continue;
				const double Off = U.X * Q.Y - U.Y * Q.X;   // (+ to the left)
				if (Off < S.L && Off > -S.R) return true;
			}
			return false;
		}
	};
	const FTarmac& Tarmac()
	{
		static FTarmac T;
		if (T.bRead) return T;
		T.bRead = true;
		FString Text; TSharedPtr<FJsonObject> J;
		if (!FFileHelper::LoadFileToString(Text, *ADPSite::File(TEXT("roads.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()) return T;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (J->TryGetArrayField(TEXT("lots"), A))
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject(); bool bPaved = true; O->TryGetBoolField(TEXT("paved"), bPaved);
				if (!bPaved || O->HasField(TEXT("frame"))) continue;
				T.Lots.Add(FBox2D(FVector2D(O->GetNumberField(TEXT("x0")), O->GetNumberField(TEXT("y0"))), FVector2D(O->GetNumberField(TEXT("x1")), O->GetNumberField(TEXT("y1")))).ExpandBy(0.3));
			}
		if (J->TryGetArrayField(TEXT("roads"), A))
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject(); const TSharedPtr<FJsonObject>* Ln = nullptr; double Wd[2] = {0, 0};
				if (!O->TryGetObjectField(TEXT("lanes"), Ln)) continue;
				for (int32 s = 0; s < 2; ++s)
				{
					const TArray<TSharedPtr<FJsonValue>>* La = nullptr;
					if (!(*Ln)->TryGetArrayField(s ? TEXT("right") : TEXT("left"), La)) continue;
					for (const TSharedPtr<FJsonValue>& L : *La)
					{
						// (a lane of a changing width counts as its narrowest, and a grass island ends the tarmac: grass is right past it)
						const TSharedPtr<FJsonObject> Lo = L->AsObject(); if (Lo->GetStringField(TEXT("type")) == TEXT("Island")) break;
						double W = 0; const TArray<TSharedPtr<FJsonValue>>* Wa = nullptr;
						if (Lo->TryGetArrayField(TEXT("w"), Wa)) { W = 1e9; for (const TSharedPtr<FJsonValue>& P : *Wa) W = FMath::Min(W, P->AsArray()[1]->AsNumber()); }
						else Lo->TryGetNumberField(TEXT("w"), W);
						Wd[s] += W;
					}
				}
				const TArray<TSharedPtr<FJsonValue>>& P = O->GetArrayField(TEXT("pts"));
				for (int32 i = 0; i + 1 < P.Num(); ++i)
				{
					FTarmac::FSeg S{FVector2D(P[i]->AsArray()[0]->AsNumber(), P[i]->AsArray()[1]->AsNumber()), FVector2D(P[i + 1]->AsArray()[0]->AsNumber(), P[i + 1]->AsArray()[1]->AsNumber()), Wd[0], Wd[1]};
					T.Segs.Add(S); T.All += S.A; T.All += S.B;
				}
			}
		if (T.All.bIsValid) T.All = T.All.ExpandBy(30.0);
		return T;
	}

	/** The forecourt's low lawn (owner, 2026-11-28: "add grass on the flat grass texture, but not super high grass, lower grass
	 *  than elsewhere ... only put it on the grass texture"). From roads.json, plan metres. Kind 2: on the flat grass itself -
	 *  a road's grass island (its full-width stretch, 30 cm in from its kerbs, clear of the zebra walkways over it) or a
	 *  roundabout's grassed middle; kind 1: the lawns and pads within 12 m of those roads' outer edges (the airfield's or the
	 *  land's grass there, as the normal grass's look down decides); 0: elsewhere - the normal grass. */
	struct FLawn
	{
		struct FLane { FString Type; TArray<FVector2D> W; };   // (W: [metres along, width]; one key with X < 0: the same all along)
		struct FRoad
		{
			TArray<FVector2D> P; TArray<double> S; bool bClosed = false; FVector2D Cn = FVector2D::ZeroVector; double Rad = 0, DiscIn = 0, DiscOut = 0;
			TArray<FLane> Lanes[2];            // 0 left, 1 right
			int32 Isl = INDEX_NONE; int32 IslSide = 1; double Full0 = 0, Full1 = -1;   // the island's lane and its full-width stretch
			TArray<FVector4> Cross;            // at, w, o0, o1
			FBox2D Box = FBox2D(ForceInit);
		};
		TArray<FRoad> Roads; bool bRead = false;
		static double W(const FLane& L, double D)
		{
			if (L.W.Num() == 0) return 0;
			if (L.W.Num() == 1 || D <= L.W[0].X) return L.W[0].Y;
			for (int32 k = 0; k + 1 < L.W.Num(); ++k)
				if (D <= L.W[k + 1].X) { const double T = (D - L.W[k].X) / FMath::Max(1e-3, L.W[k + 1].X - L.W[k].X); return FMath::Lerp(L.W[k].Y, L.W[k + 1].Y, T); }
			return L.W.Last().Y;
		}
		static double Sum(const FRoad& R, int32 Side, int32 Upto, double D) { double A = 0; for (int32 i = 0; i < FMath::Min(Upto, R.Lanes[Side].Num()); ++i) A += W(R.Lanes[Side][i], D); return A; }
		/** Along an open road: the nearest point's distance along it and its offset (+ right). */
		static bool Project(const FRoad& R, const FVector2D& Q, double& D, double& O)
		{
			double Best = 1e18; bool bGot = false;
			for (int32 i = 0; i + 1 < R.P.Num(); ++i)
			{
				const FVector2D A = R.P[i], V = R.P[i + 1] - A; const double Len = V.Size(); if (Len < 0.01) continue;
				const FVector2D U = V / Len, Pq = Q - A; const double T = FVector2D::DotProduct(Pq, U);
				if (T < -0.5 || T > Len + 0.5) continue;
				const double Off = U.X * Pq.Y - U.Y * Pq.X;
				if (FMath::Abs(Off) < Best) { Best = FMath::Abs(Off); D = R.S[i] + T; O = -Off; bGot = true; }
			}
			return bGot;
		}
		int32 Kind(double X, double Y) const
		{
			const FVector2D Q(X, Y); int32 K = 0;
			for (const FRoad& R : Roads)
			{
				if (!R.Box.IsInside(Q)) continue;
				if (R.bClosed)
				{
					const double Rq = FVector2D::Distance(Q, R.Cn);
					if (Rq < R.DiscIn) return 2;
					if (Rq < R.DiscOut) K = 1;
					continue;
				}
				double D = 0, O = 0; if (!Project(R, Q, D, O)) continue;
				if (O < -(Sum(R, 0, 99, D) + 12.0) || O > Sum(R, 1, 99, D) + 12.0) continue;
				K = 1;
				if (R.Isl == INDEX_NONE || D < R.Full0 || D > R.Full1) continue;
				const double In = Sum(R, R.IslSide, R.Isl, D), Out = In + W(R.Lanes[R.IslSide][R.Isl], D), Os = R.IslSide ? O : -O;
				if (Os < In + 0.3 || Os > Out - 0.3) continue;
				bool bWalk = false;
				for (const FVector4& C : R.Cross)
					bWalk |= FMath::Abs(D - C.X) < C.Y / 2 + 0.4 && FMath::Min(C.Z, C.W) <= Out + 0.5 && FMath::Max(C.Z, C.W) >= In - 0.5;
				if (!bWalk) return 2;
			}
			return K;
		}
	};
	const FLawn& Lawn()
	{
		static FLawn L;
		if (L.bRead) return L;
		L.bRead = true;
		FString Text; TSharedPtr<FJsonObject> J; const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!FFileHelper::LoadFileToString(Text, *ADPSite::File(TEXT("roads.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid() || !J->TryGetArrayField(TEXT("roads"), A)) return L;
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject(); FString Id; O->TryGetStringField(TEXT("id"), Id);
			if (Id != TEXT("AIRPORT-FORECOURT") && Id != TEXT("AIRPORT-ROUNDABOUT")) continue;
			const TSharedPtr<FJsonObject>* Ln = nullptr; if (!O->TryGetObjectField(TEXT("lanes"), Ln)) continue;
			FLawn::FRoad R; O->TryGetBoolField(TEXT("closed"), R.bClosed);
			for (const TSharedPtr<FJsonValue>& Pv : O->GetArrayField(TEXT("pts"))) R.P.Add(FVector2D(Pv->AsArray()[0]->AsNumber(), Pv->AsArray()[1]->AsNumber()));
			if (R.P.Num() < 2) continue;
			R.S.Add(0); for (int32 i = 1; i < R.P.Num(); ++i) R.S.Add(R.S.Last() + FVector2D::Distance(R.P[i - 1], R.P[i]));
			for (int32 s = 0; s < 2; ++s)
			{
				const TArray<TSharedPtr<FJsonValue>>* La = nullptr;
				if (!(*Ln)->TryGetArrayField(s ? TEXT("right") : TEXT("left"), La)) continue;
				for (const TSharedPtr<FJsonValue>& Lv : *La)
				{
					const TSharedPtr<FJsonObject> Lo = Lv->AsObject(); FLawn::FLane Lane; Lane.Type = Lo->GetStringField(TEXT("type"));
					const TArray<TSharedPtr<FJsonValue>>* Wa = nullptr;
					if (Lo->TryGetArrayField(TEXT("w"), Wa)) { for (const TSharedPtr<FJsonValue>& Kv : *Wa) Lane.W.Add(FVector2D(Kv->AsArray()[0]->AsNumber(), Kv->AsArray()[1]->AsNumber())); }
					else { double Wd = 0; Lo->TryGetNumberField(TEXT("w"), Wd); Lane.W.Add(FVector2D(-1, Wd)); }
					if (Lane.Type == TEXT("Island") && R.Isl == INDEX_NONE) { R.Isl = R.Lanes[s].Num(); R.IslSide = s; }
					R.Lanes[s].Add(Lane);
				}
			}
			if (R.Isl != INDEX_NONE)
			{
				// (only where it is at its full width, 3 m in from where it starts narrowing: no blade over a tapering kerb)
				const FLawn::FLane& I = R.Lanes[R.IslSide][R.Isl]; double Mx = 0; for (const FVector2D& K : I.W) Mx = FMath::Max(Mx, K.Y);
				R.Full0 = 1e18; R.Full1 = -1e18;
				for (const FVector2D& K : I.W) if (K.Y >= Mx - 0.01) { R.Full0 = FMath::Min(R.Full0, K.X); R.Full1 = FMath::Max(R.Full1, K.X); }
				if (I.W.Num() == 1) { R.Full0 = 0; R.Full1 = R.S.Last(); }
				R.Full0 += 3.0; R.Full1 -= 3.0;
			}
			const TArray<TSharedPtr<FJsonValue>>* CJ = nullptr;
			if (O->TryGetArrayField(TEXT("crossings"), CJ))
				for (const TSharedPtr<FJsonValue>& Cv : *CJ)
				{
					const TSharedPtr<FJsonObject> C = Cv->AsObject(); double At = 0, Wc = 3, O0 = 0, O1 = 0;
					C->TryGetNumberField(TEXT("at"), At); C->TryGetNumberField(TEXT("w"), Wc); C->TryGetNumberField(TEXT("o0"), O0); C->TryGetNumberField(TEXT("o1"), O1);
					R.Cross.Add(FVector4(At, Wc, O0, O1));
				}
			if (R.bClosed)
			{
				// a roundabout: its ring round its middle; the grass from its inner kerb in (island and disc), its lawns 12 m out
				const TSharedPtr<FJsonObject>* DJ = nullptr; if (!O->TryGetObjectField(TEXT("disc"), DJ)) continue;
				for (const FVector2D& Pq : R.P) R.Cn += Pq;
				R.Cn /= R.P.Num();
				for (const FVector2D& Pq : R.P) R.Rad += FVector2D::Distance(Pq, R.Cn);
				R.Rad /= R.P.Num();
				const int32 In = R.Isl != INDEX_NONE && R.IslSide == 0 ? R.Isl : R.Lanes[0].Num();
				R.DiscIn = R.Rad - FLawn::Sum(R, 0, In, 0) - 0.3; R.DiscOut = R.Rad + FLawn::Sum(R, 1, 99, 0) + 12.0;
				R.Box = FBox2D(R.Cn - FVector2D(R.DiscOut), R.Cn + FVector2D(R.DiscOut));
			}
			else
			{
				double Wm = 0;
				for (int32 s = 0; s < 2; ++s) for (const FLawn::FLane& La : R.Lanes[s]) { double M = 0; for (const FVector2D& K : La.W) M = FMath::Max(M, K.Y); Wm += M; }
				for (const FVector2D& Pq : R.P) R.Box += Pq;
				R.Box = R.Box.ExpandBy(Wm + 13.0);
			}
			L.Roads.Add(R);
		}
		ADP_LOG(TEXT("Grass: the low lawn along %d forecourt roads"), L.Roads.Num());
		return L;
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
		if (bGlobe && !ADPSite::IsDefault() && Tarmac().On(H.ImpactPoint.X / 100.0, -H.ImpactPoint.Y / 100.0)) return false;
		if ((bOurLand || bGlobe) && GrassArea().At(H.ImpactPoint.X / 100.0, -H.ImpactPoint.Y / 100.0)) return true;
		if (bOurLand) return false;
		// the real ground (the globe's terrain): grass where the land cover says the land is grassy
		if (bGlobe)
			if (ACesiumGeoreference* G = GrassGlobeGeo(C->GetWorld()))
			{
				const FVector LL = ADPGeo::ToEarth(G, H.ImpactPoint);
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
	if (Lawn().Kind(X / 100.0, -Y / 100.0)) return false;   // (the low lawn's ground: LawnAt)
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(GrassLook), true, this);   // complex: the globe's terrain has only its own shape to hit
	// (inside the airfield the terrain is cut away: the grass grows on the airfield's own grass there)
	if (!bAway && Field.IsInside(FVector2D(X, Y)))
		for (TActorIterator<ACesium3DTileset> It(GetWorld()); It; ++It) Q.AddIgnoredActor(*It);
	if (!GetWorld()->LineTraceSingleByChannel(H, FVector(X, Y, 30000.f), FVector(X, Y, -5000.f), ECC_Visibility, Q) || !IsGrass(H)) return false;
	Z = H.ImpactPoint.Z;
	return true;
}

bool AGrassField::LawnAt(float X, float Y, float& Z) const
{
	// the low lawn: on the flat grass by the roads (kind 2) the road network's own grass surface where it should be (within
	// 80 cm of the ground there), on the lawns beside them (kind 1) the grass the normal look down accepts; looked through
	// whatever stands 2.5 m or more over it (a roof's overhang, a canopy, a tree's crown), never under paving, a kerb, a bench
	const int32 K = Lawn().Kind(X / 100.0, -Y / 100.0);
	if (!K) return false;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(GrassLawn), true, this);
	if (!bAway && Field.IsInside(FVector2D(X, Y)))
		for (TActorIterator<ACesium3DTileset> It(GetWorld()); It; ++It) Q.AddIgnoredActor(*It);
	const double E = K == 2 ? ARoadNetwork::GroundAt(X / 100.0, -Y / 100.0) * 100.0 : 0.0;
	float Above = TNumericLimits<float>::Max();
	for (int32 n = 0; n < 5; ++n)
	{
		FHitResult H;
		if (!GetWorld()->LineTraceSingleByChannel(H, FVector(X, Y, 30000.f), FVector(X, Y, -5000.f), ECC_Visibility, Q) || !H.GetComponent()) return false;
		const bool bSurf = K == 2
			? H.ImpactNormal.Z >= 0.85f && !H.GetComponent()->IsA<USkeletalMeshComponent>() && FMath::Abs(H.ImpactPoint.Z - E) < 80.0
			: IsGrass(H);
		if (bSurf)
		{
			if (Above - H.ImpactPoint.Z < 250.f) return false;
			Z = H.ImpactPoint.Z;
			return true;
		}
		Above = FMath::Min(Above, float(H.ImpactPoint.Z));
		Q.AddIgnoredComponent(H.GetComponent());
	}
	return false;
}

void AGrassField::FillLawn(UHierarchicalInstancedStaticMeshComponent* K, const FIntPoint& C, const FVector& From, float Reach)
{
	// the same clumps as the near grass at about 40 % of its height and two thirds of its width, 20 a square metre spread
	// evenly (4 x 5 in every metre, a little jitter: a mown lawn), the same fading over the outer quarter of the reach; on the
	// lawns beside the roads only where the metre round it is lawn too (no blade over the paving's edge), on the islands
	// placed one by one inside the island's kerbs
	ADP_PERF_SCOPE("Grass: low lawn", 2.f);
	const FPatch& P = Patch(C);
	const int32 N = int32(Cell / 100.f);
	const float Lo = TNumericLimits<float>::Lowest();
	if (P.L.Num() != N * N) return;
	auto IsL = [&](int32 i, int32 j) { return i < 0 || j < 0 || i >= N || j >= N || P.L[j * N + i] != Lo; };
	FRandomStream R(5 ^ (C.X * 73856093) ^ (C.Y * 19349663));
	TArray<FTransform> T;
	for (int32 j = 0; j < N; ++j)
		for (int32 i = 0; i < N; ++i)
		{
			const float Z = P.L[j * N + i];
			if (Z == Lo) continue;
			const bool bRound = IsL(i - 1, j) && IsL(i + 1, j) && IsL(i, j - 1) && IsL(i, j + 1);
			for (int32 sy = 0; sy < 5; ++sy)
				for (int32 sx = 0; sx < 4; ++sx)
				{
					const FVector At(C.X * Cell + (i + (sx + R.FRandRange(0.15f, 0.85f)) / 4.f) * 100.f, C.Y * Cell + (j + (sy + R.FRandRange(0.15f, 0.85f)) / 5.f) * 100.f, Z - 1.f);
					const int32 Kd = Lawn().Kind(At.X / 100.0, -At.Y / 100.0);
					if (Kd == 0 || (Kd == 1 && !bRound)) continue;
					float F = 1.f;
					if (Reach > 0.f)
					{
						const float D = FVector2D::Distance(FVector2D(At), FVector2D(From));
						F = FMath::Clamp((Reach - D) / (Reach * 0.25f), 0.f, 1.f);
						if (F <= 0.f || R.FRand() > F) continue;
						F = FMath::Lerp(0.55f, 1.f, F);
					}
					const float S = R.FRandRange(0.62f, 0.72f) * F;
					T.Add(FTransform(FRotator(0, R.FRandRange(0, 360), 0), At, FVector(S, S, R.FRandRange(0.38f, 0.45f) * F)));
				}
		}
	if (T.Num()) { K->AddInstances(T, false, true); LawnClumps += T.Num(); }
}

bool AGrassField::PatchStep(const FIntPoint& C, double Deadline)
{
	FPatch& P = Patches.FindOrAdd(C);
	const int32 N = int32(Cell / 100.f);
	if (P.H.Num() != N * N) { P.H.SetNumUninitialized(N * N); P.L.SetNumUninitialized(N * N); P.Rows = 0; }
	while (P.Rows < N && FPlatformTime::Seconds() < Deadline)
	{
		const int32 j = P.Rows++;
		for (int32 i = 0; i < N; ++i)
		{
			float Z;
			const float X = C.X * Cell + (i + 0.5f) * 100.f, Y = C.Y * Cell + (j + 0.5f) * 100.f;
			P.H[j * N + i] = GroundAt(X, Y, Z) ? Z : TNumericLimits<float>::Lowest();
			P.L[j * N + i] = LawnAt(X, Y, Z) ? Z : TNumericLimits<float>::Lowest();
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
	// nothing grown while the menus cover the world (it isn't drawn then): the start's loading and the title film stay
	// smooth (owner, 2026-11-15); it grows when you go into the 3D world
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->bDisableWorldRendering) return;
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
			if (!bGot)
			{
				if (!bLawnLogged && LawnClumps > 0) { bLawnLogged = true; ADP_LOG(TEXT("Grass: the low lawn round the camera - %d clumps"), LawnClumps); }
				break;
			}
			Job.From = L;
		}
		if (Job.Layer == 0)
		{
			if (!PatchStep(Job.C, Deadline)) break;
			UHierarchicalInstancedStaticMeshComponent* K = NewLayer(Near, 5000.f, Job.Reach * 0.85f, Job.Reach);
			FillCell(K, Job.C, 14.f, 0.85f, 1.2f, 1, Job.From, Job.Reach);
			FillLawn(K, Job.C, Job.From, Job.Reach);   // (the low lawn in the same pieces: the same mesh, culled with them)
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
