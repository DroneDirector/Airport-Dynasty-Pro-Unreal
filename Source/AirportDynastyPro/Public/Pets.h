// The pets (owner, 2026-11-15): every dog and cat model in the game, found by itself - each SK_<Name> under
// /Game/Pets/Dogs/<Name>/ or /Game/Pets/Cats/<Name>/ (made by Art/Pets/build_pet.sh, export_pet.py and
// Tools/import_pet.py) is one choice in the creator's dogs or cats section, no list to edit; its moves are
// A_<Name>_<Move> beside it (Idle, WalkFast, SitDown, SitIdle, StandUp, LieDown, LieIdle, GetUp; Run for dogs,
// RollOver for cats). Your choice is saved as "<Kind>/<Name>", so a better model under the same name is the same pet.
// APet is one pet in the world: it sits, walks to a mark and sits there, stands up and walks out. At home (owner,
// 2026-11-26) it lives in your office: on every load it lies in its bed (DB_LiePoint, Data/owner-office-points.json),
// gets up now and then, walks to its bowls, the windows, the rug, beside your chair (or to you, when you are in the
// office), sniffs, sits or lies there and goes back to its bed. Its ways are planned on the office's furniture
// footprints (Art/OwnerOffice/make_owneroffice.py) inside the room, never by the door; it waits for people in its way.
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
	/** Your pet at home: lying in its bed in your office (gone if you have none). */
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
	/** Lives in your office: lying in its bed at BedAt (world, the cushion's top), its office's plan origin at x 114, floor
	 *  height FloorZ (world cm). */
	void SetOffice(const FVector& BedAt, float FloorZ);
	const ADPPets::FModel& Model() const { return M; }
	virtual void Tick(float Dt) override;

private:
	enum class EState : uint8 { Standing, Walking, Turning, SittingDown, Sitting, StandingUp, LyingDown, Lying, GettingUp };
	void Play(const TCHAR* Move, bool bLoop, float Rate = 1.f);
	float MoveLength(const TCHAR* Move) const;
	bool Clear(const FVector& From, const FVector& To) const;
	float Ground(const FVector& At) const;
	void Arrived();
	void LieHere();
	// the office life
	enum class EEnd : uint8 { Stand, Sit, Lie };
	void OfficeTick(float Dt);
	bool OfficeGo(const FVector2D& To, EEnd End, float FaceYaw);   // plans a way (room metres) and starts it
	bool OfficeFree(const FVector2D& A, const FVector2D& B) const;   // the straight way clear of the furniture, in the room
	bool OfficePointFree(const FVector2D& P) const;
	FVector OfficeWorld(const FVector2D& P) const;
	FVector2D OfficeRoom(const FVector& W) const;
	bool PersonAhead(const FVector& At, const FVector& Dir) const;

	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY() TMap<FString, TObjectPtr<UAnimSequence>> Moves;   // every move loaded once when placed (a move loaded on first use stalled the game)
	UAnimSequence* Move(const TCHAR* Name) const;
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
	UPROPERTY() TObjectPtr<class UBoxComponent> Solid;   // people can't walk through it
	bool bLieAtEnd = false;
	// in the office (room metres: x = plan x - 114, y = plan z)
	bool bOffice = false; FVector2D Bed = FVector2D::ZeroVector; float BedZ = 0.f, FloorZ = 0.f, Clearance = 0.35f;
	TArray<FVector2D> Route;        // the way's next corners after Target
	EEnd RouteEnd = EEnd::Stand; float RouteYaw = 0.f;
	int32 Errand = 0;               // 0 in bed, 1 on its way out, 2 there, 3 on its way back to bed
	float ErrandT = 0.f, StayFor = 0.f, PeopleT = 0.f, BlockedT = 0.f; bool bBlocked = false;
};
