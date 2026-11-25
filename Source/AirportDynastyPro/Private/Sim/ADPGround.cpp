// Ground handling, stands and gates - see Sim/ADPGround.h.
#include "Sim/ADPGround.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPStaff.h"
#include "Sim/ADPSchedule.h"
#include "Site.h"
#include "Airfield.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FStandInfo { FString Id; TCHAR Code = 'C'; bool bJetway = false, bCargo = false; };
	const TArray<FStandInfo>& GrStands()
	{
		static TArray<FStandInfo> S; static bool bRead = false;
		if (bRead) return S;
		bRead = true;
		FString T; TSharedPtr<FJsonObject> J;
		if ((FFileHelper::LoadFileToString(T, *ADPSite::File(TEXT("airfield.json"))) || FFileHelper::LoadFileToString(T, *(FPaths::ProjectContentDir() / TEXT("Data/airfield.json"))) || FFileHelper::LoadFileToString(T, *(FPaths::ProjectDir() / TEXT("Data/airfield.json")))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T), J) && J)
		{
			const TArray<TSharedPtr<FJsonValue>>* L;
			if (J->TryGetArrayField(TEXT("stands"), L))
				for (const TSharedPtr<FJsonValue>& V : *L)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject(); FStandInfo I;
					bool bBuilt = true; O->TryGetBoolField(TEXT("built"), bBuilt); if (!bBuilt && !FAirfieldData::RemoteOn()) continue;   // (a remote stand not in use yet)
					I.Id = O->GetStringField(TEXT("id")); FString C = TEXT("C"); O->TryGetStringField(TEXT("code"), C); I.Code = C.Len() ? C[0] : 'C';
					O->TryGetBoolField(TEXT("jetway"), I.bJetway); O->TryGetBoolField(TEXT("cargo"), I.bCargo);
					S.Add(I);
				}
		}
		return S;
	}
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

void ADPGround::TeamNeeds(TMap<FString, int32>& Out) { Out = {{TEXT("fuel"), 2}, {TEXT("catering"), 2}, {TEXT("cleaning"), 3}, {TEXT("ground"), 5}, {TEXT("checkin"), 3}, {TEXT("security"), 3}}; }

bool ADPGround::BuyVehicle(FADPWorld& W, const FString& AirlineId, const FString& Kind, const FDateTime& At)
{
	FADPAirline* A = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == AirlineId; });
	const double Price = Kind == TEXT("fuel") ? 260000.0 : Kind == TEXT("catering") ? 185000.0 : Kind == TEXT("tractor") ? 65000.0 : 0.0;
	if (!A || Price <= 0.0 || A->Balance < Price) return false;
	ADPEconomy::Pay(W, AirlineId, At, TEXT("purchase"), -Price, FString::Printf(TEXT("A new %s vehicle for the apron"), *Kind));
	(Kind == TEXT("fuel") ? A->FuelTrucks : Kind == TEXT("catering") ? A->CateringTrucks : A->Tractors)++;
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
	if (!A.Stand.IsEmpty()) return A.Stand;
	// (its next flight from here has a stand set aside - the passengers' gate: that one while it is free)
	if (const FADPPlanned* P = ADPSchedule::Next(W, A.Reg, ADPBoards::HereIcao()))
		if (!P->Stand.IsEmpty() && !W.Aircraft.ContainsByPredicate([&](const FADPAircraft& X) { return X.Stand == P->Stand; })) { A.Stand = P->Stand; A.StandUntil = Until; return A.Stand; }
	const FString S = PickStand(W, A, Now);
	if (S.IsEmpty()) return FString();   // (all taken: a remote stand on the apron, a bus to the terminal)
	A.Stand = S; A.StandUntil = Until;
	return A.Stand;
}

FString ADPGround::PickStand(const FADPWorld& W, const FADPAircraft& A, const FDateTime& Now)
{
	const TCHAR Need = CodeOf(A.TypeId);
	const FADPTypeInfo* T = ADPWorldData::Type(A.TypeId);
	const bool bCargo = T && T->Seats == 0;
	TSet<FString> Taken; for (const FADPAircraft& X : W.Aircraft) if (!X.Stand.IsEmpty() && X.Reg != A.Reg) Taken.Add(X.Stand);
	for (const FADPPlanned& P : W.Planned) if (P.State == TEXT("planned") && P.Reg != A.Reg && !P.Stand.IsEmpty() && (P.Est - Now).GetTotalMinutes() < 150.0) Taken.Add(P.Stand);
	// the best free stand: the right size (as small as fits), a jetway for passengers, the cargo apron for freighters
	const FStandInfo* Best = nullptr; int32 BestScore = MAX_int32;
	for (const FStandInfo& S : GrStands())
	{
		if (Taken.Contains(S.Id) || S.Code < Need || S.bCargo != bCargo) continue;
		const int32 Score = (S.Code - Need) * 10 + (S.bJetway ? 0 : 5);
		if (Score < BestScore) { BestScore = Score; Best = &S; }
	}
	return Best ? Best->Id : FString();
}

void ADPGround::FreeStand(FADPAircraft& A) { A.Stand.Empty(); A.StandUntil = FDateTime(); }

void FADPStandsModule::Advance(UADPSim& Sim, const FDateTime& From, const FDateTime& To, bool bCatchUp)
{
	FADPWorld& W = Sim.World;
	const FString Here = ADPBoards::HereIcao();
	// parked here: keep a stand; arriving here within 30 minutes: give one now
	for (FADPAircraft& A : W.Aircraft)
	{
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
