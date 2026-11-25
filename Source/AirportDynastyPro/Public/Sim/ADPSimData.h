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

/** A ground vehicle kept at the vehicle area (to-do 277 E1, Sim/ADPVehicleArea.h). */
USTRUCT()
struct FADPSpare
{
	GENERATED_BODY()
	UPROPERTY() FString Kind;    // the key of Data/vehicle-area.json
	UPROPERTY() FString Place;   // its place's name
};

/** An airline: yours or a rival's (rivals use the same record, so they can grow later). */
USTRUCT() struct FADPAirline
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString Name;
	UPROPERTY() FString Icao;
	UPROPERTY() FString LiveryMain; UPROPERTY() FString LiveryBody;   // its colours, "#rrggbb" (empty: each vehicle's own; GSEService.cpp SvcLivery)
	UPROPERTY() FString Hub;
	UPROPERTY() bool bPlayer = false;
	UPROPERTY() double Balance = 0.0;
	UPROPERTY() double Reputation = 50.0;   // 0..100
	UPROPERTY() double Quality = 0.75;      // rivals: 0.6..0.95
	UPROPERTY() FString Kind = TEXT("major");   // major | regional | bush
	UPROPERTY() FDateTime RedSince;        // in the red since (AI airlines go bankrupt after too long)
	UPROPERTY() double OnTimeEma = 0.85;   // share of legs on time, smoothed (reputation follows it)
	UPROPERTY() double PartsStock = 0.0;   // spare parts in stock, USD at list price (to-do 181)
	UPROPERTY() int32 FuelTrucks = 1;      // ground vehicles at your airport (to-do 182)
	UPROPERTY() int32 CateringTrucks = 1;
	UPROPERTY() int32 Tractors = 1;
	UPROPERTY() TArray<FADPSpare> Spares;   // bought ones, each on its place at the vehicle area (to-do 277)
	// network and markets (to-do 184-186)
	UPROPERTY() bool bFrequentFlyer = false;
	UPROPERTY() double FfMembers = 0.0;
	UPROPERTY() double SharePrice = 10.0;   // USD
	UPROPERTY() double SharesM = 50.0;      // million shares
	UPROPERTY() bool bPublic = false;
	UPROPERTY() double FloatShare = 0.0;    // part sold at the IPO
	UPROPERTY() double Pax30 = 0.0;         // passengers, about the last 30 days
	UPROPERTY() double Legs30 = 0.0;
	UPROPERTY() double ProfitEma30 = 0.0;   // profit, about the last 30 days
	UPROPERTY() double PrevBalance = 0.0;
	UPROPERTY() double HedgeKg = 0.0;       // hedged fuel left
	UPROPERTY() double HedgePrice = 0.0;    // its price, USD/kg
	UPROPERTY() FDateTime HedgeUntil;
	UPROPERTY() double WeekPax = 0.0;       // this week (the challenges)
	UPROPERTY() int32 WeekLegs = 0;
	UPROPERTY() int32 WeekOnTime = 0;
	UPROPERTY() int32 FireTrucks = 2;       // your airport's fire station (to-do 197): its crash tenders
};

/** Your airport's neighbours and the night curfew (to-do 199). */
USTRUCT() struct FADPTownState
{
	GENERATED_BODY()
	UPROPERTY() bool bStarted = false;
	UPROPERTY() double CurfewFrom = 0.0;   // local hours (From == To: no curfew)
	UPROPERTY() double CurfewTo = 6.0;
	UPROPERTY() double Mood = 55.0;        // 0..100
	UPROPERTY() double Complaints = 0.0;   // this month's, fading
	UPROPERTY() int32 Insulated = 0;       // homes insulated against noise
	UPROPERTY() FDateTime LobbyUntil;
	UPROPERTY() FString Syndic;
	UPROPERTY() double Bribes = 0.0;
	UPROPERTY() FDateTime LastBribe;
	UPROPERTY() bool bScandal = false;
};

/** An alliance of airlines (to-do 184). */
USTRUCT() struct FADPAlliance
{
	GENERATED_BODY()
	UPROPERTY() FString Name;
	UPROPERTY() FString Founder;
	UPROPERTY() TArray<FString> Members;
};

/** A weekly challenge (to-do 186). */
USTRUCT() struct FADPChallenge
{
	GENERATED_BODY()
	UPROPERTY() FString Kind;      // pax | ontime | legs | airports | profit
	UPROPERTY() FString Title;
	UPROPERTY() double Target = 0.0;
	UPROPERTY() double Base = 0.0;
	UPROPERTY() double Progress = 0.0;
	UPROPERTY() double Money = 0.0;
	UPROPERTY() double Xp = 0.0;
	UPROPERTY() bool bDone = false;
	UPROPERTY() FDateTime Until;
};

/** A maintenance check done (to-do 181): when, and the aircraft's hours and cycles then. */
USTRUCT() struct FADPCheckRecord
{
	GENERATED_BODY()
	UPROPERTY() FString Kind;              // A | C | D
	UPROPERTY() FDateTime At;
	UPROPERTY() double AtHoursMin = 0.0;
	UPROPERTY() int32 AtCycles = 0;
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
	UPROPERTY() FString PrevRouteId;       // on a job from the board: the route it goes back to
	UPROPERTY() TArray<FString> Crew;      // the crew rostered on it now (captain, first officer, cabin crew)
	UPROPERTY() TArray<FADPCheckRecord> Checks;   // its last A, C and D checks (to-do 181)
	UPROPERTY() FDateTime InService;       // in this airline's service since (the calendar intervals count from here)
	UPROPERTY() FString CheckKind;         // the check it is in now
	UPROPERTY() FDateTime CheckUntil;
	UPROPERTY() FString Stand;             // its stand at our airport (to-do 183; empty: none / a remote stand)
	UPROPERTY() FDateTime StandUntil;
	// its last arrival at our airport (the claim belts, the passengers coming off, the turnaround)
	UPROPERTY() FString ArrFlight;         // the flight number it came in as
	UPROPERTY() FString ArrFrom;
	UPROPERTY() int32 ArrPax = 0;
	UPROPERTY() FDateTime ArrAt;           // on blocks here
};

/** A ground team busy until (to-do 182): one per job and team at an airport. */
USTRUCT() struct FADPTeamSlot
{
	GENERATED_BODY()
	UPROPERTY() FString Icao;
	UPROPERTY() FString Job;
	UPROPERTY() FDateTime Until;
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
	UPROPERTY() FString Face;              // their body in the 3D airport (a built MetaHuman; ground staff, to-do 206 - StaffPeople.h)
	UPROPERTY() double DutyMin = 0.0;      // minutes on duty since the last rest (pilots)
	UPROPERTY() FDateTime RestUntil;       // resting until (also: sick until, notice until)
	UPROPERTY() FString Training;          // the type rating being trained for (empty: none)
	UPROPERTY() FDateTime TrainingUntil;
	UPROPERTY() int32 Warnings = 0;        // conduct warnings on their record
	// the roster (to-do 177, EASA flight time limitations)
	UPROPERTY() FDateTime DutyStart;       // this flight duty period began (empty: off duty)
	UPROPERTY() int32 Sectors = 0;         // legs flown in this duty
	UPROPERTY() FString WithReg;           // the aircraft they are crewing now
	UPROPERTY() FString Location;          // where they are (their base, or a night stop away)
	UPROPERTY() TArray<double> FlyDays;    // flight minutes per day, the last 28 days (newest last)
	UPROPERTY() TArray<double> FlyWeeks;   // flight minutes per week, the last 52 weeks (newest last)
	UPROPERTY() int32 DayKey = 0;          // the day FlyDays' last entry is for (days since 2000)
	UPROPERTY() int32 DaysOn = 0;          // days with duty since the last days off
	UPROPERTY() int32 LastDutyDay = 0;     // the last day they had a duty (days since 2000)
	UPROPERTY() FDateTime OffUntil;        // days off until (rostered or asked for in the Overview)
	// careers, candidates, contracts (to-do 178-180)
	UPROPERTY() FString Rank;              // Captain, First officer, Senior first officer, Cabin crew, Senior cabin crew, Purser...
	UPROPERTY() double HoursTotal = 0.0;   // flight hours in their whole career
	UPROPERTY() TArray<FString> Languages;
	UPROPERTY() FString Strengths;         // shown once interviewed
	UPROPERTY() FString Weaknesses;
	UPROPERTY() bool bInterviewed = false;
	UPROPERTY() double Asks = 0.0;         // the weekly pay they ask for (a candidate)
	UPROPERTY() FString Contract = TEXT("full");   // full | part (part time: 55% pay, at most 3 duty days in a row)
	UPROPERTY() int32 NoticeDays = 30;     // the notice period of their contract
	UPROPERTY() FDateTime HiredAt;
	UPROPERTY() FDateTime PassedOverSince; // due for promotion but none given since
	UPROPERTY() FDateTime RaiseAskedAt;
};

/** A holiday asked for (to-do 180): approved or refused in the Overview (unanswered after 7 days: approved). */
USTRUCT() struct FADPLeaveRequest
{
	GENERATED_BODY()
	UPROPERTY() FString PersonId;
	UPROPERTY() FDateTime From;
	UPROPERTY() int32 Days = 7;
	UPROPERTY() FDateTime AskedAt;
	UPROPERTY() FString State = TEXT("asked");   // asked | approved | refused | taken
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
	UPROPERTY() FDateTime OpenedAt;
	UPROPERTY() int32 Legs = 0;            // legs flown on it
	UPROPERTY() double ProfitEma = 0.0;    // each leg's profit, smoothed (the newest counts most)
	UPROPERTY() double LoadEma = 0.0;      // how full, smoothed
	UPROPERTY() FDateTime FareUntil;       // a fare war until (then back to normal)
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
	UPROPERTY() FString RouteId;
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
	UPROPERTY() TArray<FString> Crew;       // who flies it (to-do 177)
	UPROPERTY() int32 PaxConnect = 0;       // of its passengers, changing to/from another of the airline's flights (to-do 184)
	UPROPERTY() FString FromStand;          // the stand it pushed back from at our airport (the 3D traffic: to-do 191)
	UPROPERTY() FString FlightNo;           // its number on the boards ("CHB 101")
};

/** A line of your timetable (Sim/ADPSchedule.h): an aircraft flies from your base to a place and back, at this time on these days. */
USTRUCT() struct FADPSlot
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString AirlineId;
	UPROPERTY() FString Reg;
	UPROPERTY() FString RouteId;
	UPROPERTY() FString To;
	UPROPERTY() int32 DepMin = 480;        // off-blocks at the base, minutes of the local day
	UPROPERTY() int32 Days = 127;          // bit 0 Monday .. bit 6 Sunday
	UPROPERTY() int32 Number = 101;        // the flight number out (the way back: the next one)
	UPROPERTY() FDateTime OnSale;          // sold since
};

/** One planned leg: on sale, booked, checked in, boarded - until it leaves (then it is an FADPFlight with the same id). */
USTRUCT() struct FADPPlanned
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString SlotId;            // of which timetable line (empty: an airline flying its route round and round)
	UPROPERTY() bool bReturn = false;      // the way back to the base
	UPROPERTY() FString Reg;
	UPROPERTY() FString AirlineId;
	UPROPERTY() FString RouteId;
	UPROPERTY() FString FlightNo;
	UPROPERTY() FString From;
	UPROPERTY() FString To;
	UPROPERTY() FDateTime Sched;           // scheduled off-blocks
	UPROPERTY() FDateTime Est;             // expected off-blocks (Sched, or later)
	UPROPERTY() FDateTime Hold;            // not before (a delay)
	UPROPERTY() FDateTime OnSale;
	UPROPERTY() double BlockMin = 60.0;
	UPROPERTY() bool bTarget = false;      // how many will have booked is worked out
	UPROPERTY() int32 PaxT = 0;            // ... all classes, when the booking closes
	UPROPERTY() int32 PaxJT = 0;
	UPROPERTY() int32 PaxFT = 0;
	UPROPERTY() int32 PaxConnectT = 0;
	UPROPERTY() double CargoT = 0.0;
	UPROPERTY() double RevenueT = 0.0;
	UPROPERTY() double LoadT = 0.0;
	UPROPERTY() int32 Booked = 0;          // booked so far
	UPROPERTY() int32 NoShow = 0;          // booked, but not aboard when the doors closed
	UPROPERTY() bool bRolled = false;      // its odd minutes of delay are drawn
	UPROPERTY() FString Stand;             // its stand at our airport, set aside from 2.5 h before
	UPROPERTY() FString State = TEXT("planned");   // planned | gone | cancelled
	UPROPERTY() FString Why;               // what it waits for
	UPROPERTY() FDateTime Ended;           // gone / cancelled at
	int32 Pax() const { return PaxT + PaxJT + PaxFT; }
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
	UPROPERTY() int32 Priority = 0;       // 0 info, 1 important, 2 urgent
	UPROPERTY() FString Title;
	UPROPERTY() bool bSent = false;       // handed to the phone's Messages (to-do 175)
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

/** A staff group's union (one per airline and job): grievance, warnings, strike. */
USTRUCT() struct FADPUnion
{
	GENERATED_BODY()
	UPROPERTY() FString AirlineId;
	UPROPERTY() FString Role;
	UPROPERTY() int32 Stage = 0;           // 0 calm, 1 grievance, 2 warning, 3 final warning, 4 on strike
	UPROPERTY() FDateTime Since;           // in this stage since
	UPROPERTY() FDateTime Cooldown;        // no new dispute before
};

/** A court case (conduct that broke the law; later: accidents, the airline itself). */
USTRUCT() struct FADPCase
{
	GENERATED_BODY()
	UPROPERTY() FString AirlineId;
	UPROPERTY() FString PersonId;
	UPROPERTY() FString What;
	UPROPERTY() FDateTime Hearing;
	UPROPERTY() FString Outcome;           // empty until the hearing
};

/** The whole simulated world of one career. */
/** Something happening in the world: ash, a strike, a big event, a health scare (part 8). */
USTRUCT() struct FADPWorldEvent
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString Kind;             // volcano | strike | festival | health
	UPROPERTY() FString Title;
	UPROPERTY() double Lat = 0.0;
	UPROPERTY() double Lon = 0.0;
	UPROPERTY() double RadiusNm = 0.0;
	UPROPERTY() FString Country;          // (a whole country instead of a circle)
	UPROPERTY() FDateTime From;
	UPROPERTY() FDateTime Until;
	UPROPERTY() double DemandMul = 1.0;
	UPROPERTY() bool bClosed = false;     // airports in it closed
	UPROPERTY() bool bStarted = false;
};

USTRUCT() struct FADPNews
{
	GENERATED_BODY()
	UPROPERTY() FDateTime At;
	UPROPERTY() FString Topic;            // world | market | rival | airport
	UPROPERTY() FString Headline;
	UPROPERTY() bool bSent = false;       // handed to the phone's Messages (to-do 175)
};

/** A job on the board: a charter, cargo or a medical flight, paid on arrival. */
USTRUCT() struct FADPJob
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString Kind;             // charter | cargo | medical
	UPROPERTY() FString Title;
	UPROPERTY() FString From;
	UPROPERTY() FString To;
	UPROPERTY() int32 Pax = 0;
	UPROPERTY() int32 CargoKg = 0;
	UPROPERTY() double Pay = 0.0;
	UPROPERTY() FDateTime Posted;
	UPROPERTY() FDateTime Expires;        // off the board if nobody takes it
	UPROPERTY() FDateTime Deadline;       // half pay after it
	UPROPERTY() FString TakenBy;          // the aircraft
	UPROPERTY() bool bDone = false;
	UPROPERTY() FDateTime DoneAt;
};

/** Your career: XP and rank, the numbers behind the badges, badges and achievements, the day's figures for the briefing. */
USTRUCT() struct FADPCareer
{
	GENERATED_BODY()
	UPROPERTY() double Xp = 0.0;
	UPROPERTY() int32 Rank = 0;
	UPROPERTY() int64 Legs = 0;
	UPROPERTY() int64 Pax = 0;
	UPROPERTY() double Nm = 0.0;
	UPROPERTY() double Revenue = 0.0;
	UPROPERTY() double Hours = 0.0;
	UPROPERTY() int32 OnTimeStreak = 0;
	UPROPERTY() int32 BestStreak = 0;
	UPROPERTY() int32 JobsDone = 0;
	UPROPERTY() TArray<FString> Airports;
	UPROPERTY() TArray<FString> Countries;
	UPROPERTY() TArray<FString> Badges;
	UPROPERTY() TArray<FString> Achievements;
	UPROPERTY() TMap<FString, int32> TypeLegs;
	UPROPERTY() int32 DayLegs = 0;
	UPROPERTY() int64 DayPax = 0;
	UPROPERTY() double DayRevenue = 0.0;
	UPROPERTY() int32 DayOnTime = 0;
	UPROPERTY() FDateTime Started;
	UPROPERTY() FDateTime LastBriefing;
	UPROPERTY() int32 Anniversaries = 0;
};

/** A ground vehicle's wear and, broken down, what it has (to-do 277 E2, GSEBreak.cpp). State: 0 well, 2 being repaired, 3 written off, 4 gone, its new one on its way. */
USTRUCT() struct FADPVehWear
{
	GENERATED_BODY()
	UPROPERTY() FString Id;        // own:<name>, or <owner>:<kind@place> of a bought one
	UPROPERTY() double Hours = 0.0;
	UPROPERTY() double Km = 0.0;
	UPROPERTY() double Soc = -1.0;   // (an electric one: its battery, 0..1)
	UPROPERTY() int32 State = 0;
	UPROPERTY() FString Fault;
	UPROPERTY() FDateTime Until;
	UPROPERTY() FDateTime LastFail;
};

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
	UPROPERTY() TArray<FADPSlot> Slots;        // your timetable
	UPROPERTY() TArray<FADPPlanned> Planned;   // the legs to come
	UPROPERTY() int32 NextFlightNo = 101;
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
	UPROPERTY() TArray<FADPUnion> Unions;
	UPROPERTY() TArray<FADPCase> Cases;
	UPROPERTY() int32 ConductLevel = 2;    // 0 none .. 4 often (Settings > Staff)
	UPROPERTY() TArray<FADPWorldEvent> Events;
	UPROPERTY() TArray<FADPNews> News;
	UPROPERTY() TArray<FADPJob> Jobs;
	UPROPERTY() int32 NextJobId = 1;
	UPROPERTY() FADPCareer Career;
	UPROPERTY() TMap<FString, FDateTime> ModuleTimes;   // where each module had got to
	UPROPERTY() TArray<FADPLeaveRequest> LeaveRequests;
	UPROPERTY() TArray<FADPTeamSlot> TeamSlots;
	UPROPERTY() TArray<FADPAlliance> Alliances;
	UPROPERTY() TMap<FString, double> Holdings;   // your shares in AI airlines (count)
	UPROPERTY() TArray<FADPChallenge> Challenges;
	UPROPERTY() FADPTownState Town;
	UPROPERTY() FDateTime LastGroundNote;
	UPROPERTY() TArray<FADPVehWear> VehWear;
};
