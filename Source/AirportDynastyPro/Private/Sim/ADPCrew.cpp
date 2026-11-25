// The jobs and areas - see Sim/ADPCrew.h.
#include "Sim/ADPCrew.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPStaff.h"
#include "Sim/ADPTown.h"
#include "Sim/ADPWorldData.h"
#include "ADPLog.h"

const TArray<ADPCrew::FArea>& ADPCrew::Areas()
{
	static const TArray<FArea> A = {
		{TEXT("pilots"), TEXT("Pilots"), TEXT("A captain and a first officer for every aircraft")},
		{TEXT("cabin"), TEXT("Cabin crew"), TEXT("One for every 50 seats, from 20 seats")},
		{TEXT("mech"), TEXT("Mechanics"), TEXT("A licensed mechanic at every base")},
		{TEXT("ground"), TEXT("Ground crew"), TEXT("Bags, baggage tractors, belt loaders, pushback tugs, wing walkers and ground power: one team does it all")},
		{TEXT("fuel"), TEXT("Fuel"), TEXT("The fuel trucks at the stands")},
		{TEXT("catering"), TEXT("Catering"), TEXT("Meals and trolleys to the aircraft")},
		{TEXT("cleaning"), TEXT("Cleaning"), TEXT("The cabin cleaned between flights")},
		{TEXT("water"), TEXT("Water and toilet"), TEXT("Fresh water in, waste water out")},
		{TEXT("deice"), TEXT("De-icing"), TEXT("Snow and ice off the wings before the taxi")},
		{TEXT("ramp"), TEXT("Marshalling and cargo"), TEXT("Aircraft guided onto the remote stands, freight loaded")},
		{TEXT("fire"), TEXT("Fire service"), TEXT("The fire station in 24 hour watches: the airport's fire category")},
		{TEXT("checkin"), TEXT("Check-in"), TEXT("The check-in desks, and boarding at the gates")},
		{TEXT("gate"), TEXT("Gates"), TEXT("The gate desks. Boarding is done by the check-in team today")},
		{TEXT("security"), TEXT("Security"), TEXT("The security lanes")},
		{TEXT("bus"), TEXT("Bus drivers"), TEXT("Coaches, apron buses and the taxi bus")},
		{TEXT("tower"), TEXT("Tower"), TEXT("Air traffic control")},
		{TEXT("ops"), TEXT("Operations"), TEXT("The operations room")},
		{TEXT("lounge"), TEXT("Lounge"), TEXT("The lounge desk and bar")},
		{TEXT("shop"), TEXT("Shops"), TEXT("Tax free, shops and cafes")},
		{TEXT("claim"), TEXT("Baggage claim"), TEXT("The claim hall and lost bags")},
		{TEXT("parking"), TEXT("Car park"), TEXT("The car park and the forecourt")},
	};
	return A;
}

const TArray<ADPCrew::FRole>& ADPCrew::Roles()
{
	// id, name, area, 3D job, counted by the simulation, 8 hour shifts, can be offered in a talk
	static const TArray<FRole> R = {
		{TEXT("captain"), TEXT("Captain"), TEXT("pilots"), false, true, false, false},
		{TEXT("fo"), TEXT("First officer"), TEXT("pilots"), false, true, false, false},
		{TEXT("fa"), TEXT("Flight attendant"), TEXT("cabin"), false, true, false, false},
		{TEXT("mech"), TEXT("Mechanic"), TEXT("mech"), false, true, false, false},
		{TEXT("ground"), TEXT("Ground crew member"), TEXT("ground"), true, true, true, true},
		{TEXT("fuel"), TEXT("Fuel truck driver"), TEXT("fuel"), true, true, true, true},
		{TEXT("catering"), TEXT("Catering loader"), TEXT("catering"), true, true, true, true},
		{TEXT("cleaning"), TEXT("Aircraft cleaner"), TEXT("cleaning"), true, true, true, true},
		{TEXT("water"), TEXT("Water and toilet service driver"), TEXT("water"), true, false, true, true},
		{TEXT("deice"), TEXT("De-icing operator"), TEXT("deice"), false, false, true, true},
		{TEXT("marshaller"), TEXT("Marshaller"), TEXT("ramp"), false, false, true, true},
		{TEXT("cargo"), TEXT("Cargo loader"), TEXT("ramp"), false, false, true, true},
		{TEXT("fire"), TEXT("Firefighter"), TEXT("fire"), false, true, false, true},
		{TEXT("checkin"), TEXT("Passenger service agent"), TEXT("checkin"), false, true, true, true},
		{TEXT("gate"), TEXT("Gate agent"), TEXT("gate"), false, false, true, true},
		{TEXT("security"), TEXT("Security officer"), TEXT("security"), false, true, true, true},
		{TEXT("bus"), TEXT("Bus driver"), TEXT("bus"), true, false, true, true},
		// (to-do 239b: the landside taxi bus's driver - not a 3D job yet: the hired people come out of the ramp rooms airside and
		// there is no staff way out to the forecourt kerb; the general wage, no ADPStaff line)
		{TEXT("taxi"), TEXT("Taxi bus driver"), TEXT("bus"), false, false, true, true},
		{TEXT("tower"), TEXT("Air traffic controller"), TEXT("tower"), false, false, true, false},
		{TEXT("ops"), TEXT("Operations controller"), TEXT("ops"), false, false, true, true},
		{TEXT("lounge"), TEXT("Lounge host"), TEXT("lounge"), false, false, true, true},
		{TEXT("shop"), TEXT("Shop assistant"), TEXT("shop"), false, false, true, true},
		{TEXT("claim"), TEXT("Baggage claim agent"), TEXT("claim"), false, false, true, true},
		{TEXT("parking"), TEXT("Car park attendant"), TEXT("parking"), false, false, true, true},
	};
	return R;
}

const ADPCrew::FRole* ADPCrew::Role(const FString& Id) { return Roles().FindByPredicate([&](const FRole& R) { return Id == R.Id; }); }
FString ADPCrew::RoleName(const FString& Id) { const FRole* R = Role(Id); return R ? FString(R->Name) : Id; }
bool ADPCrew::AtAirport(const FString& Id) { const FRole* R = Role(Id); return R && (R->bShift || Id == TEXT("fire")); }

const TArray<ADPCrew::FShift>& ADPCrew::Shifts()
{
	// (the simulation's share: 40% of a team on duty by day, 20% at night)
	static const TArray<FShift> S = {{TEXT("early"), TEXT("Early 06-14"), 6, 14, 2.f}, {TEXT("late"), TEXT("Late 14-22"), 14, 22, 2.f}, {TEXT("night"), TEXT("Night 22-06"), 22, 6, 1.f}, {TEXT("day"), TEXT("Day 09-17"), 9, 17, 0.f}};
	return S;
}
const ADPCrew::FShift* ADPCrew::Shift(const FString& Name) { return Shifts().FindByPredicate([&](const FShift& S) { return Name == S.Name; }); }
bool ADPCrew::InShift(const FShift& S, float H) { return S.From < S.To ? H >= S.From && H < S.To : H >= S.From || H < S.To; }

void ADPCrew::GiveShift(FADPWorld& W, FADPPerson& P)
{
	if (Shift(P.Shift)) return;
	const FShift* Best = nullptr; float Bs = 1e9f;
	for (const FShift& S : Shifts())
	{
		if (S.Weight <= 0.f) continue;
		int32 N = 0; for (const FADPPerson& X : W.People) if (X.AirlineId == P.AirlineId && X.Role == P.Role && X.Base == P.Base && X.Shift == S.Name) ++N;
		const float Sc = (N + 1) / S.Weight; if (Sc < Bs - 0.001f) { Bs = Sc; Best = &S; }
	}
	if (Best) P.Shift = Best->Name;
}

bool ADPCrew::SetShift(FADPWorld& W, const FString& PersonId, const FString& Name, FString& Why)
{
	FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == PersonId && X.AirlineId == UADPSim::Me(); });
	if (!P) { Why = TEXT("not on your staff"); return false; }
	const FRole* R = Role(P->Role);
	if (!R || !R->bShift) { Why = P->Role == TEXT("fire") ? TEXT("the fire service works 24 hour watches") : TEXT("they work by the flight roster"); return false; }
	const FShift* S = Shift(Name);
	if (!S) { Why = TEXT("no such shift"); return false; }
	P->Shift = S->Name;
	ADP_LOG(TEXT("Crew: %s (%s) works %s now"), *P->Name, *P->Role, S->Label);
	return true;
}

int32 ADPCrew::Need(const FADPWorld& W, const FString& Id)
{
	int32 Craft = 0, Fa = 0; TSet<FString> Bases;
	for (const FADPAircraft& C : W.Aircraft)
		if (C.AirlineId == UADPSim::Me())
		{
			const FADPTypeInfo* T = ADPWorldData::Type(C.TypeId);
			++Craft; Fa += ADPStaff::CabinCrewFor(T ? T->Seats : 150); Bases.Add(C.Base);
		}
	if (Id == TEXT("captain") || Id == TEXT("fo")) return Craft;
	if (Id == TEXT("fa")) return Fa;
	if (Id == TEXT("mech")) return Bases.Num();
	if (!Craft) return 0;
	if (Id == TEXT("fire")) return ADPTown::FireCrewNeeded(W);
	TMap<FString, int32> G; ADPGround::TeamNeeds(G);
	const int32* N = G.Find(Id); return N ? *N : 0;
}

int32 ADPCrew::Have(const FADPWorld& W, const FString& Id, const FString& Icao)
{
	const bool bHere = AtAirport(Id); int32 N = 0;
	for (const FADPPerson& P : W.People) if (P.AirlineId == UADPSim::Me() && P.Role == Id && P.Status != TEXT("dismissed") && (!bHere || P.Base == Icao)) ++N;
	return N;
}

bool ADPCrew::FaceWorks(const FADPWorld& W, const FString& Face) { return !Face.IsEmpty() && W.People.ContainsByPredicate([&](const FADPPerson& X) { return X.AirlineId == UADPSim::Me() && X.Face == Face; }); }

bool ADPCrew::HireTalk(FADPWorld& W, const FString& RoleId, const FString& First, bool bWoman, int32 Age, const FString& Face, const FDateTime& At, FString& Name)
{
	if (!Role(RoleId) || FaceWorks(W, Face)) return false;
	const FString Me = UADPSim::Me(), C = ADPStaff::CountryOf(W, Me);
	static FRandomStream Rs(int32(FDateTime::UtcNow().GetTicks() & 0x7fffffff));
	// (a person of the pool's kind, their sex and about their age - so the hours and languages fit)
	FADPPerson P;
	for (int32 k = 0; k < 400; ++k) { P = ADPStaff::MakePerson(RoleId, C, Rs); if (P.bWoman == bWoman && (FMath::Abs(P.Age - Age) < 5 || k > 100)) break; }
	P.bWoman = bWoman; P.Age = FMath::Clamp(Age, 18, 66); P.Face = Face;
	FString F, L; if (P.Name.Split(TEXT(" "), &F, &L)) P.Name = First + TEXT(" ") + L;
	P.Asks = 0.0; P.Salary = FMath::RoundToDouble(ADPStaff::GoingRate(RoleId, C));   // (full time, as offered)
	P.Contract = TEXT("full"); P.NoticeDays = RoleId == TEXT("fo") ? 90 : 30;
	const FString Id = P.Id; Name = P.Name;
	W.People.Add(MoveTemp(P));
	const FString Here = ADPBoards::HereIcao();
	if (!ADPStaff::Hire(W, Id, Me, Here, At)) return false;
	const FDateTime Lt = ADPBoards::Local(At);
	if (FADPPerson* X = W.People.FindByPredicate([&](const FADPPerson& Y) { return Y.Id == Id; })) X->OffUntil = At + (FTimespan::FromDays(1.0) - Lt.GetTimeOfDay());
	ADP_LOG(TEXT("Crew: %s hired in a talk as %s at %s, face %s, starts after midnight"), *Name, *RoleId, *Here, *Face);
	return true;
}
