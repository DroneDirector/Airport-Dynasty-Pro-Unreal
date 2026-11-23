// New game, Continue and Load (owner, to-do 32h): what the game needs is really loaded - you (your built character), your
// hair (its grooms), the terminal (its pieces and materials), the roads and the town, and your career - behind the neon
// loading screen, each one's true percentage in its bar; when they are in, straight into the airport (Loading.cpp).
// Since 2026-11-23 it is the game's own screen (SNeonLoading): the menus' page showed it before and had the last word on
// when loading was over - an error in the page, and it stayed at 92 % for ever. The page is no longer asked.
#include "UI/ADPHUD.h"
#include "ADPLog.h"
#include "Wardrobe.h"
#include "ADPGameMode.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Sim/ADPSim.h"

void AADPHUD::StartUnrealLoad()
{
	if (bPreLoading || bStartLoad) return;   // (already loading)
	ShowStartLoad();
	PreLoad.Reset(); PreNames.Reset(); PreSent = 0.f; bPreDone = false;
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FSoftObjectPath> You, Hair, Terminal;
	// you: your built character and everything it uses (its grooms counted as your hair)
	const FString Who = AWardrobe::Chosen();
	const FName YouPkg(*FString::Printf(TEXT("/Game/People/Built/%s/BP_%s"), *Who, *Who));
	TSet<FName> Seen; TArray<FName> Todo = {YouPkg};
	while (Todo.Num() && Seen.Num() < 4000)
	{
		const FName P = Todo.Pop();
		if (Seen.Contains(P)) continue;
		Seen.Add(P);
		TArray<FAssetData> In; AR.GetAssetsByPackageName(P, In);
		for (const FAssetData& A : In)
			(A.AssetClassPath.GetAssetName().ToString().Contains(TEXT("Groom")) ? Hair : You).Add(A.GetSoftObjectPath());
		TArray<FName> Deps; AR.GetDependencies(P, Deps);
		for (const FName& D : Deps) if (D.ToString().StartsWith(TEXT("/Game/"))) Todo.Add(D);
	}
	// the terminal: its pieces and their materials
	for (const TCHAR* Dir : {TEXT("/Game/Terminal/Meshes"), TEXT("/Game/Terminal/Materials")})
	{
		TArray<FAssetData> In; AR.GetAssetsByPath(FName(Dir), In, true);
		for (const FAssetData& A : In) Terminal.Add(A.GetSoftObjectPath());
	}
	FStreamableManager& SM = UAssetManager::GetStreamableManager();
	auto Start = [&](const TCHAR* Name, TArray<FSoftObjectPath>& L)
	{
		PreNames.Add(Name);
		PreLoad.Add(L.Num() ? SM.RequestAsyncLoad(L, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority) : nullptr);
		ADP_LOG(TEXT("New game loading: %s - %d assets"), Name, L.Num());
	};
	Start(TEXT("You"), You); Start(TEXT("Your hair"), Hair); Start(TEXT("The terminal"), Terminal);
	// the simulation (to-do 173): a new career at this airport, or yours picked up and caught up behind this screen
	if (UADPSim* Sim = UADPSim::Get(this))
	{
		if (bNewCareer) Sim->NewCareer(SimDifficulty);
		else if (!Sim->PendingLoad.IsEmpty() && Sim->LoadNamed(Sim->PendingLoad)) Sim->PendingLoad.Empty();   // (a named save: to-do 192)
		else if (Sim->IsRunning() && Sim->PendingLoad.IsEmpty()) {}   // (back from the title screen: it kept running)
		else if (!Sim->ResumeCareer()) Sim->NewCareer(TEXT("normal"));
	}
	// the roads, the town and the land along the roads, built behind this screen (ADPLateWorld)
	ADPLateWorld::Go();
	PreNames.Add(TEXT("The roads and the town")); PreLoad.Add(nullptr);
	bPreLoading = true;
}

void AADPHUD::TickUnrealLoad(float Dt)
{
	if (!bPreLoading) { if (bStartLoad) EndStartLoad(); return; }
	StartLoadT += Dt;
	FString Doing, All; float Sum = 0.f; bool bAll = true;
	for (int32 i = 0; i < PreNames.Num(); ++i)
	{
		const TSharedPtr<FStreamableHandle>& H = PreLoad[i];
		const bool bRoads = PreNames[i] == TEXT("The roads and the town");
		const float P = bRoads ? (ADPLateWorld::Done() ? 1.f : FMath::Min(0.999f, ADPLateWorld::Progress())) : !H.IsValid() || H->HasLoadCompleted() || H->WasCanceled() ? 1.f : FMath::Min(0.999f, H->GetProgress());
		bAll &= P >= 1.f; Sum += P;
		if (P < 1.f && Doing.IsEmpty()) Doing = FString::Printf(TEXT("Loading %s… %d %%"), *(PreNames[i].Left(1).ToLower() + PreNames[i].Mid(1)), FMath::FloorToInt(P * 100.f));
		All += FString::Printf(TEXT("%s%s %.0f%%"), i ? TEXT(", ") : TEXT(""), *PreNames[i], P * 100.f);
	}
	// (your career: its world data and the catch-up - never waited for long, it goes on behind the airport)
	const UADPSim* Sim = UADPSim::Get(this);
	const bool bSim = !Sim || Sim->IsRunning();
	if (!bSim) { if (StartLoadT < 45.f) bAll = false; if (Doing.IsEmpty()) Doing = TEXT("Loading your career…"); }
	Sum += bSim ? 1.f : 0.f;
	const float K = Sum / float(FMath::Max(1, PreNames.Num() + 1));
	LoadFrac = FMath::Max(LoadFrac, FMath::Min(0.5f * K, LoadFrac + Dt * 0.5f));
	LoadStep = Doing.IsEmpty() ? FString(TEXT("Loading game assets")) : Doing;
	static float LogT = 0.f; LogT += Dt; if (LogT >= 2.f || bAll) { LogT = 0.f; ADP_LOG(TEXT("New game loading: %s, your career %s (%.1f s)"), *All, bSim ? TEXT("in") : TEXT("coming"), StartLoadT); }
	// (the longest it may take: after 3 minutes in with what there is, and the log says what was missing)
	const bool bLate = StartLoadT > 180.f;
	if (!bAll && !bLate) return;
	if (bLate && !bAll) ADP_WARN(TEXT("New game loading: 180 s - going on without the rest (%s)"), *All);
	bPreLoading = false;
	ADP_LOG(TEXT("New game loading: everything Unreal needs is in (%.1f s)"), StartLoadT);
	EndStartLoad();
}
