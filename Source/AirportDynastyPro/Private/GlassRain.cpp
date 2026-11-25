#include "GlassRain.h"
#include "ADPLog.h"
#include "ADPKeep.h"
#include "BabylonModel.h"
#include "Puddles.h"
#include "SunClock.h"
#include "WindSocks.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

static TAutoConsoleVariable<int32> CVarGlassRain(TEXT("adp.GlassRain"), 1, TEXT("Rain drops on the glass: 1 on, 0 off"));

float UGlassRain::Fill = 0.f;
float UGlassRain::FillRate = 0.f;

bool UGlassRain::Roofed(UWorld* W, const FVector& At, const AActor* Not)
{
	if (!W) return false;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(GlassRoof), false);
	const AActor* A = Not;
	for (int32 i = 0; A && i < 4; ++i) { Q.AddIgnoredActor(A); A = A->GetAttachParentActor(); }
	for (int32 i = 0; i < 6; ++i)
	{
		FHitResult H;
		if (!W->LineTraceSingleByChannel(H, At, At + FVector(0.f, 0.f, 4000.f), ECC_Visibility, Q)) return false;
		// (its own blocking box, whoever owns it: small and it stands inside it)
		UPrimitiveComponent* C = H.GetComponent();
		if (!C || C->Bounds.GetBox().GetSize().GetMax() > 2000.f || !C->Bounds.GetBox().ExpandBy(20.f).IsInsideXY(At)) return true;
		Q.AddIgnoredComponent(C);
	}
	return false;
}

void UGlassRain::Tick(float Dt)
{
	if (!Mpc) Mpc = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Weather/MPC_GlassRain.MPC_GlassRain"));
	const APuddles* P = APuddles::Get();
	UMaterialParameterCollectionInstance* C = Mpc && GetWorld() ? GetWorld()->GetParameterCollectionInstance(Mpc) : nullptr;
	if (!C || !P || Dt <= 0.f) { Fill = FillRate = 0.f; return; }
	// (the weather's own clock, so a test at a higher weather speed sees the same; the runners no faster than 8 times)
	const float D = Dt * FMath::Max(0.f, APuddles::WeatherSpeed), K = P->GetRain();   // (0 when it freezes: snow)
	if (!bStarted) { bStarted = true; if (P->IsRainyDay()) F = 1.f; }
	if (K > 0.03f)
	{
		// about 4.5 minutes in ordinary rain, 2.5 in heavy, 25 and more in a drizzle
		FillRate = FMath::Pow(K / 0.667f, 1.5f) / 270.f;
		F = FMath::Min(1.f, F + FillRate * D); Dry = FMath::Max(0.f, Dry - FillRate * D);
		Run = FMath::FInterpConstantTo(Run, 0.15f + 0.45f * K, D, 0.02f);
	}
	else
	{
		FillRate = 0.f;
		// over minutes each by itself, the small ones first (the material); sun and wind make it faster, cold slower
		const float Sun = ASunClock::Theme <= 2 ? FMath::Clamp(ASunClock::SunElevation / 30.f, 0.f, 1.f) : 0.f;
		Dry = FMath::Min(1.f, Dry + D * (1.f + 0.6f * Sun + AWindSocks::WindKt / 15.f) / (P->GetAirTemp() < 5.f ? 450.f : 300.f));
		Run = FMath::FInterpConstantTo(Run, 0.f, D, 0.5f / 150.f);
		if (Dry >= 1.f) F = Dry = 0.f;
	}
	const bool bOn = CVarGlassRain.GetValueOnGameThread() != 0 && APuddles::bGroundWeatherOn;
	Fill = bOn ? F : 0.f;
	if (Fill <= 0.f && bZeroSent) return;
	bZeroSent = Fill <= 0.f;
	Time = FMath::Fmod(Time + Dt * FMath::Clamp(APuddles::WeatherSpeed, 0.f, 8.f), 3600.f);
	static const FName NF(TEXT("GlassFill")), ND(TEXT("GlassDry")), NT(TEXT("GlassTime")), NR(TEXT("GlassRun"));
	C->SetScalarParameterValue(NF, Fill); C->SetScalarParameterValue(ND, Dry);
	C->SetScalarParameterValue(NT, Time); C->SetScalarParameterValue(NR, Run);
}

// #region ---- a vehicle's panes
namespace
{
	UMaterialInterface* GlassBase()
	{
		static UMaterialInterface* M = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_TruckGlass.M_TruckGlass")));
		return M;
	}
	struct FPart { UInstancedStaticMeshComponent* K; int32 Slot; FTransform Root; FBox Box; FVector Mid; };
	// its glass, each with the place of its (first) instance in the root's frame and its box there
	TArray<FPart> GlassParts(ABabylonModel* M)
	{
		TArray<FPart> Out; TArray<UInstancedStaticMeshComponent*> Ks; M->Parts(Ks);
		const FTransform Inv = M->GetActorTransform().Inverse();
		for (UInstancedStaticMeshComponent* K : Ks)
		{
			if (!K || !K->GetStaticMesh() || K->GetInstanceCount() == 0) continue;
			for (int32 i = 0; i < K->GetNumMaterials(); ++i)
			{
				UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(K->GetMaterial(i));
				if (!D || !GlassBase() || D->GetBaseMaterial() != GlassBase()->GetBaseMaterial()) continue;
				FPart P{K, i, FTransform::Identity, FBox(ForceInit), FVector::ZeroVector};
				for (int32 n = 0; n < K->GetInstanceCount(); ++n)
				{
					FTransform T; if (!K->GetInstanceTransform(n, T, true)) continue;
					T = T * Inv; if (n == 0) P.Root = T;
					P.Box += K->GetStaticMesh()->GetBoundingBox().TransformBy(T);
				}
				if (P.Box.IsValid) Out.Add(P);
				break;
			}
		}
		return Out;
	}
	UMaterialInstanceDynamic* OwnGlass(const FPart& P, TArray<UMaterialInstanceDynamic*>& NewGlass)
	{
		UMaterialInstanceDynamic* Was = Cast<UMaterialInstanceDynamic>(P.K->GetMaterial(P.Slot));
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(GlassBase(), P.K);
		if (Was) D->CopyParameterOverrides(Was);
		P.K->SetMaterial(P.Slot, D); NewGlass.Add(D);
		return D;
	}
	FLinearColor V4(const FVector& V, float W) { return FLinearColor(V.X, V.Y, V.Z, W); }
}

void FGlassRainVehicle::Dress(ABabylonModel* M, TArray<UMaterialInstanceDynamic*>& NewGlass)
{
	Panes.Reset(); CheckIn = 0.f; if (!M) return;
	TArray<FPart> Ps = GlassParts(M);
	// panes standing together (a cab's) have one middle, the way out is away from it
	TArray<FBox> Cl; TArray<int32> Of;
	for (const FPart& P : Ps) { Cl.Add(P.Box.ExpandBy(45.f)); Of.Add(Of.Num()); }
	for (bool bMore = true; bMore;)
	{
		bMore = false;
		for (int32 a = 0; a < Cl.Num(); ++a) for (int32 b = a + 1; b < Cl.Num(); ++b)
			if (Cl[a].IsValid && Cl[b].IsValid && Cl[a].Intersect(Cl[b])) { Cl[a] += Cl[b]; Cl[b] = FBox(ForceInit); for (int32& o : Of) if (o == b) o = a; bMore = true; }
	}
	TMap<UMaterialInstanceDynamic*, int32> Used; const float Seed = FMath::FRand() * 50.f; int32 NOwn = 0;
	for (int32 i = 0; i < Ps.Num(); ++i)
	{
		UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(Ps[i].K->GetMaterial(Ps[i].Slot));
		const bool bSame = Ps[i].Root.Equals(FTransform::Identity, 0.01f);
		const int32* U = Used.Find(D);
		if (U && bSame && *U == Of[i]) continue;           // (already told, the same numbers)
		if (U || !bSame) { D = OwnGlass(Ps[i], NewGlass); ++NOwn; }
		else Used.Add(D, Of[i]);
		const FVector C = Ps[i].Root.InverseTransformPosition(Cl[Of[i]].GetCenter());
		D->SetVectorParameterValue(TEXT("RainC"), V4(C, 1.f));
		D->SetScalarParameterValue(TEXT("RainSeed"), Seed + i * 3.7f);
		D->SetScalarParameterValue(TEXT("RainOwn"), Own);
		FPane N; N.D = D; N.ToLocal = Ps[i].Root.GetRotation().Inverse(); Panes.Add(N);
	}
	OwnSent = Own; LeanSent = 0.f;
}

void FGlassRainVehicle::Tick(ABabylonModel* M, float Dt, float SpeedMs, const FVector& BackRoot)
{
	if (!M || !Panes.Num()) return;
	if (UGlassRain::Fill <= 0.f) { CheckIn = FMath::FRand(); Lean = 0.f; bFresh = true; return; }   // (dry everywhere)
	if ((CheckIn -= Dt) <= 0.f)
	{
		CheckIn = FMath::FRandRange(3.f, 5.f);
		bRoof = UGlassRain::Roofed(M->GetWorld(), M->GetActorLocation() + FVector(0.f, 0.f, 150.f), M);
		if (bFresh) { bFresh = false; Own = bRoof ? 0.f : 1.f; }   // (where it stood when the rain began)
	}
	const float D = Dt * FMath::Max(0.f, APuddles::WeatherSpeed);
	if (bRoof) Own = FMath::Max(0.f, Own - D / 240.f);
	else Own = FMath::Min(1.f, Own + D * UGlassRain::FillRate);
	Lean = FMath::FInterpTo(Lean, FMath::Clamp((SpeedMs - 4.f) / 16.f, 0.f, 1.f), Dt, 1.5f);
	const bool bOwn = FMath::Abs(Own - OwnSent) > 0.004f || ((Own == 0.f || Own == 1.f) && Own != OwnSent), bLean = FMath::Abs(Lean - LeanSent) > 0.05f;
	if (!bOwn && !bLean) return;
	OwnSent = Own; LeanSent = Lean;
	for (const FPane& P : Panes)
		if (UMaterialInstanceDynamic* X = P.D.Get())
		{
			if (bOwn) X->SetScalarParameterValue(TEXT("RainOwn"), Own);
			// (backwards, and a little up a windscreen)
			if (bLean) X->SetVectorParameterValue(TEXT("RainLean"), V4(P.ToLocal.RotateVector((BackRoot + FVector(0.f, 0.f, 0.15f)).GetSafeNormal()), Lean < 0.03f ? 0.f : Lean));
		}
}
// #endregion

// #region ---- the wipers
void FGlassWipers::Wire(ABabylonModel* M, FGlassRainVehicle& Rain, const TArray<FGlassBlade>& Blades, float H, const TCHAR* What, TArray<UMaterialInstanceDynamic*>& NewGlass)
{
	if (bWired || !M || !Blades.Num()) return;
	bWired = true;
	for (const FPart& P : GlassParts(M))
	{
		bool bOn = false;
		for (const FGlassBlade& B : Blades)
			bOn |= P.Box.ExpandBy(15.f).IsInside(B.Pivot + FQuat(B.Axis, B.Sweep * 0.5f).RotateVector(B.Rest) * (B.RIn + B.ROut) * 0.5f);
		if (!bOn) continue;
		UMaterialInstanceDynamic* D = OwnGlass(P, NewGlass);
		for (int32 b = 0; b < FMath::Min(2, Blades.Num()); ++b)
		{
			const FGlassBlade& B = Blades[b]; const FTransform& R = P.Root;
			const FString N = FString::FromInt(b);
			D->SetVectorParameterValue(FName(*(TEXT("WipA") + N)), V4(R.InverseTransformPosition(B.Pivot), B.RIn));
			D->SetVectorParameterValue(FName(*(TEXT("WipB") + N)), V4(R.InverseTransformVectorNoScale(B.Rest), B.ROut));
			D->SetVectorParameterValue(FName(*(TEXT("WipC") + N)), V4(R.InverseTransformVectorNoScale(B.Axis), B.Sweep));
		}
		D->SetScalarParameterValue(TEXT("WipH"), H);
		Panes.Add(D);
		FGlassRainVehicle::FPane Own; Own.D = D; Own.ToLocal = P.Root.GetRotation().Inverse();
		Rain.Panes.Add(Own);
	}
	if (Panes.Num()) ADP_LOG(TEXT("Glass rain: %s: %d blades on %d panes"), What, Blades.Num(), Panes.Num());
	else ADP_WARN(TEXT("Glass rain: %s: no windscreen found under its %d wiper blades"), What, Blades.Num());
}

void FGlassWipers::Tick(float Dt, float T, float Ts)
{
	if (!Panes.Num()) return;
	if (T > 0.f && (T < Last || Last <= 0.f)) { Prev = FMath::Min(1000.f, Age + Dt); Age = T; ++No; }
	else if (T > 0.f) Age = T;
	else Age = FMath::Min(1000.f, Age + Dt);
	Last = T; SweepS = Ts;
	const bool bMoving = T > 0.f && T <= Ts + 0.1f;
	if (UGlassRain::Fill <= 0.f || (!bMoving && (Since += Dt) < 0.5f)) return;
	if (!bMoving && Age > 600.f && Since < 30.f) return;
	Since = 0.f;
	for (const TWeakObjectPtr<UMaterialInstanceDynamic>& W : Panes)
		if (UMaterialInstanceDynamic* D = W.Get()) D->SetVectorParameterValue(TEXT("WipT"), FLinearColor(Age, Prev, SweepS, float(No & 255)));
}
// #endregion
