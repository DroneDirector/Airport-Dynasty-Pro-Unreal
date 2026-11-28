// The airport coaches' working parts (to-do 144) - see Coaches.h.
#include "Coaches.h"
#include "Site.h"
#include "BabylonModel.h"
#include "VehicleRiders.h"
#include "Seats.h"
#include "ADPWalker.h"
#include "People.h"
#include "Unseen.h"
#include "ADPLog.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/TextRenderComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "GameFramework/Character.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TWeakObjectPtr<ACoaches> ACoaches::One;
// the coach's own sizes (Tools/make_coach.py; metres, Blender's axes: x forward, y to the left, z up - in the model's
// root space a Blender point (x, y, z) is (x, -y, z) * 100)
namespace CoachDim
{
	constexpr float XF = 6.45f, W = 2.55f, Floor = 1.40f;
	constexpr float Doors[2][2] = {{0.53f, 1.61f}, {9.60f, 10.60f}};   // front, middle (distance from the front)
	constexpr float BayTop = 1.18f, BayBottom = 0.40f;
	inline float D2X(float D) { return XF - D; }
	inline FVector Root(float X, float Y, float Z) { return FVector(X * 100.f, -Y * 100.f, Z * 100.f); }
}

ACoaches::ACoaches()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ACoaches* ACoaches::Get(UWorld* W)
{
	if (One.IsValid()) return One.Get();
	if (!W) return nullptr;
	for (TActorIterator<ACoaches> It(W); It; ++It) { One = *It; return *It; }
	FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	One = W->SpawnActor<ACoaches>(FVector::ZeroVector, FRotator::ZeroRotator, S);
	return One.Get();
}

FTransform ACoaches::RootW(const FCoach& C) const
{
	const ABabylonModel* M = C.M.Get();
	return M && M->GetModelRoot() ? M->GetModelRoot()->GetComponentTransform() : FTransform::Identity;
}

void ACoaches::Add(ABabylonModel* Coach)
{
	if (!Coach) return;
	if (!SndDoor)
	{
		SndDoor = LoadObject<USoundBase>(nullptr, TEXT("/Game/Coach/Sounds/CoachDoor.CoachDoor"));
		SndKneel = LoadObject<USoundBase>(nullptr, TEXT("/Game/Coach/Sounds/CoachKneel.CoachKneel"));
		SndBay = LoadObject<USoundBase>(nullptr, TEXT("/Game/Coach/Sounds/CoachBay.CoachBay"));
		SndHum = LoadObject<USoundBase>(nullptr, TEXT("/Game/Coach/Sounds/CoachDrive.CoachDrive"));
		SndBeep = LoadObject<USoundBase>(nullptr, TEXT("/Game/ApronBus/Sounds/ABReverseBeep.ABReverseBeep"));   // (the coach loop backing up, to-do 373)
		for (USoundBase* S : {SndDoor, SndKneel, SndBay, SndHum, SndBeep}) if (S) Keep.Add(S);   // (held: else the GC frees them)
		// the airport's own name (never baked: Data/airfield.json, as the road signs)
		ADPSite::AirportBlock()->TryGetStringField(TEXT("name"), AirportName);   // (the site's: Site.h)
	}
	FCoach& C = Coaches.AddDefaulted_GetRef();
	C.M = Coach;
	Setup(C);
}

void ACoaches::Setup(FCoach& C)
{
	using namespace CoachDim;
	ABabylonModel* M = C.M.Get();
	if (!M || !M->GetModelRoot()) return;
	C.RootBase = M->GetModelRoot()->GetRelativeTransform();
	// the doors: each leaf swings out on its outer edge (the frame's side), a few cm out first, a quarter turn
	for (int32 i = 0; i < 2; ++i)
		for (int32 k = 0; k < 2; ++k)
		{
			const float HingeD = k == 0 ? Doors[i][0] + 0.012f : Doors[i][1] - 0.012f;
			const FTransform Pivot(Root(D2X(HingeD), -(W / 2 - 0.012f) - 0.015f, 0.f));
			FMover& L = C.Leaf[i][k];
			L.Pivot = Pivot;
			L.Group = M->BindGroup({FString::Printf(TEXT("DoorLeaf_%d%d"), i, k)}, true, Pivot);
			// (leaf 0 reaches back from its hinge, leaf 1 forward: each turned out, away from the coach - root +y)
			L.OpenTurn = FQuat(FVector::UpVector, FMath::DegreesToRadians(k == 0 ? -88.f : 88.f));
			L.OpenShift = FVector(0, 5.f, 0);
		}
	// the luggage bays (Data/coach-layout.json "bays": the right side's three, then the left's): hinged on top, lifted out
	FString Text; TSharedPtr<FJsonObject> J;
	if ((FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/coach-layout.json"))) || FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/coach-layout.json"))))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J)
	{
		const TArray<TSharedPtr<FJsonValue>>* B = nullptr;
		if (J->TryGetArrayField(TEXT("bays"), B))
			for (int32 b = 0; b < FMath::Min(6, B->Num()); ++b)
			{
				const TArray<TSharedPtr<FJsonValue>>& E = (*B)[b]->AsArray();
				const float Xc = E[0]->AsNumber(), Side = E[1]->AsNumber();
				const FTransform Pivot(Root(Xc, Side * (W / 2 + 0.005f), BayTop));
				FMover& Bm = C.Bay[b];
				Bm.Pivot = Pivot;
				const FString N = FString::Printf(TEXT("_%03d"), b + 1);
				Bm.Group = M->BindGroup({TEXT("BayDoor") + N, TEXT("BayHandle") + N, TEXT("BayHandleRecess") + N, TEXT("BayLock") + N}, false, Pivot);
				// (its lower edge out and up: about the coach's length, outward on its own side - root y is -Blender y)
				Bm.OpenTurn = FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Side < 0 ? 82.f : -82.f));
			}
	}
	// the wheels, held where they are while the body kneels
	C.Wheels = M->BindGroup({TEXT("Tyre"), TEXT("Rim"), TEXT("Hub"), TEXT("HubBarrel"), TEXT("Nut"), TEXT("TreadGroove"), TEXT("BrakeDisc")}, true, FTransform::Identity);
	// the rear name plate: the airport's own name, at run time
	if (!AirportName.IsEmpty())
	{
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(M);
		T->SetupAttachment(M->GetModelRoot());
		T->SetText(FText::FromString(AirportName.ToUpper() + TEXT(" AIRPORT")));
		T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(7.f); T->SetTextRenderColor(FColor(16, 30, 64));
		T->SetRelativeLocationAndRotation(Root(-XF - 0.0115f, 0.f, 2.34f), FRotator(0.f, 180.f, 0.f));
		T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		T->RegisterComponent(); M->AddInstanceComponent(T);
		C.Plate = T; Keep.Add(T);
	}
	// its lamps: each kind's lenses get their own glowing material (off to start)
	UMaterialInterface* LampMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Coach/M_CoachLamp.M_CoachLamp"));
	if (LampMat)
	{
		struct FKind { uint8 Bit; const TCHAR* Names[4]; FLinearColor Col; };
		const FKind Kinds[] = {
			{LowBeam, {TEXT("HeadLens_"), TEXT("DrlBar_"), TEXT("DrlFin_"), TEXT("LightBlade")}, FLinearColor(1.f, 0.96f, 0.88f)},
			{TurnLeft, {TEXT("Indicator_1"), TEXT("MirrorRepeater_1"), nullptr, nullptr}, FLinearColor(1.f, 0.45f, 0.02f)},
			{TurnRight, {TEXT("Indicator_-1"), TEXT("MirrorRepeater_-1"), nullptr, nullptr}, FLinearColor(1.f, 0.45f, 0.02f)},
			{Tail, {TEXT("TailColumn_"), TEXT("TailLed_"), TEXT("RearRedBar_"), nullptr}, FLinearColor(1.f, 0.03f, 0.02f)},
			{Reverse, {TEXT("Reverse_"), nullptr, nullptr, nullptr}, FLinearColor(1.f, 1.f, 1.f)}};
		for (const FKind& K : Kinds)
			for (const TCHAR* Nm : K.Names)
			{
				if (!Nm) continue;
				TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Found;
				M->InstancesNamed(Nm, true, Found);
				TSet<UInstancedStaticMeshComponent*> Done;
				for (const auto& F : Found)
				{
					UInstancedStaticMeshComponent* Ism = F.Get<0>();
					if (!Ism || Done.Contains(Ism)) continue;
					Done.Add(Ism);
					UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(LampMat, M);
					D->SetVectorParameterValue(TEXT("Color"), K.Col); D->SetScalarParameterValue(TEXT("Glow"), 0.f);
					for (int32 s = 0; s < FMath::Max(1, Ism->GetNumMaterials()); ++s) Ism->SetMaterial(s, D);
					C.LampMats.FindOrAdd(K.Bit).Add(D); Keep.Add(D);
				}
			}
	}
	// two real lights for the beams (on only for the coach you're nearest, with its beams on): short reach, no shadows
	for (const float Side : {-1.f, 1.f})
	{
		USpotLightComponent* L = NewObject<USpotLightComponent>(M);
		L->SetupAttachment(M->GetModelRoot());
		L->SetRelativeLocationAndRotation(Root(XF + 0.06f, Side * 0.92f, 0.77f), FRotator(-4.f, 0.f, 0.f));
		L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(0.f); L->SetAttenuationRadius(3500.f);
		L->SetInnerConeAngle(14.f); L->SetOuterConeAngle(30.f); L->SetLightColor(FLinearColor(1.f, 0.96f, 0.9f));
		L->SetCastShadows(false); L->SetVisibility(false);
		L->RegisterComponent(); M->AddInstanceComponent(L);
		C.Beams.Add(L); Keep.Add(L);
	}
	// the mirrors: a capture each, its picture on the mirror's glass (only the nearest coach's capture)
	if (UMaterialInterface* MirrorMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Coach/M_CoachMirror.M_CoachMirror")))
		for (const float Side : {-1.f, 1.f})
		{
			UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
			RT->RenderTargetFormat = RTF_RGBA8_SRGB; RT->ClearColor = FLinearColor(0.05f, 0.06f, 0.07f); RT->InitAutoFormat(256, 512); RT->UpdateResourceImmediate(true);
			USceneCaptureComponent2D* Cap = NewObject<USceneCaptureComponent2D>(M);
			Cap->SetupAttachment(M->GetModelRoot());
			// (the mirror head: Tools/make_coach.py mirror(): its glass looks back along the coach, a little outward)
			Cap->SetRelativeLocationAndRotation(Root(XF + 0.15f, Side * 1.455f, 2.77f), FRotator(-3.f, 180.f - Side * -8.f, 0.f));
			Cap->TextureTarget = RT; Cap->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			Cap->bCaptureEveryFrame = false; Cap->bCaptureOnMovement = false; Cap->FOVAngle = 38.f;
			Cap->MaxViewDistanceOverride = 15000.f; Cap->LODDistanceFactor = 3.f;
			Cap->ShowFlags.SetMotionBlur(false); Cap->ShowFlags.SetDepthOfField(false);
			Cap->RegisterComponent(); M->AddInstanceComponent(Cap);
			C.Mirrors.Add(Cap); Keep.Add(Cap); Keep.Add(RT);
			TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Found;
			M->InstancesNamed(Side > 0 ? TEXT("MirrorGlass_1") : TEXT("MirrorGlass_-1"), true, Found);
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(MirrorMat, M);
			D->SetTextureParameterValue(TEXT("Reflection"), RT); Keep.Add(D); C.MirrorMats.Add(D);
			for (const auto& F : Found) if (UInstancedStaticMeshComponent* Ism = F.Get<0>()) for (int32 s = 0; s < FMath::Max(1, Ism->GetNumMaterials()); ++s) Ism->SetMaterial(s, D);
		}
	// the drive's hum (for the traffic job): looping, silent until driven
	if (SndHum)
	{
		C.Hum = UGameplayStatics::SpawnSoundAttached(SndHum, M->GetModelRoot(), NAME_None, Root(-4.5f, 0.f, 0.8f), EAttachLocation::KeepRelativeOffset, false, 0.f, 1.f, 0.f, nullptr, nullptr, false);
		if (C.Hum) { C.Hum->Stop(); Keep.Add(C.Hum); }
	}
	ADP_LOG(TEXT("Coaches: coach %d ready - doors %d/%d, bays %d, wheels %s, lamps %d kinds, mirrors %d, plate '%s'"), Coaches.Num() - 1,
		C.Leaf[0][0].Group != INDEX_NONE, C.Leaf[1][1].Group != INDEX_NONE, int32(C.Bay[0].Group != INDEX_NONE) + int32(C.Bay[5].Group != INDEX_NONE),
		C.Wheels != INDEX_NONE ? TEXT("held") : TEXT("-"), C.LampMats.Num(), C.Mirrors.Num(), *AirportName);
}

bool ACoaches::DoorSpot(int32 Ci, int32 Door, FVector& At, FVector& Out) const
{
	using namespace CoachDim;
	if (!Coaches.IsValidIndex(Ci) || Door < 0 || Door > 1) return false;
	const FTransform R = RootW(Coaches[Ci]);
	At = R.TransformPosition(Root(D2X((Doors[Door][0] + Doors[Door][1]) / 2), -(W / 2), 0.40f));
	Out = R.TransformVectorNoScale(FVector(0, 1, 0));
	return true;
}

FVector ACoaches::DoorSide(int32 Ci, int32 Door, float Along, float Out) const
{
	using namespace CoachDim;
	if (!Coaches.IsValidIndex(Ci) || Door < 0 || Door > 1) return FVector::ZeroVector;
	return RootW(Coaches[Ci]).TransformPosition(Root(D2X((Doors[Door][0] + Doors[Door][1]) / 2 + Along), -(W / 2 + Out), 0.40f));
}

bool ACoaches::ButtonStand(int32 Ci, int32 Door, FVector& Stand) const
{
	using namespace CoachDim;
	if (!Coaches.IsValidIndex(Ci) || Door < 0 || Door > 1) return false;
	// 0.45 m behind the rear edge (the rear leaf swings out on that edge: clear of it by more than a body's half-width) and 0.45 m
	// out from the side: the shoulder 0.75 m from the button (DoorButton_i, 1.15 m up) - inside the arm's 0.88 m that Pick asks
	Stand = DoorSide(Ci, Door, (Doors[Door][1] - Doors[Door][0]) / 2 + 0.45f, 0.45f);
	return true;
}

bool ACoaches::BaySpot(int32 Ci, int32 Bay, FVector& At, FVector& Out) const
{
	if (!Coaches.IsValidIndex(Ci) || Bay < 0 || Bay > 5 || Coaches[Ci].Bay[Bay].Group == INDEX_NONE) return false;
	const FTransform R = RootW(Coaches[Ci]);
	const FVector P = Coaches[Ci].Bay[Bay].Pivot.GetLocation();
	At = R.TransformPosition(FVector(P.X, P.Y, (CoachDim::BayTop + CoachDim::BayBottom) * 50.f));
	Out = R.TransformVectorNoScale(FVector(0, FMath::Sign(P.Y), 0));
	return true;
}

bool ACoaches::IsInside(const FVector& At, int32* Which) const
{
	for (int32 i = 0; i < Coaches.Num(); ++i)
	{
		if (!Coaches[i].M.IsValid()) continue;
		const FVector L = RootW(Coaches[i]).InverseTransformPosition(At);
		if (FMath::Abs(L.X) < 640.f && FMath::Abs(L.Y) < 124.f && L.Z > 30.f && L.Z < 360.f) { if (Which) *Which = i; return true; }
	}
	return false;
}

bool ACoaches::Pick(const FVector& Feet, const FVector& Facing, int32& Ci, int32& Kind, int32& Index) const
{
	float Best = 175.f; Ci = INDEX_NONE;
	const FVector F2 = Facing.GetSafeNormal2D();
	auto Try = [&](int32 c, int32 k, int32 i, const FVector& At)
	{
		const float D = FVector::Dist2D(Feet, At);
		if (D >= Best || FMath::Abs(Feet.Z - At.Z) > 220.f) return;
		const FVector To = (At - Feet).GetSafeNormal2D();
		if (D > 60.f && FVector::DotProduct(To, F2) < 0.35f) return;   // (facing it, roughly)
		// (within an arm's reach of where the hand goes: the shoulder about 1.45 m over the feet, the arm with its hand 0.88 m)
		FVector Hd, Od; if (!HandSpot(c, k, i, Feet, Hd, Od) || FVector::Dist(Feet + FVector(0, 0, 145.f), Hd) > 88.f) return;
		Best = D; Ci = c; Kind = k; Index = i;
	};
	for (int32 c = 0; c < Coaches.Num(); ++c)
	{
		FVector At, Out;
		for (int32 d = 0; d < 2; ++d) if (DoorSpot(c, d, At, Out)) { Try(c, 0, d, At + FVector(0, 0, 60.f)); Try(c, 0, d, At - Out * 90.f + FVector(0, 0, 100.f)); }   // (outside, or on its steps inside)
		for (int32 b = 0; b < 6; ++b) if (BaySpot(c, b, At, Out)) Try(c, 1, b, At - FVector(0, 0, 40.f));
	}
	return Ci != INDEX_NONE;
}

bool ACoaches::NearUse(const FVector& Feet, const FVector& Facing) const { int32 C, K, I; return Pick(Feet, Facing, C, K, I); }

bool ACoaches::Use(const APawn* Who)
{
	if (!Who) return false;
	const FVector Feet = Who->GetActorLocation() - FVector(0, 0, Who->GetSimpleCollisionHalfHeight());
	if (Reach.Ci != INDEX_NONE) return true;   // (your hand is already on its way to one)
	int32 C, K, I;
	if (!Pick(Feet, Who->GetControlRotation().Vector(), C, K, I)) return false;
	const bool bOpen = K == 0 ? !Coaches[C].bDoorOpen[I] : !Coaches[C].bBayOpen[I];
	AADPWalker* Wk = Cast<AADPWalker>(const_cast<APawn*>(Who));
	if (!Wk) { if (K == 0) SetDoor(C, I, bOpen); else SetBay(C, I, bOpen); return true; }   // (no arm to reach with)
	// the hand to its button or catch first; the door or bay moves once it is there (ReachTick)
	Reach = FReach(); Reach.Who = Wk; Reach.Ci = C; Reach.Kind = K; Reach.Index = I; Reach.bOpen = bOpen; Reach.From = Feet;
	FVector Hd, Od; Reach.bPoint = K == 0 && HandSpot(C, K, I, Feet, Hd, Od) && FVector::DotProduct(Feet - Hd, Od) > 0.f;   // (outside: a button, the finger)
	Wk->SetPoint(Reach.bPoint);
	return true;
}

bool ACoaches::HandSpot(int32 Ci, int32 Kind, int32 Index, const FVector& Feet, FVector& Hand, FVector& Out) const
{
	using namespace CoachDim;
	if (!Coaches.IsValidIndex(Ci)) return false;
	const FCoach& C = Coaches[Ci];
	const FTransform R = RootW(C);
	if (Kind == 0)
	{
		if (Index < 0 || Index > 1) return false;
		// outside: the door's button behind its rear edge (Tools/make_coach.py DoorButton_i, its lit face); from inside, on its
		// steps: the inner jamb on the same side (the model has no inside button - the hand on the frame's edge)
		const bool bOutside = R.InverseTransformPosition(Feet).Y > W * 50.f - 5.f;
		Hand = R.TransformPosition(bOutside ? Root(D2X(Doors[Index][1] + 0.06f), -(W / 2 + 0.011f), 1.15f) : Root(D2X(Doors[Index][1] - 0.03f), -(W / 2 - 0.06f), 1.60f));
		Out = R.TransformVectorNoScale(FVector(0, bOutside ? 1.f : -1.f, 0));
		return true;
	}
	if (Index < 0 || Index > 5 || C.Bay[Index].Group == INDEX_NONE) return false;
	// the bay's catch (BayHandle_nnn: 0.10 m under the hinge, on its outer face), carried round with the door as it lifts
	const FMover& B = C.Bay[Index];
	const FVector P = B.Pivot.GetLocation();
	const float S = FMath::Sign(P.Y);
	const FQuat Q = FQuat::Slerp(FQuat::Identity, B.OpenTurn, FMath::SmoothStep(0.f, 1.f, C.BayK[Index]));
	Hand = R.TransformPosition(P + Q.RotateVector(FVector(0, S * 1.3f, -10.f)));
	Out = R.TransformVectorNoScale(Q.RotateVector(FVector(0, S, 0)));
	return true;
}

void ACoaches::ReachTick(float Dt)
{
	if (Reach.Ci == INDEX_NONE) return;
	constexpr float Touch = 0.4f, Hold = 0.15f, Back = 0.35f;   // (to it, on it, back: the other vehicles' controls' pace)
	AADPWalker* Wk = Cast<AADPWalker>(Reach.Who.Get());
	if (!Wk || !Coaches.IsValidIndex(Reach.Ci)) { ReachEnd(); return; }
	const FVector Feet = Wk->GetActorLocation() - FVector(0, 0, Wk->GetSimpleCollisionHalfHeight());
	FVector Hand, Out;
	if (FVector::Dist2D(Feet, Reach.From) > 60.f || !HandSpot(Reach.Ci, Reach.Kind, Reach.Index, Reach.From, Hand, Out))
	{
		// walked off: the arm back at once, and nothing moved if the hand never got there
		ADP_LOG(TEXT("Coaches: you walked off %s - the hand back%s"), Reach.Kind == 0 ? TEXT("the door") : TEXT("the bay"), Reach.bDone ? TEXT("") : TEXT(", nothing moved"));
		ReachEnd(); return;
	}
	Reach.T += Dt;
	if (!Reach.bDone && Reach.T >= Touch)
	{
		Reach.bDone = true;
		if (Reach.Kind == 0) SetDoor(Reach.Ci, Reach.Index, Reach.bOpen); else SetBay(Reach.Ci, Reach.Index, Reach.bOpen);
	}
	if (Reach.T >= Touch + Hold + Back) { ReachEnd(); return; }
	const float W = Reach.T < Touch ? FMath::SmoothStep(0.f, 1.f, Reach.T / Touch) : 1.f - FMath::SmoothStep(0.f, 1.f, (Reach.T - Touch - Hold) / Back);
	// as the other vehicles' controls (VehicleControls.cpp Arm): a button, the fingertip on it pressing in; a catch, the palm round it
	const FVector Sh = Wk->ShoulderNow();
	FVector F = (Hand - Sh).GetSafeNormal(); if (!Out.IsNearlyZero()) F = FMath::Lerp(F, -Out, 0.5f).GetSafeNormal();
	const FVector Thumb = FVector::VectorPlaneProject(FVector::UpVector, F).GetSafeNormal();
	const float In = Reach.bPoint && Reach.T >= Touch && Reach.T < Touch + Hold ? 0.5f : 0.f;
	Wk->SetArm(W, Hand - F * ((Reach.bPoint ? 18.f : 8.5f) - In), F, Thumb);
}

void ACoaches::ReachEnd()
{
	if (AADPWalker* Wk = Cast<AADPWalker>(Reach.Who.Get())) { Wk->SetArm(0.f, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector); Wk->SetPoint(false); }
	Reach = FReach();
}

void ACoaches::SetDoor(int32 Ci, int32 Door, bool bOpen)
{
	if (!Coaches.IsValidIndex(Ci) || Door < 0 || Door > 1 || Coaches[Ci].bDoorOpen[Door] == bOpen) return;
	FCoach& C = Coaches[Ci];
	const bool bAnyBefore = C.bDoorOpen[0] || C.bDoorOpen[1];
	C.bDoorOpen[Door] = bOpen;
	FVector At, Out;
	if (DoorSpot(Ci, Door, At, Out))
	{
		if (SndDoor) UGameplayStatics::PlaySoundAtLocation(this, SndDoor, At + FVector(0, 0, 150.f));
		// (the coach kneels as its first door opens, rises as its last one shuts: the air)
		if (SndKneel && bAnyBefore != (C.bDoorOpen[0] || C.bDoorOpen[1])) UGameplayStatics::PlaySoundAtLocation(this, SndKneel, At + FVector(0, 0, 60.f));
	}
	ADP_LOG(TEXT("Coaches: coach %d %s door %s"), Ci, Door == 0 ? TEXT("front") : TEXT("middle"), bOpen ? TEXT("opened") : TEXT("shut"));
}

void ACoaches::SetBay(int32 Ci, int32 Bay, bool bOpen)
{
	if (!Coaches.IsValidIndex(Ci) || Bay < 0 || Bay > 5 || Coaches[Ci].bBayOpen[Bay] == bOpen) return;
	Coaches[Ci].bBayOpen[Bay] = bOpen;
	FVector At, Out;
	if (SndBay && BaySpot(Ci, Bay, At, Out)) UGameplayStatics::PlaySoundAtLocation(this, SndBay, At);
	ADP_LOG(TEXT("Coaches: coach %d bay %d %s"), Ci, Bay, bOpen ? TEXT("opened") : TEXT("shut"));
}

void ACoaches::SetLamps(int32 Ci, uint8 L) { if (Coaches.IsValidIndex(Ci)) Coaches[Ci].Lamps = L; }
void ACoaches::SetDrive(int32 Ci, float A) { if (Coaches.IsValidIndex(Ci)) Coaches[Ci].Drive = FMath::Clamp(A, 0.f, 1.f); }

void ACoaches::Lamp(FCoach& C, float Dt)
{
	C.Blink += Dt;
	const bool bBlinkOn = FMath::Fmod(C.Blink, 0.8f) < 0.42f;   // (turn signals: about 75 a minute)
	auto Glow = [&](uint8 Bit, float G) { if (TArray<UMaterialInstanceDynamic*>* Ds = C.LampMats.Find(Bit)) for (UMaterialInstanceDynamic* D : *Ds) D->SetScalarParameterValue(TEXT("Glow"), G); };
	const uint8 L = C.Lamps;
	Glow(LowBeam, (L & HighBeam) ? 60.f : (L & LowBeam) ? 30.f : 0.f);
	Glow(TurnLeft, (L & TurnLeft) && bBlinkOn ? 40.f : 0.f);
	Glow(TurnRight, (L & TurnRight) && bBlinkOn ? 40.f : 0.f);
	Glow(Tail, (L & Brake) ? 45.f : (L & Tail) ? 12.f : 0.f);
	Glow(Reverse, (L & Reverse) ? 35.f : 0.f);
}

void ACoaches::Mirror(float Dt, const FVector& Me)
{
	// only the nearest coach within 25 m reflects, a few times a second (owner's performance rule)
	int32 Near = INDEX_NONE; float ND = 2500.f;
	for (int32 i = 0; i < Coaches.Num(); ++i) if (ABabylonModel* M = Coaches[i].M.Get()) { const float D = FVector::Dist(M->GetActorLocation(), Me); if (D < ND) { ND = D; Near = i; } }
	MirrorsOn = Near;
	if (Near == INDEX_NONE) return;
	MirrorClock -= Dt;
	if (MirrorClock > 0.f) return;
	MirrorClock = 0.12f;
	for (USceneCaptureComponent2D* Cap : Coaches[Near].Mirrors) if (Cap) Cap->CaptureScene();
}

void ACoaches::ApplyBody(FCoach& C)
{
	ABabylonModel* M = C.M.Get();
	if (!M) return;
	const float E = FMath::SmoothStep(0.f, 1.f, C.Kneel);
	// (tipped about the far side's bottom edge: the door side, root +y, goes down)
	const FVector Hinge(0.f, -CoachDim::W * 50.f, 0.f);
	FTransform Knee = FTransform(-Hinge) * FTransform(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(1.1f * E))) * FTransform(Hinge + FVector(0, 0, -6.f * E));
	const FDriveParts& D = C.Line;
	if (!D.bOn)
	{
		M->GetModelRoot()->SetRelativeTransform(Knee * C.RootBase);
		if (C.Wheels != INDEX_NONE) M->MoveGroup(C.Wheels, Knee.Inverse());
		return;
	}
	// (driving parts on: the body's lean on its springs too, each wheel rolled and steered where it stands on the road)
	const FVector Mid(0.f, 0.f, 60.f);
	Knee = FTransform(-Mid) * FTransform(FRotator(D.LeanPitch, 0.f, D.LeanRoll)) * FTransform(Mid) * Knee;
	M->GetModelRoot()->SetRelativeTransform(Knee * C.RootBase);
	const FTransform Inv = Knee.Inverse();
	for (int32 w = 0; w < 4; ++w)
		if (D.Wheel[w] != INDEX_NONE)
		{
			const FQuat Steer(FVector::UpVector, w < 2 ? FMath::DegreesToRadians(D.SteerDeg) : 0.f);
			M->MoveGroup(D.Wheel[w], FTransform(Steer * FQuat(FVector::RightVector, D.RollRad), D.WheelAt[w]) * Inv);
			if (w < 2 && D.Knuckle[w] != INDEX_NONE) M->MoveGroup(D.Knuckle[w], FTransform(Steer, D.WheelAt[w]) * Inv);
		}
}

#pragma region DriveAndRiders
// (VehicleRiders.h: the driver, the passengers, the driving - nothing of this runs while adp.CoachLine is 0)
namespace
{
	/** One coach as its riders see it. */
	class FCoachBody : public IVehicleBody
	{
	public:
		FCoachBody(ACoaches* InC, int32 InI) : C(InC), I(InI) {}
		virtual AActor* Actor() const override { return C.IsValid() ? C->ModelOf(I) : nullptr; }
		virtual FTransform Body() const override { return C.IsValid() ? C->RootOf(I) : FTransform::Identity; }
		virtual FTransform Road() const override { return C.IsValid() ? C->RoadOf(I) : FTransform::Identity; }
		virtual void Place(const FDriveState& S) override { if (C.IsValid()) C->DrivePlace(I, S.Road, S.LeanRoll, S.LeanPitch, S.SteerDeg, S.RollRad, S.WheelTurn, S.Accel, S.Brake, S.SeatArm); }
		virtual void SetDoor(int32 D, bool b) override { if (C.IsValid()) C->SetDoor(I, D, b); }
		virtual float DoorK(int32 D) const override { return C.IsValid() ? C->DoorKOf(I, D) : 0.f; }
		virtual bool Knelt() const override { return C.IsValid() && C->KneelOf(I) > 0.f; }
		virtual void SetBay(int32 B, bool b) override { if (C.IsValid()) C->SetBay(I, B, b); }
		virtual float BayK(int32 B) const override { return C.IsValid() ? C->BayKOf(I, B) : 0.f; }
		virtual void SetLamps(bool bLit, int32 Ind, bool bBrake) override
		{
			if (C.IsValid()) C->SetLamps(I, uint8((bLit ? ACoaches::LowBeam | ACoaches::Tail : 0) | (Ind < 0 ? ACoaches::TurnLeft : Ind > 0 ? ACoaches::TurnRight : 0) | (bBrake ? ACoaches::Brake : 0)));
		}
		virtual void SetDrive(float A) override { if (C.IsValid()) C->SetDrive(I, A); }
		virtual void SetAway(bool b) override { if (C.IsValid()) ASeats::Follow(C->ModelOf(I), b); }
		virtual bool OtherVehicleAt(const FVector& At, float R) const override
		{
			if (!C.IsValid()) return false;
			for (int32 k = 0; k < C->Num(); ++k)
			{
				if (k == I) continue;
				const FVector L = C->RootOf(k).InverseTransformPosition(At);
				if (FMath::Abs(L.X) < 645.f + R && FMath::Abs(L.Y) < 128.f + R && FMath::Abs(L.Z) < 400.f) return true;
			}
			return false;
		}
	private:
		TWeakObjectPtr<ACoaches> C; int32 I;
	};
}

UVehicleRiders* ACoaches::Riders(int32 Ci)
{
	if (!Coaches.IsValidIndex(Ci) || !UVehicleRiders::LineOn()) return nullptr;
	if (!Coaches[Ci].Riders.IsValid() && DriveReady(Ci))
	{
		UVehicleRiders* R = UVehicleRiders::Make(Coaches[Ci].M.Get(), TEXT("airportcoach"), MakeUnique<FCoachBody>(this, Ci));
		if (R) R->AddTickPrerequisiteActor(this);
		Coaches[Ci].Riders = R;
	}
	return Coaches[Ci].Riders.Get();
}

bool ACoaches::DriveReady(int32 Ci)
{
	using namespace CoachDim;
	if (!Coaches.IsValidIndex(Ci) || !UVehicleRiders::LineOn()) return false;
	FCoach& C = Coaches[Ci]; FDriveParts& D = C.Line;
	ABabylonModel* M = C.M.Get();
	if (!M) return false;
	if (D.bOn) return true;
	if (D.Hand == INDEX_NONE)   // (bound once)
	{
		static const TCHAR* Parts[] = {TEXT("Tyre"), TEXT("Rim"), TEXT("Hub"), TEXT("Nut"), TEXT("TreadGroove"), TEXT("BrakeDisc")};
		for (int32 w = 0; w < 4; ++w)
		{
			const bool bFront = w < 2; const float Side = (w & 1) ? 1.f : -1.f;   // (root y: left is -)
			D.WheelAt[w] = FVector(bFront ? 370.f : -240.f, Side * (bFront ? 107.5f : 91.8f), 52.f);
			D.Wheel[w] = M->BindGroupWhere([&](const FString& N, int32, const FVector& At)
			{
				bool bPart = false; for (const TCHAR* P : Parts) bPart |= N.StartsWith(P);
				return bPart && FVector2D::Distance(FVector2D(At.X, At.Z), FVector2D(D.WheelAt[w].X, D.WheelAt[w].Z)) < 60.f && At.Y * Side > 30.f;
			}, FTransform(D.WheelAt[w]));
			if (bFront) D.Knuckle[w] = M->BindGroup({FString::Printf(TEXT("Caliper_%dF"), w == 0 ? 1 : -1), FString::Printf(TEXT("Knuckle_%d"), w == 0 ? 1 : -1)}, true, FTransform(D.WheelAt[w]));
		}
		D.HandAt = FTransform(Root(5.50f, 0.62f, 1.78f));
		D.Hand = M->BindGroup({TEXT("SWRim"), TEXT("SWHub"), TEXT("SWBadge"), TEXT("SWSpoke")}, true, D.HandAt);
		D.PedalAt = FTransform(Root(5.87f, 0.62f, 1.00f));
		D.Accel = M->BindGroup({TEXT("Accelerator")}, true, D.PedalAt);
		D.Brake = M->BindGroup({TEXT("BrakePedal")}, true, D.PedalAt);
		D.ArmAt = FTransform(Root(5.04f, 0.34f, 1.62f));
		D.Arm = M->BindGroup({TEXT("DSArm_-1")}, true, D.ArmAt);
		ADP_LOG(TEXT("Coaches: coach %d's driving parts - wheels %d %d %d %d, knuckles %d %d, steering wheel %d, pedals %d %d, seat arm %d"), Ci, D.Wheel[0], D.Wheel[1], D.Wheel[2], D.Wheel[3], D.Knuckle[0], D.Knuckle[1], D.Hand, D.Accel, D.Brake, D.Arm);
	}
	D.bOn = D.Hand != INDEX_NONE && D.Wheel[0] != INDEX_NONE && D.Wheel[2] != INDEX_NONE;
	return D.bOn;
}

void ACoaches::DrivePlace(int32 Ci, const FTransform& Road, float LeanRoll, float LeanPitch, float SteerDeg, float RollRad, float WheelTurn, float Accel, float Brake, float SeatArm)
{
	if (!DriveReady(Ci)) return;
	FCoach& C = Coaches[Ci]; FDriveParts& D = C.Line; ABabylonModel* M = C.M.Get();
	C.RootBase = Road; D.LeanRoll = LeanRoll; D.LeanPitch = LeanPitch; D.SteerDeg = SteerDeg; D.RollRad = RollRad;
	ApplyBody(C);
	// the steering wheel about its own tilted column (clockwise for the driver is to the right)
	const FVector Col(-FMath::Sin(FMath::DegreesToRadians(30.f)), 0.f, FMath::Cos(FMath::DegreesToRadians(30.f)));
	if (D.Hand != INDEX_NONE) M->MoveGroup(D.Hand, FTransform(FQuat(Col, FMath::DegreesToRadians(WheelTurn)), D.HandAt.GetLocation()));
	// the pedals, pressed about their feet (the model's accelerator stands left of its brake: put the right way round - a
	// driver's right foot works both, the accelerator on the right)
	const FVector Foot = D.PedalAt.GetLocation();
	if (D.Accel != INDEX_NONE) M->MoveGroup(D.Accel, FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(14.f * Accel)), Foot + FVector(0.f, 24.f, 0.f)));
	if (D.Brake != INDEX_NONE) M->MoveGroup(D.Brake, FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(12.f * Brake)), Foot + FVector(0.f, -14.f, 0.f)));
	if (D.Arm != INDEX_NONE) M->MoveGroup(D.Arm, FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(-80.f * SeatArm)), D.ArmAt.GetLocation()));
	M->FlushMoves();
}

void ACoaches::DriveOff(int32 Ci)
{
	if (!Coaches.IsValidIndex(Ci) || !Coaches[Ci].Line.bOn) return;
	FCoach& C = Coaches[Ci]; FDriveParts& D = C.Line; ABabylonModel* M = C.M.Get();
	if (!M) return;
	for (int32 w = 0; w < 4; ++w) { if (D.Wheel[w] != INDEX_NONE) M->MoveGroup(D.Wheel[w], FTransform(D.WheelAt[w])); if (w < 2 && D.Knuckle[w] != INDEX_NONE) M->MoveGroup(D.Knuckle[w], FTransform(D.WheelAt[w])); }
	for (const int32 G : {D.Accel, D.Brake}) if (G != INDEX_NONE) M->MoveGroup(G, D.PedalAt);
	if (D.Hand != INDEX_NONE) M->MoveGroup(D.Hand, D.HandAt);
	if (D.Arm != INDEX_NONE) M->MoveGroup(D.Arm, D.ArmAt);
	D.bOn = false; D.LeanRoll = D.LeanPitch = D.SteerDeg = D.RollRad = 0.f;
	C.Lamps = 0; C.Drive = 0.f; C.Riders = nullptr;
	ApplyBody(C); M->FlushMoves();
}
#pragma endregion

#pragma region the town hub one-way loop (to-do 373)
namespace CoachLoop
{
	constexpr float LoopAxleF = 370.f, LoopAxleR = -240.f;   // cm from the root (Data/airportcoach-riders.json)
	constexpr float LoopBackX = -650.f;                   // the body's back (cm; assumed: the model's 12.95 m)
	constexpr float LoopPullLen = 1800.f;                 // a pull-out's run into the next lane
	constexpr float LoopBackSpeed = 90.f, LoopMaxBack = 600.f, LoopPad = 30.f, LoopLook = 3500.f;
	constexpr float LoopTownLimit = 13.9f;                // m/s: 50 km/h

	FVector LoopPlan(const TArray<TSharedPtr<FJsonValue>>& A) { return A.Num() >= 2 ? FVector(A[0]->AsNumber() * 100.0, -A[1]->AsNumber() * 100.0, 0.0) : FVector::ZeroVector; }
	int32 LoopNearest(const TArray<FVector>& P, const FVector& Q)
	{
		int32 Best = 0; double Bd = 1e30;
		for (int32 i = 0; i < P.Num(); ++i) { const double D = FVector::DistSquared2D(P[i], Q); if (D < Bd) { Bd = D; Best = i; } }
		return Best;
	}
	int32 LoopBack(const TArray<FVector>& P, int32 i, float Cm)   // the lane point Cm back along it from i
	{
		float Acc = 0.f;
		while (i > 0 && Acc < Cm) { Acc += float(FVector::Dist2D(P[i], P[i - 1])); --i; }
		return i;
	}
	/** Path's end and Next's start each cut back J cm, the gap rounded with one Hermite curve (DirA: Path's way when it is a single point). */
	void LoopJoin(TArray<FVector>& Path, const TArray<FVector>& Next, float J, const FVector& DirA)
	{
		float Cut = J;
		while (Path.Num() > 1 && Cut > 0.f)
		{
			const float L = float(FVector::Dist2D(Path.Last(), Path[Path.Num() - 2]));
			if (L <= Cut) { Cut -= L; Path.Pop(); } else { Path.Last() = FMath::Lerp(Path.Last(), Path[Path.Num() - 2], Cut / L); Cut = 0.f; }
		}
		int32 S = 0; float Acc = 0.f;
		while (S + 2 < Next.Num() && Acc < J) { Acc += float(FVector::Dist2D(Next[S], Next[S + 1])); ++S; }
		const FVector A = Path.Last(), B = Next[S];
		const FVector Ta = (Path.Num() > 1 ? A - Path[Path.Num() - 2] : DirA).GetSafeNormal2D(), Tb = (Next[S + 1] - B).GetSafeNormal2D();
		const float D = float(FVector::Dist2D(A, B)), K = D * 0.9f;
		const int32 N = FMath::Max(1, FMath::CeilToInt(D / 100.f));
		for (int32 k = 1; k < N; ++k)
		{
			const float t = float(k) / N, t2 = t * t, t3 = t2 * t;
			Path.Add(A * (2 * t3 - 3 * t2 + 1) + Ta * (K * (t3 - 2 * t2 + t)) + B * (-2 * t3 + 3 * t2) + Tb * (K * (t3 - t2)));
		}
		for (int32 i = S; i < Next.Num(); ++i) Path.Add(Next[i]);
	}
	/** Two coach bodies (root places, world) closer than PadCm anywhere: their outlines as boxes, seen from above. */
	bool LoopTouch(const FTransform& Ra, const FTransform& Rb, float PadCm)
	{
		const float HalfL = (CoachDim::XF * 100.f - LoopBackX) / 2.f, Mid = (CoachDim::XF * 100.f + LoopBackX) / 2.f, HalfW = CoachDim::W * 50.f;
		const FVector Fa = Ra.GetUnitAxis(EAxis::X).GetSafeNormal2D(), Fb = Rb.GetUnitAxis(EAxis::X).GetSafeNormal2D();
		const FVector Ca = Ra.GetLocation() + Fa * Mid, Cb = Rb.GetLocation() + Fb * Mid;
		const FVector Axes[4] = {Fa, FVector(-Fa.Y, Fa.X, 0), Fb, FVector(-Fb.Y, Fb.X, 0)};
		for (const FVector& Ax : Axes)
		{
			auto Reach = [&](const FVector& F) { const FVector Rt(-F.Y, F.X, 0); return FMath::Abs(FVector::DotProduct(F, Ax)) * HalfL + FMath::Abs(FVector::DotProduct(Rt, Ax)) * HalfW; };
			if (FMath::Abs(FVector::DotProduct(Cb - Ca, Ax)) > Reach(Fa) + Reach(Fb) + PadCm) return false;
		}
		return true;
	}
}

TArray<FVector> ACoaches::RoutePath(int32 Ci, bool bToTown, int32 Bay, const FTransform& From, FString& Why) const
{
	using namespace CoachLoop;
	TArray<FVector> Path;
	FString Text; TSharedPtr<FJsonObject> J; const TSharedPtr<FJsonObject>* Rt = nullptr; const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!FFileHelper::LoadFileToString(Text, *ADPSite::File(TEXT("roads.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()
		|| !J->TryGetObjectField(TEXT("routes"), Rt) || !(*Rt)->TryGetArrayField(bToTown ? TEXT("coach-to-town") : TEXT("coach-to-terminal"), Steps))
	{ Why = TEXT("this site's roads.json has no coach loop"); return Path; }
	TMap<FString, TSharedPtr<FJsonObject>> Road;
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("roads"))) if (const TSharedPtr<FJsonObject> O = V->AsObject()) Road.Add(O->GetStringField(TEXT("id")), O);
	// the bay: where its front bumper stops (the hub's "bays"; at the terminal the coach's own stand)
	FVector Bump = FVector::ZeroVector;
	if (bToTown)
	{
		const TSharedPtr<FJsonObject>* Hub = Road.Find(TEXT("TOWN-HUB")); const TArray<TSharedPtr<FJsonValue>>* Bays = nullptr;
		if (!Hub || !(*Hub)->TryGetArrayField(TEXT("bays"), Bays) || !Bays->IsValidIndex(Bay)) { Why = FString::Printf(TEXT("no hub bay %d"), Bay); return Path; }
		Bump = LoopPlan((*Bays)[Bay]->AsArray());
	}
	else
	{
		const FTransform* H = HomeStand.Find(Ci);
		if (!H) { Why = TEXT("it has no stand at the terminal (never sent from one)"); return Path; }
		Bump = H->TransformPosition(FVector(CoachDim::XF * 100.f, 0, 0));
	}
	const FVector Axle = From.TransformPosition(FVector(LoopAxleF, 0, 0)), Fwd = From.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	// one step's lane and its run (lane point indices); bFromCoach: it starts where the coach stands
	auto StepRun = [&](int32 s, bool bFromCoach, FVehicleLane& L, int32& I0, int32& I1, bool& bPull, bool& bClosed, double& Jm) -> bool
	{
		const TSharedPtr<FJsonObject> St = (*Steps)[s]->AsObject();
		const FString Id = St->GetStringField(TEXT("road")); const bool bRight = St->GetStringField(TEXT("side")) == TEXT("right");
		const int32 Lane = int32(St->GetNumberField(TEXT("lane"))); Jm = 8.0; St->TryGetNumberField(TEXT("join"), Jm);
		if (!Road.Contains(Id) || !FVehicleLane::FromRoad(Id, bRight, Lane, L)) { Why = FString::Printf(TEXT("step %d: no lane %s %s %d"), s, *Id, bRight ? TEXT("right") : TEXT("left"), Lane); return false; }
		bClosed = false; Road[Id]->TryGetBoolField(TEXT("closed"), bClosed);
		bPull = false;
		auto Index = [&](const TCHAR* Key) -> int32
		{
			if (bFromCoach && FCString::Strcmp(Key, TEXT("from")) == 0) { bPull = true; return LoopNearest(L.Pts, Axle + Fwd * LoopPullLen); }
			const TSharedPtr<FJsonValue> V = St->TryGetField(Key);
			if (!V) return INDEX_NONE;
			if (V->Type == EJson::Array) return LoopNearest(L.Pts, LoopPlan(V->AsArray()));
			const FString T = V->AsString();
			if (T == TEXT("coach")) { bPull = true; return LoopNearest(L.Pts, Axle + Fwd * LoopPullLen); }
			if (T.StartsWith(TEXT("bay")))
			{
				const float Before = T.Len() > 4 ? FCString::Atof(*T.Mid(4)) * 100.f : 0.f;
				return LoopBack(L.Pts, LoopNearest(L.Pts, Bump), (CoachDim::XF * 100.f - LoopAxleF) + Before);
			}
			return INDEX_NONE;
		};
		I0 = Index(TEXT("from")); I1 = Index(TEXT("to"));
		return true;
	};
	// back to the terminal from somewhere along the loop (not at the hub: the views run's coaches start on the road): it
	// joins the loop at the step whose own stretch passes nearest to it
	int32 JoinStep = 0;
	if (!bToTown)
	{
		double JoinD = 1e30, HubD = 1e30;
		for (int32 s = 0; s < Steps->Num(); ++s)
		{
			FVehicleLane L; int32 A0 = INDEX_NONE, A1 = INDEX_NONE; bool bP = false, bC = false; double Jj = 0.0;
			if (!StepRun(s, s == 0, L, A0, A1, bP, bC, Jj) || A0 == INDEX_NONE || A1 == INDEX_NONE || !L.Pts.Num()) continue;
			double Near = 1e30;
			for (int32 i = A0, n = 0; n <= L.Pts.Num(); i = (i + 1) % L.Pts.Num(), ++n) { Near = FMath::Min(Near, FVector::Dist2D(L.Pts[i], Axle)); if (i == A1) break; }
			if (s == 0) HubD = Near;
			if (Near < JoinD) { JoinD = Near; JoinStep = s; }
		}
		Why.Reset();
		if (HubD < 3000.0 || JoinD > 3000.0) JoinStep = 0;   // (at the hub, or nowhere near the loop: from its start as always)
	}
	for (int32 s = JoinStep; s < Steps->Num(); ++s)
	{
		FVehicleLane L; int32 I0 = INDEX_NONE, I1 = INDEX_NONE; bool bPull = false, bClosed = false; double Jm = 8.0;
		if (!StepRun(s, s == JoinStep && JoinStep > 0, L, I0, I1, bPull, bClosed, Jm)) { Path.Reset(); return Path; }
		const FString Id = (*Steps)[s]->AsObject()->GetStringField(TEXT("road"));
		if (I0 == INDEX_NONE || I1 == INDEX_NONE || (I1 <= I0 && !bClosed)) { Why = FString::Printf(TEXT("step %d (%s) runs nowhere or backwards (%d -> %d)"), s, *Id, I0, I1); Path.Reset(); return Path; }
		TArray<FVector> Seg;
		for (int32 i = I0; i != I1; i = (i + 1) % L.Pts.Num()) Seg.Add(L.Pts[i]);
		Seg.Add(L.Pts[I1]);
		if (Seg.Num() < 2) Seg.Add(L.Pts[(I1 + 1) % L.Pts.Num()]);
		if (Path.Num() == 0) { Path.Add(Axle); LoopJoin(Path, Seg, bPull ? 0.f : float(Jm) * 100.f, Fwd); }
		else LoopJoin(Path, Seg, float(Jm) * 100.f, Fwd);
	}
	for (FVector& P : Path) P.Z = From.GetLocation().Z;   // (the riders put each wheel on the ground)
	return Path;
}

int32 ACoaches::RouteHits(int32 Ci, const FTransform& From, const TArray<FVector>& Path) const
{
	using namespace CoachLoop;
	// the body along the path: the front axle on it, the rear axle trailing (as the riders drive it)
	FVector Rear = From.TransformPosition(FVector(LoopAxleR, 0, 0)); float Run = 0.f;
	for (int32 k = 0; k < Path.Num() && Run < LoopLook; ++k)
	{
		if (k > 0) Run += float(FVector::Dist2D(Path[k], Path[k - 1]));
		const FVector F = Path[k]; FVector H = F - Rear; H.Z = 0.f; H = H.GetSafeNormal();
		if (H.IsNearlyZero()) continue;
		Rear = F - H * (LoopAxleF - LoopAxleR);
		const FTransform Me(H.Rotation(), F - H * LoopAxleF);
		for (int32 j = 0; j < Coaches.Num(); ++j)
			if (j != Ci && Coaches[j].M.IsValid() && FVector::Dist2D(RootW(Coaches[j]).GetLocation(), F) < 3000.f && LoopTouch(Me, RootW(Coaches[j]), LoopPad)) return j;
	}
	return INDEX_NONE;
}

bool ACoaches::SendOnRoute(int32 Ci, bool bToTown, int32 Bay, float LimitMs)
{
	using namespace CoachLoop;
	const TCHAR* Way = bToTown ? TEXT("to the town hub") : TEXT("back to the terminal");
	UVehicleRiders* R = Riders(Ci);
	if (!R) { ADP_WARN(TEXT("Coaches: coach %d can't go %s - no riders (adp.CoachLine off, or its driving parts missing)"), Ci, Way); return false; }
	if (!R->DriverAtWheel() || R->IsDriving() || RouteGo.Contains(Ci)) { ADP_WARN(TEXT("Coaches: coach %d can't go %s - %s"), Ci, Way, R->IsDriving() || RouteGo.Contains(Ci) ? TEXT("already on its way") : TEXT("no driver at the wheel")); return false; }
	const FTransform From = RootW(Coaches[Ci]);
	if (bToTown)
	{
		if (!HomeStand.Contains(Ci)) HomeStand.Add(Ci, From);
		if (Bay == INDEX_NONE)   // the first hub bay nobody has
		{
			TSet<int32> Taken; for (const TPair<int32, int32>& P : HubBayOf) if (P.Key != Ci) Taken.Add(P.Value);
			for (int32 b = 0; b < 64; ++b) if (!Taken.Contains(b)) { Bay = b; break; }
		}
	}
	// pulling out: backed up only as far as it needs to clear the coach in front, never into the one behind
	FString Why; TArray<FVector> Path; float Backed = 0.f; int32 Hit = INDEX_NONE, FirstHit = INDEX_NONE;
	for (Backed = 0.f; Backed <= LoopMaxBack; Backed += 25.f)
	{
		FTransform At = From; At.AddToTranslation(-From.GetUnitAxis(EAxis::X).GetSafeNormal2D() * Backed);
		if (Backed > 0.f)
		{
			bool bBehind = false;
			for (int32 j = 0; j < Coaches.Num() && !bBehind; ++j) bBehind = j != Ci && Coaches[j].M.IsValid() && LoopTouch(At, RootW(Coaches[j]), LoopPad);
			if (bBehind) { ADP_WARN(TEXT("Coaches: coach %d can't go %s - boxed in by coach %d in front, no room behind after %.2f m"), Ci, Way, FirstHit, Backed / 100.f - 0.25f); return false; }
		}
		Path = RoutePath(Ci, bToTown, Bay, At, Why);
		if (Path.Num() < 2) { ADP_WARN(TEXT("Coaches: coach %d can't go %s - %s"), Ci, Way, *Why); return false; }
		Hit = RouteHits(Ci, At, Path);
		if (Hit == INDEX_NONE) break;
		if (FirstHit == INDEX_NONE) FirstHit = Hit;
	}
	if (Hit != INDEX_NONE) { ADP_WARN(TEXT("Coaches: coach %d can't go %s - still touches coach %d after backing %.1f m"), Ci, Way, Hit, LoopMaxBack / 100.f); return false; }
	float Len = 0.f; for (int32 i = 1; i < Path.Num(); ++i) Len += float(FVector::Dist2D(Path[i], Path[i - 1]));
	FRouteGo& G = RouteGo.Add(Ci); G.Path = MoveTemp(Path); G.BackLeft = Backed; G.bToTown = bToTown; G.Bay = Bay; G.LimitMs = FMath::Clamp(LimitMs, 5.f, LoopTownLimit);
	if (bToTown) HubBayOf.Add(Ci, Bay); else HubBayOf.Remove(Ci);
	const FString BayS = bToTown ? FString::Printf(TEXT(" (hub bay %d)"), Bay) : FString();
	const FString BackS = Backed > 0.f ? FString::Printf(TEXT(", backing up %.2f m first (coach %d in front)"), Backed / 100.f, FirstHit) : FString(TEXT(", straight out"));
	ADP_LOG(TEXT("Coaches: coach %d sent %s%s - %.2f km%s"), Ci, Way, *BayS, Len / 100000.f, *BackS);
	return true;
}

namespace CoachTestT
{
	const TCHAR* const TestDrivers[] = {TEXT("MH_070_ground"), TEXT("MH_071_ground"), TEXT("MH_072_ground"), TEXT("MH_076_ground"), TEXT("MH_080_ground"), TEXT("MH_090_ground")};
	// the road's top near Z (roads, the airfield's ground; never the cut-away terrain's hidden collision)
	double RoadZ(UWorld* W, const FVector& At, double Near)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(CoachTestGround), true);
		FVector From(At.X, At.Y, Near + 1500.0); const FVector To(At.X, At.Y, Near - 1500.0);
		FHitResult Hit;
		for (int32 k = 0; k < 12 && W->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q); ++k)
		{
			const AActor* A = Hit.GetActor(); const FString C = A ? A->GetClass()->GetName() : FString();
			if (A && (C.Contains(TEXT("Road")) || C.Contains(TEXT("Airfield")) || C.Contains(TEXT("Ground")))) return Hit.ImpactPoint.Z;
			if (A) Q.AddIgnoredActor(A); else From.Z = Hit.ImpactPoint.Z - 5.0;
		}
		return Near;
	}
}

int32 ACoaches::TestOut(float Metres, float GapM)
{
	using namespace CoachLoop;
	UWorld* W = GetWorld(); if (!W) return 0;
	// the loop's way in starts at the hub: step 0's lane, its first point facing its second
	FString Text; TSharedPtr<FJsonObject> J; const TSharedPtr<FJsonObject>* Rt = nullptr; const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!FFileHelper::LoadFileToString(Text, *ADPSite::File(TEXT("roads.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()
		|| !J->TryGetObjectField(TEXT("routes"), Rt) || !(*Rt)->TryGetArrayField(TEXT("coach-to-terminal"), Steps) || !Steps->Num())
	{ ADP_WARN(TEXT("Coaches: test - no coach loop in roads.json")); return 0; }
	const TSharedPtr<FJsonObject> S0 = (*Steps)[0]->AsObject();
	FVehicleLane L0;
	if (!FVehicleLane::FromRoad(S0->GetStringField(TEXT("road")), S0->GetStringField(TEXT("side")) == TEXT("right"), int32(S0->GetNumberField(TEXT("lane"))), L0) || L0.Pts.Num() < 2)
	{ ADP_WARN(TEXT("Coaches: test - the loop's first lane is missing")); return 0; }
	const FVector HubDir = (L0.Pts[1] - L0.Pts[0]).GetSafeNormal2D();
	// each coach's way in from the hub to its own stand; the longest one (the stand furthest along) leads the file
	struct FOut { int32 Ci; TArray<FVector> Path; float Len; };
	TArray<FOut> Out;
	for (int32 i = 0; i < Coaches.Num(); ++i)
	{
		if (!Coaches[i].M.IsValid() || Coaches[i].Riders.IsValid() || RouteGo.Contains(i)) { ADP_WARN(TEXT("Coaches: test - coach %d stays (already has riders or is on its way)"), i); continue; }
		const FTransform Stand = RootW(Coaches[i]);
		HomeStand.Add(i, Stand);
		FTransform Hub(HubDir.Rotation(), FVector(L0.Pts[0].X, L0.Pts[0].Y, Stand.GetLocation().Z) - HubDir * LoopAxleF, Stand.GetScale3D());
		FString Why; TArray<FVector> P = RoutePath(i, false, INDEX_NONE, Hub, Why);
		if (P.Num() < 2) { ADP_WARN(TEXT("Coaches: test - coach %d has no way in (%s)"), i, *Why); continue; }
		float Len = 0.f; for (int32 k = 1; k < P.Num(); ++k) Len += float(FVector::Dist2D(P[k], P[k - 1]));
		Out.Add({i, MoveTemp(P), Len});
	}
	Out.Sort([](const FOut& A, const FOut& B) { return A.Len > B.Len; });
	if (!Out.Num()) return 0;
	const TArray<FVector>& Ref = Out[0].Path; const float RefLen = Out[0].Len;
	int32 Put = 0;
	for (int32 r = 0; r < Out.Num(); ++r)
	{
		const float Want = FMath::Max(0.f, RefLen - Metres * 100.f - float(r) * GapM * 100.f);
		float Run = 0.f; FVector Pt = Ref[0], Dir = (Ref[1] - Ref[0]).GetSafeNormal2D();
		for (int32 k = 1; k < Ref.Num(); ++k)
		{
			const float Seg = float(FVector::Dist2D(Ref[k], Ref[k - 1]));
			if (Seg <= 0.f) continue;
			if (Run + Seg >= Want) { const float T = (Want - Run) / Seg; Pt = FMath::Lerp(Ref[k - 1], Ref[k], T); Dir = (Ref[k] - Ref[k - 1]).GetSafeNormal2D(); break; }
			Run += Seg; Pt = Ref[k]; Dir = (Ref[k] - Ref[k - 1]).GetSafeNormal2D();
		}
		FCoach& C = Coaches[Out[r].Ci]; ABabylonModel* M = C.M.Get();
		const FTransform Stand = HomeStand[Out[r].Ci];
		FVector Root = Pt - Dir * LoopAxleF; Root.Z = CoachTestT::RoadZ(W, Root, Stand.GetLocation().Z);
		const FTransform Target(Dir.Rotation(), Root, Stand.GetScale3D());
		USceneComponent* Mr = M->GetModelRoot(); USceneComponent* Par = Mr ? Mr->GetAttachParent() : nullptr;
		if (!Mr) continue;
		C.RootBase = Par ? Target.GetRelativeTransform(Par->GetComponentTransform()) : Target;
		ApplyBody(C);
		ASeats::Follow(M, true);
		++Put;
		ADP_LOG(TEXT("Coaches: test - coach %d out on the way in at %.1f %.1f (%.0f m before its stand)"), Out[r].Ci, Root.X / 100.0, -Root.Y / 100.0, (Out[r].Len - Want) / 100.f);
	}
	return Put;
}

bool ACoaches::TestDriver(int32 Ci)
{
	UVehicleRiders* R = Riders(Ci);
	if (!R) return false;
	if (R->DriverAtWheel()) return true;
	if (!TestDriverOf.Contains(Ci))
	{
		AADPPeople* P = AADPPeople::Get(GetWorld()); if (!P) return false;
		const FTransform Root = RootOf(Ci);
		FVector At = Root.TransformPosition(FVector(320.f, 460.f, 0.f)); At.Z = CoachTestT::RoadZ(GetWorld(), At, Root.GetLocation().Z);
		if (P->AnyoneNear(At, 80.f) || !ADPUnseen::May(GetWorld(), At)) return false;
		AActor* D = P->Spawn(CoachTestT::TestDrivers[Ci % UE_ARRAY_COUNT(CoachTestT::TestDrivers)], At + FVector(0, 0, 60.f), float(Root.Rotator().Yaw) - 90.f);
		if (!D) return false;
		if (!R->HireDriver(D)) { D->Destroy(); ADP_WARN(TEXT("Coaches: test - coach %d's driver could not take the wheel"), Ci); return false; }
		TestDriverOf.Add(Ci, D);
		ADP_LOG(TEXT("Coaches: test - coach %d's driver comes"), Ci);
	}
	if (!R->IsBusy(TestDriverOf[Ci].Get())) R->DriverGetIn();
	return R->DriverAtWheel();
}

bool ACoaches::RouteFree(int32 Ci, bool bToTown) const
{
	if (!Coaches.IsValidIndex(Ci)) return false;
	FString Why; const FTransform From = RootW(Coaches[Ci]);
	const TArray<FVector> P = RoutePath(Ci, bToTown, INDEX_NONE, From, Why);
	return P.Num() >= 2 && RouteHits(Ci, From, P) == INDEX_NONE;
}

void ACoaches::RouteTick(float Dt)
{
	using namespace CoachLoop;
	for (auto It = RouteGo.CreateIterator(); It; ++It)
	{
		const int32 Ci = It.Key(); FRouteGo& G = It.Value();
		UVehicleRiders* R = Coaches.IsValidIndex(Ci) ? Riders(Ci) : nullptr;
		if (!R) { if (G.Beep) G.Beep->Stop(); ADP_WARN(TEXT("Coaches: coach %d's loop dropped (its riders gone)"), Ci); It.RemoveCurrent(); continue; }
		if (G.bDriving)
		{
			if (!R->IsDriving()) { ADP_LOG(TEXT("Coaches: coach %d %s"), Ci, G.bToTown ? *FString::Printf(TEXT("in hub bay %d"), G.Bay) : TEXT("back on its stand")); It.RemoveCurrent(); }
			continue;
		}
		FCoach& C = Coaches[Ci];
		if (G.BackLeft > 0.f)
		{
			if (!G.Beep && SndBeep && C.M.IsValid())
			{
				G.Beep = UGameplayStatics::SpawnSoundAttached(SndBeep, C.M->GetModelRoot(), NAME_None, FVector(-500.f, 0.f, 120.f), EAttachLocation::KeepRelativeOffset, false, 0.6f, 1.f, 0.f, nullptr, nullptr, false);
				if (G.Beep) Keep.Add(G.Beep);
				ADP_LOG(TEXT("Coaches: coach %d backs up %.2f m to pull out (reverse beep %s)"), Ci, G.BackLeft / 100.f, G.Beep ? TEXT("on") : TEXT("MISSING"));
			}
			if (G.Beep && !G.Beep->IsPlaying()) G.Beep->Play();
			SetLamps(Ci, uint8(LowBeam | Tail | Reverse));
			const float Step = FMath::Min(G.BackLeft, LoopBackSpeed * Dt);
			FTransform Rb = C.RootBase; Rb.AddToTranslation(-Rb.GetUnitAxis(EAxis::X).GetSafeNormal2D() * Step);
			DrivePlace(Ci, Rb, 0.f, 0.f, 0.f, C.Line.RollRad - Step / 52.f, 0.f, 0.f, 0.f, 0.f);
			G.BackLeft -= Step; G.Backed += Step;
			continue;
		}
		if (G.Beep) { G.Beep->Stop(); Keep.Remove(G.Beep); G.Beep = nullptr; ADP_LOG(TEXT("Coaches: coach %d backed %.2f m - beep off"), Ci, G.Backed / 100.f); }
		SetLamps(Ci, uint8(LowBeam | Tail));
		// the path into the coach's parent space (the riders drive its RootBase)
		const FTransform Parent = C.RootBase.Inverse() * RootW(C);
		TArray<FVector> Local; Local.Reserve(G.Path.Num());
		for (const FVector& P : G.Path) Local.Add(Parent.InverseTransformPosition(P));
		if (!R->Drive(Local, G.LimitMs)) { ADP_WARN(TEXT("Coaches: coach %d's riders won't drive the loop (someone not seated, or you aboard)"), Ci); It.RemoveCurrent(); continue; }
		G.bDriving = true;
	}
}
#pragma endregion

void ACoaches::Tick(float Dt)
{
	Super::Tick(Dt);
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	const FVector MeAt = Me ? Me->GetActorLocation() : FVector::ZeroVector;
	ReachTick(Dt);
	RouteTick(Dt);
	// your capsule hip-wide inside a coach (the aisle 0.45 m), your own width again outside
	if (ACharacter* Ch = Cast<ACharacter>(Me))
	{
		const bool bIn = IsInside(MeAt - FVector(0, 0, Ch->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		if (bIn != bCapsuleSmall) { bCapsuleSmall = bIn; Ch->GetCapsuleComponent()->SetCapsuleRadius(bIn ? 20.f : 34.f, true); ADP_LOG(TEXT("Coaches: you %s a coach (capsule %s)"), bIn ? TEXT("in") : TEXT("out of"), bIn ? TEXT("hip-wide") : TEXT("as before")); }
	}
	int32 NearBeams = INDEX_NONE; float NB = 12000.f;
	for (int32 i = 0; i < Coaches.Num(); ++i) if (ABabylonModel* M = Coaches[i].M.Get()) if (Coaches[i].Lamps & (LowBeam | HighBeam)) { const float D = FVector::Dist(M->GetActorLocation(), MeAt); if (D < NB) { NB = D; NearBeams = i; } }
	for (int32 i = 0; i < Coaches.Num(); ++i)
	{
		FCoach& C = Coaches[i];
		ABabylonModel* M = C.M.Get();
		if (!M) continue;
		bool bMoved = false;
		// doors: 1.2 s to open or shut; the kneel follows them
		for (int32 d = 0; d < 2; ++d)
		{
			const float To = C.bDoorOpen[d] ? 1.f : 0.f;
			if (C.DoorK[d] == To) continue;
			C.DoorK[d] = To > C.DoorK[d] ? FMath::Min(To, C.DoorK[d] + Dt / 1.2f) : FMath::Max(To, C.DoorK[d] - Dt / 1.2f);
			const float E = FMath::SmoothStep(0.f, 1.f, C.DoorK[d]);
			for (int32 k = 0; k < 2; ++k)
			{
				const FMover& L = C.Leaf[d][k];
				if (L.Group == INDEX_NONE) continue;
				const float Out = FMath::Min(1.f, E * 4.f);      // (out first, then the turn)
				const float Turn = FMath::Clamp((E - 0.15f) / 0.85f, 0.f, 1.f);
				M->MoveGroup(L.Group, FTransform(FQuat::Slerp(FQuat::Identity, L.OpenTurn, Turn) * L.Pivot.GetRotation(), L.Pivot.GetLocation() + L.OpenShift * Out));
				bMoved = true;
			}
		}
		for (int32 b = 0; b < 6; ++b)
		{
			const float To = C.bBayOpen[b] ? 1.f : 0.f;
			if (C.BayK[b] == To || C.Bay[b].Group == INDEX_NONE) continue;
			C.BayK[b] = To > C.BayK[b] ? FMath::Min(To, C.BayK[b] + Dt / 1.6f) : FMath::Max(To, C.BayK[b] - Dt / 1.6f);
			const FMover& Bm = C.Bay[b];
			M->MoveGroup(Bm.Group, FTransform(FQuat::Slerp(FQuat::Identity, Bm.OpenTurn, FMath::SmoothStep(0.f, 1.f, C.BayK[b])) * Bm.Pivot.GetRotation(), Bm.Pivot.GetLocation()));
			bMoved = true;
		}
		// the kneel: the body down 6 cm and tipped 1.1 deg towards the door side, the wheels kept where they stand
		const float KTo = (C.bDoorOpen[0] || C.bDoorOpen[1]) ? 1.f : 0.f;
		if (C.Kneel != KTo)
		{
			C.Kneel = KTo > C.Kneel ? FMath::Min(KTo, C.Kneel + Dt / 1.8f) : FMath::Max(KTo, C.Kneel - Dt / 2.4f);
			ApplyBody(C); bMoved = true;
		}
		if (bMoved) M->FlushMoves();
		// lamps, and the real beams for the nearest coach with its beams on
		Lamp(C, Dt);
		for (USpotLightComponent* L : C.Beams)
		{
			const bool bOn = i == NearBeams && !bNoRealBeams;
			if (L->IsVisible() != bOn) L->SetVisibility(bOn);
			if (bOn) L->SetIntensity((C.Lamps & HighBeam) ? 9000.f : 3500.f);
		}
		// the drive's hum: full outside, muffled inside (walls and floor between you and the motor at the back)
		if (C.Hum)
		{
			int32 InWhich = INDEX_NONE; const bool bIn = IsInside(MeAt, &InWhich) && InWhich == i;
			if (C.Drive > 0.01f && !C.Hum->IsPlaying()) C.Hum->Play();
			if (C.Drive <= 0.01f && C.Hum->IsPlaying()) C.Hum->Stop();
			C.Hum->SetVolumeMultiplier(C.Drive * (bIn ? 0.35f : 1.f));
			C.Hum->SetLowPassFilterEnabled(bIn); C.Hum->SetLowPassFilterFrequency(900.f);
			C.Hum->SetPitchMultiplier(0.8f + 0.5f * C.Drive);
		}
	}
	if (Me && !bNoMirrors) Mirror(Dt, MeAt);
}
