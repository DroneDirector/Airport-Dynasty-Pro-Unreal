// The world the simulation flies in (docs/AI-Simulation-Plan.md part 2): every airport and runway (Data/world-airports.json,
// OurAirports public domain, Tools/make_world_airports.py) and the aircraft types (Data/aircraft-types.json, our own table
// of published figures) - built into the game, loaded once on a background thread - and a flight's profile: its phases,
// times, heights, speeds and position along the great circle, from the type's real figures.
#pragma once

#include "CoreMinimal.h"

struct FADPRunwayInfo
{
	FString Le, He; int32 LengthFt = 0, WidthFt = 0; TCHAR Surface = 'O'; bool bLit = false;
	double LeLat = 0, LeLon = 0, HeLat = 0, HeLon = 0, LeHdg = 0; bool bEnds = false;
};

struct FADPAirportInfo
{
	FString Ident, Icao, Iata, Name, Town, Country; TCHAR Kind = 'S';   // L large, M medium, S small, H heliport, W seaplane base
	double Lat = 0, Lon = 0; int32 ElevFt = 0; bool bScheduled = false;
	TArray<FADPRunwayInfo> Runways;
	int32 LongestFt() const { int32 L = 0; for (const FADPRunwayInfo& R : Runways) L = FMath::Max(L, R.LengthFt); return L; }
};

struct FADPTypeInfo
{
	FString Id, Name, Class, Model; int32 Seats = 0, Engines = 2;
	double Mtow = 0, Mlw = 0, CruiseKt = 0, CruiseMach = 0, CeilingFt = 0, RangeNm = 0, ClimbFpm = 0, DescentFpm = 0;
	double ValueM = 0;   // market value, million USD
	double V2Kt = 0, VappKt = 0, BurnKgH = 0, ToRunwayM = 0, LdRunwayM = 0, TaxiKt = 0, MaxBankDeg = 30, TurnDegS = 3;
};

/** A flight's plan from its type and its two airports: minutes from off-blocks for each phase. */
struct FADPProfile
{
	enum EPhase : uint8 { TaxiOut, Takeoff, Climb, Cruise, Descent, Approach, Landing, TaxiIn, Done };
	double PhaseStart[Done + 1] = {0};   // minutes from off-blocks; [Done] = the block time
	double DistNm = 0, CruiseFt = 0, ClimbNm = 0, DescentNm = 0, ApproachNm = 0;
	double FromLat = 0, FromLon = 0, ToLat = 0, ToLon = 0, FromElevFt = 0, ToElevFt = 0;
	FADPTypeInfo Type;
	double BlockMin() const { return PhaseStart[Done]; }
	/** Where the flight is Min minutes after off-blocks. */
	struct FState { EPhase Phase = TaxiOut; double AlongNm = 0, AltFt = 0, GsKt = 0, Lat = 0, Lon = 0, HeadingDeg = 0, VsFpm = 0; };
	FState At(double Min) const;
	static const TCHAR* PhaseName(EPhase P);
};

namespace ADPWorldData
{
	/** Starts loading (once; a background thread). */
	void Load();
	bool IsReady();
	int32 NumAirports();
	/** By ICAO, IATA or ident. */
	const FADPAirportInfo* Airport(const FString& Code);
	/** Every airport (for searches: alternates, the nearest). */
	void ForEachAirport(TFunctionRef<void(const FADPAirportInfo&)> F);
	const FADPTypeInfo* Type(const FString& Id);
	const TArray<FADPTypeInfo>& Types();
	/** Types with a finished high-quality 3D model (owner 2026-11-21: only those can be bought - for now the A320). */
	bool HasModel(const FString& TypeId);
	double DistanceNm(double Lat1, double Lon1, double Lat2, double Lon2);
	double BearingDeg(double Lat1, double Lon1, double Lat2, double Lon2);
	/** The point a fraction F (0..1) of the way along the great circle. */
	void Along(double Lat1, double Lon1, double Lat2, double Lon2, double F, double& Lat, double& Lon);
	/** The plan of a flight of this type between two airports (false: unknown airport or type). */
	bool MakeProfile(const FString& TypeId, const FString& From, const FString& To, FADPProfile& Out);
}
