// The 3D viewer in Unreal (see FlightViewer.h).
#include "FlightViewer.h"
#include "ADPLog.h"
// (Windows' own OPAQUE macro would break Cesium's glTF names when the build puts a Windows file before this one)
#ifdef OPAQUE
#undef OPAQUE
#endif
#include "CesiumGeoreference.h"
#include "BabylonModel.h"
#include "Airfield.h"
#include "GrassField.h"
#include "Ocean.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

AFlightViewer::AFlightViewer()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Model = CreateDefaultSubobject<USceneComponent>(TEXT("Model"));
	Model->SetupAttachment(Root);
	// the Babylon airliner as imported faces +Y with its right wing to -X: turned to face +X (Unreal's forward)
	Model->SetRelativeRotation(FRotator(0, -90.f, 0));
}

void AFlightViewer::Open()
{
	if (bOpen) return;
	bOpen = true; bHave = false; bTwo = false;
	// the aircraft: every part of the imported airliner (each already in its place on the model)
	if (!Model->GetNumChildrenComponents())
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Parts;
		AR.GetAssetsByPath(FName(TEXT("/Game/Babylon/Airliner/airliner/StaticMeshes")), Parts, false);
		for (const FAssetData& A : Parts)
			if (UStaticMesh* M = Cast<UStaticMesh>(A.GetAsset()))
			{
				UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
				C->SetupAttachment(Model);
				C->SetStaticMesh(M);
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->RegisterComponent();
				const FString N = M->GetName();
				if (N.StartsWith(TEXT("strut")) || N.StartsWith(TEXT("wheel")) || N.StartsWith(TEXT("hub"))) Gear.Add(C);   // the landing gear
			}
		ADP_LOG(TEXT("Viewer: the airliner, %d parts (%d gear)"), Parts.Num(), Gear.Num());
	}
	for (TActorIterator<ACesiumGeoreference> It(GetWorld()); It; ++It) { Geo = *It; break; }
	if (Geo) OldOrigin = Geo->GetOriginLongitudeLatitudeHeight();
	Hide(true);
	SetActorHiddenInGame(true);   // until the first position comes
	if (!Cam) Cam = GetWorld()->SpawnActor<ACameraActor>();
	if (Cam) { Cam->GetCameraComponent()->SetConstraintAspectRatio(false); Cam->GetCameraComponent()->SetFieldOfView(60.f); }
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* P = PC->GetPawn()) { P->DisableInput(PC); P->SetActorHiddenInGame(true); }
		if (Cam) PC->SetViewTarget(Cam);
	}
}

void AFlightViewer::Close()
{
	if (!bOpen) return;
	bOpen = false;
	SetActorHiddenInGame(true);
	if (Geo) Geo->SetOriginLongitudeLatitudeHeight(OldOrigin);
	Hide(false);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (APawn* P = PC->GetPawn()) { P->EnableInput(PC); P->SetActorHiddenInGame(false); PC->SetViewTarget(P); }
}

void AFlightViewer::Hide(bool bHide)
{
	if (bHide)
	{
		Hidden.Reset();
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			if (It->IsA<ABabylonModel>() || It->IsA<AAirfield>() || It->IsA<AGrassField>())
				if (!It->IsHidden()) { It->SetActorHiddenInGame(true); Hidden.Add(*It); }
	}
	else
	{
		for (AActor* A : Hidden) if (A) A->SetActorHiddenInGame(false);
		Hidden.Reset();
	}
}

void AFlightViewer::OnSample(const FString& Json)
{
	if (!bOpen) return;
	TSharedPtr<FJsonObject> O;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) || !O.IsValid()) return;
	const TSharedPtr<FJsonObject>* MeP = nullptr;
	if (!O->TryGetObjectField(TEXT("me"), MeP)) return;
	const TSharedPtr<FJsonObject>& M = *MeP;
	FFlightSample S;
	M->TryGetStringField(TEXT("id"), S.Id); M->TryGetStringField(TEXT("callsign"), S.Callsign); M->TryGetStringField(TEXT("reg"), S.Reg);
	M->TryGetStringField(TEXT("type"), S.Type); M->TryGetStringField(TEXT("from"), S.From); M->TryGetStringField(TEXT("to"), S.To);
	M->TryGetStringField(TEXT("fromName"), S.FromName); M->TryGetStringField(TEXT("toName"), S.ToName); M->TryGetStringField(TEXT("airline"), S.Airline);
	M->TryGetStringField(TEXT("phase"), S.Phase);
	double D = 0;
	if (!M->TryGetNumberField(TEXT("lat"), S.Lat) || !M->TryGetNumberField(TEXT("lon"), S.Lon)) return;
	M->TryGetNumberField(TEXT("altM"), S.AltM);
	if (M->TryGetNumberField(TEXT("hdg"), D)) S.Hdg = D;
	if (M->TryGetNumberField(TEXT("gs"), D)) S.Gs = D;
	if (M->TryGetNumberField(TEXT("vs"), D)) S.Vs = D;
	if (M->TryGetNumberField(TEXT("pax"), D)) S.Pax = int32(D);
	if (M->TryGetNumberField(TEXT("paxMax"), D)) S.PaxMax = int32(D);
	M->TryGetBoolField(TEXT("onGround"), S.bOnGround);
	S.At = FPlatformTime::Seconds();
	// another flight chosen: start again from it
	if (bHave && S.Id != S1.Id) bTwo = false;
	S0 = bHave && bTwo ? S1 : S; S1 = S; bTwo = bHave; bHave = true;
	if (!bTwo) S0 = S;
}

void AFlightViewer::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bOpen || !bHave || !Geo) return;
	// between the last two positions, a quarter of a second behind (so it moves smoothly), turning the short way
	const double Span = FMath::Max(0.06, S1.At - S0.At);
	const float K = FMath::Clamp(float((FPlatformTime::Seconds() - S1.At) / Span), 0.f, 1.f);
	FFlightSample S = S1;
	S.Lat = FMath::Lerp(S0.Lat, S1.Lat, double(K)); S.Lon = FMath::Lerp(S0.Lon, S1.Lon, double(K)); S.AltM = FMath::Lerp(S0.AltM, S1.AltM, double(K));
	const float Dh = FMath::Fmod(S1.Hdg - S0.Hdg + 540.f, 360.f) - 180.f;
	S.Hdg = FMath::Fmod(S0.Hdg + Dh * K + 360.f, 360.f);
	S.Gs = FMath::Lerp(S0.Gs, S1.Gs, K); S.Vs = FMath::Lerp(S0.Vs, S1.Vs, K);
	Shown = S;
	Place(S, Dt);
}

void AFlightViewer::Place(const FFlightSample& S, float Dt)
{
	const double H = FMath::Max(S.AltM, 2.0);
	FVector P = Geo->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(S.Lon, S.Lat, H));
	// the world kept round the aircraft: the globe's middle moved to it when it has gone 20 km (numbers stay small)
	if (IsHidden() || P.Size() > 2.0e6)
	{
		Geo->SetOriginLongitudeLatitudeHeight(FVector(S.Lon, S.Lat, H));
		P = Geo->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(S.Lon, S.Lat, H));
	}
	// the aircraft's frame on the globe: east, north and up where it is
	const FMatrix Esu = Geo->ComputeEastSouthUpToUnrealTransformation(P);
	const FVector East = Esu.GetScaledAxis(EAxis::X).GetSafeNormal(), North = -Esu.GetScaledAxis(EAxis::Y).GetSafeNormal(), Up = Esu.GetScaledAxis(EAxis::Z).GetSafeNormal();
	const float Hr = FMath::DegreesToRadians(S.Hdg);
	const FVector Fwd = (North * FMath::Cos(Hr) + East * FMath::Sin(Hr)).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(Up, Fwd);
	// climbing or descending: the nose up or down with the path; turning: the wings banked (at most 25°)
	const float Spd = FMath::Max(40.f, S.Gs * 0.5144f);
	const float PitchR = S.bOnGround ? 0.f : FMath::Clamp(FMath::Atan2(S.Vs * 0.00508f, Spd), -0.28f, 0.28f) + 0.035f;
	const float Turn = Dt > 0 ? FMath::DegreesToRadians(FMath::Fmod(S.Hdg - LastHdg + 540.f, 360.f) - 180.f) / Dt : 0.f;
	LastHdg = S.Hdg;
	const float WantRoll = S.bOnGround ? 0.f : FMath::Clamp(FMath::Atan(Spd * Turn / 9.81f), -0.44f, 0.44f);
	Roll = FMath::FInterpTo(Roll, WantRoll, Dt, 1.5f);
	const FVector F2 = (Fwd * FMath::Cos(PitchR) + Up * FMath::Sin(PitchR)).GetSafeNormal();
	const FVector U1 = FVector::CrossProduct(F2, Right).GetSafeNormal();
	const FVector U2 = (U1 * FMath::Cos(Roll) + Right * FMath::Sin(-Roll)).GetSafeNormal();
	SetActorLocationAndRotation(P, FRotationMatrix::MakeFromXZ(F2, U2).ToQuat());
	SetActorHiddenInGame(false);
	// the gear: down on the ground and below 2,000 ft (near an airport), up in the air
	const bool bGearDown = S.bOnGround || S.AltM < 600.0;
	for (UStaticMeshComponent* G : Gear) if (G) G->SetVisibility(bGearDown);

	// the camera round the aircraft: the mouse turns it, the wheel brings it closer or further
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		float Mx = 0, My = 0;
		PC->GetInputMouseDelta(Mx, My);
		Yaw += Mx * 2.2f; Pitch = FMath::Clamp(Pitch + My * 2.2f, -80.f, 80.f);
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) Dist = FMath::Max(2500.f, Dist * 0.88f);
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) Dist = FMath::Min(200000.f, Dist * 1.14f);
	}
	if (Cam)
	{
		const FQuat Frame = FRotationMatrix::MakeFromXZ(Fwd, Up).ToQuat();
		const FVector Off = Frame.RotateVector(FRotator(Pitch, Yaw, 0).Vector() * -Dist);
		const FVector Eye = P + Off;
		Cam->SetActorLocationAndRotation(Eye, FRotationMatrix::MakeFromXZ((P - Eye).GetSafeNormal(), Up).ToQuat());
	}
}
