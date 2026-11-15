// The pets (owner, 2026-11-15): every dog and cat model in the game, found by itself - each SK_<Name> under
// /Game/Pets/Dogs/<Name>/ or /Game/Pets/Cats/<Name>/ (made by Art/Pets/build_pet.sh, export_pet.py and
// Tools/import_pet.py) is one choice in the creator's dogs or cats section, no list to edit; its moves are
// A_<Name>_<Move> beside it (Idle, WalkFast, SitDown, SitIdle, StandUp, LieDown, LieIdle, GetUp; Run for dogs,
// RollOver for cats). Your choice is saved as "<Kind>/<Name>", so a better model under the same name is the same pet.
// APet is one pet in the world: it sits, walks to a mark and sits there, stands up and walks out; at home (for now
// just outside the terminal's main entrance) it sits, and now and then walks up to 10 m away and back, looking ahead
// so it never walks into a wall.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Pets.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimSequence;

namespace ADPPets
{
	struct FModel
	{
		FString Kind;    // "dog" or "cat"
		FString Folder;  // "Dogs" or "Cats"
		FString Id;      // the model's name: "GermanShepherd"
		FString Name;    // for people: "German Shepherd"
	};
	/** Every pet model in the game, dogs then cats, each in name order. */
	const TArray<FModel>& All();
	/** One kind's model by its id; an empty or unknown id gives the kind's first; nullptr if the kind has none. */
	const FModel* Find(const FString& Kind, const FString& Id);
	/** The list for the creator's page: {"dog":[{"id":..,"name":..}],"cat":[..]}. */
	FString ListJson();
	/** Your pet as saved ("Dogs/GermanShepherd"), or empty for none. */
	FString Saved();
	void Save(const FString& KindSlashId);
	/** Your pet at home: for now sitting just outside the terminal's main entrance (gone if you have none). */
	void PlaceHome(UWorld* W);
}

UCLASS()
class AIRPORTDYNASTYPRO_API APet : public AActor
{
	GENERATED_BODY()
public:
	APet();
	static APet* Spawn(UWorld* W, const ADPPets::FModel& M, const FVector& At, float Yaw);
	/** Walk to To (turning towards it first if sitting: stands up), then: face EndYaw and sit (bSit), stand (not), or
	 *  vanish (bGone: past the stage's edge). */
	void WalkTo(const FVector& To, bool bSit, float EndYaw, bool bGone = false);
	/** Sit down where it is (or stay sitting). */
	void SitHere();
	/** At home: sits, and now and then walks a short way (at most MaxAway cm) and back, never through walls. */
	void SetHome(const FVector& Home, float Yaw, float MaxAway);
	const ADPPets::FModel& Model() const { return M; }
	virtual void Tick(float Dt) override;

private:
	enum class EState : uint8 { Standing, Walking, Turning, SittingDown, Sitting, StandingUp };
	void Play(const TCHAR* Move, bool bLoop, float Rate = 1.f);
	float MoveLength(const TCHAR* Move) const;
	bool Clear(const FVector& From, const FVector& To) const;
	float Ground(const FVector& At) const;
	void Arrived();

	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Body;
	ADPPets::FModel M;
	EState State = EState::Standing;
	float StateT = 0.f, WaitFor = 0.f;
	FVector Target = FVector::ZeroVector; float EndYaw = 0.f; bool bSitAtEnd = false, bGoneAtEnd = false;
	float Speed = 100.f, WalkRate = 0.6f, Length = 100.f;
	// at home
	bool bHome = false; FVector HomeAt = FVector::ZeroVector; float HomeYaw = 0.f, MaxAway = 1000.f;
	int32 Leg = 0;                 // 0 at home, 1 out, 2 a look round out there, 3 back
	float NextWander = 0.f;
	bool bGround = false;          // follow the ground and check for walls (the world; not the creator's stage)
};
