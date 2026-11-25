// The airport's real timetable - see Sim/ADPBoards.h.
#include "Sim/ADPBoards.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPWorldData.h"
#include "Site.h"
#include "Sim/ADPGround.h"
#include "PaxFlow.h"
#include "Dom/JsonObject.h"

namespace
{
	const FColor CBlue(127, 196, 245), CGreen(63, 208, 160), CAmber(242, 181, 68), CGrey(143, 163, 184), CRed(240, 90, 90);
	/** Arrivals that landed in the last 20 minutes (they leave the simulation's flights when they land). */
	struct FLanded { FADPBoardRow Row; FDateTime At; };
	TMap<FString, FLanded> GLanded;
	TMap<FString, FADPBoardRow> GLastArr;
	FString GLandSlot; FDateTime GLandAt;   // (whose landed flights those are: another career, or an older save, starts with none)

	FString BdTown(const FString& Icao, FString& Code)
	{
		const FADPAirportInfo* A = ADPWorldData::Airport(Icao);
		Code = A && !A->Iata.IsEmpty() ? A->Iata : Icao;
		if (!A) return Icao;
		return A->Town.IsEmpty() ? A->Name : A->Town;
	}
	FString BdCodeOf(const FADPWorld& W, const FString& AirlineId)
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
	static const int32 BusGates[] = {5, 6, 7, 8};   // (10 and 12 have no door to the apron)
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
	const UWorld* Wd = Ctx ? Ctx->GetWorld() : nullptr;
	auto Mins = [&](const FDateTime& T) { return (T - NowU).GetTotalMinutes(); };
	if (!bArr)
	{
		// the planned legs from here, the next 24 hours (Sim/ADPSchedule.h: your timetable's and the other airlines')
		for (const FADPPlanned& P : W.Planned)
		{
			if (P.From != Here || P.State == TEXT("gone") || Mins(P.Est) > 24 * 60) continue;
			const bool bOff = P.State == TEXT("cancelled");
			if (bOff && (P.Why == TEXT("taken out of the timetable") || Mins(P.Sched) > 180 || Mins(P.Sched) < -120)) continue;
			FADPBoardRow Row; Row.Reg = P.Reg; Row.AirlineId = P.AirlineId; Row.PlanId = P.Id; Row.Gate = ADPGround::GateOfStand(P.Stand);   // (its stand: to-do 183)
			if ((Row.Gate == 0 && Mins(P.Est) < 150) || (Row.Gate > 0 && ADPGround::PaxWay(P.Stand) == EADPPaxWay::Bus)) Row.Gate = GateFor(P.Reg);   // (a stand by bus: a bus gate)
			Row.Flight = P.FlightNo;
			Row.Place = BdTown(P.To, Row.PlaceCode);
			Row.Sched = Local(P.Sched); Row.Expected = Local(FMath::Max(P.Est, NowU));
			const double M = Mins(P.Est);
			const bool bLate = (P.Est - P.Sched).GetTotalMinutes() >= 15.0 || Mins(P.Est) < -5.0;
			if (bOff) { Row.Status = TEXT("Cancelled"); Row.Col = CRed; }
			else if (bLate && (M > 35 || (!P.Why.IsEmpty() && Mins(P.Sched) < -15))) { Row.Status = TEXT("Delayed"); Row.Col = CAmber; }
			else if (M > 150) { Row.Status = TEXT("Scheduled"); }
			else if (M > 45) { Row.Status = TEXT("Check-in"); }
			else if (M > 35) { Row.Status = TEXT("Gate open"); Row.Col = CGreen; }
			else if (M > 15) { Row.Status = TEXT("Boarding"); Row.Col = CGreen; Row.bBoarding = true; }
			else if (M > 10) { Row.Status = TEXT("Final call"); Row.Col = CGreen; Row.bBoarding = Row.bFinalCall = true; }
			else { Row.Status = TEXT("Gate closed"); Row.Col = CGrey; }
			// (its passengers walk here: what really happens at the gate, not the clock - to-do 288)
			if (!bOff && Row.Status != TEXT("Delayed") && M <= 45)
				if (const int32 C = APaxFlow::CallsState(Wd, P.Id); C >= 0)
				{
					Row.bBoarding = C == 1 || C == 2; Row.bFinalCall = C == 2;
					Row.Status = C == 0 ? TEXT("Gate open") : C == 1 ? TEXT("Boarding") : C == 2 ? TEXT("Final call") : TEXT("Gate closed");
					Row.Col = C == 3 ? CGrey : CGreen;
				}
			Out.Add(Row);
		}
		// just departed (the last 20 minutes)
		for (const FADPFlight& F : W.Flights)
		{
			if (F.From != Here || Mins(F.DepAt) < -20) continue;
			FADPBoardRow Row; Row.Reg = F.Reg; Row.AirlineId = F.AirlineId; Row.Gate = 0;
			const FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == F.Reg; });
			Row.Flight = !F.FlightNo.IsEmpty() ? F.FlightNo : FlightNumber(BdCodeOf(W, F.AirlineId), F.RouteId, A ? FMath::Max(0, A->LegIdx - 1) : 0);
			Row.PlanId = F.Id;
			Row.Place = BdTown(F.To, Row.PlaceCode);
			Row.Sched = Local(F.DepAt - FTimespan::FromMinutes(F.DelayMin)); Row.Expected = Local(F.DepAt);
			Row.Status = FString::Printf(TEXT("Departed %02d:%02d"), Row.Expected.GetHour(), Row.Expected.GetMinute()); Row.Col = CGrey;
			Out.Add(Row);
		}
	}
	else
	{
		TSet<FString> Seen;
		if (GLandSlot != W.Slot || NowU < GLandAt - FTimespan::FromMinutes(1.0)) { GLanded.Reset(); GLastArr.Reset(); GLandSlot = W.Slot; }
		GLandAt = NowU;
		for (const FADPFlight& F : W.Flights)
		{
			const bool bToHere = F.To == Here || F.Diverted == Here;
			if (!bToHere) continue;
			FADPBoardRow Row; Row.Reg = F.Reg; Row.AirlineId = F.AirlineId;
			const FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == F.Reg; });
			Row.Gate = A ? ADPGround::GateOfStand(A->Stand) : 0;
			Row.Flight = !F.FlightNo.IsEmpty() ? F.FlightNo : FlightNumber(BdCodeOf(W, F.AirlineId), F.RouteId, A ? FMath::Max(0, A->LegIdx - 1) : 0);
			Row.PlanId = F.Id;
			Row.Place = BdTown(F.From, Row.PlaceCode);
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
			if (!Seen.Contains(It->Key) && It->Value.Status == TEXT("Diverted")) It.RemoveCurrent();   // (it landed somewhere else)
			else if (!Seen.Contains(It->Key)) { FLanded L{It->Value, NowU}; L.Row.Status = FString::Printf(TEXT("Landed %02d:%02d"), NowL.GetHour(), NowL.GetMinute()); L.Row.Col = CGreen; GLanded.Add(It->Key, L); It.RemoveCurrent(); }
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

int32 ADPBoards::Turn() { return int32(FPlatformTime::Seconds() / PageSeconds); }

int32 ADPBoards::Page(int32 Count, int32 Per, int32& Pages)
{
	Pages = FMath::Max(1, FMath::DivideAndRoundUp(Count, FMath::Max(Per, 1)));
	return Turn() % Pages;
}

const TArray<FADPBoardRow>& ADPBoards::Shown(const UObject* Ctx, bool bArr)
{
	struct FHeld { TArray<FADPBoardRow> Rows; int32 Turn = -1; double At = -100.0; };
	static FHeld Held[2];
	FHeld& H = Held[bArr ? 1 : 0];
	if (!Live(Ctx)) { H.Rows.Reset(); H.Turn = -1; return H.Rows; }
	const double T = FPlatformTime::Seconds(); const int32 Tn = Turn();
	if (H.Turn == Tn && T - H.At < 2.0) return H.Rows;
	H.At = T;
	TArray<FADPBoardRow> Fresh = Rows(Ctx, bArr, 60);
	if (H.Turn != Tn || FMath::Max(Fresh.Num(), H.Rows.Num()) <= 5) { H.Rows = MoveTemp(Fresh); H.Turn = Tn; return H.Rows; }
	for (FADPBoardRow& R : H.Rows)
		if (FADPBoardRow* F = Fresh.FindByPredicate([&R](const FADPBoardRow& X) { return (!R.PlanId.IsEmpty() && X.PlanId == R.PlanId) || (X.Flight == R.Flight && X.Reg == R.Reg && X.Sched == R.Sched); })) R = *F;
	return H.Rows;
}

bool ADPBoards::AtGate(const UObject* Ctx, int32 Gate, FADPBoardRow& Out)
{
	for (const FADPBoardRow& R : Rows(Ctx, false, 64))
		if (R.Gate == Gate && !R.Status.StartsWith(TEXT("Departed")) && R.Status != TEXT("Cancelled")) { Out = R; return true; }
	return false;
}
