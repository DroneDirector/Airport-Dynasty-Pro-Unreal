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
	for (const FPlanRoom& R : Plan.Rooms)
	{
		if (R.Kind == TEXT("hall")) Hall(R);
		else if (R.Kind == TEXT("gate1") || R.Kind == TEXT("gate2")) Gate(R);
	}
	int32 N = 0;
	for (auto& K : Kits) N += K.Value->GetInstanceCount();
	UE_LOG(LogTemp, Log, TEXT("Fittings built: %d pieces"), N);
}

#pragma endregion
