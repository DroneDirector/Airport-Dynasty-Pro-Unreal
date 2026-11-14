// Using a garage-door button panel (owner, 2026-11-14): E / X at a panel (inside the fire station by each bay door, inside
// each hangar by its big door) and you step up to it, your arm comes out and your index finger points at the green
// button. Mouse up / down (or the wheel, W / S, the arrow keys, the D-pad or the left stick) moves the finger between the
// green (open) and the red (close) button; E, the left mouse button or X presses it; the view stays on the panel - the fingertip pushes the button in,
// its light swaps, and the door beside that panel (only that one) goes up or down. Changing your mind mid-move: the door
// stops, waits about a second, then goes the new way. A press that has started always finishes. Back (Esc, B or the
// pad's Back button) brings your arm all the way back to your side. Doors work only from these panels.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DoorPanelUse.generated.h"

class ABabylonModel;
class AADPWalker;

UCLASS()
class AIRPORTDYNASTYPRO_API ADoorPanelUse : public AActor
{
	GENERATED_BODY()
public:
	ADoorPanelUse();
	virtual void Tick(float Dt) override;
	/** E / X: start using the panel you look at (or the one right by you); while using one, press the chosen button.
	 *  True when it did either. */
	static bool Interact(APawn* Who, AActor* Aimed);
	/** Back: arm down, done with the panel. */
	static bool Leave();
	static bool IsActive();
	/** The button your finger is on (its cap and ring): glowing at its edge, as usable things do. */
	static void ChosenParts(TArray<TPair<class UInstancedStaticMeshComponent*, int32>>& Out);
	/** A panel within reach of At that you face (the Buttons panel lists it). */
	static bool NearPanel(const FVector& At);
	/** Tests: use this panel, pick a button (true = green), press. */
	static void TestUse(APawn* Who, ABabylonModel* P);
	static void TestPick(bool bGreen);
	static void TestPress();

	static constexpr float Reach = 160.f;

private:
	static TWeakObjectPtr<ADoorPanelUse> One;
	static ABabylonModel* PanelFor(APawn* Who, AActor* Aimed);
	void Start(AADPWalker* Me, ABabylonModel* P);
	void Press();
	void Command(bool bOpen);
	FVector ButtonAt(bool bGreen) const;          // the button cap's middle, on its face
	FVector Face() const;                          // straight out of the panel's face
	void PushButton(bool bGreen, float Depth);    // the cap pressed in by Depth cm

	TWeakObjectPtr<AADPWalker> User;
	TWeakObjectPtr<ABabylonModel> Panel;
	enum class EState : uint8 { None, In, Use, Pressing, Out } State = EState::None;
	float T = 0.f, PressT = 0.f, Aim = 1.f;       // Aim: 1 at green, 0 at red (the finger moves between)
	bool bGreen = true, bPressGreen = true, bCommanded = false, bWasThird = false;   // (bWasThird: came from behind - back to it after)
	FVector StandFrom = FVector::ZeroVector, StandAt = FVector::ZeroVector, WristAt = FVector::ZeroVector;
	float LookYaw = 0.f, MouseY = 0.f, StickWas = 0.f;
	// the close view while you use it (owner, 2026-11-14): beside the panel, your hand and some of your arm big in it
	UPROPERTY() TObjectPtr<class ACameraActor> Cam;
	void PlaceCam(float Dt, bool bSnap);
};
