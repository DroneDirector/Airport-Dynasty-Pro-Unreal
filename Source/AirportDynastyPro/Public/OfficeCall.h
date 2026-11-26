// Calling a staff member into your office (owner, 2026-11-26): taken off their job (when free), they come along the
// staff corridor out of your view, knock, open the office door, shut it behind them, walk round the furniture (waiting
// for you or the dog in their way) to the visitor's chair and sit; the talk opens once you're in the office. When it
// ends they get up, go out the same way, shut the door and go back to their job (docs/Hired-Crew-Plan.md).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OfficeCall.generated.h"

UCLASS()
class AIRPORTDYNASTYPRO_API UOfficeCall : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(const EWorldType::Type Kind) const override { return Kind == EWorldType::Game || Kind == EWorldType::PIE; }
	virtual void Tick(float Dt) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UOfficeCall, STATGROUP_Tickables); }
	static UOfficeCall* Get(UWorld* W);
	/** Calls them in; false (logged why): somebody is already called in, or they are busy at a job just now. */
	bool Call(const FString& StaffId);
	bool Busy() const { return Phase != EPhase::None; }
private:
	enum class EPhase : uint8 { None, Spawn, In, Knock, Opening, Through, Shut, ToChair, Sitting, Seated, Up, Out, OpenOut, ThroughOut, ShutOut, Away };
	EPhase Phase = EPhase::None;
	FString Id, Name, Face;
	TWeakObjectPtr<AActor> V, Pet;
	TArray<FVector> Legs;   // (world, on the floor: the walk still ahead)
	int32 Seat = INDEX_NONE;
	float T = 0.f, Waited = 0.f;
	bool bTalk = false, bToldWait = false;
	static FVector Room(float X, float Y);   // (office room metres -> world, Floor 2)
	void Go(EPhase P) { Phase = P; T = 0.f; }
	bool WalkLegs(float Dt);                  // true once the last leg is reached
	bool InYourView(const FVector& At) const;
	void Knock(const FVector& At);
	void Finish(const TCHAR* Why);
};
