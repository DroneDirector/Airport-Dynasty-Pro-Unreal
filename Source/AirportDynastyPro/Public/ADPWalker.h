// The player walking through the terminal in first person: walk, run, jump and crouch, in a body (Epic's mannequin).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ADPWalker.generated.h"

class UCameraComponent;

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

private:
	#pragma region Moves
	void MoveForward(float V);
	void MoveRight(float V);
	void RunOn();
	void RunOff();
	void ToggleCrouch();
	#pragma endregion

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Eyes;
	bool bHeadHidden = false;
};
