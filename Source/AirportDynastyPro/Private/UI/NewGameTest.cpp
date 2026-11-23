// -UINewGame (owner, 2026-11-23: 0.18.1 shipped with a new career that never got past 92 %, and a creator the world and the
// airport's interface showed through): the whole path walked by itself, as a player would - the title, New game, where is
// your airport, she or he, your looks, your pet, your partner, your airline, the difficulty, Start game, the loading, the
// airport - a picture of every step (NewGame_01_Title ...), each wait with a limit. The log says "NEWGAME TEST: PASSED" or
// "FAILED at ..." and the game closes by itself. -UIContinueGame: the same for Continue. Works in the packaged game too.
// Tools/new_game_test.sh runs it (and keeps your own careers and settings safe); part of Tools/cinematic_test.sh.
#include "UI/ADPHUD.h"
#include "UI/SSiteChoice.h"
#include "UI/TitleBg.h"
#include "ADPLog.h"
#include "CharacterCreate.h"
#include "ChainTest.h"
#include "PaxNav.h"
#include "UObject/UObjectIterator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Site.h"
#include "Sim/ADPSim.h"
#include "SWebBrowser.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

namespace ADPNewGameTest
{
	int32 Step = 0, Sub = 0, Problems = 0;
	bool bWentBack = false;   // (from "who are you" back to the title once, and in again)
	double T0 = 0.0, StepT0 = 0.0, StartedLoad = 0.0, ReachedAirport = 0.0, EndAt = 0.0;
	const TCHAR* Names[] = {TEXT("the title"), TEXT("where is your airport"), TEXT("she or he"), TEXT("your looks"), TEXT("your pet"), TEXT("your partner"),
		TEXT("your airline"), TEXT("the difficulty"), TEXT("the loading"), TEXT("the airport")};
}

void AADPHUD::TestNewGame(float Dt)
{
	using namespace ADPNewGameTest;
	static const bool bContinue = FParse::Param(FCommandLine::Get(), TEXT("UIContinueGame"));
	const double Now = FPlatformTime::Seconds();
	ADPChainTest::BlockInput();
	if (T0 == 0.0) { T0 = StepT0 = Now; ADP_LOG(TEXT("NEWGAME TEST: started (%s)"), bContinue ? TEXT("Continue") : TEXT("New game")); }
	if (EndAt > 0.0) { if (Now > EndAt) { EndAt = 1e18; FGenericPlatformMisc::RequestExit(false); } return; }
	const float T = float(Now - StepT0);
	auto Shot = [](const TCHAR* N) { FScreenshotRequest::RequestScreenshot(FString(bContinue ? TEXT("Continue_") : TEXT("NewGame_")) + N, true, false); ADP_LOG(TEXT("NEWGAME TEST: picture %s"), N); };
	auto Js = [this](const TCHAR* Code) { if (Browser.IsValid()) Browser->ExecuteJavascript(Code); };
	auto Key = [&Js](const TCHAR* K) { Js(*FString::Printf(TEXT("window.dispatchEvent(new KeyboardEvent('keydown', {key: '%s'}))"), K)); };
	auto Next = [&]() { ADP_LOG(TEXT("NEWGAME TEST: %s - done after %.1f s"), Names[Step], T); ++Step; Sub = 0; StepT0 = Now; };
	auto Wrong = [&](const FString& What) { ++Problems; ADP_WARN(TEXT("NEWGAME TEST: %s: %s"), Names[Step], *What); };
	auto Fail = [&](const FString& Why) { ADP_WARN(TEXT("NEWGAME TEST: FAILED at %s after %.0f s: %s"), Names[Step], T, *Why); Shot(TEXT("99_Failed")); EndAt = Now + 2.0; };
	// (one sub-step after another, each at its time from the step's start)
	auto At = [&](int32 N, float Time) { if (Sub == N && T >= Time) { ++Sub; return true; } return false; };
	// on the stage nothing else may be up: not the airport's interface, not its loading screen, not the title's film
	auto StageClean = [&]()
	{
		if (!OwnScreen()) Wrong(TEXT("the airport's interface is up over it"));
		if (bLoading) Wrong(TEXT("a loading screen is up over it"));
		if (ADPTitleBg::IsOn()) Wrong(TEXT("the title's film plays behind it"));
	};
	// nothing of the airport's interface, nor its map, before the game is fully in: looked at every frame of every step (the
	// title, the site question, the stage and its steps, back at the title, the loading to its last second) - to-do 227
	auto HudUp = [&]() { return HudBox.IsValid() && HudBox->GetVisibility().IsVisible(); };
	auto MapUp = [&]() { return HudUp() && MiniBox.IsValid() && MiniBox->GetVisibility().IsVisible(); };
	{
		static int32 Said = -1;
		const bool bInNow = Step >= 9 || (Step == 8 && Sub > 0 && !bGame && !bLoading && !bStartLoad);
		if (!bInNow && Said != Step && HudUp()) { Said = Step; Wrong(FString::Printf(TEXT("the airport's interface is up before the game is in (the map %s)"), MapUp() ? TEXT("too") : TEXT("not"))); }
	}
	if (Now - T0 > 600.0) { Fail(TEXT("10 minutes")); return; }

	switch (Step)
	{
	case 0:
		if (!bMenusReady || !bGame) { if (T > 240.f) Fail(TEXT("the title never came up")); Sub = 0; return; }
		if (Sub == 0) { Sub = 1; StepT0 = Now; if (CrashPanel.IsValid()) CrashPanel->SetVisibility(EVisibility::Collapsed); return; }   // (an earlier run's crash note: not part of this)
		if (At(1, 8.f)) Shot(bWentBack ? TEXT("01_TitleAgain") : TEXT("01_Title"));
		if (At(2, 9.f)) Js(bContinue ? TEXT("(function(){var b=document.querySelector('[data-id=\"game:continue\"]');if(b)b.click();})()") : TEXT("(function(){var b=document.querySelector('[data-id=\"game:new\"]');if(b)b.click();})()"));
		if (Sub == 3) { if (bContinue) { Next(); Step = 8; } else Next(); }
		return;
	case 1:
		if (ADPSite::All().Num() <= 1) { Next(); return; }
		if (!SiteChoice.IsValid()) { if (Sub > 0) { Next(); return; } if (T > 20.f) Fail(TEXT("it didn't open")); return; }
		if (Sub == 0) { Sub = 1; StepT0 = Now; return; }
		if (At(1, 2.f)) Shot(TEXT("02_Where"));
		if (At(2, 3.f)) { TSharedPtr<SSiteChoice> C = SiteChoice; if (C->OnPick) C->OnPick(ADPSite::Id()); }   // (the site loaded now: the other one loads the world again)
		return;
	case 2:
		if (!bWhoAreYou) { if (T > 20.f) Fail(TEXT("the stage didn't open")); return; }
		if (Sub == 0 && T >= 7.f && !bWentBack)
		{
			// back to the title from here (the map lay over it then), and the same way in again
			StageClean(); bWentBack = true; OnEscape();
			if (bWhoAreYou || !bGame) Wrong(TEXT("Back didn't come out at the title"));
			ADP_LOG(TEXT("NEWGAME TEST: back to the title from she or he")); Step = 0; Sub = 0; StepT0 = Now;
			return;
		}
		if (At(0, 7.f)) { StageClean(); Shot(TEXT("03_Who")); }
		if (At(1, 7.5f)) { Js(TEXT("document.querySelectorAll('.wy-half,.wy-who').forEach(function(e){ e.style.pointerEvents = 'none'; })")); Key(TEXT("ArrowLeft")); }   // (wherever the real mouse is)
		if (At(2, 12.f)) Shot(TEXT("04_She"));
		if (At(3, 12.5f)) Key(TEXT("ArrowRight"));
		if (At(4, 17.f)) { StageClean(); Shot(TEXT("05_He")); }
		if (At(5, 17.5f)) Key(TEXT("Enter"));
		if (Sub == 6 && bWhoAreYouCreator) Next();
		else if (Sub == 6 && T > 45.f) Fail(TEXT("the choice never finished"));
		return;
	case 3:
		if (At(0, 5.f)) { StageClean(); Shot(TEXT("06_Looks")); }
		if (At(1, 5.5f)) Key(TEXT("Enter"));
		if (At(2, 6.5f)) Next();
		return;
	case 4:
		if (At(0, 0.5f)) { Key(TEXT("ArrowUp")); Key(TEXT("ArrowUp")); }   // (the first dog walks in and sits)
		if (At(1, 8.f)) { StageClean(); Shot(TEXT("07_Pet")); if (ADPCharacterCreate::PetChoice().IsEmpty()) Wrong(TEXT("no pet walked in")); }
		if (At(2, 8.5f)) Key(TEXT("Enter"));
		if (At(3, 9.5f)) Next();
		return;
	case 5:
		if (At(0, 0.5f)) Key(TEXT("ArrowUp"));   // (married: your partner walks in)
		if (At(1, 7.f)) { StageClean(); Shot(TEXT("08_Partner")); }
		if (At(2, 7.5f)) Key(TEXT("Enter"));
		if (At(3, 8.5f)) Next();
		return;
	case 6:
		if (At(0, 0.5f)) Js(TEXT("(function(){ function put(f, v){ var e = document.querySelector('.wy [data-f=' + f + ']'); if (e) { e.value = v; e.dispatchEvent(new Event('input', {bubbles: true})); } } put('name', 'New Game Test Air'); put('pilot', 'Test Pilot'); })()"));
		if (At(1, 2.f)) { StageClean(); Shot(TEXT("09_Airline")); }
		if (At(2, 2.5f)) Key(TEXT("Enter"));
		if (At(3, 3.5f)) Next();
		return;
	case 7:
		if (At(0, 1.5f)) { StageClean(); Shot(TEXT("10_Difficulty")); Js(TEXT("window.ue && ue.adp && ue.adp.log('new game choices: ' + JSON.stringify(window.__ngChoices || null))")); }
		if (At(1, 2.f)) Key(TEXT("Enter"));      // Start game
		if (Sub == 2 && bStartLoad) Next();
		else if (Sub == 2 && T > 15.f) Fail(TEXT("Start game didn't start the loading"));
		return;
	case 8:
		if (Sub == 0)
		{
			if (bStartLoad || bLoading) { Sub = 1; StartedLoad = Now; StepT0 = Now; }
			else if (T > 20.f) Fail(TEXT("no loading screen"));
			return;
		}
		if (At(1, 1.f)) Shot(TEXT("11_Loading"));
		if (Sub < 4 && T > 6.f && !bGame && bLoading && LoadFrac > 0.6f) { Sub = 4; Shot(TEXT("12_Loading")); }
		{
			static float Said = 0.f; if (T - Said > 5.f || T < Said) { Said = T; ADP_LOG(TEXT("NEWGAME TEST: loading %.0f s, %.0f %%, %s"), T, LoadFrac * 100.f, *LoadStep); }
		}
		if (!bGame && !bLoading && !bStartLoad) { ReachedAirport = Now; Next(); return; }
		if (T > 300.f) Fail(FString::Printf(TEXT("still at %.0f %% after 5 minutes: %s"), LoadFrac * 100.f, *LoadStep));
		return;
	default:
		if (At(0, 5.f))
		{
			Shot(TEXT("13_Airport"));
			Js(TEXT("(function(){ var f = document.querySelector('.game-frame'), n = '?'; try { n = f.contentWindow.eval('State.companies.map(function(c){ return c.name }).join()'); } catch (e) { n = String(e); } window.ue && ue.adp && ue.adp.log('new game test: the menus have ' + n + ', opened with ' + (f ? f.src.slice(-240) : 'nothing')); })()"));
			if (!HudUp()) Wrong(TEXT("the airport's interface is away"));
			if (!MapUp()) Wrong(TEXT("the holographic map is away"));
			if (ADPTitleBg::IsOn()) Wrong(TEXT("the title's film is still on"));
			if (GEngine && GEngine->GameViewport && GEngine->GameViewport->bDisableWorldRendering) Wrong(TEXT("the world isn't drawn"));
			const UADPSim* Sim = UADPSim::Get(this);
			if (Sim && !Sim->IsRunning()) Wrong(TEXT("the career isn't running"));
			if (!PaxNav::Ready(GetWorld())) Wrong(TEXT("the walking map isn't ready"));
			else
			{
				FString S; const int32 Bad = PaxNav::Legs(GetWorld(), &S);
				ADP_LOG(TEXT("NEWGAME TEST: the walking map: %s"), *S);
				if (Bad > 0) Wrong(*FString::Printf(TEXT("%d legs of the passengers' ways fail (the log's PaxNav leg FAILED lines)"), Bad));
			}
			// (what has no size at all: the renderer can't turn its place round - the Matrix.h check in the log at loading)
			for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
			{
				if (It->GetWorld() != GetWorld() || !It->IsRegistered()) continue;
				int32 N = It->GetComponentScale().IsNearlyZero(1e-7) ? 1 : 0;
				if (const UInstancedStaticMeshComponent* K = Cast<UInstancedStaticMeshComponent>(*It))
					for (int32 k = 0; k < K->GetInstanceCount(); ++k) { FTransform X; if (K->GetInstanceTransform(k, X, true) && X.GetScale3D().IsNearlyZero(1e-7)) ++N; }
				if (N) ADP_LOG(TEXT("NEWGAME TEST: no size: %s / %s, %d copies"), It->GetOwner() ? *It->GetOwner()->GetName() : TEXT("-"), *It->GetName(), N);
			}
			const APawn* Me = GetOwningPawn();
			ADP_LOG(TEXT("NEWGAME TEST: %s - %d problems; loading %.1f s, from the title to the airport %.1f s, standing at %s"), Problems ? TEXT("FINISHED WITH PROBLEMS") : TEXT("PASSED"),
				Problems, ReachedAirport - StartedLoad, ReachedAirport - T0, Me ? *Me->GetActorLocation().ToString() : TEXT("-"));
			if (ADPChainTest::StartStage()) { EndAt = 1e17; ADPChainTest::Begin(GetWorld()); }   // (the career step after it: ChainTest.cpp ends the game)
			else EndAt = Now + 3.0;
		}
		return;
	}
}
