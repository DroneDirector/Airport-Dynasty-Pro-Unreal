// Your hired staff are the people in the 3D airport (to-do 206, docs/Hired-Crew-Plan.md): everyone you employ in a ground
// role whose shift is on is their own MetaHuman - their name, always the same face, their role and shift - coming out of
// a ramp room's staff door at the start of it, standing by at the gates between jobs, and back in through the door when
// it ends. Part 1: the people and the pool the vehicles' crew code asks (FindFree / Claim / Release / Missing). On
// by default since part 2b (adp.HiredCrew 1); 0: nothing here does anything, the fixed ground team works as before.
// Who is hired, their shift and their face live in the simulation's person records (FADPPerson: Id, Shift, Face), so a
// saved career brings the same people back; what they are doing this moment is not kept.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "StaffPeople.generated.h"

struct FStreamableHandle;
struct FADPWorld;
class ATugs;

/** What a ground crew member can do. The simulation has one ramp role, "ground", for all of it. */
enum class EStaffDuty : uint8 { TugDriver, PushbackHandler, WingWalker, GpuHandler, TractorDriver, BaggageHandler, LoaderOperator, CleanDriver, Cleaner, FuelDriver, CaterDriver, CaterLoader, WaterDriver, StairsDriver, StairsHelper, DeicerDriver, DeicerOperator, AmbuDriver, AmbuHelper, TaxiDriver, BusDriver };
/** The jobs of a turn-round that need ground crew. */
enum class EStaffTask : uint8 { Gpu, BagsOut, BagsIn, Loader, Pushback, Bags, Relief, Chocks, Cleaning, CleanRelief, Fuel, Catering, CaterRelief, Water, Toilet, Stairs, Deice, Ambulift, PushHelp, PushDriver, Taxi, ApronBus };   // (Bags: a train and its belt loader together; Relief: one for somebody who vanished at work)

UCLASS()
class AIRPORTDYNASTYPRO_API UStaffPeople : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	static UStaffPeople* Get(const UWorld* W);
	/** adp.HiredCrew: your hired people do the ground work (off: the fixed stand-in team, as before). */
	static bool On();
	/** On, and you employ enough ground crew here for the smallest job (2): your own flights are theirs. Fewer (a new game): the
	 *  handling agent's stand-ins work them, said in the log. */
	bool Hires() const { return HiresRole(TEXT("ground")); }
	/** You employ the smallest team of a role here (2; the taxi bus: its 1 driver). */
	bool HiresRole(const TCHAR* Role) const;

	// ---- the pool
	struct FNeed { EStaffDuty Duty; int32 Min = 1, Full = 1; };   // (Min: without them the job can't start; Full: the real team)
	static void Needs(EStaffTask Task, TArray<FNeed>& Out);
	/** The simulation's role that does a duty. */
	static const TCHAR* RoleOf(EStaffDuty Duty);
	static const TCHAR* DutyName(EStaffDuty Duty);
	static const TCHAR* TaskName(EStaffTask Task);
	/** The nearest person free for a duty (their staff id; empty: nobody). */
	FString FindFree(EStaffDuty Duty, const FVector& Near) const;
	struct FTaken { EStaffDuty Duty = EStaffDuty::TugDriver; FString Id; TWeakObjectPtr<AActor> A; };
	/** The queue (part 2): asks for a task's smallest team, all of it or nobody - claimed and given in Out when it is this
	 *  job's turn (the earliest departure first) and they are free; false: it waits (ask again every couple of seconds; a job
	 *  not asked for 10 s is out of the queue). Near: where they are needed; What: "LX 123 pushback". */
	bool AskTeam(const FString& Key, EStaffTask Task, const FDateTime& When, const FVector& Near, const FString& What, TArray<FTaken>& Out);
	/** One more person for a job (a wing walker): only somebody no waiting job needs. */
	bool AskExtra(EStaffDuty Duty, const FVector& Near, const FString& What, FTaken& Out);
	/** Why a job waits now ("1 pushback handler (none on shift)"); empty: it isn't waiting. */
	FString WaitsFor(const FString& Key) const;
	/** Every job that waits for people, as the missing-staff text. */
	FString Waiting() const;
	/** The faces your ground staff wear (given now if they have none yet): the stand-ins take others. */
	void Faces(TSet<FString>& Out);
	/** Takes a person for a job: from now on the caller moves and poses them, until Release. Null: not free (any more). */
	AActor* Claim(const FString& StaffId, const FString& Job);
	/** Done (or given up): back to their place by the gates, or home if their shift is over. Called with the person outdoors
	 *  on the apron, on their feet, out of any vehicle. */
	void Release(const FString& StaffId);
	/** Still on shift (false: their shift is over - finish and go home; unknown people too). */
	bool OnShift(const FString& StaffId) const;
	int32 NumFree(EStaffDuty Duty) const;
	/** Who is missing to start a task now ("1 pushback handler (none on shift)"); empty: enough people are free. */
	FString Missing(EStaffTask Task) const;
	/** A person's name; their actor (null: not in 3D now); the person an actor is (empty: none of them). */
	FString NameOf(const FString& StaffId) const;
	AActor* ActorOf(const FString& StaffId) const;
	FString IdOf(const AActor* A) const;
	/** What they are doing and where, for the Crew screen; false: not in 3D now (off shift). */
	bool Status(const FString& StaffId, FString& Doing, FString& Where) const;
	/** Everyone hired: shift, face, place, job - into the log (adp.CrewList). */
	void Dump() const;

	virtual void Tick(float Dt) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UStaffPeople, STATGROUP_Tickables); }
	virtual void Deinitialize() override;
	/** The ramp-level door nearest a stand (its room, X0 its west end; INDEX_NONE: none), taken whole while a wheelchair goes through it. */
	int32 RampNear(float StandX, float& X0) const;
	bool RampTake(int32 R);
	void RampFree(int32 R);
	bool RampSeen(int32 R) const;
protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type T) const override { return T == EWorldType::Game || T == EWorldType::PIE; }

private:
	enum class EState : uint8 { Load, Wait, Out, Back, Ready, Busy, Leave };   // loading the body; waiting for the room; out of the door; walking back after a job; free; at a job; going home
	struct FLeg { FVector At; bool bDoor = false; };   // (bDoor: the way to this point goes through the ramp room's door)
	struct FStaff
	{
		FString Id, Name, Role, Face, Shift, Job;
		bool bCalledIn = false, bWanted = true, bInRoom = false, bOvertime = false;
		bool bLand = false;   // (the landside crew - the taxi bus driver: from the car park over the crossing to the kerb, no ramp room)
		EState State = EState::Load;
		TSharedPtr<FStreamableHandle> Handle;
		TWeakObjectPtr<AActor> A;
		TArray<FLeg> Way;
		int32 Room = INDEX_NONE, Slot = INDEX_NONE; int32 Idle = 0;   // (Idle: claimed but at no job, looked at every 2 s)
		float Speed = 0.f, BlockedT = 0.f, SideT = 0.f; FVector SideDir = FVector::ZeroVector;
	};
	struct FRoom { int32 Gate = 0; float X0 = 0.f; int8 Dir = 0; int32 Inside = 0; float NextSpawn = 0.f; bool bNoDoor = false; };   // Dir: people going out (1) / in (2) through its one door now
	TArray<FStaff> Staff;
	struct FAsk { FString Key, What, Role, Why; EStaffTask Task = EStaffTask::Gpu; int32 Need = 0; FDateTime When, Since; double Last = 0.0; };
	TArray<FAsk> Asks;                       // the jobs that wait, the earliest departure first
	void DropOldAsks();
	void CheckClaims();
	mutable TMap<FString, bool> SaidHires;
	int32 OnDuty(const FString& Role) const;
	/** Free to be given a job: standing by, or on the way back from the last one. */
	bool IsFree(const FStaff& S, const FString& Role) const;
	/** The way along the terminal (under the jetways' fixed corridors, clear of their columns) when it is far. */
	void AlongWall(FStaff& S, const FVector& To) const;
	TArray<FRoom> Rooms;
	bool bRooms = false, bSaidReuse = false, bSaidNoDoor = false;
	float Poll = 0.f, NextBody = 0.f;
	TWeakObjectPtr<ATugs> Tugs;

	void FindRooms();
	void Roster(FADPWorld& W, const FDateTime& Now);
	void GiveShift(FADPWorld& W, const FString& Id) const;
	void GiveFace(FADPWorld& W, const FString& Id);
	void StepOne(FStaff& S, float Dt);
	void Body(FStaff& S);
	void WayOut(FStaff& S) const;
	void WayIn(FStaff& S) const;
	/** One step along the way; true when it is walked. */
	bool Walk(FStaff& S, float Dt);
	void Turn(FStaff& S, float Yaw, float Dt) const;
	bool Blocked(const FVector& At) const;
	bool PlayerInRoom(const FRoom& R) const;
	bool EnterDoor(FStaff& S, int8 Dir);
	void LeaveDoor(FStaff& S);
	void Gone(FStaff& S);
	FVector SpotOf(const FStaff& S) const;
	// the landside crew (to-do 239b): arrive at the car park like the passengers, over the zebra crossing at x 48, along the
	// pavement to their place behind the taxi bus; home the same way
	FVector LandSpot(const FStaff& S) const;
	void LandWayOut(FStaff& S) const;
	void LandWayIn(FStaff& S) const;
	bool LandUnseen(const FVector& At) const;
	FVector DoorOf(const FRoom& R) const;
	const FStaff* Find(const FString& Id) const;
	FStaff* Find(const FString& Id) { return const_cast<FStaff*>(const_cast<const UStaffPeople*>(this)->Find(Id)); }
	static const TCHAR* StateName(EState S);
};
