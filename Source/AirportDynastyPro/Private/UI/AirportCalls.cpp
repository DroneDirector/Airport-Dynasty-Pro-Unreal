// The airport's announcements in one table - see UI/AirportCalls.h.
#include "UI/AirportCalls.h"
#include "UI/ADPHUD.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPEvents.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPSchedule.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPWeather.h"
#include "Sim/ADPWorldData.h"
#include "BaggageBelts.h"
#include "TerminalPlan.h"
#include "ADPLog.h"

namespace
{
	/** Kind: 1 flight, 2 passenger, 3 information. bFirst: before anything waiting. bMinor: left out while the career runs
	 *  fast (over 20x). Again: game minutes before the same may come again. */
	struct FAcRow { const TCHAR* Event; uint8 Kind; bool bFirst, bMinor; float Again; const TCHAR* Text; };
	const FAcRow GAcRows[] = {
		// departures
		{TEXT("checkin-open"), 1, false, true, 600, TEXT("{who} flight {flight} to {place}: check-in and baggage drop are now open.")},
		{TEXT("checkin-closing"), 1, false, true, 600, TEXT("Check-in for {who} flight {flight} to {place} closes in ten minutes. Passengers who have not yet checked in, please go to the check-in desks immediately.")},
		{TEXT("gate-open"), 1, false, true, 600, TEXT("Passengers for {who} flight {flight} to {place}, please make your way to gate {gate}. Boarding begins shortly.")},
		{TEXT("gate-change"), 1, false, false, 5, TEXT("Attention please. This is a gate change. {who} flight {flight} to {place} will now depart from gate {gate}, and not from gate {old}. Passengers are asked to go to gate {gate}.")},
		{TEXT("delay"), 1, false, false, 5, TEXT("{who} regrets to announce that flight {flight} to {place} is delayed{why}. The new expected departure time is {time}. We apologise for any inconvenience.")},
		{TEXT("delay-open"), 1, false, false, 30, TEXT("{who} regrets to announce that flight {flight} to {place} is delayed{why}. A new departure time will be announced as soon as possible. We apologise for any inconvenience.")},
		{TEXT("delay-more"), 1, false, false, 20, TEXT("{who} regrets to announce a further delay to flight {flight} to {place}{why}. The new expected departure time is {time}. Thank you for your patience.")},
		{TEXT("delay-over"), 1, false, false, 60, TEXT("Good news for passengers of {who} flight {flight} to {place}. Your flight is now expected to depart at {time}. Please make sure you are at your gate in good time.")},
		{TEXT("standby"), 2, false, false, 60, TEXT("Would passengers on standby for {who} flight {flight} to {place} please come to the desk at gate {gate}.")},
		{TEXT("volunteers"), 1, false, false, 60, TEXT("{who} flight {flight} to {place} is fully booked. Passengers willing to take a later flight in return for compensation, please come to the desk at gate {gate}.")},
		{TEXT("boarding-paused"), 1, false, false, 15, TEXT("Boarding of {who} flight {flight} to {place} has been interrupted for a short time. Please remain at gate {gate}. We will continue as soon as possible.")},
		// arrivals
		{TEXT("landed"), 1, false, true, 600, TEXT("{who} flight {flight} from {place} has landed.")},
		{TEXT("arrival-delay"), 1, false, true, 30, TEXT("{who} flight {flight} from {place} is delayed. It is now expected at {time}.")},
		{TEXT("arrival-diverted"), 1, false, false, 600, TEXT("{who} flight {flight} from {place} has been diverted to another airport. Anyone meeting passengers of this flight, please contact the information desk in the main hall.")},
		{TEXT("arrival-diverted-here"), 1, false, false, 600, TEXT("Passengers of {who} flight {flight}, which has been diverted to this airport: please follow the instructions of the ground staff, who will tell you about your onward journey.")},
		{TEXT("belt"), 1, false, true, 60, TEXT("Baggage from flight {flight} will be delivered on belt {belt}.")},
		{TEXT("belt-late"), 1, false, true, 60, TEXT("Passengers from flight {flight} from {place}: we apologise for the delay to your baggage. It will be delivered on belt {belt} as soon as possible.")},
		{TEXT("belt-done"), 1, false, true, 60, TEXT("Baggage delivery from flight {flight} from {place} is now complete. Passengers whose baggage has not arrived, please contact the baggage service before leaving the arrivals hall.")},
		// the weather and the world
		{TEXT("storm"), 3, true, false, 90, TEXT("Attention please. Because of thunderstorms in the area, work on the apron has been interrupted for your safety. Flights, boarding and baggage delivery may be delayed. Please watch the information screens.")},
		{TEXT("storm-over"), 3, false, false, 30, TEXT("Attention please. The thunderstorm has passed and work on the apron has begun again. Thank you for your patience.")},
		{TEXT("snow"), 3, false, false, 240, TEXT("Because of winter weather, aircraft have to be de-iced before departure. Flights may be delayed. Please watch the information screens.")},
		{TEXT("fog"), 3, false, false, 240, TEXT("Because of fog at the airport, flights may be delayed or diverted. Please watch the information screens.")},
		{TEXT("wind"), 3, false, false, 240, TEXT("Because of strong winds at the airport, flights may be delayed. Please watch the information screens.")},
		{TEXT("closed"), 3, true, false, 120, TEXT("Attention please. All flights to and from this airport are suspended{why}. Passengers are asked to contact their airline. Further information will follow.")},
		{TEXT("reopened"), 3, true, false, 60, TEXT("Attention please. The airport is open to flights again. Please watch the information screens for the new departure time of your flight.")},
		// safety and the building: the features these belong to make them when it happens
		{TEXT("fire-alarm"), 3, true, false, 1, TEXT("Attention please. A fire alarm has been activated in {where}. It is being looked into. Please stay calm and wait for further instructions.")},
		{TEXT("evacuate"), 3, true, false, 1, TEXT("Attention please. Attention please. For safety reasons this building has to be cleared. Please leave at once by the nearest emergency exit. Do not use the lifts, and leave your baggage behind.")},
		{TEXT("all-clear"), 3, true, false, 1, TEXT("Attention please. The alarm is over and the building is safe. Thank you for your cooperation.")},
		{TEXT("unattended-bag"), 2, true, false, 10, TEXT("Attention please. Would the owner of the baggage left in {where} return to it immediately, or speak to a member of security.")},
		{TEXT("area-closed"), 3, true, false, 10, TEXT("Attention please. {where} is closed for a short time for security reasons. Please follow the instructions of the staff.")},
		{TEXT("doctor"), 2, true, false, 10, TEXT("If there is a doctor or a nurse in the terminal, please come to {where} immediately.")},
		{TEXT("lost-child"), 2, true, false, 10, TEXT("Would the parents of {name}, aged {age}, please come to the information desk in the main hall, where your child is waiting for you.")},
		{TEXT("security-queue"), 3, false, true, 60, TEXT("Waiting times at security are longer than usual. Please go through security as early as you can.")},
		{TEXT("passport-queue"), 3, false, true, 60, TEXT("Waiting times at passport control are longer than usual. Please have your passport ready.")},
		{TEXT("lift-out"), 3, false, true, 240, TEXT("The lift at {where} is out of service. Passengers needing help, please speak to a member of staff.")},
		{TEXT("escalator-out"), 3, false, true, 240, TEXT("The escalator at {where} is out of service. Please use the lift or the stairs.")},
		{TEXT("power"), 3, true, false, 30, TEXT("Attention please. There is a power failure in the terminal. Please stay where you are. The emergency lighting is on and power will be back shortly.")},
		{TEXT("system-failure"), 3, false, false, 60, TEXT("Because of a technical fault in {where}, waiting times are longer than usual. We apologise for any inconvenience.")},
		{TEXT("terminal-closing"), 3, false, false, 300, TEXT("Ladies and gentlemen, the last flight of the day has departed and the terminal closes in thirty minutes. Please make your way to the exits. We wish you a good night.")},
		{TEXT("parking-full"), 3, false, true, 120, TEXT("{where} is full. Drivers, please follow the signs to the other car parks.")},
		{TEXT("car-lights"), 2, false, true, 30, TEXT("Would the driver of the {colour} car with the registration {plate} please return to the car, where the lights have been left on.")},
		{TEXT("car-blocking"), 2, false, true, 30, TEXT("Would the driver of the {colour} car with the registration {plate}, parked in front of the terminal, please return to it at once. Otherwise it will be towed away.")},
		{TEXT("found"), 2, false, true, 5, TEXT("{what} has been handed in {where}. Would the owner please contact the information desk in the main hall.")},
		{TEXT("page"), 2, false, true, 5, TEXT("This is a passenger announcement. Would {name} please make your way to the information desk in the main hall, where a message is waiting for you. Thank you.")},
		{TEXT("page-to"), 2, false, true, 5, TEXT("This is a passenger announcement. Would passenger {name}, travelling to {place}, please contact the information desk in the main hall. Thank you.")},
		{TEXT("page-met"), 2, false, true, 5, TEXT("Would {name} please come to the meeting point in the arrivals hall, where your driver is waiting for you.")},
		{TEXT("bus-leaves"), 3, false, true, 10, TEXT("The {line} to {place} departs in {min} minutes from {where}.")},
		{TEXT("train-leaves"), 3, false, true, 10, TEXT("The {line} to {place} departs in {min} minutes from platform {where}.")},
		{TEXT("train-delay"), 3, false, true, 30, TEXT("The {line} to {place} is delayed by about {min} minutes.")},
		{TEXT("shop-closing"), 3, false, true, 300, TEXT("{where} closes in fifteen minutes.")},
		{TEXT("test"), 3, false, false, 1, TEXT("This is a test of the announcement system. No action is needed.")},
	};

	/** The notices now and then. Needs: "" always, "room:<kind>" once the terminal has such a room, else a feature. */
	struct FAcNote { const TCHAR* Needs; const TCHAR* Text; };
	const FAcNote GAcNotes[] = {
		{TEXT(""), TEXT("For safety and security reasons, please do not leave your baggage unattended at any time. Unattended baggage may be removed and destroyed by security.")},
		{TEXT("escalator"), TEXT("Please take care on the escalators. Hold the handrail, keep children by the hand, and use the lifts with trolleys, pushchairs and large baggage.")},
		{TEXT("room:security"), TEXT("Passengers are reminded that liquids in hand luggage must be in containers of one hundred millilitres or less, carried in a single clear plastic bag.")},
		{TEXT("taxi"), TEXT("Please use only the licensed taxis and airport buses at the stands in front of the terminal.")},
		{TEXT(""), TEXT("Ladies and gentlemen, smoking, including electronic cigarettes, is not permitted anywhere inside the terminal building. Thank you for your cooperation.")},
		{TEXT("room:security"), TEXT("To help us at security, please have your boarding pass ready, and take off coats and belts before you reach the scanners.")},
		{TEXT(""), TEXT("Passengers with reduced mobility who need help, please speak to the staff at the information desk in the main hall.")},
		{TEXT("room:customs"), TEXT("Arriving passengers with goods to declare, please use the red channel. Passengers with nothing to declare, please use the green channel.")},
		{TEXT("parking"), TEXT("Drivers are reminded that stopping in front of the terminal is only allowed for dropping off and picking up passengers. Please pay for parking before returning to your car.")},
		{TEXT("room:taxfree"), TEXT("Passengers travelling abroad are invited to visit our tax-free shop after security. Please remember to allow enough time to reach your gate.")},
		{TEXT(""), TEXT("Please keep your personal belongings with you at all times, and report any suspicious items to airport staff.")},
		{TEXT("room:eatery"), TEXT("Our restaurants and cafes are open to all passengers and visitors. Please remember to allow enough time to reach your gate.")},
		{TEXT(""), TEXT("Spare batteries and power banks must be carried in hand luggage. They are not allowed in checked baggage.")},
		{TEXT(""), TEXT("Please watch the information screens for your gate, and be there at the time printed on your boarding pass.")},
		{TEXT("room:gate2"), TEXT("The walk to some of our gates takes up to ten minutes. Please leave enough time.")},
		{TEXT("room:claim"), TEXT("For your safety, please do not sit, stand or climb on the baggage belts, and keep children away from them.")},
		{TEXT("pets"), TEXT("Animals travelling with you must stay in their carriers or on a lead inside the terminal.")},
		{TEXT("carrental"), TEXT("Customers who have rented a car, please go to the car rental in front of the terminal.")},
		{TEXT("room:museum"), TEXT("Waiting for a flight? Visitors are welcome in the airport museum.")},
		{TEXT("room:lounge"), TEXT("Business class passengers and lounge members are invited to relax in the lounge before departure.")},
		{TEXT("room:passport"), TEXT("Please have your passport open at the page with the photograph before you reach passport control.")},
		{TEXT(""), TEXT("Never carry baggage or a parcel for anyone you do not know. If you are asked to, please tell a member of staff or the police.")},
		{TEXT("room:claim"), TEXT("Please check the name on your baggage before you leave the arrivals hall. Many bags look alike.")},
		{TEXT("trolleys"), TEXT("Please return baggage trolleys to the trolley stands after use.")},
		{TEXT("train"), TEXT("Trains leave from the railway station at the airport. Please buy your ticket before boarding.")},
		{TEXT("hotel"), TEXT("Guests of the airport hotel, please follow the signs to the hotel or use the hotel shuttle.")},
		{TEXT("cargo"), TEXT("Customers of the cargo terminal, please report to the cargo reception with your papers.")},
	};

	TSet<FString> GAcFeatures = {TEXT("escalator"), TEXT("taxi"), TEXT("parking"), TEXT("carrental"), TEXT("pets"), TEXT("trolleys")};

	TMap<FString, FDateTime> GAcSaid;
	struct FAcDep { FString Status; int32 Gate = 0; FDateTime Told; bool bClosing = false, bTold = false; };
	struct FAcArr { FString Flight, Place, Who, Status; FDateTime Told, Landed; int32 Bags = 0; bool bDown = false, bLate = false, bDone = false; };
	TMap<FString, FAcDep> GAcDeps;
	TMap<FString, FAcArr> GAcArrs;
	FString GAcCareer;                       // (whose: another career starts afresh)
	double GAcNext = 0.0, GAcSkyNext = 0.0;
	bool bAcSky = false, bAcStorm = false, bAcSnow = false, bAcFog = false, bAcWind = false, bAcClosed = false;

	FString AcTime(const FDateTime& T) { return FString::Printf(TEXT("%02d:%02d"), T.GetHour(), T.GetMinute()); }
	const FADPAirline* AcAirline(const FADPWorld& W, const FString& Id) { for (const FADPAirline& A : W.Airlines) if (A.Id == Id) return &A; return nullptr; }
	FString AcWho(const FADPWorld& W, const FString& Id) { const FADPAirline* A = AcAirline(W, Id); return A ? A->Name : FString(TEXT("The airline")); }
	const FADPPlanned* AcLeg(const UObject* Ctx, const FString& PlanId)
	{
		const UADPSim* S = UADPSim::Get(Ctx);
		return S && !PlanId.IsEmpty() ? ADPSchedule::ById(const_cast<FADPWorld&>(S->World), PlanId) : nullptr;
	}
	FDateTime AcNow(const UObject* Ctx)
	{
		const UADPSim* S = UADPSim::Get(Ctx);
		return S && ADPBoards::Live(Ctx) ? S->Now() : FDateTime::UtcNow();
	}
	bool AcHas(const FTerminalPlan& Plan, const FString& Needs)
	{
		if (Needs.IsEmpty()) return true;
		if (!Needs.StartsWith(TEXT("room:"))) return GAcFeatures.Contains(Needs);
		const FString K = Needs.Mid(5);
		return Plan.Rooms.ContainsByPredicate([&K](const FPlanRoom& R) { return R.Kind == K; });
	}

	void AcDepartures(const UObject* Ctx, const UADPSim& Sim, const FDateTime& NowL)
	{
		TSet<FString> Seen;
		for (const FADPBoardRow& R : ADPBoards::Rows(Ctx, false, 64))
		{
			if (R.Status.StartsWith(TEXT("Departed")) || R.PlanId.IsEmpty()) continue;
			Seen.Add(R.PlanId);
			const bool bNew = !GAcDeps.Contains(R.PlanId), bOff = R.Status == TEXT("Cancelled"), bLate = R.Status == TEXT("Delayed");
			FAcDep& D = GAcDeps.FindOrAdd(R.PlanId);
			const double Left = (R.Expected - NowL).GetTotalMinutes();
			if (bNew)
			{
				// (what has already happened isn't called again)
				D.Status = R.Status; D.Gate = R.Gate; D.Told = R.Expected; D.bTold = bLate; D.bClosing = Left < 55.0;
				continue;
			}
			const FADPAirline* Al = AcAirline(Sim.World, R.AirlineId);
			const bool bMine = Al && Al->bPlayer;
			const TMap<FString, FString> Words = {{TEXT("who"), AcWho(Sim.World, R.AirlineId)}, {TEXT("flight"), R.Flight}, {TEXT("place"), R.Place}, {TEXT("gate"), FString::FromInt(R.Gate)},
				{TEXT("old"), FString::FromInt(D.Gate)}, {TEXT("time"), AcTime(R.Expected)}, {TEXT("why"), ADPCalls::Reason(Ctx, R.PlanId)}};
			if (!bOff && R.Gate && D.Gate && R.Gate != D.Gate && Left < 150.0 && R.Status != TEXT("Gate closed")) ADPCalls::Say(Ctx, TEXT("gate-change"), Words, R.PlanId);
			if (R.Gate) D.Gate = R.Gate;
			const double Moved = (R.Expected - D.Told).GetTotalMinutes();
			if (bLate && (!D.bTold || FMath::Abs(Moved) >= 15.0))
			{
				const bool bMore = D.bTold;
				ADPCalls::Say(Ctx, Left < 2.0 ? TEXT("delay-open") : !bMore ? TEXT("delay") : Moved > 0.0 ? TEXT("delay-more") : TEXT("delay-over"), Words, R.PlanId);
				D.bTold = true; D.Told = R.Expected;
			}
			else if (R.Status != D.Status)
			{
				if (R.Status == TEXT("Check-in") && bMine && D.Status == TEXT("Scheduled")) ADPCalls::Say(Ctx, TEXT("checkin-open"), Words, R.PlanId);
				else if (R.Status == TEXT("Gate open") && R.Gate) ADPCalls::Say(Ctx, TEXT("gate-open"), Words, R.PlanId);
				else if (D.Status == TEXT("Delayed") && R.Status == TEXT("Check-in") && D.bTold) { ADPCalls::Say(Ctx, TEXT("delay-over"), Words, R.PlanId); D.bTold = false; }
			}
			D.Status = R.Status;
			if (!D.bClosing && Left < 55.0) { D.bClosing = true; if (!bOff && !bLate && bMine && Left > 46.0) ADPCalls::Say(Ctx, TEXT("checkin-closing"), Words, R.PlanId); }
		}
		for (auto It = GAcDeps.CreateIterator(); It; ++It) if (!Seen.Contains(It->Key)) It.RemoveCurrent();
	}

	void AcArrivals(const UObject* Ctx, const UADPSim& Sim, const FDateTime& NowU)
	{
		for (const FADPBoardRow& R : ADPBoards::Rows(Ctx, true, 64))
		{
			if (R.PlanId.IsEmpty()) continue;
			const bool bNew = !GAcArrs.Contains(R.PlanId), bDown = R.Status.StartsWith(TEXT("Landed"));
			FAcArr& A = GAcArrs.FindOrAdd(R.PlanId);
			A.Flight = R.Flight; A.Place = R.Place; A.Who = AcWho(Sim.World, R.AirlineId);
			if (bNew) { A.Status = R.Status; A.Told = R.Expected; A.Landed = NowU; A.bDown = bDown; continue; }
			const TMap<FString, FString> Words = {{TEXT("who"), A.Who}, {TEXT("flight"), A.Flight}, {TEXT("place"), A.Place}, {TEXT("time"), AcTime(R.Expected)}};
			if (bDown && !A.bDown) { A.bDown = true; A.Landed = NowU; ADPCalls::Say(Ctx, TEXT("landed"), Words, R.PlanId); }
			else if (R.Status == TEXT("Diverted") && A.Status != R.Status) ADPCalls::Say(Ctx, TEXT("arrival-diverted"), Words, R.PlanId);
			else if (R.Status == TEXT("Diverted here") && A.Status != R.Status) ADPCalls::Say(Ctx, TEXT("arrival-diverted-here"), Words, R.PlanId);
			else if (!bDown && R.Status.StartsWith(TEXT("Expected")) && (!A.Status.StartsWith(TEXT("Expected")) || (R.Expected - A.Told).GetTotalMinutes() >= 15.0)) { A.Told = R.Expected; ADPCalls::Say(Ctx, TEXT("arrival-delay"), Words, R.PlanId); }
			A.Status = R.Status;
		}
		// the bags of those that have landed (a row leaves the board 20 minutes after landing: kept here for 90)
		UWorld* Wd = Ctx->GetWorld();
		for (auto It = GAcArrs.CreateIterator(); It; ++It)
		{
			FAcArr& A = It->Value;
			const double Min = (NowU - A.Landed).GetTotalMinutes();
			if (Min > (A.bDown ? 90.0 : 2880.0) || Min < -5.0) { It.RemoveCurrent(); continue; }
			if (!A.bDown) continue;
			const int32 Belt = ABaggageBelts::BeltOf(Wd, A.Flight), Tags = ABaggageBelts::TagsOf(Wd, A.Flight);
			const TMap<FString, FString> Words = {{TEXT("flight"), A.Flight}, {TEXT("place"), A.Place}, {TEXT("belt"), FString::FromInt(Belt + 1)}};
			if (!A.bLate && Min > 25.0 && Belt != INDEX_NONE && Tags > 0 && Tags >= A.Bags) { A.bLate = true; ADPCalls::Say(Ctx, TEXT("belt-late"), Words, It->Key); }
			if (!A.bDone && A.Bags > 0 && Tags == 0) { A.bDone = true; ADPCalls::Say(Ctx, TEXT("belt-done"), Words, It->Key); }
			A.Bags = FMath::Max(A.Bags, Tags);
		}
	}

	void AcSky(const UObject* Ctx, const UADPSim& Sim, const FDateTime& NowU)
	{
		const FADPAirportInfo* Ap = ADPWorldData::Airport(ADPBoards::HereIcao());
		if (!Ap) return;
		const FADPWx X = ADPWeather::At(Ap->Icao, Ap->Lat, Ap->Lon, NowU);
		const bool bSnow = X.bSnow || X.bFreezing, bFog = X.VisM < 800.0, bWind = FMath::Max(X.WindKt, X.GustKt) >= 35.0;
		const FADPWorldEvent* E = ADPEvents::ClosedBy(Sim.World, *Ap, NowU);
		if (bAcSky)
		{
			if (X.bStorm && !bAcStorm) ADPCalls::Say(Ctx, TEXT("storm"));
			if (!X.bStorm && bAcStorm) ADPCalls::Say(Ctx, TEXT("storm-over"));
			if (bSnow && !bAcSnow) ADPCalls::Say(Ctx, TEXT("snow"));
			if (bFog && !bAcFog) ADPCalls::Say(Ctx, TEXT("fog"));
			if (bWind && !bAcWind) ADPCalls::Say(Ctx, TEXT("wind"));
			if (E && !bAcClosed) ADPCalls::Say(Ctx, TEXT("closed"), {{TEXT("why"), E->Kind == TEXT("volcano") ? FString(TEXT(" because of volcanic ash")) : E->Kind == TEXT("strike") ? FString(TEXT(" because of a strike")) : FString()}});
			if (!E && bAcClosed) ADPCalls::Say(Ctx, TEXT("reopened"));
		}
		bAcSky = true; bAcStorm = X.bStorm; bAcSnow = bSnow; bAcFog = bFog; bAcWind = bWind; bAcClosed = E != nullptr;
	}
}

bool ADPCalls::Say(const UObject* Ctx, const FString& Event, const TMap<FString, FString>& Words, const FString& Key)
{
	const FAcRow* C = nullptr;
	for (const FAcRow& Q : GAcRows) if (Event == Q.Event) { C = &Q; break; }
	if (!C) { ADP_ERROR(TEXT("Announcer: no such announcement '%s'"), *Event); return false; }
	const UADPSim* S = UADPSim::Get(Ctx);
	if (C->bMinor && S && ADPBoards::Live(Ctx) && S->Speed() > 20.0) return false;
	FString Text = C->Text;
	for (const TPair<FString, FString>& W : Words) Text.ReplaceInline(*(TEXT("{") + W.Key + TEXT("}")), *W.Value);
	if (Text.Contains(TEXT("{"))) { ADP_ERROR(TEXT("Announcer: '%s' lacks a word: %s"), *Event, *Text); return false; }
	const FDateTime Now = AcNow(Ctx);
	const FString Id = Event + TEXT("|") + Key;
	if (const FDateTime* Was = GAcSaid.Find(Id)) if (Now >= *Was && (Now - *Was).GetTotalMinutes() < C->Again) return false;
	if (GAcSaid.Num() > 400) for (auto It = GAcSaid.CreateIterator(); It; ++It) if ((Now - It->Value).GetTotalMinutes() > 1440.0 || It->Value > Now) It.RemoveCurrent();
	GAcSaid.Add(Id, Now);
	ADP_LOG(TEXT("Announcer: call '%s' %s"), *Event, *Text.Left(80));
	AADPHUD::Announce(Text, C->Kind, C->bFirst);
	return true;
}

void ADPCalls::Feature(const FString& Name) { GAcFeatures.Add(Name); }

FString ADPCalls::Reason(const UObject* Ctx, const FString& PlanId)
{
	const FADPPlanned* P = AcLeg(Ctx, PlanId);
	if (!P || P->Why.IsEmpty()) return FString();
	const FString W = P->Why.ToLower();
	if (W.Contains(TEXT("thunder"))) return TEXT(" because of thunderstorms");
	if (W.Contains(TEXT("wind"))) return TEXT(" because of strong winds");
	if (W.Contains(TEXT("fog"))) return TEXT(" because of fog");
	if (W.Contains(TEXT("de-ic")) || W.Contains(TEXT("deic"))) return TEXT(" because the aircraft has to be de-iced");
	if (W.Contains(TEXT("closed")) || W.Contains(TEXT("curfew"))) return TEXT(" because of restrictions at the airport");
	if (W.Contains(TEXT("maintenance")) || W.Contains(TEXT("repair")) || W.Contains(TEXT("defect"))) return TEXT(" for technical reasons");
	return TEXT(" for operational reasons");
}

FString ADPCalls::Bus(const UObject* Ctx, const FString& PlanId)
{
	const FADPPlanned* P = AcLeg(Ctx, PlanId);
	return P && !P->Stand.IsEmpty() && ADPGround::GateOfStand(P->Stand) == 0 ? FString(TEXT(" A bus will take you to the aircraft.")) : FString();
}

void ADPCalls::Step(const UObject* Ctx)
{
	const UADPSim* S = UADPSim::Get(Ctx);
	if (!S || !ADPBoards::Live(Ctx)) return;
	const double Real = FPlatformTime::Seconds();
	if (Real < GAcNext) return;
	GAcNext = Real + (S->Speed() > 5.0 ? 1.0 : 5.0);
	const FDateTime NowU = S->Now();
	FString Career; for (const FADPAirline& A : S->World.Airlines) if (A.bPlayer) { Career = A.Id + A.Name; break; }
	if (Career != GAcCareer) { GAcCareer = Career; GAcDeps.Reset(); GAcArrs.Reset(); GAcSaid.Reset(); bAcSky = false; }
	AcDepartures(Ctx, *S, ADPBoards::Local(NowU));
	AcArrivals(Ctx, *S, NowU);
	if (Real >= GAcSkyNext) { GAcSkyNext = Real + 20.0; AcSky(Ctx, *S, NowU); }
}

void ADPCalls::Notice(const UObject* Ctx, const FTerminalPlan& Plan, const FString& Passenger, const FString& Place)
{
	static int32 Turn = 0, Next = 0;
	const int32 T = Turn++;
	auto Any = [](std::initializer_list<const TCHAR*> Q) { return FString(Q.begin()[FMath::RandRange(0, int32(Q.size()) - 1)]); };
	if (T % 4 == 1)
	{
		if (T % 8 == 5) Say(Ctx, TEXT("page-met"), {{TEXT("name"), Passenger}});
		else if (Place.IsEmpty()) Say(Ctx, TEXT("page"), {{TEXT("name"), Passenger}});
		else Say(Ctx, TEXT("page-to"), {{TEXT("name"), Passenger}, {TEXT("place"), Place}});
		return;
	}
	if (T % 8 == 3)
	{
		// (a registration read digit by digit, a made-up one)
		FString Plate = TEXT("V S 9"); for (int32 i = 0; i < 5; ++i) Plate += FString::Printf(TEXT(" %d"), FMath::RandRange(0, 9));
		Say(Ctx, T % 16 == 3 ? TEXT("car-lights") : TEXT("car-blocking"), {{TEXT("colour"), Any({TEXT("silver"), TEXT("black"), TEXT("white"), TEXT("blue"), TEXT("red"), TEXT("grey")})}, {TEXT("plate"), Plate}});
		return;
	}
	if (T % 8 == 7)
	{
		Say(Ctx, TEXT("found"), {{TEXT("what"), Any({TEXT("A black wallet"), TEXT("A mobile phone"), TEXT("A child's jacket"), TEXT("A set of keys"), TEXT("A pair of glasses"), TEXT("A blue rucksack"), TEXT("A passport")})},
			{TEXT("where"), Any({TEXT("near the check-in desks"), TEXT("at security"), TEXT("in the arrivals hall"), TEXT("near the escalators")})}});
		return;
	}
	for (int32 k = 0; k < int32(UE_ARRAY_COUNT(GAcNotes)); ++k)
	{
		const FAcNote& N = GAcNotes[Next++ % UE_ARRAY_COUNT(GAcNotes)];
		if (AcHas(Plan, N.Needs)) { AADPHUD::Announce(N.Text, 3); return; }
	}
}
