// What the talk card shows of a passenger (UI/TalkCard.h): their own flight and how far they are, from the passenger flow's
// own records and the airport's one timetable - nothing made up; what isn't known is left out.
#include "PaxFlow.h"
#include "BaggageBelts.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPSimData.h"
#include "Sim/ADPWorldData.h"
#include "UI/TalkCard.h"
#include "Engine/World.h"

namespace
{
	FString PcCode(const FADPAirportInfo* A, const FString& Icao) { return A && !A->Iata.IsEmpty() ? A->Iata : Icao; }
	FString PcTown(const FADPAirportInfo* A) { return !A ? FString() : !A->Town.IsEmpty() ? A->Town : A->Name; }
	FString PcTime(const FDateTime& T) { return FString::Printf(TEXT("%02d:%02d"), T.GetHour(), T.GetMinute()); }
}

bool APaxFlow::CardOf(UWorld* W, const AActor* Who, FTalkCard& C)
{
	const APaxFlow* F = Who ? Find(W) : nullptr; if (!F) return false;
	const FPx* P = F->Pax.FindByPredicate([Who](const FPx& X) { return X.St != EPx::Gone && X.A.Get() == Who; });
	if (!P || !F->Runs.IsValidIndex(P->Run)) return false;   // (somebody about the terminal without a flight)
	const FRun& R = F->Runs[P->Run];
	const FLinearColor Amber(FColor(255, 178, 62));
	FADPBoardRow Row; bool bRow = false;
	const TArray<FADPBoardRow> Rows = ADPBoards::Rows(F, P->bArr, 60);
	if (const FADPBoardRow* X = Rows.FindByPredicate([&](const FADPBoardRow& B) { return !R.PlanId.IsEmpty() && B.PlanId == R.PlanId; })) { Row = *X; bRow = true; }
	else if (const FADPBoardRow* Y = Rows.FindByPredicate([&](const FADPBoardRow& B) { return !R.Reg.IsEmpty() && B.Reg == R.Reg; })) { Row = *Y; bRow = true; }

	if (bRow)
	{
		const FString Icao = ADPBoards::HereIcao(); const FADPAirportInfo* Here = ADPWorldData::Airport(Icao);
		const FString HC = PcCode(Here, Icao), HN = PcTown(Here);
		C.bTrip = !Row.PlaceCode.IsEmpty();
		if (P->bArr) { C.FromCode = Row.PlaceCode; C.FromName = Row.Place; C.ToCode = HC; C.ToName = HN; }
		else { C.FromCode = HC; C.FromName = HN; C.ToCode = Row.PlaceCode; C.ToName = Row.Place; }
	}
	auto Cell = [&C](const TCHAR* K, const FString& V) { if (!V.IsEmpty()) C.Cells.Add({FString(K), V}); };
	Cell(TEXT("FLIGHT"), bRow && !Row.Flight.IsEmpty() ? Row.Flight : R.Label);
	const bool bLate = bRow && FMath::Abs((Row.Expected - Row.Sched).GetTotalMinutes()) >= 5.0;
	if (!P->bArr)
	{
		if (bRow) Cell(TEXT("DEPARTS"), PcTime(Row.Sched));
		if (bLate) Cell(TEXT("NEW TIME"), PcTime(Row.Expected));
		if (R.Gate > 0) Cell(TEXT("GATE"), FString::FromInt(R.Gate));
		Cell(TEXT("BOARDING"), R.bOff ? TEXT("Cancelled") : R.bGone || R.bClosed ? TEXT("Closed") : R.bFinal ? TEXT("Final call") : R.bCalled ? TEXT("Now") : TEXT("Not yet"));
		Cell(TEXT("BAG"), P->bBagDown ? TEXT("Checked in") : P->bHasBag ? TEXT("To check in") : TEXT("Hand luggage"));
		Cell(TEXT("SECURITY"), P->Lane < 0 ? TEXT("Not yet") : P->St == EPx::LaneQ || P->Do == EDo::JoinLane ? TEXT("In the queue") : P->Do == EDo::Screened ? TEXT("Now") : TEXT("Passed"));
	}
	else
	{
		if (bRow) Cell(TEXT("ARRIVED"), PcTime(Row.Expected));
		if (P->bArrBag && !P->bHasBag) { const int32 B = ABaggageBelts::BeltOf(W, R.Label); if (B >= 0) Cell(TEXT("BELT"), FString::FromInt(B + 1)); }
		Cell(TEXT("BAG"), P->bHasBag ? TEXT("With them") : P->bArrBag ? TEXT("To collect") : TEXT("Hand luggage"));
	}
	if (UADPSim* Sim = UADPSim::Get(F))
	{
		const FString Reg = !R.Reg.IsEmpty() ? R.Reg : Row.Reg, Al = !R.AirlineId.IsEmpty() ? R.AirlineId : Row.AirlineId;
		if (const FADPAircraft* A = Sim->World.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg; }))
		{
			const auto* T = ADPWorldData::Type(A->TypeId); Cell(TEXT("AIRCRAFT"), T ? T->Name : A->TypeId);
		}
		if (const FADPAirline* A = Sim->World.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Al; })) Cell(TEXT("AIRLINE"), A->Name);
	}
	FString S = P->Stage; if (S.Len()) S[0] = FChar::ToUpper(S[0]);
	C.Status = S;
	if (P->bHome || R.bOff || (!P->bArr && bLate)) C.Dot = Amber;
	return true;
}
