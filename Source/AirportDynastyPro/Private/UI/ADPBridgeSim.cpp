// The simulation for the menus (ue.adp.sim) - see UI/ADPBridge.h. To-do 176: the Overview's crew screens read and change
// the real simulation (one set of crew), through calls the native Overview will use too.
#include "UI/ADPBridge.h"
#include "UI/ADPHUD.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPStaff.h"
#include "Sim/ADPWorldData.h"
#include "Sim/ADPRoster.h"
#include "Sim/ADPMaint.h"
#include "Sim/ADPGround.h"
#include "Sim/ADPNetwork.h"
#include "Sim/ADPTown.h"
#include "Sim/ADPBoards.h"
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
		TArray<TSharedPtr<FJsonValue>> Ty; for (const FADPTypeInfo& T : ADPWorldData::Types()) { TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("id"), T.Id); X->SetStringField(TEXT("name"), T.Name); Ty.Add(MakeShared<FJsonValueObject>(X)); }
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
