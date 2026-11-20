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
	if (!S || !S->IsRunning()) return Ok(false, TEXT("no career running"));
	FADPWorld& W = S->World;
	const FString Me = UADPSim::Me();
	FString A, B; if (!Arg.Split(TEXT("|"), &A, &B)) A = Arg;
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
	if (Action == TEXT("vehicle")) return Ok(ADPGround::BuyVehicle(W, Me, A, At), TEXT("not enough money"));
	if (Action == TEXT("parts")) return Ok(ADPMaint::BuyParts(W, Me, FCString::Atod(*A), At), TEXT("not enough money"));
	if (Action == TEXT("dayoff")) return Ok(ADPRoster::GiveDaysOff(W, A, B.IsEmpty() ? 2 : FCString::Atoi(*B), At), TEXT("not on your staff"));
	if (Action == TEXT("train")) return Ok(ADPStaff::StartTypeRating(W, A, B, At), TEXT("can't start that course"));
	return Ok(false, TEXT("unknown action"));
}
