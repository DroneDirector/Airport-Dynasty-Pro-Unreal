// The timetable screens: their places and shapes from the Android edition (plan.ts SCREENS, fixtures.ts), their rows
// the Android edition's sample flights (fids.ts sample) round the real time, refreshed every 4 seconds.
#include "TerminalScreens.h"
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

	/** One row of the sample timetable (fids.ts sample), for the time now in minutes of the day. */
	struct FRow { FString Time, Flight, Place, Status; FColor Col; };
	TArray<FRow> Sample(bool bArr, int32 N)
	{
		static const TCHAR* P[] = {TEXT("Reykjavík"), TEXT("Copenhagen"), TEXT("Oslo"), TEXT("London"), TEXT("Vágar"), TEXT("Amsterdam"), TEXT("Frankfurt"), TEXT("Stockholm"), TEXT("Bergen"), TEXT("Edinburgh"), TEXT("Paris"), TEXT("Helsinki")};
		static const TCHAR* C[] = {TEXT("KEF"), TEXT("CPH"), TEXT("OSL"), TEXT("LHR"), TEXT("FAE"), TEXT("AMS"), TEXT("FRA"), TEXT("ARN"), TEXT("BGO"), TEXT("EDI"), TEXT("CDG"), TEXT("HEL")};
		const FDateTime Now = FDateTime::Now();
		const int64 NowMin = Now.GetTicks() / ETimespan::TicksPerMinute;
		const int64 Base = NowMin / 15 * 15;
		TArray<FRow> Out;
		for (int32 i = -2; i < 12 && Out.Num() < N; ++i)
		{
			const int32 K = int32(((Base / 15) + i + (bArr ? 5 : 0)) % 12);
			const int64 Sched = Base + i * 30, Late = (K * 7) % 5 == 0 ? 20 : 0, Exp = Sched + Late;
			if (Exp <= NowMin - 25) continue;
			FString Status = TEXT("On time");
			FColor Col(127, 196, 245);
			if (!bArr)
			{
				if (NowMin >= Exp) { Status = TEXT("Departed"); Col = FColor(143, 163, 184); }
				else if (Exp - NowMin < 40) { Status = Late ? TEXT("Delayed, boarding") : TEXT("Boarding"); Col = FColor(63, 208, 160); }
				else if (Late) { Status = TEXT("Delayed"); Col = FColor(242, 181, 68); }
			}
			else
			{
				if (NowMin >= Exp) { Status = TEXT("Landed"); Col = FColor(63, 208, 160); }
				else if (Late) { Status = TEXT("Delayed"); Col = FColor(242, 181, 68); }
				else if (Exp - NowMin < 30) Status = TEXT("Expected");
			}
			const int32 Mins = int32(((Sched % 1440) + 1440) % 1440);
			Out.Add({FString::Printf(TEXT("%02d:%02d"), Mins / 60, Mins % 60), FString::Printf(TEXT("VT %d"), 200 + K * 37 + (bArr ? 1 : 0)),
				FString::Printf(TEXT("%s %s"), C[K], P[K]), Status, Col});
		}
		return Out;
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
	Text(O, bArr ? TEXT("ARRIVALS") : TEXT("DEPARTURES"), L, H / 2 - Head / 2, FZ + Facing * 0.004f, Facing, Head * 0.6f, FColor(14, 30, 58), true);
	FBoard B;
	B.bArr = bArr;
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
	static const TCHAR* Flights[] = {TEXT("AM 210 · Copenhagen"), TEXT("AM 344 · London"), TEXT("AM 118 · Oslo"), TEXT("AM 512 · Reykjavik"), TEXT("AM 230 · Amsterdam"), TEXT("AM 610 · Barcelona"), TEXT("AM 402 · Frankfurt"), TEXT("AM 120 · Vágar")};
	static const TCHAR* Times[] = {TEXT("06:40"), TEXT("07:35"), TEXT("07:20"), TEXT("07:55"), TEXT("08:10"), TEXT("08:35"), TEXT("09:00"), TEXT("09:25")};
	const int32 I = (Gate - 1) % 8;
	Bezel(O, "black", 1.7f, 0.98f, 0.08f, 0, 0, 0);
	Bezel(O, "screenPanel", 1.6f, 0.9f, 0.01f, 0, 0, 0.045f);
	Bezel(O, "yellow", 1.6f, 0.2f, 0.005f, 0, 0.35f, 0.05f);
	const float L = -0.74f;
	Text(O, FString::Printf(TEXT("GATE %d"), Gate), L, 0.35f, 0.055f, 1, 0.13f, FColor(20, 32, 46), true);
	Text(O, Flights[I], L, 0.1f, 0.055f, 1, 0.11f, FColor::White, true);
	Text(O, Gate % 3 == 0 ? TEXT("Boarding") : TEXT("Gate open"), L, -0.12f, 0.055f, 1, 0.1f, FColor(59, 209, 111), true);
	Text(O, FString(TEXT("Scheduled ")) + Times[I], L, -0.32f, 0.055f, 1, 0.08f, FColor(143, 166, 189), true);
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
	UE_LOG(LogTemp, Log, TEXT("Timetable screens built: %d timetables"), Boards.Num());
}

#pragma endregion

#pragma region Refresh

void ATerminalScreens::Refresh()
{
	for (int32 a = 0; a < 2; ++a)
	{
		const bool bArr = a == 1;
		const TArray<FRow> Rows = Sample(bArr, 8);
		for (FBoard& B : Boards)
		{
			if (B.bArr != bArr) continue;
			for (int32 r = 0; r < B.Rows.Num(); ++r)
			{
				const bool bHas = Rows.IsValidIndex(r);
				TArray<UTextRenderComponent*>& T = B.Rows[r];
				T[0]->SetText(FText::FromString(bHas ? Rows[r].Time : FString()));
				T[1]->SetText(FText::FromString(bHas ? Rows[r].Flight : FString()));
				T[2]->SetText(FText::FromString(bHas ? Rows[r].Place : FString()));
				T[3]->SetText(FText::FromString(bHas ? Rows[r].Status : FString()));
				if (bHas) T[3]->SetTextRenderColor(Rows[r].Col);
			}
		}
	}
}

void ATerminalScreens::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	if (Clock >= 4.f) { Clock = 0; Refresh(); }
}

#pragma endregion
