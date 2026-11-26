// The cargo terminal's lights and sounds (step B2): Data/cargo-lights.json (Art/CargoTerminal/cargo_lights.py), all
// in model-local metres. Its fittings - the halls' LED high-bays on wires under the roof, the offices' ceiling panels,
// the canopies' floodlights, the truck docks' lamps and the truck court's masts - are a few instanced lists (Nanite
// where opaque); their light goes through the model's own lamp system (ABabylonModel::AddLamp: no shadows, no
// flares, each with a reach, dimmed by day as the passenger terminal's). Its sounds stand where they happen, fade
// with distance and are muffled through walls: the halls' hum, the refrigeration plant on the roof, the offices'
// ventilation and now and then a printer, and rain drumming on the metal roof and canopies while it rains. No phones.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CargoTerminalLife.generated.h"

class ABabylonModel;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;

UCLASS()
class AIRPORTDYNASTYPRO_API ACargoTerminalLife : public AActor
{
	GENERATED_BODY()
public:
	ACargoTerminalLife();
	/** The cargo terminal's lights and sounds on its model (once, after the model stands). */
	static ACargoTerminalLife* Make(ABabylonModel* Model);
	virtual void Tick(float Dt) override;

	/** A sound heard from where it is: falls off over FalloffCm, muffled through walls (the sound placement rule). */
	static USoundAttenuation* Heard(float FalloffCm);
	/** A sound by its path, kept for the game's life (held in statics: never collected). */
	static USoundBase* Keep(const TCHAR* Path);

private:
	struct FLoop { UAudioComponent* C = nullptr; float Vol = 1.f; uint8 Rain = 0; bool bOn = false; };   // (Rain: 0 always, 1 inside under the roof, 2 out under a canopy)
	TArray<FLoop> Loops;
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Audio;   // (keeps the loops' components)
	TArray<FVector> Printers;   // world
	// the truck court's gate barrier: its boom (the model's part GateBoom) swings up when the player or a vehicle comes
	// near the gate and down again once the gate has been clear a while
	TWeakObjectPtr<ABabylonModel> Model;
	int32 BoomGroup = INDEX_NONE;
	FTransform BoomPivot;      // (the model's root space)
	FVector GateAt = FVector::ZeroVector;   // world
	float BoomK = 0.f, BoomWant = 0.f, GateClear = 0.f;
	int8 BoomDir = 0;
	UPROPERTY() TObjectPtr<UAudioComponent> BoomHum;
	bool SomethingAtGate() const;
	void TickBoom(float Dt);
	float NextPrint = 90.f;
};
