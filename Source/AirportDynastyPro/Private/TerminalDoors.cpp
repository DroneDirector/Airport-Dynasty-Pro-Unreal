// Every door in the plan, built and animated as in the Android edition (doors.ts): sliding, revolving, swing, secure,
// double, one-way, jetway and open doors, opening as the player comes up to them, with the room's name over the door.
#include "TerminalDoors.h"
#include "ADPLog.h"
#include "TerminalBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"

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
	ADP_LOG(TEXT("Doors built: %d"), Rigs.Num());
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
		// the drum: two curved glass sides (each a few straight panes, 108 degrees each) across the wall's line, so its two
		// openings (72 degrees each) face out to the kerb and in to the lobby - through the wall, as a revolving door's do.
		// (2026-11-21: they were turned a quarter, the openings facing the wall's own ends, and the side panes stood across
		//  the round - the Android edition's left-handed angles taken over as they were)
		for (const float Side : {0.f, 180.f})
			for (int32 i = 0; i < 6; ++i)
			{
				const float Ad = Side - 54.f + i * 18.f + 9.f, A = FMath::DegreesToRadians(Ad);
				UStaticMeshComponent* P = Part(N, "glass", R * 0.32f, H, 0.03f, FMath::Cos(A) * R, H / 2, FMath::Sin(A) * R);
				P->SetRelativeRotation(FRotator(0, Ad + 90.f, 0));   // (along the drum's round)
			}
		Part(N, "black", W + 0.24f, 0.3f, W + 0.24f, 0, H + 0.15f, 0);
		USceneComponent* Wings = Node(N, 0, 0, 0, 0);
		Part(Wings, "chrome", 0.12f, H - 0.1f, 0.12f, 0, H / 2, 0, false);
		constexpr int32 NWings = 4;   // (four wings: four compartments - RevolvingBoard)
		for (int32 i = 0; i < NWings; ++i)
		{
			const float A = i * 360.f / NWings, Rad = FMath::DegreesToRadians(A);
			UStaticMeshComponent* Wing = Part(Wings, "glass", R - 0.08f, H - 0.15f, 0.03f, FMath::Cos(Rad) * R / 2, H / 2, FMath::Sin(Rad) * R / 2, false);
			Wing->SetRelativeRotation(FRotator(0, A, 0));
		}
		Rig.Wings = Wings; Rig.Frame = N; Rig.NWings = NWings;
		FRevolve V; V.Rig = Rigs.Num(); V.Comps.SetNum(NWings); V.Omega = 3.f * 6.f;
		Revs.Add(V);
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

TArray<FVector> ATerminalDoors::Openers;
bool ATerminalDoors::bHoldSecurity = false;
bool ATerminalDoors::bHoldEntrances = false;

void ATerminalDoors::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	FVector P = Player ? Player->GetActorLocation() : FVector(1e9);
	const float PX = P.X / 100.f, PZ = -P.Y / 100.f;
	const int32 PF = P.Z / 100.f > F2Y - 1.f ? 2 : 1;

	for (FRevolve& V : Revs) StepRevolving(V, DeltaSeconds, P);
	for (FDoorRig& R : Rigs)
	{
		if (R.Wings) continue;   // (the revolving doors: StepRevolving)
		// (owner 2026-11-21: the entrance doors into the Grand Hall open 5 m ahead - they opened too late and people walked into them)
		const float Reach = R.D.Id.StartsWith(TEXT("D-F1-ENTR")) ? 5.f : FMath::Max(2.6f, R.D.Width * 0.7f);
		bool bNear = R.D.Floor == PF && FMath::Sqrt(FMath::Square(PX - R.D.X) + FMath::Square(PZ - R.D.Z)) < Reach;
		if (!bNear && Openers.Num())
		{
			// (the passengers walking through: PaxFlow)
			const float Rr = FMath::Square(Reach * 100.f);
			const FVector Dw = FTerminalPlan::ToWorld(R.D.X, R.D.Floor >= 2 ? F2Y : 0.f, R.D.Z);
			for (const FVector& O : Openers) if (FMath::Abs(O.Z - Dw.Z) < 150.f && FVector::DistSquared2D(O, Dw) < Rr) { bNear = true; break; }
		}
		// (owner 2026-11-21: the security doors open as soon as passengers are in the terminal and stay open until every one
		//  of them is out of the main hall and through security - they walked into the closed doors)
		if (bHoldSecurity && R.D.Id.StartsWith(TEXT("D-F1-SECURITY"))) bNear = true;
		// (owner 2026-11-21: the entrance sliding doors stay open while there are passengers in or around the terminal, and
		//  close only when the airport is empty - or, later, when the player closes them)
		if (bHoldEntrances && R.D.Id.StartsWith(TEXT("D-F1-ENTR")) && R.Slides.Num()) bNear = true;
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

#pragma region Revolving doors

namespace
{
	TWeakObjectPtr<ATerminalDoors> GDoors;
	constexpr float RevExitWin = 12.f;     // (degrees either side of lined up with the exit: step out)
	constexpr float RevMissed = 16.f;      // (past the exit by this much, still inside: the safety stop)
	float WrapDeg(float A) { return FMath::UnwindDegrees(A); }
	/** The opening on side Side: its local angle (the frame's +y is 90). */
	float OpeningDeg(int32 Side) { return Side > 0 ? 90.f : 270.f; }
}

ATerminalDoors* ATerminalDoors::Find(const UWorld* W)
{
	if (GDoors.IsValid() && GDoors->GetWorld() == W) return GDoors.Get();
	if (!W) return nullptr;
	for (TActorIterator<ATerminalDoors> It(const_cast<UWorld*>(W)); It; ++It) { GDoors = *It; return *It; }
	return nullptr;
}

int32 ATerminalDoors::NearestRevolving(const FVector& World, float* DistCm) const
{
	int32 Best = INDEX_NONE; float Bd = MAX_flt;
	for (int32 i = 0; i < Revs.Num(); ++i)
	{
		const USceneComponent* F = Rigs[Revs[i].Rig].Frame; if (!F) continue;
		const float D = FVector::Dist2D(F->GetComponentLocation(), World);
		if (D < Bd) { Bd = D; Best = i; }
	}
	if (DistCm) *DistCm = Bd;
	return Best;
}

bool ATerminalDoors::RevolvingFrame(int32 Ix, FVector& Centre, FVector& Axis, float& RadiusCm) const
{
	if (!Revs.IsValidIndex(Ix)) return false;
	const FDoorRig& R = Rigs[Revs[Ix].Rig]; if (!R.Frame) return false;
	Centre = R.Frame->GetComponentLocation();
	Axis = R.Frame->GetComponentTransform().TransformVectorNoScale(FVector(0, 1, 0)).GetSafeNormal2D();
	RadiusCm = R.D.Width * 50.f;
	return true;
}

FString ATerminalDoors::RevolvingId(int32 Ix) const { return Revs.IsValidIndex(Ix) ? Rigs[Revs[Ix].Rig].D.Id : FString(); }

int32 ATerminalDoors::RevolvingBoard(int32 Ix, int32 Side, int32 Who, const FString& Name)
{
	if (!Revs.IsValidIndex(Ix) || Side == 0) return INDEX_NONE;
	FRevolve& V = Revs[Ix];
	if (V.Mode != ERev::Run || V.Omega < 3.f) return INDEX_NONE;   // (stopped or turning back: wait)
	for (const FComp& C : V.Comps) if (C.Who == Who) return INDEX_NONE;
	const float Step = 360.f / V.Comps.Num();
	for (int32 k = 0; k < V.Comps.Num(); ++k)
	{
		FComp& C = V.Comps[k];
		if (C.Who != INDEX_NONE) continue;   // (one passenger or party a compartment - never strangers together)
		// its middle coming up to the opening: from 22 degrees short of it to just past (it moves on while they step in)
		const float D = WrapDeg(V.Angle + Step * (k + 0.5f) - OpeningDeg(Side));
		if (D < -22.f || D > 2.f) continue;
		C.Who = Who; C.Side = Side; C.bRiding = false; C.Progress = D; C.Name = Name;
		return k;
	}
	return INDEX_NONE;
}

void ATerminalDoors::RevolvingRiding(int32 Ix, int32 Comp)
{
	if (Revs.IsValidIndex(Ix) && Revs[Ix].Comps.IsValidIndex(Comp)) Revs[Ix].Comps[Comp].bRiding = true;
}

FVector ATerminalDoors::RevolvingSpot(int32 Ix, int32 Comp, float Frac, FVector* Dir, float* SpeedCm) const
{
	if (!Revs.IsValidIndex(Ix) || !Revs[Ix].Comps.IsValidIndex(Comp)) return FVector::ZeroVector;
	const FRevolve& V = Revs[Ix];
	const FDoorRig& R = Rigs[V.Rig];
	const float Rc = R.D.Width * 50.f * Frac;
	const float C = FMath::DegreesToRadians(V.Angle + 360.f / V.Comps.Num() * (Comp + 0.5f));
	const FTransform& T = R.Frame->GetComponentTransform();
	if (Dir) *Dir = T.TransformVectorNoScale(FVector(-FMath::Sin(C), FMath::Cos(C), 0.f) * (V.Omega >= 0.f ? 1.f : -1.f)).GetSafeNormal2D();
	if (SpeedCm) *SpeedCm = FMath::Abs(FMath::DegreesToRadians(V.Omega)) * Rc;
	return T.TransformPosition(FVector(FMath::Cos(C) * Rc, FMath::Sin(C) * Rc, 0.f));
}

bool ATerminalDoors::RevolvingAtExit(int32 Ix, int32 Comp, bool* bHeld) const
{
	if (bHeld) *bHeld = false;
	if (!Revs.IsValidIndex(Ix) || !Revs[Ix].Comps.IsValidIndex(Comp)) return false;
	const FRevolve& V = Revs[Ix];
	const FComp& C = V.Comps[Comp];
	if (C.Who == INDEX_NONE || !C.bRiding) return false;
	const bool bForMe = V.Safety == Comp;
	if (bForMe && V.Mode == ERev::Hold) { if (bHeld) *bHeld = true; return true; }
	if (bForMe && V.Mode != ERev::Run) return false;   // (being turned back to it: wait for the stop)
	return FMath::Abs(C.Progress - 180.f) <= RevExitWin;
}

void ATerminalDoors::RevolvingLeave(int32 Ix, int32 Comp)
{
	if (!Revs.IsValidIndex(Ix) || !Revs[Ix].Comps.IsValidIndex(Comp)) return;
	Revs[Ix].Comps[Comp] = FComp();
}

void ATerminalDoors::StepRevolving(FRevolve& V, float Dt, const FVector& Player)
{
	FDoorRig& R = Rigs[V.Rig];
	if (!R.Wings || !R.Frame) return;
	const FVector Ctr = R.Frame->GetComponentLocation();
	// the presence sensor: anyone within 6 m (the player, a passenger walking up or waiting) - and someone stepping in
	bool bNear = FMath::Abs(Player.Z - Ctr.Z) < 200.f && FVector::DistSquared2D(Player, Ctr) < FMath::Square(600.f);
	if (!bNear) for (const FVector& O : Openers) if (FMath::Abs(O.Z - Ctr.Z) < 200.f && FVector::DistSquared2D(O, Ctr) < FMath::Square(600.f)) { bNear = true; break; }
	bool bEntering = false;
	for (const FComp& C : V.Comps) if (C.Who != INDEX_NONE) { bNear = true; if (!C.bRiding) bEntering = true; }
	// (a real automatic revolving door this size turns 3-4 rpm: 3.6 with people about, a little slower while someone
	//  steps in, 3.0 idle. rpm x 6 = degrees a second)
	const float Rpm = bEntering ? 2.7f : bNear ? 3.6f : 3.0f;
	FComp* S = V.Comps.IsValidIndex(V.Safety) ? &V.Comps[V.Safety] : nullptr;
	V.ModeT += Dt;
	switch (V.Mode)
	{
	case ERev::Run:
		V.Omega = FMath::FInterpConstantTo(V.Omega, Rpm * 6.f, Dt, 4.f);
		// (the safety stop: someone still inside after their exit has gone by - stop; never carry them on into the glass)
		for (int32 k = 0; k < V.Comps.Num(); ++k)
		{
			const FComp& C = V.Comps[k];
			if (C.Who == INDEX_NONE || !C.bRiding || C.Progress - 180.f <= RevMissed) continue;
			V.Mode = ERev::Pause; V.ModeT = 0.f; V.Safety = k;
			ADP_WARN(TEXT("Revolving door %s: safety stop - %s in compartment %d did not step out at the exit (%.0f degrees past it); stopping, then turning back to the exit"), *R.D.Id, *C.Name, k + 1, C.Progress - 180.f);
			break;
		}
		break;
	case ERev::Pause:
		V.Omega = FMath::FInterpConstantTo(V.Omega, 0.f, Dt, 40.f);
		if (!S || S->Who == INDEX_NONE) { V.Mode = ERev::Run; V.ModeT = 0.f; V.Safety = INDEX_NONE; break; }
		if (V.ModeT > 1.5f && FMath::Abs(V.Omega) < 0.1f) { V.Mode = ERev::Back; V.ModeT = 0.f; }
		break;
	case ERev::Back:
		// (slowly back, one turn a minute, to the exit; they ride back with it)
		V.Omega = FMath::FInterpConstantTo(V.Omega, -6.f, Dt, 12.f);
		if (!S || S->Who == INDEX_NONE || S->Progress <= 180.f + 2.f || V.ModeT > 20.f) { V.Mode = ERev::Hold; V.ModeT = 0.f; }
		break;
	case ERev::Hold:
		V.Omega = FMath::FInterpConstantTo(V.Omega, 0.f, Dt, 40.f);
		if (!S || S->Who == INDEX_NONE || V.ModeT > 10.f)
		{
			if (S && S->Who != INDEX_NONE) ADP_WARN(TEXT("Revolving door %s: %s still in compartment %d after 10 s at the exit - turning on"), *R.D.Id, *S->Name, V.Safety + 1);
			else ADP_LOG(TEXT("Revolving door %s: safety stop over - they are out at the exit; turning on"), *R.D.Id);
			V.Mode = ERev::Run; V.ModeT = 0.f; V.Safety = INDEX_NONE;
		}
		break;
	}
	const float dA = V.Omega * Dt;
	V.Angle = FMath::Fmod(V.Angle + dA + 360.f, 360.f);
	for (FComp& C : V.Comps) if (C.Who != INDEX_NONE) C.Progress += dA;
	R.Wings->SetRelativeRotation(FRotator(0, V.Angle, 0));
}

#pragma endregion
