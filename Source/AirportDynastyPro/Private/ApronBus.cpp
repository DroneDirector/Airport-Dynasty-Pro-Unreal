// The apron buses, parked (to-do 277) - see ApronBus.h.
#include "ApronBus.h"
#include "ADPKeep.h"
#include "ADPLog.h"
#include "Airfield.h"
#include "BabylonModel.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sim/ADPSim.h"
#include "Site.h"
#include "TerminalPlan.h"

TWeakObjectPtr<AApronBuses> AApronBuses::One;

// the bus's own sizes (Tools/make_apron_bus.py; the model's root space, cm: Blender's (x, y, z) m at (x, -y, z) * 100)
namespace ApronBusDim
{
	constexpr float NameZ = 273.f, NameOut = 152.4f, NameSize = 17.f;   // (the cant rail's panel: 6.2 x 0.23 m)
}

AApronBuses::AApronBuses()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AApronBuses* AApronBuses::Get(UWorld* W)
{
	if (One.IsValid()) return One.Get();
	if (!W) return nullptr;
	for (TActorIterator<AApronBuses> It(W); It; ++It) { One = *It; return *It; }
	FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	One = W->SpawnActor<AApronBuses>(FVector::ZeroVector, FRotator::ZeroRotator, S);
	return One.Get();
}

void AApronBuses::BeginPlay()
{
	Super::BeginPlay();
	Load();
}

void AApronBuses::Load()
{
	if (bLoaded) return;
	bLoaded = true;
	// (the stands' list holds the remote ones only where they are built, or with -RemoteStands)
	if (!AAirfield::Shared().Stands.ContainsByPredicate([](const FAirfieldData::FStand& S) { return S.bRemote; }))
	{ ADP_LOG(TEXT("Apron buses: no remote stands - none here")); SetActorTickEnabled(false); return; }
	FString Text; TSharedPtr<FJsonObject> J;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/apron-buses.json"))) && !FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/apron-buses.json"))))
	{ ADP_WARN(TEXT("Apron buses: Data/apron-buses.json not found")); SetActorTickEnabled(false); return; }
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid() || !J->TryGetArrayField(TEXT("buses"), Rows))
	{ ADP_WARN(TEXT("Apron buses: Data/apron-buses.json is not readable")); SetActorTickEnabled(false); return; }
	for (const TSharedPtr<FJsonValue>& V : *Rows)
	{
		const TSharedPtr<FJsonObject>* O = nullptr; if (!V->TryGetObject(O)) continue;
		FBus B; (*O)->TryGetStringField(TEXT("name"), B.Name); (*O)->TryGetStringField(TEXT("bay"), B.Bay);
		double X = 0, Y = 0, H = 0;
		if (!(*O)->TryGetNumberField(TEXT("x"), X) || !(*O)->TryGetNumberField(TEXT("y"), Y)) { ADP_WARN(TEXT("Apron buses: %s has no place"), *B.Name); continue; }
		(*O)->TryGetNumberField(TEXT("heading"), H);
		B.X = float(X); B.Y = float(Y); B.Heading = float(H);
		Buses.Add(MoveTemp(B));
	}
	ADP_LOG(TEXT("Apron buses: %d of %d rows"), Buses.Num(), Rows->Num());
	if (!Buses.Num()) SetActorTickEnabled(false);
}

int32 AApronBuses::AddBus(const FString& Name, const FString& Bay, float X, float Y, float Heading)
{
	FBus B; B.Name = Name; B.Bay = Bay; B.X = X; B.Y = Y; B.Heading = Heading;
	SetActorTickEnabled(true); return Buses.Add(MoveTemp(B));
}

void AApronBuses::Adopt(int32 Ix, ABabylonModel* M)
{
	if (!Buses.IsValidIndex(Ix) || !M) return;
	FBus& B = Buses[Ix]; B.M = M;
	Materials(B);
	AirportName(B);
	PutLivery(B);
	int32 Wheels = 0, Doors = 0; FTransform T;
	for (const TCHAR* N : {TEXT("IK_Wheel_FL"), TEXT("IK_Wheel_FR"), TEXT("IK_Wheel_RL"), TEXT("IK_Wheel_RR")}) Wheels += M->NodeBase(N, T) ? 1 : 0;
	for (int32 I = 0; I < M->NodeCount(); ++I) Doors += M->NodeName(I).StartsWith(TEXT("PIV_Door_")) ? 1 : 0;
	ADP_LOG(TEXT("Apron buses: %s at %s (%.1f, %.1f) heading %.0f - %d lamps, %d wheels, %d door leaves"), *B.Name, *B.Bay, B.X, B.Y, B.Heading, B.Lamps.Num(), Wheels, Doors);
	SetLamps(B, EApronBusLamp::DoorWarn, 0.f);
}

void AApronBuses::Materials(FBus& B)
{
	ABabylonModel* M = B.M.Get(); if (!M) return;
	static UMaterialInterface* LensM = ADPKeep(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_TruckLens.M_TruckLens")));
	if (!LensM) ADP_WARN(TEXT("Apron buses: the lens material is missing"));
	auto OwnName = [](const UInstancedStaticMeshComponent* K, int32 Si) { const UStaticMesh* Sm = K->GetStaticMesh(); const UMaterialInterface* Own = Sm ? Sm->GetMaterial(Si) : nullptr; return Own ? Own->GetName() : FString(); };
	// the lit parts (the model's lenses, light strips and destination signs): the fire truck's lens, dark until switched on
	struct FLens { const TCHAR* Mat; FLinearColor BC; float Glow, Dots; };
	static const FLens Lens[] = {{TEXT("AB_RedLens"), FLinearColor(1.f, 0.04f, 0.02f), 30.f, 4.f}, {TEXT("AB_AmberLens"), FLinearColor(1.f, 0.45f, 0.03f), 55.f, 4.f},
		{TEXT("AB_WhiteLens"), FLinearColor(1.f, 0.97f, 0.9f), 28.f, 4.f}, {TEXT("AB_LEDStrip"), FLinearColor(1.f, 0.97f, 0.92f), 6.f, 20.f}, {TEXT("AB_DestinationLED"), FLinearColor(1.f, 0.62f, 0.1f), 8.f, 40.f}};
	TArray<UInstancedStaticMeshComponent*> Ps; M->Parts(Ps);
	TMap<FString, UMaterialInstanceDynamic*> Made; int32 Empty = 0;
	for (UInstancedStaticMeshComponent* K : Ps)
		for (int32 Si = 0; Si < K->GetNumMaterials(); ++Si)
		{
			const FString N = OwnName(K, Si); if (N.IsEmpty()) { ++Empty; continue; }
			if (N == TEXT("AB_Livery_Name")) { K->SetVisibility(false); K->SetHiddenInGame(true); continue; }   // (a name drawn into the model: the real one is written at run time)
			for (const FLens& L : Lens)
				if (LensM && N == L.Mat)
				{
					UMaterialInstanceDynamic*& D = Made.FindOrAdd(N);
					if (!D)
					{
						D = UMaterialInstanceDynamic::Create(LensM, M);
						D->SetVectorParameterValue(TEXT("BC"), L.BC); D->SetScalarParameterValue(TEXT("Glow"), L.Glow); D->SetScalarParameterValue(TEXT("Dots"), L.Dots);
					}
					K->SetMaterial(Si, D); K->SetNumCustomDataFloats(1);
				}
		}
	if (Empty) ADP_WARN(TEXT("Apron buses: %d empty material slots"), Empty);
	// the road dirt and the glass - but no road dirt on its ceiling, floor covering and grab poles, and lacquer on its paint
	// (the dirt itself: AGSE's, on every vehicle of its)
	for (UInstancedStaticMeshComponent* K : Ps)
		for (int32 Si = 0; Si < K->GetNumMaterials(); ++Si)
		{
			const FString N = OwnName(K, Si);
			if (N == TEXT("AB_CeilingLining") || N == TEXT("AB_FloorVinyl") || N == TEXT("AB_YellowGrip")) K->SetMaterial(Si, K->GetStaticMesh()->GetMaterial(Si));
			else if (N == TEXT("AB_Livery_Body") || N == TEXT("AB_Livery_Main"))
			{
				if (UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(K->GetMaterial(Si))) D->SetScalarParameterValue(TEXT("CC"), 0.35f);
			}
		}
	// each lamp on its own
	struct FWhich { const TCHAR* Prefix; EApronBusLamp Kind; };
	static const FWhich Which[] = {{TEXT("Headlamp_"), EApronBusLamp::Head}, {TEXT("DRL_"), EApronBusLamp::Drl}, {TEXT("IndicatorF_"), EApronBusLamp::Indicator}, {TEXT("IndicatorR_"), EApronBusLamp::Indicator},
		{TEXT("TailBrake_"), EApronBusLamp::TailBrake}, {TEXT("ThirdBrake_"), EApronBusLamp::ThirdBrake}, {TEXT("Reverse_"), EApronBusLamp::Reverse}, {TEXT("SideMarker_"), EApronBusLamp::Marker},
		{TEXT("MarkerTop"), EApronBusLamp::Marker}, {TEXT("Beacon"), EApronBusLamp::Beacon}, {TEXT("DoorWarn_"), EApronBusLamp::DoorWarn}};
	auto IsMat = [&OwnName](const UInstancedStaticMeshComponent* K, TFunctionRef<bool(const FString&)> Pick) { for (int32 Si = 0; K && Si < K->GetNumMaterials(); ++Si) if (Pick(OwnName(K, Si))) return true; return false; };
	auto AnyLens = [](const FString& N) { return N.EndsWith(TEXT("Lens")); };
	for (const FWhich& Wh : Which)
	{
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Found; M->InstancesNamed(Wh.Prefix, true, Found);
		for (const auto& F : Found)
			if (IsMat(F.Get<0>(), AnyLens))
			{
				const FString& N = F.Get<2>(); FLamp L; L.K = F.Get<0>(); L.Ix = F.Get<1>(); L.Kind = Wh.Kind;
				// (the side: the name's piece that starts with it, "..._L_Lens", "SideMarker_R2_Lens", "DoorWarn_L-1p55_Lens")
				TArray<FString> Bits; N.ParseIntoArray(Bits, TEXT("_"));
				for (int32 b = 1; b < Bits.Num() && !L.Side; ++b)
					if (Bits[b] != TEXT("Lens")) L.Side = Bits[b].StartsWith(TEXT("L")) ? 1 : Bits[b].StartsWith(TEXT("R")) ? -1 : 0;
				B.Lamps.Add(L);
			}
	}
	for (int32 I = 0; I < M->NodeCount(); ++I)
	{
		const TPair<UInstancedStaticMeshComponent*, int32> P = M->NodeInstance(I); if (!P.Key) continue;
		const bool bIn = IsMat(P.Key, [](const FString& N) { return N == TEXT("AB_LEDStrip"); }), bDest = !bIn && IsMat(P.Key, [](const FString& N) { return N == TEXT("AB_DestinationLED"); });
		if (!bIn && !bDest) continue;
		FLamp L; L.K = P.Key; L.Ix = P.Value; L.Kind = bIn ? EApronBusLamp::Inside : EApronBusLamp::Destination; B.Lamps.Add(L);
	}
}

void AApronBuses::SetLamps(FBus& B, EApronBusLamp Kind, float On, int32 Side)
{
	for (const FLamp& L : B.Lamps)
		if (L.Kind == Kind && (!Side || L.Side == Side))
			if (UInstancedStaticMeshComponent* K = L.K.Get()) K->SetCustomDataValue(L.Ix, 0, On, true);
}

void AApronBuses::AirportName(FBus& B)
{
	using namespace ApronBusDim;
	ABabylonModel* M = B.M.Get(); FString Name;
	if (!M || !ADPSite::AirportBlock()->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) return;
	for (const float Sd : {-1.f, 1.f})   // (left: the root space's -y)
	{
		UTextRenderComponent* X = NewObject<UTextRenderComponent>(M);
		X->SetupAttachment(M->GetRootComponent()); X->SetText(FText::FromString(Name.ToUpper() + TEXT(" AIRPORT")));
		X->SetHorizontalAlignment(EHTA_Center); X->SetVerticalAlignment(EVRTA_TextCenter);
		X->SetWorldSize(FMath::Min(NameSize, NameSize * 30.f / float(Name.Len() + 8))); X->SetTextRenderColor(FColor(16, 30, 64));   // (a long name smaller: it stays on its panel)
		X->SetRelativeLocationAndRotation(FVector(0.f, Sd * NameOut, NameZ), FRotator(0.f, Sd * 90.f, 0.f));
		X->SetCollisionEnabled(ECollisionEnabled::NoCollision); X->RegisterComponent(); M->AddInstanceComponent(X);
		B.Names.Add(X);
	}
}

void AApronBuses::PutLivery(FBus& B)
{
	// your airline's colours (FADPAirline LiveryBody / LiveryMain, "#rrggbb"; empty: the model's own), as AGSE::SvcLivery
	const UADPSim* Sim = UADPSim::Get(this); FString Bo, Mn;
	if (Sim) for (const FADPAirline& A : Sim->World.Airlines) if (A.bPlayer) { Bo = A.LiveryBody; Mn = A.LiveryMain; }
	const FString Want = Bo + TEXT("|") + Mn; ABabylonModel* M = B.M.Get();
	if (!M || Want == B.Livery || (B.Livery.IsEmpty() && Want.Len() < 2)) return;
	B.Livery = Want;
	TArray<UInstancedStaticMeshComponent*> Ps; M->Parts(Ps);
	for (UInstancedStaticMeshComponent* K : Ps)
		for (int32 i = 0; i < K->GetNumMaterials(); ++i)
		{
			const UStaticMesh* Sm = K->GetStaticMesh(); const UMaterialInterface* Own = Sm ? Sm->GetMaterial(i) : nullptr; if (!Own) continue;
			const FString N = Own->GetName(); const FString* C = N == TEXT("AB_Livery_Body") ? &Bo : N == TEXT("AB_Livery_Main") ? &Mn : nullptr;
			if (!C || C->IsEmpty()) continue;
			UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(K->GetMaterial(i)); if (!D) D = K->CreateDynamicMaterialInstance(i, K->GetMaterial(i));
			const FLinearColor L(FColor::FromHex(*C)); if (D) { D->SetVectorParameterValue(TEXT("BC"), L); D->SetVectorParameterValue(TEXT("BaseColorFactor"), L); }
		}
	// (the name readable on a dark body too)
	const bool bDark = !Bo.IsEmpty() && FLinearColor(FColor::FromHex(Bo)).GetLuminance() < 0.12f;
	for (const TWeakObjectPtr<UTextRenderComponent>& X : B.Names) if (X.IsValid()) X->SetTextRenderColor(bDark ? FColor(235, 238, 242) : FColor(16, 30, 64));
}

void AApronBuses::Tick(float Dt)
{
	Super::Tick(Dt);
	LiveryIn -= Dt; const bool bLivery = LiveryIn <= 0.f; if (bLivery) LiveryIn = 1.f;
	for (FBus& B : Buses)
	{
		if (!B.M.IsValid()) continue;
		if (bLivery) PutLivery(B);
	}
}
