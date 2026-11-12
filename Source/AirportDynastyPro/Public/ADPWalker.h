// The player walking through the terminal, in first person or seen from behind (V / D-pad up switches): walk, run, jump and crouch, in a body (Epic's mannequin).
// Your body is your own MetaHuman (owner, 2026-11-12: built by Tools/make_metahumans.py "player", in the outfit chosen at
// the office wardrobe, Wardrobe.h): its blueprint rides on the capsule in place of the mannequin, which stays as the
// invisible rig the game's own animations were made for, and the MetaHuman's body plays those same animations carried
// over to its skeleton (Tools/retarget_player_anims.py: the idle / walk / run blend space, the jump and the fall), fed
// from how you move. Without a built character the mannequin shows as before.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ADPWalker.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API AADPWalker : public ACharacter
{
	GENERATED_BODY()

public:
	virtual void Tick(float Dt) override;
	/** The field of view, degrees side to side as on a 16:9 screen (Settings' slider, 60-120); wider screens see more to
	 *  the sides, never less up and down. */
	static float Fov;
	AADPWalker();
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	/** Seen from behind (third person) rather than through your own eyes (owner, 2026-11-11). */
	bool IsThirdPerson() const { return bThird; }
	void SetThirdPerson(bool bOn);
	/** Where your view would be standing at At, looking along Look (getting out of a truck glides there). */
	FTransform ViewFrom(const FVector& At, const FRotator& Look) const;
	/** Led by something else (a truck walking you to its door): your own moving and turning keys do nothing meanwhile. */
	void SetLed(bool bOn) { bLed = bOn; }
	bool IsLed() const { return bLed; }
	/** Your body as the built character Who (an outfit of yours, Wardrobe.h); false when it isn't built. */
	bool WearMetaHuman(const FString& Who);
	/** Your MetaHuman on the capsule (tests turn it to the camera), or null. */
	AActor* HumanActor() const { return Human; }
	virtual void BeginPlay() override;

private:
	#pragma region Moves
	void MoveForward(float V);
	void MoveRight(float V);
	void TurnBy(float V) { if (!bLed) AddControllerYawInput(V); }
	void LookUpBy(float V) { if (!bLed) AddControllerPitchInput(V); }
	void RunOn();
	void RunOff();
	void ToggleCrouch();
	#pragma endregion

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Eyes;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Behind;
	bool bHeadHidden = false, bThird = false, bViewRead = false, bLed = false;
	// your MetaHuman on the capsule: the whole built blueprint, its body driven here
	UPROPERTY() TObjectPtr<AActor> Human;
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> HumanBody;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> HumanHead;   // the face and its grooms: hidden from your own eyes
	UPROPERTY() TObjectPtr<class UBlendSpace> MoveBlend;
	UPROPERTY() TObjectPtr<class UAnimSequence> FallAnim;
	bool bHumanFalling = false;
	void DriveHuman(float Dt);
	void ToggleView() { SetThirdPerson(!bThird); }
};
