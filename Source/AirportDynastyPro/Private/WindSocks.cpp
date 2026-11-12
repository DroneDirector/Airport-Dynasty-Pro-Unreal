// The runway windsocks and the wind (WindSocks.h).
#include "WindSocks.h"
#include "ADPLog.h"
#include "Airfield.h"
#include "SunClock.h"
#include "TerminalPlan.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
	// the measurements, the same as the model's (Art/WindSock/make_windsock.py), centimetres
	const float SockMastH = 600.f;      // the swivel's centre above the ground
	const float SockRingX = 62.f;       // the mouth ring's centre, out from the mast's axis
	const float SockRingR = 45.f;       // the mouth's radius (0.9 m across)
	const float SockTailR = 25.f;       // the tail's radius
	const float SockLen = 360.f;        // the sock's length
	const float SockLampZ = 545.f;      // the floodlights' ring
	const int32 SockBands = 5, SockPerBand = 5, SockAround = 18;
	const float SockFullKt = 15.f;      // straight out from here (each band about 3 knots)
	const float RunwayTrue = 68.f;      // the world's +X is the runway's heading (true)
	const float ThresholdIn = 250.f;    // both thresholds displaced 250 m (Data/airfield.json)
	const float SockIn = 300.f, SockOut = 160.f;   // 300 m past the threshold, 160 m from the centre line

	// the weather themes' wind (0 clear, 1 few clouds, 2 broken, 3 overcast, 4 fog, 5 rain, 6 storm, 7 snow): mean knots
	// and the gusts on top
	const float ThemeKt[8] = {7.f, 9.f, 12.f, 13.f, 3.f, 17.f, 32.f, 12.f};
	const float ThemeGust[8] = {3.f, 4.f, 5.f, 5.f, 1.f, 8.f, 14.f, 6.f};
	const float PrevailingFrom = 100.f; // the south coast's usual easterly
}

float AWindSocks::WindFrom = PrevailingFrom;
float AWindSocks::WindKt = 8.f;
float AWindSocks::Gustiness = 0.f;

AWindSocks::AWindSocks()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AWindSocks::Build()
{
	FAirfieldData D;
	if (!D.Load()) { ADP_WARN(TEXT("Windsocks: no airfield data")); return; }
	UStaticMesh* MastMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/WindSock/windsock/StaticMeshes/WindSockMast.WindSockMast"));
	UStaticMesh* FrameMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/WindSock/windsock/StaticMeshes/WindSockFrame.WindSockFrame"));
	UMaterialInterface* Orange = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/WindSock/MI_WS_SockOrange.MI_WS_SockOrange"));
	UMaterialInterface* White = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/WindSock/MI_WS_SockWhite.MI_WS_SockWhite"));
	if (!MastMesh || !FrameMesh || !Orange || !White) { ADP_WARN(TEXT("Windsocks: model or materials missing (Tools/set_wind_sock.py)")); return; }

	auto Make = [this](UStaticMesh* M, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
		C->SetupAttachment(Root);
		C->SetStaticMesh(M);
		C->SetMobility(EComponentMobility::Movable);
		C->RegisterComponent();
		AddInstanceComponent(C);
		return C;
	};
	Masts = Make(MastMesh, TEXT("WindSockMasts"));
	Masts->SetCollisionProfileName(TEXT("BlockAll"));
	Frames = Make(FrameMesh, TEXT("WindSockFrames"));
	Frames->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// the floodlights' lenses glow at night: their own copy of the material
	const int32 LensSlot = Masts->GetMaterialIndex(TEXT("LampLens"));
	if (LensSlot != INDEX_NONE) if (UMaterialInterface* L = Masts->GetMaterial(LensSlot)) { Lens = UMaterialInstanceDynamic::Create(L, this); Masts->SetMaterial(LensSlot, Lens); }

	// on the pilots' left as they land: on 08 (flying +x) that is the far side of the runway (y up), on 26 the
	// terminal's side; both outside the strip, between the runway and the parallel taxiway
	const FVector2D Spots[2] = {{D.RwyX0 + ThresholdIn + SockIn, D.RwyY + SockOut}, {D.RwyX1 - ThresholdIn - SockIn, D.RwyY - SockOut}};
	for (int32 i = 0; i < 2; ++i)
	{
		FSock S;
		S.Foot = FTerminalPlan::ToWorld(Spots[i].X, 0.f, Spots[i].Y);
		S.Yaw = WindFrom + 180.f - RunwayTrue;
		S.Phase = i * 2.37f;
		Masts->AddInstance(FTransform(FRotator::ZeroRotator, S.Foot), true);
		Frames->AddInstance(FTransform(FRotator(0, S.Yaw, 0), S.Foot), true);
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
		P->SetupAttachment(Root);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->bUseAsyncCooking = true;
		P->SetCastShadow(true);
		P->RegisterComponent();
		AddInstanceComponent(P);
		SockMeshes.Add(P);
		S.Mesh = P;
		// the four floodlights, as on the mast: each looks up and out from its corner, so whichever way the sock has
		// turned, two or three of them light it from below; no shadows of their own (cheap), a short reach
		for (int32 k = 0; k < 4; ++k)
		{
			const float A = FMath::DegreesToRadians(45.f + 90.f * k);
			const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
			USpotLightComponent* L = NewObject<USpotLightComponent>(this);
			L->SetupAttachment(Root);
			L->SetMobility(EComponentMobility::Movable);
			L->SetWorldLocationAndRotation(S.Foot + Out * 34.f + FVector(0, 0, SockLampZ + 24.f), FRotator(38.f, FMath::RadiansToDegrees(A), 0.f));
			L->SetIntensityUnits(ELightUnits::Lumens);
			L->SetIntensity(0.f);
			L->SetUseTemperature(true); L->SetTemperature(4500.f);
			L->SetOuterConeAngle(62.f); L->SetInnerConeAngle(30.f);
			L->SetAttenuationRadius(1000.f);
			L->SetSourceRadius(6.f);
			L->SetCastShadows(false);
			L->MaxDrawDistance = 150000.f; L->MaxDistanceFadeRange = 30000.f;
			L->SetVisibility(false);
			L->RegisterComponent();
			AddInstanceComponent(L);
			Lamps.Add(L);
		}
		Socks.Add(S);
		DrawSock(Socks.Last(), true);
		P->SetMaterial(0, Orange);
		P->SetMaterial(1, White);
	}
	ADP_LOG(TEXT("Windsocks: %d placed, wind %03.0f/%.0f kt"), Socks.Num(), WindFrom, WindKt);
}

void AWindSocks::UpdateWind(float Dt)
{
	// a test can fix the wind (Settings, Testing): ADP.Testing WindFrom / WindKt
	float FixKt = -1.f, FixFrom = -1.f;
	GConfig->GetFloat(TEXT("ADP.Testing"), TEXT("WindKt"), FixKt, GGameUserSettingsIni);
	GConfig->GetFloat(TEXT("ADP.Testing"), TEXT("WindFrom"), FixFrom, GGameUserSettingsIni);
	const int32 T = FMath::Clamp(ASunClock::Theme, 0, 7);
	// the direction wanders slowly round the prevailing wind; the strength rises and falls in gusts of a few seconds
	const float From = FixFrom >= 0.f ? FixFrom : PrevailingFrom + 25.f * FMath::Sin(Clock / 300.f) + 8.f * FMath::Sin(Clock / 47.f + 1.3f);
	const float G = FMath::Clamp(FMath::PerlinNoise1D(Clock * 0.17f) * 1.6f + 0.15f, 0.f, 1.f);
	const float Mean = FixKt >= 0.f ? FixKt : ThemeKt[T] * (1.f + 0.15f * FMath::Sin(Clock / 120.f));
	WindFrom = FMath::Fmod(From + 360.f, 360.f);
	WindKt = FMath::Max(0.f, Mean + (FixKt >= 0.f ? 0.f : ThemeGust[T] * G));
	Gustiness = FixKt >= 0.f ? 0.f : G;
}

void AWindSocks::DrawSock(FSock& S, bool bFirst)
{
	// the sock's centre line from the mouth to the tail, in the sock's own frame (x downwind, z up): each part of it
	// stands out once the wind is strong enough for it (the mouth first, the tail last: about 3 knots a band) and
	// hangs down as far as it is not; a little sag even when full; a travelling ripple sways it side to side and
	// up and down, stronger towards the tail and in gusts
	const int32 NR = SockBands * SockPerBand + 1;
	TArray<FVector> C; C.SetNum(NR);
	TArray<float> Lift; Lift.SetNum(NR);
	const float Kt = WindKt, Seg = SockLen / (NR - 1);
	const float Flap = (2.f + 3.f * Gustiness + 0.12f * FMath::Min(Kt, 30.f)) * FMath::Clamp(Kt / 4.f, 0.f, 1.f);
	const float W = 2.f * PI * (0.6f + Kt / 18.f), Tm = Clock + S.Phase;
	C[0] = FVector(SockRingX, 0, SockMastH);
	for (int32 i = 0; i < NR; ++i)
	{
		const float s = float(i) / (NR - 1);
		Lift[i] = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((Kt - SockFullKt * s) / 3.f, 0.f, 1.f));
		if (i == 0) continue;
		const float sm = (i - 0.5f) / (NR - 1);
		const float L = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((Kt - SockFullKt * sm) / 3.f, 0.f, 1.f));
		const float Down = (1.f - L) * 84.f + 5.f * sm * (1.f - FMath::Clamp(Kt / 28.f, 0.f, 1.f))
			+ Flap * 0.6f * sm * FMath::Sin(W * 0.83f * Tm - 5.1f * sm);
		const float Side = Flap * sm * FMath::Sin(W * Tm - 6.f * sm) + (1.f - L) * 3.f * FMath::Sin(0.7f * Tm);
		const float P = FMath::DegreesToRadians(FMath::Clamp(Down, -10.f, 89.f)), Y = FMath::DegreesToRadians(Side);
		C[i] = C[i - 1] + FVector(FMath::Cos(P) * FMath::Cos(Y), FMath::Cos(P) * FMath::Sin(Y), -FMath::Sin(P)) * Seg;
	}
	// the rings round it: round at the mouth (on its ring), flattening as the cloth goes limp
	TArray<FVector> V[2], N[2]; TArray<FVector2D> UV[2]; TArray<int32> Tri[2];
	for (int32 b = 0; b < SockBands; ++b)
	{
		const int32 Sec = b % 2;
		const int32 Base = V[Sec].Num();
		for (int32 i = b * SockPerBand; i <= (b + 1) * SockPerBand; ++i)
		{
			const float s = float(i) / (NR - 1);
			const FVector Tn = ((i + 1 < NR ? C[i + 1] : C[i]) - (i > 0 ? C[i - 1] : C[i])).GetSafeNormal();
			FVector Nrm = FVector::CrossProduct(Tn, FVector(0, 1, 0)).GetSafeNormal();
			if (Nrm.IsNearlyZero()) Nrm = FVector(0, 0, 1);
			const FVector Bn = FVector::CrossProduct(Nrm, Tn).GetSafeNormal();
			const float R = FMath::Lerp(SockRingR, SockTailR, s), Hold = FMath::Clamp(s / 0.12f, 0.f, 1.f);
			const float Limp = (1.f - Lift[i]) * Hold;
			const float RN = R * (1.f - 0.65f * Limp), RB = R * (1.f + 0.22f * Limp);
			for (int32 k = 0; k <= SockAround; ++k)
			{
				const float a = 2.f * PI * k / SockAround;
				const FVector Dir = Nrm * FMath::Cos(a) * RN + Bn * FMath::Sin(a) * RB;
				V[Sec].Add(C[i] + Dir);
				N[Sec].Add((Nrm * FMath::Cos(a) / FMath::Max(RN, 1.f) + Bn * FMath::Sin(a) / FMath::Max(RB, 1.f)).GetSafeNormal());
				UV[Sec].Add(FVector2D(float(k) / SockAround, s * 4.f));
			}
		}
		if (bFirst)
			for (int32 i = 0; i < SockPerBand; ++i)
				for (int32 k = 0; k < SockAround; ++k)
				{
					const int32 A0 = Base + i * (SockAround + 1) + k, A1 = A0 + 1, B0 = A0 + SockAround + 1, B1 = B0 + 1;
					Tri[Sec].Append({A0, B0, A1, A1, B0, B1});
				}
	}
	for (int32 Sec = 0; Sec < 2; ++Sec)
		if (bFirst) S.Mesh->CreateMeshSection(Sec, V[Sec], Tri[Sec], N[Sec], UV[Sec], TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		else S.Mesh->UpdateMeshSection(Sec, V[Sec], N[Sec], UV[Sec], TArray<FColor>(), TArray<FProcMeshTangent>());
	S.Mesh->SetWorldLocationAndRotation(S.Foot, FRotator(0, S.Yaw, 0));
}

void AWindSocks::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Socks.Num()) return;
	Clock += Dt;
	UpdateWind(Dt);
	// the swivel: the wind pushes the sock round towards downwind, harder the stronger it blows; the bearing's
	// friction slows it, so it swings past a little and settles; in calm it stays where it was
	const float Target = WindFrom + 180.f - RunwayTrue;
	for (int32 i = 0; i < Socks.Num(); ++i)
	{
		FSock& S = Socks[i];
		const float Err = FMath::FindDeltaAngleDegrees(S.Yaw, Target + 4.f * Gustiness * FMath::Sin(Clock * 1.7f + S.Phase));
		const float Push = FMath::Clamp(WindKt / 10.f, 0.f, 2.f);
		S.YawSpeed += (Err * 2.2f * Push - S.YawSpeed * 1.8f) * FMath::Min(Dt, 0.05f);
		S.Yaw += S.YawSpeed * FMath::Min(Dt, 0.05f);
		Frames->UpdateInstanceTransform(i, FTransform(FRotator(0, S.Yaw, 0), S.Foot), true, i == Socks.Num() - 1, true);
		// the cloth only when someone can see it
		if (S.Mesh->WasRecentlyRendered(0.5f)) DrawSock(S, false);
		else S.Mesh->SetWorldLocationAndRotation(S.Foot, FRotator(0, S.Yaw, 0));
	}
	// lit as the apron masts are: on at sunset, off just after sunrise, on in fog and storms
	static TWeakObjectPtr<ADirectionalLight> Sun;
	if (!Sun.IsValid()) Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	const float El = Sun.IsValid() ? -Sun->GetActorRotation().Pitch : 30.f;
	const bool bLowVis = ASunClock::Theme == 4 || ASunClock::Theme == 6;
	const bool bWant = bLowVis || (bLit ? El < 3.f : El < 0.f);
	if (bWant != bLit)
	{
		bLit = bWant;
		for (USpotLightComponent* L : Lamps) { L->SetIntensity(bLit ? 1600.f : 0.f); L->SetVisibility(bLit); }
		if (Lens) Lens->SetScalarParameterValue(TEXT("EmissiveStrength"), bLit ? 25.f : 0.f);
	}
}
