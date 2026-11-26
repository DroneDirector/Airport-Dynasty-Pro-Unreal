// The jobs and areas - see Sim/ADPCrew.h.
#include "Sim/ADPCrew.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPStaff.h"
#include "Sim/ADPTown.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPSim.h"
#include "StaffPeople.h"
#include "ADPLog.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const TArray<ADPCrew::FArea>& ADPCrew::Areas()
{
	static const TArray<FArea> A = {
		{TEXT("pilots"), TEXT("Pilots"), TEXT("Fly your aircraft: a captain and a first officer on every flight")},
		{TEXT("cabin"), TEXT("Cabin crew"), TEXT("Look after passengers' safety and service on board: one for every 50 seats, from 20 seats")},
		{TEXT("mech"), TEXT("Mechanics"), TEXT("Inspect and repair your aircraft and vehicles: a licensed mechanic at every base")},
		{TEXT("ground"), TEXT("Ground crew"), TEXT("Handle the aircraft on the stand: bags, baggage tractors, belt loaders, pushback, wing walking and ground power")},
		{TEXT("fuel"), TEXT("Fuel"), TEXT("Drive the fuel trucks and refuel the aircraft at the stands")},
		{TEXT("catering"), TEXT("Catering"), TEXT("Bring meals and trolleys to the aircraft by catering truck")},
		{TEXT("cleaning"), TEXT("Cleaning"), TEXT("Clean the cabins between flights")},
		{TEXT("water"), TEXT("Water and toilet"), TEXT("Fill the aircraft's fresh water and empty its toilets")},
		{TEXT("deice"), TEXT("De-icing"), TEXT("Spray snow and ice off the aircraft before it taxis")},
		{TEXT("ramp"), TEXT("Marshalling and cargo"), TEXT("Guide aircraft onto the remote stands with wands; load and unload freight")},
		{TEXT("fire"), TEXT("Fire service"), TEXT("Stand ready at the fire station day and night for aircraft and airport fires; their number sets the airport's fire category")},
		{TEXT("checkin"), TEXT("Check-in"), TEXT("Check passengers and their bags in at the desks, and board them at the gates")},
		{TEXT("gate"), TEXT("Gates"), TEXT("Run the gate desks. Boarding is done by the check-in team today")},
		{TEXT("security"), TEXT("Security"), TEXT("Screen passengers and their hand luggage at the security lanes")},
		{TEXT("bus"), TEXT("Bus drivers"), TEXT("Drive the coaches, apron buses and the taxi bus")},
		{TEXT("airside"), TEXT("Airside operations"), TEXT("Drive the runway and taxiways to check for debris, damaged lights and the surface, and scare birds and animals away with bangers and distress calls")},
		{TEXT("tower"), TEXT("Tower"), TEXT("Air traffic control: clear aircraft and vehicles on the runway, taxiways and in the air")},
		{TEXT("ops"), TEXT("Operations"), TEXT("Plan the day in the operations room: stands, gates, delays")},
		{TEXT("lounge"), TEXT("Lounge"), TEXT("Welcome guests at the lounge desk and serve at its bar")},
		{TEXT("shop"), TEXT("Shops"), TEXT("Serve in the tax free, shops and cafes")},
		{TEXT("claim"), TEXT("Baggage claim"), TEXT("Look after the claim hall and lost bags")},
		{TEXT("parking"), TEXT("Car park"), TEXT("Look after the car park and keep the forecourt moving")},
	};
	return A;
}

const TArray<ADPCrew::FRole>& ADPCrew::Roles()
{
	// id, name, area, 3D job, counted by the simulation, 8 hour shifts, can be offered in a talk
	static const TArray<FRole> R = {
		{TEXT("captain"), TEXT("Captain"), TEXT("pilots"), false, true, false, false, TEXT("fly your aircraft")},
		{TEXT("fo"), TEXT("First officer"), TEXT("pilots"), false, true, false, false, TEXT("fly your aircraft")},
		{TEXT("fa"), TEXT("Flight attendant"), TEXT("cabin"), false, true, false, false, TEXT("the cabin of your flights")},
		{TEXT("mech"), TEXT("Mechanic"), TEXT("mech"), false, true, false, false, TEXT("the checks of your aircraft; mend vehicles here sooner")},
		{TEXT("ground"), TEXT("Ground crew member"), TEXT("ground"), true, true, true, true, TEXT("the bags, the tugs, power, chocks, stairs, ambulift, bus charging")},
		{TEXT("fuel"), TEXT("Fuel truck driver"), TEXT("fuel"), true, true, true, true, TEXT("drive the fuel trucks")},
		{TEXT("catering"), TEXT("Catering loader"), TEXT("catering"), true, true, true, true, TEXT("the catering trucks")},
		{TEXT("cleaning"), TEXT("Aircraft cleaner"), TEXT("cleaning"), true, true, true, true, TEXT("clean the cabins")},
		{TEXT("water"), TEXT("Water and toilet service driver"), TEXT("water"), true, false, true, true, TEXT("the water and toilet trucks")},
		{TEXT("deice"), TEXT("De-icing operator"), TEXT("deice"), true, false, true, true, TEXT("the de-icing truck")},
		{TEXT("marshaller"), TEXT("Marshaller"), TEXT("ramp"), false, false, true, true, nullptr},
		{TEXT("cargo"), TEXT("Cargo loader"), TEXT("ramp"), false, false, true, true, nullptr},
		{TEXT("fire"), TEXT("Firefighter"), TEXT("fire"), false, true, false, true, TEXT("the fire category of the airport")},
		{TEXT("checkin"), TEXT("Passenger service agent"), TEXT("checkin"), false, true, true, true, TEXT("open the check-in and the boarding")},
		{TEXT("gate"), TEXT("Gate agent"), TEXT("gate"), false, true, true, true, TEXT("open the door at the gate")},
		{TEXT("security"), TEXT("Security officer"), TEXT("security"), false, true, true, true, TEXT("open the security lanes")},
		{TEXT("bus"), TEXT("Bus driver"), TEXT("bus"), true, false, true, true, TEXT("drive the apron buses")},
		// (to-do 239b: the landside taxi bus's driver: comes along the pavement, not from a ramp room; the general wage, no ADPStaff line)
		{TEXT("taxi"), TEXT("Taxi bus driver"), TEXT("bus"), true, false, true, true, TEXT("drive the taxi bus")},
		// (owner, 2026-11-26; to-do 310: the runway checks and bird control with their amber-lit car - no 3D job yet)
		{TEXT("airside"), TEXT("Airside operations officer"), TEXT("airside"), false, false, true, true, nullptr},
		{TEXT("tower"), TEXT("Air traffic controller"), TEXT("tower"), false, false, true, false, nullptr},
		{TEXT("ops"), TEXT("Operations controller"), TEXT("ops"), false, false, true, true, nullptr},
		{TEXT("lounge"), TEXT("Lounge host"), TEXT("lounge"), false, false, true, true, nullptr},
		{TEXT("shop"), TEXT("Shop assistant"), TEXT("shop"), false, false, true, true, nullptr},
		{TEXT("claim"), TEXT("Baggage claim agent"), TEXT("claim"), false, false, true, true, nullptr},
		{TEXT("parking"), TEXT("Car park attendant"), TEXT("parking"), false, false, true, true, nullptr},
	};
	return R;
}

const ADPCrew::FRole* ADPCrew::Role(const FString& Id) { return Roles().FindByPredicate([&](const FRole& R) { return Id == R.Id; }); }
FString ADPCrew::AreaText(const FArea& Area)
{
	for (const FRole& R : Roles()) if (R.Work && FCString::Strcmp(R.Area, Area.Id) == 0) return Area.Duties;
	return FString(Area.Duties) + TEXT(" - no job in this version yet, hiring changes nothing");
}

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

// ------------------------------------------------------------------ beginners (to-do 301)

namespace
{
	struct FCrBegin { int32 N[3] = {0, 0, 0}; int32 Craft = 0, Add = 0, Most = 0; };
	struct FCrBegins { TMap<FString, FCrBegin> Jobs; double RMin = 1.0, RMax = 2.0, Pay = 0.72, SMin = 1.25, SMax = 1.35; };
	const FCrBegins& CrBegins()
	{
		static FCrBegins B; static bool bRead = false;
		if (bRead) return B;
		bRead = true;
		FString Text; TSharedPtr<FJsonObject> O; const TSharedPtr<FJsonObject>* Js = nullptr;
		if ((!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/beginners.json"))) && !FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/beginners.json"))))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O.IsValid() || !O->TryGetObjectField(TEXT("jobs"), Js))
		{
			ADP_WARN(TEXT("Crew: Data/beginners.json couldn't be read - the built-in numbers"));
			auto Put = [&](const TCHAR* Id, int32 E, int32 L, int32 N, int32 Craft, int32 Add, int32 Most) { FCrBegin X; X.N[0] = E; X.N[1] = L; X.N[2] = N; X.Craft = Craft; X.Add = Add; X.Most = Most; B.Jobs.Add(Id, X); };
			Put(TEXT("ground"), 4, 4, 2, 2, 2, 8); Put(TEXT("fuel"), 1, 1, 1, 3, 1, 2); Put(TEXT("catering"), 2, 2, 2, 4, 2, 4); Put(TEXT("cleaning"), 3, 3, 2, 3, 2, 5);
			Put(TEXT("water"), 1, 1, 1, 4, 1, 2); Put(TEXT("deice"), 2, 2, 2, 0, 0, 2); Put(TEXT("bus"), 1, 1, 1, 4, 1, 2); Put(TEXT("taxi"), 1, 1, 1, 0, 0, 1);
			Put(TEXT("checkin"), 2, 2, 1, 2, 1, 4); Put(TEXT("security"), 2, 2, 1, 2, 1, 4); Put(TEXT("gate"), 1, 1, 1, 2, 1, 3); Put(TEXT("mech"), 1, 0, 0, 0, 0, 1);
			return B;
		}
		O->TryGetNumberField(TEXT("ratingMin"), B.RMin); O->TryGetNumberField(TEXT("ratingMax"), B.RMax); O->TryGetNumberField(TEXT("pay"), B.Pay);
		O->TryGetNumberField(TEXT("slowMin"), B.SMin); O->TryGetNumberField(TEXT("slowMax"), B.SMax);
		for (const ADPCrew::FRole& R : ADPCrew::Roles())
		{
			const TSharedPtr<FJsonObject>* J = nullptr; if (!R.Work || !(*Js)->TryGetObjectField(FString(R.Id), J)) continue;
			FCrBegin X; (*J)->TryGetNumberField(TEXT("early"), X.N[0]); (*J)->TryGetNumberField(TEXT("late"), X.N[1]); (*J)->TryGetNumberField(TEXT("night"), X.N[2]);
			(*J)->TryGetNumberField(TEXT("craft"), X.Craft); (*J)->TryGetNumberField(TEXT("add"), X.Add); (*J)->TryGetNumberField(TEXT("most"), X.Most);
			B.Jobs.Add(FString(R.Id), X);
		}
		return B;
	}
	const TCHAR* CrShifts[3] = {TEXT("early"), TEXT("late"), TEXT("night")};
	FString CrBase(const FADPWorld& W) { const FADPAirline* A = W.Airlines.FindByPredicate([](const FADPAirline& X) { return X.bPlayer; }); return A ? A->Hub : FString(); }
	bool CrMine(const FADPPerson& P, const FString& Role, const FString& Base) { return P.AirlineId == UADPSim::Me() && P.Role == Role && P.Base == Base && P.Status != TEXT("dismissed"); }
}

double ADPCrew::SlowOf(const FADPPerson& P)
{
	if (!P.bBeginner) return 1.0;
	const FCrBegins& B = CrBegins();
	return FMath::Lerp(B.SMax, B.SMin, FMath::Clamp((P.Rating - B.RMin) / FMath::Max(0.1, B.RMax - B.RMin), 0.0, 1.0));
}

double ADPCrew::PayPart(const FADPPerson& P) { return P.bBeginner ? CrBegins().Pay : 1.0; }

double ADPCrew::SlowTeam(const FADPWorld& W, const FString& Id, const FString& Icao, int32 Team)
{
	TArray<double> S;
	for (const FADPPerson& P : W.People) if (CrMine(P, Id, Icao) && P.Status == TEXT("on")) S.Add(SlowOf(P));
	if (!S.Num()) return 1.0;
	S.Sort();   // (the player's own people first)
	double Sum = 0.0; const int32 N = FMath::Clamp(Team, 1, S.Num());
	for (int32 i = 0; i < N; ++i) Sum += S[i];
	return Sum / N;
}

int32 ADPCrew::BeginnerNeed(const FADPWorld& W, const FString& Id, const FString& ShiftName)
{
	const FCrBegin* B = CrBegins().Jobs.Find(Id); const FRole* R = Role(Id);
	if (!B || !R) return 0;
	int32 k = 0;
	while (k < 3 && ShiftName != CrShifts[k]) ++k;
	if (k == 3) return 0;
	int32 N = B->N[k];
	if (k < 2 && B->Craft > 0)
	{
		const FString Base = CrBase(W); int32 Craft = 0;
		for (const FADPAircraft& C : W.Aircraft) if (C.AirlineId == UADPSim::Me() && C.Base == Base) ++Craft;
		N = FMath::Min(FMath::Max(B->Most, N), N + FMath::Max(0, Craft - 1) / B->Craft * B->Add);
	}
	return R->b3D && R->bShift ? FMath::Max(N, UStaffPeople::LeastOf(Id)) : N;
}

int32 ADPCrew::FillBeginners(FADPWorld& W, const FDateTime& At, FRandomStream& Rng, bool bDayEnds)
{
	const FString Me = UADPSim::Me(), Base = CrBase(W); const FCrBegins& B = CrBegins();
	if (W.Slot.IsEmpty() || Base.IsEmpty() || !B.Jobs.Num()) return 0;   // (the self-tests' own worlds: nobody)
	const FString Country = ADPStaff::CountryOf(W, Me);
	int32 Came = 0, Left = 0; TArray<FString> Short; FString What;
	// (how many of a sex a shift has in 3D: never more than there are built people of it; the shifts share the faces)
	const int32 Faces[2] = {UStaffPeople::NumFaces(false), UStaffPeople::NumFaces(true)};
	auto Wearing = [&](const FString& Shift, bool bWoman)
	{
		int32 N = 0;
		for (const FADPPerson& P : W.People) { const FRole* R = Role(P.Role); if (R && R->b3D && CrMine(P, P.Role, Base) && P.Shift == Shift && P.bWoman == bWoman) ++N; }
		return N;
	};
	// a job of one shift (or, without shifts, the whole job): hires up to Need, lets the beginners too many go when the day ends
	auto Fill = [&](const FRole& R, const FString& Shift, int32 Need)
	{
		int32 Have = 0; TArray<FString> Beg;
		for (const FADPPerson& P : W.People) if (CrMine(P, R.Id, Base) && (Shift.IsEmpty() || P.Shift == Shift) && P.Status != TEXT("notice")) { ++Have; if (P.bBeginner) Beg.Add(P.Id); }
		for (; bDayEnds && Have > Need && Beg.Num(); --Have)
		{
			const FString Id = Beg.Pop();
			// (off the roster now, the record goes with the next day's end; no severance)
			if (FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == Id; })) { P->Status = TEXT("dismissed"); P->NoticeDays = 0; P->RestUntil = At + FTimespan::FromHours(12.0); ++Left; }
		}
		if (W.NoBeginners.Contains(FString(R.Id))) return;
		const int32 Was = Came;
		for (; Have < Need; ++Have)
		{
			const int32 L[2] = {R.b3D ? Faces[0] - Wearing(Shift, false) : 99, R.b3D ? Faces[1] - Wearing(Shift, true) : 99};
			FADPPerson P; bool bOk = false;
			// (a sex with a built person free, and nobody's name twice among your people while the generator has others)
			for (int32 k = 0; k < 60 && !bOk; ++k)
			{
				P = ADPStaff::MakePerson(R.Id, Country, Rng);
				bOk = L[P.bWoman ? 1 : 0] > 0 && (k > 40 || !W.People.ContainsByPredicate([&](const FADPPerson& X) { return X.AirlineId == Me && X.Name == P.Name; }));
			}
			if (!bOk) { Short.Add(FString::Printf(TEXT("%s (%s)"), *FString(R.Id), *Shift)); break; }
			P.bBeginner = true; P.Level = 1; P.Xp = 0.0; P.HoursTotal = 0.0; P.Contract = TEXT("full"); P.NoticeDays = 7;
			P.Rating = FMath::RoundToDouble(Rng.FRandRange(B.RMin, B.RMax) * 10.0) / 10.0;
			P.Salary = FMath::RoundToDouble(ADPStaff::GoingRate(R.Id, Country) * B.Pay); P.Asks = P.Salary; P.Mood = Rng.FRandRange(52.0, 62.0);
			P.Strengths = TEXT("quick learner"); P.Weaknesses = TEXT("no experience yet"); P.Shift = Shift;
			const FString Id = P.Id; W.People.Add(P);
			if (!ADPStaff::Hire(W, Id, Me, Base, At))
			{
				ADP_ERROR(TEXT("Crew: a beginner for %s couldn't be hired"), *FString(R.Id));
				W.People.RemoveAll([&](const FADPPerson& X) { return X.Id == Id && X.AirlineId.IsEmpty(); }); break;
			}
			++Came;
		}
		if (Came > Was) What += FString::Printf(TEXT("%s%d %s%s%s"), What.IsEmpty() ? TEXT("") : TEXT(", "), Came - Was, *FString(R.Id), Shift.IsEmpty() ? TEXT("") : TEXT(" "), *Shift);
	};
	for (const FRole& R : Roles())
	{
		const FString Id = R.Id; const bool bFire = Id == TEXT("fire");
		if (!R.Work || (!bFire && !B.Jobs.Contains(Id))) continue;
		// (the player let one go: the job is theirs, until nobody at all is left for it)
		if (W.NoBeginners.Contains(Id) && !W.People.ContainsByPredicate([&](const FADPPerson& P) { return CrMine(P, Id, Base); }))
		{ W.NoBeginners.Remove(Id); ADP_LOG(TEXT("Crew: nobody is left as %s - the airport hires beginners again"), *Id); }
		if (bFire) { Fill(R, FString(), ADPTown::FireCrewNeeded(W)); continue; }
		if (!R.bShift) { Fill(R, FString(), BeginnerNeed(W, Id, CrShifts[0])); continue; }
		for (FADPPerson& P : W.People) if (CrMine(P, Id, Base)) GiveShift(W, P);   // (an old career's people without a shift yet)
		for (const TCHAR* S : CrShifts) Fill(R, S, BeginnerNeed(W, Id, S));
	}
	if (Came || Left) ADP_LOG(TEXT("Crew: the airport hired %d beginners (%s), %d leave as your own people are enough"), Came, *What, Left);
	if (Short.Num()) ADP_WARN(TEXT("Crew: no built person free for more beginners: %s"), *FString::Join(Short, TEXT(", ")));
	if (Came && !W.bBeginnersSaid)
	{
		W.bBeginnersSaid = true;
		FADPNotice N; N.At = At; N.Kind = TEXT("staff"); N.Priority = 1; N.Title = TEXT("The airport has hired beginners");
		N.Text = FString::Printf(TEXT("%d beginners fill the jobs at the airport so that everything runs. They are cheap, inexperienced and slower. Hire better people under Crew: your own are given the work first, and beginners leave when you have enough."), Came);
		W.Notices.Add(N);
	}
	return Came;
}
