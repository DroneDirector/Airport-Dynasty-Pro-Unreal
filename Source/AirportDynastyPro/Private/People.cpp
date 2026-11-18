#include "People.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "InteractOutline.h"
#include "GroomComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ADPPeopleInternal
{
	using ETone = AADPPeople::ETone;
	struct FL { const TCHAR* Text; ETone Tone; };

	// first names (made up at random per person, by their own seed)
	const TCHAR* Women[] = {TEXT("Anna"), TEXT("Maria"), TEXT("Sara"), TEXT("Elin"), TEXT("Laura"), TEXT("Nina"), TEXT("Hanna"), TEXT("Julia"), TEXT("Sofia"), TEXT("Emma"), TEXT("Ragna"), TEXT("Lisa"), TEXT("Clara"), TEXT("Ida"), TEXT("Eva"), TEXT("Marta")};
	const TCHAR* Men[] = {TEXT("Jon"), TEXT("Peter"), TEXT("David"), TEXT("Tomas"), TEXT("Erik"), TEXT("Simon"), TEXT("Martin"), TEXT("Jakob"), TEXT("Olaf"), TEXT("Daniel"), TEXT("Rúni"), TEXT("Michael"), TEXT("Andreas"), TEXT("Paul"), TEXT("Leo"), TEXT("Hans")};

	// the questions (bPersonal: refused by the private ones); passengers and staff have their own
	struct FQ { const TCHAR* Pax; const TCHAR* Staff; bool bPersonal; };
	const FQ Questions[] = {
		{TEXT("How are you today?"), TEXT("How are you today?"), false},
		{TEXT("Where are you travelling to?"), TEXT("How is work going today?"), false},
		{TEXT("What do you think of the airport?"), TEXT("What's it like working here?"), false},
		{TEXT("Have you been waiting long?"), TEXT("How long is your shift today?"), false},
		{TEXT("What do you do for a living?"), TEXT("How long have you worked here?"), true},
		{TEXT("Are you travelling with family?"), TEXT("Do you have a family at home?"), true},
		{TEXT("Where are you from?"), TEXT("Where are you from?"), true},
		{TEXT("Is there anything we could do better here?"), TEXT("Is there anything that would make your job easier?"), false},
	};
	// answers per question: [0] friendly and short, [1] friendly and chatty, [2] grumpy
	const TCHAR* PaxAns[][3] = {
		{TEXT("Good, thanks."), TEXT("Oh, quite well, thank you for asking - a bit tired, but looking forward to the trip."), TEXT("I've been better.")},
		{TEXT("Copenhagen."), TEXT("Down to Copenhagen, and on to Barcelona after that - a week in the sun, I hope!"), TEXT("Somewhere warmer than here, I hope.")},
		{TEXT("It's nice. Clean."), TEXT("It's lovely, really - light and calm, and the view over the mountains is something else."), TEXT("It'll do. Bit pricey in the shops.")},
		{TEXT("Not too long."), TEXT("About an hour, but I came early on purpose - I like to sit and watch the planes."), TEXT("Long enough.")},
		{TEXT("I'm a teacher."), TEXT("I work in fishing exports - lots of travel, lots of meetings, never enough sleep."), TEXT("Office work. Nothing exciting.")},
		{TEXT("With my partner."), TEXT("With my kids and my mother - she's never flown before, so it's a big day."), TEXT("Alone, thankfully.")},
		{TEXT("From Tórshavn."), TEXT("I grew up in a little village up north, but I've lived in town for years now."), TEXT("Around here.")},
		{TEXT("More seats by the gates."), TEXT("Maybe more places to charge a phone - and a few more seats by the gates when it's busy."), TEXT("Cheaper coffee, for a start.")},
	};
	const TCHAR* StaffAns[][3] = {
		{TEXT("Good, thanks!"), TEXT("Pretty good, thanks - busy morning, but a good team on today."), TEXT("Tired. Long day.")},
		{TEXT("Busy, but fine."), TEXT("Busy! Three departures in a row, but everything's on time so far, touch wood."), TEXT("Don't ask.")},
		{TEXT("I like it."), TEXT("Honestly, it's a good place - people look out for each other, and no two days are the same."), TEXT("It's a job.")},
		{TEXT("Eight hours."), TEXT("Twelve today, I'm covering for a colleague - but I get the next two days off."), TEXT("Too long.")},
		{TEXT("Five years."), TEXT("Almost eight years now - I started when the old terminal was still here."), TEXT("Long enough to know better.")},
		{TEXT("Yes, two kids."), TEXT("My partner and two little ones - they love coming to watch the planes."), TEXT("That's my business.")},
		{TEXT("From Klaksvík."), TEXT("From Klaksvík originally, but I moved here for this job."), TEXT("Around here.")},
		{TEXT("More staff at peak times."), TEXT("A few more hands when the morning flights all go at once - that would help a lot."), TEXT("Better pay.")},
	};
	const TCHAR* Refusals[] = {TEXT("I'd rather not talk about that, sorry."), TEXT("That's a bit personal - I'll keep that to myself."), TEXT("Let's just say... no comment."), TEXT("I don't really want to talk about it.")};
	const TCHAR* HireNo[] = {TEXT("Thanks, that's kind - but I'm not looking for work."), TEXT("Oh, thank you, but I'm happy where I am."), TEXT("Thanks, but no - I've got a job already."), TEXT("That's nice of you to ask, but not right now.")};
	const TCHAR* HireYes[] = {
		TEXT("Yes, I would love to work for you - I've always wanted to work at an airport."),
		TEXT("Really? Yes! My contract ends next month and I've been looking for something new."),
		TEXT("Yes, I'd love that - I want to stay on the islands, and good jobs are hard to find here."),
		TEXT("I would! I've heard good things about how you run this place."),
		TEXT("Yes please - I'm good with people, and I'm tired of working nights.")};

	// unruly passengers: what they're doing, and the lines (with you: a bit aggressive; with security/police: far more)
	const TCHAR* Doing[] = {TEXT("shouting at the gate staff about a delay"), TEXT("refusing to put their bag through security"), TEXT("being loud and rude to people in the queue"), TEXT("arguing at the desk about a baggage fee")};
	const FL PaxAtYou[] = {{TEXT("Who are you to tell me anything?"), ETone::Aggressive}, {TEXT("This is ridiculous! Nobody here knows what they're doing!"), ETone::Aggressive}, {TEXT("Don't tell me to calm down!"), ETone::Aggressive}, {TEXT("I've been waiting for hours! Hours!"), ETone::Aggressive}};
	const FL Calmed[] = {{TEXT("...You're right. I'm sorry - I lost my temper. It won't happen again."), ETone::Mild}, {TEXT("Sorry. It's been a long day. I'll behave, I promise."), ETone::Mild}, {TEXT("I apologise. I shouldn't have shouted at anyone."), ETone::Calm}};
	const FL Refused[] = {{TEXT("No! I'm not calming down until somebody fixes this!"), ETone::Aggressive}, {TEXT("Leave me alone! I know my rights!"), ETone::Aggressive}, {TEXT("Get out of my face!"), ETone::Aggressive}};
	const FL GuardSays[] = {{TEXT("Sir, madam - airport security. I need you to lower your voice."), ETone::Firm}, {TEXT("Let's step over here and talk about it calmly."), ETone::Calm}, {TEXT("You can't behave like this in the terminal. Last chance to settle down."), ETone::Firm}, {TEXT("I hear you. But shouting won't get you on that plane."), ETone::Calm}};
	const FL PaxAtGuard[] = {{TEXT("Don't touch me! I haven't done anything!"), ETone::VeryAggressive}, {TEXT("Call whoever you want, I'm not moving!"), ETone::VeryAggressive}, {TEXT("You people are useless! Useless!"), ETone::VeryAggressive}, {TEXT("Back off! I mean it!"), ETone::VeryAggressive}};
	const FL PaxAtGuardMild[] = {{TEXT("I'm just upset, alright? Nobody's telling us anything."), ETone::Firm}, {TEXT("Fine, fine... but this isn't fair."), ETone::Mild}};
	const FL GuardWins[] = {{TEXT("Okay... okay. I'm sorry. I'll sit down."), ETone::Mild}};
	const FL GuardCalls[] = {{TEXT("Control, this is security at the gate - we need police assistance."), ETone::Firm}};
	const FL CopSays[] = {{TEXT("Police. What seems to be the problem here?"), ETone::Firm}, {TEXT("You need to calm down, right now."), ETone::Firm}, {TEXT("If you don't cooperate, you're leaving this airport with us."), ETone::Firm}, {TEXT("Turn around and put your hands behind your back."), ETone::Firm}};
	const FL PaxAtCop[] = {{TEXT("I'm not going anywhere!"), ETone::VeryAggressive}, {TEXT("This is harassment! I'm filming this!"), ETone::VeryAggressive}, {TEXT("You can't arrest me for this!"), ETone::VeryAggressive}};
	const FL PaxGivesIn[] = {{TEXT("Alright, alright... I'm done."), ETone::Mild}, {TEXT("Fine. Take me. Whatever."), ETone::Firm}};
	const FL CopWarn[] = {{TEXT("Taser! This is your last warning - get on the ground, NOW!"), ETone::Firm}};
	const FL PaxObeysWarn[] = {{TEXT("Okay! Okay! I'm getting down!"), ETone::Mild}};
	const FL PaxRefusesWarn[] = {{TEXT("No! I'm not getting on any floor!"), ETone::VeryAggressive}, {TEXT("Make me!"), ETone::VeryAggressive}};
	const FL CopForce[] = {{TEXT("I will use force!"), ETone::Firm}};
	const FL Defiant[] = {{TEXT("And so what!!!"), ETone::VeryAggressive}, {TEXT("Go ahead then!"), ETone::VeryAggressive}, {TEXT("You wouldn't dare!"), ETone::VeryAggressive}, {TEXT("Do it then!"), ETone::VeryAggressive}, {TEXT("So what?!"), ETone::VeryAggressive}};
	const FL CopCuff[] = {{TEXT("Stay down. Hands behind your back."), ETone::Firm}, {TEXT("You're under arrest. Let's go."), ETone::Firm}};

	const TCHAR* GLast = nullptr;
	template<int N> const FL& Pick(const FL (&A)[N], FRandomStream& R)
	{
		// (not the line just said, when there is another)
		int32 i = R.RandRange(0, N - 1);
		for (int32 k = 0; k < 4 && N > 1 && A[i].Text == GLast; ++k) i = R.RandRange(0, N - 1);
		GLast = A[i].Text; return A[i];
	}
	template<int N> const TCHAR* PickT(const TCHAR* (&A)[N], FRandomStream& R) { return A[R.RandRange(0, N - 1)]; }
	uint32 Seed(const FString& S) { return GetTypeHash(S) * 2654435761u; }
}
using namespace ADPPeopleInternal;

AADPPeople::AADPPeople()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Rng.Initialize(FPlatformTime::Cycles());
}

AADPPeople* AADPPeople::Get(UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<AADPPeople> It(W); It; ++It) return *It;
	return W->SpawnActor<AADPPeople>();
}

const TCHAR* AADPPeople::ToneName(ETone T)
{
	switch (T) { case ETone::Calm: return TEXT("calm"); case ETone::Mild: return TEXT("mild"); case ETone::Firm: return TEXT("firm"); case ETone::Aggressive: return TEXT("a bit aggressive"); default: return TEXT("very aggressive"); }
}

void AADPPeople::LoadRoster()
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/people-roster.json")))) { ADP_WARN(TEXT("People: no Data/people-roster.json (Tools/make_people_roster.py)")); return; }
	TArray<TSharedPtr<FJsonValue>> Rows; const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(R, Rows)) return;
	for (const TSharedPtr<FJsonValue>& V : Rows)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject(); if (!O) continue;
		FRosterRow Row; Row.Role = O->GetStringField(TEXT("role")); Row.bFemale = O->GetBoolField(TEXT("female")); Row.Age = (int32)O->GetNumberField(TEXT("age"));
		Roster.Add(O->GetStringField(TEXT("name")), Row);
	}
	ADP_LOG(TEXT("People: roster of %d"), Roster.Num());
}

void AADPPeople::Gather()
{
	// every MetaHuman in the world (a built person: /Game/People/Built/<Name>/BP_<Name>) that isn't you: listed once,
	// their character made from their own name, their outline on
	if (!Roster.Num()) LoadRoster();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AActor* MeHuman = nullptr;
	if (PC && PC->GetPawn()) for (AActor* Ch : PC->GetPawn()->Children) if (Ch && Ch->GetClass()->GetPathName().Contains(TEXT("/Game/People/Built/"))) MeHuman = Ch;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!A || A == MeHuman || (PC && A->GetOwner() == PC->GetPawn())) continue;
		const FString Path = A->GetClass()->GetPathName();
		if (!Path.Contains(TEXT("/Game/People/Built/MH_"))) continue;
		if (Find(A)) continue;
		FWho W; W.A = A;
		W.Name = Path.Mid(Path.Find(TEXT("/Built/")) + 7); W.Name = W.Name.Left(W.Name.Find(TEXT("/")));
		const FRosterRow* Row = Roster.Find(W.Name);
		W.Role = Row ? Row->Role : TEXT("passenger"); W.bFemale = Row && Row->bFemale; W.Age = Row ? Row->Age : 35;
		FRandomStream S(Seed(W.Name));
		W.Open = S.FRand(); W.Temper = S.FRand(); W.Chat = S.FRand();
		W.First = W.bFemale ? Women[S.RandRange(0, UE_ARRAY_COUNT(Women) - 1)] : Men[S.RandRange(0, UE_ARRAY_COUNT(Men) - 1)];
		TArray<USkeletalMeshComponent*> Skels; A->GetComponents<USkeletalMeshComponent>(Skels);
		for (USkeletalMeshComponent* Sk : Skels)
		{
			if (Sk->GetName().StartsWith(TEXT("Body"))) W.Body = Sk;
			// (E finds them by looking at them: their meshes answer the look; the outline draws round them)
			Sk->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Sk->SetCollisionResponseToAllChannels(ECR_Ignore); Sk->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			AInteractOutline::Add(Sk, INDEX_NONE, (uint64)(UPTRINT)A);
		}
		// their hair, brows and beard (hair strands, a component of their own) in the same outline: the whole person one shape
		{ TArray<UGroomComponent*> Gs; A->GetComponents<UGroomComponent>(Gs); for (UGroomComponent* Gm : Gs) AInteractOutline::Add(Gm, INDEX_NONE, (uint64)(UPTRINT)A); }
		if (W.Body)
		{
			for (USkeletalMeshComponent* Sk : Skels) if (Sk != W.Body && !Sk->GetName().StartsWith(TEXT("Face"))) Sk->SetLeaderPoseComponent(W.Body);
			static UBlendSpace* Move = [] { UBlendSpace* B = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/People/Anims/BS_Idle_Walk_Run_MH.BS_Idle_Walk_Run_MH")); if (B) B->AddToRoot(); return B; }();   // (kept loaded for good, like the garage door sounds)
			if (Move) { W.Body->SetAnimationMode(EAnimationMode::AnimationSingleNode); W.Body->SetAnimation(Move); W.Body->Play(true); }
		}
		People.Add(W);
		ADP_LOG(TEXT("People: %s (%s, %s, %d) - open %.2f, temper %.2f, chat %.2f"), *W.First, *W.Role, W.bFemale ? TEXT("woman") : TEXT("man"), W.Age, W.Open, W.Temper, W.Chat);
	}
}

AActor* AADPPeople::Spawn(const FString& Name, const FVector& At, float Yaw)
{
	UClass* C = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Game/People/Built/%s/BP_%s.BP_%s_C"), *Name, *Name, *Name));
	if (!C) { ADP_WARN(TEXT("People: %s isn't built"), *Name); return nullptr; }
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FVector Spot = At;
	FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(PeopleFloor), true);
	if (GetWorld()->LineTraceSingleByChannel(H, At + FVector(0, 0, 150.f), At - FVector(0, 0, 500.f), ECC_Visibility, Q)) Spot.Z = H.ImpactPoint.Z;
	AActor* A = GetWorld()->SpawnActor<AActor>(C, Spot, FRotator(0.f, Yaw - 90.f, 0.f), P);   // (a MetaHuman faces along its actor's +Y: yaw - 90 turns it to face Yaw)
	Gather();
	return A;
}

AADPPeople::FWho* AADPPeople::Find(AActor* A)
{
	for (FWho& W : People) if (W.A.Get() == A && A) return &W;
	return nullptr;
}

AActor* AADPPeople::Beside(APawn* Me)
{
	if (!Me) return nullptr;
	AActor* Best = nullptr; float BestD = 250.f;
	const FVector F = Me->GetControlRotation().Vector().GetSafeNormal2D();
	for (FWho& W : People)
		if (AActor* A = W.A.Get())
		{
			const FVector D = A->GetActorLocation() - Me->GetActorLocation();
			if (D.Size2D() < BestD && FVector::DotProduct(D.GetSafeNormal2D(), F) > 0.5f) { BestD = D.Size2D(); Best = A; }
		}
	return Best;
}

void AADPPeople::Face(AActor* A, const FVector& At)
{
	if (!A) return;
	const FVector D = (At - A->GetActorLocation()).GetSafeNormal2D();
	if (!D.IsNearlyZero()) A->SetActorRotation(FRotator(0.f, D.Rotation().Yaw - 90.f, 0.f));
}

bool AADPPeople::Hearing(const FVector& At) const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	return PC && PC->PlayerCameraManager && FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), At) < 2500.f;
}

void AADPPeople::Say(AActor* Who, const FString& Line, ETone T)
{
	if (Line == LastSaid && Line.Len() > 0) return;   // (callers re-pick; a line never said twice in a row)
	LastSaid = Line;
	FWho* W = Find(Who);
	const FString Name = W ? (W->Role == TEXT("security") && Who == Scene.Cop.Get() ? FString(TEXT("Police")) : W->Role == TEXT("security") ? FString(TEXT("Security")) : W->First) : FString(TEXT("?"));
	ADP_LOG(TEXT("People: %s says (%s): %s"), *Name, ToneName(T), *Line);
	if (Who && !Hearing(Who->GetActorLocation())) return;   // (too far off to hear)
	SubWho = Name; SubLine = Line; SubTone = T; SubT = FMath::Clamp(1.5f + Line.Len() * 0.06f, 2.5f, 7.f);
}

void AADPPeople::Moment(AActor* Who, const TCHAR* Name)
{
	// a named moment of the body (agitated, rigid fall, limp, cuffed, lifted up ...): logged now; the real animations
	// (to-do 14, 69h, 69i) play here later
	FWho* W = Find(Who);
	ADP_LOG(TEXT("People: moment \"%s\" for %s"), Name, W ? *W->First : TEXT("?"));
}

// ---------------------------------------------------------------------------------------------- the menu

void AADPPeople::Open(AActor* Who)
{
	FWho* W = Find(Who); if (!W) return;
	bMenu = true; MenuWho = Who; AInteractOutline::Hide = (uint64)(UPTRINT)Who;   // (no outline on them while you talk)
	if (W->bUnruly && !SceneOn()) ShowConduct(); else ShowStart();
	Face(Who, GetWorld()->GetFirstPlayerController() && GetWorld()->GetFirstPlayerController()->GetPawn() ? GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation() : Who->GetActorLocation());
}

void AADPPeople::Close() { bMenu = false; MenuWho = nullptr; MenuOptions.Reset(); AInteractOutline::Hide = 0; }

void AADPPeople::ShowStart()
{
	FWho* W = Find(MenuWho.Get()); if (!W) { Close(); return; }
	Mode = EMenu::Start;
	MenuTitle = FString::Printf(TEXT("%s  ·  %s"), *W->First.ToUpper(), *W->Role.ToUpper());
	MenuText = W->Temper > 0.8f ? TEXT("They look at you, a little impatient.") : TEXT("They turn to you with a smile.");
	MenuOptions = {TEXT("Just talk"), TEXT("Ask if they would like to work for you"), TEXT("Goodbye")};
}

void AADPPeople::ShowQuestions()
{
	FWho* W = Find(MenuWho.Get()); if (!W) { Close(); return; }
	Mode = EMenu::Questions;
	// five of the questions this time, in a random order (the generated options)
	AskOrder.Reset(); for (int32 i = 0; i < UE_ARRAY_COUNT(Questions); ++i) AskOrder.Add(i);
	for (int32 i = AskOrder.Num() - 1; i > 0; --i) AskOrder.Swap(i, Rng.RandRange(0, i));
	AskOrder.SetNum(5);
	const bool bStaff = W->Role != TEXT("passenger");
	MenuOptions.Reset();
	for (const int32 Q : AskOrder) MenuOptions.Add(bStaff ? Questions[Q].Staff : Questions[Q].Pax);
	MenuOptions.Add(TEXT("Back"));
}

void AADPPeople::Answer(int32 Topic)
{
	FWho* W = Find(MenuWho.Get()); if (!W) { Close(); return; }
	Mode = EMenu::Answer;
	const bool bStaff = W->Role != TEXT("passenger");
	FString A;
	// personal topics: the private ones keep it to themselves; the others answer in their own way
	if (Questions[Topic].bPersonal && W->Open < 0.4f) A = PickT(Refusals, Rng);
	else { const int32 K = W->Temper > 0.78f ? 2 : W->Chat > 0.5f ? 1 : 0; A = bStaff ? StaffAns[Topic][K] : PaxAns[Topic][K]; }
	MenuText = FString::Printf(TEXT("\"%s\""), *A);
	Say(W->A.Get(), A, W->Temper > 0.78f ? ETone::Firm : ETone::Calm);
	MenuOptions = {TEXT("Ask something else"), TEXT("Goodbye")};
}

void AADPPeople::AskWork()
{
	FWho* W = Find(MenuWho.Get()); if (!W) { Close(); return; }
	Mode = EMenu::Answer;
	// asked once: the answer stays (13% yes)
	if (!W->bAskedWork) { W->bAskedWork = true; W->bWorks = Rng.FRand() < 0.13f; }
	const FString A = W->bWorks ? PickT(HireYes, Rng) : PickT(HireNo, Rng);
	MenuText = FString::Printf(TEXT("\"%s\"%s"), *A, W->bWorks ? TEXT("\n\n(They will join your staff when hiring opens - the staff screens come later.)") : TEXT(""));
	Say(W->A.Get(), A, W->bWorks ? ETone::Mild : ETone::Calm);
	ADP_LOG(TEXT("People: %s asked to work for you - %s"), *W->First, W->bWorks ? TEXT("YES") : TEXT("no"));
	MenuOptions = {TEXT("Ask something else"), TEXT("Goodbye")};
}

void AADPPeople::ShowConduct()
{
	FWho* W = Find(MenuWho.Get()); if (!W) { Close(); return; }
	Mode = EMenu::Conduct;
	FRandomStream S(Seed(W->Name) + 7);
	MenuTitle = FString::Printf(TEXT("%s  ·  PASSENGER"), *W->First.ToUpper());
	MenuText = FString::Printf(TEXT("%s is %s."), *W->First, PickT(Doing, S));
	MenuOptions = {TEXT("Ask them firmly to calm down"), TEXT("Talk to them calmly and listen"), TEXT("Leave it")};
	const FL& L = Pick(PaxAtYou, Rng); Say(W->A.Get(), L.Text, L.Tone);
	Moment(W->A.Get(), TEXT("Agitated"));
}

void AADPPeople::TryCalm(bool bFirm)
{
	FWho* W = Find(MenuWho.Get()); if (!W) { Close(); return; }
	// 75% calm down, say sorry humbly, promise to behave and do; 25% won't listen
	const bool bCalm = ForceCalm >= 0 ? ForceCalm == 0 : Rng.FRand() < 0.75f;
	ADP_LOG(TEXT("People: you %s - %s"), bFirm ? TEXT("told them firmly to calm down") : TEXT("talked calmly and listened"), bCalm ? TEXT("they calm down") : TEXT("they won't listen"));
	if (bCalm)
	{
		const FL& L = Pick(Calmed, Rng); Say(W->A.Get(), L.Text, L.Tone);
		W->bUnruly = false; Moment(W->A.Get(), TEXT("Humble"));
		Mode = EMenu::Answer; MenuText = FString::Printf(TEXT("\"%s\""), L.Text); MenuOptions = {TEXT("Goodbye")};
	}
	else
	{
		const FL& L = Pick(Refused, Rng); Say(W->A.Get(), L.Text, L.Tone);
		Moment(W->A.Get(), TEXT("Agitated"));
		Mode = EMenu::Refused; MenuText = FString::Printf(TEXT("\"%s\"\n\nThey won't listen."), L.Text);
		MenuOptions = {TEXT("Call airport security"), TEXT("Leave it")};
	}
}

void AADPPeople::Choose(int32 I)
{
	switch (Mode)
	{
	case EMenu::Start: if (I == 0) ShowQuestions(); else if (I == 1) AskWork(); else Close(); break;
	case EMenu::Questions: if (AskOrder.IsValidIndex(I)) Answer(AskOrder[I]); else ShowStart(); break;
	case EMenu::Answer: if (I == 0 && MenuOptions.Num() > 1) ShowStart(); else Close(); break;
	case EMenu::Conduct: if (I <= 1) TryCalm(I == 0); else Close(); break;
	case EMenu::Refused: if (I == 0) CallSecurity(); Close(); break;
	}
}

// ---------------------------------------------------------------------------------------------- the director

FString AADPPeople::PickBuilt(const TCHAR* WhatRole, bool* bFemale)
{
	if (!Roster.Num()) LoadRoster();
	TArray<FString> Names; for (const TPair<FString, FRosterRow>& R : Roster) if (R.Value.Role == WhatRole) Names.Add(R.Key);
	if (!Names.Num()) return FString();
	const FString N = Names[Rng.RandRange(0, Names.Num() - 1)];
	if (bFemale) *bFemale = Roster[N].bFemale;
	return N;
}

FVector AADPPeople::HiddenSpot(const FVector& Near) const
{
	// a spot 18-40 m off on a floor, that you can't see from where you are, with a clear straight way to the passenger
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector Eye = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Near;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(PeopleHidden), false);
	FVector Best = Near + FVector(2500.f, 0, 0); float BestScore = -1.f;
	for (int32 k = 0; k < 48; ++k)
	{
		const float A = k * 2.f * PI / 48.f, R = 1800.f + (k % 4) * 700.f;
		FVector P = Near + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R;
		FHitResult H;
		if (!GetWorld()->LineTraceSingleByChannel(H, P + FVector(0, 0, 200.f), P - FVector(0, 0, 400.f), ECC_Visibility, Q)) continue;
		P.Z = H.ImpactPoint.Z;
		if (FMath::Abs(P.Z - Near.Z) > 60.f) continue;                                   // (the same floor)
		FHitResult Hw;
		if (GetWorld()->SweepSingleByChannel(Hw, P + FVector(0, 0, 90.f), Near + FVector(0, 0, 90.f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(30.f), Q)
			&& FVector::Dist2D(Hw.ImpactPoint, Near) > 150.f) continue;                   // (no clear way to the passenger)
		const bool bSeen = !GetWorld()->LineTraceSingleByChannel(H, Eye, P + FVector(0, 0, 150.f), ECC_Visibility, Q);
		const float Score = (bSeen ? 0.f : 10.f) + R / 1000.f;
		if (Score > BestScore) { BestScore = Score; Best = P; }
	}
	return Best;
}

bool AADPPeople::WalkTo(FWho* W, const FVector& To, float Dt, float Stop)
{
	AActor* A = W ? W->A.Get() : nullptr; if (!A) return true;
	FVector D = To - A->GetActorLocation(); D.Z = 0.f;
	const float Dist = D.Size();
	const bool bThere = Dist < Stop;
	const float Want = bThere ? 0.f : 140.f;   // (a brisk walk, 1.4 m/s)
	W->Speed = FMath::FInterpTo(W->Speed, Want, Dt, 4.f);
	if (!bThere)
	{
		FVector P = A->GetActorLocation() + D / Dist * FMath::Min(Dist, W->Speed * Dt);
		FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(PeopleWalk), false, A);
		if (GetWorld()->LineTraceSingleByChannel(H, P + FVector(0, 0, 120.f), P - FVector(0, 0, 200.f), ECC_Visibility, Q)) P.Z = H.ImpactPoint.Z;
		A->SetActorLocation(P);
		A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, D.Rotation().Yaw - 90.f, 0.f), Dt, 6.f));
	}
	if (W->Body) if (UAnimSingleNodeInstance* One = W->Body->GetSingleNodeInstance())
	{
		One->SetBlendSpacePosition(FVector(0.f, FMath::Min(W->Speed * 1.5f, 300.f), 0.f));
		One->SetPlayRate(W->Speed > 20.f ? FMath::Clamp(W->Speed / FMath::Max(W->Speed * 1.5f, 100.f), 0.5f, 1.4f) : 1.f);
	}
	return bThere;
}

void AADPPeople::MakeUnruly(AActor* Who, bool bViolent)
{
	if (FWho* W = Find(Who)) { W->bUnruly = true; W->bViolent = bViolent; ADP_LOG(TEXT("People: %s is now an unruly passenger%s"), *W->First, bViolent ? TEXT(" (violent)") : TEXT("")); }
}

void AADPPeople::CallSecurity()
{
	FWho* P = Find(MenuWho.Get()); if (!P || SceneOn()) return;
	ADP_LOG(TEXT("People: you call airport security for %s"), *P->First);
	Scene = FScene(); Scene.Pax = P->A; Scene.Stage = EStage::SecurityComing;
	Scene.Wait = Rng.FRandRange(3.f, 6.f);                         // (a few seconds before they come into it)
	Scene.GuardFrom = HiddenSpot(P->A->GetActorLocation());
	Scene.StartTalkAt = Rng.FRandRange(400.f, 1000.f);              // (they start talking within 10 m - at random closer)
}

void AADPPeople::StepScene(float Dt)
{
	FScene& S = Scene;
	FWho* P = Find(S.Pax.Get());
	if (!P) { S.Stage = EStage::None; return; }
	S.T += Dt;
	FWho* G = Find(S.Guard.Get()); FWho* C = Find(S.Cop.Get());
	const FVector PaxAt = P->A->GetActorLocation();
	auto Exchange = [&](AActor* Officer, const FL* OffLines, int32 NOff, bool bAngry)
	{
		// talking back and forth: a line every 3-5 s, the officer and the passenger in turn
		if ((S.Wait -= Dt) > 0.f) return;
		S.Wait = Rng.FRandRange(3.f, 5.f);
		if (S.Line % 2 == 0) { int32 Ix = Rng.RandRange(0, NOff - 1); for (int32 k = 0; k < 4 && NOff > 1 && OffLines[Ix].Text == GLast; ++k) Ix = Rng.RandRange(0, NOff - 1); const FL& L = OffLines[Ix]; GLast = L.Text; Say(Officer, L.Text, L.Tone); }
		else { const FL& L = !bAngry ? Pick(PaxAtGuardMild, Rng) : Officer == S.Cop.Get() ? Pick(PaxAtCop, Rng) : Pick(PaxAtGuard, Rng); Say(P->A.Get(), L.Text, L.Tone); if (bAngry) Moment(P->A.Get(), TEXT("ViolentGestures")); }
		++S.Line;
	};
	switch (S.Stage)
	{
	case EStage::SecurityComing:
		if (!G)
		{
			if ((S.Wait -= Dt) > 0.f) break;
			AActor* A = Spawn(PickBuilt(TEXT("security")), S.GuardFrom, 0.f); S.Guard = A; G = Find(A);
			ADP_LOG(TEXT("People: security on the way from %.0f m off"), FVector::Dist(S.GuardFrom, PaxAt) / 100.f);
			if (!G) { S.Stage = EStage::None; break; }
		}
		if (!S.bSpoke && FVector::Dist2D(G->A->GetActorLocation(), PaxAt) < S.StartTalkAt) { S.bSpoke = true; Say(G->A.Get(), TEXT("Security - everything alright over here?"), ETone::Firm); }
		if (WalkTo(G, PaxAt, Dt, 150.f)) { Face(G->A.Get(), PaxAt); Face(P->A.Get(), G->A->GetActorLocation()); S.Stage = EStage::SecurityTalk; S.T = 0.f; S.TalkFor = Rng.FRandRange(14.f, 30.f); S.Line = 0; S.Wait = 0.5f; }
		break;
	case EStage::SecurityTalk:
		WalkTo(G, PaxAt, Dt, 150.f);
		Exchange(G->A.Get(), GuardSays, UE_ARRAY_COUNT(GuardSays), P->bViolent || true);
		if (S.T > S.TalkFor)
		{
			// security settle it (40%), or call the police
			const bool bSettled = ForceSecurity >= 0 ? ForceSecurity == 0 : Rng.FRand() < 0.4f;
			if (bSettled) { const FL& L = Pick(GuardWins, Rng); Say(P->A.Get(), L.Text, L.Tone); P->bUnruly = false; Moment(P->A.Get(), TEXT("Humble")); S.Stage = EStage::Escort; S.ExitTo = S.GuardFrom; S.T = 0.f; ADP_LOG(TEXT("People: security settled it")); }
			else
			{
				const FL& L = Pick(GuardCalls, Rng); Say(G->A.Get(), L.Text, L.Tone);
				S.Stage = EStage::PoliceComing; S.T = 0.f; S.Wait = Rng.FRandRange(4.f, 8.f); S.CopFrom = HiddenSpot(PaxAt); S.bSpoke = false; S.StartTalkAt = Rng.FRandRange(400.f, 1000.f);
				ADP_LOG(TEXT("People: security calls the police"));
			}
		}
		break;
	case EStage::PoliceComing:
		if (!C)
		{
			if ((S.Wait -= Dt) > 0.f) break;
			// (police stand-in: a security MetaHuman until the police have their own people - test only)
			AActor* A = Spawn(PickBuilt(TEXT("security")), S.CopFrom, 0.f); S.Cop = A; C = Find(A);
			if (!C) { S.Stage = EStage::None; break; }
			ADP_LOG(TEXT("People: police on the way from %.0f m off (stand-in: %s)"), FVector::Dist(S.CopFrom, PaxAt) / 100.f, *C->Name);
		}
		if (!S.bSpoke && FVector::Dist2D(C->A->GetActorLocation(), PaxAt) < S.StartTalkAt) { S.bSpoke = true; Say(C->A.Get(), TEXT("Police! Step away from the passenger."), ETone::Firm); }
		if (WalkTo(C, PaxAt, Dt, 160.f)) { Face(C->A.Get(), PaxAt); Face(P->A.Get(), C->A->GetActorLocation()); S.Stage = EStage::PoliceTalk; S.T = 0.f; S.TalkFor = Rng.FRandRange(14.f, 26.f); S.Line = 0; S.Wait = 0.5f; }
		break;
	case EStage::PoliceTalk:
		WalkTo(C, PaxAt, Dt, 160.f);
		Exchange(C->A.Get(), CopSays, UE_ARRAY_COUNT(CopSays), true);
		if (S.T > S.TalkFor)
		{
			const bool bViolent = ForceViolent >= 0 ? ForceViolent == 1 : (P->bViolent || Rng.FRand() < 0.15f);
			if (!bViolent) { const FL& L = Pick(PaxGivesIn, Rng); Say(P->A.Get(), L.Text, L.Tone); S.Stage = EStage::Cuff; S.T = 0.f; S.StunPhase = -1; }
			else { const FL& L = Pick(CopWarn, Rng); Say(C->A.Get(), L.Text, L.Tone); Moment(C->A.Get(), TEXT("DrawStunGun")); Moment(P->A.Get(), TEXT("ViolentGestures")); S.Stage = EStage::LastWarning; S.T = 0.f; S.Line = 0; }
		}
		break;
	case EStage::LastWarning:
		if (S.Line == 0 && S.T > 3.f)
		{
			const bool bObeys = ForceObey >= 0 ? ForceObey == 1 : Rng.FRand() < 0.4f;
			if (bObeys) { const FL& L = Pick(PaxObeysWarn, Rng); Say(P->A.Get(), L.Text, L.Tone); Moment(P->A.Get(), TEXT("GetDownOnTheFloor")); S.Stage = EStage::Cuff; S.T = 0.f; S.StunPhase = -1; break; }
			const FL& L = Pick(PaxRefusesWarn, Rng); Say(P->A.Get(), L.Text, L.Tone); S.Line = 1;
		}
		if (S.Line == 1 && S.T > 6.f) { const FL& L = Pick(CopForce, Rng); Say(C->A.Get(), L.Text, L.Tone); S.Line = 2; }
		if (S.Line == 2 && S.T > 8.5f) { const FL& L = Pick(Defiant, Rng); Say(P->A.Get(), L.Text, L.Tone); S.Line = 3; }   // (always a defiant reply)
		if (S.Line == 3 && S.T > 10.5f)
		{
			Moment(C->A.Get(), TEXT("FireStunGun"));
			S.Stage = EStage::Stun; S.T = 0.f; S.StunPhase = 0; S.Stood = P->A->GetActorRotation();
			if (P->Body) if (UAnimSingleNodeInstance* One = P->Body->GetSingleNodeInstance()) One->SetPlaying(false);   // (the body locks)
			Moment(P->A.Get(), TEXT("StunRigid"));
		}
		break;
	case EStage::Stun:
	{
		// rigid at once and falling stiff without catching themselves (the first 0.8 s: pivoting forward over the feet,
		// faster and faster, face down); rigid on the floor for the rest of the 5 s cycle; then limp; the officer comes
		// over 2 s later
		const float Fall = FMath::Clamp(S.T / 0.8f, 0.f, 1.f);
		FRotator R = S.Stood; R.Roll = -88.f * Fall * Fall;   // (a MetaHuman faces its own right: tipping forward is a roll)
		P->A->SetActorRotation(R);
		if (S.StunPhase == 0 && S.T > 5.f) { S.StunPhase = 1; Moment(P->A.Get(), TEXT("StunLimp")); Say(P->A.Get(), TEXT("Aaagh..."), ETone::Mild); }
		if (S.StunPhase == 1 && S.T > 7.f) { if (WalkTo(C, PaxAt, Dt, 110.f)) { S.Stage = EStage::Cuff; S.T = 0.f; } }
		break;
	}
	case EStage::Cuff:
		if (S.Line < 100) { S.Line = 100; const FL& L = Pick(CopCuff, Rng); Say(C ? C->A.Get() : nullptr, L.Text, L.Tone); Moment(P->A.Get(), S.StunPhase >= 0 ? TEXT("CuffedFaceDown") : TEXT("Cuffed")); }
		if (S.StunPhase >= 0 && S.T > 3.f && S.T < 5.f)
		{
			// lifted up off the floor
			FRotator R = S.Stood; R.Roll = -88.f * (1.f - FMath::Clamp((S.T - 3.f) / 1.5f, 0.f, 1.f)); P->A->SetActorRotation(R);
			if (S.Line == 100) { S.Line = 101; Moment(P->A.Get(), TEXT("LiftedUp")); if (P->Body) if (UAnimSingleNodeInstance* One = P->Body->GetSingleNodeInstance()) One->SetPlaying(true); }
		}
		if (S.T > 5.f) { S.Stage = EStage::Escort; S.T = 0.f; S.ExitTo = S.CopFrom; Moment(P->A.Get(), TEXT("Escorted")); ADP_LOG(TEXT("People: %s escorted out"), *P->First); }
		break;
	case EStage::Escort:
	{
		// police involved: the passenger walked out the way the officers came (the officer just behind), all gone out
		// of sight. Settled by security: the passenger stays, the guard walks back the way they came
		const bool bTaken = S.Cop.IsValid();
		bool bOver = S.T > 60.f;
		if (bTaken)
		{
			bOver |= WalkTo(P, S.ExitTo, Dt, 120.f);
			WalkTo(C, P->A->GetActorLocation() - (S.ExitTo - P->A->GetActorLocation()).GetSafeNormal2D() * 90.f, Dt, 60.f);
			if (G) WalkTo(G, S.GuardFrom, Dt, 120.f);
		}
		else bOver |= (G == nullptr) || WalkTo(G, S.GuardFrom, Dt, 120.f);
		if (bOver)
		{
			ADP_LOG(TEXT("People: the scene is over (%s)"), bTaken ? TEXT("the passenger taken away") : TEXT("settled by security"));
			if (S.Guard.IsValid()) S.Guard->Destroy();
			if (S.Cop.IsValid()) S.Cop->Destroy();
			if (bTaken) P->A->Destroy();
			S.Stage = EStage::None;
		}
		break;
	}
	default: break;
	}
}

void AADPPeople::Tick(float Dt)
{
	ADP_PERF_SCOPE("people: talk and conduct (each frame)", 2.f);
	Super::Tick(Dt);
	if ((GatherIn -= Dt) <= 0.f) { GatherIn = 10.f; ADP_PERF_SCOPE("people: finding the people in the world", 4.f); Gather(); }   // (people placed here list themselves at once)
	People.RemoveAll([](const FWho& W) { return !W.A.IsValid(); });
	if (SubT > 0.f) SubT -= Dt;
	if (SceneOn()) StepScene(Dt);
	// the menu closes when you walk away from the person
	if (bMenu)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (!MenuWho.IsValid() || (PC && PC->GetPawn() && FVector::Dist2D(PC->GetPawn()->GetActorLocation(), MenuWho->GetActorLocation()) > 450.f)) Close();
	}
}
