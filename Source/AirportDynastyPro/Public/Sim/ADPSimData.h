// The simulation's data (docs/AI-Simulation-Plan.md part 1): one world record holding everything the game simulates,
// saved as a versioned JSON file per career slot (Saved/Careers/<slot>.json) and, later, synced with the Android app.
// Each module adds its own fields as it is built; the places for what the 3D world does not have yet (aircraft cabins)
// are here now, marked unfinished (to-do 155).
#pragma once

#include "CoreMinimal.h"
#include "ADPSimData.generated.h"

/** A money movement (the ledger). */
USTRUCT() struct FADPTxn
{
	GENERATED_BODY()
	UPROPERTY() FDateTime At;
	UPROPERTY() FString Kind;          // revenue, fuel, crew, maint, fine, purchase, lease, fees, salary, sale, loan, insurance, marketing, charter, training
	UPROPERTY() double Amount = 0.0;   // + in, - out
	UPROPERTY() FString What;
	UPROPERTY() FString Link;          // the flight, person or aircraft it belongs to
};

/** An airline: yours or a rival's (rivals use the same record, so they can grow later). */
USTRUCT() struct FADPAirline
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString Name;
	UPROPERTY() FString Icao;
	UPROPERTY() FString Hub;
	UPROPERTY() bool bPlayer = false;
	UPROPERTY() double Balance = 0.0;
	UPROPERTY() double Reputation = 50.0;   // 0..100
	UPROPERTY() double Quality = 0.75;      // rivals: 0.6..0.95
	UPROPERTY() FString Kind = TEXT("major");   // major | regional | bush
};

USTRUCT() struct FADPAircraft
{
	GENERATED_BODY()
	UPROPERTY() FString Reg;
	UPROPERTY() FString TypeId;
	UPROPERTY() FString AirlineId;
	UPROPERTY() FString Base;
	UPROPERTY() FString Location;          // where it stands now (ICAO)
	UPROPERTY() double HoursMin = 0.0;
	UPROPERTY() int32 Cycles = 0;
	UPROPERTY() double Condition = 100.0;   // 0..100
	UPROPERTY() FString Status = TEXT("parked");   // parked | flying | grounded | maintenance | leased
	UPROPERTY() FString Cabin;             // the seat layout
	UPROPERTY() bool bCabinBuilt3D = false;   // (unfinished: the cabin's 3D model and seats - to-do 155)
	UPROPERTY() FString RouteId;           // the route it flies (empty: standby)
	UPROPERTY() int32 LegIdx = 0;          // the next stop in the route's list
	UPROPERTY() FDateTime ReadyAt;         // when it may leave next (after the turnaround)
	UPROPERTY() FDateTime WaitingSince;    // waiting at the gate since (cancelled after 5 h)
	UPROPERTY() FString Ownership = TEXT("owned");   // owned | leased
	UPROPERTY() double Value = 0.0;        // what it is worth now (USD; owned: falls a little every month)
	UPROPERTY() double LeaseMonthly = 0.0; // leased: the rent a month
};

/** A person: staff of any kind, pilots included (passengers live in FADPPassenger). */
USTRUCT() struct FADPPerson
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString Name;
	UPROPERTY() FString Role;              // captain, fo, fa, mech, ground, security, gate, checkin, cleaning, catering, fuel, bus, fire, tower, ops
	UPROPERTY() FString AirlineId;
	UPROPERTY() FString Base;
	UPROPERTY() bool bWoman = false;
	UPROPERTY() int32 Age = 30;
	UPROPERTY() double Salary = 0.0;       // a week
	UPROPERTY() double Rating = 4.0;       // 0..5
	UPROPERTY() int32 Level = 1;
	UPROPERTY() double Xp = 0.0;
	UPROPERTY() double Mood = 70.0;        // 0..100
	UPROPERTY() FString Status = TEXT("on");   // on | off | sick | leave | training | notice
	UPROPERTY() TArray<FString> Types;     // type ratings
	UPROPERTY() FString Home;              // where they live (the town)
	UPROPERTY() FString Shift;             // early | day | late | night
	UPROPERTY() double DutyMin = 0.0;      // minutes on duty since the last rest (pilots)
	UPROPERTY() FDateTime RestUntil;       // resting until
};

USTRUCT() struct FADPRoute
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString AirlineId;
	UPROPERTY() TArray<FString> Stops;     // ICAO, in order
	UPROPERTY() FString TypeId;
	UPROPERTY() FString PilotId;
	UPROPERTY() double FareIndex = 1.0;
};

/** A flight in the air or on its way out. */
USTRUCT() struct FADPFlight
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString Reg;
	UPROPERTY() FString AirlineId;
	UPROPERTY() FString PilotId;
	UPROPERTY() FString From;
	UPROPERTY() FString To;
	UPROPERTY() FDateTime DepAt;
	UPROPERTY() double DurMin = 0.0;
	UPROPERTY() int32 Pax = 0;
	UPROPERTY() int32 PaxJ = 0;
	UPROPERTY() int32 PaxF = 0;
	UPROPERTY() double CargoKg = 0.0;
	UPROPERTY() FString Phase = TEXT("Taxi out");
	UPROPERTY() FString TypeId;
	UPROPERTY() double DelayMin = 0.0;    // minutes late off the gate
	UPROPERTY() double Lat = 0.0;         // where it is now (for the map)
	UPROPERTY() double Lon = 0.0;
	UPROPERTY() double AltFt = 0.0;
	UPROPERTY() double GsKt = 0.0;
	UPROPERTY() double HeadingDeg = 0.0;
	UPROPERTY() double HoldMin = 0.0;       // minutes added in the air (holds before landing)
	UPROPERTY() bool bWxChecked = false;    // the destination's weather looked at on the way down
	UPROPERTY() FString Diverted;           // the destination it was going to, when it diverted
	UPROPERTY() FString Emergency;          // what went wrong, when it did
	UPROPERTY() double LoadFactor = 0.0;    // how full (0..1)
	UPROPERTY() double Revenue = 0.0;       // the ticket and cargo money (USD), booked by the economy
};

/** One passenger of a flight at your airport, for the 3D world and the tracking map (to-do 155). */
USTRUCT() struct FADPPassenger
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString Name;
	UPROPERTY() FString FlightId;
	UPROPERTY() FString Seat;
	UPROPERTY() FString Stage = TEXT("home");   // home, coach, car, forecourt, checkin, security, airside, gate, boarding, cabin, gone
	UPROPERTY() FVector Where = FVector::ZeroVector;   // on the airport (cm), for the map's dot
	UPROPERTY() bool bConduct = false;        // a red dot: security or police called
};

/** A coach on its way to or from the airport (to-do 155: the tracking map shows them with how many sit inside). */
USTRUCT() struct FADPCoach
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString Route;            // from which area
	UPROPERTY() int32 Aboard = 0;
	UPROPERTY() FVector Where = FVector::ZeroVector;
};

USTRUCT() struct FADPNotice
{
	GENERATED_BODY()
	UPROPERTY() FDateTime At;
	UPROPERTY() FString Kind;             // mechanic, authority, finance, pilot, company, flight, staff, birthday
	UPROPERTY() FString Text;
	UPROPERTY() bool bRead = false;
};

/** A bank loan. */
USTRUCT() struct FADPLoan
{
	GENERATED_BODY()
	UPROPERTY() FString AirlineId;
	UPROPERTY() double Owed = 0.0;
	UPROPERTY() double RateYear = 0.06;    // interest a year
	UPROPERTY() int32 MonthsLeft = 60;
	UPROPERTY() double Monthly = 0.0;      // the instalment (interest + repayment)
};

/** An aircraft offered on the used market. */
USTRUCT() struct FADPUsedOffer
{
	GENERATED_BODY()
	UPROPERTY() FString TypeId;
	UPROPERTY() int32 AgeYears = 5;
	UPROPERTY() double Condition = 80.0;
	UPROPERTY() double Price = 0.0;
	UPROPERTY() FString Location;
};

/** A marketing campaign: more travellers choose the airline (on one route, or everywhere) until it ends. */
USTRUCT() struct FADPCampaign
{
	GENERATED_BODY()
	UPROPERTY() FString AirlineId;
	UPROPERTY() FString RouteId;           // empty: the whole airline
	UPROPERTY() double Boost = 0.1;        // +10% pull
	UPROPERTY() FDateTime Until;
};

/** The whole simulated world of one career. */
USTRUCT() struct FADPWorld
{
	GENERATED_BODY()
	UPROPERTY() int32 Version = 1;
	UPROPERTY() FString Slot;
	UPROPERTY() FDateTime SavedAt;
	UPROPERTY() FDateTime SimAt;           // the simulation time when saved (the catch-up starts here)
	UPROPERTY() FString Difficulty = TEXT("normal");
	UPROPERTY() TArray<FADPAirline> Airlines;
	UPROPERTY() TArray<FADPAircraft> Aircraft;
	UPROPERTY() TArray<FADPPerson> People;
	UPROPERTY() TArray<FADPRoute> Routes;
	UPROPERTY() TArray<FADPFlight> Flights;
	UPROPERTY() TArray<FADPPassenger> Passengers;
	UPROPERTY() TArray<FADPCoach> Coaches;
	UPROPERTY() TArray<FADPTxn> Ledger;
	UPROPERTY() TArray<FADPNotice> Notices;
	UPROPERTY() TArray<FADPLoan> Loans;
	UPROPERTY() double FuelIndex = 1.0;    // the fuel market (1 = normal), 0.55..2.2
	UPROPERTY() double PartsIndex = 1.0;   // the parts market
	UPROPERTY() double FuelTarget = 1.0;
	UPROPERTY() double PartsTarget = 1.0;
	UPROPERTY() FDateTime NextMarketNews;
	UPROPERTY() TArray<FADPUsedOffer> UsedMarket;
	UPROPERTY() TArray<FADPCampaign> Campaigns;
	UPROPERTY() int32 NextReg = 1;
	UPROPERTY() TMap<FString, FDateTime> ModuleTimes;   // where each module had got to
};
