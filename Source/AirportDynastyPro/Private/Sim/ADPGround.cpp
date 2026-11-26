// Ground handling, stands and gates - see Sim/ADPGround.h.
#include "Sim/ADPGround.h"
#include "Sim/ADPCrew.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPVehicleArea.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPStaff.h"
#include "Sim/ADPSchedule.h"
#include "Site.h"
#include "Airfield.h"
#include "TerminalPlan.h"
#include "ADPLog.h"
#include "VehiclesOwned.h"

namespace
{
	const FAirfieldData::FStand* GrStand(const FString& Id) { return Id.IsEmpty() ? nullptr : AAirfield::Shared().Stands.FindByPredicate([&](const FAirfieldData::FStand& S) { return S.Id == Id; }); }
	/** It uses a jetway: the jets. A turboprop's, a small plane's or a helicopter's door is too low for one. */
	bool GrJetwayType(const FADPTypeInfo* T) { return !T || T->Class == TEXT("airliner") || T->Class == TEXT("widebody") || T->Class == TEXT("regional"); }
	/** A320-class and similar and larger: may be sent to a remote stand when no gate is free. */
	bool GrRemoteType(const FADPTypeInfo* T) { return !T || T->Class == TEXT("airliner") || T->Class == TEXT("widebody"); }
	/** The stands that aircraft may not have: another's (on it, or on its way to it), or set aside for another's planned flight. */
	TSet<FString> GrTaken(const FADPWorld& W, const FADPAircraft& A, const FDateTime& Now)
	{
		TSet<FString> Taken; for (const FADPAircraft& X : W.Aircraft) if (!X.Stand.IsEmpty() && X.Reg != A.Reg) Taken.Add(X.Stand);
		for (const FADPPlanned& P : W.Planned) if (P.State == TEXT("planned") && P.Reg != A.Reg && !P.Stand.IsEmpty() && (P.Est - Now).GetTotalMinutes() < 150.0) Taken.Add(P.Stand);
		// (placed at once, not arriving: nor one that another is only just being pushed back from)
		if (A.Status != TEXT("flying")) for (const FADPFlight& F : W.Flights) if (F.Reg != A.Reg && F.From == ADPBoards::HereIcao() && !F.FromStand.IsEmpty() && (Now - F.DepAt).GetTotalMinutes() < 6.0) Taken.Add(F.FromStand);
		return Taken;
	}
	/** Walked to the aircraft at most this far (m), from the ground gate's door along the way out to it: 120 m is about a
	 *  minute and a half, what airports let passengers walk on an apron; further, or over a taxilane: the bus. */
	constexpr float WalkMaxM = 120.f;
	/** People of a job on duty at an airport now (about 40% of them: three shifts and days off; half that at night). */
	int32 GrOnShift(const FADPWorld& W, const FString& AirlineId, const FString& Role, const FString& Icao, const FDateTime& Utc)
	{
		int32 N = 0;
		for (const FADPPerson& P : W.People) if (P.AirlineId == AirlineId && P.Role == Role && P.Base == Icao && P.Status == TEXT("on")) ++N;
		const int32 H = ADPBoards::Local(Utc).GetHour();
		const double Share = H >= 0 && H < 5 ? 0.2 : 0.4;
		return N == 0 ? 0 : FMath::Max(1, FMath::RoundToInt(N * Share + 0.3));
	}
	/** When a team of this job at this airport is free (it works one aircraft at a time; Slots = how many teams). */
	FDateTime GrTeamFree(FADPWorld& W, const FString& Icao, const FString& Job, int32 Slots, const FDateTime& At)
	{
		TArray<FADPTeamSlot*> Mine;
		for (FADPTeamSlot& S : W.TeamSlots) if (S.Icao == Icao && S.Job == Job) Mine.Add(&S);
		while (Mine.Num() < Slots) { FADPTeamSlot N; N.Icao = Icao; N.Job = Job; W.TeamSlots.Add(N); Mine.Reset(); for (FADPTeamSlot& S : W.TeamSlots) if (S.Icao == Icao && S.Job == Job) Mine.Add(&S); }
		FDateTime Best = FDateTime::MaxValue();
		for (int32 i = 0; i < FMath::Min(Slots, Mine.Num()); ++i) Best = FMath::Min(Best, Mine[i]->Until);
		return FMath::Max(Best, At);
	}
	void GrTeamTake(FADPWorld& W, const FString& Icao, const FString& Job, int32 Slots, const FDateTime& Start, const FDateTime& Until)
	{
		FADPTeamSlot* Earliest = nullptr; int32 Seen = 0;
		for (FADPTeamSlot& S : W.TeamSlots) if (S.Icao == Icao && S.Job == Job && Seen++ < Slots) if (!Earliest || S.Until < Earliest->Until) Earliest = &S;
		if (Earliest) Earliest->Until = Until;
	}
}

TCHAR ADPGround::CodeOf(const FString& TypeId)
{
	const FADPTypeInfo* T = ADPWorldData::Type(TypeId);
	if (!T) return 'C';
	if (T->Class == TEXT("widebody")) return T->Seats > 450 ? 'F' : 'E';
	return 'C';
}

int32 ADPGround::OnShift(const FADPWorld& W, const FString& AirlineId, const FString& Role, const FString& Icao, const FDateTime& Utc) { return GrOnShift(W, AirlineId, Role, Icao, Utc); }

void ADPGround::TeamNeeds(TMap<FString, int32>& Out) { Out = {{TEXT("fuel"), 1}, {TEXT("catering"), 2}, {TEXT("cleaning"), 3}, {TEXT("ground"), 5}, {TEXT("checkin"), 3}, {TEXT("security"), 3}, {TEXT("gate"), 1}, {TEXT("water"), 1}, {TEXT("deice"), 2}, {TEXT("bus"), 1}, {TEXT("taxi"), 1}}; }

bool ADPGround::BuyVehicle(FADPWorld& W, const FString& AirlineId, const FString& Kind, const FDateTime& At, FString& Why, UWorld* World)
{
	FADPAirline* A = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == AirlineId; });
	const ADPVehicleArea::FKind* K = ADPVehicleArea::Kind(Kind);
	// (the kinds with no place at the vehicle area: one more parked beside its own kind, to-do 318 - GSEExtra.cpp)
	if (const ADPOwned::FExtraKind* E = !K ? ADPOwned::Extra(Kind) : nullptr)
	{
		if (!A) { Why = TEXT("no such vehicle"); return false; }
		if (A->Balance < E->Price) { Why = TEXT("not enough money"); return false; }
		FADPSpare S; if (!ADPOwned::ExtraSpot(World, W, E->Key, S, Why)) return false;
		ADPEconomy::Pay(W, AirlineId, At, TEXT("purchase"), -E->Price, FString::Printf(TEXT("A new %s for the apron"), E->Label));
		if (S.Kind == TEXT("tractor")) A->Tractors++;
		A->Spares.Add(S);
		ADP_LOG(TEXT("Vehicles owned: bought a %s, its spot plan (%.1f, %.1f) heading %.0f"), E->Label, S.X, S.Y, S.Heading);
		return true;
	}
	const bool bTractor = Kind == TEXT("tractor"), bFuel = Kind == TEXT("fuel"), bCater = Kind == TEXT("catering");   // (counted for the turnaround as before)
	if (!A || (!K && !bTractor)) { Why = TEXT("no such vehicle"); return false; }
	const double Price = K ? K->Price : 65000.0; const FString Label = K ? K->Label : FString(TEXT("baggage tractor"));
	// (a tractor, and any bought while there is no vehicle area, is counted only)
	const bool bArea = K && ADPVehicleArea::Built(); const ADPVehicleArea::FPlace* P = bArea ? ADPVehicleArea::FirstFree(W, *K) : nullptr;
	if (K && !bArea && !bFuel && !bCater) { Why = TEXT("there is no vehicle area to keep it at yet"); return false; }
	if (bArea && !P) { Why = FString::Printf(TEXT("the vehicle area is full: no free place for a %s"), *Label); return false; }
	if (A->Balance < Price) { Why = TEXT("not enough money"); return false; }
	ADPEconomy::Pay(W, AirlineId, At, TEXT("purchase"), -Price, FString::Printf(TEXT("A new %s for the apron"), *Label));
	if (bFuel) A->FuelTrucks++; else if (bCater) A->CateringTrucks++; else if (bTractor) A->Tractors++;
	if (P) { FADPSpare S; S.Kind = K->Key; S.Place = P->Name; A->Spares.Add(S); }
	return true;
}

double ADPGround::Turnaround(FADPWorld& W, FADPAircraft& A, const FDateTime& At)
{
	const FADPTypeInfo* T = ADPWorldData::Type(A.TypeId);
	const int32 Seats = T ? T->Seats : 150;
	const double Size = FMath::Clamp(Seats / 180.0, 0.25, 3.0);   // (an A320 = 1)
	const bool bOurs = A.AirlineId == UADPSim::Me() && A.Location == ADPBoards::HereIcao();
	if (!bOurs)
	{
		// a handling agent: the usual turnaround and its fee
		if (A.AirlineId == UADPSim::Me()) ADPEconomy::Pay(W, A.AirlineId, At, TEXT("fees"), -(900.0 + 9.0 * Seats), FString::Printf(TEXT("%s: ground handling at %s"), *A.Reg, *A.Location), A.Reg);
		return 25.0 + 15.0 * Size;
	}
	FADPAirline* Al = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == A.AirlineId; });
	const FString Here = A.Location;
	// the work (minutes with a full team and its vehicle), the people a full team has, and the vehicles it needs
	struct FTask { const TCHAR* Job; double Min; int32 Team; int32 Vehicles; };
	const FTask Tasks[] = {
		{TEXT("fuel"), 14.0 * Size, 1, Al ? Al->FuelTrucks : 0},
		{TEXT("catering"), 15.0 * Size, 2, Al ? Al->CateringTrucks : 0},
		{TEXT("cleaning"), 12.0 * Size, 3, 99},
		{TEXT("ground"), 22.0 * Size, 4, Al ? FMath::Max(1, Al->Tractors) : 1},
	};
	const double Deboard = 8.0 * Size, Board = 18.0 * Size;
	FDateTime Done = At + FTimespan::FromMinutes(Deboard);
	FString Short;
	for (const FTask& K : Tasks)
	{
		const int32 People = GrOnShift(W, A.AirlineId, K.Job, Here, At);
		const int32 Teams = FMath::Max(1, FMath::Min(People / FMath::Max(1, K.Team), K.Vehicles));
		double Min = K.Min;
		if (People == 0 || K.Vehicles == 0) { Min = K.Min * 3.0; Short += FString::Printf(TEXT("%s%s"), Short.IsEmpty() ? TEXT("") : TEXT(", "), People == 0 ? *FString::Printf(TEXT("no %s staff on shift"), K.Job) : *FString::Printf(TEXT("no %s vehicle"), K.Job)); }
		else if (People < K.Team) { Min = K.Min * K.Team / People; Short += FString::Printf(TEXT("%s%s short-staffed"), Short.IsEmpty() ? TEXT("") : TEXT(", "), K.Job); }
		if (People > 0 && A.AirlineId == UADPSim::Me()) Min *= ADPCrew::SlowTeam(W, K.Job, Here, K.Team);   // (beginners take longer: to-do 301)
		const FDateTime Start = GrTeamFree(W, Here, K.Job, Teams, At + FTimespan::FromMinutes(K.Job == FString(TEXT("cleaning")) ? Deboard : 0.0));
		const FDateTime End = Start + FTimespan::FromMinutes(Min);
		GrTeamTake(W, Here, K.Job, Teams, Start, End);
		Done = FMath::Max(Done, End);
	}
	const double Turn = (Done - At).GetTotalMinutes() + Board * 0.5;   // (boarding overlaps the end of the loading)
	if (!Short.IsEmpty() && (W.LastGroundNote == FDateTime() || (At - W.LastGroundNote).GetTotalHours() > 12.0))
	{
		W.LastGroundNote = At;
		FADPNotice N; N.At = At; N.Kind = TEXT("flight"); N.Priority = 1; N.Title = TEXT("Ground handling slow");
		N.Text = FString::Printf(TEXT("%s's turnaround took %.0f minutes: %s. Hire ground staff or buy vehicles."), *A.Reg, Turn, *Short); W.Notices.Add(N);
	}
	return Turn;
}

int32 ADPGround::GateOfStand(const FString& Stand)
{
	FString L, R; if (Stand.Split(TEXT("-"), &L, &R) && L == TEXT("STAND")) return FCString::Atoi(*R);
	return 0;
}

FString ADPGround::AssignStand(FADPWorld& W, FADPAircraft& A, const FDateTime& Now, const FDateTime& Until)
{
	// (only cargo aircraft on the cargo stands, and they on no others: a stand kept from before that is given up here)
	const FADPTypeInfo* Ty = ADPWorldData::Type(A.TypeId);
	if (const FAirfieldData::FStand* Kept = A.Stand.IsEmpty() || A.Status != TEXT("flying") ? nullptr : GrStand(A.Stand); Kept && Kept->bCargo != (Ty && Ty->Seats == 0)) { ADP_WARN(TEXT("Stands: %s (%s) had %s, of the other kind - given another"), *A.Reg, *A.TypeId, *A.Stand); A.Stand.Reset(); }
	if (!A.Stand.IsEmpty()) return A.Stand;
	// (its next flight from here has a stand set aside - the passengers' gate: that one while it is free)
	if (const FADPPlanned* P = ADPSchedule::Next(W, A.Reg, ADPBoards::HereIcao()))
		if (const FAirfieldData::FStand* Ps = GrStand(P->Stand); Ps && Ps->bCargo == (Ty && Ty->Seats == 0) && !GrTaken(W, A, Now).Contains(P->Stand)) { A.Stand = P->Stand; A.StandUntil = Until; return A.Stand; }
	FString Why;
	const FString S = PickStand(W, A, Now, true, &Why);
	if (S.IsEmpty())
	{
		// (said once, and again every half hour it still waits)
		static TMap<FString, FDateTime> Said; FDateTime& At = Said.FindOrAdd(A.Reg);
		if (At > Now || (Now - At).GetTotalMinutes() >= 30.0) { At = Now; ADP_LOG(TEXT("Stands: %s (%s) waits - no free stand: %s"), *A.Reg, *A.TypeId, *Why); }
		return FString();
	}
	A.Stand = S; A.StandUntil = Until;
	if (const FAirfieldData::FStand* St = GrStand(S); St && St->bRemote) ADP_LOG(TEXT("Stands: %s (%s) gets the remote %s - %s"), *A.Reg, *A.TypeId, *S, *Why);
	return A.Stand;
}

FString ADPGround::PickStand(const FADPWorld& W, const FADPAircraft& A, const FDateTime& Now, bool bRemoteToo, FString* Why, const TSet<FString>* AlsoTaken)
{
	const TCHAR Need = CodeOf(A.TypeId);
	const FADPTypeInfo* T = ADPWorldData::Type(A.TypeId);
	const bool bCargo = T && T->Seats == 0;
	TSet<FString> Taken = GrTaken(W, A, Now); if (AlsoTaken) Taken.Append(*AlsoTaken);
	// the owner's order (to-do 277): jetway gates its own size, walk-out gates, larger jetway gates, remote stands - of the
	// first kind with a free one the smallest, as the layout lists them; a freighter: the cargo apron
	enum { Jet, WalkOut, JetBig, Far, Kinds }; int32 Fit[Kinds] = {0}; const FAirfieldData::FStand* Free[Kinds] = {nullptr};
	bool bAnyRemote = false;
	for (const FAirfieldData::FStand& S : AAirfield::Shared().Stands)
	{
		bAnyRemote |= S.bRemote;
		if (S.bCargo != bCargo || S.Code < Need) continue;
		if (!S.HasTug())
		{
			// (its own stop there leaves no room for a pushback tug: an aircraft could never leave it - said once per stand and type)
			static TSet<FString> Said; const FString Ks = S.Id + TEXT(" ") + A.TypeId; bool bSaid = false; Said.Add(Ks, &bSaid);
			if (!bSaid) ADP_LOG(TEXT("Stands: %s kept off %s - its stop at %.1f leaves no room for a pushback tug"), *A.TypeId, *S.Id, S.Nose);
			continue;
		}
		const int32 K = S.bRemote ? Far : !S.bJetway ? WalkOut : S.Code == Need ? Jet : JetBig;
		if ((S.bJetway && !GrJetwayType(T)) || (K == Far && (!bRemoteToo || !GrRemoteType(T)))) continue;
		++Fit[K];
		if (!Taken.Contains(S.Id) && (!Free[K] || S.Code < Free[K]->Code)) Free[K] = &S;
	}
	if (Why)
	{
		*Why = bCargo ? FString::Printf(TEXT("%d cargo stands its size, all taken"), Fit[WalkOut])
			: (GrJetwayType(T) ? FString::Printf(TEXT("%d jetway gates it fits, all taken"), Fit[Jet] + Fit[JetBig]) : FString(TEXT("not a type for the jetways"))) + FString::Printf(TEXT("; %d walk-out gates it fits, all taken"), Fit[WalkOut]);
		if (!bCargo && !Free[Far]) *Why += !bAnyRemote ? FString(TEXT("; the remote stands aren't in use")) : !GrRemoteType(T) ? FString(TEXT("; too small for a remote stand unless told")) : FString::Printf(TEXT("; %d remote stands it fits, all taken"), Fit[Far]);
	}
	for (const FAirfieldData::FStand* S : Free) if (S) return S->Id;
	return FString();
}

bool ADPGround::TellStand(FADPWorld& W, FADPAircraft& A, const FString& StandId, const FDateTime& Now, FString& Why)
{
	const FAirfieldData::FStand* S = GrStand(StandId);
	const FADPTypeInfo* T = ADPWorldData::Type(A.TypeId);
	const FString Here = ADPBoards::HereIcao();
	Why.Reset();
	if (!S) Why = TEXT("there is no such stand in use");
	else if (S->bCargo != (T && T->Seats == 0)) Why = S->bCargo ? TEXT("that is a cargo stand") : TEXT("a freighter goes to the cargo stands");
	else if (S->Code < CodeOf(A.TypeId)) Why = TEXT("that stand is too small for it");
	else if (S->bJetway && !GrJetwayType(T)) Why = TEXT("the jetway doesn't fit it");
	else if (A.Stand == StandId) return true;
	else if (GrTaken(W, A, Now).Contains(StandId)) Why = TEXT("another aircraft has that stand");
	else if (A.Status == TEXT("flying"))
	{
		const FADPFlight* F = W.Flights.FindByPredicate([&](const FADPFlight& X) { return X.Reg == A.Reg && X.To == Here; });
		if (!F) Why = TEXT("it isn't on its way here");
		else if (!A.Stand.IsEmpty() && (F->DepAt + FTimespan::FromMinutes(F->DurMin + F->HoldMin) - Now).GetTotalMinutes() < 9.5) Why = TEXT("it is on the approach already");   // (SimTraffic.cpp: in 3D from 9 minutes)
	}
	else if (A.Location != Here) Why = TEXT("it isn't here");
	else if (!A.Stand.IsEmpty()) Why = TEXT("it stands on another stand already");   // (a tow from stand to stand isn't built)
	if (!Why.IsEmpty()) { ADP_LOG(TEXT("Stands: %s not to %s - %s"), *A.Reg, *StandId, *Why); return false; }
	A.Stand = StandId; A.StandUntil = Now + FTimespan::FromHours(2.0);
	if (FADPPlanned* P = ADPSchedule::Next(W, A.Reg, Here)) P->Stand = StandId;   // (its passengers' gate follows)
	ADP_LOG(TEXT("Stands: %s told to %s"), *A.Reg, *StandId);
	return true;
}

EADPPaxWay ADPGround::PaxWay(const FString& StandId)
{
	const FAirfieldData& Fd = AAirfield::Shared();
	const FAirfieldData::FStand* S = GrStand(StandId);
	if (!S || S->bRemote || S->bCargo) return EADPPaxWay::Bus;   // (none known: as a remote one)
	if (S->bJetway) return EADPPaxWay::Jetway;
	// a ground gate: its own room's door to the apron nearest the aircraft (nothing moves: worked out once a stand)
	static TMap<FString, EADPPaxWay> Known; if (const EADPPaxWay* K = Known.Find(StandId)) return *K;
	static FTerminalPlan Plan; static const bool bPlan = Plan.Load();
	const FString Room = FString::Printf(TEXT("F1-GATE-%d"), GateOfStand(StandId));
	const FVector2f Ac = S->Frame().P(2.f, -0.3f);   // (the front left door, on the ground: Data/aircraft-doors.json's L1 - the A320 stands for every type)
	float Best = -1.f; bool bOver = false;
	if (bPlan) for (const FPlanDoor& D : Plan.Doors)
	{
		if (D.A != Room || D.B != TEXT("APRON")) continue;
		// the walkway: straight out from the door, then square across to the aircraft's door (none is painted yet)
		const FVector2f Pts[3] = {FVector2f(D.X, D.Z), FVector2f(D.X, Ac.Y), Ac};
		const float Len = (Pts[1] - Pts[0]).Size() + (Pts[2] - Pts[1]).Size();
		if (Best >= 0.f && Len >= Best) continue;
		Best = Len; bOver = false;
		for (int32 k = 0; k < 2; ++k)
			for (float Al = 0.f, L = (Pts[k + 1] - Pts[k]).Size(); Al <= L; Al += 1.f)
			{
				const FVector2f P = Pts[k] + (Pts[k + 1] - Pts[k]) * (L > 0.f ? Al / L : 0.f);
				for (const FAirfieldData::FTwy& Tw : Fd.Taxiways)
					for (int32 i = 0; i + 1 < Tw.Pts.Num(); ++i)
					{
						const FVector2f Ab = Tw.Pts[i + 1] - Tw.Pts[i];
						const float t = FMath::Clamp(FVector2f::DotProduct(P - Tw.Pts[i], Ab) / FMath::Max(Ab.SizeSquared(), 1e-6f), 0.f, 1.f);
						bOver |= (P - (Tw.Pts[i] + Ab * t)).Size() < Tw.W / 2.f;
					}
			}
	}
	const EADPPaxWay Way = Best >= 0.f && Best <= WalkMaxM && !bOver ? EADPPaxWay::Walk : EADPPaxWay::Bus;
	if (Best < 0.f) ADP_WARN(TEXT("Stands: %s has no walk-out door in the terminal plan (%s) - its passengers by bus"), *StandId, *Room);
	else ADP_LOG(TEXT("Stands: %s - %.0f m from its gate's door%s: %s"), *StandId, Best, bOver ? TEXT(", over a taxilane") : TEXT(""), Way == EADPPaxWay::Walk ? TEXT("they walk") : TEXT("by bus"));
	if (bPlan) Known.Add(StandId, Way);
	return Way;
}

void ADPGround::FreeStand(FADPAircraft& A) { A.Stand.Empty(); A.StandUntil = FDateTime(); }

void FADPStandsModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
	FADPWorld& W = Sim.World;
	const FString Here = ADPBoards::HereIcao();
	// parked here: keep a stand; arriving here within 30 minutes: give one now
	for (FADPAircraft& A : W.Aircraft)
	{
		// (no longer coming here, a stand not in use any more, or - an old career - one that another stands on: let go)
		if (!A.Stand.IsEmpty() && (!GrStand(A.Stand) || (A.Status == TEXT("flying") && (!W.Flights.ContainsByPredicate([&](const FADPFlight& F) { return F.Reg == A.Reg && F.To == Here; })
			|| W.Aircraft.ContainsByPredicate([&](const FADPAircraft& X) { return X.Reg != A.Reg && X.Stand == A.Stand && X.Status != TEXT("flying"); })))))
		{ ADP_LOG(TEXT("Stands: %s lets go of %s"), *A.Reg, *A.Stand); ADPGround::FreeStand(A); }
		if (A.Location != Here || A.Status == TEXT("flying")) { if (A.Status != TEXT("flying")) ADPGround::FreeStand(A); continue; }
		if (A.Stand.IsEmpty()) { if (!ADPGround::AssignStand(W, A, To, A.ReadyAt).IsEmpty()) ++Assigned; else ++Remote; }
	}
	for (const FADPFlight& F : W.Flights)
	{
		if (F.To != Here) continue;
		const FDateTime Eta = F.DepAt + FTimespan::FromMinutes(F.DurMin + F.HoldMin);
		if ((Eta - To).GetTotalMinutes() > 30.0) continue;
		FADPAircraft* A = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == F.Reg; });
		if (!A || !A->Stand.IsEmpty()) continue;
		if (!ADPGround::AssignStand(W, *A, To, Eta + FTimespan::FromHours(2.0)).IsEmpty()) ++Assigned; else ++Remote;
	}
}
