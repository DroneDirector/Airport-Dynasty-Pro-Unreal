// The short conversations between people (NpcChat.h).
#include "NpcChat.h"
#include "ADPLog.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "People.h"
#include "PerfLog.h"
#include "TalkLook.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	constexpr int32 ChatMost = 3;            // (never more at once: the voices are not flooded)
	constexpr float ChatNear = 250.f;        // (cm between the two of them to begin)
	constexpr float ChatPart = 400.f;        // (further apart than this and it is over)
	constexpr float ChatLookIn = 3.f;        // (how often a new pair is looked for, s)
	constexpr int32 ChatSeen = 40;           // (how many people near you are weighed at a time)

	/** Who is talking: two passengers, a passenger and a member of staff (the passenger starts), two of the staff. */
	enum class EChatKind : uint8 { PaxPax = 0, PaxStaff = 1, StaffStaff = 2 };

	/** A conversation as it is said, line by line, the two of them taking turns (the first line is the first one's). */
	struct FChatScript { EChatKind Kind; int32 Num; const TCHAR* L[6]; };

	// Everyday small talk only - nothing about you, nothing a person could not know.
	const FChatScript ChatScripts[] = {
		// passenger <-> passenger
		{EChatKind::PaxPax, 2, {TEXT("Have you looked at the board? Ours is still on time."), TEXT("Good. I hate running for a gate.")}},
		{EChatKind::PaxPax, 4, {TEXT("The queue at security was long this morning."), TEXT("Twenty minutes for me."), TEXT("At least it keeps moving."), TEXT("It usually does here.")}},
		{EChatKind::PaxPax, 4, {TEXT("Is there anywhere decent for a coffee up here?"), TEXT("On the floor above, by the windows."), TEXT("Thank you, I have an hour to fill."), TEXT("So have I. Better than sitting at the gate.")}},
		{EChatKind::PaxPax, 4, {TEXT("The mountains looked wonderful on the way in."), TEXT("Snow the whole way down the valley."), TEXT("It is my first time here."), TEXT("You picked a good week for it.")}},
		{EChatKind::PaxPax, 3, {TEXT("Did you check a bag in?"), TEXT("Only hand luggage. Never again with a big suitcase."), TEXT("Very sensible.")}},
		{EChatKind::PaxPax, 3, {TEXT("My son is meeting me at the other end."), TEXT("That is a nice landing, then."), TEXT("It is. I have not seen him since the spring.")}},
		{EChatKind::PaxPax, 3, {TEXT("It is warm in here."), TEXT("They always have the heating too high."), TEXT("I will take my coat off at the gate.")}},
		{EChatKind::PaxPax, 3, {TEXT("Any idea which gate it is?"), TEXT("They have not put it up yet."), TEXT("Half an hour to go and still nothing.")}},
		{EChatKind::PaxPax, 2, {TEXT("Shall we sit down over there?"), TEXT("Yes, my feet have had enough.")}},
		{EChatKind::PaxPax, 5, {TEXT("Were you here for work?"), TEXT("Three days of meetings, and I saw nothing but a hotel."), TEXT("That is a shame, it is beautiful outside."), TEXT("Next time I will stay the weekend."), TEXT("You should.")}},
		// passenger <-> staff (the passenger speaks first)
		{EChatKind::PaxStaff, 4, {TEXT("Excuse me, is the gate down this way?"), TEXT("Straight on past the shops, then to your left."), TEXT("Thank you very much."), TEXT("Enjoy your flight.")}},
		{EChatKind::PaxStaff, 3, {TEXT("Do you know if the flight is delayed?"), TEXT("Nothing on my screen, so it should go on time."), TEXT("That is a relief.")}},
		{EChatKind::PaxStaff, 3, {TEXT("Where can I fill a water bottle?"), TEXT("By the toilets, there is a tap on the wall."), TEXT("Perfect, thank you.")}},
		{EChatKind::PaxStaff, 4, {TEXT("Busy day for you?"), TEXT("Three departures in the next hour, so yes."), TEXT("I will keep out of your way, then."), TEXT("You are no trouble at all.")}},
		{EChatKind::PaxStaff, 2, {TEXT("Is it always this cold out on the apron?"), TEXT("In November, yes. You get used to it.")}},
		// staff <-> staff
		{EChatKind::StaffStaff, 3, {TEXT("How is your shift going?"), TEXT("Quiet so far. The afternoon will be worse."), TEXT("It always is.")}},
		{EChatKind::StaffStaff, 3, {TEXT("Did the catering truck turn up?"), TEXT("Ten minutes late, but it is loading now."), TEXT("Good, we still have time.")}},
		{EChatKind::StaffStaff, 3, {TEXT("Coffee when you are finished?"), TEXT("If the turn-round holds, yes."), TEXT("I will keep one warm for you.")}},
		{EChatKind::StaffStaff, 4, {TEXT("The wind is picking up out there."), TEXT("They said it would by three."), TEXT("I will get the cones in."), TEXT("Thanks.")}},
		{EChatKind::StaffStaff, 2, {TEXT("Is the stand clear for the next one?"), TEXT("Chocks and cones are away already.")}},
		{EChatKind::StaffStaff, 3, {TEXT("Who is on the late shift with you?"), TEXT("Nobody yet. They are still working it out."), TEXT("Good luck with that.")}},
	};

	/** A conversation going on: the two of them, which lines, how far it has got. */
	struct FChatNow
	{
		TWeakObjectPtr<AActor> Who[2];
		TWeakObjectPtr<USkeletalMeshComponent> Body[2];
		const FChatScript* Script = nullptr;
		int32 At = 0;          // the next line
		int32 Said = -1;       // who said the last one (0 / 1)
		float Wait = 0.f;      // until the next line: the one talking has to finish first
		int32 Seated[2] = {0, 0};
	};
	TArray<FChatNow> GChats;
	float GLookIn = 5.f;       // (nothing in the first seconds of the airport)
	FRandomStream GRng(0x6ea7);

	bool InAChat(const AActor* A)
	{
		for (const FChatNow& C : GChats)
			if (C.Who[0].Get() == A || C.Who[1].Get() == A) return true;
		return false;
	}

	/** The way a person faces (a MetaHuman faces along its actor's +Y). */
	FVector FacingOf(const AActor* A)
	{
		FVector F = A->GetActorRightVector(); F.Z = 0.f;
		return F.GetSafeNormal();
	}

	/** How long a line takes to say: the same length as its subtitle (AADPPeople::Say), and a breath after it. */
	float LineTime(const FString& Line)
	{
		return FMath::Clamp(1.5f + Line.Len() * 0.06f, 2.5f, 7.f) + GRng.FRandRange(0.4f, 0.9f);
	}

	/** Turns a person towards the other one, never a snap - a seated one keeps their seat and only turns the head. */
	void TurnToOther(AADPPeople* P, AActor* A, const AActor* At, float Dt, bool bSeated)
	{
		if (!P || !A || !At || bSeated) return;
		FVector Want = At->GetActorLocation() - A->GetActorLocation(); Want.Z = 0.f;
		if (!Want.Normalize()) return;
		const FVector Now = FacingOf(A);
		const FVector D = Now.IsNearlyZero() ? Want : FMath::VInterpNormalRotationTo(Now, Want, Dt, 150.f);
		P->TurnTo(A, A->GetActorLocation() + D * 200.f);
	}

	void EndChat(int32 I, const TCHAR* Why)
	{
		if (!GChats.IsValidIndex(I)) return;
		FChatNow& C = GChats[I];
		for (int32 K = 0; K < 2; ++K) if (AActor* A = C.Who[K].Get()) ADPTalkLook::Unpair(A);
		ADP_LOG(TEXT("NpcChat: a conversation ends (%s), %d still going"), Why, GChats.Num() - 1);
		GChats.RemoveAt(I);
	}

	/** Both of them still there, still free, still together and still within your hearing. */
	bool ChatHolds(AADPPeople* P, FChatNow& C)
	{
		AActor* A = C.Who[0].Get(); AActor* B = C.Who[1].Get();
		if (!A || !B || !C.Body[0].IsValid() || !C.Body[1].IsValid()) return false;
		for (int32 K = 0; K < 2; ++K)
		{
			const int32 St = P->ChatState(C.Who[K].Get());
			if (St == 0) return false;                 // (walked off, sat down, taken by a job, or you are talking to them)
			C.Seated[K] = St == 2 ? 1 : 0;
		}
		const FVector Mid = (A->GetActorLocation() + B->GetActorLocation()) * 0.5f;
		if (FVector::Dist2D(A->GetActorLocation(), B->GetActorLocation()) > ChatPart) return false;
		if (!P->CanHear(Mid)) return false;            // (out of earshot: no voice is spent on it)
		return true;
	}

	/** A pair that could talk: on their feet or seated, close, roughly facing, on the same floor, both free. */
	void FindPair(AADPPeople* P)
	{
		const TArray<AADPPeople::FWho>& All = P->Everyone();
		struct FCand { AActor* A; USkeletalMeshComponent* Body; FVector At; bool bStaff; int32 Seated; };
		TArray<FCand, TInlineAllocator<ChatSeen>> Cand;
		const int32 N = All.Num();
		if (N < 2) return;
		const int32 From = GRng.RandRange(0, N - 1);
		for (int32 K = 0; K < N && Cand.Num() < ChatSeen; ++K)
		{
			const AADPPeople::FWho& W = All[(From + K) % N];
			AActor* A = W.A.Get();
			if (!A || !W.Body || W.bUnruly || InAChat(A)) continue;
			const FVector At = A->GetActorLocation();
			if (!P->CanHear(At)) continue;             // (only what you could hear)
			const int32 St = P->ChatState(A);
			if (St == 0) continue;
			Cand.Add({A, W.Body, At, W.Role != TEXT("passenger"), St == 2 ? 1 : 0});
		}
		for (int32 I = 0; I < Cand.Num(); ++I)
			for (int32 J = I + 1; J < Cand.Num(); ++J)
			{
				const FCand& X = Cand[I]; const FCand& Y = Cand[J];
				if (FMath::Abs(X.At.Z - Y.At.Z) > 120.f) continue;                 // (the same floor)
				const float D = FVector::Dist2D(X.At, Y.At);
				if (D > ChatNear || D < 55.f) continue;
				// roughly facing: whoever is on their feet has the other one in front of them (a seated one turns the head)
				FVector XY = Y.At - X.At; XY.Z = 0.f; XY.Normalize();
				if (!X.Seated && FVector::DotProduct(FacingOf(X.A), XY) < 0.f) continue;
				if (!Y.Seated && FVector::DotProduct(FacingOf(Y.A), -XY) < 0.f) continue;
				// who is talking, and a conversation of that kind
				const EChatKind Kind = X.bStaff && Y.bStaff ? EChatKind::StaffStaff : (X.bStaff != Y.bStaff ? EChatKind::PaxStaff : EChatKind::PaxPax);
				TArray<const FChatScript*, TInlineAllocator<24>> Fits;
				for (const FChatScript& S : ChatScripts) if (S.Kind == Kind) Fits.Add(&S);
				if (!Fits.Num()) continue;
				const FChatScript* S = Fits[GRng.RandRange(0, Fits.Num() - 1)];
				// a passenger starts the one with a member of staff (they are the one with the question)
				const bool bSwap = Kind == EChatKind::PaxStaff ? X.bStaff : GRng.FRand() < 0.5f;
				const FCand& A1 = bSwap ? Y : X; const FCand& A2 = bSwap ? X : Y;
				FChatNow C;
				C.Who[0] = A1.A; C.Who[1] = A2.A; C.Body[0] = A1.Body; C.Body[1] = A2.Body;
				C.Seated[0] = A1.Seated; C.Seated[1] = A2.Seated;
				C.Script = S; C.At = 0; C.Said = -1; C.Wait = GRng.FRandRange(0.2f, 0.8f);
				GChats.Add(C);
				ADPTalkLook::Pair(A1.A, A1.Body, A2.A, A2.Body);
				ADPTalkLook::Pair(A2.A, A2.Body, A1.A, A1.Body);
				ADP_LOG(TEXT("NpcChat: two people start talking (kind %d, %d lines), %d going now"), (int32)Kind, S->Num, GChats.Num());
				return;
			}
	}
}

namespace ADPNpcChat
{
int32 Running() { return GChats.Num(); }

bool Busy(const AActor* Who) { return Who && InAChat(Who); }

void Drop()
{
	for (const FChatNow& C : GChats)
		for (int32 K = 0; K < 2; ++K) if (const AActor* A = C.Who[K].Get()) ADPTalkLook::Unpair(A);
	GChats.Reset();
	GLookIn = 5.f;
}

void Tick(UWorld* W, float Dt)
{
	AADPPeople* P = W ? AADPPeople::Get(W) : nullptr;
	if (!P) return;
	// the conversations going on (a handful at most, no scanning here)
	for (int32 I = GChats.Num() - 1; I >= 0; --I)
	{
		FChatNow& C = GChats[I];
		if (!ChatHolds(P, C)) { EndChat(I, TEXT("one of them left, or you are out of earshot")); continue; }
		AActor* A = C.Who[0].Get(); AActor* B = C.Who[1].Get();
		TurnToOther(P, A, B, Dt, C.Seated[0] != 0);
		TurnToOther(P, B, A, Dt, C.Seated[1] != 0);
		if (P->MenuOpen() || P->SceneOn()) continue;   // (your own talk keeps the subtitle to itself; they stay as they are)
		if ((C.Wait -= Dt) > 0.f) continue;            // (the one talking finishes before the other answers)
		if (!C.Script || C.At >= C.Script->Num) { EndChat(I, TEXT("they said what they had to say")); continue; }
		const int32 Turn = C.At % 2;                   // (they take turns, never both at once)
		const FString Line = C.Script->L[C.At];
		P->SayAloud(C.Who[Turn].Get(), Line, AADPPeople::ETone::Calm);
		C.Said = Turn; ++C.At;
		C.Wait = LineTime(Line);
	}
	// now and then: a new pair, if there is room for one
	if ((GLookIn -= Dt) > 0.f) return;
	GLookIn = ChatLookIn;
	if (GChats.Num() >= ChatMost || P->MenuOpen() || P->SceneOn()) return;
	ADP_PERF_SCOPE("npc chat: looking for two people to talk (every 3 s)", 2.f);
	FindPair(P);
}
}
