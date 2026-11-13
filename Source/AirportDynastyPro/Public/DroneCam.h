// Drone mode (owner, 2026-11-13): no drone you can see, a camera that flies like one, for looking round and testing.
// Opened from the full-screen holographic map's DRONE MODE button; Esc (or B / Circle on a pad) takes you back to
// where you stood, in the view you had. It flies through everything (no collision). Forward, back and sideways stay
// level whatever way you look, as a real drone does; up and down only with their own keys; the mouse turns and tilts
// the view. The mouse wheel (D-pad up/down) steps the speed; holding Shift (the left stick pressed) boosts it hugely,
// up to 300 m/s, to reach the clouds in seconds. It speeds up and slows down smoothly, never dead stops.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DroneCam.generated.h"

class UCameraComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API ADroneCam : public APawn
{
	GENERATED_BODY()
public:
	ADroneCam();
	virtual void Tick(float Dt) override;
	virtual void SetupPlayerInputComponent(UInputComponent* In) override;
	/** The speed steps (m/s) the wheel moves through, and the boost's top speed. */
	static constexpr int32 NumSteps = 9;
	static const float Steps[NumSteps];
	static constexpr float BoostSpeed = 300.f;
	float SpeedNow() const { return Vel.Size() / 100.f; }          // m/s
	float StepSpeed() const { return Steps[Step]; }                // m/s
	bool Boosting() const { return bBoost; }
	/** Height above whatever is below (m), or -1 when nothing is within 20 km. */
	float HeightAbove() const { return Above; }
	/** Called on Esc / B: the HUD takes you back. */
	TFunction<void()> OnLeave;
	/** Tests: fly as if these were held (forward, right, up: -1..1), boost. */
	void SetTestInput(float F, float R, float U, bool bB) { bTest = true; TF = F; TR = R; TU = U; bBoost = bB; }
	void StepUp() { Step = FMath::Min(Step + 1, NumSteps - 1); }
	void StepDown() { Step = FMath::Max(Step - 1, 0); }

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Cam;
	FVector Vel = FVector::ZeroVector;
	float InF = 0, InR = 0, InUpKey = 0, InDownKey = 0, InUpPad = 0, InDownPad = 0, Above = -1;
	int32 Step = 4;
	bool bBoost = false, bTest = false;
	float TF = 0, TR = 0, TU = 0;
	void Fwd(float V) { InF = V; }
	void Right(float V) { InR = V; }
	void Turn(float V) { AddControllerYawInput(V); }
	void Look(float V) { AddControllerPitchInput(V); }
	void UpPad(float V) { InUpPad = V; }
	void DownPad(float V) { InDownPad = V; }
	void UpOn() { InUpKey = 1; }
	void UpOff() { InUpKey = 0; }
	void DownOn() { InDownKey = 1; }
	void DownOff() { InDownKey = 0; }
	void BoostOn() { bBoost = true; }
	void BoostOff() { bBoost = false; }
	void Leave() { if (OnLeave) OnLeave(); }
};
