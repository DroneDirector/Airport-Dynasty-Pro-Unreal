// The apron buses' battery (to-do 277 E3b): used by the way, the hours, heating or cooling and the doors (Data/bus-battery.json),
// charged at the vehicle area's chargers. A hired ground crew member opens the flap, carries the plug from its holster and plugs
// in; unplugs again when it is full or needed. You do the same by E. Never driven off plugged in; nothing is moved at once.
#include "GSE.h"
#include "BusCharge.h"
#include "BusCharger.h"
#include "BabylonModel.h"
#include "CableComponent.h"
#include "CrewPose.h"
#include "Puddles.h"
#include "ServiceVehicles.h"
#include "StaffPeople.h"
#include "Tugs.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPVehicleArea.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FChRules { double KmFull = 100., HoursFull = 14., ChargeHours = 1.5, KW = 150., Taper = 0.8, TaperEnd = 0.25, Low = 0.25, Full = 0.995, TopUp = 0.95, Reserve = 0.06, JobKm = 1.5, MildC = 19., Climate = 0.0016, Door = 0.00025, OwnMin = 0.55, OwnMax = 0.95, Limp = 220.; };
	const FChRules& ChRules()
	{
		static FChRules R; static bool bRead = false;
		if (bRead) return R;
		bRead = true;
		FString Text; TSharedPtr<FJsonObject> J; const TCHAR* F = TEXT("Data/bus-battery.json");
		if ((!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / F)) && !FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / F))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
		{
			ADP_WARN(TEXT("Bus battery: %s not found or not readable - the built-in values"), F); return R;
		}
		auto N = [&](const TCHAR* K, double& V) { J->TryGetNumberField(K, V); };
		N(TEXT("kmFull"), R.KmFull); N(TEXT("hoursFull"), R.HoursFull); N(TEXT("chargeHours"), R.ChargeHours); N(TEXT("kW"), R.KW); N(TEXT("taperFrom"), R.Taper); N(TEXT("taperEnd"), R.TaperEnd); N(TEXT("low"), R.Low); N(TEXT("full"), R.Full);
		N(TEXT("topUp"), R.TopUp); N(TEXT("reserve"), R.Reserve); N(TEXT("jobKm"), R.JobKm); N(TEXT("mildC"), R.MildC); N(TEXT("climatePerHourPerC"), R.Climate); N(TEXT("doorOpen"), R.Door); N(TEXT("ownMin"), R.OwnMin); N(TEXT("ownMax"), R.OwnMax); N(TEXT("limpCmS"), R.Limp);
		R.KmFull = FMath::Max(R.KmFull, 1.); R.HoursFull = FMath::Max(R.HoursFull, 0.1); R.ChargeHours = FMath::Max(R.ChargeHours, 0.01); R.Taper = FMath::Clamp(R.Taper, 0.1, 0.99); R.TaperEnd = FMath::Clamp(R.TaperEnd, 0.02, 1.);
		return R;
	}
	// the share of its full power it takes at a charge (1 up to the taper, then down in a straight line), and the rate per hour that
	// makes empty to full take just the hours of the table
	double ChPower(double S) { const FChRules& R = ChRules(); return S < R.Taper ? 1. : FMath::Lerp(1., R.TaperEnd, FMath::Clamp((S - R.Taper) / (1. - R.Taper), 0., 1.)); }
	double ChRate() { const FChRules& R = ChRules(); const double e = R.TaperEnd; return (R.Taper + (1. - R.Taper) * (e > 0.999 ? 1. : -FMath::Loge(e) / (1. - e))) / R.ChargeHours; }
	double ChLeftH(double S) { double H = 0.; for (double x = S; x < 1.; x += 0.005) H += 0.005 / (ChRate() * ChPower(x)); return H; }

	constexpr float ChReach = 470.f;      // (inlet to gland: the 5 m cable, slack kept)
	const FVector ChHold(16.5, 45.5, 105.), ChGland(-22., 46.5, 112.), ChBoot(-26.6, 0., -13.2), ChGrip(-11., 0., 5.4);   // (the charger's own cm; the boot and the grip: in the plug's, x the way it goes in)
	FTransform ChPlugX(const FVector& At, const FVector& X) { return FTransform(FRotationMatrix::MakeFromXZ(X, FVector::UpVector).ToQuat(), At); }

	// one per charger: its moving parts, cable, lamp and screen
	struct FChRig
	{
		TWeakObjectPtr<ABabylonModel> M; int32 GPlug = INDEX_NONE, GCable = INDEX_NONE; bool bShown = true;
		TWeakObjectPtr<UCableComponent> Cable; TWeakObjectPtr<UTextRenderComponent> Text; TWeakObjectPtr<UMaterialInstanceDynamic> Lamp; int32 Lit = -1; FString Said;
	};
	TMap<FString, FChRig>& ChRigs() { static TMap<FString, FChRig> S; return S; }
	FChRig* ChRigOf(UWorld* W, const FString& Bay)
	{
		ABabylonModel* M = Bay.IsEmpty() ? nullptr : ADPBusCharger::Of(W, Bay); if (!M) return nullptr;
		FChRig& R = ChRigs().FindOrAdd(Bay); if (R.M.Get() == M) return &R;
		R = FChRig(); R.M = M;
		R.GPlug = M->BindGroup({TEXT("Plug")}, false, ChPlugX(ChHold, FVector(0., -1., 0.)));
		R.GCable = M->BindGroup({TEXT("Cable")}, false, FTransform(ChGland));
		if (R.GPlug == INDEX_NONE || R.GCable == INDEX_NONE) ADP_WARN(TEXT("Bus charger %s: its plug or cable is not a part of the model"), *Bay);
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> G;
		UCableComponent* C = NewObject<UCableComponent>(M, *FString::Printf(TEXT("ChargeCable%s"), *Bay)); C->SetupAttachment(M->GetModelRoot());
		C->CableWidth = 3.6f; C->NumSegments = 32; C->NumSides = 10; C->SolverIterations = 16; C->SubstepTime = 0.01f; C->CollisionFriction = 0.9f; C->bEnableCollision = true; C->bEnableStiffness = true; C->CableLength = 480.f;
		C->SetRelativeLocation(ChGland);
		M->InstancesNamed(TEXT("Cable"), false, G); if (G.Num()) C->SetMaterial(0, G[0].Get<0>()->GetMaterial(0));
		C->SetVisibility(false); C->bAutoActivate = false; C->RegisterComponent(); M->AddInstanceComponent(C); C->SetActive(false); C->SetComponentTickEnabled(false);
		// (it meets only these: the ground, the posts and the cabinet - and nothing else does)
		C->SetCollisionObjectType(ECC_Destructible); C->SetCollisionResponseToAllChannels(ECR_Ignore); C->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
		auto Box = [&](const FVector& At, const FVector& Half)
		{
			UBoxComponent* B = NewObject<UBoxComponent>(M); B->SetupAttachment(M->GetModelRoot()); B->SetRelativeLocation(At); B->SetBoxExtent(Half); B->SetCollisionObjectType(ECC_Destructible);
			B->SetCollisionResponseToAllChannels(ECR_Ignore); B->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block); B->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			B->SetCanEverAffectNavigation(false); B->SetGenerateOverlapEvents(false); B->SetHiddenInGame(true); B->RegisterComponent(); M->AddInstanceComponent(B);
		};
		Box(FVector(150., 0., -8.), FVector(600., 400., 10.)); Box(FVector(0., 0., 7.5), FVector(37.5, 50., 7.5)); Box(FVector(0., 0., 108.), FVector(27., 41., 94.));
		for (const double Y : {-55., 55.}) Box(FVector(62., Y, 50.), FVector(7., 7., 50.));
		R.Cable = C;
		G.Reset(); M->InstancesNamed(TEXT("Lamp"), false, G);
		if (G.Num()) R.Lamp = G[0].Get<0>()->CreateDynamicMaterialInstance(0);
		UTextRenderComponent* X = NewObject<UTextRenderComponent>(M); X->SetupAttachment(M->GetModelRoot()); X->SetText(FText::GetEmpty()); X->SetHorizontalAlignment(EHTA_Center); X->SetVerticalAlignment(EVRTA_TextCenter);
		X->SetWorldSize(3.4f); X->SetTextRenderColor(FColor(190, 235, 205)); X->SetRelativeLocation(FVector(26.6, 0., 150.)); X->SetCollisionEnabled(ECollisionEnabled::NoCollision); X->SetCastShadow(false); X->RegisterComponent(); M->AddInstanceComponent(X);
		R.Text = X;
		ADP_LOG(TEXT("Bus charger %s: plug, cable, lamp and screen ready"), *Bay);
		return &R;
	}
	FTransform ChX(const FChRig& R) { return R.M.IsValid() && R.M->GetModelRoot() ? R.M->GetModelRoot()->GetComponentTransform() : FTransform::Identity; }
	void ChLook(FChRig& R, int32 Lit, const FString& Say)   // (0 free: green, 1 charging: blue, 2 full: white-green)
	{
		if (R.Lit != Lit && R.Lamp.IsValid())
		{
			const FLinearColor C = Lit == 1 ? FLinearColor(0.05f, 0.25f, 1.f) : Lit == 2 ? FLinearColor(0.7f, 1.f, 0.8f) : FLinearColor(0.05f, 0.9f, 0.15f);
			R.Lamp->SetVectorParameterValue(TEXT("BaseColorFactor"), C); R.Lamp->SetVectorParameterValue(TEXT("EmissiveFactor"), C); R.Lamp->SetScalarParameterValue(TEXT("EmissiveStrength"), 6.f); R.Lamp->SetScalarParameterValue(TEXT("EB"), 6.f);
		}
		R.Lit = Lit;
		if (R.Said != Say && R.Text.IsValid()) { R.Said = Say; R.Text->SetText(FText::FromString(Say)); }
	}
}

// ------------------------------------------------------------------ its parts

void AGSE::ChargeDress(FTractor& T)
{
	const float Had = T.Ch ? T.Ch->Soc : -1.f;
	T.Ch = MakeShared<FBusCharge>(); FBusCharge& C = *T.Ch; ABabylonModel* M = T.M.Get(); if (Had >= 0.f) C.Soc = Had;
	C.FlapPiv = SvcP(T, TEXT("PIV_ChargeFlap")); C.InletP = SvcP(T, TEXT("SOCK_Inlet"));
	T.Pt.Add(TEXT("BUS_ChFlap"), C.FlapPiv + FVector(14., 1., 0.)); T.Pt.Add(TEXT("BUS_ChPlug"), C.InletP);
	C.GFlap = M ? M->BindGroup({TEXT("ChargeFlap")}, false, FTransform(C.FlapPiv)) : INDEX_NONE;
	if (C.GFlap == INDEX_NONE) ADP_WARN(TEXT("Apron bus %s: its charging flap is not a part of the model"), *T.Name);
	// the percentage and the km left on the driver's display
	TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> G; if (M) M->InstancesNamed(TEXT("DashCluster"), false, G);
	if (G.Num() && G[0].Get<0>()->GetStaticMesh())
	{
		FTransform In; G[0].Get<0>()->GetInstanceTransform(G[0].Get<1>(), In, true);
		const FBox B = G[0].Get<0>()->GetStaticMesh()->GetBoundingBox().TransformBy(In.GetRelativeTransform(M->GetModelRoot()->GetComponentTransform()));
		UTextRenderComponent* X = NewObject<UTextRenderComponent>(M); X->SetupAttachment(M->GetModelRoot()); X->SetText(FText::GetEmpty()); X->SetHorizontalAlignment(EHTA_Center); X->SetVerticalAlignment(EVRTA_TextCenter);
		X->SetWorldSize(2.2f); X->SetTextRenderColor(FColor(150, 230, 170)); X->SetRelativeLocationAndRotation(FVector(B.Min.X - 0.4, B.GetCenter().Y, B.Max.Z - 2.5), FRotator(0.f, 180.f, 0.f));
		X->SetCollisionEnabled(ECollisionEnabled::NoCollision); X->SetCastShadow(false); X->RegisterComponent(); M->AddInstanceComponent(X); C.Read = X;
	}
	else ADP_WARN(TEXT("Apron bus %s: no display for its battery"), *T.Name);
}

// its battery from the career (GSEBreak.cpp BreakLoad): -1 none yet
void AGSE::ChargeLoad(FTractor& T, double Soc)
{
	if (!T.Ch) return; const FChRules& R = ChRules();
	T.Ch->Soc = float(Soc >= 0. ? FMath::Clamp(Soc, 0., 1.) : T.bSpare ? 1. : FMath::Lerp(R.OwnMin, R.OwnMax, (GetTypeHash(T.BrkId) % 997) / 996.));
	T.Ch->bHasSoc = true; T.Ch->LastClock = FDateTime(); T.Ch->bBayRead = false; T.Ch->NotBefore = FDateTime();
	ADP_LOG(TEXT("Apron bus %s: battery %.0f %%, %.0f km"), *T.Name, T.Ch->Soc * 100.f, T.Ch->Soc * R.KmFull);
}

double AGSE::ChargeSoc(const FTractor& T) const { return T.Ch ? double(T.Ch->Soc) : -1.; }
bool AGSE::ChargeHolds(const FTractor& T) const { return T.Ch && T.Ch->Holds(); }
bool AGSE::ChargeEmpty(const FTractor& T) const { return T.Ch && T.Ch->Soc <= 0.f; }
float AGSE::ChargeLimp() { return float(ChRules().Limp); }
bool AGSE::ChargeLow(const FTractor& T) const { return T.Ch && (T.Ch->Soc < ChRules().Low || T.Ch->bGo); }
// enough for a way out of so many cm, its passengers' way, and back
bool AGSE::ChargeFor(const FTractor& T, double Cm) const { const FChRules& R = ChRules(); return !T.Ch || T.Ch->Soc > R.Reserve + 1.25 * (2. * Cm / 1e5 + R.JobKm) / R.KmFull; }
FVector AGSE::ChargeBay(const FTractor& T, float& Yaw) const { const bool b = T.Ch && T.Ch->bGo; Yaw = b ? T.Ch->BayYaw : T.HomeYaw; return b ? T.Ch->BayAt : T.Home; }
bool AGSE::ChargeAway(const FTractor& T) const { return T.Ch && !T.Ch->Bay.IsEmpty() && FVector::Dist2D(T.At, T.Ch->BayAt) < 250.f; }

// somebody wants it: its crew unplug it first
void AGSE::ChargeNeed(const FTractor& T) const
{
	if (!T.Ch || T.Ch->bNeeded) return;
	T.Ch->bNeeded = true; T.Ch->Wait = 0.f;
	ADP_LOG(TEXT("Apron bus %s: needed while it charges (%.0f %%) - to be unplugged first"), *T.Name, T.Ch->Soc * 100.f);
}

// the charger its inlet is in reach of; "" none
FString AGSE::ChargerBy(const FTractor& T) const
{
	if (!T.Ch || FMath::Abs(T.Speed) > 1.f) return FString();
	const FVector P = TW(T, T.Ch->InletP); FString Out; double Best = ChReach;
	for (const ADPVehicleArea::FCharger& Q : ADPVehicleArea::Chargers())
		if (const ABabylonModel* M = ADPBusCharger::Of(GetWorld(), Q.Bay)) { const double D = FVector::Dist(M->GetActorTransform().TransformPosition(ChGland), P); if (D < Best) { Best = D; Out = Q.Bay; } }
	if (Out.IsEmpty()) return Out;
	for (const FTractor& O : Tractors) if (&O != &T && O.Ch && O.Ch->Plug != FBusCharge::Holster && O.Ch->Cable == Out) return FString();   // (its plug is another's)
	return Out;
}

// ------------------------------------------------------------------ each frame: the flap, the plug, the cable

void AGSE::ChargeParts(FTractor& T, float Dt)
{
	FBusCharge& C = *T.Ch; ABabylonModel* M = T.M.Get(); if (!M) return;
	C.FlapNow = FMath::FInterpConstantTo(C.FlapNow, C.FlapWant, Dt, 1.1f);
	if (C.GFlap != INDEX_NONE && C.FlapNow != C.FlapWas)
	{
		C.FlapWas = C.FlapNow; M->MoveGroup(C.GFlap, FTransform(FRotator(0.f, 110.f * FMath::SmoothStep(0.f, 1.f, C.FlapNow), 0.f), C.FlapPiv)); M->FlushMoves();
	}
	FChRig* R = ChRigOf(GetWorld(), C.Cable); if (!R || !R->M.IsValid()) return;
	const FTransform X = ChX(*R); const FVector Hol = X.TransformPosition(ChHold), HolX = X.TransformVectorNoScale(FVector(0., -1., 0.));
	const FVector In = TW(T, C.InletP), InX = FRotator(0.f, T.Yaw, 0.f).RotateVector(FVector(0., -1., 0.));
	FVector Want = Hol, WantX = HolX; float Fast = 12.f;
	if (C.Plug == FBusCharge::CrewHand)
	{
		ATugs* G = Tg(); const USkeletalMeshComponent* B = G && G->Crew.IsValidIndex(C.Ci) ? Body(C.Ci) : nullptr;
		if (B)
		{
			// its nose the way they look, in their right hand
			const FVector F = G->Crew[C.Ci].A.IsValid() ? G->Crew[C.Ci].A->GetActorForwardVector().GetSafeNormal2D() : InX;
			WantX = F; Want = B->GetBoneLocation(TEXT("hand_r")) + F * 18.; Want.Z -= 8.; Fast = 30.f;
			if (FVector::Dist(C.PlugAt, Hol) < 60. || FVector::Dist(C.PlugAt, In) < 60.) Fast = 10.f;
		}
	}
	else if (C.Plug == FBusCharge::YourHand) { Want = FTransform(FRotator(0.f, T.Yaw, 0.f), T.At).TransformPosition(T.YouHand); WantX = (In - Want).GetSafeNormal2D(); if (FVector::Dist2D(Want, Hol) < FVector::Dist2D(Want, In)) WantX = HolX; Fast = 20.f; }
	else if (C.Plug == FBusCharge::Inlet) { Want = In; WantX = InX; }
	else if (C.Plug == FBusCharge::Dropped) { Want = FVector(C.PlugAt.X, C.PlugAt.Y, T.At.Z + 6.); WantX = C.PlugX.GetSafeNormal2D(); Fast = 9.f; if (WantX.IsNearlyZero()) WantX = InX; }   // (straight down)
	if (C.PlugAt.IsZero()) { C.PlugAt = Hol; C.PlugX = HolX; }
	C.PlugAt = FMath::VInterpTo(C.PlugAt, Want, Dt, Fast); C.PlugX = FMath::VInterpTo(C.PlugX, WantX, Dt, 8.f).GetSafeNormal(); if (C.PlugX.IsNearlyZero()) C.PlugX = WantX;
	const bool bHome = C.Plug == FBusCharge::Holster && FVector::Dist(C.PlugAt, Hol) < 1.f;
	if (bHome) { C.PlugAt = Hol; C.PlugX = HolX; }
	const FTransform P = ChPlugX(C.PlugAt, C.PlugX);
	if (R->GPlug != INDEX_NONE) R->M->MoveGroup(R->GPlug, P.GetRelativeTransform(X));
	if (R->bShown != bHome && R->GCable != INDEX_NONE) { R->bShown = bHome; R->M->MoveGroup(R->GCable, FTransform(FQuat::Identity, ChGland, FVector(bHome ? 1. : 0.001))); }
	R->M->FlushMoves();
	if (UCableComponent* K = R->Cable.Get())
	{
		K->EndLocation = K->GetComponentTransform().InverseTransformPosition(P.TransformPosition(ChBoot));
		K->CableLength = FMath::Max(480.f, float(FVector::Dist(K->GetComponentLocation(), P.TransformPosition(ChBoot))) * 1.04f);
		if (K->IsVisible() == bHome) { K->SetVisibility(!bHome); K->SetActive(!bHome); K->SetComponentTickEnabled(!bHome); }
	}
	if (bHome && C.Stage == FBusCharge::Idle) { ChLook(*R, 0, TEXT("READY")); C.Cable.Reset(); C.PlugAt = FVector::ZeroVector; }
}

// ------------------------------------------------------------------ its battery and what it does about it

void AGSE::ChargeTick(float Dt)
{
	UADPSim* Sim = UADPSim::Get(this); if (!Sim) return;
	for (FTractor& T : Tractors) if (T.Ch && T.M.IsValid() && !T.bAway) ChargeParts(T, Dt);
	if ((ChargePoll -= Dt) > 0.f) return;
	const float Step = 1.f - ChargePoll; ChargePoll = 1.f;
	ADP_PERF_SCOPE("GSE: bus batteries", 2.f);
	const FChRules& R = ChRules(); const FDateTime Now = Sim->Now(); ATugs* G = Tg();
	const double Air = APuddles::Get() ? double(APuddles::Get()->GetAirTemp()) : R.MildC;
	for (int32 Ti = 0; Ti < Tractors.Num(); ++Ti)
	{
		FTractor& T = Tractors[Ti]; if (!T.Ch || !T.M.IsValid() || T.bAway) continue;
		FBusCharge& C = *T.Ch; const float Was = C.Soc;
		if (C.LastClock.GetTicks() == 0 || C.LastClock > Now) { C.LastClock = Now; C.LastAt = T.At; }
		const double H = FMath::Min((Now - C.LastClock).GetTotalHours(), 6.), Km = FVector::Dist2D(T.At, C.LastAt) / 1e5; C.LastClock = Now; C.LastAt = T.At;
		uint8 Open = 0; for (int32 d = 0; d < T.SvcDoors.Num() && d < 8; ++d) if (T.SvcDoors[d].Now > 0.5f) Open |= 1 << d;
		const uint8 Fresh = Open & ~C.Doors; C.Doors = Open;
		double Use = Km < 0.2 ? Km / R.KmFull : 0.;   // (further in a second: a load, not driven)
		if (T.bOn) { if (Km < 1e-5) Use += H / R.HoursFull; Use += H * R.Climate * FMath::Max(0., FMath::Abs(Air - R.MildC) - 2.); }
		Use += R.Door * FMath::CountBits(Fresh);
		C.Soc = FMath::Clamp(C.Soc - float(Use), 0.f, 1.f);
		const bool bIn = C.Plug == FBusCharge::Inlet && (C.Stage == FBusCharge::Idle || C.Stage == FBusCharge::Unplugging);
		if (bIn && C.Stage == FBusCharge::Idle)
		{
			for (double Lh = H; Lh > 0. && C.Soc < 1.f; Lh -= 0.02) C.Soc = FMath::Min(1.f, C.Soc + float(FMath::Min(Lh, 0.02) * ChRate() * ChPower(C.Soc)));
			if (Was < R.Full && C.Soc >= R.Full) ADP_LOG(TEXT("Apron bus %s: fully charged"), *T.Name);
		}
		if (Was > 0.f && C.Soc <= 0.f) ADP_WARN(TEXT("Apron bus %s: its battery is empty - it creeps on"), *T.Name);
		if (Was >= R.Low && C.Soc < R.Low) ADP_LOG(TEXT("Apron bus %s: battery below %.0f %% - to a charger at the next chance"), *T.Name, R.Low * 100.);
		if (FChRig* Rg = ChRigOf(GetWorld(), C.Cable))
		{
			const bool bFull = C.Soc >= R.Full; const int32 Min = FMath::CeilToInt(ChLeftH(C.Soc) * 60.);
			if (bIn) ChLook(*Rg, bFull ? 2 : 1, bFull ? FString(TEXT("FULL  100 %")) : FString::Printf(TEXT("CHARGING  %.0f %%<br>%.0f kW   %d:%02d"), C.Soc * 100.f, R.KW * ChPower(C.Soc), Min / 60, Min % 60).Replace(TEXT("<br>"), LINE_TERMINATOR));
			else ChLook(*Rg, 0, C.Plug == FBusCharge::Holster ? TEXT("READY") : TEXT("CONNECT"));
		}
		const bool bHum = bIn && C.Stage == FBusCharge::Idle && C.Soc < R.Full;
		if (bHum && !C.Hum.IsValid()) { if (UAudioComponent* A = SndOn(T, TEXT("ABMotorWhineLoop"), C.InletP + FVector(-200., 0., 60.), 0.22f, true)) { A->SetPitchMultiplier(0.55f); C.Hum = A; } }
		else if (!bHum && C.Hum.IsValid()) { C.Hum->FadeOut(0.8f, 0.f); C.Hum.Reset(); }
		if (const int32 Pc = FMath::CeilToInt(C.Soc * 100.f); Pc != C.Shown && C.Read.IsValid())
		{
			C.Shown = Pc; C.Read->SetText(FText::FromString(FString::Printf(TEXT("%d %%   %.0f km"), Pc, C.Soc * R.KmFull))); C.Read->SetTextRenderColor(Pc < R.Low * 100. ? FColor(255, 120, 60) : FColor(150, 230, 170));
		}

		// --- its crew
		if (C.Stage != FBusCharge::Idle)
		{
			if (!G || !G->Crew.IsValidIndex(C.Ci) || !G->Crew[C.Ci].A.IsValid() || G->Crew[C.Ci].bSlotFree)
			{
				ADP_WARN(TEXT("Apron bus %s: who handled its plug is gone - the plug is let go of, asked again"), *T.Name);
				if (C.Plug == FBusCharge::CrewHand) C.Plug = FBusCharge::Dropped;
				C.Stage = FBusCharge::Idle; C.Ci = INDEX_NONE; if (C.Plug == FBusCharge::Holster) C.FlapWant = 0.f;
			}
			continue;
		}
		// its own place, when that has a charger
		if (!C.bBayRead)
		{
			C.bBayRead = true; C.bOwnBay = false;
			for (const ADPVehicleArea::FPlace& P : ADPVehicleArea::Places()) if (P.bCharge && FVector::Dist2D(Plan(P.X, P.Y), T.Home) < 200.f) { C.bOwnBay = true; C.BayName = P.Name; }
		}
		if (C.bGo && (T.Player.IsValid() || (BusStage(T) == 0 && T.Driver == INDEX_NONE && !ChargeAway(T)))) { C.bGo = false; C.Bay.Reset(); ADP_LOG(TEXT("Apron bus %s: does not go to charge now"), *T.Name); }
		if (BusStage(T) == 0) C.bHoming = false;
		if (C.Plug == FBusCharge::Dropped)
		{
			// (put back when nobody sees it, else carried back by the ground crew when it lies where they can walk: between the bus and
			// the next one, or behind it; you can pick it up too)
			const FChRig* Rg = ChRigOf(GetWorld(), C.Cable);
			if (!Rg || (TaxiUnseen(C.PlugAt) && TaxiUnseen(ChX(*Rg).TransformPosition(ChHold)))) { C.Plug = FBusCharge::Holster; C.PlugAt = FVector::ZeroVector; C.FlapWant = 0.f; ADP_LOG(TEXT("Apron bus %s: the dropped plug is in its holster again (unseen)"), *T.Name); continue; }
			if ((C.Wait -= Step) > 0.f) continue;
			C.Wait = 10.f; FString No; const FVector L = FTransform(FRotator(0.f, T.Yaw, 0.f), T.At).InverseTransformPosition(C.PlugAt);
			if (L.Y < 150. || L.Y > 330. || L.X < -900. || L.X > 400.) No = TEXT("it lies where they cannot walk");
			else if (ChargeCrew(Ti, false, No)) continue;
			if (GetWorld()->GetTimeSeconds() > C.SaidT) { C.SaidT = GetWorld()->GetTimeSeconds() + 60.f; ADP_LOG(TEXT("Apron bus %s: its charging plug lies on the ground: %s"), *T.Name, *No); }
			continue;
		}
		if (T.Player.IsValid() || T.Brk > 1 || T.Path.Num() || T.Until) continue;
		if (!C.Bay.IsEmpty() && !C.bGo && !ChargeAway(T)) { C.Bay.Reset(); }   // (it has left the bay it was lent)
		if ((C.Wait -= Step) > 0.f) continue;
		const bool bBay = ChargeAway(T) || (C.bOwnBay && FVector::Dist2D(T.At, T.Home) < 150.f);
		auto Says = [&](const FString& W) { if (W != C.Why || GetWorld()->GetTimeSeconds() > C.SaidT) { C.Why = W; C.SaidT = GetWorld()->GetTimeSeconds() + 60.f; ADP_LOG(TEXT("Apron bus %s (%.0f %%): %s"), *T.Name, C.Soc * 100.f, *W); } C.Wait = 10.f; };
		FString Why;
		if (C.Plug == FBusCharge::Inlet)
		{
			const bool bFull = C.Soc >= R.Full; if (!bFull && !C.bNeeded) continue;
			if (!ChargeCrew(Ti, false, Why)) Says(FString::Printf(TEXT("%s, waits to be unplugged: %s"), bFull ? TEXT("full") : TEXT("needed"), *Why));
			continue;
		}
		if (C.Plug != FBusCharge::Holster) continue;   // (in your hand)
		const APlayerController* Pc = GetWorld()->GetFirstPlayerController(); const bool bYou = Pc && Pc->GetPawn() && FVector::Dist2D(Pc->GetPawn()->GetActorLocation(), TW(T, C.InletP)) < 800.;
		if (BusStage(T) != 0 || T.Job != EJob::None) continue;
		if (bBay && C.Soc < R.TopUp && Now >= C.NotBefore)
		{
			if (ChargerBy(T).IsEmpty()) { Says(TEXT("no charger in reach of its inlet, or its plug is taken")); continue; }
			if (!ChargeCrew(Ti, true, Why)) Says(FString::Printf(TEXT("waits to be plugged in: %s"), *Why));
			continue;
		}
		if (C.Holds()) { if (!C.Cable.Len() && C.FlapWant > 0.f && C.FlapNow > 0.9f && !bYou) { C.FlapWant = 0.f; ADP_LOG(TEXT("Apron bus %s: its charging flap springs shut, nobody is at it"), *T.Name); } continue; }
		if (ChargeAway(T) && Now >= C.NotBefore && C.Soc >= R.TopUp)   // (charged: back to its own place, the bay free for the next)
		{
			if (!BusHome(Ti, [this, Ti] { if (Tractors[Ti].Ch) Tractors[Ti].Ch->Bay.Reset(); BusLeave(Ti); }, Why)) Says(FString::Printf(TEXT("charged, waits to go home: %s"), *Why)); else C.Wait = 20.f;
			continue;
		}
		if (!bBay && !C.bGo && C.Soc < R.Low)
		{
			if (!ChargeGo(Ti, Why)) Says(FString::Printf(TEXT("stays on its place, not charged yet: %s"), *Why)); else C.Wait = 20.f;
		}
	}
}

// a free charging bay, and off to it
bool AGSE::ChargeGo(int32 Ti, FString& Why)
{
	FTractor& T = Tractors[Ti]; FBusCharge& C = *T.Ch; if (!ADPVehicleArea::Built()) { Why = TEXT("no vehicle area"); return false; }
	FString Bay; FVector At; float Yaw = 0.f;
	for (const ADPVehicleArea::FPlace& P : ADPVehicleArea::Places())
	{
		if (!P.bCharge || P.For != TEXT("bus")) continue;
		const FVector W = Plan(P.X, P.Y); bool bFree = false;
		for (const ADPVehicleArea::FCharger& Q : ADPVehicleArea::Chargers()) if (FVector::Dist2D(Plan(Q.X, Q.Y), W) < 1200.f && FMath::Abs(Q.X - P.X) < 2.5f && ADPBusCharger::Of(GetWorld(), Q.Bay)) bFree = true;
		for (const FTractor& O : Tractors) if (&O != &T && !O.bAway && O.M.IsValid() && (FVector::Dist2D(O.Home, W) < 300.f || FVector::Dist2D(O.At, W) < 800.f || (O.Ch && O.Ch->Bay.Len() && FVector::Dist2D(O.Ch->BayAt, W) < 300.f))) bFree = false;
		if (!bFree) continue;
		Bay = P.Name; At = W; const float a = FMath::DegreesToRadians(P.Heading); Yaw = FMath::RadiansToDegrees(FMath::Atan2(-FMath::Sin(a), FMath::Cos(a))); break;
	}
	// (none: the bay of a bus that is out at work - the two change places, the other comes home to this one's)
	const int32 Ow = Bay.IsEmpty() ? ChargeOwner(Ti) : INDEX_NONE;
	if (Ow != INDEX_NONE) { Bay = Tractors[Ow].Ch->BayName; At = Tractors[Ow].Home; Yaw = Tractors[Ow].HomeYaw; }
	if (Bay.IsEmpty()) { Why = TEXT("every charging bay is taken"); return false; }
	const int32 Ord = ++BusOrder.FindOrAdd(Ti);
	if (!BusMan(Ti, [this, Ti, Ord] { if (BusOrder.FindRef(Ti) == Ord) ChargeDrive(Ti); else if (Tractors[Ti].Ch) Tractors[Ti].Ch->bGo = false; }, Why)) return false;
	C.Bay = Bay; C.BayAt = At; C.BayYaw = Yaw; C.bGo = true;
	ADP_LOG(TEXT("Apron bus %s: battery %.0f %% - goes to charge at %s"), *T.Name, C.Soc * 100.f, *Bay);
	return true;
}

// its own place, or the one it just gave away
bool AGSE::BusInPlace(const FTractor& T) const { return FVector::Dist2D(T.At, T.Home) < 200.f || (T.Ch && FVector::Dist2D(T.At, T.Ch->WasHome) < 200.f); }

// The three bays are shared (to-do 300): one of the airport's own buses whose place is a charging bay, out with passengers and not
// on its way home. Never a bought one's: that place is its owner's.
int32 AGSE::ChargeOwner(int32 Ti) const
{
	const FTractor& T = Tractors[Ti]; if (T.bSpare || !T.Ch || DepotAt(T.Home)) return INDEX_NONE;
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		const FTractor& O = Tractors[i];
		if (i == Ti || !O.Ch || !O.Bu || O.bSpare || O.bAway || !O.M.IsValid() || O.Brk || O.Player.IsValid() || !O.Ch->bOwnBay || O.Ch->BayName.IsEmpty() || BusStage(O) != 2 || O.Ch->bGo || O.Ch->bHoming || DepotAt(O.At) || O.Ch->Soc < ChRules().Low) continue;
		bool bFree = false;
		for (const ADPVehicleArea::FCharger& Q : ADPVehicleArea::Chargers()) if (Q.Bay == O.Ch->BayName && ADPBusCharger::Of(GetWorld(), Q.Bay)) bFree = true;
		for (const FTractor& V : Tractors) if (&V != &T && &V != &O && !V.bAway && V.M.IsValid() && (FVector::Dist2D(V.At, O.Home) < 800.f || (V.Ch && V.Ch->Bay.Len() && FVector::Dist2D(V.Ch->BayAt, O.Home) < 300.f))) bFree = false;
		if (bFree) return i;
	}
	return INDEX_NONE;
}

// which free bus goes to the passengers: not one with a low battery when another is there; while one waits for a bay, the full one
// that stands in a bay (so it leaves it)
void AGSE::ChargePick(TArray<int32>& Free) const
{
	const FChRules& R = ChRules(); bool bWaits = false;
	for (const FTractor& T : Tractors) if (T.Ch && T.Bu && !T.bAway && T.M.IsValid() && !T.bSpare && !DepotAt(T.At) && BusStage(T) == 0 && T.Ch->Soc < R.Low) bWaits = true;
	auto Keep = [&](TFunctionRef<bool(const FTractor&)> Is) { if (Free.ContainsByPredicate([&](int32 i) { return Is(Tractors[i]); })) Free.RemoveAll([&](int32 i) { return !Is(Tractors[i]); }); };
	Keep([&](const FTractor& T) { return !T.Ch || T.Ch->Soc >= R.Low; });
	if (bWaits) Keep([&](const FTractor& T) { return T.Ch && T.Ch->bOwnBay && T.Ch->Soc >= R.Full && DepotAt(T.At); });
}

void AGSE::ChargeDrive(int32 Ti)
{
	{
		FTractor& T = Tractors[Ti]; FBusCharge& C = *T.Ch;
		if (FTractor* O = Tractors.FindByPredicate([&](const FTractor& X) { return &X != &T && !X.bAway && X.M.IsValid() && X.Svc == T.Svc && FVector::Dist2D(X.Home, C.BayAt) < 300.f; }))
		{
			C.Bay.Reset();   // (its own hold on the bay does not count)
			if (ChargeOwner(Ti) != int32(O - Tractors.GetData()))
			{
				ADP_LOG(TEXT("Apron bus %s: %s comes home to its charging bay - stays"), *T.Name, *O->Name);
				C.bGo = false; C.Wait = 30.f; BusLeave(Ti); return;
			}
			ADP_LOG(TEXT("Apron bus %s: changes places with %s (out at work)"), *T.Name, *O->Name);
			C.Bay = O->Ch->BayName; C.WasHome = T.Home; Swap(T.Home, O->Home); Swap(T.HomeYaw, O->HomeYaw); C.bBayRead = O->Ch->bBayRead = false; O->Ch->BayName.Reset(); O->Ch->bOwnBay = false;
		}
	}
	BusSetAll(Ti, false); Motor(Tractors[Ti], true);
	SpareHome(Ti, [this, Ti]
	{
		FTractor& U = Tractors[Ti]; if (!U.Ch) return;
		U.Ch->bGo = false; U.Ch->Wait = 4.f; Motor(U, false); BusLeave(Ti);
		ADP_LOG(TEXT("Apron bus %s: in the charging bay %s"), *U.Name, *U.Ch->Bay);
		U.Ch->WasHome = FVector(1e9, 0., 0.); if (FVector::Dist2D(U.Home, U.Ch->BayAt) < 100.f) U.Ch->Bay.Reset();   // (its own place now)
	});
}

// ------------------------------------------------------------------ a hired ground crew member plugs it in, or out

bool AGSE::ChargeCrew(int32 Ti, bool bPlug, FString& Why)
{
	FTractor& T = Tractors[Ti]; FBusCharge& C = *T.Ch; ATugs* G = Tg(); UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
	if (!G || !Sp) { Why = TEXT("no crew yet"); return false; }
	if (!Sp->HiresRole(TEXT("ground"))) { Why = TEXT("no ground crew is hired"); return false; }
	const FString Cab = bPlug ? ChargerBy(T) : C.Cable; FChRig* R = ChRigOf(GetWorld(), Cab); if (!R) { Why = TEXT("no charger in reach"); return false; }
	TArray<UStaffPeople::FTaken> Team; const FString Key = TEXT("charge:") + T.BrkId;
	if (!Sp->AskTeam(Key, EStaffTask::BusCharge, FDateTime(2000, 1, 1), T.At, T.Name, Team) || !Team.Num()) { Why = TEXT("nobody of the ground crew is free"); return false; }
	const int32 Ci = G->Enlist(Team[0].A.Get(), Team[0].Id, UStaffPeople::DutyName(Team[0].Duty));
	if (Ci == INDEX_NONE) { Why = TEXT("no crew place"); return false; }
	G->Crew[Ci].bGse = true; C.Ci = Ci; C.Stage = bPlug ? FBusCharge::Plugging : FBusCharge::Unplugging; C.Cable = Cab;
	ADP_LOG(TEXT("Apron bus %s: %s goes to %s"), *T.Name, *G->Crew[Ci].Name, bPlug ? TEXT("plug it in") : TEXT("unplug it"));
	HiredWay(Ci, T.At, 0.f);
	// round its nose to its right side, up the middle of the gap to its flap
	const FTransform X = ChX(*R); const FVector StW = X.TransformPosition(FVector(25., 115., 0.)), Fl(-645., 215., 0.);
	auto At = [this, Ci, Ti](const FVector& L, float Stop) { WalkTo(Ci, [this, Ti, L] { return TW(Tractors[Ti], L); }, Stop); };
	auto AtPlug = [this, Ci, StW] { WalkTo(Ci, [StW] { return StW; }, 10.f); };
	// a hand to a place (world) and back, what it does done while it is there
	auto Hand = [this, Ci](TFunction<FVector()> Feet, TFunction<FVector()> To, bool bLow, float S, TFunction<void()> Fn)
	{
		auto Ps = [Feet, To, bLow](float E, float PH, FDriverPose& P)
		{
			const FVector A = Feet(), H = To(), F = (H - A).GetSafeNormal2D(); P = bLow ? ADPPose::Stoop(A, F, 1, H, 0.9f, PH, E) : ADPPose::Reach(A, F, 1, H, 0.9f, PH, E);
		};
		PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(FMath::SmoothStep(0.f, 1.f, K), PH, P); }, S * 0.5f);
		Do(Ci, MoveTemp(Fn));
		PoseAct(Ci, [Ps](float K, float PH, FDriverPose& P) { Ps(1.f - FMath::SmoothStep(0.f, 1.f, K), PH, P); }, S * 0.5f);
	};
	auto FlFeet = [this, Ti, Fl] { return TW(Tractors[Ti], Fl); }; auto PlFeet = [StW] { return StW; };
	auto FlapP = [this, Ti] { const FBusCharge& Q = *Tractors[Ti].Ch; return TW(Tractors[Ti], Q.FlapPiv + FRotator(0.f, 110.f * Q.FlapNow, 0.f).RotateVector(FVector(14., 1., 0.))); };
	auto InP = [this, Ti] { const FTractor& Q = Tractors[Ti]; return TW(Q, Q.Ch->InletP + FVector(0., -ChGrip.X, ChGrip.Z)); };
	const FVector HolP = X.TransformPosition(ChHold + FVector(0., -ChGrip.X, ChGrip.Z));
	auto Flap = [this, Ti](bool bOpen) { FTractor& Q = Tractors[Ti]; Q.Ch->FlapWant = bOpen ? 1.f : 0.f; ADP_LOG(TEXT("Apron bus %s: charging flap %s"), *Q.Name, bOpen ? TEXT("opened") : TEXT("shut")); };
	auto Set = [this, Ti, Ci](uint8 P, const TCHAR* Say) { FTractor& Q = Tractors[Ti]; if (Q.Ch->Ci != Ci) return; Q.Ch->Plug = P; ADP_LOG(TEXT("Apron bus %s: %s (%.0f %%)"), *Q.Name, Say, Q.Ch->Soc * 100.f); };
	At(FVector(950., 280., 0.), 60.f); At(FVector(-540., 280., 0.), 40.f); At(Fl, 10.f);
	if (bPlug)
	{
		Hand(FlFeet, FlapP, true, 1.6f, [Flap] { Flap(true); });
		WaitFor(Ci, [this, Ti](float E) { return Tractors[Ti].Ch->FlapNow > 0.98f || E > 4.f; });
		AtPlug();
		Hand(PlFeet, [HolP] { return HolP; }, false, 1.8f, [Set] { Set(FBusCharge::CrewHand, TEXT("the plug is out of its holster")); });
		At(Fl, 10.f);
		Hand(FlFeet, InP, true, 2.4f, [Set] { Set(FBusCharge::Inlet, TEXT("plugged in")); });
		Wait(Ci, 1.5f);   // (the lamp and the screen looked at)
	}
	else
	{
		if (C.Plug == FBusCharge::Dropped)   // (picked up off the ground, from beside it)
		{
			const FVector Pl = C.PlugAt + FVector(0., 0., ChGrip.Z), L = FTransform(FRotator(0.f, T.Yaw, 0.f), T.At).InverseTransformPosition(Pl);
			const FVector Ft(L.X + 45., FMath::Clamp(L.Y, 200., 290.), 0.);
			At(Ft, 10.f);
			Hand([this, Ti, Ft] { return TW(Tractors[Ti], Ft); }, [Pl] { return Pl; }, true, 2.4f, [Set] { Set(FBusCharge::CrewHand, TEXT("the dropped plug is picked up")); });
		}
		else Hand(FlFeet, InP, true, 2.4f, [Set] { Set(FBusCharge::CrewHand, TEXT("unplugged")); });
		AtPlug();
		Hand(PlFeet, [HolP] { return HolP; }, false, 1.8f, [Set] { Set(FBusCharge::Holster, TEXT("the plug is in its holster")); });
		At(Fl, 10.f);
		Hand(FlFeet, FlapP, true, 1.6f, [Flap] { Flap(false); });
		WaitFor(Ci, [this, Ti](float E) { return Tractors[Ti].Ch->FlapNow < 0.02f || E > 4.f; });
	}
	At(FVector(-540., 280., 0.), 40.f); 
	Do(Ci, [this, Ti, Ci, bPlug]
	{
		FTractor& Q = Tractors[Ti]; FBusCharge& S = *Q.Ch; if (S.Ci != Ci) return;
		S.Ci = INDEX_NONE; S.Stage = FBusCharge::Idle;
		if (!bPlug) { if (S.bNeeded) if (const UADPSim* Sm = UADPSim::Get(this)) S.NotBefore = Sm->Now() + FTimespan::FromMinutes(15.); S.bNeeded = false; }
	});
	At(FVector(950., 280., 0.), 60.f);
	// (out of the gap by its nose, then the way they came)
	Do(Ci, [this, Ci] { Later([this, Ci] { GiveBackHired(Ci, 0.f); }); });
	WaitFor(Ci, [](float E) { return E > 0.5f; });
	return true;
}

// ------------------------------------------------------------------ you, by E

namespace { constexpr int32 ChSwFlap = 2, ChSwHolster = 3, ChSwInlet = 4; }

bool AGSE::ChargeSpot(const FTractor& T, int32 Code, FVector& Out) const
{
	if (!T.Ch || Code < ChSwFlap || Code > ChSwInlet) return false;
	const FBusCharge& C = *T.Ch;
	if (Code == ChSwFlap) { Out = TW(T, C.FlapPiv + FRotator(0.f, 110.f * C.FlapNow, 0.f).RotateVector(FVector(14., 1., 0.))); return true; }
	if (Code == ChSwInlet) { Out = TW(T, C.InletP + FVector(0., C.Plug == FBusCharge::Inlet ? 11. : 2., 0.)); return true; }
	if (C.Plug == FBusCharge::Dropped) { Out = C.PlugAt; return true; }
	const FString B = C.Cable.Len() ? C.Cable : ChargerBy(T); const ABabylonModel* M = ADPBusCharger::Of(GetWorld(), B);
	Out = M && M->GetModelRoot() ? M->GetModelRoot()->GetComponentTransform().TransformPosition(ChHold + FVector(0., 11., 5.)) : T.At + FVector(0., 0., -5000.);
	return true;
}

FString AGSE::ChargeSay(const FTractor& T, int32 Code) const
{
	if (!T.Ch || T.YouOn) return FString();
	const FBusCharge& C = *T.Ch; using B = FBusCharge;
	if (Code == ChSwFlap) return C.Plug == B::YourHand || C.Plug == B::Inlet ? FString() : C.FlapWant > 0.5f ? TEXT("Shut the charging flap") : TEXT("Open the charging flap");
	if (Code == ChSwHolster && C.Plug == B::Dropped) return TEXT("Pick up the charging plug");
	if (Code == ChSwHolster) return C.Plug == B::YourHand ? TEXT("Put the charging plug in its holster") : C.Plug == B::Holster && ChargerBy(T).Len() ? FString::Printf(TEXT("Take the charging plug (battery %.0f %%)"), C.Soc * 100.f) : FString();
	if (C.Plug == B::Inlet) return FString::Printf(TEXT("Unplug the bus (battery %.0f %%, %.0f km)"), C.Soc * 100.f, C.Soc * ChRules().KmFull);
	return C.Plug == B::YourHand ? TEXT("Plug the bus in") : FString();
}

bool AGSE::ChargeMay(const FTractor& T, int32 Code, FString& Why) const
{
	if (!T.Ch) return false;
	const FBusCharge& C = *T.Ch;
	if (C.Stage != FBusCharge::Idle) Why = TEXT("the ground crew are at it");
	else if (FMath::Abs(T.Speed) > 1.f || T.Path.Num()) Why = TEXT("the bus is not standing");
	else if (Code == ChSwInlet && C.Plug == FBusCharge::YourHand && C.FlapNow < 0.9f) Why = TEXT("the charging flap is shut");
	else if (Code == ChSwHolster && C.Plug == FBusCharge::Holster && T.Bu && T.Driver != INDEX_NONE) Why = TEXT("its driver is at the wheel");
	return Why.IsEmpty();
}

bool AGSE::ChargeSwitch(int32 Ti, int32 Code)
{
	FTractor& T = Tractors[Ti]; FString Why; if (!ChargeMay(T, Code, Why)) return false;
	FBusCharge& C = *T.Ch; using B = FBusCharge;
	if (Code == ChSwFlap) C.FlapWant = C.FlapWant > 0.5f ? 0.f : 1.f;
	else if (Code == ChSwHolster) { if (C.Plug == B::YourHand) C.Plug = B::Holster; else if (C.Plug == B::Dropped) C.Plug = B::YourHand; else { const FString Q = ChargerBy(T); if (Q.IsEmpty()) return false; C.Cable = Q; C.Plug = B::YourHand; } }
	else if (C.Plug == B::Inlet) { C.Plug = B::YourHand; if (const UADPSim* Sm = UADPSim::Get(this)) C.NotBefore = Sm->Now() + FTimespan::FromMinutes(15.); }
	else if (C.Plug == B::YourHand) { C.Plug = B::Inlet; C.bNeeded = false; }
	else return false;
	ADP_LOG(TEXT("Apron bus %s: you - %s"), *T.Name, Code == ChSwFlap ? (C.FlapWant > 0.5f ? TEXT("the charging flap opened") : TEXT("the charging flap shut")) : C.Plug == B::Inlet ? TEXT("plugged in") : C.Plug == B::YourHand ? TEXT("the plug in your hand") : TEXT("the plug in its holster"));
	return true;
}

const TCHAR* AGSE::ChargeHeld(const FTractor& T) const { return T.Ch && T.Ch->Plug == FBusCharge::YourHand ? TEXT("the charging plug") : nullptr; }
float AGSE::ChargeLeash(const FTractor& T, FVector& From) const
{
	const ABabylonModel* M = T.Ch ? ADPBusCharger::Of(GetWorld(), T.Ch->Cable) : nullptr;
	if (M && M->GetModelRoot()) From = FTransform(FRotator(0.f, T.Yaw, 0.f), T.At).InverseTransformPosition(M->GetModelRoot()->GetComponentTransform().TransformPosition(ChGland));
	return 500.f;
}
void AGSE::ChargeDrop(FTractor& T) { if (T.Ch && T.Ch->Plug == FBusCharge::YourHand) { T.Ch->Plug = FBusCharge::Dropped; T.Ch->Wait = 3.f; } }
