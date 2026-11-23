// The timetable and the planned legs - see Sim/ADPSchedule.h.
#include "Sim/ADPSchedule.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPDemand.h"
#include "Sim/ADPEvents.h"
#include "Sim/ADPFlightsModule.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPNetwork.h"
#include "Sim/ADPStaff.h"
#include "Sim/ADPWorldData.h"
#include "ADPLog.h"
#include "Site.h"

TFunction<bool(const FADPPlanned&, const FDateTime&, FString&)> ADPSchedule::Ready3D;
TFunction<FString()> ADPSchedule::Waiting3D;

namespace
{
	constexpr double TurnAwayMin = 50.0;      // at the other end
	constexpr double TurnHomeMin = 50.0;      // at the base, the least between two of an aircraft's lines
	constexpr double CloseMin = 130.0;        // the booking closes before check-in opens (so everyone who comes is known)
	constexpr double LeadMin = 150.0;         // the first departure of a new line, the earliest
	FDateTime ScUtc(const FDateTime& Local)   // a local time at our airport as UTC
	{
		const FDateTime U = Local - FTimespan::FromHours(ADPSite::UtcOffsetHours(Local));
		return Local - FTimespan::FromHours(ADPSite::UtcOffsetHours(U));
	}
	FString ScCode(const FADPWorld& W, const FString& AirlineId)
	{
		for (const FADPAirline& A : W.Airlines) if (A.Id == AirlineId) return A.Icao.IsEmpty() ? A.Name.Left(3).ToUpper() : A.Icao;
		return TEXT("XX");
	}
	FString ScTown(const FString& Icao) { const FADPAirportInfo* A = ADPWorldData::Airport(Icao); return !A ? Icao : A->Town.IsEmpty() ? A->Name : A->Town; }
	bool ScLive(const FADPPlanned& P) { return P.State == TEXT("planned"); }
	FRandomStream& ScRng() { static FRandomStream R(1123); return R; }

	/** How many will have booked when the booking closes: the demand on the pair that day, less for a flight put on sale late. */
	void ScTarget(FADPWorld& W, FADPPlanned& P, const FADPTypeInfo& T)
	{
		P.bTarget = true;
		const FADPAirportInfo* A = ADPWorldData::Airport(P.From); const FADPAirportInfo* B = ADPWorldData::Airport(P.To);
		if (!A || !B) return;
		FADPFlight F; F.AirlineId = P.AirlineId; F.RouteId = P.RouteId; F.From = P.From; F.To = P.To; F.TypeId = T.Id;
		if (!ADPEvents::JobLoad(W, P.RouteId, P.To, F))
		{
			const FADPBooking K = ADPDemand::Book(W, P.AirlineId, P.RouteId, T, *A, *B, P.Sched, ScRng());
			// (sold for a day or more: everyone who wants to go; put on sale a few hours before: fewer have seen it)
			const double OnSaleH = (P.Sched - P.OnSale).GetTotalHours();
			const double Sold = P.AirlineId == UADPSim::Me() && !P.SlotId.IsEmpty() ? FMath::Clamp(0.4 + 0.6 * OnSaleH / 24.0, 0.4, 1.0) : 1.0;
			F.Pax = FMath::RoundToInt(K.Pax * Sold); F.PaxJ = FMath::RoundToInt(K.PaxJ * Sold); F.PaxF = FMath::RoundToInt(K.PaxF * Sold);
			F.CargoKg = K.CargoKg; F.Revenue = K.Revenue * Sold; F.LoadFactor = K.LoadFactor * Sold;
			ADPNetwork::Boost(W, F, T.Seats);
		}
		P.PaxT = F.Pax; P.PaxJT = F.PaxJ; P.PaxFT = F.PaxF; P.PaxConnectT = F.PaxConnect; P.CargoT = F.CargoKg; P.RevenueT = F.Revenue; P.LoadT = F.LoadFactor;
	}
}

bool ADPSchedule::Schengen(const FString& Country)
{
	static const TSet<FString> S = {TEXT("AT"), TEXT("BE"), TEXT("BG"), TEXT("HR"), TEXT("CZ"), TEXT("DK"), TEXT("EE"), TEXT("FI"), TEXT("FR"), TEXT("DE"), TEXT("GR"), TEXT("HU"), TEXT("IS"), TEXT("IT"), TEXT("LV"), TEXT("LI"),
		TEXT("LT"), TEXT("LU"), TEXT("MT"), TEXT("NL"), TEXT("NO"), TEXT("PL"), TEXT("PT"), TEXT("RO"), TEXT("SK"), TEXT("SI"), TEXT("ES"), TEXT("SE"), TEXT("CH")};
	return S.Contains(Country);
}

double ADPSchedule::Block(const FString& TypeId, const FString& From, const FString& To)
{
	static TMap<FString, double> Kept;
	const FString K = TypeId + TEXT("|") + From + TEXT("|") + To;
	if (const double* V = Kept.Find(K)) return *V;
	FADPProfile P; const double M = ADPWorldData::MakeProfile(TypeId, From, To, P) ? P.BlockMin() : 0.0;
	return Kept.Add(K, M);
}

bool ADPSchedule::CanReach(const FString& TypeId, const FString& From, const FString& To, FString& Why)
{
	const FADPTypeInfo* T = ADPWorldData::Type(TypeId); const FADPAirportInfo* A = ADPWorldData::Airport(From); const FADPAirportInfo* B = ADPWorldData::Airport(To);
	if (!T || !A || !B) { Why = TEXT("unknown airport"); return false; }
	if (From == To) { Why = TEXT("that is its base"); return false; }
	const double D = ADPWorldData::DistanceNm(A->Lat, A->Lon, B->Lat, B->Lon);
	if (D > T->RangeNm * 0.85) { Why = FString::Printf(TEXT("%.0f nm - too far for the %s"), D, *T->Name); return false; }
	if (D < 60.0) { Why = TEXT("too near - nobody would fly it"); return false; }
	const double Need = FMath::Max(T->ToRunwayM, T->LdRunwayM) * 3.281;
	if (B->LongestFt() < Need) { Why = FString::Printf(TEXT("its longest runway, %.0f m, is too short for the %s"), B->LongestFt() / 3.281, *T->Name); return false; }
	if (Block(TypeId, From, To) <= 0.0) { Why = TEXT("no flight plan there"); return false; }
	return true;
}

bool ADPSchedule::HasSlots(const FADPWorld& W, const FString& Reg) { return W.Slots.ContainsByPredicate([&](const FADPSlot& S) { return S.Reg == Reg; }); }

bool ADPSchedule::AddSlot(FADPWorld& W, const FString& Reg, const FString& To, int32 DepMin, int32 Days, const FDateTime& Now, FString& Why)
{
	FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == Reg && X.AirlineId == UADPSim::Me(); });
	if (!A) { Why = TEXT("not your aircraft"); return false; }
	Days &= 127; if (!Days) { Why = TEXT("no day chosen"); return false; }
	DepMin = FMath::Clamp(DepMin, 0, 1439);
	if (!CanReach(A->TypeId, A->Base, To, Why)) return false;
	const double Len = Block(A->TypeId, A->Base, To) + TurnAwayMin + Block(A->TypeId, To, A->Base) + TurnHomeMin;
	// the night curfew, out and back
	{
		const FADPAirportInfo* Ap = ADPWorldData::Airport(A->Base);
		const FDateTime Day = ADPBoards::Local(Now).GetDate() + FTimespan::FromDays(1.0);
		const FDateTime Dep = ScUtc(Day + FTimespan::FromMinutes(DepMin)), Back = Dep + FTimespan::FromMinutes(Len - TurnHomeMin);
		if (Ap && FADPFlightsModule::InCurfew(Ap->Icao, Ap->Lon, Dep)) { Why = TEXT("the airport is closed then (night curfew)"); return false; }
		if (Ap && FADPFlightsModule::InCurfew(Ap->Icao, Ap->Lon, Back)) { Why = FString::Printf(TEXT("it would be back at %s, in the night curfew"), *ADPBoards::Local(Back).ToString(TEXT("%H:%M"))); return false; }
	}
	// never two lines of one aircraft at once (minutes of the week)
	for (const FADPSlot& S : W.Slots)
	{
		if (S.Reg != Reg) continue;
		const double L2 = Block(A->TypeId, A->Base, S.To) + TurnAwayMin + Block(A->TypeId, S.To, A->Base) + TurnHomeMin;
		for (int32 d = 0; d < 7; ++d) for (int32 e = 0; e < 7; ++e)
		{
			if (!(Days & (1 << d)) || !(S.Days & (1 << e))) continue;
			const double P = d * 1440.0 + DepMin, Q = e * 1440.0 + S.DepMin;
			const double Gap = FMath::Fmod(Q - P + 2 * 10080.0, 10080.0);   // (from this one's departure to the other's)
			if (Gap < Len || 10080.0 - Gap < L2) { Why = FString::Printf(TEXT("%s is away on its %02d:%02d to %s then"), *Reg, S.DepMin / 60, S.DepMin % 60, *ScTown(S.To)); return false; }
		}
	}
	FADPRoute* R = W.Routes.FindByPredicate([&](const FADPRoute& X) { return X.AirlineId == UADPSim::Me() && X.Stops.Num() == 2 && X.Stops[0] == A->Base && X.Stops[1] == To && X.TypeId == A->TypeId; });
	if (!R)
	{
		FADPRoute N; N.Id = FString::Printf(TEXT("me-%s-%s-%s"), *A->Base, *To, *A->TypeId); N.AirlineId = UADPSim::Me(); N.Stops = {A->Base, To}; N.TypeId = A->TypeId; N.OpenedAt = Now;
		R = &W.Routes.Add_GetRef(N);
	}
	FADPSlot S; S.Number = W.NextFlightNo; W.NextFlightNo += 2; S.Id = FString::Printf(TEXT("s%d"), S.Number);
	S.AirlineId = UADPSim::Me(); S.Reg = Reg; S.RouteId = R->Id; S.To = To; S.DepMin = DepMin; S.Days = Days; S.OnSale = Now;
	// (a route flown round and round, from before the timetable: the timetable takes over)
	A->RouteId = R->Id; A->LegIdx = A->Location == A->Base ? 0 : 1;
	W.Planned.RemoveAll([&](const FADPPlanned& P) { return P.Reg == Reg && P.SlotId.IsEmpty() && ScLive(P); });
	ADP_LOG(TEXT("Schedule: %s %d - %s to %s at %02d:%02d, days %d (block %.0f min out)"), *ScCode(W, S.AirlineId), S.Number, *Reg, *To, DepMin / 60, DepMin % 60, Days, Block(A->TypeId, A->Base, To));
	W.Slots.Add(S);
	Step(W, Now);
	return true;
}

bool ADPSchedule::RemoveSlot(FADPWorld& W, const FString& Id, const FDateTime& Now)
{
	const int32 I = W.Slots.IndexOfByPredicate([&](const FADPSlot& S) { return S.Id == Id; });
	if (I == INDEX_NONE) return false;
	W.Slots.RemoveAt(I);
	// (a rotation already out flies home as planned; the rest are off)
	for (FADPPlanned& P : W.Planned)
	{
		if (P.SlotId != Id || !ScLive(P)) continue;
		const FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == P.Reg; });
		const bool bComing = W.Flights.ContainsByPredicate([&](const FADPFlight& F) { return F.Reg == P.Reg && F.To == P.From; });
		if (P.bReturn && A && (A->Location == P.From || bComing) && (P.Sched - Now).GetTotalHours() < 12.0) continue;
		P.State = TEXT("cancelled"); P.Why = TEXT("taken out of the timetable"); P.Ended = Now;
	}
	return true;
}

FADPPlanned* ADPSchedule::Next(FADPWorld& W, const FString& Reg, const FString& From)
{
	FADPPlanned* Best = nullptr;
	for (FADPPlanned& P : W.Planned) if (P.Reg == Reg && P.From == From && ScLive(P) && (!Best || P.Sched < Best->Sched)) Best = &P;
	return Best;
}

const FADPPlanned* ADPSchedule::Next(const FADPWorld& W, const FString& Reg, const FString& From) { return Next(const_cast<FADPWorld&>(W), Reg, From); }

FADPPlanned* ADPSchedule::ById(FADPWorld& W, const FString& Id) { return W.Planned.FindByPredicate([&](const FADPPlanned& P) { return P.Id == Id; }); }

void ADPSchedule::Cancel(FADPWorld& W, FADPPlanned& P, const FDateTime& Now, const FString& Why)
{
	P.State = TEXT("cancelled"); P.Why = Why; P.Ended = Now;
	if (P.AirlineId == UADPSim::Me())
	{
		FADPNotice N; N.At = Now; N.Kind = TEXT("flight"); N.Priority = 2; N.Title = P.FlightNo + TEXT(" cancelled");
		N.Text = FString::Printf(TEXT("%s to %s is cancelled: %s. The passengers go home with their money back."), *P.FlightNo, *ScTown(P.To), *Why); W.Notices.Add(N);
	}
	ADP_LOG(TEXT("Schedule: %s %s-%s cancelled - %s"), *P.FlightNo, *P.From, *P.To, *Why);
	if (P.SlotId.IsEmpty() || P.bReturn) return;
	// (it never went: nothing to bring back)
	const FString Back = P.Id.LeftChop(2) + TEXT("-r");
	if (FADPPlanned* R = ById(W, Back)) if (ScLive(*R)) { R->State = TEXT("cancelled"); R->Why = Why; R->Ended = Now; }
}

int32 ADPSchedule::OnShift(const FADPWorld& W, const TCHAR* Role, const FDateTime& Now) { return ADPGround::OnShift(W, UADPSim::Me(), Role, ADPBoards::HereIcao(), Now); }

FString ADPSchedule::MissingStaff(const FADPWorld& W, const FDateTime& Now)
{
	TArray<FString> M;
	if (OnShift(W, TEXT("checkin"), Now) < 1) M.Add(TEXT("no passenger service agent on shift (check-in and boarding)"));
	if (OnShift(W, TEXT("security"), Now) < 1) M.Add(TEXT("no security officer on shift (the security lanes)"));
	if (OnShift(W, TEXT("ground"), Now) < 2) M.Add(TEXT("not enough ground crew on shift (2 for the ground power, the bags and the pushback)"));
	return FString::Join(M, TEXT("; "));
}

void ADPSchedule::Tell(FADPWorld& W, const FDateTime& Now, const FString& Key, const FString& Title, const FString& Text)
{
	static TMap<FString, FDateTime> Last;
	if (const FDateTime* L = Last.Find(Key)) if (Now >= *L && (Now - *L).GetTotalMinutes() < 30.0) return;
	Last.Add(Key, Now);
	FADPNotice N; N.At = Now; N.Kind = TEXT("flight"); N.Priority = 1; N.Title = Title; N.Text = Text; W.Notices.Add(N);
	ADP_LOG(TEXT("Schedule: %s - %s"), *Title, *Text);
}

void ADPSchedule::Step(FADPWorld& W, const FDateTime& Now)
{
	const FString Here = ADPBoards::HereIcao();
	// ---- your timetable's lines: their legs three days ahead
	for (int32 s = W.Slots.Num() - 1; s >= 0; --s)
	{
		const FADPSlot S = W.Slots[s];
		const FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == S.Reg && X.AirlineId == S.AirlineId; });
		if (!A) { RemoveSlot(W, S.Id, Now); continue; }   // (sold or handed back)
		const double Out = Block(A->TypeId, A->Base, S.To), Back = Block(A->TypeId, S.To, A->Base);
		if (Out <= 0.0 || Back <= 0.0) continue;
		const FDateTime Today = ADPBoards::Local(Now).GetDate();
		for (int32 d = -1; d <= 3; ++d)
		{
			const FDateTime Day = Today + FTimespan::FromDays(d);
			if (!(S.Days & (1 << int32(Day.GetDayOfWeek())))) continue;
			const FDateTime Dep = ScUtc(Day + FTimespan::FromMinutes(S.DepMin));
			if (Dep < S.OnSale + FTimespan::FromMinutes(LeadMin) || Dep < Now) continue;
			const FString Id = FString::Printf(TEXT("%s-%04d%02d%02d"), *S.Id, Day.GetYear(), Day.GetMonth(), Day.GetDay());
			if (ById(W, Id + TEXT("-o"))) continue;
			FADPPlanned P; P.Id = Id + TEXT("-o"); P.SlotId = S.Id; P.Reg = S.Reg; P.AirlineId = S.AirlineId; P.RouteId = S.RouteId;
			P.FlightNo = FString::Printf(TEXT("%s %d"), *ScCode(W, S.AirlineId), S.Number); P.From = A->Base; P.To = S.To; P.Sched = P.Est = Dep; P.BlockMin = Out; P.OnSale = FMath::Max(S.OnSale, Dep - FTimespan::FromDays(3.0));
			FADPPlanned R = P; R.Id = Id + TEXT("-r"); R.bReturn = true; R.FlightNo = FString::Printf(TEXT("%s %d"), *ScCode(W, S.AirlineId), S.Number + 1); R.From = S.To; R.To = A->Base; R.BlockMin = Back;
			R.Sched = R.Est = Dep + FTimespan::FromMinutes(Out + TurnAwayMin);
			W.Planned.Add(P); W.Planned.Add(R);
		}
	}
	// ---- the aircraft flying a route round and round (the other airlines', and yours without a timetable): their leg from here
	for (const FADPAircraft& A : W.Aircraft)
	{
		if (A.RouteId.IsEmpty() || HasSlots(W, A.Reg)) continue;
		const FADPRoute* R = W.Routes.FindByPredicate([&](const FADPRoute& X) { return X.Id == A.RouteId; });
		if (!R || R->Stops.Num() < 2 || !R->Stops.Contains(Here)) continue;
		FDateTime From; int32 Leg = 0;
		if (A.Location == Here && A.Status == TEXT("parked")) { From = FMath::Max(A.ReadyAt, Now); Leg = A.LegIdx; }
		else if (const FADPFlight* F = W.Flights.FindByPredicate([&](const FADPFlight& X) { return X.Reg == A.Reg && X.To == Here; }))
		{
			const FADPTypeInfo* T = ADPWorldData::Type(A.TypeId);
			From = F->DepAt + FTimespan::FromMinutes(F->DurMin + F->HoldMin + 35.0 + 15.0 * FMath::Clamp((T ? T->Seats : 150) / 180.0, 0.25, 3.0)); Leg = (A.LegIdx + 1) % R->Stops.Num();
		}
		else continue;
		const FString To = R->Stops[(Leg + 1) % R->Stops.Num()];
		if (To == Here) continue;
		if (Next(W, A.Reg, Here)) continue;
		const double Blk = Block(A.TypeId, Here, To); if (Blk <= 0.0) continue;
		FADPPlanned P; P.Id = FString::Printf(TEXT("%s-%lld"), *A.Reg, From.GetTicks() / ETimespan::TicksPerSecond); P.Reg = A.Reg; P.AirlineId = A.AirlineId; P.RouteId = R->Id;
		P.FlightNo = ADPBoards::FlightNumber(ScCode(W, A.AirlineId), R->Id, Leg); P.From = Here; P.To = To; P.Sched = P.Est = From; P.BlockMin = Blk; P.OnSale = Now - FTimespan::FromDays(3.0);
		W.Planned.Add(P);
	}
	// ---- each planned leg: its bookings, when it can really leave, its stand
	for (int32 i = 0; i < W.Planned.Num(); ++i)
	{
		FADPPlanned& P = W.Planned[i];
		if (!ScLive(P)) continue;
		const FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == P.Reg; });
		if (!A) { P.State = TEXT("cancelled"); P.Why = TEXT("no aircraft"); P.Ended = Now; continue; }
		const FADPTypeInfo* T = ADPWorldData::Type(A->TypeId); if (!T) continue;
		const double MinTo = (P.Sched - Now).GetTotalMinutes();
		if (!P.bTarget) ScTarget(W, P, *T);
		{
			const FDateTime Close = P.Sched - FTimespan::FromMinutes(CloseMin);
			const double K = Now >= Close ? 1.0 : FMath::Clamp((Now - P.OnSale).GetTotalMinutes() / FMath::Max(1.0, (Close - P.OnSale).GetTotalMinutes()), 0.0, 1.0);
			P.Booked = FMath::RoundToInt(P.Pax() * FMath::Pow(K, 0.7));
		}
		// when: on time, or after the aircraft is in and turned round, or after what it waits for
		FDateTime Est = FMath::Max(P.Sched, P.Hold);
		if (A->Location == P.From && A->Status != TEXT("flying")) Est = FMath::Max(Est, A->ReadyAt);
		else
		{
			const FADPFlight* F = W.Flights.FindByPredicate([&](const FADPFlight& X) { return X.Reg == A->Reg && X.To == P.From; });
			if (F) Est = FMath::Max(Est, F->DepAt + FTimespan::FromMinutes(F->DurMin + F->HoldMin + (P.From == Here ? TurnHomeMin : 40.0)));
			else if (P.bReturn) { if (const FADPPlanned* O = ById(W, P.Id.LeftChop(2) + TEXT("-o"))) if (ScLive(*O)) Est = FMath::Max(Est, O->Est + FTimespan::FromMinutes(O->BlockMin + 40.0)); }
		}
		P.Est = Est;
		// (the aircraft isn't coming: five hours after its time with it nowhere near)
		if (MinTo < -FADPFlightsModule::CancelAfterH * 60.0 && A->Location != P.From && A->Status != TEXT("flying") && !P.bReturn) { Cancel(W, P, Now, TEXT("the aircraft didn't come back in time")); continue; }
		// its stand: the one it is on; else one set aside 2.5 h before (the passengers' gate)
		if (P.From == Here)
		{
			if (A->Location == Here && A->Status != TEXT("flying") && !A->Stand.IsEmpty()) P.Stand = A->Stand;
			else if (P.Stand.IsEmpty() && (P.Est - Now).GetTotalMinutes() < 150.0) P.Stand = A->Stand.IsEmpty() ? ADPGround::PickStand(W, *A, Now) : A->Stand;
		}
	}
	W.Planned.RemoveAll([&](const FADPPlanned& P) { return !ScLive(P) && (Now - P.Ended).GetTotalHours() > 6.0; });
}
