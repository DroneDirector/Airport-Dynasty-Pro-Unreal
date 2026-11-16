// Sets up the terminal test when play starts: sun, sky, fog, the grey-box terminal, the crowd test and
// the player standing at the plan's spawn point.
#include "ADPGameMode.h"
#include "Pets.h"
#include "ADPLog.h"
#include "Containers/Ticker.h"
#include "Escalators.h"
#include "JetwayRigs.h"
#include "GroundDecals.h"
#include "AirfieldLights.h"
#include "GroundQuality.h"
#include "Adverts.h"
#include "TerminalLift.h"
#include "Misc/PackageName.h"
#include "SunClock.h"
#include "ADPWalker.h"
#include "UI/ADPHUD.h"
#include "TerminalBuilder.h"
#include "TerminalDoors.h"
#include "TerminalFittings.h"
#include "TerminalLifts.h"
#include "TerminalJetways.h"
#include "TerminalScreens.h"
#include "TerminalCrowd.h"
#include "Airfield.h"
#include "GrassField.h"
#include "AirTraffic.h"
#include "Globe.h"
#include "PhotoTour.h"
#include "BabylonModel.h"
#include "LightMasts.h"
#include "WindSocks.h"
#include "Puddles.h"
#include "Offices.h"
#include "FireStation.h"
#include "TownBuildings.h"
#include "ParkedAircraft.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "RoadSigns.h"
#include "StreetLights.h"
#include "TowerADC.h"
#include "Hangar.h"
#include "RoadNetwork.h"
#include "LandCorridor.h"
#include "TerminalProps.h"
#include "Misc/Paths.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

#pragma region Setup

AADPGameMode::AADPGameMode()
{
	DefaultPawnClass = AADPWalker::StaticClass();
	HUDClass = AADPHUD::StaticClass();
}

void AADPGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	UWorld* W = GetWorld();
	if (!W) return;

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (!UGameplayStatics::GetActorOfClass(W, ADirectionalLight::StaticClass())) SpawnSkyAndLight();

	// the terminal is the Babylon edition's own model when it has been imported (Content/Babylon/Terminal, placed in
	// the map): then Unreal's stand-in pieces and moving aircraft step aside, and the airfield keeps only what lies
	// beyond the model's own site
	// (asked of the asset registry, not the disk: in a release build the model is inside the game's pak files)
	// (the true-Unreal terminal, /Game/Terminal, since 2026-11-14; the Babylon-exported set only in the editor, to compare)
	const bool bBabylon = FPackageName::DoesPackageExist(TEXT("/Game/Terminal/Meshes/terminal_mesh_0")) || FPackageName::DoesPackageExist(TEXT("/Game/Babylon/Terminal/terminal/StaticMeshes/terminal_mesh_0"));
	FTerminalPlan Plan;
	if (!Plan.Load()) return;
	{
		// the Babylon model, placed as it is
		if (bBabylon)
			if (ABabylonModel* Model = W->SpawnActor<ABabylonModel>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Model->Place(TEXT("Terminal"));
		// his jetways, rebuilt for the stands 12 m out (owner, 2026-11-13), as their own model in the terminal's place
		if (bBabylon && FPackageName::DoesPackageExist(TEXT("/Game/Babylon/Jetways/jetways/StaticMeshes/jetways_mesh_0")))
			if (ABabylonModel* Jw = W->SpawnActor<ABabylonModel>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Jw->Place(TEXT("Jetways"));
			// the old offices' things in the terminal's offices (yours with your pet, as chosen in the game)
			{ FString Pet = TEXT("dog"); GConfig->GetString(TEXT("ADP.Office"), TEXT("Pet"), Pet, GGameUserSettingsIni); ADPOffices::Place(W, Pet); }
			// the airport fire station, its training tower and its two crash tenders (FireStation.h)
			ADPFireStation::Place(W);
		if (!bBabylon)
		{
			if (ATerminalBuilder* Terminal = W->SpawnActor<ATerminalBuilder>(FVector::ZeroVector, FRotator::ZeroRotator, P)) Terminal->Build();
			if (ATerminalDoors* Doors = W->SpawnActor<ATerminalDoors>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Doors->Build(Plan);
			if (ATerminalFittings* Fittings = W->SpawnActor<ATerminalFittings>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Fittings->Build(Plan);
			if (ATerminalLifts* Lifts = W->SpawnActor<ATerminalLifts>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Lifts->Build(Plan);
			if (ATerminalJetways* Jetways = W->SpawnActor<ATerminalJetways>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Jetways->Build(Plan);
			if (ATerminalScreens* Screens = W->SpawnActor<ATerminalScreens>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Screens->Build(Plan);
		}
		// the terminal's placed props from data: the self check-in kiosk... (TerminalProps.h)
		if (ATerminalProps* Props = W->SpawnActor<ATerminalProps>(FVector::ZeroVector, FRotator::ZeroRotator, P)) Props->Build();
		// the LED advert screens on the terminal's walls (Adverts.h)
		if (AAdverts* Ads = W->SpawnActor<AAdverts>(FVector::ZeroVector, FRotator::ZeroRotator, P)) Ads->Build(Plan);
		// the control tower (ADC) at site A, west of the pier: walked into, its lift, doors and desk flap moving
		if (ATowerADC* Tower = W->SpawnActor<ATowerADC>(FVector::ZeroVector, FRotator::ZeroRotator, P)) Tower->Build();
		// the apron light masts, one in front of each gate and two in front of each hangar
		if (ALightMasts* MastsA = W->SpawnActor<ALightMasts>(FVector::ZeroVector, FRotator::ZeroRotator, P)) MastsA->Build();
		// a windsock near each runway end, turning into the wind
		if (AWindSocks* Socks = W->SpawnActor<AWindSocks>(FVector::ZeroVector, FRotator::ZeroRotator, P)) Socks->Build();
		// the airfield round the terminal, and its traffic
		if (AAirfield* Field = W->SpawnActor<AAirfield>(FVector::ZeroVector, FRotator::ZeroRotator, P); Field && Field->Build(bBabylon))
		{
			// the hangars on the hangar apron, each on its own lead-in line (Data/airfield.json "hangars")
			for (const FAirfieldData::FHangarSite& H : Field->Data().Hangars)
				if (AHangar* Hg = W->SpawnActor<AHangar>(FVector::ZeroVector, FRotator::ZeroRotator, P)) Hg->Build(H.Size, H.X, H.Y, H.bOpen);
			// his A320 on the terminal's stands, nose tyres on their stop bars (ParkedAircraft.h)
			if (bBabylon) ADPParked::Place(W, Field->Data());
			// marks on the paving: stand stains, touchdown rubber, apron patches (GroundDecals.h)
			if (AGroundDecals* GD = W->SpawnActor<AGroundDecals>()) GD->Place(Field->Data());
			// the taxiway lights: blue edges, green centrelines (AirfieldLights.h)
			if (AAirfieldLights* AL = W->SpawnActor<AAirfieldLights>()) AL->Place(Field->Data());
			ADPGroundQuality::Apply(W);   // (the "Airport ground" setting, GroundQuality.h)
			// the road network (forecourt, the road to town, the town's streets), the town's buildings on the land the roads
			// shaped, our own land along the roads, and the posts and signs set down on it: built later, one road a frame,
			// when you first go into the 3D world (or behind a new career's loading screen) - the title screen used to wait
			// 40 s for all of it (ADPLateWorld in ADPGameMode.h; owner, 2026-11-15)
			ADPLateWorld::Prepare(W);
			{
				FTimerHandle Th;
				W->GetTimerManager().SetTimer(Th, FTimerDelegate::CreateLambda([W]()
				{
					// your pet, sitting just outside the terminal's main entrance for now (Pets.h)
					ADPPets::PlaceHome(W);
					// the escalators in the new escalator room (Escalators.h): their foot at the Grand Hall's side, rising
					// west to the Floor 2 landing (their top combs on its edge, x 21), the pair in the guardrail's gap
					if (FPackageName::DoesPackageExist(TEXT("/Game/Escalator/SM_EscStep")) && !FParse::Param(FCommandLine::Get(), TEXT("NoEscalators")))
					{
						FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						const float Foot = 21.f + AEscalatorPair::LengthAlong();
						// two pairs (owner 2026-11-16: the pair copied beside itself with 50 cm between them): each pair 3.22 m
						// across its outsides (two escalators 1.60 m wide, 1.62 m centre to centre), so the second pair's
						// origin is 3.72 m along; all four centred on the landing (plan z 15 = Unreal y -15)
						for (const float Y : { -12.33f, -16.05f })
							if (AEscalatorPair* E = W->SpawnActor<AEscalatorPair>(FVector(Foot * 100.f, Y * 100.f, 1.2f), FRotator(0.f, -90.f, 0.f), Sp)) E->Build();
						// the staff lift up to the offices (TerminalLift.h): its car in the shaft in the staff entrance hall
						if (ATerminalLift* Lf = W->SpawnActor<ATerminalLift>(FVector::ZeroVector, FRotator::ZeroRotator, Sp)) Lf->Build(152.6f, 155.6f, 47.8f, 51.2f, 48.9f, 50.1f);
					}
					AJetwayRigs::Get(W);   // (his jetways rigged to move, JetwayRigs.h)
				}), 1.0f, false);
			}
			if (!bBabylon)
				if (AAirTraffic* Traffic = W->SpawnActor<AAirTraffic>(FVector::ZeroVector, FRotator::ZeroRotator, P))
					Traffic->Start(Field->Data());
			// the globe round the airport
			if (AGlobe* Globe = W->SpawnActor<AGlobe>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Globe->Build(Field->Data());
			// the grass on the airfield and the lawns: real blades, from your feet to the far edge of the field
			if (AGrassField* Grass = W->SpawnActor<AGrassField>(FVector::ZeroVector, FRotator::ZeroRotator, P))
			{ const auto& B = Field->Data().Boundary; Grass->Start(B.X0, B.Y0, B.X1, B.Y1); }
			// the real sun over the airport, for the date and the time of day
			// the puddles and wet ground (the ground's own material, one global wetness: Tools/set_puddles.py)
			W->SpawnActor<APuddles>(FVector::ZeroVector, FRotator::ZeroRotator, P);
			if (ASunClock* Sun = W->SpawnActor<ASunClock>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Sun->Setup(Field->Data().Lat, Field->Data().Lon, Field->Data().Heading);
		}
		// the passengers are switched off for now (owner, 2026-11-09); -Crowd brings them back
		if (FParse::Param(FCommandLine::Get(), TEXT("Crowd")))
			if (ATerminalCrowd* Crowd = W->SpawnActor<ATerminalCrowd>(FVector::ZeroVector, FRotator::ZeroRotator, P))
				Crowd->Start(Plan, CrowdSize);
		// photo mode, for screenshots of the airport
		if (FParse::Param(FCommandLine::Get(), TEXT("PhotoTour"))) W->SpawnActor<APhotoTour>(FVector::ZeroVector, FRotator::ZeroRotator, P);
		if (!UGameplayStatics::GetActorOfClass(W, APlayerStart::StaticClass()))
			W->SpawnActor<APlayerStart>(FTerminalPlan::ToWorld(Plan.Spawn.X, 1.0f, Plan.Spawn.Z), FRotator(0, -90.f, 0), P); // facing airside
	}
}

#pragma endregion

#pragma region Sky and light

void AADPGameMode::SpawnSkyAndLight()
{
	UWorld* W = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(FVector(0, 0, 5000), FRotator(-35.f, 40.f, 0), P))
	{
		Sun->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			L->SetAtmosphereSunLight(true);
			L->SetIntensity(10.f);
		}
	}
	W->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, P);

	FTransform At(FVector(0, 0, 1000));
	if (ASkyLight* Sky = W->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Sky->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		Sky->GetLightComponent()->bRealTimeCapture = true;
		Sky->FinishSpawning(At);
	}
	W->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	// the weather: volumetric clouds over the airport (the shared world's real weather drives them later)
	W->SpawnActor<AVolumetricCloud>(FVector::ZeroVector, FRotator::ZeroRotator, P);

	if (APostProcessVolume* PP = W->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, P))
	{
		PP->bUnbound = true;
		PP->Settings.bOverride_AutoExposureMinBrightness = true;
		PP->Settings.AutoExposureMinBrightness = -1.f;
		PP->Settings.bOverride_AutoExposureMaxBrightness = true;
		PP->Settings.AutoExposureMaxBrightness = 12.f;
		// the darkest shadows lifted a little, nothing brighter touched (owner, 2026-11-11: nowhere pitch black; there is
		// always some light from the sky or the moon): only the deepest tones get a small lift (LightMasts tints it cool
		// by night)
		PP->Settings.bOverride_ColorCorrectionShadowsMax = true;
		PP->Settings.ColorCorrectionShadowsMax = 0.05f;
		PP->Settings.bOverride_ColorOffsetShadows = true;
		PP->Settings.ColorOffsetShadows = FVector4(0.f, 0.f, 0.f, 0.0045f);
	}
}

#pragma endregion


// ---- the world's late part: roads, town, land along the roads (see InitGame) ----
namespace ADPLateWorld
{
	static TWeakObjectPtr<UWorld> World;
	static TWeakObjectPtr<ARoadNetwork> Roads;
	static bool bGo = false, bDone = false;
	static int32 Stage = 0;
	static FTSTicker::FDelegateHandle Tick;

	void Prepare(UWorld* W)
	{
		World = W; bGo = false; bDone = false; Stage = 0;
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARoadNetwork* R = W->SpawnActor<ARoadNetwork>(FVector::ZeroVector, FRotator::ZeroRotator, P);
		Roads = R;
		if (R) R->Build();
		// (built when New game, Continue or Load game opens the loading screen, or when you first go into the 3D world;
		// test runs, the photo tour and -LateWorldNow straight away: they show the world from the start)
		const TCHAR* C = FCommandLine::Get();
		if ((FParse::Param(C, TEXT("UIShot")) && !FParse::Param(C, TEXT("UIContinueShots"))) || FParse::Param(C, TEXT("PhotoTour")) || FParse::Param(C, TEXT("LateWorldNow"))) Go();
	}

	void Go()
	{
		if (bGo || bDone) return;
		bGo = true;
		ADP_LOG(TEXT("World: building the roads, the town and the land along the roads"));
		Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			UWorld* W = World.Get();
			if (!W) { bDone = true; return false; }
			switch (Stage)
			{
			case 0: if (!Roads.IsValid() || Roads->BuildSome(0.25)) Stage = 1; return true;   // (saved roads: many a frame; a road built from scratch: one)
			case 1: ADPTown::Place(W); Stage = 2; return true;
			case 2:
			{
				FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (ALandCorridor* Land = W->SpawnActor<ALandCorridor>(FVector::ZeroVector, FRotator::ZeroRotator, P)) Land->Build();
				Stage = 3; return true;
			}
			case 3: Stage = 4; return true;   // (a frame for the land's collision)
			default:
				for (TActorIterator<AStreetLights> It(W); It; ++It) It->Resettle();
				for (TActorIterator<ARoadSigns> It(W); It; ++It) It->Resettle();
				bDone = true;
				ADP_LOG(TEXT("World: roads, town and land are in"));
				return false;
			}
		}), 0.f);
	}

	bool Done() { return bDone; }
	bool Started() { return bGo; }
	float Progress() { return bDone ? 1.f : Roads.IsValid() ? 0.9f * Roads->Progress() + 0.1f * Stage / 4.f : 1.f; }
	float RoadsProgress() { return bDone || Stage > 0 ? 1.f : Roads.IsValid() ? Roads->Progress() : 1.f; }
	float LandProgress() { return bDone ? 1.f : Stage / 4.f; }
}
