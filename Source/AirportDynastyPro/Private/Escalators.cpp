#include "Escalators.h"
#include "ADPLog.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// (Blender's escalator: +Y up the slope, X across, Z up; imported, its +Y is Unreal's -Y)
	FVector Local(float X, float U, float H) { return FVector(X, -U * 100.f, H * 100.f); }
	bool ReadPath(TArray<FVector2f>& Path, TArray<float>& S, float& Pitch, float& TopU)
	{
		FString Txt;
		const FString A = FPaths::ProjectContentDir() / TEXT("Data/escalator.json"), B = FPaths::ProjectDir() / TEXT("Data/escalator.json");
		if (!FFileHelper::LoadFileToString(Txt, *A) && !FFileHelper::LoadFileToString(Txt, *B)) return false;
		TSharedPtr<FJsonObject> J; if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Txt), J) || !J.IsValid()) return false;
		Pitch = J->GetNumberField(TEXT("pitch")); TopU = J->GetNumberField(TEXT("comb_top_u"));
		for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("path")))
		{
			const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray();
			S.Add(P[0]->AsNumber()); Path.Add(FVector2f(P[1]->AsNumber(), P[2]->AsNumber()));
		}
		return Path.Num() > 2;
	}
}

AEscalatorPair::AEscalatorPair()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

float AEscalatorPair::LengthAlong()
{
	TArray<FVector2f> P; TArray<float> S; float Pi = 0.4f, T = 13.76f;
	return ReadPath(P, S, Pi, T) ? T : 13.76f;
}

FVector2f AEscalatorPair::At(float S) const
{
	int32 Lo = 0, Hi = PathS.Num() - 1;
	if (S <= PathS[0]) return Path[0];
	if (S >= PathS[Hi]) return Path[Hi];
	while (Hi - Lo > 1) { const int32 M = (Lo + Hi) / 2; (PathS[M] <= S ? Lo : Hi) = M; }
	const float T = (S - PathS[Lo]) / FMath::Max(1e-6f, PathS[Hi] - PathS[Lo]);
	return FMath::Lerp(Path[Lo], Path[Hi], T);
}

float AEscalatorPair::SlopeAt(float S) const
{
	const FVector2f A = At(S - 0.05f), B = At(S + 0.05f);
	return FMath::Atan2(B.Y - A.Y, FMath::Max(1e-4f, B.X - A.X));
}

bool AEscalatorPair::Build(float Speed, bool bShortEnds)
{
	if (!ReadPath(Path, PathS, Pitch, TopU)) { ADP_WARN(TEXT("Escalators: no Data/escalator.json (Art/Escalator/make_escalator.py)")); return false; }
	PathLen = PathS.Last();
	NSteps = FMath::CeilToInt(PathLen / Pitch) + 1;           // a loop a little longer than the path: the rest is out of sight
	Loop = NSteps * Pitch;
	UStaticMesh* StepM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Escalator/SM_EscStep.SM_EscStep"));
	UStaticMesh* BodyM = LoadObject<UStaticMesh>(nullptr, bShortEnds ? TEXT("/Game/Escalator/SM_EscBodyShort.SM_EscBodyShort") : TEXT("/Game/Escalator/SM_EscBody.SM_EscBody"));
	UStaticMesh* RampM = LoadObject<UStaticMesh>(nullptr, bShortEnds ? TEXT("/Game/Escalator/SM_EscRampShort.SM_EscRampShort") : TEXT("/Game/Escalator/SM_EscRamp.SM_EscRamp"));
	if (!StepM || !BodyM || !RampM) { ADP_WARN(TEXT("Escalators: the meshes aren't in /Game/Escalator (Tools/import_escalator.py)")); return false; }
	for (int32 k = 0; k < 2; ++k)
	{
		FUnit Un; Un.X = k * 162.f; Un.Speed = k == 0 ? Speed : -Speed; Un.Phase = k * 0.17f;
		Un.Body = NewObject<UStaticMeshComponent>(this); Un.Body->SetStaticMesh(BodyM); Un.Body->SetupAttachment(GetRootComponent());
		Un.Body->SetRelativeLocation(FVector(Un.X, 0, 0)); Un.Body->SetMobility(EComponentMobility::Movable);   // (a still part under a movable root must be movable too, or it isn't attached)
		 Un.Body->RegisterComponent();
		Un.Ramp = NewObject<UStaticMeshComponent>(this); Un.Ramp->SetStaticMesh(RampM); Un.Ramp->SetupAttachment(GetRootComponent());
		Un.Ramp->SetRelativeLocation(FVector(Un.X, 0, 0)); Un.Ramp->SetMobility(EComponentMobility::Movable); Un.Ramp->SetHiddenInGame(true); Un.Ramp->SetCastShadow(false);
		Un.Ramp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Un.Ramp->SetCollisionProfileName(TEXT("BlockAll")); Un.Ramp->RegisterComponent();
		Un.Steps = NewObject<UHierarchicalInstancedStaticMeshComponent>(this); Un.Steps->SetStaticMesh(StepM); Un.Steps->SetupAttachment(GetRootComponent());
		Un.Steps->SetCollisionEnabled(ECollisionEnabled::NoCollision); Un.Steps->SetMobility(EComponentMobility::Movable); Un.Steps->RegisterComponent();
		TArray<FTransform> T; for (int32 i = 0; i < NSteps; ++i) T.Add(FTransform(Local(Un.X, 0, -5)));
		Un.Steps->AddInstances(T, false);
		Units.Add(Un);
	}
	ADP_LOG(TEXT("Escalators: two side by side, %d steps each on a %.1f m loop (%.1f m of path), %.2f m/s up and down"), NSteps, Loop, PathLen, Speed);
	return true;
}

void AEscalatorPair::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Units.Num()) return;
	for (FUnit& Un : Units)
	{
		Un.Phase = FMath::Fmod(Un.Phase + Un.Speed * Dt + Loop, Loop);
		TArray<FTransform> T; T.Reserve(NSteps);
		for (int32 i = 0; i < NSteps; ++i)
		{
			const float S = FMath::Fmod(i * Pitch + Un.Phase, Loop);
			if (S > PathLen) { T.Add(FTransform(FQuat::Identity, Local(Un.X, 0.f, -3.f), FVector(0.001f))); continue; }   // round the back, out of sight
			const FVector2f P = At(S);
			T.Add(FTransform(Local(Un.X, P.X, P.Y)));             // the tread stays level all the way, as on a real escalator
		}
		Un.Steps->BatchUpdateInstancesTransforms(0, T, false, true, true);
	}
	Carry(Dt);
}

void AEscalatorPair::Carry(float Dt)
{
	// whoever stands on the steps rides with them
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me) return;
	const FVector L = GetActorTransform().InverseTransformPosition(Me->GetActorLocation() - FVector(0, 0, Me->GetSimpleCollisionHalfHeight()));
	const float U = -L.Y / 100.f;
	if (U < -0.2f || U > TopU + 0.2f) return;
	for (const FUnit& Un : Units)
	{
		if (FMath::Abs(L.X - Un.X) > 50.f) continue;
		// the tread height there: the path at this u (searched by u along the flat-ish path)
		float H = 0.f; for (int32 i = 1; i < Path.Num(); ++i) if (Path[i].X >= U) { H = Path[i].Y; break; }
		if (FMath::Abs(L.Z / 100.f - H) > 0.35f) continue;
		float S = 0.f; for (int32 i = 1; i < Path.Num(); ++i) if (Path[i].X >= U) { S = PathS[i]; break; }
		const float A = SlopeAt(S);
		const FVector Dir = GetActorTransform().TransformVectorNoScale(FVector(0, -FMath::Cos(A), FMath::Sin(A)));
		Me->AddActorWorldOffset(Dir * Un.Speed * 100.f * Dt, true);
		return;
	}
}
