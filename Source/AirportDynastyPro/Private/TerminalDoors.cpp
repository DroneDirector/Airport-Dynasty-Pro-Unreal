// Every door in the plan, built and animated as in the Android edition (doors.ts): sliding, revolving, swing, secure,
// double, one-way, jetway and open doors, opening as the player comes up to them, with the room's name over the door.
#include "TerminalDoors.h"
#include "TerminalBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

#pragma region Setup

ATerminalDoors::ATerminalDoors()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UMaterialInterface* ATerminalDoors::M(const FName Name)
{
	if (Name == "glass" && GlassM) return GlassM;
	if (TObjectPtr<UMaterialInterface>* Found = Mats.Find(Name)) return *Found;
	UMaterialInterface* Mat = ATerminalBuilder::KitMaterial(this, Name);
	Mats.Add(Name, Mat);
	return Mat;
}

USceneComponent* ATerminalDoors::Node(USceneComponent* Parent, float X, float Y, float Z, float YawDeg)
{
	USceneComponent* N = NewObject<USceneComponent>(this);
	N->SetupAttachment(Parent);
	N->SetMobility(EComponentMobility::Movable);
	N->SetRelativeLocationAndRotation(FVector(X * 100.f, Z * 100.f, Y * 100.f), FRotator(0, YawDeg, 0));
	N->RegisterComponent();
	AddInstanceComponent(N);
	return N;
}

UStaticMeshComponent* ATerminalDoors::Part(USceneComponent* Parent, const FName Mat, float W, float H, float Dp, float X, float Y, float Z, bool bCollide)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Parent);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Cube);
	C->SetMaterial(0, M(Mat));
	// the door's frame: x across the opening, y up, z through it; the engine cube is 100 cm with its pivot in the middle
	C->SetRelativeLocation(FVector(X * 100.f, Z * 100.f, Y * 100.f));
	C->SetRelativeScale3D(FVector(W, Dp, H));
	C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	if (Mat == "glass") C->SetCastShadow(false);
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

#pragma endregion

#pragma region Build

void ATerminalDoors::Build(const FTerminalPlan& Plan)
{
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	GlassM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glass.M_Glass"));
	if (!Cube) return;
	F2Y = Plan.F2Y;
	for (const FPlanDoor& D : Plan.Doors)
		if (D.Type != TEXT("museum")) BuildDoor(D, Plan); // the museum has its own portal
	UE_LOG(LogTemp, Log, TEXT("Doors built: %d"), Rigs.Num());
}

void ATerminalDoors::BuildDoor(const FPlanDoor& D, const FTerminalPlan& Plan)
{
	const float H = D.Height(), W = D.Width;
	const FString& T = D.Type;
	FDoorRig Rig;
	Rig.D = D;

	// the door's own frame: along x it faces the plan's z; along z it is turned a quarter
	USceneComponent* N = NewObject<USceneComponent>(this);
	N->SetupAttachment(Root);
	N->SetMobility(EComponentMobility::Movable);
	N->SetRelativeLocationAndRotation(FTerminalPlan::ToWorld(D.X, Plan.FloorY(D.Floor), D.Z), FRotator(0, D.bAlongX ? 0.f : -90.f, 0));
	N->RegisterComponent();
	AddInstanceComponent(N);

	#pragma region Frame
	const FName Frame = T == TEXT("secure") ? FName("steel") : (T == TEXT("swing") || T == TEXT("double")) ? FName("walnut") : FName("black");
	if (T != TEXT("open"))
	{
		Part(N, Frame, 0.1f, H, 0.32f, -W / 2 - 0.05f, H / 2, 0);
		Part(N, Frame, 0.1f, H, 0.32f, W / 2 + 0.05f, H / 2, 0);
		Part(N, Frame, W + 0.3f, 0.14f, 0.32f, 0, H + 0.07f, 0);
	}
	else Part(N, "mullion", W + 0.2f, 0.12f, 0.34f, 0, H + 0.06f, 0);
	#pragma endregion

	#pragma region Leaves
	if (T == TEXT("slide") || T == TEXT("oneway") || T == TEXT("jetway"))
	{
		for (const float S : {-1.f, 1.f})
		{
			Rig.Slides.Add({Part(N, "glass", W / 2 - 0.02f, H - 0.04f, 0.04f, S * W / 4, (H - 0.04f) / 2, 0), S * W / 4, S});
			Rig.Slides.Add({Part(N, "black", 0.05f, H - 0.04f, 0.06f, S * (W / 2 - 0.03f), (H - 0.04f) / 2, 0), S * (W / 2 - 0.03f), S});
			Rig.Slides.Add({Part(N, "chrome", W / 2 - 0.1f, 0.06f, 0.05f, S * W / 4, 1.05f, 0, false), S * W / 4, S});
		}
		Part(N, "black", W + 0.1f, 0.22f, 0.2f, 0, H - 0.11f, 0.14f); // the sliding gear's cover
		if (T == TEXT("oneway"))
		{
			UTextRenderComponent* Sign = NewObject<UTextRenderComponent>(this);
			Sign->SetupAttachment(N);
			Sign->SetText(FText::FromString(TEXT("NO RE-ENTRY")));
			Sign->SetHorizontalAlignment(EHTA_Center);
			Sign->SetVerticalAlignment(EVRTA_TextCenter);
			Sign->SetWorldSize(22.f);
			Sign->SetTextRenderColor(FColor::White);
			Sign->SetRelativeLocationAndRotation(FVector(0, -24.f, (H + 0.4f) * 100.f), FRotator(0, -90.f, 0));
			Sign->RegisterComponent();
			AddInstanceComponent(Sign);
			Part(N, "red", 2.4f, 0.4f, 0.03f, 0, H + 0.4f, -0.2f, false);
		}
	}
	else if (T == TEXT("swing") || T == TEXT("secure") || T == TEXT("double"))
	{
		const bool bDouble = T == TEXT("double");
		const float LW = bDouble ? W / 2 : W;
		const bool bPale = T == TEXT("swing") && (D.A.Contains(TEXT("WC")) || D.A.Contains(TEXT("CLEAN")));
		const FName Leaf = T == TEXT("secure") ? FName("steel") : bPale ? FName("offwhite") : FName("oak");
		const bool bWindow = T != TEXT("secure") && (D.A.Contains(TEXT("OFFICE")) || D.A.Contains(TEXT("ACADEMY")) || D.A.Contains(TEXT("CREW")));
		TArray<float> Sides = {-1.f};
		if (bDouble) Sides.Add(1.f);
		for (const float S : Sides)
		{
			USceneComponent* Hinge = Node(N, S * W / 2, 0, 0, 0);
			Part(Hinge, Leaf, LW - 0.04f, H - 0.03f, 0.05f, -S * LW / 2, (H - 0.03f) / 2, 0);
			Part(Hinge, "chrome", 0.03f, 0.03f, 0.16f, -S * (LW - 0.12f), 1.0f, 0, false);
			if (bWindow) Part(Hinge, "glass", LW * 0.35f, 0.6f, 0.06f, -S * LW / 2, 1.5f, 0, false);
			Rig.Hinges.Add({Hinge, S});
		}
		if (T == TEXT("secure"))
		{
			Part(N, "black", 0.09f, 0.14f, 0.05f, W / 2 + 0.28f, 1.25f, -0.2f, false); // the card reader
			Part(N, "red", 0.04f, 0.02f, 0.06f, W / 2 + 0.28f, 1.3f, -0.23f, false);   // its light
		}
	}
	else if (T == TEXT("revolving"))
	{
		const float R = W / 2;
		// the drum: two curved glass sides (each a few straight panes), open towards the two sides of the wall
		for (const float Side : {0.f, 180.f})
			for (int32 i = 0; i < 6; ++i)
			{
				const float A = FMath::DegreesToRadians(Side + 36.f + i * 18.f + 9.f);
				UStaticMeshComponent* P = Part(N, "glass", R * 0.32f, H, 0.03f, FMath::Cos(A) * R, H / 2, FMath::Sin(A) * R);
				P->SetRelativeRotation(FRotator(0, -FMath::RadiansToDegrees(A) + 90.f, 0));
			}
		Part(N, "black", W + 0.24f, 0.3f, W + 0.24f, 0, H + 0.15f, 0);
		USceneComponent* Wings = Node(N, 0, 0, 0, 0);
		Part(Wings, "chrome", 0.12f, H - 0.1f, 0.12f, 0, H / 2, 0, false);
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = i * 90.f, Rad = FMath::DegreesToRadians(A);
			UStaticMeshComponent* Wing = Part(Wings, "glass", R - 0.08f, H - 0.15f, 0.03f, FMath::Cos(Rad) * R / 2, H / 2, FMath::Sin(Rad) * R / 2, false);
			Wing->SetRelativeRotation(FRotator(0, -A, 0));
		}
		Rig.Wings = Wings;
	}
	#pragma endregion

	#pragma region Name sign
	// the room's name over the door, on the public side (the side away from the room)
	static const TArray<FString> Public = {TEXT("HALL"), TEXT("AIRSIDE"), TEXT("LANDING"), TEXT("FOOD"), TEXT("WALK"), TEXT("BAR"), TEXT("SECURITY"), TEXT("STAFF-CORR"), TEXT("BAGHALL"), TEXT("KERB")};
	const FPlanRoom* Room = Plan.Rooms.FindByPredicate([&](const FPlanRoom& R) { return R.Id == D.A; });
	bool bPublic = false;
	for (const FString& P : Public) bPublic |= D.B.Contains(P);
	if (Room && bPublic && !D.B.StartsWith(TEXT("J-")))
	{
		FString Name = Room->Name;
		int32 Paren;
		if (Name.FindChar(TEXT('('), Paren)) Name = Name.Left(Paren).TrimEnd();
		const float Away = D.bAlongX ? -FMath::Sign(D.Z - (Room->Z0 + Room->Z1) / 2) : FMath::Sign(D.X - (Room->X0 + Room->X1) / 2);
		const float SW = FMath::Clamp(Name.Len() * 0.2f, 1.8f, 4.2f);
		Part(N, "navy", SW, 0.42f, 0.03f, 0, H + 0.42f, Away * 0.2f, false);
		UTextRenderComponent* Txt = NewObject<UTextRenderComponent>(this);
		Txt->SetupAttachment(N);
		Txt->SetText(FText::FromString(Name.ToUpper()));
		Txt->SetHorizontalAlignment(EHTA_Center);
		Txt->SetVerticalAlignment(EVRTA_TextCenter);
		Txt->SetWorldSize(24.f);
		Txt->SetTextRenderColor(FColor(242, 244, 247));
		Txt->SetRelativeLocationAndRotation(FVector(0, Away * 22.f, (H + 0.42f) * 100.f), FRotator(0, Away > 0 ? 90.f : -90.f, 0));
		Txt->RegisterComponent();
		AddInstanceComponent(Txt);
	}
	#pragma endregion

	Rigs.Add(MoveTemp(Rig));
}

#pragma endregion

#pragma region Opening and shutting

void ATerminalDoors::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	FVector P = Player ? Player->GetActorLocation() : FVector(1e9);
	const float PX = P.X / 100.f, PZ = -P.Y / 100.f;
	const int32 PF = P.Z / 100.f > F2Y - 1.f ? 2 : 1;

	for (FDoorRig& R : Rigs)
	{
		if (R.Wings) { R.Wings->SetRelativeRotation(FRotator(0, FMath::RadiansToDegrees(Time * 0.6f), 0)); continue; }
		const bool bNear = R.D.Floor == PF && FMath::Sqrt(FMath::Square(PX - R.D.X) + FMath::Square(PZ - R.D.Z)) < FMath::Max(2.6f, R.D.Width * 0.7f);
		const float Want = bNear ? 1.f : 0.f;
		if (FMath::Abs(R.K - Want) < 0.001f) continue;
		const float Speed = R.Slides.Num() ? 1.8f : 1.4f;
		R.K += FMath::Sign(Want - R.K) * FMath::Min(FMath::Abs(Want - R.K), DeltaSeconds * Speed);
		const float E = R.K * R.K * (3.f - 2.f * R.K); // ease in and out
		for (const FSlide& S : R.Slides)
		{
			FVector L = S.Part->GetRelativeLocation();
			L.X = (S.X0 + S.Dir * E * (R.D.Width / 2 - 0.08f)) * 100.f;
			S.Part->SetRelativeLocation(L);
		}
		for (const FHinge& Hg : R.Hinges)
			Hg.Leaf->SetRelativeRotation(FRotator(0, Hg.Dir * E * 86.f, 0));
	}
}

#pragma endregion
