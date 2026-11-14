// The airfield: read from Data/airfield.json and laid out on its grade (Airfield.h), each surface a draped mesh a few
// millimetres over the one under it so they never flicker through each other: grass, then aprons, taxiways, runway,
// then the markings. Only what stands up (the cargo building) is still a slab.
#include "Airfield.h"
#include "Algo/Reverse.h"
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
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"

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
	const TArray<TSharedPtr<FJsonValue>>* Rs = nullptr;
	if (R->TryGetArrayField(TEXT("roads"), Rs))
		for (const TSharedPtr<FJsonValue>& V : *Rs)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FRoad Rd;
			O->TryGetStringField(TEXT("id"), Rd.Id); Rd.W = N(O, TEXT("w"), 7); O->TryGetBoolField(TEXT("hold"), Rd.bHold);
			for (const TSharedPtr<FJsonValue>& P : O->GetArrayField(TEXT("pts"))) { const TArray<TSharedPtr<FJsonValue>>& A = P->AsArray(); Rd.Pts.Add(FVector2f(A[0]->AsNumber(), A[1]->AsNumber())); }
			if (Rd.Pts.Num() >= 2) Roads.Add(Rd);
		}
	const TArray<TSharedPtr<FJsonValue>>* Ls = nullptr;
	if (R->TryGetArrayField(TEXT("lots"), Ls))
		for (const TSharedPtr<FJsonValue>& V : *Ls)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FLot L{TEXT(""), N(O, TEXT("x0")), N(O, TEXT("y0")), N(O, TEXT("x1")), N(O, TEXT("y1"))};
			O->TryGetStringField(TEXT("id"), L.Id); O->TryGetBoolField(TEXT("paved"), L.bPaved); O->TryGetBoolField(TEXT("onModel"), L.bOnModel);
			const TSharedPtr<FJsonObject>* B;
			if (O->TryGetObjectField(TEXT("bays"), B)) { L.BayX0 = N(*B, TEXT("x0")); L.BayY = N(*B, TEXT("y")); L.Bays = int32(N(*B, TEXT("n"))); L.BayW = N(*B, TEXT("w"), 2.6); L.BayD = N(*B, TEXT("d"), 5.2); L.BayDir = N(*B, TEXT("dir"), 1); }
			Lots.Add(L);
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
	// no colour given: the green carries how dirty the paint is here (the paint instances' PaintDirt, Tools/set_ground.py),
	// a smooth noise over the airfield so every marking has its own amount, a long line changing gently along itself
	FLinearColor C = Col;
	if (Col.Equals(FLinearColor::White))
	{
		auto H = [](int32 I, int32 J) { return FMath::Frac(FMath::Sin(I * 127.1f + J * 311.7f) * 43758.5453f); };
		const float Fx = X / 45.f, Fy = Y / 45.f;
		const int32 I = FMath::FloorToInt(Fx), J = FMath::FloorToInt(Fy);
		float U = Fx - I, V = Fy - J;
		U = U * U * (3 - 2 * U); V = V * V * (3 - 2 * V);
		const float N = FMath::Lerp(FMath::Lerp(H(I, J), H(I + 1, J), U), FMath::Lerp(H(I, J + 1), H(I + 1, J + 1), U), V);
		C.G = 0.10f + 0.55f * N * N;
	}
	// the corner on the grade; its normal from the slope (the world's y runs against the plan's); 10 m texture tiles
	const FVector2f Sl = SlopeAt(X, Y);
	S.V.Add(FTerminalPlan::ToWorld(X, GroundAt(X, Y) + Lift + Sink, Y));
	S.N.Add(FVector(-Sl.X, Sl.Y, 1.f).GetSafeNormal());
	S.UV.Add(UVOverride ? *UVOverride : FVector2D(X / 10.0, Y / 10.0));
	S.C.Add(C);
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
	// the runway, taxiways and aprons get theirs from the paving's outline (PaveTaxiways); here only the landside road
	// and car park, on its open sides
	if (!bModelSite) EdgeRect(-160, -170, 392, -0.5f, -0.05f, 1.f, 13);
}

#pragma region The taxiway system

namespace
{
	float SegDist(const FVector2f& P, const FVector2f& A, const FVector2f& B)
	{
		const FVector2f AB = B - A;
		const float T = FMath::Clamp(FVector2f::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1e-6f), 0.f, 1.f);
		return (P - (A + AB * T)).Size();
	}
	// signed distance to a rectangle: negative inside
	float BoxSd(const FVector2f& P, float X0, float Y0, float X1, float Y1)
	{
		const float Qx = FMath::Abs(P.X - (X0 + X1) / 2) - FMath::Abs(X1 - X0) / 2, Qy = FMath::Abs(P.Y - (Y0 + Y1) / 2) - FMath::Abs(Y1 - Y0) / 2;
		return FMath::Sqrt(FMath::Square(FMath::Max(Qx, 0.f)) + FMath::Square(FMath::Max(Qy, 0.f))) + FMath::Min(FMath::Max(Qx, Qy), 0.f);
	}
	// the smooth union of two shapes: a fillet of about K metres where they meet
	float SMin(float A, float B, float K)
	{
		const float H = FMath::Max(K - FMath::Abs(A - B), 0.f) / K;
		return FMath::Min(A, B) - H * H * K * 0.25f;
	}
	// the fields every grid corner carries: the taxiway pavement, its shoulders (the outline of all the paving), the runway,
	// the runway with its shoulders, the aprons and the bare taxiways
	enum EField { FPave, FShoulder, FRwy, FRwyOuter, FApron, FTaxi, NF };
	struct FCorner { FVector2f P; float F[NF]; };
	struct FCut { int32 Field; float Level, Sign; };   // keeps Sign * (field - Level) <= 0
	void ClipPoly(TArray<FCorner, TInlineAllocator<12>>& Poly, const FCut& C)
	{
		if (Poly.Num() < 3) return;
		TArray<FCorner, TInlineAllocator<12>> Out;
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			const FCorner& A = Poly[i];
			const FCorner& B = Poly[(i + 1) % Poly.Num()];
			const float Fa = C.Sign * (A.F[C.Field] - C.Level), Fb = C.Sign * (B.F[C.Field] - C.Level);
			if (Fa <= 0.f) Out.Add(A);
			if ((Fa <= 0.f) != (Fb <= 0.f))
			{
				const float T = Fa / (Fa - Fb);
				FCorner X;
				X.P = FMath::Lerp(A.P, B.P, T);
				for (int32 f = 0; f < NF; ++f) X.F[f] = FMath::Lerp(A.F[f], B.F[f], T);
				Out.Add(X);
			}
		}
		Poly = MoveTemp(Out);
	}
}

void AAirfield::PaveTaxiways(bool bModelSite)
{
	const float RY = D.RwyY, HW = D.RwyW / 2, HWo = HW + 7.5f, Shoulder = 7.5f;
	const float Rx0 = D.RwyX0, Rx1 = D.RwyX1;
	// the model's own aprons (with the Babylon terminal): nothing of ours is drawn on them
	auto OnModelApron = [&](const FVector2f& P, float Margin)
	{
		if (!bModelSite) return false;
		for (const FAirfieldData::FRect& A : D.Aprons)
			if ((A.Id == TEXT("APRON-T") || A.Id == TEXT("APRON-9")) && BoxSd(P, A.X0, A.Y0, A.X1, A.Y1) < Margin) return true;
		return false;
	};

	// ---- the pieces: every taxiway, link and taxilane as its centre line, plus the turn curves at each junction
	struct FPiece { TArray<FVector2f> Pts; float Hw; bool bArc; };
	TArray<FPiece> Pieces;
	for (const FAirfieldData::FTwy& T : D.Taxiways)
		if (T.Pts.Num() >= 2) Pieces.Add({T.Pts, T.W / 2, false});
	// a turn curve from a link onto the line it joins (real junctions: the yellow line curves round, the pavement is
	// widened under the curve so the main wheels stay on it; owner's photos, Boston and Seattle)
	auto AddArc = [&](const FVector2f& E, const FVector2f& In, const FVector2f& Along, float R, float Hw)
	{
		const FVector2f C = E + In * R + Along * R;
		FPiece Arc{{}, Hw, true};
		constexpr int32 N = 24;
		for (int32 k = 0; k <= N; ++k)
		{
			const float Th = HALF_PI * k / N;
			Arc.Pts.Add(C - Along * (R * FMath::Cos(Th)) - In * (R * FMath::Sin(Th)));
		}
		Pieces.Add(Arc);
	};
	const int32 NStraight = Pieces.Num();
	for (const FAirfieldData::FTwy& L : D.Taxiways)
	{
		if (L.Kind == TEXT("curve") || L.Pts.Num() < 2) continue;
		for (const int32 e : {0, L.Pts.Num() - 1})
		{
			const FVector2f E = L.Pts[e], Nx = L.Pts[e == 0 ? 1 : e - 1];
			const FVector2f In = (Nx - E).GetSafeNormal();
			const float Len = (Nx - E).Size();
			// onto the runway's centre line: lead-on and lead-off curves both ways, where the runway goes on
			if (FMath::Abs(E.Y - RY) < 1.f && FMath::Abs(In.X) < 0.2f && Len > 70.f)
			{
				for (const float S : {-1.f, 1.f})
					if (E.X + S * 60.f > Rx0 + 30.f && E.X + S * 60.f < Rx1 - 30.f) AddArc(E, In, FVector2f(S, 0.f), 60.f, L.W / 2);
				continue;
			}
			// onto another taxiway's or taxilane's side (a T, not the corner where two end: those have their own curves)
			for (const FAirfieldData::FTwy& O : D.Taxiways)
			{
				if (&O == &L || O.Kind == TEXT("curve") || O.Kind == TEXT("link")) continue;
				if ((E - O.Pts[0]).Size() < 1.f || (E - O.Pts.Last()).Size() < 1.f) continue;
				for (int32 k = 0; k + 1 < O.Pts.Num(); ++k)
				{
					const FVector2f A = O.Pts[k], B = O.Pts[k + 1], U = (B - A).GetSafeNormal();
					if (SegDist(E, A, B) > 1.f || FMath::Abs(FVector2f::DotProduct(In, U)) > 0.2f) continue;
					for (const float S : {-1.f, 1.f})
						if (SegDist(E + U * (S * 45.f), A, B) < 0.5f && Len > 50.f) AddArc(E, In, U * S, 45.f, L.W / 2);
				}
			}
		}
	}
	ADP_LOG(TEXT("Airfield: %d taxiway pieces, %d turn curves"), NStraight, Pieces.Num() - NStraight);

	// ---- the grid of fields over the whole paving
	float X0 = Rx0 - 100.f, X1 = Rx1 + 100.f, Y0 = RY - HWo - 40.f, Y1 = RY + HWo + 40.f;
	for (const FPiece& P : Pieces) for (const FVector2f& Q : P.Pts) { X0 = FMath::Min(X0, Q.X - 40.f); X1 = FMath::Max(X1, Q.X + 40.f); Y0 = FMath::Min(Y0, Q.Y - 40.f); Y1 = FMath::Max(Y1, Q.Y + 40.f); }
	for (const FAirfieldData::FRect& A : D.Aprons) { X0 = FMath::Min(X0, FMath::Min(A.X0, A.X1) - 40.f); X1 = FMath::Max(X1, FMath::Max(A.X0, A.X1) + 40.f); Y0 = FMath::Min(Y0, FMath::Min(A.Y0, A.Y1) - 40.f); Y1 = FMath::Max(Y1, FMath::Max(A.Y0, A.Y1) + 40.f); }
	constexpr float St = 2.5f;
	const int32 Nx = FMath::CeilToInt((X1 - X0) / St), Ny = FMath::CeilToInt((Y1 - Y0) / St), Nx1 = Nx + 1;
	const int32 Nodes = Nx1 * (Ny + 1);
	TArray<float> F[NF];
	for (int32 f = 0; f < NF; ++f) F[f].Init(1e4f, Nodes);
	auto NodeP = [&](int32 n) { return FVector2f(X0 + (n % Nx1) * St, Y0 + (n / Nx1) * St); };
	// the bare taxiways: each piece's distance only near it
	for (const FPiece& P : Pieces)
		for (int32 k = 0; k + 1 < P.Pts.Num(); ++k)
		{
			const FVector2f A = P.Pts[k], B = P.Pts[k + 1];
			const float Reach = P.Hw + Shoulder + 25.f;
			const int32 I0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(A.X, B.X) - Reach - X0) / St)), I1 = FMath::Min(Nx, FMath::CeilToInt((FMath::Max(A.X, B.X) + Reach - X0) / St));
			const int32 J0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(A.Y, B.Y) - Reach - Y0) / St)), J1 = FMath::Min(Ny, FMath::CeilToInt((FMath::Max(A.Y, B.Y) + Reach - Y0) / St));
			for (int32 j = J0; j <= J1; ++j)
				for (int32 i = I0; i <= I1; ++i)
				{
					const int32 n = j * Nx1 + i;
					F[FTaxi][n] = FMath::Min(F[FTaxi][n], SegDist(NodeP(n), A, B) - P.Hw);
				}
		}
	auto Fields = [&](const FVector2f& Q, float Taxi, float* Out)
	{
		float Ap = 1e4f;
		for (const FAirfieldData::FRect& A : D.Aprons) Ap = FMath::Min(Ap, BoxSd(Q, A.X0, A.Y0, A.X1, A.Y1));
		Out[FTaxi] = Taxi;
		Out[FApron] = Ap;
		Out[FRwy] = BoxSd(Q, Rx0, RY - HW, Rx1, RY + HW);
		Out[FRwyOuter] = BoxSd(Q, Rx0 - 60.f, RY - HWo, Rx1 + 60.f, RY + HWo);
		// fillets where the taxiways meet the runway and the aprons; the shoulders round the outside of it all
		Out[FPave] = SMin(SMin(Taxi, Out[FRwy], 8.f), Ap, 10.f);
		Out[FShoulder] = SMin(SMin(Taxi - Shoulder, Out[FRwyOuter], 10.f), Ap, 10.f);
	};
	for (int32 n = 0; n < Nodes; ++n)
	{
		float V[NF];
		Fields(NodeP(n), F[FTaxi][n], V);
		for (int32 f = 0; f < NF; ++f) F[f][n] = V[f];
	}

	// ---- the sheets, each the part of the grid inside its cuts (cut along the fields, so their edges follow the curves)
	struct FSpec { FName Mat; float Lift; TArray<FCut> Cuts; };
	TArray<FSpec> Specs;
	// the taxiway pavement: up to the fillets, a little under the runway and the aprons, only near the taxiways
	Specs.Add({TEXT("asphalt"), -0.0225f, {{FPave, 0.f, 1.f}, {FRwy, -0.3f, -1.f}, {FApron, -0.5f, -1.f}, {FTaxi, 9.f, 1.f}}});
	// the shoulders: from the pavement's edge to the grass, under the runway's own shoulders
	Specs.Add({TEXT("shoulder"), -0.036f, {{FShoulder, 0.f, 1.f}, {FPave, -0.3f, -1.f}, {FRwyOuter, -0.3f, -1.f}, {FApron, -0.3f, -1.f}, {FTaxi, Shoulder + 11.f, 1.f}}});
	// the taxi side stripes (ICAO: two solid yellow lines 15 cm wide, 15 cm apart, at the edge of the full-strength
	// pavement), following the fillets, stopping short of the runway's shoulders and the aprons
	Specs.Add({TEXT("paintyellow"), -0.018f, {{FPave, 0.f, 1.f}, {FPave, -0.15f, -1.f}, {FRwyOuter, 0.5f, -1.f}, {FApron, 0.5f, -1.f}}});
	Specs.Add({TEXT("paintyellow"), -0.018f, {{FPave, -0.3f, 1.f}, {FPave, -0.45f, -1.f}, {FRwyOuter, 0.5f, -1.f}, {FApron, 0.5f, -1.f}}});
	auto Corner = [&](int32 n) { FCorner C; C.P = NodeP(n); for (int32 f = 0; f < NF; ++f) C.F[f] = F[f][n]; return C; };
	for (const FSpec& Sp : Specs)
	{
		FSheet& S = Sheets.FindOrAdd(Sp.Mat);
		TArray<int32> Cache;
		Cache.Init(-1, Nodes);
		auto Shared = [&](int32 n) { if (Cache[n] < 0) { Cache[n] = S.V.Num(); const FVector2f P = NodeP(n); Vertex(S, P.X, P.Y, Sp.Lift); } return Cache[n]; };
		int32 Tris = 0;
		for (int32 j = 0; j < Ny; ++j)
			for (int32 i = 0; i < Nx; ++i)
			{
				const int32 n0 = j * Nx1 + i, n1 = n0 + 1, n2 = n0 + Nx1 + 1, n3 = n0 + Nx1;
				if (bModelSite && OnModelApron(NodeP(n0), -St * 2)) continue;
				const int32 TT[2][3] = {{n0, n1, n2}, {n0, n2, n3}};
				for (const auto& T : TT)
				{
					bool bAllIn = true, bOut = false;
					for (const FCut& C : Sp.Cuts)
					{
						int32 In = 0;
						for (const int32 n : T) In += C.Sign * (F[C.Field][n] - C.Level) <= 0.f;
						if (In == 0) { bOut = true; break; }
						if (In < 3) bAllIn = false;
					}
					if (bOut) continue;
					if (bAllIn) { Tri(S, Shared(T[0]), Shared(T[1]), Shared(T[2])); ++Tris; continue; }
					TArray<FCorner, TInlineAllocator<12>> Poly = {Corner(T[0]), Corner(T[1]), Corner(T[2])};
					for (const FCut& C : Sp.Cuts) ClipPoly(Poly, C);
					if (Poly.Num() < 3) continue;
					const int32 Base = S.V.Num();
					for (const FCorner& C : Poly) Vertex(S, C.P.X, C.P.Y, Sp.Lift);
					for (int32 k = 1; k + 1 < Poly.Num(); ++k) { Tri(S, Base, Base + k, Base + k + 1); ++Tris; }
				}
			}
		ADP_LOG(TEXT("Airfield: taxiway sheet %s %d triangles"), *Sp.Mat.ToString(), Tris);
	}

	// ---- the yellow centre lines, the turn curves' too (the straight ones stop at the runway's shoulders: on the runway
	// only the curves lead on and off, as painted for real)
	for (int32 p = 0; p < Pieces.Num(); ++p)
	{
		const FPiece& P = Pieces[p];
		for (int32 k = 0; k + 1 < P.Pts.Num(); ++k)
		{
			FVector2f A = P.Pts[k], B = P.Pts[k + 1];
			if (!P.bArc)
			{
				auto In = [&](const FVector2f& Q) { return FMath::Abs(Q.Y - RY) < HWo && Q.X > Rx0 - 60.f && Q.X < Rx1 + 60.f; };
				if (In(A) && In(B)) continue;
				if (In(A)) A.Y = B.Y < RY ? RY - HWo : RY + HWo;
				if (In(B)) B.Y = A.Y < RY ? RY - HWo : RY + HWo;
			}
			if (OnModelApron(A, -1.f) && OnModelApron(B, -1.f) && !P.bArc) continue;   // the taxilanes there: Aprons()
			if ((B - A).Size() <= 0.05f) continue;
			if (bModelSite && (OnModelApron(A, 1.f) || OnModelApron(B, 1.f)))
			{
				// crossing onto the terminal's apron (laid 2 cm over the model's ground): drawn in short pieces, each at its height
				const int32 N = FMath::Max(1, FMath::CeilToInt((B - A).Size() / 2.f));
				for (int32 q = 0; q < N; ++q)
				{
					const FVector2f Pa = FMath::Lerp(A, B, float(q) / N), Pb = FMath::Lerp(A, B, float(q + 1) / N);
					Band("paintyellow", Pa, Pb, 0.3f, OnModelApron((Pa + Pb) / 2, 0.f) ? 0.028f - Sink : -0.018f, 4.f);
				}
			}
			else Band("paintyellow", A, B, 0.3f, -0.018f, P.bArc ? 4.f : 10.f);
		}
	}

	// ---- the soft grass edge all round the paving: the outline of the shoulders' field, followed cell by cell
	auto Inside = [&](int32 n) { return F[FShoulder][n] <= 0.f; };
	auto EdgeP = [&](int64 Id)
	{
		const int32 n = int32(Id / 2), m = (Id & 1) ? n + Nx1 : n + 1;
		const float A = F[FShoulder][n], B = F[FShoulder][m];
		return FMath::Lerp(NodeP(n), NodeP(m), FMath::Clamp(A / (A - B), 0.f, 1.f));
	};
	TArray<TPair<int64, int64>> Segs;
	TMap<int64, TArray<int32, TInlineAllocator<2>>> Adj;
	auto AddSeg = [&](int64 A, int64 B) { const int32 s = Segs.Add({A, B}); Adj.FindOrAdd(A).Add(s); Adj.FindOrAdd(B).Add(s); };
	for (int32 j = 0; j < Ny; ++j)
		for (int32 i = 0; i < Nx; ++i)
		{
			const int32 n0 = j * Nx1 + i, n1 = n0 + 1, n2 = n0 + Nx1 + 1, n3 = n0 + Nx1;
			const bool c0 = Inside(n0), c1 = Inside(n1), c2 = Inside(n2), c3 = Inside(n3);
			const int64 eB = int64(n0) * 2, eR = int64(n1) * 2 + 1, eT = int64(n3) * 2, eL = int64(n0) * 2 + 1;
			TArray<int64, TInlineAllocator<4>> X;
			if (c0 != c1) X.Add(eB);
			if (c1 != c2) X.Add(eR);
			if (c2 != c3) X.Add(eT);
			if (c3 != c0) X.Add(eL);
			if (X.Num() == 2) AddSeg(X[0], X[1]);
			else if (X.Num() == 4)
			{
				const bool cC = (F[FShoulder][n0] + F[FShoulder][n1] + F[FShoulder][n2] + F[FShoulder][n3]) * 0.25f <= 0.f;
				if (cC == c0) { AddSeg(eB, eR); AddSeg(eT, eL); }
				else { AddSeg(eL, eB); AddSeg(eR, eT); }
			}
		}
	TArray<bool> Used;
	Used.Init(false, Segs.Num());
	auto Extend = [&](TArray<int64>& C)
	{
		for (;;)
		{
			const TArray<int32, TInlineAllocator<2>>* L = Adj.Find(C.Last());
			int32 Next = -1;
			if (L) for (const int32 t : *L) if (!Used[t]) { Next = t; break; }
			if (Next < 0) return;
			Used[Next] = true;
			C.Add(Segs[Next].Key == C.Last() ? Segs[Next].Value : Segs[Next].Key);
		}
	};
	auto Masked = [&](const FVector2f& Q)
	{
		if (OnModelApron(Q, 4.f)) return true;
		// without the model: the terminal's own apron meets the building along its south side
		if (!bModelSite) for (const FAirfieldData::FRect& A : D.Aprons)
			if (A.Id == TEXT("APRON-T") && Q.Y < FMath::Min(A.Y0, A.Y1) + 2.f && Q.X > FMath::Min(A.X0, A.X1) - 2.f && Q.X < 250.f) return true;
		return false;
	};
	auto Sample = [&](const FVector2f& Q)
	{
		const float Fx = FMath::Clamp((Q.X - X0) / St, 0.f, Nx - 0.001f), Fy = FMath::Clamp((Q.Y - Y0) / St, 0.f, Ny - 0.001f);
		const int32 i = int32(Fx), j = int32(Fy), n = j * Nx1 + i;
		const float U = Fx - i, V = Fy - j;
		return FMath::Lerp(FMath::Lerp(F[FShoulder][n], F[FShoulder][n + 1], U), FMath::Lerp(F[FShoulder][n + Nx1], F[FShoulder][n + Nx1 + 1], U), V);
	};
	int32 Runs = 0;
	for (int32 s = 0; s < Segs.Num(); ++s)
	{
		if (Used[s]) continue;
		Used[s] = true;
		TArray<int64> Chain = {Segs[s].Key, Segs[s].Value};
		Extend(Chain);
		Algo::Reverse(Chain);
		Extend(Chain);
		const bool bClosed = Chain.Num() > 3 && Chain[0] == Chain.Last();
		TArray<FVector2f> Pts;
		for (const int64 Id : Chain) Pts.Add(EdgeP(Id));
		if (bClosed) Pts.Pop();
		const int32 N = Pts.Num();
		if (N < 2) continue;
		TArray<bool> M;
		for (const FVector2f& Q : Pts) M.Add(Masked(Q));
		// the runs between masked stretches (a loop with none: one closed run)
		TArray<TArray<int32>> RunIdx;
		const int32 FirstMasked = M.Find(true);
		if (FirstMasked == INDEX_NONE)
		{
			TArray<int32> R;
			for (int32 k = 0; k < N; ++k) R.Add(k);
			if (bClosed) R.Add(0);
			RunIdx.Add(R);
		}
		else
		{
			TArray<int32> R;
			const int32 Count = bClosed ? N + 1 : N, Start = bClosed ? FirstMasked : 0;
			for (int32 c = 0; c < Count; ++c)
			{
				const int32 k = bClosed ? (Start + c) % N : c;
				if (M[k]) { if (R.Num() >= 2) RunIdx.Add(R); R.Reset(); }
				else R.Add(k);
			}
			if (R.Num() >= 2) RunIdx.Add(R);
		}
		for (const TArray<int32>& R : RunIdx)
		{
			TArray<FVector2f> P, Out, Flags;
			TArray<float> Lift;
			for (int32 q = 0; q < R.Num(); ++q)
			{
				const FVector2f Q = Pts[R[q]];
				const FVector2f Prev = Pts[R[FMath::Max(q - 1, 0)]], Next = Pts[R[FMath::Min(q + 1, R.Num() - 1)]];
				FVector2f T = (Next - Prev).GetSafeNormal(), Nm(T.Y, -T.X);
				if (Sample(Q + Nm) < Sample(Q - Nm)) Nm = -Nm;
				float V[NF];
				Fields(Q, 1e4f, V);
				P.Add(Q); Out.Add(Nm);
				Flags.Add(FVector2f(V[FApron] < 3.f ? 0.f : 1.f, (V[FRwyOuter] < 3.f && (Q.X < Rx0 || Q.X > Rx1)) ? 1.f : 0.f));
				Lift.Add(V[FRwyOuter] < 1.f ? -0.025f : V[FApron] < 1.f ? -0.04f : -0.036f);
			}
			EdgePoly(P, Out, Flags, Lift, !(bClosed && FirstMasked == INDEX_NONE));
			++Runs;
		}
	}
	ADP_LOG(TEXT("Airfield: the paving's outline, %d edge runs"), Runs);
}

void AAirfield::Aprons(bool bModelSite)
{
	// the terminal's aprons sit on the model's ground (2 cm over it, so ours is what shows); the others on the grade
	auto IsModel = [&](const FAirfieldData::FRect& A) { return bModelSite && (A.Id == TEXT("APRON-T") || A.Id == TEXT("APRON-9")); };
	auto ApronAt = [&](const FVector2f& P) -> const FAirfieldData::FRect*
	{
		for (const FAirfieldData::FRect& A : D.Aprons) if (BoxSd(P, A.X0, A.Y0, A.X1, A.Y1) <= 0.01f) return &A;
		return nullptr;
	};
	auto Base = [&](const FVector2f& P) { const FAirfieldData::FRect* A = ApronAt(P); return A && IsModel(*A) ? 0.02f - Sink : -0.04f; };
	for (const FAirfieldData::FRect& A : D.Aprons)
		Quad("apron", A.X0, A.Y0, A.X1, A.Y1, IsModel(A) ? 0.02f - Sink : -0.04f, IsModel(A) ? 5.f : 20.f);
	// the taxilanes' centre lines on the terminal's aprons (the others' are drawn with the taxiways)
	for (const FAirfieldData::FTwy& T : D.Taxiways)
		for (int32 k = 0; k + 1 < T.Pts.Num(); ++k)
		{
			const FVector2f A = T.Pts[k], B = T.Pts[k + 1], M = (A + B) / 2;
			const FAirfieldData::FRect* R = ApronAt(M);
			if (R && IsModel(*R)) Band("paintyellow", A, B, 0.2f, Base(M) + 0.008f, 10.f);
		}

	auto Hash = [](float A) { return FMath::Frac(FMath::Sin(A * 12.9898f + 4.1414f) * 43758.5453f); };
	auto Line = [&](const FName Mat, const FVector2f& A, const FVector2f& B, float W, float Up)
	{
		if ((B - A).Size() <= 0.05f) return;
		Band(Mat, A, B, W, Base((A + B) / 2) + Up, 4.f);
		// red lines edged in white, as on European aprons (reference photos: Kristiansand, Kyiv Boryspil)
		if (Mat == TEXT("paintred") && W >= 0.15f) Band("paintwhite", A, B, W + 0.2f, Base((A + B) / 2) + Up - 0.002f, 4.f);
	};
	auto Text = [&](const FString& S, float X, float Y, float H, float Yaw, const FColor& C)
	{
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
		T->SetupAttachment(Root);
		T->SetText(FText::FromString(S));
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(H * 100.f);
		T->SetTextRenderColor(C);
		T->SetRelativeLocationAndRotation(FTerminalPlan::ToWorld(X, Base(FVector2f(X, Y)) + 0.014f + Sink, Y), FRotator(90.f, Yaw + 180.f, 0.f));   // face up, its feet towards the reader
		T->RegisterComponent();
		AddInstanceComponent(T);
	};

	// ---- the stands, in rows (the same direction and taxilane), each row side by side by x
	struct FRow { float TY; float Dir; TArray<int32> Idx; };
	TArray<FRow> Rows;
	for (int32 i = 0; i < D.Stands.Num(); ++i)
	{
		const FAirfieldData::FStand& St = D.Stands[i];
		const float B = -St.Dir;   // from the nose back towards the tail and the taxilane
		float TY = St.Nose + B * (St.Len + 40.f), Best = 1e9f;
		for (const FAirfieldData::FTwy& T : D.Taxiways)
			if (T.Kind == TEXT("taxilane") && T.Pts.Num() >= 2)
			{
				const float Y = T.Pts[0].Y, D_ = (Y - St.Nose) * B;
				const float Xa = FMath::Min(T.Pts[0].X, T.Pts.Last().X), Xb = FMath::Max(T.Pts[0].X, T.Pts.Last().X);
				if (D_ > St.Len && D_ < Best && St.X > Xa - 5.f && St.X < Xb + 5.f) { Best = D_; TY = Y; }
			}
		FRow* R = Rows.FindByPredicate([&](const FRow& Q) { return Q.Dir == St.Dir && FMath::Abs(Q.TY - TY) < 1.f; });
		if (!R) { R = &Rows.AddDefaulted_GetRef(); R->TY = TY; R->Dir = St.Dir; }
		R->Idx.Add(i);
	}
	for (FRow& R : Rows)
	{
		R.Idx.Sort([&](int32 A, int32 B) { return D.Stands[A].X < D.Stands[B].X; });
		const float B = -R.Dir;
		// the boundaries: halfway between neighbours close enough to share one, otherwise each stand's own (its wingtips
		// plus 4.5 m, ICAO code C clearance)
		TArray<float> Left, Right;
		for (int32 k = 0; k < R.Idx.Num(); ++k)
		{
			const FAirfieldData::FStand& St = D.Stands[R.Idx[k]];
			Left.Add(St.X - St.Span / 2 - 4.5f); Right.Add(St.X + St.Span / 2 + 4.5f);
		}
		for (int32 k = 0; k + 1 < R.Idx.Num(); ++k)
			if (Right[k] > Left[k + 1] - 6.f) { const float Mid = (D.Stands[R.Idx[k]].X + D.Stands[R.Idx[k + 1]].X) / 2; Right[k] = Mid; Left[k + 1] = Mid; }
		float RoadAt = -1e9f * B;   // the service road behind the stands' tails
		for (const int32 i : R.Idx) { const FAirfieldData::FStand& St = D.Stands[i]; const float Te = St.Nose + B * (St.Len + 3.f); RoadAt = B > 0 ? FMath::Max(RoadAt, Te) : FMath::Min(RoadAt, Te); }
		for (int32 k = 0; k < R.Idx.Num(); ++k)
		{
			const int32 i = R.Idx[k];
			const FAirfieldData::FStand& St = D.Stands[i];
			const bool bBig = St.Span > 50.f;
			const FAirfieldData::FRect* Ap = ApronAt(FVector2f(St.X, St.Nose));
			const float Wall = Ap ? (R.Dir < 0 ? FMath::Min(Ap->Y0, Ap->Y1) : FMath::Max(Ap->Y0, Ap->Y1)) + R.Dir * -0.6f : St.Nose + R.Dir * 20.f;
			const float Te = St.Nose + B * (St.Len + 3.f);
			// the lead-in line (ICAO 5.2.13: yellow, at least 15 cm) from the taxilane to the stop, with its turns off the
			// taxilane both ways
			Line("paintyellow", FVector2f(St.X, R.TY), FVector2f(St.X, St.Nose), 0.2f, 0.008f);
			for (const float S : {-1.f, 1.f})
			{
				const float Rr = bBig ? 40.f : 30.f;
				const FVector2f E(St.X, R.TY), In(0.f, R.Dir), Al(S, 0.f), C = E + In * Rr + Al * Rr;
				FVector2f Prev = E + In * Rr;
				for (int32 q = 1; q <= 20; ++q)
				{
					const float Th = HALF_PI * q / 20;
					const FVector2f P = C - Al * (Rr * FMath::Cos(Th)) - In * (Rr * FMath::Sin(Th));
					Line("paintyellow", Prev, P, 0.2f, 0.008f);
					Prev = P;
				}
			}
			// the stop bars (owner, 2026-11-13: a stop position for each aircraft type): every type stops with its nose on
			// the stand's nose line, so each bar is where that type's nose wheel stands; the jetway reaches whichever front
			// door that gives. Yellow, 30 cm wide, the types written beside each bar, read by the pilot coming in. (Nose
			// wheel setbacks approximate, to be checked against each maker's airport planning document.)
			struct FStopType { const TCHAR* Label; float NoseGear; };
			static const FStopType Narrow[] = {{TEXT("E190"), 3.6f}, {TEXT("B737"), 4.4f}, {TEXT("A319 A320 A321 B757"), 5.1f}};
			static const FStopType Wide[] = {{TEXT("B767"), 5.6f}, {TEXT("A330 A350 B787"), 6.5f}, {TEXT("B777 B747"), 7.6f}};
			const TArrayView<const FStopType> Types = bBig ? TArrayView<const FStopType>(Wide) : TArrayView<const FStopType>(Narrow);
			for (int32 t = 0; t < Types.Num(); ++t)
			{
				const float Stop = St.Nose + B * Types[t].NoseGear, HalfW = t == Types.Num() - 1 ? 3.f : 1.5f;
				Quad("paintyellow", St.X - HalfW, Stop - 0.15f, St.X + HalfW, Stop + 0.15f, Base(FVector2f(St.X, Stop)) + 0.009f, 2.f);
				// its types on the pilot's right of the line, small yellow letters
				const float Tx = St.X + R.Dir * -(HalfW + 0.4f + 0.16f * FCString::Strlen(Types[t].Label));
				Text(Types[t].Label, Tx, Stop, 0.45f, R.Dir < 0 ? 90.f : -90.f, FColor(250, 205, 30));
			}
			// the stand's number on the lead-in line where the aircraft turns in, yellow on black (the pilot reads it
			// coming in)
			FString Num = St.Id;
			Num.RemoveFromStart(TEXT("STAND-"));
			const float NumY = R.TY - B * 22.f, TH = 3.f, TW = 2.1f * Num.Len() + 1.2f;
			Quad("paintblack", St.X - TW / 2, NumY - TH / 2 - 0.6f, St.X + TW / 2, NumY + TH / 2 + 0.6f, Base(FVector2f(St.X, NumY)) + 0.0085f, 2.f);
			Text(Num, St.X, NumY, TH, R.Dir < 0 ? 90.f : -90.f, FColor(250, 205, 30));
			// the red stand boundaries and the equipment restraint line across the tail end (vehicles wait behind it while
			// the aircraft comes in)
			Line("paintred", FVector2f(Left[k], Wall), FVector2f(Left[k], Te), 0.2f, 0.0075f);
			Line("paintred", FVector2f(Right[k], Wall), FVector2f(Right[k], Te), 0.2f, 0.0075f);
			Line("paintred", FVector2f(Left[k], Te), FVector2f(Right[k], Te), 0.2f, 0.0075f);
			// the jetway's red zone on the left of the nose (the aircraft's left door side): outlined and hatched, nothing
			// parks where the bridge swings
			if (St.bJetway)
			{
				const float X0 = St.X + 3.f, X1 = St.X + 16.f, Y0 = Wall, Y1 = St.Nose + B * 9.f;
				const float Ya = FMath::Min(Y0, Y1), Yb = FMath::Max(Y0, Y1);
				Line("paintred", FVector2f(X0, Ya), FVector2f(X1, Ya), 0.15f, 0.0075f); Line("paintred", FVector2f(X0, Yb), FVector2f(X1, Yb), 0.15f, 0.0075f);
				Line("paintred", FVector2f(X0, Ya), FVector2f(X0, Yb), 0.15f, 0.0075f); Line("paintred", FVector2f(X1, Ya), FVector2f(X1, Yb), 0.15f, 0.0075f);
				for (float Cc = Ya - (X1 - X0); Cc < Yb; Cc += 1.8f)   // hatching at 45 degrees: y = x - X0 + Cc
				{
					FVector2f P0(X0, Cc), P1(X1, Cc + (X1 - X0));
					if (P0.Y < Ya) { P0.X += Ya - P0.Y; P0.Y = Ya; }
					if (P1.Y > Yb) { P1.X -= P1.Y - Yb; P1.Y = Yb; }
					if (P1.X - P0.X > 0.3f) Line("paintred", P0, P1, 0.12f, 0.007f);
				}
			}
			// the stand's own wear (Tools/set_stand_wear.py): soot behind the engines, drips where the wheels stop, faint
			// tyre marks on some stands; every stand its own (the red: its seed, the green: tyre marks, 0 on most)
			{
				FSheet& W = Sheets.FindOrAdd(TEXT("standwear"));
				const float Seed = Hash(St.X * 0.37f + St.Nose * 0.11f);
				const float Tyre = Hash(Seed * 91.f) < 0.35f ? 0.35f + 0.4f * Hash(Seed * 17.f) : 0.f;
				const float Wd = bBig ? 22.f : 15.f, L = St.Len + 2.f;
				const int32 Nx = FMath::CeilToInt(2 * Wd / 2.5f), Ny = FMath::CeilToInt(L / 2.5f), Base0 = W.V.Num();
				for (int32 j = 0; j <= Ny; ++j)
					for (int32 q = 0; q <= Nx; ++q)
					{
						const float Ac = -Wd + 2 * Wd * q / Nx, Al = L * j / Ny;
						const float X = St.X + Ac, Y = St.Nose + B * Al;
						const FVector2D UV(Ac, Al);
						Vertex(W, X, Y, Base(FVector2f(X, Y)) + 0.004f, FLinearColor(Seed, Tyre, bBig ? 1.f : 0.f, 1.f), &UV);
					}
				for (int32 j = 0; j < Ny; ++j)
					for (int32 q = 0; q < Nx; ++q)
					{
						const int32 A = Base0 + j * (Nx + 1) + q, Bq = A + 1, C = A + Nx + 1, Dd = C + 1;
						Tri(W, A, Bq, Dd); Tri(W, A, Dd, C);
					}
			}
		}
		// the service road behind the stands for the ground vehicles (white: solid edges, dashed middle), from the row's
		// first boundary to its last, 8 m wide
		const float X0 = Left[0] - 6.f, X1 = Right.Last() + 6.f, Y0 = RoadAt + B * 1.f, Y1 = RoadAt + B * 9.f;
		Line("paintwhite", FVector2f(X0, Y0), FVector2f(X1, Y0), 0.15f, 0.008f);
		Line("paintwhite", FVector2f(X0, Y1), FVector2f(X1, Y1), 0.15f, 0.008f);
		for (float X = X0; X < X1 - 1.f; X += 6.f) Line("paintwhite", FVector2f(X, (Y0 + Y1) / 2), FVector2f(FMath::Min(X + 3.f, X1), (Y0 + Y1) / 2), 0.12f, 0.008f);
	}
}

void AAirfield::ServiceRoads(bool bModelSite)
{
	// on an apron the road is the apron's own surface: only its lines show there, at the apron's height
	auto OnApron = [&](const FVector2f& Q, float Margin, bool& bModel)
	{
		for (const FAirfieldData::FRect& A : D.Aprons)
			if (BoxSd(Q, A.X0, A.Y0, A.X1, A.Y1) < Margin) { bModel = bModelSite && (A.Id == TEXT("APRON-T") || A.Id == TEXT("APRON-9")); return true; }
		return false;
	};
	// the Babylon model's own lawn round the terminal lies 25 cm over the airfield's ground (Sink): what is laid inside it
	// (a road, the tower's lot) is lifted just over it there
	auto Lawn = [&](const FVector2f& Q) { return false && Q.X > 0.f; };   // (the model's lawn is left out now: the airfield's grass runs under the terminal)
	auto Lifted = [&](const FVector2f& Q, float Lift) { return Lawn(Q) ? Lift + 0.01f - Sink : Lift; };
	// the lots: their tarmac, the parking spaces' white lines (a line between every two, a line across their heads), soft edges
	for (const FAirfieldData::FLot& Lt : D.Lots)
	{
		const FVector2f Mid((Lt.X0 + Lt.X1) / 2, (Lt.Y0 + Lt.Y1) / 2);
		if (Lt.bPaved) Quad("asphalt", Lt.X0, Lt.Y0, Lt.X1, Lt.Y1, Lifted(Mid, -0.03f), 3.f, FLinearColor(1, 1, 1, 1));
		// on a model's own paving (the fire station's site, laid flat at 2 cm): the lines just over it
		auto PaintLift = [&]() { return Lt.bOnModel ? 0.035f - GroundAt(Mid.X, Mid.Y) - Sink : Lifted(Mid, -0.024f); };
		const float YHead = Lt.BayY + Lt.BayDir * Lt.BayD;   // the spaces run from BayY into the lot by their depth
		for (int32 b = 0; b <= Lt.Bays; ++b)
		{
			const float X = Lt.BayX0 + b * Lt.BayW;
			Quad("paintwhite", X - 0.06f, FMath::Min(Lt.BayY, YHead), X + 0.06f, FMath::Max(Lt.BayY, YHead), PaintLift(), 2.f);
		}
		Quad("paintwhite", Lt.BayX0, YHead - 0.06f, Lt.BayX0 + Lt.Bays * Lt.BayW, YHead + 0.06f, PaintLift(), 2.f);
		if (Lt.bPaved && !Lawn(Mid)) EdgeRect(Lt.X0, Lt.Y0, Lt.X1, Lt.Y1, -0.03f, 1.f, 15);
		ADP_LOG(TEXT("Airfield: lot %s with %d parking spaces"), *Lt.Id, Lt.Bays);
	}
	for (const FAirfieldData::FRoad& Rd : D.Roads)
	{
		// the centre line with its corners rounded (radius 15 m, or what the legs allow), a point every 1.5 m
		TArray<FVector2f> C;
		const TArray<FVector2f>& P = Rd.Pts;
		C.Add(P[0]);
		for (int32 i = 1; i + 1 < P.Num(); ++i)
		{
			const FVector2f A = P[i - 1], B = P[i], Cn = P[i + 1];
			const FVector2f U0 = (B - A).GetSafeNormal(), U1 = (Cn - B).GetSafeNormal();
			const float Turn = FMath::Acos(FMath::Clamp(FVector2f::DotProduct(U0, U1), -1.f, 1.f));
			if (Turn < 0.02f) { C.Add(B); continue; }
			const float T = FMath::Min3(15.f * FMath::Tan(Turn / 2), (B - A).Size() * 0.45f, (Cn - B).Size() * 0.45f);
			const FVector2f S0 = B - U0 * T, S1 = B + U1 * T;
			for (int32 k = 0; k <= 12; ++k) { const float t = k / 12.f; C.Add(FMath::Lerp(FMath::Lerp(S0, B, t), FMath::Lerp(B, S1, t), t)); }   // a smooth curve through the corner
		}
		C.Add(P.Last());
		TArray<FVector2f> Pts;
		for (int32 i = 0; i + 1 < C.Num(); ++i)
		{
			const int32 Nn = FMath::Max(1, FMath::CeilToInt((C[i + 1] - C[i]).Size() / 1.5f));
			for (int32 k = 0; k < Nn; ++k) Pts.Add(FMath::Lerp(C[i], C[i + 1], float(k) / Nn));
		}
		Pts.Add(C.Last());
		const int32 N = Pts.Num();
		TArray<FVector2f> Nrm; TArray<float> Along;
		Along.Add(0.f);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2f T = (Pts[FMath::Min(i + 1, N - 1)] - Pts[FMath::Max(i - 1, 0)]).GetSafeNormal();
			Nrm.Add(FVector2f(-T.Y, T.X));
			if (i > 0) Along.Add(Along.Last() + (Pts[i] - Pts[i - 1]).Size());
		}
		// a strip of the road between two offsets from its centre line, dashed or not
		auto Strip = [&](const FName Mat, float D0, float D1, float Lift, float DashOn, float DashOff)
		{
			const float StopAt = Rd.bHold && DashOn > 0.f ? Along.Last() - 18.f : 1e9f;   // the middle line ends before the STOP marking
			FSheet& S = Sheets.FindOrAdd(Mat);
			const float Period = DashOn + DashOff;
			for (int32 i = 0; i + 1 < N; ++i)
			{
				if (Period > 0.f && FMath::Fmod(Along[i], Period) > DashOn) continue;
				if (Along[i] > StopAt) continue;
				bool bM = false;
				const bool bOn = OnApron((Pts[i] + Pts[i + 1]) / 2, -0.5f, bM);
				if (bOn && Mat == TEXT("asphalt")) continue;                 // the apron is the road there
				const float L = bOn ? (bM ? 0.02f - Sink : -0.04f) + (Lift + 0.03f) + 0.002f : Lifted((Pts[i] + Pts[i + 1]) / 2, Lift);
				const int32 B0 = S.V.Num();
				for (const int32 k : {i, i + 1})
					for (const float Dd : {D0, D1}) { const FVector2f Q = Pts[k] + Nrm[k] * Dd; Vertex(S, Q.X, Q.Y, L); }
				Tri(S, B0, B0 + 1, B0 + 3); Tri(S, B0, B0 + 3, B0 + 2);
			}
		};
		const float H = Rd.W / 2;
		Strip("asphalt", -H, H, -0.03f, 0.f, 0.f);                         // the road
		for (const float S : {-1.f, 1.f}) Strip("paintwhite", S * (H - 0.45f), S * (H - 0.3f), -0.024f, 0.f, 0.f);   // its edge lines
		Strip("paintwhite", -0.06f, 0.06f, -0.024f, 3.f, 3.f);             // the dashed middle
		// the soft edges into the grass, both sides
		for (const float S : {-1.f, 1.f})
		{
			TArray<FVector2f> E, O, F; TArray<float> L;
			auto Flush = [&]() { if (E.Num() >= 2) EdgePoly(E, O, F, L, true); E.Reset(); O.Reset(); F.Reset(); L.Reset(); };
			for (int32 i = 0; i < N; ++i)
			{
				bool bM = false;
				const FVector2f Q = Pts[i] + Nrm[i] * (S * H);
				if (OnApron(Q, 1.f, bM)) { Flush(); continue; }   // none along an apron
				E.Add(Q); O.Add(Nrm[i] * S); F.Add(FVector2f(1.f, 0.f)); L.Add(Lifted(Q, -0.03f));
			}
			Flush();
		}
		// where it meets a taxiway: the road-holding position, a white stop line across it and STOP before it
		if (Rd.bHold)
		{
			const FVector2f Dir = (Pts[N - 1] - Pts[N - 2]).GetSafeNormal(), Nn(-Dir.Y, Dir.X), At = Pts.Last() - Dir * 10.f;
			FSheet& S = Sheets.FindOrAdd(TEXT("paintwhite"));
			const int32 B0 = S.V.Num();
			for (const float a : {0.f, 0.5f}) for (const float b : {-H + 0.3f, H - 0.3f}) { const FVector2f Q = At + Dir * a + Nn * b; Vertex(S, Q.X, Q.Y, -0.024f); }
			Tri(S, B0, B0 + 1, B0 + 3); Tri(S, B0, B0 + 3, B0 + 2);
			UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
			T->SetupAttachment(Root);
			T->SetText(FText::FromString(TEXT("STOP")));
			T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter);
			T->SetWorldSize(180.f); T->SetTextRenderColor(FColor(235, 235, 230));
			const FVector2f Tp = At - Dir * 3.f;
			// read by the driver coming up to it: plan direction (x, y) is world (x, -y)
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Dir.Y, Dir.X));
			T->SetRelativeLocationAndRotation(FTerminalPlan::ToWorld(Tp.X, GroundAt(Tp.X, Tp.Y) - 0.02f + Sink, Tp.Y), FRotator(90.f, Yaw + 180.f, 0.f));
			T->RegisterComponent();
			AddInstanceComponent(T);
		}
		ADP_LOG(TEXT("Airfield: road %s, %.0f m"), *Rd.Id, Along.Last());
	}
}

void AAirfield::EdgePoly(const TArray<FVector2f>& P, const TArray<FVector2f>& Out, const TArray<FVector2f>& Flags, const TArray<float>& InnerLift, bool bFadeEnds)
{
	FSheet& S = Sheets.FindOrAdd(TEXT("edge"));
	const int32 N = P.Num();
	if (N < 2) return;
	constexpr float In = 1.2f, Over = 1.4f, Fade = 6.f;
	TArray<float> Along;
	Along.Add(0.f);
	for (int32 k = 1; k < N; ++k) Along.Add(Along.Last() + (P[k] - P[k - 1]).Size());
	const float L = Along.Last();
	const float Across[3] = {-In, 0.f, Over}, Vs[3] = {0.f, 0.5f, 1.f};
	const int32 Base = S.V.Num();
	for (int32 j = 0; j < 3; ++j)
		for (int32 k = 0; k < N; ++k)
		{
			const FVector2f Q = P[k] + Out[k] * Across[j];
			const float Alpha = bFadeEnds ? FMath::Clamp(FMath::Min(Along[k], L - Along[k]) / Fade, 0.f, 1.f) : 1.f;
			const FVector2D UV(Along[k] / 4.0, Vs[j]);
			Vertex(S, Q.X, Q.Y, j < 2 ? InnerLift[k] + 0.004f : -0.076f, FLinearColor(Flags[k].X, Flags[k].Y, 0.f, Alpha), &UV);
		}
	for (int32 j = 0; j < 2; ++j)
		for (int32 k = 0; k + 1 < N; ++k)
		{
			const int32 Pa = Base + j * N + k, Pb = Pa + 1, Pc = Pa + N, Pd = Pc + 1;
			Tri(S, Pa, Pb, Pd); Tri(S, Pa, Pd, Pc);
		}
}

#pragma endregion

void AAirfield::Tri(FSheet& S, int32 A, int32 B, int32 C)
{
	// wound to face up (Unreal shows a triangle from the side its corners run clockwise on)
	const FVector N = FVector::CrossProduct(S.V[B] - S.V[A], S.V[C] - S.V[A]);
	if (N.Z < 0.f) { S.I.Add(A); S.I.Add(B); S.I.Add(C); } else { S.I.Add(A); S.I.Add(C); S.I.Add(B); }
}

void AAirfield::Quad(const FName Mat, float X0, float Y0, float X1, float Y1, float Lift, float Step, const FLinearColor& Col)
{
	FSheet& S = Sheets.FindOrAdd(Mat);
	const float Xa = FMath::Min(X0, X1), Xb = FMath::Max(X0, X1), Ya = FMath::Min(Y0, Y1), Yb = FMath::Max(Y0, Y1);
	const int32 Nx = FMath::Max(1, FMath::CeilToInt((Xb - Xa) / Step)), Ny = FMath::Max(1, FMath::CeilToInt((Yb - Ya) / Step));
	const int32 Base = S.V.Num();
	for (int32 j = 0; j <= Ny; ++j)
		for (int32 i = 0; i <= Nx; ++i)
			Vertex(S, FMath::Lerp(Xa, Xb, float(i) / Nx), FMath::Lerp(Ya, Yb, float(j) / Ny), Lift, Col);
	for (int32 j = 0; j < Ny; ++j)
		for (int32 i = 0; i < Nx; ++i)
		{
			const int32 A = Base + j * (Nx + 1) + i, B = A + 1, C = A + Nx + 1, Dd = C + 1;
			Tri(S, A, B, Dd); Tri(S, A, Dd, C);
		}
}

void AAirfield::Band(const FName Mat, const FVector2f& A, const FVector2f& B, float W, float Lift, float Step, const FLinearColor& Col)
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
			Vertex(S, P.X, P.Y, Lift, Col);
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
		else
		{
			// the airfield's ground on the ground master's own instances (Tools/set_ground.py), never the kit's (the kit's
			// asphalt, concrete, apron and grass also dress buildings); the terminal model's apron under the layout's apron
			static const TMap<FName, FName> Ground = {{TEXT("asphalt"), TEXT("gasphalt")}, {TEXT("concrete"), TEXT("gconcrete")}, {TEXT("apron"), TEXT("gapron")}, {TEXT("grass"), TEXT("ggrass")}};
			UMaterialInterface* Mat = nullptr;
			if (It.Key == TEXT("babylonApron")) Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terminal/Materials/MI_T_apron.MI_T_apron"));   // (the terminal's own, true Unreal)
			else if (It.Key == TEXT("standwear")) Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ground/M_StandWear.M_StandWear"));
			if (!Mat) Mat = ATerminalBuilder::KitMaterial(this, Ground.Contains(It.Key) ? Ground[It.Key] : It.Key);
			M->SetMaterial(0, Mat);
			if (It.Key == TEXT("standwear")) M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
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
	Quad("grass", D.Boundary.X0, D.Boundary.Y0, D.Boundary.X1, D.Boundary.Y1, -0.08f, 8.f);   // fine enough that it never pokes through the shoulders on the grade
	// landside in front of the terminal: the kerb, the road and the car park (level ground)
	if (!bModelSite) Quad("asphalt", -160, -170, 392, -0.5f, -0.05f, 50.f);
	// the aprons (concrete, level); with the Babylon model only those away from the terminal (it has the terminal's own)
	Aprons(bModelSite);   // every apron and every stand's markings and wear, from the layout
	ServiceRoads(bModelSite);
	// the taxiways: one smooth pavement with its turn curves, fillets, shoulders, edge stripes, centre lines and the soft
	// grass edge round all the paving, riding the grade
	PaveTaxiways(bModelSite);
	// the runway: shoulders, pavement, centre line dashes, edge lines, threshold bars, designators, touchdown marks
	const float RY = D.RwyY, HW = D.RwyW / 2;
	Quad("concrete", D.RwyX0 - 60, RY - HW - 7.5f, D.RwyX1 + 60, RY + HW + 7.5f, -0.025f);
	Quad("asphalt", D.RwyX0, RY - HW, D.RwyX1, RY + HW, -0.02f);
	for (float X = D.RwyX0 + 400; X < D.RwyX1 - 400; X += 50) Quad("paintwhite", X, RY - 0.45f, X + 30, RY + 0.45f, -0.008f);
	for (const float S : {-1.f, 1.f}) Quad("paintwhite", D.RwyX0, RY + S * (HW - 0.6f) - 0.45f, D.RwyX1, RY + S * (HW - 0.6f) + 0.45f, -0.008f);
	for (const float End : {0.f, 1.f})
	{
		const float X0 = End ? D.RwyX1 - 6 - 30 : D.RwyX0 + 6, Dir = End ? -1.f : 1.f;
		// the threshold's piano keys: 16 bars, 30 m long
		for (int32 k = 0; k < 16; ++k)
		{
			const float Y = RY - HW + 3 + k * ((D.RwyW - 6) / 16) + (k >= 8 ? 1.8f : 0);
			Quad("paintwhite", X0, Y, X0 + 30, Y + 1.8f, -0.008f);
		}
		// the touchdown zone marks
		for (int32 k = 0; k < 6; ++k)
		{
			const float X = (End ? D.RwyX1 : D.RwyX0) + Dir * (400 + k * 150);
			for (const float S : {-1.f, 1.f}) Quad("paintwhite", X, RY + S * 10 - 1.5f, X + Dir * 22.5f, RY + S * 10 + 1.5f, -0.008f);
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
	// the runway-holding positions, as painted on real airports (reference photos: Boston, Mumbai, FAA AC 150/5340-1M; ICAO
	// Annex 14 5.2.10 pattern A): four yellow lines across the taxiway, 30 cm wide and 15 cm apart, the two solid ones on
	// the holding side, the two dashed ones (0.9 m dashes, 0.9 m gaps) on the runway side; the enhanced centre line for
	// the last 45 m before it (dashed yellow lines either side of the centre line, 0.15 m wide and 0.15 m off it, 2.7 m
	// dashes, 0.9 m gaps); and the mandatory instruction markings (ICAO 5.2.16): the runway's designators in white on red
	// reaching 0.5 m beyond the letters, one each side of the centre line (which runs on between them), their near edge
	// 0.9 m from the hold line, read by the pilot coming up to it, the designator of the runway end on the pilot's left first
	for (const FVector2f& H : D.Holds)
	{
		const float S = D.RwyY > H.Y ? 1.f : -1.f;   // towards the runway
		const float Half = 12.5f, LW = 0.30f, Gap = 0.15f, Depth = 4 * LW + 3 * Gap;
		Quad("paintblack", H.X - Half - 0.3f, H.Y - Depth / 2 - 0.3f, H.X + Half + 0.3f, H.Y + Depth / 2 + 0.3f, -0.0203f, 2.f);   // on black, as on the real ones
		for (int32 k = 0; k < 4; ++k)
		{
			const float Y0 = H.Y + S * (-Depth / 2 + k * (LW + Gap)), Y1 = Y0 + S * LW;
			if (k < 2) Quad("paintyellow", H.X - Half, Y0, H.X + Half, Y1, -0.018f, 2.f);
			else for (float X = H.X - Half; X < H.X + Half - 0.05f; X += 1.8f) Quad("paintyellow", X, Y0, FMath::Min(X + 0.9f, H.X + Half), Y1, -0.018f, 2.f);
		}
		const float Hold0 = H.Y - S * Depth / 2;   // the hold line's holding-side edge
		for (const float Side : {-1.f, 1.f})
			for (float A = 0.f; A < 45.f; A += 3.6f)
			{
				const float Ya = Hold0 - S * A, Yb = Hold0 - S * FMath::Min(A + 2.7f, 45.f);
				Quad("paintyellow", H.X + Side * 0.30f - 0.075f, FMath::Min(Ya, Yb), H.X + Side * 0.30f + 0.075f, FMath::Max(Ya, Yb), -0.018f, 3.f);
			}
		const FString Txt = S > 0.f ? TEXT("08-26") : TEXT("26-08");   // coming up from the south (+y), the 08 end is on the left
		constexpr float TextH = 3.2f, Margin = 0.5f, TextW = 9.6f;
		const float Near = Hold0 - S * 0.9f, Far = Near - S * (TextH + 2 * Margin), Cy = (Near + Far) / 2;
		for (const float Side : {-1.f, 1.f})
		{
			const float Cx = H.X + Side * (0.9f + TextW / 2 + Margin);
			Quad("paintblack", Cx - TextW / 2 - Margin - 0.25f, FMath::Min(Near, Far) - 0.25f, Cx + TextW / 2 + Margin + 0.25f, FMath::Max(Near, Far) + 0.25f, -0.019f, 2.f);   // edged in black
			Quad("paintred", Cx - TextW / 2 - Margin, FMath::Min(Near, Far), Cx + TextW / 2 + Margin, FMath::Max(Near, Far), -0.0165f, 2.f);
			UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
			T->SetupAttachment(Root);
			T->SetText(FText::FromString(Txt));
			T->SetHorizontalAlignment(EHTA_Center);
			T->SetVerticalAlignment(EVRTA_TextCenter);
			T->SetWorldSize(TextH * 100.f);
			T->SetTextRenderColor(FColor(235, 235, 230));
			const FVector At = FTerminalPlan::ToWorld(Cx, GroundAt(Cx, Cy) - 0.012f + Sink, Cy);
			// the plan's +y is the world's -y; the letters' feet towards the pilot coming up to the line
			T->SetRelativeLocationAndRotation(At, FRotator(90.f, S > 0.f ? 90.f : -90.f, 0.f));   // face up, the letters' feet towards the pilot
			T->RegisterComponent();
			AddInstanceComponent(T);
		}
	}
	// each hangar's lead-in line: from taxilane K straight to the apron's edge on the hangar's centre line, where the
	// hangar's own line carries on into the hall (whatever its size, its centre line is this line)
	if (const FAirfieldData::FTwy* K = D.Find(TEXT("K")))
		for (const FAirfieldData::FHangarSite& H : D.Hangars) Band("paintyellow", FVector2f(H.X, K->Pts[0].Y), FVector2f(H.X, H.Y), 0.3f, -0.028f);
	EdgeBands(bModelSite);
	FlushSheets();
	// the ground material's tyre rubber follows the runway (Tools/set_ground.py): its centre line's ends in world cm (z:
	// half its width) and its displaced thresholds (250 m each end, Data/airfield.json)
	if (UMaterialParameterCollection* Mpc = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Weather/MPC_Weather.MPC_Weather")))
	{
		const FVector A = FTerminalPlan::ToWorld(D.RwyX0, 0.f, D.RwyY), B = FTerminalPlan::ToWorld(D.RwyX1, 0.f, D.RwyY);
		UKismetMaterialLibrary::SetVectorParameterValue(this, Mpc, TEXT("RwyA"), FLinearColor(A.X, A.Y, D.RwyW * 50.f, 0.f));
		UKismetMaterialLibrary::SetVectorParameterValue(this, Mpc, TEXT("RwyB"), FLinearColor(B.X, B.Y, D.RwyW * 50.f, 0.f));
		UKismetMaterialLibrary::SetVectorParameterValue(this, Mpc, TEXT("RwyDisp"), FLinearColor(25000.f, 25000.f, 0.f, 0.f));
		ADP_LOG(TEXT("Airfield: the ground's runway for its tyre rubber %s -> %s"), *A.ToString(), *B.ToString());
	}
	// the cargo building
	Slab("liftClad", D.Cargo.X0, D.Cargo.Y0, D.Cargo.X1, D.Cargo.Y1, 0, 18);
	ADP_LOG(TEXT("Airfield built: %d taxiways, %d stands"), D.Taxiways.Num(), D.Stands.Num());
	return true;
}

#pragma endregion
