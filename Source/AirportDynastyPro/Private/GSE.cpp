#include "GSE.h"
#include "GSEAmbulift.h"
#include "GSEDeicer.h"
#include "RollerDoor.h"
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
#include "GameFramework/Pawn.h"
#include "DepartureBelts.h"

// The baggage tractors and carts: the vehicles themselves - their places, parts, driving, the train, the routes, the
// lamps and sounds, the test's own checks. Their crew's part (walking over, climbing in, coupling by hand, the
// curtains) is in GSECrew.cpp.
//
// Measures (Blender metres -> the model's root: (x, -y, z) * 100; Art/BaggageTractor/baggagetractor-parts.json,
// Art/BaggageCart/baggagecart-parts.json, Art/GPU/gpu-parts.json):
//  tractor: axles x +0.72 / -0.86 (front tyres 0.26 m, rear 0.31 m), front wheels steered round kingpins at y +-0.42, rear
//           pin hitch x -1.62 z 0.40 (its pin lifts 0.40 m), driver's door on the left hinged at x 0.347, the step, the
//           grab handle, the seat in the middle (0.92 m), pedals, steering wheel
//  cart:    turntable front axle x 0.80 steered by the drawbar (pivot x 1.30 z 0.30, towing eye x 2.01 z 0.40; standing up
//           = parking brake), rear axle x -0.80, rear coupler jaw x -1.27 z 0.40 (its pin lifts 0.10 m), tyres 0.2075 m;
//           roll-up curtains both sides; chained eye-into-jaw: one cart's eye 3.28 m behind the one in front's middle
//  GPU:     towing eye x 2.05 z 0.40 on its drawbar, steered front axle x 0.62, rear axle x -0.78, tyres 0.28 m

namespace
{
	// the tractor
	const FVector GseTPin(-162.f, 0.f, 40.f);
	const FVector GseTWheelAt[4] = {FVector(72.f, -56.f, 26.f), FVector(72.f, 56.f, 26.f), FVector(-86.f, -56.f, 31.f), FVector(-86.f, 56.f, 31.f)};
	const FVector GseTKing[2] = {FVector(72.f, -42.f, 26.f), FVector(72.f, 42.f, 26.f)};
	const FVector GseTHinge(34.7f, -71.2f, 125.f), GseTWheelC(46.f, 0.f, 125.f), GseTPinTop(-162.f, 0.f, 62.5f);
	const TCHAR* GseWKey[4] = {TEXT("FL"), TEXT("FR"), TEXT("RL"), TEXT("RR")};
	constexpr float GseTHalfL = 171.f, GseTHalfW = 102.f;     // (its outline with the mirrors)
	// the cart
	const FVector GseCEye(201.f, 0.f, 40.f), GseCJaw(-127.f, 0.f, 40.f), GseCAxle(80.f, 0.f, 38.9f), GseCBarPivot(130.f, 0.f, 30.f);
	const FVector GseCWheelAt[4] = {FVector(80.f, -60.f, 20.75f), FVector(80.f, 60.f, 20.75f), FVector(-80.f, -60.f, 20.75f), FVector(-80.f, 60.f, 20.75f)};
	constexpr float GseCEyeToAxle = 121.f, GseCHalfL = 172.f, GseCHalfW = 83.f, GseCChain = 328.f;
	// the GPU
	const FVector GseGEye(205.f, 0.f, 40.f), GseGAxle(62.f, 0.f, 35.2f);
	const FVector GseGWheelAt[4] = {FVector(62.f, -70.f, 28.f), FVector(62.f, 70.f, 28.f), FVector(-78.f, -70.f, 28.f), FVector(-78.f, 70.f, 28.f)};
	constexpr float GseGEyeToAxle = 143.f;
	// driving (cm/s): on the apron 15 km/h, in the baggage block 9 km/h, reversing 4 km/h
	constexpr float GseApronSpeed = 420.f, GseHallSpeed = 250.f, GseBackSpeed = 110.f, GseTurnR = 300.f;
	enum GseELensKind : uint8 { GLow, GHigh, GTail, GMarker, GReverse, GBeacon, GIndicator, GWork, GBlue };

	USoundAttenuation* GseAtt()
	{
		static USoundAttenuation* A = nullptr;
		if (!A)
		{
			A = ADPKeep(NewObject<USoundAttenuation>(GetTransientPackage()));
			FSoundAttenuationSettings& S = A->Attenuation;
			S.bAttenuate = true; S.bSpatialize = true; S.AttenuationShapeExtents = FVector(150.f); S.FalloffDistance = 4500.f;
			S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; S.dBAttenuationAtMax = -60.f;
			// heard from where you stand: through the terminal's walls muffled (the sound placement rule)
			S.bEnableOcclusion = true; S.OcclusionTraceChannel = ECC_Visibility; S.OcclusionLowPassFilterFrequency = 900.f; S.OcclusionVolumeAttenuation = 0.35f; S.OcclusionInterpolationTime = 0.15f;
		}
		return A;
	}
	USoundBase* GseSnd(const TCHAR* Name)
	{
		static TMap<FString, USoundBase*> Cache;
		if (USoundBase** F = Cache.Find(Name)) return *F;
		const TCHAR* Dir = FCString::Strncmp(Name, TEXT("BC"), 2) == 0 ? TEXT("BaggageCart") : FCString::Strncmp(Name, TEXT("BL"), 2) == 0 ? TEXT("BeltLoader") : TEXT("BaggageTractor");
		USoundBase* S = ADPKeep(LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/%s/Sounds/%s.%s"), Dir, Name, Name)));
		if (!S) ADP_WARN(TEXT("GSE: sound %s missing (/Game/%s/Sounds)"), Name, Dir);
		Cache.Add(Name, S); return S;
	}
	FVector2f GseP2(const FVector& W) { return FVector2f(W.X / 100.f, -W.Y / 100.f); }
	bool GseIndoors(float X, float Y) { return Y < 82.6f && Y > -10.f && X > -242.f && X < 552.f; }
}

AGSE::AGSE()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

AGSE* AGSE::Find(const UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<AGSE> It(const_cast<UWorld*>(W)); It; ++It) return *It;
	return nullptr;
}

AGSE* AGSE::Get(UWorld* W)
{
	if (AGSE* G = Find(W)) return G;
	if (!W) return nullptr;
	AGSE* G = W->SpawnActor<AGSE>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (G) G->Setup();
	return G;
}

ATugs* AGSE::Tg() const
{
	if (!TugsRef.IsValid()) const_cast<AGSE*>(this)->TugsRef = ATugs::Get(GetWorld());
	return TugsRef.Get();
}

FVector AGSE::Plan(float X, float Y) const
{
	// (the baggage block and hall: the terminal's ground floor; outside: the airfield's ground)
	return FTerminalPlan::ToWorld(X, GseIndoors(X, Y) ? 0.f : AAirfield::GroundAt(X, Y), Y);
}

FVector AGSE::PlanPose(float X, float Y, float Hx, float Hy, float& Yaw) const
{
	Yaw = FVector(Hx, -Hy, 0.f).Rotation().Yaw;   // (plan +y is the world's -y)
	return Plan(X, Y);
}

FVector AGSE::TW(const FTractor& T, const FVector& Local) const { return T.At + FRotator(0.f, T.Yaw, 0.f).RotateVector(Local); }
FVector AGSE::CW(const FCart& C, const FVector& Local) const { return C.At + FRotator(0.f, C.Yaw, 0.f).RotateVector(Local); }

void AGSE::Setup()
{
	if (bReady) return;
	bReady = true;
	bTest = FParse::Param(FCommandLine::Get(), TEXT("GseTest"));
	// the departure cart lane (plan x 114.5-155.5, z 38-51.5; its roller door to the baggage hall at x 128-136): the carts
	// in pairs, facing east, drawbars up; the tractors nose-in in their boxes at the east end
	struct FAt { float X, Y; };
	static const FAt CartAt[] = {{127.8f, 39.8f}, {127.8f - 3.28f, 39.8f}, {127.8f - 6.56f, 39.8f}, {127.8f - 9.84f, 39.8f},
		{127.8f, 43.3f}, {127.8f - 3.28f, 43.3f}, {127.8f, 46.8f}, {127.8f - 3.28f, 46.8f}};
	for (const FAt& A : CartAt)
	{
		FCart C; float Yw = 0.f; C.At = PlanPose(A.X, A.Y, 1.f, 0.f, Yw); C.Yaw = C.AxleYaw = Yw;
		if (SpawnCart(C)) Carts.Add(C);
	}
	// (inside the cart lane itself - x 114-136, z 40-52 (Art/TerminalAnnex/make_bag_block.py): x 136-156 is the walled staff
	//  entrance with its stair and lift - the first test drove them into its wall and the lift. Two against the east wall
	//  south of the staff door (z 45.2-46.8), two in the lane's north-west corner; the way to the roller door (x 128-136,
	//  z 52) kept clear)
	static const FAt BoxAt[] = {{133.f, 41.2f}, {133.f, 43.9f}, {117.6f, 49.8f}, {121.2f, 49.8f}};
	for (int32 i = 0; i < 4; ++i)
	{
		FTractor T; float Yw = 0.f; T.Home = T.At = PlanPose(BoxAt[i].X, BoxAt[i].Y, 1.f, 0.f, Yw); T.HomeYaw = T.Yaw = Yw;
		T.Name = FString::Printf(TEXT("tractor %d"), i + 1); SetKind(T, false);
		if (SpawnTractor(T)) Tractors.Add(T);
	}
	// the belt loader: parked on the apron west of the Ramp services 13 opening, along the terminal (2.5 m off its wall,
	// 9.5 m clear of the stand line at x 246.5), facing the opening
	{
		FTractor L; float Yw = 0.f; L.Home = L.At = PlanPose(256.f, 88.f, 1.f, 0.f, Yw); L.HomeYaw = L.Yaw = Yw;
		L.Name = TEXT("belt loader 1"); SetKind(L, true);
		if (SpawnTractor(L)) Tractors.Add(L);
	}
	SvcSetup();   // (the service vehicles: GSEService.cpp)
	for (FTractor& T : Tractors) Pose(T);
	for (FCart& C : Carts) PoseCart(C);
	ADP_LOG(TEXT("GSE: %d baggage tractors / belt loaders in their places and %d baggage carts (unhitched, drawbars up) in the departure cart lane%s"), Tractors.Num(), Carts.Num(), bTest ? TEXT(" - test (-GseTest): a train to the next departure and a GPU move") : TEXT(""));
}

bool AGSE::SpawnTractor(FTractor& T)
{
	ABabylonModel* M = GetWorld()->SpawnActor<ABabylonModel>(T.At, FRotator(0.f, T.Yaw, 0.f));
	const TCHAR* Model = T.Svc != INDEX_NONE ? SvcModel(T.Svc) : T.bLoader ? TEXT("BeltLoader") : TEXT("BaggageTractor");
	if (!M || !M->Place(Model)) { if (M) M->Destroy(); ADP_WARN(TEXT("GSE: the %s model is missing (Tools/import_model.py ... %s)"), Model, Model); return false; }
	M->SetPartsCollision(false);
	T.M = M;
	// the lenses: the fire truck's lens material, one glow value per lens (its windows: the truck's glass - VehicleDirt)
	UMaterialInterface* LensM = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_TruckLens.M_TruckLens")));
	TArray<UInstancedStaticMeshComponent*> Parts; M->Parts(Parts);
	TMap<UMaterialInterface*, UMaterialInterface*> Swap;
	for (UInstancedStaticMeshComponent* K : Parts)
		for (int32 Si = 0; Si < K->GetNumMaterials(); ++Si)
		{
			UMaterialInterface* Mi = K->GetMaterial(Si); if (!Mi) continue;
			if (UMaterialInterface** F = Swap.Find(Mi)) { if (*F) { K->SetMaterial(Si, *F); K->SetNumCustomDataFloats(1); } continue; }
			const UMaterialInstanceDynamic* Md = Cast<UMaterialInstanceDynamic>(Mi);
			const FString N = Md && Md->Parent ? Md->Parent->GetName() : Mi->GetName();
			UMaterialInstanceDynamic* D = nullptr;
			if (LensM && N.Contains(TEXT("Lens")))
			{
				D = UMaterialInstanceDynamic::Create(LensM, this);
				const bool bRed = N.Contains(TEXT("Red")), bAmber = N.Contains(TEXT("Amber")), bGreen = N.Contains(TEXT("Green")), bBlueL = N.Contains(TEXT("Blue"));
				D->SetVectorParameterValue(TEXT("BC"), bRed ? FLinearColor(1.f, 0.04f, 0.02f) : bAmber ? FLinearColor(1.f, 0.45f, 0.03f) : bBlueL ? FLinearColor(0.03f, 0.12f, 1.f) : bGreen ? FLinearColor(0.05f, 1.f, 0.15f) : FLinearColor(1.f, 0.97f, 0.9f));
				D->SetScalarParameterValue(TEXT("Glow"), bBlueL ? 75.f : bAmber ? 55.f : bRed ? 30.f : 28.f); D->SetScalarParameterValue(TEXT("Dots"), 4.f);
				K->SetNumCustomDataFloats(1);
			}
			Swap.Add(Mi, D); if (D) K->SetMaterial(Si, D);
		}
	struct FW { const TCHAR* Name; GseELensKind Kind; };
	static const FW Which[] = {{TEXT("LowBeam_"), GLow}, {TEXT("HighBeam_"), GHigh}, {TEXT("TailBrake_"), GTail}, {TEXT("SideMarker_"), GMarker}, {TEXT("Reverse_"), GReverse},
		{TEXT("BeaconLens"), GBeacon}, {TEXT("IndicatorF_"), GIndicator}, {TEXT("IndicatorR_"), GIndicator}, {TEXT("WorkLamp_Lens"), GWork},
		{TEXT("BeaconR_Lens"), GBeacon}, {TEXT("Cabin_Beacon_Lens"), GBeacon}, {TEXT("CabinLamp"), GWork}, {TEXT("RearWorkLamp_Lens"), GWork}, {TEXT("PanelLamp_Lens"), GWork},
		{TEXT("BlueFront_"), GBlue}, {TEXT("BlueRear_"), GBlue}, {TEXT("BlueGrille_"), GBlue}, {TEXT("BoxLight"), GWork}, {TEXT("BoxWork"), GWork}, {TEXT("BoxSideMark"), GMarker}, {TEXT("BoxCorner"), GMarker}};
	for (const FW& Wh : Which)
	{
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Found; M->InstancesNamed(Wh.Name, true, Found);
		for (const auto& F : Found)
			if (F.Get<2>().EndsWith(TEXT("_Lens")) || F.Get<2>() == TEXT("BeaconLens") || F.Get<2>() == TEXT("WorkLamp_Lens"))
			{ FLens L; L.K = F.Get<0>(); L.Ix = F.Get<1>(); L.Kind = Wh.Kind; if (Wh.Kind == GIndicator) L.Side = F.Get<2>().Contains(TEXT("_L_")) ? 1 : -1;
				// (the blue bars: units 0-3 the one half, 4-7 the other; the grille's left and right pair)
				if (Wh.Kind == GBlue) { const FString& Bn = F.Get<2>(); const int32 Dg = Bn.Len() > 6 && FChar::IsDigit(Bn[Bn.Len() - 6]) ? int32(Bn[Bn.Len() - 6] - TEXT('0')) : 0; L.Side = Bn.StartsWith(TEXT("BlueGrille_")) ? (Bn.Contains(TEXT("_L")) ? 1 : -1) : Dg < 4 ? 1 : -1; }
				if (FCString::Strcmp(Wh.Name, TEXT("BoxLight")) == 0) L.Side = 2;   // (inside: its own switch)
				T.Lenses.Add(L); }
	}
	// the moving parts
	for (int32 w = 0; w < 4; ++w)
	{
		TArray<FString> Names; for (const TCHAR* P : {TEXT("Tyre_"), TEXT("Rim_"), TEXT("WheelDisc_"), TEXT("Hub_"), TEXT("HubCap_"), TEXT("Nut_")}) Names.Add(FString(P) + GseWKey[w]);
		T.GWheel[w] = M->BindGroup(Names, true, FTransform(T.KWheel[w]));
		if (w < 2) T.GSteer[w] = M->BindGroup({FString(TEXT("Spindle_")) + GseWKey[w], FString(TEXT("SteerArm_")) + GseWKey[w]}, true, FTransform(T.KKing[w]));
	}
	T.GDoor = M->BindGroup({TEXT("DoorDriver")}, true, FTransform(T.KHinge));
	T.GDoor2 = M->BindGroup({TEXT("HingeDoor_0"), TEXT("HingeDoor_1")}, false, FTransform(T.KHinge));
	T.GWheelS = M->BindGroup({TEXT("SteeringWheel")}, true, FTransform(T.KWheelC));
	if (T.IsTrain())
	{
		// (round its wheel: the model has no levers for them yet)
		T.Pt.Add(TEXT("CAB_Horn"), T.KWheelC); T.Pt.Add(TEXT("CAB_StalkL"), T.KWheelC + FVector(6.f, -17.f, -3.f)); T.Pt.Add(TEXT("CAB_StalkR"), T.KWheelC + FVector(6.f, 17.f, -3.f));
		T.Pt.Add(TEXT("IK_ParkBrake"), T.KWheelC + FVector(8.f, 30.f, -12.f)); T.Pt.Add(TEXT("IK_GearSelect"), T.KWheelC + FVector(8.f, 24.f, -6.f)); T.Pt.Add(TEXT("TR_Hitch"), GseTPinTop);
	}
	if (T.IsTrain()) T.GPin = M->BindGroup({TEXT("HitchPinRear")}, true, FTransform(GseTPinTop));
	else if (T.bLoader)
	{
		// the boom (its belt, its rails, its work lamp) about its lower hinge; the bumper pad on its pin; the two rams
		T.GBoom = M->BindGroup({TEXT("Boom_"), TEXT("BeltSurface"), TEXT("WorkLamp_")}, true, FTransform(FVector(-320.f, 0.f, 76.f)));
		T.GBumper = M->BindGroup({TEXT("BumperPad")}, true, FTransform(FVector(386.f, 0.f, 75.5f)));
		// its controls (make_belt_loader.py; those on the boom as it lies down)
		T.Pt.Add(TEXT("IK_Hand_BoomLever"), FVector(-34.f, -53.5f, 98.f));
		T.Pt.Add(TEXT("LD_CabBelt"), FVector(-47.f, -53.5f, 89.4f));
		T.Pt.Add(TEXT("LD_CabStop"), FVector(34.f, -93.8f, 82.f));
		T.Pt.Add(TEXT("LD_RearStop"), FVector(-337.f, 0.f, 42.f));
		T.Pt.Add(TEXT("IK_GearSelect"), FVector(9.4f, -94.5f, 102.6f));
		T.Pt.Add(TEXT("CAB_Horn"), FVector(2.f, -80.f, 110.f));
		for (int32 p = 0; p < 2; ++p)
		{
			const float X = p ? 320.f : -350.f; const FString Nm = p ? TEXT("LD_T") : TEXT("LD_B");
			for (int32 k = 0; k < 3; ++k) T.Pt.Add(Nm + FString::FromInt(k), FVector(X - 3.5f, -47.f, 91.f - 4.5f * k));
			T.Pt.Add(Nm + TEXT("Stop"), FVector(X + 3.f, -47.9f, 83.f)); T.Pt.Add(Nm + TEXT("Lamp"), FVector(X + 3.f, -47.1f, 89.f));
		}
		T.GJoy = M->BindGroup({TEXT("CabBoomJoystick")}, true, FTransform(FVector(-34.f, -53.5f, 88.f)));
		T.GRam[0] = M->BindGroup({TEXT("RamFront_Barrel")}, true, FTransform(FVector(-40.f, 0.f, 42.f)));
		T.GRam[1] = M->BindGroup({TEXT("RamFront_Rod")}, true, FTransform(FVector(170.f, 0.f, 64.f)));
		T.GRam[2] = M->BindGroup({TEXT("RamRear_Barrel")}, true, FTransform(FVector(-270.f, 0.f, 42.f)));
		T.GRam[3] = M->BindGroup({TEXT("RamRear_Rod")}, true, FTransform(FVector(-130.f, 0.f, 64.f)));
	}
	if (T.Svc != INDEX_NONE) SvcDress(T);
	T.Dirt.Dress(M, this, T.Name);
	Lamps(T);
	return true;
}

bool AGSE::SpawnCart(FCart& C)
{
	ABabylonModel* M = GetWorld()->SpawnActor<ABabylonModel>(C.At, FRotator(0.f, C.Yaw, 0.f));
	if (!M || !M->Place(TEXT("BaggageCart"))) { if (M) M->Destroy(); ADP_WARN(TEXT("GSE: the baggage cart model is missing (Tools/import_model.py ... BaggageCart)")); return false; }
	M->SetPartsCollision(false);
	C.M = M;
	for (int32 w = 0; w < 4; ++w)
	{
		TArray<FString> Names; for (const TCHAR* P : {TEXT("Tyre_"), TEXT("Rim_"), TEXT("WheelDisc_"), TEXT("Hub_"), TEXT("HubCap_"), TEXT("Nut_")}) Names.Add(FString(P) + GseWKey[w]);
		C.GWheel[w] = M->BindGroup(Names, true, FTransform(GseCWheelAt[w]));
	}
	C.GAxle = M->BindGroup({TEXT("FrontAxle_")}, true, FTransform(GseCAxle));
	C.GBar = M->BindGroup({TEXT("Drawbar_")}, true, FTransform(GseCBarPivot));
	C.GPin = M->BindGroup({TEXT("CouplerRear_Pin")}, true, FTransform(FVector(-127.f, 0.f, 59.55f)));
	for (int32 s = 0; s < 2; ++s)
	{
		const FString S = s == 0 ? TEXT("L") : TEXT("R");
		const FTransform Piv(FVector(0.f, s == 0 ? -75.f : 75.f, 184.f));
		C.GSheet[s] = M->BindGroup({TEXT("Curtain_") + S}, false, Piv);
		C.GHem[s] = M->BindGroup({TEXT("Curtain_") + S + TEXT("_Hem")}, false, Piv);
		C.GStrap[s] = M->BindGroup({TEXT("Curtain_") + S + TEXT("_PullStrap"), TEXT("Curtain_") + S + TEXT("_Strap_"), TEXT("Curtain_") + S + TEXT("_Buckle_")}, true, Piv);
		C.GRoll[s] = M->BindGroup({TEXT("CurtainRoll_") + S}, false, Piv);
	}
	C.Dirt.Dress(M, this, FString::Printf(TEXT("baggage cart %d"), Carts.Num() + 1));
	C.Bags.Init(INDEX_NONE, 24);
	C.bMoved = true;
	return true;
}

// ------------------------------------------------------------------ posing, parts, lamps, sounds

void AGSE::Pose(FTractor& T)
{
	ABabylonModel* M = T.M.Get(); if (!M) return;
	// every wheel on the surface (owner 2026-11-20): the ground under each, the body leaning with it - a degree or so
	const FRotator R0(0.f, T.Yaw, 0.f);
	float Z[4]; for (int32 w = 0; w < 4; ++w) { const FVector P = T.At + R0.RotateVector(T.KWheel[w]); Z[w] = Plan(P.X / 100.f, -P.Y / 100.f).Z; }
	const float Zm = (Z[0] + Z[1] + Z[2] + Z[3]) / 4.f;
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan(((Z[0] + Z[1]) - (Z[2] + Z[3])) * 0.5f / T.KWB));
	const float Roll = FMath::RadiansToDegrees(FMath::Atan(((Z[1] + Z[3]) - (Z[0] + Z[2])) * 0.5f / T.KTrack));
	T.At.Z = Zm;
	M->SetActorLocationAndRotation(T.At, FRotator(Pitch, T.Yaw, Roll));
}

void AGSE::PoseCart(FCart& C)
{
	ABabylonModel* M = C.M.Get(); if (!M) return;
	const FRotator R0(0.f, C.Yaw, 0.f);
	float Z[4]; for (int32 w = 0; w < 4; ++w) { const FVector P = C.At + R0.RotateVector(GseCWheelAt[w]); Z[w] = Plan(P.X / 100.f, -P.Y / 100.f).Z; }
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan(((Z[0] + Z[1]) - (Z[2] + Z[3])) * 0.5f / 160.f));
	const float Roll = FMath::RadiansToDegrees(FMath::Atan(((Z[1] + Z[3]) - (Z[0] + Z[2])) * 0.5f / 120.f));
	C.At.Z = (Z[0] + Z[1] + Z[2] + Z[3]) / 4.f;
	M->SetActorLocationAndRotation(C.At, FRotator(Pitch, C.Yaw, Roll));
	C.bMoved = true;
}

void AGSE::Sound(const TCHAR* Name, const FVector& At)
{
	if (USoundBase* S = GseSnd(Name)) UGameplayStatics::PlaySoundAtLocation(this, S, At, 1.f, 1.f, 0.f, GseAtt());
}

void AGSE::Motor(FTractor& T, bool bOn)
{
	if (bOn == T.bOn || !T.M.IsValid()) return;
	T.bOn = bOn;
	if (bOn)
	{
		if (!T.SndStart.IsEmpty()) Sound(*T.SndStart, T.At);
		T.Motor = UGameplayStatics::SpawnSoundAttached(GseSnd(*T.SndLoop), T.M->GetRootComponent(), NAME_None, FVector(0, 0, 60.f), EAttachLocation::KeepRelativeOffset, false, 0.7f, 0.85f, 0.f, GseAtt(), nullptr, false);
	}
	else { if (T.Motor) T.Motor->Stop(); T.Motor = nullptr; if (!T.SndStop.IsEmpty()) Sound(*T.SndStop, T.At); }
}

void AGSE::Lamps(FTractor& T)
{
	// switched on: low beams, tail lamps, the side markers, the amber beacon flashing (airside: always while it moves);
	// brake lamps bright while slowing and at a standstill with the motor on; the indicators on the side it turns to;
	// reversing lamps and the beeper backing
	const bool bRun = T.bOn, bBack = bRun && T.bReverse && (T.Path.Num() > 0 || T.Player.IsValid());
	const float F = FMath::Frac(Flash + FMath::Abs(T.Home.X) * 0.0013f);
	const bool bFlash = bRun && (F < 0.08f || (F > 0.18f && F < 0.26f));
	const bool bMine = CabMine(T), bLit = bRun && T.Beam < 2;   // (a service vehicle with you at its wheel: its lights as you switch them)
	const bool bBrake = bRun && (T.BrakeHold > 0.f || (T.MeasSpeed < 0.05f && !(bMine && T.bPark)));
	const int8 Turn = !bRun ? 0 : bMine ? T.Sig : T.Steer > 8.f ? -1 : T.Steer < -8.f ? 1 : 0;
	const bool bBlink = FMath::Fmod(Flash * 1.5f, 1.f) < 0.5f;
	for (FLens& L : T.Lenses)
	{
		float V = 0.f;
		switch (L.Kind)
		{
		case GLow: V = bLit ? 0.8f : 0.f; break;
		case GHigh: V = bLit && T.Beam == 1 ? 1.f : 0.f; break;
		case GTail: V = bBrake ? 1.f : bLit ? 0.3f : 0.f; break;
		case GIndicator: V = ((T.bSwHaz || (Turn != 0 && L.Side == Turn)) && bBlink) ? 1.f : 0.f; break;
		case GMarker: V = bLit ? 0.5f : 0.f; break;
		case GReverse: V = bBack ? 1.f : 0.f; break;
		case GBeacon: V = bFlash && !T.bSwNoBeacon ? 1.f : 0.f; break;
		case GBlue: V = AmbuBlue(T, L.Side); break;
		case GWork: V = (L.Side == 2 ? T.bSwLight : T.bSwWork) || (T.Am && T.Am->bLive && T.Job == EJob::AtStand) || (bRun && T.Boom < -0.5f) || (T.Di && (T.Di->bYours || (T.Di->Ph >= FSvcDeicer::OpIn && T.Di->Ph <= FSvcDeicer::OpOut))) ? 1.f : 0.f; break;
		default: V = 0.f;
		}
		if (V != L.Now && L.K) { L.Now = V; L.K->SetCustomDataValue(L.Ix, 0, V, true); }
	}
	if (bBack && !T.Beep && T.M.IsValid())
		T.Beep = UGameplayStatics::SpawnSoundAttached(GseSnd(*T.SndBeep), T.M->GetRootComponent(), NAME_None, FVector(-150.f, 0, 80.f), EAttachLocation::KeepRelativeOffset, false, 0.6f, 1.f, 0.f, GseAtt(), nullptr, false);
	if (!bBack && T.Beep) { T.Beep->Stop(); T.Beep = nullptr; }
	if (T.Motor) { T.Motor->SetPitchMultiplier(0.85f + FMath::Clamp(T.MeasSpeed / 4.2f, 0.f, 1.f) * 0.45f); T.Motor->SetVolumeMultiplier(0.45f + FMath::Clamp(T.MeasSpeed / 4.2f, 0.f, 1.f) * 0.5f); }
}

void AGSE::TractorParts(FTractor& T, float Dt)
{
	ABabylonModel* M = T.M.Get(); if (!M || Dt <= 0.f) return;
	if (!T.bPrev) { T.bPrev = true; T.PrevAt = T.At; T.PrevYaw = T.Yaw; }
	const FVector Fwd = FRotator(0.f, T.Yaw, 0.f).Vector(), D = T.At - T.PrevAt;
	const float Dist = D.Size2D(), Dir = FVector::DotProduct(D, Fwd) >= 0.f ? 1.f : -1.f;
	const float YawRate = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(T.PrevYaw, T.Yaw)) / Dt;
	T.PrevAt = T.At; T.PrevYaw = T.Yaw;
	// rolled by the distance over each tyre's radius; steered (front wheels): tan(angle) = wheelbase * yaw rate / speed
	T.SpinF = FMath::Fmod(T.SpinF + Dir * Dist / T.KRadF, 2.f * PI);
	T.SpinR = FMath::Fmod(T.SpinR + Dir * Dist / T.KRadR, 2.f * PI);
	const float Speed = Dist / Dt / 100.f;
	T.MeasSpeed = FMath::FInterpTo(T.MeasSpeed, Speed, Dt, 6.f);
	T.BrakeHold = T.MeasSpeed < T.LastSpeed - 0.15f * Dt ? 0.6f : FMath::Max(0.f, T.BrakeHold - Dt); T.LastSpeed = T.MeasSpeed;
	if (Speed > 0.05f) T.Steer = FMath::FInterpTo(T.Steer, FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(T.KWB / 100.f * YawRate / (Speed * Dir))), -T.KMaxSteer, T.KMaxSteer), Dt, 5.f);
	for (int32 w = 0; w < 4; ++w)
	{
		if (T.GWheel[w] == INDEX_NONE) continue;
		const FQuat Spin(FVector::RightVector, w < 2 ? T.SpinF : T.SpinR);
		if (w < 2)
		{
			const FQuat Qs(FVector::UpVector, FMath::DegreesToRadians(T.Steer));
			M->MoveGroup(T.GWheel[w], FTransform(Qs * Spin, T.KKing[w] + Qs.RotateVector(T.KWheel[w] - T.KKing[w])));
			if (T.GSteer[w] != INDEX_NONE) M->MoveGroup(T.GSteer[w], FTransform(Qs, T.KKing[w]));
		}
		else M->MoveGroup(T.GWheel[w], FTransform(Spin, T.KWheel[w]));
	}
	// the steering wheel (about its column, ~16 turns of the wheel lock to lock... a gentle 8x), the door, the hitch pin
	if (T.GWheelS != INDEX_NONE) M->MoveGroup(T.GWheelS, FTransform(FQuat(FVector(-0.7071f, 0.f, 0.7071f), FMath::DegreesToRadians(-T.Steer * 8.f)), T.KWheelC));
	if (!FMath::IsNearlyEqual(T.DoorAng, T.DoorWant, 0.05f))
	{
		const float Was = T.DoorAng;
		T.DoorAng = FMath::FInterpTo(T.DoorAng, T.DoorWant, Dt, 5.f); if (FMath::Abs(T.DoorAng - T.DoorWant) < 0.5f) T.DoorAng = T.DoorWant;
		// (swung out round its front edge: Blender's negative turn about +z is the root's positive)
		const FTransform Dr(FQuat(FVector::UpVector, FMath::DegreesToRadians(T.DoorAng)), T.KHinge);
		if (T.GDoor != INDEX_NONE) M->MoveGroup(T.GDoor, Dr);
		if (T.GDoor2 != INDEX_NONE) M->MoveGroup(T.GDoor2, Dr);
		if (Was > 0.f && T.DoorAng == 0.f) Sound(*T.SndDoorClose, TW(T, T.KHinge));
	}
	if (!FMath::IsNearlyEqual(T.Pin, T.PinWant, 0.001f))
	{
		T.Pin = FMath::FInterpConstantTo(T.Pin, T.PinWant, Dt, 1.2f);
		if (T.GPin != INDEX_NONE) M->MoveGroup(T.GPin, FTransform(GseTPinTop + FVector(0, 0, 40.f * T.Pin)));
	}
	if (T.bLoader) LoaderParts(T, Dt);
	if (T.Svc != INDEX_NONE) SvcParts(T, Dt);
	M->FlushMoves();
	T.Dirt.Tick(Dt, T.MeasSpeed);
}

void AGSE::CartParts(FCart& C, float Dt)
{
	ABabylonModel* M = C.M.Get(); if (!M || Dt <= 0.f) return;
	bool bAny = C.bMoved; C.bMoved = false;
	float Ms = 0.f;
	if (!C.bPrev) { C.bPrev = true; C.PrevAt = C.At; }
	{
		const FVector D = C.At - C.PrevAt; const float Dist = D.Size2D();
		if (Dist > 0.01f) { const float Dir = FVector::DotProduct(D, FRotator(0.f, C.Yaw, 0.f).Vector()) >= 0.f ? 1.f : -1.f; C.Spin = FMath::Fmod(C.Spin + Dir * Dist / 20.75f, 2.f * PI); bAny = true; }
		Ms = Dist / Dt / 100.f; C.PrevAt = C.At;
	}
	// the drawbar up or down (by hand: ~3 s), the curtains rolled, the coupler pin
	if (!FMath::IsNearlyEqual(C.Lift, C.LiftWant, 0.01f)) { C.Lift = FMath::FInterpConstantTo(C.Lift, C.LiftWant, Dt, 32.f); bAny = true; }
	for (int32 s = 0; s < 2; ++s) if (!FMath::IsNearlyEqual(C.Curtain[s], C.CurtainWant[s], 0.001f)) { C.Curtain[s] = FMath::FInterpConstantTo(C.Curtain[s], C.CurtainWant[s], Dt, 0.42f); bAny = true; }
	if (!FMath::IsNearlyEqual(C.Pin, C.PinWant, 0.001f)) { C.Pin = FMath::FInterpConstantTo(C.Pin, C.PinWant, Dt, 2.f); bAny = true; }
	if (bAny)
	{
		// the turntable axle turned by its drawbar (relative to the body), the drawbar lifted about its pivot on the axle
		const float Rel = FMath::Clamp(FMath::FindDeltaAngleDegrees(C.Yaw, C.AxleYaw), -80.f, 80.f);
		const FQuat Qa(FVector::UpVector, FMath::DegreesToRadians(Rel)), Spin(FVector::RightVector, C.Spin);
		if (C.GAxle != INDEX_NONE) M->MoveGroup(C.GAxle, FTransform(Qa, GseCAxle));
		if (C.GBar != INDEX_NONE) M->MoveGroup(C.GBar, FTransform(Qa * FQuat(FVector::RightVector, FMath::DegreesToRadians(C.Lift)), GseCAxle + Qa.RotateVector(GseCBarPivot - GseCAxle)));
		for (int32 w = 0; w < 4; ++w)
			if (C.GWheel[w] != INDEX_NONE)
				M->MoveGroup(C.GWheel[w], w < 2 ? FTransform(Qa * Spin, GseCAxle + Qa.RotateVector(GseCWheelAt[w] - GseCAxle)) : FTransform(Spin, GseCWheelAt[w]));
		if (C.GPin != INDEX_NONE) M->MoveGroup(C.GPin, FTransform(FVector(-127.f, 0.f, 59.55f + 10.f * C.Pin)));
		// the curtains (baggagecart-parts.json): the sheet shortened up into the roll, the hem bar and the straps carried up
		// 1.256 m, the straps wrapped away near the top, the roll fatter
		for (int32 s = 0; s < 2; ++s)
		{
			const float f = C.Curtain[s]; const FVector Pv(0.f, s == 0 ? -75.f : 75.f, 184.f);
			if (C.GSheet[s] != INDEX_NONE) M->MoveGroup(C.GSheet[s], FTransform(FQuat::Identity, Pv, FVector(1.f, 1.f, FMath::Max(0.03f, 1.f - 0.97f * f))));
			if (C.GHem[s] != INDEX_NONE) M->MoveGroup(C.GHem[s], FTransform(Pv + FVector(0, 0, 125.6f * f)));
			if (C.GStrap[s] != INDEX_NONE) M->MoveGroup(C.GStrap[s], FTransform(FQuat::Identity, Pv + FVector(0, 0, 125.6f * f), FVector(f > 0.9f ? 0.0001f : 1.f)));
			if (C.GRoll[s] != INDEX_NONE) M->MoveGroup(C.GRoll[s], FTransform(FQuat::Identity, Pv, FVector(1.f, 1.f + 0.4f * f, 1.f + 0.4f * f)));
		}
		M->FlushMoves();
	}
	// its rolling sound while towed
	const bool bRoll = C.Tractor != INDEX_NONE && Ms > 0.3f;
	if (bRoll && !C.Roll) C.Roll = UGameplayStatics::SpawnSoundAttached(GseSnd(TEXT("BCRollLoop")), M->GetRootComponent(), NAME_None, FVector(0, 0, 30.f), EAttachLocation::KeepRelativeOffset, false, 0.5f, 1.f, 0.f, GseAtt(), nullptr, false);
	if (C.Roll) { if (!bRoll && Ms < 0.1f) { C.Roll->Stop(); C.Roll = nullptr; } else { C.Roll->SetVolumeMultiplier(0.25f + FMath::Clamp(Ms / 4.f, 0.f, 1.f) * 0.55f); C.Roll->SetPitchMultiplier(0.85f + FMath::Clamp(Ms / 4.f, 0.f, 1.f) * 0.3f); } }
	C.Dirt.Tick(Dt, Ms);
}

// ------------------------------------------------------------------ driving and the train

void AGSE::Go(FTractor& T, TArray<FVector> Path, bool bReverse, TFunction<void()> Then)
{
	T.Path = MoveTemp(Path); T.bReverse = bReverse; T.OnArrive = MoveTemp(Then); T.Hold = 0.f;
	T.bEndYaw = T.Path.Num() >= 2;
	if (T.bEndYaw) { const FVector D = T.Path.Last() - T.Path[T.Path.Num() - 2]; T.EndYaw = (bReverse ? -D : D).Rotation().Yaw; }
	if (!T.Path.Num() && T.OnArrive) { TFunction<void()> F = MoveTemp(T.OnArrive); T.OnArrive = nullptr; F(); }
}

void AGSE::Drive(FTractor& T, float Dt)
{
	// as the tug drives (Tugs.cpp Drive): eased speed, steering for a point ahead on its way, never tighter than its turning
	// circle, slower in bends, a creep into an exact place; with a train: gentle (the carts are loaded), never backing
	if (!T.Path.Num()) { T.Speed = 0.f; return; }
	if (DriverLost(T)) { T.Speed = 0.f; return; }   // (to-do 206: nobody at the wheel - it stands until somebody takes over)
	const FVector End = T.Path.Last();
	while (T.Path.Num() > 1 && FVector::Dist2D(T.At, T.Path[0]) < 110.f) T.Path.RemoveAt(0);
	// (a waypoint gone past - beside it, not through it - counts as reached; and never round and round one: after a full turn on
	//  the spot it goes for the next)
	while (T.Path.Num() > 1 && FVector::Dist2D(T.At, T.Path[0]) < 400.f && FVector::DotProduct((T.Path[1] - T.Path[0]).GetSafeNormal2D(), (T.At - T.Path[0]).GetSafeNormal2D()) > 0.2f) T.Path.RemoveAt(0);
	if (T.OrbitN != T.Path.Num()) { T.OrbitN = T.Path.Num(); T.Orbit = 0.f; T.OrbitYaw = T.Yaw; }
	T.Orbit += FMath::FindDeltaAngleDegrees(T.OrbitYaw, T.Yaw); T.OrbitYaw = T.Yaw;
	if (FMath::Abs(T.Orbit) > 330.f && T.Path.Num() > 1) { ADP_LOG(TEXT("GSE: %s missed a bend - on to the next point of its way"), *T.Name); T.Path.RemoveAt(0); T.OrbitN = T.Path.Num(); T.Orbit = 0.f; }
	const bool bLast = T.Path.Num() == 1;
	FVector Aim = T.Path[0];
	{
		float Look = T.bReverse ? 170.f : 240.f; FVector From = T.At;
		for (int32 i = 0; i < T.Path.Num(); ++i)
		{
			const float Seg = FVector::Dist2D(From, T.Path[i]);
			if (Seg >= Look) { Aim = From + (T.Path[i] - From).GetSafeNormal2D() * Look; break; }
			Look -= Seg; From = T.Path[i]; Aim = T.Path[i];
		}
	}
	FVector To = Aim - T.At; To.Z = 0.f;
	const float DistEnd = FVector::Dist2D(T.At, End);
	if (bLast && DistEnd < 6.f)
	{
		T.At = FVector(End.X, End.Y, T.At.Z); T.Path.Reset(); T.Speed = 0.f;
		if (T.bEndYaw && FMath::Abs(FMath::FindDeltaAngleDegrees(T.Yaw, T.EndYaw)) < 10.f) T.Yaw = T.EndYaw;
		Pose(T);
		if (T.OnArrive) { TFunction<void()> F = MoveTemp(T.OnArrive); T.OnArrive = nullptr; F(); }
		return;
	}
	// giving way: another tractor or its carts, a tug, close in front of the way it moves; aircraft (moving) within 45 m
	// ahead on the apron - aircraft always have right of way (owner 2026-11-21)
	const FVector Move = (T.bReverse ? -1.f : 1.f) * FRotator(0.f, T.Yaw, 0.f).Vector();
	bool bBlocked = false; FString Why;
	auto Ahead = [&](const FVector& P, float R) { const FVector D = P - T.At; return D.Size2D() < R && FVector::DotProduct(D.GetSafeNormal2D(), Move) > 0.45f; };
	// (one that stands still - loading at the carousel, parked - only where the way really goes through it: two trains are
	//  made up side by side in the lane, the second never waits for the first to leave)
	auto OnWay = [&](const FVector& P)
	{
		FVector From = T.At; float Len = 0.f;
		for (int32 i = 0; i < T.Path.Num() && Len < 700.f; ++i) { if (FMath::PointDistToSegment(FVector(P.X, P.Y, 0.f), FVector(From.X, From.Y, 0.f), FVector(T.Path[i].X, T.Path[i].Y, 0.f)) < 215.f) return true; Len += FVector::Dist2D(From, T.Path[i]); From = T.Path[i]; }
		return false;
	};
	for (const FTractor& O : Tractors)
	{
		if (&O == &T || !O.M.IsValid()) continue;
		if (!O.Path.Num() && O.Speed < 5.f)
		{
			bool bIn = O.Job != EJob::None && Ahead(O.At, 650.f) && OnWay(O.At); for (const int32 c : O.Train) if (Carts.IsValidIndex(c) && Ahead(Carts[c].At, 450.f) && OnWay(Carts[c].At)) bIn = true;
			if (bIn) { bBlocked = true; Why = O.Name + TEXT(" (standing in its way)"); }
			continue;
		}
		if (Ahead(O.At, 650.f) && (O.Path.Num() > 0 || O.Job != EJob::None) && &O < &T) { bBlocked = true; Why = O.Name; }
		for (const int32 c : O.Train) if (Carts.IsValidIndex(c) && Ahead(Carts[c].At, 450.f)) { bBlocked = true; Why = O.Name + TEXT("'s train"); }
	}
	// one train at a time where only one fits: the cart lane's door and the passage at Ramp services 13 (4 m doors). Who is first
	// has it until the last cart is out; the other waits before it, on its own side of the hall or the apron
	if (T.IsTrain())
		for (int32 z = 0; z < 2; ++z)
		{
			static const FBox2f Zn[2] = {FBox2f(FVector2f(131.f, 46.f), FVector2f(147.f, 60.f)), FBox2f(FVector2f(262.f, 53.f), FVector2f(273.f, 94.f))};
			auto In = [&](const FVector& P) { return Zn[z].IsInside(GseP2(P)); };
			auto Has = [&](const FTractor& X) { if (In(X.At)) return true; for (const int32 c : X.Train) if (Carts.IsValidIndex(c) && In(Carts[c].At)) return true; return false; };
			bool bSoon = false; { FVector From = T.At; float Len = 0.f; for (int32 i = 0; i < T.Path.Num() && Len < 1100.f && !bSoon; ++i) { Len += FVector::Dist2D(From, T.Path[i]); From = T.Path[i]; bSoon = Len < 1300.f && In(From); } }
			const bool bIn = Has(T);
			if (!bIn && !bSoon) { T.bZone[z] = false; continue; }
			if (T.bZone[z]) continue;
			bool bOther = false; for (const FTractor& O : Tractors) if (&O != &T && O.M.IsValid() && O.IsTrain() && (O.bZone[z] || (z == 1 && O.Path.Num() && Has(O)))) { bOther = true; Why = O.Name + TEXT(" (only one train fits there)"); }
			if (bIn || !bOther) T.bZone[z] = true;
			else bBlocked = true;
		}
	if (const ATugs* G = Tg())
	{
		for (const ATugs::FTug& U : G->Tugs) if (U.M.IsValid() && U.State != ATugs::EState::Parked && Ahead(U.At, 800.f)) { bBlocked = true; Why = TEXT("a tug"); }
		if (GseP2(T.At).Y > 82.6f) for (const FVector& Pl : G->Planes) if (Ahead(Pl, 4500.f)) { bBlocked = true; Why = TEXT("an aircraft"); }
	}
	if (T.Svc != INDEX_NONE && !bBlocked && SvcPersonAhead(T, Move)) { bBlocked = true; Why = TEXT("somebody on foot"); }
	if (T.YouOn) { bBlocked = true; Why = TEXT("you, standing on it"); T.YouStopAt = GetWorld()->GetTimeSeconds(); }
	if (bBlocked) { T.Hold += Dt; if (FMath::Fmod(T.Hold, 15.f) < Dt) ADP_LOG(TEXT("GSE: %s gives way to %s"), *T.Name, *Why); }
	// steering: the heading wanted (the back leads reversing), limited by the turning circle at this speed
	const float WantYaw = T.bReverse ? (-To).Rotation().Yaw : To.Rotation().Yaw;
	const float dYaw = FMath::FindDeltaAngleDegrees(T.Yaw, WantYaw);
	const float MaxRate = FMath::Min(45.f, FMath::RadiansToDegrees(FMath::Max(T.Speed, 50.f) / T.KTurnR));
	T.Yaw += FMath::Clamp(dYaw, -MaxRate * Dt, MaxRate * Dt);
	// speed: the lane's in the building, the apron's outside; slower in bends ahead; a creep for the last metres
	const bool bIn = GseIndoors(GseP2(T.At).X, GseP2(T.At).Y);
	float Want = T.bReverse ? GseBackSpeed : bIn ? GseHallSpeed : GseApronSpeed;
	if (T.Train.Num() || T.TowGpu != INDEX_NONE) Want *= 0.85f;
	Want *= FMath::Clamp(1.f - FMath::Abs(dYaw) / 120.f, 0.55f, 1.f);
	if (T.Path.Num() >= 2)
	{
		float Turn = 0.f, Len = 0.f; FVector Prev = T.At; FVector PD = Move;
		for (int32 i = 0; i < FMath::Min(T.Path.Num(), 30); ++i)
		{
			const FVector D2 = (T.Path[i] - Prev).GetSafeNormal2D(); const float Seg = FVector::Dist2D(T.Path[i], Prev);
			if (Seg < 1.f) continue;
			Turn += FMath::Abs(FMath::FindDeltaAngleDegrees(PD.Rotation().Yaw, D2.Rotation().Yaw)); Len += Seg; PD = D2; Prev = T.Path[i];
			if (Len > 1200.f) break;
		}
		if (Turn > 25.f && Len > 1.f) { const float Rcm = Len / FMath::DegreesToRadians(Turn); Want = FMath::Min(Want, FMath::Max(90.f, 0.6f * Rcm)); }
	}
	Want = FMath::Min(Want, FMath::Max(25.f, DistEnd * 0.5f));
	if (bBlocked) Want = 0.f;
	const float Acc = Want > T.Speed ? 80.f : 130.f;                       // (cm/s^2: gently away, gently stopping)
	T.Speed = FMath::FInterpConstantTo(T.Speed, Want, Dt, Acc);
	if (bLast && (DistEnd < 200.f || FMath::Abs(T.Orbit) > 330.f) && FMath::Abs(dYaw) > 45.f) T.At += (End - T.At).GetSafeNormal2D() * FMath::Min(T.Speed, 40.f) * Dt;
	else T.At += Move * T.Speed * Dt;
	Pose(T);
}

void AGSE::CartFollow(FCart& C, const FVector& Hitch)
{
	const FVector Fw = FRotator(0.f, C.Yaw, 0.f).Vector();
	const FVector Fax = C.At + Fw * GseCAxle.X, Rax = C.At - Fw * 80.f;
	FVector D = Hitch - Fax; D.Z = 0.f;
	if (D.SizeSquared() > 4.f) C.AxleYaw = D.Rotation().Yaw;
	const FVector FaxN = FVector(Hitch.X, Hitch.Y, C.At.Z) - FRotator(0.f, C.AxleYaw, 0.f).Vector() * GseCEyeToAxle;
	FVector B = FaxN - Rax; B.Z = 0.f;
	float Yaw = B.SizeSquared() > 1.f ? B.Rotation().Yaw : C.Yaw;
	// (the turntable stops at 75 degrees: the body pulled round)
	const float Rel = FMath::FindDeltaAngleDegrees(Yaw, C.AxleYaw);
	if (FMath::Abs(Rel) > 75.f) Yaw = C.AxleYaw - FMath::Sign(Rel) * 75.f;
	C.Yaw = Yaw; C.At = FaxN - FRotator(0.f, Yaw, 0.f).Vector() * GseCAxle.X;
	PoseCart(C);
}

void AGSE::Train(FTractor& T, float Dt)
{
	// the carts follow as a real trailer train: each cart's eye on the hitch in front of it, its turntable axle turned to
	// its drawbar's line, its body pulled after its axle (rear axle 1.6 m behind)
	FVector Hitch = TW(T, GseTPin);
	for (const int32 c : T.Train)
	{
		if (!Carts.IsValidIndex(c)) continue;
		CartFollow(Carts[c], Hitch);
		Hitch = CW(Carts[c], GseCJaw);
	}
	if (T.TowGpu != INDEX_NONE) GpuFollow(T.TowGpu, Hitch, T.bReverse, Dt);
}

FVector AGSE::GpuEye(int32 Gate) const
{
	const ATugs* G = Tg(); if (!G || !G->Tugs.IsValidIndex(Gate) || !G->Tugs[Gate].Gpu.M.IsValid()) return FVector::ZeroVector;
	return G->Tugs[Gate].Gpu.M->GetActorTransform().TransformPosition(GseGEye);
}

void AGSE::GpuFollow(int32 Gate, const FVector& Eye, bool bReverse, float Dt)
{
	ATugs* G = Tg(); if (!G || !G->Tugs.IsValidIndex(Gate)) return;
	ABabylonModel* M = G->Tugs[Gate].Gpu.M.Get(); if (!M) return;
	FGpuRig& R = GpuRigs.FindOrAdd(Gate);
	if (!R.bBound)
	{
		R.bBound = true; R.Home = M->GetActorLocation(); R.HomeYaw = M->GetActorRotation().Yaw; R.AxleYaw = R.HomeYaw; R.PrevAt = R.Home;
		for (int32 w = 0; w < 4; ++w)
		{
			TArray<FString> Names; for (const TCHAR* P : {TEXT("Tyre_"), TEXT("Rim_"), TEXT("WheelDisc_"), TEXT("Hub_"), TEXT("HubCap_"), TEXT("Nut_")}) Names.Add(FString(P) + GseWKey[w]);
			R.GWheel[w] = M->BindGroup(Names, true, FTransform(GseGWheelAt[w]));
		}
		R.GAxle = M->BindGroup({TEXT("FrontAxle_"), TEXT("Drawbar_")}, true, FTransform(GseGAxle));
	}
	FVector At = M->GetActorLocation(); float Yaw = M->GetActorRotation().Yaw;
	if (bReverse)
	{
		// (pushed straight back into its box: drawbar straight, the GPU backed along its own line)
		R.AxleYaw = Yaw; At = FVector(Eye.X, Eye.Y, At.Z) - FRotator(0.f, Yaw, 0.f).Vector() * GseGEye.X;
	}
	else
	{
		const FVector Fw = FRotator(0.f, Yaw, 0.f).Vector(); const FVector Fax = At + Fw * GseGAxle.X, Rax = At - Fw * 78.f;
		FVector D = Eye - Fax; D.Z = 0.f; if (D.SizeSquared() > 4.f) R.AxleYaw = D.Rotation().Yaw;
		const FVector FaxN = FVector(Eye.X, Eye.Y, At.Z) - FRotator(0.f, R.AxleYaw, 0.f).Vector() * GseGEyeToAxle;
		FVector B = FaxN - Rax; B.Z = 0.f; if (B.SizeSquared() > 1.f) Yaw = B.Rotation().Yaw;
		At = FaxN - FRotator(0.f, Yaw, 0.f).Vector() * GseGAxle.X;
	}
	At.Z = Plan(At.X / 100.f, -At.Y / 100.f).Z;
	const float Dist = FVector::Dist2D(At, R.PrevAt); const float Dir = FVector::DotProduct(At - R.PrevAt, FRotator(0.f, Yaw, 0.f).Vector()) >= 0.f ? 1.f : -1.f;
	R.Spin = FMath::Fmod(R.Spin + Dir * Dist / 28.f, 2.f * PI); R.PrevAt = At;
	M->SetActorLocationAndRotation(At, FRotator(0.f, Yaw, 0.f));
	const float Rel = FMath::Clamp(FMath::FindDeltaAngleDegrees(Yaw, R.AxleYaw), -40.f, 40.f);
	const FQuat Qa(FVector::UpVector, FMath::DegreesToRadians(Rel)), Spin(FVector::RightVector, R.Spin);
	if (R.GAxle != INDEX_NONE) M->MoveGroup(R.GAxle, FTransform(Qa, GseGAxle));
	for (int32 w = 0; w < 4; ++w) if (R.GWheel[w] != INDEX_NONE) M->MoveGroup(R.GWheel[w], w < 2 ? FTransform(Qa * Spin, GseGAxle + Qa.RotateVector(GseGWheelAt[w] - GseGAxle)) : FTransform(Spin, GseGWheelAt[w]));
	M->FlushMoves();
}

// ------------------------------------------------------------------ routes

void AGSE::Dubins(TArray<FVector>& Out, const FVector& From, float FromYaw, const FVector& To, float ToYaw, float R) const
{
	// the shortest way from one place and heading to another with turns no tighter than R (Dubins: turn, straight, turn),
	// sampled every 40 cm - the world's (x, y) as a plane, headings as the world's yaw
	const double Dx = (To.X - From.X) / R, Dy = (To.Y - From.Y) / R, D = FMath::Sqrt(Dx * Dx + Dy * Dy);
	const double Th = FMath::Atan2(Dy, Dx);
	auto Mod = [](double A) { A = FMath::Fmod(A, 2.0 * PI); return A < 0 ? A + 2.0 * PI : A; };
	const double A = Mod(FMath::DegreesToRadians(double(FromYaw)) - Th), B = Mod(FMath::DegreesToRadians(double(ToYaw)) - Th);
	const double Sa = FMath::Sin(A), Sb = FMath::Sin(B), Ca = FMath::Cos(A), Cb = FMath::Cos(B), Cab = FMath::Cos(A - B);
	struct FW { double T = -1, P = 0, Q = 0; int32 Ty = 0; } Best; double BestL = 1e18;
	// (types: 0 LSL, 1 RSR, 2 LSR, 3 RSL - left is a positive turn of the yaw)
	{ const double P2s = 2 + D * D - 2 * Cab + 2 * D * (Sa - Sb); if (P2s >= 0) { const double Tm = FMath::Atan2(Cb - Ca, D + Sa - Sb); FW W{Mod(-A + Tm), FMath::Sqrt(P2s), Mod(B - Tm), 0}; const double L = W.T + W.P + W.Q; if (L < BestL) { BestL = L; Best = W; } } }
	{ const double P2s = 2 + D * D - 2 * Cab + 2 * D * (Sb - Sa); if (P2s >= 0) { const double Tm = FMath::Atan2(Ca - Cb, D - Sa + Sb); FW W{Mod(A - Tm), FMath::Sqrt(P2s), Mod(-B + Tm), 1}; const double L = W.T + W.P + W.Q; if (L < BestL) { BestL = L; Best = W; } } }
	{ const double P2s = -2 + D * D + 2 * Cab + 2 * D * (Sa + Sb); if (P2s >= 0) { const double P = FMath::Sqrt(P2s); const double Tm = FMath::Atan2(-Ca - Cb, D + Sa + Sb) - FMath::Atan2(-2.0, P); FW W{Mod(-A + Tm), P, Mod(-Mod(B) + Tm), 2}; const double L = W.T + W.P + W.Q; if (L < BestL) { BestL = L; Best = W; } } }
	{ const double P2s = D * D - 2 + 2 * Cab - 2 * D * (Sa + Sb); if (P2s >= 0) { const double P = FMath::Sqrt(P2s); const double Tm = FMath::Atan2(Ca + Cb, D - Sa - Sb) - FMath::Atan2(2.0, P); FW W{Mod(A - Tm), P, Mod(B - Tm), 3}; const double L = W.T + W.P + W.Q; if (L < BestL) { BestL = L; Best = W; } } }
	if (Best.T < 0) { Out.Add(To); return; }
	static const int32 Dirs[4][3] = {{1, 0, 1}, {-1, 0, -1}, {1, 0, -1}, {-1, 0, 1}};
	double X = From.X, Y = From.Y, H = FMath::DegreesToRadians(double(FromYaw));
	const double Seg[3] = {Best.T * R, Best.P * R, Best.Q * R};
	const double Step = 40.0; double Since = 0.0;
	for (int32 k = 0; k < 3; ++k)
	{
		const int32 Dr = Dirs[Best.Ty][k];
		const int32 N = FMath::Max(1, FMath::CeilToInt(Seg[k] / 10.0)); const double Ds = Seg[k] / N;
		for (int32 i = 0; i < N; ++i)
		{
			if (Dr == 0) { X += FMath::Cos(H) * Ds; Y += FMath::Sin(H) * Ds; }
			else { const double Hm = H + Dr * Ds / R * 0.5; X += FMath::Cos(Hm) * Ds; Y += FMath::Sin(Hm) * Ds; H += Dr * Ds / R; }
			if ((Since += Ds) >= Step) { Since = 0.0; Out.Add(Plan(float(X / 100.0), float(-Y / 100.0))); }
		}
	}
	Out.Add(To);
}

void AGSE::Leg(TArray<FVector>& Out, float X0, float Y0, float Hx0, float Hy0, float X1, float Y1, float Hx1, float Hy1, float R) const
{
	float Y0w = 0.f, Y1w = 0.f; const FVector A = PlanPose(X0, Y0, Hx0, Hy0, Y0w), B = PlanPose(X1, Y1, Hx1, Hy1, Y1w);
	Dubins(Out, A, Y0w, B, Y1w, R);
}

namespace
{
	// the apron for a vehicle's way (plan metres): the stands' red lines (crossed only straight across, never followed), the
	// jetways' red zones, every aircraft on the ground (fuselage, wings, engines, tail - never under one), the other
	// vehicles, the terminal's wall and the taxilane
	struct GseFObs { FVector2f C, X; float Hl, Hw; };
	struct GseFApronMap
	{
		TArray<GseFObs> Boxes; TArray<TPair<FVector2f, FVector2f>> Lines; TArray<FBox2f> Zones;
		bool Hard(const FVector2f& P, float R) const
		{
			if (P.Y < 84.f || P.Y > 188.f) return true;
			for (const FBox2f& Z : Zones) if (P.X > Z.Min.X - R && P.X < Z.Max.X + R && P.Y > Z.Min.Y - R && P.Y < Z.Max.Y + R) return true;
			for (const GseFObs& B : Boxes)
			{
				const FVector2f D = P - B.C; const float A = FVector2f::DotProduct(D, B.X), S = D.X * -B.X.Y + D.Y * B.X.X;
				if (FMath::Abs(A) < B.Hl + R && FMath::Abs(S) < B.Hw + R) return true;
			}
			return false;
		}
		bool OnLine(const FVector2f& P, float R) const
		{
			for (const auto& L : Lines)
			{
				const FVector2f AB = L.Value - L.Key; const float L2 = AB.SizeSquared();
				const float U = L2 > 0 ? FMath::Clamp(FVector2f::DotProduct(P - L.Key, AB) / L2, 0.f, 1.f) : 0.f;
				if (FVector2f::Distance(P, L.Key + AB * U) < R) return true;
			}
			return false;
		}
	};
	struct GseFHeapLess { bool operator()(const TPair<float, int32>& A, const TPair<float, int32>& B) const { return A.Key < B.Key; } };
}

bool AGSE::Apron(TArray<FVector>& Out, const FVector& FromW, const FVector& ToW, const FTractor* Self) const
{
	ADP_PERF_SCOPE("GSE: a way across the apron (once per trip)", 25.f);
	GseFApronMap Mp;
	const FAirfieldData& F = AAirfield::Shared();
	{
		// the boundary lines as Airfield.cpp draws them (wingtips + 4.5 m, one shared line between close neighbours)
		TArray<const FAirfieldData::FStand*> Row; for (const FAirfieldData::FStand& St : F.Stands) if (!St.bCargo) Row.Add(&St);
		Row.Sort([](const FAirfieldData::FStand& A, const FAirfieldData::FStand& B) { return A.Nose != B.Nose ? A.Nose < B.Nose : A.X < B.X; });
		TArray<float> Lf, Rt; for (const auto* S : Row) { Lf.Add(S->X - S->Span / 2 - 4.5f); Rt.Add(S->X + S->Span / 2 + 4.5f); }
		for (int32 k = 0; k + 1 < Row.Num(); ++k)
			if (Row[k]->Nose == Row[k + 1]->Nose && Row[k]->Dir == Row[k + 1]->Dir && Rt[k] > Lf[k + 1] - 6.f) { const float Mid = (Row[k]->X + Row[k + 1]->X) / 2; Rt[k] = Mid; Lf[k + 1] = Mid; }
		for (int32 k = 0; k < Row.Num(); ++k)
		{
			const FAirfieldData::FStand& St = *Row[k]; const float B = -St.Dir, Wall = 82.6f, Te = St.Nose + B * (St.Len + 3.f);
			for (const float Bx : {Lf[k], Rt[k]}) Mp.Lines.Add(TPair<FVector2f, FVector2f>(FVector2f(Bx, Wall), FVector2f(Bx, Te)));
			if (St.bJetway) Mp.Zones.Add(FBox2f(FVector2f(St.X + 3.f, FMath::Min(Wall, St.Nose + B * 9.f)), FVector2f(St.X + 16.f, FMath::Max(Wall, St.Nose + B * 9.f))));
		}
	}
	// the aircraft (their models tagged SIM/ parked, MOVE/ moving; the actor at the nose wheel, the nose its -y): an A320's
	// shape, scaled for a bigger stand's aircraft
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		bool bPlane = false; for (const FName& Tg2 : It->Tags) { const FString S = Tg2.ToString(); if (S.StartsWith(TEXT("SIM/")) || S.StartsWith(TEXT("MOVE/"))) bPlane = true; }
		if (!bPlane) continue;
		const FVector2f N = GseP2(It->GetActorLocation());
		const FVector Nw = -It->GetActorRightVector(); FVector2f Fa(Nw.X, -Nw.Y); Fa = Fa.GetSafeNormal(); if (Fa.IsNearlyZero()) Fa = FVector2f(0.f, -1.f);
		const FVector2f Sd(-Fa.Y, Fa.X);
		float k = 1.f; for (const FAirfieldData::FStand& St : F.Stands) if (FMath::Abs(N.X - St.X) < 6.f && N.Y > St.Nose - 4.f && N.Y < St.Nose + 14.f) k = FMath::Max(1.f, St.Span / 36.f);
		Mp.Boxes.Add({N - Fa * (13.65f * k), Fa, 18.85f * k + 0.5f, 2.2f * k});            // the fuselage, nose tip to tail
		Mp.Boxes.Add({N - Fa * (15.5f * k), Fa, 4.5f * k, 18.f * k + 0.5f});               // the wings
		Mp.Boxes.Add({N - Fa * (31.f * k), Fa, 2.5f * k, 6.5f * k + 0.5f});                // the tailplane
		for (const float s : {-1.f, 1.f}) Mp.Boxes.Add({N - Fa * (6.5f * k) + Sd * (5.75f * k * s), Fa, 3.f * k, 1.3f * k});   // the engines
	}
	// the other vehicles: tugs, GPUs (not one being towed), the other tractors and carts
	if (const ATugs* G = Tg())
		for (int32 i = 0; i < G->Tugs.Num(); ++i)
		{
			const ATugs::FTug& U = G->Tugs[i];
			if (U.M.IsValid()) { const FVector Fw = FRotator(0.f, U.Yaw, 0.f).Vector(); Mp.Boxes.Add({GseP2(U.At), FVector2f(Fw.X, -Fw.Y), 2.55f, 1.26f}); }
			if (U.Gpu.M.IsValid() && !(Self && Self->TowGpu == i)) { const FVector Fw = U.Gpu.M->GetActorForwardVector(); Mp.Boxes.Add({GseP2(U.Gpu.M->GetActorTransform().TransformPosition(FVector(40.f, 0.f, 0.f))), FVector2f(Fw.X, -Fw.Y), 1.7f, 0.9f}); }
		}
	for (const FTractor& O : Tractors) if (&O != Self && O.M.IsValid()) { const FVector Fw = FRotator(0.f, O.Yaw, 0.f).Vector(); Mp.Boxes.Add({GseP2(O.At), FVector2f(Fw.X, -Fw.Y), O.Svc != INDEX_NONE ? O.KHalfL / 100.f : 1.72f, O.Svc != INDEX_NONE ? O.KHalfW / 100.f : 1.f}); }
	for (const FCart& C : Carts) if (C.M.IsValid() && !(Self && Self->Train.Contains(int32(&C - Carts.GetData())))) { const FVector Fw = FRotator(0.f, C.Yaw, 0.f).Vector(); Mp.Boxes.Add({GseP2(C.At), FVector2f(Fw.X, -Fw.Y), 1.72f, 0.83f}); }
	// the grid (1.5 m) and the search: lines cost a lot to cross (so only where it must, and straight across)
	const FVector2f A = GseP2(FromW), Z = GseP2(ToW); const float Cl = 1.5f, Rv = 1.3f, RLine = 1.3f;
	const FVector2f Lo(FMath::Min(A.X, Z.X) - 60.f, 84.f), Hi(FMath::Max(A.X, Z.X) + 60.f, 188.f);
	const int32 Nx = FMath::CeilToInt((Hi.X - Lo.X) / Cl), Ny = FMath::CeilToInt((Hi.Y - Lo.Y) / Cl);
	if (Nx <= 0 || Ny <= 0 || Nx * Ny > 200000) return false;
	auto At = [&](int32 x, int32 y) { return FVector2f(Lo.X + x * Cl, Lo.Y + y * Cl); };
	auto CellOf = [&](const FVector2f& P) { return FIntPoint(FMath::Clamp(FMath::RoundToInt((P.X - Lo.X) / Cl), 0, Nx - 1), FMath::Clamp(FMath::RoundToInt((P.Y - Lo.Y) / Cl), 0, Ny - 1)); };
	TArray<uint8> Bl; Bl.SetNumZeroed(Nx * Ny); TArray<uint8> Ln; Ln.SetNumZeroed(Nx * Ny);
	for (int32 y = 0; y < Ny; ++y) for (int32 x = 0; x < Nx; ++x) { const FVector2f P = At(x, y); Bl[y * Nx + x] = Mp.Hard(P, Rv) ? 1 : 0; Ln[y * Nx + x] = Mp.OnLine(P, RLine) ? 1 : 0; }
	const FIntPoint S = CellOf(A), G2 = CellOf(Z);
	Bl[S.Y * Nx + S.X] = 0; Bl[G2.Y * Nx + G2.X] = 0;
	TArray<float> Cost; Cost.Init(1e9f, Nx * Ny); TArray<int32> From; From.Init(-1, Nx * Ny);
	TArray<TPair<float, int32>> Open; Cost[S.Y * Nx + S.X] = 0.f; Open.HeapPush(TPair<float, int32>(0.f, S.Y * Nx + S.X), GseFHeapLess());
	bool bFound = false;
	while (Open.Num())
	{
		TPair<float, int32> Top; Open.HeapPop(Top, GseFHeapLess());
		const int32 I = Top.Value; if (I == G2.Y * Nx + G2.X) { bFound = true; break; }
		const int32 x = I % Nx, y = I / Nx;
		for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx)
		{
			if (!dx && !dy) continue; const int32 X2 = x + dx, Y2 = y + dy; if (X2 < 0 || Y2 < 0 || X2 >= Nx || Y2 >= Ny) continue;
			const int32 J = Y2 * Nx + X2; if (Bl[J]) continue;
			const float Nc = Cost[I] + ((dx && dy) ? 1.414f : 1.f) * (Ln[J] ? 25.f : 1.f);
			if (Nc < Cost[J]) { Cost[J] = Nc; From[J] = I; Open.HeapPush(TPair<float, int32>(Nc + FVector2f::Distance(At(X2, Y2), Z) / Cl, J), GseFHeapLess()); }
		}
	}
	if (!bFound) { ADP_WARN(TEXT("GSE: no way across the apron from plan (%.0f, %.0f) to (%.0f, %.0f) - waiting"), A.X, A.Y, Z.X, Z.Y); return false; }
	TArray<FVector2f> Cells; for (int32 I = G2.Y * Nx + G2.X; I >= 0; I = From[I]) Cells.Insert(At(I % Nx, I / Nx), 0);
	Cells[0] = A; Cells.Last() = Z;
	// straightened: from each point on to the farthest one it sees clear, crossing no more lines than the grid's way did
	auto LineCells = [&](const FVector2f& P, const FVector2f& Q, bool& bHard) { int32 N = 0; bHard = false; const int32 K = FMath::Max(1, FMath::CeilToInt(FVector2f::Distance(P, Q) / 0.5f)); bool bWas = false;
		for (int32 i = 0; i <= K; ++i) { const FVector2f M = FMath::Lerp(P, Q, float(i) / K); if (Mp.Hard(M, Rv)) bHard = true; const bool bL = Mp.OnLine(M, RLine); if (bL && !bWas) ++N; bWas = bL; } return N; };
	TArray<int32> Crossed; Crossed.SetNumZeroed(Cells.Num());
	for (int32 i = 1; i < Cells.Num(); ++i) { const int32 Ia = CellOf(Cells[i - 1]).Y * Nx + CellOf(Cells[i - 1]).X, Ib = CellOf(Cells[i]).Y * Nx + CellOf(Cells[i]).X; Crossed[i] = Crossed[i - 1] + (Ln[Ib] && !Ln[Ia] ? 1 : 0); }
	TArray<FVector2f> Way = {Cells[0]}; int32 k = 0;
	while (k < Cells.Num() - 1)
	{
		int32 Far = k + 1;
		for (int32 j = Cells.Num() - 1; j > k + 1; --j) { bool bH = false; const int32 Nl = LineCells(Cells[k], Cells[j], bH); if (!bH && Nl <= Crossed[j] - Crossed[k]) { Far = j; break; } }
		Way.Add(Cells[Far]); k = Far;
	}
	// every metre, for the driving
	for (int32 i = 1; i < Way.Num(); ++i)
	{
		const int32 K = FMath::Max(1, FMath::CeilToInt(FVector2f::Distance(Way[i - 1], Way[i]) / 1.f));
		for (int32 j = 1; j <= K; ++j) { const FVector2f M = FMath::Lerp(Way[i - 1], Way[i], float(j) / K); Out.Add(Plan(M.X, M.Y)); }
	}
	int32 NCross = 0; for (int32 i = 1; i < Way.Num(); ++i) { bool bH = false; NCross += LineCells(Way[i - 1], Way[i], bH); }
	ADP_LOG(TEXT("GSE: a way across the apron of %d legs, %.0f m, crossing %d stand lines (straight across) - from plan (%.0f, %.0f) to (%.0f, %.0f)"), Way.Num() - 1, Out.Num() * 1.f, NCross, A.X, A.Y, Z.X, Z.Y);
	return true;
}

void AGSE::RouteOut(FTractor& T, TFunction<void()> Then)
{
	// out of the lane through its roller door (x 128-136), along the baggage hall's south side (z 57: never across the
	// arrivals belt at z 63), north through Ramp services 13 (x 262-276) and out onto the apron
	const FVector2f P = GseP2(T.At);
	TArray<FVector> W;
	const float Hx = FMath::Cos(FMath::DegreesToRadians(T.Yaw)), Hy = -FMath::Sin(FMath::DegreesToRadians(T.Yaw));
	// (from the carts' rows: round to the door's line first; from further north - a tractor already facing the door - straight
	//  onto it)
	if (P.Y < 44.5f) { Leg(W, P.X, P.Y, Hx, Hy, 134.43f, 44.0f, 0.f, 1.f, GseTurnR); Leg(W, 134.43f, 44.0f, 0.f, 1.f, 134.43f, 53.5f, 0.f, 1.f, GseTurnR); }
	else Leg(W, P.X, P.Y, Hx, Hy, 134.43f, 53.5f, 0.f, 1.f, GseTurnR);
	Leg(W, 134.43f, 53.5f, 0.f, 1.f, 137.93f, 57.f, 1.f, 0.f, 350.f);
	// (out on the hall's south lane, z 54.4; back in on z 57: two trains pass each other)
	Leg(W, 137.93f, 57.f, 1.f, 0.f, 152.f, 54.4f, 1.f, 0.f, 400.f);
	Leg(W, 152.f, 54.4f, 1.f, 0.f, 265.f, 54.4f, 1.f, 0.f, 400.f);
	Leg(W, 265.f, 54.4f, 1.f, 0.f, 269.f, 58.4f, 0.f, 1.f, 400.f);
	Leg(W, 269.f, 58.4f, 0.f, 1.f, 269.f, 88.f, 0.f, 1.f, 400.f);
	ADP_LOG(TEXT("GSE: %s out of the lane, through the baggage hall and Ramp services 13 to the apron%s"), *T.Name, T.Train.Num() ? *FString::Printf(TEXT(" with %d carts"), T.Train.Num()) : TEXT(""));
	Go(T, MoveTemp(W), false, MoveTemp(Then));
}

void AGSE::RouteIn(FTractor& T, const FVector& ApronFrom, float FromYaw, TFunction<void()> Then)
{
	// back across the apron to Ramp services 13, south through it, west along the hall and in through the lane's door
	TArray<FVector> W;
	// (down to the passage east of the way out, x 275: one waiting there leaves the way out free)
	if (!Apron(W, ApronFrom, Plan(275.f, 114.f), &T)) { W.Add(Plan(275.f, 114.f)); }
	Leg(W, 275.f, 114.f, 0.f, -1.f, 275.f, 108.f, 0.f, -1.f, 400.f);
	Leg(W, 275.f, 108.f, 0.f, -1.f, 269.f, 93.f, 0.f, -1.f, 400.f);
	Leg(W, 269.f, 93.f, 0.f, -1.f, 269.f, 61.f, 0.f, -1.f, 400.f);
	Leg(W, 269.f, 61.f, 0.f, -1.f, 265.f, 57.f, -1.f, 0.f, 400.f);
	Leg(W, 265.f, 57.f, -1.f, 0.f, 137.f, 57.f, -1.f, 0.f, 400.f);
	Leg(W, 137.f, 57.f, -1.f, 0.f, 133.f, 53.f, 0.f, -1.f, 400.f);
	Leg(W, 133.f, 53.f, 0.f, -1.f, 133.f, 50.f, 0.f, -1.f, GseTurnR);
	ADP_LOG(TEXT("GSE: %s back to the baggage block"), *T.Name);
	Go(T, MoveTemp(W), false, MoveTemp(Then));
}

bool AGSE::StandGeo(float StandX, float& S, float& N, float& Span) const
{
	for (const FAirfieldData::FStand& St : AAirfield::Shared().Stands)
		if (FMath::Abs(St.X - StandX) < 1.f) { S = St.X; N = St.Nose + 5.1f * -St.Dir; Span = St.Span; return true; }   // (N: the nose wheel's stop, 5.1 m behind the stop line)
	return false;
}

bool AGSE::AircraftAt(float StandX) const
{
	float S, N, Sp; if (!StandGeo(StandX, S, N, Sp)) return false;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		for (const FName& Tg2 : It->Tags)
			if (Tg2.ToString().StartsWith(TEXT("SIM/")) || Tg2.ToString().StartsWith(TEXT("MOVE/")))
			{ const FVector2f P = GseP2(It->GetActorLocation()); if (FMath::Abs(P.X - S) < 8.f && FMath::Abs(P.Y - N) < 14.f) return true; break; }
	return false;
}

bool AGSE::PlaneNear(const FVector& At, float R) const
{
	if (const ATugs* G = Tg()) for (const FVector& Pl : G->Planes) if (FVector::Dist2D(Pl, At) < R) return true;
	return false;
}

bool AGSE::BusyAt(float StandX) const
{
	if (WaitingAt(StandX)) return true;   // (to-do 206: its bags wait for your people)
	for (const FTractor& T : Tractors)
		if (T.Svc == INDEX_NONE && FMath::Abs(T.StandX - StandX) < 1.f && (T.LYouX > -1e5f || T.Job == EJob::Out || T.Job == EJob::AtStand || T.Job == EJob::Loading))
		{
			// (never holding a departure for ever: a train stuck for 25 minutes lets it go - logged)
			if (T.JobT > 1500.f) { static TSet<FString> Told; if (!Told.Contains(T.Key)) { Told.Add(T.Key); ADP_WARN(TEXT("GSE: %s's train for %s is late - the departure goes without waiting for it"), *T.Name, *T.Key); } return false; }
			return true;
		}
	return false;
}

void AGSE::Outlines(TArray<FOutline>& Out) const
{
	for (const FTractor& T : Tractors) if (T.M.IsValid()) { const FVector F = FRotator(0.f, T.Yaw, 0.f).Vector(); Out.Add({T.At + F * T.KCentreX, F, FVector(-F.Y, F.X, 0.f), T.KHalfL, T.KHalfW}); }
	for (const FCart& C : Carts) if (C.M.IsValid()) { const FVector F = FRotator(0.f, C.Yaw, 0.f).Vector(); Out.Add({C.At - F * 2.f, F, FVector(-F.Y, F.X, 0.f), 140.f, GseCHalfW}); }
}

// ------------------------------------------------------------------ the frame

void AGSE::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bReady) Setup();
	Flash += Dt; Clock += Dt;
	WantTick(Dt);
	SvcTick(Dt);
	// the test's first jobs, once the crew stand at their gates
	if (bTest && !bTestStarted && Clock > 6.f && Tg())
	{
		bTestStarted = true;
		if (Tractors.Num() >= 4 && Carts.Num() >= 6) StartArrival(0, {4, 5}, 0.f, FString());   // (the arriving bags first)
		if (Tractors.Num() >= 4 && Carts.Num() >= 2) StartTrain(2, {0, 1});
		// (a GPU to move: a gate with no aircraft, its GPU unplugged - the nearest to the apron exit)
		int32 Best = INDEX_NONE; float BestD = 1e9f;
		if (const ATugs* G = Tg())
			for (int32 i = 0; i < G->Tugs.Num(); ++i)
			{
				const ATugs::FTug& U = G->Tugs[i];
				if (!U.Gpu.M.IsValid() || U.Gpu.bConnected || U.Gpu.bInHand || AircraftAt(U.StandX) || !U.Key.IsEmpty()) continue;
				const float D = FMath::Abs(U.StandX - 269.f); if (D < BestD) { BestD = D; Best = i; }
			}
		if (Best != INDEX_NONE && Tractors.Num() >= 4 && TractorFree(Tractors[3])) StartGpu(3, Best);
		else ADP_LOG(TEXT("GSE: no free GPU to move for the test (every gate has an aircraft or its GPU plugged in)"));
	}
	// (the roller doors open for the vehicles: anything driving within 22 m holds the nearest doors open - the first test
	//  drove the carts into the apron door at Ramp services 13, closed)
	if ((DoorClock += Dt) > 0.5f)
	{
		DoorClock = 0.f;
		for (const FTractor& T : Tractors) if (T.Path.Num() || FMath::Abs(T.Speed) > 10.f) ARollerDoor::OpenFor(GetWorld(), T.At, 2200.f, 10.f);
	}
	static const bool bChecks = bTest || FParse::Param(FCommandLine::Get(), TEXT("UITugShot")) || FParse::Param(FCommandLine::Get(), TEXT("TugCheck"));
	const bool bCheck = bChecks && (CheckClock += Dt) > 0.5f; if (bCheck) CheckClock = 0.f;
	const bool bSay = (SayClock += Dt) > 6.f; if (bSay) SayClock = 0.f;
	for (int32 i = 0; i < Tractors.Num(); ++i)
	{
		FTractor& T = Tractors[i];
		if (!T.M.IsValid()) continue;
		if (T.Until) { if (T.Until()) { T.Until = nullptr; TFunction<void()> F = MoveTemp(T.AfterUntil); T.AfterUntil = nullptr; if (F) F(); } }
		// (in short steps however long the frame is: the steering is worked out from where it is - long steps overshoot a bend)
		if (T.Path.Num()) { const int32 Ns = FMath::Clamp(FMath::CeilToInt(Dt / 0.03f), 1, 8); for (int32 n = 0; n < Ns && T.Path.Num(); ++n) { Drive(T, Dt / Ns); Train(T, Dt / Ns); } }
		if (SvcYours(T) && T.PlayerSeat == 0) { const int32 Ns = FMath::Clamp(FMath::CeilToInt(Dt / 0.03f), 1, 5); for (int32 n = 0; n < Ns; ++n) SvcPlayerDrive(T, FMath::Min(Dt, 0.15f) / Ns); }   // (you drive it: ServiceDrive.cpp)
		T.JobT += Dt;
		CabTick(T, i, Dt); Lamps(T); TractorParts(T, Dt);
		if (bCheck && (T.Path.Num() || T.Speed > 1.f)) Check(T, i);   // (moving only: a belt loader's bumper on the door sill touches the aircraft on purpose)
		if (bSay && T.Job != EJob::None)
		{
			static const TCHAR* JN[] = {TEXT("idle"), TEXT("the driver getting in"), TEXT("backing to the carts"), TEXT("coupling"), TEXT("towing out"), TEXT("at the stand"), TEXT("loading"), TEXT("towing back"), TEXT("uncoupling"), TEXT("to its box"), TEXT("to the GPU"), TEXT("coupling the GPU"), TEXT("moving the GPU"), TEXT("uncoupling the GPU")};
			const FVector2f P = GseP2(T.At);
			ADP_LOG(TEXT("GSE: %s %s at plan (%.1f, %.1f) facing %.0f deg, %.1f km/h, %d carts%s%s"), *T.Name, JN[uint8(T.Job)], P.X, P.Y, T.Yaw, T.MeasSpeed * 3.6f, T.Train.Num(), T.TowGpu != INDEX_NONE ? TEXT(", towing a GPU") : TEXT(""), T.Key.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" for %s"), *T.Key));
		}
	}
	for (FCart& C : Carts) CartParts(C, Dt);
	for (int32 i = 0; i < Tractors.Num(); ++i) if (Tractors[i].bLoader) { if (Tractors[i].Job == EJob::None && Tractors[i].M.IsValid()) YouLoader(i, Dt); LoaderBags(i, Dt); BeltTick(Tractors[i], Dt); }
	YouTractors(Dt);
	PollBags(Dt);
	// (crew plans asked for from inside a crew act)
	// (bags handed to the arrivals conveyor: gone as its own bag rides off)
	for (int32 i = PendingRemove.Num() - 1; i >= 0; --i) if (GetWorld()->GetTimeSeconds() >= PendingRemove[i].Key) { if (ADepartureBelts* Db = ADepartureBelts::Get(GetWorld())) Db->RemoveBag(PendingRemove[i].Value); PendingRemove.RemoveAt(i); }
	if (Deferred.Num()) { TArray<TFunction<void()>> D = MoveTemp(Deferred); Deferred.Reset(); for (TFunction<void()>& F : D) if (F) F(); }
}

// the test's own eyes (owner 2026-11-21: a vehicle on a red marking, under an aircraft or through anything is a fault -
// fix it and run again): twice a second each moving tractor and its carts against the stands' red lines (crossing one
// straight across is how the apron is crossed - logged, not a fault), the jetways' red zones, the aircraft on the ground,
// the terminal's wall (outside its openings), the other vehicles, and anything solid in its way (a ray along its front
// and sides at knee and chest height)
void AGSE::Check(const FTractor& T, int32 Index)
{
	struct FFoot { FString What; FVector At; float Yaw, Hl, Hw; };
	TArray<FFoot> Feet = {{T.Name, T.At + FRotator(0.f, T.Yaw, 0.f).Vector() * T.KCentreX, T.Yaw, T.KHalfL, T.KHalfW}};
	for (const int32 c : T.Train) if (Carts.IsValidIndex(c)) Feet.Add({FString::Printf(TEXT("%s's cart %d"), *T.Name, c + 1), Carts[c].At, Carts[c].Yaw, 140.f, GseCHalfW});
	const FAirfieldData& F = AAirfield::Shared();
	FString Hit, Note;
	TArray<const AActor*> Mine; Mine.Add(T.M.Get()); for (const int32 c : T.Train) if (Carts.IsValidIndex(c)) Mine.Add(Carts[c].M.Get());
	if (T.TowGpu != INDEX_NONE && Tg() && Tg()->Tugs.IsValidIndex(T.TowGpu)) Mine.Add(Tg()->Tugs[T.TowGpu].Gpu.M.Get());
	for (const FFoot& Ft : Feet)
	{
		const FVector Fw3 = FRotator(0.f, Ft.Yaw, 0.f).Vector(); const FVector2f Fw(Fw3.X, -Fw3.Y), Sd(-Fw.Y, Fw.X), P = GseP2(Ft.At);
		TArray<FVector2f> Pts; for (const float a : {-1.f, 0.f, 1.f}) for (const float b : {-1.f, 1.f}) Pts.Add(P + Fw * (a * Ft.Hl / 100.f) + Sd * (b * Ft.Hw / 100.f));
		const bool bIn = GseIndoors(P.X, P.Y);
		auto Seg1 = [](FVector2f Q, FVector2f A, FVector2f B) { const FVector2f AB = B - A; const float L2 = AB.SizeSquared(); const float U = L2 > 0 ? FMath::Clamp(FVector2f::DotProduct(Q - A, AB) / L2, 0.f, 1.f) : 0.f; return FVector2f::Distance(Q, A + AB * U); };
		if (!bIn)
		{
			// red lines and zones
			TArray<const FAirfieldData::FStand*> Row; for (const FAirfieldData::FStand& St : F.Stands) if (!St.bCargo) Row.Add(&St);
			Row.Sort([](const FAirfieldData::FStand& A, const FAirfieldData::FStand& B) { return A.Nose != B.Nose ? A.Nose < B.Nose : A.X < B.X; });
			TArray<float> Lf, Rt; for (const auto* S : Row) { Lf.Add(S->X - S->Span / 2 - 4.5f); Rt.Add(S->X + S->Span / 2 + 4.5f); }
			for (int32 k = 0; k + 1 < Row.Num(); ++k) if (Row[k]->Nose == Row[k + 1]->Nose && Row[k]->Dir == Row[k + 1]->Dir && Rt[k] > Lf[k + 1] - 6.f) { const float Mid = (Row[k]->X + Row[k + 1]->X) / 2; Rt[k] = Mid; Lf[k + 1] = Mid; }
			for (int32 k = 0; k < Row.Num(); ++k)
			{
				const FAirfieldData::FStand& St = *Row[k]; const float B = -St.Dir, Wall = 82.6f, Te = St.Nose + B * (St.Len + 3.f);
				for (const float Bx : {Lf[k], Rt[k]})
				{
					float D = 1e9f; for (const FVector2f& Q : Pts) D = FMath::Min(D, Seg1(Q, FVector2f(Bx, Wall), FVector2f(Bx, Te)));
					if (D < 0.1f) { if (FMath::Abs(Fw.X) > 0.8f) Note += FString::Printf(TEXT(" %s crossing a line of %s;"), *Ft.What, *St.Id); else Hit += FString::Printf(TEXT(" %s along the boundary line of %s;"), *Ft.What, *St.Id); }
				}
				if (St.bJetway)
				{
					const float Y0 = FMath::Min(Wall, St.Nose + B * 9.f), Y1 = FMath::Max(Wall, St.Nose + B * 9.f);
					for (const FVector2f& Q : Pts) if (Q.X > St.X + 3.f && Q.X < St.X + 16.f && Q.Y > Y0 && Q.Y < Y1) { Hit += FString::Printf(TEXT(" %s in the jetway red zone of %s;"), *Ft.What, *St.Id); break; }
				}
			}
			// the terminal's wall (the Ramp services 13 opening excepted)
			for (const FVector2f& Q : Pts) if (Q.Y < 83.f && !(Q.X > 262.4f && Q.X < 276.4f)) { Hit += FString::Printf(TEXT(" %s at the terminal wall;"), *Ft.What); break; }
		}
		// the aircraft
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			bool bPlane = false; for (const FName& Tg2 : It->Tags) { const FString S = Tg2.ToString(); if (S.StartsWith(TEXT("SIM/")) || S.StartsWith(TEXT("MOVE/"))) bPlane = true; }
			if (!bPlane) continue;
			const FVector2f N = GseP2(It->GetActorLocation());
			const FVector Nw = -It->GetActorRightVector(); FVector2f Fa(Nw.X, -Nw.Y); Fa = Fa.GetSafeNormal(); if (Fa.IsNearlyZero()) Fa = FVector2f(0.f, -1.f);
			const FVector2f Sa(-Fa.Y, Fa.X);
			for (const FVector2f& Q : Pts)
			{
				const FVector2f D = Q - N; const float Al = FVector2f::DotProduct(D, Fa), Ac = FVector2f::DotProduct(D, Sa);
				if (Al < 5.2f && Al > -32.5f && FMath::Abs(Ac) < 2.2f) { Hit += FString::Printf(TEXT(" %s under an aircraft's fuselage;"), *Ft.What); break; }
				// (the A320's swept wing from the nose wheel: leading edge 7.5 m behind it at the root, 15 m at the tip; trailing
				//  edge 13.8 m at the root, 16.6 m at the tip - each with half a metre to spare)
				const float Lat = FMath::Abs(Ac), Bh = -Al;
				if (Lat > 2.f && Lat < 18.4f && Bh > 7.f + 0.51f * (Lat - 2.f) && Bh < 14.3f + 0.19f * (Lat - 2.f)) { Hit += FString::Printf(TEXT(" %s under an aircraft's wing;"), *Ft.What); break; }
				if (Bh > 3.5f && Bh < 9.5f && FMath::Abs(Lat - 5.75f) < 1.3f) { Hit += FString::Printf(TEXT(" %s at an aircraft's engine;"), *Ft.What); break; }
			}
		}
		// the other vehicles
		auto Overlap = [&](const FVector& C2, float Yaw2, float Hl2, float Hw2)
		{
			const FVector F2 = FRotator(0.f, Yaw2, 0.f).Vector(); const FVector2f X2(F2.X, -F2.Y), Y2(-X2.Y, X2.X), C = GseP2(C2);
			for (const FVector2f& Q : Pts) { const FVector2f D = Q - C; if (FMath::Abs(FVector2f::DotProduct(D, X2)) < Hl2 / 100.f && FMath::Abs(FVector2f::DotProduct(D, Y2)) < Hw2 / 100.f) return true; }
			return false;
		};
		for (const FTractor& O : Tractors) if (&O != &T && O.M.IsValid() && Overlap(O.At + FRotator(0.f, O.Yaw, 0.f).Vector() * O.KCentreX, O.Yaw, O.KHalfL, O.KHalfW)) Hit += FString::Printf(TEXT(" %s into %s;"), *Ft.What, *O.Name);
		for (int32 c = 0; c < Carts.Num(); ++c) if (!T.Train.Contains(c) && Carts[c].M.IsValid() && Overlap(Carts[c].At, Carts[c].Yaw, 140.f, GseCHalfW)) Hit += FString::Printf(TEXT(" %s into baggage cart %d;"), *Ft.What, c + 1);
		if (const ATugs* G = Tg())
			for (int32 g = 0; g < G->Tugs.Num(); ++g)
			{
				const ATugs::FTug& U = G->Tugs[g];
				if (U.M.IsValid() && Overlap(U.At, U.Yaw, 255.f, 126.f)) Hit += FString::Printf(TEXT(" %s into tug %d;"), *Ft.What, g);
				if (g != T.TowGpu && U.Gpu.M.IsValid() && Overlap(U.Gpu.M->GetActorTransform().TransformPosition(FVector(-5.f, 0, 0)), U.Gpu.M->GetActorRotation().Yaw, 125.f, 88.f)) Hit += FString::Printf(TEXT(" %s into the GPU at gate x %.0f;"), *Ft.What, U.StandX);
			}
		// anything solid in its way: rays along its front (the way it moves) and its sides at 0.6 m and 1.5 m
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPGseCheck), false);
		for (const AActor* Me : Mine) if (Me) Q.AddIgnoredActor(Me);
		if (const ATugs* G = Tg()) for (const auto& Cr : G->Crew) if (Cr.A.IsValid()) Q.AddIgnoredActor(Cr.A.Get());
		const FVector Fo = FRotator(0.f, Ft.Yaw, 0.f).Vector(), Ri = FRotator(0.f, Ft.Yaw + 90.f, 0.f).Vector();
		const float Lead = (&Ft == &Feet[0] && T.bReverse) ? -1.f : 1.f;
		for (const float Hz : {60.f, 150.f})
		{
			const FVector Base = Ft.At + FVector(0, 0, Hz);
			const FVector Rays[3][2] = {{Base + Fo * Ft.Hl * Lead + Ri * Ft.Hw, Base + Fo * Ft.Hl * Lead - Ri * Ft.Hw}, {Base + Fo * Ft.Hl + Ri * Ft.Hw, Base - Fo * Ft.Hl + Ri * Ft.Hw}, {Base + Fo * Ft.Hl - Ri * Ft.Hw, Base - Fo * Ft.Hl - Ri * Ft.Hw}};
			for (const auto& R : Rays)
			{
				FHitResult H;
				if (GetWorld()->LineTraceSingleByChannel(H, R[0], R[1], ECC_Visibility, Q) && H.GetActor() && !H.GetActor()->IsA<APawn>())
				{ Hit += FString::Printf(TEXT(" %s through %s (%s) at %.1f m up;"), *Ft.What, *H.GetActor()->GetName(), H.GetComponent() ? *H.GetComponent()->GetName() : TEXT("-"), Hz / 100.f); break; }
			}
		}
	}
	const FVector2f P = GseP2(T.At);
	if (!Hit.IsEmpty()) ADP_WARN(TEXT("GSE: CHECK %s at plan (%.1f, %.1f):%s"), *T.Name, P.X, P.Y, *Hit);
	if (!Note.IsEmpty()) { static double Said = 0.0; if (FPlatformTime::Seconds() - Said > 3.0) { Said = FPlatformTime::Seconds(); ADP_LOG(TEXT("GSE: %s at plan (%.1f, %.1f):%s"), *T.Name, P.X, P.Y, *Note); } }
}

bool AGSE::WatchPoints(TArray<FVector>& Pts, FVector& Side) const
{
	// the job going on: the hand work first (coupling, the curtains), then a train on the move
	const FTractor* Pick = nullptr; int32 Rank = 0;
	for (const FTractor& T : Tractors)
	{
		if (T.Job == EJob::None || !T.M.IsValid()) continue;
		const int32 R = (T.Job == EJob::Coupling || T.Job == EJob::Loading || T.Job == EJob::Uncoupling || T.Job == EJob::GpuCouple || T.Job == EJob::GpuUncouple || T.Job == EJob::Boarding) ? 3 : T.Path.Num() ? 2 : 1;
		if (R > Rank) { Rank = R; Pick = &T; }
	}
	if (!Pick) return false;
	const FTractor& T = *Pick;
	const FVector F = FRotator(0.f, T.Yaw, 0.f).Vector(), Rt = FRotator(0.f, T.Yaw + 90.f, 0.f).Vector();
	Pts.Reset();
	for (const float a : {-T.KHalfL, T.KHalfL}) for (const float b : {-T.KHalfW - 18.f, T.KHalfW + 18.f}) Pts.Add(T.At + F * (a + T.KCentreX) + Rt * b);
	Pts.Add(T.At + FVector(0, 0, 220.f));
	for (const int32 c : T.Train) if (Carts.IsValidIndex(c)) { const FCart& C = Carts[c]; const FVector Cf = FRotator(0.f, C.Yaw, 0.f).Vector(); Pts.Add(C.At + Cf * 200.f); Pts.Add(C.At - Cf * 140.f + FVector(0, 0, 200.f)); }
	// (backing to its carts: the carts it is going to couple)
	if (T.Job == EJob::ToCarts || T.Job == EJob::Coupling) for (int32 c = 0; c < Carts.Num(); ++c) if (Carts[c].Tractor == INDEX_NONE && FVector::Dist2D(Carts[c].At, T.At) < 900.f) Pts.Add(Carts[c].At + FVector(0, 0, 180.f));
	if (T.TowGpu != INDEX_NONE && Tg() && Tg()->Tugs.IsValidIndex(T.TowGpu) && Tg()->Tugs[T.TowGpu].Gpu.M.IsValid()) { const FVector G = Tg()->Tugs[T.TowGpu].Gpu.M->GetActorLocation(); Pts.Add(G + FVector(0, 0, 180.f)); Pts.Add(G); }
	// (at the aircraft: its belt loader too - the boom's top at the hold door, the hold loader; making up: the carousel)
	if (T.IsTrain() && T.StandX != 0.f && (T.Job == EJob::AtStand || T.Job == EJob::Loading)) { const int32 Li = LoaderAt(T.StandX); if (Li != INDEX_NONE) { const FTractor& L = Tractors[Li]; Pts.Add(BoomW(L, FVector(400.f, 0.f, 200.f))); Pts.Add(TW(L, FVector(-390.f, 0.f, 0.f))); } }
	if (T.bLoader) { Pts.Add(BoomW(T, FVector(400.f, 0.f, 220.f))); if (const ATugs* G = Tg()) if (G->Crew.IsValidIndex(T.TopCrew) && G->Crew[T.TopCrew].A.IsValid()) Pts.Add(G->Crew[T.TopCrew].A->GetActorLocation() + FVector(0, 0, 185.f)); }
	if (T.IsTrain() && T.StandX == 0.f && T.Job == EJob::Loading) { Pts.Add(PickStand(int32(&T - Tractors.GetData()))); Pts.Add(PickStand(int32(&T - Tractors.GetData())) + FVector(0, 0, 150.f)); }
	Side = -Rt;   // (the driver's door side)
	if (const ATugs* G = Tg())
		if (G->Crew.IsValidIndex(T.Driver) && G->Crew[T.Driver].A.IsValid())
		{
			const FVector P = G->Crew[T.Driver].A->GetActorLocation(); Pts.Add(P); Pts.Add(P + FVector(0, 0, 185.f));
			if (!G->Crew[T.Driver].Hold) { const FVector D = (P - T.At).GetSafeNormal2D(); if (!D.IsNearlyZero()) Side = D; }
		}
	return true;
}

bool AGSE::AllDone() const
{
	if (!bWorked) return false;
	for (const FTractor& T : Tractors) if (T.Job != EJob::None) return false;
	if (const ATugs* G = Tg()) for (const auto& C : G->Crew) if (C.bGse) return false;
	return true;
}

void AGSE::Departure(const FString& Key, float StandX)
{
	if (Key.IsEmpty() || Served.Contains(Key)) return;
	float S, N, Sp; if (!StandGeo(StandX, S, N, Sp) || Sp > 40.f) return;   // (the contact stands' A320s; the big stands' trains come with their belt loaders and containers)
	Served.Add(Key);
	// a train being made up for this flight's bags (started by its bags coming: PollBags) learns its stand; one not yet given
	// a flight takes it (the test's first train goes to the next departure)
	for (FTractor& T : Tractors) if (T.IsTrain() && T.Key == Key && T.StandX == 0.f && T.Job != EJob::None) { T.StandX = StandX; ADP_LOG(TEXT("GSE: %s's train for %s - its stand at x %.0f"), *T.Name, *Key, StandX); return; }
	for (FTractor& T : Tractors) if (T.IsTrain() && !T.bArrival && (T.Job == EJob::Boarding || T.Job == EJob::ToCarts || T.Job == EJob::Coupling || T.Job == EJob::Loading || T.Job == EJob::Out) && T.Key.IsEmpty() && T.TowGpu == INDEX_NONE && T.StandX == 0.f)
	{ T.Key = Key; T.StandX = StandX; for (const int32 c : T.Train) if (Carts.IsValidIndex(c)) Carts[c].Flight = Key; ADP_LOG(TEXT("GSE: %s's train goes to %s (stand at x %.0f)"), *T.Name, *Key, StandX); return; }
	if (bTest) { if (!bTestStand) { bTestStand = true; TestKey = Key; TestStand = StandX; } return; }
	if (HiredFlight(Key, StandX)) { if (!BagKey.FindKey(Key)) AddWant(EWant::Dep, Key, StandX); return; }   // (to-do 206: your people)
	StartFor(Key, StandX);
}

bool AGSE::StartFor(const FString& Key, float StandX)
{
	// a free tractor and two free carts standing coupled-ready (a pair in a row, front to back: b's eye at a's rear jaw),
	// the front one's eye clear to back up to
	int32 Ti = INDEX_NONE; for (int32 t = 0; t < Tractors.Num() && Ti == INDEX_NONE; ++t) if (TractorFree(Tractors[t])) Ti = t;
	if (Ti == INDEX_NONE) { ADP_LOG(TEXT("GSE: no tractor free for %s's baggage"), *Key); return false; }
	for (int32 a = 0; a < Carts.Num(); ++a)
	{
		if (!CartFree(a)) continue;
		bool bFronted = false; for (int32 o = 0; o < Carts.Num(); ++o) if (o != a && FVector::Dist2D(CW(Carts[a], GseCEye), CW(Carts[o], GseCJaw)) < 30.f) bFronted = true;
		if (bFronted) continue;   // (another cart in front of it)
		for (int32 b = 0; b < Carts.Num(); ++b)
		{
			if (a == b || !CartFree(b) || FVector::Dist2D(CW(Carts[b], GseCEye), CW(Carts[a], GseCJaw)) > 30.f) continue;
			Tractors[Ti].Key = Key; Tractors[Ti].StandX = StandX;
			BagKey.Add(Ti, Key);
			StartTrain(Ti, {a, b});
			if (Tractors[Ti].Job == EJob::None) { Tractors[Ti].Key.Reset(); Tractors[Ti].StandX = 0.f; BagKey.Remove(Ti); return false; }
			return true;
		}
	}
	ADP_LOG(TEXT("GSE: no pair of free carts for %s's baggage"), *Key);
	return false;
}
