// Our own lamp flares (owner, 2026-11-16: "create our own realistic flare"; Unreal's camera flares ghost and stay off):
// a sprite at each lamp face (Tools/make_lamp_flare.py: core, soft glow, six thin aperture spikes) that shows while its
// lamp is lit, strongest when the lamp faces you (a floodlight seen from behind gives almost none), fading in with the
// dark. Light masts and street lights add theirs; the Settings switch "LAMP FLARES AT NIGHT" turns them all off.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LampFlares.generated.h"

class ULocalLightComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API ALampFlares : public AActor
{
	GENERATED_BODY()
public:
	ALampFlares();
	virtual void Tick(float Dt) override;
	/** The one in the world (made on first use). */
	static ALampFlares* Get(UWorld* W);
	/** A flare on this lamp: shown while the light is visible, its colour, its size (a share of the screen's height). */
	void Add(ULocalLightComponent* Light, const FLinearColor& Color, float Size = 0.05f);
private:
	struct FFlare { TWeakObjectPtr<ULocalLightComponent> Light; TObjectPtr<class UMaterialBillboardComponent> Sprite; TObjectPtr<class UMaterialInstanceDynamic> Mat; float Now = -1.f; };
	TArray<FFlare> Flares;
	UPROPERTY() TArray<TObjectPtr<UObject>> Keep;
	float Check = 0.f; bool bOn = true;
};
