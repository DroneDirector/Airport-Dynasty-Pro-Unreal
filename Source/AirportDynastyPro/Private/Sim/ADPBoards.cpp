// The airport's real timetable - see Sim/ADPBoards.h.
#include "Sim/ADPBoards.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPWorldData.h"
#include "Site.h"
#include "Sim/ADPGround.h"
#include "Dom/JsonObject.h"

namespace
{
	const FColor CBlue(127, 196, 245), CGreen(63, 208, 160), CAmber(242, 181, 68), CGrey(143, 163, 184), CRed(240, 90, 90);
	/** Arrivals that landed in the last 20 minutes (they leave the simulation's flights when they land). */
	struct FLanded { FADPBoardRow Row; FDateTime At; };
	TMap<FString, FLanded> GLanded;
	TMap<FString, FADPBoardRow> GLastArr;

	FString Town(const FString& Icao, FString& Code)
	{
		const FADPAirportInfo* A = ADPWorldData::Airport(Icao);
		Code = A && !A->Iata.IsEmpty() ? A->Iata : Icao;
		if (!A) return Icao;
		return A->Town.IsEmpty() ? A->Name : A->Town;
	}
	FString CodeOf(const FADPWorld& W, const FString& AirlineId)
	{
		for (const FADPAirline& A : W.Airlines) if (A.Id == AirlineId) return A.Icao.IsEmpty() ? A.Name.Left(3).ToUpper() : A.Icao;
		return TEXT("XX");
	}
}

FDateTime ADPBoards::Local(const FDateTime& Utc) { return Utc + FTimespan::FromHours(ADPSite::UtcOffsetHours(Utc)); }

FString ADPBoards::HereIcao()
{
	FString Icao = TEXT("BIAP");
	if (TSharedPtr<FJsonObject> A = ADPSite::AirportBlock()) A->TryGetStringField(TEXT("icao"), Icao);
	return Icao;
}

bool ADPBoards::Live(const UObject* Ctx)
{
	const UADPSim* S = UADPSim::Get(Ctx);
	return S && S->IsRunning() && ADPWorldData::IsReady();
}

int32 ADPBoards::GateFor(const FString& Reg)
{
	// (an aircraft on a remote stand boards by bus from one of the bus gates - the gates without a jetway)
	static const int32 BusGates[] = {5, 6, 7, 8, 10, 12};
	return BusGates[GetTypeHash(Reg) % UE_ARRAY_COUNT(BusGates)];
}

FString ADPBoards::FlightNumber(const FString& AirlineCode, const FString& RouteId, int32 LegIdx)
{
	// (each route its own hundred, each leg of it one number: even outbound, odd back - as airlines number them)
	const int32 N = 100 + int32(GetTypeHash(RouteId) % 89) * 10 + FMath::Clamp(LegIdx, 0, 9);
	return FString::Printf(TEXT("%s %d"), *AirlineCode, N);
}

TArray<FADPBoardRow> ADPBoards::Rows(const UObject* Ctx, bool bArr, int32 N)
{
	TArray<FADPBoardRow> Out;
	const UADPSim* S = UADPSim::Get(Ctx);
	if (!S || !Live(Ctx)) return Out;
	const FADPWorld& W = S->World;
	const FString Here = HereIcao();
	const FDateTime NowU = S->Now(), NowL = Local(NowU);
	auto Mins = [&](const FDateTime& T) { return (T - NowU).GetTotalMinutes(); };
	if (!bArr)
	{
		// the next leg of every aircraft standing here with a route
		for (const FADPAircraft& A : W.Aircraft)
		{
			if (A.Location != Here || A.Status != TEXT("parked") || A.RouteId.IsEmpty()) continue;
			const FADPRoute* R = W.Routes.FindByPredicate([&](const FADPRoute& X) { return X.Id == A.RouteId; });
			if (!R || !R->Stops.Num()) continue;
			const FString To = R->Stops[A.LegIdx % R->Stops.Num()];
			if (To == Here) continue;
			FADPBoardRow Row; Row.Reg = A.Reg; Row.AirlineId = A.AirlineId; Row.Gate = ADPGround::GateOfStand(A.Stand);   // (its stand: to-do 183)
			if (Row.Gate == 0) Row.Gate = GateFor(A.Reg);   // (a remote stand: a bus gate)
			Row.Flight = FlightNumber(CodeOf(W, A.AirlineId), R->Id, A.LegIdx);
			Row.Place = Town(To, Row.PlaceCode);
			const bool bWaiting = A.WaitingSince != FDateTime() || A.ReadyAt < NowU - FTimespan::FromMinutes(1);
			Row.Sched = Local(bWaiting && A.WaitingSince != FDateTime() ? A.WaitingSince : A.ReadyAt);
			Row.Expected = Local(A.ReadyAt > NowU ? A.ReadyAt : NowU + FTimespan::FromMinutes(30));
			const double M = Mins(A.ReadyAt);
			if (bWaiting) { Row.Status = TEXT("Delayed"); Row.Col = CAmber; }
			else if (M > 120) { Row.Status = TEXT("Scheduled"); }
			else if (M > 45) { Row.Status = TEXT("Check-in"); }
			else if (M > 35) { Row.Status = TEXT("Gate open"); Row.Col = CGreen; }
			else if (M > 15) { Row.Status = TEXT("Boarding"); Row.Col = CGreen; Row.bBoarding = true; }
			else if (M > 10) { Row.Status = TEXT("Final call"); Row.Col = CGreen; Row.bBoarding = Row.bFinalCall = true; }
			else { Row.Status = TEXT("Gate closed"); Row.Col = CGrey; }
			Out.Add(Row);
		}
		// just departed (the last 20 minutes)
		for (const FADPFlight& F : W.Flights)
		{
			if (F.From != Here || Mins(F.DepAt) < -20) continue;
			FADPBoardRow Row; Row.Reg = F.Reg; Row.AirlineId = F.AirlineId; Row.Gate = 0;
			const FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == F.Reg; });
			Row.Flight = FlightNumber(CodeOf(W, F.AirlineId), F.RouteId, A ? FMath::Max(0, A->LegIdx - 1) : 0);
			Row.Place = Town(F.To, Row.PlaceCode);
			Row.Sched = Local(F.DepAt - FTimespan::FromMinutes(F.DelayMin)); Row.Expected = Local(F.DepAt);
			Row.Status = FString::Printf(TEXT("Departed %02d:%02d"), Row.Expected.GetHour(), Row.Expected.GetMinute()); Row.Col = CGrey;
			Out.Add(Row);
		}
	}
	else
	{
		TSet<FString> Seen;
		for (const FADPFlight& F : W.Flights)
		{
			const bool bToHere = F.To == Here || F.Diverted == Here;
			if (!bToHere) continue;
			FADPBoardRow Row; Row.Reg = F.Reg; Row.AirlineId = F.AirlineId;
			const FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == F.Reg; });
			Row.Gate = A ? ADPGround::GateOfStand(A->Stand) : 0;
			Row.Flight = FlightNumber(CodeOf(W, F.AirlineId), F.RouteId, A ? FMath::Max(0, A->LegIdx - 1) : 0);
			Row.Place = Town(F.From, Row.PlaceCode);
			const FDateTime Eta = F.DepAt + FTimespan::FromMinutes(F.DurMin + F.HoldMin);
			Row.Sched = Local(F.DepAt - FTimespan::FromMinutes(F.DelayMin) + FTimespan::FromMinutes(F.DurMin)); Row.Expected = Local(Eta);
			const double Late = (Row.Expected - Row.Sched).GetTotalMinutes();
			if (F.Diverted == Here) { Row.Status = TEXT("Diverted here"); Row.Col = CAmber; }
			else if (!F.Diverted.IsEmpty()) { Row.Status = TEXT("Diverted"); Row.Col = CRed; }
			else if (F.Phase.Contains(TEXT("Land")) || F.Phase.Contains(TEXT("Taxi in"))) { Row.Status = TEXT("Landed"); Row.Col = CGreen; }
			else if (Late >= 15) { Row.Status = FString::Printf(TEXT("Expected %02d:%02d"), Row.Expected.GetHour(), Row.Expected.GetMinute()); Row.Col = CAmber; }
			else if (Mins(Eta) < 30) { Row.Status = TEXT("Approaching"); }
			else { Row.Status = TEXT("On time"); }
			Out.Add(Row); Seen.Add(F.Id); GLastArr.Add(F.Id, Row);
		}
		// landed: an arrival that left the flights since the last look stays 20 minutes as Landed
		for (auto It = GLastArr.CreateIterator(); It; ++It)
			if (!Seen.Contains(It->Key)) { FLanded L{It->Value, NowU}; L.Row.Status = FString::Printf(TEXT("Landed %02d:%02d"), NowL.GetHour(), NowL.GetMinute()); L.Row.Col = CGreen; GLanded.Add(It->Key, L); It.RemoveCurrent(); }
		for (auto It = GLanded.CreateIterator(); It; ++It)
		{
			if ((NowU - It->Value.At).GetTotalMinutes() > 20) { It.RemoveCurrent(); continue; }
			Out.Add(It->Value.Row);
		}
	}
	Out.Sort([](const FADPBoardRow& A, const FADPBoardRow& B) { return A.Sched < B.Sched; });
	if (Out.Num() > N) Out.SetNum(N);
	return Out;
}

bool ADPBoards::AtGate(const UObject* Ctx, int32 Gate, FADPBoardRow& Out)
{
	for (const FADPBoardRow& R : Rows(Ctx, false, 64))
		if (R.Gate == Gate && !R.Status.StartsWith(TEXT("Departed"))) { Out = R; return true; }
	return false;
}
