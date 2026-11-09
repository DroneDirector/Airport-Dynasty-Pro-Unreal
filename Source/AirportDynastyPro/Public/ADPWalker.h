// The player walking through the terminal in first person: walk, run, jump and crouch.
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
};
