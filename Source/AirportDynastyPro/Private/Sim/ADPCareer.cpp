// Your career in the simulation (to-do 173; owner, 2026-11-20 night): New game makes your airline at the airport you chose,
// Continue / Load picks it up and catches up on the time the game was closed. See Sim/ADPSim.h.
#include "Sim/ADPSim.h"
#include "ADPLog.h"
#include "Site.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPFlightsModule.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPStaff.h"
#include "Sim/ADPRivals.h"
#include "Sim/ADPEvents.h"
#include "Sim/ADPMaint.h"
#include "Sim/ADPGround.h"
#include "Containers/Ticker.h"
#include "Sim/ADPRoster.h"
#include "Sim/ADPBoards.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"

namespace
{
	TSharedPtr<FADPRivalsModule> GCareerRivals;
	TSharedPtr<FADPFlightsModule> GCareerFlights;
	bool GModulesAdded = false;
	/** The site's airport code (LSCH, BIAP...). */
	FString SiteIcao()
	{
		FString Icao = TEXT("BIAP");
		if (TSharedPtr<FJsonObject> A = ADPSite::AirportBlock()) A->TryGetStringField(TEXT("icao"), Icao);
		return Icao;
	}
	/** Runs F once the airports and aircraft types are loaded (at once if they are). */
	void WhenWorldReady(TFunction<void()> F)
	{
		if (ADPWorldData::IsReady()) { F(); return; }
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([F](float) -> bool
		{ if (!ADPWorldData::IsReady()) return true; F(); return false; }), 0.1f);
	}
}

FString UADPSim::SiteSlot() { return TEXT("career-") + ADPSite::Id(); }

double UADPSim::StartingMoney(const FString& Difficulty)
{
	// hard: one used A320-class aircraft (about 12 years old, fair condition), its crew (2 captains, 2 first officers,
	// 4 cabin crew: 8 weeks of wages and the hiring fees) and some spare for fuel and fees - never a second aircraft
	const FADPTypeInfo* T = ADPWorldData::Type(TEXT("A320"));
	const double Used = (T ? T->ValueM : 101.0) * 1e6 * FMath::Pow(0.93, 12) * (0.55 + 0.45 * 0.75);
	double Crew = 0.0;
	for (const TCHAR* R : {TEXT("captain"), TEXT("captain"), TEXT("fo"), TEXT("fo"), TEXT("fa"), TEXT("fa"), TEXT("fa"), TEXT("fa")})
		Crew += ADPStaff::GoingRate(R, TEXT("CH")) * 9.0;
	const double Hard = Used + Crew + 2.5e6;
	const FString D = Difficulty.ToLower();
	if (D == TEXT("easy")) return Hard * 3.0;     // (a lot of freedom; the starter rule still allows one aircraft at first)
	if (D == TEXT("hard")) return Hard;
	return Hard * 1.4;                            // normal: a little more freedom
}

void UADPSim::AddGameModules()
{
	if (GModulesAdded) return;
	GModulesAdded = true;
	GCareerFlights = MakeShared<FADPFlightsModule>(); Register(GCareerFlights.ToSharedRef());
	Register(MakeShared<FADPEconomyModule>());
	Register(MakeShared<FADPStaffModule>());
	GCareerRivals = MakeShared<FADPRivalsModule>(); Register(GCareerRivals.ToSharedRef());
	Register(MakeShared<FADPEventsModule>());
	Register(MakeShared<FADPMaintModule>());   // (to-do 181)
	Register(MakeShared<FADPStandsModule>());  // (to-do 183)
}

void UADPSim::NewCareer(const FString& Difficulty, const FString& AirlineName)
{
	bRunning = false;
	WhenWorldReady([this, Difficulty, AirlineName]()
	{
		const FDateTime T = Now();
		World = FADPWorld();
		World.Slot = SiteSlot(); World.Difficulty = Difficulty.IsEmpty() ? TEXT("normal") : Difficulty.ToLower();
		const FString Icao = SiteIcao();
		const FADPAirportInfo* Home = ADPWorldData::Airport(Icao);
		FADPAirline Me;
		Me.Id = UADPSim::Me(); Me.bPlayer = true; Me.Hub = Icao; Me.Kind = TEXT("regional");
		Me.Name = AirlineName.IsEmpty() ? (ADPSite::IsDefault() ? TEXT("Mýrdalssandur Air") : TEXT("Chablais Air")) : AirlineName;
		Me.Icao = Home && Home->Country == TEXT("CH") ? TEXT("CHB") : TEXT("MYR");
		Me.Balance = StartingMoney(World.Difficulty);
		World.Airlines.Add(Me);
		World.Career.Started = T;
		{
			FADPTxn Tx; Tx.At = T; Tx.Kind = TEXT("capital"); Tx.Amount = Me.Balance; Tx.Link = Me.Id; Tx.What = TEXT("Founding capital");
			World.Ledger.Add(Tx);
		}
		AddGameModules();
		if (GCareerRivals) GCareerRivals->Found(World, 12, T);
		{ FRandomStream Rs(int32(T.GetTicks() & 0x7fffffff)); ADPStaff::FillPool(World, Rs); }   // (people looking for work from the first minute)
		{
			FADPNotice N; N.At = T; N.Kind = TEXT("company"); N.Priority = 1; N.Title = TEXT("Welcome to ") + (Home ? Home->Name : Icao);
			N.Text = FString::Printf(TEXT("%s is founded at %s with $%.1f million. First hire the crew one aircraft needs, then buy or lease your first aircraft (new or used)."),
				*Me.Name, *Icao, Me.Balance / 1e6);
			World.Notices.Add(N);
		}
		Start(T);   // (nothing to catch up: it starts now and runs live)
		SaveCareer(World.Slot);
		ADP_LOG(TEXT("Sim: new career %s - %s at %s (%s), %s, $%.1fM, %d AI airlines"), *World.Slot, *Me.Name, *Icao, Home ? *Home->Name : TEXT("not in the world list!"),
			*World.Difficulty, Me.Balance / 1e6, World.Airlines.Num() - 1);
	});
}

bool UADPSim::ResumeCareer()
{
	if (!CareerSlots().Contains(SiteSlot())) return false;
	bRunning = false;
	WhenWorldReady([this]()
	{
		if (!LoadCareer(SiteSlot())) { ADP_WARN(TEXT("Sim: %s couldn't be read - a new career instead"), *SiteSlot()); NewCareer(TEXT("normal")); return; }
		World.Slot = SiteSlot();
		AddGameModules();
		const FDateTime Last = World.SimAt == FDateTime() ? Now() : World.SimAt;
		Start(Last);
		// each module from where it had got to (older than its own limit is skipped in Start)
		ADP_LOG(TEXT("Sim: career %s resumed - %.1f hours to catch up"), *World.Slot, (Now() - Last).GetTotalHours());
	});
	return true;
}

// -SimCareerTest (to-do 172-177): a hard career at this site, crew hired, an A320 leased onto a route from here, four
// days flown in the catch-up - then the roster, the boards and the messages are checked in the log and the game closes
void UADPSim::CareerSelfTest()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("SimCareerTest"))) return;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float) -> bool
	{
		static int32 Stage = 0; static double LiveAt = 0.0;
		if (!ADPWorldData::IsReady()) return true;
		const FString Here = ADPBoards::HereIcao();
		if (Stage == 0)
		{
			const FADPAirportInfo* H = ADPWorldData::Airport(Here);
			ADP_LOG(TEXT("SimCareerTest: %s in the world list: %s (%s), %d runway(s) %s/%s, %d ft"), *Here, H ? *H->Name : TEXT("MISSING"), H ? *H->Iata : TEXT("-"), H ? H->Runways.Num() : 0,
				H && H->Runways.Num() ? *H->Runways[0].Le : TEXT("-"), H && H->Runways.Num() ? *H->Runways[0].He : TEXT("-"), H ? H->ElevFt : 0);
			for (const TCHAR* D : {TEXT("easy"), TEXT("normal"), TEXT("hard")}) ADP_LOG(TEXT("SimCareerTest: starting money %s $%.1fM"), D, StartingMoney(D) / 1e6);
			NewCareer(TEXT("hard"));
			Stage = 1; return true;
		}
		if (Stage == 1)
		{
			if (!bRunning) return true;
			const FDateTime T0 = Now() - FTimespan::FromDays(4.0);
			int32 Hired = 0;
			for (const TCHAR* Role : {TEXT("captain"), TEXT("captain"), TEXT("captain"), TEXT("fo"), TEXT("fo"), TEXT("fo"), TEXT("fa"), TEXT("fa"), TEXT("fa"), TEXT("fa"), TEXT("fa"), TEXT("fa"), TEXT("fa"), TEXT("fa")})
				if (FADPPerson* P = World.People.FindByPredicate([&](const FADPPerson& X) { return X.AirlineId.IsEmpty() && X.Role == Role; }))
					Hired += ADPStaff::Hire(World, P->Id, Me(), Here, T0);
			const FString Reg = ADPEconomy::BuyAircraft(World, Me(), TEXT("A320"), true, Here, T0);
			FADPRoute R; R.Id = TEXT("r-test"); R.AirlineId = Me(); R.Stops = {Here, TEXT("EGLL")}; R.TypeId = TEXT("A320"); R.OpenedAt = T0; World.Routes.Add(R);
			if (FADPAircraft* A = World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; })) { A->RouteId = R.Id; A->Location = Here; A->ReadyAt = T0; }
			ADP_LOG(TEXT("SimCareerTest: hired %d, leased %s, balance $%.1fM; flying 4 days from %s"), Hired, *Reg, World.Airlines[0].Balance / 1e6, *T0.ToString());
			Start(T0);
			Stage = 2; return true;
		}
		if (Stage == 2)
		{
			if (bCatchingUp) return true;
			if (LiveAt == 0.0) { LiveAt = FPlatformTime::Seconds(); return true; }
			if (FPlatformTime::Seconds() - LiveAt < 8.0) return true;
			const FADPFlightsModule* M = GCareerFlights.Get();
			ADP_LOG(TEXT("SimCareerTest: %lld departures, %lld landings, crew waits %lld, %d notices"), M ? M->Departures : -1, M ? M->Landings : -1, M ? M->CrewWaits : -1, World.Notices.Num());
			bool bLegal = true;
			for (const FADPPerson& P : World.People) if (P.AirlineId == Me())
			{
				const double F28 = ADPRoster::Fly28(P) / 60.0;
				bLegal &= F28 <= 100.0;
				ADP_LOG(TEXT("SimCareerTest: %s %s - %.1f h in 28 days, %d days on, at %s, %s%s"), *P.Role, *P.Name, F28, P.DaysOn, P.Location.IsEmpty() ? *P.Base : *P.Location,
					P.WithReg.IsEmpty() ? TEXT("off duty") : *(TEXT("on ") + P.WithReg), P.RestUntil > Now() ? TEXT(", resting") : TEXT(""));
			}
			for (const bool bArr : {false, true}) for (const FADPBoardRow& B : ADPBoards::Rows(this, bArr, 8))
				ADP_LOG(TEXT("SimCareerTest: board %s %s %s %s %s gate %d"), bArr ? TEXT("ARR") : TEXT("DEP"), *B.Time(), *B.Flight, *B.Place, *B.Status, B.Gate);
			int32 Shown = 0; for (const FADPNotice& N : World.Notices) if (Shown++ < 12) ADP_LOG(TEXT("SimCareerTest: notice [%s] %s %s"), *N.Kind, *N.Title, *N.Text);
			ADP_LOG(TEXT("SimCareerTest: %s"), M && M->Landings >= 4 && bLegal ? TEXT("PASS") : TEXT("FAIL"));
			IFileManager::Get().Delete(*CareerFile(World.Slot));
			FPlatformMisc::RequestExit(false);
			return false;
		}
		return true;
	}), 0.2f);
}
