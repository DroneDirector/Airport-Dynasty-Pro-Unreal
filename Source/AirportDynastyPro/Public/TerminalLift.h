// The staff lift up to the new offices (owner, 2026-11-15: "new offices get access from an elevator from the staff
// area hallway ... elevator door on 2nd floor leading into the offices (working elevator)"): a car in the shaft in the
// staff entrance hall's north-east corner (Art/TerminalAnnex), between Floor 1 (+0.01) and Floor 2 (+6.01). Its sliding
// doors open by themselves (the door rule: sliding doors open on their own): stand at its door on a floor and it comes
// and opens; step in and after a moment it closes, rides to the other floor and opens there.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerminalLift.generated.h"

class UStaticMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API ATerminalLift : public AActor
{
	GENERATED_BODY()
public:
	ATerminalLift();
	/** The lift in its shaft: plan metres of the car's floor (x0, x1, z0, z1), its door on the west side from z D0 to D1. */
	void Build(float X0, float X1, float Z0, float Z1, float D0, float D1);
	virtual void Tick(float Dt) override;

private:
	UStaticMeshComponent* Box(USceneComponent* Parent, const FVector& Lo, const FVector& Hi, const TCHAR* Mat);
	UPROPERTY() TObjectPtr<USceneComponent> Car;
	TArray<TObjectPtr<UStaticMeshComponent>> CarDoors;         // two leaves riding with the car
	TArray<TObjectPtr<UStaticMeshComponent>> LandDoors[2];     // two leaves at each floor
	float FloorY[2] = {0.01f, 6.01f};
	FVector2D Door = FVector2D::ZeroVector, Across = FVector2D::ZeroVector;   // plan z of the door's ends; car x span
	FBox2D Inside;                                                           // the car's floor (plan x, z)
	int32 At = 0, Going = -1;
	float Y = 0.01f, Open = 0.f, Wait = 0.f;
};
