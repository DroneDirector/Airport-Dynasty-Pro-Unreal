#include "People.h"
#include "PeopleLayers.h"
#include "PaxAnimInstance.h"
#include "ADPLog.h"
#include "PerfLog.h"
#include "InteractOutline.h"
#include "GroomComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "TerminalPlan.h"
#include "PaxFlow.h"
#include "Seats.h"
#include "TruckDriverRig.h"
#include "DriverPoseAnim.h"
#include "Components/PoseableMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "PaxNav.h"
#include "Site.h"
#include "StaffPeople.h"
#include "Sim/ADPCrew.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPStaff.h"

namespace ADPPeopleInternal
{
	using ETone = AADPPeople::ETone;
	struct FL { const TCHAR* Text; ETone Tone; };

	// first names (made up at random per person, by their own seed)
	const TCHAR* Women[] = {TEXT("Anna"), TEXT("Maria"), TEXT("Sophie"), TEXT("Céline"), TEXT("Laura"), TEXT("Nina"), TEXT("Camille"), TEXT("Julia"), TEXT("Sofia"), TEXT("Emma"), TEXT("Chloé"), TEXT("Lisa"), TEXT("Clara"), TEXT("Isabelle"), TEXT("Eva"), TEXT("Giulia")};
	const TCHAR* Men[] = {TEXT("Luca"), TEXT("Peter"), TEXT("David"), TEXT("Thomas"), TEXT("Nicolas"), TEXT("Simon"), TEXT("Martin"), TEXT("Julien"), TEXT("Olivier"), TEXT("Daniel"), TEXT("Marco"), TEXT("Michael"), TEXT("Andreas"), TEXT("Paul"), TEXT("Leo"), TEXT("Reto")};

	// the questions (bPersonal: refused by the private ones); passengers and staff have their own
	struct FQ { const TCHAR* Pax; const TCHAR* Staff; bool bPersonal; };
	const FQ Questions[] = {
		{TEXT("How are you today?"), TEXT("How are you today?"), false},
		{TEXT("Where are you travelling to?"), TEXT("How is work going today?"), false},
		{TEXT("What do you think of the airport?"), TEXT("What's it like working here?"), false},
		{TEXT("How do you like Switzerland?"), TEXT("What do you like about living in Switzerland?"), false},
		{TEXT("Have you been to Montreux?"), TEXT("What should I see in Montreux?"), false},
		{TEXT("Have you been waiting long?"), TEXT("How long is your shift today?"), false},
		{TEXT("What do you do for a living?"), TEXT("How long have you worked here?"), true},
		{TEXT("Are you travelling with family?"), TEXT("Do you have a family at home?"), true},
		{TEXT("Where are you from?"), TEXT("Where are you from?"), true},
		{TEXT("Is there anything we could do better here?"), TEXT("Is there anything that would make your job easier?"), false},
	};
	// answers per question: [0] friendly and short, [1] friendly and chatty, [2] grumpy
	const TCHAR* PaxAns[][3] = {
		{TEXT("Good, thanks."), TEXT("Oh, quite well, thank you for asking - a bit tired, but looking forward to the trip."), TEXT("I've been better.")},
		{TEXT(""), TEXT(""), TEXT("")},   // (their own flight and why: Trip())
		{TEXT("It's nice. Clean."), TEXT("It's lovely, really - light and calm, and the view over the mountains is something else."), TEXT("It'll do. Bit pricey in the shops.")},
		{TEXT("It's beautiful."), TEXT("I love it - the mountains, the lake, the trains that are always on time. I took the cogwheel railway up to Rochers-de-Naye and didn't want to come down again."), TEXT("Nice views. Everything costs a fortune, though.")},
		{TEXT("Yes, it's lovely."), TEXT("Yes! We walked along the lake all the way to Chillon Castle, and the flowers on the promenade were wonderful."), TEXT("Too many tourists by the lake for me.")},
		{TEXT("Not too long."), TEXT("About an hour, but I came early on purpose - I like to sit and watch the planes."), TEXT("Long enough.")},
		{TEXT("I'm a teacher."), TEXT("I work for a watchmaker - lots of travel, lots of meetings, never enough sleep."), TEXT("Office work. Nothing exciting.")},
		{TEXT("With my partner."), TEXT("With my kids and my mother - she's never flown before, so it's a big day."), TEXT("Alone, thankfully.")},
		{TEXT("From Lausanne."), TEXT("I grew up in a little village up in the Valais, but I've lived in Montreux for years now."), TEXT("Around here.")},
		{TEXT("More seats by the gates."), TEXT("Maybe more places to charge a phone - and a few more seats by the gates when it's busy."), TEXT("Cheaper coffee, for a start.")},
	};
	const TCHAR* StaffAns[][3] = {
		{TEXT("Good, thanks!"), TEXT("Pretty good, thanks - busy morning, but a good team on today."), TEXT("Tired. Long day.")},
		{TEXT("Busy, but fine."), TEXT("Busy! Three departures in a row, but everything's on time so far, touch wood."), TEXT("Don't ask.")},
		{TEXT("I like it."), TEXT("Honestly, it's a good place - people look out for each other, and no two days are the same."), TEXT("It's a job.")},
		{TEXT("The mountains."), TEXT("Skiing in winter, swimming in the lake in summer, and the chocolate all year round - what's not to like?"), TEXT("The holidays. When I get any.")},
		{TEXT("Chillon Castle."), TEXT("Chillon Castle, for sure - and take the train up through the vineyards at sunset, you won't forget it. In July there's the jazz festival too."), TEXT("The lake. It's hard to miss.")},
		{TEXT("Eight hours."), TEXT("Twelve today, I'm covering for a colleague - but I get the next two days off."), TEXT("Too long.")},
		{TEXT("Five years."), TEXT("Almost eight years now - I started when the old terminal was still here."), TEXT("Long enough to know better.")},
		{TEXT("Yes, two kids."), TEXT("My partner and two little ones - they love coming to watch the planes."), TEXT("That's my business.")},
		{TEXT("From Aigle."), TEXT("From Fribourg originally, but I moved to Villeneuve for this job."), TEXT("Around here.")},
		{TEXT("More staff at peak times."), TEXT("A few more hands when the morning flights all go at once - that would help a lot."), TEXT("Better pay.")},
	};
	const TCHAR* Refusals[] = {TEXT("I'd rather not talk about that, sorry."), TEXT("That's a bit personal - I'll keep that to myself."), TEXT("Let's just say... no comment."), TEXT("I don't really want to talk about it.")};
	const TCHAR* HireNo[] = {TEXT("Thanks, that's kind - but I'm not looking for work."), TEXT("Oh, thank you, but I'm happy where I am."), TEXT("Thanks, but no - I've got a job already."), TEXT("That's nice of you to ask, but not right now.")};
	const TCHAR* HireYes[] = {
		TEXT("Yes, I would love to work for you - I've always wanted to work at an airport."),
		TEXT("Really? Yes! My contract ends next month and I've been looking for something new."),
		TEXT("Yes, I'd love that - I want to stay here by the lake, and good jobs close to home are hard to find."),
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
	// why they go there (%s: the place), and what those who came here say
	const TCHAR* TripWhy[] = {
		TEXT("To %s - my sister just had a baby and I get to meet him for the first time. I can hardly wait!"),
		TEXT("To %s! It's our anniversary, and it's where we first met."),
		TEXT("To %s - a whole week without meetings. I've been looking forward to it since winter."),
		TEXT("To %s - I'm going home. Switzerland was wonderful, but I miss my own bed."),
		TEXT("To %s. An old friend is getting married there, and I'm giving a speech - wish me luck."),
		TEXT("To %s - I've always wanted to see it, and I have a list of restaurants this long."),
		TEXT("To %s, for work, sadly - but I'll stay the weekend and see a concert."),
		TEXT("To %s - my grandchildren live there. They've already planned every day for me."),
		TEXT("To %s - I'm starting a new job there on Monday. A bit nervous, but mostly excited!"),
		TEXT("To %s. I'm running the marathon there, and I even trained for it this time.")};
	const TCHAR* TripShort[] = {TEXT("To %s. I'm looking forward to it."), TEXT("To %s, to see my family."), TEXT("%s - a few days off."), TEXT("To %s, back home.")};
	const TCHAR* TripGrumpy[] = {TEXT("%s. Work. What else."), TEXT("%s, if we ever leave."), TEXT("%s. The in-laws.")};
	const TCHAR* CameWhy[] = {
		TEXT("Right here! I've just flown in from %s - a week of hiking in the mountains is waiting for me."),
		TEXT("I'm already there - I came in from %s to visit my daughter in Montreux."),
		TEXT("Just arrived from %s. First thing I'll do is eat a proper fondue by the lake."),
		TEXT("I've just come home from %s. It was great, but nothing beats seeing the Dents du Midi again.")};
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
	static TMap<TWeakObjectPtr<UClass>, bool> Built;   // (each class's path looked at once: the world has tens of thousands of actors)
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		const bool* Is = Built.Find(A->GetClass());
		if (!Is) Is = &Built.Add(A->GetClass(), A->GetClass()->GetPathName().Contains(TEXT("/Game/People/Built/MH_")));
		if (!*Is || A == MeHuman || (PC && A->GetOwner() == PC->GetPawn())) continue;
		AddPerson(A);
	}
}

void AADPPeople::AddPerson(AActor* A)
{
	if (!A || Find(A)) return;
	if (!Roster.Num()) LoadRoster();
	{
		const FString Path = A->GetClass()->GetPathName();
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
			if (Move) { W.Body->SetAnimationMode(EAnimationMode::AnimationSingleNode); W.Body->SetAnimation(Move); W.Body->Play(true); UPaxAnimInstance::Install(W.Body); }   // (ours: the feet on the ground)
		}
		ADPLayers::Add(A, W.Body, W.Name);
		People.Add(W);
		ADP_LOG(TEXT("People: %s (%s, %s, %d) - open %.2f, temper %.2f, chat %.2f"), *W.First, *W.Role, W.bFemale ? TEXT("woman") : TEXT("man"), W.Age, W.Open, W.Temper, W.Chat);
	}
}

AActor* AADPPeople::Spawn(const FString& Name, const FVector& At, float Yaw)
{
	UClass* C = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Game/People/Built/%s/BP_%s.BP_%s_C"), *Name, *Name, *Name));
	if (!C) { ADP_WARN(TEXT("People: %s isn't built"), *Name); return nullptr; }
	ADP_PERF_SCOPE("people: one person made", 6.f);
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FVector Spot = At;
	FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(PeopleFloor), true);
	if (GetWorld()->LineTraceSingleByChannel(H, At + FVector(0, 0, 150.f), At - FVector(0, 0, 500.f), ECC_Visibility, Q)) Spot.Z = H.ImpactPoint.Z;
	const ADPLayers::FBodyOnly BodyOnly(C, Name);   // (face, clothes and hair follow one a frame - PeopleLayers.h)
	AActor* A = GetWorld()->SpawnActor<AActor>(C, Spot, FRotator(0.f, Yaw - 90.f, 0.f), P);   // (a MetaHuman faces along its actor's +Y: yaw - 90 turns it to face Yaw)
	PaxNav::NoNav(A);   // (people walk on the navmesh, they never shape it: no rebuilds as they move or come and go)
	AddPerson(A);   // (only this one: no look through the whole world for each passenger that comes)
	return A;
}

AADPPeople::FWho* AADPPeople::Find(AActor* A)
{
	for (FWho& W : People) if (W.A.Get() == A && A) return &W;
	return nullptr;
}

bool AADPPeople::AnyoneNear(const FVector& At, float Radius) const
{
	for (const FWho& W : People)
		if (const AActor* A = W.A.Get()) { const FVector L = A->GetActorLocation(); if (FMath::Abs(L.Z - At.Z) < 150.f && FVector::DistSquared2D(L, At) < Radius * Radius) return true; }
	return false;
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
	else
	{
		const int32 K = W->Temper > 0.78f ? 2 : W->Chat > 0.5f ? 1 : 0;
		A = bStaff ? StaffAns[Topic][K] : PaxAns[Topic][K];
		if (A.IsEmpty())
		{
			// where they go (their own flight) and why: the same answer each time they are asked
			FRandomStream S(int32(GetTypeHash(W->First) ^ GetTypeHash(W->A.Get()))); FString Place; bool bArr = false;
			if (!APaxFlow::TripOf(GetWorld(), W->A.Get(), Place, bArr) || Place.IsEmpty())
			{
				TArray<FString> Names, Codes; ADPSite::Destinations(Names, Codes);
				Place = Names.Num() ? Names[S.RandRange(0, Names.Num() - 1)] : FString(TEXT("Geneva")); bArr = false;
			}
			A = FString(bArr ? PickT(CameWhy, S) : K == 2 ? PickT(TripGrumpy, S) : K == 1 ? PickT(TripWhy, S) : PickT(TripShort, S)).Replace(TEXT("%s"), *Place);
		}
	}
	MenuText = FString::Printf(TEXT("\"%s\""), *A);
	Say(W->A.Get(), A, W->Temper > 0.78f ? ETone::Firm : ETone::Calm);
	MenuOptions = {TEXT("Ask something else"), TEXT("Goodbye")};
}

void AADPPeople::AskWork()
{
	FWho* W = Find(MenuWho.Get()); if (!W) { Close(); return; }
	Mode = EMenu::Answer;
	// somebody on your staff already (to-do 206 part 3)
	UADPSim* Sim = UADPSim::Get(GetWorld()); const bool bCareer = Sim && Sim->IsRunning();
	const UStaffPeople* Sp = UStaffPeople::Get(GetWorld());
	if ((Sp && !Sp->IdOf(W->A.Get()).IsEmpty()) || (bCareer && ADPCrew::FaceWorks(Sim->World, W->Name)))
	{
		MenuText = TEXT("\"I already work for you!\"");
		Say(W->A.Get(), TEXT("I already work for you!"), ETone::Calm);
		MenuOptions = {TEXT("Ask something else"), TEXT("Goodbye")};
		return;
	}
	// asked once: the answer stays (13% yes)
	if (!W->bAskedWork) { W->bAskedWork = true; W->bWorks = Rng.FRand() < 0.13f; }
	const FString A = W->bWorks ? PickT(HireYes, Rng) : PickT(HireNo, Rng);
	MenuText = FString::Printf(TEXT("\"%s\""), *A);
	Say(W->A.Get(), A, W->bWorks ? ETone::Mild : ETone::Calm);
	ADP_LOG(TEXT("People: %s asked to work for you - %s"), *W->First, W->bWorks ? TEXT("YES") : TEXT("no"));
	MenuOptions = {TEXT("Ask something else"), TEXT("Goodbye")};
	if (!W->bWorks) return;
	if (!bCareer) { MenuText += TEXT("\n\n(There is no airline of yours yet to hire them for.)"); return; }
	// the job: crew and staff their own; a passenger any that needs no licence
	Jobs.Reset(); JobPage = 0;
	const TCHAR* Own = W->Role == TEXT("pilot") ? TEXT("fo") : W->Role == TEXT("cabin") ? TEXT("fa") : nullptr;
	if (Own) Jobs.Add(Own);
	else if (ADPCrew::Role(W->Role)) Jobs.Add(W->Role);
	else for (const ADPCrew::FRole& R : ADPCrew::Roles()) if (R.bTalk) Jobs.Add(R.Id);
	if (Jobs.Num()) ShowJobs();
}

void AADPPeople::ShowJobs()
{
	UADPSim* Sim = UADPSim::Get(GetWorld());
	if (!Find(MenuWho.Get()) || !Sim || !Sim->IsRunning()) { Close(); return; }
	Mode = EMenu::Job;
	const FString C = ADPStaff::CountryOf(Sim->World, UADPSim::Me());
	MenuOptions.Reset();
	for (int32 k = JobPage * 6; k < FMath::Min(Jobs.Num(), JobPage * 6 + 6); ++k)
		MenuOptions.Add(FString::Printf(TEXT("Hire: %s, $%.0f a week"), *ADPCrew::RoleName(Jobs[k]), ADPStaff::GoingRate(Jobs[k], C)));
	if (Jobs.Num() > 6) MenuOptions.Add(TEXT("Other jobs"));
	MenuOptions.Add(TEXT("Not now"));
}

void AADPPeople::TakeJob(int32 I)
{
	FWho* W = Find(MenuWho.Get()); UADPSim* Sim = UADPSim::Get(GetWorld());
	if (!W || !Sim || !Sim->IsRunning()) { Close(); return; }
	const int32 On = FMath::Clamp(Jobs.Num() - JobPage * 6, 0, 6);
	if (I >= On)
	{
		if (Jobs.Num() > 6 && I == On) { JobPage = (JobPage + 1) % FMath::DivideAndRoundUp(Jobs.Num(), 6); ShowJobs(); }
		else ShowStart();
		return;
	}
	const FString Job = Jobs[JobPage * 6 + I]; FString Name;
	Mode = EMenu::Answer;
	const bool bOk = ADPCrew::HireTalk(Sim->World, Job, W->First, W->bFemale, W->Age, W->Name, Sim->Now(), Name);
	MenuText = bOk ? FString::Printf(TEXT("\"Thank you! See you tomorrow.\"\n\n(%s is on your staff: %s, the first week's pay taken. Their shift and wage are under Crew in the Overview.)"), *Name, *ADPCrew::RoleName(Job)) : FString(TEXT("(Not hired.)"));
	if (bOk) Say(W->A.Get(), TEXT("Thank you! See you tomorrow."), ETone::Mild);
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
	case EMenu::Job: TakeJob(I); break;
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

// ---- strolling passengers (owner, 2026-11-19, for now - until their trips and animations are done): a group only in the
// Grand Hall, a group only on Floor 2 (the gate corridor up and down, the tax free, the gates and their seats, the
// benches); some lean on a wall, some sit. Never out of their own rooms: no stairs, no other floor, no jetway, never
// through a door (doors open only for you, not for them), and every goal and every straight way to it checked first.
void AADPPeople::StartWanderers()
{
	if (bStrollersStarted) return;
	bStrollersStarted = true;
	APaxFlow::Get(GetWorld());   // (the departing and arriving passengers' trips - PaxFlow.h - start with the airport)
	// (owner 2026-11-23: no flight, no passengers - nobody strolls about the terminal who isn't flying; -Strollers: tests)
	if (!FParse::Param(FCommandLine::Get(), TEXT("Strollers"))) return;
	FTerminalPlan Plan; if (!Plan.Load()) { ADP_WARN(TEXT("People: no terminal plan - no strolling passengers")); return; }
	auto Boxes = [&](TFunctionRef<bool(const FPlanRoom&)> Pick, TArray<FBox2D>& Out, TArray<FString>& Ids)
	{
		for (const FPlanRoom& Rm : Plan.Rooms)
			if (Pick(Rm))
			{
				const FVector A = FTerminalPlan::ToWorld(Rm.X0 + 1.2f, 0.f, Rm.Z0 + 1.2f), B = FTerminalPlan::ToWorld(Rm.X1 - 1.2f, 0.f, Rm.Z1 - 1.2f);
				FBox2D Bx(ForceInit); Bx += FVector2D(A.X, A.Y); Bx += FVector2D(B.X, B.Y); Out.Add(Bx); Ids.Add(Rm.Id);
			}
	};
	TArray<FBox2D> Hall, F2; TArray<FString> HallIds, F2Ids;
	Boxes([](const FPlanRoom& Rm) { return Rm.Id == TEXT("F1-HALL"); }, Hall, HallIds);
	Boxes([](const FPlanRoom& Rm) { return Rm.Id.StartsWith(TEXT("F2-")) && (Rm.Id == TEXT("F2-FOOD") || Rm.Id == TEXT("F2-LANDING") || Rm.Id.StartsWith(TEXT("F2-TAXFREE")) || Rm.Id == TEXT("F2-LINK") || Rm.Id == TEXT("F2-WALK") || Rm.Id.StartsWith(TEXT("F2-GATE-"))); }, F2, F2Ids);
	struct FGroup { const TCHAR* What; TArray<FBox2D>* Area; TArray<FString>* Ids; float FloorZ; TArray<EHabit> Habits; };
	const float Z1 = FTerminalPlan::ToWorld(0.f, Plan.FloorY(1), 0.f).Z, Z2 = FTerminalPlan::ToWorld(0.f, Plan.FloorY(2), 0.f).Z;
	const FGroup Groups[] = {
		{ TEXT("the Grand Hall"), &Hall, &HallIds, Z1, { EHabit::Hall, EHabit::Hall, EHabit::Hall, EHabit::Hall, EHabit::Hall } },
		{ TEXT("Floor 2"), &F2, &F2Ids, Z2, { EHabit::Corridor, EHabit::Corridor, EHabit::Shopper, EHabit::Shopper, EHabit::Gates, EHabit::Gates, EHabit::Bench, EHabit::Bench } } };
	TSet<FString> Used;
	for (const FGroup& G : Groups)
	{
		if (!G.Area->Num()) { ADP_WARN(TEXT("People: no rooms for %s"), G.What); continue; }
		int32 Made = 0;
		for (const EHabit Hb : G.Habits)
		{
			FStroll S; S.Area = *G.Area; S.AreaIds = *G.Ids; S.FloorZ = G.FloorZ; S.Habit = Hb;
			// (a start spot: floor at its own floor's height and room for a body there - in the habit's own rooms where it has
			// some: the corridor people in the corridor, the shoppers in the tax free, the gate people at a gate)
			FVector Spot; bool bOk = false;
			FCollisionQueryParams Qs(SCENE_QUERY_STAT(PeopleStart), false);
			for (const FStroll& O : Strollers) if (AActor* Oa = O.A.Get()) Qs.AddIgnoredActor(Oa);
			TArray<int32> Prefer;
			for (int32 i = 0; i < S.AreaIds.Num(); ++i)
			{
				const FString& Id = S.AreaIds[i];
				if ((Hb == EHabit::Corridor && Id == TEXT("F2-WALK")) || (Hb == EHabit::Shopper && Id.StartsWith(TEXT("F2-TAXFREE"))) || (Hb == EHabit::Gates && Id.StartsWith(TEXT("F2-GATE-"))) || (Hb == EHabit::Bench && (Id == TEXT("F2-FOOD") || Id == TEXT("F2-LANDING")))) Prefer.Add(i);
			}
			for (int32 Try = 0; Try < 40 && !bOk; ++Try)
			{
				const FBox2D& Bx = S.Area[Prefer.Num() ? Prefer[Rng.RandRange(0, Prefer.Num() - 1)] : Rng.RandRange(0, S.Area.Num() - 1)];
				const FVector P(Rng.FRandRange(Bx.Min.X, Bx.Max.X), Rng.FRandRange(Bx.Min.Y, Bx.Max.Y), S.FloorZ);
				FHitResult H;
				if (!GetWorld()->LineTraceSingleByChannel(H, P + FVector(0, 0, 150.f), P - FVector(0, 0, 150.f), ECC_Visibility, Qs)) continue;
				if (FMath::Abs(H.ImpactPoint.Z - S.FloorZ) > 40.f || H.ImpactNormal.Z < 0.9f) continue;
				if (GetWorld()->OverlapAnyTestByChannel(H.ImpactPoint + FVector(0, 0, 100.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(35.f, 70.f), Qs)) continue;
				Spot = H.ImpactPoint; bOk = true;
			}
			if (!bOk) continue;
			FString Name; for (int32 k = 0; k < 10; ++k) { Name = PickBuilt(TEXT("passenger")); if (!Used.Contains(Name)) break; }
			if (Name.IsEmpty()) break;
			Used.Add(Name);
			AActor* A = Spawn(Name, Spot, Rng.FRandRange(0.f, 360.f));
			if (!A) continue;
			S.A = A; S.For = Rng.FRandRange(0.5f, 4.f);
			Strollers.Add(S); ++Made;
		}
		ADP_LOG(TEXT("People: %d passengers about %s (only there)"), Made, G.What);
	}
}

bool AADPPeople::SitOn(AActor* Who, int32 Seat, const TArray<FBox2D>& Area, float FloorZ)
{
	FVector SAt, SF; float SH;
	if (!Who || !Find(Who) || !ASeats::SeatOf(Seat, SAt, SF, SH) || !ASeats::ClaimSeat(Who, Seat)) return false;   // (never on another's, nor yours)
	FStroll S; S.A = Who; S.Area = Area; S.FloorZ = FloorZ; S.Habit = EHabit::Bench; S.bHeld = true;
	for (int32 i = 0; i < Area.Num(); ++i) S.AreaIds.Add(TEXT("PAXFLOW"));
	S.Seat = Seat; SeatFor(S, SAt, SF, SH); S.To = FVector(SAt.X, SAt.Y, FloorZ) + SF * 60.f; S.Doing = EDoing::ToSeat; S.T = 0.f; S.SitPose = Rng.RandRange(0, 7);
	Strollers.Add(S);
	return true;
}

void AADPPeople::StandFrom(AActor* Who)
{
	for (FStroll& S : Strollers)
		if (S.bHeld && S.A.Get() == Who)
		{
			if (S.Doing == EDoing::ToSeat) { ASeats::Release(Who); S.bRelease = true; }   // (not sat yet: free at once)
			else { S.bStandSoon = true; if (S.Doing == EDoing::Sit) S.For = 0.f; }
		}
}

bool AADPPeople::IsHeld(AActor* Who) const
{
	for (const FStroll& S : Strollers) if (S.bHeld && !S.bRelease && S.A.Get() == Who) return true;
	return false;
}

bool AADPPeople::StrollerDoing(int32 Kind, FVector& At, FVector& Facing) const
{
	for (const FStroll& S : Strollers)
		if (AActor* A = S.A.Get())
			if ((Kind == 0 && S.Doing == EDoing::Sit) || (Kind == 1 && S.Doing == EDoing::Lean && S.T > 1.f) || (Kind == 2 && (S.Doing == EDoing::Stand || S.Doing == EDoing::Walk)))
			{
				At = A->GetActorLocation();
				Facing = Kind == 1 ? FRotator(0.f, S.LeanYaw, 0.f).Vector() : FRotator(0.f, A->GetActorRotation().Yaw + 90.f, 0.f).Vector();
				FVector SAt, SF; float SH; if (Kind == 0 && ASeats::SeatOf(S.Seat, SAt, SF, SH)) { At = SAt; Facing = SF; }
				return true;
			}
	return false;
}

bool AADPPeople::InArea(const FStroll& S, const FVector& P) const
{
	for (const FBox2D& B : S.Area) if (B.IsInside(FVector2D(P.X, P.Y))) return true;
	return false;
}

bool AADPPeople::StrollSpot(const FStroll& S, const FVector& From, FVector& Out, int32 Box)
{
	// a spot in its own rooms (Box: that room only) with floor at its own floor's height (not a stair, not the floor
	// below), a clear straight way there from where it stands (no walls, desks, seats or people) that stays in its rooms,
	// and not right on top of another stroller's goal
	UWorld* W = GetWorld();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(PeopleStroll), false);
	for (const FStroll& O : Strollers) if (AActor* A = O.A.Get()) Q.AddIgnoredActor(A);
	for (int32 Try = 0; Try < 10; ++Try)
	{
		const FBox2D& Bx = S.Area[Box != INDEX_NONE ? Box : Rng.RandRange(0, S.Area.Num() - 1)];
		const FVector P(Rng.FRandRange(Bx.Min.X, Bx.Max.X), Rng.FRandRange(Bx.Min.Y, Bx.Max.Y), S.FloorZ);
		FHitResult H;
		if (!W->LineTraceSingleByChannel(H, P + FVector(0, 0, 150.f), P - FVector(0, 0, 150.f), ECC_Visibility, Q)) continue;
		if (FMath::Abs(H.ImpactPoint.Z - S.FloorZ) > 40.f || H.ImpactNormal.Z < 0.9f) continue;
		const FVector Goal(P.X, P.Y, H.ImpactPoint.Z);
		bool bNear = false; for (const FStroll& O : Strollers) bNear |= O.Doing == EDoing::Walk && FVector::Dist2D(O.To, Goal) < 150.f;
		if (bNear) continue;
		// the way there stays in its rooms (checked every metre) and is clear for a body
		bool bIn = true; const float L = FVector::Dist2D(From, Goal);
		for (float d = 0.f; d < L && bIn; d += 100.f) bIn = InArea(S, FMath::Lerp(From, Goal, d / FMath::Max(L, 1.f)));
		if (!bIn) continue;
		const FVector A0(From.X, From.Y, From.Z + 100.f), A1(Goal.X, Goal.Y, Goal.Z + 100.f);
		if (L > 30.f && W->SweepTestByChannel(A0, A1, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(32.f, 60.f), Q)) continue;
		Out = Goal; return true;
	}
	return false;
}

bool AADPPeople::WallSpot(const FStroll& S, const FVector& From, FVector& Out, float& Yaw)
{
	// a wall within 6 m that it can walk straight up to: the spot just in front of it, its back to the wall
	UWorld* W = GetWorld(); FCollisionQueryParams Q(SCENE_QUERY_STAT(PeopleLean), false);
	for (const FStroll& O : Strollers) if (AActor* A = O.A.Get()) Q.AddIgnoredActor(A);
	const float Start = Rng.FRandRange(0.f, 360.f);
	for (int32 k = 0; k < 12; ++k)
	{
		const FVector Dir = FRotator(0.f, Start + k * 30.f, 0.f).Vector();
		const FVector A0 = From + FVector(0, 0, 110.f);
		FHitResult H;
		if (!W->LineTraceSingleByChannel(H, A0, A0 + Dir * 600.f, ECC_Visibility, Q)) continue;
		if (FMath::Abs(H.ImpactNormal.Z) > 0.2f || H.Distance < 80.f) continue;
		const FVector N = FVector(H.ImpactNormal.X, H.ImpactNormal.Y, 0.f).GetSafeNormal();
		const FVector Spot = FVector(H.ImpactPoint.X, H.ImpactPoint.Y, From.Z) + N * 18.f;   // (its back right against the wall)
		if (!InArea(S, Spot)) continue;
		FHitResult Fl;
		if (!W->LineTraceSingleByChannel(Fl, Spot + FVector(0, 0, 150.f), Spot - FVector(0, 0, 150.f), ECC_Visibility, Q) || FMath::Abs(Fl.ImpactPoint.Z - S.FloorZ) > 40.f) continue;
		if (W->SweepTestByChannel(A0, FVector(Spot.X, Spot.Y, A0.Z), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(28.f, 55.f), Q)) continue;
		Out = FVector(Spot.X, Spot.Y, Fl.ImpactPoint.Z); Yaw = N.Rotation().Yaw;
		return true;
	}
	return false;
}

void AADPPeople::NextThing(FStroll& S)
{
	// what it does next, by its habit: mostly strolling, now and then leaning on a wall a while or sitting down
	AActor* A = S.A.Get(); if (!A) return;
	const FVector From = A->GetActorLocation();
	const float R = Rng.FRand();
	// a seat near (a bench, a gate's seats): for the bench and gate people often, the others now and then
	const float SitChance = S.Habit == EHabit::Bench ? 0.65f : S.Habit == EHabit::Gates ? 0.5f : 0.12f;
	if (R < SitChance && ASeats::Count() > 0)
	{
		// (its own search: ASeats::NearestTo is the seat in front of YOU, within arm's reach)
		TArray<int32> Free;
		for (int32 i = 0; i < ASeats::Count(); ++i)
		{
			FVector SAt, SF; float SH;
			if (!ASeats::SeatOf(i, SAt, SF, SH) || ASeats::IsTaken(i)) continue;
			if (FMath::Abs(SAt.Z - SH - S.FloorZ) > 60.f || FVector::Dist2D(SAt, From) > 2000.f || SH > 55.f) continue;   // (no bar stools: too high to sit down on naturally)
			const FVector Front = FVector(SAt.X, SAt.Y, S.FloorZ) + SF * 60.f;
			if (!InArea(S, Front)) continue;
			if (S.Habit == EHabit::Gates)
			{
				bool bGate = false;
				for (int32 k = 0; k < S.AreaIds.Num(); ++k) bGate |= S.AreaIds[k].StartsWith(TEXT("F2-GATE-")) && S.Area[k].IsInside(FVector2D(SAt.X, SAt.Y));
				if (!bGate && Rng.FRand() < 0.8f) continue;
			}
			Free.Add(i);
		}
		for (int32 Try = 0; Try < 6 && Free.Num(); ++Try)
		{
			int32 I = Free[Rng.RandRange(0, Free.Num() - 1)];
			if (Try < 3) if (const int32 In = ASeats::Innermost(I); Free.Contains(In)) I = In;   // (a row against a wall: its inner end first)
			FVector SAt, SF; float SH; ASeats::SeatOf(I, SAt, SF, SH);
			const FVector Front = FVector(SAt.X, SAt.Y, S.FloorZ) + SF * 60.f;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(PeopleSeat), false, A);
			bool bIn = true; const float L = FVector::Dist2D(From, Front);
			for (float d = 0.f; d < L && bIn; d += 100.f) bIn = InArea(S, FMath::Lerp(From, Front, d / FMath::Max(L, 1.f)));
			if (!bIn || GetWorld()->SweepTestByChannel(From + FVector(0, 0, 100.f), Front + FVector(0, 0, 100.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(28.f, 55.f), Q)) continue;
			if (ASeats::ClaimSeat(A, I)) { S.Seat = I; S.KeptSeat = INDEX_NONE; SeatFor(S, SAt, SF, SH); S.To = Front; S.Doing = EDoing::ToSeat; S.T = 0.f; S.SitPose = Rng.RandRange(0, 7); S.CrossLift = 0.f; static bool bCrossTest = FParse::Param(FCommandLine::Get(), TEXT("UICrossLegs")); if (bCrossTest) S.SitPose = 8; return; }
		}
	}
	if (R > 0.82f) { FVector Spot; float Yaw; if (WallSpot(S, From, Spot, Yaw)) { S.To = Spot; S.LeanYaw = Yaw; S.Doing = EDoing::Lean; S.T = -1.f; S.For = Rng.FRandRange(15.f, 45.f); return; } }
	// a stroll: the corridor people far along the corridor, the shoppers into the tax free, the gate people to a gate
	int32 Box = INDEX_NONE;
	for (int32 i = 0; i < S.AreaIds.Num(); ++i)
	{
		const FString& Id = S.AreaIds[i];
		if ((S.Habit == EHabit::Corridor && Id == TEXT("F2-WALK")) || (S.Habit == EHabit::Shopper && Id.StartsWith(TEXT("F2-TAXFREE")) && Rng.FRand() < 0.7f) || (S.Habit == EHabit::Gates && Id.StartsWith(TEXT("F2-GATE-")) && Rng.FRand() < 0.3f)) { Box = i; break; }
	}
	FVector To;
	if (StrollSpot(S, From, To, Box) || StrollSpot(S, From, To)) { S.To = To; S.Doing = EDoing::Walk; return; }
	S.Doing = EDoing::Stand; S.For = 1.f; S.T = 0.f;
}

bool AADPPeople::SeatFor(FStroll& S, FVector& At, FVector& Facing, float& SeatH)
{
	// (the seat taken, not whatever has its number now: the list of seats is gathered again as models come and go)
	if (S.KeptSeat == S.Seat && S.Seat != INDEX_NONE)
	{
		if (!ASeats::SeatOf(S.Seat, At, Facing, SeatH) || FVector::Dist(At, S.KeptAt) > 40.f || FVector::DotProduct(Facing, S.KeptF) < 0.5f)
		{
			static int32 Said = 0; if (Said++ < 4) ADP_LOG(TEXT("People: seat %d is another one in the list now - the one taken is kept"), S.Seat);
			At = S.KeptAt; Facing = S.KeptF; SeatH = S.KeptH; return true;
		}
	}
	else if (!ASeats::SeatOf(S.Seat, At, Facing, SeatH)) return false;
	S.KeptSeat = S.Seat; S.KeptAt = At; S.KeptF = Facing; S.KeptH = SeatH;
	return true;
}

void AADPPeople::SitPose(FStroll& S, float K)
{
	// its own body posed on the seat (the same rig and pose as yours when you sit - Seats.cpp), K 0 standing .. 1 sat
	AActor* A = S.A.Get(); FWho* W = A ? Find(A) : nullptr;
	FVector SAt, SF; float SH;
	if (!W || !W->Body || !SeatFor(S, SAt, SF, SH)) return;
	if (!S.Rig)
	{
		S.Rig = new FTruckDriverRig();
		S.Rider = NewObject<UPoseableMeshComponent>(this); S.Rider->SetupAttachment(RootComponent); S.Rider->SetVisibility(false); S.Rider->RegisterComponent();
		S.Frame = NewObject<USceneComponent>(this); S.Frame->SetupAttachment(RootComponent); S.Frame->RegisterComponent();
	}
	if (!S.Pose)
	{
		S.Rider->SetSkinnedAssetAndUpdate(W->Body->GetSkeletalMeshAsset());
		if (!S.Rig->Init(S.Rider)) return;

		S.BodyInActor = W->Body->GetComponentTransform().GetRelativeTransform(A->GetActorTransform());
		// (the cause of to-do 233: the body took its frame's turn here, before the frame was put at the seat - a new frame,
		// unturned - so everyone faced the world's +Y, on nearly all the rows' seats right at the backrest:
		// Tools/check_seat_facing.py. The frame goes to the seat first, then the body)
		S.Frame->SetWorldLocationAndRotation(SAt - SF.GetSafeNormal2D() * (20.f - S.Fwd) - FVector(0, 0, SH), FRotator(0.f, SF.Rotation().Yaw - 90.f, 0.f));
		A->SetActorTransform(S.BodyInActor.Inverse() * S.Frame->GetComponentTransform());
		W->Body->SetAnimInstanceClass(UDriverPoseAnim::StaticClass());
		S.Pose = Cast<UDriverPoseAnim>(W->Body->GetAnimInstance());
	}
	// (the frame: 20 cm back into the seat, less the forward step the cushion check below asked for)
	S.Frame->SetWorldLocationAndRotation(SAt - SF.GetSafeNormal2D() * (20.f - S.Fwd) - FVector(0, 0, SH), FRotator(0.f, SF.Rotation().Yaw - 90.f, 0.f));
	const float PH = S.Rig->PelvisHeight(), E = FMath::SmoothStep(0.f, 1.f, K);
	// (left and right: on these bodies side 0 is their right, so its foot and hand go on the frame's +x - with -x they
	// crossed their legs and arms, owner 2026-11-19; the natural pose itself follows his photo)
	FDriverPose St; St.Facing = FVector(0, 1, 0); St.Pelvis = FVector(0.f, 30.f, PH); St.Foot[0] = FVector(12.f, 32.f, 0.f); St.Foot[1] = FVector(-12.f, 32.f, 0.f);
	// (his photo, Art/Reference/From-Phone/2026-11-19/sitting-pose-reference.jpg: upright, leaning a little forward; the
	// knees about hip-wide, the feet planted a bit wider and a little forward; the forearms on the thighs, both hands
	// resting together over the left knee)
	// (the seat's point is near its front edge: the whole frame goes 20 cm back into the seat - the rig keeps the hips over
	// the frame's own point - so they sit on it, not perched on its edge)
	// (owner: different poses so it always looks natural - each time it sits one of four, the hands always resting on
	// the thighs, knees or lap, never through the body; the feet apart or closer together)
	FDriverPose Si; Si.Facing = FVector(0, 1, 0); Si.Pelvis = FVector(0.f, -4.f, SH + 9.f);
	const bool bNarrow = (S.SitPose & 4) != 0;
	Si.Foot[0] = FVector(bNarrow ? 13.f : 23.f, bNarrow ? 44.f : 40.f, 0.f); Si.Foot[1] = FVector(bNarrow ? -13.f : -23.f, bNarrow ? 44.f : 40.f, 0.f);
	Si.bHand[0] = Si.bHand[1] = true; Si.Grip[0] = Si.Grip[1] = 0.15f;
	switch (S.SitPose & 3)
	{
	case 0: Si.Lean = 4.f; Si.Hand[0] = FVector(-10.f, 30.f, SH + 15.f); Si.Hand[1] = FVector(-19.f, 33.f, SH + 13.f); break;   // his photo: both hands on the left knee
	case 1: Si.Lean = 4.f; Si.Hand[0] = FVector(19.f, 33.f, SH + 13.f); Si.Hand[1] = FVector(10.f, 30.f, SH + 15.f); break;     // the same on the right knee
	case 2: Si.Lean = 2.f; Si.Hand[0] = FVector(13.f, 22.f, SH + 14.f); Si.Hand[1] = FVector(-13.f, 22.f, SH + 14.f); break;    // each hand on its own thigh
	default: Si.Lean = -7.f; Si.Hand[0] = FVector(4.f, 16.f, SH + 16.f); Si.Hand[1] = FVector(-4.f, 18.f, SH + 15.f); break;    // back in the seat, hands in the lap
	}
	// legs crossed (owner, 2026-11-19 - shown first in the -UICrossLegs test): the left leg over the right, its foot off the
	// floor beside the other shin, both hands on the top knee; the thighs kept apart by the IK step below
	const bool bCross = S.SitPose == 8;
	if (bCross)
	{
		Si.Lean = 2.f;
		Si.Foot[0] = FVector(12.f, 40.f, 0.f);              // (the right foot planted)
		Si.Foot[1] = FVector(14.f, 50.f, 30.f + S.CrossLift);   // (the left foot across, lifted - more if the thighs met)
		Si.Hand[0] = FVector(2.f, 34.f, SH + 26.f); Si.Hand[1] = FVector(-6.f, 36.f, SH + 24.f);
	}
	// the seat (owner, 2026-11-19: a small IK step): its real surface measured once, straight down at the seat, and the
	// hips one thigh's thickness over it - so the thighs rest on the seat whatever the body
	const float ThighR = S.Rig->ThighRadius();
	if (!S.bSurf)
	{
		S.bSurf = true; S.SurfZ = SH;
		FHitResult Hs; FCollisionQueryParams Qs(SCENE_QUERY_STAT(PeopleSeatTop), false, A);
		const FVector Mid = SAt - SF.GetSafeNormal2D() * 20.f;
		if (GetWorld()->LineTraceSingleByChannel(Hs, Mid + FVector(0, 0, 80.f), Mid - FVector(0, 0, 60.f), ECC_Visibility, Qs)) S.SurfZ = Hs.ImpactPoint.Z - S.Frame->GetComponentLocation().Z;
	}
	Si.Pelvis.Z = S.SurfZ + ThighR + S.Lift;
	FDriverPose P = Si;
	P.Pelvis = FMath::Lerp(St.Pelvis, Si.Pelvis, E); P.Lean = FMath::Lerp(0.f, Si.Lean, E) + 22.f * FMath::Sin(PI * E);
	for (int32 s = 0; s < 2; ++s) { P.Foot[s] = FMath::Lerp(St.Foot[s], Si.Foot[s], E); P.bHand[s] = E > 0.35f; }
	// a seated idle movement (SeatFidget), blended in and out: a look round (the head, the neck, a little of the spine),
	// a neck roll, or the phone - both hands brought together in front at chest height, the head down at them
	float PhoneW = 0.f;
	// (a phone or tablet held for a long while - APaxProps, SeatDevice: the same hold as the phone movement, the tablet's
	// hands wider apart at its sides, blended by DevW)
	auto HoldDevice = [&](float Ew, float Half)
	{
		PhoneW = Ew;
		const float Cz = P.Pelvis.Z + 30.f;
		for (int32 s = 0; s < 2; ++s)
		{
			const FVector Ph(s == 0 ? Half : -Half, Half > 6.f ? 30.f : 28.f, Cz);
			P.Hand[s] = FMath::Lerp(Si.Hand[s], Ph, Ew); P.Grip[s] = FMath::Lerp(Si.Grip[s], 0.45f, Ew);
			P.PalmDir[s] = FVector(-0.6f, 0.f, 0.8f); P.FingerDir[s] = FVector(-0.3f, 0.8f, 0.5f); P.PalmW[s] = Ew;
		}
		P.LookHand = 0; P.LookW = Ew; P.HeadPitch = 6.f * Ew;
	};
	if (S.DevW > 0.f && E >= 1.f) HoldDevice(FMath::SmoothStep(0.f, 1.f, S.DevW), S.DevKind == 2 ? 12.f : 4.5f);
	else if (S.FidK >= 0 && E >= 1.f && S.FidDur > 0.f)
	{
		const float Kt = FMath::Clamp(S.FidT / S.FidDur, 0.f, 1.f);
		const float Ew = FMath::SmoothStep(0.f, 0.2f, Kt) * (1.f - FMath::SmoothStep(0.8f, 1.f, Kt));
		switch (S.FidK)
		{
		case 0: P.HeadYaw = 45.f * S.FidSide * FMath::Sin(2.f * PI * Kt) * Ew; P.Twist = 6.f * S.FidSide * FMath::Sin(2.f * PI * Kt) * Ew; break;
		case 1: { const float Th = 2.f * PI * 1.25f * Kt * S.FidSide; P.HeadRoll = 18.f * FMath::Sin(Th) * Ew; P.HeadPitch = (6.f + 9.f * FMath::Cos(Th)) * Ew; break; }
		default:
		{
			PhoneW = Ew;
			const float Cz = P.Pelvis.Z + 30.f;
			for (int32 s = 0; s < 2; ++s)
			{
				// (side 0 is on the frame's +x on these bodies - see above)
				const FVector Ph(s == 0 ? 4.5f : -4.5f, 28.f, Cz);
				P.Hand[s] = FMath::Lerp(Si.Hand[s], Ph, Ew); P.Grip[s] = FMath::Lerp(Si.Grip[s], 0.45f, Ew);
				P.PalmDir[s] = FVector(-0.6f, 0.f, 0.8f); P.FingerDir[s] = FVector(-0.3f, 0.8f, 0.5f); P.PalmW[s] = Ew;
			}
			P.LookHand = 0; P.LookW = Ew; P.HeadPitch = 6.f * Ew;
			break;
		}
		}
	}
	S.Rig->Pose(P);
	// (crossed legs: the top thigh must lie on the other one, never in it - the gap between the two thighs' middle lines
	// measured; if it is less than two thighs' thickness the crossing foot (and so its knee) goes up, a few passes)
	if (bCross && E >= 1.f)
	{
		auto SegDist = [](const FVector& A0, const FVector& A1, const FVector& B0, const FVector& B1)
		{
			float Best = 1e9f;
			for (int32 i = 0; i <= 12; ++i) { const FVector Pa = FMath::Lerp(A0, A1, i / 12.f); Best = FMath::Min(Best, FMath::PointDistToSegment(Pa, B0, B1)); }
			return Best;
		};
		for (int32 Pass = 0; Pass < 5; ++Pass)
		{
			const float D = SegDist(S.Rig->HipAt(1), S.Rig->KneeAt(1), S.Rig->HipAt(0), S.Rig->KneeAt(0)), Need = 2.f * ThighR * 0.95f;
			if (D >= Need) break;
			S.CrossLift += (Need - D) * 1.8f; P.Foot[1].Z += (Need - D) * 1.8f;
			S.Rig->Pose(P);
		}
		static int32 Said = 0; if (Said++ < 2) ADP_LOG(TEXT("People: legs crossed - the thighs %.1f cm apart (they need %.1f), the crossing foot lifted %.0f cm"), SegDist(S.Rig->HipAt(1), S.Rig->KneeAt(1), S.Rig->HipAt(0), S.Rig->KneeAt(0)), 2.f * ThighR * 0.95f, 30.f + S.CrossLift);
	}
	// the hands (the same IK step): each one onto the top of the thigh it rests on - its own body's thigh, at its own
	// thickness - so it lies on it, never in it or floating over it; then the pose once more with them there
	// the seat under the thighs (the IK step): sat, the thighs' undersides checked against the cushion from hip to knee;
	// where they go into it (its raised front) the body comes forward a little at a time, up to 12 cm, then up by the rest
	if (E >= 1.f && !S.bCushionOk)
	{
		const float Deep = S.Rig->ThighsInSeat(GetWorld(), S.Frame->GetComponentTransform(), ThighR, A);
		if (Deep <= 0.4f || S.CushionTries > 12) { S.bCushionOk = true; static int32 Said = 0; if (Said++ < 4) ADP_LOG(TEXT("People: on the cushion - forward %.0f cm, up %.1f cm (thighs %.1f cm into it)"), S.Fwd, S.Lift, Deep); }
		else { ++S.CushionTries; if (S.Fwd < 12.f) S.Fwd += 2.f; else if (S.Lift < 8.f) S.Lift = FMath::Min(8.f, S.Lift + Deep); else { S.bCushionOk = true; static int32 SaidCap = 0; if (SaidCap++ < 4) ADP_LOG(TEXT("People: on the cushion (limits) - forward %.0f cm, up %.1f cm, thighs still %.1f cm into it"), S.Fwd, S.Lift, Deep); } }
	}
	if (E > 0.35f) S.Rig->RestHandsOnThighs(P, ThighR, FMath::Clamp((E - 0.35f) / 0.3f, 0.f, 1.f) * (1.f - PhoneW));   // (the phone held: off the thighs)
	if (S.Pose) S.Pose->Local = S.Rig->LocalPose();
	// (the body on the seat's frame - the hips at the rig's own height over the seat)
	{
		const FTransform Base = S.BodyInActor.Inverse() * S.Frame->GetComponentTransform();
		// (measured once: a seat's point is not on its surface - the rig's own height is right; no lowering)
		S.Lower = 0.f;
		// (its turn too, each time: it was set once only, so whoever was turned or given another seat after that sat the wrong way round)
		A->SetActorLocationAndRotation(Base.GetLocation() - FVector(0, 0, S.Lower * E), Base.GetRotation());
	}
	// (the facing proved in the log, once per kind of seat: the way the thighs really point against the measured backrest)
	if (K >= 1.f) ASeats::LogFacing(TEXT("AI"), S.Seat, SAt, SF, SH, W->Body->GetComponentTransform().TransformVector(S.Rig->KneeAt(0) + S.Rig->KneeAt(1) - S.Rig->HipAt(0) - S.Rig->HipAt(1)), A);
	if (K >= 1.f) { static int32 Said = 0; if (Said++ < 4) { const FVector Pv = W->Body->GetBoneLocation(TEXT("pelvis")); const FVector Rel = FRotationMatrix(SF.Rotation()).InverseTransformVector(Pv - SAt); ADP_LOG(TEXT("People: sat - the hips %.0f cm ahead of the seat's middle, %.0f to its side, %.0f over it (seat %.0f high)"), Rel.X, Rel.Y, Rel.Z, SH); } }
}

void AADPPeople::SeatDevice(AActor* Who, int32 Kind)
{
	for (FStroll& S : Strollers) if (S.A.Get() == Who) { S.Device = Kind; if (Kind > 0) S.DevKind = Kind; return; }
}

int32 AADPPeople::SeatDeviceNow(const AActor* Who) const
{
	for (const FStroll& S : Strollers)
		if (S.A.Get() == Who) return S.DevW > 0.f ? S.DevKind : (S.Doing == EDoing::Sit && S.FidK == 2) ? 1 : 0;
	return 0;
}

void AADPPeople::SeatFidget(FStroll& S, float Dt)
{
	// the seated idle movements (owner 2026-11-21: only the subtle ones sat down - a look round, the phone, the neck): after
	// 30 s sat, now and then one (6-20 s apart, 3.5-6 s each, never the same twice running), only within 60 m of the
	// camera (posing a seated body costs a little; far off nobody sees it)
	const AActor* A = S.A.Get(); if (!A) return;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const bool bNear = PC && PC->PlayerCameraManager && FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), A->GetActorLocation()) < FMath::Square(6000.f);
	if (S.FidK >= 0)
	{
		S.FidT += Dt;
		const bool bEnd = S.FidT >= S.FidDur || !bNear;
		if (bEnd) { S.FidK = -1; S.FidNext = Rng.FRandRange(6.f, 20.f); }
		SitPose(S, 1.f);   // (the last call, with no movement, leaves the plain sitting pose)
		return;
	}
	if (!bNear || S.T < 30.f || (S.FidNext -= Dt) > 0.f) return;
	int32 K = Rng.RandRange(0, 2); if (K == S.FidLast) K = (K + 1 + Rng.RandRange(0, 1)) % 3;
	S.FidK = S.FidLast = K; S.FidT = 0.f; S.FidDur = Rng.FRandRange(3.5f, 6.f); S.FidSide = Rng.FRand() < 0.5f ? 1.f : -1.f;
}

void AADPPeople::StepStrollers(float Dt)
{
	// (PaxFlow's passengers handed back: their rig and pose parts go)
	for (int32 i = Strollers.Num() - 1; i >= 0; --i)
		if (Strollers[i].bRelease || (Strollers[i].bHeld && !Strollers[i].A.IsValid()))
		{
			FStroll& S = Strollers[i];
			if (!S.A.IsValid()) ASeats::Release(nullptr);
			delete S.Rig; S.Rig = nullptr;
			if (S.Rider) S.Rider->DestroyComponent();
			if (S.Frame) S.Frame->DestroyComponent();
			Strollers.RemoveAt(i);
		}
	if (!Strollers.Num()) return;
	ADP_PERF_SCOPE("people: strolling passengers (each frame)", 1.f);
	{ static double SaidAt = 0.0; const double Tn = FPlatformTime::Seconds(); if (Tn - SaidAt > 15.0) { SaidAt = Tn; int32 N[7] = {}; for (const FStroll& S : Strollers) ++N[(int32)S.Doing]; ADP_LOG(TEXT("People strolling: %d standing, %d walking, %d leaning, %d to a seat, %d sitting"), N[0], N[1], N[2], N[3], N[4] + N[5] + N[6]); } }
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector Me = PC && PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector(1e9f);
	for (FStroll& S : Strollers)
	{
		AActor* A = S.A.Get(); if (!A) continue;
		FWho* W = Find(A); if (!W) continue;
		S.T += Dt;
		switch (S.Doing)
		{
		case EDoing::Stand:
			WalkTo(W, A->GetActorLocation(), Dt, 50.f);   // (the walk eases out to the idle)
			if (S.T >= S.For) NextThing(S);
			break;
		case EDoing::Walk:
		case EDoing::ToSeat:
		case EDoing::Lean:
		{
			if (S.Doing == EDoing::Lean && S.T >= 0.f)
			{
				// leaning on the wall: its back to it, tilted a little back against it, a while
				WalkTo(W, A->GetActorLocation(), Dt, 50.f);
				A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, S.LeanYaw - 90.f, -5.f), Dt, 3.f));
				if (S.T >= S.For) { A->SetActorRotation(FRotator(0.f, S.LeanYaw - 90.f, 0.f)); S.Doing = EDoing::Stand; S.T = 0.f; S.For = Rng.FRandRange(1.f, 4.f); }
				break;
			}
			// the seat it walks to became yours (to-do 293): another one, or handed back to PaxFlow on its feet
			if (S.Doing == EDoing::ToSeat)
			{
				FVector SAt, SF; float SH;
				if (!SeatFor(S, SAt, SF, SH) || !ASeats::HoldsAt(A, SAt))
				{
					ASeats::Release(A); ADP_LOG(TEXT("People: %s was walking to seat %d - it is taken now, another one"), *W->Name, S.Seat);
					S.Seat = S.KeptSeat = INDEX_NONE;
					if (S.bHeld) S.bRelease = true; else { S.Doing = EDoing::Stand; S.T = 0.f; S.For = Rng.FRandRange(0.4f, 1.2f); }
					break;
				}
			}
			// you in its way, close ahead: it stops and waits for you
			const FVector Ahead = (S.To - A->GetActorLocation()).GetSafeNormal2D(), ToMe = Me - A->GetActorLocation();
			if (ToMe.Size2D() < 110.f && FVector::DotProduct(ToMe.GetSafeNormal2D(), Ahead) > 0.5f) { WalkTo(W, A->GetActorLocation(), Dt, 50.f); break; }
			if (WalkTo(W, S.To, Dt, 40.f))
			{
				if (S.Doing == EDoing::Lean) S.T = 0.f;
				else if (S.Doing == EDoing::ToSeat) { S.Doing = EDoing::SitDown; S.T = 0.f; ASeats::Sat(A); }
				else { S.Doing = EDoing::Stand; S.T = 0.f; S.For = Rng.FRandRange(2.f, 8.f); }
			}
			// (never out of its rooms, whatever happens)
			if (!InArea(S, A->GetActorLocation()) && S.Doing != EDoing::SitDown)
			{
				float Bd = 1e12f; FVector Best = A->GetActorLocation();
				for (const FBox2D& B : S.Area) { const FVector2D C = B.GetClosestPointTo(FVector2D(A->GetActorLocation())); const float D = FVector2D::DistSquared(C, FVector2D(A->GetActorLocation())); if (D < Bd) { Bd = D; Best = FVector(C.X, C.Y, A->GetActorLocation().Z); } }
				A->SetActorLocation(Best); S.Doing = EDoing::Stand; S.T = 0.f; S.For = 1.f;
			}
			break;
		}
		case EDoing::SitDown: SitPose(S, FMath::Min(1.f, S.T / 0.9f)); if (S.T >= 0.9f) { S.Doing = EDoing::Sit; S.T = 0.f; S.For = S.bHeld ? (S.bStandSoon ? 0.f : 1e9f) : Rng.FRandRange(30.f, 120.f); } break;
		case EDoing::Sit:
			if (!S.bCushionOk) SitPose(S, 1.f);   // (the cushion check settles over its first frames)
			else if (S.Device > 0 || S.DevW > 0.f)
			{
				// (a device held: no idle movements meanwhile; posed while the hands move, then left as it is)
				if (S.FidK >= 0) S.FidK = -1;
				const float Was = S.DevW; S.DevW = FMath::FInterpConstantTo(S.DevW, S.Device > 0 ? 1.f : 0.f, Dt, 1.f / 0.7f);
				if (S.DevW != Was || S.DevPosed != S.Device) { SitPose(S, 1.f); if (S.DevW == Was) S.DevPosed = S.Device; else S.DevPosed = -1; }
				if (S.DevW <= 0.f) { S.FidNext = Rng.FRandRange(6.f, 20.f); S.DevPosed = -1; }
			}
			else SeatFidget(S, Dt);
			if (S.T >= S.For) { if (S.FidK >= 0) { S.FidK = -1; SitPose(S, 1.f); } S.Doing = EDoing::StandUp; S.T = 0.f; }
			break;
		case EDoing::StandUp:
			SitPose(S, FMath::Max(0.f, 1.f - S.T / 0.9f));
			if (S.T >= 0.9f)
			{
				// back on its feet in front of the seat, walking again
				FVector SAt, SF; float SH;
				if (SeatFor(S, SAt, SF, SH))
				{
					// (its feet on the real floor in front of the seat, not the plan's height)
					FVector P = FVector(SAt.X, SAt.Y, S.FloorZ) + SF * 60.f; FHitResult Fl; FCollisionQueryParams Qf(SCENE_QUERY_STAT(PeopleUp), false, A);
					if (GetWorld()->LineTraceSingleByChannel(Fl, P + FVector(0, 0, 120.f), P - FVector(0, 0, 200.f), ECC_Visibility, Qf)) P.Z = Fl.ImpactPoint.Z;
					A->SetActorLocationAndRotation(P, FRotator(0.f, SF.Rotation().Yaw - 90.f, 0.f));
				}
				if (W->Body) { static UBlendSpace* Move = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/People/Anims/BS_Idle_Walk_Run_MH.BS_Idle_Walk_Run_MH")); W->Body->SetAnimationMode(EAnimationMode::AnimationSingleNode); if (Move) { W->Body->SetAnimation(Move); W->Body->Play(true); UPaxAnimInstance::Install(W->Body); } }
				if (S.bHeld) { ASeats::Release(A); S.bRelease = true; break; }   // (PaxFlow's passenger: handed back on its feet)
				ASeats::Release(A); S.Seat = S.KeptSeat = INDEX_NONE; S.Pose = nullptr; S.bLowered = false; S.Lower = 0.f; S.bSurf = false; S.Fwd = 0.f; S.Lift = 0.f; S.bCushionOk = false; S.CushionTries = 0;
				S.Doing = EDoing::Stand; S.T = 0.f; S.For = Rng.FRandRange(1.f, 3.f);
			}
			break;
		}
	}
}

void AADPPeople::Tick(float Dt)
{
	ADP_PERF_SCOPE("people: talk and conduct (each frame)", 2.f);
	Super::Tick(Dt);
	if ((GatherIn -= Dt) <= 0.f) { GatherIn = 10.f; ADP_PERF_SCOPE("people: finding the people in the world", 4.f); Gather(); }   // (people placed here list themselves at once)
	People.RemoveAll([](const FWho& W) { return !W.A.IsValid(); });
	ADPLayers::Tick(GetWorld(), Dt);
	if (SubT > 0.f) SubT -= Dt;
	if (SceneOn()) StepScene(Dt);
	StepStrollers(Dt);
	// the menu closes when you walk away from the person
	if (bMenu)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (!MenuWho.IsValid() || (PC && PC->GetPawn() && FVector::Dist2D(PC->GetPawn()->GetActorLocation(), MenuWho->GetActorLocation()) > 450.f)) Close();
	}
}
