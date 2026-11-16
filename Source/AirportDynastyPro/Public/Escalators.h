// The terminal's escalators (owner, 2026-11-15): two side by side, one up and one down, as in a real airport, their
// steps running round endlessly. The steps' path comes from Data/escalator.json (Art/Escalator/make_escalator.py):
// in from under the bottom floor plate, three flat steps, the curve into 30 degrees, the slope, the curve out, three
// flat steps, under the top floor plate - beyond that a step is out of sight (in the truss) until it comes round to
// the bottom again. Each escalator's still parts are one mesh (skirts, decking, cladding, comb and floor plates,
// glass, newels, handrails); its steps are copies of one Nanite step moved along the path every frame. An invisible
// sloped surface along the steps is what you walk on, and standing on it carries you at the steps' speed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Escalators.generated.h"

class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API AEscalatorPair : public AActor
{
	GENERATED_BODY()
public:
	AEscalatorPair();
	/** Both escalators at this actor (its origin: the bottom comb's teeth, in the middle of the up escalator; its -Y the
	 *  way up). Speed in m/s (0.5 is the usual). */
	bool Build(float Speed = 0.5f, bool bShortEnds = false);   // (bShortEnds: the arrivals escalators - their ends past the combs a third as long, SM_EscBodyShort)
	virtual void Tick(float Dt) override;
	/** Escalators' length from the bottom comb to the top comb (m, along the floor), for placing them. */
	static float LengthAlong();

private:
	struct FUnit
	{
		TObjectPtr<UStaticMeshComponent> Body;
		TObjectPtr<UStaticMeshComponent> Ramp;
		TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Steps;
		float X = 0.f;          // across (cm, local)
		float Speed = 0.f;      // m/s along the path; + up, - down
		float Phase = 0.f;      // m
	};
	TArray<FUnit> Units;
	TArray<FVector2f> Path;     // (u, h) in m, by arc length below
	TArray<float> PathS;
	float PathLen = 0.f, Pitch = 0.4f, Loop = 0.f, TopU = 0.f;
	int32 NSteps = 0;
	FVector2f At(float S) const;
	float SlopeAt(float S) const;
	void Carry(float Dt);
};
