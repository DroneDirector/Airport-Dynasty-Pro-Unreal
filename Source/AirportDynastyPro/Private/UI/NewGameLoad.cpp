// The new career's loading screen also shows what Unreal itself loads (owner, to-do 32h): while the neon loading screen
// is up after Start game, Unreal really loads what the game needs from it - you (your built character), your hair (its
// grooms) and the terminal (its pieces and materials) - and tells the screen each one's true percentage
// (window.__ueLoad), so the bar only finishes when they are in too.
#include "UI/ADPHUD.h"
#include "ADPLog.h"
#include "Wardrobe.h"
#include "ADPGameMode.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "SWebBrowser.h"
#include "Sim/ADPSim.h"

void AADPHUD::StartUnrealLoad()
{
	if (bPreLoading) return;   // (already loading: New game asks twice - from the page's screen and at Start game)
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
		else if (!Sim->ResumeCareer()) Sim->NewCareer(TEXT("normal"));
	}
	// the roads, the town and the land along the roads, built behind this screen (ADPLateWorld)
	ADPLateWorld::Go();
	PreNames.Add(TEXT("The roads and the town")); PreLoad.Add(nullptr);
	bPreLoading = true;
}

void AADPHUD::TickUnrealLoad(float Dt)
{
	if (!bPreLoading) return;
	PreSent += Dt;
	if (PreSent < 0.25f && !bPreDone) return;
	PreSent = 0.f;
	FString J = TEXT("["); bool bAll = true;
	for (int32 i = 0; i < PreNames.Num(); ++i)
	{
		const TSharedPtr<FStreamableHandle>& H = PreLoad[i];
		const float P = PreNames[i] == TEXT("The roads and the town") ? ADPLateWorld::Progress() : !H.IsValid() ? 1.f : (H->HasLoadCompleted() ? 1.f : H->GetProgress());
		bAll &= P >= 1.f;
		J += FString::Printf(TEXT("%s{\"n\":\"%s\",\"p\":%.3f}"), i ? TEXT(",") : TEXT(""), *PreNames[i], P);
	}
	J += TEXT("]");
	static float LogT = 0.f; LogT += 0.25f; if (LogT >= 2.f || bAll) { LogT = 0.f; ADP_LOG(TEXT("New game loading: %s"), *J); }
	if (Browser.IsValid()) Browser->ExecuteJavascript(FString::Printf(TEXT("window.__ueLoad && window.__ueLoad(%s, %s)"), *J, bAll ? TEXT("true") : TEXT("false")));
	if (bAll) { bPreLoading = false; ADP_LOG(TEXT("New game loading: everything Unreal needs is in")); }
}
