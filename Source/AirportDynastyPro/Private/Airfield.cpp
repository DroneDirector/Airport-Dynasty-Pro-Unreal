// The airfield: read from Data/airfield.json and laid out on its grade (Airfield.h), each surface a draped mesh a few
// millimetres over the one under it so they never flicker through each other: grass, then aprons, taxiways, runway,
// then the markings. Only what stands up (the cargo building) is still a slab.
#include "Airfield.h"
#include "ADPLog.h"
#include "TerminalBuilder.h"
#include "TerminalPlan.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#pragma region Loading

bool FAirfieldData::Load()
{
	FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/airfield.json"));
	if (!FPaths::FileExists(Path)) Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/airfield.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { ADP_ERROR(TEXT("Airfield not found: %s"), *Path); return false; }
	TSharedPtr<FJsonObject> R;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), R) || !R) { ADP_ERROR(TEXT("Airfield could not be read")); return false; }
	auto N = [](const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Def = 0) { double V = Def; O->TryGetNumberField(K, V); return float(V); };
	const TSharedPtr<FJsonObject> Rw = R->GetObjectField(TEXT("runway"));
	RwyX0 = N(Rw, TEXT("x0")); RwyX1 = N(Rw, TEXT("x1")); RwyY = N(Rw, TEXT("y")); RwyW = N(Rw, TEXT("w"), 60);
	const TSharedPtr<FJsonObject> Ys = R->GetObjectField(TEXT("ys"));
	AY = N(Ys, TEXT("AY")); BY = N(Ys, TEXT("BY")); TY = N(Ys, TEXT("TY")); KY = N(Ys, TEXT("KY"));
	const TSharedPtr<FJsonObject> Ap = R->GetObjectField(TEXT("airport"));
	Ap->TryGetNumberField(TEXT("lat"), Lat); Ap->TryGetNumberField(TEXT("lon"), Lon);
	ElevFt = N(Ap, TEXT("elevFt"), 130); Heading = N(Ap, TEXT("hdg"), 68);
	for (const TSharedPtr<FJsonValue>& V : R->GetArrayField(TEXT("taxiways")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FTwy T;
		T.Id = O->GetStringField(TEXT("id")); T.Kind = O->GetStringField(TEXT("kind")); T.W = N(O, TEXT("w"), 25);
		for (const TSharedPtr<FJsonValue>& P : O->GetArrayField(TEXT("pts")))
		{
			const TArray<TSharedPtr<FJsonValue>>& A = P->AsArray();
			T.Pts.Add(FVector2f(A[0]->AsNumber(), A[1]->AsNumber()));
		}
		Taxiways.Add(T);
	}
	for (const TSharedPtr<FJsonValue>& V : R->GetArrayField(TEXT("stands")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FStand S;
		S.Id = O->GetStringField(TEXT("id")); S.X = N(O, TEXT("x")); S.Nose = N(O, TEXT("nose")); S.Dir = N(O, TEXT("dir"), -1);
		S.Span = N(O, TEXT("span"), 36); S.Len = N(O, TEXT("len"), 44.5f);
		O->TryGetBoolField(TEXT("jetway"), S.bJetway); O->TryGetBoolField(TEXT("cargo"), S.bCargo);
		Stands.Add(S);
	}
	for (const TSharedPtr<FJsonValue>& V : R->GetArrayField(TEXT("holds")))
		Holds.Add(FVector2f(N(V->AsObject(), TEXT("x")), N(V->AsObject(), TEXT("y"))));
	for (const TSharedPtr<FJsonValue>& V : R->GetArrayField(TEXT("aprons")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FRect A{N(O, TEXT("x0")), N(O, TEXT("y0")), N(O, TEXT("x1")), N(O, TEXT("y1"))};
		O->TryGetStringField(TEXT("id"), A.Id);
		Aprons.Add(A);
	}
	const TArray<TSharedPtr<FJsonValue>>* Hs = nullptr;
	if (R->TryGetArrayField(TEXT("hangars"), Hs))
		for (const TSharedPtr<FJsonValue>& V : *Hs)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FHangarSite H;
			O->TryGetStringField(TEXT("id"), H.Id); O->TryGetStringField(TEXT("size"), H.Size);
			H.X = N(O, TEXT("x")); H.Y = N(O, TEXT("y")); O->TryGetBoolField(TEXT("open"), H.bOpen);
			Hangars.Add(H);
		}
	const TSharedPtr<FJsonObject> B = R->GetObjectField(TEXT("boundary")), C = R->GetObjectField(TEXT("cargoBuilding"));
	Boundary = {N(B, TEXT("x0")), N(B, TEXT("y0")), N(B, TEXT("x1")), N(B, TEXT("y1"))};
	Cargo = {N(C, TEXT("x0")), N(C, TEXT("y0")), N(C, TEXT("x1")), N(C, TEXT("y1"))};
	return true;
}

#pragma endregion

#pragma region The grade

namespace
{
	// the runway's lengthwise profile, metres above the aprons' level at seven stations along it (a smooth curve runs
	// through them): both ends low, a crest past the middle, as real runways roll (ICAO keeps such slopes under
	// 1.25%; these are about 0.3%, enough to see looking down the runway, nothing a truck or an aircraft notices)
	const float Profile[7] = {0.3f, 1.6f, 3.0f, 3.6f, 2.6f, 1.2f, 0.4f};
	const float CrownM = 0.22f;      // the crown: the centre line this much above the runway's edges
	const float GradeFull = 130.f;   // the grade in full within this of the runway's centre line ...
	const float GradeGone = 300.f;   // ... and gone (the level aprons and buildings) this far out
	const float EdgeFade = 160.f;    // how far inside the boundary the ground comes back down to the globe's
	float Smooth(float A, float B, float X) { return FMath::SmoothStep(A, B, X); }
	float ProfileAt(float T)
	{
		// Catmull-Rom through the stations, the ends held level beyond them
		const float U = FMath::Clamp(T, 0.f, 1.f) * 6.f;
		const int32 K = FMath::Clamp(int32(U), 0, 5);
		const float F = U - K;
		const float P0 = Profile[FMath::Max(K - 1, 0)], P1 = Profile[K], P2 = Profile[K + 1], P3 = Profile[FMath::Min(K + 2, 6)];
		return 0.5f * ((2.f * P1) + (-P0 + P2) * F + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * F * F + (-P0 + 3.f * P1 - 3.f * P2 + P3) * F * F * F);
	}
}

const FAirfieldData& AAirfield::Shared()
{
	static FAirfieldData S;
	static bool bLoaded = false;
	if (!bLoaded) { bLoaded = true; S.Load(); }
	return S;
}

float AAirfield::GroundAt(float X, float Y)
{
	const FAirfieldData& D = Shared();
	const float Dy = FMath::Abs(Y - D.RwyY), HalfW = D.RwyW / 2 + 7.5f;
	const float P = ProfileAt((X - D.RwyX0) / (D.RwyX1 - D.RwyX0));
	const float Crown = Dy < HalfW ? CrownM * (1.f - FMath::Square(Dy / HalfW)) : 0.f;
	const float Reach = 1.f - Smooth(GradeFull, GradeGone, Dy);
	const float InX = FMath::Min(Smooth(0.f, EdgeFade, X - D.Boundary.X0), Smooth(0.f, EdgeFade, D.Boundary.X1 - X));
	const float InY = FMath::Min(Smooth(0.f, EdgeFade, Y - D.Boundary.Y0), Smooth(0.f, EdgeFade, D.Boundary.Y1 - Y));
	return (P + Crown) * Reach * FMath::Clamp(InX, 0.f, 1.f) * FMath::Clamp(InY, 0.f, 1.f);
}

FVector2f AAirfield::SlopeAt(float X, float Y)
{
	return FVector2f((GroundAt(X + 2.f, Y) - GroundAt(X - 2.f, Y)) / 4.f, (GroundAt(X, Y + 2.f) - GroundAt(X, Y - 2.f)) / 4.f);
}

#pragma endregion

#pragma region Pieces

AAirfield::AAirfield()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UInstancedStaticMeshComponent* AAirfield::Kit(const FName Mat)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* K = Kits.Find(Mat)) return *K;
	UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this, Mat);
	K->SetupAttachment(Root);
	K->SetStaticMesh(Cube);
	K->SetMaterial(0, ATerminalBuilder::KitMaterial(this, Mat));
	K->SetCollisionProfileName(TEXT("BlockAll"));
	K->RegisterComponent();
	AddInstanceComponent(K);
	Kits.Add(Mat, K);
	return K;
}

void AAirfield::Slab(const FName Mat, float X0, float Y0, float X1, float Y1, float Bottom, float Top)
{
	const FVector A = FTerminalPlan::ToWorld(FMath::Min(X0, X1), Bottom + Sink, FMath::Min(Y0, Y1));
	const FVector B = FTerminalPlan::ToWorld(FMath::Max(X0, X1), Top + Sink, FMath::Max(Y0, Y1));
	const FVector S = (B - A).GetAbs();
	Kit(Mat)->AddInstance(FTransform(FRotator::ZeroRotator, (A + B) * 0.5f, S / 100.f));
}

void AAirfield::Strip(const FName Mat, const FVector2f& A, const FVector2f& B, float W, float Top, float Thick)
{
	const FVector WA = FTerminalPlan::ToWorld(A.X, Top + Sink - Thick / 2, A.Y), WB = FTerminalPlan::ToWorld(B.X, Top + Sink - Thick / 2, B.Y);
	const FVector Dir = WB - WA;
	if (Dir.IsNearlyZero()) return;
	Kit(Mat)->AddInstance(FTransform(Dir.Rotation(), (WA + WB) * 0.5f, FVector(Dir.Size() / 100.f, W, Thick)));
}

void AAirfield::Vertex(FSheet& S, float X, float Y, float Lift, const FLinearColor& Col, const FVector2D* UVOverride)
{
	// the corner on the grade; its normal from the slope (the world's y runs against the plan's); 10 m texture tiles
	const FVector2f Sl = SlopeAt(X, Y);
	S.V.Add(FTerminalPlan::ToWorld(X, GroundAt(X, Y) + Lift + Sink, Y));
	S.N.Add(FVector(-Sl.X, Sl.Y, 1.f).GetSafeNormal());
	S.UV.Add(UVOverride ? *UVOverride : FVector2D(X / 10.0, Y / 10.0));
	S.C.Add(Col);
}

void AAirfield::EdgeBand(const FVector2f& A, const FVector2f& B, const FVector2f& Out, float InnerLift, float OuterLift, float Asphalt, float End, bool bFadeA, bool bFadeB)
{
	FSheet& S = Sheets.FindOrAdd(TEXT("edge"));
	const FVector2f U = (B - A).GetSafeNormal(), N = Out.GetSafeNormal();
	const float L = (B - A).Size();
	if (L < 1.f) return;
	constexpr float In = 1.2f, Over = 1.4f, Step = 2.f, Fade = 6.f;
	const int32 Ns = FMath::Max(1, FMath::CeilToInt(L / Step));
	// across: the pavement side, the edge, the ground side (the picture runs 0 to 1 across, the edge at its middle)
	const float Across[3] = {-In, 0.f, Over}, Lifts[3] = {InnerLift + 0.004f, InnerLift + 0.004f, OuterLift + 0.004f}, Vs[3] = {0.f, 0.5f, 1.f};
	const int32 Base = S.V.Num();
	for (int32 j = 0; j < 3; ++j)
		for (int32 i = 0; i <= Ns; ++i)
		{
			const float Along = L * i / Ns;
			const FVector2f P = A + U * Along + N * Across[j];
			float Alpha = 1.f;
			if (bFadeA) Alpha = FMath::Min(Alpha, FMath::Clamp(Along / Fade, 0.f, 1.f));
			if (bFadeB) Alpha = FMath::Min(Alpha, FMath::Clamp((L - Along) / Fade, 0.f, 1.f));
			const FVector2D UV(Along / 4.0, Vs[j]);
			Vertex(S, P.X, P.Y, Lifts[j], FLinearColor(Asphalt, End, 0.f, Alpha), &UV);
		}
	for (int32 j = 0; j < 2; ++j)
		for (int32 i = 0; i < Ns; ++i)
		{
			const int32 Pa = Base + j * (Ns + 1) + i, Pb = Pa + 1, Pc = Pa + Ns + 1, Pd = Pc + 1;
			Tri(S, Pa, Pb, Pd); Tri(S, Pa, Pd, Pc);
		}
}

void AAirfield::EdgeRect(float X0, float Y0, float X1, float Y1, float InnerLift, float Asphalt, int32 Sides, float EndsAtX)
{
	const float Xa = FMath::Min(X0, X1), Xb = FMath::Max(X0, X1), Ya = FMath::Min(Y0, Y1), Yb = FMath::Max(Y0, Y1), Grass = -0.08f;
	// the short sides of a runway (EndsAtX > 0: those at x = Xa and x = Xb) get the bigger cracks
	if (Sides & 1) EdgeBand(FVector2f(Xa, Ya), FVector2f(Xb, Ya), FVector2f(0, -1), InnerLift, Grass, Asphalt, 0.f, false, false);
	if (Sides & 2) EdgeBand(FVector2f(Xa, Yb), FVector2f(Xb, Yb), FVector2f(0, 1), InnerLift, Grass, Asphalt, 0.f, false, false);
	if (Sides & 4) EdgeBand(FVector2f(Xa, Ya), FVector2f(Xa, Yb), FVector2f(-1, 0), InnerLift, Grass, Asphalt, EndsAtX > 0 ? 1.f : 0.f, false, false);
	if (Sides & 8) EdgeBand(FVector2f(Xb, Ya), FVector2f(Xb, Yb), FVector2f(1, 0), InnerLift, Grass, Asphalt, EndsAtX > 0 ? 1.f : 0.f, false, false);
}

void AAirfield::EdgeBands(bool bModelSite)
{
	const float RY = D.RwyY, HW = D.RwyW / 2 + 7.5f;   // the runway with its shoulders
	// the runway: asphalt-style all round, the ends with the bigger cracks
	EdgeRect(D.RwyX0 - 60, RY - HW, D.RwyX1 + 60, RY + HW, -0.025f, 1.f, 15, 1.f);
	// the aprons: concrete-style; the terminal's own apron only where it meets grass (its south side is the building)
	for (const FAirfieldData::FRect& A : D.Aprons)
	{
		if (bModelSite && (A.Id == TEXT("APRON-T") || A.Id == TEXT("APRON-9"))) continue;
		EdgeRect(A.X0, A.Y0, A.X1, A.Y1, -0.04f, 0.f, A.Id == TEXT("APRON-T") ? 14 : 15);
	}
	// the landside road and car park: asphalt-style on its open sides
	if (!bModelSite) EdgeRect(-160, -170, 392, -0.5f, -0.05f, 1.f, 13);
	// the taxiways: asphalt-style along both sides of each stretch (the shoulders are the taxiway's), cut where they
	// run into the runway's band or an apron, and the long taxiways cut round each link that joins them
	for (const FAirfieldData::FTwy& T : D.Taxiways)
	{
		if (T.Kind == TEXT("taxilane")) continue;
		const float Half = T.W / 2 + 17.5f;
		for (int32 i = 0; i + 1 < T.Pts.Num(); ++i)
		{
			FVector2f A = T.Pts[i], B = T.Pts[i + 1];
			const FVector2f U = (B - A).GetSafeNormal(), N(-U.Y, U.X);
			// clipped out of the runway's shoulders (a link ends on the runway's centre line)
			auto ClipRunway = [&](FVector2f& P, const FVector2f& Other)
			{
				const bool bIn = FMath::Abs(P.Y - RY) < HW + 1.5f;
				if (bIn && FMath::Abs(Other.Y - RY) >= HW + 1.5f) P.Y = Other.Y < RY ? RY - HW - 1.5f : RY + HW + 1.5f;
			};
			ClipRunway(A, B); ClipRunway(B, A);
			if ((B - A).Size() < 2.f) continue;
			// the long taxiways: gaps where each link joins
			TArray<float> Cuts;   // along-the-way stations of the gaps' middles
			if (FMath::Abs(U.X) > 0.9f)
				for (const FAirfieldData::FTwy& O : D.Taxiways)
					if (O.Kind == TEXT("link") && O.Pts.Num() >= 2 && FMath::Abs(O.Pts[0].X - O.Pts[1].X) < 1.f)
					{
						const float Ymin = FMath::Min(O.Pts[0].Y, O.Pts[1].Y), Ymax = FMath::Max(O.Pts[0].Y, O.Pts[1].Y);
						if (A.Y > Ymin - Half - 1.f && A.Y < Ymax + Half + 1.f) Cuts.Add((O.Pts[0].X - A.X) * U.X);
					}
			Cuts.Sort();
			for (const float Side : {-1.f, 1.f})
			{
				const FVector2f Off = N * (Side * Half);
				float From = 0.f;
				const float L = (B - A).Size(), GapHalf = 25.f / 2 + 17.5f + 1.4f;
				for (int32 c = 0; c <= Cuts.Num(); ++c)
				{
					const float To = c < Cuts.Num() ? Cuts[c] - GapHalf : L;
					if (To - From > 2.f) EdgeBand(A + Off + U * From, A + Off + U * To, N * Side, -0.035f, -0.08f, 1.f, 0.f, From > 0.f, c < Cuts.Num());
					From = c < Cuts.Num() ? Cuts[c] + GapHalf : L;
				}
			}
		}
	}
}

void AAirfield::Tri(FSheet& S, int32 A, int32 B, int32 C)
{
	// wound to face up (Unreal shows a triangle from the side its corners run clockwise on)
	const FVector N = FVector::CrossProduct(S.V[B] - S.V[A], S.V[C] - S.V[A]);
	if (N.Z < 0.f) { S.I.Add(A); S.I.Add(B); S.I.Add(C); } else { S.I.Add(A); S.I.Add(C); S.I.Add(B); }
}

void AAirfield::Quad(const FName Mat, float X0, float Y0, float X1, float Y1, float Lift, float Step)
{
	FSheet& S = Sheets.FindOrAdd(Mat);
	const float Xa = FMath::Min(X0, X1), Xb = FMath::Max(X0, X1), Ya = FMath::Min(Y0, Y1), Yb = FMath::Max(Y0, Y1);
	const int32 Nx = FMath::Max(1, FMath::CeilToInt((Xb - Xa) / Step)), Ny = FMath::Max(1, FMath::CeilToInt((Yb - Ya) / Step));
	const int32 Base = S.V.Num();
	for (int32 j = 0; j <= Ny; ++j)
		for (int32 i = 0; i <= Nx; ++i)
			Vertex(S, FMath::Lerp(Xa, Xb, float(i) / Nx), FMath::Lerp(Ya, Yb, float(j) / Ny), Lift);
	for (int32 j = 0; j < Ny; ++j)
		for (int32 i = 0; i < Nx; ++i)
		{
			const int32 A = Base + j * (Nx + 1) + i, B = A + 1, C = A + Nx + 1, Dd = C + 1;
			Tri(S, A, B, Dd); Tri(S, A, Dd, C);
		}
}

void AAirfield::Band(const FName Mat, const FVector2f& A, const FVector2f& B, float W, float Lift, float Step)
{
	FSheet& S = Sheets.FindOrAdd(Mat);
	const FVector2f U = (B - A).GetSafeNormal(), Nm(-U.Y, U.X);
	const float L = (B - A).Size();
	if (L < 0.01f) return;
	const int32 Ns = FMath::Max(1, FMath::CeilToInt(L / Step)), Nt = FMath::Max(1, FMath::CeilToInt(W / Step));
	const int32 Base = S.V.Num();
	for (int32 j = 0; j <= Nt; ++j)
		for (int32 i = 0; i <= Ns; ++i)
		{
			const FVector2f P = A + U * (L * i / Ns) + Nm * (W * (float(j) / Nt - 0.5f));
			Vertex(S, P.X, P.Y, Lift);
		}
	for (int32 j = 0; j < Nt; ++j)
		for (int32 i = 0; i < Ns; ++i)
		{
			const int32 Pa = Base + j * (Ns + 1) + i, Pb = Pa + 1, Pc = Pa + Ns + 1, Pd = Pc + 1;
			Tri(S, Pa, Pb, Pd); Tri(S, Pa, Pd, Pc);
		}
}

void AAirfield::FlushSheets()
{
	// each material's sheet as one mesh that the trucks, the walker and the grass's traces all stand on
	for (TPair<FName, FSheet>& It : Sheets)
	{
		UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(this, It.Key);
		M->SetupAttachment(Root);
		M->bUseComplexAsSimpleCollision = true;
		M->bUseAsyncCooking = true;
		M->SetCollisionProfileName(TEXT("BlockAll"));
		M->SetCastShadow(false);
		M->RegisterComponent();
		AddInstanceComponent(M);
		const bool bEdge = It.Key == TEXT("edge");
		M->CreateMeshSection_LinearColor(0, It.Value.V, It.Value.I, It.Value.N, It.Value.UV, It.Value.C, TArray<FProcMeshTangent>(), !bEdge);
		if (bEdge)
		{
			// the soft edges: a see-through picture over both surfaces (Tools/set_edge_bands.py); without it, none
			UMaterialInterface* EdgeM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Edges/M_EdgeBand.M_EdgeBand"));
			if (!EdgeM) { ADP_WARN(TEXT("Airfield: /Game/Edges/M_EdgeBand is missing (Tools/set_edge_bands.py); the pavement edges stay hard")); M->SetVisibility(false); }
			else M->SetMaterial(0, EdgeM);
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		else M->SetMaterial(0, ATerminalBuilder::KitMaterial(this, It.Key));
		SheetMeshes.Add(M);
		ADP_LOG(TEXT("Airfield sheet %s: %d corners, %d triangles"), *It.Key.ToString(), It.Value.V.Num(), It.Value.I.Num() / 3);
	}
	Sheets.Empty();
}

#pragma endregion

#pragma region Build

bool AAirfield::Build(bool bModelSite)
{
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube || !D.Load()) return false;
	Sink = bModelSite ? -0.25f : 0.f;

	// the grass over the whole airport on the grade (the globe's own ground meets it at the boundary, where the grade
	// has come back down to it)
	Quad("grass", D.Boundary.X0, D.Boundary.Y0, D.Boundary.X1, D.Boundary.Y1, -0.08f, 25.f);
	// landside in front of the terminal: the kerb, the road and the car park (level ground)
	if (!bModelSite) Quad("asphalt", -160, -170, 392, -0.5f, -0.05f, 50.f);
	// the aprons (concrete, level); with the Babylon model only those away from the terminal (it has the terminal's own)
	for (const FAirfieldData::FRect& A : D.Aprons)
		if (!bModelSite || !(A.Id == TEXT("APRON-T") || A.Id == TEXT("APRON-9"))) Quad("apron", A.X0, A.Y0, A.X1, A.Y1, -0.04f, 20.f);
	// the taxiways: shoulders, pavement and the yellow centre line, all riding the grade
	for (const FAirfieldData::FTwy& T : D.Taxiways)
		for (int32 i = 0; i + 1 < T.Pts.Num(); ++i)
		{
			const FVector2f A = T.Pts[i], B = T.Pts[i + 1], U = (B - A).GetSafeNormal();
			const bool bLane = T.Kind == TEXT("taxilane");
			if (!bLane) Band("concrete", A - U * 20, B + U * 20, T.W + 35, -0.035f);
			Band("asphalt", A - U * (T.W / 2), B + U * (T.W / 2), T.W, -0.03f);
			Band("yellow", A, B, 0.3f, -0.018f);
		}
	// the runway: shoulders, pavement, centre line dashes, edge lines, threshold bars, designators, touchdown marks
	const float RY = D.RwyY, HW = D.RwyW / 2;
	Quad("concrete", D.RwyX0 - 60, RY - HW - 7.5f, D.RwyX1 + 60, RY + HW + 7.5f, -0.025f);
	Quad("asphalt", D.RwyX0, RY - HW, D.RwyX1, RY + HW, -0.02f);
	for (float X = D.RwyX0 + 400; X < D.RwyX1 - 400; X += 50) Quad("white", X, RY - 0.45f, X + 30, RY + 0.45f, -0.008f);
	for (const float S : {-1.f, 1.f}) Quad("white", D.RwyX0, RY + S * (HW - 0.6f) - 0.45f, D.RwyX1, RY + S * (HW - 0.6f) + 0.45f, -0.008f);
	for (const float End : {0.f, 1.f})
	{
		const float X0 = End ? D.RwyX1 - 6 - 30 : D.RwyX0 + 6, Dir = End ? -1.f : 1.f;
		// the threshold's piano keys: 16 bars, 30 m long
		for (int32 k = 0; k < 16; ++k)
		{
			const float Y = RY - HW + 3 + k * ((D.RwyW - 6) / 16) + (k >= 8 ? 1.8f : 0);
			Quad("white", X0, Y, X0 + 30, Y + 1.8f, -0.008f);
		}
		// the touchdown zone marks
		for (int32 k = 0; k < 6; ++k)
		{
			const float X = (End ? D.RwyX1 : D.RwyX0) + Dir * (400 + k * 150);
			for (const float S : {-1.f, 1.f}) Quad("white", X, RY + S * 10 - 1.5f, X + Dir * 22.5f, RY + S * 10 + 1.5f, -0.008f);
		}
		// the designator painted on the runway, read from an aircraft landing
		UTextRenderComponent* Num = NewObject<UTextRenderComponent>(this);
		Num->SetupAttachment(Root);
		Num->SetText(FText::FromString(End ? TEXT("26") : TEXT("08")));
		Num->SetHorizontalAlignment(EHTA_Center);
		Num->SetVerticalAlignment(EVRTA_TextCenter);
		Num->SetWorldSize(1800.f);
		Num->SetTextRenderColor(FColor::White);
		const float NX = (End ? D.RwyX1 : D.RwyX0) + Dir * 60;
		const FVector At = FTerminalPlan::ToWorld(NX, GroundAt(NX, RY) - 0.008f + Sink, RY);
		const FVector2f Sl = SlopeAt(NX, RY);
		Num->SetRelativeLocationAndRotation(At, FRotator(90.f - FMath::RadiansToDegrees(FMath::Atan(Sl.X)) * (End ? -1.f : 1.f), End ? 0.f : 180.f, 0));
		Num->RegisterComponent();
		AddInstanceComponent(Num);
	}
	// the runway-holding positions: four yellow lines across the taxiway
	for (const FVector2f& H : D.Holds)
		for (int32 k = 0; k < 4; ++k) Quad("yellow", H.X - 12.5f, H.Y - 1.5f + k * 1.0f, H.X + 12.5f, H.Y - 1.2f + k * 1.0f, -0.018f);
	// the stands' lead-in lines on the apron (the terminal's own stands already have theirs)
	for (const FAirfieldData::FStand& S : D.Stands)
		if (S.bCargo || S.Id == TEXT("STAND-9")) Band("yellow", FVector2f(S.X, S.Nose), FVector2f(S.X, S.Nose - S.Dir * (S.Len + 20)), 0.3f, -0.028f);
	// each hangar's lead-in line: from taxilane K straight to the apron's edge on the hangar's centre line, where the
	// hangar's own line carries on into the hall (whatever its size, its centre line is this line)
	if (const FAirfieldData::FTwy* K = D.Find(TEXT("K")))
		for (const FAirfieldData::FHangarSite& H : D.Hangars) Band("yellow", FVector2f(H.X, K->Pts[0].Y), FVector2f(H.X, H.Y), 0.3f, -0.028f);
	EdgeBands(bModelSite);
	FlushSheets();
	// the cargo building
	Slab("liftClad", D.Cargo.X0, D.Cargo.Y0, D.Cargo.X1, D.Cargo.Y1, 0, 18);
	ADP_LOG(TEXT("Airfield built: %d taxiways, %d stands"), D.Taxiways.Num(), D.Stands.Num());
	return true;
}

#pragma endregion
