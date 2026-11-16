#include "Adverts.h"
#include "TerminalPlan.h"
#include "ADPLog.h"
#include "ADPKeep.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

AAdverts::AAdverts()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

namespace
{
	// a spot: the floor, a point in the room (plan metres), the way to the wall (plan x, z), the screen's width (m) and
	// the height of its middle over the floor (m)
	struct FAdSpot { int32 Floor; float X, Z, DX, DZ, Width, Mid; const TCHAR* Where; };
	TArray<FAdSpot> Spots()
	{
		TArray<FAdSpot> S = {
			// the Grand Hall (check-in): both end walls, and the walls between the three entrances, over the doors' height
			{1, 40.f, 15.f, -1, 0, 6.f, 3.2f, TEXT("hall west")}, {1, 110.f, 15.f, 1, 0, 6.f, 3.2f, TEXT("hall east")},
			{1, 61.5f, 4.f, 0, -1, 6.f, 3.6f, TEXT("hall south 1")}, {1, 89.f, 4.f, 0, -1, 6.f, 3.6f, TEXT("hall south 2")},
			// airside: the lobby after security, the Food Walk, the Tax Free walk-through
			{1, 54.f, 47.f, -1, 0, 4.f, 2.8f, TEXT("airside lobby")},
			{2, 35.f, 34.f, 0, -1, 6.f, 3.2f, TEXT("food walk")}, {2, 60.f, 36.f, -1, 0, 4.f, 2.8f, TEXT("tax free")},
		};
		// the walkway to the gates: on its south wall, every 50 m or so, facing the people walking
		for (const float X : {-55.f, -5.f, 45.f, 120.f, 170.f, 220.f}) S.Add({2, X, 56.f, 0, -1, 5.f, 3.0f, TEXT("walkway")});
		// each gate's waiting area: its west wall (plan.ts MOD: the four gate modules)
		for (int32 k = 0; k < 4; ++k) S.Add({2, -72.6f + 80.55f * k + 3.f, 75.f, -1, 0, 4.f, 2.8f, TEXT("gate")});
		return S;
	}
}

void AAdverts::Build(const FTerminalPlan& Plan)
{
	UStaticMesh* M = ADPKeep(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Adverts/SM_LedScreen.SM_LedScreen")));
	if (!M) { ADP_WARN(TEXT("Adverts: SM_LedScreen is missing (Tools/import_adverts.py)")); return; }
	Screens = NewObject<UInstancedStaticMeshComponent>(this, TEXT("LedScreens"));
	Screens->SetupAttachment(GetRootComponent()); Screens->SetStaticMesh(M);
	Screens->SetNumCustomDataFloats(3);
	Screens->SetCollisionProfileName(TEXT("BlockAll"));
	Screens->RegisterComponent();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AdWall), true); Q.AddIgnoredActor(this); Q.bReturnFaceIndex = true;
	FRandomStream R(52);
	int32 N = 0;
	for (const FAdSpot& S : Spots())
	{
		const float Y = Plan.FloorY(S.Floor) + S.Mid;
		const FVector From = FTerminalPlan::ToWorld(S.X, Y, S.Z);
		const FVector Dir = (FTerminalPlan::ToWorld(S.X + S.DX, Y, S.Z + S.DZ) - From).GetSafeNormal();
		FHitResult H;
		if (!GetWorld()->LineTraceSingleByChannel(H, From, From + Dir * 2500.f, ECC_Visibility, Q) || FMath::Abs(H.ImpactNormal.Z) > 0.3f)
		{ ADP_LOG(TEXT("Adverts: no wall found for the %s screen"), S.Where); continue; }
		// never on glass (the façades and the gates' glass walls): a screen hangs on a solid wall
		{
			int32 Sec = 0; UMaterialInterface* Mi = H.GetComponent() ? H.GetComponent()->GetMaterialFromCollisionFaceIndex(H.FaceIndex, Sec) : nullptr;
			const FString Mn = Mi ? Mi->GetName() : FString();
			if (Mn.Contains(TEXT("glass"), ESearchCase::IgnoreCase) || Mn.Contains(TEXT("window"), ESearchCase::IgnoreCase)) { ADP_LOG(TEXT("Adverts: the %s spot is glass (%s) - none there"), S.Where, *Mn); continue; }
		}
		const FVector Nrm = FVector(H.ImpactNormal.X, H.ImpactNormal.Y, 0.f).GetSafeNormal();
		const float Sc = S.Width / 1.6f;
		const FVector At = H.ImpactPoint + Nrm * 1.f;
		const int32 Ix = Screens->AddInstance(FTransform(Nrm.Rotation(), At, FVector(Sc)), true);
		FShow Sh; Sh.A = R.RandRange(0, 7); Sh.B = (Sh.A + 1 + R.RandRange(0, 5)) % 8; Sh.Hold = R.FRandRange(8.f, 13.f); Sh.Clock = R.FRandRange(0.f, 8.f);
		Shows.Add(Sh);
		const float Data[3] = {(float)Sh.A, (float)Sh.B, 0.f};
		Screens->SetCustomData(Ix, TArrayView<const float>(Data, 3), false);
		Faces.Add(TPair<FVector, FVector>(At + Nrm * 9.f * Sc, Nrm));
		++N;
	}
	Screens->MarkRenderStateDirty();
	ADP_LOG(TEXT("Adverts: %d LED screens up"), N);
}

void AAdverts::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Screens) return;
	bool bAny = false;
	for (int32 i = 0; i < Shows.Num(); ++i)
	{
		FShow& S = Shows[i];
		S.Clock += Dt;
		// held for its time, then a 1.2 s wipe over to the next; then the next becomes the shown one
		const float Wipe = FMath::Clamp((S.Clock - S.Hold) / 1.2f, 0.f, 1.f);
		if (S.Clock >= S.Hold + 1.2f) { S.A = S.B; S.B = (S.B + 1 + (i % 3)) % 8; if (S.B == S.A) S.B = (S.B + 1) % 8; S.Clock = 0.f; }
		if (S.Clock >= S.Hold || S.Clock < Dt * 1.5f)
		{
			const float Data[3] = {(float)S.A, (float)S.B, S.Clock >= S.Hold ? Wipe : 0.f};
			Screens->SetCustomData(i, TArrayView<const float>(Data, 3), false); bAny = true;
		}
	}
	if (bAny) Screens->MarkRenderStateDirty();
}
