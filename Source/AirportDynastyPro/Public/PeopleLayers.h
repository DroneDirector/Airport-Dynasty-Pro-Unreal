// The people's layers (to-do 205, 140; owner 2026-11-25): up close the whole MetaHuman (body, face, clothes, hair), from about
// 20 m on one light mesh made from them (Content/People/Layers/<Name>, Tools/make_people_layers.py), following the same body's
// pose. The graphics card's own drawing distances make the change, so nothing is built or let go as they come and go; the whole
// person's parts are made one a frame while they are still further off, and let go again when they are far away. People made
// by the game's flow come with the light mesh and never all at once (a whole MetaHuman in one frame took 15-28 ms).
#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

class AActor;
class UActorComponent;
class USkeletalMeshComponent;

namespace ADPLayers
{
	/** What is to be in memory before a person is made: the person and their light mesh. */
	TArray<FSoftObjectPath> Paths(const FString& Name);
	/** While it lives, the people of this class are made with their body alone (their parts are left for Tick). Only when
	 *  their light mesh is in memory. */
	struct FBodyOnly
	{
		FBodyOnly(UClass* C, const FString& Name);
		~FBodyOnly();
	private:
		TArray<TWeakObjectPtr<UActorComponent>> Off;
	};
	/** A person there is (AADPPeople): their light mesh on, the rest seen to from now on. */
	void Add(AActor* A, USkeletalMeshComponent* Body, const FString& Name);
	/** Each frame. */
	void Tick(UWorld* W, float Dt);
	/** Every part of this person there now (close-ups and the like). */
	void Whole(AActor* A);
	/** For the tests: how many people are whole, how many light, and how many have no light mesh. */
	FString Counts();
	/** Is this a light mesh (not one of the person's own parts)? */
	bool IsLight(const UActorComponent* C);
}
