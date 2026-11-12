// The office wardrobe (owner, 2026-11-12): a tall two-door walnut cabinet in every office (Art/Wardrobe, Tools/
// set_wardrobe.py), where you change your clothes. E / pad X at it swings the doors open and shows the outfits you
// own: every built copy of your own character (your MetaHuman, built by Tools/make_metahumans.py "player"; each
// outfit made later is another built copy). Picking one stands you beside the wardrobe wearing it, so you can look,
// and saves the choice (ADP.Player Outfit), which your walking body takes once it walks as your MetaHuman.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Wardrobe.generated.h"

class UStaticMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API AWardrobe : public AActor
{
	GENERATED_BODY()

public:
	AWardrobe();
	/** Builds the cabinet where the actor stands (its front along its own +y). */
	bool Build();
	virtual void Tick(float Dt) override;
	void SetOpen(bool bOpen);
	bool IsOpen() const { return OpenTarget > 0.5f; }

	struct FOutfit { FString Label; FString Who; };
	/** The outfits you own, the saved choice first: every built copy of your own character. */
	static TArray<FOutfit> Outfits();
	/** The outfit you chose last (saved), or your character as built. */
	static FString Chosen();
	/** You beside the wardrobe wearing Who (the built character of that name), the choice saved. */
	void Dress(const FString& Who, const FVector& LookFrom);
	/** The one standing there taken away (the wardrobe closed). */
	void Undress();

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DoorL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DoorR;
	UPROPERTY() TObjectPtr<AActor> Preview;
	float OpenK = 0.f, OpenTarget = 0.f;
};
