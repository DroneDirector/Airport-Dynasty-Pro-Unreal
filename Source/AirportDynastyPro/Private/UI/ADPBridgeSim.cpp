// The simulation for the menus (ue.adp.sim) - see UI/ADPBridge.h. To-do 176: the Overview's crew screens read and change
// the real simulation (one set of crew), through calls the native Overview will use too.
#include "UI/ADPBridge.h"
#include "UI/ADPHUD.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPStaff.h"
#include "Sim/ADPEconomy.h"
#include "Sim/ADPSchedule.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPRoster.h"
#include "Sim/ADPMaint.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPNetwork.h"
#include "Sim/ADPTown.h"
#include "Sim/ADPWeather.h"
#include "Site.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPDemand.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FString Out(const TSharedRef<FJsonObject>& O)
	{
		FString S; const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&S); FJsonSerializer::Serialize(O, W); return S;
	}
	FString Ok(bool b, const FString& Why = FString())
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), b); if (!Why.IsEmpty()) O->SetStringField(TEXT("why"), Why); return Out(O);
	}
	TSharedRef<FJsonObject> PersonJson(const FADPWorld& W, const FADPPerson& P)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), P.Id); O->SetStringField(TEXT("name"), P.Name); O->SetStringField(TEXT("role"), P.Role);
		O->SetStringField(TEXT("base"), P.Base); O->SetBoolField(TEXT("woman"), P.bWoman); O->SetNumberField(TEXT("age"), P.Age);
		O->SetNumberField(TEXT("salary"), FMath::RoundToDouble(P.Salary));
		O->SetNumberField(TEXT("going"), FMath::RoundToDouble(ADPStaff::GoingRate(P.Role, ADPStaff::CountryOf(W, P.AirlineId.IsEmpty() ? FString() : P.AirlineId))));
		O->SetNumberField(TEXT("rating"), P.Rating); O->SetNumberField(TEXT("level"), P.Level); O->SetNumberField(TEXT("mood"), FMath::RoundToDouble(P.Mood));
		O->SetStringField(TEXT("status"), P.Status); O->SetStringField(TEXT("home"), P.Home); O->SetStringField(TEXT("shift"), P.Shift);
		O->SetNumberField(TEXT("warnings"), P.Warnings);
		TArray<TSharedPtr<FJsonValue>> T; for (const FString& X : P.Types) T.Add(MakeShared<FJsonValueString>(X)); O->SetArrayField(TEXT("types"), T);
		if (!P.Training.IsEmpty()) { O->SetStringField(TEXT("training"), P.Training); O->SetStringField(TEXT("trainingUntil"), P.TrainingUntil.ToIso8601()); }
		if (P.Status != TEXT("on") && P.RestUntil != FDateTime()) O->SetStringField(TEXT("until"), P.RestUntil.ToIso8601());
		// the roster (to-do 177)
		O->SetNumberField(TEXT("fly28"), FMath::RoundToDouble(ADPRoster::Fly28(P) / 6.0) / 10.0);
		O->SetNumberField(TEXT("flyYear"), FMath::RoundToDouble(ADPRoster::FlyYear(P) / 60.0));
		O->SetStringField(TEXT("with"), P.WithReg); O->SetStringField(TEXT("at"), P.Location.IsEmpty() ? P.Base : P.Location);
		O->SetNumberField(TEXT("daysOn"), P.DaysOn);
		// careers, candidates, contracts (to-do 178-180)
		O->SetStringField(TEXT("rank"), P.Rank); O->SetNumberField(TEXT("hours"), FMath::RoundToDouble(P.HoursTotal));
		TArray<TSharedPtr<FJsonValue>> Lg; for (const FString& X : P.Languages) Lg.Add(MakeShared<FJsonValueString>(X)); O->SetArrayField(TEXT("languages"), Lg);
		O->SetBoolField(TEXT("interviewed"), P.bInterviewed);
		if (P.bInterviewed) { O->SetStringField(TEXT("strengths"), P.Strengths); O->SetStringField(TEXT("weaknesses"), P.Weaknesses); }
		O->SetNumberField(TEXT("asks"), FMath::RoundToDouble(P.Asks)); O->SetStringField(TEXT("contract"), P.Contract); O->SetNumberField(TEXT("noticeDays"), P.NoticeDays);
		if (P.HiredAt != FDateTime()) O->SetStringField(TEXT("hiredAt"), P.HiredAt.ToIso8601());
		if (P.RestUntil > FDateTime::UtcNow()) O->SetStringField(TEXT("restUntil"), P.RestUntil.ToIso8601());
		if (P.OffUntil > FDateTime::UtcNow()) O->SetStringField(TEXT("offUntil"), P.OffUntil.ToIso8601());
		return O;
	}
}

FString UADPBridge::Sim(const FString& Action, const FString& Arg)
{
	AADPHUD* H = Hud.Get();
	UADPSim* S = H ? UADPSim::Get(H) : nullptr;
	// (at the title screen too: the named saves, and one chosen to load when the game starts - to-do 192)
	if (S && Action == TEXT("saves"))
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true);
		TArray<TSharedPtr<FJsonValue>> L; for (const TSharedPtr<FJsonObject>& J : UADPSim::NamedSaves()) L.Add(MakeShared<FJsonValueObject>(J));
		O->SetArrayField(TEXT("saves"), L); return Out(O);
	}
	if (S && Action == TEXT("load") && !S->IsRunning()) { S->PendingLoad = Arg; return Ok(true); }
	if (!S || !S->IsRunning()) return Ok(false, TEXT("no career running"));
	FADPWorld& W = S->World;
	const FString Me = UADPSim::Me();
	FString A, B; if (!Arg.Split(TEXT("|"), &A, &B)) A = Arg;
	if (Action == TEXT("saves"))
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true);
		TArray<TSharedPtr<FJsonValue>> L; for (const TSharedPtr<FJsonObject>& J : UADPSim::NamedSaves()) L.Add(MakeShared<FJsonValueObject>(J));
		O->SetArrayField(TEXT("saves"), L); return Out(O);
	}
	if (Action == TEXT("save")) { FString Slot; const bool b = S->SaveNamed(Arg, Slot); if (b && H) H->Hint(FString::Printf(TEXT("Saved: %s"), *Arg)); return Ok(b, TEXT("nothing to save yet")); }
	if (Action == TEXT("load")) return Ok(S->LoadNamed(Arg), TEXT("that save is from another airport - choose it at the title screen"));
	if (Action == TEXT("crew"))
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true);
		const FADPAirline* My = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Me; });
		if (My) { O->SetStringField(TEXT("airline"), My->Name); O->SetStringField(TEXT("hub"), My->Hub); O->SetNumberField(TEXT("balance"), FMath::RoundToDouble(My->Balance)); }
		TArray<TSharedPtr<FJsonValue>> Mine, Pool;
		for (const FADPPerson& P : W.People)
		{
			if (P.AirlineId == Me) Mine.Add(MakeShared<FJsonValueObject>(PersonJson(W, P)));
			else if (P.AirlineId.IsEmpty()) Pool.Add(MakeShared<FJsonValueObject>(PersonJson(W, P)));
		}
		O->SetArrayField(TEXT("people"), Mine); O->SetArrayField(TEXT("pool"), Pool);
		// what the fleet needs to fly: per aircraft a captain and a first officer, its cabin crew; a mechanic per base
		int32 NCap = 0, NFo = 0, NFa = 0; TSet<FString> Bases;
		for (const FADPAircraft& C : W.Aircraft) if (C.AirlineId == Me)
		{
			const FADPTypeInfo* T = ADPWorldData::Type(C.TypeId);
			++NCap; ++NFo; NFa += ADPStaff::CabinCrewFor(T ? T->Seats : 150); Bases.Add(C.Base);
		}
		TSharedRef<FJsonObject> Need = MakeShared<FJsonObject>();
		Need->SetNumberField(TEXT("captain"), NCap); Need->SetNumberField(TEXT("fo"), NFo); Need->SetNumberField(TEXT("fa"), NFa); Need->SetNumberField(TEXT("mech"), Bases.Num());
		// ground teams at your airport (to-do 182): what one turnaround at a time needs
		{ TMap<FString, int32> G; ADPGround::TeamNeeds(G); if (Bases.Num()) for (const auto& K : G) Need->SetNumberField(K.Key, K.Value); }
		if (My) { O->SetNumberField(TEXT("fuelTrucks"), My->FuelTrucks); O->SetNumberField(TEXT("cateringTrucks"), My->CateringTrucks); O->SetNumberField(TEXT("tractors"), My->Tractors); }
		O->SetObjectField(TEXT("need"), Need);
		TArray<TSharedPtr<FJsonValue>> Lv;
		for (int32 i = 0; i < W.LeaveRequests.Num(); ++i)
		{
			const FADPLeaveRequest& L = W.LeaveRequests[i];
			const FADPPerson* P = W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == L.PersonId; });
			if (!P || P->AirlineId != Me) continue;
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
			X->SetNumberField(TEXT("i"), i); X->SetStringField(TEXT("name"), P->Name); X->SetStringField(TEXT("role"), P->Role);
			X->SetStringField(TEXT("from"), L.From.ToString(TEXT("%d.%m.%Y"))); X->SetNumberField(TEXT("days"), L.Days); X->SetStringField(TEXT("state"), L.State);
			Lv.Add(MakeShared<FJsonValueObject>(X));
		}
		O->SetArrayField(TEXT("leave"), Lv);
		TArray<TSharedPtr<FJsonValue>> Ty; for (const FADPTypeInfo& T : ADPWorldData::Types()) if (ADPWorldData::HasModel(T.Id)) { TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("id"), T.Id); X->SetStringField(TEXT("name"), T.Name); Ty.Add(MakeShared<FJsonValueObject>(X)); }
		O->SetArrayField(TEXT("types"), Ty);
		return Out(O);
	}
	if (Action == TEXT("fleet"))
	{
		// your aircraft and their maintenance (to-do 181): status, condition, the next check and how far due it is
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true);
		const FADPAirline* My = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Me; });
		O->SetNumberField(TEXT("partsStock"), My ? FMath::RoundToDouble(My->PartsStock) : 0.0); O->SetNumberField(TEXT("partsIndex"), W.PartsIndex);
		TArray<TSharedPtr<FJsonValue>> L;
		for (const FADPAircraft& C : W.Aircraft) if (C.AirlineId == Me)
		{
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
			double F = 0; const FString Due = ADPMaint::NextDue(W, C, S->Now(), F);
			X->SetStringField(TEXT("reg"), C.Reg); X->SetStringField(TEXT("type"), C.TypeId); X->SetStringField(TEXT("status"), C.Status); X->SetStringField(TEXT("at"), C.Location);
			X->SetNumberField(TEXT("condition"), FMath::RoundToDouble(C.Condition)); X->SetNumberField(TEXT("hours"), FMath::RoundToDouble(C.HoursMin / 60.0)); X->SetNumberField(TEXT("cycles"), C.Cycles);
			X->SetStringField(TEXT("nextCheck"), Due); X->SetNumberField(TEXT("due"), FMath::RoundToDouble(F * 100.0));
			if (!C.CheckKind.IsEmpty()) { X->SetStringField(TEXT("inCheck"), C.CheckKind); X->SetStringField(TEXT("until"), C.CheckUntil.ToIso8601()); }
			TArray<TSharedPtr<FJsonValue>> Cr; for (const FString& Id : C.Crew) Cr.Add(MakeShared<FJsonValueString>(Id)); X->SetArrayField(TEXT("crew"), Cr);
			L.Add(MakeShared<FJsonValueObject>(X));
		}
		O->SetArrayField(TEXT("aircraft"), L);
		return Out(O);
	}
	if (Action == TEXT("boards"))
	{
		// the terminal's boards (drawn by the menus' page): rows as its drawing wants them (times as moments, ms since 1970)
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true); O->SetStringField(TEXT("base"), ADPBoards::HereIcao());
		// (the airport's clock and its second language, for the board's header and times)
		O->SetNumberField(TEXT("tz"), ADPSite::UtcOffsetHours(S->Now()) * 60.0); O->SetStringField(TEXT("lang"), ADPSite::SignLanguage());
		auto Ms = [&](const FDateTime& L) { const FDateTime U = L - FTimespan::FromHours(ADPSite::UtcOffsetHours(L)); return double((U - FDateTime(1970, 1, 1)).GetTotalMilliseconds()); };
		for (const bool bArr : {false, true})
		{
			TArray<TSharedPtr<FJsonValue>> L;
			for (const FADPBoardRow& R : ADPBoards::Rows(H, bArr, 48))
			{
				TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
				X->SetStringField(TEXT("key"), R.Flight + R.Time()); X->SetNumberField(TEXT("sched"), Ms(R.Sched)); X->SetNumberField(TEXT("exp"), Ms(R.Expected));
				X->SetStringField(TEXT("flight"), R.Flight); X->SetStringField(TEXT("place"), FString::Printf(TEXT("%s %s"), *R.PlaceCode, *R.Place.Left(13)));
				const FADPAircraft* C = W.Aircraft.FindByPredicate([&](const FADPAircraft& Z) { return Z.Reg == R.Reg; });
				X->SetStringField(TEXT("ac"), C ? C->TypeId : FString()); X->SetStringField(TEXT("status"), R.Gate && !bArr ? FString::Printf(TEXT("%s · G%d"), *R.Status, R.Gate) : R.Status);
				X->SetStringField(TEXT("col"), FString::Printf(TEXT("#%02X%02X%02X"), R.Col.R, R.Col.G, R.Col.B)); X->SetNumberField(TEXT("gate"), R.Gate);
				L.Add(MakeShared<FJsonValueObject>(X));
			}
			O->SetArrayField(bArr ? TEXT("arr") : TEXT("dep"), L);
		}
		return Out(O);
	}
	if (Action == TEXT("overview"))
	{
		// the new Overview (to-do 188): everything at a glance from the simulation, refreshed every few seconds
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true);
		const FDateTime NowU = S->Now(), NowL = ADPBoards::Local(NowU);
		const FADPAirline* My = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Me; });
		auto Num = [&](const TCHAR* K, double V) { O->SetNumberField(K, V); };
		if (My)
		{
			O->SetStringField(TEXT("airline"), My->Name); O->SetStringField(TEXT("hub"), My->Hub);
			Num(TEXT("balance"), FMath::RoundToDouble(My->Balance)); Num(TEXT("sharePrice"), FMath::RoundToDouble(My->SharePrice * 100.0) / 100.0);
			Num(TEXT("reputation"), FMath::RoundToDouble(My->Reputation)); Num(TEXT("onTime"), FMath::RoundToDouble(My->OnTimeEma * 100.0));
			Num(TEXT("pax30"), FMath::RoundToDouble(My->Pax30)); Num(TEXT("legs30"), FMath::RoundToDouble(My->Legs30)); Num(TEXT("profit30"), FMath::RoundToDouble(My->ProfitEma30));
			int32 Pos = 0, All = 0; for (const ADPNetwork::FRank& R : ADPNetwork::League(W)) { ++All; if (R.AirlineId == Me) Pos = All; }
			Num(TEXT("leaguePos"), Pos); Num(TEXT("leagueOf"), All);
		}
		const FString Here = ADPBoards::HereIcao();
		O->SetStringField(TEXT("airport"), ADPSite::Name(ADPSite::Id())); O->SetStringField(TEXT("icao"), Here);
		O->SetStringField(TEXT("localTime"), NowL.ToString(TEXT("%H:%M"))); O->SetStringField(TEXT("date"), NowL.ToString(TEXT("%d.%m.%Y")));
		if (TSharedPtr<FJsonObject> Ab = ADPSite::AirportBlock())
		{
			double La = 0, Lo = 0; Ab->TryGetNumberField(TEXT("lat"), La); Ab->TryGetNumberField(TEXT("lon"), Lo);
			const FADPWx X = ADPWeather::At(Here, La, Lo, NowU);
			TSharedRef<FJsonObject> Wx = MakeShared<FJsonObject>();
			Wx->SetNumberField(TEXT("windDir"), X.WindDir); Wx->SetNumberField(TEXT("windKt"), X.WindKt); Wx->SetNumberField(TEXT("gustKt"), X.GustKt); Wx->SetNumberField(TEXT("tempC"), X.TempC);
			Wx->SetNumberField(TEXT("visM"), X.VisM); Wx->SetBoolField(TEXT("rain"), X.bRain); Wx->SetBoolField(TEXT("snow"), X.bSnow); Wx->SetBoolField(TEXT("storm"), X.bStorm); Wx->SetBoolField(TEXT("fog"), X.bFog);
			Wx->SetBoolField(TEXT("live"), X.bLive); Wx->SetStringField(TEXT("station"), X.Station); Wx->SetStringField(TEXT("metar"), X.Raw);
			O->SetObjectField(TEXT("weather"), Wx);
		}
		Num(TEXT("curfewFrom"), W.Town.CurfewFrom); Num(TEXT("curfewTo"), W.Town.CurfewTo); Num(TEXT("townMood"), FMath::RoundToDouble(W.Town.Mood));
		Num(TEXT("fireCat"), ADPTown::FireCategoryAt(W, Here, NowU));
		// your flights in the air
		TArray<TSharedPtr<FJsonValue>> Air;
		for (const FADPFlight& F : W.Flights) if (F.AirlineId == Me)
		{
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
			X->SetStringField(TEXT("reg"), F.Reg); X->SetStringField(TEXT("from"), F.From); X->SetStringField(TEXT("to"), F.To); X->SetStringField(TEXT("phase"), F.Phase);
			X->SetNumberField(TEXT("alt"), FMath::RoundToDouble(F.AltFt)); X->SetNumberField(TEXT("gs"), FMath::RoundToDouble(F.GsKt)); X->SetNumberField(TEXT("pax"), F.Pax + F.PaxJ + F.PaxF);
			const FDateTime Eta = ADPBoards::Local(F.DepAt + FTimespan::FromMinutes(F.DurMin + F.HoldMin)); X->SetStringField(TEXT("eta"), Eta.ToString(TEXT("%H:%M")));
			X->SetNumberField(TEXT("progress"), FMath::Clamp((NowU - F.DepAt).GetTotalMinutes() / FMath::Max(1.0, F.DurMin + F.HoldMin), 0.0, 1.0));
			Air.Add(MakeShared<FJsonValueObject>(X));
		}
		O->SetArrayField(TEXT("inAir"), Air);
		for (const bool bArr : {false, true})
		{
			TArray<TSharedPtr<FJsonValue>> L;
			for (const FADPBoardRow& R : ADPBoards::Rows(H, bArr, 7))
			{
				TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("time"), R.Time()); X->SetStringField(TEXT("flight"), R.Flight);
				X->SetStringField(TEXT("place"), R.Place); X->SetStringField(TEXT("status"), R.Status); X->SetNumberField(TEXT("gate"), R.Gate); X->SetBoolField(TEXT("mine"), R.AirlineId == Me);
				L.Add(MakeShared<FJsonValueObject>(X));
			}
			O->SetArrayField(bArr ? TEXT("arrivals") : TEXT("departures"), L);
		}
		// the fleet and the crew in numbers
		int32 Fl = 0, Flying = 0, Parked = 0, Mx = 0, Grounded = 0; TArray<TSharedPtr<FJsonValue>> Checks;
		for (const FADPAircraft& C : W.Aircraft) if (C.AirlineId == Me)
		{
			++Fl; Flying += C.Status == TEXT("flying"); Parked += C.Status == TEXT("parked"); Mx += C.Status == TEXT("maintenance"); Grounded += C.Status == TEXT("grounded");
			double F = 0; const FString Due = ADPMaint::NextDue(W, C, NowU, F);
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("reg"), C.Reg); X->SetStringField(TEXT("check"), Due); X->SetNumberField(TEXT("due"), FMath::RoundToDouble(F * 100.0)); X->SetNumberField(TEXT("condition"), FMath::RoundToDouble(C.Condition));
			Checks.Add(MakeShared<FJsonValueObject>(X));
		}
		Num(TEXT("fleet"), Fl); Num(TEXT("flying"), Flying); Num(TEXT("parked"), Parked); Num(TEXT("inCheck"), Mx); Num(TEXT("grounded"), Grounded);
		O->SetArrayField(TEXT("checks"), Checks);
		int32 Staff = 0, OnDuty = 0, Resting = 0, Off = 0, Sick = 0, Leave = 0;
		for (const FADPPerson& P : W.People) if (P.AirlineId == Me)
		{ ++Staff; OnDuty += !P.WithReg.IsEmpty(); Resting += P.RestUntil > NowU && P.Status == TEXT("on") && P.WithReg.IsEmpty(); Off += P.OffUntil > NowU; Sick += P.Status == TEXT("sick"); Leave += P.Status == TEXT("leave"); }
		Num(TEXT("staff"), Staff); Num(TEXT("onDuty"), OnDuty); Num(TEXT("resting"), Resting); Num(TEXT("daysOff"), Off); Num(TEXT("sick"), Sick); Num(TEXT("leave"), Leave);
		// the newest messages and news
		TArray<TSharedPtr<FJsonValue>> Nt;
		for (int32 i = W.Notices.Num() - 1; i >= 0 && Nt.Num() < 8; --i)
		{
			const FADPNotice& N = W.Notices[i]; TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
			X->SetStringField(TEXT("kind"), N.Kind); X->SetStringField(TEXT("title"), N.Title); X->SetStringField(TEXT("text"), N.Text); X->SetNumberField(TEXT("prio"), N.Priority);
			X->SetStringField(TEXT("at"), ADPBoards::Local(N.At).ToString(TEXT("%d.%m %H:%M"))); Nt.Add(MakeShared<FJsonValueObject>(X));
		}
		O->SetArrayField(TEXT("notices"), Nt);
		TArray<TSharedPtr<FJsonValue>> Nw;
		for (int32 i = W.News.Num() - 1; i >= 0 && Nw.Num() < 5; --i) Nw.Add(MakeShared<FJsonValueString>(W.News[i].Headline));
		O->SetArrayField(TEXT("news"), Nw);
		TArray<TSharedPtr<FJsonValue>> Ch;
		for (const FADPChallenge& C : W.Challenges) { TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("title"), C.Title); X->SetNumberField(TEXT("progress"), C.Progress); X->SetNumberField(TEXT("target"), C.Target); X->SetBoolField(TEXT("done"), C.bDone); Ch.Add(MakeShared<FJsonValueObject>(X)); }
		O->SetArrayField(TEXT("challenges"), Ch);
		return Out(O);
	}
	if (Action == TEXT("market"))
	{
		// the league, alliances, shares, fuel hedge, frequent flyers and this week's challenges (to-do 184-186)
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true);
		const FADPAirline* My = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Me; });
		TArray<TSharedPtr<FJsonValue>> Lg; int32 Pos = 0;
		for (const ADPNetwork::FRank& R : ADPNetwork::League(W))
		{
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); ++Pos;
			X->SetNumberField(TEXT("pos"), Pos); X->SetStringField(TEXT("id"), R.AirlineId); X->SetStringField(TEXT("name"), R.Name); X->SetBoolField(TEXT("me"), R.AirlineId == Me);
			X->SetNumberField(TEXT("score"), FMath::RoundToDouble(R.Score * 10.0) / 10.0); X->SetNumberField(TEXT("pax30"), FMath::RoundToDouble(R.Pax30));
			X->SetNumberField(TEXT("onTime"), FMath::RoundToDouble(R.OnTime * 100.0)); X->SetNumberField(TEXT("reputation"), FMath::RoundToDouble(R.Reputation)); X->SetNumberField(TEXT("profit30"), FMath::RoundToDouble(R.Profit30));
			if (const FADPAirline* A2 = W.Airlines.FindByPredicate([&](const FADPAirline& Z) { return Z.Id == R.AirlineId; }))
			{ X->SetNumberField(TEXT("price"), FMath::RoundToDouble(A2->SharePrice * 100.0) / 100.0); X->SetNumberField(TEXT("held"), W.Holdings.FindRef(R.AirlineId)); X->SetStringField(TEXT("hub"), A2->Hub); }
			Lg.Add(MakeShared<FJsonValueObject>(X));
		}
		O->SetArrayField(TEXT("league"), Lg);
		TArray<TSharedPtr<FJsonValue>> Als;
		for (const FADPAlliance& A2 : W.Alliances)
		{
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("name"), A2.Name); X->SetBoolField(TEXT("mine"), A2.Members.Contains(Me));
			TArray<TSharedPtr<FJsonValue>> Mb; for (const FString& M : A2.Members) if (const FADPAirline* Z = W.Airlines.FindByPredicate([&](const FADPAirline& Q) { return Q.Id == M; })) Mb.Add(MakeShared<FJsonValueString>(Z->Name));
			X->SetArrayField(TEXT("members"), Mb); Als.Add(MakeShared<FJsonValueObject>(X));
		}
		O->SetArrayField(TEXT("alliances"), Als);
		if (My)
		{
			O->SetNumberField(TEXT("sharePrice"), FMath::RoundToDouble(My->SharePrice * 100.0) / 100.0); O->SetBoolField(TEXT("public"), My->bPublic);
			O->SetBoolField(TEXT("ff"), My->bFrequentFlyer); O->SetNumberField(TEXT("ffMembers"), FMath::RoundToDouble(My->FfMembers));
			O->SetNumberField(TEXT("hedgeTonnes"), FMath::RoundToDouble(My->HedgeKg / 1000.0)); O->SetNumberField(TEXT("hedgePrice"), My->HedgePrice);
			if (My->HedgeKg > 0) O->SetStringField(TEXT("hedgeUntil"), My->HedgeUntil.ToString(TEXT("%d.%m.%Y")));
			O->SetNumberField(TEXT("fuelIndex"), W.FuelIndex); O->SetNumberField(TEXT("reputation"), My->Reputation); O->SetNumberField(TEXT("balance"), My->Balance);
		}
		TArray<TSharedPtr<FJsonValue>> Ch;
		for (const FADPChallenge& C : W.Challenges)
		{
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("title"), C.Title); X->SetNumberField(TEXT("progress"), C.Progress); X->SetNumberField(TEXT("target"), C.Target);
			X->SetNumberField(TEXT("money"), C.Money); X->SetNumberField(TEXT("xp"), C.Xp); X->SetBoolField(TEXT("done"), C.bDone); Ch.Add(MakeShared<FJsonValueObject>(X));
		}
		O->SetArrayField(TEXT("challenges"), Ch);
		// your airport: fire cover and the town (to-do 197 / 199)
		{
			const FString Here = ADPBoards::HereIcao();
			O->SetNumberField(TEXT("fireCat"), ADPTown::FireCategoryAt(W, Here, S->Now())); O->SetNumberField(TEXT("fireTrucks"), My ? My->FireTrucks : 0);
			int32 NeedCat = 0; for (const FADPAircraft& C : W.Aircraft) if (C.AirlineId == Me) NeedCat = FMath::Max(NeedCat, ADPTown::FireCategoryOf(C.TypeId));
			O->SetNumberField(TEXT("fireNeed"), NeedCat);
			const FADPTownState& T = W.Town;
			O->SetNumberField(TEXT("curfewFrom"), T.CurfewFrom); O->SetNumberField(TEXT("curfewTo"), T.CurfewTo); O->SetNumberField(TEXT("townMood"), FMath::RoundToDouble(T.Mood));
			O->SetNumberField(TEXT("complaints"), FMath::RoundToDouble(T.Complaints)); O->SetNumberField(TEXT("insulated"), T.Insulated); O->SetStringField(TEXT("syndic"), ADPTown::SyndicName(W));
			O->SetBoolField(TEXT("lobbying"), T.LobbyUntil > S->Now()); O->SetBoolField(TEXT("scandal"), T.bScandal);
		}
		return Out(O);
	}
	const FDateTime At = S->Now();
	// ---- your aircraft and your timetable (owner 2026-11-23): what is for sale, bought, leased, sold; the lines of the
	//      timetable and the flights they become, with who has booked
	if (Action == TEXT("aircraft"))
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true);
		const FADPAirline* My = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Me; });
		O->SetNumberField(TEXT("balance"), My ? FMath::RoundToDouble(My->Balance) : 0.0); O->SetStringField(TEXT("hub"), My ? My->Hub : FString());
		TArray<TSharedPtr<FJsonValue>> Ty, Us, Fl;
		for (const FADPTypeInfo& T : ADPWorldData::Types()) if (ADPWorldData::HasModel(T.Id))
		{
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("id"), T.Id); X->SetStringField(TEXT("name"), T.Name); X->SetNumberField(TEXT("seats"), T.Seats); X->SetNumberField(TEXT("range"), T.RangeNm);
			X->SetNumberField(TEXT("price"), FMath::RoundToDouble(T.ValueM * 1e6)); X->SetNumberField(TEXT("lease"), FMath::RoundToDouble(ADPEconomy::LeaseRate(T.Id))); X->SetNumberField(TEXT("deposit"), FMath::RoundToDouble(ADPEconomy::LeaseRate(T.Id) * 2.0));
			X->SetNumberField(TEXT("fa"), ADPStaff::CabinCrewFor(T.Seats)); Ty.Add(MakeShared<FJsonValueObject>(X));
		}
		for (int32 i = 0; i < W.UsedMarket.Num(); ++i)
		{
			const FADPUsedOffer& U = W.UsedMarket[i]; if (!ADPWorldData::HasModel(U.TypeId)) continue;
			const FADPTypeInfo* T = ADPWorldData::Type(U.TypeId);
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetNumberField(TEXT("i"), i); X->SetStringField(TEXT("type"), U.TypeId); X->SetStringField(TEXT("name"), T ? T->Name : U.TypeId); X->SetNumberField(TEXT("age"), U.AgeYears);
			X->SetNumberField(TEXT("condition"), FMath::RoundToDouble(U.Condition)); X->SetNumberField(TEXT("price"), FMath::RoundToDouble(U.Price)); Us.Add(MakeShared<FJsonValueObject>(X));
		}
		for (const FADPAircraft& C : W.Aircraft) if (C.AirlineId == Me)
		{
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("reg"), C.Reg); X->SetStringField(TEXT("type"), C.TypeId); X->SetStringField(TEXT("status"), C.Status); X->SetStringField(TEXT("at"), C.Location); X->SetStringField(TEXT("base"), C.Base);
			X->SetStringField(TEXT("stand"), C.Stand); X->SetNumberField(TEXT("condition"), FMath::RoundToDouble(C.Condition)); X->SetStringField(TEXT("own"), C.Ownership); X->SetNumberField(TEXT("lease"), FMath::RoundToDouble(C.LeaseMonthly));
			X->SetNumberField(TEXT("value"), FMath::RoundToDouble(C.Value * (0.55 + 0.45 * C.Condition / 100.0) * 0.94)); X->SetNumberField(TEXT("hours"), FMath::RoundToDouble(C.HoursMin / 60.0)); X->SetNumberField(TEXT("cycles"), C.Cycles);
			int32 Ln = 0; for (const FADPSlot& Sl : W.Slots) Ln += Sl.Reg == C.Reg; X->SetNumberField(TEXT("lines"), Ln);
			const FADPPlanned* Nx = ADPSchedule::Next(W, C.Reg, C.Location);
			if (Nx) X->SetStringField(TEXT("next"), FString::Printf(TEXT("%s to %s %s"), *Nx->FlightNo, *Nx->To, *ADPBoards::Local(Nx->Est).ToString(TEXT("%d.%m %H:%M"))));
			Fl.Add(MakeShared<FJsonValueObject>(X));
		}
		O->SetArrayField(TEXT("types"), Ty); O->SetArrayField(TEXT("used"), Us); O->SetArrayField(TEXT("fleet"), Fl);
		return Out(O);
	}
	if (Action == TEXT("buy"))
	{
		const FADPAirline* My = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Me; });
		const FString Reg = My ? ADPEconomy::BuyAircraft(W, Me, A, B == TEXT("lease"), My->Hub, At) : FString();
		if (Reg.IsEmpty()) return Ok(false, TEXT("not enough money"));
		for (FADPAircraft& C : W.Aircraft) if (C.Reg == Reg) ADPGround::AssignStand(W, C, At, At + FTimespan::FromDays(1.0));   // (delivered: on its stand at once)
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true); O->SetStringField(TEXT("reg"), Reg); return Out(O);
	}
	if (Action == TEXT("buyused"))
	{
		const FADPAirline* My = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Me; });
		const FString Reg = My ? ADPEconomy::BuyUsed(W, Me, FCString::Atoi(*A), My->Hub, At) : FString();
		if (Reg.IsEmpty()) return Ok(false, TEXT("not enough money"));
		for (FADPAircraft& C : W.Aircraft) if (C.Reg == Reg) ADPGround::AssignStand(W, C, At, At + FTimespan::FromDays(1.0));   // (delivered: on its stand at once)
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true); O->SetStringField(TEXT("reg"), Reg); return Out(O);
	}
	if (Action == TEXT("sell"))
	{
		const FADPAircraft* C = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == A && X.AirlineId == Me; });
		if (!C) return Ok(false, TEXT("not your aircraft"));
		if (C->Status == TEXT("flying") || C->Location != C->Base) return Ok(false, TEXT("it has to stand at its base"));
		return Ok(ADPEconomy::SellAircraft(W, A, At), TEXT("it is in the air"));
	}
	if (Action == TEXT("places"))
	{
		// where this aircraft may fly from its base, the best markets first (Schengen first); a search by name or code
		const FADPAircraft* C = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == A && X.AirlineId == Me; });
		const FADPAirportInfo* Hub = C ? ADPWorldData::Airport(C->Base) : nullptr;
		if (!Hub) return Ok(false, TEXT("not your aircraft"));
		struct FP { const FADPAirportInfo* Ap; double Score, Nm; };
		TArray<FP> L; const FString Q = B.ToUpper().TrimStartAndEnd();
		ADPWorldData::ForEachAirport([&](const FADPAirportInfo& Ap)
		{
			if (Ap.Icao.IsEmpty() || !Ap.bScheduled || (Ap.Kind != 'L' && Ap.Kind != 'M')) return;
			if (!Q.IsEmpty() && !Ap.Icao.ToUpper().StartsWith(Q) && !Ap.Iata.ToUpper().StartsWith(Q) && !Ap.Name.ToUpper().Contains(Q) && !Ap.Town.ToUpper().Contains(Q)) return;
			const double Nm = ADPWorldData::DistanceNm(Hub->Lat, Hub->Lon, Ap.Lat, Ap.Lon);
			if (Nm < 60.0 || Nm > 2700.0) return;
			L.Add({&Ap, ADPDemand::MarketPax(*Hub, Ap, At) * (ADPSchedule::Schengen(Ap.Country) ? 1.0 : 0.5), Nm});
		});
		L.Sort([](const FP& X, const FP& Y) { return X.Score > Y.Score; });
		TArray<TSharedPtr<FJsonValue>> Ps; int32 Tried = 0;
		for (const FP& P : L)
		{
			if (Ps.Num() >= 40 || ++Tried > 120) break;
			FString Why; if (!ADPSchedule::CanReach(C->TypeId, C->Base, P.Ap->Icao, Why)) continue;
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("icao"), P.Ap->Icao); X->SetStringField(TEXT("iata"), P.Ap->Iata); X->SetStringField(TEXT("name"), P.Ap->Name); X->SetStringField(TEXT("town"), P.Ap->Town); X->SetStringField(TEXT("country"), P.Ap->Country);
			X->SetNumberField(TEXT("nm"), FMath::RoundToDouble(P.Nm)); X->SetNumberField(TEXT("pax"), FMath::RoundToDouble(ADPDemand::MarketPax(*Hub, *P.Ap, At))); X->SetNumberField(TEXT("block"), FMath::RoundToDouble(ADPSchedule::Block(C->TypeId, C->Base, P.Ap->Icao)));
			X->SetBoolField(TEXT("schengen"), ADPSchedule::Schengen(P.Ap->Country) && ADPSchedule::Schengen(Hub->Country)); Ps.Add(MakeShared<FJsonValueObject>(X));
		}
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true); O->SetArrayField(TEXT("places"), Ps); return Out(O);
	}
	if (Action == TEXT("timetable"))
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("ok"), true);
		O->SetStringField(TEXT("now"), ADPBoards::Local(At).ToString(TEXT("%H:%M"))); O->SetStringField(TEXT("missing"), ADPSchedule::MissingStaff(W, At));
		auto Town = [](const FString& I) { const FADPAirportInfo* Ap = ADPWorldData::Airport(I); return !Ap ? I : Ap->Town.IsEmpty() ? Ap->Name : Ap->Town; };
		TArray<TSharedPtr<FJsonValue>> Sl, Pl, Fl;
		for (const FADPSlot& X0 : W.Slots)
		{
			const FADPAircraft* C = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == X0.Reg; });
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("id"), X0.Id); X->SetStringField(TEXT("reg"), X0.Reg); X->SetStringField(TEXT("to"), X0.To); X->SetStringField(TEXT("town"), Town(X0.To));
			X->SetNumberField(TEXT("dep"), X0.DepMin); X->SetNumberField(TEXT("days"), X0.Days); X->SetNumberField(TEXT("number"), X0.Number);
			if (C) { X->SetNumberField(TEXT("out"), FMath::RoundToDouble(ADPSchedule::Block(C->TypeId, C->Base, X0.To))); X->SetNumberField(TEXT("back"), FMath::RoundToDouble(ADPSchedule::Block(C->TypeId, X0.To, C->Base))); }
			const FADPRoute* R = W.Routes.FindByPredicate([&](const FADPRoute& Z) { return Z.Id == X0.RouteId; });
			if (R) { X->SetNumberField(TEXT("fare"), R->FareIndex); X->SetNumberField(TEXT("load"), FMath::RoundToDouble(R->LoadEma * 100.0)); X->SetNumberField(TEXT("profit"), FMath::RoundToDouble(R->ProfitEma)); X->SetStringField(TEXT("route"), R->Id); }
			Sl.Add(MakeShared<FJsonValueObject>(X));
		}
		for (const FADPPlanned& P : W.Planned) if (P.AirlineId == Me && P.Why != TEXT("taken out of the timetable"))
		{
			const FADPTypeInfo* T = nullptr; for (const FADPAircraft& C : W.Aircraft) if (C.Reg == P.Reg) T = ADPWorldData::Type(C.TypeId);
			TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("flight"), P.FlightNo); X->SetStringField(TEXT("reg"), P.Reg); X->SetStringField(TEXT("from"), P.From); X->SetStringField(TEXT("to"), P.To); X->SetStringField(TEXT("town"), Town(P.To));
			X->SetNumberField(TEXT("at"), double((P.Sched - FDateTime(1970, 1, 1)).GetTotalMinutes())); X->SetStringField(TEXT("time"), ADPBoards::Local(P.Sched).ToString(TEXT("%d.%m %H:%M"))); X->SetNumberField(TEXT("late"), FMath::RoundToDouble((P.Est - P.Sched).GetTotalMinutes()));
			X->SetNumberField(TEXT("booked"), P.Booked); X->SetNumberField(TEXT("seats"), T ? T->Seats : 0); X->SetStringField(TEXT("state"), P.State); X->SetStringField(TEXT("why"), P.Why); X->SetNumberField(TEXT("gate"), ADPGround::GateOfStand(P.Stand));
			Pl.Add(MakeShared<FJsonValueObject>(X));
		}
		for (const FADPAircraft& C : W.Aircraft) if (C.AirlineId == Me) { TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("reg"), C.Reg); X->SetStringField(TEXT("type"), C.TypeId); X->SetStringField(TEXT("base"), C.Base); Fl.Add(MakeShared<FJsonValueObject>(X)); }
		O->SetArrayField(TEXT("slots"), Sl); O->SetArrayField(TEXT("flights"), Pl); O->SetArrayField(TEXT("fleet"), Fl);
		return Out(O);
	}
	if (Action == TEXT("slot"))
	{
		// reg|to|HH:MM|days (bit 0 Monday)
		TArray<FString> P; Arg.ParseIntoArray(P, TEXT("|"), false);
		if (P.Num() < 4) return Ok(false, TEXT("incomplete"));
		FString Hh, Mm; if (!P[2].Split(TEXT(":"), &Hh, &Mm)) return Ok(false, TEXT("no time"));
		const FADPAirportInfo* To = ADPWorldData::Airport(P[1].ToUpper());
		FString Why; return Ok(ADPSchedule::AddSlot(W, P[0], To && !To->Icao.IsEmpty() ? To->Icao : P[1].ToUpper(), FCString::Atoi(*Hh) * 60 + FCString::Atoi(*Mm), FCString::Atoi(*P[3]), At, Why), Why);
	}
	if (Action == TEXT("slotdel")) return Ok(ADPSchedule::RemoveSlot(W, A, At), TEXT("not in the timetable"));
	if (Action == TEXT("fare"))
	{
		FADPRoute* R = W.Routes.FindByPredicate([&](const FADPRoute& X) { return X.Id == A && X.AirlineId == Me; });
		if (!R) return Ok(false, TEXT("not your route"));
		R->FareIndex = FMath::Clamp(FCString::Atod(*B), 0.5, 2.0); return Ok(true);
	}
	if (Action == TEXT("hire"))
	{
		const FADPAirline* My = W.Airlines.FindByPredicate([&](const FADPAirline& X) { return X.Id == Me; });
		return Ok(ADPStaff::Hire(W, A, Me, B.IsEmpty() && My ? My->Hub : B, At), TEXT("no longer looking for work"));
	}
	if (Action == TEXT("dismiss")) return Ok(ADPStaff::Dismiss(W, A, At), TEXT("not on your staff"));
	if (Action == TEXT("pay")) return Ok(ADPStaff::SetPay(W, A, FCString::Atod(*B)), TEXT("not on your staff"));
	if (Action == TEXT("base")) return Ok(ADPStaff::SetBase(W, A, B), TEXT("unknown airport"));
	if (Action == TEXT("interview")) return Ok(ADPStaff::Interview(W, A), TEXT("no longer looking for work"));
	if (Action == TEXT("advert")) { static FRandomStream Rs(int32(FDateTime::UtcNow().GetTicks() & 0x7fffffff)); return Ok(ADPStaff::Advert(W, Me, A, At, Rs), TEXT("not enough money for the advert")); }
	if (Action == TEXT("contract")) return Ok(ADPStaff::SetContract(W, A, B), TEXT("no change"));
	if (Action == TEXT("leave")) return Ok(ADPStaff::AnswerLeave(W, FCString::Atoi(*A), B == TEXT("yes"), At), TEXT("already answered"));
	if (Action == TEXT("check"))
	{
		FADPAircraft* C = W.Aircraft.FindByPredicate([&](const FADPAircraft& X) { return X.Reg == A && X.AirlineId == Me; });
		FString Why; return Ok(C && ADPMaint::StartCheck(W, *C, B.IsEmpty() ? TEXT("A") : B, At, Why), C ? Why : TEXT("not your aircraft"));
	}
	if (Action == TEXT("fund")) return Ok(ADPTown::CommunityFund(W, FCString::Atod(*A), At), TEXT("not enough money"));
	if (Action == TEXT("insulate")) return Ok(ADPTown::Insulate(W, FCString::Atoi(*A), At), TEXT("not enough money"));
	if (Action == TEXT("compensate")) return Ok(ADPTown::Compensate(W, FCString::Atod(*A), At), TEXT("not enough money"));
	if (Action == TEXT("lobby")) { FString Why; return Ok(ADPTown::Lobby(W, At, Why), Why); }
	if (Action == TEXT("bribe")) { FString Why; return Ok(ADPTown::Bribe(W, FCString::Atod(*A), At, Why), Why); }
	if (Action == TEXT("firetruck")) return Ok(ADPTown::BuyFireTruck(W, At), TEXT("a crash tender costs $850,000"));
	if (Action == TEXT("ff")) return Ok(ADPNetwork::SetFrequentFlyer(W, Me, A == TEXT("on")));
	if (Action == TEXT("join")) { FString Why; return Ok(ADPNetwork::JoinAlliance(W, Me, A, At, Why), Why); }
	if (Action == TEXT("found")) { FString Why; return Ok(ADPNetwork::FoundAlliance(W, Me, A, At, Why), Why); }
	if (Action == TEXT("leaveAlliance")) return Ok(ADPNetwork::LeaveAlliance(W, Me), TEXT("not in an alliance"));
	if (Action == TEXT("ipo")) { FString Why; return Ok(ADPNetwork::Ipo(W, Me, At, Why), Why); }
	if (Action == TEXT("shares")) { FString Why; return Ok(ADPNetwork::TradeShares(W, A, FCString::Atod(*B), At, Why), Why); }
	if (Action == TEXT("hedge")) { FString Why; return Ok(ADPNetwork::Hedge(W, Me, FCString::Atod(*A), B.IsEmpty() ? 3 : FCString::Atoi(*B), At, Why), Why); }
	if (Action == TEXT("vehicle")) return Ok(ADPGround::BuyVehicle(W, Me, A, At), TEXT("not enough money"));
	if (Action == TEXT("parts")) return Ok(ADPMaint::BuyParts(W, Me, FCString::Atod(*A), At), TEXT("not enough money"));
	if (Action == TEXT("dayoff")) return Ok(ADPRoster::GiveDaysOff(W, A, B.IsEmpty() ? 2 : FCString::Atoi(*B), At), TEXT("not on your staff"));
	if (Action == TEXT("train")) return Ok(ADPStaff::StartTypeRating(W, A, B, At), TEXT("can't start that course"));
	return Ok(false, TEXT("unknown action"));
}
