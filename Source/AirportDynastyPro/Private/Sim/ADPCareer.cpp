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
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"

namespace
{
	TSharedPtr<FADPRivalsModule> GCareerRivals;
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
	Register(MakeShared<FADPFlightsModule>());
	Register(MakeShared<FADPEconomyModule>());
	Register(MakeShared<FADPStaffModule>());
	GCareerRivals = MakeShared<FADPRivalsModule>(); Register(GCareerRivals.ToSharedRef());
	Register(MakeShared<FADPEventsModule>());
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
