// An aircraft hangar with its crew annex (Hangar.h), in four sizes.
#include "Hangar.h"
#include "TimerManager.h"
#include "BabylonModel.h"
#include "WashHose.h"
#include "ADPLog.h"
#include "TerminalBuilder.h"
#include "TerminalPlan.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"

#pragma region Sizes
namespace
{
	constexpr float RoofPitch = 2.5f * PI / 180.f;     // the roof's fall to each side, hidden behind the parapet
	constexpr float RefCandela = 230.f;             // a lamp in the terminal's hall (BabylonModel LightFittings) ...
	constexpr float RefHeight = 3.5f;               // ... at about this height over the floor
}

const TArray<FHangarSpec>& FHangarSpec::All()
{
	// clear inside width W x depth D, door W x DH: the aircraft's span and length with room to walk round (Blueprints/Hangars)
	static const TArray<FHangarSpec> A = {
		{TEXT("S"), TEXT("SMALL"), TEXT("Airbus A318 - Learjet class"), 44, 42, 15, 1.8f, 8, 6.f, 0.6f, false, 2.4f},
		{TEXT("M"), TEXT("MEDIUM"), TEXT("Airbus A320 class"), 46, 48, 15, 1.8f, 8, 6.f, 0.6f, false, 2.4f},
		{TEXT("L"), TEXT("LARGE"), TEXT("Airbus A321 class"), 46, 55, 15, 1.8f, 8, 6.1f, 0.6f, false, 2.4f},
		{TEXT("XL"), TEXT("EXTRA LARGE"), TEXT("Airbus A380 class"), 90, 84, 28, 3.0f, 12, 7.f, 1.0f, true, 4.0f},
	};
	return A;
}

const FHangarSpec& FHangarSpec::Get(const FString& Id)
{
	for (const FHangarSpec& S : All()) if (S.Id == Id) return S;
	return All()[1];
}

float FHangarSpec::Ridge() const { return Eave() + (W / 2 + Col) * FMath::Tan(RoofPitch); }
#pragma endregion

#pragma region Pieces

AHangar::AHangar()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UMaterialInterface* AHangar::Mat(const FName Name)
{
	if (TObjectPtr<UMaterialInterface>* F = Mats.Find(Name)) return *F;
	UMaterialInterface* M = nullptr;
	const FString N = Name.ToString();
	// the photo textures (Tools/set_hangar.py)
	if (N.StartsWith(TEXT("T:")))
	{
		const FString T = N.Mid(2);
		M = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Materials/Hangar/M_Hangar%s.M_Hangar%s"), *T, *T));
	}
	if (!M && (Name == "glass" || Name == "polyc"))
		if (UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glass.M_Glass")))
		{
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(G, this);
			if (Name == "polyc") { D->SetScalarParameterValue(TEXT("Opacity"), 0.55f); D->SetScalarParameterValue(TEXT("Roughness"), 0.35f); }
			M = D;
		}
	if (!M)
	{
		// plain colours (linear), as a copy of the kit's own surface; the photo textures' stand-ins until they are made
		struct FC { const TCHAR* Base; FLinearColor C; float R; };
		static const TMap<FName, FC> Col = {
			{"frame", {TEXT("steel"), FLinearColor(0.076f, 0.093f, 0.115f), 0.45f}},
			{"frameDark", {TEXT("steel"), FLinearColor(0.044f, 0.058f, 0.084f), 0.45f}},
			{"galv", {TEXT("steel"), FLinearColor(0.40f, 0.43f, 0.46f), 0.35f}},
			{"roofTop", {TEXT("steel"), FLinearColor(0.30f, 0.32f, 0.34f), 0.5f}},
			{"ral", {TEXT("white"), FLinearColor(0.58f, 0.62f, 0.59f), 0.4f}},
			{"blueGrey", {TEXT("white"), FLinearColor(0.10f, 0.19f, 0.30f), 0.4f}},
			{"glassDark", {TEXT("black"), FLinearColor(0.012f, 0.02f, 0.028f), 0.05f}},
			{"motorBlue", {TEXT("steel"), FLinearColor(0.05f, 0.11f, 0.21f), 0.4f}},
			{"orange", {TEXT("white"), FLinearColor(0.75f, 0.13f, 0.01f), 0.45f}},
			{"rackBlue", {TEXT("steel"), FLinearColor(0.02f, 0.035f, 0.07f), 0.45f}},
			{"card", {TEXT("white"), FLinearColor(0.48f, 0.25f, 0.10f), 0.85f}},
			{"binBlue", {TEXT("white"), FLinearColor(0.025f, 0.11f, 0.40f), 0.55f}},
			{"binGrey", {TEXT("white"), FLinearColor(0.15f, 0.16f, 0.17f), 0.55f}},
			{"cartBlue", {TEXT("white"), FLinearColor(0.19f, 0.29f, 0.48f), 0.55f}},
			{"worktop", {TEXT("white"), FLinearColor(0.88f, 0.88f, 0.86f), 0.2f}},
			{"T:Clad", {TEXT("white"), FLinearColor(0.62f, 0.65f, 0.70f), 0.42f}},
			{"T:Liner", {TEXT("white"), FLinearColor(0.70f, 0.70f, 0.68f), 0.6f}},
			{"T:Floor", {TEXT("concrete"), FLinearColor(0.06f, 0.056f, 0.048f), 0.28f}},
			{"T:Deck", {TEXT("steel"), FLinearColor(0.035f, 0.04f, 0.045f), 0.55f}},
			{"T:Corrugated", {TEXT("steel"), FLinearColor(0.34f, 0.37f, 0.42f), 0.38f}},
			{"T:Wood", {TEXT("walnut"), FLinearColor(0.16f, 0.08f, 0.035f), 0.45f}},
			{"T:Slats", {TEXT("steel"), FLinearColor(0.045f, 0.04f, 0.04f), 0.45f}},
			{"T:Asphalt", {TEXT("asphalt"), FLinearColor(0.15f, 0.13f, 0.12f), 0.9f}},
			{"alu", {TEXT("steel"), FLinearColor(0.55f, 0.57f, 0.60f), 0.3f}},
			{"amber", {TEXT("lamp"), FLinearColor(1.f, 0.42f, 0.04f), 0.4f}},
			{"exitGreen", {TEXT("lamp"), FLinearColor(0.08f, 0.9f, 0.25f), 0.4f}},
		};
		if (const FC* C = Col.Find(Name))
		{
			if (UMaterialInterface* Base = ATerminalBuilder::KitMaterial(this, C->Base))
			{
				UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, this);
				D->SetVectorParameterValue(TEXT("Color"), C->C);
				D->SetScalarParameterValue(TEXT("Roughness"), C->R);
				M = D;
			}
		}
		else M = ATerminalBuilder::KitMaterial(this, Name);
	}
	Mats.Add(Name, M);
	return M;
}

UInstancedStaticMeshComponent* AHangar::Kit(UStaticMesh* Mesh, const FName MatName, bool bCollide)
{
	USceneComponent* Par = Target ? Target : Root.Get();
	const FString Key = FString::Printf(TEXT("%s|%s|%d|%s"), *Mesh->GetName(), *MatName.ToString(), bCollide ? 1 : 0, *Par->GetName());
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Kits.Find(Key)) return *Found;
	UInstancedStaticMeshComponent* K = NewObject<UInstancedStaticMeshComponent>(this);
	K->SetupAttachment(Par);
	K->SetMobility(EComponentMobility::Movable);
	K->SetStaticMesh(Mesh);
	K->SetMaterial(0, Mat(MatName));
	K->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	if (MatName == "glass" || MatName == "polyc" || MatName == "lamp" || MatName == "amber" || MatName == "exitGreen") K->SetCastShadow(false);
	K->RegisterComponent();
	AddInstanceComponent(K);
	Kits.Add(Key, K);
	return K;
}

void AHangar::Box(const FName M, float X0, float Y0, float Z0, float X1, float Y1, float Z1, bool bCollide)
{
	const FVector A = U(FMath::Min(X0, X1), FMath::Min(Y0, Y1), FMath::Min(Z0, Z1)), B = U(FMath::Max(X0, X1), FMath::Max(Y0, Y1), FMath::Max(Z0, Z1));
	const FVector Size = (B - A).GetAbs();
	if (Size.X < 0.2f || Size.Y < 0.2f || Size.Z < 0.2f) return;
	// the engine cube is 100 cm across with its pivot in the middle
	Kit(Cube, M, bCollide)->AddInstance(FTransform(FRotator::ZeroRotator, (A + B) * 0.5f, Size / 100.f));
}

void AHangar::Beam(const FName M, const FVector& A, const FVector& B, float W, float H, bool bCollide)
{
	const FVector WA = U(A.X, A.Y, A.Z), WB = U(B.X, B.Y, B.Z), Dir = WB - WA;
	if (Dir.IsNearlyZero()) return;
	Kit(Cube, M, bCollide)->AddInstance(FTransform(Dir.Rotation(), (WA + WB) * 0.5f, FVector(Dir.Size() / 100.f, W, H)));
}

void AHangar::Rod(const FName M, const FVector& A, const FVector& B, float R, bool bCollide)
{
	const FVector WA = U(A.X, A.Y, A.Z), WB = U(B.X, B.Y, B.Z), Dir = WB - WA;
	if (Dir.IsNearlyZero()) return;
	// the engine cylinder stands along its own z, 100 cm across and tall
	Kit(Cylinder, M, bCollide)->AddInstance(FTransform(FRotationMatrix::MakeFromZ(Dir).Rotator(), (WA + WB) * 0.5f, FVector(R * 2, R * 2, Dir.Size() / 100.f)));
}

void AHangar::Cyl(const FName M, float X, float Y0, float Z, float R, float H, bool bCollide)
{
	Kit(Cylinder, M, bCollide)->AddInstance(FTransform(FRotator::ZeroRotator, U(X, Y0 + H / 2, Z), FVector(R * 2, R * 2, H)));
}

void AHangar::Lamp(float X, float Y, float Z, float Reach)
{
	// as bright on the floor as a lamp of the terminal's hall is on its floor: the light grows with the square of the
	// height it shines from (the same lux under it), soft-edged, warm-neutral like the photos' LEDs; no shadows (the sun
	// and sky keep theirs), dimmed by day as the terminal's lamps are
	USpotLightComponent* L = NewObject<USpotLightComponent>(this);
	L->SetupAttachment(Root);
	L->SetRelativeLocationAndRotation(U(X, Y, Z), FRotator(-90.f, 0.f, 0.f));
	L->SetIntensityUnits(ELightUnits::Candelas);
	const float Cd = RefCandela * FMath::Square(FMath::Max(Y, RefHeight) / RefHeight);
	L->SetIntensity(Cd);
	L->SetLightColor(FLinearColor(1.f, 0.96f, 0.9f));
	L->SetOuterConeAngle(62.f); L->SetInnerConeAngle(30.f);
	L->SetAttenuationRadius(Reach * 100.f);
	L->SetSourceRadius(20.f); L->SetSourceLength(110.f);
	L->SetCastShadows(false);
	L->RegisterComponent();
	AddInstanceComponent(L);
	Lights.Add(L); LightBase.Add(Cd);
}

void AHangar::Swing(float HX, float Y0, float HZ, bool bAlongX, float W, float H, float Th, const FName M, bool bFrosted)
{
	USceneComponent* Hinge = NewObject<USceneComponent>(this);
	Hinge->SetupAttachment(Root);
	Hinge->SetMobility(EComponentMobility::Movable);
	Hinge->SetRelativeLocation(U(HX, Y0, HZ));
	Hinge->RegisterComponent();
	AddInstanceComponent(Hinge);
	SwingNodes.Add(Hinge);
	// a part of the leaf (hinge space, metres: A along the leaf from the hinge, Y up)
	auto Part = [&](const FName Mm, float A, float Y, float Len, float Hh, float T, bool bCollide)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetupAttachment(Hinge);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Cube);
		C->SetMaterial(0, Mat(Mm));
		C->SetRelativeLocation(bAlongX ? FVector(A * 100.f, 0, Y * 100.f) : FVector(0, -A * 100.f, Y * 100.f));
		C->SetRelativeScale3D(bAlongX ? FVector(Len, T, Hh) : FVector(T, Len, Hh));
		C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		if (Mm == "glass" || Mm == "polyc") C->SetCastShadow(false);
		C->RegisterComponent();
		AddInstanceComponent(C);
	};
	Part(M, W / 2, H / 2, W - 0.01f, H, Th, true);
	if (bFrosted) Part("polyc", W / 2, H / 2 + 0.05f, W - 0.3f, H - 0.7f, Th + 0.015f, false);
	Part("chrome", W - 0.1f, 1.05f, 0.14f, 0.025f, Th + 0.12f, false);      // the lever handles, both sides
	FSwingDoor D;
	D.Hinge = Hinge;
	D.Mid = bAlongX ? U(HX + W / 2, Y0 + 1.f, HZ) : U(HX, Y0 + 1.f, HZ + W / 2);
	Swings.Add(D);
	SwingAlongX.Add(bAlongX);
}

UStaticMeshComponent* AHangar::Button(const FName M, const FVector& At, const FVector& Dir, float R, float Len, int32 What)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Root);
	C->SetStaticMesh(Cylinder);
	C->SetMaterial(0, Mat(M));
	const FVector A = U(At.X, At.Y, At.Z), B = U(At.X + Dir.X * Len, At.Y + Dir.Y * Len, At.Z + Dir.Z * Len);
	C->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromZ(B - A).Rotator(), (A + B) * 0.5f, FVector(R * 2, R * 2, Len)));
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->RegisterComponent();
	AddInstanceComponent(C);
	Buttons.Add(C); ButtonWhat.Add(What);
	return C;
}

#pragma endregion

#pragma region Build

void AHangar::Build(const FString& SizeId, float X, float ApronEdge, bool bDoorOpen)
{
	S = FHangarSpec::Get(SizeId);
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!Cube || !Cylinder) return;
	// the door line: the threshold's front edge on the apron's edge, the centre line on the lead-in line
	SetActorLocation(FTerminalPlan::ToWorld(X, 0.f, ApronEdge + S.Lip()));
	AZ0 = S.Back() - AnnexL;
	PrimaryActorTick.TickInterval = 0.f;
	DoorK = bDoorOpen ? 1.f : 0.f;
	// the wall doors sit in a bay's middle, clear of the columns: two in the east wall; in the west wall one ahead of the
	// annex and one into the annex's corridor; the store's roll-up door between two frames
	HallDoorZ = BayMid(AZ0 + 19.f, AZ0 + 3.f, AZ0 + 20.8f);
	RollupZ = BayMid(AZ0 + 27.f, AZ0 + 24.4f, AZ0 + 30.1f);
	DoorZE = {BayMid(S.D * 0.3f, 2.f, S.D - 2.f), BayMid(S.D * 0.62f, 2.f, S.D - 2.f)};
	DoorZW = {BayMid(AZ0 - 4.f, 2.f, AZ0 - 1.5f), HallDoorZ};
	BuildFloor();
	BuildFrame();
	BuildSkin();
	BuildDoor(bDoorOpen);
	BuildPersonnelDoors();
	BuildLampsAndServices();
	BuildKit();
	BuildAnnex();
	BuildAnnexRooms();
	// the wash hose (owner, 2026-11-14: the same one as the fire station's, working the same way, to wash vehicles in
	// here): on the east wall's inside face, in the first bay behind the door opening - on the wall, never on the doors
	{
		const float Hz = BayMid(2.5f, 1.f, 8.f), Hx = S.WallX() - 0.24f;   // (on the liner's face: BuildSkin's T:Liner, 12 cm in front of the wall)
		const FTransform At = FTransform(FRotator(0.f, -90.f, 0.f), FVector(Hx * 100.f, -Hz * 100.f, 0.f)) * GetActorTransform();
		FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABabylonModel* Hose = GetWorld()->SpawnActor<ABabylonModel>(At.GetLocation(), At.Rotator(), Sp);
		if (Hose && Hose->Place(TEXT("WashHose")))
		{
			if (AWashHose* Wh = GetWorld()->SpawnActor<AWashHose>(At.GetLocation(), At.Rotator(), Sp)) Wh->Setup(Hose);
		}
		else { if (Hose) Hose->Destroy(); ADP_WARN(TEXT("Hangar %s: the wash hose is missing"), *S.Id); }
	}
	// the door button panel (owner, 2026-11-14): inside, on the east wall as near the big door as it goes - a moment after
	// the hangar is up, when its walls can be found
	{
		TWeakObjectPtr<AHangar> WH = this;
		FTimerHandle Th;
		GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateLambda([WH]()
		{
			AHangar* Hg = WH.Get();
			if (!Hg) return;
			const FTransform HX = Hg->GetActorTransform();
			// (the inside of the walls is lined from 1 m up - BuildSkin's T:Liner, its face at WallX - 0.24, with no collision,
			// so no look finds it: the panel goes straight onto the liner's face, looking into the hangar)
			const FVector At = HX.TransformPosition(FVector((Hg->S.WallX() - 0.24f) * 100.f, -0.8f * 100.f, 0.f));
			const FVector In = HX.TransformVectorNoScale(FVector(-1.f, 0.f, 0.f));
			ABabylonModel* Pn = ABabylonModel::PlaceDoorPanelAt(Hg->GetWorld(), At, In, HX.GetLocation().Z);
			if (Pn) { Pn->Tags.Add(TEXT("HangarDoor")); Pn->SetOwner(Hg); }   // (its buttons work only this hangar's door)
			else ADP_WARN(TEXT("Hangar %s: no wall found for the door panel"), *Hg->S.Id);
		}), 1.0f, false);
	}
	ADP_LOG(TEXT("Hangar %s at x %.1f: %d kits, %d lights, door %s"), *S.Id, X, Kits.Num(), Lights.Num(), bDoorOpen ? TEXT("open") : TEXT("shut"));
}

float AHangar::BayMid(float Want, float Lo, float Hi) const
{
	const int32 NB = FMath::Max(4, FMath::RoundToInt(S.D / S.Bay));
	const float Bay = S.D / NB;
	float Best = FMath::Clamp(Want, Lo, Hi), BestD = 1e9f;
	for (int32 i = 0; i < NB; ++i)
	{
		const float M = (i + 0.5f) * Bay;
		if (M >= Lo && M <= Hi && FMath::Abs(M - Want) < BestD) { BestD = FMath::Abs(M - Want); Best = M; }
	}
	return Best;
}

bool AHangar::Press(const UPrimitiveComponent* C)
{
	const int32 I = Buttons.IndexOfByKey(C);
	if (I == INDEX_NONE) return false;
	const int32 W = ButtonWhat[I];
	DoorRun = W > 0 ? (DoorK < 1.f ? 1 : 0) : W < 0 ? (DoorK > 0.f ? -1 : 0) : 0;
	ADP_LOG(TEXT("Hangar %s door: %s at %.0f%%"), *S.Id, W > 0 ? TEXT("OPEN") : W < 0 ? TEXT("CLOSE") : TEXT("STOP"), DoorK * 100.f);
	return true;
}

void AHangar::Command(bool bOpen)
{
	const int32 Want = bOpen ? (DoorK < 1.f ? 1 : 0) : (DoorK > 0.f ? -1 : 0);
	if (DoorRun != 0 && Want != DoorRun) { DoorRun = 0; DoorPause = 1.f; DoorNext = Want; }   // (a hard stop, then the other way)
	else if (DoorPause <= 0.f) DoorRun = Want;
	else DoorNext = Want;
	ADP_LOG(TEXT("Hangar %s door: sent %s by its panel"), *S.Id, bOpen ? TEXT("up") : TEXT("down"));
}

void AHangar::Tick(float Dt)
{
	Super::Tick(Dt);
	if (DoorPause > 0.f) { DoorPause -= Dt; if (DoorPause <= 0.f) DoorRun = DoorNext; }
	// the main door: 10 minutes from shut to fully open, held by STOP; the beacons flash while it moves
	if (DoorRun != 0)
	{
		DoorK = FMath::Clamp(DoorK + DoorRun * Dt / 600.f, 0.f, 1.f);
		if (DoorK <= 0.f || DoorK >= 1.f) DoorRun = 0;
		for (const FLeaf& L : Leaves) if (L.Node) L.Node->SetRelativeLocation(FVector(L.Travel * DoorK, 0, 0));
	}
	Blink += Dt;
	const bool bFlash = DoorRun != 0 && FMath::Fmod(Blink, 1.f) < 0.5f;
	for (UInstancedStaticMeshComponent* B : Beacons) if (B && B->IsVisible() != bFlash) B->SetVisibility(bFlash);
	// the swinging doors: open away from whoever comes within reach, shut again behind them
	if (const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector At = GetActorTransform().InverseTransformPosition(P->GetActorLocation());
		for (int32 i = 0; i < Swings.Num(); ++i)
		{
			FSwingDoor& D = Swings[i];
			if (!D.Hinge) continue;
			const bool bNear = FVector::Dist2D(At, D.Mid) < 230.f && FMath::Abs(At.Z - D.Mid.Z) < 220.f;
			if (bNear && D.Open <= 0.f) D.Sign = SwingAlongX[i] ? (At.Y > D.Mid.Y ? -1.f : 1.f) : (At.X > D.Mid.X ? -1.f : 1.f);
			const float Want = bNear ? 1.f : 0.f;
			if (D.Open == Want) continue;
			D.Open = FMath::FInterpConstantTo(D.Open, Want, Dt, 1.4f);
			const float E = D.Open * D.Open * (3.f - 2.f * D.Open);
			D.Hinge->SetRelativeRotation(FRotator(0.f, D.Sign * 90.f * E, 0.f));
		}
	}
	// the lamps by daylight, as the terminal's (BabylonModel::LampsByDaylight): full after dark, a fifth by day
	static TWeakObjectPtr<ADirectionalLight> Sun;
	if (!Sun.IsValid()) Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	if (!Sun.IsValid() || !Lights.Num()) return;
	const float El = -Sun->GetActorRotation().Pitch;
	const float Scale = FMath::Lerp(1.f, 0.2f, FMath::SmoothStep(-6.f, 15.f, El));
	if (FMath::Abs(Scale - LightScale) < 0.005f) return;
	LightScale = Scale;
	for (int32 i = 0; i < Lights.Num(); ++i) if (Lights[i]) Lights[i]->SetIntensity(LightBase[i] * Scale);
}

#pragma endregion

#pragma region Floor and markings

void AHangar::BuildFloor()
{
	const float wx = S.WallX(), back = S.Back(), D = S.D, W = S.W;
	// the slab (polished concrete, the photos' floor), its threshold strip under the door, a concrete skirt round the walls
	Box("T:Floor", -wx, -0.4f, -0.12f, wx, 0.f, back);
	Box("concrete", -S.PocketX(), -0.4f, -S.Lip(), S.PocketX(), -0.01f, -0.12f);
	Box("concrete", -wx - 1.5f, -0.4f, -0.12f, -wx, -0.03f, back + 1.5f);
	Box("concrete", wx, -0.4f, -0.12f, wx + 1.5f, -0.03f, back + 1.5f);
	Box("concrete", -wx, -0.4f, back, wx, -0.03f, back + 1.5f);
	// the lead-in line inside: on the centre line, carrying on from the apron's own line (the airfield draws that one to
	// the threshold), to the stop bar where the nose wheel stops
	const float Y = 0.004f, Stop = D - 8.3f;
	Box("yellow", -0.075f, -0.01f, -S.Lip(), 0.075f, Y, Stop, false);
	Box("yellow", -2.f, -0.01f, Stop - 0.2f, 2.f, Y, Stop + 0.2f, false);
	// the walkways along the walls, the keep-clear line behind the door
	for (const float Sx : {-1.f, 1.f})
		for (const float Off : {1.2f, 2.6f}) Box("yellow", Sx * (W / 2 - Off) - 0.05f, -0.01f, 0.5f, Sx * (W / 2 - Off) + 0.05f, Y, D - 0.5f, false);
	Box("red", -W / 2 + 1, -0.01f, 1.15f, W / 2 - 1, Y, 1.25f, false);
}

#pragma endregion

#pragma region Frame

void AHangar::BuildFrame()
{
	const float W = S.W, D = S.D, E = S.Eave(), cd = S.Col, fl = cd * 0.5f, colX = W / 2 + cd / 2, colH = E - 0.15f;
	const int32 NB = FMath::Max(4, FMath::RoundToInt(D / S.Bay));
	const float Bay = D / NB;
	auto RoofY = [&](float X) { return E + (W / 2 + cd - FMath::Abs(X)) * FMath::Tan(RoofPitch); };
	for (int32 i = 0; i <= NB; ++i)
	{
		const float Z = i * Bay;
		for (const float Sx : {-1.f, 1.f})
		{
			// the column: an I-section (two flanges and the web), its base plate, grout and anchor bolts, the stiffeners
			const float X = Sx * colX;
			Box("frame", X - cd / 2, 0.06f, Z - fl / 2, X - cd / 2 + 0.025f, colH, Z + fl / 2);
			Box("frame", X + cd / 2 - 0.025f, 0.06f, Z - fl / 2, X + cd / 2, colH, Z + fl / 2);
			Box("frame", X - cd / 2, 0.06f, Z - 0.008f, X + cd / 2, colH, Z + 0.008f);
			Box("galv", X - cd / 2 - 0.12f, 0.f, Z - fl / 2 - 0.12f, X + cd / 2 + 0.12f, 0.06f, Z + fl / 2 + 0.12f);
			for (const float Bx : {-1.f, 1.f}) for (const float Bz : {-1.f, 1.f}) Cyl("galv", X + Bx * (cd / 2 + 0.06f), 0.06f, Z + Bz * (fl / 2 + 0.06f), 0.02f, 0.05f, false);
			for (const float Bz : {-1.f, 1.f}) Box("frame", X - 0.006f, 0.1f, Z + Bz * 0.02f, X + 0.006f, 0.4f, Z + Bz * (fl / 2 - 0.02f), false);
			if (!S.bTruss)
			{
				// the rafter up to the apex, its deeper tapered haunch at the eave, the bolted apex plate
				Beam("frame", FVector(X, E - 0.45f, Z), FVector(0, RoofY(0) - 0.45f, Z), fl, 0.75f);
				Beam("frame", FVector(X, E - 1.0f, Z), FVector(Sx * (colX - 2.2f), RoofY(colX - 2.2f) - 0.75f, Z), fl, 0.6f);
			}
			else
			{
				// a lattice truss: top chord along the roof, bottom chord level, verticals and diagonals between
				const float BotY = E - 3.0f;
				Beam("frame", FVector(X, RoofY(colX) - 0.3f, Z), FVector(0, RoofY(0) - 0.3f, Z), 0.35f, 0.5f);
				Beam("frame", FVector(X, BotY, Z), FVector(0, BotY, Z), 0.3f, 0.4f);
				const int32 P = 14;
				for (int32 k = 0; k < P; ++k)
				{
					const float Xa = X * (1 - float(k) / P), Xb = X * (1 - float(k + 1) / P);
					const float Ya = (k % 2) ? BotY : RoofY(Xa) - 0.5f, Yb = (k % 2) ? RoofY(Xb) - 0.5f : BotY;
					Beam("frame", FVector(Xa, Ya, Z), FVector(Xb, Yb, Z), 0.2f, 0.22f);
					Beam("frame", FVector(Xb, BotY, Z), FVector(Xb, RoofY(Xb) - 0.5f, Z), 0.18f, 0.18f);
				}
			}
		}
		if (!S.bTruss) Box("frame", -0.03f, RoofY(0) - 0.9f, Z - fl / 2 - 0.03f, 0.03f, RoofY(0), Z + fl / 2 + 0.03f, false);
	}
	// purlins under the roof (dark, as the photos), girts along the walls, the X-bracing in the end bays of the side walls
	// and of the back wall
	const float Span = W / 2 + cd;
	const int32 NP = FMath::CeilToInt(FMath::Sqrt(FMath::Square(Span) + FMath::Square(RoofY(0) - E)) / 1.7f);
	for (const float Sx : {-1.f, 1.f})
		for (int32 k = 0; k <= NP; ++k)
		{
			const float X = Sx * Span * (1 - float(k) / NP);
			if (FMath::Abs(X) < S.Sky + 0.2f) continue;
			Box("frameDark", X - 0.04f, RoofY(X) - 0.29f, 0.f, X + 0.04f, RoofY(X) - 0.05f, D, false);
		}
	const float WX = S.WallX();
	for (const float Sx : {-1.f, 1.f})
	{
		for (float Y = 2.2f; Y < E - 0.5f; Y += 2.1f) Box("frame", Sx * (WX - 0.23f), Y - 0.04f, 0.f, Sx * (WX - 0.03f), Y + 0.04f, D, false);
		for (const float Z0 : {0.f, D - Bay})
		{
			const float Xb = Sx * (W / 2 + cd + 0.03f);
			Rod("frame", FVector(Xb, 0.6f, Z0), FVector(Xb, E - 0.6f, Z0 + Bay), 0.05f);
			Rod("frame", FVector(Xb, E - 0.6f, Z0), FVector(Xb, 0.6f, Z0 + Bay), 0.05f);
		}
	}
	const float BZ = D + cd / 2 + 0.02f, PB = FMath::Min(W / 4, 12.f);
	for (const float Sx : {-1.f, 1.f})
	{
		const float Xa = Sx * (W / 2 - 0.3f), Xb = Sx * (W / 2 - 0.3f - PB);
		Rod("frame", FVector(Xa, 0.6f, BZ), FVector(Xb, E - 0.6f, BZ), 0.05f);
		Rod("frame", FVector(Xa, E - 0.6f, BZ), FVector(Xb, 0.6f, BZ), 0.05f);
		Box("frame", Xb - 0.2f, 0.06f, BZ - 0.15f, Xb + 0.2f, E - 0.2f, BZ + 0.15f, true);
	}
}

#pragma endregion

#pragma region Walls, roof and rooflight

void AHangar::BuildSkin()
{
	const float W = S.W, D = S.D, E = S.Eave(), Top = S.Top(), wx = S.WallX(), back = S.Back(), cd = S.Col;
	auto RoofY = [&](float X) { return E + (W / 2 + cd - FMath::Abs(X)) * FMath::Tan(RoofPitch); };
	for (const float Sx : {-1.f, 1.f})
	{
		// the side walls: a concrete plinth wall, white ribbed cladding outside up to the parapet, the white lining inside
		// (left open for the personnel doors: 1.1 m wide, 2.25 m tall)
		TArray<float> Cuts = {0.f};
		for (const float Dz : (Sx > 0 ? DoorZE : DoorZW)) { Cuts.Add(Dz - 0.56f); Cuts.Add(Dz + 0.56f); }
		Cuts.Add(back);
		for (int32 c = 0; c + 1 < Cuts.Num(); ++c)
		{
			const float Za = Cuts[c], Zb = Cuts[c + 1], Y0 = (c % 2) ? 2.25f : 0.f;
			if (!(c % 2)) Box("concrete", Sx * (wx - 0.25f), 0.f, Za, Sx * wx, 1.0f, Zb);
			Box("T:Clad", Sx * (wx - 0.04f), FMath::Max(1.0f, Y0), Za, Sx * (wx + 0.02f), Top, Zb);
			Box("T:Liner", Sx * (wx - 0.24f), FMath::Max(1.0f, Y0), FMath::Max(Za, 0.15f), Sx * (wx - 0.2f), E, FMath::Min(Zb, back - 0.2f), false);
			if (c % 2) { Box("frameDark", Sx * (wx - 0.26f), 2.25f, Za, Sx * (wx + 0.04f), 2.32f, Zb, false); for (const float Ze : {Za, Zb - 0.06f}) Box("frameDark", Sx * (wx - 0.26f), 0.f, Ze, Sx * (wx + 0.04f), 2.32f, Ze + 0.06f); }
		}
		Box("galv", Sx * (wx - 0.2f), Top, -0.1f, Sx * (wx + 0.08f), Top + 0.08f, back + 0.1f);   // the coping
		// downpipes at every second frame, with their brackets and shoes
		const int32 NB = FMath::Max(4, FMath::RoundToInt(D / S.Bay));
		for (int32 i = 0; i <= NB; i += 2)
		{
			const float Z = FMath::Min(back - 0.3f, i * D / NB + 0.3f);
			Cyl("galv", Sx * (wx + 0.12f), 0.15f, Z, 0.055f, E - 0.2f, false);
			for (float Y = 1.5f; Y < E; Y += 3.f) Box("galv", Sx * wx, Y - 0.02f, Z - 0.07f, Sx * (wx + 0.12f), Y + 0.02f, Z + 0.07f, false);
		}
	}
	// the back wall: plinth, cladding to the parapet, lining, coping
	Box("concrete", -wx, 0.f, back - 0.25f, wx, 1.0f, back);
	Box("T:Clad", -wx, 1.0f, back - 0.04f, wx, Top, back + 0.02f);
	Box("T:Liner", -wx + 0.24f, 1.0f, back - 0.24f, wx - 0.24f, E, back - 0.2f, false);
	Box("galv", -wx - 0.1f, Top, back - 0.2f, wx + 0.1f, Top + 0.08f, back + 0.1f);
	// the front over the door head: cladding from the eave to the parapet, lining inside
	Box("T:Clad", -wx, E, -0.02f, wx, Top, 0.04f);
	Box("T:Liner", -wx, E, 0.2f, wx, RoofY(0), 0.24f, false);
	Box("galv", -wx - 0.1f, Top, -0.1f, wx + 0.1f, Top + 0.08f, 0.2f);
	// the roof: two slopes, left open along the ridge from 8 % to 92 % of the depth for the rooflight; grey sheet outside,
	// the dark deck (the photos' roof seen from below) inside
	const float Z0 = D * 0.08f, Z1 = D * 0.92f, Sk = S.Sky, Ex = wx - 0.05f;
	for (const float Sx : {-1.f, 1.f})
	{
		auto Slope = [&](float Xa, float Xb, float Za, float Zb, const FName M, float Dy, float Th)
		{
			const float Zc = (Za + Zb) / 2, Wd = Zb - Za;
			Beam(M, FVector(Sx * Xa, RoofY(Xa) + Dy, Zc), FVector(Sx * Xb, RoofY(Xb) + Dy, Zc), Wd, Th);
		};
		Slope(Ex, Sk, 0.f, back, "roofTop", 0.05f, 0.06f);
		Slope(Sk, 0.f, 0.f, Z0, "roofTop", 0.05f, 0.06f);
		Slope(Sk, 0.f, Z1, back, "roofTop", 0.05f, 0.06f);
		Slope(wx - 0.2f, Sk, 0.f, back - 0.2f, "T:Deck", -0.02f, 0.04f);
		Slope(Sk, 0.f, 0.f, Z0, "T:Deck", -0.02f, 0.04f);
		Slope(Sk, 0.f, Z1, back - 0.2f, "T:Deck", -0.02f, 0.04f);
		// the rooflight: its glass raised on an upstand, the glazing bars every 1.2 m
		Slope(Sk, 0.f, Z0, Z1, "glass", 0.3f, 0.02f);
		for (float Z = Z0; Z <= Z1 + 0.01f; Z += 1.2f) Beam("alu", FVector(Sx * Sk, RoofY(Sk) + 0.32f, Z), FVector(0, RoofY(0) + 0.32f, Z), 0.05f, 0.06f, false);
		Box("alu", Sx * Sk - 0.06f, RoofY(Sk) - 0.05f, Z0, Sx * Sk + 0.06f, RoofY(Sk) + 0.3f, Z1);
	}
	Box("alu", -0.25f, RoofY(0) + 0.32f, Z0 - 0.1f, 0.25f, RoofY(0) + 0.44f, Z1 + 0.1f);
	Box("alu", -Sk, RoofY(Sk) - 0.05f, Z0 - 0.06f, Sk, RoofY(0) + 0.3f, Z0 + 0.06f);
	Box("alu", -Sk, RoofY(Sk) - 0.05f, Z1 - 0.06f, Sk, RoofY(0) + 0.3f, Z1 + 0.06f);
	// the obstruction light on the parapet's top corner, and the roof's own red light at the back
	Cyl("galv", wx - 0.3f, Top, -0.1f, 0.03f, 1.2f, false);
	Cyl("red", wx - 0.3f, Top + 1.2f, -0.1f, 0.09f, 0.16f, false);
	Cyl("galv", -wx + 0.3f, Top, back - 0.3f, 0.03f, 1.2f, false);
	Cyl("red", -wx + 0.3f, Top + 1.2f, back - 0.3f, 0.09f, 0.16f, false);
}

#pragma endregion

#pragma region The main door

void AHangar::BuildDoor(bool bOpen)
{
	const float W = S.W, H = S.DH, E = S.Eave(), wx = S.WallX(), lw = S.LeafW(), Fr = S.Front(), pX = S.PocketX();
	const int32 NT = S.Tracks();
	// the door head: a deep beam over the opening from pocket to pocket, clad outside, its dark fascia band
	Box("T:Clad", -pX, H, Fr - 0.3f, pX, E, Fr);
	Box("T:Liner", -wx, H, Fr, wx, E, 0.f, false);
	Box("frameDark", -pX, H - 0.12f, Fr - 0.32f, pX, H + 0.25f, Fr - 0.28f);
	// the top guides: one inverted channel per track under the head; the leaves' rollers run in it and hold them upright
	for (int32 T = 0; T < NT; ++T)
	{
		const float Z = S.TrackZ(T);
		Box("galv", -pX, H, Z - 0.11f, pX, H + 0.04f, Z + 0.11f);
		for (const float Dz : {-0.1f, 0.1f}) Box("galv", -pX, H - 0.24f, Z + Dz - 0.008f, pX, H, Z + Dz + 0.008f, false);
		for (float X = -pX + 0.3f; X < pX; X += 1.5f) Box("galv", X - 0.006f, H - 0.25f, Z - 0.13f, X + 0.006f, H + 0.05f, Z + 0.13f, false);
		// the floor rail, its base plate, bolted down
		Box("galv", -pX, 0.f, Z - 0.03f, pX, 0.05f, Z + 0.03f);
		Box("galv", -pX, -0.01f, Z - 0.08f, pX, 0.004f, Z + 0.08f, false);
	}
	// the drainage channel's grating in front of the door
	Box("black", -pX, -0.02f, Fr - 0.5f, pX, 0.004f, Fr - 0.2f, false);
	for (float X = -pX; X < pX; X += 0.25f) Box("galv", X - 0.01f, -0.01f, Fr - 0.5f, X + 0.01f, 0.006f, Fr - 0.2f, false);
	// the festoon track inside the head, with its flat cables in loops
	const float Fz = S.TrackZ(NT - 1) - 0.35f;
	Box("galv", -pX, H - 0.08f, Fz - 0.03f, pX, H - 0.02f, Fz + 0.03f, false);
	// the pockets the leaves stack into: clad outer side and top, corner posts, beacon, hazard stripes
	for (const float Sx : {-1.f, 1.f})
	{
		const float Xo = Sx * pX, Xi = Sx * wx;
		Box("T:Clad", Xo - Sx * 0.02f, 0.f, Fr - 0.3f, Xo + Sx * 0.04f, E, 0.f);
		Box("T:Clad", Xi, E - 0.3f, Fr - 0.3f, Xo, E, 0.f);
		for (const float Z : {Fr - 0.3f, -0.05f}) Box("frameDark", Xo - Sx * 0.3f, 0.f, Z - 0.15f, Xo, E, Z + 0.15f);
		Cyl("black", Xo - Sx * 0.15f, H + 0.3f, Fr - 0.5f, 0.09f, 0.08f, false);
		Cyl("amber", Xo - Sx * 0.15f, H + 0.38f, Fr - 0.5f, 0.08f, 0.16f, false);
		for (int32 k = 0; k < 8; ++k) Box(k % 2 ? "black" : "yellow", Xo - Sx * 0.32f, k * 0.25f, Fr - 0.47f, Xo + Sx * 0.01f, (k + 1) * 0.25f, Fr - 0.13f, false);
	}
	// the leaves: left half stack to the west pocket, right half to the east; each on its own track
	for (int32 i = 0; i < S.Leaves; ++i)
	{
		const bool bLeft = i < S.Leaves / 2;
		const int32 K = bLeft ? i : S.Leaves - 1 - i, T = NT - 1 - K;
		const float Z = S.TrackZ(T);
		const float Xs = -W / 2 + (W / S.Leaves) * (i + 0.5f), Xo = (bLeft ? -1.f : 1.f) * (wx + lw / 2 + 0.1f);
		// the leaf on its own node, built shut and slid along its rail by the door's drive (Tick)
		USceneComponent* LeafN = NewObject<USceneComponent>(this);
		LeafN->SetupAttachment(Root);
		LeafN->SetMobility(EComponentMobility::Movable);
		LeafN->RegisterComponent();
		AddInstanceComponent(LeafN);
		LeafNodes.Add(LeafN);
		Leaves.Add({LeafN, (Xo - Xs) * 100.f});
		LeafN->SetRelativeLocation(FVector((Xo - Xs) * 100.f * (bOpen ? 1.f : 0.f), 0, 0));
		Target = LeafN;
		const float X = Xs, Hh = H - 0.1f, Th = 0.2f, Wy = H * 0.62f, Wb = Wy - 0.58f, Wt = Wy + 0.58f;
		const float X0 = X - lw / 2, X1 = X + lw / 2;
		// its cladding outside and lining inside, broken by the band of windows; the frame, seals, inner bracing
		Box("T:Clad", X0, 0.06f, Z - Th / 2 - 0.02f, X1, Wb, Z - Th / 2);
		Box("T:Clad", X0, Wt, Z - Th / 2 - 0.02f, X1, Hh + 0.06f, Z - Th / 2);
		Box("T:Liner", X0, 0.06f, Z + Th / 2, X1, Wb, Z + Th / 2 + 0.02f, false);
		Box("T:Liner", X0, Wt, Z + Th / 2, X1, Hh + 0.06f, Z + Th / 2 + 0.02f, false);
		Box("polyc", X0 + 0.2f, Wb, Z - 0.01f, X1 - 0.2f, Wt, Z + 0.01f);
		for (int32 q = 0; q <= 4; ++q) { const float Mx = X0 + 0.2f + (lw - 0.4f) * q / 4; Box("frameDark", Mx - 0.03f, Wb, Z - Th / 2 - 0.02f, Mx + 0.03f, Wt, Z + Th / 2 + 0.02f, false); }
		for (const float Ex : {-1.f, 1.f})
		{
			Box("frameDark", X + Ex * (lw / 2 - 0.12f), 0.06f, Z - Th / 2, X + Ex * lw / 2, Hh + 0.06f, Z + Th / 2);
			Box("rubber", X + Ex * lw / 2, 0.06f, Z - Th / 2 - 0.03f, X + Ex * (lw / 2 + 0.03f), Hh + 0.06f, Z - Th / 2 + 0.02f, false);
		}
		Box("frameDark", X0, 0.06f, Z - Th / 2 - 0.01f, X1, 0.24f, Z + Th / 2 + 0.01f);
		Box("frameDark", X0, Hh - 0.08f, Z - Th / 2 - 0.01f, X1, Hh + 0.08f, Z + Th / 2 + 0.01f);
		Box("rubber", X0, 0.0f, Z - Th / 2, X1, 0.06f, Z - Th / 2 + 0.04f, false);
		for (const float Yb : {H * 0.25f, H * 0.45f, H * 0.8f}) Box("frameDark", X0 + 0.12f, Yb - 0.05f, Z + Th / 2 + 0.02f, X1 - 0.12f, Yb + 0.05f, Z + Th / 2 + 0.12f, false);
		Rod("frameDark", FVector(X0 + 0.15f, 0.3f, Z + Th / 2 + 0.07f), FVector(X1 - 0.15f, Wb - 0.1f, Z + Th / 2 + 0.07f), 0.03f);
		Rod("frameDark", FVector(X1 - 0.15f, 0.3f, Z + Th / 2 + 0.07f), FVector(X0 + 0.15f, Wb - 0.1f, Z + Th / 2 + 0.07f), 0.03f);
		Rod("frameDark", FVector(X0 + 0.15f, Wt + 0.1f, Z + Th / 2 + 0.07f), FVector(X1 - 0.15f, Hh - 0.2f, Z + Th / 2 + 0.07f), 0.03f);
		Rod("frameDark", FVector(X1 - 0.15f, Wt + 0.1f, Z + Th / 2 + 0.07f), FVector(X0 + 0.15f, Hh - 0.2f, Z + Th / 2 + 0.07f), 0.03f);
		// the meeting edges in the middle: hazard stripes
		if (i == S.Leaves / 2 - 1 || i == S.Leaves / 2)
		{
			const float Ex = i == S.Leaves / 2 - 1 ? 1.f : -1.f;
			for (int32 k = 0; k < 9; ++k) Box(k % 2 ? "black" : "yellow", X + Ex * (lw / 2 - 0.3f), 0.25f + k * 0.25f, Z - Th / 2 - 0.03f, X + Ex * lw / 2, 0.5f + k * 0.25f, Z + Th / 2 + 0.03f, false);
		}
		// the bottom: two bogies with flanged wheels on the rail; the drive bogie with its geared motor, terminal box
		// and the cable up to the leaf's junction box
		for (const float Bx : {-1.f, 1.f})
		{
			const float Gx = X + Bx * (lw / 2 - 0.55f);
			Box("frameDark", Gx - 0.35f, 0.04f, Z - Th / 2 - 0.06f, Gx + 0.35f, 0.32f, Z - Th / 2 - 0.03f, false);
			Box("frameDark", Gx - 0.35f, 0.2f, Z + Th / 2 + 0.03f, Gx + 0.35f, 0.32f, Z + Th / 2 + 0.06f, false);
			for (const float Wxx : {-0.22f, 0.22f})
			{
				Rod("galv", FVector(Gx + Wxx, 0.15f, Z - Th / 2 - 0.08f), FVector(Gx + Wxx, 0.15f, Z + Th / 2 + 0.08f), 0.03f);
				Rod("galv", FVector(Gx + Wxx, 0.15f, Z - 0.04f), FVector(Gx + Wxx, 0.15f, Z + 0.04f), 0.13f);
			}
			if (Bx > 0)
			{
				Rod("motorBlue", FVector(Gx, 0.27f, Z + Th / 2 + 0.15f), FVector(Gx, 0.27f, Z + Th / 2 + 0.6f), 0.13f);
				Box("motorBlue", Gx - 0.15f, 0.07f, Z + Th / 2 + 0.06f, Gx + 0.15f, 0.37f, Z + Th / 2 + 0.26f, false);
				Box("black", Gx + 0.04f, 0.37f, Z + Th / 2 + 0.26f, Gx + 0.2f, 0.47f, Z + Th / 2 + 0.38f, false);
				Rod("black", FVector(Gx + 0.12f, 0.47f, Z + Th / 2 + 0.32f), FVector(Gx + 0.12f, 2.6f, Z + Th / 2 + 0.12f), 0.016f);
			}
		}
		// its junction box, the cable riser up to the head, and the festoon cable from the head's track down to the riser
		const float Jx = X1 - 0.43f;
		Box("ral", Jx - 0.15f, 2.6f, Z + Th / 2 + 0.03f, Jx + 0.15f, 2.98f, Z + Th / 2 + 0.17f, false);
		Rod("galv", FVector(Jx, 2.98f, Z + Th / 2 + 0.1f), FVector(Jx, H - 0.3f, Z + Th / 2 + 0.1f), 0.018f);
		for (int32 q = 0; q < 6; ++q)
		{
			auto P = [&](float T) { return FVector(Jx - 1.2f + 1.2f * T, H - 0.08f - 0.45f * 4 * T * (1 - T), Fz); };
			Rod("black", P(q / 6.f), P((q + 1) / 6.f), 0.02f);
		}
		Rod("black", FVector(Jx, H - 0.08f, Fz), FVector(Jx, H - 0.3f, Z + Th / 2 + 0.1f), 0.02f);
		// the top: two guide-roller brackets, the rollers up inside the channel
		for (const float Bx : {-1.f, 1.f})
		{
			const float Gx = X + Bx * (lw / 2 - 0.6f);
			Box("galv", Gx - 0.12f, H - 0.17f, Z - 0.025f, Gx + 0.12f, H - 0.05f, Z + 0.025f, false);
			for (const float Dz : {-0.06f, 0.06f}) Cyl("black", Gx, H - 0.05f, Z + Dz, 0.04f, 0.1f, false);
		}
		// a wicket door in one middle leaf
		if (i == S.Leaves / 2)
		{
			Box("T:Clad", X0 + 0.5f, 0.1f, Z - Th / 2 - 0.035f, X0 + 1.5f, 2.2f, Z - Th / 2 - 0.02f, false);
			Box("frameDark", X0 + 0.45f, 2.2f, Z - Th / 2 - 0.04f, X0 + 1.55f, 2.26f, Z - Th / 2 - 0.02f, false);
			Rod("chrome", FVector(X0 + 1.3f, 1.05f, Z - Th / 2 - 0.09f), FVector(X0 + 1.45f, 1.05f, Z - Th / 2 - 0.09f), 0.015f);
		}
		Target = nullptr;
	}
	Beacons.Add(Kit(Cylinder, "amber", false));
	Beacons[0]->SetVisibility(false);
	// the door's controls outside: a grey box on the west pocket's front with OPEN (green), CLOSE (black) and the red
	// STOP mushroom, facing the apron (the panel inside is by the west pocket: BuildLampsAndServices)
	{
		const float Px = -pX + 0.7f, Pz = Fr - 0.3f;
		Box("ral", Px - 0.22f, 1.15f, Pz - 0.16f, Px + 0.22f, 1.75f, Pz);
		Button("green", FVector(Px - 0.1f, 1.6f, Pz - 0.16f), FVector(0, 0, -1), 0.032f, 0.03f, 1);
		Button("black", FVector(Px + 0.1f, 1.6f, Pz - 0.16f), FVector(0, 0, -1), 0.032f, 0.03f, -1);
		Cyl("yellow", Px, 1.3f - 0.06f, Pz - 0.165f, 0.06f, 0.005f, false);
		Button("red", FVector(Px, 1.3f, Pz - 0.16f), FVector(0, 0, -1), 0.045f, 0.06f, 0);
		Cyl("exitGreen", Px - 0.15f, 1.7f, Pz - 0.165f, 0.012f, 0.01f, false);
	}
}

#pragma endregion

#pragma region Personnel doors

void AHangar::BuildPersonnelDoors()
{
	const float wx = S.WallX(), D = S.D;
	auto Door = [&](float Sx, float Z, bool bIntoAnnex)
	{
		const float Xo = Sx * (wx + 0.03f), Xi = Sx * (wx - 0.26f), O = Sx;   // outside face, inside face, outward
		// the steel frame and leaf (grey), its hinges and lever handle; the card reader and lamp outside; the step and
		// canopy where it opens to the outside
		Box("frameDark", Xo, 0.f, Z - 0.62f, Xo + O * 0.04f, 2.3f, Z + 0.62f, false);
		Swing(Sx * (wx - 0.12f), 0.02f, Z - 0.5f, false, 1.0f, 2.1f, 0.05f, "ral");
		Box("black", Xo + O * 0.04f, 1.18f, Z + 0.78f, Xo + O * 0.07f, 1.32f, Z + 0.87f, false);
		Box("frameDark", Xo, 2.42f, Z - 0.15f, Xo + O * 0.1f, 2.6f, Z + 0.15f, false);
		Box("lamp", Xo + O * 0.1f, 2.45f, Z - 0.12f, Xo + O * 0.11f, 2.57f, Z + 0.12f, false);
		if (!bIntoAnnex)
		{
			Box("galv", Xo, 2.78f, Z - 0.85f, Xo + O * 0.9f, 2.83f, Z + 0.85f, false);
			Rod("galv", FVector(Xo + O * 0.85f, 2.8f, Z - 0.8f), FVector(Xo, 3.4f, Z - 0.8f), 0.015f);
			Rod("galv", FVector(Xo + O * 0.85f, 2.8f, Z + 0.8f), FVector(Xo, 3.4f, Z + 0.8f), 0.015f);
			Box("concrete", Xo, -0.15f, Z - 0.7f, Xo + O * 1.2f, 0.f, Z + 0.7f);
		}
		// inside: the leaf, the push bar, the closer, the green EXIT sign, the light switches and their conduit
		Box("galv", Xi - O * 0.04f, 2.33f, Z - 0.4f, Xi - O * 0.1f, 2.39f, Z - 0.1f, false);
		Box("exitGreen", Xi - O * 0.04f, 2.4f, Z - 0.2f, Xi - O * 0.07f, 2.56f, Z + 0.2f, false);
		Box("white", Xi - O * 0.02f, 1.2f, Z - 0.95f, Xi - O * 0.05f, 1.29f, Z - 0.77f, false);
		Rod("galv", FVector(Xi - O * 0.03f, 1.3f, Z - 0.86f), FVector(Xi - O * 0.03f, FMath::Min(6.f, S.Eave() - 3.f), Z - 0.86f), 0.012f);
	};
	// two on each side wall: on the east wall both to the outside; on the west wall one to the outside ahead of the
	// annex and one into the annex's corridor (the way to the crew lounge)
	Door(1.f, DoorZE[0], false);
	Door(1.f, DoorZE[1], false);
	Door(-1.f, DoorZW[0], false);
	Door(-1.f, DoorZW[1], true);
	(void)D;
	// the roll-up door from the hall into the annex's store bay, seen from the hall: slats, guides, drum hood, motor
	const float Z = RollupZ, X = -wx + 0.26f;
	Box("T:Slats", X - 0.03f, 0.f, Z - 2.f, X + 0.03f, 4.2f, Z + 2.f);
	Box("galv", X, 4.2f, Z - 2.1f, X + 0.55f, 4.8f, Z + 2.1f, false);
	Box("frameDark", X + 0.05f, 4.3f, Z + 2.15f, X + 0.4f, 4.7f, Z + 2.5f, false);
	for (const float E : {-2.06f, 2.06f}) Box("frameDark", X, 0.f, Z + E - 0.05f, X + 0.15f, 4.2f, Z + E + 0.05f, false);
}

#pragma endregion

#pragma region Lamps, trays, sockets, the doors controls

void AHangar::BuildLampsAndServices()
{
	const float W = S.W, D = S.D, E = S.Eave(), cd = S.Col;
	auto RoofY = [&](float X) { return E + (W / 2 + cd - FMath::Abs(X)) * FMath::Tan(RoofPitch); };
	const int32 NB = FMath::Max(4, FMath::RoundToInt(D / S.Bay));
	const float Bay = D / NB, LampY = E - (S.bTruss ? 2.0f : 2.4f), Step = S.bTruss ? 10.f : 8.f;
	const float Z0 = D * 0.08f, Z1 = D * 0.92f;
	// the rectangular twin-strip LED lamps (as the photos), on wires from the purlins, each fed by its cable from the
	// tray; a light of its own for each
	for (float Z = Bay / 2; Z < D; Z += Bay)
		for (float X = -W / 2 + Step / 2 + 1; X < W / 2 - 1; X += Step)
		{
			if (FMath::Abs(X) < S.Sky + 0.5f && Z > Z0 && Z < Z1) continue;
			Box("frameDark", X - 0.6f, LampY, Z - 0.24f, X + 0.6f, LampY + 0.09f, Z + 0.24f, false);
			for (const float Dz : {-0.12f, 0.12f}) Box("lamp", X - 0.56f, LampY - 0.01f, Z + Dz - 0.06f, X + 0.56f, LampY, Z + Dz + 0.06f, false);
			for (const float Dx : {-0.5f, 0.5f}) Rod("galv", FVector(X + Dx, LampY + 0.09f, Z), FVector(X + Dx, RoofY(X + Dx) - 0.3f, Z), 0.004f);
			Rod("black", FVector(X + 0.1f, LampY + 0.09f, Z + 0.1f), FVector(X + 0.1f, RoofY(X) - 0.42f, Z + 0.5f), 0.007f);
			Lamp(X, LampY - 0.05f, Z, LampY * 2.2f);
		}
	for (float X = -W / 2 + Step / 2 + 1; X < W / 2 - 1; X += Step)
	{
		if (FMath::Abs(X) < S.Sky + 0.5f) continue;
		Box("galv", X - 0.04f, RoofY(X) - 0.43f, 0.f, X + 0.16f, RoofY(X) - 0.42f, D, false);
	}
	// the cable trays along both side walls with their cables, sockets at every second column with their conduits
	const float TrayY = FMath::Min(6.f, E - 3.f);
	for (const float Sx : {-1.f, 1.f})
	{
		const float X = Sx * (W / 2 + 0.35f);
		Box("galv", X - 0.2f, TrayY - 0.01f, 0.f, X + 0.2f, TrayY + 0.01f, D, false);
		for (const float Ex : {-0.19f, 0.19f}) Box("galv", X + Ex - 0.01f, TrayY, 0.f, X + Ex + 0.01f, TrayY + 0.08f, D, false);
		for (int32 c = 0; c < 4; ++c) Rod(c == 2 ? "orange" : "black", FVector(X - 0.12f + c * 0.08f, TrayY + 0.04f, 0.2f), FVector(X - 0.12f + c * 0.08f, TrayY + 0.04f, D - 0.2f), 0.016f + (c % 2) * 0.006f);
		for (int32 i = 1; i < NB; i += 2)
		{
			const float Z = i * Bay + 0.45f, Xs = Sx * (W / 2 - 0.06f);
			Box("ral", Xs - Sx * 0.0f, 1.09f, Z - 0.16f, Xs - Sx * 0.12f, 1.51f, Z + 0.16f, false);
			Cyl("red", Xs - Sx * 0.15f, 1.32f, Z - 0.08f, 0.05f, 0.06f, false);
			Cyl("blueGrey", Xs - Sx * 0.15f, 1.32f, Z + 0.08f, 0.04f, 0.06f, false);
			Rod("galv", FVector(Sx * (W / 2 - 0.03f), 1.52f, Z + 0.1f), FVector(Sx * (W / 2 - 0.03f), TrayY, Z + 0.1f), 0.016f);
		}
	}
	// the main door's control panel inside by the west pocket: OPEN, CLOSE, RESET, LIGHTS, the emergency stop on its yellow
	// plate, the green and red lamps; its conduit up to the tray
	const float Px = -W / 2 + 0.12f, Pz = 1.4f;
	Box("ral", Px, 1.1f, Pz - 0.25f, Px + 0.22f, 1.8f, Pz + 0.25f);
	Button("green", FVector(Px + 0.22f, 1.62f, Pz - 0.12f), FVector(1, 0, 0), 0.028f, 0.03f, 1);       // OPEN
	Button("black", FVector(Px + 0.22f, 1.62f, Pz + 0.12f), FVector(1, 0, 0), 0.028f, 0.03f, -1);      // CLOSE
	Rod("blueGrey", FVector(Px + 0.22f, 1.43f, Pz - 0.12f), FVector(Px + 0.25f, 1.43f, Pz - 0.12f), 0.028f);   // RESET
	Rod("white", FVector(Px + 0.22f, 1.43f, Pz + 0.12f), FVector(Px + 0.25f, 1.43f, Pz + 0.12f), 0.028f);      // LIGHTS
	Rod("yellow", FVector(Px + 0.22f, 1.25f, Pz), FVector(Px + 0.226f, 1.25f, Pz), 0.06f);
	Button("red", FVector(Px + 0.226f, 1.25f, Pz), FVector(1, 0, 0), 0.045f, 0.06f, 0);               // STOP
	Rod("galv", FVector(Px + 0.11f, 1.8f, Pz), FVector(Px + 0.11f, 5.f, Pz), 0.02f);
	// the distribution board on the east wall near the door
	Box("ral", W / 2 + 0.05f, 1.2f, 3.4f, W / 2 + 0.35f, 3.2f, 4.6f);
	Box("black", W / 2 + 0.04f, 1.25f, 3.99f, W / 2 + 0.05f, 3.15f, 4.01f, false);
	for (int32 c = 0; c < 6; ++c) Rod("black", FVector(W / 2 + 0.2f, 3.2f, 3.6f + c * 0.16f), FVector(W / 2 + 0.2f, TrayY, 3.6f + c * 0.16f), 0.02f);
}

#pragma endregion

#pragma region Kit

void AHangar::BuildKit()
{
	const float W = S.W, D = S.D;
	// workbenches with vices and tool boards, red roll cabinets (east wall, middle)
	const float Bz = D * 0.42f, Bx = W / 2 - 0.5f;
	for (int32 q = 0; q < 2; ++q)
	{
		const float Z = Bz + q * 2.3f;
		Box("oak", Bx - 0.4f, 0.87f, Z - 1.f, Bx + 0.4f, 0.92f, Z + 1.f);
		Box("blueGrey", Bx - 0.38f, 0.f, Z - 0.98f, Bx + 0.38f, 0.87f, Z - 0.9f);
		Box("blueGrey", Bx - 0.38f, 0.f, Z + 0.9f, Bx + 0.38f, 0.87f, Z + 0.98f);
		Box("blueGrey", Bx - 0.35f, 0.44f, Z + 0.1f, Bx + 0.3f, 0.86f, Z + 0.85f);
		Box("blueGrey", Bx - 0.35f, 0.92f, Z - 0.8f, Bx - 0.1f, 1.05f, Z - 0.6f, false);
		Box("galv", Bx + 0.35f, 1.1f, Z - 1.f, Bx + 0.38f, 2.3f, Z + 1.f, false);
	}
	for (int32 q = 0; q < 2; ++q) { const float Z = Bz + 4.8f + q * 1.f; Box("red", Bx - 0.25f, 0.1f, Z - 0.45f, Bx + 0.3f, 1.15f, Z + 0.45f); for (int32 v = 0; v < 6; ++v) Box("chrome", Bx - 0.27f, 0.25f + v * 0.15f, Z - 0.2f, Bx - 0.25f, 0.27f + v * 0.15f, Z + 0.2f, false); }
	// ground power unit, nitrogen cart, tow bar, chocks, drums on a spill pallet, a scissor lift (west side, front half)
	{ const float X = -W / 2 + 3.5f, Z = D * 0.22f; Box("yellow", X - 0.55f, 0.3f, Z - 1.1f, X + 0.55f, 1.6f, Z + 1.1f); Box("frameDark", X - 0.6f, 0.2f, Z - 1.15f, X + 0.6f, 0.3f, Z + 1.15f);
		for (const float Dz : {-0.8f, 0.8f}) Rod("rubber", FVector(X - 0.62f, 0.2f, Z + Dz), FVector(X + 0.62f, 0.2f, Z + Dz), 0.2f); Rod("black", FVector(X + 0.56f, 1.f, Z + 0.9f), FVector(X + 2.5f, 0.05f, Z + 2.5f), 0.03f); }
	{ const float X = -W / 2 + 3.0f, Z = D * 0.36f; Box("blueGrey", X - 0.4f, 0.3f, Z - 0.8f, X + 0.4f, 0.4f, Z + 0.8f); for (int32 k = 0; k < 3; ++k) Cyl("green", X, 0.4f, Z - 0.45f + k * 0.42f, 0.12f, 1.55f); }
	{ const float X = -W / 2 + 6.0f, Z = D * 0.46f; Rod("yellow", FVector(X - 2.2f, 0.35f, Z), FVector(X + 2.2f, 0.35f, Z), 0.05f); for (const float E : {-0.3f, 0.3f}) Rod("rubber", FVector(X - 2.f, 0.12f, Z + E), FVector(X - 1.9f, 0.12f, Z + E), 0.12f); }
	for (int32 q = 0; q < 4; ++q) Box("yellow", -W / 2 + 2.7f + q * 0.6f, 0.f, D * 0.52f - 0.25f, -W / 2 + 3.f + q * 0.6f, 0.2f, D * 0.52f + 0.25f);
	{ const float X = -W / 2 + 1.5f, Z = D * 0.58f; Box("yellow", X - 0.65f, 0.f, Z - 0.65f, X + 0.65f, 0.3f, Z + 0.65f); for (int32 k = 0; k < 4; ++k) Cyl(k % 2 ? "blueGrey" : "red", X + (k % 2 ? 0.3f : -0.3f), 0.3f, Z + (k < 2 ? -0.3f : 0.3f), 0.28f, 0.9f); }
	{ const float X = -W / 2 + 7.5f, Z = D * 0.12f; Box("orange", X - 1.2f, 0.15f, Z - 0.55f, X + 1.2f, 0.45f, Z + 0.55f); Box("galv", X - 1.2f, 2.1f, Z - 0.55f, X + 1.2f, 2.2f, Z + 0.55f);
		Rod("orange", FVector(X - 1.1f, 0.45f, Z - 0.42f), FVector(X + 1.1f, 2.1f, Z - 0.42f), 0.04f); Rod("orange", FVector(X + 1.1f, 0.45f, Z - 0.42f), FVector(X - 1.1f, 2.1f, Z - 0.42f), 0.04f);
		Rod("orange", FVector(X - 1.1f, 0.45f, Z + 0.42f), FVector(X + 1.1f, 2.1f, Z + 0.42f), 0.04f); Rod("orange", FVector(X + 1.1f, 0.45f, Z + 0.42f), FVector(X - 1.1f, 2.1f, Z + 0.42f), 0.04f);
		for (const float E : {-0.55f, 0.55f}) Box("yellow", X - 1.2f, 3.15f, Z + E - 0.02f, X + 1.2f, 3.2f, Z + E + 0.02f, false); }
	// a maintenance platform with its stair and rails, a tyre rack, a tripod jack (east side, back)
	{ const float X = W / 4, Z = D - 9.5f; Box("galv", X - 1.2f, 2.0f, Z - 2.f, X + 1.2f, 2.1f, Z + 2.f); for (const float Ex : {-1.1f, 1.1f}) for (const float Ez : {-1.9f, 1.9f}) Box("yellow", X + Ex - 0.05f, 0.f, Z + Ez - 0.05f, X + Ex + 0.05f, 3.1f, Z + Ez + 0.05f);
		for (const float Ex : {-1.15f, 1.15f}) Box("yellow", X + Ex - 0.02f, 3.05f, Z - 2.f, X + Ex + 0.02f, 3.1f, Z + 2.f, false);
		for (int32 k = 0; k < 9; ++k) Box("galv", X - 1.2f - 0.25f * (k + 1), 0.22f * (k + 1) - 0.05f, Z - 0.45f, X - 1.2f - 0.25f * k, 0.22f * (k + 1), Z + 0.45f); }
	{ const float X = W / 2 - 1.f, Z = D - 4.f; Box("yellow", X - 0.4f, 0.f, Z - 1.5f, X + 0.4f, 0.06f, Z + 1.5f); for (int32 k = 0; k < 4; ++k) Rod("rubber", FVector(X - 0.2f, 0.5f, Z - 1.2f + k * 0.8f), FVector(X + 0.2f, 0.5f, Z - 1.2f + k * 0.8f), 0.45f); }
	{ const float X = W / 4 - 4.f, Z = D * 0.62f; for (int32 k = 0; k < 3; ++k) { const float A = k * 2.094f; Rod("yellow", FVector(X + FMath::Cos(A) * 0.8f, 0.f, Z + FMath::Sin(A) * 0.8f), FVector(X, 2.2f, Z), 0.04f); } Cyl("red", X, 1.6f, Z, 0.08f, 1.2f, false); }
	// on the columns: extinguishers, hose reels, first aid and an eye-wash station
	const int32 NB = FMath::Max(4, FMath::RoundToInt(D / S.Bay));
	for (int32 i = 1; i < NB; ++i)
		for (const float Sx : {-1.f, 1.f})
		{
			const float Z = i * D / NB - S.Col * 0.25f - 0.12f, X = Sx * (W / 2 + S.Col / 2) - Sx * (S.Col / 2 + 0.12f);
			if (i % 2) { Cyl("red", X, 0.6f, Z, 0.08f, 0.6f, false); Box("red", X - 0.01f, 1.8f, Z - 0.1f, X + 0.01f, 2.05f, Z + 0.1f, false); }
			else { Box("yellow", X - 0.02f, 2.15f, Z - 0.25f, X + 0.02f, 2.65f, Z + 0.25f, false); Rod("black", FVector(X - Sx * 0.02f, 2.4f, Z), FVector(X - Sx * 0.14f, 2.4f, Z), 0.15f); }
		}
	Box("green", W / 2 + 0.05f, 1.35f, 6.4f, W / 2 + 0.17f, 1.65f, 6.8f, false);
	Box("green", W / 2 + 0.05f, 1.05f, 7.25f, W / 2 + 0.23f, 1.55f, 7.55f, false);
}

#pragma endregion

#pragma region The crew annex: building

void AHangar::BuildAnnex()
{
	const float wx = S.WallX();
	constexpr float AW = AnnexW, AL = AnnexL, TOP = AnnexTop, STORE_H = 6.2f, MAIN_L = 22.f;
	// the annex frame: u out from the hall's west wall, v along it from the annex's front end
	auto A = [&](const FName M, float U0, float U1, float Y0, float Y1, float V0, float V1, bool bC = true) { Box(M, -wx - U0, Y0, AZ0 + V0, -wx - U1, Y1, AZ0 + V1, bC); };
	struct FOp { float A0, A1, Y0, Y1; };
	// a wall in the plane u = At (span along v) or v = At (span along u), the openings left out
	auto Wall = [&](bool bAlongV, float At, float S0, float S1, float Y0, float Y1, float Th, const FName M, const TArray<FOp>& Ops)
	{
		TArray<float> Cuts = {S0, S1};
		for (const FOp& O : Ops) { Cuts.Add(O.A0); Cuts.Add(O.A1); }
		Cuts.Sort();
		for (int32 i = 0; i + 1 < Cuts.Num(); ++i)
		{
			const float Ca = FMath::Max(Cuts[i], S0), Cb = FMath::Min(Cuts[i + 1], S1);
			if (Cb - Ca < 1e-3f) continue;
			const float Mid = (Ca + Cb) / 2;
			TArray<FOp> H; for (const FOp& O : Ops) if (O.A0 <= Mid && O.A1 >= Mid) H.Add(O);
			H.Sort([](const FOp& P, const FOp& Q) { return P.Y0 < Q.Y0; });
			float Y = Y0;
			auto Seg = [&](float Ya, float Yb) { if (Yb - Ya < 1e-3f) return; if (bAlongV) A(M, At - Th / 2, At + Th / 2, Ya, Yb, Ca, Cb); else A(M, Ca, Cb, Ya, Yb, At - Th / 2, At + Th / 2); };
			for (const FOp& O : H) { Seg(Y, FMath::Max(Y, O.Y0)); Y = FMath::Max(Y, O.Y1); }
			Seg(Y, Y1);
		}
	};
	auto Window = [&](bool bAlongV, float At, float A0, float A1, float Y0, float Y1, int32 NX, int32 NY)
	{
		auto Pane = [&](float Sa, float Sb, float Ya, float Yb, const FName M, float Th, bool bC) { if (bAlongV) A(M, At - Th / 2, At + Th / 2, Ya, Yb, Sa, Sb, bC); else A(M, Sa, Sb, Ya, Yb, At - Th / 2, At + Th / 2, bC); };
		Pane(A0, A1, Y0, Y1, "glassDark", 0.04f, true);
		const float F = 0.06f;
		Pane(A0, A1, Y0, Y0 + F, "alu", 0.2f, false); Pane(A0, A1, Y1 - F, Y1, "alu", 0.2f, false);
		Pane(A0, A0 + F, Y0, Y1, "alu", 0.2f, false); Pane(A1 - F, A1, Y0, Y1, "alu", 0.2f, false);
		for (int32 i = 1; i < NX; ++i) { const float Sx = A0 + (A1 - A0) * i / NX; Pane(Sx - 0.03f, Sx + 0.03f, Y0, Y1, "alu", 0.18f, false); }
		for (int32 j = 1; j < NY; ++j) { const float Y = Y0 + (Y1 - Y0) * j / NY; Pane(A0, A1, Y - 0.03f, Y + 0.03f, "alu", 0.18f, false); }
	};
	// the slab
	A("T:Floor", 0, AW, -0.4f, 0.02f, 0, AL);
	// the outer facade (u = AW): lounge windows, the lobby entrance, upstairs windows; the store bay's roll-up door and
	// side door; fine silver corrugated cladding with blue-grey panels between the bays (as the photos)
	const TArray<FOp> Outer = {
		{1.4f, 4.6f, 0.9f, 3.5f}, {5.4f, 8.6f, 0.9f, 3.5f}, {9.4f, 12.6f, 0.9f, 3.5f}, {15.6f, 19.4f, 0.f, 3.1f},
		{1.4f, 4.6f, 5.5f, 8.1f}, {5.4f, 8.6f, 5.5f, 8.1f}, {9.4f, 12.6f, 5.5f, 8.1f}, {15.4f, 20.6f, 5.5f, 8.1f}};
	Wall(true, AW - 0.12f, 0, MAIN_L, 0, TOP, 0.24f, "T:Corrugated", Outer);
	Wall(true, AW - 0.12f, MAIN_L, AL, 0, STORE_H, 0.24f, "T:Corrugated", {{23.6f, 28.4f, 0.f, 4.4f}, {29.4f, 30.4f, 0.f, 2.2f}});
	for (const FOp& O : Outer) if (O.Y0 > 0.5f) Window(true, AW - 0.12f, O.A0, O.A1, O.Y0, O.Y1, 2, O.Y0 > 5 ? 2 : 1);
	// the entrance: sidelights, the transom, and the two glass leaves that open as you come
	Window(true, AW - 0.12f, 15.6f, 16.55f, 0.f, 3.1f, 1, 1); Window(true, AW - 0.12f, 18.45f, 19.4f, 0.f, 3.1f, 1, 1); Window(true, AW - 0.12f, 16.55f, 18.45f, 2.45f, 3.1f, 2, 1);
	Swing(-wx - (AW - 0.12f), 0.02f, AZ0 + 16.55f, false, 0.95f, 2.4f, 0.04f, "glass");
	Swing(-wx - (AW - 0.12f), 0.02f, AZ0 + 17.5f, false, 0.95f, 2.4f, 0.04f, "glass");
	A("alu", AW - 0.2f, AW + 0.02f, 2.4f, 2.48f, 15.6f, 19.4f, false);
	for (const float V : {17.1f, 17.9f}) Rod("chrome", FVector(-wx - AW - 0.06f, 0.65f, AZ0 + V), FVector(-wx - AW - 0.06f, 1.55f, AZ0 + V), 0.015f);
	for (const float V : {4.6f, 8.6f, 12.6f}) A("blueGrey", AW - 0.02f, AW + 0.04f, 0.6f, 8.5f, V, V + 0.8f, false);
	A("blueGrey", AW - 0.02f, AW + 0.04f, 0.6f, 8.5f, 14.4f, 15.4f, false);
	Swing(-wx - (AW - 0.12f), 0.02f, AZ0 + 29.4f, false, 1.0f, 2.15f, 0.05f, "ral");
	// the front facade (v = 0, towards the apron)
	const TArray<FOp> Front = {{3.4f, 7.6f, 0.9f, 3.5f}, {8.4f, 12.4f, 0.9f, 3.5f}, {3.4f, 7.6f, 5.5f, 8.1f}, {8.4f, 12.4f, 5.5f, 8.1f}};
	Wall(false, 0.12f, 0, AW, 0, TOP, 0.24f, "T:Corrugated", Front);
	for (const FOp& O : Front) Window(false, 0.12f, O.A0, O.A1, O.Y0, O.Y1, 2, O.Y0 > 5 ? 2 : 1);
	for (const float U : {7.6f, 12.4f}) A("blueGrey", U, U + 0.8f, 0.6f, 8.5f, -0.04f, 0.02f, false);
	// the back: the store bay's end wall, the main block's wall above the store's roof
	Wall(false, AL - 0.12f, 0, AW, 0, STORE_H, 0.24f, "T:Corrugated", {});
	Wall(false, MAIN_L, 0, AW, STORE_H - 0.3f, TOP, 0.24f, "T:Corrugated", {});
	// the hall-side lining
	{ const float Hv = HallDoorZ - AZ0; A("white", -0.02f, 0.12f, 0, TOP - 0.5f, 0, Hv - 0.56f); A("white", -0.02f, 0.12f, 2.25f, TOP - 0.5f, Hv - 0.56f, Hv + 0.56f); A("white", -0.02f, 0.12f, 0, TOP - 0.5f, Hv + 0.56f, MAIN_L); }
	A("white", -0.02f, 0.12f, 0, STORE_H - 0.3f, MAIN_L, AL);
	// flat roofs with their coping, the rooftop units
	A("roofTop", 0, AW, TOP - 0.7f, TOP - 0.5f, 0, MAIN_L); A("roofTop", 0, AW, STORE_H - 0.5f, STORE_H - 0.3f, MAIN_L, AL);
	A("galv", AW - 0.28f, AW + 0.04f, TOP - 0.02f, TOP + 0.06f, 0, MAIN_L); A("galv", 0, AW + 0.04f, TOP - 0.02f, TOP + 0.06f, -0.04f, 0.28f);
	A("galv", AW - 0.28f, AW + 0.04f, STORE_H - 0.02f, STORE_H + 0.06f, MAIN_L, AL); A("galv", 0, AW + 0.04f, STORE_H - 0.02f, STORE_H + 0.06f, AL - 0.28f, AL + 0.04f);
	for (const FVector2f& P : {FVector2f(4, 5), FVector2f(9, 5), FVector2f(6.5f, 14)})
	{
		A("ral", P.X - 1.1f, P.X + 1.1f, TOP - 0.5f, TOP + 0.9f, P.Y - 0.8f, P.Y + 0.8f);
		for (int32 k = 0; k < 5; ++k) A("galv", P.X - 1.11f, P.X + 1.11f, TOP + 0.05f + k * 0.16f, TOP + 0.09f + k * 0.16f, P.Y - 0.81f, P.Y - 0.79f, false);
		Cyl("black", -wx - P.X, TOP + 0.9f, AZ0 + P.Y, 0.45f, 0.06f, false);
	}
	// the canopies: over the entrance (with its tie rods and the name), over the upper window band and the front windows
	A("frameDark", AW, AW + 2.6f, 3.3f, 3.55f, 14.8f, 20.2f);
	for (const float V : {15.0f, 20.0f}) Rod("frameDark", FVector(-wx - AW - 2.5f, 3.55f, AZ0 + V), FVector(-wx - AW + 0.1f, 5.2f, AZ0 + V), 0.03f);
	A("frameDark", AW, AW + 1.3f, 8.35f, 8.55f, 1.0f, 21.0f); A("frameDark", AW + 1.25f, AW + 1.32f, 8.2f, 8.55f, 1.0f, 21.0f, false);
	A("frameDark", 1.0f, 12.4f, 8.35f, 8.55f, -1.3f, 0); A("frameDark", 1.0f, 12.4f, 8.2f, 8.55f, -1.32f, -1.25f, false);
	// wall lamps, downpipes, cameras at the corners; the footpath along the outer side
	for (const float V : {3.f, 11.f, 21.f, 26.f}) { const float Y = V > MAIN_L ? STORE_H - 1.0f : TOP - 1.4f; A("frameDark", AW, AW + 0.5f, Y, Y + 0.12f, V - 0.2f, V + 0.2f, false); A("lamp", AW + 0.1f, AW + 0.48f, Y - 0.02f, Y, V - 0.15f, V + 0.15f, false); }
	for (const float V : {0.5f, 13.9f, 21.6f, 31.9f}) { const float H = V > MAIN_L ? STORE_H : TOP; Cyl("galv", -wx - AW - 0.12f, 0.f, AZ0 + V, 0.06f, H, false); }
	A("white", AW + 0.05f, AW + 0.33f, TOP - 0.65f, TOP - 0.55f, 0.25f, 0.35f, false);
	A("concrete", AW, AW + 2.2f, -0.03f, 0.12f, -1.0f, AL + 1.0f);
}

#pragma endregion

#pragma region The crew annex: rooms

void AHangar::BuildAnnexRooms()
{
	const float wx = S.WallX();
	constexpr float AW = AnnexW, FL1 = 4.5f, TOP = AnnexTop, STORE_H = 6.2f, MAIN_L = 22.f, GH = FL1 - 0.05f, UH = TOP - 0.7f;
	auto A = [&](const FName M, float U0, float U1, float Y0, float Y1, float V0, float V1, bool bC = true) { Box(M, -wx - U0, Y0, AZ0 + V0, -wx - U1, Y1, AZ0 + V1, bC); };
	auto P = [&](float U, float Y, float V) { return FVector(-wx - U, Y, AZ0 + V); };
	struct FOp { float A0, A1, Y0, Y1; };
	auto Wall = [&](bool bAlongV, float At, float S0, float S1, float Y0, float Y1, float Th, const FName M, const TArray<FOp>& Ops)
	{
		TArray<float> Cuts = {S0, S1};
		for (const FOp& O : Ops) { Cuts.Add(O.A0); Cuts.Add(O.A1); }
		Cuts.Sort();
		for (int32 i = 0; i + 1 < Cuts.Num(); ++i)
		{
			const float Ca = FMath::Max(Cuts[i], S0), Cb = FMath::Min(Cuts[i + 1], S1);
			if (Cb - Ca < 1e-3f) continue;
			const float Mid = (Ca + Cb) / 2;
			TArray<FOp> H; for (const FOp& O : Ops) if (O.A0 <= Mid && O.A1 >= Mid) H.Add(O);
			H.Sort([](const FOp& Pp, const FOp& Q) { return Pp.Y0 < Q.Y0; });
			float Y = Y0;
			auto Seg = [&](float Ya, float Yb) { if (Yb - Ya < 1e-3f) return; if (bAlongV) A(M, At - Th / 2, At + Th / 2, Ya, Yb, Ca, Cb); else A(M, Ca, Cb, Ya, Yb, At - Th / 2, At + Th / 2); };
			for (const FOp& O : H) { Seg(Y, FMath::Max(Y, O.Y0)); Y = FMath::Max(Y, O.Y1); }
			Seg(Y, Y1);
		}
	};
	// a door leaf, shut, in its frame (wood veneer with frosted glass, or grey steel), with its lever handles
	auto Door = [&](bool bAlongV, float At, float A0, float A1, float H, const FName M, float Y0, bool bFrosted)
	{
		auto Pane = [&](float Sa, float Sb, float Ya, float Yb, const FName Mm, float Th, bool bC) { if (bAlongV) A(Mm, At - Th / 2, At + Th / 2, Ya, Yb, Sa, Sb, bC); else A(Mm, Sa, Sb, Ya, Yb, At - Th / 2, At + Th / 2, bC); };
		Pane(A0 - 0.05f, A0, Y0, Y0 + H + 0.05f, "galv", 0.2f, false); Pane(A1, A1 + 0.05f, Y0, Y0 + H + 0.05f, "galv", 0.2f, false); Pane(A0 - 0.05f, A1 + 0.05f, Y0 + H, Y0 + H + 0.05f, "galv", 0.2f, false);
		// the leaf on its hinge (opens as you come)
		if (bAlongV) Swing(-wx - At, Y0 + 0.01f, AZ0 + A0, false, A1 - A0, H - 0.02f, 0.05f, M, bFrosted);
		else Swing(-wx - A1, Y0 + 0.01f, AZ0 + At, true, A1 - A0, H - 0.02f, 0.05f, M, bFrosted);
	};
	auto RoomLight = [&](float U, float Y, float V, float Reach) { Lamp(-wx - U, Y, AZ0 + V, Reach); };

	// ---- ground floor: the first floor's slab (open over the lobby) and the dark exposed ceiling under it
	A("T:Deck", 0, AW, FL1 - 0.25f, FL1, 0, 14); A("T:Deck", 0, 6.5f, FL1 - 0.25f, FL1, 14, MAIN_L); A("T:Deck", 6.5f, 8.2f, FL1 - 0.25f, FL1, 14, 15.8f);
	A("T:Floor", 0, AW - 0.24f, FL1, FL1 + 0.012f, 0.24f, 14, false); A("T:Floor", 0, 6.5f, FL1, FL1 + 0.012f, 14, MAIN_L - 0.1f, false);
	for (float U = 1; U < AW; U += 1.5f) A("frameDark", U - 0.06f, U + 0.06f, FL1 - 0.65f, FL1 - 0.25f, 0.2f, 13.8f, false);
	Rod("galv", P(7.5f, FL1 - 0.9f, 0.5f), P(7.5f, FL1 - 0.9f, 13.5f), 0.3f); Rod("galv", P(0.2f, FL1 - 0.75f, 10), P(7.4f, FL1 - 0.75f, 10), 0.22f);
	// the corridor wall (u 2.4): the door into the lounge at the far end, the toilets' doors, the passage from the lobby
	Wall(true, 2.4f, 0, MAIN_L, 0, GH, 0.15f, "white", {{1.2f, 2.2f, 0, 2.15f}, {15.2f, 16.1f, 0, 2.15f}, {18.1f, 19.0f, 0, 2.15f}, {20.7f, 21.9f, 0, 2.6f}});
	Door(true, 2.4f, 1.2f, 2.2f, 2.15f, "T:Wood", 0, true); Door(true, 2.4f, 15.2f, 16.1f, 2.15f, "T:Wood", 0, false); Door(true, 2.4f, 18.1f, 19.0f, 2.15f, "T:Wood", 0, false);
	Door(false, MAIN_L - 0.08f, 0.7f, 1.7f, 2.15f, "ral", 0, false);
	A("exitGreen", 1.0f, 1.4f, 2.45f, 2.6f, MAIN_L - 0.24f, MAIN_L - 0.2f, false);
	// the lounge / lobby wall (v 14) with its double doors, the toilets (two rooms), the store bay's wall (v 22)
	Wall(false, 14, 2.4f, AW, 0, GH, 0.15f, "white", {{8.6f, 10.4f, 0, 2.3f}});
	Door(false, 14, 8.6f, 9.5f, 2.3f, "T:Wood", 0, true); Door(false, 14, 9.5f, 10.4f, 2.3f, "T:Wood", 0, true);
	Wall(true, 6.5f, 14, 20.5f, 0, GH, 0.15f, "white", {}); Wall(false, 17.25f, 2.4f, 6.5f, 0, GH, 0.12f, "white", {}); Wall(false, 20.5f, 2.4f, 6.5f, 0, GH, 0.15f, "white", {});
	for (const float V0 : {14.f, 17.25f})
	{
		A("tile", 6.3f, 6.42f, 0, 2.0f, V0 + 0.1f, V0 + 3.15f, false);
		A("porcelain", 5.6f, 6.2f, 0, 0.42f, V0 + 2.1f, V0 + 2.5f); A("porcelain", 6.0f, 6.3f, 0.42f, 0.82f, V0 + 2.1f, V0 + 2.5f, false);
		A("porcelain", 2.9f, 3.45f, 0.78f, 0.92f, V0 + 2.35f, V0 + 2.85f, false); A("chrome", 2.5f, 2.52f, 1.2f, 1.9f, V0 + 2.35f, V0 + 2.85f, false);
		RoomLight(4.4f, GH - 0.2f, V0 + 1.6f, 6.f);
	}
	Wall(false, MAIN_L, 2.4f, AW, 0, STORE_H - 0.3f, 0.2f, "white", {});
	// the corridor's ceiling lights, extinguisher
	for (const float V : {3.f, 8.f, 13.f, 18.f}) A("lamp", 0.9f, 1.5f, GH - 0.03f, GH - 0.01f, V - 0.3f, V + 0.3f, false);
	RoomLight(1.2f, GH - 0.2f, 6.f, 9.f); RoomLight(1.2f, GH - 0.2f, 16.f, 9.f);
	Cyl("red", -wx - 2.3f, 0.3f, AZ0 + 12.f, 0.08f, 0.5f, false);

	// ---- the crew lounge (u 2.4 … 13, v 0 … 14): black leather sofas and armchairs on rugs, slatted steel tables, high
	// tables with stools, the TV, plants, a floor lamp, the mountain photo wall, white pendant lamps under the dark deck
	auto Sofa = [&](float U, float V, int32 Dir, int32 Seats)
	{
		// Dir: which way the seat faces (0 -v, 1 +u, 2 +v, 3 -u)
		const float Wd = 0.85f * Seats + 0.3f, Hd = Wd / 2;
		const bool bAlongU = Dir == 0 || Dir == 2;
		const float Bs = (Dir == 0 || Dir == 3) ? 1.f : -1.f;   // the back's side
		auto B = [&](float A0, float A1, float Y0, float Y1, float D0, float D1) { if (bAlongU) A("leatherBlack", U + A0, U + A1, Y0, Y1, V + D0, V + D1); else A("leatherBlack", U + D0, U + D1, Y0, Y1, V + A0, V + A1); };
		B(-Hd, Hd, 0.17f, 0.45f, -0.42f, 0.42f);
		B(-Hd, Hd, 0.45f, 0.9f, Bs * 0.24f - 0.1f, Bs * 0.24f + 0.1f);
		B(-Hd, -Hd + 0.16f, 0.45f, 0.65f, -0.42f, 0.42f); B(Hd - 0.16f, Hd, 0.45f, 0.65f, -0.42f, 0.42f);
		B(-Hd + 0.05f, Hd - 0.05f, 0.f, 0.17f, -0.38f, 0.38f);
	};
	auto Tub = [&](float U, float V) { Cyl("leatherBlack", -wx - U, 0.f, AZ0 + V, 0.43f, 0.76f); Cyl("black", -wx - U, 0.46f, AZ0 + V, 0.34f, 0.05f, false); };
	auto CTable = [&](float U, float V) { A("galv", U - 0.6f, U + 0.6f, 0.38f, 0.41f, V - 0.28f, V + 0.28f); for (int32 k = 0; k < 9; ++k) A("chrome", U - 0.55f + k * 0.1375f - 0.012f, U - 0.55f + k * 0.1375f + 0.012f, 0.f, 0.38f, V - 0.26f, V + 0.26f, false); };
	auto Rug = [&](float U, float V, float W, float D) { A("carpetGrey", U - W / 2, U + W / 2, 0.02f, 0.035f, V - D / 2, V + D / 2, false); };
	auto Plant = [&](float U, float V) { Cyl("black", -wx - U, 0.f, AZ0 + V, 0.26f, 0.6f); Cyl("soil", -wx - U, 0.6f, AZ0 + V, 0.03f, 0.9f, false); for (int32 k = 0; k < 6; ++k) { const float An = k * 1.05f; Cyl("plant", -wx - U + FMath::Cos(An) * 0.2f, 0.8f + k * 0.17f, AZ0 + V + FMath::Sin(An) * 0.2f, 0.28f - k * 0.02f, 0.3f, false); } };
	auto High = [&](float U, float V)
	{
		A("black", U - 0.6f, U + 0.6f, 1.03f, 1.07f, V - 0.3f, V + 0.3f);
		for (const float Sx : {-1.f, 1.f}) for (const float Tz : {-1.f, 1.f}) A("black", U + Sx * 0.56f - 0.02f, U + Sx * 0.56f + 0.02f, 0, 1.03f, V + Tz * 0.26f - 0.02f, V + Tz * 0.26f + 0.02f, false);
		for (const float Tz : {-1.f, 1.f}) { A("leatherBlack", U - 0.21f, U + 0.21f, 0.72f, 0.78f, V + Tz * 0.65f - 0.21f, V + Tz * 0.65f + 0.21f); for (const float Sx : {-1.f, 1.f}) A("chrome", U + Sx * 0.18f - 0.012f, U + Sx * 0.18f + 0.012f, 0, 0.72f, V + Tz * 0.65f - 0.012f, V + Tz * 0.65f + 0.012f, false); }
	};
	auto Pendant = [&](float U, float V, float Y, float TopY) { Cyl("white", -wx - U, Y, AZ0 + V, 0.26f, 0.28f, false); A("lamp", U - 0.2f, U + 0.2f, Y - 0.01f, Y, V - 0.2f, V + 0.2f, false); Rod("black", P(U, Y + 0.28f, V), P(U, TopY, V), 0.01f); };
	Rug(6.2f, 5.0f, 4.4f, 3.6f); Rug(10.4f, 9.2f, 4.0f, 3.4f);
	Sofa(6.2f, 3.0f, 2, 2); Sofa(4.2f, 5.2f, 3, 1); Sofa(8.3f, 5.2f, 1, 1); CTable(6.2f, 5.0f); Tub(5.4f, 6.9f); Tub(7.0f, 6.9f);
	Sofa(10.4f, 11.0f, 0, 3); Sofa(12.2f, 9.0f, 1, 1); CTable(10.4f, 9.2f); Tub(9.4f, 7.6f); Tub(11.4f, 7.6f);
	High(3.4f, 10.5f); High(3.4f, 12.6f); High(11.8f, 1.3f);
	Plant(12.4f, 0.6f); Plant(2.9f, 0.7f); Plant(12.4f, 13.3f);
	A("black", 7.0f, 8.6f, 1.24f, 2.16f, 13.86f, 13.92f, false);                              // the TV
	A("navy", 2.47f, 2.49f, 0.6f, 3.2f, 4.5f, 10.5f, false);                            // the mountain photo wall
	Cyl("chrome", -wx - 12.5f, 0.f, AZ0 + 6.6f, 0.015f, 1.5f, false); Cyl("white", -wx - 12.5f, 1.45f, AZ0 + 6.6f, 0.19f, 0.32f, false);
	for (const float U : {4.5f, 8.f, 11.5f}) for (const float V : {3.f, 8.f, 12.f}) Pendant(U, V, 3.2f, FL1 - 0.3f);
	for (const float U : {4.5f, 11.f}) for (const float V : {3.5f, 10.5f}) RoomLight(U, 3.1f, V, 9.f);
	A("frameDark", 6.4f, 6.7f, 0, FL1, 7.0f, 7.3f);                                          // the steel column in the middle

	// ---- the lobby (u 6.5 … 13, v 14 … 22): double height; the grey steel stairs in two flights with their landing and
	// railings of horizontal bars; the bench, the check-in desk, the sign post, the AED
	auto Flight = [&](float U0, float U1, float V0, float V1, float Y0, float Y1, bool bAlongV)
	{
		const int32 N = 13; const float Dy = (Y1 - Y0) / N, Run = bAlongV ? (V1 - V0) : (U1 - U0), Ds = Run / N;
		for (int32 k = 0; k < N; ++k)
		{
			const float Ya = Y0 + Dy * (k + 1);
			if (bAlongV) { A("galv", U0, U1, Ya - 0.05f, Ya, V0 + Ds * k, V0 + Ds * (k + 1)); A("frameDark", U0 + 0.02f, U1 - 0.02f, Ya - Dy, Ya - 0.05f, V0 + Ds * k, V0 + Ds * k + (Ds > 0 ? 0.02f : -0.02f), false); }
			else { A("galv", U0 + Ds * k, U0 + Ds * (k + 1), Ya - 0.05f, Ya, V0, V1); A("frameDark", U0 + Ds * k, U0 + Ds * k + (Ds > 0 ? 0.02f : -0.02f), Ya - Dy, Ya - 0.05f, V0 + 0.02f, V1 - 0.02f, false); }
		}
		for (const int32 Side : {0, 1})
		{
			const FVector Pa = bAlongV ? P(Side ? U1 : U0, Y0, V0) : P(U0, Y0, Side ? V1 : V0), Pb = bAlongV ? P(Side ? U1 : U0, Y1, V1) : P(U1, Y1, Side ? V1 : V0);
			Beam("frameDark", Pa + FVector(0, -0.15f, 0), Pb + FVector(0, -0.15f, 0), 0.06f, 0.3f);
			for (int32 k = 0; k < 7; ++k) Rod("frameDark", Pa + FVector(0, 0.25f + k * 0.13f, 0), Pb + FVector(0, 0.25f + k * 0.13f, 0), 0.012f);
			Rod("galv", Pa + FVector(0, 1.1f, 0), Pb + FVector(0, 1.1f, 0), 0.025f);
			for (int32 k = 0; k <= 3; ++k) { const FVector Q = FMath::Lerp(Pa, Pb, k / 3.f); Rod("frameDark", Q, Q + FVector(0, 1.1f, 0), 0.025f); }
		}
	};
	Flight(12.6f, 8.96f, 20.6f, 21.8f, 0, FL1 / 2, false);
	A("galv", 6.5f, 8.96f, FL1 / 2 - 0.15f, FL1 / 2, 19.4f, 21.8f); A("frameDark", 6.5f, 8.96f, FL1 / 2 - 0.4f, FL1 / 2 - 0.15f, 19.4f, 21.8f, false);
	Cyl("frameDark", -wx - 8.9f, 0.f, AZ0 + 19.5f, 0.06f, FL1 / 2, false); Cyl("frameDark", -wx - 6.6f, 0.f, AZ0 + 19.5f, 0.06f, FL1 / 2, false);
	Flight(7.0f, 8.2f, 19.4f, 15.8f, FL1 / 2, FL1, true);
	for (int32 k = 0; k < 7; ++k) A("frameDark", 6.5f, 6.53f, FL1 + 0.25f + k * 0.13f, FL1 + 0.27f + k * 0.13f, 14, MAIN_L, false);
	A("galv", 6.48f, 6.56f, FL1 + 1.05f, FL1 + 1.12f, 14, MAIN_L, false);
	A("T:Wood", 12.1f, 12.55f, 0.42f, 0.46f, 14.2f, 16.2f); A("chrome", 12.15f, 12.5f, 0, 0.42f, 14.38f, 14.42f, false); A("chrome", 12.15f, 12.5f, 0, 0.42f, 15.98f, 16.02f, false);
	A("T:Wood", 8.85f, 9.55f, 0, 1.1f, 16.5f, 17.5f); A("worktop", 8.81f, 9.59f, 1.1f, 1.14f, 16.46f, 17.54f);
	Cyl("white", -wx - 10.6f, 0.f, AZ0 + 19.2f, 0.06f, 2.6f, false); A("blueGrey", 10.55f, 10.65f, 2.2f, 2.42f, 18.6f, 19.8f, false); A("blueGrey", 10.55f, 10.65f, 1.9f, 2.12f, 18.7f, 19.9f, false); A("black", 10.52f, 10.68f, 1.25f, 1.85f, 18.7f, 19.7f, false);
	A("white", 12.88f, 13.0f, 1.3f, 1.7f, 20.85f, 21.15f, false); A("red", 12.87f, 12.88f, 1.42f, 1.58f, 20.95f, 21.05f, false);
	Pendant(10.5f, 17.5f, FL1 + 2.0f, TOP - 0.7f); RoomLight(10.f, FL1 + 1.8f, 18.f, 10.f);

	// ---- the store bay (v 22 … 32.4): shelving with boxes, the glass-door fridge, washers, ice machine, carts, bins,
	// the gas-cylinder cage, the hand sink; the roll-up doors to the hall and to the outside
	auto Rack = [&](float U0, float V0, float Len, bool bAlongV)
	{
		const float Lv[] = {0.15f, 1.0f, 1.9f, 2.8f};
		for (const float Y : Lv) bAlongV ? A("rackBlue", U0, U0 + 0.9f, Y, Y + 0.04f, V0, V0 + Len) : A("rackBlue", U0, U0 + Len, Y, Y + 0.04f, V0, V0 + 0.9f);
		const int32 NPost = FMath::RoundToInt(Len / 1.8f);
		for (int32 k = 0; k <= NPost; ++k) for (const float Sd : {0.f, 0.86f}) { const float Al = Len * k / NPost; bAlongV ? A("rackBlue", U0 + Sd, U0 + Sd + 0.05f, 0, 3.4f, V0 + Al - 0.025f, V0 + Al + 0.025f) : A("rackBlue", U0 + Al - 0.025f, U0 + Al + 0.025f, 0, 3.4f, V0 + Sd, V0 + Sd + 0.05f); }
		int32 n = 0;
		for (const float Y : Lv) for (float Al = 0.3f; Al < Len - 0.3f; Al += 0.6f, ++n)
		{
			if ((n * 7) % 5 == 0) continue;
			const float H = 0.3f + (n * 13 % 5) * 0.08f; const FName M = n % 4 == 0 ? FName("binGrey") : FName("card");
			bAlongV ? A(M, U0 + 0.1f, U0 + 0.8f, Y + 0.04f, Y + 0.04f + H, V0 + Al - 0.25f, V0 + Al + 0.25f, false) : A(M, U0 + Al - 0.25f, U0 + Al + 0.25f, Y + 0.04f, Y + 0.04f + H, V0 + 0.1f, V0 + 0.8f, false);
		}
	};
	Rack(11.9f, 22.6f, 7.2f, true); Rack(2.2f, 31.3f, 6.0f, false);
	A("galv", 0.3f, 1.1f, 0, 2.1f, 23.5f, 25.5f); A("glass", 1.1f, 1.12f, 0.35f, 1.95f, 23.6f, 25.4f, false); A("lamp", 1.08f, 1.1f, 0.4f, 1.9f, 23.65f, 25.35f, false);
	for (const float T : {0.f, 0.75f}) { A("white", 0.15f, 0.85f, 0, 0.94f, 25.85f + T, 26.55f + T); Rod("black", P(0.86f, 0.55f, 26.2f + T), P(0.88f, 0.55f, 26.2f + T), 0.16f); }
	A("galv", 0.15f, 0.95f, 0, 1.8f, 28.2f, 29.0f);
	for (const FVector2f& C : {FVector2f(5.4f, 25.5f), FVector2f(7.0f, 27.4f)}) { for (const float Y : {0.25f, 0.85f}) A("cartBlue", C.X - 0.3f, C.X + 0.3f, Y - 0.03f, Y + 0.03f, C.Y - 0.5f, C.Y + 0.5f); for (const float Sx : {-1.f, 1.f}) for (const float Tz : {-1.f, 1.f}) A("cartBlue", C.X + Sx * 0.27f - 0.02f, C.X + Sx * 0.27f + 0.02f, 0.05f, 0.85f, C.Y + Tz * 0.47f - 0.02f, C.Y + Tz * 0.47f + 0.02f, false); }
	Cyl("binGrey", -wx - 3.0f, 0.f, AZ0 + 23.f, 0.3f, 0.84f); Cyl("black", -wx - 3.7f, 0.f, AZ0 + 23.f, 0.3f, 0.84f); Cyl("binBlue", -wx - 4.4f, 0.f, AZ0 + 23.f, 0.3f, 0.84f);
	A("yellow", 9.2f, 10.4f, 0, 2.0f, 31.3f, 31.9f); Cyl("green", -wx - 9.55f, 2.0f, AZ0 + 31.6f, 0.12f, 0.4f, false); Cyl("green", -wx - 9.95f, 2.0f, AZ0 + 31.6f, 0.12f, 0.4f, false);
	A("galv", 0.08f, 0.53f, 0, 0.9f, 30.3f, 30.9f); A("galv", 0.05f, 0.55f, 0.92f, 0.98f, 30.28f, 30.93f);
	for (const float V : {24.5f, 29.5f}) A("lamp", 4, 9, STORE_H - 0.6f, STORE_H - 0.55f, V - 0.1f, V + 0.1f, false);
	RoomLight(6.5f, STORE_H - 0.8f, 27.f, 10.f);
	for (const float Uu : {AW - 0.12f})
	{
		A("T:Slats", Uu - 0.04f, Uu + 0.04f, 0, 4.4f, 23.6f, 28.4f);
		A("galv", Uu - 0.55f, Uu - 0.05f, 4.4f, 5.0f, 23.5f, 28.5f, false);
		A("frameDark", Uu - 0.45f, Uu - 0.1f, 4.5f, 4.9f, 28.5f, 28.85f, false);
	}

	// ---- first floor: the office (v 0 … 6), the kitchen (v 6 … 14), the mezzanine and upstairs corridor
	Wall(true, 2.4f, 0, 14, FL1, UH, 0.15f, "white", {{2.6f, 3.6f, FL1, FL1 + 2.15f}, {9.4f, 11.2f, FL1, FL1 + 2.3f}});
	Door(true, 2.4f, 2.6f, 3.6f, 2.15f, "T:Wood", FL1, true); Door(true, 2.4f, 9.4f, 10.3f, 2.3f, "ral", FL1, false); Door(true, 2.4f, 10.3f, 11.2f, 2.3f, "ral", FL1, false);
	Wall(false, 6, 2.4f, AW, FL1, UH, 0.15f, "white", {});
	Wall(false, 14, 2.4f, 6.5f, FL1, UH, 0.15f, "white", {{3.2f, 4.2f, FL1, FL1 + 2.15f}}); Door(false, 14, 3.2f, 4.2f, 2.15f, "T:Wood", FL1, false);
	Wall(false, 14, 6.5f, AW, FL1, UH, 0.15f, "white", {{7.5f, 12.0f, FL1 + 1.0f, FL1 + 2.6f}}); A("glass", 7.5f, 12.0f, FL1 + 1.0f, FL1 + 2.6f, 13.99f, 14.01f, false);
	for (float U = 1; U < AW; U += 1.5f) A("frameDark", U - 0.06f, U + 0.06f, UH - 0.4f, UH, 0.2f, MAIN_L - 0.2f, false);
	A("T:Deck", 0, AW, UH - 0.02f, UH, 0, MAIN_L, false);
	Rod("galv", P(5, UH - 0.7f, 0.5f), P(5, UH - 0.7f, 21.5f), 0.3f);
	for (const FVector2f& D : {FVector2f(5, 2), FVector2f(8.5f, 2), FVector2f(5, 4.3f), FVector2f(8.5f, 4.3f)})
	{
		A("white", D.X - 0.8f, D.X + 0.8f, FL1 + 0.72f, FL1 + 0.76f, D.Y - 0.4f, D.Y + 0.4f);
		for (const float Sx : {-1.f, 1.f}) A("galv", D.X + Sx * 0.75f - 0.025f, D.X + Sx * 0.75f + 0.025f, FL1, FL1 + 0.72f, D.Y - 0.35f, D.Y + 0.35f, false);
		A("black", D.X - 0.3f, D.X + 0.3f, FL1 + 0.87f, FL1 + 1.23f, D.Y + 0.24f, D.Y + 0.27f, false);
		A("black", D.X - 0.25f, D.X + 0.25f, FL1 + 0.44f, FL1 + 0.52f, D.Y - 0.95f, D.Y - 0.45f); A("black", D.X - 0.24f, D.X + 0.24f, FL1 + 0.55f, FL1 + 1.15f, D.Y - 1.0f, D.Y - 0.94f, false);
	}
	RoomLight(6.5f, UH - 0.6f, 3.f, 9.f);
	{
		const float V1 = 13.9f, UA = 3.0f, UB = 10.6f;
		A("T:Wood", UA, UB, FL1, FL1 + 0.9f, V1 - 0.62f, V1 - 0.02f); A("worktop", UA - 0.02f, UB + 0.02f, FL1 + 0.9f, FL1 + 0.94f, V1 - 0.66f, V1);
		A("T:Wood", UA + 0.9f, UB, FL1 + 1.55f, FL1 + 2.35f, V1 - 0.37f, V1 - 0.02f);
		for (float U = UA + 1.0f; U < UB; U += 0.6f) { A("chrome", U, U + 0.02f, FL1 + 0.5f, FL1 + 0.75f, V1 - 0.66f, V1 - 0.63f, false); A("chrome", U, U + 0.02f, FL1 + 1.65f, FL1 + 1.9f, V1 - 0.41f, V1 - 0.38f, false); }
		A("galv", UA - 0.92f, UA - 0.02f, FL1, FL1 + 1.8f, V1 - 0.8f, V1 - 0.02f); A("chrome", UA - 0.3f, UA - 0.28f, FL1 + 0.3f, FL1 + 1.6f, V1 - 0.82f, V1 - 0.8f, false);
		A("galv", UA + 1.0f, UA + 1.75f, FL1 + 1.25f, FL1 + 1.65f, V1 - 0.45f, V1 - 0.02f, false);
		A("chrome", UA + 2.5f, UA + 3.1f, FL1 + 0.92f, FL1 + 0.95f, V1 - 0.55f, V1 - 0.15f, false);
		for (const float U : {UA + 4.6f, UA + 6.3f}) A("galv", U, U + 0.32f, FL1 + 0.94f, FL1 + 1.35f, V1 - 0.45f, V1 - 0.1f, false);
		A("T:Wood", 6.5f, 7.5f, FL1 + 0.72f, FL1 + 0.76f, 8.3f, 10.7f);
		for (const float Sx : {-1.f, 1.f}) for (const float Tz : {-1.f, 1.f}) A("black", 7 + Sx * 0.42f - 0.02f, 7 + Sx * 0.42f + 0.02f, FL1, FL1 + 0.72f, 9.5f + Tz * 1.1f - 0.02f, 9.5f + Tz * 1.1f + 0.02f, false);
		for (const float Sx : {-1.f, 1.f}) for (const float Q : {-0.8f, 0.f, 0.8f}) { A("black", 7 + Sx * 0.85f - 0.22f, 7 + Sx * 0.85f + 0.22f, FL1 + 0.44f, FL1 + 0.48f, 9.5f + Q - 0.22f, 9.5f + Q + 0.22f); A("black", 7 + Sx * 1.05f - 0.02f, 7 + Sx * 1.05f + 0.02f, FL1 + 0.48f, FL1 + 1.0f, 9.5f + Q - 0.21f, 9.5f + Q + 0.21f, false); }
		for (const float V : {8.5f, 10.5f}) Pendant(7, V, FL1 + 2.6f, UH - 0.4f);
		for (const float V : {7.2f, 12.4f}) Pendant(4.0f, V, FL1 + 2.8f, UH - 0.4f);
		RoomLight(5.5f, UH - 0.6f, 8.f, 9.f); RoomLight(8.5f, UH - 0.6f, 11.5f, 9.f);
	}
	Sofa(4.6f, 18.5f, 3, 2);
	for (const float V : {3.f, 8.f, 13.f, 18.f}) A("lamp", 0.9f, 1.5f, UH - 0.03f, UH - 0.01f, V - 0.3f, V + 0.3f, false);
	RoomLight(1.2f, UH - 0.2f, 9.f, 9.f); RoomLight(4.f, UH - 0.6f, 18.f, 9.f);
}

#pragma endregion

#pragma region The map

void AHangar::MapOutline(TArray<FVector4f>& Walls, TArray<FVector4f>& Door) const
{
	// hangar metres (x across, z from the door line in) to plan metres through the actor's own place
	const FTransform T = GetActorTransform();
	auto P = [&T](float X, float Z) { const FVector W = T.TransformPosition(FVector(X * 100.f, -Z * 100.f, 0.f)); return FVector2f(W.X / 100.f, -W.Y / 100.f); };
	auto Seg = [&](TArray<FVector4f>& L, float X0, float Z0, float X1, float Z1) { const FVector2f A = P(X0, Z0), B = P(X1, Z1); L.Add(FVector4f(A.X, A.Y, B.X, B.Y)); };
	const float wx = S.WallX(), bk = S.Back(), px = S.PocketX(), hw = S.W / 2;
	// the hall: side walls, back wall, the door line's pockets either side of the opening
	Seg(Walls, -wx, 0, -wx, bk); Seg(Walls, wx, 0, wx, bk); Seg(Walls, -wx, bk, wx, bk);
	Seg(Walls, -px, 0, -hw, 0); Seg(Walls, hw, 0, px, 0);
	Seg(Door, -hw, 0, hw, 0);
	// the crew annex against the west wall
	const float ax = -wx - AnnexW, z0 = AZ0, z1 = AZ0 + AnnexL;
	Seg(Walls, -wx, z0, ax, z0); Seg(Walls, ax, z0, ax, z1); Seg(Walls, ax, z1, -wx, z1);
}

#pragma endregion
