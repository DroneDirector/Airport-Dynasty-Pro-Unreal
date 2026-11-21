#include "Tugs.h"
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
		USoundBase* S = ADPKeep(LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/PushbackTug/Sounds/%s.%s"), Name, Name)));
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

FVector ATugs::Plan(float X, float Y) const { return FTerminalPlan::ToWorld(X, AAirfield::GroundAt(X, Y), Y); }

void ATugs::Setup()
{
	if (bReady) return;
	bReady = true;
	// (owner 2026-11-21: "the pushback truck should always be placed in front of the terminal, right outside of the gates")
	// one tug at every contact gate: on the stand's centre line in front of the terminal, facing where the nose will be -
	// an arriving aircraft stops ~7 m short of its towbar; nothing on a red marking or under a wing
	for (const FAirfieldData::FStand& St : AAirfield::Shared().Stands)
	{
		if (!St.bJetway || St.bCargo || St.Span > 40.f) continue;
		FTug T; T.StandX = St.X; T.Home = T.At = Plan(St.X + BoxX, BoxY);
		T.HomeYaw = T.Yaw = FVector(0.f, St.Dir < 0 ? -1.f : 1.f, 0.f).Rotation().Yaw;   // (facing along the stand, away from the terminal: plan +y is world -y)
		if (SpawnAt(T)) { Pose(T); Tugs.Add(T); }
	}
	ADP_LOG(TEXT("Tugs: %d tugs ready at the gates"), Tugs.Num());
}

bool ATugs::SpawnAt(FTug& T)
{
	ABabylonModel* M = GetWorld()->SpawnActor<ABabylonModel>(T.At, FRotator::ZeroRotator);
	if (!M || !M->Place(TEXT("PushbackTug"))) { if (M) M->Destroy(); ADP_WARN(TEXT("Tugs: the tug model is missing (Tools/import_model.py ... PushbackTug)")); return false; }
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
			if (F.Get<2>().EndsWith(TEXT("_Lens")) || F.Get<2>() == TEXT("BeaconLens")) { FLens L; L.K = F.Get<0>(); L.Ix = F.Get<1>(); L.Kind = Wh.Kind; T.Lenses.Add(L); }
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
	const bool bRun = T.Engine != nullptr, bBack = bRun && T.bReverse && T.Path.Num() > 0;
	const float F = FMath::Fmod(Flash, 1.f);
	const bool bFlash = bRun && (F < 0.08f || (F > 0.18f && F < 0.26f));
	for (FLens& L : T.Lenses)
	{
		float V = 0.f;
		switch (L.Kind)
		{
		case Low: V = bRun ? 0.8f : 0.f; break;
		case Tail: V = bRun ? 0.3f : 0.f; break;
		case Marker: V = bRun ? 0.5f : 0.f; break;
		case Reverse: V = bBack ? 1.f : 0.f; break;
		case Beacon: V = bFlash ? 1.f : 0.f; break;
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
	// the gate's own tug (ready in front of the terminal): its engine on, waiting for off-blocks
	int32 Best = INDEX_NONE;
	for (int32 i = 0; i < Tugs.Num(); ++i) if (Tugs[i].State == EState::Parked && FMath::Abs(Tugs[i].StandX - StandX) < 1.f) Best = i;
	if (Best == INDEX_NONE) return INDEX_NONE;
	FTug& T = Tugs[Best];
	T.Key = Key; T.State = EState::Waiting; T.Path.Reset();
	ADP_LOG(TEXT("Tugs: tug %d at its gate gets ready for %s"), Best, *Key);
	Engine(T, true);
	return Best;
}

void ATugs::Prepare(const FString& Key, float StandX)
{
	Assign(Key, StandX);
}

bool ATugs::Push(const FString& Key, float StandX, const FVector& Fitting, const FVector& Fwd)
{
	const int32 I = Assign(Key, StandX);
	if (I == INDEX_NONE) return false;
	FTug& T = Tugs[I];
	const FVector F = Fwd.GetSafeNormal2D();
	T.HookAt = Fitting + F * HeadAhead * 100.f; T.HookYaw = (-F).Rotation().Yaw;
	if (T.State == EState::Waiting)
	{
		// in to the nose: off the road towards the stand, then straight at the aircraft, slowly
		T.State = EState::ToNose; T.bReverse = false; ADP_LOG(TEXT("Tugs: in to the nose of %s"), *Key);
		const FVector In = T.HookAt + F * 800.f;          // 8 m short of the hook place, in line with the nose
		T.Path = {Plan(StandX + BoxX, BoxY + 6.f), Plan(StandX + BoxX * 0.5f, BoxY + 12.5f), Plan(StandX, BoxY + 18.f), T.HookAt};   // (out of its box in a smooth S-curve onto the centre line, then straight in)
	}
	if (T.State == EState::Hooked)
	{
		static TSet<FString> Said;
		if (!Said.Contains(Key)) { Said.Add(Key); ADP_LOG(TEXT("Tugs: on %s - fitting at plan (%.1f, %.1f), the aircraft's nose towards (%.2f, %.2f), the tug at (%.1f, %.1f) facing %.0f deg"),
			*Key, Fitting.X / 100.f, -Fitting.Y / 100.f, F.X, -F.Y, T.HookAt.X / 100.f, -T.HookAt.Y / 100.f, T.HookYaw); }
		T.At = T.HookAt; T.Yaw = T.HookYaw; Pose(T); return true;
	}
	return false;
}

void ATugs::Release(const FString& Key)
{
	for (FTug& T : Tugs)
		if (T.Key == Key && T.State == EState::Hooked)
		{
			T.State = EState::Unhook; T.Clock = 0.f; ADP_LOG(TEXT("Tugs: unhooking from %s"), *Key);
			Sound(TEXT("TugHitchClunk"), T.At - FRotator(0.f, T.Yaw, 0.f).Vector() * HeadAhead * 100.f);
		}
}

bool ATugs::IsClear(const FString& Key) const
{
	for (const FTug& T : Tugs) if (T.Key == Key && (T.State == EState::ToNose || T.State == EState::Hooked || T.State == EState::Unhook || T.State == EState::Backing)) return false;
	return true;
}

int32 ATugs::NumAtWork() const
{
	int32 N = 0; for (const FTug& T : Tugs) N += T.State != EState::Parked; return N;
}

bool ATugs::WatchSpot(FVector& At, float& Yaw, bool* bHooked) const
{
	for (const FTug& T : Tugs) if ((T.State == EState::Hooked || T.State == EState::Unhook) && T.M.IsValid()) { At = T.At; Yaw = T.Yaw; if (bHooked) *bHooked = true; return true; }
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
		if (R.Size2D() < 4500.f && FVector::DotProduct(R.GetSafeNormal2D(), Move) > 0.f && T.State != EState::ToNose) bBlocked = true;
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
	TArray<const FAirfieldData::FStand*> Row; for (const FAirfieldData::FStand& St : F.Stands) Row.Add(&St);
	Row.Sort([](const FAirfieldData::FStand& A, const FAirfieldData::FStand& B) { return A.Nose != B.Nose ? A.Nose < B.Nose : A.X < B.X; });
	TArray<float> Lf, Rt; for (const auto* S : Row) { Lf.Add(S->X - S->Span / 2 - 4.5f); Rt.Add(S->X + S->Span / 2 + 4.5f); }
	for (int32 k = 0; k + 1 < Row.Num(); ++k)
		if (Row[k]->Nose == Row[k + 1]->Nose && Row[k]->Dir == Row[k + 1]->Dir && Rt[k] > Lf[k + 1] - 6.f) { const float Mid = (Row[k]->X + Row[k + 1]->X) / 2; Rt[k] = Mid; Lf[k + 1] = Mid; }
	for (int32 k = 0; k < Row.Num(); ++k)
	{
		const FAirfieldData::FStand& St = *Row[k];
		const float B = -St.Dir, Wall = 82.6f, Te = St.Nose + B * (St.Len + 3.f);
		for (const float Bx : {Lf[k], Rt[k]})
			if (Seg(FVector2f(Bx, Wall), FVector2f(Bx, Te)) < R) Hit += FString::Printf(TEXT(" boundary line of %s;"), *St.Id);
		if (St.bJetway)
		{
			const float Y0 = FMath::Min(Wall, St.Nose + B * 9.f), Y1 = FMath::Max(Wall, St.Nose + B * 9.f);
			if (Inside(St.X + 3.f, St.X + 16.f, Y0, Y1)) Hit += FString::Printf(TEXT(" jetway red zone of %s;"), *St.Id);
		}
	}
	// aircraft on stands (parked, or waiting for their pushback): nose tyre 5.1 m behind the stop line, the body back from it
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		bool bPlane = false; for (const FName& Tg : It->Tags) { const FString S = Tg.ToString(); if (S.StartsWith(TEXT("SIM/")) || S.StartsWith(TEXT("MOVE/"))) bPlane = true; }
		if (!bPlane) continue;
		const FVector L = It->GetActorLocation(); const FVector2f N(L.X / 100.f, -L.Y / 100.f);
		// its nose direction: towards the nearest stand's terminal side when it stands on one (the stands all face -y here)
		const FVector2f Fa(0.f, -1.f);
		if (FMath::Abs(N.Y - 125.f) > 6.f) continue;              // (only aircraft standing on a stand)
		const FVector2f Tip = N + Fa * 5.2f, Tail = N - Fa * 32.5f;
		for (const FVector2f& Q : Pts) { const float Along = FVector2f::DotProduct(Q - Tail, (Tip - Tail).GetSafeNormal()); if (Along > 0.f && Along < 37.7f && Seg1(Q, Tip, Tail) < 2.f) { Hit += TEXT(" under an aircraft's fuselage;"); break; } }
		const FVector2f W0 = N - Fa * 12.f, W1 = N - Fa * 19.f;  // the wings: 12-19 m behind the nose tyre, 18 m each side
		if (Inside(N.X - 18.f, N.X + 18.f, FMath::Min(W0.Y, W1.Y), FMath::Max(W0.Y, W1.Y))) Hit += TEXT(" under an aircraft's wing;");
	}
	// the terminal itself: nothing within 1 m of its apron-side wall (the terminal apron's edge, plan y 82)
	for (const FVector2f& Q : Pts) if (Q.Y < 83.f) { Hit += TEXT(" at the terminal wall;"); break; }
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
		static const TCHAR* SN[] = {TEXT("parked"), TEXT("to the stand"), TEXT("waiting"), TEXT("to the nose"), TEXT("hooked"), TEXT("unhooking"), TEXT("backing"), TEXT("home")};
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
		if (bCheck && T.State != EState::Parked && T.State != EState::Hooked && T.State != EState::Unhook) CheckTug(GetWorld(), int32(&T - Tugs.GetData()), T.At, T.Yaw, T.Key);
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
			if (!T.Path.Num()) { ADP_LOG(TEXT("Tugs: hooked on %s"), *T.Key); T.State = EState::Hooked; T.At = T.HookAt; T.Yaw = T.HookYaw; Pose(T); Sound(TEXT("TugHitchClunk"), T.At - FRotator(0.f, T.Yaw, 0.f).Vector() * HeadAhead * 100.f); }
			break;
		case EState::Unhook:
			// the towbar's pin out and the bar lifted off (4 s), then backs away from the nose
			if ((T.Clock += Dt) > 4.f)
			{
				T.State = EState::Backing; T.bReverse = true;
				const FVector Back = -FRotator(0.f, T.Yaw, 0.f).Vector();
				T.Path = {T.At + Back * 1200.f};
			}
			break;
		case EState::Backing:
			Drive(T, Dt);
			if (!T.Path.Num())
			{
				// clear of the aircraft: home along the road, into its park place front first (to the west)
				ADP_LOG(TEXT("Tugs: clear of %s, home to the park"), *T.Key); T.State = EState::Home; T.Key.Reset(); T.bReverse = false;
				// home up the stand it just cleared (empty now: its aircraft is out on the taxilane) - never under an aircraft -
				// to the head-of-stand road, along it to the park
				// back up its own stand (empty now) on the side away from the jetway, a 7 m loop in front of the terminal, and back
				// on the centre line facing the gate's next aircraft
				// (up its stand just left of the centre line, a 6 m loop in front of the terminal ending in line with its box facing
				//  the gate, then reversed straight into the box - as real drivers park)
				const float Sx = T.StandX, Rl = 6.f, Ly = BoxY + 3.4f;
				T.Path = {Plan(Sx - 1.f, Ly + 40.f), Plan(Sx - 1.f, Ly)};
				for (int32 k = 1; k <= 8; ++k) { const float A = PI * k / 8.f; T.Path.Add(Plan(Sx - 1.f - Rl + Rl * FMath::Cos(A), Ly - Rl * FMath::Sin(A))); }
			}
			break;
		case EState::Home:
			Drive(T, Dt);
			if (!T.Path.Num())
			{
				if (!T.bReverse) { T.bReverse = true; T.Path = {T.Home}; break; }       // (the loop done: reversed into its box)
				T.bReverse = false; T.State = EState::Parked; T.At = T.Home; T.Yaw = T.HomeYaw; Pose(T); Engine(T, false); ADP_LOG(TEXT("Tugs: parked in its box, engine off"));
			}
			break;
		default: break;
		}
		Lamps(T);
		Wheels(T, Dt);
	}
}
