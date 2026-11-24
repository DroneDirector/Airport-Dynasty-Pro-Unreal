// You in a service vehicle (to-do 212; GSEService.cpp): E at a door - you step up to it, the same hand on the handle, door,
// steps and seat as the crew (AGSE's own poses, on your own body), then you drive it (the driver's seat) or sit (a crew seat,
// or the seat beside the driver: there you may ride along with its crew). E again: out the same way, standing still. V: the
// seat's view or from behind. The vehicle itself stays AGSE's: its wheels, lamps, doors, dirt and sounds.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TruckDriverRig.h"
#include "ServiceDrive.generated.h"

class AGSE;
class UCameraComponent;
class UPoseableMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API AServiceDrive : public APawn
{
	GENERATED_BODY()
public:
	AServiceDrive();
	virtual void Tick(float Dt) override;
	virtual void SetupPlayerInputComponent(UInputComponent* In) override;
	bool Begin(AGSE* G, int32 Vehicle, int32 InSeat, APawn* Walker);
	/** Out by the door (at a standstill; it is braked first when you drive). */
	void Exit();
	/** Out at once (a quick-start that moves you elsewhere). */
	void ExitNow();
	bool AtWheel() const { return Seat == 0 && Seq == ESeq::Sat; }
	float Throttle() const { return AtWheel() && !bOutWanted ? FMath::Clamp(FMath::Max(KeyFwd, PadT), 0.f, 1.f) : 0.f; }
	float Brake() const { return AtWheel() ? (bOutWanted ? 1.f : FMath::Clamp(FMath::Max(-KeyFwd, PadB), 0.f, 1.f)) : 1.f; }
	float SteerIn() const { return AtWheel() && !bOutWanted ? FMath::Clamp(KeySteer, -1.f, 1.f) : 0.f; }
	bool Leaving() const { return bOutWanted; }
	// in the de-icing truck's cabin (GSEDeicer.cpp): the boom by the move keys / left stick, out and in by Shift / Ctrl or the
	// triggers, type I / type IV by the left / right mouse button or the right / left shoulder button, Home (pad Y): stowed
	bool Seated() const { return Seq == ESeq::Sat; }
	float BoomLift() const { return Seated() && !bOutWanted ? FMath::Clamp(KeyFwd, -1.f, 1.f) : 0.f; }
	float BoomSlew() const { return Seated() && !bOutWanted ? FMath::Clamp(KeySteer, -1.f, 1.f) : 0.f; }
	float BoomOut() const;
	int32 SprayKey() const;
	bool StowKey() const;
	float LookY() const { return LookYaw; }
	float LookP() const { return LookPitch; }

private:
	enum class ESeq : uint8 { Walk, Handle, In, Sat, Out };
	UPROPERTY() TObjectPtr<UCameraComponent> Cam;
	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Rider;
	UPROPERTY() TObjectPtr<class UDriverPoseAnim> Human;
	UPROPERTY() TObjectPtr<class USkeletalMesh> Manny;
	TWeakObjectPtr<AGSE> Gse; TWeakObjectPtr<APawn> Me;
	int32 Ti = INDEX_NONE, Seat = 0; ESeq Seq = ESeq::Walk; float T = 0.f, CamT = 0.f;
	FTruckDriverRig Rig; FTransform CamFrom;
	bool bOutWanted = false, bBehind = false, bHead = false, bDoor = false;
	float KeyFwd = 0.f, KeySteer = 0.f, PadT = 0.f, PadB = 0.f, LookYaw = 0.f, LookPitch = 0.f;
	void Climb();
	void Pose(float K);
	void View(float Dt);
	void Finish();
	void Fwd(float V) { KeyFwd = V; }
	void Right(float V) { KeySteer = V; }
	void Turn(float V) { LookYaw = FMath::Clamp(LookYaw + V * 2.f, -150.f, 150.f); }
	void LookUp(float V) { LookPitch = FMath::Clamp(LookPitch + V * 2.f, -60.f, 50.f); }
	void PadThrottle(float V) { PadT = V; }
	void PadBrake(float V) { PadB = V; }
	void ToggleView() { bBehind = !bBehind; CamT = 10.f; LookYaw = LookPitch = 0.f; }
};
