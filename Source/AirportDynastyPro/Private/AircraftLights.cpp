// Every aircraft's own lights from its type's table (to-do 243): AircraftLights.h.
#include "AircraftLights.h"
#include "ADPKeep.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	enum EGroup : uint8 { GBeacon, GStrobe, GNav, GLogo, GLanding, GTakeoff, GTaxi, GTurnoff, GWingScan, GEngineScan, GCabin, GNone };
	struct FLamp
	{
		FString Id; EGroup G = GNone;
		FVector At = FVector::ZeroVector, Dir = FVector::ZeroVector; bool bDir = false; float Cone = 180.f;
		FLinearColor Col = FLinearColor::White;
		float Glow = 0.f, Size = 0.f, Skin = 0.f, Reach = 0.f, Lumens = 0.f, Range = 0.f; bool bPool = false;
		float Cd = 0.f, Radius = 0.f;   // its real light on the nearest aircraft: candela (at night) and where it ends (m)
		bool bNoseGear = false;   // ("on": "nose-gear": on the nose leg's strut - folds with it, lit only when it is down and locked)
	};
	struct FType { FString Name; TArray<FLamp> Lamps; };
	TArray<FType> GTypes;
	bool GRead = false;

	EGroup GroupOf(const FString& S)
	{
		static const TCHAR* Names[] = {TEXT("beacon"), TEXT("strobe"), TEXT("nav"), TEXT("logo"), TEXT("landing"), TEXT("takeoff"), TEXT("taxi"), TEXT("turnoff"), TEXT("wing-scan"), TEXT("engine-scan"), TEXT("cabin-window")};
		for (int32 i = 0; i < int32(UE_ARRAY_COUNT(Names)); ++i) if (S == Names[i]) return EGroup(i);
		return GNone;
	}

	// Data/aircraft-lights.json, once (the packaged game's copy first, as the airfield)
	void Read()
	{
		if (GRead) return;
		GRead = true;
		FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/aircraft-lights.json"));
		if (!FPaths::FileExists(Path)) Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/aircraft-lights.json"));
		FString Text; TSharedPtr<FJsonObject> R;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { ADP_ERROR(TEXT("Aircraft lights: %s not found - every aircraft stays dark"), *Path); return; }
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), R) || !R) { ADP_ERROR(TEXT("Aircraft lights: %s could not be read"), *Path); return; }
		auto Vec = [](const TSharedPtr<FJsonObject>& O, const TCHAR* K, FVector& Out) -> bool
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (!O->TryGetArrayField(K, A) || A->Num() < 3) return false;
			Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
			return true;
		};
		for (const auto& Kv : R->Values)
		{
			const TSharedPtr<FJsonObject>* T = nullptr;
			if (!Kv.Value.IsValid() || !Kv.Value->TryGetObject(T)) continue;
			const TArray<TSharedPtr<FJsonValue>>* Ls = nullptr;
			if (!(*T)->TryGetArrayField(TEXT("lamps"), Ls)) continue;
			FType Ty; Ty.Name = Kv.Key;
			for (const TSharedPtr<FJsonValue>& V : *Ls)
			{
				const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
				if (!O) continue;
				FLamp L; L.Id = O->GetStringField(TEXT("id")); L.G = GroupOf(O->GetStringField(TEXT("group")));
				if (L.G == GNone) { ADP_WARN(TEXT("Aircraft lights: %s lamp %s has an unknown group - left out"), *Ty.Name, *L.Id); continue; }
				if (!Vec(O, TEXT("at"), L.At)) { ADP_WARN(TEXT("Aircraft lights: %s lamp %s has no place - left out"), *Ty.Name, *L.Id); continue; }
				L.bDir = Vec(O, TEXT("dir"), L.Dir) && !L.Dir.IsNearlyZero(); L.Dir = L.Dir.GetSafeNormal();
				FVector C(1.0); Vec(O, TEXT("color"), C); L.Col = FLinearColor(C.X, C.Y, C.Z);
				double D = 0.0;
				if (O->TryGetNumberField(TEXT("cone"), D)) L.Cone = D;
				if (O->TryGetNumberField(TEXT("glow"), D)) L.Glow = D;
				if (O->TryGetNumberField(TEXT("size"), D)) L.Size = D;
				if (O->TryGetNumberField(TEXT("skin"), D)) L.Skin = D;
				if (O->TryGetNumberField(TEXT("reach"), D)) L.Reach = D;
				if (O->TryGetNumberField(TEXT("lumens"), D)) L.Lumens = D;
				if (O->TryGetNumberField(TEXT("range"), D)) L.Range = D;
				if (O->TryGetNumberField(TEXT("cd"), D)) L.Cd = D;
				if (O->TryGetNumberField(TEXT("radius"), D)) L.Radius = D;
				O->TryGetBoolField(TEXT("pool"), L.bPool);
				FString On; if (O->TryGetStringField(TEXT("on"), On)) L.bNoseGear = On == TEXT("nose-gear");
				Ty.Lamps.Add(L);
			}
			ADP_LOG(TEXT("Aircraft lights: %s - %d lamps"), *Ty.Name, Ty.Lamps.Num());
			GTypes.Add(MoveTemp(Ty));
		}
	}

	// the nearest aircraft (to the camera) get real lights: at most two, within 300 m, fading out from 220 m
	constexpr int32 MaxNear = 2;
	constexpr float NearIn = 22000.f, NearOut = 30000.f;
	TArray<TWeakObjectPtr<UADPAircraftLights>> GAll;
	uint64 GRankFrame = MAX_uint64;
	TMap<const UADPAircraftLights*, float> GNearW;

	FADPLightState ParkedFor(bool bNight, bool bPowered)
	{
		FADPLightState S; S.bPowered = bPowered;
		S.bNav = S.bLogo = S.bCabin = bPowered && bNight;
		return S;
	}
}

namespace
{
	// the sun's height in degrees (as BabylonModel's LampsByDaylight reads it); -90 when there is no sun
	float SunHeight(const UWorld* W)
	{
		static TWeakObjectPtr<ADirectionalLight> Sun;
		static TWeakObjectPtr<const UWorld> SunW;
		if (!W) return -90.f;
		if (!Sun.IsValid() || SunW.Get() != W) { Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(W, ADirectionalLight::StaticClass())); SunW = W; }
		return Sun.IsValid() ? float(-Sun->GetActorRotation().Pitch) : -90.f;
	}
	// The lamps' strength against the light round them (owner 2026-11-28: real strength at sunset, not washed out). The
	// game's sun is 10 lux (SunClock) and its floodlit night apron about 3 lux, where the real ones are ~50 000 lux by day,
	// ~1 000 at sunset and ~30 on a lit apron at night: the table holds candela against the game's night, then the real
	// ratio as the sun rises - 1 at night (sun 6 deg under), 1/20 at sunset (sun 1 deg up), 1/400 by day (15 deg up).
	float AmbientShare(float El)
	{
		const float L20 = FMath::Loge(20.f), L400 = FMath::Loge(400.f);
		const float Ln = El <= 1.f ? L20 * FMath::Clamp((El + 6.f) / 7.f, 0.f, 1.f) : L20 + (L400 - L20) * FMath::Clamp((El - 1.f) / 14.f, 0.f, 1.f);
		return FMath::Exp(-Ln);
	}
}

bool ADPAircraftLights::Night(const UWorld* W) { return SunHeight(W) < 0.f; }

FADPLightState ADPAircraftLights::Parked(const UWorld* W, bool bPowered) { return ParkedFor(Night(W), bPowered); }

UADPAircraftLights::UADPAircraftLights()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

UADPAircraftLights* UADPAircraftLights::Fit(AActor* Plane, const FString& Type)
{
	if (!Plane) return nullptr;
	if (UADPAircraftLights* Old = Plane->FindComponentByClass<UADPAircraftLights>()) return Old;
	Read();
	const int32 Ix = GTypes.IndexOfByPredicate([&](const FType& T) { return T.Name == Type; });
	if (Ix == INDEX_NONE) { ADP_WARN(TEXT("Aircraft lights: no table for %s in Data/aircraft-lights.json - %s stays dark"), *Type, *Plane->GetName()); return nullptr; }
	// the lamp face's size with distance: real size close by, then slowly bigger so a lamp stays a visible point far off
	static UCurveFloat* Far = nullptr;
	if (!Far)
	{
		Far = ADPKeep(NewObject<UCurveFloat>(GetTransientPackage()));
		Far->FloatCurve.AddKey(0.f, 1.f); Far->FloatCurve.AddKey(3000.f, 1.f); Far->FloatCurve.AddKey(30000.f, 3.f); Far->FloatCurve.AddKey(300000.f, 15.f);
	}
	static UMaterialInterface* Glow = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Lamps/M_LampGlow.M_LampGlow")));
	static UMaterialInterface* Shine = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Lamps/M_AircraftLampLight.M_AircraftLampLight")));
	static bool bSaid = false;
	if (!Shine && !bSaid) { bSaid = true; ADP_ERROR(TEXT("Aircraft lights: M_AircraftLampLight is missing (Tools/make_aircraft_lamp_light.py) - the lamps light nothing round them")); }
	if (!Glow && !bSaid) { bSaid = true; ADP_ERROR(TEXT("Aircraft lights: the lamp glow M_LampGlow is missing (Tools/make_lamp_glow.py) - no lamp faces")); }
	USceneComponent* Root = Plane->GetRootComponent();
	if (!Root) { ADP_WARN(TEXT("Aircraft lights: %s has no root - no lamps"), *Plane->GetName()); return nullptr; }
	UADPAircraftLights* C = NewObject<UADPAircraftLights>(Plane);
	C->TypeIx = Ix;
	C->Phase = float(GetTypeHash(Plane->GetName()) % 1000u) / 1000.f;
	const FType& Ty = GTypes[Ix];
	for (int32 i = 0; i < Ty.Lamps.Num(); ++i)
	{
		const FLamp& L = Ty.Lamps[i];
		FLampRt R; R.Lamp = i;
		if (L.Glow > 0.f && L.Size > 0.f && Glow)
		{
			// (was a sphere: M_LampGlow's round gradient is drawn for a disc facing you, so on a sphere's UVs the lens hardly
			// showed - owner 2026-11-28: flashing lights but no light source to be seen)
			UMaterialBillboardComponent* F = NewObject<UMaterialBillboardComponent>(Plane);
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Glow, Plane);
			M->SetVectorParameterValue(TEXT("Color"), L.Col); M->SetScalarParameterValue(TEXT("Brightness"), 0.f);
			const float Half = L.Size * 100.f * 0.75f;   // (the lens itself and a faint edge: owner 11-28, the orbs were too big - the glow round it comes from the bloom)
			F->AddElement(M, nullptr, false, Half, Half, Far);
			F->SetCastShadow(false); F->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			F->SetupAttachment(Root); F->SetRelativeLocation(L.At);
			F->SetVisibility(false);
			F->RegisterComponent();
			R.Face = F; R.FaceM = M; C->Keep.Add(M);
		}
		if (Shine && L.Skin > 0.f && L.Reach > 0.f)
		{
			// the light decal: round the lamp (its skin and what is right there), or along the beam for a pool on the ground
			const float Rc = L.Reach * 100.f;
			UDecalComponent* D = NewObject<UDecalComponent>(Plane);
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Shine, Plane);
			M->SetVectorParameterValue(TEXT("Color"), L.Col); M->SetScalarParameterValue(TEXT("Strength"), 0.f);
			D->SetDecalMaterial(M);
			D->DecalSize = FVector(L.bPool && L.bDir ? Rc * 0.5f : Rc);
			D->SetupAttachment(Root);
			D->SetRelativeLocation(L.bPool && L.bDir ? L.At + L.Dir * Rc * 0.5f : L.At);
			D->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));   // (projected straight down; the material works out the light from the lamp's place)
			D->SetFadeScreenSize(0.0002f);                        // (seen from any distance: no aircraft stays dark)
			D->SetVisibility(false);
			D->RegisterComponent();
			R.Decal = D; R.DecalM = M; C->Keep.Add(M);
		}
		C->Rt.Add(R);
	}
	C->RegisterComponent();
	GAll.Add(C);
	return C;
}

void UADPAircraftLights::SetNoseGear(const FTransform& Fold, bool bDownLocked)
{
	bNoseDown = bDownLocked;
	if (!Fold.Equals(NoseXf, 0.01)) { NoseXf = Fold; bNoseMoved = true; }
}

void UADPAircraftLights::OnUnregister()
{
	GAll.RemoveAll([this](const TWeakObjectPtr<UADPAircraftLights>& P) { return !P.IsValid() || P.Get() == this; });
	GNearW.Remove(this);
	Super::OnUnregister();
}

float UADPAircraftLights::NearWanted() const
{
	UWorld* W = GetWorld();
	if (GRankFrame != GFrameCounter)
	{
		GRankFrame = GFrameCounter; GNearW.Reset();
		const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (!PC || !PC->PlayerCameraManager) return 0.f;
		const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
		TArray<TPair<float, const UADPAircraftLights*>> By;
		for (const TWeakObjectPtr<UADPAircraftLights>& P : GAll)
			if (const UADPAircraftLights* L = P.Get()) if (L->GetWorld() == W && L->GetOwner())
			{
				const float D = FVector::Dist(Cam, L->GetOwner()->GetActorLocation());
				if (D < NearOut) By.Add(TPair<float, const UADPAircraftLights*>(D, L));
			}
		By.Sort([](const TPair<float, const UADPAircraftLights*>& A, const TPair<float, const UADPAircraftLights*>& B) { return A.Key < B.Key; });
		for (int32 i = 0; i < FMath::Min(MaxNear, By.Num()); ++i) GNearW.Add(By[i].Value, 1.f - FMath::SmoothStep(NearIn, NearOut, By[i].Key));
	}
	return GNearW.FindRef(this);
}

void UADPAircraftLights::MakeReal(FLampRt& R)
{
	const FLamp& L = GTypes[TypeIx].Lamps[R.Lamp];
	AActor* O = GetOwner();
	// a beam's pool on the ground (lumens, its range) or the lamp's light on the skin round it (candela, a short radius);
	// inverse-square, so it is strong right at the lamp and gone within its real reach
	const bool bPoolLight = L.bPool && L.Lumens > 0.f;
	ULocalLightComponent* S = nullptr;
	if (L.bDir)
	{
		USpotLightComponent* Sp = NewObject<USpotLightComponent>(O);
		Sp->SetOuterConeAngle(FMath::Clamp(L.Cone, 1.f, 80.f)); Sp->SetInnerConeAngle(FMath::Clamp(L.Cone * 0.5f, 0.5f, 79.f));
		S = Sp;
	}
	else S = NewObject<UPointLightComponent>(O);
	S->SetMobility(EComponentMobility::Movable);
	S->SetupAttachment(O->GetRootComponent());
	const FVector At = L.bNoseGear ? NoseXf.TransformPosition(L.At) : L.At, Dir = L.bNoseGear ? NoseXf.TransformVectorNoScale(L.Dir) : L.Dir;
	S->SetRelativeLocation(At); if (L.bDir) S->SetRelativeRotation(Dir.Rotation());
	S->SetIntensityUnits(bPoolLight ? ELightUnits::Lumens : ELightUnits::Candelas); S->SetIntensity(0.f); S->SetLightColor(L.Col);
	if (UPointLightComponent* Pt = Cast<UPointLightComponent>(S)) { Pt->bUseInverseSquaredFalloff = true; Pt->SetSourceRadius(FMath::Min(L.Size * 50.f, 10.f)); }
	S->SetAttenuationRadius(FMath::Max(bPoolLight ? L.Range : L.Radius, 0.5f) * 100.f);
	S->SetCastShadows(false);   // (no shadows for now: the frame-rate measurement on the owner's word decides)
	S->SetVisibility(false);
	S->RegisterComponent();
	R.Real = S;
}

void UADPAircraftLights::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	ADP_PERF_SCOPE("aircraft lights (each frame)", 1.f);
	AActor* O = GetOwner(); UWorld* W = GetWorld();
	if (!O || !W || !GTypes.IsValidIndex(TypeIx)) return;
	if ((NightPoll -= Dt) <= 0.f) { NightPoll = 2.f; const float El = SunHeight(W); bNight = El < 0.f; Amb = AmbientShare(El); }
	if (bParked) State = ParkedFor(bNight, bParkPower);
	// the flashing (owner 2026-11-21: the real timing): beacons about once a second, each aircraft its own beat; strobes a
	// double flash every 1.2 s
	const double T = W->GetTimeSeconds();
	const bool bBeat = FMath::Fmod(T + Phase, 1.0) < 0.12;
	const double Ph = FMath::Fmod(T, 1.2);
	const bool bFlash = Ph < 0.05 || (Ph > 0.12 && Ph < 0.17);
	Near = FMath::FInterpConstantTo(Near, NearWanted(), Dt, 0.4f);   // (2.5 s from decal to real light: not seen)
	const FTransform Xf = O->GetActorTransform();
	const bool bMoved = !bPlaced || !Xf.Equals(LastXf, 0.5);
	if (bMoved) { LastXf = Xf; bPlaced = true; }
	const FADPLightState& S = State;
	const bool bFolded = bNoseMoved; bNoseMoved = false;
	for (FLampRt& R : Rt)
	{
		const FLamp& L = GTypes[TypeIx].Lamps[R.Lamp];
		// the nose gear's lamps go where the strut is (the retraction's turn about its hinge)
		const FVector LAt = L.bNoseGear ? NoseXf.TransformPosition(L.At) : L.At, LDir = L.bNoseGear ? NoseXf.TransformVectorNoScale(L.Dir) : L.Dir;
		if (L.bNoseGear && bFolded)
		{
			if (R.Face) R.Face->SetRelativeLocation(LAt);
			if (R.Decal) R.Decal->SetRelativeLocation(L.bPool && L.bDir ? LAt + LDir * L.Reach * 50.f : LAt);
			if (R.Real) { R.Real->SetRelativeLocation(LAt); R.Real->SetRelativeRotation(LDir.Rotation()); }
		}
		bool b = false;
		if (S.bPowered)
			switch (L.G)
			{
			case GBeacon: b = S.bBeacon && bBeat; break;
			case GStrobe: b = S.bStrobe && bFlash; break;
			case GNav: b = S.bNav; break;
			case GLogo: b = S.bLogo; break;
			case GLanding: b = S.bLanding; break;
			case GTakeoff: b = S.bTakeoff; break;
			case GTaxi: b = S.bTaxi; break;
			case GTurnoff: b = S.bTurnoff; break;
			case GWingScan: b = S.bWingScan; break;
			case GEngineScan: b = S.bEngineScan; break;
			case GCabin: b = S.bCabin; break;
			default: break;
			}
		if (L.bNoseGear && !bNoseDown) b = false;   // (switched off by the gear lever: not down and locked)
		const float On = b ? 1.f : 0.f;
		// the lens: brighter against the dark, yet always a clear bright point (a lens is far brighter than the skin round it)
		const float FaceV = On * FMath::Pow(Amb, 0.4f);
		if (R.Face && FaceV != R.Was) { R.FaceM->SetScalarParameterValue(TEXT("Brightness"), L.Glow * FaceV); R.Face->SetVisibility(b); }
		R.Was = FaceV;
		// its light on the skin and round it; on the nearest aircraft the decal gives way to the real light
		const bool bRealLamp = (L.bPool && L.Lumens > 0.f && L.bDir) || L.Cd > 0.f;
		const float Dv = On * Amb * (bRealLamp ? 1.f - Near : 1.f);
		if (R.Decal)
		{
			if (Dv > 0.001f && (bMoved || R.WasDecal <= 0.001f || (L.bNoseGear && bFolded)))
			{
				const FVector P = Xf.TransformPosition(LAt);
				const FVector Dw = L.bDir ? Xf.TransformVectorNoScale(LDir) : FVector::ZeroVector;
				R.DecalM->SetVectorParameterValue(TEXT("LampPos"), FLinearColor(P.X, P.Y, P.Z, L.Reach * 100.f));
				R.DecalM->SetVectorParameterValue(TEXT("LampDir"), FLinearColor(Dw.X, Dw.Y, Dw.Z, L.bDir ? FMath::Cos(FMath::DegreesToRadians(FMath::Min(L.Cone, 179.f))) : -1.f));
			}
			// (all round: no direction, the cone term is a flat half - doubled back)
			if (Dv != R.WasDecal) { R.DecalM->SetScalarParameterValue(TEXT("Strength"), L.Skin * Dv * (L.bDir ? 1.f : 2.f)); R.Decal->SetVisibility(Dv > 0.001f); }
			R.WasDecal = Dv;
		}
		if (bRealLamp)
		{
			const float Rv = On * Near * Amb;
			if (Rv > 0.00001f && !R.Real) MakeReal(R);
			if (R.Real && Rv != R.WasReal) { R.Real->SetIntensity((L.bPool && L.Lumens > 0.f ? L.Lumens : L.Cd) * Rv); R.Real->SetVisibility(Rv > 0.00001f); }
			R.WasReal = Rv;
		}
	}
}
