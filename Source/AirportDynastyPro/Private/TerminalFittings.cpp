// The fittings of the Grand Hall and the gates, placed exactly as in the Android edition's furnish.ts (same positions,
// sizes and materials), all as instanced pieces.
#include "TerminalFittings.h"
#include "TerminalBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"

#pragma region Setup

ATerminalFittings::ATerminalFittings()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UInstancedStaticMeshComponent* ATerminalFittings::Kit(EShape Shape, const FName Mat)
{
	static const TCHAR* Names[] = {TEXT("box"), TEXT("cyl"), TEXT("ball")};
	const FName Key(*FString::Printf(TEXT("%s_%s"), Names[uint8(Shape)], *Mat.ToString()));
	if (TObjectPtr<UInstancedStaticMeshComponent>* K = Kits.Find(Key)) return *K;
	UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this, Key);
	K->SetupAttachment(Root);
	K->SetStaticMesh(Meshes[uint8(Shape)]);
	UMaterialInterface* Glass = Mat == "glass" ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glass.M_Glass")) : nullptr;
	K->SetMaterial(0, Glass ? Glass : ATerminalBuilder::KitMaterial(this, Mat));
	if (Glass) K->SetCastShadow(false);
	K->SetCollisionProfileName(TEXT("BlockAll"));
	K->RegisterComponent();
	AddInstanceComponent(K);
	Kits.Add(Key, K);
	return K;
}

void ATerminalFittings::Part(const FAt& O, EShape Shape, const FName Mat, float W, float H, float D, float X, float Y, float Z)
{
	if (W <= 0 || H <= 0 || D <= 0) return;
	// the object's frame turned by its yaw as in the Android edition: local x goes to (cos, -sin), local z to (sin, cos)
	const float C = FMath::Cos(O.Yaw), S = FMath::Sin(O.Yaw);
	const float PX = O.X + X * C + Z * S, PZ = O.Z - X * S + Z * C;
	const FVector At = FTerminalPlan::ToWorld(PX, O.Y + Y, PZ);
	// the engine shapes are 100 cm across with their pivot in the middle
	Kit(Shape, Mat)->AddInstance(FTransform(FRotator(0, FMath::RadiansToDegrees(O.Yaw), 0), At, FVector(W, D, H)));
}

void ATerminalFittings::Text(const FAt& O, const FString& Words, float X, float Y, float Z, float Facing, float Size, FColor Colour)
{
	const float C = FMath::Cos(O.Yaw), S = FMath::Sin(O.Yaw);
	const float PX = O.X + X * C + Z * S, PZ = O.Z - X * S + Z * C;
	// the way the words face, in the plan (the object's forward turned by its yaw), and in Unreal (plan z is -Y)
	const float FX = Facing * S, FZ = Facing * C;
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
	T->SetupAttachment(Root);
	T->SetText(FText::FromString(Words));
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetWorldSize(Size * 100.f);
	T->SetTextRenderColor(Colour);
	T->SetRelativeLocationAndRotation(FTerminalPlan::ToWorld(PX, O.Y + Y, PZ), FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(-FZ, FX)), 0));
	T->RegisterComponent();
	AddInstanceComponent(T);
}

#pragma endregion

#pragma region Objects

void ATerminalFittings::OfficeChair(const FAt& O)
{
	Box(O, "fabricGrey", 0.5f, 0.09f, 0.48f, 0, 0.48f, 0);
	Box(O, "fabricGrey", 0.48f, 0.62f, 0.07f, 0, 0.85f, -0.22f);
	Cyl(O, "chrome", 0.03f, 0.4f, 0, 0.25f, 0);
	Cyl(O, "black", 0.3f, 0.04f, 0, 0.05f, 0);
}

void ATerminalFittings::Seats(const FAt& O, int32 N)
{
	const float W = 0.62f, L = N * W;
	Box(O, "steel", L, 0.06f, 0.08f, 0, 0.32f, 0);
	for (const float SX : {-L / 2 + 0.2f, L / 2 - 0.2f}) Box(O, "steel", 0.06f, 0.32f, 0.5f, SX, 0.16f, 0);
	for (int32 i = 0; i < N; ++i)
	{
		const float SX = -L / 2 + W * (i + 0.5f);
		Box(O, "navy", W - 0.06f, 0.07f, 0.48f, SX, 0.44f, 0);
		Box(O, "navy", W - 0.06f, 0.48f, 0.06f, SX, 0.72f, -0.24f);
	}
}

void ATerminalFittings::Plant(const FAt& O, bool bBig)
{
	if (bBig)
	{
		Cyl(O, "black", 0.46f, 0.6f, 0, 0.3f, 0);
		for (int32 i = 0; i < 9; ++i)
		{
			const float A = i * 2.4f;
			Ball(O, "plantDark", 0.18f, FMath::Sin(A) * 0.2f, 0.9f + (i % 3) * 0.32f, FMath::Cos(A) * 0.2f, 2.6f);
		}
	}
	else
	{
		Cyl(O, "white", 0.22f, 0.45f, 0, 0.22f, 0);
		Ball(O, "plant", 0.34f, 0, 0.75f, 0, 1.2f);
	}
}

void ATerminalFittings::TreePlanter(const FAt& O)
{
	Cyl(O, "white", 2.22f, 0.75f, 0, 0.375f, 0);
	Cyl(O, "soil", 2.1f, 0.06f, 0, 0.74f, 0);
	Cyl(O, "trunk", 0.14f, 3.2f, 0, 2.2f, 0);
	Ball(O, "plant", 1.9f, 0, 4.6f, 0, 1.15f);
}

void ATerminalFittings::Bin(const FAt& O)
{
	Cyl(O, "steel", 0.2f, 0.8f, 0, 0.4f, 0);
	Cyl(O, "black", 0.21f, 0.04f, 0, 0.8f, 0);
}

void ATerminalFittings::Sign(const FAt& O, const FString& Words, float W, float H, const FName Back, FColor Colour)
{
	// a sign board facing the object's forward, its words on the front
	Box(O, Back, W, H, 0.04f, 0, 0, 0);
	Text(O, Words, 0, 0, 0.03f, 1.f, H * 0.5f, Colour);
}

void ATerminalFittings::Chair(const FAt& O, const FName Mat)
{
	Box(O, Mat, 0.46f, 0.07f, 0.44f, 0, 0.46f, 0);
	Box(O, Mat, 0.46f, 0.5f, 0.06f, 0, 0.74f, -0.2f);
	for (const float SX : {-0.2f, 0.2f}) for (const float SZ : {-0.18f, 0.18f}) Box(O, "steel", 0.03f, 0.45f, 0.03f, SX, 0.22f, SZ);
}

void ATerminalFittings::Armchair(const FAt& O, const FName Mat)
{
	Box(O, Mat, 0.8f, 0.22f, 0.76f, 0, 0.32f, 0);
	Box(O, Mat, 0.8f, 0.5f, 0.15f, 0, 0.62f, -0.31f);
	for (const float S : {-1.f, 1.f}) Box(O, Mat, 0.13f, 0.32f, 0.76f, S * 0.4f, 0.52f, 0);
	for (const float SX : {-0.33f, 0.33f}) for (const float SZ : {-0.3f, 0.3f}) Cyl(O, "brass", 0.02f, 0.22f, SX, 0.11f, SZ);
}

void ATerminalFittings::Sofa(const FAt& O, float Len, const FName Mat)
{
	Box(O, Mat, Len, 0.42f, 0.9f, 0, 0.21f, 0);
	Box(O, Mat, Len, 0.5f, 0.2f, 0, 0.62f, -0.36f);
	for (const float S : {-1.f, 1.f}) Box(O, Mat, 0.18f, 0.62f, 0.9f, S * Len / 2, 0.31f, 0);
}

void ATerminalFittings::Table(const FAt& O, float Rad, float H, const FName Top)
{
	Cyl(O, Top, Rad, 0.04f, 0, H, 0);
	Cyl(O, "black", 0.05f, H, 0, H / 2, 0);
	Cyl(O, "black", 0.25f, 0.03f, 0, 0.015f, 0);
}

void ATerminalFittings::Desk(const FAt& O, float W, float D, const FName Top, int32 Screens)
{
	Box(O, Top, W, 0.04f, D, 0, 0.74f, 0);
	for (const float S : {-1.f, 1.f}) Box(O, "white", 0.04f, 0.72f, D - 0.06f, S * (W / 2 - 0.03f), 0.36f, 0);
	Box(O, "white", W - 0.1f, 0.4f, 0.02f, 0, 0.5f, -D / 2 + 0.03f);
	for (int32 i = 0; i < Screens; ++i)
	{
		const float SX = (i - (Screens - 1) / 2.f) * 0.58f;
		Box(O, "black", 0.54f, 0.32f, 0.03f, SX, 1.02f, -D / 2 + 0.2f);
		Box(O, "screenPanel", 0.5f, 0.28f, 0.01f, SX, 1.02f, -D / 2 + 0.22f);
		Box(O, "black", 0.05f, 0.16f, 0.05f, SX, 0.84f, -D / 2 + 0.18f);
	}
	Box(O, "black", 0.44f, 0.02f, 0.15f, 0, 0.77f, 0.08f);
}

void ATerminalFittings::Shelf(const FAt& O, float W, float H, float D)
{
	static const FName Goods[] = {"red", "navy", "yellow", "gold", "tealUI", "offwhite"};
	Box(O, "white", W, H, 0.04f, 0, H / 2, 0);
	for (int32 i = 0; i < 4; ++i) Box(O, "white", W, 0.03f, D, 0, 0.15f + i * (H - 0.25f) / 3, 0);
	for (int32 i = 0; i < 4; ++i)
		for (int32 j = 0; j < 6; ++j)
			Box(O, Goods[(i * 3 + j) % 6], 0.22f, 0.24f + ((i + j) % 3) * 0.06f, 0.2f, -W / 2 + 0.2f + j * (W - 0.4f) / 5, 0.29f + i * (H - 0.25f) / 3, D * 0.15f);
}

void ATerminalFittings::Counter(const FAt& O, float W, float D, const FName Front, const FName Top, float H)
{
	Box(O, Front, W, H, D, 0, H / 2, 0);
	Box(O, Top, W + 0.06f, 0.05f, D + 0.06f, 0, H + 0.025f, 0);
}

void ATerminalFittings::Lockers(const FAt& O, int32 N)
{
	const float W = N * 0.42f;
	for (int32 i = 0; i < N; ++i)
	{
		Box(O, "steel", 0.4f, 1.9f, 0.5f, -W / 2 + 0.21f + i * 0.42f, 0.95f, 0);
		Box(O, "black", 0.02f, 0.12f, 0.03f, -W / 2 + 0.36f + i * 0.42f, 1.1f, 0.26f);
	}
}

void ATerminalFittings::BarStool(const FAt& O)
{
	Cyl(O, "leatherBlack", 0.21f, 0.07f, 0, 0.8f, 0);
	Cyl(O, "brass", 0.2f, 0.02f, 0, 0.755f, 0);
	for (int32 q = 0; q < 4; ++q)
	{
		const float A = q / 4.f * 2 * PI + 0.4f;
		Cyl(O, "brass", 0.013f, 0.78f, FMath::Cos(A) * 0.15f, 0.38f, FMath::Sin(A) * 0.15f);
	}
}

void ATerminalFittings::Lamps(const FPlanRoom& R)
{
	// the lights of every room: panels in the ceiling (the hall, lounge, bar and walkway have their own)
	const FString& K = R.Kind;
	if (K == TEXT("hall") || K == TEXT("museum") || K == TEXT("lounge") || K == TEXT("bar") || K == TEXT("walk")) return;
	const float H = K == TEXT("vestibule") ? 3.4f : 5.4f;
	const float Top = (R.Floor >= 2 ? F2Y : 0.f) + H - 0.07f;
	const float Step = 4.2f;
	const int32 NX = FMath::Max(1, FMath::RoundToInt((R.X1 - R.X0) / Step)), NZ = FMath::Max(1, FMath::RoundToInt((R.Z1 - R.Z0) / Step));
	const bool bLong = K == TEXT("baghall");
	for (int32 i = 0; i < NX; ++i)
		for (int32 j = 0; j < NZ; ++j)
		{
			const float X = R.X0 + (R.X1 - R.X0) * (i + 0.5f) / NX, Z = R.Z0 + (R.Z1 - R.Z0) * (j + 0.5f) / NZ;
			bool bStair = false;
			for (const FVector4f& S : Stairs) bStair |= X > S.X - 0.5f && X < S.Z + 0.5f && Z > S.Y - 0.5f && Z < S.W + 0.5f;
			if (!bStair) Box({X, Top, Z, 0}, "lamp", bLong ? 2.4f : 1.2f, 0.04f, bLong ? 0.3f : 0.6f, 0, 0, 0);
		}
}

#pragma endregion

#pragma region Rooms

void ATerminalFittings::Hall(const FPlanRoom& R)
{
	const FColor White(242, 244, 247), Navy(20, 32, 46);
	// the two check-in islands: desks 1–12 and 13–24 facing the entrance, the belt behind them, the banner over it
	for (int32 Isl = 0; Isl < 2; ++Isl)
	{
		const float X0 = Isl ? 71.f : 44.f;
		const FAt Banner{X0 + 12.5f, 0, 20.6f, 0};
		Box(Banner, "navy", 25, 2.2f, 0.3f, 0, 3.4f, 0);
		Box(Banner, "white", 25, 0.9f, 0.3f, 0, 1.6f, 0);
		Box(Banner, "tealUI", 25, 0.25f, 0.32f, 0, 2.2f, 0);
		Text(Banner, Isl ? TEXT("CHECK-IN  13–24") : TEXT("CHECK-IN  1–12"), 0, 3.4f, -0.17f, -1.f, 0.9f, FColor::White);
		const FAt Belt{X0 + 12.5f, 0, 18.4f, 0};
		Box(Belt, "steel", 25, 0.45f, 1.0f, 0, 0.22f, 0);
		Box(Belt, "rubber", 24.8f, 0.04f, 0.8f, 0, 0.46f, 0);
		for (int32 k = 0; k < 12; ++k)
		{
			const int32 N = Isl * 12 + k + 1;
			const float X = X0 + 1.05f + k * 2.08f;
			const FAt Desk{X, 0, 16.4f, PI};
			Box(Desk, "white", 1.7f, 1.05f, 0.75f, 0, 0.52f, 0);
			Box(Desk, "marbleDark", 1.74f, 0.05f, 0.85f, 0, 1.07f, 0);
			Box(Desk, "black", 0.5f, 0.32f, 0.03f, 0.3f, 1.32f, 0.15f);
			Box(Desk, "steel", 0.6f, 0.2f, 0.75f, -1.15f, 0.12f, 0);
			Box(Desk, "rubber", 0.6f, 0.03f, 0.75f, -1.15f, 0.24f, 0);
			Box(Desk, "steel", 0.04f, 0.5f, 0.04f, 0.5f, 1.3f, 0);
			Box(Desk, "yellow", 0.5f, 0.3f, 0.03f, 0.5f, 1.6f, 0);
			Text(Desk, FString::FromInt(N), 0.5f, 1.6f, 0.02f, 1.f, 0.22f, Navy);
			Text(Desk, FString::FromInt(N), 0.5f, 1.6f, -0.02f, -1.f, 0.22f, Navy);
			OfficeChair({X + 0.2f, 0, 17.25f, PI});
		}
		// queue lines in front: posts and their belts
		for (int32 k = 0; k < 6; ++k)
		{
			const FAt Q{X0 + 2 + k * 4.2f, 0, 13.6f, 0};
			Cyl(Q, "chrome", 0.04f, 0.95f, 0, 0.48f, 0);
			Cyl(Q, "chrome", 0.16f, 0.03f, 0, 0.015f, 0);
			if (k < 5) Box(Q, "navy", 4.2f, 0.05f, 0.01f, 2.1f, 0.92f, 0);
		}
	}
	// the bag drop: eight self-service points
	for (int32 k = 0; k < 8; ++k)
	{
		const FAt B{k < 4 ? 101.5f : 108.5f, 0, 10.5f + (k % 4) * 4.2f, PI};
		Box(B, "white", 0.9f, 1.5f, 0.5f, 0.9f, 0.75f, -0.2f);
		Box(B, "black", 0.6f, 0.4f, 0.03f, 0.9f, 1.25f, 0.06f);
		Box(B, "steel", 1.0f, 0.35f, 1.4f, -0.2f, 0.17f, -0.4f);
		Box(B, "rubber", 1.0f, 0.03f, 1.4f, -0.2f, 0.36f, -0.4f);
		Part(B, EShape::Box, "glass", 0.05f, 1.3f, 1.4f, -0.75f, 1.0f, -0.4f);
	}
	Sign({105, 4.4f, 26.5f, PI}, TEXT("BAG DROP · SELF SERVICE"), 6, 0.8f, "navy", White);
	// trees in white planters, benches round them, bins
	for (const FVector2f& T : {FVector2f(58, 2.55f), FVector2f(92, 2.55f), FVector2f(38.55f, 25), FVector2f(92.5f, 27.45f)}) TreePlanter({T.X, 0, T.Y, 0});
	Seats({64, 0, 0.5f, 0}, 5);
	Seats({86, 0, 0.5f, 0}, 5);
	Seats({36.5f, 0, 9, HALF_PI}, 5);
	for (const FVector2f& B : {FVector2f(66, 0.45f), FVector2f(89, 0.45f), FVector2f(36.45f, 5.5f), FVector2f(97, 29.6f)}) Bin({B.X, 0, B.Y, 0});
	Sign({70, 3.4f, 29.75f, PI}, TEXT("SECURITY · ALL GATES"), 6.4f, 0.7f, "navy", White);
	// DEPARTURES over the main entrance, outside
	Sign({75, 7.6f, -0.4f, PI}, TEXT("DEPARTURES"), 12, 1.6f, "white", Navy);
	// the canopies over the three ways in, and benches on the kerb
	for (const FVector2f& C : {FVector2f(50, 13), FVector2f(75, 17), FVector2f(101, 13)})
	{
		const FAt O{C.X, 3.8f, -6, 0};
		Box(O, "white", C.Y, 0.25f, 4, 0, 0, 0);
		Box(O, "lamp", C.Y - 0.4f, 0.02f, 3.6f, 0, -0.13f, 0);
		for (const float S : {-1.f, 1.f}) Cyl(O, "mullion", 0.1f, 3.8f, S * (C.Y / 2 - 0.4f), -1.9f, -1.6f);
	}
	for (const FVector2f& B : {FVector2f(40, -12), FVector2f(62, -14), FVector2f(88, -14), FVector2f(110, -12)})
	{
		Box({B.X, 0, B.Y, 0}, "oak", 2.4f, 0.08f, 0.6f, 0, 0.45f, 0);
		Box({B.X, 0, B.Y, 0}, "steel", 2.2f, 0.42f, 0.5f, 0, 0.21f, 0);
	}
}

void ATerminalFittings::Lounge(const FPlanRoom& R)
{
	const float LH = 4.8f;
	// the ceiling: copper strips in parallel waves under a dark ceiling, with warm light strips at its edges
	Box({(R.X0 + R.X1) / 2, LH - 0.09f, (R.Z0 + R.Z1) / 2, 0}, "loungeDark", R.X1 - R.X0, 0.02f, R.Z1 - R.Z0, 0, 0, 0);
	for (int32 j = 0; j < 40; ++j)
	{
		const float Z = R.Z0 + 0.3f + j * (R.Z1 - R.Z0 - 0.6f) / 39;
		const int32 Segs = 18;
		for (int32 k = 0; k < Segs; ++k)
		{
			auto Y = [&](int32 q) { return LH - 0.5f - 0.4f * FMath::Sin(q / float(Segs) * PI * 3 + j * 0.25f); };
			const float XA = R.X0 + 0.1f + k * (R.X1 - R.X0 - 0.2f) / Segs, XB = R.X0 + 0.1f + (k + 1) * (R.X1 - R.X0 - 0.2f) / Segs;
			const float YA = Y(k), YB = Y(k + 1), Len = FMath::Sqrt(FMath::Square(XB - XA) + FMath::Square(YB - YA));
			const FVector At = FTerminalPlan::ToWorld((XA + XB) / 2, (YA + YB) / 2, Z);
			Kit(EShape::Box, "copper")->AddInstance(FTransform(FRotator(FMath::RadiansToDegrees(FMath::Atan2(YB - YA, XB - XA)), 0, 0), At, FVector(Len + 0.01f, 0.1f, 0.1f)));
		}
	}
	for (const float Z : {R.Z0 + 0.3f, R.Z1 - 0.15f}) Box({(R.X0 + R.X1) / 2, LH - 0.13f, Z, 0}, "lampWarm", R.X1 - R.X0 - 0.3f, 0.03f, 0.08f, 0, 0, 0);
	// walnut wainscot with a brass rail round the room
	for (const FVector4f& W : {FVector4f((R.X0 + R.X1) / 2, R.Z0 + 0.17f, R.X1 - R.X0, 0.03f), FVector4f((R.X0 + R.X1) / 2, R.Z1 - 0.17f, R.X1 - R.X0, 0.03f), FVector4f(R.X1 - 0.17f, (R.Z0 + R.Z1) / 2, 0.03f, R.Z1 - R.Z0), FVector4f(R.X0 + 0.17f, 37, 0.03f, 14)})
	{
		Box({W.X, 0, W.Y, 0}, "walnut", W.Z, 1.1f, W.W, 0, 0.55f, 0);
		Box({W.X, 0, W.Y, 0}, "brass", W.Z, 0.04f, W.W + 0.02f, 0, 1.12f, 0);
	}
	// fireplace 7, art deco brass: the black lacquer chimney breast, the brass surround in sunburst steps, the fire
	{
		const float FZ = 37;
		const FAt O{R.X0 + 0.18f, 0, FZ, HALF_PI};
		Box(O, "lacquer", 3.0f, LH, 0.12f, 0, LH / 2, 0.06f);
		for (int32 s = 0; s < 5; ++s)
		{
			const float W2 = 1.2f + s * 0.28f, H2 = 1.0f + s * 0.14f, D2 = 0.14f + (4 - s) * 0.02f;
			Box(O, "brass", W2 + 0.04f, 0.04f, D2, 0, H2, 0.12f + D2 / 2);
			for (const float Sd : {-1.f, 1.f}) Box(O, "brass", 0.04f, H2, D2, Sd * W2 / 2, H2 / 2, 0.12f + D2 / 2);
		}
		Box(O, "black", 1.16f, 0.96f, 0.06f, 0, 0.48f, 0.13f);
		Box(O, "marbleDark", 2.6f, 0.05f, 0.6f, 0, 0.025f, 0.42f);
		Box(O, "trunk", 0.9f, 0.06f, 0.18f, 0, 0.08f, 0.26f);
		Box(O, "lampWarm", 0.8f, 0.5f, 0.04f, 0, 0.35f, 0.18f);
		Armchair({R.X0 + 2.6f, 0, FZ - 1.0f, -HALF_PI - 0.4f}, "leather");
		Armchair({R.X0 + 2.6f, 0, FZ + 1.0f, -HALF_PI + 0.4f}, "leather");
	}
	// the bar, black and brass: an L-shaped counter returning to the north wall, the back bar with its bottles, stools
	{
		const float BZ = 49.4f, XA = 95, XR = 105.2f;
		const FAt O{0, 0, 0, 0};
		Box(O, "lacquer", XR - XA, 1.08f, 0.6f, (XA + XR) / 2, 0.54f, BZ);
		Box(O, "lacquer", 0.6f, 1.08f, R.Z1 - BZ - 0.3f, XR - 0.3f, 0.54f, (BZ + R.Z1 - 0.3f) / 2);
		Box(O, "marbleDark", XR - XA + 0.2f, 0.06f, 0.8f, (XA + XR) / 2, 1.1f, BZ);
		Box(O, "marbleDark", 0.8f, 0.06f, R.Z1 - BZ - 0.4f, XR - 0.3f, 1.1f, (BZ + R.Z1 - 0.4f) / 2);
		Box(O, "brass", XR - XA, 0.04f, 0.04f, (XA + XR) / 2, 0.2f, BZ - 0.4f);
		const FAt BB{99, 0, R.Z1 - 0.38f, PI};
		Box(BB, "lacquer", 6, 0.9f, 0.35f, 0, 0.45f, 0);
		Box(BB, "black", 6.1f, 2.1f, 0.04f, 0, 2.0f, 0.15f);
		static const FName Bottle[] = {"plantDark", "gold", "red", "offwhite", "navy", "trunk", "tealUI", "yellow", "brass", "black", "porcelain"};
		for (const float YY : {1.4f, 1.85f, 2.3f})
		{
			Part(BB, EShape::Box, "glass", 5.6f, 0.015f, 0.25f, 0, YY, 0);
			for (int32 k = 0; k < 20; ++k) Cyl(BB, Bottle[(k * 7 + FMath::RoundToInt(YY * 10)) % 11], 0.035f, 0.26f, -2.7f + k * 0.285f, YY + 0.14f, -0.02f);
		}
		for (int32 k = 0; k < 7; ++k) BarStool({XA + 0.7f + k * 1.25f, 0, BZ - 0.8f, 0});
		for (const float SZ : {50.2f, 50.95f, 51.6f}) BarStool({XR + 0.75f, 0, SZ - 0.4f, 0});
		for (int32 k = 0; k < 6; ++k)
		{
			const FAt G{XA + 0.6f + k * (XR - XA - 1.2f) / 5, 0, BZ - 0.5f, 0};
			Cyl(G, "black", 0.004f, LH - 2.85f, 0, (LH + 2.85f) / 2, 0);
			Ball(G, "lamp", 0.17f, 0, 2.85f, 0);
		}
	}
	// seating: two rows of three groups of armchairs round coffee tables, on rugs
	int32 K = 0;
	for (const float GX : {97.5f, 103.5f, 109.5f})
	{
		for (const float GZ : {34.2f, 40.2f})
		{
			Box({GX, 0.024f, GZ, 0}, "carpet", 4.2f, 0.01f, 4.2f, 0, 0, 0);
			Table({GX, 0, GZ, 0}, 0.5f, 0.42f, "walnut");
			const FName M = (K + FMath::RoundToInt(GZ)) % 2 ? FName("fabricTeal") : FName("fabricRust");
			Armchair({GX, 0, GZ - 1.1f, 0}, M);
			Armchair({GX, 0, GZ + 1.1f, PI}, M);
			Armchair({GX - 1.1f, 0, GZ, HALF_PI}, M);
			Armchair({GX + 1.1f, 0, GZ, -HALF_PI}, M);
		}
		++K;
	}
	for (const FVector2f& P : {FVector2f(R.X0 + 0.8f, R.Z0 + 0.8f), FVector2f(R.X1 - 0.8f, R.Z0 + 0.8f), FVector2f(R.X1 - 0.8f, 44)}) Plant({P.X, 0, P.Y, 0}, true);
	for (const FVector2f& P : {FVector2f(100, 36), FVector2f(106, 36), FVector2f(100, 44), FVector2f(106, 44)})
	{
		const FAt G{P.X, 0, P.Y, 0};
		Cyl(G, "black", 0.006f, 1.2f, 0, LH - 0.6f, 0);
		Cyl(G, "brass", 0.28f, 0.3f, 0, LH - 1.3f, 0);
		Ball(G, "lamp", 0.08f, 0, LH - 1.38f, 0);
	}
}

void ATerminalFittings::Bar(const FPlanRoom& R)
{
	// the big bar: an oval counter of black lacquer and brass with its back bar of bottles, stools round it, tables round the room
	const float Y = R.Floor >= 2 ? F2Y : 0.f, CX = 125, CZ = 40.9f, AX = 9, AZ = 4.5f;
	const int32 N = 44;
	for (int32 k = 0; k < N; ++k)
	{
		const float A = k / float(N) * 2 * PI, A2 = (k + 1) / float(N) * 2 * PI;
		const float X1 = FMath::Cos(A) * AX, Z1 = FMath::Sin(A) * AZ, X2 = FMath::Cos(A2) * AX, Z2 = FMath::Sin(A2) * AZ;
		const float Len = FMath::Sqrt(FMath::Square(X2 - X1) + FMath::Square(Z2 - Z1)) + 0.04f, Yaw = -FMath::Atan2(Z2 - Z1, X2 - X1);
		Box({CX + (X1 + X2) / 2, Y, CZ + (Z1 + Z2) / 2, Yaw}, "lacquer", Len, 1.08f, 0.6f, 0, 0.54f, 0);
		Box({CX + (X1 + X2) / 2 * 1.01f, Y, CZ + (Z1 + Z2) / 2 * 1.01f, Yaw}, "marbleDark", Len, 0.06f, 0.82f, 0, 1.1f, 0);
		Box({CX + (X1 + X2) / 2 * 1.06f, Y, CZ + (Z1 + Z2) / 2 * 1.08f, Yaw}, "brass", Len, 0.03f, 0.03f, 0, 0.2f, 0);
	}
	Part({CX, Y, CZ, 0}, EShape::Cyl, "lacquer", 12.8f, 0.9f, 4.8f, 0, 0.45f, 0);
	static const FName Bottle[] = {"plantDark", "gold", "red", "offwhite", "navy", "trunk", "tealUI", "yellow", "brass", "black", "porcelain"};
	for (int32 k = 0; k < 36; ++k)
	{
		const float A = k / 36.f * 2 * PI;
		Cyl({CX, Y, CZ, 0}, Bottle[k % 11], 0.035f, 0.28f, FMath::Cos(A) * AX * 0.5f, 1.05f, FMath::Sin(A) * AZ * 0.42f);
	}
	for (int32 k = 0; k < 26; ++k)
	{
		const float A = k / 26.f * 2 * PI;
		BarStool({CX + FMath::Cos(A) * (AX + 0.85f), Y, CZ + FMath::Sin(A) * (AZ + 0.85f), 0});
	}
	for (int32 k = 0; k < 10; ++k)
	{
		const float A = k / 10.f * 2 * PI;
		const FAt G{CX + FMath::Cos(A) * AX * 0.7f, Y, CZ + FMath::Sin(A) * AZ * 0.7f, 0};
		Cyl(G, "black", 0.004f, 2.4f, 0, 4.2f, 0);
		Ball(G, "lamp", 0.2f, 0, 3.0f, 0);
	}
	for (const float X : {110.f, 118.9f, 127.9f, 136.9f})
		for (const float Z : {48.8f, 32.9f})
		{
			if (X > 136 && Z > 45) continue;
			Table({X, Y, Z, 0}, 0.6f, 0.74f, "marbleDark");
			Chair({X, Y, Z - 0.9f, 0}, "leather"); Chair({X, Y, Z + 0.9f, PI}, "leather");
			Chair({X - 0.9f, Y, Z, HALF_PI}, "leather"); Chair({X + 0.9f, Y, Z, -HALF_PI}, "leather");
		}
	for (const FVector2f& P : {FVector2f(105, 31), FVector2f(145, 31), FVector2f(105, 51)}) Plant({P.X, Y, P.Y, 0}, true);
	Sign({125, Y + 4.6f, 30.25f, 0}, TEXT("THE BAR"), 4, 0.8f, "lacquer", FColor(212, 175, 55));
}

void ATerminalFittings::Room(const FPlanRoom& R, const FTerminalPlan& Plan)
{
	const FString& K = R.Kind;
	const float Y = R.Floor >= 2 ? F2Y : 0.f, CX = (R.X0 + R.X1) / 2, CZ = (R.Z0 + R.Z1) / 2;
	const float PG = 68, PN = 82;
	const FColor White(242, 244, 247), Red(179, 38, 30);
	if (K == TEXT("vestibule")) Box({CX, 0.022f, CZ, 0}, "rubber", R.X1 - R.X0 - 1, 0.01f, 2.6f, 0, 0, 0);
	else if (K == TEXT("plant"))
	{
		for (int32 k = 0; k < 4; ++k)
		{
			const FAt O{5.f + k * 8, 0, 40, 0};
			Box(O, "offwhite", 6, 2.8f, 3.2f, 0, 1.4f, 0);
			Box(O, "steel", 1.6f, 1.6f, 0.05f, -1.8f, 1.6f, 1.62f);
			Cyl(O, "steel", 0.4f, 2.6f, 2.2f, 4.1f, 0);
		}
		for (int32 k = 0; k < 3; ++k)
		{
			const FAt O{6.f + k * 9, 0, 48, 0};
			Box(O, "navy", 1.6f, 1.2f, 1.0f, 0, 0.6f, 0);
			Cyl(O, "steel", 0.15f, 3, 0.9f, 1.5f, 0);
		}
		for (int32 k = 0; k < 4; ++k) Part({18, 4.6f + k * 0.35f, 33, 0}, EShape::Box, "steel", 34, 0.28f, 0.28f, 0, 0, 0);
	}
	else if (K == TEXT("wc"))
	{
		// stalls along the longer wall, the sinks and the mirror across from them
		const bool bAlong = (R.X1 - R.X0) >= (R.Z1 - R.Z0);
		const int32 N = FMath::Max(2, FMath::FloorToInt(((bAlong ? R.X1 - R.X0 : R.Z1 - R.Z0) - 2) / 1.5f));
		for (int32 i = 0; i < N; ++i)
		{
			const float U = (bAlong ? R.X0 : R.Z0) + 1 + i * 1.5f + 0.75f;
			const FAt O{bAlong ? U : R.X1 - 1.0f, Y, bAlong ? R.Z1 - 1.0f : U, bAlong ? PI : -HALF_PI};
			for (const float S : {-1.f, 1.f}) Box(O, "offwhite", 0.04f, 2.0f, 1.6f, S * 0.75f, 1.1f, 0);
			Box(O, "navy", 1.46f, 1.9f, 0.04f, 0, 1.1f, 0.8f);
			Box(O, "porcelain", 0.4f, 0.42f, 0.6f, 0, 0.21f, -0.45f);
			Box(O, "porcelain", 0.38f, 0.4f, 0.18f, 0, 0.6f, -0.72f);
		}
		const float Len = (bAlong ? R.X1 - R.X0 : R.Z1 - R.Z0) * 0.5f;
		const FAt S{bAlong ? CX : R.X0 + 0.5f, Y, bAlong ? R.Z0 + 0.5f : CZ, bAlong ? 0.f : HALF_PI};
		Box(S, "marbleDark", Len, 0.1f, 0.55f, 0, 0.85f, 0);
		Box(S, "white", Len, 0.9f, 0.04f, 0, 1.6f, -0.26f);
		Box(S, "chrome", Len - 0.1f, 0.8f, 0.01f, 0, 1.6f, -0.24f);
		for (int32 i = 0; i < FMath::FloorToInt(Len / 0.9f); ++i)
		{
			const float SX = -Len / 2 + 0.45f + i * 0.9f;
			Cyl(S, "porcelain", 0.18f, 0.08f, SX, 0.86f, 0.02f);
			Cyl(S, "chrome", 0.015f, 0.2f, SX, 1.0f, -0.18f);
		}
	}
	else if (K == TEXT("clean"))
	{
		Shelf({40, 0, 51.4f, PI}, 3, 2, 0.5f);
		Shelf({46, 0, 51.4f, PI}, 3, 2, 0.5f);
		Box({44, 0, 46, 0}, "yellow", 1.0f, 0.9f, 0.55f, 0, 0.6f, 0);
		Cyl({44, 0, 46, 0}, "navy", 0.2f, 0.5f, -0.3f, 1.25f, 0);
	}
	else if (K == TEXT("security"))
	{
		const float Lanes[] = {53, 59.2f, 65.3f, 71.5f, 77.7f, 83.8f};
		for (const float X : Lanes)
		{
			const FAt O{X, 0, 36, 0};
			Box(O, "steel", 0.8f, 0.8f, 8, 0, 0.4f, 0);
			Box(O, "rubber", 0.7f, 0.03f, 8, 0, 0.81f, 0);
			Box(O, "offwhite", 1.4f, 1.2f, 1.8f, 0, 1.2f, -0.5f);
			Box(O, "black", 0.9f, 0.5f, 0.05f, 0, 1.0f, -1.42f);
			Box(O, "black", 0.9f, 0.5f, 0.05f, 0, 1.0f, 0.42f);
			for (int32 t = 0; t < 4; ++t) Box(O, "navy", 0.55f, 0.08f, 0.4f, 0, 0.86f, 1.4f + t * 0.55f);
			Box(O, "offwhite", 0.12f, 2.2f, 0.6f, 1.15f, 1.1f, -0.4f);
			Box(O, "offwhite", 0.12f, 2.2f, 0.6f, 2.2f, 1.1f, -0.4f);
			Box(O, "offwhite", 1.17f, 0.25f, 0.6f, 1.675f, 2.2f, -0.4f);
			Box(O, "green", 0.9f, 0.04f, 0.05f, 1.675f, 2.05f, -0.1f);
			const FAt M{X - 0.9f, 0, 35, HALF_PI};
			Box(M, "white", 0.6f, 0.8f, 0.5f, 0, 0.4f, 0);
			Box(M, "black", 0.4f, 0.3f, 0.03f, 0, 1.0f, 0);
		}
		for (int32 k = 0; k < 6; ++k)
		{
			const FAt Q{54.f + k * 6, 0, 31.4f, 0};
			Cyl(Q, "chrome", 0.04f, 0.95f, 0, 0.48f, 0);
			Cyl(Q, "chrome", 0.16f, 0.03f, 0, 0.015f, 0);
		}
	}
	else if (K == TEXT("airside"))
	{
		for (const float X : {74.f, 80.f, 86.f}) Seats({X, 0, 51.5f, PI}, 6);
		Plant({52, 0, 50, 0}, true); Plant({88, 0, 50, 0}, true); Bin({72, 0, 50.5f, 0});
		Sign({62, 4.2f, 42.3f, 0}, TEXT("GATES 1–8 · FLOOR 2"), 5, 0.6f, "navy", White);
	}
	else if (K == TEXT("lounge")) Lounge(R);
	else if (K == TEXT("office"))
	{
		const bool bEast = R.X0 >= 136;
		const float DX = bEast ? 141.5f : 119.5f, DZ = R.Z0 + 4.7f;
		Desk({DX, Y, DZ, PI}, 1.8f, 0.8f, "oak", 2);
		OfficeChair({DX, Y, DZ - 1.0f, 0});
		Chair({DX - 0.5f, Y, DZ + 1.3f, PI}, "fabricGrey"); Chair({DX + 0.5f, Y, DZ + 1.3f, PI}, "fabricGrey");
		Shelf({bEast ? R.X1 - 1.2f : R.X0 + 1.2f, Y, R.Z1 - 0.4f, PI}, 2, 2.0f, 0.4f);
		Shelf({bEast ? R.X1 - 3.4f : R.X0 + 3.4f, Y, R.Z1 - 0.4f, PI}, 2, 2.0f, 0.4f);
		Box({bEast ? R.X1 - 0.5f : R.X0 + 0.5f, Y, R.Z0 + 1.2f, bEast ? -HALF_PI : HALF_PI}, "steel", 0.9f, 1.3f, 0.6f, 0, 0.65f, 0);
		Plant({bEast ? R.X1 - 0.6f : R.X0 + 0.6f, Y, R.Z1 - 1.6f, 0}, true);
		const float MX = bEast ? R.X0 + 12 : R.X1 - 6;
		Box({MX, Y, R.Z0 + 5, 0}, "white", 2.4f, 0.05f, 1.1f, 0, 0.74f, 0);
		Box({MX, Y, R.Z0 + 5, 0}, "steel", 0.1f, 0.72f, 0.6f, 0, 0.36f, 0);
		for (const float S : {-1.f, 1.f}) for (const float Q : {-0.6f, 0.6f}) Chair({MX + Q, Y, R.Z0 + 5 + S * 0.85f, S > 0 ? PI : 0.f});
	}
	else if (K == TEXT("passage")) Sign({116, 2.6f, 21, -HALF_PI}, TEXT("STAFF ONLY"), 1.4f, 0.3f, "red", White);
	else if (K == TEXT("ops"))
	{
		const FAt W{123, 0, 33.6f, PI};
		Box(W, "black", 16, 3.0f, 0.2f, 0, 2.2f, 0);
		for (int32 i = 0; i < 6; ++i) Box(W, "screenPanel", 2.5f, 1.4f, 0.02f, -6.5f + i * 2.6f, 2.2f, 0.11f);
		Desk({119.5f, 0, 27.9f, PI}, 3.0f, 0.9f, "white", 3); OfficeChair({119.5f, 0, 26.8f, 0});
		for (const float Z : {27.9f, 30.5f}) { Desk({126, 0, Z, PI}, 2.4f, 0.8f, "white", 2); OfficeChair({126, 0, Z - 0.95f, 0}); }
		for (int32 i = 0; i < 4; ++i) Part({131.8f, 3.6f, 28, -HALF_PI}, EShape::Cyl, "white", 0.44f, 0.44f, 0.04f, -1.2f + i * 0.8f, 0, 0);
	}
	else if (K == TEXT("crew"))
	{
		Sofa({118, 0, 50.8f, PI}, 3, "fabricBlue"); Sofa({118, 0, 45, 0}, 3, "fabricBlue");
		Table({118, 0, 48, 0}, 0.6f, 0.45f, "oak");
		Lockers({127, 0, 51.5f, PI}, 8);
		Counter({126, 0, 43, 0}, 4, 0.65f, "white", "marbleDark", 0.92f);
		Box({125, 0.97f, 43, 0}, "chrome", 0.4f, 0.5f, 0.4f, 0, 0.25f, 0);
		Box({114.2f, 1.6f, 48, HALF_PI}, "black", 1.6f, 0.92f, 0.06f, 0, 0, 0);
		Box({114.2f, 1.6f, 48, HALF_PI}, "screenPanel", 1.5f, 0.84f, 0.01f, 0, 0, 0.035f);
	}
	else if (K == TEXT("corridor")) { for (float Z = 3; Z < 52; Z += 6) Box({132.2f, 2.2f, Z, HALF_PI}, "lampWarm", 0.4f, 0.12f, 0.08f, 0, 0, 0); }
	else if (K == TEXT("academy"))
	{
		Box({146, 0, 19.8f, PI}, "white", 8, 1.4f, 0.05f, 0, 1.7f, 0);
		Box({146, 0, 19.8f, PI}, "steel", 8.1f, 0.06f, 0.12f, 0, 0.98f, 0.05f);
		Sign({146, 3.4f, 19.75f, PI}, TEXT("Airport Dynasty Pro · Academy"), 3.2f, 0.5f, "navy", White);
		Desk({144.9f, 0, 17.3f, 0}, 1.8f, 0.8f, "oak", 1); OfficeChair({144.9f, 0, 18.4f, PI});
		for (const float Z : {14.2f, 10.6f, 7.1f, 3.5f})
			for (const float X : {138.7f, 142.3f, 145.9f, 149.5f, 153.1f})
			{
				const FAt O{X, 0, Z, PI};
				Box(O, "oak", 1.2f, 0.04f, 0.6f, 0, 0.74f, 0);
				for (const float S : {-1.f, 1.f}) Box(O, "steel", 0.04f, 0.72f, 0.55f, S * 0.57f, 0.36f, 0);
				Box(O, "black", 0.32f, 0.02f, 0.22f, 0, 0.77f, 0.05f);
				Chair({X, 0, Z - 0.65f, 0}, "fabricBlue");
			}
	}
	else if (K == TEXT("staffentr"))
	{
		Box({147, 0, 47, 0}, "white", 3, 1.05f, 0.7f, 0, 0.52f, 0);
		Box({147, 0, 47, 0}, "marbleDark", 3.1f, 0.05f, 0.8f, 0, 1.07f, 0);
		OfficeChair({147, 0, 48.2f, PI});
		for (const float Z : {42.5f, 44.2f}) { Box({152, 0, Z, 0}, "steel", 0.3f, 1.0f, 1.0f, 0, 0.5f, 0); Part({152, 0, Z, 0}, EShape::Box, "glass", 0.02f, 0.4f, 0.8f, 0.5f, 0.9f, 0); }
		Lockers({150, 0, 40.5f, 0}, 10);
		Sign({146, 3.2f, 51.8f, PI}, TEXT("STAFF ENTRANCE · ID REQUIRED"), 4.4f, 0.42f, "red", White);
	}
	else if (K == TEXT("baghall"))
	{
		// the sorting belts, in pieces between the doors (so you can cross from the corridor to the ramps)
		TArray<float> Gaps = {18, 134};
		for (int32 k = 0; k < 4; ++k) Gaps.Add(-72.6f + 80.55f * k + 14);
		Gaps.Sort();
		static const FName Bags[] = {"navy", "red", "black", "trunk"};
		for (const float Z0 : {56.5f, 63.5f})
		{
			float X = R.X0 + 2;
			TArray<float> Ends = Gaps;
			Ends.Add(R.X1);
			for (const float GX : Ends)
			{
				const float A = X, B = GX - 3;
				if (B - A > 4)
				{
					const FAt O{(A + B) / 2, 0, Z0, 0};
					Box(O, "steel", B - A, 0.7f, 1.1f, 0, 0.35f, 0);
					Box(O, "rubber", B - A, 0.03f, 0.9f, 0, 0.71f, 0);
					for (int32 k = 0; k < FMath::FloorToInt((B - A) / 6); ++k) Box(O, Bags[k % 4], 0.6f, 0.4f, 0.4f, -(B - A) / 2 + 3 + k * 6, 0.92f, 0);
				}
				X = GX + 3;
			}
		}
		for (int32 k = 0; k < 10; ++k)
		{
			const FAt O{R.X0 + 20 + k * 32, 0, 60, HALF_PI};
			Box(O, "steel", 1.4f, 0.06f, 2.6f, 0, 0.55f, 0);
			Box(O, "yellow", 1.42f, 0.08f, 2.6f, 0, 1.6f, 0);
			for (const float SX : {-0.6f, 0.6f}) for (const float SZ : {-1.0f, 1.0f}) Cyl(O, "black", 0.16f, 0.12f, SX, 0.18f, SZ);
		}
	}
	else if (K == TEXT("ramp"))
	{
		for (int32 k = 0; k < 3; ++k)
		{
			const FAt O{R.X0 + 6 + k * 4, 0, PG + 8, 0};
			Box(O, "steel", 1.6f, 0.06f, 3.0f, 0, 0.55f, 0);
			for (const float S : {-1.f, 1.f}) Box(O, "steel", 1.6f, 1.2f, 0.05f, 0, 1.15f, S * 1.5f);
			Box(O, "yellow", 1.62f, 0.08f, 3.0f, 0, 1.75f, 0);
			for (const float SX : {-0.7f, 0.7f}) for (const float SZ : {-1.2f, 1.2f}) Cyl(O, "black", 0.18f, 0.12f, SX, 0.2f, SZ);
		}
		Box({R.X0 + 20, 0, PG + 7, 0}, "yellow", 1.6f, 1.0f, 2.6f, 0, 0.75f, 0);
		Part({R.X0 + 20, 0, PG + 7, 0}, EShape::Box, "glass", 1.4f, 0.9f, 1.0f, 0, 1.7f, -0.4f);
		Lockers({R.X0 + 14, 0, PG + 1.2f, 0}, 10);
	}
	else if (K == TEXT("view"))
	{
		for (int32 k = 0; k < 3; ++k) Seats({CX, Y, PG + 3 + k * 3.2f, 0}, 26);
		Plant({R.X0 + 1.2f, Y, PN - 1.4f, 0}, true); Plant({R.X1 - 1.2f, Y, PN - 1.4f, 0}, true);
	}
	else if (K == TEXT("link"))
	{
		for (const float Z : {36.f, 44.f}) Seats({6.5f, Y, Z, HALF_PI}, 6);
		Plant({7, Y, 31.2f, 0}, false); Plant({13, Y, 31.2f, 0}, false);
	}
	else if (K == TEXT("food"))
	{
		for (const float X : {18.f, 24.5f, 31.f, 37.5f, 44.f, 50.5f})
			for (const float Z : {39.3f, 33.9f})
			{
				Table({X, Y, Z, 0}, 0.55f, 0.74f, "white");
				Chair({X, Y, Z - 0.85f, 0}, "fabricRust"); Chair({X, Y, Z + 0.85f, PI}, "fabricRust");
				Chair({X - 0.85f, Y, Z, HALF_PI}, "fabricRust"); Chair({X + 0.85f, Y, Z, -HALF_PI}, "fabricRust");
			}
		for (const float X : {16.f, 53.5f}) Plant({X, Y, 31, 0}, true);
	}
	else if (K == TEXT("eatery"))
	{
		static const FName Cols[] = {"red", "plantDark", "yellow", "navy", "fabricBlue"};
		FString Num;
		for (const TCHAR Ch : R.Id) if (FChar::IsDigit(Ch)) Num = FString::Chr(Ch);
		const int32 N = FCString::Atoi(*Num);
		Counter({CX, Y, 49.2f, 0}, 6.4f, 0.8f, "white", "marbleDark");
		Box({CX, Y, 51.4f, PI}, "steel", 7.6f, 1.0f, 0.7f, 0, 0.5f, 0);
		Box({CX, Y, 51.4f, PI}, "steel", 7.6f, 0.9f, 0.4f, 0, 2.2f, 0.1f);
		Sign({CX, Y + 2.9f, 51.7f, PI}, TEXT("EATERY ") + Num, 6, 1.2f, Cols[FMath::Clamp(N - 1, 0, 4)], White);
		for (const float SX : {-2.2f, -0.8f, 0.6f, 2.0f})
		{
			Cyl({CX + SX, Y, 48.3f, 0}, "leatherBlack", 0.19f, 0.06f, 0, 0.75f, 0);
			Cyl({CX + SX, Y, 48.3f, 0}, "chrome", 0.03f, 0.72f, 0, 0.36f, 0);
		}
	}
	else if (K == TEXT("landing"))
	{
		Seats({79.3f, Y, 51.5f, PI}, 18); Seats({83.5f, Y, 42.5f, 0}, 18);
		Plant({57.2f, Y, 51, 0}, true); Plant({88.8f, Y, 43, 0}, true);
	}
	else if (K == TEXT("taxfree"))
	{
		for (const float X : {60.9f, 66.9f, 72.9f, 78.9f, 84.9f}) for (const float Z : {37.5f, 33.5f}) Shelf({X, Y, Z, 0}, 2.6f, 1.6f, 0.9f);
		Counter({62, Y, 40.8f, PI}, 3.2f, 0.8f, "lacquer", "gold");
		Counter({84, Y, 40.8f, PI}, 3.2f, 0.8f, "lacquer", "gold");
		Sign({73, Y + 4.3f, 41.7f, PI}, TEXT("TAX FREE"), 5, 0.8f, "black", FColor(212, 175, 55));
		Cyl({73, Y, 35.5f, 0}, "white", 1.0f, 1.0f, 0, 0.5f, 0);
		for (int32 k = 0; k < 10; ++k) { const float A = k / 10.f * 2 * PI; Cyl({73, Y, 35.5f, 0}, "gold", 0.05f, 0.16f, FMath::Cos(A) * 0.75f, 1.08f, FMath::Sin(A) * 0.75f); }
	}
	else if (K == TEXT("family"))
	{
		Box({97, Y + 0.03f, 35, 0}, "tealUI", 6, 0.04f, 5, 0, 0, 0);
		const FName Toys[] = {"red", "yellow", "navy"};
		for (int32 k = 0; k < 3; ++k) Box({95.f + k * 1.6f, Y, 34.5f, 0}, Toys[k], 0.6f, 0.6f, 0.6f, 0, 0.3f, 0);
		Sofa({97, Y, 40.6f, PI}, 3, "fabricTeal");
		Table({101.5f, Y, 33, 0}, 0.5f, 0.5f, "white");
		for (const float A : {0.f, 2.1f, 4.2f}) Chair({101.5f + FMath::Cos(A) * 0.85f, Y, 33 + FMath::Sin(A) * 0.85f, -A - HALF_PI}, "fabricBlue");
		Counter({92, Y, 41.2f, PI}, 2.2f, 0.7f, "white", "white", 0.9f);
	}
	else if (K == TEXT("bar")) Bar(R);
	else if (K == TEXT("walk"))
	{
		// the two moving walkways side by side down the middle, rails between them, seating and planters by the south wall
		const float ZC = (R.Z0 + R.Z1) / 2;
		const FVector2f Segs[] = {FVector2f(-75, -5), FVector2f(5, 75), FVector2f(85, 155), FVector2f(165, 235)};
		for (const FVector2f& S : Segs)
		{
			const float L = S.Y - S.X, MX = (S.X + S.Y) / 2;
			for (const float DZ : {-0.85f, 0.85f})
			{
				const FAt O{MX, Y, ZC + DZ, 0};
				Box(O, "rubber", L, 0.12f, 1.5f, 0, 0.06f, 0);
				Box(O, "yellow", 0.6f, 0.02f, 1.5f, -L / 2 + 0.3f, 0.125f, 0);
				Box(O, "yellow", 0.6f, 0.02f, 1.5f, L / 2 - 0.3f, 0.125f, 0);
			}
			for (const float DZ : {-1.68f, 0.f, 1.68f})
			{
				Part({MX, Y, ZC + DZ, 0}, EShape::Box, "glass", L, 0.9f, 0.06f, 0, 0.55f, 0);
				Box({MX, Y, ZC + DZ, 0}, "black", L, 0.07f, DZ != 0 ? 0.14f : 0.2f, 0, 1.02f, 0);
			}
		}
		for (const float GX : {0.f, 80.f, 160.f}) Seats({GX, Y, R.Z0 + 0.5f, 0}, 6);
		for (float X = -78; X < 242; X += 40) Plant({X + 2, Y, R.Z0 + 0.8f, 0}, true);
	}
}

void ATerminalFittings::Gate(const FPlanRoom& R)
{
	const bool bUpper = R.Kind == TEXT("gate2");
	const float Y = R.Floor >= 2 ? F2Y : 0.f, X0 = R.X0, PG = R.Z0, PN = R.Z1;
	const float RowA = bUpper ? X0 + 2.5f : X0 + 5.5f, RowB = bUpper ? X0 + 26 : X0 + 38.5f;
	for (int32 k = 0; k < 3; ++k) Seats({(RowA + RowB) / 2, Y, PG + 3.4f + k * 3.0f, 0}, FMath::FloorToInt((RowB - RowA) / 0.62f));
	const float DX = bUpper ? X0 + 17 : X0 + 20;
	const FAt Desk{DX, Y, PN - 1.6f, 0};
	Box(Desk, "white", 2.6f, 1.05f, 0.7f, 0, 0.52f, 0);
	Box(Desk, "marbleDark", 2.7f, 0.05f, 0.8f, 0, 1.07f, 0);
	Box(Desk, "black", 0.5f, 0.3f, 0.03f, 0.6f, 1.3f, -0.1f);
	OfficeChair({DX, Y, PN - 0.7f, PI});
	FString Num;
	for (const TCHAR Ch : R.Id) if (FChar::IsDigit(Ch)) Num = FString::Chr(Ch); // the gate's number: the id's last digit
	Sign({DX, Y + 3.5f, PN - 0.25f, PI}, TEXT("GATE ") + Num, 2.4f, 0.6f, "yellow", FColor(20, 32, 46));
	Plant({R.X1 - 1.2f, Y, PG + 1, 0}, true);
	Bin({DX - 2.6f, Y, PN - 1.6f, 0});
}

#pragma endregion

#pragma region Build

void ATerminalFittings::Build(const FTerminalPlan& Plan)
{
	Meshes[0] = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Meshes[1] = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Meshes[2] = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Meshes[0] || !Meshes[1] || !Meshes[2]) return;
	F2Y = Plan.F2Y;
	for (const FPlanStair& S : Plan.Stairs) Stairs.Add(FVector4f(S.X0, S.Z0, S.X1, S.Z1));
	for (const FPlanRoom& R : Plan.Rooms)
	{
		Lamps(R);
		if (R.Kind == TEXT("hall")) Hall(R);
		else if (R.Kind == TEXT("gate1") || R.Kind == TEXT("gate2")) Gate(R);
		else Room(R, Plan);
	}
	int32 N = 0;
	for (auto& K : Kits) N += K.Value->GetInstanceCount();
	UE_LOG(LogTemp, Log, TEXT("Fittings built: %d pieces"), N);
}

#pragma endregion
