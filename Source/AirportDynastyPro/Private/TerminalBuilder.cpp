// Builds the terminal at run time from the shared layout: floors, walls with door openings, glass walls (frames and
// clear panes), columns, stairs, lifts and stand markings, in the Android edition's materials for each kind of room.
#include "TerminalBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#pragma region Sizes and materials
namespace
{
	constexpr float WallT = 0.2f;    // wall thickness, m
	constexpr float SlabT = 0.3f;    // floor slab thickness, m
	constexpr float MullionStep = 1.5f;
	constexpr float MullionW = 0.08f;
	constexpr float PaneT = 0.024f;  // glass pane thickness, m

	// the floor and wall of each kind of room, as in the Android edition (shell.ts FLOOR_M and WALL_M)
	FName FloorMat(const FString& Kind)
	{
		static const TMap<FString, FName> M = {
			{TEXT("hall"), "terrazzo"}, {TEXT("vestibule"), "terrazzo"}, {TEXT("plant"), "epoxy"}, {TEXT("wc"), "tile"},
			{TEXT("clean"), "epoxy"}, {TEXT("security"), "terrazzo"}, {TEXT("airside"), "terrazzo"}, {TEXT("lounge"), "herring"},
			{TEXT("office"), "carpetGrey"}, {TEXT("passage"), "tile"}, {TEXT("ops"), "carpetBlue"}, {TEXT("crew"), "carpetGrey"},
			{TEXT("corridor"), "tile"}, {TEXT("academy"), "carpetBlue"}, {TEXT("staffentr"), "tileDark"}, {TEXT("baghall"), "concrete"},
			{TEXT("ramp"), "concrete"}, {TEXT("gate1"), "carpetBlue"}, {TEXT("link"), "terrazzo"}, {TEXT("food"), "terrazzo"},
			{TEXT("landing"), "terrazzo"}, {TEXT("taxfree"), "tile"}, {TEXT("family"), "carpet"}, {TEXT("bar"), "herring"},
			{TEXT("walk"), "terrazzo"}, {TEXT("eatery"), "tileDark"}, {TEXT("gate2"), "carpetBlue"}, {TEXT("view"), "carpetBlue"}};
		const FName* F = M.Find(Kind);
		return F ? *F : FName("terrazzo");
	}
	FName WallMat(const FString& Kind)
	{
		static const TMap<FString, FName> M = {
			{TEXT("hall"), "white"}, {TEXT("plant"), "concrete"}, {TEXT("baghall"), "concrete"}, {TEXT("ramp"), "concrete"},
			{TEXT("wc"), "tile"}, {TEXT("eatery"), "tileDark"}, {TEXT("clean"), "concrete"}, {TEXT("lounge"), "plaster"}};
		const FName* W = M.Find(Kind);
		return W ? *W : FName("plaster");
	}
	bool RoughCeiling(const FString& Kind) { return Kind == TEXT("baghall") || Kind == TEXT("plant") || Kind == TEXT("ramp"); }

	constexpr float LiftHole = 1.02f; // half the size of the hole round each lift in Floor 2's slab and the ceilings (lifts.ts)

	struct FRect { float X0, Z0, X1, Z1; };
	/** A rectangle less the holes in it, as smaller rectangles (shell.ts rectMinus). */
	TArray<FRect> RectMinus(const FRect& R, const TArray<FRect>& Holes)
	{
		TArray<FRect> Hs;
		for (const FRect& H : Holes)
			if (H.X1 > R.X0 && H.X0 < R.X1 && H.Z1 > R.Z0 && H.Z0 < R.Z1) Hs.Add(H);
		if (!Hs.Num()) return {R};
		TArray<float> Xs = {R.X0, R.X1}, Zs = {R.Z0, R.Z1};
		for (const FRect& H : Hs)
		{
			Xs.Add(FMath::Max(R.X0, H.X0)); Xs.Add(FMath::Min(R.X1, H.X1));
			Zs.Add(FMath::Max(R.Z0, H.Z0)); Zs.Add(FMath::Min(R.Z1, H.Z1));
		}
		Xs.Sort(); Zs.Sort();
		TArray<FRect> Out;
		for (int32 i = 0; i + 1 < Xs.Num(); ++i)
			for (int32 j = 0; j + 1 < Zs.Num(); ++j)
			{
				const FRect C{Xs[i], Zs[j], Xs[i + 1], Zs[j + 1]};
				if (C.X1 - C.X0 < 0.01f || C.Z1 - C.Z0 < 0.01f) continue;
				const float MX = (C.X0 + C.X1) / 2, MZ = (C.Z0 + C.Z1) / 2;
				if (Hs.ContainsByPredicate([&](const FRect& H) { return MX > H.X0 && MX < H.X1 && MZ > H.Z0 && MZ < H.Z1; })) continue;
				Out.Add(C);
			}
		return Out;
	}
	/** The stairs' and lifts' openings through Floor 2 and the ceilings. */
	TArray<FRect> Openings(const FTerminalPlan& Plan)
	{
		TArray<FRect> H;
		for (const FPlanStair& S : Plan.Stairs) H.Add({S.X0, S.Z0, S.X1, S.Z1});
		for (const FPlanPoint& L : Plan.Lifts) H.Add({L.X - LiftHole, L.Z - LiftHole, L.X + LiftHole, L.Z + LiftHole});
		return H;
	}

	// a plain colour for a kit material, used only when its material asset is missing
	FLinearColor Fallback(const FName Name)
	{
		static const TMap<FName, FLinearColor> C = {
			{"asphalt", FLinearColor(0.02f, 0.02f, 0.025f)}, {"apron", FLinearColor(0.26f, 0.26f, 0.25f)},
			{"concrete", FLinearColor(0.48f, 0.47f, 0.44f)}, {"carpetBlue", FLinearColor(0.03f, 0.05f, 0.1f)},
			{"carpetGrey", FLinearColor(0.1f, 0.11f, 0.13f)}, {"tileDark", FLinearColor(0.04f, 0.05f, 0.06f)},
			{"herring", FLinearColor(0.54f, 0.32f, 0.15f)}, {"yellow", FLinearColor(0.89f, 0.54f, 0.03f)},
			{"steel", FLinearColor(0.32f, 0.36f, 0.41f)}, {"epoxy", FLinearColor(0.2f, 0.24f, 0.28f)}};
		const FLinearColor* F = C.Find(Name);
		return F ? *F : FLinearColor(0.82f, 0.82f, 0.8f);
	}
}
#pragma endregion

#pragma region Setup

ATerminalBuilder::ATerminalBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UInstancedStaticMeshComponent* ATerminalBuilder::MakeKit(const FName Key, UStaticMesh* Mesh, UMaterialInterface* Material)
{
	UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this, Key);
	K->SetupAttachment(Root);
	K->SetStaticMesh(Mesh);
	K->SetMaterial(0, Material);
	K->SetCollisionProfileName(TEXT("BlockAll"));
	K->RegisterComponent();
	AddInstanceComponent(K);
	Kits.Add(Key, K);
	return K;
}

UMaterialInterface* ATerminalBuilder::KitMaterial(UObject* Outer, const FName Name)
{
	const FString Path = FString::Printf(TEXT("/Game/Materials/Kit/MI_%s.MI_%s"), *Name.ToString(), *Name.ToString());
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *Path)) return M;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Base) return nullptr;
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, Outer);
	D->SetVectorParameterValue(TEXT("Color"), Fallback(Name));
	return D;
}

UInstancedStaticMeshComponent* ATerminalBuilder::Mat(const FName Name)
{
	const FName Key(*(TEXT("box_") + Name.ToString()));
	if (TObjectPtr<UInstancedStaticMeshComponent>* K = Kits.Find(Key)) return *K;
	return MakeKit(Key, Cube, KitMaterial(this, Name));
}

UInstancedStaticMeshComponent* ATerminalBuilder::MatCyl(const FName Name)
{
	const FName Key(*(TEXT("cyl_") + Name.ToString()));
	if (TObjectPtr<UInstancedStaticMeshComponent>* K = Kits.Find(Key)) return *K;
	return MakeKit(Key, Cylinder, KitMaterial(this, Name));
}

void ATerminalBuilder::Box(UInstancedStaticMeshComponent* K, float X0, float Y0, float Z0, float X1, float Y1, float Z1)
{
	const FVector A = FTerminalPlan::ToWorld(FMath::Min(X0, X1), FMath::Min(Y0, Y1), FMath::Min(Z0, Z1));
	const FVector B = FTerminalPlan::ToWorld(FMath::Max(X0, X1), FMath::Max(Y0, Y1), FMath::Max(Z0, Z1));
	const FVector Size = (B - A).GetAbs();
	if (Size.X < 1.f || Size.Y < 1.f || Size.Z < 1.f) return;
	// the engine cube is 100 cm across with its pivot in the middle
	K->AddInstance(FTransform(FRotator::ZeroRotator, (A + B) * 0.5f, Size / 100.f));
}

#pragma endregion

#pragma region Build

bool ATerminalBuilder::Build()
{
	for (auto& Pair : Kits) if (Pair.Value) Pair.Value->DestroyComponent();
	Kits.Reset();

	Plan = FTerminalPlan();
	if (!Plan.Load()) return false;

	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!Cube || !Cylinder) return false;

	// the glass panes: clear glass (Content/Materials/M_Glass), no shadow so daylight passes through
	UMaterialInterface* GlassM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glass.M_Glass"));
	MakeKit("glass", Cube, GlassM ? GlassM : KitMaterial(this, "white"))->SetCastShadow(false);

	// apron and landside ground around the whole plan
	float MinX = 1e9f, MaxX = -1e9f, MinZ = 1e9f, MaxZ = -1e9f;
	for (const FPlanRoom& R : Plan.Rooms)
	{
		MinX = FMath::Min(MinX, R.X0); MaxX = FMath::Max(MaxX, R.X1);
		MinZ = FMath::Min(MinZ, R.Z0); MaxZ = FMath::Max(MaxZ, R.Z1);
	}
	Box(Mat("apron"), MinX - 400, -0.52f, MinZ - 300, MaxX + 400, -0.02f, MaxZ + 400);

	for (const FPlanRoom& R : Plan.Rooms) BuildRoom(R);
	for (const FPlanStair& S : Plan.Stairs) BuildStair(S);

	for (const FPlanPoint& C : Plan.Columns)
	{
		const FVector P = FTerminalPlan::ToWorld(C.X, Plan.Roof * 0.5f, C.Z);
		MatCyl("white")->AddInstance(FTransform(FRotator::ZeroRotator, P, FVector(0.6f, 0.6f, Plan.Roof)));
	}
	for (const FPlanPoint& S : Plan.Stands)
		Box(Mat("yellow"), S.X - 0.15f, 0, S.Z, S.X + 0.15f, 0.02f, S.Z + 45.f); // stand lead-in line from the nose position

	UE_LOG(LogTemp, Log, TEXT("Terminal built: %d rooms, %d doors"), Plan.Rooms.Num(), Plan.Doors.Num());
	return true;
}

#pragma endregion

#pragma region Rooms and walls

void ATerminalBuilder::BuildRoom(const FPlanRoom& R)
{
	const float Y0 = Plan.FloorY(R.Floor), Y1 = Plan.CeilY(R.Floor);
	const TArray<FRect> Holes = Openings(Plan);
	// the floor (on Floor 2 less the stairs and lifts going through it) and the ceiling (less them on both floors)
	const FRect Whole{R.X0, R.Z0, R.X1, R.Z1};
	for (const FRect& C : R.Floor >= 2 ? RectMinus(Whole, Holes) : TArray<FRect>{Whole})
		Box(Mat(FloorMat(R.Kind)), C.X0, Y0 - SlabT, C.Z0, C.X1, Y0, C.Z1);
	for (const FRect& C : RectMinus(Whole, Holes))
		Box(Mat(RoughCeiling(R.Kind) ? FName("concrete") : FName("offwhite")), C.X0, Y1, C.Z0, C.X1, Y1 + 0.1f, C.Z1);
	if (R.Floor >= 2 || R.Kind == TEXT("hall")) Box(Mat("white"), R.X0, Plan.Roof, R.Z0, R.X1, Plan.Roof + 0.4f, R.Z1);

	BuildWall(R, true,  R.Z0, R.X0, R.X1, R.Glass.Contains(TEXT("s")));
	BuildWall(R, true,  R.Z1, R.X0, R.X1, R.Glass.Contains(TEXT("n")));
	BuildWall(R, false, R.X0, R.Z0, R.Z1, R.Glass.Contains(TEXT("w")));
	BuildWall(R, false, R.X1, R.Z0, R.Z1, R.Glass.Contains(TEXT("e")));
}

void ATerminalBuilder::BuildWall(const FPlanRoom& R, bool bAlongX, float Fixed, float From, float To, bool bGlass)
{
	const float Y0 = Plan.FloorY(R.Floor), Y1 = Plan.CeilY(R.Floor);
	UInstancedStaticMeshComponent* WallK = Mat(WallMat(R.Kind));
	UInstancedStaticMeshComponent* FrameK = Mat("mullion");
	UInstancedStaticMeshComponent* GlassK = Kits.FindRef("glass");

	// door openings on this wall line: from, to and the opening's height
	TArray<FVector3f> Gaps;
	for (const FPlanDoor& D : Plan.Doors)
	{
		if (D.Floor != R.Floor || D.bAlongX != bAlongX) continue;
		const float Line = bAlongX ? D.Z : D.X, At = bAlongX ? D.X : D.Z;
		if (FMath::Abs(Line - Fixed) > 0.35f || At < From || At > To) continue;
		Gaps.Add(FVector3f(At - D.Width * 0.5f, At + D.Width * 0.5f, D.Height()));
	}
	Gaps.Sort([](const FVector3f& A, const FVector3f& B) { return A.X < B.X; });

	auto Piece = [&](UInstancedStaticMeshComponent* K, float A, float B, float Bottom, float Top, float T)
	{
		if (B - A < 0.01f) return;
		if (bAlongX) Box(K, A, Bottom, Fixed - T * 0.5f, B, Top, Fixed + T * 0.5f);
		else         Box(K, Fixed - T * 0.5f, Bottom, A, Fixed + T * 0.5f, Top, B);
	};
	auto Solid = [&](float A, float B)
	{
		if (!bGlass) { Piece(WallK, A, B, Y0, Y1, WallT); return; }
		// glass wall: sill, head, slim mullions and one pane between each pair of mullions
		Piece(FrameK, A, B, Y0, Y0 + 0.1f, WallT);
		Piece(FrameK, A, B, Y1 - 0.15f, Y1, WallT);
		for (float M = A; M <= B; M += MullionStep)
		{
			Piece(FrameK, M - MullionW * 0.5f, M + MullionW * 0.5f, Y0, Y1, WallT);
			if (GlassK) Piece(GlassK, M + MullionW * 0.5f, FMath::Min(M + MullionStep, B) - MullionW * 0.5f, Y0 + 0.1f, Y1 - 0.15f, PaneT);
		}
	};

	float Cursor = From;
	for (const FVector3f& G : Gaps)
	{
		Solid(Cursor, G.X);
		Piece(bGlass ? FrameK : WallK, G.X, G.Y, Y0 + G.Z, Y1, WallT); // lintel over the opening
		Cursor = FMath::Max(Cursor, G.Y);
	}
	Solid(Cursor, To);
}

#pragma endregion

#pragma region Stairs

void ATerminalBuilder::BuildStair(const FPlanStair& S)
{
	const int32 N = FMath::Max(1, FMath::RoundToInt(Plan.F2Y / 0.18f));
	const float Rise = Plan.F2Y / N;
	const bool bX = S.Dir.StartsWith(TEXT("x"));
	const bool bUp = S.Dir.EndsWith(TEXT("+"));
	const float A0 = bX ? S.X0 : S.Z0, A1 = bX ? S.X1 : S.Z1;
	const float Run = (A1 - A0) / N;
	// the steps, oak and walnut in turn as in the Android edition
	for (int32 i = 0; i < N; ++i)
	{
		const int32 k = bUp ? i : N - 1 - i; // step k rises to (k+1)*Rise
		const float From = A0 + Run * i, To = From + Run;
		const float Top = (k + 1) * Rise;
		UInstancedStaticMeshComponent* StepK = Mat(k % 2 ? FName("oak") : FName("walnut"));
		if (bX) Box(StepK, From, 0, S.Z0, To, Top, S.Z1);
		else    Box(StepK, S.X0, 0, From, S.X1, Top, To);
	}

	// the glass sides and handrails, straight along the slope from the bottom to the top
	const float Len = FMath::Abs(A1 - A0), Wid = bX ? S.Z1 - S.Z0 : S.X1 - S.X0;
	const float Slope = FMath::RadiansToDegrees(FMath::Atan2(Plan.F2Y, Len));
	const float Along = FMath::Sqrt(Len * Len + Plan.F2Y * Plan.F2Y);
	UInstancedStaticMeshComponent* GlassK = Kits.FindRef("glass");
	for (const float Lat : {0.06f, Wid - 0.06f})
	{
		const float MidA = (A0 + A1) / 2;
		const float PX = bX ? MidA : S.X0 + Lat, PZ = bX ? S.Z0 + Lat : MidA;
		// pitch up towards the top: in Unreal x is the plan's x and -y the plan's z
		const float Yaw = bX ? (bUp ? 0.f : 180.f) : (bUp ? -90.f : 90.f);
		const FRotator Rot(Slope, Yaw, 0);
		if (GlassK) GlassK->AddInstance(FTransform(Rot, FTerminalPlan::ToWorld(PX, Plan.F2Y / 2 + 0.58f, PZ), FVector(Along / 1.f, 0.024f, 0.95f)));
		Mat("chrome")->AddInstance(FTransform(Rot, FTerminalPlan::ToWorld(PX, Plan.F2Y / 2 + 1.07f, PZ), FVector(Along, 0.06f, 0.06f)));
	}

	// the opening's glass balustrade on Floor 2, on every side but the top of the stairs, where you step off
	const TCHAR TopSide = bX ? (bUp ? TEXT('e') : TEXT('w')) : (bUp ? TEXT('n') : TEXT('s'));
	const float Y = Plan.F2Y;
	for (const TCHAR Side : {TEXT('n'), TEXT('s'), TEXT('e'), TEXT('w')})
	{
		if (Side == TopSide) continue;
		const bool bAx = Side == TEXT('n') || Side == TEXT('s');
		const float Line = Side == TEXT('n') ? S.Z1 : Side == TEXT('s') ? S.Z0 : Side == TEXT('e') ? S.X1 : S.X0;
		const float A = bAx ? S.X0 : S.Z0, B = bAx ? S.X1 : S.Z1;
		if (bAx)
		{
			if (GlassK) Box(GlassK, A, Y, Line - 0.02f, B, Y + 1.05f, Line + 0.02f);
			Box(Mat("chrome"), A, Y + 1.05f, Line - 0.035f, B, Y + 1.1f, Line + 0.035f);
		}
		else
		{
			if (GlassK) Box(GlassK, Line - 0.02f, Y, A, Line + 0.02f, Y + 1.05f, B);
			Box(Mat("chrome"), Line - 0.035f, Y + 1.05f, A, Line + 0.035f, Y + 1.1f, B);
		}
	}
}

#pragma endregion
