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
	/** Its body (the interaction outline, InteractOutline.h). */
	UPrimitiveComponent* BodyPart() const;

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

	// the changing room (owner, 2026-11-12, as in the big open-world games): the camera glides in and frames you big on
	// the right of the screen with the menu to the left, the room behind you blurred as a portrait photo while you stay
	// sharp; what you are changing sets the view: the whole body, your face (hats, sunglasses), your feet (shoes)
	enum class EView : uint8 { Body, Face, Feet };
	/** The categories the menu shows, always all of them (an empty one reads empty). */
	static const TArray<FString>& Categories();
	static EView ViewFor(const FString& Category);
	/** The camera to the changing-room view of you (From: your own view, where the glide starts). */
	void Look(EView View, class APlayerController* PC);
	/** The camera back to you. */
	void LookBack(class APlayerController* PC);

private:
	UPROPERTY() TObjectPtr<class ACameraActor> Cam;
	EView ViewNow = EView::Body;
	void PlaceCam(EView View);

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DoorL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DoorR;
	UPROPERTY() TObjectPtr<AActor> Preview;
	float OpenK = 0.f, OpenTarget = 0.f;
};
