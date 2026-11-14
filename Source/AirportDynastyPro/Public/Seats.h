// Sitting down (owner, 2026-11-14: "the option to be able to sit at any chair at the airport"): every chair, stool,
// armchair, bench and gate seat in the terminal, the offices, the ops room and the lounge is a place to sit
// (ABabylonModel::SeatSpots finds them: the seat's middle, its height, the way it faces - away from its back). E / X by
// one sits you down - your own body lowering onto it, feet on the floor, hands on your thighs (the same posing as the
// fire truck's seat, TruckDriverRig.h) - and you can look about; E / X again and you stand up the same way back.
// Each seat is outlined when you're near (InteractOutline.h).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BabylonModel.h"
#include "TruckDriverRig.h"
#include "Seats.generated.h"

class AADPWalker;
class UPoseableMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API ASeats : public AActor
{
	GENERATED_BODY()
public:
	ASeats();
	virtual void Tick(float Dt) override;
	/** E / X: sit on the nearest seat within reach, or stand up when sitting. True when it did either. */
	static bool Interact(APawn* Who);
	/** Sitting (or sitting down / getting up) now. */
	static bool IsSitting();
	/** The nearest seat within reach of At (for the Buttons panel and tests). */
	static bool NearSeat(const FVector& At);
	/** Tests: the seats found, and a spot to stand by seat I facing it. */
	static int32 Count();
	static bool StandSpot(int32 I, FVector& At, float& Yaw);
	static int32 NearestTo(const FVector& At);
	/** Someone else sitting (owner, 2026-11-14: nobody sits where someone already is): Who takes the seat nearest At
	 *  (within 1.5 m; INDEX_NONE when none is free), and is asked to get up by AskToLeave when you want your own chair
	 *  (your office's, the ops room's) - you sit once they've released it. */
	static int32 Claim(AActor* Who, const FVector& At, TFunction<void()> AskToLeave = nullptr);
	static void Release(AActor* Who);
	static bool IsTaken(int32 Seat);

	static constexpr float Reach = 130.f;
	static constexpr float SitTime = 0.9f;

private:
	static TWeakObjectPtr<ASeats> One;
	TArray<ABabylonModel::FSeatSpot> Spots;
	float GatherIn = 2.f;
	int32 GatherRounds = 0;
	TSet<TPair<UInstancedStaticMeshComponent*, int32>> Outlined;
	void Gather();

	void Sit(AADPWalker* Me, int32 I);
	void Stand();
	TWeakObjectPtr<AADPWalker> Sitter;
	int32 Seat = INDEX_NONE;
	enum class EState : uint8 { None, Down, Sat, Up } State = EState::None;
	float T = 0.f;
	FVector Before = FVector::ZeroVector;   // where you stood before sitting
	// who sits where (kept by the seat's place, so finding the seats again keeps them)
	struct FTaken { TWeakObjectPtr<AActor> Who; TFunction<void()> AskToLeave; };
	TMap<FIntVector, FTaken> Taken;
	static FIntVector SpotKey(const FVector& At) { return FIntVector(FMath::RoundToInt(At.X / 10.f), FMath::RoundToInt(At.Y / 10.f), FMath::RoundToInt(At.Z / 10.f)); }
	TWeakObjectPtr<AADPWalker> Waiting; int32 WaitSeat = INDEX_NONE; float WaitT = 0.f;   // waiting for your own chair to be free
	FDriverPose Standing, Sitting;
	FTruckDriverRig Rig;
	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Rider;
	UPROPERTY() TObjectPtr<USceneComponent> Frame;
	class UDriverPoseAnim* Pose = nullptr;
	void Place(float K);
};
