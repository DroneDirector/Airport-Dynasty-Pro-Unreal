// Swiss flags that really wave (to-do 168; owner, 2026-11-20 night): at Chablais one on a pole by the fire station and one
// on the forecourt in front of the terminal. The Swiss flag in its official proportions (square; the cross's arms 1/6
// longer than wide: the cross 20/32 of the flag, its arms 6/32 wide). A real cloth, always simulated (our own: a grid of
// particles held together by stretch, shear and bend springs - position-based dynamics), hanging from its rope on the
// pole: limp in calm air, streaming out and snapping in strong wind, its direction and gusts from the REAL wind of the
// live weather (the windsocks' wind, to-do 193). Never a flat picture, never switched near/far; it only pauses while
// nobody can see it is its mesh redrawn less often (the cloth itself is always simulated). The fabric: M_FlagCloth (light through it, sheen, weave, wet in rain).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Flags.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class AIRPORTDYNASTYPRO_API AFlags : public AActor
{
	GENERATED_BODY()
public:
	AFlags();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	/** Places the site's flags (a Swiss site only - Iceland keeps its own until the owner says). */
	static void Place(UWorld* W);

private:
	struct FCloth
	{
		FVector Top;                        // the top of the hoist (where the rope holds the flag), world cm
		TArray<FVector> P, Prev;            // particles, world cm
		UProceduralMeshComponent* Mesh = nullptr;   // (owned by the actor: AddInstanceComponent)
		TArray<FVector> Nrm;                // the face normals (scratch, each step)
		float Pause = 0.f;                  // seconds since its mesh was last redrawn
	};
	TArray<FCloth> Cloths;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Mat;
	float Accum = 0.f, Clock = 0.f, RunwayHdg = 336.f;
	void Add(const FVector& Foot, float PoleM);
	void Step(FCloth& C, float H);
	void Draw(FCloth& C, bool bFirst);
};
