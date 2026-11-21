// What the passengers carry (owner 2026-11-21: "passengers waiting at the gate use their phone; some have a tablet").
// Everybody has a phone, about one in ten a tablet too. The props are the owner's own phone and tablet - the very models
// the player holds (Art/Smartphone, Art/Tablet; /Game/Babylon/Smartphone, /Game/Babylon/Tablet) - every part of them,
// their screens lit with the device screen material (/Game/Tablet/M_DeviceScreen). No new models.
// Cheap for a crowd: one instanced list per device part for ALL passengers (the phone's 17 parts, the tablet's 50: 67
// lists in all, however many people), an instance only while a device is out; in a pocket it is not drawn at all.
// HoldPhone / HoldTablet: the device comes up out of the right trouser pocket (the tablet out from the side) into the
// hand(s) over ~0.7 s and goes back the same way; the arms reach it with two-bone IK to its grip points, the head bends
// down to the screen (UPaxAnimInstance: Device). Placed after the animation each frame (TG_PostUpdateWork), so the device
// stays in the hand.
// Backpacks and earphones (the same brief) are to-do 204 - no models yet, so Dress gives none (shares kept at 0 here).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaxProps.generated.h"

class UInstancedStaticMeshComponent;
class USkeletalMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API APaxProps : public AActor
{
	GENERATED_BODY()
public:
	APaxProps();
	static APaxProps* Get(UWorld* W);
	static APaxProps* Find(const UWorld* W);

	/** A new passenger: what they carry, from Seed (always a phone; ~10% a tablet). */
	static void Dress(AActor* Passenger, USkeletalMeshComponent* Body, int32 Seed);
	/** Out of the pocket into the hands (true) or put away (false). One device at a time: taking one puts the other away. */
	static void HoldPhone(AActor* Passenger, bool bOn);
	static void HoldTablet(AActor* Passenger, bool bOn);
	/** Tests and the flow: does this passenger own a tablet; is a device out (or on its way). */
	static bool HasTablet(const AActor* Passenger);
	/** Tests: this passenger owns a tablet (as well as the phone). */
	static void GiveTablet(AActor* Passenger);
	static bool IsUsing(const AActor* Passenger);
	/** The passenger has gone (despawned): their devices go too. */
	static void Forget(AActor* Passenger);
	/** Tests: where a device is in use now, for the camera (false when none is out). */
	bool UsingPoint(const AActor* Passenger, FVector& Out) const;

	virtual void Tick(float Dt) override;


private:
	enum : int32 { KPhone = 0, KTablet = 1 };
	struct FPart { TObjectPtr<UInstancedStaticMeshComponent> List = nullptr; FTransform Rel; };
	struct FKit { TArray<FPart> Parts; int32 Slots = 0; TArray<int32> Free; bool bTried = false, bOk = false; };
	struct FWho
	{
		TWeakObjectPtr<AActor> A; TWeakObjectPtr<USkeletalMeshComponent> Body;
		bool bTablet = false;
		bool bWant[2] = {false, false};
		float K[2] = {0.f, 0.f};
		int32 Slot[2] = {INDEX_NONE, INDEX_NONE};
		mutable FVector Center = FVector::ZeroVector;   // (the device in use, world - for the tests' camera)
	};
	FKit Kits[2];
	TArray<FWho> Who;
	TMap<const AActor*, int32> ByActor;
	TArray<int32> FreeWho;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Lists;
	TSet<UInstancedStaticMeshComponent*> Dirty;
	TWeakObjectPtr<class AADPPeople> PeopleW;

	bool MakeKit(int32 Kind);
	int32 TakeSlot(int32 Kind);
	void PutSlot(int32 Kind, int32 Slot, const FTransform& At);
	void Hold(AActor* Passenger, int32 Kind, bool bOn);
	void Anim(FWho& W);
	bool DeviceAt(const FWho& W, int32 Kind, float K, FTransform& Out) const;
};
