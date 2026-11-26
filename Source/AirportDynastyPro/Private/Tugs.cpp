#include "Tugs.h"
#include "Site.h"
#include "GSE.h"
#include "ApronBus.h"
#include "StaffPeople.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "ADPKeep.h"
#include "BabylonModel.h"
#include "Airfield.h"
#include "TerminalPlan.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"

namespace
{
	// the tug's own measures (Tools/make_pushback_tug.py, metres): its middle to the towbar head's pin
	constexpr float HeadAhead = 2.5525f + 0.12f + 5.25f;    // (the 5.2 m bar: the tug ~0.9 m in front of an A320's nose tip)
	// the ramp (plan metres, Data/airfield.json): the head-of-stand road between the terminal and the noses, the GSE park
	// at the terminal apron's west end (two places, front to the west), the staging spot beside a stand outside its red line
	// (owner 2026-11-21: no vehicle ever on a jetway's red zone - St.X+3..+16 from the terminal wall to 9 m past the nose:
	//  Airfield.cpp - so no road between the terminal and the noses: the service lane runs behind the stands' tails,
	//  between their tail restraint lines and the taxilane; the way in to a nose is up the side away from the jetway,
	//  past the wingtip, then across in front of the nose)
	constexpr float RoadY = 104.f, LaneY = 176.f, ParkX = -257.f, ParkExitX = -252.f, StageAside = -21.f, SideX = -19.8f;
	// (the tugs' parking boxes at the head of each contact stand - Airfield.cpp draws them: x StandX-15..-11, y 87..99 - the
	//  tug in the middle, facing the gate)
	constexpr float GateY = 104.f, BoxX = -13.f, BoxY = 91.f;   // (the tug 88.4..93.6, its bar to 98.9: wholly inside the box)   // (1.3+ m off every red line: the stand boundaries at St.X -+ 22.5, the wingtip at -18)
	const float ParkY[] = {92.f, 112.f};
	constexpr float DriveSpeed = 700.f, SlowSpeed = 150.f;   // cm/s (25 km/h on the apron's straights; 5 km/h reversing, by the aircraft)
	enum ELensKind : uint8 { Low, Tail, Marker, Reverse, Beacon, Indicator };
	USoundAttenuation* Att()
	{
		static USoundAttenuation* A = nullptr;
		if (!A)
		{
			A = ADPKeep(NewObject<USoundAttenuation>(GetTransientPackage()));
			FSoundAttenuationSettings& S = A->Attenuation;
			S.bAttenuate = true; S.bSpatialize = true; S.AttenuationShapeExtents = FVector(200.f); S.FalloffDistance = 6000.f;
			S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; S.dBAttenuationAtMax = -60.f;
			// heard from where you stand: through walls and the terminal muffled (the sound placement rule)
			S.bEnableOcclusion = true; S.OcclusionTraceChannel = ECC_Visibility; S.OcclusionLowPassFilterFrequency = 900.f; S.OcclusionVolumeAttenuation = 0.35f; S.OcclusionInterpolationTime = 0.15f;
		}
		return A;
	}
	USoundBase* Snd(const TCHAR* Name)
	{
		static TMap<FString, USoundBase*> Cache;
		if (USoundBase** F = Cache.Find(Name)) return *F;
		USoundBase* S = ADPKeep(LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/%s/Sounds/%s.%s"), FCString::Strncmp(Name, TEXT("GPU"), 3) == 0 ? TEXT("GPU") : TEXT("PushbackTug"), Name, Name)));
		if (!S) ADP_WARN(TEXT("Tugs: sound %s missing"), Name);
		Cache.Add(Name, S); return S;
	}
}

ATugs::ATugs()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

ATugs* ATugs::Get(UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<ATugs> It(W); It; ++It) return *It;
	ATugs* T = W->SpawnActor<ATugs>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (T) T->Setup();
	return T;
}

bool ATugs::RemoteStand(float StandX)
{
	const FAirfieldData::FStand* St = AAirfield::Shared().StandAt(StandX);
	return St && St->bRemote;
}

bool ATugs::CargoStand(float StandX)
{
	const FAirfieldData::FStand* St = AAirfield::Shared().StandAt(StandX);
	return St && St->bCargo;
}

float ATugs::CrewYaw(float StandX)
{
	return (RemoteStand(StandX) || CargoStand(StandX) ? FVector(-0.3f, 1.f, 0.f) : FVector(0.3f, -1.f, 0.f)).Rotation().Yaw;   // (looking out at the stand)
}

int32 ATugs::SideOf(double WorldY)
{
	const double Y = -WorldY / 100.0;
	return Y < 240.0 ? 0 : Y < AAirfield::Shared().RwyY ? 1 : 2;
}

void ATugs::BoxT(float StandX, float& Side, float& Run, float& Y)
{
	float Rl, Ly; BoxT(StandX, Side, Run, Y, Rl, Ly);
}

void ATugs::BoxT(float StandX, float& Side, float& Run, float& Y, float& Rl, float& Ly)
{
	Side = BoxX; Run = -1.f; Y = BoxY; Rl = 6.f; Ly = BoxY + 3.4f;
	const FAirfieldData::FStand* St = AAirfield::Shared().StandAt(StandX); if (!St) return;
	if (const float Head = St->TugHead(Rl); Head > 0.f) { Side = 0.f; Run = -2.f * Rl; Y = Head; Ly = Head + (St->bRemote ? 5.6f : St->bCargo ? 9.4f : St->Span > 40.f ? 11.9f : 9.f); }
}

bool ATugs::HeadStand(float StandX)
{
	float Rl; const FAirfieldData::FStand* St = AAirfield::Shared().StandAt(StandX);
	return St && St->TugHead(Rl) > 0.f;
}

FVector ATugs::StandT(float StandX, float X, float Y) const
{
	const FAirfieldData::FStand* St = AAirfield::Shared().StandAt(StandX);
	const FVector2f P = St ? St->Frame().Turn(FVector2f(X, Y)) : FVector2f(X, Y);
	return Plan(P.X, P.Y);
}

FVector ATugs::GateT(float StandX, float X, float Y) const
{
	// (a cargo stand: the strip between the noses and the cargo building, clear of the loop)
	if (CargoStand(StandX)) return StandT(StandX, X - StandX < -12.f ? X - 7.f : X, AAirfield::Shared().StandAt(StandX)->Frame().Y - 17.1f + (Y - 86.f) * 0.42f);
	if (!RemoteStand(StandX))
	{
		// (a walk-out stand: behind the buses in front of the gate's doors; a big one: drawn in to the 12 m to its nose, clear of the loop)
		if (!HeadStand(StandX)) return Plan(X, Y);
		if (AAirfield::Shared().StandAt(StandX)->Span <= 40.f) return Plan(X, 95.f + (Y - 86.f) * 0.75f);
		return Plan(X - StandX < -12.f ? X - 10.f : X, 85.f + (Y - 86.f) * 0.55f);
	}
	return StandT(StandX, X, AAirfield::Shared().StandAt(StandX)->Frame().Y - 20.f + (Y - 86.f) * 11.f / 24.f);
}

bool ATugs::SameSide(const FCrew& C, float StandX) const
{
	return C.A.IsValid() && SideOf(C.A->GetActorLocation().Y) == (RemoteStand(StandX) ? 1 : CargoStand(StandX) ? 2 : 0);
}

FVector ATugs::Plan(float X, float Y) const
{
	// (inside the terminal - the baggage hall, the departure cart lane, where the crew walk to the baggage tractors - its
	//  ground floor; outside the airfield's ground)
	const bool bIn = Y < 82.6f && Y > -10.f && X > -242.f && X < 552.f;
	return FTerminalPlan::ToWorld(X, bIn ? 0.f : AAirfield::GroundAt(X, Y), Y);
}

void ATugs::Setup()
{
	if (bReady) return;
	bReady = true;
	// (owner 2026-11-21: "the pushback truck should always be placed in front of the terminal, right outside of the gates")
	// one tug at every contact gate: on the stand's centre line in front of the terminal, facing where the nose will be -
	// an arriving aircraft stops ~7 m short of its towbar; nothing on a red marking or under a wing
	// (the C gates with a jetway and the remote stands first: the few test crews are given out in this order; then the walk-out, then the big stands)
	for (int32 Pass = 0; Pass < 3; ++Pass) for (const FAirfieldData::FStand& St : AAirfield::Shared().Stands)
	{
		if (!St.HasTug() || (St.Span > 40.f ? 2 : St.bJetway || St.bRemote ? 0 : 1) != Pass) continue;
		float Bs = 0.f, Br = 0.f, By = 0.f; BoxT(St.X, Bs, Br, By);
		FTug T; T.StandX = St.X; T.StandNose = St.Nose; T.StandDir = St.Dir; T.Home = T.At = StandT(St.X, St.X + Bs, By);
		T.HomeYaw = T.Yaw = FVector(0.f, St.Frame().Face().Y, 0.f).Rotation().Yaw;   // (facing along the stand, away from the terminal: plan +y is world -y)
		if (SpawnAt(T)) { Pose(T); GpuSetup(T); Tugs.Add(T); }
	}
	// (a cargo stand's tug among the first five: one of the few test crews stands on the cargo apron - nobody walks there across the runway)
	if (const int32 Cg = Tugs.IndexOfByPredicate([](const FTug& T) { return CargoStand(T.StandX); }); Cg > 4) { FTug Mv = MoveTemp(Tugs[Cg]); Tugs.RemoveAt(Cg); Tugs.Insert(MoveTemp(Mv), 4); }
	ADP_LOG(TEXT("Tugs: %d tugs ready at the gates"), Tugs.Num());
	SetupCrew();
	Tugs.Reserve(Tugs.Num() + 16);   // (room for bought ones, to-do 318: the entries never move - ExtraAdd)
	HangarSetup();   // (after the gates' crews: it has none of its own - your hired people only, to-do 311)
	// the baggage tractors and carts (GSE.h): the same ground crew, the same apron
	AGSE::Get(GetWorld());
	// the apron buses (ApronBus.h): parked, only where there are remote stands
	AApronBuses::Get(GetWorld());
}

bool ATugs::SpawnAt(FTug& T, const TCHAR* Model)
{
	ABabylonModel* M = GetWorld()->SpawnActor<ABabylonModel>(T.At, FRotator::ZeroRotator);
	if (!M || !M->Place(Model)) { if (M) M->Destroy(); ADP_WARN(TEXT("Tugs: the tug model is missing (Tools/import_model.py ... %s)"), Model); return false; }
	M->SetPartsCollision(false);
	T.M = M;
	// its windows: the fire truck's glass; its lenses: the fire truck's lens material, one glow value per lens
	UMaterialInterface* Glass = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_TruckGlass.M_TruckGlass")));
	UMaterialInterface* LensM = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_TruckLens.M_TruckLens")));
	TArray<UInstancedStaticMeshComponent*> Parts; M->Parts(Parts);
	TMap<UMaterialInterface*, UMaterialInterface*> Swap;
	for (UInstancedStaticMeshComponent* K : Parts)
		for (int32 Si = 0; Si < K->GetNumMaterials(); ++Si)
		{
			UMaterialInterface* Mi = K->GetMaterial(Si); if (!Mi) continue;
			if (UMaterialInterface** F = Swap.Find(Mi)) { if (*F) K->SetMaterial(Si, *F); continue; }
			const UMaterialInstanceDynamic* Md = Cast<UMaterialInstanceDynamic>(Mi);
			const FString N = Md && Md->Parent ? Md->Parent->GetName() : Mi->GetName();
			UMaterialInstanceDynamic* D = nullptr;
			if (N.Contains(TEXT("Tug_Glass")) && Glass)
			{
				D = UMaterialInstanceDynamic::Create(Glass, this); D->SetVectorParameterValue(TEXT("BC"), FLinearColor(0.015f, 0.022f, 0.026f, 0.38f));
			}
			else if (LensM && (N.Contains(TEXT("Tug_HeadLens")) || N.Contains(TEXT("Tug_RedLens")) || N.Contains(TEXT("Tug_BeaconLens"))))
			{
				D = UMaterialInstanceDynamic::Create(LensM, this);
				const bool bRed = N.Contains(TEXT("Red")), bAmber = N.Contains(TEXT("Beacon"));
				D->SetVectorParameterValue(TEXT("BC"), bRed ? FLinearColor(1.f, 0.04f, 0.02f) : bAmber ? FLinearColor(1.f, 0.45f, 0.03f) : FLinearColor(1.f, 0.97f, 0.9f));
				D->SetScalarParameterValue(TEXT("Glow"), bAmber ? 60.f : bRed ? 30.f : 28.f); D->SetScalarParameterValue(TEXT("Dots"), 4.f);
				K->SetNumCustomDataFloats(1);
			}
			Swap.Add(Mi, D); if (D) K->SetMaterial(Si, D);
		}
	// which lens is which lamp (its node's name, Tools/make_pushback_tug.py)
	struct FW { const TCHAR* Name; ELensKind Kind; };
	static const FW Which[] = {{TEXT("LowBeam_"), Low}, {TEXT("TailBrake_"), Tail}, {TEXT("SideMarker_"), Marker}, {TEXT("Reverse_"), Reverse},
		{TEXT("BeaconLens"), Beacon}, {TEXT("FlasherRear_"), Beacon}, {TEXT("IndicatorF_"), Indicator}, {TEXT("IndicatorR_"), Indicator}};
	for (const FW& Wh : Which)
	{
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Found; M->InstancesNamed(Wh.Name, true, Found);
		for (const auto& F : Found)
			if (F.Get<2>().EndsWith(TEXT("_Lens")) || F.Get<2>() == TEXT("BeaconLens")) { FLens L; L.K = F.Get<0>(); L.Ix = F.Get<1>(); L.Kind = Wh.Kind; if (Wh.Kind == Indicator) L.Side = F.Get<2>().Contains(TEXT("_-1")) ? -1 : 1; T.Lenses.Add(L); }
	}
	// the wheels: Blender (x, y, z) m is (x, -y, z) * 100 in the model's root; the axles at x +-1.1555, the wheels' middles
	// 1.054 m out, the hubs 0.538 m up (Tools/make_pushback_tug.py: WB 2.311, TR 2.108, TD 1.076); names: <part>_<axle><side>
	{
		const TCHAR* Keys[4] = {TEXT("0-1"), TEXT("01"), TEXT("1-1"), TEXT("11")};
		const float Ax[4] = {1.1555f, 1.1555f, -1.1555f, -1.1555f}, Sd[4] = {-1.f, 1.f, -1.f, 1.f};
		for (int32 w = 0; w < 4; ++w)
		{
			T.WheelAt[w] = FVector(Ax[w] * 100.f, -Sd[w] * 105.4f, 53.8f);
			TArray<FString> Names; for (const TCHAR* P : {TEXT("Tyre_"), TEXT("Rim_"), TEXT("WheelDisc_"), TEXT("Hub_"), TEXT("HubCap_"), TEXT("Nut_")}) Names.Add(FString(P) + Keys[w]);
			T.Wheel[w] = M->BindGroup(Names, true, FTransform(T.WheelAt[w]));
		}
	}
	// dirt and washing as the fire truck (owner 2026-11-21: every ground vehicle - VehicleDirt.h; after the glass and lenses)
	T.Dirt.Dress(M, this, FString::Printf(TEXT("pushback tug at x %.0f"), T.StandX));
	Lamps(T);
	return true;
}

void ATugs::Wheels(FTug& T, float Dt)
{
	ABabylonModel* M = T.M.Get(); if (!M || Dt <= 0.f) return;
	if (!T.bPrev) { T.bPrev = true; T.PrevAt = T.At; T.PrevYaw = T.Yaw; return; }
	const FVector Fwd = FRotator(0.f, T.Yaw, 0.f).Vector();
	const FVector D = T.At - T.PrevAt;
	const float Dist = D.Size2D(), Dir = FVector::DotProduct(D, Fwd) >= 0.f ? 1.f : -1.f;
	const float YawRate = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(T.PrevYaw, T.Yaw)) / Dt;
	T.PrevAt = T.At; T.PrevYaw = T.Yaw;
	// rolled: the distance over the tyre's radius (0.538 m)
	T.Spin = FMath::Fmod(T.Spin + Dir * Dist / 53.8f, 2.f * PI);
	// steered: four-wheel steering turns about the middle - each axle half the wheelbase from it, so tan(angle) =
	// (WB/2) * yaw rate / speed; at most 30 degrees, eased in and out
	const float Speed = Dist / Dt / 100.f;
	// (the brake lamps follow the measured speed - driven or pushed on the aircraft: on while slowing, held 0.6 s)
	T.MeasSpeed = FMath::FInterpTo(T.MeasSpeed, Speed, Dt, 6.f);
	T.BrakeHold = T.MeasSpeed < T.LastSpeed - 0.15f * Dt ? 0.6f : FMath::Max(0.f, T.BrakeHold - Dt); T.LastSpeed = T.MeasSpeed;
	float Want = Speed > 0.05f ? FMath::RadiansToDegrees(FMath::Atan(1.1555f * YawRate / Speed)) * Dir : 0.f;
	Want = FMath::Clamp(Want, -30.f, 30.f);
	T.Steer = FMath::FInterpTo(T.Steer, Want, Dt, 4.f);
	for (int32 w = 0; w < 4; ++w)
	{
		if (T.Wheel[w] == INDEX_NONE) continue;
		const float S = (w < 2 ? 1.f : -1.f) * T.Steer;
		// (root space: x forward, y = -Blender y, z up; the axle is the y axis; steering about z)
		const FQuat Q = FQuat(FVector::UpVector, FMath::DegreesToRadians(-S)) * FQuat(FVector::RightVector, -T.Spin);
		M->MoveGroup(T.Wheel[w], FTransform(Q, T.WheelAt[w]));
	}
	M->FlushMoves();
}

void ATugs::Sound(const TCHAR* Name, const FVector& At)
{
	if (USoundBase* S = Snd(Name)) UGameplayStatics::PlaySoundAtLocation(this, S, At, 1.f, 1.f, 0.f, Att());
}

void ATugs::Engine(FTug& T, bool bOn)
{
	if (bOn == (T.Engine != nullptr) || !T.M.IsValid()) return;
	if (bOn)
	{
		Sound(TEXT("TugStart"), T.At);
		T.Engine = UGameplayStatics::SpawnSoundAttached(Snd(TEXT("TugIdle")), T.M->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, 0.9f, 1.f, 0.f, Att(), nullptr, false);
	}
	else { T.Engine->Stop(); T.Engine = nullptr; Sound(TEXT("TugStop"), T.At); }
}

void ATugs::Pose(FTug& T)
{
	// the model's front (Blender +x, the towbar's end) faces the actor's forward: the actor turned the way the tug looks
	if (ABabylonModel* M = T.M.Get()) M->SetActorLocationAndRotation(T.At, FRotator(0.f, T.Yaw, 0.f));   // (owner 2026-11-21: with +180 the towbar pointed away from the aircraft)
}

void ATugs::Lamps(FTug& T)
{
	// running: low beams, tail lamps and markers; backing: the reversing lamps; the beacons flash as the jetway beacons do
	const bool bRun = T.Engine != nullptr, bBack = bRun && (T.State == EState::You ? T.bYouBack : T.bReverse && T.Path.Num() > 0);
	const float F = FMath::Fmod(Flash, 1.f);
	const bool bFlash = bRun && (F < 0.08f || (F > 0.18f && F < 0.26f));
	// (owner 2026-11-21: the lights animated as on a real tug) brake lamps: bright while it slows down and at a standstill with
	// the engine running (the driver's foot on the brake), dim tail lamps when it rolls on; the indicators blink (1.5 Hz,
	// the legal 60-120 a minute) on the side it turns to - the front wheels' steer, ~6 deg and more
	const bool bBrake = bRun && (T.BrakeHold > 0.f || T.MeasSpeed < 0.05f);
	const bool bYou = T.State == EState::You, bHaz = bYou && T.bYouHaz;   // (with you: your own switches)
	const int8 Turn = bYou ? T.YouSig : !bRun ? 0 : T.Steer > 6.f ? -1 : T.Steer < -6.f ? 1 : 0;   // (yaw + is a right turn: the right side, -1)
	const bool bBlink = FMath::Fmod(Flash * 1.5f, 1.f) < 0.5f;
	for (FLens& L : T.Lenses)
	{
		float V = 0.f;
		switch (L.Kind)
		{
		case Low: V = bRun && !(bYou && T.YouBeam == 2) ? (bYou && T.YouBeam == 1 ? 1.f : 0.8f) : 0.f; break;
		case Tail: V = bBrake ? 1.f : bRun ? 0.3f : 0.f; break;
		case Indicator: V = ((bHaz || (Turn != 0 && L.Side == Turn)) && bBlink) ? 1.f : 0.f; break;
		case Marker: V = bRun ? 0.5f : 0.f; break;
		case Reverse: V = bBack ? 1.f : 0.f; break;
		case Beacon: V = bFlash && !(bYou && T.bYouNoBeacon) ? 1.f : 0.f; break;
		default: V = 0.f;
		}
		if (V != L.Now && L.K) { L.Now = V; L.K->SetCustomDataValue(L.Ix, 0, V, true); }
	}
	// the reversing beeper with the reversing lamps
	if (bBack && !T.Beep && T.M.IsValid())
		T.Beep = UGameplayStatics::SpawnSoundAttached(Snd(TEXT("TugReverseBeep")), T.M->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, 0.7f, 1.f, 0.f, Att(), nullptr, false);
	if (!bBack && T.Beep) { T.Beep->Stop(); T.Beep = nullptr; }
}

int32 ATugs::Assign(const FString& Key, float StandX)
{
	for (int32 i = 0; i < Tugs.Num(); ++i) if (Tugs[i].Key == Key) return i;
	if (Key.StartsWith(TEXT("TOW/")))
	{
		// (a hangar tow: the hangar tug alone, never the stand's own - from its place by the bus chargers, your hired crew, to-do 311)
		// (the one you coupled at its nose, else a free one - a bought one too, to-do 318 - else the first)
		int32 H = Tugs.IndexOfByPredicate([&](const FTug& U) { return U.bHangar && U.State == EState::You && U.YouTowKey == Key; });
		if (H == INDEX_NONE) H = Tugs.IndexOfByPredicate([this](const FTug& U) { return U.bHangar && U.State == EState::Parked && U.Key.IsEmpty() && (!U.bHired || HiredFree(U)); });
		if (H == INDEX_NONE) H = Tugs.IndexOfByPredicate([](const FTug& U) { return U.bHangar; });
		if (H == INDEX_NONE) return INDEX_NONE;
		FTug& T = Tugs[H];
		// (coupled by you at that aircraft's nose and you still there: your tow, to-do 311 step C - TugYou.cpp, HangarTow::YouRun)
		if (T.State == EState::You && T.YouStep == 3 && !T.bYouCrew && T.Key.IsEmpty() && T.YouTowKey == Key && T.YouLeft < 120.f)
		{
			T.StandX = StandX;
			if (const int32 S = StandTug(StandX); S != INDEX_NONE) { T.StandNose = Tugs[S].StandNose; T.StandDir = Tugs[S].StandDir; }
			T.Key = Key; T.Path.Reset(); T.Seq = 0; T.SeqT = 0.f; T.EngT = -1.f; T.bDeiced = T.bPushReq = T.bCleared = T.bEngines = false; T.PreSeq = 0; T.PreAt = 0.0; T.Talk.Reset(); T.TalkT = 0.f;
			T.bYouPush = true; T.bYouDone = T.bYouGo = T.bYouCrewIn = false; T.bYouRejoin = true; T.YouUn = 0; T.YouBend = 0.f; T.YouBeat = GetWorld()->GetTimeSeconds(); T.YouLeft = 0.f; T.YouWhy.Reset(); T.YouCap = 0.f;
			T.bHired = HiredJob(Key); T.Driver = T.Handler = INDEX_NONE; T.CrewAsk = 0.f;
			Engine(T, true); ADP_LOG(TEXT("Tugs: the hangar tug %d - you tow %s (your own hired crew %d)"), H, *Key, T.bHired);
			return H;
		}
		if (!HiredJob(Key)) return INDEX_NONE;
		if (T.State != EState::Parked || !T.Key.IsEmpty()) return INDEX_NONE;
		if (T.bHired) { if (!HiredFree(T)) return INDEX_NONE; HiredEnd(T); }
		T.StandX = StandX;
		if (const int32 S = StandTug(StandX); S != INDEX_NONE) { T.StandNose = Tugs[S].StandNose; T.StandDir = Tugs[S].StandDir; }
		T.Key = Key; T.Path.Reset(); T.Seq = 0; T.SeqT = 0.f; T.EngT = -1.f; T.bDeiced = T.bPushReq = T.bCleared = T.bEngines = false; T.PreSeq = 0; T.PreAt = 0.0; T.Talk.Reset(); T.TalkT = 0.f;
		T.bHired = true; T.Driver = T.Handler = INDEX_NONE; T.CrewAsk = 0.f; T.HtRetry = 0.f;
		ADP_LOG(TEXT("Tugs: the hangar tug %d gets ready for %s at x %.0f"), H, *Key, StandX);
		return H;
	}
	// (coupled by you and you still there: your push, its driver stays where he is - TugYou.cpp)
	for (int32 y = 0; y < Tugs.Num(); ++y)
		if (FTug& Y = Tugs[y]; !Y.bHangar && Y.State == EState::You && Y.YouStep == 3 && !Y.bYouCrew && Y.Key.IsEmpty() && FMath::Abs(Y.StandX - StandX) < 1.f && Y.YouLeft < 120.f)
		{
			Y.Key = Key; Y.Path.Reset(); Y.Seq = 0; Y.SeqT = 0.f; Y.EngT = -1.f; Y.bDeiced = Y.bPushReq = Y.bCleared = Y.bEngines = false; Y.PreSeq = 0; Y.PreAt = 0.0; Y.Talk.Reset(); Y.TalkT = 0.f;
			Y.bYouPush = true; Y.bYouDone = Y.bYouGo = Y.bYouCrewIn = false; Y.bYouRejoin = true; Y.YouUn = 0; Y.YouBend = 0.f; Y.YouBeat = GetWorld()->GetTimeSeconds(); Y.YouLeft = 0.f; Y.YouWhy.Reset();
			Y.bHired = !CargoStand(StandX) && HiredJob(Key); if (Y.bHired) { Y.Driver = Y.Handler = INDEX_NONE; Y.CrewAsk = 0.f; }
			Engine(Y, true); ADP_LOG(TEXT("Tugs: tug %d - you push %s (your own hired crew %d)"), y, *Key, Y.bHired);
			if (!Y.bHired && Y.Driver != INDEX_NONE) CrewWalkers(y);
			if (!Y.bHired && Y.Handler != INDEX_NONE) { if (Y.Gpu.bConnected) GpuDisconnect(y, Crew[Y.Handler]); }
			return y;
		}
	for (FTug& Y : Tugs) if (!Y.bHangar && Y.State == EState::You && Y.YouStep > 0 && !Y.bYouIn && !Y.bYouCrew && Y.Key.IsEmpty() && FMath::Abs(Y.StandX - StandX) < 1.f) Y.State = EState::Parked;   // (coupled by you: its crew go on from there, TugYou.cpp)
	// the gate's own tug (ready in front of the terminal): its engine on, waiting for off-blocks
	int32 Best = INDEX_NONE;
	for (int32 i = 0; i < Tugs.Num(); ++i) if (!Tugs[i].bHangar && (Tugs[i].State == EState::Parked || Tugs[i].State == EState::Manned) && Tugs[i].Key.IsEmpty() && FMath::Abs(Tugs[i].StandX - StandX) < 1.f && (Best == INDEX_NONE || (Tugs[Best].bHired && !HiredFree(Tugs[Best])))) Best = i;   // (the first free one: the stand's own with its GPU, then a bought one, to-do 318)
	if (Best == INDEX_NONE) return INDEX_NONE;
	FTug& T = Tugs[Best];
	// (to-do 206: the last job's hired people still at it - not yet)
	if (T.bHired) { if (!HiredFree(T)) return INDEX_NONE; HiredEnd(T); }
	const bool bHire = T.State == EState::Parked && !CargoStand(StandX) && HiredJob(Key);   // (the cargo apron: its own handling team's crew)
	// (no crew of its own: one from a quiet gate walks over - nobody free: the aircraft waits for them, it is never pushed by nothing)
	if (!bHire && T.State == EState::Parked && T.Driver == INDEX_NONE && CrewAny() && !LendCrew(Best)) return INDEX_NONE;
	T.Key = Key; T.Path.Reset(); T.Seq = 0; T.SeqT = 0.f; T.EngT = -1.f; T.bDeiced = false; T.bPushReq = false; T.bCleared = false; T.PreSeq = 0; T.PreAt = 0.0; T.bEngines = false; T.Talk.Reset(); T.TalkT = 0.f;
	ADP_LOG(TEXT("Tugs: tug %d at its gate gets ready for %s"), Best, *Key);
	if (bHire)
	{
		// (your own flight: your hired people, as soon as they are free - until then the tug stands in its box: HiredTick)
		T.bHired = true; T.Driver = T.Handler = INDEX_NONE; T.CrewAsk = 0.f;
		return Best;
	}
	// (its crew: the driver walks over and climbs in, then the engine starts - TugCrew.cpp)
	if (T.State == EState::Manned) { T.State = EState::Waiting; Engine(T, true); CrewBoard(Best); ADP_LOG(TEXT("Tugs: the driver, already aboard, starts the engine")); }   // (CrewBoard: only the handler and walkers then)
	else if (T.Driver != INDEX_NONE) { T.State = EState::Boarding; CrewBoard(Best); }
	else { T.State = EState::Waiting; Engine(T, true); }
	return Best;
}

bool ATugs::CrewAny() const
{
	return Tugs.ContainsByPredicate([](const FTug& T) { return T.StandInDriver != INDEX_NONE; });
}

int32 ATugs::LendFrom(int32 Tug, int32* Free) const
{
	// a tug in its box with no job whose own driver and handler stand about; for the other side of taxilane T only when the
	// terminal keeps two such crews, and nobody comes back from there for the terminal's
	const bool bRem = RemoteStand(Tugs[Tug].StandX), bCargo = CargoStand(Tugs[Tug].StandX);
	auto Quiet = [this](int32 Ci) { const FCrew& C = Crew[Ci]; return C.A.IsValid() && !C.bGse && !C.bHired && !C.Q.Num() && !C.Hold && !C.Follow; };
	int32 Best = INDEX_NONE, N = 0; double BestD = 0.0;
	for (int32 i = 0; i < Tugs.Num(); ++i)
	{
		const FTug& D = Tugs[i];
		if (i == Tug || D.State != EState::Parked || !D.Key.IsEmpty() || D.bHired || (D.Seq > 0 && D.Seq < 14) || !D.PinKey.IsEmpty() || D.GpuCi != INDEX_NONE || D.Gpu.bInHand || D.Walkers.Num()) continue;
		if (D.Driver == INDEX_NONE || !Quiet(D.Driver) || (D.Handler != INDEX_NONE && !Quiet(D.Handler))) continue;
		if (CargoStand(D.StandX) != bCargo) continue;   // (never across the runway)
		const bool bFar = RemoteStand(D.StandX);
		if (!bFar) ++N;
		if (bFar && !bRem) continue;
		const double Dist = FVector::Dist2D(Crew[D.Driver].A->GetActorLocation(), Tugs[Tug].Home) + (D.Gpu.bConnected ? 1.0e5 : 0.0) + (bFar != bRem ? 1.0e6 : 0.0);
		if (Best == INDEX_NONE || Dist < BestD) { Best = i; BestD = Dist; }
	}
	if (Free) *Free = N;
	return Best != INDEX_NONE && bRem && !RemoteStand(Tugs[Best].StandX) && N <= 2 ? INDEX_NONE : Best;
}

bool ATugs::LendCrew(int32 Tug)
{
	FTug& T = Tugs[Tug];
	if (T.bHired) return false;
	const int32 Di = LendFrom(Tug);
	if (Di == INDEX_NONE)
	{
		static TMap<int32, float> Said; float& At = Said.FindOrAdd(Tug, -100.f); const float Now = GetWorld()->GetTimeSeconds();
		if (Now - At > 60.f || Now < At) { At = Now; ADP_LOG(TEXT("Tugs: the tug at x %.0f has no crew of its own and no other gate's crew is free - its aircraft waits for one"), T.StandX); }
		return false;
	}
	FTug& D = Tugs[Di];
	ADP_LOG(TEXT("Tugs: the crew of the tug at x %.0f walks over to the tug at x %.0f"), D.StandX, T.StandX);
	const float Sx = T.StandX; const bool bRem = RemoteStand(Sx), bCross = bRem && !RemoteStand(D.StandX);
	auto Send = [&](int32 Ci, float X, float Y)
	{
		FCrew& C = Crew[Ci];
		SafeWay(C, Sx);
		C.Spot = GateT(Sx, X, Y); C.SpotYaw = CrewYaw(Sx); C.Anchor = C.Spot; C.AnchorYaw = C.SpotYaw; C.IdleOff = FVector::ZeroVector;
		FCrewAct A; A.Kind = FCrewAct::Walk; const FVector P = C.Spot; A.To = [P] { return P; }; A.Stop = 40.f; A.Max = 160.f; C.Q.Add(A);
	};
	T.Driver = T.StandInDriver = D.Driver; T.Handler = T.StandInHandler = D.Handler;
	D.Driver = D.Handler = D.StandInDriver = D.StandInHandler = INDEX_NONE;
	T.LentFrom = bCross ? Di : bRem ? D.LentFrom : INDEX_NONE; D.LentFrom = INDEX_NONE; T.BackT = 0.f;
	Crew[T.Driver].Tug = Tug; Send(T.Driver, Sx - 17.5f, 86.f);
	if (T.Handler != INDEX_NONE) { Crew[T.Handler].Tug = Tug; Send(T.Handler, Sx - 18.7f, 87.4f); }
	// (the first crew over there: three wing / tail walkers with them, a push needs them - three stay on this side)
	if (bCross)
	{
		int32 Here = 0, There = 0;
		for (const FCrew& C : Crew) if (C.bWalker && !C.bHired && C.A.IsValid()) (SideOf(C.Spot.Y) == 1 ? There : Here) += 1;
		for (int32 i = 0; i < Crew.Num() && There < 3 && Here > 3; ++i)
			if (const FCrew& C = Crew[i]; C.bWalker && !C.bHired && C.Tug == INDEX_NONE && !C.bGse && !C.Q.Num() && C.A.IsValid() && SideOf(C.Spot.Y) == 0) { Crew[i].bAway = true; Crew[i].OwnSpot = C.Spot; Crew[i].OwnYaw = C.SpotYaw; Send(i, Sx - 20.5f, 86.f + There * 3.f); ++There; --Here; }
	}
	return true;
}

int32 ATugs::WalkNeed(const FTug& T) const
{
	// (your own people, to-do 206: with the walkers there are; the cargo apron has no wing walkers: its driver and handler)
	return T.bHired || CargoStand(T.StandX) ? T.Walkers.Num() : 3;
}

void ATugs::ReturnCrew(int32 Tug)
{
	FTug& T = Tugs[Tug];
	if (T.LentFrom == INDEX_NONE) return;
	auto Quiet = [this](int32 Ci) { const FCrew& C = Crew[Ci]; return C.A.IsValid() && !C.bGse && !C.bHired && !C.Q.Num() && !C.Hold && !C.Follow; };
	// nothing more to do there: the tug back in its box, no job, no aircraft on the stand, the ground power and the chocks off
	bool bDone = T.State == EState::Parked && T.Key.IsEmpty() && !T.bHired && !(T.Seq > 0 && T.Seq < 14) && T.PinKey.IsEmpty() && T.GpuCi == INDEX_NONE && !T.Gpu.bInHand && !T.Gpu.bConnected && !T.Walkers.Num() && T.Ck.Stage == 0 && T.Driver != INDEX_NONE && Quiet(T.Driver) && (T.Handler == INDEX_NONE || Quiet(T.Handler));
	const FAirfieldData::FStand* St = AAirfield::Shared().StandAt(T.StandX);
	if (bDone && St) { const FVector2f N = St->Frame().P(0.f, 12.f); const FVector At = Plan(N.X, N.Y); for (const FVector& Pl : Planes) bDone &= FVector::Dist2D(Pl, At) > 4000.f; }
	if (!bDone) { T.BackT = 0.f; return; }
	if ((T.BackT += 5.f) < 30.f) return;   // (ReturnCrew is asked every 5 s)
	// its own gate (taken by another crew meanwhile: the nearest one at the terminal without one)
	auto Open = [this](int32 i) { const FTug& D = Tugs[i]; return D.Driver == INDEX_NONE && D.StandInDriver == INDEX_NONE && !D.bHired && D.State == EState::Parked; };
	int32 Di = Tugs.IsValidIndex(T.LentFrom) && Open(T.LentFrom) ? T.LentFrom : INDEX_NONE;
	if (Di == INDEX_NONE)
		for (int32 i = 0; i < Tugs.Num(); ++i)
			if (!Tugs[i].bHangar && !RemoteStand(Tugs[i].StandX) && !CargoStand(Tugs[i].StandX) && Open(i) && (Di == INDEX_NONE || FMath::Abs(Tugs[i].StandX - Tugs[T.LentFrom].StandX) < FMath::Abs(Tugs[Di].StandX - Tugs[T.LentFrom].StandX))) Di = i;
	T.BackT = 0.f;
	if (Di == INDEX_NONE) return;
	FTug& D = Tugs[Di];
	ADP_LOG(TEXT("Tugs: the crew at the remote stand at x %.0f is done and walks back to the tug at x %.0f"), T.StandX, D.StandX);
	auto Send = [&](int32 Ci, float ForX, const FVector& Spot, float Yaw)
	{
		FCrew& C = Crew[Ci];
		SafeWay(C, ForX);
		C.Spot = Spot; C.SpotYaw = Yaw; C.Anchor = Spot; C.AnchorYaw = Yaw; C.IdleOff = FVector::ZeroVector;
		FCrewAct A; A.Kind = FCrewAct::Walk; A.To = [Spot] { return Spot; }; A.Stop = 40.f; A.Max = 160.f; C.Q.Add(A);
	};
	D.Driver = D.StandInDriver = T.Driver; D.Handler = D.StandInHandler = T.Handler;
	T.Driver = T.Handler = T.StandInDriver = T.StandInHandler = INDEX_NONE; T.LentFrom = INDEX_NONE;
	Crew[D.Driver].Tug = Di; Send(D.Driver, D.StandX, GateT(D.StandX, D.StandX - 17.5f, 86.f), CrewYaw(D.StandX));
	if (D.Handler != INDEX_NONE) { Crew[D.Handler].Tug = Di; Send(D.Handler, D.StandX, GateT(D.StandX, D.StandX - 18.7f, 87.4f), CrewYaw(D.StandX)); }
	// (the last of them over there: the wing / tail walkers sent with them go back too, the free ones)
	if (!Tugs.ContainsByPredicate([](const FTug& O) { return O.LentFrom != INDEX_NONE; }))
		for (int32 i = 0; i < Crew.Num(); ++i)
			if (FCrew& C = Crew[i]; C.bAway && C.Tug == INDEX_NONE && Quiet(i)) { C.bAway = false; Send(i, float(C.OwnSpot.X / 100.0) + 20.5f, C.OwnSpot, C.OwnYaw); }
}

void ATugs::Arrived(float StandX)
{
	for (int32 i = 0; i < Tugs.Num(); ++i)
		if (!Tugs[i].bHangar && FMath::Abs(Tugs[i].StandX - StandX) < 1.f && !Tugs[i].bHired && Tugs[i].Driver == INDEX_NONE && Tugs[i].State == EState::Parked && Tugs[i].Key.IsEmpty() && CrewAny()) LendCrew(i);   // (its ground power)
	for (int32 i = 0; i < Tugs.Num(); ++i)
		if (const int32 Dr = Tugs[i].bHired ? Tugs[i].StandInDriver : Tugs[i].Driver; !Tugs[i].bHangar && FMath::Abs(Tugs[i].StandX - StandX) < 1.f && !Tugs[i].Gpu.bConnected && Tugs[i].GpuCi == INDEX_NONE && Dr != INDEX_NONE)
		{ const int32 Hd = Tugs[i].bHired ? Tugs[i].StandInHandler : Tugs[i].Handler; const int32 Ci = Hd != INDEX_NONE ? Hd : Dr; GpuConnect(i, Crew[Ci]); ADP_LOG(TEXT("Tugs: aircraft on blocks at x %.0f - ground power going on"), StandX); }
}

void ATugs::Arriving(float StandX, bool bMine)
{
	if (bMine && UStaffPeople::On()) return;   // (your own people are not kept sitting in it through the whole turn-round: they come for the push)
	for (FTug& T : Tugs) if (T.bHired && !T.bHangar && FMath::Abs(T.StandX - StandX) < 1.f && T.State == EState::Parked && T.Key.IsEmpty() && HiredFree(T)) HiredEnd(T);
	for (int32 i = 0; i < Tugs.Num(); ++i)
		if (!Tugs[i].bHangar && Tugs[i].State == EState::Parked && Tugs[i].Driver != INDEX_NONE && FMath::Abs(Tugs[i].StandX - StandX) < 1.f)
		{
			// (not the last crew that is free: it would sit in this tug the whole turn-round while another stand's aircraft waits for one)
			int32 Free = 0; LendFrom(i, &Free); if (Free < 1 && Tugs.ContainsByPredicate([](const FTug& Q) { return Q.StandInDriver == INDEX_NONE; })) continue;
			Tugs[i].State = EState::Boarding; ADP_LOG(TEXT("Tugs: aircraft due at x %.0f in 3 minutes - the tug driver goes to his tug"), StandX); CrewBoard(i, true); }
}

void ATugs::Prepare(const FString& Key, float StandX)
{
	Assign(Key, StandX);
	// (its baggage goes out to it on a tractor train - GSE.h)
	if (AGSE* G = AGSE::Get(GetWorld())) if (G->IsTest()) G->Departure(Key, StandX);   // (in the game the turnaround sends it, under the flight's number: Turnaround.cpp)
}

bool ATugs::Push(const FString& Key, float StandX, const FVector& Fitting, const FVector& Fwd)
{
	const int32 I = Assign(Key, StandX);
	if (I == INDEX_NONE) return false;
	FTug& T = Tugs[I];
	const FVector F = Fwd.GetSafeNormal2D();
	T.HookAt = Fitting + F * HeadAhead * 100.f; T.HookYaw = (-F).Rotation().Yaw;
	if (T.YouBend != 0.f)
	{
		// (taken over from you with the bar at an angle: straightened as it is pushed)
		if (T.State == EState::Hooked) T.YouBend *= FMath::Exp(-FVector::Dist2D(Fitting, T.Fit) / (HeadAhead * 100.f));
		if (FMath::Abs(T.YouBend) < 0.02f) T.YouBend = 0.f;
		T.HookYaw += T.YouBend; T.HookAt = Fitting - FRotator(0.f, T.HookYaw, 0.f).Vector() * HeadAhead * 100.f; T.HookAt.Z = Plan(T.HookAt.X / 100.f, -T.HookAt.Y / 100.f).Z;
	}
	T.Fit = Fitting; T.Fwd = F;
	if (T.State == EState::You) { T.YouBeat = GetWorld()->GetTimeSeconds(); if (!T.bYouPush) return false; if (T.Seq > 0 || T.bYouDone) return PushYou(I); }
	if (T.State == EState::Boarding || T.State == EState::Coupling) return false;
	// (a baggage train still beside the aircraft: the stand cleared of every vehicle but the tug before the push)
	const bool bYou = T.State == EState::You;
	if (T.State == EState::Waiting || bYou) if (const AGSE* G = AGSE::Find(GetWorld())) if (G->BusyAt(StandX))
	{ T.YouWhy = TEXT("a vehicle is still beside the aircraft"); static TSet<FString> Told; if (!Told.Contains(Key)) { Told.Add(Key); ADP_LOG(TEXT("Tugs: %s waits - its baggage train is still at the stand"), *Key); } return false; }
	if (T.State == EState::Waiting || bYou)
	{
		// (owner 2026-11-21: the tug stays in its place until the aircraft asks to be pushed back - doors closed, the jetway
		//  at rest: the cockpit calls the pushback crew on the ramp frequency, they answer, and only then does it come out)
		// (owner 2026-11-21: the tug stays in its place until the aircraft has its PUSHBACK CLEARANCE from ATC: doors closed,
		//  everyone aboard, the jetway at rest - the cockpit asks ground control, ATC answers in 10-60 s, the pilot reads it
		//  back, and only then does the tug come out to the nose; after coupling the interphone check, then brakes released)
		if ((T.Driver != INDEX_NONE || bYou) && T.PreSeq < 2)
		{
			T.YouWhy = TEXT("waiting for ATC's pushback clearance");
			const double Now = GetWorld()->GetTimeSeconds();
			const FString Cs = T.Callsign.IsEmpty() ? Key : T.Callsign;
			const FString Gnd = ADPSite::Name(ADPSite::Id()).Replace(TEXT(" Airport"), TEXT("")) + TEXT(" Ground");
			static const TCHAR* Ph[] = {TEXT("Alpha"), TEXT("Bravo"), TEXT("Charlie"), TEXT("Delta"), TEXT("Echo"), TEXT("Foxtrot"), TEXT("Golf"), TEXT("Hotel"), TEXT("India"), TEXT("Juliett"), TEXT("Kilo"), TEXT("Lima"), TEXT("Mike"), TEXT("November"), TEXT("Oscar"), TEXT("Papa"), TEXT("Quebec"), TEXT("Romeo"), TEXT("Sierra"), TEXT("Tango"), TEXT("Uniform"), TEXT("Victor"), TEXT("Whiskey"), TEXT("X-ray"), TEXT("Yankee"), TEXT("Zulu")};
			if (T.PreSeq == 0)
			{
				T.PreSeq = 1; T.bPushReq = true;
				if (T.bTow) Say(T, 1, FString::Printf(TEXT("%s, %s, %s"), *Gnd, *Cs, *T.TowAsk), true);   // (a hangar tow, to-do 311)
				else Say(T, 1, FString::Printf(TEXT("%s, %s, stand at the terminal, information %s, request pushback."), *Gnd, *Cs, Ph[ADPSite::LocalNow().GetHour() % 26]), true);
				T.PreAt = Now + FMath::FRandRange(10.f, Planes.Num() > 1 ? 45.f : 25.f);
				ADP_LOG(TEXT("Tugs: %s requests pushback from ATC - the tug stays in its box until it is approved"), *Key);
				return false;
			}
			if (T.Talk.Num() || Now < T.PreAt) return false;
			const FString Face = T.Face.IsEmpty() ? FString(TEXT("north")) : T.Face;
			if (T.bTow) { Say(T, 2, FString::Printf(TEXT("%s, %s, %s"), *Cs, *Gnd, *T.TowOk), true); Say(T, 1, FString::Printf(TEXT("%s, %s."), *T.TowOk.LeftChop(1), *Cs), true); }
			else
			{
				Say(T, 2, FString::Printf(TEXT("%s, %s, pushback approved, face %s."), *Cs, *Gnd, *Face), true);
				Say(T, 1, FString::Printf(TEXT("Pushback approved, face %s, %s."), *Face, *Cs), true);
			}
			T.PreSeq = 2; T.bCleared = true;
			ADP_LOG(TEXT("Tugs: %s pushback approved by ATC - now the tug comes out"), *Key);
			return false;
		}
		if (T.Talk.Num()) return false;   // (still talking: the tug stays put)
		if (bYou) return PushYou(I);
		// in to the nose: off the road towards the stand, then straight at the aircraft, slowly
		T.State = EState::ToNose; T.bReverse = false; ADP_LOG(TEXT("Tugs: in to the nose of %s"), *Key);
		const FVector In = T.HookAt + F * 800.f;          // 8 m short of the hook place, in line with the nose
		T.Path = {Plan(StandX + BoxX, BoxY + 6.f), Plan(StandX + BoxX * 0.5f, BoxY + 12.5f), Plan(StandX, BoxY + 18.f), T.Handler != INDEX_NONE ? T.HookAt + F * 150.f : T.HookAt};
		if (HeadStand(StandX)) T.Path = {T.Handler != INDEX_NONE ? T.HookAt + F * 150.f : T.HookAt};   // (its box is on the centre line)
		if (T.bTow && T.TowWay.Num() && T.YouStep == 0)   // (a hangar tow: out along the taxiways to the aircraft, to-do 311)
		{ T.Path = {Plan(StandX + BoxX, BoxY + 6.f)}; T.Path.Append(T.TowWay); T.Path.Add(T.HookAt + F * 1500.f); T.Path.Add(T.HookAt); }
		if (T.bHangar && T.YouStep == 0 && !HangarOut(T, F)) { T.State = EState::Waiting; T.Path.Reset(); return false; }   // (the hangar tug: from its place, no way clear yet - it tries again)
		if (T.YouStep > 0) { T.Path.Reset(); if (FVector::Dist2D(T.At, T.HookAt) > 30.f) { T.Path = {T.HookAt}; T.YouStep = 0; } }   // (you coupled it: already there)
		// (with a handler it stops 1.5 m short: he waves it in the last bit)   // (out of its box in a smooth S-curve onto the centre line, then straight in)
	}
	if (T.State == EState::Hooked)
	{
		static TSet<FString> Said;
		if (!Said.Contains(Key)) { Said.Add(Key); ADP_LOG(TEXT("Tugs: on %s - fitting at plan (%.1f, %.1f), the aircraft's nose towards (%.2f, %.2f), the tug at (%.1f, %.1f) facing %.0f deg"),
			*Key, Fitting.X / 100.f, -Fitting.Y / 100.f, F.X, -F.Y, T.HookAt.X / 100.f, -T.HookAt.Y / 100.f, T.HookYaw); }
		T.At = T.HookAt; T.Yaw = T.HookYaw; Pose(T);
		// (the push starts only with the wing walkers and the tail walker in their places)
		if (T.Driver == INDEX_NONE) return true;
		// (a hangar tow never hangs unsaid: why it waits goes to the log every 30 s, to-do 311)
		auto Hold = [this, &T, &Key](const TCHAR* Why) { const double Tn = GetWorld()->GetTimeSeconds(); if (T.bTow && Tn - T.TowSaid > 30.0) { T.TowSaid = Tn; ADP_LOG(TEXT("Tugs: the hangar tow %s waits - %s"), *Key, Why); } return false; };
		// (the hangar tug: the stand's own ground power and chocks - its own tug keeps them)
		const int32 Si = T.bHangar || !T.Gpu.M.IsValid() ? StandTug(StandX) : I; FTug& Sg = Tugs.IsValidIndex(Si) ? Tugs[Si] : T;
		if (T.bTow && Sg.Gpu.bConnected && !T.bTowGpu && !Sg.Gpu.bYou && !GpuCrewAt(Sg))
		{
			// (the ground power still on it - plugged in after its crew came, or no handler to unplug it: one of them does it now)
			const int32 Gc = T.Handler != INDEX_NONE ? T.Handler : T.Walkers.Num() ? T.Walkers[0] : INDEX_NONE;
			if (Crew.IsValidIndex(Gc)) { T.bTowGpu = true; GpuDisconnect(Tugs.IsValidIndex(Si) ? Si : I, Crew[Gc]); ADP_LOG(TEXT("Tugs: the hangar tow %s - %s takes the ground power off first"), *Key, *Crew[Gc].Name); }
		}
		if (Sg.Gpu.bConnected || Sg.Gpu.bYou || Sg.Gpu.bLying) return Hold(Sg.Gpu.bYou ? TEXT("you are at its ground power") : Sg.Gpu.bLying ? TEXT("the ground power cable still lies out") : TEXT("its ground power is still plugged in"));   // (the ground power off and its cable stowed before any push)
		if (!ChkOut(Sg)) return Hold(TEXT("its cones and chocks are still being taken away"));   // (the cones in and the chocks off - the last ones once it is hooked on: TugChocks.cpp)
		if (T.WalkersReady < WalkNeed(T)) return Hold(TEXT("its wing walkers are not at the wingtips yet"));   // (no crew, no push - owner; your own people, to-do 206: the driver and the handler at least, and every walker there is)
		if (T.Seq == 0) { T.Seq = 1; T.SeqT = 0.f; T.SeqWait = FMath::FRandRange(2.f, 5.f); }
		return T.Seq >= 8;   // (brakes released, "commencing pushback" - the 11 steps, SeqTick)
	}
	return false;
}

void ATugs::Release(const FString& Key)
{
	for (FTug& T : Tugs)
		if (T.Key == Key && T.State == EState::You && T.bYouPush && !T.bYouDone)
		{
			T.bYouDone = true; T.YouUn = 0; T.bPinShown = false; T.PinKey = T.Key; YouMarks(nullptr, {});
			ADP_LOG(TEXT("Tugs: %s pushed back by you - brakes set, now uncoupled by hand"), *Key);
			WalkersDone(int32(&T - Tugs.GetData()));
			if (T.Seq < 9) { T.Seq = 9; T.SeqT = 0.f; T.SeqWait = FMath::FRandRange(2.f, 5.f); }
			if (T.EngT < 0.f && !T.bEngines) T.EngT = 0.f;
			if (T.Handler != INDEX_NONE) CrewUncouple(int32(&T - Tugs.GetData()));
		}
		else if (T.Key == Key && T.State == EState::Hooked)
		{
			T.State = EState::Unhook; T.Clock = 0.f; ADP_LOG(TEXT("Tugs: unhooking from %s"), *Key);
			WalkersDone(int32(&T - Tugs.GetData()));
			if (T.Driver != INDEX_NONE && T.Seq < 9) { T.Seq = 9; T.SeqT = 0.f; T.SeqWait = FMath::FRandRange(2.f, 5.f); }   // (push complete: "set brakes")
			if (T.Driver != INDEX_NONE) CrewUncouple(int32(&T - Tugs.GetData()));
			else Sound(TEXT("TugHitchClunk"), T.At - FRotator(0.f, T.Yaw, 0.f).Vector() * HeadAhead * 100.f);
		}
}

bool ATugs::IsClear(const FString& Key) const
{
	for (const FTug& T : Tugs)
	{
		if (T.Key == Key && (T.State == EState::ToNose || T.State == EState::Hooked || T.State == EState::Unhook || T.State == EState::Backing
			|| T.State == EState::Boarding || T.State == EState::Coupling || T.State == EState::ShowPin)) return false;
		if (T.Key == Key && T.State == EState::You && T.bYouPush) { T.YouBeat = GetWorld()->GetTimeSeconds(); return false; }   // (your push: clear when you are, TugYou.cpp)
		// (the pilot never taxis before the bypass pin has been shown and everyone is clear - owner 2026-11-21)
		if (T.PinKey == Key && !T.bPinShown) return false;
		if (T.PinKey == Key && (T.Driver != INDEX_NONE || T.bHired) && T.Seq < 14) return false;   // (engines stable, ready to taxi, ATC's taxi clearance)
	}
	return true;
}

int32 ATugs::NumAtWork() const
{
	int32 N = 0; for (const FTug& T : Tugs) N += T.State != EState::Parked; return N;
}

bool ATugs::WatchSpot(FVector& At, float& Yaw, bool* bHooked, bool* bAtGate) const
{
	// (the ground power being worked: the GPU, the plug and its cable - from the side)
	for (const FTug& T : Tugs) if ((T.Gpu.bInHand || T.Gpu.PanelWant > 0.f) && T.Gpu.M.IsValid()) { const FVector G = T.Gpu.M->GetActorLocation(), R = Receptacle(T); At = (G + R) * 0.5f; Yaw = (R - G).Rotation().Yaw + 30.f; if (bHooked) *bHooked = false; if (bAtGate) *bAtGate = false; return true; }
	if (bAtGate) { *bAtGate = false; for (const FTug& T : Tugs) if ((T.State == EState::Coupling || (T.State == EState::Hooked && FVector::Dist2D(T.At, T.Fit) < 900.f && T.WalkersReady < WalkNeed(T))) && T.M.IsValid()) *bAtGate = true; }
	for (const FTug& T : Tugs) if ((T.State == EState::Hooked || T.State == EState::Unhook || T.State == EState::Coupling) && T.M.IsValid()) { At = T.At; Yaw = T.Yaw; if (bHooked) *bHooked = true; return true; }
	// (waiting beside a stand: straight down at the parked aircraft's nose - which way it faces)
	for (const FTug& T : Tugs) if (T.State == EState::Waiting && T.M.IsValid()) { At = T.At; Yaw = T.Yaw; if (bHooked) *bHooked = false; return true; }   // (waiting in its box: seen from the side, the box's lines round it)
	if (bHooked) *bHooked = false;
	for (const FTug& T : Tugs) if (T.State != EState::Parked && T.M.IsValid()) { At = T.At; Yaw = T.Yaw; return true; }
	if (Tugs.Num()) { At = Tugs[0].At; Yaw = Tugs[0].Yaw; return true; }
	return false;
}

void ATugs::Drive(FTug& T, float Dt)
{
	// driven as a real tug (owner 2026-11-21: smooth and believable): eased speed, steering towards a point 5 m ahead on its
	// way (corners become curves), never tighter than its turning circle (5.5 m, four-wheel steering), slower in bends, a slow
	// creep into an exact final place (the hook, the park)
	if (!T.Path.Num()) { T.Speed = 0.f; return; }
	const bool bLast = T.Path.Num() == 1;
	const FVector End = T.Path.Last();
	// the point to steer for: 5 m ahead along the way (or the next waypoint when nearer)
	FVector Aim = T.Path[0];
	{
		float Look = 350.f; FVector From = T.At;          // (3.5 m: it follows tight bends without cutting or swinging wide)
		for (int32 i = 0; i < T.Path.Num(); ++i)
		{
			const float Seg = FVector::Dist2D(From, T.Path[i]);
			if (Seg >= Look) { Aim = From + (T.Path[i] - From).GetSafeNormal2D() * Look; break; }
			Look -= Seg; From = T.Path[i]; Aim = T.Path[i];
		}
	}
	// waypoints passed: within 1.5 m (the last one is reached exactly)
	while (T.Path.Num() > 1 && FVector::Dist2D(T.At, T.Path[0]) < 150.f) T.Path.RemoveAt(0);
	FVector To = Aim - T.At; To.Z = 0.f;
	const float DistEnd = FVector::Dist2D(T.At, End);
	if (bLast && DistEnd < 25.f) { T.At = End; T.Path.Reset(); T.Speed = 0.f; Pose(T); return; }
	// give way: a tug ahead of it in the list within 8 m in the way - wait (eased to a stop)
	const FVector Move = (T.bReverse ? -1.f : 1.f) * FRotator(0.f, T.Yaw, 0.f).Vector();
	bool bBlocked = false;
	for (const FTug& O : Tugs)
		if (&O < &T && O.M.IsValid() && O.State != EState::Parked)
		{
			const FVector R = O.At - T.At;
			if (R.Size2D() < 800.f && FVector::DotProduct(R.GetSafeNormal2D(), Move) > 0.3f) bBlocked = true;
		}
	// aircraft have right of way (owner 2026-11-21): an aircraft moving within 45 m in front of the way it drives - it holds
	// (its own aircraft, which it is driving in to, excepted - that one stands still for it)
	for (const FVector& Pl : Planes)
	{
		const FVector R = Pl - T.At;
		if (R.Size2D() < 4500.f && FVector::DotProduct(R.GetSafeNormal2D(), Move) > 0.f && T.State != EState::ToNose && T.State != EState::Coupling) bBlocked = true;   // (not its own aircraft: driving in to it, creeping on)
	}
	// steering: the heading wanted (the back leads when reversing), limited by the turning circle at this speed
	const float WantYaw = T.bReverse ? (-To).Rotation().Yaw : To.Rotation().Yaw;
	const float dYaw = FMath::FindDeltaAngleDegrees(T.Yaw, WantYaw);
	const float MaxRate = FMath::Min(40.f, FMath::RadiansToDegrees(FMath::Max(T.Speed, 60.f) / 550.f));   // (deg/s: v / R)
	T.Yaw += FMath::Clamp(dYaw, -MaxRate * Dt, MaxRate * Dt);
	// speed: the road speed, less in bends and while turning hard, slow when reversing, a creep towards the end; eased
	float Want = T.bReverse ? SlowSpeed : DriveSpeed;
	Want *= FMath::Clamp(1.f - FMath::Abs(dYaw) / 150.f, 0.7f, 1.f);      // (only a little slower in bends)
	// (a tight bend ahead: no faster than it can really follow - it turns at most 40 deg/s, so a bend of radius R needs
	//  v <= 0.7 R m/s; the path's bend measured over the next waypoints)
	if (T.Path.Num() >= 2)
	{
		float Turnsum = 0.f; FVector Prev = T.At; FVector PrevDir = FRotator(0.f, T.Yaw, 0.f).Vector() * (T.bReverse ? -1.f : 1.f); float Len = 0.f;
		for (int32 i = 0; i < FMath::Min(T.Path.Num(), 6); ++i)
		{
			const FVector D2 = (T.Path[i] - Prev).GetSafeNormal2D(); const float Seg = FVector::Dist2D(T.Path[i], Prev);
			if (Seg < 1.f) continue;
			Turnsum += FMath::Abs(FMath::FindDeltaAngleDegrees(PrevDir.Rotation().Yaw, D2.Rotation().Yaw)); Len += Seg; PrevDir = D2; Prev = T.Path[i];
			if (Len > 1500.f) break;
		}
		if (Turnsum > 30.f && Len > 1.f) { const float Rcm = Len / FMath::DegreesToRadians(Turnsum); Want = FMath::Min(Want, FMath::Max(120.f, 0.7f * Rcm)); }
	}
	Want = FMath::Min(Want, FMath::Max(40.f, DistEnd * 0.6f));          // (brakes to a creep over the last metres)
	if (T.State == EState::ToNose) Want = FMath::Min(Want, 250.f);       // (by the aircraft: 9 km/h at most)
	if (T.State == EState::Coupling) Want = FMath::Min(Want, 40.f);      // (waved in: a walking-pace crawl, 1.5 km/h)
	if (bBlocked) Want = 0.f;
	const float Acc = Want > T.Speed ? 100.f : 160.f;                   // (cm/s^2: 1.0 m/s^2 up, 1.6 down)
	T.Speed = FMath::FInterpConstantTo(T.Speed, Want, Dt, Acc);
	// it moves the way it faces (the back first when reversing) - a vehicle, not a slide
	// (the last 3 m to an exact place with the heading still well off: straight onto it at a creep, never circling round it)
	if (bLast && DistEnd < 300.f && FMath::Abs(dYaw) > 45.f) T.At += (End - T.At).GetSafeNormal2D() * FMath::Min(T.Speed, 60.f) * Dt;
	else T.At += (T.bReverse ? -1.f : 1.f) * FRotator(0.f, T.Yaw, 0.f).Vector() * T.Speed * Dt;
	T.At.Z = FMath::FInterpTo(T.At.Z, End.Z, Dt, 4.f);
	Pose(T);
}

// the test's own eyes (owner 2026-11-21: a vehicle on a red marking or under an aircraft is a fault - recreate the scene):
// every half second each working tug's footprint (a 2.6 m circle round its middle, plan metres) against the stands'
// red boundary lines, the jetways' red zones and the aircraft standing on the stands (fuselage and wings); logged as CHECK
static void CheckTug(UWorld* W, int32 Index, const FVector& AtW, float Yaw, const FString& Key)
{
	const FAirfieldData& F = AAirfield::Shared();
	const FVector2f P(AtW.X / 100.f, -AtW.Y / 100.f);
	// the tug's own outline (2.5 x 5.1 m, turned the way it faces): its corners and side middles
	const FVector Fw3 = FRotator(0.f, Yaw, 0.f).Vector(); const FVector2f Fw(Fw3.X, -Fw3.Y), Sd(-Fw.Y, Fw.X);
	TArray<FVector2f> Pts;
	for (const float a : {-2.55f, 0.f, 2.55f}) for (const float b : {-1.26f, 1.26f}) Pts.Add(P + Fw * a + Sd * b);
	const float R = 0.1f;
	auto Seg1 = [](FVector2f Q, FVector2f A, FVector2f B) { const FVector2f AB = B - A; const float L2 = AB.SizeSquared(); const float U = L2 > 0 ? FMath::Clamp(FVector2f::DotProduct(Q - A, AB) / L2, 0.f, 1.f) : 0.f; return FVector2f::Distance(Q, A + AB * U); };
	auto Seg = [&](FVector2f A, FVector2f B) { float D = 1e9f; for (const FVector2f& Q : Pts) D = FMath::Min(D, Seg1(Q, A, B)); return D; };
	auto Inside = [&](float X0, float X1, float Y0, float Y1) { for (const FVector2f& Q : Pts) if (Q.X > X0 && Q.X < X1 && Q.Y > Y0 && Q.Y < Y1) return true; return false; };
	FString Hit;
	// the boundary lines as Airfield.cpp draws them: each stand's wingtips + 4.5 m, one shared line halfway between close neighbours
	TArray<FAirfieldData::FStandLine> Ls; TArray<TPair<const FAirfieldData::FStand*, FBox2f>> Zs; F.StandLines(true, Ls, Zs);
	for (const auto& L : Ls) if (Seg(L.A, L.B) < R) Hit += FString::Printf(TEXT(" boundary line of %s;"), *L.Stand->Id);
	for (const auto& Z : Zs) if (Inside(Z.Value.Min.X, Z.Value.Max.X, Z.Value.Min.Y, Z.Value.Max.Y)) Hit += FString::Printf(TEXT(" jetway red zone of %s;"), *Z.Key->Id);
	// aircraft on stands (parked, or waiting for their pushback): nose tyre 5.1 m behind the stop line, the body back from it
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		bool bPlane = false; for (const FName& Tg : It->Tags) { const FString S = Tg.ToString(); if (S.StartsWith(TEXT("SIM/")) || S.StartsWith(TEXT("MOVE/"))) bPlane = true; }
		if (!bPlane) continue;
		const FVector L = It->GetActorLocation(); const FVector2f N(L.X / 100.f, -L.Y / 100.f);
		// its nose direction: its stand's (only aircraft standing on one: by the terminal row's nose wheel line, or by a remote stand's)
		FVector2f Fa(0.f, -1.f);
		if (!F.Stands.ContainsByPredicate([&](const FAirfieldData::FStand& Q) { return !Q.bRemote && !Q.bCargo && Q.Frame().Local(N).GetAbs().GetMax() < 6.f; }))
		{
			const FAirfieldData::FStand* Os = F.Stands.FindByPredicate([&](const FAirfieldData::FStand& Q) { return (Q.bRemote || Q.bCargo) && Q.Frame().Local(N).GetAbs().GetMax() < 6.f; });
			if (!Os) continue;
			Fa = Os->Frame().Face();
		}
		const FVector2f Tip = N + Fa * 5.2f, Tail = N - Fa * 32.5f;
		for (const FVector2f& Q : Pts) { const float Along = FVector2f::DotProduct(Q - Tail, (Tip - Tail).GetSafeNormal()); if (Along > 0.f && Along < 37.7f && Seg1(Q, Tip, Tail) < 2.f) { Hit += TEXT(" under an aircraft's fuselage;"); break; } }
		const FVector2f W0 = N - Fa * 12.f, W1 = N - Fa * 19.f;  // the wings: 12-19 m behind the nose tyre, 18 m each side
		if (Inside(N.X - 18.f, N.X + 18.f, FMath::Min(W0.Y, W1.Y), FMath::Max(W0.Y, W1.Y))) Hit += TEXT(" under an aircraft's wing;");
	}
	// the terminal itself: nothing within 1 m of its apron-side wall (the terminal apron's edge, plan y 82)
	for (const FVector2f& Q : Pts) if (Q.Y < 83.f) { Hit += TEXT(" at the terminal wall;"); break; }
	for (const FVector2f& Q : Pts) if (Q.Y > 1230.f && Q.Y < 1302.f && Q.X > 199.f && Q.X < 666.f) { Hit += TEXT(" at the cargo building;"); break; }
	if (!Hit.IsEmpty()) ADP_WARN(TEXT("Tugs: CHECK tug %d (%s) at plan (%.1f, %.1f):%s"), Index, *Key, P.X, P.Y, *Hit);
}

void ATugs::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bReady) Setup();
	Flash += Dt;
	static float Say = 0.f;
	if ((Say += Dt) > 5.f)
	{
		Say = 0.f;
		for (int32 i = 0; i < Tugs.Num(); ++i) ReturnCrew(i);
		static const TCHAR* SN[] = {TEXT("parked"), TEXT("to the stand"), TEXT("waiting"), TEXT("to the nose"), TEXT("hooked"), TEXT("unhooking"), TEXT("backing"), TEXT("home"), TEXT("the driver getting in"), TEXT("coupling"), TEXT("showing the pin"), TEXT("manned, engine off"), TEXT("with you")};
		for (int32 i = 0; i < Tugs.Num(); ++i) if (Tugs[i].State != EState::Parked)
			ADP_LOG(TEXT("Tugs: tug %d %s at plan (%.1f, %.1f) facing %.0f deg%s"), i, SN[uint8(Tugs[i].State)], Tugs[i].At.X / 100.f, -Tugs[i].At.Y / 100.f, Tugs[i].Yaw, Tugs[i].bReverse ? TEXT(" (reversing)") : TEXT(""));
	}
	// (the check: tests only, -UITugShot / -TugCheck - it walks the world's actors, never in a normal game)
	static const bool bChecks = FParse::Param(FCommandLine::Get(), TEXT("UITugShot")) || FParse::Param(FCommandLine::Get(), TEXT("TugCheck"));
	// the moving aircraft (their 3D models tagged MOVE/ by SimTraffic), once a second - vehicles give way to them
	if ((PlaneClock += Dt) >= 1.f)
	{
		ADP_PERF_SCOPE("the tugs: where the moving aircraft are (once a second)", 1.f);
		PlaneClock = 0.f; Planes.Reset();
		for (TActorIterator<ABabylonModel> It(GetWorld()); It; ++It)
			for (const FName& Tg : It->Tags) if (Tg.ToString().StartsWith(TEXT("MOVE/"))) { Planes.Add(It->GetActorLocation()); break; }
	}
	static float CheckClock = 0.f; const bool bCheck = bChecks && (CheckClock += Dt) > 0.5f; if (bCheck) CheckClock = 0.f;
	for (FTug& T : Tugs)
	{
		if (!T.M.IsValid()) continue;
		// (hooked on / unhooking it is in front of its own aircraft's nose on purpose - checked while it drives)
		if (bCheck && T.State != EState::Parked && T.State != EState::Hooked && T.State != EState::Unhook && T.State != EState::Coupling && T.State != EState::Boarding) CheckTug(GetWorld(), int32(&T - Tugs.GetData()), T.At, T.Yaw, T.Key);
		switch (T.State)
		{
		case EState::ToStand:
			Drive(T, Dt);
			if (!T.Path.Num())
			{
				if (T.bReverse)
				{
					// out of the park: forwards along the road to the staging spot beside the stand
					T.bReverse = false;
					T.Path = {Plan(ParkExitX, LaneY - 6.f), Plan(ParkExitX + 8.f, LaneY), Plan(T.StandX + StageAside, LaneY)};
				}
				else { T.State = EState::Waiting; ADP_LOG(TEXT("Tugs: waiting beside the stand for %s, outside the red line"), *T.Key); }
			}
			break;
		case EState::ToNose:
			Drive(T, Dt);
			if (!T.Path.Num())
			{
				if (T.Driver != INDEX_NONE) { T.State = EState::Coupling; ADP_LOG(TEXT("Tugs: at the nose of %s - coupling by hand"), *T.Key); CrewCouple(int32(&T - Tugs.GetData())); }
				else { ADP_LOG(TEXT("Tugs: hooked on %s"), *T.Key); T.State = EState::Hooked; T.At = T.HookAt; T.Yaw = T.HookYaw; Pose(T); Sound(TEXT("TugHitchClunk"), T.At - FRotator(0.f, T.Yaw, 0.f).Vector() * HeadAhead * 100.f); }
			}
			break;
		case EState::Coupling:
			// (waved in by the handler: the last 1.5 m at a crawl)
			if (T.Path.Num()) { Drive(T, Dt); if (!T.Path.Num()) { T.At = T.HookAt; T.Yaw = T.HookYaw; Pose(T); } }
			break;
		case EState::Unhook:
			// the towbar's pin out and the bar lifted off (4 s), then backs away from the nose
			if (T.Driver == INDEX_NONE && (T.Clock += Dt) > 4.f) StartBacking(T);   // (with a crew: when they have unpinned it)
			break;
		case EState::Backing:
			Drive(T, Dt);
			if (!T.Path.Num())
			{
				if (T.Driver != INDEX_NONE && T.Handler == INDEX_NONE && !T.bPinShown) { T.State = EState::ShowPin; ADP_LOG(TEXT("Tugs: clear of %s - the driver gets out to show the pin"), *T.Key); }
				else StartHome(T);
			}
			break;
		case EState::Home:
			// (the way re-checked every 2 s: something new in it that shouldn't be there - and it goes round)
			if (T.bHangar && T.HtRetry > 0.f) { if (GetWorld()->GetTimeSeconds() >= T.HtRetry) HangarHome(T, TArray<FVector>()); break; }   // (its way home not clear yet)
			if (!T.bHangar && T.Path.Num() > 9 && !T.Pending.Num() && (T.RouteCheck += Dt) > 2.f) { T.RouteCheck = 0.f; if (PathBlocked(T, T.Path, T.At)) RouteHome(T); }
			Drive(T, Dt);
			if (!T.Path.Num() && T.Pending.Num()) { T.Path = T.Pending; T.Pending.Reset(); T.bReverse = T.bPendingReverse; }
			if (!T.Path.Num())
			{
				if (!T.bReverse) { T.bReverse = true; T.Path = {T.Home}; break; }       // (the loop done: reversed into its box)
				T.bReverse = false; T.State = EState::Parked; T.At = T.Home; T.Yaw = T.HomeYaw; Pose(T); Engine(T, false); ADP_LOG(TEXT("Tugs: parked in its box, engine off"));
				if (T.bHangar) T.StandX = -100000.f;   // (the hangar tug home in its place: at no stand)
				CrewParked(int32(&T - Tugs.GetData()));
			}
			break;
		default: break;
		}
		Lamps(T);
		Wheels(T, Dt);
		Door(T, Dt);
		// dirt by distance and time, the beads drying (VehicleDirt.h): the tug, and its gate's GPU (it moves when towed)
		T.Dirt.Tick(Dt, T.MeasSpeed);
		if (T.Gpu.M.IsValid())
		{
			const FVector G = T.Gpu.M->GetActorLocation();
			const float Gs = T.Gpu.DirtAt.IsZero() ? 0.f : FVector::Dist2D(G, T.Gpu.DirtAt) / FMath::Max(Dt, 1e-3f) / 100.f;
			T.Gpu.DirtAt = G; T.Gpu.Dirt.Tick(Dt, FMath::Min(Gs, 10.f));
		}
		SeqTick(T, Dt);
		if (T.bYouPush) YouAfter(int32(&T - Tugs.GetData()), Dt);
		GpuYouTick(T, Dt);
		GpuTick(T, Dt);
		if (!T.bHangar) ChkTick(int32(&T - Tugs.GetData()), Dt);   // (the hangar tug: the stand's own chocks, JobTug)
		// (short of walkers for a departure: look for free ones every 3 s)
		if (T.bHired) HiredTick(int32(&T - Tugs.GetData()), Dt);
		if (T.Driver != INDEX_NONE && !T.Key.IsEmpty() && T.Walkers.Num() < 3 && !T.bYouDone && !(T.bHired && T.Seq > 0) && T.State != EState::Unhook && T.State != EState::Backing && T.State != EState::Home && (T.WalkerLook += Dt) > 3.f) { T.WalkerLook = 0.f; CrewWalkers(int32(&T - Tugs.GetData())); }
	}
	for (FCrew& C : Crew) StepCrew(C, Dt);
	// (to-do 206: your people done with their job - after their last act, never inside it)
	for (int32 i = 0; i < Crew.Num(); ++i) if (Crew[i].bLeave && !Crew[i].Q.Num()) Discharge(i);
	// (and one of them whose body is gone: the job never stays stuck)
	for (int32 i = 0; i < Crew.Num(); ++i) if (Crew[i].bHired && !Crew[i].bSlotFree) { if (Crew[i].A.IsValid()) Crew[i].LastAt = Crew[i].A->GetActorLocation(); else LostTick(i, Dt); }
}

void ATugs::StartBacking(FTug& T)
{
	if (T.State != EState::Unhook) return;
	T.State = EState::Backing; T.bReverse = true;
	const FVector Back = -FRotator(0.f, T.Yaw, 0.f).Vector();
	T.Path = {T.At + Back * 1200.f};
	if (FVector::Dist2D(T.At, T.Fit) > HeadAhead * 100.f + 150.f) T.Path.Reset();   // (you had driven it off the nose already: straight home)
}

namespace
{
	// the apron's obstacles for a tug at a plan point (m): stand boundary lines, jetway red zones, aircraft on the ground
	// (fuselage, wings, tailplane), other tugs, the terminal wall - against a circle of the tug's half diagonal (2.9 m)
	struct FApronObs
	{
		struct FSeg { FVector2f A, B; float R; };
		TArray<FSeg> Segs; TArray<FBox2f> Boxes;
		void Build(UWorld* W, const TArray<FVector2f>& OtherTugs)
		{
			const FAirfieldData& F = AAirfield::Shared();
			TArray<FAirfieldData::FStandLine> Ls; TArray<TPair<const FAirfieldData::FStand*, FBox2f>> Zs; F.StandLines(true, Ls, Zs);
			for (const auto& L : Ls) Segs.Add({L.A, L.B, 0.3f});
			for (const auto& Z : Zs) Boxes.Add(Z.Value);
			for (TActorIterator<AActor> It(W); It; ++It)
			{
				bool bPlane = false; for (const FName& Tg : It->Tags) { const FString S = Tg.ToString(); if (S.StartsWith(TEXT("SIM/")) || S.StartsWith(TEXT("MOVE/"))) bPlane = true; }
				if (!bPlane) continue;
				const FVector L = It->GetActorLocation(); const FVector2f N(L.X / 100.f, -L.Y / 100.f);
				// (its nose way: the model's nose is the actor's -Y; plan y is the world's -y)
				const FVector Nw = -It->GetActorRightVector(); FVector2f Fa(Nw.X, -Nw.Y); Fa = Fa.GetSafeNormal(); if (Fa.IsNearlyZero()) Fa = FVector2f(0.f, -1.f);
				const FVector2f Sd(-Fa.Y, Fa.X);
				Segs.Add({N + Fa * 5.2f, N - Fa * 32.5f, 2.2f});                          // the fuselage
				Segs.Add({N - Fa * 14.f + Sd * 18.f, N - Fa * 14.f - Sd * 18.f, 3.5f});    // the wings (a band 7 m deep)
				Segs.Add({N - Fa * 31.f + Sd * 6.5f, N - Fa * 31.f - Sd * 6.5f, 1.5f});    // the tailplane
			}
			for (const FVector2f& O : OtherTugs) Segs.Add({O, O, 4.f});
		}
		bool Blocked(const FVector2f& P, float R = 2.9f) const
		{
			if (P.Y < 82.6f + R) return true;
			for (const FSeg& S : Segs)
			{
				const FVector2f AB = S.B - S.A; const float L2 = AB.SizeSquared();
				const float U = L2 > 0 ? FMath::Clamp(FVector2f::DotProduct(P - S.A, AB) / L2, 0.f, 1.f) : 0.f;
				if (FVector2f::Distance(P, S.A + AB * U) < S.R + R) return true;
			}
			for (const FBox2f& B : Boxes) if (P.X > B.Min.X - R && P.X < B.Max.X + R && P.Y > B.Min.Y - R && P.Y < B.Max.Y + R) return true;
			return false;
		}
		bool LineClear(const FVector2f& A, const FVector2f& B) const
		{
			const int32 N = FMath::Max(1, FMath::CeilToInt(FVector2f::Distance(A, B) / 0.75f));
			for (int32 i = 0; i <= N; ++i) if (Blocked(FMath::Lerp(A, B, float(i) / N))) return false;
			return true;
		}
	};
	FVector2f ToPlan(const FVector& W) { return FVector2f(W.X / 100.f, -W.Y / 100.f); }
	struct FHeapLess { bool operator()(const TPair<float, int32>& A, const TPair<float, int32>& B) const { return A.Key < B.Key; } };
}

bool ATugs::PathBlocked(const FTug& T, const TArray<FVector>& Path, const FVector& From) const
{
	TArray<FVector2f> Others; for (const FTug& O : Tugs) if (&O != &T) Others.Add(ToPlan(O.At));
	FApronObs Ob; Ob.Build(GetWorld(), Others);
	FVector2f A = ToPlan(From);
	// (the last legs - the loop in front of the terminal and the box - are its own: only the way there is checked)
	for (int32 i = 0; i < FMath::Min(Path.Num() - 8, 2); ++i) { const FVector2f B = ToPlan(Path[i]); if (!Ob.LineClear(A, B)) return true; A = B; }
	return false;
}

bool ATugs::RouteHome(FTug& T)
{
	TArray<FVector2f> Others; for (const FTug& O : Tugs) if (&O != &T) Others.Add(ToPlan(O.At));
	FApronObs Ob; Ob.Build(GetWorld(), Others);
	float Bs = 0.f, Run = -1.f, By = BoxY, Rl = 6.f, Ly = 0.f; BoxT(T.StandX, Bs, Run, By, Rl, Ly); const bool bRem = RemoteStand(T.StandX), bCg = CargoStand(T.StandX);
	const float Sx = T.StandX;
	const FVector2f Start = ToPlan(T.At), Goal = ToPlan(StandT(Sx, Sx + Run, Ly + 8.f));
	// boxed in? back off (or pull forward) first, 6 m, whichever way is free (owner 2026-11-21)
	const FVector Fw3 = FRotator(0.f, T.Yaw, 0.f).Vector(); const FVector2f Fw(Fw3.X, -Fw3.Y);
	T.Pending.Reset();
	bool bFrontBlocked = false, bBackBlocked = false;
	for (float d = 3.f; d <= 6.f; d += 1.5f) { bFrontBlocked |= Ob.Blocked(Start + Fw * d, 1.4f); bBackBlocked |= Ob.Blocked(Start - Fw * d, 1.4f); }
	FVector2f Escape = Start; bool bEscRev = false;
	if (bFrontBlocked && !bBackBlocked) { Escape = Start - Fw * 6.f; bEscRev = true; ADP_LOG(TEXT("Tugs: something right in front - backing off first")); }
	else if (bBackBlocked && !bFrontBlocked && T.bReverse) { Escape = Start + Fw * 6.f; ADP_LOG(TEXT("Tugs: something right behind - pulling forward first")); }
	// the grid search (1.5 m cells) over the apron round the way
	const float C = 1.5f;
	const FVector2f Lo(FMath::Min(Escape.X, Goal.X) - 60.f, FMath::Max(84.f, FMath::Min(Escape.Y, Goal.Y) - 30.f));
	const FVector2f Hi(FMath::Max(Escape.X, Goal.X) + 60.f, FMath::Max(Escape.Y, Goal.Y) + 60.f);
	const int32 Nx = FMath::CeilToInt((Hi.X - Lo.X) / C), Ny = FMath::CeilToInt((Hi.Y - Lo.Y) / C);
	if (Nx <= 0 || Ny <= 0 || Nx * Ny > 120000) return false;
	auto Cell = [&](const FVector2f& P) { return FIntPoint(FMath::Clamp(FMath::RoundToInt((P.X - Lo.X) / C), 0, Nx - 1), FMath::Clamp(FMath::RoundToInt((P.Y - Lo.Y) / C), 0, Ny - 1)); };
	auto At = [&](int32 x, int32 y) { return FVector2f(Lo.X + x * C, Lo.Y + y * C); };
	TArray<uint8> Bl; Bl.SetNumZeroed(Nx * Ny);
	for (int32 y = 0; y < Ny; ++y) for (int32 x = 0; x < Nx; ++x) Bl[y * Nx + x] = Ob.Blocked(At(x, y)) || (bRem && At(x, y).Y > 245.f && !AAirfield::Shared().OnRemote(At(x, y))) || (bCg && At(x, y).Y > 1229.f) ? 1 : 0;
	const FIntPoint S = Cell(Escape), G = Cell(Goal);
	Bl[S.Y * Nx + S.X] = 0; Bl[G.Y * Nx + G.X] = 0;
	TArray<float> Cost; Cost.Init(1e9f, Nx * Ny); TArray<int32> From; From.Init(-1, Nx * Ny);
	TArray<TPair<float, int32>> Open; Cost[S.Y * Nx + S.X] = 0.f; Open.HeapPush(TPair<float, int32>(0.f, S.Y * Nx + S.X), FHeapLess());
	bool bFound = false;
	while (Open.Num())
	{
		TPair<float, int32> Top; Open.HeapPop(Top, FHeapLess());
		const int32 I = Top.Value; if (I == G.Y * Nx + G.X) { bFound = true; break; }
		const int32 x = I % Nx, y = I / Nx;
		for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx)
		{
			if (!dx && !dy) continue; const int32 X2 = x + dx, Y2 = y + dy; if (X2 < 0 || Y2 < 0 || X2 >= Nx || Y2 >= Ny) continue;
			const int32 J = Y2 * Nx + X2; if (Bl[J]) continue;
			const float Nc = Cost[I] + ((dx && dy) ? 1.414f : 1.f);
			if (Nc < Cost[J]) { Cost[J] = Nc; From[J] = I; Open.HeapPush(TPair<float, int32>(Nc + FVector2f::Distance(At(X2, Y2), Goal) / C, J), FHeapLess()); }
		}
	}
	if (!bFound) { ADP_WARN(TEXT("Tugs: no way home round the obstacle - waiting")); return false; }
	TArray<FVector2f> Cells; for (int32 I = G.Y * Nx + G.X; I >= 0; I = From[I]) Cells.Insert(At(I % Nx, I / Nx), 0);
	// straightened: from each point on to the farthest one it can see clear
	TArray<FVector2f> Way = {Escape}; int32 k = 0;
	while (k < Cells.Num() - 1) { int32 Far = k + 1; for (int32 j = Cells.Num() - 1; j > k + 1; --j) if (Ob.LineClear(Cells[k], Cells[j])) { Far = j; break; } Way.Add(Cells[Far]); k = Far; }
	TArray<FVector> Route; for (int32 i = 1; i < Way.Num(); ++i) Route.Add(Plan(Way[i].X, Way[i].Y));
	// then the loop in front of the terminal and back into the box, as always
	const float Rt = HeadStand(Sx) ? -Rl : Rl; Route.Add(StandT(Sx, Sx + Run, Ly));
	for (int32 j = 1; j <= 8; ++j) { const float A = PI * j / 8.f; Route.Add(StandT(Sx, Sx + Run - Rt + Rt * FMath::Cos(A), Ly - Rl * FMath::Sin(A))); }
	if (Escape != Start) { T.Pending = Route; T.bPendingReverse = false; T.Path = {Plan(Escape.X, Escape.Y)}; T.bReverse = bEscRev; }
	else { T.Path = Route; T.bReverse = false; }
	ADP_LOG(TEXT("Tugs: round the obstacle - a way home of %d legs"), Route.Num());
	return true;
}

void ATugs::StartHome(FTug& T)
{
	// clear of the aircraft: home up the stand it just cleared (just left of the centre line), a 6 m loop in front of the
	// terminal ending in line with its box facing the gate, then reversed straight into the box - as real drivers park;
	// only if something is in the way that shouldn't be there (owner 2026-11-21): round it (RouteHome)
	const TArray<FVector> TowHome = T.bTow ? T.TowBack : TArray<FVector>(); T.bTow = false; T.bTowGpu = false; T.TowWay.Reset(); T.TowBack.Reset(); Tows.Remove(T.Key);
	ADP_LOG(TEXT("Tugs: clear of %s, home to the park"), *T.Key); T.State = EState::Home; T.Key.Reset(); T.bReverse = false; T.RouteCheck = 0.f;
	if (T.bHangar) { T.Path.Reset(); T.HtRetry = 0.f; HangarHome(T, TowHome); return; }   // (the hangar tug: home to its place by the bus chargers)
	float Bs = 0.f, Run = -1.f, By = BoxY, Rl = 6.f, Ly = 0.f; BoxT(T.StandX, Bs, Run, By, Rl, Ly);
	const float Sx = T.StandX, Rt = HeadStand(Sx) ? -Rl : Rl;
	T.Path = {StandT(Sx, Sx + Run, Ly + 40.f), StandT(Sx, Sx + Run, Ly)};
	for (int32 k = 1; k <= 8; ++k) { const float A = PI * k / 8.f; T.Path.Add(StandT(Sx, Sx + Run - Rt + Rt * FMath::Cos(A), Ly - Rl * FMath::Sin(A))); }
	if (TowHome.Num()) { T.Path.Insert(TowHome, 0); return; }   // (back from a hangar tow: along the taxiways first)
	if (PathBlocked(T, T.Path, T.At)) RouteHome(T);
}


bool ATugs::WatchPoints(TArray<FVector>& Pts, FVector& Side) const
{
	Pts.Reset();
	auto Person = [&](int32 Ci) { if (Crew.IsValidIndex(Ci) && Crew[Ci].A.IsValid()) { Pts.Add(Crew[Ci].A->GetActorLocation()); Pts.Add(Crew[Ci].A->GetActorLocation() + FVector(0, 0, 185.f)); } };
	// the ground power being worked: the unit, the receptacle, the one carrying the plug
	for (const FTug& T : Tugs)
		if ((T.Gpu.bInHand || T.Gpu.PanelWant > 0.f || T.Gpu.PanelAng > 1.f) && T.Gpu.M.IsValid())
		{
			const FVector G = T.Gpu.M->GetActorLocation(), R = Receptacle(T);
			Pts.Add(G - T.Gpu.M->GetActorForwardVector() * 130.f); Pts.Add(G + T.Gpu.M->GetActorForwardVector() * 220.f + FVector(0, 0, 180.f)); Pts.Add(R); Pts.Add(FVector(R.X, R.Y, G.Z));
			Person(T.Handler != INDEX_NONE ? T.Handler : T.Driver);
			Side = FVector::CrossProduct(R - G, FVector::UpVector).GetSafeNormal2D();
			return true;
		}
	// the baggage tractors and carts at work (GSE.h) - unless a tug is at the aircraft (the push and its coupling come first)
	{
		bool bTugKey = false;
		for (const FTug& T : Tugs) if (T.State == EState::ToNose || T.State == EState::Coupling || T.State == EState::Hooked || T.State == EState::Unhook || T.State == EState::Backing || T.State == EState::ShowPin) bTugKey = true;
		if (!bTugKey) if (const AGSE* G = AGSE::Find(GetWorld())) if (G->WatchPoints(Pts, Side)) return true;
	}
	// a tug at work: the tug (its four corners), what it is coupled to, its crew
	for (const FTug& T : Tugs)
	{
		if (T.State == EState::Parked || T.State == EState::Manned || !T.M.IsValid()) continue;
		const FVector F = FRotator(0.f, T.Yaw, 0.f).Vector(), Rt = FRotator(0.f, T.Yaw + 90.f, 0.f).Vector();
		for (const float a : {-280.f, 280.f}) for (const float b : {-140.f, 140.f}) Pts.Add(T.At + F * a + Rt * b);
		Pts.Add(T.At + FVector(0, 0, 220.f));
		if (T.State == EState::Coupling || T.State == EState::Hooked || T.State == EState::Unhook || T.State == EState::ToNose)
		{ Pts.Add(T.Fit); Pts.Add(T.Fit + FVector(0, 0, 400.f)); Pts.Add(T.Fit + T.Fwd * 500.f + FVector(0, 0, 350.f)); }   // (the nose of the aircraft)
		Person(T.Handler); if (T.State == EState::Boarding || T.State == EState::ShowPin) Person(T.Driver);
		if (T.State == EState::Hooked) for (const int32 w : T.Walkers) Person(w);
		Side = Rt;
		return true;
	}
	return false;
}

bool ATugs::AllDone() const
{
	bool bWorked = false;
	for (const FTug& T : Tugs) { if (!T.PinKey.IsEmpty()) bWorked = true; if (T.State != EState::Parked && T.State != EState::Manned) return false; if (T.Gpu.bInHand || T.Gpu.PanelWant > 0.f) return false; }
	for (const FCrew& C : Crew) if (C.Q.Num() || C.Follow || C.Hold) return false;
	// (-GseTest: done when the baggage tractors' jobs are done - and the push of the departure they served, if any)
	if (const AGSE* G = AGSE::Find(GetWorld())) if (G->IsTest()) return G->AllDone() && (bWorked || !G->HadDeparture());
	return bWorked;
}
