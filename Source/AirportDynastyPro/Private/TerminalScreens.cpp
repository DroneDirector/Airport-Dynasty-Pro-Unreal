// The timetable screens: their places and shapes from the Android edition (plan.ts SCREENS, fixtures.ts), their rows the
// simulation's real flights (ADPBoards), page after page, looked at every second; no flights: empty rows (to-do 286).
#include "TerminalScreens.h"
#include "Site.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSim.h"
#include "ADPLog.h"
#include "TerminalBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"

#pragma region Places
namespace
{
	enum class EKind : uint8 { DepWall, ArrWall, DepTotem, ArrTotem, Dep, Pair, Gate };
	struct FScreenAt { EKind Kind; int32 Floor; float X, Z, Y, Face; int32 Gate; };

	/** Every screen as in plan.ts SCREENS (the gate screens follow its loop over the four gate modules). */
	TArray<FScreenAt> Places()
	{
		TArray<FScreenAt> S = {
			{EKind::DepWall, 1, 70, 29.75f, 4.2f, PI, 0}, {EKind::ArrWall, 1, 86, 29.75f, 4.2f, PI, 0},
			{EKind::DepTotem, 1, 50, 6.1f, 0, PI, 0}, {EKind::DepTotem, 1, 99.6f, 6.1f, 0, PI, 0},
			{EKind::DepTotem, 1, 70, 10, 0, PI, 0}, {EKind::ArrTotem, 1, 82, 10, 0, PI, 0},
			{EKind::Dep, 1, 80, 51.75f, 2.6f, PI, 0}, {EKind::Dep, 1, 104, 30.25f, 2.4f, 0, 0},
			{EKind::Pair, 2, 80, 51.75f, 2.6f, PI, 0}, {EKind::Pair, 2, 22.4f, 43.75f, 2.4f, PI, 0}, {EKind::Pair, 2, 108, 51.75f, 2.6f, PI, 0},
			{EKind::Dep, 2, -40, 52.25f, 3.0f, 0, 0}, {EKind::Dep, 2, 45, 52.25f, 3.0f, 0, 0},
			{EKind::Dep, 2, 95, 52.25f, 3.0f, 0, 0}, {EKind::Dep, 2, 190, 52.25f, 3.0f, 0, 0}};
		const float PG = 68.f;
		for (int32 k = 0; k < 4; ++k)
		{
			const float X0 = -72.6f + 80.55f * k; // plan.ts MOD
			S.Add({EKind::Gate, 2, X0 + 22, PG + 0.25f, 2.6f, 0, k + 1});
			S.Add({EKind::Gate, 1, X0 + 48, PG + 0.25f, 2.6f, 0, k + 5});
		}
		return S;
	}
}
#pragma endregion

#pragma region Setup

ATerminalScreens::ATerminalScreens()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ATerminalScreens::Bezel(const FAt& O, const FName Mat, float W, float H, float D, float X, float Y, float Z)
{
	UInstancedStaticMeshComponent* K = nullptr;
	if (TObjectPtr<UInstancedStaticMeshComponent>* F = Kits.Find(Mat)) K = *F;
	else
	{
		K = NewObject<UInstancedStaticMeshComponent>(this, Mat);
		K->SetupAttachment(Root);
		K->SetStaticMesh(Cube);
		K->SetMaterial(0, ATerminalBuilder::KitMaterial(this, Mat));
		K->SetCollisionProfileName(TEXT("BlockAll"));
		K->RegisterComponent();
		AddInstanceComponent(K);
		Kits.Add(Mat, K);
	}
	// the object's frame turned by its yaw as in the Android edition: local x to (cos, -sin), local z to (sin, cos)
	const float Cs = FMath::Cos(O.Yaw), Sn = FMath::Sin(O.Yaw);
	const FVector At = FTerminalPlan::ToWorld(O.X + X * Cs + Z * Sn, O.Y + Y, O.Z - X * Sn + Z * Cs);
	K->AddInstance(FTransform(FRotator(0, FMath::RadiansToDegrees(O.Yaw), 0), At, FVector(W, D, H)));
}

UTextRenderComponent* ATerminalScreens::Text(const FAt& O, const FString& Words, float X, float Y, float Z, float Facing, float Size, FColor Colour, bool bLeft)
{
	const float Cs = FMath::Cos(O.Yaw), Sn = FMath::Sin(O.Yaw);
	// words read left to right as you face the screen: the object's x mirrored when it faces its back
	const float XX = X * Facing;
	const FVector At = FTerminalPlan::ToWorld(O.X + XX * Cs + Z * Sn, O.Y + Y, O.Z - XX * Sn + Z * Cs);
	const float FX = Facing * Sn, FZ = Facing * Cs; // the way the words face, in the plan
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
	T->SetupAttachment(Root);
	T->SetText(FText::FromString(Words));
	T->SetHorizontalAlignment(bLeft ? EHTA_Left : EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetWorldSize(Size * 100.f);
	T->SetTextRenderColor(Colour);
	T->SetRelativeLocationAndRotation(At, FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(-FZ, FX)), 0));
	T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	T->RegisterComponent();
	AddInstanceComponent(T);
	return T;
}

#pragma endregion

#pragma region Shapes

void ATerminalScreens::Panel(const FAt& O, float W, float H, float Z, float Facing, bool bArr, int32 NRows)
{
	// the lit panel, a header with its title, then the rows: time, flight, destination or origin, status
	Bezel(O, "screenPanel", W, H, 0.01f, 0, 0, Z);
	const float Head = H * 0.16f, RowH = (H - Head - H * 0.04f) / NRows, L = -W / 2 + W * 0.03f;
	const float FZ = Z + Facing * 0.01f;
	Bezel(O, bArr ? FName("arrHeader") : FName("depHeader"), W, Head, 0.005f, 0, H / 2 - Head / 2, Z + Facing * 0.006f);
	{
		// (a French-speaking site: "DÉPARTS · DEPARTURES" - to-do 170)
		const FString En = bArr ? TEXT("ARRIVALS") : TEXT("DEPARTURES"), Lo = ADPSite::Translate(En);
		Text(O, Lo == En ? En : Lo + TEXT(" · ") + En, L, H / 2 - Head / 2, FZ + Facing * 0.004f, Facing, Head * 0.6f, FColor(14, 30, 58), true);
	}
	FBoard B;
	B.bArr = bArr;
	B.Clock = Text(O, FString(), W / 2 - W * 0.03f, H / 2 - Head / 2, FZ + Facing * 0.004f, Facing, Head * 0.42f, FColor(14, 30, 58), true);
	B.Clock->SetHorizontalAlignment(EHTA_Right);
	const float Sz = RowH * 0.55f;
	for (int32 r = 0; r < NRows; ++r)
	{
		const float Y = H / 2 - Head - RowH * (r + 0.5f) - H * 0.02f;
		TArray<UTextRenderComponent*> Row;
		Row.Add(Text(O, TEXT(""), L, Y, FZ, Facing, Sz, FColor::White, true));
		Row.Add(Text(O, TEXT(""), L + W * 0.14f, Y, FZ, Facing, Sz, FColor(255, 210, 74), true));
		Row.Add(Text(O, TEXT(""), L + W * 0.32f, Y, FZ, Facing, Sz, FColor::White, true));
		Row.Add(Text(O, TEXT(""), L + W * 0.72f, Y, FZ, Facing, Sz, FColor::White, true));
		B.Rows.Add(Row);
	}
	Boards.Add(B);
}

void ATerminalScreens::GateScreen(const FAt& O, int32 Gate)
{
	Bezel(O, "black", 1.7f, 0.98f, 0.08f, 0, 0, 0);
	Bezel(O, "screenPanel", 1.6f, 0.9f, 0.01f, 0, 0, 0.045f);
	Bezel(O, "yellow", 1.6f, 0.2f, 0.005f, 0, 0.35f, 0.05f);
	const float L = -0.74f;
	Text(O, FString::Printf(TEXT("GATE %d"), Gate), L, 0.35f, 0.055f, 1, 0.13f, FColor(20, 32, 46), true);
	FGateTexts G; G.Gate = Gate;
	G.Flight = Text(O, FString(), L, 0.1f, 0.055f, 1, 0.11f, FColor::White, true);
	G.Status = Text(O, TEXT("Gate closed"), L, -0.12f, 0.055f, 1, 0.1f, FColor(143, 163, 184), true);
	G.Time = Text(O, FString(), L, -0.32f, 0.055f, 1, 0.08f, FColor(143, 166, 189), true);
	Gates.Add(G);
}

#pragma endregion

#pragma region Build

void ATerminalScreens::Build(const FTerminalPlan& Plan)
{
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) return;
	for (const FScreenAt& S : Places())
	{
		const FAt O{S.X, (S.Floor == 2 ? Plan.F2Y : 0.f) + S.Y, S.Z, S.Face};
		switch (S.Kind)
		{
		case EKind::DepWall: case EKind::ArrWall:
			Bezel(O, "black", 5.2f, 2.0f, 0.12f, 0, 0, 0);
			Panel(O, 5.0f, 1.82f, 0.065f, 1, S.Kind == EKind::ArrWall, 8);
			break;
		case EKind::DepTotem: case EKind::ArrTotem:
		{
			Bezel(O, "black", 1.95f, 2.6f, 0.32f, 0, 1.6f, 0);
			Bezel(O, "black", 1.25f, 0.3f, 0.5f, 0, 0.15f, 0);
			const FAt Up{O.X, O.Y + 1.75f, O.Z, O.Yaw};
			Panel(Up, 1.75f, 1.3f, 0.165f, 1, S.Kind == EKind::ArrTotem, 5);
			Panel(Up, 1.75f, 1.3f, -0.165f, -1, S.Kind == EKind::ArrTotem, 5);
			break;
		}
		case EKind::Pair:
			for (const float K : {-1.f, 1.f})
			{
				const FAt Half{O.X + K * 0.9f * FMath::Cos(O.Yaw), O.Y, O.Z - K * 0.9f * FMath::Sin(O.Yaw), O.Yaw};
				Bezel(Half, "black", 1.7f, 0.98f, 0.08f, 0, 0, 0);
				Panel(Half, 1.6f, 0.9f, 0.045f, 1, K > 0, 5);
			}
			break;
		case EKind::Dep:
			Bezel(O, "black", 2.1f, 1.22f, 0.08f, 0, 0, 0);
			Panel(O, 2.0f, 1.12f, 0.045f, 1, false, 6);
			break;
		case EKind::Gate:
			GateScreen(O, S.Gate);
			break;
		}
	}
	Refresh();
	ADP_LOG(TEXT("Timetable screens built: %d timetables"), Boards.Num());
}

#pragma endregion

#pragma region Refresh

void ATerminalScreens::Refresh()
{
	const UADPSim* Sim = UADPSim::Get(this);
	const FString Hm = ADPBoards::Local(Sim && ADPBoards::Live(this) ? Sim->Now() : FDateTime::UtcNow()).ToString(TEXT("%H:%M"));
	// (words set only when they change: a text's mesh is made again at every setting)
	const auto Say = [](UTextRenderComponent* T, const FString& W) { if (T && T->Text.ToString() != W) T->SetText(FText::FromString(W)); };
	for (FBoard& B : Boards)
	{
		const TArray<FADPBoardRow>& Rows = ADPBoards::Shown(this, B.bArr);
		int32 Pages = 1; const int32 Page = ADPBoards::Page(Rows.Num(), B.Rows.Num(), Pages);
		Say(B.Clock, Pages > 1 ? FString::Printf(TEXT("Page %d/%d   %s"), Page + 1, Pages, *Hm) : Hm);
		for (int32 r = 0; r < B.Rows.Num(); ++r)
		{
			const int32 i = Page * B.Rows.Num() + r;
			const FADPBoardRow* R = Rows.IsValidIndex(i) ? &Rows[i] : nullptr;
			TArray<UTextRenderComponent*>& T = B.Rows[r];
			Say(T[0], R ? R->Time() : FString());
			Say(T[1], R ? R->Flight : FString());
			Say(T[2], R ? FString::Printf(TEXT("%s %s"), *R->PlaceCode, *R->Place) : FString());
			Say(T[3], !R ? FString() : !B.bArr && R->Gate ? FString::Printf(TEXT("%s · Gate %d"), *R->Status, R->Gate) : R->Status);
			if (R && T[3]->TextRenderColor != R->Col) T[3]->SetTextRenderColor(R->Col);
		}
	}
}

void ATerminalScreens::RefreshGates()
{
	for (const FGateTexts& G : Gates)
	{
		FADPBoardRow R;
		const bool bHas = ADPBoards::AtGate(this, G.Gate, R);
		if (G.Flight) G.Flight->SetText(FText::FromString(bHas ? FString::Printf(TEXT("%s · %s"), *R.Flight, *R.Place) : FString()));
		if (G.Status) { G.Status->SetText(FText::FromString(bHas ? R.Status : TEXT("Gate closed"))); G.Status->SetTextRenderColor(bHas ? R.Col : FColor(143, 163, 184)); }
		if (G.Time) G.Time->SetText(FText::FromString(bHas ? FString::Printf(TEXT("Scheduled %s"), *R.Time()) : FString()));
	}
}

void ATerminalScreens::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	if (Clock >= 1.f) { Clock = 0; Refresh(); if (++GateClock >= 4) { GateClock = 0; RefreshGates(); } }
}

#pragma endregion
