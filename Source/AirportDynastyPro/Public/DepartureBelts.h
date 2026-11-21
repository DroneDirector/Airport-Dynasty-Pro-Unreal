// The DEPARTURE baggage system (owner 2026-11-21; blueprint Blueprints/Terminal/Departure-Baggage-v2.png): every bag
// a passenger checks in rides real belts from the desk to its flight's make-up carousel, pinned to the belt the whole
// way (never flying), and keeps its tag (passenger + flight + desk) all the way. Matches the arrivals' belts
// (BaggageBelts.h) in look and behaviour - the same plates, flaps, bags, screens and sounds.
//   A bag's way: the passenger sets it down on the desk's WEIGH + TAG belt (24 desks, each its own belt; the weight on
//   the display, the tag printed) -> through the hatch behind the desk down into the service tunnel 2 m under the hall
//   (z 18.6; the hall part is ON HOLD - owner 2026-11-21: walk-throughs to security, collectors under the floor; built
//   provisionally as that plan, see Build) -> east under the wall (x 114), rising into the baggage block through the
//   floor -> the distribution belt shares the bags between the two HOLD BAGGAGE SCREENING lines (z 7 and z 13),
//   each through a CT scanner (EU Standard 3 EDS) -> the merge belt north (x 139.5); bags the CT flagged (about 2 %)
//   are pushed off to the LEVEL 3 MANUAL SEARCH room (x 142-150) -> the SORTER loop (z 23.4 / 25.8, recirculating) ->
//   a paddle diverter pushes each bag down its flight's chute -> onto that flight's MAKE-UP CAROUSEL (centres x 124 and
//   x 145, z 33.5), where the ground crew lift the bags onto the carts (TakeBag / PutOnSlot).
//   The motors run only while bags are on their part (start-up, running loop, a stop after a while), heard as from where
//   you stand (distance, walls). Glow by material only (the CT's lamp, the weight displays) - no lights.
// Spawned by ABaggageBelts::Build() (one line there); everything instanced, the moving parts Nanite-free, the rest Nanite.
// Tests: -DepBagTest makes passengers check bags in at random desks for the departures coming up (the simulation's, or
// test flights when it isn't running), so the whole system visibly runs.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DepartureBelts.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UAudioComponent;
class UTextRenderComponent;
class USoundAttenuation;

/** A bag's tag: who it belongs to, which flight, where it was checked in. */
struct FDepBagTag
{
	FString Pax, Flight;
	int32 Desk = 0;        // 1..24 (the desk numbers on the signs)
	float Kg = 0.f;
};

/** Where hands go on a bag (world space, now): the carry handle for one hand, or both sides for a big one. */
struct FDepBagGrips
{
	FVector Handle = FVector::ZeroVector, Left = FVector::ZeroVector, Right = FVector::ZeroVector;
	bool bTwoHands = false;
};

UCLASS()
class AIRPORTDYNASTYPRO_API ADepartureBelts : public AActor
{
	GENERATED_BODY()
public:
	ADepartureBelts();
	static ADepartureBelts* Get(UWorld* W);
	void Build();
	virtual void Tick(float Dt) override;

	// ---- check-in ----
	/** A bag straight onto a desk's weigh belt (as soon as the belt is free). Desk 1..24. Returns the bag's id. */
	int32 CheckIn(const FString& Pax, const FString& Flight, int32 Desk);
	/** A passenger at a desk lifts their bag: it is in their hand (Hand + Socket, or held at their side when none) until
	 *  SetDownOnDesk - or, with bAutoSetDown, set down by itself a moment later (lowered from the hand onto the belt's
	 *  slot; the hands can follow its grips). Then the belt pins it and carries it on. Returns the bag's id. */
	int32 PassengerDrop(AActor* Passenger, int32 Desk, const FString& Pax, const FString& Flight, USceneComponent* Hand = nullptr, FName Socket = NAME_None, bool bAutoSetDown = true);
	/** The held bag is lowered onto its desk's weigh belt (waits in the hand while the belt is busy). */
	bool SetDownOnDesk(int32 Id, int32 Desk = 0);
	/** Where a passenger stands to put a bag on a desk's belt, the belt's slot and the way the passenger faces (plan north). */
	FVector DeskStandPoint(int32 Desk) const;
	FTransform DeskSlot(int32 Desk) const;

	// ---- people taking bags off and putting them on (passengers, security, the ground crew) ----
	/** The nearest bag on a belt / carousel within Reach (cm) of Near: taken off, now in the hand (it follows Hand's
	 *  Socket, or is carried off out of sight when no hand). Returns its id, or INDEX_NONE. */
	int32 TakeBag(const FVector& Near, USceneComponent* Hand = nullptr, FName Socket = NAME_None, float Reach = 150.f);
	/** A known bag taken off wherever it is (e.g. the crew picking a flight's bags off its carousel). */
	bool TakeBagById(int32 Id, USceneComponent* Hand = nullptr, FName Socket = NAME_None);
	/** A held bag put onto a make-up carousel (0 or 1) at the nearest free place to where it is now. */
	bool PutOnCarousel(int32 Id, int32 Carousel);
	/** A held bag pinned to a slot (a cart's load slot, an aircraft hold's): it follows Slot (+ Socket) with Offset. */
	bool PutOnSlot(int32 Id, USceneComponent* Slot, FName Socket = NAME_None, const FTransform& Offset = FTransform::Identity);
	/** The bag is gone from the system (loaded into the hold, handed back to its passenger...). */
	void RemoveBag(int32 Id);

	// ---- reading ----
	const FDepBagTag* Tag(int32 Id) const;
	FTransform BagTransform(int32 Id) const;
	FDepBagGrips Grips(int32 Id) const;
	/** The bags of a flight still in the system (on belts, carousels, held or on slots). */
	TArray<int32> BagsOf(const FString& Flight) const;
	/** The bags of a flight on its make-up carousel now (for the crew loading carts). */
	TArray<int32> BagsOnCarousel(const FString& Flight) const;
	/** The make-up carousel a flight's bags go to (0 / 1), assigned when its first bag comes; INDEX_NONE if none yet. */
	int32 CarouselOf(const FString& Flight) const;
	FVector CarouselCentre(int32 Carousel) const;

	/** Until the crew load carts themselves: bags of a flight about to leave are taken off its carousel by an unseen
	 *  crew (and searched bags cleared). Turn off once the handlers do it (TakeBag / PutOnSlot). */
	bool bAutoClear = true;

private:
	enum class EState : uint8 { Gone, Belt, Held, Lowering, Slot, Waiting };
	enum class EDiv : uint8 { Alternate, Reject, Carousel };
	struct FDivert { float At = 0.f; int32 To = 0; float ToS = 0.f; EDiv Kind = EDiv::Carousel; int32 Carousel = 0; int32 Arm = INDEX_NONE; bool bFlip = false; };
	struct FSeg
	{
		TArray<FVector> P;          // plan (x, belt top height, z)
		TArray<float> Cum;
		float Len = 0.f, Speed = 0.6f, Gap = 0.9f;
		int32 Zone = 0, Next = INDEX_NONE, Desk = 0, Carousel = INDEX_NONE;
		float NextS = 0.f;
		bool bClosed = false, bAlong = false;   // a loop; bags lying along the belt (weigh belts, carousels)
		bool bOval = false; FVector2f C = FVector2f::ZeroVector;   // a carousel: its analytic oval round C
		TArray<FDivert> Div;
		float CTAt = -1.f;          // the CT scanner's middle on this line (the flagging point)
	};
	struct FBag
	{
		int32 Id = 0, Kind = 0, Ix = 0, Seg = 0, Carousel = INDEX_NONE, Desk = 0;
		EState State = EState::Gone;
		float S = 0.f, Yaw = 0.f, Side = 0.f, Lap = 0.f, Wait = 0.f, T = 0.f;
		bool bRejectRoll = false, bReject = false, bScanned = false, bMoved = true;
		FDepBagTag Tag;
		TWeakObjectPtr<USceneComponent> Holder; FName Socket; FTransform Offset;
		FTransform From;           // (lowering: where it started)
		FTransform World;
	};
	struct FZone
	{
		FVector Ear;               // the sounds' place (plan)
		bool bRun = false; float Ramp = 0.f, Idle = 0.f, ClackIn = 4.f, EarIn = 0.f, Env = 1.f, EnvLp = 20000.f;
		TObjectPtr<UAudioComponent> SStart, SRun, SClack;
		FString Name;
	};
	struct FFlap { int32 Ix = 0; int32 Seg = 0; FVector Hinge; FQuat Base; FVector Axis; float Len = 0.47f, A = 0.f, V = 0.f; };
	struct FArm { int32 Ix = 0; FVector Pivot; FVector IdleDir, PushDir; float A = 0.f, Hold = 0.f; };
	struct FDesk { FVector2f At; int32 Seg = 0; TArray<FDepBagTag> Queue; TObjectPtr<UTextRenderComponent> Display; float ShownKg = -1.f; };
	struct FCarousel { FVector2f C; int32 Seg = 0; TArray<FString> Flights; TObjectPtr<UInstancedStaticMeshComponent> Plates; float Phase = 0.f; TArray<TObjectPtr<UTextRenderComponent>> Screens; FString Shown; };
	struct FDep { FString Id; FDateTime DepAt; };

	TArray<FSeg> Segs;
	TArray<FBag> Bags;
	TArray<int32> FreeSlots;
	TMap<int32, int32> IdToIx;
	TArray<TArray<int32>> SegBags;       // per seg: the bags on it (rebuilt every tick)
	TArray<FZone> Zones;
	TArray<FFlap> Flaps;
	TArray<FArm> Arms;
	TArray<FDesk> Desks;
	TArray<FCarousel> Carousels;
	TArray<FDep> Deps;
	TMap<FString, int32> FlightCarousel;
	int32 NextId = 1;
	float DepPoll = 0.f, TestIn = 3.f, ClearIn = 5.f;
	bool bTest = false;
	int32 Checked = 0, Rejected = 0, Delivered = 0, Cleared = 0;

	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> BagMeshes;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> FlapK;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ArmK;
	UPROPERTY() TArray<TObjectPtr<UObject>> Keep;
	TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Kits;
	TArray<TArray<int32>> FreeIx;

	// building
	UInstancedStaticMeshComponent* Kit(const TCHAR* Name, bool bMoving = false, bool bCollide = true);
	void Put(const TCHAR* Name, const FVector& PlanAt, const FRotator& R, const FVector& Scale = FVector(1));
	int32 AddSeg(const TArray<FVector>& Pts, int32 Zone, float Speed = 0.6f, bool bClosed = false);
	void LayConveyor(int32 Seg, float TrimStart, float TrimEnd, bool bGuardL, bool bGuardR, const TArray<FVector2f>& Gaps, float GuardFrom = -1.f, float GuardTo = -1.f, bool bLegs = true);
	void Corner(const FVector2f& At, float H, const FVector2f& In, const FVector2f& Out);
	void Curtain(const FVector& PlanHinge, const FVector& PlanDir, float Width, float Len, int32 Seg, int32 Strips = 8);
	int32 AddArm(int32 Seg, float At, const FVector2f& PushDir);
	FZone& AddZone(const FString& Name, const FVector& PlanEar);

	// running
	FVector SegPos(const FSeg& G, float S, FVector& Dir) const;     // plan position + world direction
	float Wrap(const FSeg& G, float D) const;
	bool Free(int32 Seg, float S, float Gap, int32 Except, bool bBothSides) const;
	FTransform PlaceOnSeg(const FBag& B) const;
	FTransform PlateAt(const FCarousel& K, float S) const;
	int32 NewBag(const FDepBagTag& Tag);
	void HideBag(FBag& B);
	void Enter(FBag& B, int32 Seg, float S);
	int32 AssignCarousel(const FString& Flight);
	void PollDepartures();
	void TestTick(float Dt);
	void AutoClear(float Dt);
	void Sounds(FZone& Z, float Dt);
	void SetZone(int32 Z, bool bOn);
	FBag* Find(int32 Id);
	const FBag* Find(int32 Id) const;
	static FVector W(float X, float H, float Z);
	static FVector WDir(const FVector& PlanDir);
	static FRotator RotAlong(const FVector& WorldDir);              // a part's Blender +Y along this world direction, up kept up
};
