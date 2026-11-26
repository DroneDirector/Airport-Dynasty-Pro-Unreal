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
	/** The one way in from everywhere (the staff list on the PC, laptop, phone and tablet, a conduct report, the talk menu):
	 *  called in now, or - while they are at a job or somebody else is called in - queued until both are free. Line: what
	 *  to tell you. False only when they can't be queued (not on your staff here / not at the airport just now). */
	bool Ask(const FString& StaffId, FString& Line);
	/** "coming" (called in, on the way or in the office), "queued" or empty: for the staff list. */
	FString StateOf(const FString& StaffId) const;
private:
	enum class EPhase : uint8 { None, Spawn, In, Knock, Opening, Through, Shut, ToChair, Sitting, Seated, Up, Out, OpenOut, ThroughOut, ShutOut, Away };
	float HandW = 0.f, KnockAt = -1.f;   // (the hand on the door: its blend; when the knock's sound started, phase time)
	EPhase Phase = EPhase::None;
	TArray<FString> Queue;   // (staff ids waiting to be called in, first first; tried every 10 s while nobody is called in)
	float QueueT = 0.f;
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
	/** One frame of the door moved by the visitor's hand (bIn: its inside handle, Door_HandleIn, else the corridor's):
	 *  they stand half a metre off that face, follow the handle as the leaf swings, and the leaf only moves while the hand
	 *  is on the handle and they are where they should be. True once it is at KTo (the hand then lets go). */
	bool DoorByHand(float Dt, bool bIn, float KTo);
	/** The hand blended in or out on a place (UPaxAnimInstance::SetReach on their body). */
	void Hand(float Dt, bool bOn, int32 Side, const FVector& At, const FVector& Palm, const FVector& Fingers, float Curl, bool bKnuckles);
	FVector HandAt = FVector::ZeroVector, HandPalm = FVector::ZeroVector, HandFing = FVector::ZeroVector; int32 HandSide = 1; float HandCurl = 0.f; bool bHandKnuck = false;
	void Finish(const TCHAR* Why);
};
