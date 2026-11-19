// Talking to people, hiring them, and unruly passengers (owner, 2026-11-16 - docs/People-Talk-Conduct-Plan.md, to-do
// 69a-69i). The MetaHumans in the game (Content/People/Built, Data/people-roster.json: who is a man or a woman, how old,
// what they do) get the interact outline; E at one opens a menu:
//  - Just talk: generated questions, answered in each person's own way (each has a fixed character: how open, how
//    hot-tempered, how chatty); personal topics refused by the private ones, answered by the open ones
//  - Ask them to work for you: 13% yes with their own reasons, else "thanks, not looking for work"
// A passenger behaving badly gets only conduct options instead: two ways to deal with it; if they won't calm down
// (25%; 75% calm down, apologise and behave) you may call airport security. The director then plays the scene: security
// arrive from out of your sight and walk over (they start talking within 10 m, at random closer), talk with the
// passenger 20-30 s; if that fails the police come and talk; usually cuffs and out; 15% violent - the stun gun, the last
// warning, "I will use force", always a defiant reply, the stun: rigid 5 s, falls stiff, then limp; cuffed face down,
// lifted, escorted out. Every line carries its tone (for the voices later); the animations are named moments the real
// animations replace later. Until police and security have their own people, built security MetaHumans stand in for
// both (tests only - never shipped as final).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "People.generated.h"

class USkeletalMeshComponent;

UCLASS()
class AIRPORTDYNASTYPRO_API AADPPeople : public AActor
{
	GENERATED_BODY()
public:
	AADPPeople();
	static AADPPeople* Get(UWorld* W);
	/** Passengers about the terminal for now (owner, 2026-11-19: until their trips and animations are done): a group that
	 *  only walks the Grand Hall and a group that only walks Floor 2 (the Food Walk and the Landing) - each strolls from
	 *  spot to spot inside its own area, never out of it. Once, when the airport has loaded. */
	void StartWanderers();
	/** For the tests: a stroller sitting (Kind 0) or leaning (1) now - where it is and the way it faces. */
	bool StrollerDoing(int32 Kind, FVector& At, FVector& Facing) const;
	virtual void Tick(float Dt) override;

	/** How a line is said (for the voices later): calm, mild, firm, a bit aggressive, very aggressive. */
	enum class ETone : uint8 { Calm, Mild, Firm, Aggressive, VeryAggressive };

	/** A person: who they are (the roster) and their own character, fixed for them. */
	struct FWho
	{
		TWeakObjectPtr<AActor> A; USkeletalMeshComponent* Body = nullptr;
		FString Name, Role, First; bool bFemale = false; int32 Age = 30;
		float Open = 0.5f, Temper = 0.3f, Chat = 0.5f;   // (how much they share, how quick to anger, how much they say)
		bool bAskedWork = false, bWorks = false, bUnruly = false, bViolent = false;
		float Speed = 0.f;                                // (walking, m/s - the blend space's input)
	};

	/** Places a built MetaHuman (Content/People/Built/<Name>) standing at a spot, facing Yaw. */
	AActor* Spawn(const FString& Name, const FVector& At, float Yaw);
	/** The person this actor is, if one. */
	FWho* Find(AActor* A);
	/** A person by you that you face (within 2.5 m), for E when the aim missed. */
	AActor* Beside(APawn* Me);

	// ---- the menu (the HUD draws it: a title, a few lines of text, up to 8 choices) ----
	bool MenuOpen() const { return bMenu; }
	void Open(AActor* Who);
	void Close();
	void Choose(int32 I);
	void ShowStartForTest() { ShowStart(); }
	FString MenuTitle, MenuText;
	TArray<FString> MenuOptions;

	// ---- what is being said within your hearing (the HUD shows it as a subtitle) ----
	FString SubWho, SubLine; ETone SubTone = ETone::Calm; float SubT = 0.f;
	FString LastSaid;
	static const TCHAR* ToneName(ETone T);

	/** Tests (-UIConduct): makes this person an unruly passenger; forces the next outcomes (-1 random, 0 good, 1 bad). */
	void MakeUnruly(AActor* Who, bool bViolent);
	int32 ForceCalm = -1, ForceSecurity = -1, ForceViolent = -1, ForceObey = -1;
	/** The conduct scene is running (security or police on their way or there). */
	bool SceneOn() const { return Scene.Stage != EStage::None; }

private:
	TArray<FWho> People;
	/** A strolling passenger (owner, 2026-11-19): its area (boxes of the plan's rooms, world cm, on its floor's height),
	 *  its habit (on Floor 2: up and down the gate corridor, into the tax free, to the gates and their seats, a bench),
	 *  what it is doing now (walking somewhere, standing, leaning on a wall, sitting), and for how long. */
	enum class EHabit : uint8 { Hall, Corridor, Shopper, Gates, Bench };
	enum class EDoing : uint8 { Stand, Walk, Lean, ToSeat, SitDown, Sit, StandUp };
	struct FStroll
	{
		TWeakObjectPtr<AActor> A; TArray<FBox2D> Area; TArray<FString> AreaIds; float FloorZ = 0.f; EHabit Habit = EHabit::Hall;
		EDoing Doing = EDoing::Stand; FVector To = FVector::ZeroVector; float T = 0.f, For = 0.f, LeanYaw = 0.f;
		int32 Seat = INDEX_NONE; class FTruckDriverRig* Rig = nullptr; class UPoseableMeshComponent* Rider = nullptr; USceneComponent* Frame = nullptr;
		class UDriverPoseAnim* Pose = nullptr; FTransform BodyInActor; float Lower = 0.f; bool bLowered = false; int32 SitPose = 0;
	};
	TArray<FStroll> Strollers;
	bool bStrollersStarted = false;
	void StepStrollers(float Dt);
	bool StrollSpot(const FStroll& S, const FVector& From, FVector& Out, int32 Box = INDEX_NONE);
	bool WallSpot(const FStroll& S, const FVector& From, FVector& Out, float& Yaw);
	bool InArea(const FStroll& S, const FVector& P) const;
	void NextThing(FStroll& S);
	void SitPose(FStroll& S, float K);
	struct FRosterRow { FString Role; bool bFemale = false; int32 Age = 30; };
	TMap<FString, FRosterRow> Roster;
	void LoadRoster();
	void Gather();
	float GatherIn = 0.f;
	FRandomStream Rng;

	// the menu's state
	bool bMenu = false; TWeakObjectPtr<AActor> MenuWho;
	enum class EMenu : uint8 { Start, Questions, Answer, Conduct, Refused } Mode = EMenu::Start;
	TArray<int32> AskOrder;   // which questions this time
	void ShowStart(); void ShowQuestions(); void Answer(int32 Topic); void AskWork();
	void ShowConduct(); void TryCalm(bool bFirm); void CallSecurity();

	// the conduct scene (the director)
	enum class EStage : uint8 { None, SecurityComing, SecurityTalk, PoliceComing, PoliceTalk, LastWarning, Stun, Cuff, Escort, Done };
	struct FScene
	{
		EStage Stage = EStage::None; float T = 0.f, Wait = 0.f, TalkFor = 0.f, StartTalkAt = 10.f;
		TWeakObjectPtr<AActor> Pax, Guard, Cop; FVector GuardFrom, CopFrom, ExitTo;
		int32 Line = 0; bool bSpoke = false;
		// the stun: 0 rigid (5 s), 1 limp, then the officer comes after 2 s
		int32 StunPhase = 0; float FallYaw = 0.f; FRotator Stood;
	} Scene;
	void StepScene(float Dt);
	void Say(AActor* Who, const FString& Line, ETone T);
	void Moment(AActor* Who, const TCHAR* Name);         // a named animation moment (the real animations later)
	bool WalkTo(FWho* W, const FVector& To, float Dt, float Stop);   // true when there
	FVector HiddenSpot(const FVector& Near) const;        // somewhere out of your sight with a clear way to Near
	FString PickBuilt(const TCHAR* WhatRole, bool* bFemale = nullptr);
	bool Hearing(const FVector& At) const;
	void Face(AActor* A, const FVector& At);
};
