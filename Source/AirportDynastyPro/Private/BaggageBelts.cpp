#include "BaggageBelts.h"
#include "ADPLog.h"
#include "ADPKeep.h"
#include "InteractOutline.h"
#include "TerminalPlan.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"

namespace
{
	constexpr float BL = 15.f, BR = 1.8f, CZ = 31.f;               // a belt's straight, its centre-line radius, its middle (plan z)
	constexpr float PER = 2.f * BL + 2.f * PI * BR;                  // 41.3 m round
	constexpr float SPEED = 0.46f, CONV = 0.6f;                      // the plates', the conveyors' (m/s)
	constexpr float TRUNKZ = 63.f, WALLZ = 52.2f, STUB0 = 62.45f;    // the long conveyor's line, the claim wall, the input's far end
	constexpr float DROP_S = BL / 2.f + 6.f;                          // where the feed drops a bag on the plates (east side, by the shaft)
	const float BELTX[] = {-60.8f, -48.15f, -35.5f, -22.85f, -10.2f};
	const float STATIONS[] = {-58.6f, 21.95f, 102.5f, 183.05f};       // the loading points: south of each ramp-services door
	const TCHAR* BAGM[] = {TEXT("SM_Bag0"), TEXT("SM_Bag1"), TEXT("SM_Bag2"), TEXT("SM_Bag3")};
	UStaticMesh* Mesh(const TCHAR* N) { return ADPKeep(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Baggage/%s.%s"), N, N))); }
	TWeakObjectPtr<ABaggageBelts> GBelts;
}

ABaggageBelts::ABaggageBelts()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

ABaggageBelts* ABaggageBelts::Get(UWorld* Wo)
{
	if (GBelts.IsValid()) return GBelts.Get();
	if (!Wo) return nullptr;
	for (TActorIterator<ABaggageBelts> It(Wo); It; ++It) { GBelts = *It; return *It; }
	return nullptr;
}

FVector ABaggageBelts::W(float X, float H, float Z) const { return FTerminalPlan::ToWorld(X, H + 0.01f, Z); }

FVector2f ABaggageBelts::PathAt(float S, FVector2f& Dir) const
{
	S = FMath::Fmod(S, PER); if (S < 0.f) S += PER;
	if (S < BL) { Dir = FVector2f(0, 1); return FVector2f(BR, -BL / 2 + S); }
	S -= BL;
	if (S < PI * BR) { const float A = S / BR; Dir = FVector2f(-FMath::Sin(A), FMath::Cos(A)); return FVector2f(BR * FMath::Cos(A), BL / 2 + BR * FMath::Sin(A)); }
	S -= PI * BR;
	if (S < BL) { Dir = FVector2f(0, -1); return FVector2f(-BR, BL / 2 - S); }
	S -= BL; const float A = PI + S / BR; Dir = FVector2f(-FMath::Sin(A), FMath::Cos(A)); return FVector2f(BR * FMath::Cos(A), -BL / 2 + BR * FMath::Sin(A));
}

static float YawOf(const FVector2f& Dir)   // the yaw that turns a part's local +Y (plan +z) to this plan direction
{
	const FVector Wd = FTerminalPlan::ToWorld(Dir.X, 0, Dir.Y);
	return FMath::RadiansToDegrees(FMath::Atan2(Wd.Y, Wd.X)) + 90.f;
}

void ABaggageBelts::Build()
{
	GBelts = this;
	UStaticMesh* Body = Mesh(TEXT("SM_BeltBody")); UStaticMesh* Plate = Mesh(TEXT("SM_BeltPlate")); UStaticMesh* Incline = Mesh(TEXT("SM_BeltIncline"));
	UStaticMesh* Flap = Mesh(TEXT("SM_Flap")); UStaticMesh* Btn = Mesh(TEXT("SM_BeltButton")); UStaticMesh* BtnPlate = Mesh(TEXT("SM_ButtonPlate"));
	UStaticMesh* Crew = Mesh(TEXT("SM_CrewBox")); UStaticMesh* Conv = Mesh(TEXT("SM_InputConv")); UStaticMesh* Frame = Mesh(TEXT("SM_WallFrame")); UStaticMesh* Scr = Mesh(TEXT("SM_BeltScreen"));
	if (!Body || !Plate || !Flap || !Btn || !Conv) { ADP_WARN(TEXT("Baggage belts: the parts are missing (Tools/import_baggage.py)")); return; }
	auto Still = [this](UStaticMesh* M, const FVector& At, const FRotator& R, const FVector& Sc = FVector(1)) -> UStaticMeshComponent*
	{
		if (!M) return nullptr;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this); C->SetStaticMesh(M); C->SetupAttachment(GetRootComponent());
		C->SetWorldLocationAndRotation(At, R); C->SetWorldScale3D(Sc); C->RegisterComponent(); Keep.Add(C); return C;
	};
	auto List = [this](UStaticMesh* M, bool bCollide) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this); K->SetStaticMesh(M); K->SetupAttachment(GetRootComponent());
		K->SetMobility(EComponentMobility::Movable); K->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		K->RegisterComponent(); Keep.Add(K); return K;
	};
	UInstancedStaticMeshComponent* FlapK = List(Flap, false);
	USoundBase* SStart = LoadObject<USoundBase>(nullptr, TEXT("/Game/Baggage/Sounds/BeltStart.BeltStart"));
	USoundBase* SRun = LoadObject<USoundBase>(nullptr, TEXT("/Game/Baggage/Sounds/BeltRun.BeltRun"));
	USoundBase* SClack = LoadObject<USoundBase>(nullptr, TEXT("/Game/Baggage/Sounds/BeltClack.BeltClack"));
	USoundAttenuation* Att = NewObject<USoundAttenuation>(this); Keep.Add(Att);
	Att->Attenuation.bAttenuate = true; Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Att->Attenuation.AttenuationShapeExtents = FVector(600.f); Att->Attenuation.FalloffDistance = 4000.f;   // (loud close by, gone at about 46 m)
	Att->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; Att->Attenuation.bSpatialize = true;
	auto Audio = [&](USoundBase* S, const FVector& At) -> UAudioComponent*
	{
		if (!S) return nullptr;
		UAudioComponent* A = NewObject<UAudioComponent>(this); A->SetupAttachment(GetRootComponent()); A->SetSound(S); A->bAutoActivate = false;
		A->AttenuationSettings = Att; A->SetWorldLocation(At); A->SetLowPassFilterEnabled(true); A->RegisterComponent(); Keep.Add(A); return A;
	};
	// the long conveyor along the sorting hall (the corridor under the gates' walkway), from its east end to past belt 1
	Still(Conv, W(-62.f, -0.01f, TRUNKZ), FRotator(0, 90.f, 0), FVector(1, 190.05f + 62.f, 1));
	for (int32 b = 0; b < UE_ARRAY_COUNT(BELTX); ++b)
	{
		FBelt Bt; Bt.X = BELTX[b];
		const float X = Bt.X;
		Still(Body, W(X, 0.f, CZ), FRotator::ZeroRotator);
		// the incline out of the shaft, 16.6 degrees, its middle at (0.525, 6.0) a little under the plates
		{
			const FRotator R(16.6f, 0, 0); const FVector Piv = FVector(52.5f, -600.f, 0.f);
			Still(Incline, W(X + 0.525f, 0.16f, CZ + 6.f) - R.RotateVector(Piv), R);
		}
		Bt.Plates = List(Plate, true);
		const int32 NP = FMath::FloorToInt(PER / 0.3f);
		for (int32 k = 0; k < NP; ++k) { FVector2f D; const FVector2f P = PathAt(k * PER / NP, D); Bt.Plates->AddInstance(FTransform(FRotator(0, YawOf(D), 0), W(X + P.X, 0.425f, CZ + P.Y)), true); }
		// the hood's flaps: 8 strips across its mouth (facing east), hung from the hinge bar
		Bt.FirstFlap = Flaps.Num();
		for (int32 k = 0; k < 8; ++k)
		{
			FFlap F; F.K = FlapK; F.Hinge = W(X + 0.74f, 1.13f, CZ + 5.53f + k * 0.135f); F.Base = FRotator(0, 90.f, 0).Quaternion(); F.Axis = FVector(0, 1, 0);
			F.Ix = FlapK->AddInstance(FTransform(F.Base, F.Hinge, FVector(1, 1, 1)), true); Flaps.Add(F);
		}
		// the crew side: the input conveyor from the long one to the wall, the wall's yellow frame round a dark opening,
		// its flaps, the crew's control box
		Still(Conv, W(X, -0.01f, WALLZ), FRotator::ZeroRotator, FVector(1, STUB0 - WALLZ, 1));
		Still(Frame, W(X, 0.f, 52.18f), FRotator::ZeroRotator);
		Still(Scr, W(X, 1.25f, 52.09f), FRotator::ZeroRotator, FVector(1.4f / 2.12f, 0.2f, 1.05f / 1.2f));
		// the crew's sign over the opening (its plate is on the frame's part): which belt this input feeds
		{
			UTextRenderComponent* T = NewObject<UTextRenderComponent>(this); T->SetupAttachment(GetRootComponent()); T->RegisterComponent(); Keep.Add(T);
			T->SetWorldLocationAndRotation(W(X, 2.21f, 52.18f + 0.045f), FRotator(0, -90.f, 0));
			T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter); T->SetWorldSize(26.f);
			T->SetTextRenderColor(FColor(245, 196, 40)); T->SetText(FText::FromString(FString::Printf(TEXT("BELT %d"), b + 1)));
		}
		for (int32 k = 0; k < 10; ++k)
		{
			FFlap F; F.K = FlapK; F.Hinge = W(X - 0.63f + k * 0.14f, 1.73f, 52.22f); F.Base = FQuat::Identity; F.Axis = FVector(1, 0, 0);
			F.Ix = FlapK->AddInstance(FTransform(F.Base, F.Hinge, FVector(1.1f, 1, 2.2f)), true); Flaps.Add(F);
		}
		Bt.NumFlaps = Flaps.Num() - Bt.FirstFlap;
		Still(Crew, W(X - 1.3f, 0.f, 60.6f), FRotator::ZeroRotator);
		Bt.CrewButton = Still(Btn, W(X - 1.3f, 1.16f, 60.6f + 0.16f), FRotator(0, -90.f, 0));
		// the passengers' start / stop button on the skirt's east side, by the shaft
		Still(BtnPlate, W(X + 2.49f, 0.24f, CZ + 3.2f), FRotator::ZeroRotator);
		Bt.Button = Still(Btn, W(X + 2.47f, 0.24f, CZ + 3.2f), FRotator::ZeroRotator);
		if (Bt.Button) Bt.BtnBase = Bt.Button->GetComponentLocation(); if (Bt.CrewButton) Bt.CrewBase = Bt.CrewButton->GetComponentLocation();
		for (UStaticMeshComponent* C : {Bt.Button.Get(), Bt.CrewButton.Get()}) if (C) { C->SetCollisionProfileName(TEXT("BlockAll")); AInteractOutline::Add(C, INDEX_NONE, (uint64)C); }
		// its screen over the north end: the belt's number and the flight on it
		Still(Scr, W(X, 3.56f, CZ + 10.45f), FRotator::ZeroRotator);
		Bt.Screen = NewObject<UTextRenderComponent>(this); Bt.Screen->SetupAttachment(GetRootComponent()); Bt.Screen->RegisterComponent(); Keep.Add(Bt.Screen);
		Bt.Screen->SetWorldLocationAndRotation(W(X, 3.56f, CZ + 10.39f), FRotator(0, 90.f, 0));
		Bt.Screen->SetHorizontalAlignment(EHTA_Center); Bt.Screen->SetVerticalAlignment(EVRTA_TextCenter); Bt.Screen->SetWorldSize(16.f);
		Bt.Screen->SetTextRenderColor(FColor(245, 196, 40)); Bt.Screen->SetText(FText::FromString(FString::Printf(TEXT("BELT %d"), b + 1)));
		// its sounds, at the belt's middle (the start and the run), the clack by the drop
		Bt.SStart = Audio(SStart, W(X, 0.5f, CZ)); Bt.SRun = Audio(SRun, W(X, 0.5f, CZ)); Bt.SClack = Audio(SClack, W(X + 1.8f, 0.5f, CZ + 6.f));
		Belts.Add(Bt);
	}
	FlapK->MarkRenderStateDirty();
	for (const TCHAR* N : BAGM) { UInstancedStaticMeshComponent* K = List(Mesh(N), true); BagMeshes.Add(K); }
	FreeIx.SetNum(BagMeshes.Num());
	ADP_LOG(TEXT("Baggage belts: %d belts, %d flaps, the long conveyor from x 190 to -62"), Belts.Num(), Flaps.Num());
	// -BaggageTest: bags straight onto every belt's input (and some on the long conveyor), each belt started by its crew
	if (FParse::Param(FCommandLine::Get(), TEXT("BaggageTest")))
		for (int32 b = 0; b < Belts.Num(); ++b) { Belts[b].Flight = FString::Printf(TEXT("TEST %d"), b + 1); Unload(b, 14, 4); Unload(b, 6, 1); }
}

int32 ABaggageBelts::NewBag(int32 Belt, int32 Station)
{
	FBag B; B.Belt = Belt; B.Kind = FMath::RandRange(0, BagMeshes.Num() - 1); B.Yaw = FMath::FRandRange(-12.f, 12.f); B.Side = FMath::FRandRange(-0.1f, 0.1f);
	B.Stage = EStage::Trunk; B.P = STATIONS[FMath::Clamp(Station, 0, 3)];
	if (Station >= 4 || B.P < BELTX[Belt]) { B.Stage = EStage::Stub; B.P = 0.f; }   // (4: straight onto the input - the tests; a loading point west of its belt: the same)
	UInstancedStaticMeshComponent* K = BagMeshes[B.Kind];
	if (FreeIx[B.Kind].Num()) B.Ix = FreeIx[B.Kind].Pop();
	else B.Ix = K->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, 0, -100000.f), FVector(0.001f)), true);
	return Bags.Add(B);
}

void ABaggageBelts::Unload(int32 Belt, int32 Count, int32 Station)
{
	if (!Belts.IsValidIndex(Belt)) return;
	if (Station < 0) { Station = 1; for (int32 s = 0; s < 4; ++s) if (STATIONS[s] > BELTX[Belt]) { Station = s; break; } }
	Belts[Belt].Waiting += Count;
	Unloads.Add({Belt, Station, Count, Station >= 4 ? 1.6f : 2.5f, 0.f});
}

void ABaggageBelts::OnArrived(UWorld* Wo, float PlanX, const FString& Flight)
{
	ABaggageBelts* Me = Get(Wo); if (!Me || Me->Belts.Num() == 0) return;
	// the loading point nearest the aircraft; a belt that is free (the one with the fewest bags on it and coming)
	int32 St = 0; for (int32 s = 1; s < 4; ++s) if (FMath::Abs(STATIONS[s] - PlanX) < FMath::Abs(STATIONS[St] - PlanX)) St = s;
	int32 Best = INDEX_NONE, Load = MAX_int32;
	for (int32 b = 0; b < Me->Belts.Num(); ++b)
	{
		if (STATIONS[St] < BELTX[b] && St != 0) continue;
		int32 N = Me->Belts[b].Waiting; for (const FBag& B : Me->Bags) if (B.Stage != EStage::Gone && B.Belt == b) ++N;
		if (N < Load) { Load = N; Best = b; }
	}
	if (Best == INDEX_NONE) return;
	Me->Belts[Best].Flight = Flight;
	if (Me->Belts[Best].Screen) Me->Belts[Best].Screen->SetText(FText::FromString(FString::Printf(TEXT("BELT %d   %s"), Best + 1, *Flight)));
	// the crew start unloading the hold a few minutes after it parks: a bag every 2.5 s
	const int32 N = FMath::RandRange(35, 70);
	Me->Unloads.Add({Best, St, N, 2.5f, -150.f});
	Me->Belts[Best].Waiting += N;
	ADP_LOG(TEXT("Baggage: flight %s parked (plan x %.0f): %d bags to belt %d from loading point %d"), *Flight, PlanX, N, Best + 1, St + 1);
}

void ABaggageBelts::SetRunning(int32 b, bool bOn)
{
	if (!Belts.IsValidIndex(b) || Belts[b].bRun == bOn) return;
	FBelt& Bt = Belts[b]; Bt.bRun = bOn; Bt.Idle = 0.f;
	if (bOn) { if (Bt.SStart) Bt.SStart->Play(); if (Bt.SRun) Bt.SRun->FadeIn(0.8f, 1.f, FMath::FRand() * 10.f); }
	else if (Bt.SRun) Bt.SRun->FadeOut(1.5f, 0.f);   // (the motor running down)
	ADP_LOG(TEXT("Baggage: belt %d %s"), b + 1, bOn ? TEXT("started") : TEXT("stopped"));
}

bool ABaggageBelts::Interact(UPrimitiveComponent* Hit, const FVector& Where)
{
	// a button you look at or stand right by: pressed (in and out), the belt started or stopped
	for (int32 b = 0; b < Belts.Num(); ++b)
	{
		FBelt& Bt = Belts[b];
		const bool bP = Bt.Button && (Hit == Bt.Button || FVector::Dist(Where, Bt.Button->GetComponentLocation()) < 120.f);
		const bool bC = Bt.CrewButton && (Hit == Bt.CrewButton || FVector::Dist(Where, Bt.CrewButton->GetComponentLocation()) < 120.f);
		if (bP || bC) { (bP ? Bt.Press : Bt.CrewPress) = 0.35f; SetRunning(b, !Bt.bRun); return true; }
	}
	// a bag within reach: taken off (in the claim hall by you as a passenger, at the input by you as crew)
	for (FBag& B : Bags)
	{
		if (B.Stage != EStage::Belt && B.Stage != EStage::Stub && B.Stage != EStage::Trunk) continue;
		FTransform T; BagMeshes[B.Kind]->GetInstanceTransform(B.Ix, T, true);
		if (FVector::Dist(T.GetLocation(), Where) < 140.f || (Hit == BagMeshes[B.Kind] && FVector::Dist(T.GetLocation(), Where) < 250.f))
		{
			B.Stage = EStage::Gone; PlaceBag(B); FreeIx[B.Kind].Add(B.Ix); BagMeshes[B.Kind]->MarkRenderStateDirty();
			ADP_LOG(TEXT("Baggage: a bag taken off belt %d"), B.Belt + 1); return true;
		}
	}
	return false;
}

bool ABaggageBelts::Free(int32 Belt, EStage St, float P, float Gap, int32 Except) const
{
	for (int32 i = 0; i < Bags.Num(); ++i)
	{
		const FBag& O = Bags[i];
		if (i == Except || O.Stage != St || (St != EStage::Trunk && O.Belt != Belt)) continue;
		float D = O.P - P; if (St == EStage::Belt) { D = FMath::Fmod(D + PER * 2.f, PER); if (D > PER / 2) D -= PER; }
		if (St == EStage::Trunk) D = -D;   // (the long conveyor runs west: ahead is a smaller x)
		if (D > -0.01f && D < Gap) return false;
	}
	return true;
}

void ABaggageBelts::PlaceBag(FBag& B)
{
	UInstancedStaticMeshComponent* K = BagMeshes[B.Kind];
	const float X = BELTX[B.Belt];
	FTransform T(FRotator::ZeroRotator, FVector(0, 0, -100000.f), FVector(0.001f));
	switch (B.Stage)
	{
	case EStage::Trunk: T = FTransform(FRotator(0, 90.f + B.Yaw, 0), W(B.P, 0.75f, TRUNKZ + B.Side)); break;
	case EStage::Stub: T = FTransform(FRotator(0, B.Yaw, 0), W(X + B.Side, 0.75f, STUB0 - B.P)); break;
	case EStage::Rise:
	{
		const float U = FMath::Clamp(B.P, 0.f, 1.f);
		T = FTransform(FRotator(16.6f * (U < 0.95f ? 1.f : 0.f), 90.f, 0), W(X - 0.3f + 1.95f * U, -0.45f + 0.9f * FMath::Min(U * 1.05f, 1.f), CZ + 6.f + B.Side * 0.5f));
		break;
	}
	case EStage::Belt:
	{
		FVector2f D; const FVector2f P = PathAt(B.P, D);
		const FVector2f N(D.Y, -D.X);
		T = FTransform(FRotator(0, YawOf(D) + 90.f + B.Yaw, 0), W(X + P.X + N.X * B.Side, 0.45f, CZ + P.Y + N.Y * B.Side));
		break;
	}
	default: break;
	}
	K->UpdateInstanceTransform(B.Ix, T, true, false, true);
}

void ABaggageBelts::Tick(float Dt)
{
	Super::Tick(Dt);
	if (Belts.Num() == 0) return;
	Dt = FMath::Min(Dt, 0.1f);
	// the crew unloading the holds onto the long conveyor, a bag at a time
	for (int32 u = Unloads.Num() - 1; u >= 0; --u)
	{
		FUnload& U = Unloads[u];
		if ((U.In += Dt) < U.Every) continue;
		const float StartP = STATIONS[FMath::Min(U.Station, 3)];
		const bool bRoom = U.Station >= 4 || StartP < BELTX[U.Belt] ? Free(U.Belt, EStage::Stub, 0.f, 0.9f, INDEX_NONE) : Free(U.Belt, EStage::Trunk, StartP, 0.9f, INDEX_NONE);
		if (!bRoom) continue;
		U.In = 0.f; NewBag(U.Belt, U.Station); --U.Left; Belts[U.Belt].Waiting = FMath::Max(0, Belts[U.Belt].Waiting - 1);
		if (U.Left <= 0) Unloads.RemoveAt(u);
	}
	// every bag on its way: fixed to what carries it, never through the one ahead
	for (int32 i = 0; i < Bags.Num(); ++i)
	{
		FBag& B = Bags[i]; if (B.Stage == EStage::Gone) continue;
		FBelt& Bt = Belts[B.Belt];
		switch (B.Stage)
		{
		case EStage::Trunk:
			if (Free(B.Belt, EStage::Trunk, B.P - CONV * Dt, 0.9f, i)) B.P -= CONV * Dt;
			if (B.P <= BELTX[B.Belt] && Free(B.Belt, EStage::Stub, 0.f, 0.9f, i)) { B.Stage = EStage::Stub; B.P = 0.f; }
			break;
		case EStage::Stub:
			// the input runs with its belt (the crew start it when the first bag reaches it)
			if (!Bt.bRun && !Bt.Flight.IsEmpty() && Bt.Idle >= 0.f) SetRunning(B.Belt, true);
			if (Bt.bRun && Free(B.Belt, EStage::Stub, B.P + CONV * Dt, 0.9f, i)) B.P += CONV * Dt;
			if (B.P >= STUB0 - WALLZ) { B.Stage = EStage::Hidden; B.Wait = 3.f; }
			break;
		case EStage::Hidden:
			if ((B.Wait -= Dt) <= 0.f && Bt.bRun && Free(B.Belt, EStage::Rise, 0.f, 0.6f, i)) { B.Stage = EStage::Rise; B.P = 0.f; }
			break;
		case EStage::Rise:
			if (Bt.bRun) B.P += Dt / 3.5f;
			if (B.P >= 1.f && Free(B.Belt, EStage::Belt, DROP_S - 0.8f, 1.6f, i)) { B.Stage = EStage::Belt; B.P = DROP_S; B.Side = FMath::FRandRange(-0.08f, 0.08f); if (Bt.SClack) Bt.SClack->Play(); }
			B.P = FMath::Min(B.P, 1.f);
			break;
		case EStage::Belt:
			if (Bt.bRun) { B.P += SPEED * Dt; B.Lap += SPEED * Dt; }
			// the passengers of its flight take their bags as they come round (after the first lap, now and then)
			if (Bt.bRun && B.Lap > PER * 0.6f && FMath::FRand() < Dt * 0.035f) { B.Stage = EStage::Gone; FreeIx[B.Kind].Add(B.Ix); }
			break;
		default: break;
		}
		PlaceBag(B);
	}
	// (each moved instance goes to the GPU on its own; no full rebuild every frame - 2026-11-17)
	// the belts: their plates going round while they run; the crew stop an empty belt after a minute
	for (int32 b = 0; b < Belts.Num(); ++b)
	{
		FBelt& Bt = Belts[b];
		bool bAny = Bt.Waiting > 0; for (const FBag& B : Bags) if (B.Stage != EStage::Gone && B.Belt == b) { bAny = true; break; }
		if (Bt.bRun)
		{
			Bt.Phase = FMath::Fmod(Bt.Phase + SPEED * Dt, PER);
			const int32 NP = Bt.Plates->GetInstanceCount();
			for (int32 k = 0; k < NP; ++k)
			{
				FVector2f D; const FVector2f P = PathAt(k * PER / NP + Bt.Phase, D);
				Bt.Plates->UpdateInstanceTransform(k, FTransform(FRotator(0, YawOf(D), 0), W(Bt.X + P.X, 0.425f, CZ + P.Y)), true, false, true);
			}
			Bt.Idle = bAny ? 0.f : Bt.Idle + Dt;
			if (Bt.Idle > 60.f && !Bt.Flight.IsEmpty()) { SetRunning(b, false); Bt.Flight.Empty(); if (Bt.Screen) Bt.Screen->SetText(FText::FromString(FString::Printf(TEXT("BELT %d"), b + 1))); }
			if ((Bt.ClackIn -= Dt) <= 0.f) { Bt.ClackIn = FMath::FRandRange(3.f, 7.f); if (Bt.SClack && FMath::FRand() < 0.5f) Bt.SClack->Play(); }
		}
		// the buttons: pressed in 8 mm, back out
		auto Push = [Dt](UStaticMeshComponent* C, float& T, const FVector& Base, const FVector& In)
		{
			if (!C || T <= 0.f) return;
			T = FMath::Max(0.f, T - Dt);
			const float K = FMath::Sin(FMath::Clamp(T / 0.35f, 0.f, 1.f) * PI);   // (in, then back out)
			C->SetWorldLocation(Base + In * 0.8f * K);
		};
		Push(Bt.Button, Bt.Press, Bt.BtnBase, FVector(-1, 0, 0));
		Push(Bt.CrewButton, Bt.CrewPress, Bt.CrewBase, FVector(0, 1, 0));
		Sounds(Bt, b, Dt);
	}
	// the flaps: a bag pushing through swings them aside; then they fall back like hanging rubber (a damped spring)
	bool bFlapMoved = false;
	for (FFlap& F : Flaps)
	{
		float Want = 0.f;
		for (const FBag& B : Bags)
		{
			if (B.Stage != EStage::Rise && B.Stage != EStage::Stub) continue;
			FTransform T; BagMeshes[B.Kind]->GetInstanceTransform(B.Ix, T, true);
			const float D = FVector::Dist(T.GetLocation() + FVector(0, 0, 15.f), F.Hinge - FVector(0, 0, 40.f));
			if (D < 55.f) Want = FMath::Max(Want, 1.25f * (1.f - D / 55.f));
		}
		const float Acc = 60.f * (Want - F.A) - 5.f * F.V;
		F.V += Acc * Dt; F.A += F.V * Dt;
		if (FMath::Abs(F.A) > 0.002f || FMath::Abs(F.V) > 0.002f)
		{
			const FQuat Q = FQuat(F.Axis, F.A) * F.Base;
			FTransform T; F.K->GetInstanceTransform(F.Ix, T, true); T.SetRotation(Q);
			F.K->UpdateInstanceTransform(F.Ix, T, true, false, true); bFlapMoved = true;
		}
	}
	(void)bFlapMoved;
}

void ABaggageBelts::Sounds(FBelt& Bt, int32 Index, float Dt)
{
	// heard as from where you are (owner, 2026-11-16): fading with distance (the attenuation), muffled and quieter when a
	// wall stands between you and the belt (looked at twice a second), the change gliding
	if ((Bt.EarIn -= Dt) <= 0.f)
	{
		Bt.EarIn = 0.5f;
		float Want = 1.f, WantLp = 20000.f;
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (PC->PlayerCameraManager)
			{
				const FVector Ear = PC->PlayerCameraManager->GetCameraLocation(), Src = W(Bt.X, 1.0f, CZ);
				FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(BeltEar), false, this);
				if (const APawn* Me = PC->GetPawn()) Q.AddIgnoredActor(Me);
				if (FVector::Dist(Ear, Src) < 6000.f && GetWorld()->LineTraceSingleByChannel(H, Ear, Src, ECC_Visibility, Q)) { Want = 0.4f; WantLp = 1400.f; }
			}
		Bt.Env = Want; Bt.EnvLp = WantLp;
	}
	for (UAudioComponent* A : {Bt.SStart.Get(), Bt.SRun.Get(), Bt.SClack.Get()})
		if (A && A->IsPlaying()) { A->SetLowPassFilterFrequency(Bt.EnvLp); A->SetVolumeMultiplier(FMath::Max(0.001f, (A == Bt.SClack ? 0.7f : 1.f) * Bt.Env)); }
}
