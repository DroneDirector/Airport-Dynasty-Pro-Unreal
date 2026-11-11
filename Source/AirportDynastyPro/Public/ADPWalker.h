// The player walking through the terminal, in first person or seen from behind (V / D-pad up switches): walk, run, jump and crouch, in a body (Epic's mannequin).
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

private:
	#pragma region Moves
	void MoveForward(float V);
	void MoveRight(float V);
	void RunOn();
	void RunOff();
	void ToggleCrouch();
	#pragma endregion

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Eyes;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Behind;
	bool bHeadHidden = false, bThird = false, bViewRead = false;
	void ToggleView() { SetThirdPerson(!bThird); }
};
