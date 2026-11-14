// The terminal's placed props, from data (TerminalProps.h).
#include "TerminalProps.h"
#include "ADPLog.h"
#include "TerminalCrowd.h"
#include "TerminalPlan.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TMap<FString, TPair<FVector, FVector>> ATerminalProps::Placed;

ATerminalProps::ATerminalProps()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

int32 ATerminalProps::Build()
{
	FString Text; TSharedPtr<FJsonObject> R;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/terminal-props.json"))) && !FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/terminal-props.json")))) return 0;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), R) || !R.IsValid()) { ADP_ERROR(TEXT("Terminal props: Data/terminal-props.json can't be read")); return 0; }
	FTerminalPlan Plan; Plan.Load();
	int32 N = 0;
	for (const TSharedPtr<FJsonValue>& V : R->GetArrayField(TEXT("props")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const FString Id = O->GetStringField(TEXT("id")), MeshPath = O->GetStringField(TEXT("mesh"));
		double X = 0, Z = 0, Heading = 0, FrontYaw = 0, Front = 1.5, Sides = 0.6, Back = 0.6; int32 Floor = 1;
		O->TryGetNumberField(TEXT("x"), X); O->TryGetNumberField(TEXT("z"), Z); O->TryGetNumberField(TEXT("heading"), Heading);
		O->TryGetNumberField(TEXT("modelFrontYaw"), FrontYaw); O->TryGetNumberField(TEXT("floor"), Floor);
		const TSharedPtr<FJsonObject>* CZ = nullptr;
		if (O->TryGetObjectField(TEXT("clear"), CZ)) { (*CZ)->TryGetNumberField(TEXT("front"), Front); (*CZ)->TryGetNumberField(TEXT("sides"), Sides); (*CZ)->TryGetNumberField(TEXT("back"), Back); }
		TObjectPtr<UInstancedStaticMeshComponent>& C = ByMesh.FindOrAdd(MeshPath);
		if (!C)
		{
			UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *MeshPath);
			if (!M) { ADP_WARN(TEXT("Terminal props: %s - its model %s is missing"), *Id, *MeshPath); continue; }
			C = NewObject<UInstancedStaticMeshComponent>(this);
			C->SetupAttachment(Root); C->SetStaticMesh(M); C->SetCollisionProfileName(TEXT("BlockAll")); C->SetMobility(EComponentMobility::Movable);
			C->RegisterComponent(); AddInstanceComponent(C);
		}
		// heading 0: its front toward the landside entrances (the plan's -z: the world's +Y); the model's own front is at
		// modelFrontYaw in its local space (the import's turn)
		const double FacingWorldYaw = 90.0 - Heading;   // (heading 0 = world +Y = yaw 90; heading 90 = plan +x = world +X)
		const FVector At = FTerminalPlan::ToWorld(X, Plan.FloorY(Floor), Z);
		C->AddInstance(FTransform(FRotator(0.0, FacingWorldYaw - FrontYaw, 0.0), At), true);
		const FVector Dir = FRotator(0.0, FacingWorldYaw, 0.0).Vector();
		// its own lights ("lights": [{at: [x, y, z] cm in the model, color: [r, g, b], lumens, radius cm}]): the kiosk's
		// green status lamp shining on its handle and round its top (owner, 2026-11-14: lit by Lumen, glowing)
		const TArray<TSharedPtr<FJsonValue>>* LJ = nullptr;
		if (O->TryGetArrayField(TEXT("lights"), LJ))
			for (const TSharedPtr<FJsonValue>& LV : *LJ)
			{
				const TSharedPtr<FJsonObject> L = LV->AsObject();
				const TArray<TSharedPtr<FJsonValue>>& A = L->GetArrayField(TEXT("at")); const TArray<TSharedPtr<FJsonValue>>& Cl = L->GetArrayField(TEXT("color"));
				double Lm = 20, Rad = 150; L->TryGetNumberField(TEXT("lumens"), Lm); L->TryGetNumberField(TEXT("radius"), Rad);
				UPointLightComponent* PL = NewObject<UPointLightComponent>(this);
				PL->SetupAttachment(Root); PL->SetMobility(EComponentMobility::Movable);
				const FTransform T(FRotator(0.0, FacingWorldYaw - FrontYaw, 0.0), At);
				PL->SetWorldLocation(T.TransformPosition(FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber())));
				PL->SetIntensityUnits(ELightUnits::Lumens); PL->SetIntensity(Lm);
				PL->SetLightColor(FLinearColor(Cl[0]->AsNumber(), Cl[1]->AsNumber(), Cl[2]->AsNumber()));
				PL->SetAttenuationRadius(Rad); PL->SetSourceRadius(4.f); PL->SetSourceLength(28.f); PL->SetCastShadows(false);
				PL->RegisterComponent(); AddInstanceComponent(PL);
			}
		Placed.Add(Id, TPair<FVector, FVector>(At, Dir));
		// its clear zone for the passengers: a circle round its middle as wide as its widest side's clearance, set toward
		// the front by half the front/back difference (plan metres)
		const double Body = 0.36;
		const double Rad = Body + FMath::Max(Sides, (Front + Back) / 2.0);
		const FVector2D Fw(Dir.X / 100.0 * 100.0, -Dir.Y);   // (world +Y is plan -z)
		const FVector2D Centre(X + Fw.X * (Front - Back) / 2.0, Z + Fw.Y * (Front - Back) / 2.0);
		ATerminalCrowd::Obstacles.Add(FVector4(Centre.X, Plan.FloorY(Floor), Centre.Y, Rad));
		ADP_LOG(TEXT("Terminal props: %s at %.1f, %.1f (floor %d), heading %.0f, clear zone %.1f m round"), *Id, X, Z, Floor, Heading, Rad);
		++N;
	}
	return N;
}
